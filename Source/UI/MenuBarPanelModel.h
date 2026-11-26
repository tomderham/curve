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

#include "MainHostWindow.h"
#include "PanelPlugins.h"
#include "MenuBarPanelState.h"
#include "VolumeKeyManager.h"
#include "OutputVolume.h"
#include "OutputMeter.h"
#include "OutputMonitor.h"

//==============================================================================
/**
    What the menu bar panel shows, and what its controls do, apart from AppKit.

    Everything here runs on the message thread and only reads what the graph and device already
    hold: names, bypass and mute flags, the preset list. It never asks a plug-in for its state or
    takes its callback lock, so the panel can't hold up the audio. It listens for changes only
    while the panel is open (see setListening()), and tells the panel once per batch of changes.
*/
class MenuBarPanelModel final : public MenuBarPanelState::Source,
                                private juce::ChangeListener,
                                private juce::AsyncUpdater
{
public:
    using Preset = MenuBarPanelState::Preset;
    using Plugin = MenuBarPanelState::Plugin;
    using State  = MenuBarPanelState::State;

    explicit MenuBarPanelModel (MainHostWindow& w) : mainWindow (w) {}

    ~MenuBarPanelModel() override
    {
        setListening (false);
    }

    void setListening (bool shouldListen) override
    {
        if (shouldListen == isListening)
            return;

        isListening = shouldListen;

        auto* graph = getGraph();

        if (shouldListen)
        {
            if (graph != nullptr)
            {
                graph->addChangeListener (this);
                graph->graph.addChangeListener (this);
                listenedGraph = graph;
            }

            mainWindow.getDeviceManager().addChangeListener (this);
        }
        else
        {
            if (listenedGraph != nullptr)
            {
                listenedGraph->removeChangeListener (this);
                listenedGraph->graph.removeChangeListener (this);
                listenedGraph = nullptr;
            }

            mainWindow.getDeviceManager().removeChangeListener (this);
            cancelPendingUpdate();
        }
    }

    State getState() const override
    {
        State state;
        auto* graph = getGraph();

        addPresets (state, graph);

        if (graph != nullptr)
        {
            // Only a saved preset can have changes; a new graph counts as changed from the start
            state.isPresetModified = graph->getFile().existsAsFile() && graph->hasChangedSinceSaved();
            addPlugins (state, *graph);
            state.latencySamples = graph->graph.getLatencySamples();
            state.numPluginWindows = graph->getNumPluginWindows();
            state.arePluginWindowsHidden = graph->arePluginWindowsHidden();
            state.arePluginWindowPositionsLocked = PluginWindow::arePositionsLocked();
            state.isBypassingAll = graph->isBypassingAll();
        }

        if (auto* device = mainWindow.getDeviceManager().getCurrentAudioDevice())
        {
            state.deviceName = device->getName();
            state.sampleRate = device->getCurrentSampleRate();
            state.bufferSize = device->getCurrentBufferSizeSamples();
        }

        using Kind = MenuBarPanelState::State::VolumeKind;

        switch (VolumeKeyManager::getVolumeControl())
        {
            case VolumeKeyManager::VolumeControl::curve:
                state.volumeKind = Kind::curve;
                state.volumeDescription = VolumeKeyManager::describeVolume();
                break;

            case VolumeKeyManager::VolumeControl::interface:
                state.volumeKind = Kind::interface;
                state.interfaceVolume = VolumeKeyManager::getInterfaceVolume();
                state.volumeDescription = juce::String (juce::roundToInt (state.interfaceVolume * 100.0f)) + "%";
                break;

            case VolumeKeyManager::VolumeControl::none:
                break;
        }

        state.volumeSteps = VolumeKeyManager::getVolumeSteps();
        state.isVolumeMuted = VolumeKeyManager::isVolumeMuted();
        state.maxVolumeSteps = OutputVolume::maxSteps;
        state.isEditorShowing = mainWindow.isVisible();
        state.isMuted = OutputMonitor::muted.load();
        state.isDimmed = OutputMonitor::dimmed.load();
        state.isMono = OutputMonitor::mono.load();
        return state;
    }

    //==============================================================================
    void loadPreset (const juce::File& file) override  { mainWindow.loadPreset (file); }
    void saveAsPreset() override                       { mainWindow.saveAsPreset(); }

    void setBypassed (juce::uint32 id, bool shouldBypass) override
    {
        if (auto* graph = getGraph())
            graph->setNodeBypassed (NodeID (id), shouldBypass);
    }

    void setMuted (juce::uint32 id, bool shouldMute) override
    {
        if (auto* graph = getGraph())
            graph->setNodeMuted (NodeID (id), shouldMute);
    }

    void toggleEditor (juce::uint32 id) override
    {
        if (auto* graph = getGraph())
        {
            if (auto node = graph->graph.getNodeForId (NodeID (id)))
            {
                // Showing: closed. Hidden with the others, or minimised: shown again
                if (graph->isWindowShowing (*node, PluginWindow::Type::normal))
                    graph->closeWindowFor (*node, PluginWindow::Type::normal);
                // In front without taking the keys, which would close the panel
                else if (auto* w = graph->getOrCreateWindowFor (node, PluginWindow::Type::normal))
                    WindowArrangement::bringToFront ({ w });

                triggerAsyncUpdate();
            }
        }
    }

    // Shown again from the node's menu in the graph editor, Show in Menu Bar Panel
    void hideFromPanel (juce::uint32 id) override
    {
        if (auto* graph = getGraph())
        {
            if (auto node = graph->graph.getNodeForId (NodeID (id)))
            {
                node->properties.set (PluginGraph::hiddenFromPanelProp, true);
                graph->changed();
            }
        }
    }

    void setVolume (int steps, bool muted) override  { VolumeKeyManager::setVolume (steps, muted); }
    void setInterfaceVolume (float volume) override  { VolumeKeyManager::setInterfaceVolume (volume); }

    void setWindowKeptOnTop (juce::uint32 id, bool shouldKeepOnTop) override
    {
        if (auto* graph = getGraph())
            graph->setWindowKeptOnTop (NodeID (id), shouldKeepOnTop);
    }

    void setPluginWindowPositionsLocked (bool shouldLock) override
    {
        if (auto* graph = getGraph())
            graph->setPluginWindowPositionsLocked (shouldLock);
    }

    void setMetering (bool shouldMeter) override                 { OutputMeter::setEnabled (shouldMeter); }
    void readOutputPeaks (float& left, float& right) override    { OutputMeter::read (left, right); }

    void setOutputSwitch (MenuBarPanelState::OutputSwitch which, bool on) override
    {
        using S = MenuBarPanelState::OutputSwitch;

        switch (which)
        {
            case S::bypassAll:  if (auto* graph = getGraph()) graph->setBypassingAll (on); break;
            case S::mute:       OutputMonitor::muted.store (on); break;
            case S::dim:        OutputMonitor::dimmed.store (on); break;
            case S::mono:       OutputMonitor::mono.store (on); break;
        }

        triggerAsyncUpdate();
    }

    void arrangePluginWindows() override
    {
        if (auto* graph = getGraph())
            graph->arrangePluginWindows();
    }

    void togglePluginWindows() override
    {
        if (auto* graph = getGraph())
            graph->togglePluginWindows();
    }

    void toggleGraphEditor() override
    {
        if (mainWindow.isVisible())
            mainWindow.hideWindow();
        else
            mainWindow.showWindow();
    }

    /** The menu bar panel's preset shortcuts: 1-9 and 0 for the first ten, then with Option, then
        with Shift, as the menu had. */
    static juce::String getShortcut (int index)
    {
        if (index < 0 || index >= 30)
            return {};

        const auto digit = index % 10 == 9 ? juce::String ("0") : juce::String (index % 10 + 1);

        if (index >= 20) return juce::String::fromUTF8 ("\xe2\x87\xa7") + digit; // ⇧
        if (index >= 10) return juce::String::fromUTF8 ("\xe2\x8c\xa5") + digit; // ⌥
        return digit;
    }

private:
    using NodeID = juce::AudioProcessorGraph::NodeID;

    PluginGraph* getGraph() const
    {
        return mainWindow.graphHolder != nullptr ? mainWindow.graphHolder->graph.get() : nullptr;
    }

    void addPresets (State& state, PluginGraph* graph) const
    {
        // Sorted alphabetically, matching MIDI program change numbers
        const auto files = PluginGraph::getPresetFiles();
        const auto current = graph != nullptr ? graph->getFile() : juce::File();

        juce::StringArray groups;
        std::map<juce::String, int> groupSizes;

        for (const auto& file : files)
        {
            groups.add (PluginGraph::readSceneGroup (file));

            if (groups.strings.getLast().isNotEmpty())
                ++groupSizes[groups.strings.getLast()];
        }

        for (int i = 0; i < files.size(); ++i)
        {
            Preset preset;
            preset.name = files[i].getFileNameWithoutExtension();
            preset.file = files[i];
            preset.isCurrent = files[i] == current;
            preset.shortcut = getShortcut (i);

            if (groups[i].isNotEmpty() && groupSizes[groups[i]] > 1)
                preset.groupColour = PluginGraph::getSceneGroupColour (groups[i]);

            state.presets.add (preset);
        }
    }

    static void addPlugins (State& state, PluginGraph& graph)
    {
        using IOProcessor = juce::AudioProcessorGraph::AudioGraphIOProcessor;

        std::vector<Curve::PanelPlugins::Node> nodes;
        std::map<juce::uint32, Plugin> plugins;

        for (auto* node : graph.graph.getNodes())
        {
            auto* processor = node->getProcessor();

            if (processor == nullptr)
                continue;

            Plugin plugin;
            plugin.id = node->nodeID.uid;
            plugin.canBypass = PluginGraph::canBeBypassed (*processor);
            plugin.isBypassed = node->isBypassed();
            plugin.isMuted = graph.isNodeMuted (node->nodeID);
            plugin.hasEditor = processor->hasEditor() && ! PluginGraph::isConfiguredByAudioSettings (*processor);
            plugin.isWindowOpen = graph.isWindowShowing (*node, PluginWindow::Type::normal);
            plugin.isWindowKeptOnTop = graph.isWindowKeptOnTop (node->nodeID);

            // The graph's I/O and the loopback nodes are set up in Audio Settings, not here
            const bool isIO = dynamic_cast<IOProcessor*> (processor) != nullptr
                           || PluginGraph::isConfiguredByAudioSettings (*processor);

            const bool listed = ! isIO && ! node->properties[PluginGraph::hiddenFromPanelProp]
                             && (plugin.canBypass || plugin.hasEditor);

            nodes.push_back ({ node->nodeID.uid,
                               graph.getNodeDisplayName (node->nodeID).toStdString(),
                               listed,
                               (double) node->properties.getWithDefault ("x", 0.5),
                               (double) node->properties.getWithDefault ("y", 0.5) });

            plugins[node->nodeID.uid] = plugin;
        }

        std::vector<Curve::PanelPlugins::Edge> edges;

        for (const auto& connection : graph.graph.getConnections())
            edges.emplace_back (connection.source.nodeID.uid, connection.destination.nodeID.uid);

        for (const auto& entry : Curve::PanelPlugins::list (nodes, edges))
        {
            auto plugin = plugins[entry.id];
            plugin.label = juce::String (entry.label);
            state.plugins.push_back (plugin);
        }
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        // The graph and device each send their own message; one update covers both
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        // A preset load can replace the graph, which mustn't be left listened to
        if (isListening && getGraph() != listenedGraph.get())
        {
            setListening (false);
            setListening (true);
        }

        if (onChange != nullptr)
            onChange();
    }

    MainHostWindow& mainWindow;
    bool isListening = false;
    juce::WeakReference<PluginGraph> listenedGraph;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MenuBarPanelModel)
};
