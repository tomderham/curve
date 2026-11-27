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
/** What the menu bar panel shows, and what it can ask for, without the rest of Curve, so the
    AppKit side (MenuBarPanel.mm) needn't include the graph. MenuBarPanelModel provides both. */
namespace MenuBarPanelState
{
    struct Preset
    {
        juce::String name;
        juce::File file;
        bool isCurrent = false;
        std::optional<juce::Colour> groupColour; // Scenes of a group share a colour
        juce::String group;                       // The group's name, for a scene with others
        juce::String shortcut;                    // e.g. "1", "⌥3"
        int keySlot = -1;                         // Which key selects it (see PresetSlots), -1 if none
        bool isKeyAssigned = false;               // Its own, rather than from its place in the order
    };

    struct Plugin
    {
        juce::uint32 id = 0; // The node's ID
        juce::String label;
        bool canBypass = false, isBypassed = false, isMuted = false;
        bool canTurnOff = false; // Passes nothing on, e.g. a Meter, so it's on or off (muted) instead
        bool hasEditor = false, isWindowOpen = false, isWindowKeptOnTop = false;
        bool isMeter = false; // One of Curve's Meter plug-ins, whose level the panel can show
        int meterMode = 0;    // What its window shows (see MeterReading::Mode)
    };

    struct State
    {
        juce::Array<Preset> presets;
        bool isPresetModified = false;

        std::vector<Plugin> plugins;

        juce::String deviceName; // Empty if there's no audio device
        double sampleRate = 0.0;
        int bufferSize = 0, latencySamples = 0;

        // What the volume slider sets (see VolumeKeyManager::getVolumeControl()): Curve's own volume,
        // in steps, or the interface's, 0 to 1
        enum class VolumeKind { none, curve, interface };
        VolumeKind volumeKind = VolumeKind::none;
        int volumeSteps = 0, maxVolumeSteps = 64;
        bool isVolumeMuted = false;
        float interfaceVolume = 0.0f;
        juce::String volumeDescription; // e.g. "-11.9 dB", "60%"

        bool isEditorShowing = false;
        bool isEditorInView = false; // Showing, and not on another Space or mostly behind other windows

        // The preset's tempo and transport (see HostTransport)
        double tempo = 120.0;
        bool isTransportPlaying = false;

        int numPluginWindows = 0;          // Open, shown or hidden
        bool arePluginWindowsHidden = false;
        bool arePluginWindowPositionsLocked = false;

        // The output switches (see OutputMonitor), and Bypass All (see PluginGraph::setBypassingAll())
        bool isBypassingAll = false, isMuted = false, isDimmed = false, isMono = false;
    };

    enum class OutputSwitch { bypassAll, mute, dim, mono };

    /** What a Meter plug-in measures, as its window shows it: levels, VU or loudness. */
    struct MeterReading
    {
        enum class Mode { levels, vu, loudness, spectrum };
        Mode mode = Mode::levels;
        int numChannels = 2;
        float peak[2] {}, truePeak[2] {}, rms[2] {}, vu[2] {}; // Linear: peaks since the last reading
        float correlation = 0.0f;                   // -1 to +1
        bool over[2] {};                            // Gone over full scale, since its window cleared it
        int vuReferenceDb = -18;                    // The dBFS that reads 0 VU
        float momentary = -200.0f, shortTerm = -200.0f, integrated = -200.0f; // LUFS; -200 for none yet
        bool isValid = false;
    };

    class Source
    {
    public:
        virtual ~Source() = default;

        virtual State getState() const = 0;

        /** Starts or stops telling onChange about changes, once per batch. */
        virtual void setListening (bool) = 0;
        std::function<void()> onChange;

        virtual void loadPreset (const juce::File&) = 0;

        /** Gives a preset a key (see PresetSlots), taking it from any other; -1 for automatic. */
        virtual void assignKey (const juce::File&, int keySlot) = 0;
        virtual void saveAsPreset() = 0;
        virtual void setBypassed (juce::uint32 nodeID, bool) = 0;
        virtual void setMuted (juce::uint32 nodeID, bool) = 0;
        virtual void toggleEditor (juce::uint32 nodeID) = 0; // Opens or closes the plug-in's window
        virtual void hideFromPanel (juce::uint32 nodeID) = 0;
        virtual void setVolume (int steps, bool muted) = 0;
        virtual void setInterfaceVolume (float) = 0;
        virtual void toggleGraphEditor() = 0;
        virtual void togglePluginWindows() = 0;
        virtual void setOutputSwitch (OutputSwitch, bool) = 0;

        /** The output's peak levels since the last read, 0 to 1, while metering is on. */
        virtual void setMetering (bool) = 0;

        /** Whether the Meter plug-ins in the plug-in list measure for the panel, apart from the
            output meter, which may not be shown. */
        virtual void setPluginMetering (bool) = 0;
        virtual void readOutputPeaks (float& left, float& right) = 0;

        /** A Meter plug-in's measurements, while metering is on. Each call takes the peaks since the last. */
        virtual MeterReading readMeter (juce::uint32 nodeID) = 0;

        virtual void setTempo (double bpm) = 0;
        virtual void tapTempo() = 0;
        virtual void setTransportPlaying (bool) = 0;
        virtual void setWindowKeptOnTop (juce::uint32 nodeID, bool) = 0;
        virtual void setPluginWindowPositionsLocked (bool) = 0;
    };
}
