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

juce::PropertiesFile* getUserSettings();

#if JUCE_MAC
#include "../Plugins/OutputInterfaceLoopbackNode.h"
#include "SystemSleepManager.h"
struct MacOSSleepWakeNotifierBase
{
    virtual ~MacOSSleepWakeNotifierBase() = default;
};
std::unique_ptr<MacOSSleepWakeNotifierBase> createMacOSSleepWakeNotifier (std::function<void(bool)> callback);
#endif

class AudioResilienceManager : private juce::MultiTimer,
                               private juce::ChangeListener
{
public:
    using ConfigRestoredCallback = std::function<void()>;

    enum TimerIds
    {
        timerPeriodicHealthCheck = 1,
        timerDebouncedResilience = 2,
        timerWakeRecovery        = 3
    };

    AudioResilienceManager(juce::AudioDeviceManager& dm, ConfigRestoredCallback callback = nullptr)
        : deviceManager(dm), onConfigRestored(callback)
    {
        deviceManager.addChangeListener(this);

        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            lastDeviceName = device->getName();
            lastInputChannels = device->getActiveInputChannels();
            lastOutputChannels = device->getActiveOutputChannels();
            lastSampleRate = device->getCurrentSampleRate();
            lastBufferSize = device->getCurrentBufferSizeSamples();
        }

        lastTimeCheckMonotonic = juce::Time::getMillisecondCounter();
        lastTimeCheckWallClock = juce::Time::getCurrentTime().toMilliseconds();
        updateTargetSettings();

       #if JUCE_MAC
        auto aliveToken = isAlive;
        sleepWakeNotifier = createMacOSSleepWakeNotifier ([this, aliveToken] (bool isWake)
        {
            if (! aliveToken->load (std::memory_order_acquire))
                return;

            if (isWake)
            {
                consecutiveEnforceFailures = 0;
                wokeFromSleepFlag = true;
                wakeRecoveryStartTimeMs = juce::Time::getMillisecondCounter();
                setSuspended (false);
                SystemSleepManager::applyCurrentSetting();
                doResilience();
                // Schedule wake recovery checks to allow USB/Thunderbolt/Bluetooth peripherals time to enumerate
                startTimer (timerWakeRecovery, 350);
                startTimer (timerDebouncedResilience, 200);
            }
            else
            {
                stopTimer (timerWakeRecovery);
                stopTimer (timerDebouncedResilience);
                setSuspended (true);
                OutputInterfaceLoopbackNode::teardownTap();
            }
        });
       #endif

        startTimer (timerPeriodicHealthCheck, 5000);
    }

    ~AudioResilienceManager() override
    {
        if (isAlive)
            isAlive->store (false, std::memory_order_release);
        #if JUCE_MAC
        sleepWakeNotifier.reset();
        #endif
        stopTimer (timerPeriodicHealthCheck);
        stopTimer (timerDebouncedResilience);
        stopTimer (timerWakeRecovery);
        deviceManager.removeChangeListener(this);
    }

    void setSuspended (bool suspend)
    {
        isSuspended = suspend;
        if (!isSuspended)
        {
            lastTimeCheckMonotonic = juce::Time::getMillisecondCounter();
            lastTimeCheckWallClock = juce::Time::getCurrentTime().toMilliseconds();
        }
    }

    void setAlwaysEnforceSampleRate (bool enforce)
    {
        alwaysEnforceSampleRate = enforce;

        if (alwaysEnforceSampleRate)
        {
            if (auto* currentDevice = deviceManager.getCurrentAudioDevice())
            {
                targetSampleRate = currentDevice->getCurrentSampleRate();
                if (cachedAudioState != nullptr && targetSampleRate > 0.0)
                {
                    cachedAudioState->setAttribute ("audioDeviceRate", targetSampleRate);
                    if (auto* settings = getUserSettings())
                    {
                        settings->setValue ("audioDeviceState", cachedAudioState.get());
                        settings->saveIfNeeded();
                    }
                }
            }
        }
    }

    bool isAlwaysEnforcingSampleRate() const
    {
        return alwaysEnforceSampleRate;
    }

    void updateTargetSettings()
    {
        auto* settings = getUserSettings();
        if (settings != nullptr)
            alwaysEnforceSampleRate = settings->getBoolValue ("enforceSampleRate", false);

        auto savedState = (settings != nullptr) ? settings->getXmlValue ("audioDeviceState") : nullptr;
        if (savedState != nullptr)
        {
            targetInputDeviceName = savedState->getStringAttribute ("audioInputDeviceName");
            targetOutputDeviceName = savedState->getStringAttribute ("audioOutputDeviceName");
            targetSampleRate = savedState->getDoubleAttribute ("audioDeviceRate", savedState->getDoubleAttribute ("sampleRate", 0.0));
            targetBufferSize = savedState->getIntAttribute ("audioDeviceBufferSize", savedState->getIntAttribute ("bufferSize", 0));
            cachedAudioState = std::move (savedState);
        }
        else
        {
            // Preserve the configured target device when an audio interface is disconnected.
            if (targetOutputDeviceName.isEmpty())
            {
                if (auto* currentDevice = deviceManager.getCurrentAudioDevice())
                {
                    targetOutputDeviceName = currentDevice->getName();
                    targetInputDeviceName = {};
                    targetSampleRate = currentDevice->getCurrentSampleRate();
                    targetBufferSize = currentDevice->getCurrentBufferSizeSamples();
                    cachedAudioState = deviceManager.createStateXml();
                }
                else
                {
                    targetInputDeviceName = "";
                    targetOutputDeviceName = "";
                    targetSampleRate = 0.0;
                    targetBufferSize = 0;
                    cachedAudioState = nullptr;
                }
            }
        }

        if (! alwaysEnforceSampleRate)
        {
            if (auto* currentDevice = deviceManager.getCurrentAudioDevice())
            {
                targetSampleRate = currentDevice->getCurrentSampleRate();
                if (cachedAudioState != nullptr && targetSampleRate > 0.0)
                {
                    cachedAudioState->setAttribute ("audioDeviceRate", targetSampleRate);
                    if (auto* userSettings = getUserSettings())
                    {
                        userSettings->setValue ("audioDeviceState", cachedAudioState.get());
                        userSettings->saveIfNeeded();
                    }
                }
            }
        }
    }

    // callback when an audio config change occurs
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        if (isRestarting || isSuspended) return;

        // Update baseline device properties immediately
        auto* currentDevice = deviceManager.getCurrentAudioDevice();
        juce::String currentName = (currentDevice != nullptr) ? currentDevice->getName() : juce::String();
        juce::BigInteger currentInputChannels = (currentDevice != nullptr) ? currentDevice->getActiveInputChannels() : juce::BigInteger();
        juce::BigInteger currentOutputChannels = (currentDevice != nullptr) ? currentDevice->getActiveOutputChannels() : juce::BigInteger();
        double currentSampleRate = (currentDevice != nullptr) ? currentDevice->getCurrentSampleRate() : 0.0;
        int currentBufferSize = (currentDevice != nullptr) ? currentDevice->getCurrentBufferSizeSamples() : 0;

        bool isDeviceActive = (currentDevice != nullptr && currentDevice->isPlaying() && currentName.isNotEmpty());
        bool devicePropertiesChanged = (currentName != lastDeviceName 
                                        || currentInputChannels != lastInputChannels 
                                        || currentOutputChannels != lastOutputChannels
                                        || std::abs (currentSampleRate - lastSampleRate) > 0.001
                                        || currentBufferSize != lastBufferSize);

        if (devicePropertiesChanged)
        {
            lastDeviceName = currentName;
            lastInputChannels = currentInputChannels;
            lastOutputChannels = currentOutputChannels;
            lastSampleRate = currentSampleRate;
            lastBufferSize = currentBufferSize;
        }

        if (!isWarmedUp) return;

        // Hardware topology changed (e.g. USB plug/unplug): reset backoff so we immediately attempt configuration
        consecutiveEnforceFailures = 0;

        // Update cached target settings
        updateTargetSettings();

        // If target output device was abruptly disconnected, tear down immediately to avoid zombie tap
        scanDevices();
        if (! isDeviceAvailable (targetOutputDeviceName, false, false))
        {
            stopTimer (timerDebouncedResilience);
            forceNullDevice();
            return;
        }

        // Debounce incoming changes (200ms) to coalesce rapid property cascades
        startTimer (timerDebouncedResilience, 200);

        // Notify listeners asynchronously when device properties change
        if (devicePropertiesChanged && isDeviceActive && onConfigRestored != nullptr)
        {
            juce::MessageManager::callAsync ([callback = onConfigRestored] {
                if (callback)
                    callback();
            });
        }
    }

    // Timer callback runs every 5s for periodic health checks
    void timerCallback (int timerId) override
    {
        if (isSuspended) return;

        if (timerId == timerWakeRecovery)
        {
            scanDevices();
            bool isPresent = isDeviceAvailable (targetOutputDeviceName, false, false)
                          && isDeviceAvailable (targetInputDeviceName, true, false);

            auto now = juce::Time::getMillisecondCounter();
            bool timedOut = (now - wakeRecoveryStartTimeMs) > 4000;

            if (isPresent || timedOut)
            {
                stopTimer (timerWakeRecovery);
                stopTimer (timerDebouncedResilience);
                doResilience();
            }
            else
            {
                // Peripheral not yet enumerated; retry in 500ms
                startTimer (timerWakeRecovery, 500);
            }
            return;
        }

        if (timerId == timerDebouncedResilience)
        {
            stopTimer (timerDebouncedResilience);

            // If wake recovery is active and device not yet present, let wake recovery continue
            if (wokeFromSleepFlag && isTimerRunning (timerWakeRecovery))
            {
                scanDevices();
                if (! (isDeviceAvailable (targetOutputDeviceName, false, false)
                       && isDeviceAvailable (targetInputDeviceName, true, false)))
                {
                    return;
                }
                stopTimer (timerWakeRecovery);
            }

            doResilience();
            return;
        }

        if (timerId == timerPeriodicHealthCheck)
        {
            if (!isWarmedUp)
            {
                isWarmedUp = true;
                startTimer (timerPeriodicHealthCheck, 2500);
                return;
            }

            // Skip periodic check if a debounced update or wake recovery is pending
            if (isTimerRunning (timerDebouncedResilience) || isTimerRunning (timerWakeRecovery))
                return;

            doResilience();
            return;
        }
    }

    void doResilience()
    {
        if (isSuspended)
            return;

        // Defer resilience checks while audio configuration dialogs are modal
        for (int i = 0; i < juce::Component::getNumCurrentlyModalComponents(); ++i)
        {
            if (auto* modal = juce::Component::getCurrentlyModalComponent (i))
            {
                if (containsAudioDeviceSelector (modal))
                    return;
            }
        }

        juce::uint32 nowMonotonic = juce::Time::getMillisecondCounter();
        juce::int64 nowWallClock = juce::Time::getCurrentTime().toMilliseconds();

        bool wokeFromSleep = wokeFromSleepFlag;
        wokeFromSleepFlag = false;

       #if ! JUCE_MAC
        juce::uint32 elapsedMonotonic = nowMonotonic - lastTimeCheckMonotonic;
        juce::int64 elapsedWallClock = nowWallClock - lastTimeCheckWallClock;
        if (! wokeFromSleep)
            wokeFromSleep = (elapsedWallClock > (juce::int64) elapsedMonotonic + 4000);
       #endif

        lastTimeCheckMonotonic = nowMonotonic;
        lastTimeCheckWallClock = nowWallClock;

        if (wokeFromSleep)
            consecutiveEnforceFailures = 0;

        // Use cached settings to avoid parsing XML on every tick
        bool isTargetDeviceSaved = targetOutputDeviceName.isNotEmpty();
        if (!isTargetDeviceSaved || cachedAudioState == nullptr)
            return;

        // Fast path: bypass checks when target device is already active, playing, and matching configured parameters
        auto* currentDevice = deviceManager.getCurrentAudioDevice();
        if (!wokeFromSleep && isConfigurationMatching (currentDevice))
        {
            disconnectedCheckCount = 0;
            consecutiveEnforceFailures = 0;

           #if JUCE_MAC
            bool autoSync = true;
            if (auto* settings = getUserSettings())
                autoSync = settings->getBoolValue ("autoSyncSystemOutput", true);

            if (autoSync)
                OutputInterfaceLoopbackNode::syncSystemOutputDevice (targetOutputDeviceName);

            if (OutputInterfaceLoopbackNode::isAnyTapActiveInGraph())
            {
                bool healthy = OutputInterfaceLoopbackNode::ensureTapHealthy (targetOutputDeviceName,
                                                                              currentDevice->getCurrentSampleRate(),
                                                                              currentDevice->getCurrentBufferSizeSamples());
                if (! healthy)
                    startTimer (timerPeriodicHealthCheck, 1000);
                else
                    startTimer (timerPeriodicHealthCheck, 2500);
            }
           #endif

            return;
        }

        // Rescan hardware devices to discover newly connected interfaces
        scanDevices();

        bool isTargetPhysicallyPresent = isDeviceAvailable(targetOutputDeviceName, false, false)
                                      && isDeviceAvailable(targetInputDeviceName, true, false);
        if (!isTargetPhysicallyPresent)
        {
            // Allow USB/Thunderbolt interfaces time to enumerate immediately on system wake
            if (wokeFromSleep)
            {
                wokeFromSleepFlag = true;
                return;
            }

            if (currentDevice != nullptr)
                forceNullDevice();
            return;
        }

        disconnectedCheckCount = 0;

        // Exponential backoff between configuration retries to prevent CPU spinning if device is unavailable
        if (consecutiveEnforceFailures > 0 && !wokeFromSleep)
        {
            juce::uint32 backoffMs = juce::jmin (30000u, 1000u * (1u << juce::jmin (5, consecutiveEnforceFailures)));
            if (nowMonotonic - lastEnforceFailureTime < backoffMs)
                return;
        }

        // Apply configuration immediately on wake
        if (wokeFromSleep)
        {
            enforceConfiguration (cachedAudioState.get(), true /* forceTapReinit on wake */);
            return;
        }

        // Apply target configuration if not currently matching configured parameters
        if (! isConfigurationMatching (currentDevice))
        {
            enforceConfiguration (cachedAudioState.get());
        }
    }

    static bool containsAudioDeviceSelector (juce::Component* comp)
    {
        if (comp == nullptr)
            return false;
        if (dynamic_cast<juce::AudioDeviceSelectorComponent*> (comp) != nullptr)
            return true;
        for (int i = 0; i < comp->getNumChildComponents(); ++i)
            if (containsAudioDeviceSelector (comp->getChildComponent (i)))
                return true;
        return false;
    }

    bool isConfigurationMatching (juce::AudioIODevice* currentDevice) const
    {
        if (currentDevice == nullptr || ! currentDevice->isPlaying())
            return false;

        juce::AudioDeviceManager::AudioDeviceSetup currentSetup;
        deviceManager.getAudioDeviceSetup (currentSetup);

        bool isOutputMatching = (currentSetup.outputDeviceName == targetOutputDeviceName);
        bool isInputMatching  = (targetInputDeviceName.isEmpty() || currentSetup.inputDeviceName == targetInputDeviceName);
        bool isBufferMatching = (targetBufferSize <= 0 || currentDevice->getCurrentBufferSizeSamples() == targetBufferSize);

        if (! isOutputMatching || ! isInputMatching || ! isBufferMatching)
            return false;

        if (alwaysEnforceSampleRate)
        {
            bool isRateMatching = (targetSampleRate <= 0.0 || std::abs (currentDevice->getCurrentSampleRate() - targetSampleRate) < 1.0);
            if (! isRateMatching)
                return false;
        }

        return true;
    }

