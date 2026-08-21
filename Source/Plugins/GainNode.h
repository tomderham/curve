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

//==============================================================================
/**
    An internal AudioProcessor that applies smooth, precise gain adjustment
    (-60 dB to +60 dB) with optional muting and 0 added latency.
*/
class GainNode : public juce::AudioPluginInstance
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
    void setGainDb (float newGainDb);
    float getGainDb() const noexcept { return gainDb.load (std::memory_order_relaxed); }

    void setMuted (bool shouldMute);
    bool isMuted() const noexcept { return muted.load (std::memory_order_relaxed); }

private:
    void updateTargetGain();

    std::atomic<float> gainDb { 0.0f };
    std::atomic<bool> muted { false };
    std::atomic<float> targetGainLinear { 1.0f };

    juce::LinearSmoothedValue<float> smoothedGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainNode)
};
