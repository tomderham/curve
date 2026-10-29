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
    An internal AudioProcessor that applies smooth, precise gain adjustment
    (-60 dB to +60 dB) with optional muting and 0 added latency.
*/
class GainNode : public juce::AudioPluginInstance,
                 private juce::AudioProcessorParameter::Listener
{
public:
    GainNode();
    ~GainNode() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Gain"; }
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
    InternalParameter& getGainParameter() noexcept  { return *gainParam; }  // dB
    InternalParameter& getMuteParameter() noexcept  { return *muteParam; }

private:
    void updateTargetGain();

    void parameterValueChanged (int, float) override    { updateTargetGain(); }
    void parameterGestureChanged (int, bool) override   {}

    InternalParameter* gainParam = nullptr;
    InternalParameter* muteParam = nullptr;
    std::atomic<float> targetGainLinear { 1.0f };

    juce::LinearSmoothedValue<float> smoothedGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainNode)
};
