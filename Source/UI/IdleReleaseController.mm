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

#import <CoreAudio/CoreAudio.h>
#import <Foundation/Foundation.h>
#include <Block.h>
#include <algorithm>
#include <unistd.h>
#include <vector>

#include "IdleReleaseController.h"
#include "AudioResilienceManager.h"
#include "../AudioDiagnostics.h"

juce::PropertiesFile* getUserSettings();

//==============================================================================
namespace
{
    std::vector<AudioObjectID> getObjectList (AudioObjectID object, AudioObjectPropertySelector selector,
                                              AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal)
    {
        AudioObjectPropertyAddress address { selector, scope, kAudioObjectPropertyElementMain };
        UInt32 size = 0;

        if (AudioObjectGetPropertyDataSize (object, &address, 0, nullptr, &size) != noErr || size == 0)
            return {};

        std::vector<AudioObjectID> ids (size / sizeof (AudioObjectID));

        if (AudioObjectGetPropertyData (object, &address, 0, nullptr, &size, ids.data()) != noErr)
            return {};

        ids.resize (size / sizeof (AudioObjectID));
        return ids;
    }

    template <typename T>
    T getValue (AudioObjectID object, AudioObjectPropertySelector selector, T defaultValue)
    {
        AudioObjectPropertyAddress address { selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        T value = defaultValue;
        UInt32 size = sizeof (T);
        return AudioObjectGetPropertyData (object, &address, 0, nullptr, &size, &value) == noErr ? value : defaultValue;
    }

    juce::String getName (AudioObjectID object)
    {
        AudioObjectPropertyAddress address { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        CFStringRef name = nullptr;
        UInt32 size = sizeof (name);

        if (AudioObjectGetPropertyData (object, &address, 0, nullptr, &size, &name) != noErr || name == nullptr)
            return {};

        juce::String result = juce::String::fromCFString (name);
        CFRelease (name);
        return result;
    }

    bool contains (const std::vector<AudioObjectID>& ids, AudioObjectID id)
    {
        return std::find (ids.begin(), ids.end(), id) != ids.end();
    }

    struct RunningSomewhereListener
    {
        RunningSomewhereListener (AudioObjectID deviceIn, std::function<void()> onChange)
            : device (deviceIn)
        {
            block = Block_copy (^(UInt32, const AudioObjectPropertyAddress*) { onChange(); });

            if (AudioObjectAddPropertyListenerBlock (device, &address, queue, block) != noErr)
            {
                Block_release (block);
                block = nullptr;
            }
        }

        ~RunningSomewhereListener()
        {
            if (block != nullptr)
            {
                AudioObjectRemovePropertyListenerBlock (device, &address, queue, block);
                Block_release (block);
            }
        }

        AudioObjectID device;
        AudioObjectPropertyAddress address { kAudioDevicePropertyDeviceIsRunningSomewhere,
                                             kAudioObjectPropertyScopeGlobal,
                                             kAudioObjectPropertyElementMain };
        dispatch_queue_t queue = dispatch_get_global_queue (QOS_CLASS_USER_INITIATED, 0);
        AudioObjectPropertyListenerBlock block = nullptr;
    };
}

namespace IdleReleaseCoreAudio
{
    juce::uint32 findDeviceID (const juce::String& deviceName)
    {
        if (deviceName.isEmpty())
            return 0;

        for (auto device : getObjectList (kAudioObjectSystemObject, kAudioHardwarePropertyDevices))
            if (getName (device) == deviceName)
                return device;

        return 0;
    }

    bool isAnotherProcessUsingDevice (juce::uint32 deviceID)
    {
        const auto ownPid = getpid();

        for (auto process : getObjectList (kAudioObjectSystemObject, kAudioHardwarePropertyProcessObjectList))
        {
            if (getValue<pid_t> (process, kAudioProcessPropertyPID, 0) == ownPid)
                continue;

            const bool runningOutput = getValue<UInt32> (process, kAudioProcessPropertyIsRunningOutput, 0) != 0;
            const bool runningInput  = getValue<UInt32> (process, kAudioProcessPropertyIsRunningInput, 0) != 0;

            if (runningOutput && contains (getObjectList (process, kAudioProcessPropertyDevices, kAudioObjectPropertyScopeOutput), deviceID))
                return true;

            if (runningInput && contains (getObjectList (process, kAudioProcessPropertyDevices, kAudioObjectPropertyScopeInput), deviceID))
                return true;
        }

        return false;
    }

    bool isVirtualDevice (juce::uint32 deviceID)
    {
        return getValue<UInt32> (deviceID, kAudioDevicePropertyTransportType, 0) == kAudioDeviceTransportTypeVirtual;
    }

    std::shared_ptr<void> addRunningSomewhereListener (juce::uint32 deviceID, std::function<void()> onChange)
    {
        if (deviceID == 0)
            return {};

        return std::make_shared<RunningSomewhereListener> (deviceID, std::move (onChange));
    }
}

//==============================================================================
IdleReleaseController::IdleReleaseController (juce::AudioDeviceManager& dm, BusyPredicate isHostBusyIn, OutputHold holdOutputIn)
    : deviceManager (dm), isHostBusy (std::move (isHostBusyIn)), holdOutput (std::move (holdOutputIn))
{
    IdleAudioActivity::markActiveNow();
    settingsChanged();
    startTimer (1000);
}

IdleReleaseController::~IdleReleaseController()
{
    isAlive->store (false, std::memory_order_release);
    stopTimer();
    wakeListeners.clear();
}

bool IdleReleaseController::isEnabledInSettings()
{
    if (auto* settings = getUserSettings())
        return settings->getBoolValue ("idleReleaseEnabled", false);

    return false;
}

void IdleReleaseController::setEnabledInSettings (bool shouldBeEnabled)
{
    if (auto* settings = getUserSettings())
    {
        settings->setValue ("idleReleaseEnabled", shouldBeEnabled);
        settings->saveIfNeeded();
    }

    if (auto* controller = getIdleReleaseController())
        controller->settingsChanged();
}

void IdleReleaseController::settingsChanged()
{
    enabled = isEnabledInSettings();

    // Restart the silence countdown so enabling the setting never releases instantly
    IdleAudioActivity::markActiveNow();
    lastBlockingReason = nullptr;

    CURVE_AUDIO_LOG ("[IdleRelease] %s (timeout %d s)", enabled ? "Enabled" : "Disabled", idleSeconds);

    if (! enabled && released)
        resume ("setting disabled");
}

void IdleReleaseController::resumeIfReleased()
{
    if (released)
        resume ("requested");
}

const char* IdleReleaseController::getReasonNotToRelease (juce::StringArray& wakeDevicesOut) const
{
    wakeDevicesOut.clear();

    auto* rm = getResilienceManager();

    if (rm == nullptr)
        return "no resilience manager";

    if (rm->isSuspendedOrRestarting())
        return "dialog open, preset loading or device restarting";

    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr || ! device->isPlaying())
        return "audio device not running";

    if (isHostBusy != nullptr && isHostBusy())
        return "a recorder node is recording";

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup (setup);

    // Another app starting to use the output interface wakes Curve (this covers the Interface
    // Loopback tap, and a hardware loopback on the interface such as RME TotalMix)
    wakeDevicesOut.add (setup.outputDeviceName);

    if (! device->getActiveInputChannels().isZero()
        && setup.inputDeviceName.isNotEmpty()
        && setup.inputDeviceName != setup.outputDeviceName)
    {
        // A separate input can only wake Curve if other apps feed it, i.e. a virtual device such as
        // BlackHole. A separate microphone or line input couldn't, so never release for those.
        // (Inputs on the output interface itself are allowed: a live microphone routed by the preset
        // reaches the output, so the silence countdown keeps the interface open.)
        const auto inputID = IdleReleaseCoreAudio::findDeviceID (setup.inputDeviceName);

        if (inputID == 0 || ! IdleReleaseCoreAudio::isVirtualDevice (inputID))
            return "audio input is a separate physical device, which couldn't wake Curve";

        wakeDevicesOut.add (setup.inputDeviceName);
    }

    for (auto& name : wakeDevicesOut)
    {
        const auto deviceID = IdleReleaseCoreAudio::findDeviceID (name);

        // Without a device to listen on we couldn't wake up again
        if (deviceID == 0)
            return "audio device not found for wake-up monitoring";

        // Another app (e.g. one routed through a loopback, or paused but still holding the device)
        // keeps it busy anyway; releasing would only cause a release/wake loop
        if (IdleReleaseCoreAudio::isAnotherProcessUsingDevice (deviceID))
            return "another app is using the audio device";
    }

    return nullptr;
}

void IdleReleaseController::timerCallback()
{
    if (released)
    {
        // Fallback for missed notifications
        for (auto deviceID : armedDeviceIDs)
        {
            if (deviceID != 0 && IdleReleaseCoreAudio::isAnotherProcessUsingDevice (deviceID))
            {
                resume ("another app is using the audio device");
                return;
            }
        }

        // Re-arm if a device was unplugged/replugged (its ID changes)
        for (int i = 0; i < wakeDeviceNames.size(); ++i)
        {
            if (IdleReleaseCoreAudio::findDeviceID (wakeDeviceNames[i]) != armedDeviceIDs[(size_t) i])
            {
                armWakeListeners();
                break;
            }
        }

        return;
    }

    if (! enabled)
        return;

    const auto silentMs = juce::Time::getMillisecondCounter()
                        - IdleAudioActivity::lastNonSilentMs.load (std::memory_order_relaxed);

    juce::StringArray devicesToWatch;
    const char* reason = silentMs < (juce::uint32) idleSeconds * 1000u ? "waiting for the silence timeout"
                                                                       : getReasonNotToRelease (devicesToWatch);

    // Log only when the blocking reason changes, to keep the diagnostics log readable
    if (reason != lastBlockingReason)
    {
        lastBlockingReason = reason;

        if (reason != nullptr)
            CURVE_AUDIO_LOG ("[IdleRelease] Not releasing: %s", reason);
    }

    if (reason == nullptr)
        release (devicesToWatch);
}

void IdleReleaseController::release (const juce::StringArray& devicesToWatch)
{
    auto* rm = getResilienceManager();

    if (rm == nullptr)
        return;

    wakeDeviceNames = devicesToWatch;
    rm->enterIdleRelease();
    released = true;
    armWakeListeners();

    CURVE_AUDIO_LOG ("[IdleRelease] Released after %d s of silence; waking on: %s",
                     idleSeconds, wakeDeviceNames.joinIntoString (", ").toRawUTF8());
}

void IdleReleaseController::resume (const char* reason)
{
    juce::ignoreUnused (reason); // only used when audio diagnostics logging is enabled
    wakeListeners.clear();
    armedDeviceIDs.clear();
    released = false;
    lastBlockingReason = nullptr;

    // Give the waking app's audio time to arrive before the silence countdown can run out again
    IdleAudioActivity::markActiveNow();

    CURVE_AUDIO_LOG ("[IdleRelease] Resuming (%s)", reason);

    if (holdOutput != nullptr)
        holdOutput (true);

    if (auto* rm = getResilienceManager())
        rm->exitIdleRelease();

    // The device and tap are running again (or recovery has taken over): fade in. Always called,
    // so a device opened later by recovery doesn't stay silent.
    if (holdOutput != nullptr)
        holdOutput (false);
}

void IdleReleaseController::armWakeListeners()
{
    wakeListeners.clear();
    armedDeviceIDs.clear();

    auto aliveToken = isAlive;

    for (auto& name : wakeDeviceNames)
    {
        const auto deviceID = IdleReleaseCoreAudio::findDeviceID (name);
        armedDeviceIDs.push_back (deviceID);

        wakeListeners.push_back (IdleReleaseCoreAudio::addRunningSomewhereListener (deviceID, [this, aliveToken, deviceID]
        {
            // Runs on a CoreAudio queue. Our own device closing also changes this property,
            // so only wake when another process is actually using the device.
            if (! IdleReleaseCoreAudio::isAnotherProcessUsingDevice (deviceID))
                return;

            juce::MessageManager::callAsync ([this, aliveToken]
            {
                if (aliveToken->load (std::memory_order_acquire) && released)
                    resume ("another app started using the audio device");
            });
        }));
    }
}
