/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-8-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================

    Curve
   Copyright (c) Thomas Derham

   The modifications for Curve are licensed to you solely under the terms of the 
   AGPLv3 license terms as described above.

   CURVE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Plugins/PluginGraph.h"
#include "../AudioDiagnostics.h"

class MainHostWindow;

//==============================================================================
/**
    A panel that displays and edits a PluginGraph.
*/
class GraphEditorPanel final : public Component,
                               public ChangeListener,
                               private Timer
{
public:
    //==============================================================================
    GraphEditorPanel (PluginGraph& graph);
    ~GraphEditorPanel() override;

    void createNewPlugin (const PluginDescriptionAndPreference&, Point<int> position);

    void paint (Graphics&) override;
    void resized() override;

    void mouseDown (const MouseEvent&) override;
    void mouseUp   (const MouseEvent&) override;
    void mouseDrag (const MouseEvent&) override;

    void changeListenerCallback (ChangeBroadcaster*) override;

    //==============================================================================
    void updateComponents();

    //==============================================================================
    void showPopupMenu (Point<int> position);

    /** Returns the AudioProcessorGraph node located under the given panel coordinate, if any. */
    AudioProcessorGraph::Node::Ptr getNodeAt (Point<int> position) const;

    //==============================================================================
    void beginConnectorDrag (AudioProcessorGraph::NodeAndChannel source,
                             AudioProcessorGraph::NodeAndChannel dest,
                             const MouseEvent&);
    void dragConnector (const MouseEvent&);
    void endDraggingConnector (const MouseEvent&);

    //==============================================================================
    PluginGraph& graph;

private:
    struct PluginComponent;
    struct ConnectorComponent;
    struct PinComponent;

    OwnedArray<PluginComponent> nodes;
    OwnedArray<ConnectorComponent> connectors;
    std::unique_ptr<ConnectorComponent> draggingConnector;
    std::unique_ptr<PopupMenu> menu;

    class BurgerButton;
    std::unique_ptr<BurgerButton> burgerButton;

    PluginComponent* getComponentForPlugin (AudioProcessorGraph::NodeID) const;
    ConnectorComponent* getComponentForConnection (const AudioProcessorGraph::Connection&) const;
    PinComponent* findPinAt (Point<float>) const;

    //==============================================================================
    Point<int> originalTouchPos;

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPanel)
};


//==============================================================================
class FadingAudioProcessorPlayer final : public AudioIODeviceCallback
{
public:
    FadingAudioProcessorPlayer() : player (true) {}

    void setProcessor (AudioProcessor* proc)
    {
        player.setProcessor (proc);
    }

    AudioProcessor* getCurrentProcessor() const
    {
        return player.getCurrentProcessor();
    }

    MidiMessageCollector& getMidiMessageCollector()
    {
        return player.getMidiMessageCollector();
    }

    void setMidiOutput (MidiOutput* midiOutputToUse)
    {
        player.setMidiOutput (midiOutputToUse);
    }

    void audioDeviceAboutToStart (AudioIODevice* device) override
    {
        player.audioDeviceAboutToStart (device);
        double sr = (device != nullptr) ? device->getCurrentSampleRate() : 44100.0;
        currentSampleRate.store (sr, std::memory_order_relaxed);
        fadeGain.reset (sr, 0.02);
        fadeGain.setCurrentAndTargetValue (1.0f);
        targetGain.store (1.0f, std::memory_order_relaxed);
        isFadedOut.store (false, std::memory_order_release);
    }

    void audioDeviceStopped() override
    {
        player.audioDeviceStopped();
    }

    void startFadeOut()
    {
        targetGain.store (0.0f, std::memory_order_release);
        isFadedOut.store (false, std::memory_order_release);
    }

    void startFadeIn()
    {
        targetGain.store (1.0f, std::memory_order_release);
        isFadedOut.store (false, std::memory_order_release);
    }

