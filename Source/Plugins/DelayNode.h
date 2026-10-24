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
    An internal AudioProcessor that delays every channel by a whole number of
    samples (up to 2 seconds), with no interpolation or filtering.

    The delay is deliberately not reported as plugin latency, otherwise the graph
    would compensate for it on parallel paths and defeat the purpose.
*/
class DelayNode : public juce::AudioPluginInstance
{
public:
    enum class DisplayUnit { samples = 0, milliseconds = 1, centimeters = 2 };

    static constexpr double maxDelaySeconds = 2.0;

    DelayNode();
    ~DelayNode() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Delay"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return maxDelaySeconds; }
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
    void setDelaySamples (int newDelaySamples);
    int getDelaySamples() const noexcept { return delaySamples.load (std::memory_order_relaxed); }

    void setDisplayUnit (DisplayUnit newUnit) { displayUnit.store ((int) newUnit, std::memory_order_relaxed); }
    DisplayUnit getDisplayUnit() const noexcept { return (DisplayUnit) displayUnit.load (std::memory_order_relaxed); }

    // Sample rate the delay is currently running at (or a default before playback starts)
    double getEffectiveSampleRate() const;
    int getMaxDelaySamples() const;

private:
    std::atomic<int> delaySamples { 0 };
    std::atomic<int> displayUnit { (int) DisplayUnit::milliseconds };

    juce::AudioBuffer<float> delayLine;
    int writePosition = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayNode)
};
