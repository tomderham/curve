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
    A looper, as a looper pedal is: what comes in passes through, with the loop played over it.

    One control does the most: the first press records, the next closes the loop and plays it, the
    next overdubs onto it, and the next plays it again. Others stop and play it, undo and redo the
    last overdub, and clear it. Each is a parameter that acts each time it's set on, so MIDI Learn
    maps footswitches to them, whether they send toggles or presses.

    - Recording can start at once, after a bar's count-in at the preset's tempo, or when you start
      playing, so a singer or player needn't press anything at the right moment
    - With Sync to Tempo, the first loop is a whole number of bars at the preset's tempo: stopped
      early, it records on to the bar; stopped late, it's cut back to it
    - Reverse and Half Speed play the loop backwards, or at half speed an octave down
    - Older Layers fades what's there each time round while overdubbing, so a loop can keep changing
    - Fade Out lets the loop die away over a few seconds when stopped, rather than stopping dead

    The loop is made and changed on the audio thread, which takes requests from the others through
    an atomic. Undo keeps a copy of the loop from before each overdub, made on the message thread
    before the overdub starts, so the audio thread only swaps which copy it plays.
*/
class LooperNode final : public juce::AudioPluginInstance,
                         private juce::AudioProcessorParameter::Listener,
                         private juce::AsyncUpdater
{
public:
    LooperNode();
    ~LooperNode() override;

    enum class State { empty, waiting, countIn, recording, playing, overdubbing, stopped };

    /** How the first recording starts once Loop is pressed. */
    enum class Start { now, countIn, onSound };

    State getState() const noexcept                 { return (State) state.load (std::memory_order_relaxed); }
    int getLoopLength() const noexcept              { return loopLength.load (std::memory_order_relaxed); }
    int getPosition() const noexcept                { return position.load (std::memory_order_relaxed); }
    int getRecordedLength() const noexcept          { return recordedLength.load (std::memory_order_relaxed); }
    bool canUndo() const noexcept                   { return hasUndo.load() && ! undone.load(); }
    bool canRedo() const noexcept                   { return hasUndo.load() && undone.load(); }
    bool isFadingOut() const noexcept               { return fadingOut.load (std::memory_order_relaxed); }
    double getLooperSampleRate() const noexcept     { return currentSampleRate; }
    int getMaxLength() const noexcept               { return loops[0].getNumSamples(); }
    int getLayers() const noexcept                  { return layers.load(); }

    /** The preset's tempo, as it was last block. */
    double getTempo() const noexcept                { return tempo.load (std::memory_order_relaxed); }

    /** In a count-in, the beat it's on, 1 to 4. */
    int getCountInBeat() const noexcept             { return countInBeat.load (std::memory_order_relaxed); }

    /** Bars of the preset's tempo, when syncing; 0 if not. */
    int getBars() const noexcept                    { return bars.load(); }

    bool isSyncedToTempo() const noexcept           { return syncToTempo.load(); }
    void setSyncedToTempo (bool shouldSync)         { syncToTempo = shouldSync; }

    Start getStart() const noexcept                 { return (Start) startMode.load(); }
    void setStart (Start s)                         { startMode = (int) s; }

    bool isFadingOnStop() const noexcept            { return fadeOnStop.load(); }
    void setFadingOnStop (bool shouldFade)          { fadeOnStop = shouldFade; }

    InternalParameter& getLoopParameter() noexcept      { return *loopParam; }
    InternalParameter& getStopParameter() noexcept      { return *stopParam; }
    InternalParameter& getUndoParameter() noexcept      { return *undoParam; }
    InternalParameter& getClearParameter() noexcept     { return *clearParam; }
    InternalParameter& getLevelParameter() noexcept     { return *levelParam; }
    InternalParameter& getFeedbackParameter() noexcept  { return *feedbackParam; }
    InternalParameter& getReverseParameter() noexcept   { return *reverseParam; }
    InternalParameter& getHalfSpeedParameter() noexcept { return *halfSpeedParam; }

    /** The loop's loudness along it, for its window: one peak per column. */
    void getOverview (std::vector<float>& peaks, int numColumns) const;

    /** Writes the loop to a 24-bit WAV file. False if there's no loop, or it couldn't. */
    bool saveLoop (const juce::File&);

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }
    const juce::String getName() const override     { return "Looper"; }
    bool acceptsMidi() const override               { return false; }
    bool producesMidi() const override              { return false; }
    double getTailLengthSeconds() const override    { return 0.0; }
    int getNumPrograms() override                   { return 1; }
    int getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override           {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    void fillInPluginDescription (juce::PluginDescription&) const override;

    static constexpr int maxSeconds = 60;

    /** Playing louder than this starts a recording that waits for sound. */
    static constexpr float soundThresholdDb = -40.0f;

private:
    // What the message thread asks the audio thread to do
    enum Request : int { none = 0, record, closeLoop, overdub, endOverdub, stop, play, undo, clear, cancel };

    void parameterValueChanged (int, float) override;
    void parameterGestureChanged (int, bool) override {}
    void handleAsyncUpdate() override;

    void request (Request r)                        { pendingRequest.store (r, std::memory_order_release); }
    void handleRequest (Request, double samplesPerBar);

    // Audio thread
    void beginRecording();
    void finishRecording (bool thenStop, double samplesPerBar);
    int recordInto (juce::AudioBuffer<float>&, int start, int num);
    int playLoop (juce::AudioBuffer<float>&, int start, int num);
    void addClicks (juce::AudioBuffer<float>&, int start, int num);
    void startFade (double seconds);

    InternalParameter* loopParam = nullptr;
    InternalParameter* stopParam = nullptr;
    InternalParameter* undoParam = nullptr;
    InternalParameter* clearParam = nullptr;
    InternalParameter* levelParam = nullptr;
    InternalParameter* feedbackParam = nullptr;
    InternalParameter* reverseParam = nullptr;
    InternalParameter* halfSpeedParam = nullptr;

    // Two copies: the one played, and the one from before the last overdub, for undo
    juce::AudioBuffer<float> loops[2];
    std::atomic<int> active { 0 };
    std::atomic<bool> hasUndo { false }, undone { false };

    std::atomic<int> state { (int) State::empty };
    std::atomic<int> loopLength { 0 }, position { 0 }, recordedLength { 0 }, targetLength { 0 };
    std::atomic<int> layers { 0 }, bars { 0 }, countInBeat { 0 };
    std::atomic<int> pendingRequest { none };
    std::atomic<int> pressedLoop { 0 }, pressedStop { 0 }, pressedUndo { 0 }, pressedClear { 0 };
    std::atomic<double> tempo { 120.0 };
    std::atomic<bool> fadingOut { false };

    std::atomic<bool> syncToTempo { false }, fadeOnStop { false };
    std::atomic<int> startMode { (int) Start::now };

    // Audio thread only
    double readPosition = 0.0;              // Where it plays, between samples at half speed
    float fadeGain = 1.0f, fadeStep = 0.0f; // Stopping: down to nothing, then it stops
    int countInLength = 0, countInDone = 0;

    juce::LinearSmoothedValue<float> level { 1.0f };
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LooperNode)
};
