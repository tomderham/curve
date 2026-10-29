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
    An internal AudioProcessor that places a mono input in a stereo output using
    a selectable pan law. With a stereo input it acts as a balance control.
    0 added latency.
*/
class PanNode : public juce::AudioPluginInstance,
                private juce::AudioProcessorParameter::Listener
{
public:
    // Level of each output at centre, for a mono input
    enum class PanLaw { zeroDb = 0, minus3Db = 1, minus4p5Db = 2, minus6Db = 3 };

    PanNode();
    ~PanNode() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Pan"; }
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
    void processorLayoutsChanged() override;

    void fillInPluginDescription (juce::PluginDescription& description) const override;

    //==============================================================================
    // -100 = hard left, 0 = centre, +100 = hard right
    InternalParameter& getPanParameter() noexcept { return *panParam; }

    // -1 = hard left, 0 = centre, +1 = hard right
    float getPan() const noexcept { return panParam->get() / 100.0f; }

    void setPanLaw (PanLaw newLaw);
    PanLaw getPanLaw() const noexcept { return (PanLaw) panLaw.load (std::memory_order_relaxed); }

    bool hasMonoInput() const { return getMainBusNumInputChannels() == 1; }

private:
    void updateTargetGains();

    void parameterValueChanged (int, float) override    { updateTargetGains(); }
    void parameterGestureChanged (int, bool) override   {}

    InternalParameter* panParam = nullptr;
    std::atomic<int> panLaw { (int) PanLaw::minus3Db };
    std::atomic<bool> monoInput { true };
    std::atomic<float> targetGainLeft { 1.0f };
    std::atomic<float> targetGainRight { 1.0f };

    juce::LinearSmoothedValue<float> smoothedGainLeft;
    juce::LinearSmoothedValue<float> smoothedGainRight;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanNode)
};
