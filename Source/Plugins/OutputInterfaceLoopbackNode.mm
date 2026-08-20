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
#include "OutputInterfaceLoopbackNode.h"
#include "../UI/AudioResilienceManager.h"

#if JUCE_MAC
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <CoreAudio/CoreAudio.h>
#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#import <AVFoundation/AVFoundation.h>
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
static AudioObjectID findDeviceID (const juce::String& targetNameOrUID)
{
    if (targetNameOrUID.isEmpty())
    {
        AudioObjectPropertyAddress defProp = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        AudioObjectID defID = kAudioObjectUnknown;
        UInt32 defSize = sizeof (defID);
        AudioObjectGetPropertyData (kAudioObjectSystemObject, &defProp, 0, NULL, &defSize, &defID);
        return defID;
    }

    AudioObjectPropertyAddress devicesProp = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    UInt32 dataSize = 0;
    if (AudioObjectGetPropertyDataSize (kAudioObjectSystemObject, &devicesProp, 0, NULL, &dataSize) != noErr || dataSize == 0)
        return kAudioObjectUnknown;

    size_t numDevices = (size_t)(dataSize / sizeof (AudioObjectID));
    std::vector<AudioObjectID> devIDs (numDevices);
    if (AudioObjectGetPropertyData (kAudioObjectSystemObject, &devicesProp, 0, NULL, &dataSize, devIDs.data()) != noErr)
        return kAudioObjectUnknown;

    AudioObjectPropertyAddress uidProp = { kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    AudioObjectPropertyAddress nameProp = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };

    // Pass 1: Exact match on UID or name
    for (size_t i = 0; i < numDevices; ++i)
    {
        AudioObjectID devID = devIDs[i];

        CFStringRef devUID = NULL;
        UInt32 uidSize = sizeof (devUID);
        if (AudioObjectGetPropertyData (devID, &uidProp, 0, NULL, &uidSize, &devUID) == noErr && devUID != NULL)
        {
            juce::String uidStr = juce::String::fromUTF8 ([(__bridge NSString*)devUID UTF8String]);
            CFRelease (devUID);
            if (uidStr.equalsIgnoreCase (targetNameOrUID))
                return devID;
        }

        CFStringRef devName = NULL;
        UInt32 nameSize = sizeof (devName);
        if (AudioObjectGetPropertyData (devID, &nameProp, 0, NULL, &nameSize, &devName) == noErr && devName != NULL)
        {
            juce::String nameStr = juce::String::fromUTF8 ([(__bridge NSString*)devName UTF8String]);
            CFRelease (devName);
            if (nameStr.equalsIgnoreCase (targetNameOrUID))
                return devID;
        }
    }

    // Pass 2: Prefix / Substring match
    for (size_t i = 0; i < numDevices; ++i)
    {
        AudioObjectID devID = devIDs[i];
        CFStringRef devName = NULL;
        UInt32 nameSize = sizeof (devName);
        if (AudioObjectGetPropertyData (devID, &nameProp, 0, NULL, &nameSize, &devName) == noErr && devName != NULL)
        {
            juce::String nameStr = juce::String::fromUTF8 ([(__bridge NSString*)devName UTF8String]);
            CFRelease (devName);
            if (targetNameOrUID.startsWithIgnoreCase (nameStr) || nameStr.startsWithIgnoreCase (targetNameOrUID)
                || targetNameOrUID.containsIgnoreCase (nameStr) || nameStr.containsIgnoreCase (targetNameOrUID))
                return devID;
        }
    }

    return kAudioObjectUnknown;
}

