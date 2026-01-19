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

#pragma once
#include <JuceHeader.h>
#include <functional>

class AudioResilienceManager : private juce::Timer,
                               private juce::ChangeListener
{
public:
    using ConfigRestoredCallback = std::function<void()>;

    AudioResilienceManager(juce::AudioDeviceManager& dm, ConfigRestoredCallback callback = nullptr)
        : deviceManager(dm), onConfigRestored(callback)
    {
        deviceManager.addChangeListener(this);

        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            lastDeviceName = device->getName();
            lastInputChannels = device->getActiveInputChannels();
            lastOutputChannels = device->getActiveOutputChannels();
        }

        lastTimeCheck = juce::Time::getMillisecondCounter();
        startTimer(5000);
    }

    ~AudioResilienceManager() override
    {
        deviceManager.removeChangeListener(this);
    }

    // callback when an audio config change occurs
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        if (!isWarmedUp || isRestarting) return;

        // call doResilience
        doResilience();

        // if device name or channels changed, call the lambda function to reload the current preset
        auto* currentDevice = deviceManager.getCurrentAudioDevice();
        juce::String currentName = (currentDevice != nullptr) ? currentDevice->getName() : juce::String();
        juce::BigInteger currentInputChannels = (currentDevice != nullptr) ? currentDevice->getActiveInputChannels() : juce::BigInteger();
        juce::BigInteger currentOutputChannels = (currentDevice != nullptr) ? currentDevice->getActiveOutputChannels() : juce::BigInteger();
        if (currentName != lastDeviceName || currentInputChannels != lastInputChannels || currentOutputChannels != lastOutputChannels)
        {
            lastDeviceName = currentName;
            lastInputChannels = currentInputChannels;
            lastOutputChannels = currentOutputChannels;
            if (onConfigRestored != nullptr)
                onConfigRestored();
        }
    }

    // callback every 1 sec (except during initial 5 sec warmup)
    void timerCallback() override
    {
        if (!isWarmedUp)
        {
            isWarmedUp = true;
            startTimer(1000);
            return;
        }

        // call doResilience
        doResilience();
    }

    void doResilience()
    {
        // Skip resilience checks while a modal component is active (e.g. to allow user to change target device in audio settings)
        if (juce::Component::getCurrentlyModalComponent() != nullptr)
            return;

        uint32 now = juce::Time::getMillisecondCounter();
        bool wokeFromSleep = (now > lastTimeCheck + 4000);
        lastTimeCheck = now;

        // try to get saved Xml state; bail if there is no saved state or saved state doesn't specify target device
        auto savedState = getAppProperties().getUserSettings()->getXmlValue ("audioDeviceState");
        if (savedState == nullptr)
            return;
        juce::String savedInputDeviceName = savedState->getStringAttribute("audioInputDeviceName");
        juce::String savedOutputDeviceName = savedState->getStringAttribute("audioOutputDeviceName");
        bool isTargetDeviceSaved = savedInputDeviceName != "" && savedOutputDeviceName != "";
        if (!isTargetDeviceSaved)
            return;

        // if target device is not physically present, null currentDevice and bail
        bool isTargetPhysicallyPresent = isDeviceAvailable(savedInputDeviceName) && isDeviceAvailable(savedOutputDeviceName);
        auto* currentDevice = deviceManager.getCurrentAudioDevice();
        if (!isTargetPhysicallyPresent)
        {
            if (currentDevice != nullptr)
                forceNullDevice();
            return;
        }

        // if we just woke from sleep (and target device is physically present), enforce config then bail
        if (wokeFromSleep)
        {
            enforceConfiguration(savedState.get());
            return;
        }

        // if target device is physically present but not selected in currentSetup, or if it is selected but not playing, enforce config
        juce::AudioDeviceManager::AudioDeviceSetup currentSetup;
        deviceManager.getAudioDeviceSetup(currentSetup);
        bool isTargetSelected = (currentSetup.inputDeviceName == savedInputDeviceName) &&
                                (currentSetup.outputDeviceName == savedOutputDeviceName);
        if (!isTargetSelected || (currentDevice != nullptr && !currentDevice->isPlaying()))
        {
                enforceConfiguration(savedState.get());
        }
    }

private:
    juce::AudioDeviceManager& deviceManager;
    uint32 lastTimeCheck;
    bool isRestarting = false;
    bool isWarmedUp = false;
    juce::String lastDeviceName;
    juce::BigInteger lastInputChannels;
    juce::BigInteger lastOutputChannels;

    bool isDeviceAvailable(const juce::String& name)
    {
        for (auto* type : deviceManager.getAvailableDeviceTypes())
        {
            type->scanForDevices();
            if (type->getDeviceNames(true).contains(name)) return true;
            if (type->getDeviceNames(false).contains(name)) return true;
        }
        return false;
    }

    void enforceConfiguration(XmlElement* savedState)
    {
        if (isRestarting) return;
        isRestarting = true;
        // close and re-initialize device using saved state (without fallback to default device is target is not available)
        deviceManager.closeAudioDevice();
        bool granted = RuntimePermissions::isGranted (RuntimePermissions::recordAudio);
        deviceManager.initialise (granted ? 256 : 0, 256, savedState, false);
        isRestarting = false;
    }

    void forceNullDevice()
    {
        if (isRestarting) return;
        isRestarting = true;
        // close device and clear previous state by using default-constructed setup
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.setAudioDeviceSetup(setup, false);
        isRestarting = false;
    }

    ConfigRestoredCallback onConfigRestored;
};
