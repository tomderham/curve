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
#include <map>
#include <vector>

juce::PropertiesFile* getUserSettings();

#include "../Plugins/OutputInterfaceLoopbackNode.h"
#include "SystemSleepManager.h"
struct MacOSSleepWakeNotifierBase
{
    virtual ~MacOSSleepWakeNotifierBase() = default;
};
std::unique_ptr<MacOSSleepWakeNotifierBase> createMacOSSleepWakeNotifier (std::function<void(bool)> callback);

/** An output device as JUCE's CoreAudio device list names it (with "Name (2)" for duplicates),
    and its CoreAudio UID, which stays the same when devices are renamed or reconnected. */
struct MacOSOutputDevice
{
    juce::String name, uid;
    bool isDefault = false; // macOS's current output
};
std::vector<MacOSOutputDevice> getMacOSOutputDevices();

/** macOS's current output device, named as JUCE's CoreAudio device list names it. Empty if unknown. */
juce::String getMacOSDefaultOutputDeviceName();

/** The CoreAudio UID of the output device JUCE calls juceDeviceName. Empty if it isn't connected. */
juce::String getMacOSOutputDeviceUID (const juce::String& juceDeviceName);

/** See DefaultPresets.h. */
void loadDefaultPresetForOutputDevice (const juce::String& juceDeviceName);

