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
    Shows a score, chart or lyric sheet, as a PDF or an image, in its window, for playing from on
    stage. It has no audio. The file and the page are its state, so a preset opens it where it was,
    and with Vary by Scene, each scene has its own page.

    Pages turn from:
    - its Next Page and Previous Page parameters, which turn one page each time they're set on,
      so MIDI Learn maps a pedal or button to them, whether it sends toggles or presses
    - the keys keyboard page turners send, while Curve is the active app (see PdfView)
    - a horizontal swipe on a trackpad, and the buttons in its window
*/
class ScoreViewerNode final : public juce::AudioPluginInstance,
                              public juce::ChangeBroadcaster,
                              private juce::AudioProcessorParameter::Listener,
                              private juce::AsyncUpdater
{
public:
    ScoreViewerNode();
    ~ScoreViewerNode() override;

    //==============================================================================
    juce::File getFile() const                  { return file; }
    void setFile (const juce::File&);           // From the first page

    int getPage() const noexcept                { return page; }
    int getNumPages() const noexcept            { return numPages; }
    void turn (int direction);                  // A page, or a spread in two-up, onwards or back
    void showPage (int index);

    bool isTwoUp() const noexcept               { return twoUp; }
    void setTwoUp (bool);

    InternalParameter& getNextPageParameter() noexcept      { return *nextParam; }
    InternalParameter& getPreviousPageParameter() noexcept  { return *previousParam; }

    //==============================================================================
    void prepareToPlay (double, int) override   {}
    void releaseResources() override            {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { buffer.clear(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override             { return true; }
    const juce::String getName() const override { return "Score Viewer"; }
    bool acceptsMidi() const override           { return false; }
    bool producesMidi() const override          { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override               { return 1; }
    int getCurrentProgram() override            { return 0; }
    void setCurrentProgram (int) override       {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }

    void fillInPluginDescription (juce::PluginDescription&) const override;

    static constexpr juce::uint32 uniqueId = 0x53434F52; // "SCOR"

private:
    void parameterValueChanged (int, float) override;
    void parameterGestureChanged (int, bool) override {}
    void handleAsyncUpdate() override;

    juce::File file;
    int page = 0, numPages = 0;
    bool twoUp = false;

    InternalParameter* nextParam = nullptr;
    InternalParameter* previousParam = nullptr;
    std::atomic<int> pendingTurns { 0 }; // From the parameters, on whichever thread set them

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScoreViewerNode)
};
