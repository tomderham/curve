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

    // Pass 2: Prefix match (e.g. CoreAudio device names with appended suffixes)
    for (size_t i = 0; i < numDevices; ++i)
    {
        AudioObjectID devID = devIDs[i];
        CFStringRef devName = NULL;
        UInt32 nameSize = sizeof (devName);
        if (AudioObjectGetPropertyData (devID, &nameProp, 0, NULL, &nameSize, &devName) == noErr && devName != NULL)
        {
            juce::String nameStr = juce::String::fromUTF8 ([(__bridge NSString*)devName UTF8String]);
            CFRelease (devName);
            if (targetNameOrUID.startsWithIgnoreCase (nameStr) || nameStr.startsWithIgnoreCase (targetNameOrUID))
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
    NSString* activeTapUUIDString = nil;
    
    juce::SpinLock listenerLock;
    static constexpr size_t maxListeners = 8;
    std::array<OutputInterfaceLoopbackNode*, maxListeners> listeners {};
    size_t numListeners = 0;
    
    SharedTapSession() = default;
    
    void addListener (OutputInterfaceLoopbackNode* node)
    {
        if (node == nullptr) return;
        const juce::SpinLock::ScopedLockType sl (listenerLock);
        for (size_t i = 0; i < numListeners; ++i)
            if (listeners[i] == node)
                return;

        if (numListeners < maxListeners)
            listeners[numListeners++] = node;
    }
    
    void removeListener (OutputInterfaceLoopbackNode* node)
    {
        if (node == nullptr) return;
        const juce::SpinLock::ScopedLockType sl (listenerLock);
        for (size_t i = 0; i < numListeners; ++i)
        {
            if (listeners[i] == node)
            {
                for (size_t j = i; j < numListeners - 1; ++j)
                    listeners[j] = listeners[j + 1];
                listeners[--numListeners] = nullptr;
                break;
            }
        }
    }
    
    void broadcastAudioInterleaved (const float* data, int numChannels, int numSamples)
    {
        const juce::SpinLock::ScopedLockType sl (listenerLock);
        for (size_t i = 0; i < numListeners; ++i)
            if (auto* l = listeners[i])
                l->pushAudioInterleaved (data, numChannels, numSamples);
    }
    
    void broadcastAudio (const float* const* channelData, int numChannels, int numSamples)
    {
        const juce::SpinLock::ScopedLockType sl (listenerLock);
        for (size_t i = 0; i < numListeners; ++i)
            if (auto* l = listeners[i])
                l->pushAudio (channelData, numChannels, numSamples);
    }
    
    void updateMuteBehavior (bool shouldBeActive)
    {
        if (tapID != 0)
        {
            AudioObjectPropertyAddress descProp = { kAudioTapPropertyDescription, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
            CFTypeRef curDescRef = NULL;
            UInt32 size = sizeof (curDescRef);
            if (AudioObjectGetPropertyData (tapID, &descProp, 0, NULL, &size, &curDescRef) == noErr && curDescRef != NULL)
            {
                CATapDescription* curDesc = (CATapDescription*) curDescRef;
                curDesc.muteBehavior = shouldBeActive ? CATapMutedWhenTapped : CATapUnmuted;
                AudioObjectSetPropertyData (tapID, &descProp, 0, NULL, sizeof (curDesc), &curDesc);
                CFRelease (curDescRef);
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
        if (activeTapUUIDString != nil)
        {
            [activeTapUUIDString release];
            activeTapUUIDString = nil;
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

        std::fprintf (stderr, "[LoopbackNode] ensureInitialized called: target='%s', sampleRate=%.1f, blockSize=%d (currentDev='%s', currentRate=%.1f, tapID=%u, aggID=%u)\n",
                      resolvedName.toRawUTF8(), sampleRate, samplesPerBlock, currentDeviceName.toRawUTF8(), currentSampleRate, (unsigned) tapID, (unsigned) aggregateDeviceID);

        if (tapID != 0 && aggregateDeviceID != 0 && currentDeviceName == resolvedName
            && sampleRate > 0.0 && std::abs (currentSampleRate - sampleRate) < 1.0)
        {
            return true;
        }

        if (@available(macOS 14.2, *))
        {
            @autoreleasepool 
            {
                if (tapID != 0 || aggregateDeviceID != 0)
                {
                    std::fprintf (stderr, "[LoopbackNode] Re-initializing tap session for '%s' @ %.1f Hz (previous: '%s' @ %.1f Hz, tapID=%u, aggID=%u)\n",
                                  resolvedName.toRawUTF8(), sampleRate, currentDeviceName.toRawUTF8(), currentSampleRate, (unsigned) tapID, (unsigned) aggregateDeviceID);
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
                    if (activeTapUUIDString != nil)
                    {
                        [activeTapUUIDString release];
                        activeTapUUIDString = nil;
                    }
                    currentDeviceName.clear();
                    currentSampleRate = 0.0;

                    // Allow CoreAudio HAL server (coreaudiod) a brief window to finalize tearing down the old stream
                    [NSThread sleepForTimeInterval:0.040];
                }

                pid_t myPid = getpid();
                AudioObjectID myProcessObjectID = 0;
                UInt32 size = sizeof(myProcessObjectID);
                AudioObjectPropertyAddress prop = { kAudioHardwarePropertyTranslatePIDToProcessObject, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop, sizeof(myPid), &myPid, &size, &myProcessObjectID) != noErr) {
                    std::fprintf (stderr, "[LoopbackNode] FAILED to translate PID to process object ID\n");
                    return false;
                }

                AudioObjectID outputDevID = findDeviceID (resolvedName);
                CFStringRef outputUID = getDeviceUID (outputDevID);
                if (outputUID == NULL) {
                    std::fprintf (stderr, "[LoopbackNode] FAILED to get UID for output device '%s' (devID=%u)\n", resolvedName.toRawUTF8(), (unsigned) outputDevID);
                    return false;
                }

                // Wait for the physical device to finish switching its hardware PLL sample rate
                AudioObjectPropertyAddress srProp = { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                if (outputDevID != kAudioObjectUnknown && sampleRate > 0.0)
                {
                    Float64 physRate = 0.0;
                    UInt32 rateSize = sizeof(physRate);
                    for (int retry = 0; retry < 15; ++retry)
                    {
                        if (AudioObjectGetPropertyData (outputDevID, &srProp, 0, NULL, &rateSize, &physRate) == noErr)
                        {
                            if (std::abs (physRate - sampleRate) < 1.0)
                                break;
                        }
                        [NSThread sleepForTimeInterval:0.020];
                    }
                }

                if (tapID == 0)
                {
                    std::fprintf (stderr, "[LoopbackNode] Creating Process Tap for output UID: %s\n", [(__bridge NSString*)outputUID UTF8String]);

                    CATapDescription *tapDesc = [[[CATapDescription alloc] initExcludingProcesses:@[ @(myProcessObjectID) ]
                                                                                     andDeviceUID:(__bridge NSString*)outputUID
                                                                                       withStream:0] autorelease];
                    if (!tapDesc) {
                        tapDesc = [[[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:@[ @(myProcessObjectID) ]] autorelease];
                    }

                    if (!tapDesc) {
                        std::fprintf (stderr, "[LoopbackNode] FAILED to allocate CATapDescription\n");
                        CFRelease(outputUID);
                        return false;
                    }

                    tapDesc.UUID = [NSUUID UUID];
                    tapDesc.muteBehavior = (activeTapCount.load (std::memory_order_relaxed) > 0) ? CATapMutedWhenTapped : CATapUnmuted;

                    OSStatus tapErr = AudioHardwareCreateProcessTap(tapDesc, &tapID);
                    if (tapErr != noErr || tapID == 0) {
                        std::fprintf (stderr, "[LoopbackNode] AudioHardwareCreateProcessTap FAILED with OSStatus: %d (tapID=%u)\n", (int) tapErr, (unsigned) tapID);
                        CFRelease(outputUID);
                        return false;
                    }
                    std::fprintf (stderr, "[LoopbackNode] AudioHardwareCreateProcessTap succeeded: tapID=%u\n", (unsigned) tapID);

                    if (activeTapUUIDString != nil)
                        [activeTapUUIDString release];
                    activeTapUUIDString = [tapDesc.UUID.UUIDString copy];
                }

                NSMutableDictionary *aggDict = [NSMutableDictionary dictionaryWithDictionary:@{
                    @(kAudioAggregateDeviceNameKey): @"Curve Loopback Tap",
                    @(kAudioAggregateDeviceUIDKey): [[NSUUID UUID] UUIDString],
                    @(kAudioAggregateDeviceTapListKey): @[ @{ 
                        @(kAudioSubTapUIDKey): activeTapUUIDString,
                        @(kAudioSubTapDriftCompensationKey): @NO
                    } ],
                    @(kAudioAggregateDeviceIsPrivateKey): @YES,
                    @(kAudioAggregateDeviceSubDeviceListKey): @[ @{
                        @(kAudioSubDeviceUIDKey): (__bridge NSString*)outputUID
                    } ],
                    @(kAudioAggregateDeviceMasterSubDeviceKey): (__bridge NSString*)outputUID,
                    @"main": (__bridge NSString*)outputUID
                }];
                OSStatus aggErr = AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)aggDict, &aggregateDeviceID);

                if (aggErr != noErr) {
                    std::fprintf (stderr, "[LoopbackNode] AudioHardwareCreateAggregateDevice FAILED with OSStatus: %d\n", (int) aggErr);
                    CFRelease(outputUID);
                    if (tapID != 0) { AudioHardwareDestroyProcessTap (tapID); tapID = 0; }
                    return false;
                }
                std::fprintf (stderr, "[LoopbackNode] AudioHardwareCreateAggregateDevice succeeded: aggID=%u\n", (unsigned) aggregateDeviceID);

                // Explicitly lock the aggregate clock to the physical hardware device
                AudioObjectPropertyAddress mainProp = { kAudioAggregateDevicePropertyMainSubDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                AudioObjectSetPropertyData (aggregateDeviceID, &mainProp, 0, NULL, sizeof(CFStringRef), &outputUID);

                CFStringRef clockUID = NULL;
                UInt32 clockSize = sizeof(clockUID);
                AudioObjectPropertyAddress clockProp = { kAudioAggregateDevicePropertyClockDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
                if (AudioObjectGetPropertyData (aggregateDeviceID, &clockProp, 0, NULL, &clockSize, &clockUID) == noErr && clockUID != NULL)
                {
                    std::fprintf (stderr, "[LoopbackNode] Aggregate clock subdevice confirmed: %s\n", [(__bridge NSString*)clockUID UTF8String]);
                    CFRelease (clockUID);
                }

                CFRelease(outputUID);

                Float64 targetSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
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
                        constexpr int maxTapChannels = 32;
                        int clampedChannels = std::min (tapChannelCount, maxTapChannels);
                        const float* channelPointers[maxTapChannels];
                        UInt32 minBytes = UINT32_MAX;
                        for (int ch = 0; ch < clampedChannels; ++ch) {
                            int bufIdx = (int)numBuffers - tapChannelCount + ch;
                            channelPointers[ch] = static_cast<const float*>(inInputData->mBuffers[bufIdx].mData);
                            minBytes = std::min (minBytes, inInputData->mBuffers[bufIdx].mDataByteSize);
                        }
                        int numSamples = static_cast<int> (minBytes / sizeof(float));
                        if (numSamples > 0)
                            selfSession->broadcastAudio (channelPointers, clampedChannels, numSamples);
                    } else if (tapBuffer.mNumberChannels >= 1 && tapBuffer.mData != nullptr && tapBuffer.mDataByteSize > 0) {
                        int inChannels = static_cast<int> (tapBuffer.mNumberChannels);
                        const float* interleavedData = static_cast<const float*>(tapBuffer.mData);
                        int numSamples = static_cast<int> (tapBuffer.mDataByteSize / (tapBuffer.mNumberChannels * sizeof(float)));
                        selfSession->broadcastAudioInterleaved (interleavedData, inChannels, numSamples);
                    }
                };

                OSStatus ioProcErr = AudioDeviceCreateIOProcIDWithBlock(&ioProcID, aggregateDeviceID, NULL, ioBlock);
                if (ioProcErr == noErr) {
                    OSStatus startErr = AudioDeviceStart(aggregateDeviceID, ioProcID);
                    if (startErr == noErr) {
                        currentDeviceName = resolvedName;
                        currentSampleRate = sampleRate;
                        std::fprintf (stderr, "[LoopbackNode] Fresh Aggregate & IOProc started for '%s' @ %.1f Hz\n", currentDeviceName.toRawUTF8(), currentSampleRate);
                        return true;
                    } else {
                        std::fprintf (stderr, "[LoopbackNode] AudioDeviceStart FAILED with OSStatus: %d\n", (int) startErr);
                        if (ioProcID != nullptr) { AudioDeviceDestroyIOProcID (aggregateDeviceID, ioProcID); ioProcID = nullptr; }
                        if (aggregateDeviceID != 0) { AudioHardwareDestroyAggregateDevice (aggregateDeviceID); aggregateDeviceID = 0; }
                    }
                } else {
                    std::fprintf (stderr, "[LoopbackNode] AudioDeviceCreateIOProcIDWithBlock FAILED with OSStatus: %d\n", (int) ioProcErr);
                    if (aggregateDeviceID != 0) { AudioHardwareDestroyAggregateDevice (aggregateDeviceID); aggregateDeviceID = 0; }
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
      ringBuffer (maxTapChannels, ringBufferCapacity)
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

void OutputInterfaceLoopbackNode::warmUpTap (const juce::String& targetDevice, double sampleRate, int bufferSize)
{
#if JUCE_MAC
    if (@available(macOS 14.2, *))
    {
        auto& session = getSharedTapSession();
        session.ensureInitialized (targetDevice, sampleRate, bufferSize);
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
    juce::ignoreUnused (workgroup);
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
    std::fprintf (stderr, "[LoopbackNode] prepareToPlay: sampleRate=%.1f, samplesPerBlock=%d, targetDevice='%s'\n",
                  sampleRate, samplesPerBlock, targetOutputDeviceName.toRawUTF8());
#if JUCE_MAC
    getSharedTapSession().removeListener (this);
#endif

    {
        // Lock only around buffer reset so hot-swapping does not block the audio thread
        juce::ScopedLock lock (getCallbackLock());
        fifo.reset();
        ringBuffer.clear();
        isPrimed.store (false, std::memory_order_release);
        hadUnderrunLastBlock.store (false, std::memory_order_release);
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
#if JUCE_MAC
    getSharedTapSession().removeListener (this);
#endif

    // Reset buffer tracking without destroying the underlying OS CoreAudio tap stream.
    juce::ScopedLock lock (getCallbackLock());
    fifo.reset();
    ringBuffer.clear();
    isPrimed.store (false, std::memory_order_release);
    hadUnderrunLastBlock.store (false, std::memory_order_release);
}

void OutputInterfaceLoopbackNode::pushAudio (const float* const* channelData, int numChannels, int numSamples)
{
    if (numChannels <= 0 || numSamples <= 0 || channelData == nullptr) return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

    int totalSpace = size1 + size2;
    if (totalSpace < numSamples)
    {
        // FIFO full, drop audio
        return;
    }

    int destChannels = ringBuffer.getNumChannels();
    int copyChannels = juce::jmin (destChannels, numChannels);

    for (int ch = 0; ch < copyChannels; ++ch)
    {
        if (size1 > 0)
            ringBuffer.copyFrom (ch, start1, channelData[ch], size1);
        if (size2 > 0)
            ringBuffer.copyFrom (ch, start2, channelData[ch] + size1, size2);
    }

    for (int ch = copyChannels; ch < destChannels; ++ch)
    {
        if (size1 > 0)
            ringBuffer.clear (ch, start1, size1);
        if (size2 > 0)
            ringBuffer.clear (ch, start2, size2);
    }

    fifo.finishedWrite (size1 + size2);
}

void OutputInterfaceLoopbackNode::pushAudioInterleaved (const float* interleavedData, int numChannels, int numSamples)
{
    if (numChannels <= 0 || numSamples <= 0 || interleavedData == nullptr) return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

    int totalSpace = size1 + size2;
    if (totalSpace < numSamples)
    {
        // FIFO full, drop audio
        return;
    }

    int destChannels = ringBuffer.getNumChannels();
    if (numChannels == 2 && destChannels >= 2)
    {
        if (size1 > 0)
        {
            float* dest0 = ringBuffer.getWritePointer (0, start1);
            float* dest1 = ringBuffer.getWritePointer (1, start1);
            for (int s = 0; s < size1; ++s)
            {
                dest0[s] = interleavedData[s * 2];
                dest1[s] = interleavedData[s * 2 + 1];
            }
            for (int ch = 2; ch < destChannels; ++ch)
                ringBuffer.clear (ch, start1, size1);
        }

        if (size2 > 0)
        {
            float* dest0 = ringBuffer.getWritePointer (0, start2);
            float* dest1 = ringBuffer.getWritePointer (1, start2);
            const float* src2 = interleavedData + (size1 * 2);
            for (int s = 0; s < size2; ++s)
            {
                dest0[s] = src2[s * 2];
                dest1[s] = src2[s * 2 + 1];
            }
            for (int ch = 2; ch < destChannels; ++ch)
                ringBuffer.clear (ch, start2, size2);
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
    
    double sr = getSampleRate();
    if (sr <= 0.0) sr = 48000.0;

    int numSamplesNeeded = buffer.getNumSamples();
    int currentReady = fifo.getNumReady();

    // Discard excessive frames (>40ms) to bound latency
    int maxCushion = juce::jmin (ringBufferCapacity - 1024, numSamplesNeeded * 4 + (int) std::ceil (sr * 0.040));
    if (currentReady > maxCushion)
    {
        int dropCount = currentReady - (numSamplesNeeded * 2);
        int start1, size1, start2, size2;
        fifo.prepareToRead (dropCount, start1, size1, start2, size2);
        fifo.finishedRead (size1 + size2);
        currentReady -= (size1 + size2);
    }

    // On initial start or sample-rate switch, wait once for 10ms of phase margin to prevent phase-jitter starvation
    if (! isPrimed.load (std::memory_order_relaxed))
    {
        int primeThreshold = juce::jmax (numSamplesNeeded * 4, (int) std::ceil (sr * 0.010));
        if (currentReady >= primeThreshold)
        {
            isPrimed.store (true, std::memory_order_relaxed);
            hadUnderrunLastBlock.store (false, std::memory_order_relaxed);
        }
        else
        {
            return;
        }
    }

    // Read available frames immediately without pausing
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
            int fadeSamples = juce::jmin (numSamplesToRead, juce::jmax (32, (int) std::ceil (sr * 0.002)));

            if (wasUnderrun)
            {
                int fade = juce::jmin (fadeSamples, numSamplesToRead);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.applyGainRamp (ch, 0, fade, 0.0f, 1.0f);
                hadUnderrunLastBlock.store (false, std::memory_order_relaxed);
            }

            if (willUnderrun)
            {
                int fade = juce::jmin (fadeSamples, numSamplesToRead);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.applyGainRamp (ch, numSamplesToRead - fade, fade, 1.0f, 0.0f);
                hadUnderrunLastBlock.store (true, std::memory_order_relaxed);
            }
        }
    }
    else
    {
        hadUnderrunLastBlock.store (true, std::memory_order_relaxed);
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
