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
    };

    struct Plugin
    {
        juce::uint32 id = 0; // The node's ID
        juce::String label;
        bool canBypass = false, isBypassed = false, isMuted = false;
        bool hasEditor = false, isWindowOpen = false, isWindowKeptOnTop = false;
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

        int numPluginWindows = 0;          // Open, shown or hidden
        bool arePluginWindowsHidden = false;
        bool arePluginWindowPositionsLocked = false;

        // The output switches (see OutputMonitor), and Bypass All (see PluginGraph::setBypassingAll())
        bool isBypassingAll = false, isMuted = false, isDimmed = false, isMono = false;
    };

    enum class OutputSwitch { bypassAll, mute, dim, mono };

    class Source
    {
    public:
        virtual ~Source() = default;

        virtual State getState() const = 0;

        /** Starts or stops telling onChange about changes, once per batch. */
        virtual void setListening (bool) = 0;
        std::function<void()> onChange;

        virtual void loadPreset (const juce::File&) = 0;
        virtual void saveAsPreset() = 0;
        virtual void setBypassed (juce::uint32 nodeID, bool) = 0;
        virtual void setMuted (juce::uint32 nodeID, bool) = 0;
        virtual void toggleEditor (juce::uint32 nodeID) = 0; // Opens or closes the plug-in's window
        virtual void hideFromPanel (juce::uint32 nodeID) = 0;
        virtual void setVolume (int steps, bool muted) = 0;
        virtual void setInterfaceVolume (float) = 0;
        virtual void toggleGraphEditor() = 0;
        virtual void togglePluginWindows() = 0;
        virtual void arrangePluginWindows() = 0;
        virtual void setOutputSwitch (OutputSwitch, bool) = 0;

        /** The output's peak levels since the last read, 0 to 1, while metering is on. */
        virtual void setMetering (bool) = 0;
        virtual void readOutputPeaks (float& left, float& right) = 0;
        virtual void setWindowKeptOnTop (juce::uint32 nodeID, bool) = 0;
        virtual void setPluginWindowPositionsLocked (bool) = 0;
    };
}