    bool isFullyFadedOut() const
    {
        return isFadedOut.load (std::memory_order_acquire);
    }

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const AudioIODeviceCallbackContext& context) override
    {
        auto startTicks = juce::Time::getHighResolutionTicks();

        player.audioDeviceIOCallbackWithContext (inputChannelData, numInputChannels,
                                                 outputChannelData, numOutputChannels,
                                                 numSamples, context);

        auto endTicks = juce::Time::getHighResolutionTicks();
        double elapsedSec = juce::Time::highResolutionTicksToSeconds (endTicks - startTicks);
        uint32_t elapsedUs = static_cast<uint32_t> (elapsedSec * 1000000.0);

        double sr = currentSampleRate.load (std::memory_order_relaxed);
        if (sr <= 0.0) sr = 48000.0;
        uint32_t deadlineUs = static_cast<uint32_t> ((numSamples / sr) * 1000000.0);

        AudioDiagnostics::getInstance().recordCallbackTiming (elapsedUs, deadlineUs);

        float target = targetGain.load (std::memory_order_acquire);
        if (std::abs (fadeGain.getTargetValue() - target) > 0.0001f)
            fadeGain.setTargetValue (target);

        if (fadeGain.isSmoothing())
        {
            if (numOutputChannels == 2 && outputChannelData[0] != nullptr && outputChannelData[1] != nullptr)
            {
                auto* out0 = outputChannelData[0];
                auto* out1 = outputChannelData[1];
                for (int s = 0; s < numSamples; ++s)
                {
                    float g = fadeGain.getNextValue();
                    out0[s] *= g;
                    out1[s] *= g;
                }
            }
            else
            {
                for (int s = 0; s < numSamples; ++s)
                {
                    float g = fadeGain.getNextValue();
                    for (int ch = 0; ch < numOutputChannels; ++ch)
                    {
                        if (auto* out = outputChannelData[ch])
                            out[s] *= g;
                    }
                }
            }

            if (! fadeGain.isSmoothing() && fadeGain.getCurrentValue() <= 0.0001f)
                isFadedOut.store (true, std::memory_order_release);
        }
        else if (fadeGain.getCurrentValue() <= 0.0001f)
        {
            for (int ch = 0; ch < numOutputChannels; ++ch)
            {
                if (outputChannelData[ch] != nullptr)
                    juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);
            }
            isFadedOut.store (true, std::memory_order_release);
        }
    }

private:
    AudioProcessorPlayer player;
    juce::LinearSmoothedValue<float> fadeGain { 1.0f };
    std::atomic<float> targetGain { 1.0f };
    std::atomic<bool> isFadedOut { false };
    std::atomic<double> currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FadingAudioProcessorPlayer)
};

//==============================================================================
/**
    A panel that embeds a GraphEditorPanel with a midi keyboard at the bottom.

    It also manages the graph itself, and plays it.
*/
class GraphDocumentComponent final : public Component,
                                     public DragAndDropTarget,
                                     public DragAndDropContainer,
                                     private ChangeListener,
                                     private AsyncUpdater
{
public:
    GraphDocumentComponent (AudioPluginFormatManager& formatManager,
                            AudioDeviceManager& deviceManager,
                            KnownPluginList& pluginList);

    ~GraphDocumentComponent() override;

    //==============================================================================
    void createNewPlugin (const PluginDescriptionAndPreference&, Point<int> position);
    bool closeAnyOpenPluginWindows();

    //==============================================================================
    std::unique_ptr<PluginGraph> graph;

    void resized() override;
    void releaseGraph();

    //==============================================================================
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    //==============================================================================
    std::unique_ptr<GraphEditorPanel> graphPanel;

    //==============================================================================
    void showSidePanel (bool isSettingsPanel);
    void hideLastSidePanel();

    BurgerMenuComponent burgerMenu;

    void propagateDeviceSettingsToNodes();

    void startPresetTransition() { graphPlayer.startFadeOut(); }
    void endPresetTransition()   { graphPlayer.startFadeIn(); }
    bool isTransitionFadedOut() const { return graphPlayer.isFullyFadedOut(); }
    void showUpdateBanner (const String& version, const String& downloadUrl);

private:
    //==============================================================================
    AudioDeviceManager& deviceManager;
    KnownPluginList& pluginList;

    FadingAudioProcessorPlayer graphPlayer;
    MidiKeyboardState keyState;
    MidiOutput* midiOutput = nullptr;

    class UpdateBannerComponent;
    std::unique_ptr<UpdateBannerComponent> updateBanner;

    struct TooltipBar;
    std::unique_ptr<TooltipBar> statusBar;

    class TitleBarComponent;
    std::unique_ptr<TitleBarComponent> titleBarComponent;

    //==============================================================================
    struct PluginListBoxModel;
    std::unique_ptr<PluginListBoxModel> pluginListBoxModel;

    ListBox pluginListBox;

    SidePanel mobileSettingsSidePanel { "Settings", 300, true };
    SidePanel pluginListSidePanel    { "Plugins", 250, false };
    SidePanel* lastOpenedSidePanel = nullptr;

    //==============================================================================
    void changeListenerCallback (ChangeBroadcaster*) override;
    void handleAsyncUpdate() override;

    void init();
    void checkAvailableWidth();
    void updateMidiOutput();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphDocumentComponent)
};