static CFStringRef getDeviceUID (AudioObjectID devID)
{
    AudioObjectPropertyAddress uidProp = { kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    CFStringRef devUID = NULL;
    UInt32 uidSize = sizeof (devUID);
    if (AudioObjectGetPropertyData (devID, &uidProp, 0, NULL, &uidSize, &devUID) == noErr && devUID != NULL)
        return devUID;
    return NULL;
}

static std::atomic<int> activeTapCount { 0 };

//==============================================================================
// Process-wide Shared Tap Session to maintain persistent CoreAudio client hooks
// across node destructions, preset changes, and graph topology updates.
struct SharedTapSession {
    juce::SpinLock lock;
    AudioObjectID tapID = 0;
    AudioObjectID aggregateDeviceID = 0;
    AudioDeviceIOProcID ioProcID = nullptr;
    juce::String currentDeviceName;
    double currentSampleRate = 0.0;
    
    juce::SpinLock listenerLock;
    std::vector<OutputInterfaceLoopbackNode*> listeners;
    
    SharedTapSession()
    {
        listeners.reserve (8);
    }
    
    void addListener (OutputInterfaceLoopbackNode* node)
    {
        const juce::SpinLock::ScopedLockType sl (listenerLock);
        if (std::find (listeners.begin(), listeners.end(), node) == listeners.end())
            listeners.push_back (node);
    }
    
    void removeListener (OutputInterfaceLoopbackNode* node)
    {
        const juce::SpinLock::ScopedLockType sl (listenerLock);
        listeners.erase (std::remove (listeners.begin(), listeners.end(), node), listeners.end());
    }
    
    void broadcastAudioInterleaved (const float* data, int numChannels, int numSamples)
    {
        const juce::SpinLock::ScopedLockType sl (listenerLock);
        for (auto* l : listeners)
            l->pushAudioInterleaved (data, numChannels, numSamples);
    }
    
    void broadcastAudio (const float* const* channelData, int numChannels, int numSamples)
    {
        const juce::SpinLock::ScopedLockType sl (listenerLock);
        for (auto* l : listeners)
            l->pushAudio (channelData, numChannels, numSamples);
    }
    
    void updateMuteBehavior (bool shouldBeActive)
    {
        if (tapID != 0)
        {
            AudioObjectPropertyAddress descProp = { kAudioTapPropertyDescription, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
            CATapDescription* curDesc = nil;
            UInt32 size = sizeof (curDesc);
            if (AudioObjectGetPropertyData (tapID, &descProp, 0, NULL, &size, &curDesc) == noErr && curDesc != nil)
            {
                curDesc.muteBehavior = shouldBeActive ? CATapMutedWhenTapped : CATapUnmuted;
                AudioObjectSetPropertyData (tapID, &descProp, 0, NULL, sizeof (curDesc), &curDesc);
            }
        }
    }

    void stopAndDestroy()
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        currentDeviceName.clear();
        currentSampleRate = 0.0;
        
        if (ioProcID != nullptr && aggregateDeviceID != 0)
        {
            AudioDeviceStop (aggregateDeviceID, ioProcID);
            AudioDeviceDestroyIOProcID (aggregateDeviceID, ioProcID);
            ioProcID = nullptr;
        }
        if (aggregateDeviceID != 0)
        {
            AudioHardwareDestroyAggregateDevice (aggregateDeviceID);
            aggregateDeviceID = 0;
        }
        if (tapID != 0)
        {
            AudioHardwareDestroyProcessTap (tapID);
            tapID = 0;
        }
    }

    bool ensureInitialized (const juce::String& targetDeviceName, double sampleRate, int samplesPerBlock)
    {
        const juce::SpinLock::ScopedLockType sl (lock);

        juce::String resolvedName = targetDeviceName;
        if (resolvedName.isEmpty())
        {
            if (auto* settings = getUserSettings())
                if (auto savedState = settings->getXmlValue ("audioDeviceState"))
                    resolvedName = savedState->getStringAttribute ("audioOutputDeviceName");
        }

        if (tapID != 0 && aggregateDeviceID != 0 && currentDeviceName == resolvedName)
            return true;

        if (tapID != 0)
        {
            currentDeviceName.clear();
            currentSampleRate = 0.0;
            if (ioProcID != nullptr && aggregateDeviceID != 0)
            {
                AudioDeviceStop (aggregateDeviceID, ioProcID);
                AudioDeviceDestroyIOProcID (aggregateDeviceID, ioProcID);
                ioProcID = nullptr;
            }
            if (aggregateDeviceID != 0)
            {
                AudioHardwareDestroyAggregateDevice (aggregateDeviceID);
                aggregateDeviceID = 0;
            }
            if (tapID != 0)
            {
                AudioHardwareDestroyProcessTap (tapID);
                tapID = 0;
            }
        }

        if (@available(macOS 14.2, *))
        {
            @autoreleasepool 
            {
                pid_t myPid = getpid();
                AudioObjectID myProcessObjectID = 0;
                UInt32 size = sizeof(myProcessObjectID);
                AudioObjectPropertyAddress prop = { kAudioHardwarePropertyTranslatePIDToProcessObject, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop, sizeof(myPid), &myPid, &size, &myProcessObjectID) != noErr) {
                    return false;
                }

                AudioObjectID outputDevID = findDeviceID (resolvedName);
                CFStringRef outputUID = getDeviceUID (outputDevID);
                if (outputUID == NULL) {
                    return false;
                }

                CATapDescription *tapDesc = [[[CATapDescription alloc] initExcludingProcesses:@[ @(myProcessObjectID) ]
                                                                                 andDeviceUID:(__bridge NSString*)outputUID
                                                                                   withStream:0] autorelease];
                if (!tapDesc) {
                    tapDesc = [[[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:@[ @(myProcessObjectID) ]] autorelease];
                }

                if (!tapDesc) {
                    CFRelease(outputUID);
                    return false;
                }

                tapDesc.UUID = [NSUUID UUID];
                tapDesc.muteBehavior = (activeTapCount.load (std::memory_order_relaxed) > 0) ? CATapMutedWhenTapped : CATapUnmuted;

                OSStatus tapErr = AudioHardwareCreateProcessTap(tapDesc, &tapID);
                if (tapErr != noErr || tapID == 0) {
                    CFRelease(outputUID);
                    return false;
                }

                NSMutableDictionary *aggDict = [NSMutableDictionary dictionaryWithDictionary:@{
                    @(kAudioAggregateDeviceNameKey): @"Curve Loopback Tap",
                    @(kAudioAggregateDeviceUIDKey): [[NSUUID UUID] UUIDString],
                    @(kAudioAggregateDeviceTapListKey): @[ @{ 
                        @(kAudioSubTapUIDKey): tapDesc.UUID.UUIDString,
                        @(kAudioSubTapDriftCompensationKey): @NO
                    } ],
                    @(kAudioAggregateDeviceIsPrivateKey): @YES,
                    @(kAudioAggregateDeviceSubDeviceListKey): @[ @{
                        @(kAudioSubDeviceUIDKey): (__bridge NSString*)outputUID
                    } ],
                    @(kAudioAggregateDeviceMasterSubDeviceKey): (__bridge NSString*)outputUID
                }];
                OSStatus aggErr = AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)aggDict, &aggregateDeviceID);

                if (aggErr != noErr) {
                    CFRelease(outputUID);
                    if (tapID != 0) { AudioHardwareDestroyProcessTap (tapID); tapID = 0; }
                    return false;
                }
                CFRelease(outputUID);

                Float64 targetSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
                AudioObjectPropertyAddress srProp = { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                AudioObjectSetPropertyData(aggregateDeviceID, &srProp, 0, NULL, sizeof(Float64), &targetSampleRate);

                UInt32 targetBufferSize = (UInt32) juce::jlimit (64, 1024, samplesPerBlock > 0 ? samplesPerBlock : 128);
                AudioObjectPropertyAddress bsProp = { kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                AudioObjectSetPropertyData(aggregateDeviceID, &bsProp, 0, NULL, sizeof(UInt32), &targetBufferSize);

                SharedTapSession* selfSession = this;
                AudioDeviceIOBlock ioBlock = ^(const AudioTimeStamp *inNow, const AudioBufferList *inInputData, const AudioTimeStamp *inInputTime, AudioBufferList *outOutputData, const AudioTimeStamp *inOutputTime) {
                    juce::ignoreUnused(inNow, inInputTime, outOutputData, inOutputTime);

                    if (inInputData == nullptr || inInputData->mNumberBuffers == 0) return;
                    UInt32 numBuffers = inInputData->mNumberBuffers;

                    const ::AudioBuffer& tapBuffer = inInputData->mBuffers[numBuffers - 1];

                    int tapChannelCount = 0;
                    for (int b = (int)numBuffers - 1; b >= 0; --b)
                    {
                        if (inInputData->mBuffers[b].mNumberChannels == 1 && inInputData->mBuffers[b].mData != nullptr)
                            ++tapChannelCount;
                        else
                            break;
                    }

                    if (tapChannelCount > 1 && tapBuffer.mNumberChannels == 1) {
                        juce::HeapBlock<const float*> channelPointers (tapChannelCount);
                        UInt32 minBytes = UINT32_MAX;
                        for (int ch = 0; ch < tapChannelCount; ++ch) {
                            int bufIdx = (int)numBuffers - tapChannelCount + ch;
                            channelPointers[ch] = static_cast<const float*>(inInputData->mBuffers[bufIdx].mData);
                            minBytes = std::min (minBytes, inInputData->mBuffers[bufIdx].mDataByteSize);
                        }
                        int numSamples = static_cast<int> (minBytes / sizeof(float));
                        if (numSamples > 0)
                            selfSession->broadcastAudio (channelPointers.get(), tapChannelCount, numSamples);
                    } else if (tapBuffer.mNumberChannels >= 1 && tapBuffer.mData != nullptr && tapBuffer.mDataByteSize > 0) {
                        int inChannels = static_cast<int> (tapBuffer.mNumberChannels);
                        const float* interleavedData = static_cast<const float*>(tapBuffer.mData);
                        int numSamples = static_cast<int> (tapBuffer.mDataByteSize / (tapBuffer.mNumberChannels * sizeof(float)));
                        selfSession->broadcastAudioInterleaved (interleavedData, inChannels, numSamples);
                    }
                };

                if (AudioDeviceCreateIOProcIDWithBlock(&ioProcID, aggregateDeviceID, NULL, ioBlock) == noErr) {
                    if (AudioDeviceStart(aggregateDeviceID, ioProcID) == noErr) {
                        currentDeviceName = resolvedName;
                        currentSampleRate = sampleRate;
                        return true;
                    } else {
                        if (ioProcID != nullptr) { AudioDeviceDestroyIOProcID (aggregateDeviceID, ioProcID); ioProcID = nullptr; }
                        if (aggregateDeviceID != 0) { AudioHardwareDestroyAggregateDevice (aggregateDeviceID); aggregateDeviceID = 0; }
                        if (tapID != 0) { AudioHardwareDestroyProcessTap (tapID); tapID = 0; }
                    }
                } else {
                    if (aggregateDeviceID != 0) { AudioHardwareDestroyAggregateDevice (aggregateDeviceID); aggregateDeviceID = 0; }
                    if (tapID != 0) { AudioHardwareDestroyProcessTap (tapID); tapID = 0; }
                }
            }
        }
        return false;
    }
};

static SharedTapSession& getSharedTapSession()
{
    static SharedTapSession s_session;
    return s_session;
}
#endif

//==============================================================================
OutputInterfaceLoopbackNode::OutputInterfaceLoopbackNode()
    : AudioPluginInstance (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      ringBuffer (2, ringBufferCapacity)
{
    ringBuffer.clear();
    if (auto* settings = getUserSettings())
        if (auto savedState = settings->getXmlValue ("audioDeviceState"))
            targetOutputDeviceName = savedState->getStringAttribute ("audioOutputDeviceName");

    isActiveInGraph.store (true, std::memory_order_release);
    activeTapCount.fetch_add (1, std::memory_order_relaxed);
#if JUCE_MAC
    if (@available(macOS 14.2, *))
        getSharedTapSession().updateMuteBehavior (true);
#endif
}

OutputInterfaceLoopbackNode::~OutputInterfaceLoopbackNode()
{
#if JUCE_MAC
    getSharedTapSession().removeListener (this);
#endif

    if (isActiveInGraph.load (std::memory_order_relaxed))
        activeTapCount.fetch_sub (1, std::memory_order_relaxed);

#if JUCE_MAC
    if (activeTapCount.load (std::memory_order_relaxed) <= 0)
        getSharedTapSession().updateMuteBehavior (false);
#endif
}

void OutputInterfaceLoopbackNode::setTargetOutputDeviceName (const juce::String& name)
{
    if (targetOutputDeviceName != name)
    {
        targetOutputDeviceName = name;
#if JUCE_MAC
        auto& session = getSharedTapSession();
        if (session.currentDeviceName.isNotEmpty() && session.currentDeviceName != name && session.tapID != 0)
        {
            session.stopAndDestroy();
            refreshCapture();
        }
#endif
    }
}

void OutputInterfaceLoopbackNode::refreshCapture()
{
    if (isActiveInGraph.load (std::memory_order_acquire))
    {
        double sr = getSampleRate();
        int bs = getBlockSize();
        if (sr <= 0.0) sr = 44100.0;
        if (bs <= 0)   bs = 512;
        isCapturing.store (false, std::memory_order_release);
        prepareToPlay (sr, bs);
    }
}

void OutputInterfaceLoopbackNode::setActiveState (bool shouldBeActive)
{
    if (isActiveInGraph.load (std::memory_order_relaxed) == shouldBeActive)
        return;

    isActiveInGraph.store (shouldBeActive, std::memory_order_release);

    if (shouldBeActive)
        activeTapCount.fetch_add (1, std::memory_order_relaxed);
    else
        activeTapCount.fetch_sub (1, std::memory_order_relaxed);

#if JUCE_MAC
    if (@available(macOS 14.2, *))
    {
        getSharedTapSession().updateMuteBehavior (activeTapCount.load (std::memory_order_relaxed) > 0);
    }
#endif

    if (shouldBeActive)
    {
        double sr = getSampleRate();
        int bs = getBlockSize();
        if (sr <= 0.0) sr = 44100.0;
        if (bs <= 0)   bs = 512;
        isCapturing.store (false, std::memory_order_release);
        prepareToPlay (sr, bs);
    }
    else
    {
        releaseResources();
    }
}

void OutputInterfaceLoopbackNode::updateGlobalMuteBehavior()
{
#if JUCE_MAC
    if (@available(macOS 14.2, *))
    {
        getSharedTapSession().updateMuteBehavior (activeTapCount.load (std::memory_order_relaxed) > 0);
    }
#endif
}

bool OutputInterfaceLoopbackNode::isAnyTapActiveInGraph()
{
    return activeTapCount.load (std::memory_order_relaxed) > 0;
}

void OutputInterfaceLoopbackNode::warmUpTap (const juce::String& targetDevice, double sampleRate)
{
#if JUCE_MAC
    if (@available(macOS 14.2, *))
    {
        auto& session = getSharedTapSession();
        session.ensureInitialized (targetDevice, sampleRate, 128);
        session.updateMuteBehavior (OutputInterfaceLoopbackNode::isAnyTapActiveInGraph());
    }
#endif
}

void OutputInterfaceLoopbackNode::syncSystemOutputDevice (const juce::String& targetDeviceName)
{
#if JUCE_MAC
    AudioObjectID targetID = findDeviceID (targetDeviceName);
    if (targetID == kAudioObjectUnknown)
        return;

    AudioObjectPropertyAddress defaultOutputAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    AudioObjectID currentDefault = kAudioObjectUnknown;
    UInt32 curSize = sizeof (currentDefault);
    if (AudioObjectGetPropertyData (kAudioObjectSystemObject, &defaultOutputAddress, 0, NULL, &curSize, &currentDefault) == noErr)
    {
        if (currentDefault != targetID)
            AudioObjectSetPropertyData (kAudioObjectSystemObject, &defaultOutputAddress, 0, NULL, sizeof (AudioObjectID), &targetID);
    }
#endif
}

void OutputInterfaceLoopbackNode::setAudioWorkgroup (const juce::AudioWorkgroup& workgroup)
{
    const juce::SpinLock::ScopedLockType lock (workgroupLock);
    pendingWorkgroup = workgroup;
    workgroupNeedsUpdate.store (true, std::memory_order_release);
}

bool OutputInterfaceLoopbackNode::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannels() != 0)
        return false;

    auto outputSet = layouts.getMainOutputChannelSet();
    if (outputSet.isDisabled())
        return true;

    int numOuts = outputSet.size();
    return (numOuts >= 1 && numOuts <= 32);
}

void OutputInterfaceLoopbackNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    int numChannels = juce::jmax (1, getMainBusNumOutputChannels());

    {
        // Lock only around buffer reset so hot-swapping does not block the audio thread
        juce::ScopedLock lock (getCallbackLock());
        fifo.reset();
        ringBuffer.setSize (numChannels, ringBufferCapacity, false, true, true);
        ringBuffer.clear();
        isBuffering.store (true, std::memory_order_release);
        hadUnderrunLastBlock.store (true, std::memory_order_release);
    }

#if JUCE_MAC
    auto& session = getSharedTapSession();
    session.addListener (this);

    if (@available(macOS 14.2, *))
    {
        session.ensureInitialized (targetOutputDeviceName, sampleRate, samplesPerBlock);
        session.updateMuteBehavior (OutputInterfaceLoopbackNode::isAnyTapActiveInGraph());
        isCapturing.store (true, std::memory_order_release);
    }
#endif
}

void OutputInterfaceLoopbackNode::releaseResources()
{
    // Reset buffer tracking without destroying the underlying OS CoreAudio tap stream.
    juce::ScopedLock lock (getCallbackLock());
    fifo.reset();
    ringBuffer.clear();
    isBuffering.store (true, std::memory_order_release);
    hadUnderrunLastBlock.store (true, std::memory_order_release);

#if JUCE_MAC
    getSharedTapSession().removeListener (this);
#endif
}

