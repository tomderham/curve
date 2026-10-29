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
#include "MidiMapping.h"

class PluginGraph;

//==============================================================================
/**
    Receives MIDI from the inputs enabled in Audio Settings and applies MIDI
    mappings to plugin parameters, node bypass and preset selection.

    Mapped messages are consumed; everything else is forwarded unchanged to the
    graph's MIDI input. Matching happens on the MIDI thread, and mappings are
    applied on the message thread.

    In learn mode, moving a control in a plugin's GUI (or clicking a node's
    bypass button) selects a target, and the next CC, note or program change
    received is mapped to it.
*/
class MidiControlEngine final : public MidiInputCallback,
                                private AsyncUpdater,
                                private ChangeListener
{
public:
    explicit MidiControlEngine (MidiMessageCollector& passThroughCollector);
    ~MidiControlEngine() override;

    void setGraph (PluginGraph*);

    // All called on the message thread
    std::function<void (const File&)> onLoadPreset;
    std::function<void (const String&)> onStatusMessage;    // an empty string clears it
    std::function<void()> onLearnStateChanged;

    //==============================================================================
    bool isLearning() const                                     { return learnMode; }
    void setLearning (bool shouldLearn);

    const std::optional<MidiMappingTarget>& getLearnTarget() const { return learnTarget; }
    void selectLearnTarget (const MidiMappingTarget&);
    bool learnTargetHasMapping() const;
    void removeMappingForLearnTarget();

    /** Waits for one MIDI control and assigns it to next/previous preset. */
    void learnPresetStep (bool next, std::function<void (const MidiTrigger&)> onLearned);
    void cancelPresetStepLearn();

    //==============================================================================
    // App-wide controls, kept in user settings rather than in presets
    const MidiMappingSet& getGlobalMappings() const             { return globalMappings; }
    void clearGlobalMappings();

    static bool isProgramChangePresetSelectEnabled();
    void setProgramChangePresetSelectEnabled (bool);

    //==============================================================================
    void handleIncomingMidiMessage (MidiInput*, const MidiMessage&) override;

private:
    struct Table
    {
        std::vector<MidiMapping> mappings;
        bool programChangeSelectsPreset = false;
    };

    struct PendingMessage
    {
        MidiMessage message;
        bool forLearning = false;
    };

    class ParameterWatcher;

    MidiMessageCollector& passThrough;
    PluginGraph* graph = nullptr;
    MidiMappingSet globalMappings;

    SpinLock tableLock;
    std::shared_ptr<const Table> table;

    CriticalSection pendingLock;
    std::vector<PendingMessage> pending;
    bool presetLoaded = false;

    // Learn state (message thread), plus a flag the MIDI thread reads
    bool learnMode = false;
    std::optional<MidiMappingTarget> learnTarget;
    std::optional<MidiMapping> lastLearned;
    uint32 lastLearnedTime = 0;
    std::optional<MidiMappingTarget> presetStepTarget;
    std::function<void (const MidiTrigger&)> onPresetStepLearned;
    std::atomic<bool> capturing { false };
    std::vector<std::unique_ptr<ParameterWatcher>> watchers;

    std::shared_ptr<const Table> getTable() const;
    void rebuildTable();
    void updateCapturing();
    void updateWatchers();
    void saveGlobalMappings();

    void handleAsyncUpdate() override;
    void changeListenerCallback (ChangeBroadcaster*) override;

    void handleLearnMessage (const MidiMessage&);
    void applyMapping (const MidiMapping&, int controlValue);
    void stepPreset (int delta);
    void loadPresetAt (int index);
    void parameterTouched (AudioProcessorGraph::NodeID, int paramIndex);
    String describeTarget (const MidiMappingTarget&) const;
    void showLearnStatus (const String& detail = {});

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiControlEngine)
};
