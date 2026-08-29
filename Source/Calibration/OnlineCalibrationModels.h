/*
  ==============================================================================

    Curve - Online Calibration Models

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

enum class CalibrationFilterType
{
    peaking = 0,
    lowShelf,
    highShelf,
    lowPass,
    highPass,
    bandPass,
    notch,
    unknown
};

struct CalibrationFilterBand
{
    int index = 0;
    bool enabled = true;
    CalibrationFilterType type = CalibrationFilterType::peaking;
    double frequencyHz = 1000.0;
    double gainDb = 0.0;
    double qFactor = 1.414;
    double bandwidthOctaves = 1.0;
};

struct CalibrationProfile
{
    juce::String name;
    juce::String source;
    juce::String formFactor;
    juce::String target;
    double preampGainDb = 0.0;
    std::vector<CalibrationFilterBand> bands;
};

struct CalibrationHeadphoneEntry
{
    juce::String name;
    juce::String source;
    juce::String formFactor;
    juce::String target;
    juce::String folderPath; // Relative path in AutoEQ results/ directory
};

