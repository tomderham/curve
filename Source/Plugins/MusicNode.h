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
    Controls Music or Spotify from Curve's windows: what's playing, with its artwork, play and pause,
    next and previous, and where it is in the track, and for Music, searching the library and playing
    playlists, or for Spotify, playing a link pasted in. It has no audio.

    It talks to the apps with AppleScript, run by osascript on a thread of its own, as an app can be
    slow to answer. macOS asks the first time whether Curve may control each app. It asks the apps
    what's playing only while its window is open; Play/Pause, Next and Previous are parameters, so
    MIDI Learn maps controls to them, and they work whether its window is open or not.
*/
class MusicNode final : public juce::AudioPluginInstance,
                        public juce::ChangeBroadcaster,
                        private juce::AudioProcessorParameter::Listener,
                        private juce::AsyncUpdater
{
public:
    MusicNode();
    ~MusicNode() override;

    enum class App { none, music, spotify };

    struct NowPlaying
    {
        App app = App::none;
        bool isPlaying = false;
        juce::String title, artist, album, trackID, artworkURL;
        double duration = 0.0, position = 0.0;
        juce::Image artwork;
    };

    struct Item
    {
        juce::String name, detail, id; // A track's persistent ID, or a playlist's
    };

    NowPlaying getNowPlaying() const;

    /** Why it can't control the apps, if it can't, e.g. not allowed to. Empty if it can. */
    juce::String getProblem() const;

    static bool isSpotifyInstalled();

    void playPause();
    void next();
    void previous();
    void seek (double seconds);

    /** Music: searches the library, and calls back with the songs, on the message thread. */
    void search (const juce::String& text, std::function<void (juce::Array<Item>)> onResults);
    void getPlaylists (std::function<void (juce::Array<Item>)> onResults);
    void playTrack (const juce::String& persistentID);
    void playPlaylist (const juce::String& persistentID);

    /** Spotify: plays a link copied from it, e.g. https://open.spotify.com/track/... */
    void playSpotifyLink (const juce::String& link);

    /** While its window is open, it asks the apps what's playing, once a second. */
    void setWatching (bool shouldWatch);

    InternalParameter& getPlayPauseParameter() noexcept { return *playPauseParam; }
    InternalParameter& getNextParameter() noexcept      { return *nextParam; }
    InternalParameter& getPreviousParameter() noexcept  { return *previousParam; }

    //==============================================================================
    void prepareToPlay (double, int) override       {}
    void releaseResources() override                {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { buffer.clear(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                 { return true; }
    const juce::String getName() const override     { return "Music"; }
    bool acceptsMidi() const override               { return false; }
    bool producesMidi() const override              { return false; }
    double getTailLengthSeconds() const override    { return 0.0; }
    int getNumPrograms() override                   { return 1; }
    int getCurrentProgram() override                { return 0; }
    void setCurrentProgram (int) override           {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }

    void fillInPluginDescription (juce::PluginDescription&) const override;

    class Worker;

private:
    void parameterValueChanged (int, float) override;
    void parameterGestureChanged (int, bool) override {}
    void handleAsyncUpdate() override;

    // Runs a script on the worker, and calls back with its output on the message thread
    void run (const juce::String& script, std::function<void (const juce::String&)> onDone = nullptr);
    juce::String controlScript (const juce::String& musicCommand, const juce::String& spotifyCommand) const;

    void updateNowPlaying (const juce::String& musicOutput, const juce::String& spotifyOutput);

    Worker& getWorker();
    std::unique_ptr<Worker> worker;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    mutable juce::CriticalSection lock;
    NowPlaying nowPlaying;
    juce::String problem, artworkFor;

    InternalParameter* playPauseParam = nullptr;
    InternalParameter* nextParam = nullptr;
    InternalParameter* previousParam = nullptr;
    std::atomic<int> pressedPlayPause { 0 }, pressedNext { 0 }, pressedPrevious { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicNode)
};
