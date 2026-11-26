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
    Records what reaches it to a file in the recordings folder (Preferences > Files), on a background
    thread, so the audio thread never waits on the disk. Where it is in the graph decides what it
    records: before the effects, or after them.

    - It can keep the last few seconds in memory, so a recording also has what was played just before
      Record was pressed (retroactive recording)
    - In Auto, it starts recording when sound begins, with what it kept, and stops after a silence
    - WAV (16 or 24-bit, or 32-bit float), FLAC (16 or 24-bit) or AIFF (24-bit)

    Record and Auto are parameters, so MIDI Learn maps controls to them. The options are its state,
    saved with the preset; the recordings made are kept app-wide, so its window can show the last.
*/
class AudioRecorderNode : public juce::AudioPluginInstance,
                          private juce::Thread,
                          private juce::AudioProcessorParameter::Listener,
                          private juce::AsyncUpdater,
                          private juce::Timer
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
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processorLayoutsChanged() override;
    void fillInPluginDescription (juce::PluginDescription& description) const override;

    //==============================================================================
    /** On while recording. Setting it (e.g. from MIDI) starts or stops a recording. */
    InternalParameter& getRecordParameter() noexcept { return *recordParam; }

    /** On while it records by itself: from when sound begins to after a silence. */
    InternalParameter& getAutoParameter() noexcept   { return *autoParam; }

    void startRecording();
    void stopRecording();
    bool isRecording() const noexcept { return isRecordingFlag.load (std::memory_order_relaxed); }
    juce::int64 getRecordedSampleCount() const noexcept { return samplesRecorded.load (std::memory_order_relaxed); }
    juce::File getLastRecordedFile() const { return lastRecordedFile; }
    static constexpr int maxRecorderChannels = 32;

    //==============================================================================
    enum class Format { wav16, wav24, wav32Float, flac16, flac24, aiff24 };
    static juce::StringArray getFormatNames();

    Format getFormat() const noexcept               { return format; }
    void setFormat (Format f)                       { format = f; }

    /** What recordings are called: with a name, "Name 1", "Name 2" and so on; without one, "Curve
        Recording" and the date and time. */
    juce::String getFileName() const                { return fileName; }
    void setFileName (const juce::String& name)     { fileName = juce::File::createLegalFileName (name.trim()); }

    /** The file the next recording goes to. */
    juce::File getNextFile() const;

    /** How many seconds it keeps in memory for a recording to start with: 0, 10, 30 or 60. */
    int getHistorySeconds() const noexcept          { return historySeconds; }
    void setHistorySeconds (int seconds);

    /** In Auto, how long a silence ends a recording. */
    int getSilenceSeconds() const noexcept          { return silenceSeconds; }
    void setSilenceSeconds (int seconds)            { silenceSeconds = juce::jlimit (1, 600, seconds); }

    /** Sound louder than this starts a recording in Auto, and keeps it going. */
    static constexpr float soundThresholdDb = -50.0f;

    /** The loudest it's heard since the last call, 0 to 1, for its window's waveform. */
    float readPeak() noexcept { return peakSinceRead.exchange (0.0f, std::memory_order_relaxed); }

    double getRecorderSampleRate() const noexcept   { return currentSampleRate; }

    /** The recordings any recorder has made, newest first, kept in user settings. */
    static juce::Array<juce::File> getRecentRecordings();
    static void forgetRecording (const juce::File&);

    /** Renames a recording, keeping its place in the list. False if it can't, e.g. the name's taken. */
    static bool renameRecording (const juce::File&, const juce::String& newName);

private:
    void run() override;

    void parameterValueChanged (int, float) override    { triggerAsyncUpdate(); }
    void parameterGestureChanged (int, bool) override   {}

    // Brings the recording in line with the Record parameter, on the message thread
    void handleAsyncUpdate() override;

    // Auto: starts and stops recordings from what the audio thread hears
    void timerCallback() override;

    void writeFromFifo (int numSamples);
    void writeHistory();
    void allocateHistory();
    static void rememberRecording (const juce::File&);

    InternalParameter* recordParam = nullptr;
    InternalParameter* autoParam = nullptr;

    std::atomic<bool> isRecordingFlag { false };
    std::atomic<juce::int64> samplesRecorded { 0 };
    juce::File lastRecordedFile;

    static constexpr int fifoCapacity = 131072;
    juce::AbstractFifo fifo { fifoCapacity };
    juce::AudioBuffer<float> fifoBuffer { maxRecorderChannels, fifoCapacity };

    // What it heard last, round and round, for a recording to start with. The audio thread marks
    // where a recording starts in it (historyMark), so what's kept and what's recorded meet exactly
    juce::AudioBuffer<float> historyBuffer;
    std::atomic<juce::int64> historyWritten { 0 };          // In total, by the audio thread
    std::atomic<juce::int64> historyMark { -1 };            // Where the recording starts, once marked
    std::atomic<bool> markRequested { false };
    static constexpr int historyMarginSeconds = 5;           // Room for the writer to copy it

    // What the audio thread hears, for Auto and the waveform
    std::atomic<float> peakSinceRead { 0.0f };
    std::atomic<juce::int64> samplesSinceSound { 0 };
    std::atomic<bool> heardSound { false };

    bool startedByAuto = false;            // Auto stops only what it started
    juce::uint32 startRequestTime = 0;

    Format format = Format::wav32Float;
    juce::String fileName;
    int historySeconds = 0, silenceSeconds = 5;

    double currentSampleRate = 96000.0;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    juce::CriticalSection writerLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRecorderNode)
};
