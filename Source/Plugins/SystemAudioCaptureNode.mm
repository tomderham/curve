/*
==============================================================================
   Copyright (c) Thomas Derham

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   CURVE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.
==============================================================================
*/

#define DONT_SET_USING_JUCE_NAMESPACE 1
#include "SystemAudioCaptureNode.h"
#include "../UI/AudioResilienceManager.h"

#if JUCE_MAC
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <CoreAudio/CoreAudio.h>
#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#include <unistd.h>
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
#include <emmintrin.h>
#endif

//==============================================================================
class MacOSSleepWakeNotifier final : public MacOSSleepWakeNotifierBase
{
public:
    explicit MacOSSleepWakeNotifier (std::function<void(bool)> callbackIn)
        : callback (std::move (callbackIn))
    {
        NSNotificationCenter* center = [[NSWorkspace sharedWorkspace] notificationCenter];
        
        wakeObserver = [center addObserverForName:NSWorkspaceDidWakeNotification
                                           object:nil
                                            queue:[NSOperationQueue mainQueue]
                                       usingBlock:^(NSNotification*) {
            if (callback)
                callback (true);
        }];
        
        sleepObserver = [center addObserverForName:NSWorkspaceWillSleepNotification
                                            object:nil
                                             queue:[NSOperationQueue mainQueue]
                                        usingBlock:^(NSNotification*) {
            if (callback)
                callback (false);
        }];
    }

    ~MacOSSleepWakeNotifier()
    {
        NSNotificationCenter* center = [[NSWorkspace sharedWorkspace] notificationCenter];
        if (wakeObserver != nil)
            [center removeObserver:wakeObserver];
        if (sleepObserver != nil)
            [center removeObserver:sleepObserver];
    }

private:
    std::function<void(bool)> callback;
    id wakeObserver = nil;
    id sleepObserver = nil;
};

std::unique_ptr<MacOSSleepWakeNotifierBase> createMacOSSleepWakeNotifier (std::function<void(bool)> callback)
{
    return std::make_unique<MacOSSleepWakeNotifier> (std::move (callback));
}
#endif

using namespace juce;

juce::PropertiesFile* getUserSettings();

#if JUCE_MAC
//==============================================================================
struct SystemAudioCaptureNode::TapWrapper {
    AudioObjectID tapID = 0;
    AudioObjectID aggregateDeviceID = 0;
    AudioDeviceIOProcID ioProcID = nullptr;
};
#endif

//==============================================================================
SystemAudioCaptureNode::SystemAudioCaptureNode()
    : AudioPluginInstance (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      ringBuffer (2, ringBufferCapacity)
{
    ringBuffer.clear();
    
    if (auto* settings = getUserSettings())
        if (auto savedState = settings->getXmlValue ("audioDeviceState"))
            targetOutputDeviceName = savedState->getStringAttribute ("audioOutputDeviceName");

#if JUCE_MAC
    tapWrapper = std::make_unique<TapWrapper>();
#endif
}

SystemAudioCaptureNode::~SystemAudioCaptureNode()
{
    releaseResources();
}

void SystemAudioCaptureNode::setTargetOutputDeviceName (const juce::String& name)
{
    if (targetOutputDeviceName != name)
    {
        targetOutputDeviceName = name;

        // If the tap is already running, gracefully hot-swap the aggregate device
        if (isCapturing.load (std::memory_order_acquire))
        {
            double sr = getSampleRate();
            int bs = getBlockSize();
            if (sr > 0.0 && bs > 0)
            {
                isCapturing.store (false, std::memory_order_release);
                prepareToPlay (sr, bs);
            }
        }
    }
}

void SystemAudioCaptureNode::setAudioWorkgroup (const juce::AudioWorkgroup& workgroup)
{
    const juce::SpinLock::ScopedLockType lock (workgroupLock);
    pendingWorkgroup = workgroup;
    workgroupNeedsUpdate.store (true, std::memory_order_release);
}

bool SystemAudioCaptureNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return (layouts.getMainInputChannels() == 0 && layouts.getMainOutputChannels() <= 2);
}

void SystemAudioCaptureNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    isCapturing.store (false, std::memory_order_release);
    releaseResources();

    {
        // Lock only around buffer reset so hot-swapping does not block the audio thread
        juce::ScopedLock lock (getCallbackLock());
        fifo.reset();
        ringBuffer.clear();
        isBuffering.store (true, std::memory_order_release);
        hadUnderrunLastBlock.store (true, std::memory_order_release);
    }

#if JUCE_MAC
    if (@available(macOS 14.2, *)) {
        @autoreleasepool 
        {
            pid_t myPid = getpid();
            AudioObjectID myProcessObjectID = 0;
            UInt32 size = sizeof(myProcessObjectID);
            AudioObjectPropertyAddress prop = { kAudioHardwarePropertyTranslatePIDToProcessObject, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
            if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop, sizeof(myPid), &myPid, &size, &myProcessObjectID) != noErr) {
                return;
            }

            // Autorelease description for non-ARC environments
            CATapDescription *tapDesc = [[[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:@[ @(myProcessObjectID) ]] autorelease];
            if (!tapDesc) {
                return;
            }
            tapDesc.UUID = [NSUUID UUID];

            // Mute original system audio output while tapped
            tapDesc.muteBehavior = CATapMutedWhenTapped;

            if (AudioHardwareCreateProcessTap(tapDesc, &tapWrapper->tapID) != noErr) {
                return;
            }

            // Target output device clock reference, defaulting to system output
            CFStringRef outputUID = NULL;
            if (targetOutputDeviceName.isNotEmpty()) {
                AudioObjectPropertyAddress devicesProp = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                UInt32 dataSize = 0;
                if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devicesProp, 0, NULL, &dataSize) == noErr) {
                    int numDevices = dataSize / sizeof(AudioObjectID);
                    juce::HeapBlock<AudioObjectID> deviceIDs (numDevices);
                    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &devicesProp, 0, NULL, &dataSize, deviceIDs.get()) == noErr) {
                        // Pass 1: Prioritize exact matches on device name or UID
                        for (int i = 0; i < numDevices; ++i) {
                            AudioObjectID devID = deviceIDs[i];
                            CFStringRef devName = NULL;
                            UInt32 nameSize = sizeof(devName);
                            AudioObjectPropertyAddress nameProp = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                            juce::String name;
                            if (AudioObjectGetPropertyData(devID, &nameProp, 0, NULL, &nameSize, &devName) == noErr && devName != NULL) {
                                name = juce::String::fromCFString(devName);
                                CFRelease(devName);
                            }

                            CFStringRef devUID = NULL;
                            UInt32 uidSize = sizeof(devUID);
                            AudioObjectPropertyAddress uidProp = { kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                            juce::String uidStr;
                            if (AudioObjectGetPropertyData(devID, &uidProp, 0, NULL, &uidSize, &devUID) == noErr && devUID != NULL) {
                                uidStr = juce::String::fromCFString(devUID);
                                CFRelease(devUID);
                            }

                            if (name == targetOutputDeviceName || uidStr == targetOutputDeviceName) {
                                if (uidStr.isNotEmpty()) {
                                    outputUID = uidStr.toCFString();
                                    break;
                                }
                            }
                        }

                        // Pass 2: Fallback to case-insensitive partial substring match
                        if (!outputUID) {
                            for (int i = 0; i < numDevices; ++i) {
                                AudioObjectID devID = deviceIDs[i];
                                CFStringRef devName = NULL;
                                UInt32 nameSize = sizeof(devName);
                                AudioObjectPropertyAddress nameProp = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                                juce::String name;
                                if (AudioObjectGetPropertyData(devID, &nameProp, 0, NULL, &nameSize, &devName) == noErr && devName != NULL) {
                                    name = juce::String::fromCFString(devName);
                                    CFRelease(devName);
                                }

                                CFStringRef devUID = NULL;
                                UInt32 uidSize = sizeof(devUID);
                                AudioObjectPropertyAddress uidProp = { kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                                juce::String uidStr;
                                if (AudioObjectGetPropertyData(devID, &uidProp, 0, NULL, &uidSize, &devUID) == noErr && devUID != NULL) {
                                    uidStr = juce::String::fromCFString(devUID);
                                    CFRelease(devUID);
                                }

                                if ((name.isNotEmpty() && name.containsIgnoreCase (targetOutputDeviceName))
                                    || (targetOutputDeviceName.isNotEmpty() && targetOutputDeviceName.containsIgnoreCase (name))) {
                                    if (uidStr.isNotEmpty()) {
                                        outputUID = uidStr.toCFString();
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (!outputUID) {
                AudioObjectID defaultOutputID = kAudioObjectUnknown;
                UInt32 sizeID = sizeof(defaultOutputID);
                AudioObjectPropertyAddress defaultOutputAddress = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                AudioObjectGetPropertyData(kAudioObjectSystemObject, &defaultOutputAddress, 0, NULL, &sizeID, &defaultOutputID);
                if (defaultOutputID != kAudioObjectUnknown) {
                    UInt32 sizeUID = sizeof(outputUID);
                    AudioObjectPropertyAddress uidAddress = { kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                    AudioObjectGetPropertyData(defaultOutputID, &uidAddress, 0, NULL, &sizeUID, &outputUID);
                }
            }

            if (!outputUID) {
                releaseResources();
                return;
            }

            // Build Aggregate Device linking the Tap and the Output Interface
            NSMutableDictionary *aggDict = [NSMutableDictionary dictionaryWithDictionary:@{
                @(kAudioAggregateDeviceNameKey): @"Curve System Tap",
                @(kAudioAggregateDeviceUIDKey): [[NSUUID UUID] UUIDString],
                @(kAudioAggregateDeviceTapListKey): @[ @{ 
                    @(kAudioSubTapUIDKey): tapDesc.UUID.UUIDString,
                   #if defined(kAudioSubTapDriftCompensationKey)
                    @(kAudioSubTapDriftCompensationKey): @YES // Enable CoreAudio's native ASRC/Drift Correction
                   #else
                    @(kAudioSubDeviceDriftCompensationKey): @YES // Enable CoreAudio's native ASRC/Drift Correction
                   #endif
                } ],
                @(kAudioAggregateDeviceIsPrivateKey): @YES,
                @(kAudioAggregateDeviceSubDeviceListKey): @[ @{
                    @(kAudioSubDeviceUIDKey): (__bridge NSString*)outputUID
                } ],
                @(kAudioAggregateDeviceMasterSubDeviceKey): (__bridge NSString*)outputUID
            }];
            if (AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)aggDict, &tapWrapper->aggregateDeviceID) != noErr) {
                CFRelease(outputUID);
                releaseResources();
                return;
            }
            CFRelease(outputUID);

            Float64 targetSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
            AudioObjectPropertyAddress srProp = { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
            AudioObjectSetPropertyData(tapWrapper->aggregateDeviceID, &srProp, 0, NULL, sizeof(Float64), &targetSampleRate);

            // Set tap buffer size aligned with host buffer size
            UInt32 targetBufferSize = (UInt32) juce::jlimit (64, 1024, samplesPerBlock > 0 ? samplesPerBlock : 128);
            AudioObjectPropertyAddress bsProp = { kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
            AudioObjectSetPropertyData(tapWrapper->aggregateDeviceID, &bsProp, 0, NULL, sizeof(UInt32), &targetBufferSize);

            SystemAudioCaptureNode* node = this;
            AudioDeviceIOBlock ioBlock = ^(const AudioTimeStamp *inNow, const AudioBufferList *inInputData, const AudioTimeStamp *inInputTime, AudioBufferList *outOutputData, const AudioTimeStamp *inOutputTime) {
                juce::ignoreUnused(inNow, inInputTime, outOutputData, inOutputTime);
                if (!node) return;

                // Join tap IO thread to the output device audio workgroup using thread_local token management
                if (node->workgroupNeedsUpdate.load (std::memory_order_acquire))
                {
                    const juce::SpinLock::ScopedTryLockType tryLock (node->workgroupLock);
                    if (tryLock.isLocked())
                    {
                        auto workgroup = node->pendingWorkgroup;
                        node->workgroupNeedsUpdate.store (false, std::memory_order_release);

                        thread_local juce::WorkgroupToken workgroupToken;
                        workgroupToken = {};
                        if (workgroup)
                            workgroup.join (workgroupToken);
                    }
                }

                if (inInputData == nullptr || inInputData->mNumberBuffers == 0) return;
                UInt32 numBuffers = inInputData->mNumberBuffers;

                // Taps are appended to the end of the Aggregate Device's buffer list.
                const ::AudioBuffer& lastBuffer = inInputData->mBuffers[numBuffers - 1];

                if (lastBuffer.mNumberChannels == 1 && numBuffers >= 2) {
                    // Non-interleaved: the tap's L and R channels are the last two separate buffers
                    const ::AudioBuffer& leftBuffer = inInputData->mBuffers[numBuffers - 2];
                    
                    if (leftBuffer.mNumberChannels == 1 && leftBuffer.mData != nullptr && lastBuffer.mData != nullptr && leftBuffer.mDataByteSize > 0 && lastBuffer.mDataByteSize > 0) {
                        const float* channelPointers[2];
                        channelPointers[0] = static_cast<const float*>(leftBuffer.mData);
                        channelPointers[1] = static_cast<const float*>(lastBuffer.mData);
                        int numSamples = static_cast<int> (std::min (leftBuffer.mDataByteSize, lastBuffer.mDataByteSize) / sizeof(float));
                        
                        node->pushAudio(channelPointers, 2, numSamples);
                    }
                } else if (lastBuffer.mNumberChannels >= 2 && lastBuffer.mData != nullptr && lastBuffer.mDataByteSize > 0) {
                    // Interleaved: the tap's stereo channels are combined in the final buffer
                    int numChannels = static_cast<int> (lastBuffer.mNumberChannels);
                    const float* interleavedData = static_cast<const float*>(lastBuffer.mData);
                    int numSamples = static_cast<int> (lastBuffer.mDataByteSize / (lastBuffer.mNumberChannels * sizeof(float)));
                    
                    node->pushAudioInterleaved(interleavedData, numChannels, numSamples);
                } else if (lastBuffer.mNumberChannels == 1 && numBuffers == 1 && lastBuffer.mData != nullptr && lastBuffer.mDataByteSize > 0) {
                    // Single mono buffer: duplicate channel to stereo
                    const float* channelPointers[2];
                    channelPointers[0] = static_cast<const float*>(lastBuffer.mData);
                    channelPointers[1] = static_cast<const float*>(lastBuffer.mData);
                    int numSamples = static_cast<int> (lastBuffer.mDataByteSize / sizeof(float));
                    
                    node->pushAudio(channelPointers, 2, numSamples);
                }
            };

            if (AudioDeviceCreateIOProcIDWithBlock(&tapWrapper->ioProcID, tapWrapper->aggregateDeviceID, NULL, ioBlock) == noErr) {
                if (AudioDeviceStart(tapWrapper->aggregateDeviceID, tapWrapper->ioProcID) == noErr) {
                    isCapturing.store (true, std::memory_order_release);
                } else {
                    releaseResources();
                }
            } else {
                releaseResources();
            }
        }
    }
#endif
}

void SystemAudioCaptureNode::releaseResources()
{
#if JUCE_MAC
    isCapturing.store (false, std::memory_order_release);

    if (tapWrapper != nullptr)
    {
        if (tapWrapper->ioProcID != nullptr && tapWrapper->aggregateDeviceID != 0) {
            AudioDeviceStop (tapWrapper->aggregateDeviceID, tapWrapper->ioProcID);
            AudioDeviceDestroyIOProcID (tapWrapper->aggregateDeviceID, tapWrapper->ioProcID);
            tapWrapper->ioProcID = nullptr;
        }
        if (tapWrapper->aggregateDeviceID != 0) {
            AudioHardwareDestroyAggregateDevice (tapWrapper->aggregateDeviceID);
            tapWrapper->aggregateDeviceID = 0;
        }
        if (tapWrapper->tapID != 0) {
            AudioHardwareDestroyProcessTap (tapWrapper->tapID);
            tapWrapper->tapID = 0;
        }
    }
#endif
}

void SystemAudioCaptureNode::pushAudio (const float* const* channelData, int numChannels, int numSamples)
{
    if (numChannels <= 0 || numSamples <= 0) return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);
    
    int destChannels = ringBuffer.getNumChannels();
    
    if (size1 > 0)
        for (int ch = 0; ch < destChannels; ++ch)
        {
            int sourceCh = juce::jmin (ch, numChannels - 1);
            ringBuffer.copyFrom (ch, start1, channelData[sourceCh], size1);
        }
            
    if (size2 > 0)
        for (int ch = 0; ch < destChannels; ++ch)
        {
            int sourceCh = juce::jmin (ch, numChannels - 1);
            ringBuffer.copyFrom (ch, start2, channelData[sourceCh] + size1, size2);
        }
            
    fifo.finishedWrite (size1 + size2);
}

void SystemAudioCaptureNode::pushAudioInterleaved (const float* interleavedData, int numChannels, int numSamples)
{
    if (numChannels <= 0 || numSamples <= 0) return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);
    
    int destChannels = ringBuffer.getNumChannels();

    if (destChannels == 2 && numChannels == 2)
    {
        if (size1 > 0)
        {
            float* dest0 = ringBuffer.getWritePointer (0, start1);
            float* dest1 = ringBuffer.getWritePointer (1, start1);
            int s = 0;
           #if defined(__ARM_NEON) || defined(__ARM_NEON__)
            for (; s <= size1 - 4; s += 4)
            {
                float32x4x2_t v = vld2q_f32 (interleavedData + (s * 2));
                vst1q_f32 (dest0 + s, v.val[0]);
                vst1q_f32 (dest1 + s, v.val[1]);
            }
           #elif defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
            for (; s <= size1 - 4; s += 4)
            {
                __m128 in0 = _mm_loadu_ps (interleavedData + (s * 2));
                __m128 in1 = _mm_loadu_ps (interleavedData + (s * 2) + 4);
                __m128 l = _mm_shuffle_ps (in0, in1, _MM_SHUFFLE (2, 0, 2, 0));
                __m128 r = _mm_shuffle_ps (in0, in1, _MM_SHUFFLE (3, 1, 3, 1));
                _mm_storeu_ps (dest0 + s, l);
                _mm_storeu_ps (dest1 + s, r);
            }
           #endif
            for (; s < size1; ++s)
            {
                dest0[s] = interleavedData[s * 2];
                dest1[s] = interleavedData[s * 2 + 1];
            }
        }

        if (size2 > 0)
        {
            float* dest0 = ringBuffer.getWritePointer (0, start2);
            float* dest1 = ringBuffer.getWritePointer (1, start2);
            const float* src2 = interleavedData + (size1 * 2);
            int s = 0;
           #if defined(__ARM_NEON) || defined(__ARM_NEON__)
            for (; s <= size2 - 4; s += 4)
            {
                float32x4x2_t v = vld2q_f32 (src2 + (s * 2));
                vst1q_f32 (dest0 + s, v.val[0]);
                vst1q_f32 (dest1 + s, v.val[1]);
            }
           #elif defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
            for (; s <= size2 - 4; s += 4)
            {
                __m128 in0 = _mm_loadu_ps (src2 + (s * 2));
                __m128 in1 = _mm_loadu_ps (src2 + (s * 2) + 4);
                __m128 l = _mm_shuffle_ps (in0, in1, _MM_SHUFFLE (2, 0, 2, 0));
                __m128 r = _mm_shuffle_ps (in0, in1, _MM_SHUFFLE (3, 1, 3, 1));
                _mm_storeu_ps (dest0 + s, l);
                _mm_storeu_ps (dest1 + s, r);
            }
           #endif
            for (; s < size2; ++s)
            {
                dest0[s] = src2[s * 2];
                dest1[s] = src2[s * 2 + 1];
            }
        }
    }
    else
    {
        if (size1 > 0)
        {
            for (int s = 0; s < size1; ++s)
            {
                const float* src = interleavedData + (s * numChannels);
                for (int ch = 0; ch < destChannels; ++ch)
                {
                    int sourceCh = juce::jmin (ch, numChannels - 1);
                    ringBuffer.setSample (ch, start1 + s, src[sourceCh]);
                }
            }
        }

        if (size2 > 0)
        {
            for (int s = 0; s < size2; ++s)
            {
                const float* src = interleavedData + ((size1 + s) * numChannels);
                for (int ch = 0; ch < destChannels; ++ch)
                {
                    int sourceCh = juce::jmin (ch, numChannels - 1);
                    ringBuffer.setSample (ch, start2 + s, src[sourceCh]);
                }
            }
        }
    }
            
    fifo.finishedWrite (size1 + size2);
}

void SystemAudioCaptureNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    if (! isCapturing.load (std::memory_order_acquire))
        return;
    
    // Safety buffer margin (~3ms or minimum 128 frames) for OS thread scheduling alignment
    double sr = getSampleRate();
    int rateCushion = (sr > 0.0) ? (int) std::ceil (sr * 0.003) : 128;
    int targetCushion = buffer.getNumSamples() + juce::jmax (128, rateCushion);
    int currentReady = fifo.getNumReady();
    int numSamplesNeeded = buffer.getNumSamples();

    // Drop overflow frames when backlog exceeds safety threshold
    int maxCushion = juce::jmin (ringBufferCapacity - 1024, targetCushion + buffer.getNumSamples() * 4);
    if (currentReady > maxCushion)
    {
        int dropCount = currentReady - targetCushion;
        int start1, size1, start2, size2;
        fifo.prepareToRead (dropCount, start1, size1, start2, size2);
        fifo.finishedRead (size1 + size2);
        currentReady -= (size1 + size2);
        hadUnderrunLastBlock.store (true, std::memory_order_relaxed);
    }
    
    if (isBuffering.load (std::memory_order_relaxed))
    {
        hadUnderrunLastBlock.store (true, std::memory_order_relaxed);
        if (currentReady >= targetCushion)
            isBuffering.store (false, std::memory_order_relaxed);
        else
            return; // Buffering initial frames
    }

    // Read available frames during transient underrun
    int numSamplesToRead = juce::jmin (currentReady, numSamplesNeeded);
    if (numSamplesToRead > 0)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (numSamplesToRead, start1, size1, start2, size2);
        
        if (size1 + size2 > 0)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                if (size1 > 0 && ch < ringBuffer.getNumChannels())
                    buffer.copyFrom (ch, 0, ringBuffer.getReadPointer(ch, start1), size1);
                if (size2 > 0 && ch < ringBuffer.getNumChannels())
                    buffer.copyFrom (ch, size1, ringBuffer.getReadPointer(ch, start2), size2);
            }
            fifo.finishedRead (size1 + size2);

            bool wasUnderrun = hadUnderrunLastBlock.load (std::memory_order_relaxed);
            bool willUnderrun = (numSamplesToRead < numSamplesNeeded);
            int fadeSamples = juce::jmin (numSamplesToRead, juce::jmax (64, (int) std::ceil (sr * 0.005)));

            if (wasUnderrun && willUnderrun)
            {
                // Split available samples between in-ramp and out-ramp on compound underruns
                int inSamples = juce::jmin (fadeSamples, numSamplesToRead / 2);
                int outSamples = juce::jmin (fadeSamples, numSamplesToRead - inSamples);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    if (inSamples > 0)
                        buffer.applyGainRamp (ch, 0, inSamples, 0.0f, 1.0f);
                    if (outSamples > 0)
                        buffer.applyGainRamp (ch, numSamplesToRead - outSamples, outSamples, 1.0f, 0.0f);
                }
                hadUnderrunLastBlock.store (true, std::memory_order_relaxed);
                isBuffering.store (true, std::memory_order_relaxed);
            }
            else if (wasUnderrun)
            {
                // Smoothly ramp in when recovering from underrun or initial buffering
                int fade = juce::jmin (fadeSamples, numSamplesToRead);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.applyGainRamp (ch, 0, fade, 0.0f, 1.0f);
                hadUnderrunLastBlock.store (false, std::memory_order_relaxed);
            }
            else if (willUnderrun)
            {
                // Smoothly ramp out during underrun and re-enter buffering mode
                int fade = juce::jmin (fadeSamples, numSamplesToRead);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.applyGainRamp (ch, numSamplesToRead - fade, fade, 1.0f, 0.0f);
                hadUnderrunLastBlock.store (true, std::memory_order_relaxed);
                isBuffering.store (true, std::memory_order_relaxed);
            }
        }
    }
    else
    {
        hadUnderrunLastBlock.store (true, std::memory_order_relaxed);
        isBuffering.store (true, std::memory_order_relaxed);
    }
}

void SystemAudioCaptureNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = getName();
    description.pluginFormatName = "Internal";
    description.category = "I/O";
    description.fileOrIdentifier = "SystemAudio";
    description.uniqueId = 0x53415450; // "SATP"
    description.isInstrument = false;
    description.numInputChannels = 0;
    description.numOutputChannels = 2;
}