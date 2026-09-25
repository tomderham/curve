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

//==============================================================================
/**
    A wrapper around a real AudioIODeviceType (e.g. CoreAudio) that intercepts
    device-list-changed notifications.

    When the active audio device disappears (e.g. USB-C unplug), JUCE's default
    AudioDeviceManager::audioDeviceListChanged() tries to fall back to the system
    default device, which triggers:
      - synchronous MIDIPortDispose XPC calls (~1.2 s)
      - CoreAudio stop() sleep loops (~300 ms)

    This wrapper detects the disappearance first and closes the audio device
    directly via AudioDeviceManager::closeAudioDevice(), then sends a change
    message so the AudioResilienceManager can handle recovery.  The expensive
    JUCE fallback path is never reached.
*/
class ResilientAudioIODeviceType : public juce::AudioIODeviceType,
                                   private juce::AudioIODeviceType::Listener
{
public:
    ResilientAudioIODeviceType (std::unique_ptr<juce::AudioIODeviceType> inner,
                                juce::AudioDeviceManager& dm)
        : juce::AudioIODeviceType (inner->getTypeName()),
          innerType (std::move (inner)),
          deviceManager (dm)
    {
        innerType->addListener (this);
    }

    ~ResilientAudioIODeviceType() override
    {
        innerType->removeListener (this);
    }

    //==============================================================================
    // Delegated AudioIODeviceType interface
    void scanForDevices() override                                          { innerType->scanForDevices(); }
    juce::StringArray getDeviceNames (bool wantInputNames) const override   { return innerType->getDeviceNames (wantInputNames); }
    int getDefaultDeviceIndex (bool forInput) const override                { return innerType->getDefaultDeviceIndex (forInput); }
    int getIndexOfDevice (juce::AudioIODevice* d, bool asInput) const override { return innerType->getIndexOfDevice (d, asInput); }
    bool hasSeparateInputsAndOutputs() const override                       { return innerType->hasSeparateInputsAndOutputs(); }

    juce::AudioIODevice* createDevice (const juce::String& outputDeviceName,
                                       const juce::String& inputDeviceName) override
    {
        return innerType->createDevice (outputDeviceName, inputDeviceName);
    }

private:
    //==============================================================================
    // AudioIODeviceType::Listener — called when the inner type detects hardware changes
    void audioDeviceListChanged() override
    {
        auto* currentDevice = deviceManager.getCurrentAudioDevice();

        if (currentDevice != nullptr)
        {
            // Check whether the active device is still present in the hardware list
            auto currentTypeName  = currentDevice->getTypeName();
            auto currentDeviceName = currentDevice->getName();

            bool deviceStillAvailable = false;

            if (currentTypeName == innerType->getTypeName())
            {
                innerType->scanForDevices();

                for (auto& name : innerType->getDeviceNames (true))
                    if (currentDeviceName == name)
                        { deviceStillAvailable = true; break; }

                if (! deviceStillAvailable)
                    for (auto& name : innerType->getDeviceNames (false))
                        if (currentDeviceName == name)
                            { deviceStillAvailable = true; break; }
            }
            else
            {
                // Active device is a different type (e.g. built-in speakers managed
                // by a different AudioIODeviceType instance) — forward normally.
                deviceStillAvailable = true;
            }

            if (! deviceStillAvailable)
            {
                // The active device has disappeared.  Close it immediately without
                // triggering JUCE's fallback-to-default path, which would synchronously
                // tear down MIDI ports and sleep on CoreAudio IO threads.
                deviceManager.closeAudioDevice();
                deviceManager.sendChangeMessage();
                return;
            }
        }

        // Device is still present, or no device is active — forward normally.
        // JUCE's audioDeviceListChanged() will run, but since the device is still
        // available (or null), it won't trigger the expensive fallback path.
        callDeviceChangeListeners();
    }

    //==============================================================================
    std::unique_ptr<juce::AudioIODeviceType> innerType;
    juce::AudioDeviceManager& deviceManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResilientAudioIODeviceType)
};


//==============================================================================
/**
    A subclass of AudioDeviceManager that registers a ResilientAudioIODeviceType
    wrapper around the CoreAudio device type, preventing the main-thread hang
    on audio device disconnection.
*/
class CurveAudioDeviceManager : public juce::AudioDeviceManager
{
public:
    CurveAudioDeviceManager() = default;

    void createAudioDeviceTypes (juce::OwnedArray<juce::AudioIODeviceType>& types) override
    {
        if (auto* coreAudioType = juce::AudioIODeviceType::createAudioIODeviceType_CoreAudio())
        {
            types.add (new ResilientAudioIODeviceType (
                std::unique_ptr<juce::AudioIODeviceType> (coreAudioType), *this));
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveAudioDeviceManager)
};

