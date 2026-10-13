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
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

//==============================================================================
/**
    Tracks when Curve last produced audible output. Written from the audio thread
    by FadingAudioProcessorPlayer, read by IdleReleaseController on the message thread.
    Only output is measured: an input the preset doesn't use (e.g. a selected but unrouted
    interface input) carries analogue noise and would otherwise never count as silent, while
    an input the preset does route (e.g. a live microphone) shows up in the output anyway.
*/
namespace IdleAudioActivity
{
    inline std::atomic<juce::uint32> lastNonSilentMs { 0 };

    // ~ -100 dBFS: below this a block counts as silence
    constexpr float silenceThreshold = 1.0e-5f;

    inline void noteBlock (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (channels[ch] == nullptr)
                continue;

            auto range = juce::FloatVectorOperations::findMinAndMax (channels[ch], numSamples);

            if (juce::jmax (-range.getStart(), range.getEnd()) > silenceThreshold)
            {
                lastNonSilentMs.store (juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
                return;
            }
        }
    }

    inline void markActiveNow() noexcept
    {
        lastNonSilentMs.store (juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
    }
}

//==============================================================================
/** CoreAudio queries used by IdleReleaseController (implemented in IdleReleaseController.mm). */
namespace IdleReleaseCoreAudio
{
    /** Returns the AudioObjectID of the device with this name, or 0 if not present. */
    juce::uint32 findDeviceID (const juce::String& deviceName);

    /** True if any process other than Curve is running audio input or output on this device. */
    bool isAnotherProcessUsingDevice (juce::uint32 deviceID);

    /** True for virtual devices such as BlackHole or Loopback, which are fed by other apps. */
    bool isVirtualDevice (juce::uint32 deviceID);

    /** Calls onChange (on a CoreAudio queue) whenever kAudioDevicePropertyDeviceIsRunningSomewhere
        changes for this device. The listener is removed when the returned token is destroyed. */
    std::shared_ptr<void> addRunningSomewhereListener (juce::uint32 deviceID, std::function<void()> onChange);
}

//==============================================================================
/**
    Optionally releases the audio interface after a period of silence, so that macOS
    no longer considers the Mac to be using audio (which otherwise blocks AirPods
    automatic switching), and re-opens it as soon as another app starts using the
    configured interface.

    Release only happens when it can be woken again reliably: any active input is a loopback
    fed by other apps (the output interface itself, or a virtual device), no other process is
    using those devices, and the host isn't busy (e.g. recording). See docs/airpods-idle-release.md.
*/
class AudioResilienceManager;

class IdleReleaseController : private juce::Timer
{
public:
    using BusyPredicate = std::function<bool()>;
    using OutputHold = std::function<void (bool shouldHold)>;

    /** holdOutput (true) makes the graph output start silent when the device next starts;
        holdOutput (false) fades it in. Used to resume without a click. */
    IdleReleaseController (juce::AudioDeviceManager& dm, BusyPredicate isHostBusyIn, OutputHold holdOutputIn);
    ~IdleReleaseController() override;

    /** Re-reads the idleReleaseEnabled setting. Resumes if now disabled. */
    void settingsChanged();

    /** Re-opens the interface immediately if it is currently released. */
    void resumeIfReleased();

    static bool isEnabledInSettings();
    static void setEnabledInSettings (bool shouldBeEnabled);

private:
    void timerCallback() override;

    /** Returns why the interface can't be released right now, or nullptr if it can.
        On success, wakeDevicesOut holds the devices whose use by another app should wake Curve. */
    const char* getReasonNotToRelease (juce::StringArray& wakeDevicesOut) const;
    void release (const juce::StringArray& devicesToWatch);
    void resume (const char* reason);
    void armWakeListeners();

    juce::AudioDeviceManager& deviceManager;
    BusyPredicate isHostBusy;
    OutputHold holdOutput;

    static constexpr int idleSeconds = 5;

    bool enabled = false;
    bool released = false;
    const char* lastBlockingReason = nullptr;

    juce::StringArray wakeDeviceNames;
    std::vector<juce::uint32> armedDeviceIDs;
    std::vector<std::shared_ptr<void>> wakeListeners;
    std::shared_ptr<std::atomic<bool>> isAlive = std::make_shared<std::atomic<bool>> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IdleReleaseController)
};

IdleReleaseController* getIdleReleaseController();
