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
    An internal AudioProcessor that converts stereo between L/R and M/S, with
    0 latency. The encoder outputs M = (L + R) / 2, S = (L - R) / 2 and the
    decoder outputs L = M + S, R = M - S, so a pair can wrap separate mid and
    side processing. The round trip is bit-exact for sources up to 24-bit.
*/
class MidSideNode : public juce::AudioPluginInstance
{
public:
    enum class Mode { encode, decode };

    explicit MidSideNode (Mode modeToUse);
    ~MidSideNode() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return mode == Mode::decode ? "Mid/Side Decoder" : "Mid/Side Encoder"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void fillInPluginDescription (juce::PluginDescription& description) const override;

private:
    const Mode mode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidSideNode)
};
