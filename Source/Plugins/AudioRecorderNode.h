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
    An internal AudioProcessor that captures bit-exact 32-bit floating point WAV
    files directly to disk on a background thread with zero audio thread blocking.
*/
class AudioRecorderNode : public juce::AudioPluginInstance,
                          private juce::Thread
{
public:
    AudioRecorderNode();
    ~AudioRecorderNode() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Audio Recorder"; }
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
    void processorLayoutsChanged() override;

    void fillInPluginDescription (juce::PluginDescription& description) const override;

    //==============================================================================
    void startRecording();
    void stopRecording();
    bool isRecording() const noexcept { return isRecordingFlag.load (std::memory_order_relaxed); }
    juce::int64 getRecordedSampleCount() const noexcept { return samplesRecorded.load (std::memory_order_relaxed); }
    juce::File getLastRecordedFile() const { return lastRecordedFile; }

private:
    void run() override;

    std::atomic<bool> isRecordingFlag { false };
    std::atomic<juce::int64> samplesRecorded { 0 };
    juce::File lastRecordedFile;

    static constexpr int maxRecorderChannels = 32;
    static constexpr int fifoCapacity = 131072;
    juce::AbstractFifo fifo { fifoCapacity };
    juce::AudioBuffer<float> fifoBuffer { maxRecorderChannels, fifoCapacity };

    double currentSampleRate = 96000.0;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    juce::CriticalSection writerLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRecorderNode)
};
