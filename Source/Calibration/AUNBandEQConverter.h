/*
  ==============================================================================

    Curve - AUNBandEQ Converter

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "OnlineCalibrationModels.h"

namespace AUNBandEQConverter
{
    /** Converts a filter quality factor (Q) to bandwidth in octaves (N). */
    double qFactorToOctaveBandwidth (double q);

    /** Converts bandwidth in octaves (N) back to quality factor (Q). */
    double octaveBandwidthToQFactor (double octaves);

    /** Returns true if the plugin description matches Apple's AUNBandEQ. */
    bool isAUNBandEQ (const juce::PluginDescription& desc);

    /** Returns true if the AudioProcessor instance is Apple's AUNBandEQ. */
    bool isAUNBandEQ (juce::AudioProcessor* proc);

    /** Finds the AUNBandEQ plugin description in a KnownPluginList. */
    std::optional<juce::PluginDescription> findAUNBandEQ (const juce::KnownPluginList& knownPlugins);
    std::optional<juce::PluginDescription> findAUNBandEQ (juce::KnownPluginList& knownPlugins);

    /** Applies an online calibration profile to an AUNBandEQ audio unit instance.
        Returns true if parameters were successfully applied.
    */
    bool applyProfileToAUNBandEQ (juce::AudioProcessor* proc, const CalibrationProfile& profile);

    /** Restores an AUNBandEQ audio processor to a previously saved state and notifies host/GUI. */
    bool restoreAUNBandEQState (juce::AudioProcessor* proc, const juce::MemoryBlock& state);
}

