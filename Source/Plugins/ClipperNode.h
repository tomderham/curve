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
    An internal AudioProcessor that hard clips every channel at a ceiling
    (-50 dBFS to 0 dBFS), as a safety guard after EQ boosts. 0 added latency;
    samples below the ceiling pass through bit-exact.
*/
class ClipperNode : public juce::AudioPluginInstance,
                    private juce::AudioProcessorParameter::Listener
{
public:
    static constexpr float minCeilingDb = -50.0f;
    static constexpr float maxCeilingDb = 0.0f;

    ClipperNode();
    ~ClipperNode() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Clipper"; }
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
    InternalParameter& getCeilingParameter() noexcept { return *ceilingParam; }  // dBFS
    float getCeilingDb() const noexcept { return ceilingParam->get(); }

    // True if any sample was clipped since the last call
    bool consumeClipFlag() noexcept { return clipped.exchange (false, std::memory_order_relaxed); }

private:
    void parameterValueChanged (int, float) override;
    void parameterGestureChanged (int, bool) override {}

    InternalParameter* ceilingParam = nullptr;
    std::atomic<float> ceilingLinear { juce::Decibels::decibelsToGain (-1.0f) };
    std::atomic<bool> clipped { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipperNode)
};