void OutputInterfaceLoopbackNode::pushAudio (const float* const* channelData, int numChannels, int numSamples)
{
    if (numChannels <= 0 || numSamples <= 0 || channelData == nullptr) return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);
    
    int destChannels = ringBuffer.getNumChannels();
    int copyChans = juce::jmin (destChannels, numChannels);
    
    if (size1 > 0)
    {
        for (int ch = 0; ch < copyChans; ++ch)
            if (channelData[ch] != nullptr)
                ringBuffer.copyFrom (ch, start1, channelData[ch], size1);
        for (int ch = copyChans; ch < destChannels; ++ch)
            ringBuffer.clear (ch, start1, size1);
    }
            
    if (size2 > 0)
    {
        for (int ch = 0; ch < copyChans; ++ch)
            if (channelData[ch] != nullptr)
                ringBuffer.copyFrom (ch, start2, channelData[ch] + size1, size2);
        for (int ch = copyChans; ch < destChannels; ++ch)
            ringBuffer.clear (ch, start2, size2);
    }
            
    fifo.finishedWrite (size1 + size2);
}

void OutputInterfaceLoopbackNode::pushAudioInterleaved (const float* interleavedData, int numChannels, int numSamples)
{
    if (numChannels <= 0 || numSamples <= 0 || interleavedData == nullptr) return;

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
        int copyChans = juce::jmin (destChannels, numChannels);
        if (copyChans == 2)
        {
            if (size1 > 0)
            {
                float* dest0 = ringBuffer.getWritePointer (0, start1);
                float* dest1 = ringBuffer.getWritePointer (1, start1);
                for (int s = 0; s < size1; ++s)
                {
                    const float* frame = interleavedData + (s * numChannels);
                    dest0[s] = frame[0];
                    dest1[s] = frame[1];
                }
                for (int ch = 2; ch < destChannels; ++ch)
                    ringBuffer.clear (ch, start1, size1);
            }

            if (size2 > 0)
            {
                float* dest0 = ringBuffer.getWritePointer (0, start2);
                float* dest1 = ringBuffer.getWritePointer (1, start2);
                const float* src2 = interleavedData + (size1 * numChannels);
                for (int s = 0; s < size2; ++s)
                {
                    const float* frame = src2 + (s * numChannels);
                    dest0[s] = frame[0];
                    dest1[s] = frame[1];
                }
                for (int ch = 2; ch < destChannels; ++ch)
                    ringBuffer.clear (ch, start2, size2);
            }
        }
        else
        {
            if (size1 > 0)
            {
                for (int ch = 0; ch < copyChans; ++ch)
                {
                    float* dest = ringBuffer.getWritePointer (ch, start1);
                    for (int s = 0; s < size1; ++s)
                        dest[s] = interleavedData[s * numChannels + ch];
                }
                for (int ch = copyChans; ch < destChannels; ++ch)
                    ringBuffer.clear (ch, start1, size1);
            }

            if (size2 > 0)
            {
                const float* src2 = interleavedData + (size1 * numChannels);
                for (int ch = 0; ch < copyChans; ++ch)
                {
                    float* dest = ringBuffer.getWritePointer (ch, start2);
                    for (int s = 0; s < size2; ++s)
                        dest[s] = src2[s * numChannels + ch];
                }
                for (int ch = copyChans; ch < destChannels; ++ch)
                    ringBuffer.clear (ch, start2, size2);
            }
        }
    }
            
    fifo.finishedWrite (size1 + size2);
}

void OutputInterfaceLoopbackNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    if (! isCapturing.load (std::memory_order_acquire))
        return;
    
    // Target buffer cushion (~3ms)
    double sr = getSampleRate();
    int rateCushion = (sr > 0.0) ? (int) std::ceil (sr * 0.003) : 128;
    int targetCushion = buffer.getNumSamples() + juce::jmax (128, rateCushion);
    int currentReady = fifo.getNumReady();
    int numSamplesNeeded = buffer.getNumSamples();

    // Discard overflow frames
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
            return;
    }

    // Read available frames
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
                int fade = juce::jmin (fadeSamples, numSamplesToRead);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.applyGainRamp (ch, 0, fade, 0.0f, 1.0f);
                hadUnderrunLastBlock.store (false, std::memory_order_relaxed);
            }
            else if (willUnderrun)
            {
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

void OutputInterfaceLoopbackNode::fillInPluginDescription (juce::PluginDescription& description) const
{
    description.name = getName();
    description.descriptiveName = getName();
    description.pluginFormatName = "Internal";
    description.category = "Audio I/O";
    description.fileOrIdentifier = "OutputInterfaceLoopback";
    description.uniqueId = 0x53415450; // "SATP"
    description.isInstrument = false;
    description.numInputChannels = 0;
    description.numOutputChannels = getMainBusNumOutputChannels();
}
