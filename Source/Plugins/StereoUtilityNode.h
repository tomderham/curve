/*
==============================================================================
   Curve
   Copyright (c) Thomas Derham

   The modifications for Curve are licensed to you solely under the terms of the
   AGPLv3 license terms.

   CURVE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.
==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "InternalParameter.h"

//==============================================================================
/**
    An internal AudioProcessor for common stereo channel operations: swap,
    mono sum, left or right only, and stereo width (0-200%). 0 added latency,
    and bit-exact in Stereo mode at 100% width.
*/
class StereoUtilityNode : public juce::AudioPluginInstance
{
public:
    enum class Mode { stereo = 0, swap = 1, mono = 2, leftOnly = 3, rightOnly = 4 };

    StereoUtilityNode();
    ~StereoUtilityNode() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Stereo Utility"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void fillInPluginDescription (juce::PluginDescription& description) const override;

    //==============================================================================
    InternalParameter& getModeParameter() noexcept  { return *modeParam; }
    Mode getMode() const noexcept { return (Mode) juce::jlimit (0, 4, modeParam->getIndex()); }

    // 0 = mono, 100 = unchanged, 200 = double the side signal (%). Applies in Stereo and Swap modes.
    InternalParameter& getWidthParameter() noexcept { return *widthParam; }
    float getWidth() const noexcept { return widthParam->get() / 100.0f; }

    static juce::StringArray getModeNames();

    static bool modeUsesWidth (Mode m) noexcept { return m == Mode::stereo || m == Mode::swap; }

private:
    // outL = c[0] * L + c[1] * R, outR = c[2] * L + c[3] * R
    using Matrix = std::array<float, 4>;

    Matrix computeTargetMatrix() const;

    InternalParameter* modeParam = nullptr;
    InternalParameter* widthParam = nullptr;

    std::array<juce::LinearSmoothedValue<float>, 4> smoothedMatrix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoUtilityNode)
};
