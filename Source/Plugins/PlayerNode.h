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
    Plays backing tracks from a playlist, over what comes in, which passes through, so it can go
    anywhere in a chain. Files are read ahead on a thread of their own, so the audio thread never
    waits on the disk.

    Play/Pause, Stop, Next and Previous are parameters that act each time they're set on, so MIDI
    Learn maps controls to them. The playlist and the track it's on are its state, so a preset opens
    it where it was, and with Vary by Scene, each scene has its own track.
*/
class PlayerNode final : public juce::AudioPluginInstance,
                         public juce::ChangeBroadcaster,
                         private juce::AudioProcessorParameter::Listener,
                         private juce::AsyncUpdater,
                         private juce::ChangeListener
{
public:
    PlayerNode();
    ~PlayerNode() override;

    /** What it does when a track ends. */
    enum class AtEnd { stop, next, repeat };

    const juce::Array<juce::File>& getPlaylist() const noexcept     { return playlist; }
    int getCurrentIndex() const noexcept                            { return currentIndex; }
    juce::File getCurrentFile() const                               { return playlist[currentIndex]; }

    void addFiles (const juce::Array<juce::File>&);
    void removeFile (int index);

    /** Moves a track to before the one at newIndex (or to the end), keeping the one it's on. */
    void moveFile (int index, int newIndex);
    void selectTrack (int index, bool andPlay);

    bool isPlaying() const                                          { return transport.isPlaying(); }
    void play();
    void pause();
    void stop();
    void next();
    void previous();

    double getPosition() const                                      { return transport.getCurrentPosition(); }
    double getLength() const                                        { return transport.getLengthInSeconds(); }
    void setPosition (double seconds)                               { transport.setPosition (seconds); }

    AtEnd getAtEnd() const noexcept                                 { return atEnd; }
    void setAtEnd (AtEnd);

    juce::AudioFormatManager& getFormatManager() noexcept           { return formatManager; }
    static bool canPlay (const juce::File&);

    InternalParameter& getPlayParameter() noexcept      { return *playParam; }
    InternalParameter& getStopParameter() noexcept      { return *stopParam; }
    InternalParameter& getNextParameter() noexcept      { return *nextParam; }
    InternalParameter& getPreviousParameter() noexcept  { return *previousParam; }
    InternalParameter& getLevelParameter() noexcept     { return *levelParam; }

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }
    const juce::String getName() const override     { return "Player"; }
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

    void fillInPluginDescription (juce::PluginDescription& d) const override   { describe (d); }

    /** Its description, without making one, which starts a thread to read files with. */
    static void describe (juce::PluginDescription&);

private:
    void parameterValueChanged (int, float) override;
    void parameterGestureChanged (int, bool) override {}
    void handleAsyncUpdate() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void load (int index);

    juce::AudioFormatManager formatManager;
    juce::TimeSliceThread readAhead { "Curve Player" };
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioBuffer<float> playBuffer;

    juce::Array<juce::File> playlist;
    int currentIndex = 0;
    AtEnd atEnd = AtEnd::stop;
    bool stoppedOnPurpose = false;

    InternalParameter* playParam = nullptr;
    InternalParameter* stopParam = nullptr;
    InternalParameter* nextParam = nullptr;
    InternalParameter* previousParam = nullptr;
    InternalParameter* levelParam = nullptr;
    std::atomic<int> pressedPlay { 0 }, pressedStop { 0 }, pressedNext { 0 }, pressedPrevious { 0 };

    juce::LinearSmoothedValue<float> level { 1.0f };
    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayerNode)
};
