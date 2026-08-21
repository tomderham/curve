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
    An internal AudioProcessor that captures macOS output interface loopback
    audio natively via CoreAudio process taps.
*/
class OutputInterfaceLoopbackNode : public juce::AudioPluginInstance
{
public:
    OutputInterfaceLoopbackNode();
    ~OutputInterfaceLoopbackNode() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "Interface Loopback (In)"; }
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
    void refreshCapture();
    void setActiveState (bool shouldBeActive);
    static void updateGlobalMuteBehavior();
    static void syncSystemOutputDevice (const juce::String& name);
    static bool isAnyTapActiveInGraph();
    static void warmUpTap (const juce::String& targetDevice, double sampleRate, int bufferSize = 128);

    // Lets the tap's own real-time thread join the same audio workgroup as the main
    // output device's IO thread, so the OS scheduler treats them as one deadline chain.
    void setAudioWorkgroup (const juce::AudioWorkgroup& workgroup);

    // Pushes incoming samples from the Core Audio Delegate to the FIFO
    void pushAudio (const float* const* channelData, int numChannels, int numSamples);
    void pushAudioInterleaved (const float* interleavedData, int numChannels, int numSamples);

private:
    // Lock-free ring buffer components
    static constexpr int maxTapChannels = 32;
    static constexpr int ringBufferCapacity = 32768; // 2^15 samples: power-of-two alignment & fits inside L2/L3 cache
    juce::AbstractFifo fifo { ringBufferCapacity };
    juce::AudioBuffer<float> ringBuffer { maxTapChannels, ringBufferCapacity };

    std::atomic<bool> isCapturing { false };
    std::atomic<bool> isPrimed { false };
    std::atomic<bool> hadUnderrunLastBlock { false };
    std::atomic<bool> isActiveInGraph { true };

    juce::String targetOutputDeviceName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputInterfaceLoopbackNode)
};