private:
    juce::AudioDeviceManager& deviceManager;
    juce::uint32 lastTimeCheckMonotonic;
    juce::int64 lastTimeCheckWallClock;
    bool isRestarting = false;
    bool isWarmedUp = false;
    bool isSuspended = false;
    bool wokeFromSleepFlag = false;
    juce::uint32 wakeRecoveryStartTimeMs = 0;
    std::shared_ptr<std::atomic<bool>> isAlive = std::make_shared<std::atomic<bool>> (true);
   #if JUCE_MAC
    std::unique_ptr<MacOSSleepWakeNotifierBase> sleepWakeNotifier;
   #endif
    int disconnectedCheckCount = 0;
    int consecutiveEnforceFailures = 0;
    juce::uint32 lastEnforceFailureTime = 0;
    juce::String lastDeviceName;
    juce::BigInteger lastInputChannels;
    juce::BigInteger lastOutputChannels;
    double lastSampleRate = 0.0;
    int lastBufferSize = 0;

    juce::String targetInputDeviceName;
    juce::String targetOutputDeviceName;
    double targetSampleRate = 0.0;
    int targetBufferSize = 0;
    bool alwaysEnforceSampleRate = false;
    std::unique_ptr<juce::XmlElement> cachedAudioState;

    void scanDevices()
    {
        for (auto* type : deviceManager.getAvailableDeviceTypes())
            type->scanForDevices();
    }

    bool isDeviceAvailable(const juce::String& name, bool isInput, bool forceScan)
    {
        if (name.isEmpty())
            return true; // No device required/specified for input

        for (auto* type : deviceManager.getAvailableDeviceTypes())
        {
            if (forceScan)
                type->scanForDevices();

            if (type->getDeviceNames(isInput).contains(name)) return true;
        }
        return false;
    }

    void enforceConfiguration(juce::XmlElement* savedState, bool forceTapReinit = false)
    {
        if (isRestarting) return;
        isRestarting = true;
        // Re-initialize device using saved configuration without default fallback
        deviceManager.closeAudioDevice();
        int numInputs = targetInputDeviceName.isNotEmpty() ? 256 : 0;

        if (savedState != nullptr)
        {
            if (alwaysEnforceSampleRate && targetSampleRate > 0.0)
                savedState->setAttribute ("audioDeviceRate", targetSampleRate);
            else if (! alwaysEnforceSampleRate)
                savedState->removeAttribute ("audioDeviceRate");

            if (targetBufferSize > 0)
                savedState->setAttribute ("audioDeviceBufferSize", targetBufferSize);
        }

        auto error = deviceManager.initialise (numInputs, 256, savedState, false);
        isRestarting = false;

        auto* currentDevice = deviceManager.getCurrentAudioDevice();
        if (currentDevice != nullptr && currentDevice->isPlaying() && error.isEmpty())
        {
            consecutiveEnforceFailures = 0;
            lastDeviceName = currentDevice->getName();
            lastInputChannels = currentDevice->getActiveInputChannels();
            lastOutputChannels = currentDevice->getActiveOutputChannels();
            lastSampleRate = currentDevice->getCurrentSampleRate();
            lastBufferSize = currentDevice->getCurrentBufferSizeSamples();

            // Adapt targets to actual hardware capabilities to prevent repeated enforcement cycles
            targetSampleRate = lastSampleRate;
            targetBufferSize = lastBufferSize;

           #if JUCE_MAC
            if (OutputInterfaceLoopbackNode::isAnyTapActiveInGraph())
            {
                OutputInterfaceLoopbackNode::ensureTapHealthy (targetOutputDeviceName,
                                                               currentDevice->getCurrentSampleRate(),
                                                               currentDevice->getCurrentBufferSizeSamples(),
                                                               forceTapReinit);
            }
           #endif

            if (onConfigRestored != nullptr)
            {
                // Defer asynchronously to ensure CoreAudio device stop/restart notifications complete first
                juce::MessageManager::callAsync ([callback = onConfigRestored] {
                    if (callback)
                        callback();
                });
            }
        }
        else
        {
            consecutiveEnforceFailures++;
            lastEnforceFailureTime = juce::Time::getMillisecondCounter();
        }
    }

    void forceNullDevice()
    {
        if (isRestarting) return;
        isRestarting = true;
        // Close audio device to maintain silence when target hardware is disconnected
        deviceManager.closeAudioDevice();
       #if JUCE_MAC
        OutputInterfaceLoopbackNode::teardownTap();
       #endif
        isRestarting = false;
    }

    ConfigRestoredCallback onConfigRestored;
};

AudioResilienceManager* getResilienceManager();