struct MacOSDefaultOutputChangeNotifierBase
{
    virtual ~MacOSDefaultOutputChangeNotifierBase() = default;
};
/** Calls back on the message thread whenever macOS's output device changes. */
std::unique_ptr<MacOSDefaultOutputChangeNotifierBase> createMacOSDefaultOutputChangeNotifier (std::function<void()> callback);

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

        defaultOutputNotifier = createMacOSDefaultOutputChangeNotifier ([this, aliveToken]
        {
            if (! aliveToken->load (std::memory_order_acquire))
                return;

            // Follow mode: switch soon after macOS does, rather than at the next health check
            if (isFollowingSystemOutput() && isWarmedUp && ! isSuspended && ! isIdleReleased)
                startTimer (timerDebouncedResilience, 200);
        });

        startTimer (timerPeriodicHealthCheck, 5000);
    }

    //==============================================================================
    // Follow mode ("Use output interface selected in macOS"): Curve's output device is whatever
    // macOS outputs to. The followed device is written into audioDeviceState like any other choice,
    // so turning follow mode off keeps the current device.
    static bool isFollowingSystemOutput()
    {
        if (auto* settings = getUserSettings())
            return settings->getBoolValue ("followSystemOutput", false);
        return false;
    }

    /** Whether "Force macOS system output to this interface" applies: never while following, since macOS's output
        is already Curve's output. */
    static bool shouldForceSystemOutput()
    {
        if (isFollowingSystemOutput())
            return false;
        if (auto* settings = getUserSettings())
            return settings->getBoolValue ("autoSyncSystemOutput", true);
        return true;
    }

    /** Sample rate, buffer size and active outputs last used on each output device, so following
        macOS from a 96 kHz interface to 44.1 kHz speakers and back returns the interface to 96 kHz
        and the outputs that were enabled on it. 0 / empty for a device Curve hasn't used. */
    struct DeviceSettings
    {
        double sampleRate = 0.0;
        int bufferSize = 0;
        juce::BigInteger outputChannels;

        bool matches (const DeviceSettings& other) const
        {
            return std::abs (sampleRate - other.sampleRate) < 0.001
                && bufferSize == other.bufferSize
                && outputChannels == other.outputChannels;
        }

        static DeviceSettings of (juce::AudioIODevice& device)
        {
            return { device.getCurrentSampleRate(), device.getCurrentBufferSizeSamples(), device.getActiveOutputChannels() };
        }
    };

    static std::map<juce::String, DeviceSettings> getAllRememberedDeviceSettings()
    {
        std::map<juce::String, DeviceSettings> result;
        if (auto* settings = getUserSettings())
            if (auto xml = settings->getXmlValue ("outputDeviceSettings"))
                for (auto* e : xml->getChildWithTagNameIterator ("DEVICE"))
                {
                    DeviceSettings d { e->getDoubleAttribute ("rate"), e->getIntAttribute ("bufferSize"), {} };
                    d.outputChannels.parseString (e->getStringAttribute ("outputChannels"), 2);
                    result[e->getStringAttribute ("name")] = d;
                }
        return result;
    }

    static DeviceSettings getRememberedDeviceSettings (const juce::String& deviceName)
    {
        auto all = getAllRememberedDeviceSettings();
        auto it = all.find (deviceName);
        return it != all.end() ? it->second : DeviceSettings {};
    }

    static void rememberDeviceSettings (juce::AudioIODevice* device)
    {
        if (device == nullptr || ! device->isPlaying() || device->getName().isEmpty())
            return;

        const auto current = DeviceSettings::of (*device);
        if (getRememberedDeviceSettings (device->getName()).matches (current))
            return;

        auto* settings = getUserSettings();
        if (settings == nullptr)
            return;

        auto xml = settings->getXmlValue ("outputDeviceSettings");
        if (xml == nullptr)
            xml = std::make_unique<juce::XmlElement> ("OUTPUTDEVICESETTINGS");

        auto* entry = xml->getChildByAttribute ("name", device->getName());
        if (entry == nullptr)
        {
            entry = xml->createNewChildElement ("DEVICE");
            entry->setAttribute ("name", device->getName());
        }

        entry->setAttribute ("rate", current.sampleRate);
        entry->setAttribute ("bufferSize", current.bufferSize);
        entry->setAttribute ("outputChannels", current.outputChannels.toString (2));
        settings->setValue ("outputDeviceSettings", xml.get());
        settings->saveIfNeeded();
    }

    /** The device follow mode should use: macOS's output, unless that's one of Curve's own devices. */
    static juce::String getFollowedOutputDeviceName()
    {
        auto name = getMacOSDefaultOutputDeviceName();
        if (name.containsIgnoreCase ("Curve Loopback"))
            return {};
        return name;
    }

    ~AudioResilienceManager() override
    {
        if (isAlive)
            isAlive->store (false, std::memory_order_release);
        sleepWakeNotifier.reset();
        defaultOutputNotifier.reset();
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

    bool isSuspendedNow() const
    {
        return isSuspended;
    }

    bool isSuspendedOrRestarting() const
    {
        return isSuspended || isRestarting;
    }

    // Idle release: the interface is closed on purpose (see IdleReleaseController), so none of
    // the recovery paths below may reopen it until exitIdleRelease() is called.
    void enterIdleRelease()
    {
        if (isIdleReleased || isRestarting)
            return;

        stopTimer (timerDebouncedResilience);
        stopTimer (timerWakeRecovery);
        isIdleReleased = true;

        isRestarting = true;
        deviceManager.closeAudioDevice();
        OutputInterfaceLoopbackNode::teardownTap();
        isRestarting = false;
    }

    void exitIdleRelease()
    {
        if (! isIdleReleased)
            return;

        isIdleReleased = false;
        consecutiveEnforceFailures = 0;

        // Following, and macOS changed output while released: open the new device instead
        if (resolveFollowedTarget())
        {
            enforceConfiguration (cachedAudioState.get(), true /* forceTapReinit */);
            return;
        }

        // Reopen exactly the setup that was running before the release (including its active
        // channels). Re-initialising from the saved XML instead would enable all input channels
        // whenever an input device name is saved, which would then block the next idle release.
        isRestarting = true;
        deviceManager.restartLastAudioDevice();
        isRestarting = false;

        auto* currentDevice = deviceManager.getCurrentAudioDevice();

        if (currentDevice != nullptr && currentDevice->isPlaying())
        {
            lastDeviceName = currentDevice->getName();
            lastInputChannels = currentDevice->getActiveInputChannels();
            lastOutputChannels = currentDevice->getActiveOutputChannels();
            lastSampleRate = currentDevice->getCurrentSampleRate();
            lastBufferSize = currentDevice->getCurrentBufferSizeSamples();

            if (OutputInterfaceLoopbackNode::isAnyTapActiveInGraph())
                OutputInterfaceLoopbackNode::ensureTapHealthy (targetOutputDeviceName,
                                                               lastSampleRate,
                                                               lastBufferSize,
                                                               true /* forceTapReinit */);

            if (onConfigRestored != nullptr)
                juce::MessageManager::callAsync ([callback = onConfigRestored] {
                    if (callback)
                        callback();
                });

            loadPendingDefaultPreset (currentDevice->getName());
            return;
        }

        // The previous device couldn't be restarted (e.g. unplugged while released): fall back to normal recovery
        updateTargetSettings();

        if (cachedAudioState != nullptr)
            enforceConfiguration (cachedAudioState.get(), true /* forceTapReinit */);
        else
            doResilience();
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
            // Not while a switch to another device is pending (following macOS): that device's
            // remembered rate is what's wanted, not the old device's
            auto* currentDevice = deviceManager.getCurrentAudioDevice();
            if (currentDevice != nullptr && currentDevice->getName() == targetOutputDeviceName)
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

        resolveFollowedTarget();
    }

    /** In follow mode, retargets to macOS's current output device if it has changed and JUCE
        can see it. Returns true if the target changed. */
    bool resolveFollowedTarget()
    {
        if (! isFollowingSystemOutput())
            return false;

        auto name = getFollowedOutputDeviceName();
        if (name.isEmpty() || name == targetOutputDeviceName)
            return false;

        // JUCE rescans on hardware changes itself; scan here only if the new device isn't listed yet
        if (! isDeviceAvailable (name, false, false))
        {
            scanDevices();
            if (! isDeviceAvailable (name, false, false))
                return false; // not enumerated yet; the next pass tries again
        }

        if (cachedAudioState == nullptr)
            cachedAudioState = deviceManager.createStateXml();
        if (cachedAudioState == nullptr)
            cachedAudioState = std::make_unique<juce::XmlElement> ("DEVICESETUP");

        applyFollowedDevice (*cachedAudioState, name);
        targetOutputDeviceName = name;
        targetSampleRate = cachedAudioState->getDoubleAttribute ("audioDeviceRate", 0.0);
        targetBufferSize = cachedAudioState->getIntAttribute ("audioDeviceBufferSize", 0);
        followedRateToApply = targetSampleRate;
        consecutiveEnforceFailures = 0;

        // A real change of interface (restoring the same one never gets here). Its default
        // preset loads once the device has actually opened: see loadPendingDefaultPreset().
        pendingDefaultPresetDevice = name;

        if (auto* settings = getUserSettings())
        {
            settings->setValue ("audioDeviceState", cachedAudioState.get());
            settings->saveIfNeeded();
        }

        return true;
    }

    /** With "Always enforce sample rate" on, the rate in use carries over to every interface,
        instead of each one's remembered rate. Read from the settings, as it's needed at launch too. */
    static bool isSampleRateEnforcedSetting()
    {
        auto* settings = getUserSettings();
        return settings != nullptr && settings->getBoolValue ("enforceSampleRate", false);
    }

    /** Points a saved device state (as createStateXml() writes it) at another output device, with
        that device's remembered rate, buffer size and outputs rather than the previous device's.
        With nothing remembered they're left out, so JUCE keeps the hardware's current rate and
        opens all outputs. Used when following macOS, here and at launch. The rate is kept
        instead if it's enforced. */
    static void applyFollowedDevice (juce::XmlElement& state, const juce::String& deviceName)
    {
        const bool keepRate = isSampleRateEnforcedSetting() && state.getDoubleAttribute ("audioDeviceRate", 0.0) > 0.0;

        state.setAttribute ("audioOutputDeviceName", deviceName);
        const auto remembered = getRememberedDeviceSettings (deviceName);

        if (! remembered.outputChannels.isZero())
            state.setAttribute ("audioDeviceOutChans", remembered.outputChannels.toString (2));
        else
            state.removeAttribute ("audioDeviceOutChans");

        if (keepRate)
        {
            // Enforced on every interface
        }
        else if (remembered.sampleRate > 0.0)
        {
            state.setAttribute ("audioDeviceRate", remembered.sampleRate);
        }
        else
        {
            state.removeAttribute ("audioDeviceRate");
        }

        if (remembered.bufferSize > 0)
            state.setAttribute ("audioDeviceBufferSize", remembered.bufferSize);
        else
            state.removeAttribute ("audioDeviceBufferSize");
    }

    /** Following macOS, an input device that isn't connected doesn't stop the output opening:
        Curve opens without it and adds it back when it reappears. It stays in the saved settings. */
    bool isInputMissingWhileFollowing() const
    {
        return isFollowingSystemOutput()
            && targetInputDeviceName.isNotEmpty()
            && ! isDeviceAvailable (targetInputDeviceName, true, false);
    }

    bool isTargetInputPresent() const
    {
        return isDeviceAvailable (targetInputDeviceName, true, false) || isInputMissingWhileFollowing();
    }

    // callback when an audio config change occurs
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        // Before the early return: also records changes made in Audio Settings, which suspends this
        if (! isRestarting)
            rememberDeviceSettings (deviceManager.getCurrentAudioDevice());

        if (isRestarting || isSuspended || isIdleReleased) return;

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

            // Following: macOS may not have picked its next output yet. Look again shortly.
            if (isFollowingSystemOutput())
                startTimer (timerDebouncedResilience, 500);
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
        if (isIdleReleased)
        {
            // exitIdleRelease() reconfigures directly, so pending one-shot recovery timers aren't needed
            if (timerId != timerPeriodicHealthCheck)
                stopTimer (timerId);
            return;
        }

        if (isSuspended) return;

        if (timerId == timerWakeRecovery)
        {
            resolveFollowedTarget();
            scanDevices();
            // Waits for the input too, even when following: after wake it's usually still
            // enumerating, and opening without it would mean reopening moments later
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
        if (isSuspended || isIdleReleased)
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

        lastTimeCheckMonotonic = nowMonotonic;
        lastTimeCheckWallClock = nowWallClock;

        if (wokeFromSleep)
            consecutiveEnforceFailures = 0;

        resolveFollowedTarget();

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

            // Already on the new interface (e.g. switched from Audio Settings)
            loadPendingDefaultPreset (currentDevice->getName());

            if (shouldForceSystemOutput())
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

            return;
        }

        // Rescan hardware devices to discover newly connected interfaces
        scanDevices();

        bool isTargetPhysicallyPresent = isDeviceAvailable(targetOutputDeviceName, false, false)
                                      && isTargetInputPresent();
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
        bool isInputMatching  = (targetInputDeviceName.isEmpty() || currentSetup.inputDeviceName == targetInputDeviceName
                                 || isInputMissingWhileFollowing());
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
    bool isIdleReleased = false;
    bool wokeFromSleepFlag = false;
    juce::uint32 wakeRecoveryStartTimeMs = 0;
    std::shared_ptr<std::atomic<bool>> isAlive = std::make_shared<std::atomic<bool>> (true);
    std::unique_ptr<MacOSSleepWakeNotifierBase> sleepWakeNotifier;
    std::unique_ptr<MacOSDefaultOutputChangeNotifierBase> defaultOutputNotifier;
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
    double followedRateToApply = 0.0;
    juce::String pendingDefaultPresetDevice;

    // After onConfigRestored, so the preset loads against the device that's now open
    void loadPendingDefaultPreset (const juce::String& openedDeviceName)
    {
        if (pendingDefaultPresetDevice.isEmpty() || pendingDefaultPresetDevice != openedDeviceName)
            return;

        pendingDefaultPresetDevice.clear();
        juce::MessageManager::callAsync ([name = openedDeviceName] { loadDefaultPresetForOutputDevice (name); });
    }
    std::unique_ptr<juce::XmlElement> cachedAudioState;

    void scanDevices()
    {
        for (auto* type : deviceManager.getAvailableDeviceTypes())
            type->scanForDevices();
    }

    bool isDeviceAvailable(const juce::String& name, bool isInput, bool forceScan) const
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
            if (followedRateToApply > 0.0)
                savedState->setAttribute ("audioDeviceRate", followedRateToApply); // once, on a follow switch
            else if (alwaysEnforceSampleRate && targetSampleRate > 0.0)
                savedState->setAttribute ("audioDeviceRate", targetSampleRate);
            else if (! alwaysEnforceSampleRate)
                savedState->removeAttribute ("audioDeviceRate");

            if (targetBufferSize > 0)
                savedState->setAttribute ("audioDeviceBufferSize", targetBufferSize);
        }

        // Following, without an input that isn't connected (see isInputMissingWhileFollowing())
        std::unique_ptr<juce::XmlElement> withoutInput;
        if (savedState != nullptr && isInputMissingWhileFollowing())
        {
            withoutInput = std::make_unique<juce::XmlElement> (*savedState);
            withoutInput->removeAttribute ("audioInputDeviceName");
            withoutInput->removeAttribute ("audioDeviceInChans");
            numInputs = 0;
        }

        auto error = deviceManager.initialise (numInputs, 256, withoutInput != nullptr ? withoutInput.get() : savedState, false);
        isRestarting = false;

        auto* currentDevice = deviceManager.getCurrentAudioDevice();
        if (currentDevice != nullptr && currentDevice->isPlaying() && error.isEmpty())
        {
            consecutiveEnforceFailures = 0;
            followedRateToApply = 0.0; // applied; kept for a retry if opening failed
            lastDeviceName = currentDevice->getName();
            lastInputChannels = currentDevice->getActiveInputChannels();
            lastOutputChannels = currentDevice->getActiveOutputChannels();
            lastSampleRate = currentDevice->getCurrentSampleRate();
            lastBufferSize = currentDevice->getCurrentBufferSizeSamples();

            // Adapt targets to actual hardware capabilities to prevent repeated enforcement cycles
            targetSampleRate = lastSampleRate;
            targetBufferSize = lastBufferSize;

            if (OutputInterfaceLoopbackNode::isAnyTapActiveInGraph())
            {
                OutputInterfaceLoopbackNode::ensureTapHealthy (targetOutputDeviceName,
                                                               currentDevice->getCurrentSampleRate(),
                                                               currentDevice->getCurrentBufferSizeSamples(),
                                                               forceTapReinit);
            }

            if (onConfigRestored != nullptr)
            {
                // Defer asynchronously to ensure CoreAudio device stop/restart notifications complete first
                juce::MessageManager::callAsync ([callback = onConfigRestored] {
                    if (callback)
                        callback();
                });
            }

            loadPendingDefaultPreset (currentDevice->getName());
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
        OutputInterfaceLoopbackNode::teardownTap();
        isRestarting = false;
    }

    ConfigRestoredCallback onConfigRestored;
};

AudioResilienceManager* getResilienceManager();
