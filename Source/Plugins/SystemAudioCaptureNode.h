/*
==============================================================================
   Copyright (c) Thomas Derham

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   CURVE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.
==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    A custom internal JUCE AudioProcessor that natively taps the macOS System Audio
    using Core Audio Taps, completely bypassing the need for a virtual loopback driver.
*/
class SystemAudioCaptureNode : public juce::AudioPluginInstance
{
public:
    SystemAudioCaptureNode();
    ~SystemAudioCaptureNode() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "System Audio Input"; }
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

    void setTargetOutputDeviceName (const juce::String& name);

    // Pushes incoming samples from the Core Audio Delegate to the FIFO
    void pushAudio (const float* const* channelData, int numChannels, int numSamples);
    void pushAudioInterleaved (const float* interleavedData, int numChannels, int numSamples);

private:
    // Lock-free ring buffer components
    static constexpr int ringBufferCapacity = 192000 * 2; // ~2 seconds of buffer at max sample rate
    juce::AbstractFifo fifo { ringBufferCapacity };
    juce::AudioBuffer<float> ringBuffer;
    
    // PIMPL idiom to hide Objective-C++ details from this header
    struct TapWrapper;
    std::unique_ptr<TapWrapper> tapWrapper;
    
    std::atomic<bool> isCapturing { false };
    bool isBuffering { true };

    juce::String targetOutputDeviceName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SystemAudioCaptureNode)
};