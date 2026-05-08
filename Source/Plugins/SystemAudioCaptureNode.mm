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

#if JUCE_MAC
#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>
#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#include <unistd.h>
#endif

using namespace juce;

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
        // Lock audio processing while swapping the target device clock
        juce::ScopedLock lock (getCallbackLock());
        
        targetOutputDeviceName = name;
        
        // If the tap is already running, gracefully hot-swap the aggregate device
        if (isCapturing.load())
        {
            double sr = getSampleRate();
            int bs = getBlockSize();
            if (sr > 0.0 && bs > 0)
            {
                releaseResources();
                prepareToPlay (sr, bs);
            }
        }
    }
}

bool SystemAudioCaptureNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return (layouts.getMainInputChannels() == 0 && layouts.getMainOutputChannels() <= 2);
}

void SystemAudioCaptureNode::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    fifo.reset();
    ringBuffer.clear();
    isBuffering = true;
    
#if JUCE_MAC
    if (!isCapturing.exchange(true)) {
        if (@available(macOS 14.2, *)) {
            @autoreleasepool 
            {
                pid_t myPid = getpid();
                AudioObjectID myProcessObjectID = 0;
                UInt32 size = sizeof(myProcessObjectID);
                AudioObjectPropertyAddress prop = { kAudioHardwarePropertyTranslatePIDToProcessObject, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop, sizeof(myPid), &myPid, &size, &myProcessObjectID) != noErr) {
                    isCapturing.store (false);
                    return;
                }

                // Autorelease to prevent memory leaks in non-ARC compilation environments
                CATapDescription *tapDesc = [[[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:@[ @(myProcessObjectID) ]] autorelease];
                if (!tapDesc) {
                    isCapturing.store (false);
                    return;
                }
                tapDesc.UUID = [NSUUID UUID];

                // Mute the original system audio so we don't hear a mix of unprocessed and processed audio
                tapDesc.muteBehavior = CATapMutedWhenTapped;

                if (AudioHardwareCreateProcessTap(tapDesc, &tapWrapper->tapID) != noErr) {
                    isCapturing.store (false);
                    return;
                }

                // Find the master clock reference - Curve's output device, or fallback to system default output
                CFStringRef outputUID = NULL;
                if (targetOutputDeviceName.isNotEmpty()) {
                    AudioObjectPropertyAddress devicesProp = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                    UInt32 dataSize = 0;
                    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devicesProp, 0, NULL, &dataSize) == noErr) {
                        int numDevices = dataSize / sizeof(AudioObjectID);
                        juce::HeapBlock<AudioObjectID> deviceIDs (numDevices);
                        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &devicesProp, 0, NULL, &dataSize, deviceIDs.get()) == noErr) {
                            for (int i = 0; i < numDevices; ++i) {
                                AudioObjectID devID = deviceIDs[i];
                                CFStringRef devName = NULL;
                                UInt32 nameSize = sizeof(devName);
                                AudioObjectPropertyAddress nameProp = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                                if (AudioObjectGetPropertyData(devID, &nameProp, 0, NULL, &nameSize, &devName) == noErr && devName != NULL) {
                                    juce::String name = juce::String::fromCFString(devName);
                                    CFRelease(devName);
                                    if (name == targetOutputDeviceName) {
                                        UInt32 uidSize = sizeof(outputUID);
                                        AudioObjectPropertyAddress uidProp = { kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                                        AudioObjectGetPropertyData(devID, &uidProp, 0, NULL, &uidSize, &outputUID);
                                        break;
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

                // Build Aggregate Device linking the Tap and the Output Interface
                NSMutableDictionary *aggDict = [NSMutableDictionary dictionaryWithDictionary:@{
                    @(kAudioAggregateDeviceNameKey): @"Curve System Tap",
                    @(kAudioAggregateDeviceUIDKey): [[NSUUID UUID] UUIDString],
                    @(kAudioAggregateDeviceTapListKey): @[ @{ 
                        @(kAudioSubTapUIDKey): tapDesc.UUID.UUIDString,
                        @(kAudioSubDeviceDriftCompensationKey): @YES // Enable CoreAudio's native ASRC/Drift Correction
                    } ],
                    @(kAudioAggregateDeviceIsPrivateKey): @YES
                }];
                if (outputUID) {
                    aggDict[@(kAudioAggregateDeviceSubDeviceListKey)] = @[ @{
                        @(kAudioSubDeviceUIDKey): (__bridge NSString*)outputUID
                    } ];
                    aggDict[@(kAudioAggregateDeviceMasterSubDeviceKey)] = (__bridge NSString*)outputUID;
                }
                if (AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)aggDict, &tapWrapper->aggregateDeviceID) != noErr) {
                    if (outputUID) CFRelease(outputUID);
                    releaseResources();
                    return;
                }
                if (outputUID) CFRelease(outputUID);

                Float64 targetSampleRate = sampleRate;
                AudioObjectPropertyAddress srProp = { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                AudioObjectSetPropertyData(tapWrapper->aggregateDeviceID, &srProp, 0, NULL, sizeof(Float64), &targetSampleRate);

                // Force CoreAudio tap to 64 frames to minimize OS-side latency
                UInt32 targetBufferSize = 64;
                AudioObjectPropertyAddress bsProp = { kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                AudioObjectSetPropertyData(tapWrapper->aggregateDeviceID, &bsProp, 0, NULL, sizeof(UInt32), &targetBufferSize);

                SystemAudioCaptureNode* node = this;
                AudioDeviceIOBlock ioBlock = ^(const AudioTimeStamp *inNow, const AudioBufferList *inInputData, const AudioTimeStamp *inInputTime, AudioBufferList *outOutputData, const AudioTimeStamp *inOutputTime) {
                    juce::ignoreUnused(inNow, inInputTime, outOutputData, inOutputTime);
                    if (!node) return;

                    UInt32 numBuffers = inInputData->mNumberBuffers;
                    if (numBuffers == 0) return;

                    // Taps are appended to the end of the Aggregate Device's buffer list.
                    const ::AudioBuffer& lastBuffer = inInputData->mBuffers[numBuffers - 1];

                    if (lastBuffer.mNumberChannels == 1 && numBuffers >= 2) {
                        // Non-interleaved: the tap's L and R channels are the last two separate buffers
                        const ::AudioBuffer& leftBuffer = inInputData->mBuffers[numBuffers - 2];
                        
                        if (leftBuffer.mNumberChannels == 1) {
                            const float* channelPointers[2];
                            channelPointers[0] = static_cast<const float*>(leftBuffer.mData);
                            channelPointers[1] = static_cast<const float*>(lastBuffer.mData);
                            int numSamples = static_cast<int> (leftBuffer.mDataByteSize / sizeof(float));
                            
                            node->pushAudio(channelPointers, 2, numSamples);
                        }
                    } else if (lastBuffer.mNumberChannels >= 2) {
                        // Interleaved: the tap's stereo channels are combined in the final buffer
                        int numChannels = static_cast<int> (lastBuffer.mNumberChannels);
                        const float* interleavedData = static_cast<const float*>(lastBuffer.mData);
                        int numSamples = static_cast<int> (lastBuffer.mDataByteSize / (lastBuffer.mNumberChannels * sizeof(float)));
                        
                        node->pushAudioInterleaved(interleavedData, numChannels, numSamples);
                    }
                };

                if (AudioDeviceCreateIOProcIDWithBlock(&tapWrapper->ioProcID, tapWrapper->aggregateDeviceID, NULL, ioBlock) == noErr) {
                    AudioDeviceStart(tapWrapper->aggregateDeviceID, tapWrapper->ioProcID);
                } else {
                    releaseResources();
                }
            }
        } else {
            isCapturing.store (false);
        }
    }
#endif
}

void SystemAudioCaptureNode::releaseResources()
{
#if JUCE_MAC
    if (isCapturing.exchange(false)) {
        if (tapWrapper->ioProcID && tapWrapper->aggregateDeviceID) {
            AudioDeviceStop(tapWrapper->aggregateDeviceID, tapWrapper->ioProcID);
            AudioDeviceDestroyIOProcID(tapWrapper->aggregateDeviceID, tapWrapper->ioProcID);
            tapWrapper->ioProcID = nullptr;
        }
        if (tapWrapper->aggregateDeviceID) {
            AudioHardwareDestroyAggregateDevice(tapWrapper->aggregateDeviceID);
            tapWrapper->aggregateDeviceID = 0;
        }
        if (tapWrapper->tapID) {
            AudioHardwareDestroyProcessTap(tapWrapper->tapID);
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
    
    if (size1 > 0)
        for (int ch = 0; ch < destChannels; ++ch)
        {
            int sourceCh = juce::jmin (ch, numChannels - 1);
            float* dest = ringBuffer.getWritePointer (ch, start1);
            for (int s = 0; s < size1; ++s)
                dest[s] = interleavedData[s * numChannels + sourceCh];
        }
            
    if (size2 > 0)
        for (int ch = 0; ch < destChannels; ++ch)
        {
            int sourceCh = juce::jmin (ch, numChannels - 1);
            float* dest = ringBuffer.getWritePointer (ch, start2);
            for (int s = 0; s < size2; ++s)
                dest[s] = interleavedData[(size1 + s) * numChannels + sourceCh];
        }
            
    fifo.finishedWrite (size1 + size2);
}

void SystemAudioCaptureNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    buffer.clear();
    
    // Small safety margin (e.g., 128 frames) to absorb the OS thread phase offset
    int targetCushion = buffer.getNumSamples() + 128;
    int currentReady = fifo.getNumReady();
    int numSamplesNeeded = buffer.getNumSamples();

    // Hard-drop limit just in case the audio thread stalls heavily.
    int maxCushion = targetCushion + buffer.getNumSamples() * 4;
    if (currentReady > maxCushion)
    {
        int dropCount = currentReady - targetCushion;
        int start1, size1, start2, size2;
        fifo.prepareToRead (dropCount, start1, size1, start2, size2);
        fifo.finishedRead (size1 + size2);
        currentReady -= (size1 + size2);
    }
    
    if (isBuffering)
    {
        if (currentReady >= targetCushion)
            isBuffering = false;
        else
            return; // Still in initial buffering state, output silence.
    }

    if (currentReady < numSamplesNeeded) {
        // A transient underrun. Output silence for this block and wait for the fifo to fill up again.
        return;
    }
    
    int start1, size1, start2, size2;
    fifo.prepareToRead (numSamplesNeeded, start1, size1, start2, size2);
    
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