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
#include "PresetSlots.h"
#include "WindowArrangement.h"
#include "../Plugins/MeterNode.h"

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
        setMetering (false);
        setPluginMetering (false);
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
        state.isEditorInView = state.isEditorShowing && WindowArrangement::isInView (mainWindow);

        if (graph != nullptr)
        {
            state.tempo = graph->getTransport().getTempo();
            state.isTransportPlaying = graph->getTransport().isPlaying();
        }

        state.isMuted = OutputMonitor::muted.load();
        state.isDimmed = OutputMonitor::dimmed.load();
        state.isMono = OutputMonitor::mono.load();
        return state;
    }

    //==============================================================================
    void loadPreset (const juce::File& file) override  { mainWindow.loadPreset (file); }
    void assignKey (const juce::File& file, int slot) override { PresetSlots::assign (PresetSlots::Kind::key, file, slot); }

    void setTempo (double bpm) override
    {
        if (auto* graph = getGraph())
            graph->getTransport().setTempo (bpm);
    }

    void tapTempo() override
    {
        if (auto* graph = getGraph())
            graph->getTransport().tap();
    }

    void setTransportPlaying (bool shouldPlay) override
    {
        if (auto* graph = getGraph())
            graph->getTransport().setPlaying (shouldPlay);
    }

    // A Meter plug-in measures only while something shows it, so the panel asks it to while it's open
    MenuBarPanelState::MeterReading readMeter (juce::uint32 id) override
    {
        MenuBarPanelState::MeterReading reading;

        if (! metering)
            return reading;

        auto* graph = getGraph();
        auto node = graph != nullptr ? graph->graph.getNodeForId (NodeID { id }) : nullptr;
        auto* meter = node != nullptr ? dynamic_cast<MeterNode*> (node->getProcessor()) : nullptr;

        if (meter == nullptr)
            return reading;

        if (std::none_of (measured.begin(), measured.end(), [&node] (const auto& n) { return n.get() == node; }))
        {
            meter->addMeasurer();
            measured.push_back (node);
        }

        using Mode = MenuBarPanelState::MeterReading::Mode;
        auto& dsp = meter->getDsp();
        const auto channels = juce::jlimit (1, 2, meter->getTotalNumInputChannels());

        reading.isValid = true;
        reading.mode = (Mode) (int) meter->getMode();
        reading.numChannels = channels;
        reading.vuReferenceDb = meter->getVuReferenceDb();
        reading.momentary = dsp.getMomentaryLufs();
        reading.shortTerm = dsp.getShortTermLufs();
        reading.integrated = dsp.getIntegratedLufs();
        reading.correlation = dsp.getCorrelation();

        for (int ch = 0; ch < channels; ++ch)
        {
            reading.peak[ch] = dsp.consumeSecondPeak (ch);
            reading.truePeak[ch] = dsp.consumeSecondTruePeak (ch);
            reading.rms[ch] = dsp.getRms (ch);
            reading.vu[ch] = dsp.getVu (ch);
            reading.over[ch] = dsp.hasOver (ch);
        }

        if (channels == 1)
        {
            reading.peak[1] = reading.peak[0];
            reading.truePeak[1] = reading.truePeak[0];
            reading.rms[1] = reading.rms[0];
            reading.vu[1] = reading.vu[0];
            reading.over[1] = reading.over[0];
        }

        return reading;
    }
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

    void setMetering (bool shouldMeter) override    { OutputMeter::setEnabled (shouldMeter); }

    void setPluginMetering (bool shouldMeter) override
    {
        metering = shouldMeter;

        if (! shouldMeter)
        {
            for (auto& node : measured)
                if (auto* meter = dynamic_cast<MeterNode*> (node->getProcessor()))
                    meter->removeMeasurer();

            measured.clear();
        }
    }
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

    void togglePluginWindows() override
    {
        if (auto* graph = getGraph())
            graph->togglePluginWindows();
    }

    // Hidden if it's in view; brought to the front if it's open but behind other windows, or on
    // another Space, which is easy to lose with no Dock icon to click
    void toggleGraphEditor() override
    {
        if (mainWindow.isVisible() && WindowArrangement::isInView (mainWindow))
            mainWindow.hideWindow();
        else
            mainWindow.showWindow();
    }

private:
    using NodeID = juce::AudioProcessorGraph::NodeID;

    // The Meter plug-ins the panel has asked to measure while it's open; held, so one removed from
    // the graph meanwhile can still be told to stop
    bool metering = false;
    std::vector<juce::AudioProcessorGraph::Node::Ptr> measured;

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

        // A group's, if it has others in it: as shown, with each group's scenes together
        juce::StringArray shownGroups;

        for (const auto& g : groups)
            shownGroups.add (g.isNotEmpty() && groupSizes[g] > 1 ? g : juce::String());

        const auto keys = PresetSlots::resolveKeys (files, shownGroups);
        const auto assigned = PresetSlots::load (PresetSlots::Kind::key);

        for (int i = 0; i < files.size(); ++i)
        {
            Preset preset;
            preset.name = files[i].getFileNameWithoutExtension();
            preset.file = files[i];
            preset.isCurrent = files[i] == current;
            preset.keySlot = keys[(size_t) i];
            preset.shortcut = PresetSlots::describeKey (preset.keySlot);
            preset.isKeyAssigned = assigned.count (preset.keySlot) > 0 && juce::String (assigned.at (preset.keySlot)) == preset.name;

            if (groups[i].isNotEmpty() && groupSizes[groups[i]] > 1)
            {
                preset.groupColour = PluginGraph::getSceneGroupColour (groups[i]);
                preset.group = groups[i];
            }

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
            plugin.canTurnOff = ! plugin.canBypass && processor->getTotalNumInputChannels() > 0
                             && processor->getTotalNumOutputChannels() == 0 && ! processor->producesMidi();
            plugin.hasEditor = processor->hasEditor() && ! PluginGraph::isConfiguredByAudioSettings (*processor);
            plugin.isWindowOpen = graph.isWindowShowing (*node, PluginWindow::Type::normal);
            plugin.isWindowKeptOnTop = graph.isWindowKeptOnTop (node->nodeID);
            if (auto* meter = dynamic_cast<MeterNode*> (processor))
            {
                plugin.isMeter = true;
                plugin.meterMode = (int) meter->getMode();
            }

            // The graph's I/O and the loopback nodes are set up in Audio Settings, not here
            const bool isIO = dynamic_cast<IOProcessor*> (processor) != nullptr
                           || PluginGraph::isConfiguredByAudioSettings (*processor);

            const bool listed = ! isIO && ! node->properties[PluginGraph::hiddenFromPanelProp]
                             && (plugin.canBypass || plugin.canTurnOff || plugin.hasEditor);

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
