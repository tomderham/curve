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
#include "MeterDsp.h"

//==============================================================================
/**
    An internal AudioProcessor that shows levels, VU, loudness or a spectrum of its
    input. It has no outputs by default, so it can branch off any pin. Outputs can be
    enabled to put it inline, where audio passes through untouched, with 0 latency.

    It only measures while its window is open, so a closed meter costs nothing.
    The spectrum's FFT runs on the message thread, from samples copied through a
    FIFO only while that view is showing.
*/
class MeterNode : public juce::AudioPluginInstance
{
public:
    enum class Mode { levels = 0, vu, loudness, spectrum };

    MeterNode();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Meter"; }
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
    MeterDsp& getDsp() noexcept { return dsp; }
    double getCurrentSampleRate() const noexcept { return currentSampleRate.load (std::memory_order_relaxed); }

    Mode getMode() const noexcept { return mode; }
    void setMode (Mode newMode) noexcept { mode = newMode; }

    static constexpr int vuReferences[] { -20, -18, -14, -10 };   // common 0 VU alignments, in dBFS
    int getVuReferenceDb() const noexcept { return vuReferenceDb; }
    void setVuReferenceDb (int newReference) noexcept { vuReferenceDb = newReference; }

    bool getSpectrumSplit() const noexcept { return spectrumSplit; }
    void setSpectrumSplit (bool shouldSplit) noexcept { spectrumSplit = shouldSplit; }

    juce::Point<int> getEditorSize() const noexcept { return editorSize; }
    void setEditorSize (juce::Point<int> newSize) noexcept { editorSize = newSize; }

    // Message thread, by one reader at a time
    void setSpectrumWanted (bool shouldCopy) noexcept { spectrumWanted.store (shouldCopy, std::memory_order_relaxed); }
    int readSpectrumSamples (float* left, float* right, int maxSamples) noexcept;

private:
    void pushSpectrumSamples (const float* left, const float* right, int numSamples) noexcept;

    MeterDsp dsp;
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<bool> spectrumWanted { false };

    static constexpr int spectrumFifoSize = 1 << 16;
    juce::AbstractFifo spectrumFifo { spectrumFifoSize };
    juce::AudioBuffer<float> spectrumSamples { 2, spectrumFifoSize };

    Mode mode = Mode::levels;
    int vuReferenceDb = -18;
    bool spectrumSplit = false;
    juce::Point<int> editorSize { 390, 200 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterNode)
};
