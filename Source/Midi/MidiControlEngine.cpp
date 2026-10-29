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

#include "MidiControlEngine.h"
#include "../Plugins/PluginGraph.h"
#include "../UI/MainHostWindow.h"

static constexpr const char* globalMappingsKey = "midiPresetMappings";
static constexpr const char* programChangeSelectsPresetKey = "midiProgramChangeSelectsPreset";

// How soon after a learned CC press a release must arrive for the button to count as momentary
static constexpr uint32 momentaryReleaseWindowMs = 1000;

//==============================================================================
// While learning, reports which parameter of a node the user last moved in the
// plugin's GUI. Holding the node keeps its processor alive, so the listeners
// can always be removed safely.
class MidiControlEngine::ParameterWatcher final : private AudioProcessorParameter::Listener,
                                                  private AsyncUpdater
{
public:
    ParameterWatcher (MidiControlEngine& e, AudioProcessorGraph::Node::Ptr n)
        : node (std::move (n)), engine (e)
    {
        for (auto* param : node->getProcessor()->getParameters())
            param->addListener (this);
    }

    ~ParameterWatcher() override
    {
        cancelPendingUpdate();

        for (auto* param : node->getProcessor()->getParameters())
            param->removeListener (this);
    }

    const AudioProcessorGraph::Node::Ptr node;

private:
    // Both may be called on any thread
    void parameterGestureChanged (int index, bool gestureIsStarting) override
    {
        if (! gestureIsStarting)
            return;

        sawGesture = true;
        touched (index, true);
    }

    void parameterValueChanged (int index, float) override
    {
        // Fallback for plugins that never report gestures
        if (! sawGesture)
            touched (index, false);
    }

    void touched (int index, bool fromGesture)
    {
        lastTouchedIndex = index;
        lastTouchWasGesture = fromGesture;
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        // Without a gesture, only count changes made with the mouse, not the plugin's own updates
        if (! lastTouchWasGesture && ! ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown())
            return;

        engine.parameterTouched (node->nodeID, lastTouchedIndex);
    }

    MidiControlEngine& engine;
    std::atomic<bool> sawGesture { false };
    std::atomic<bool> lastTouchWasGesture { false };
    std::atomic<int> lastTouchedIndex { -1 };
};

//==============================================================================
MidiControlEngine::MidiControlEngine (MidiMessageCollector& collector)
    : passThrough (collector)
{
    if (auto* settings = getUserSettings())
        if (auto xml = settings->getXmlValue (globalMappingsKey))
            globalMappings.restoreFromXml (*xml);

    rebuildTable();
}

MidiControlEngine::~MidiControlEngine()
{
    cancelPendingUpdate();
    setGraph (nullptr);
}

void MidiControlEngine::setGraph (PluginGraph* newGraph)
{
    if (graph == newGraph)
        return;

    watchers.clear();
    learnTarget.reset();
    lastLearned.reset();

    if (graph != nullptr)
    {
        graph->removeChangeListener (this);
        graph->graph.removeChangeListener (this);
    }

    graph = newGraph;

    if (graph != nullptr)
    {
        graph->addChangeListener (this);
        graph->graph.addChangeListener (this);
    }

    rebuildTable();
    updateWatchers();
}

//==============================================================================
std::shared_ptr<const MidiControlEngine::Table> MidiControlEngine::getTable() const
{
    const SpinLock::ScopedLockType sl (tableLock);
    return table;
}

void MidiControlEngine::rebuildTable()
{
    auto newTable = std::make_shared<Table>();

    if (graph != nullptr)
        newTable->mappings = graph->getMidiMappings().getMappings();

    for (auto& m : globalMappings.getMappings())
        newTable->mappings.push_back (m);

    newTable->programChangeSelectsPreset = isProgramChangePresetSelectEnabled();

    const SpinLock::ScopedLockType sl (tableLock);
    table = std::move (newTable);
}

void MidiControlEngine::updateCapturing()
{
    capturing = learnMode || presetStepTarget.has_value();
}

void MidiControlEngine::updateWatchers()
{
    if (! learnMode || graph == nullptr)
    {
        watchers.clear();
        return;
    }

    watchers.erase (std::remove_if (watchers.begin(), watchers.end(),
                                    [this] (const auto& w) { return graph->graph.getNodeForId (w->node->nodeID) != w->node.get(); }),
                    watchers.end());

    for (auto* node : graph->graph.getNodes())
    {
        auto* processor = node->getProcessor();

        if (processor == nullptr || processor->getParameters().isEmpty())
            continue;

        const bool alreadyWatched = std::any_of (watchers.begin(), watchers.end(),
                                                 [node] (const auto& w) { return w->node.get() == node; });

        if (! alreadyWatched)
            watchers.push_back (std::make_unique<ParameterWatcher> (*this, node));
    }
}

void MidiControlEngine::changeListenerCallback (ChangeBroadcaster*)
{
    rebuildTable();

    if (! learnMode)
        return;

    updateWatchers();

    if (learnTarget.has_value() && graph->graph.getNodeForId (learnTarget->nodeID) == nullptr)
    {
        learnTarget.reset();
        lastLearned.reset();
        showLearnStatus();

        if (onLearnStateChanged != nullptr)
            onLearnStateChanged();
    }
}

//==============================================================================
void MidiControlEngine::handleIncomingMidiMessage (MidiInput*, const MidiMessage& message)
{
    const bool forLearning = capturing && MidiTrigger::fromMessage (message).has_value();
    bool consume = forLearning;

    if (! consume)
    {
        if (auto t = getTable())
        {
            consume = (t->programChangeSelectsPreset && message.isProgramChange())
                   || std::any_of (t->mappings.begin(), t->mappings.end(),
                                   [&] (const MidiMapping& m) { return m.trigger.matches (message); });
        }
    }

    if (! consume)
    {
        passThrough.addMessageToQueue (message);
        return;
    }

    {
        const ScopedLock sl (pendingLock);
        pending.push_back ({ message, forLearning });
    }

    triggerAsyncUpdate();
}

void MidiControlEngine::handleAsyncUpdate()
{
    std::vector<PendingMessage> messages;

    {
        const ScopedLock sl (pendingLock);
        messages.swap (pending);
    }

    const auto t = getTable();

    // Only the latest value of each absolute mapping matters, so a fast knob doesn't flood the plugin
    std::vector<std::pair<const MidiMapping*, int>> latestAbsoluteValues;
    presetLoaded = false;

    for (auto& p : messages)
    {
        // The rest were meant for the nodes of the previous preset
        if (presetLoaded)
            return;

        if (p.forLearning)
        {
            handleLearnMessage (p.message);
            continue;
        }

        bool matched = false;

        for (auto& m : t->mappings)
        {
            if (! m.trigger.matches (p.message))
                continue;

            matched = true;
            const auto value = MidiMapping::getControlValue (p.message);

            if (m.mode != MidiMapping::Mode::absolute)
            {
                applyMapping (m, value);
                continue;
            }

            auto existing = std::find_if (latestAbsoluteValues.begin(), latestAbsoluteValues.end(),
                                          [&m] (const auto& entry) { return entry.first == &m; });

            if (existing != latestAbsoluteValues.end())
                existing->second = value;
            else
                latestAbsoluteValues.emplace_back (&m, value);
        }

        if (! matched && t->programChangeSelectsPreset && p.message.isProgramChange())
            loadPresetAt (p.message.getProgramChangeNumber());
    }

    if (! presetLoaded)
        for (auto& [mapping, value] : latestAbsoluteValues)
            applyMapping (*mapping, value);
}

void MidiControlEngine::applyMapping (const MidiMapping& m, int controlValue)
{
    const bool pressed = MidiMapping::isPress (controlValue);

    switch (m.target.kind)
    {
        case MidiMappingTarget::Kind::nextPreset:
        case MidiMappingTarget::Kind::previousPreset:
            if (pressed)
                stepPreset (m.target.kind == MidiMappingTarget::Kind::nextPreset ? 1 : -1);
            return;

        case MidiMappingTarget::Kind::bypass:
        {
            if (graph == nullptr)
                return;

            if (auto* node = graph->graph.getNodeForId (m.target.nodeID))
            {
                if (m.mode == MidiMapping::Mode::follow)
                    graph->setNodeBypassed (m.target.nodeID, pressed != m.invert);
                else if (pressed)
                    graph->setNodeBypassed (m.target.nodeID, ! node->isBypassed());
            }

            return;
        }

        case MidiMappingTarget::Kind::parameter:
        {
            if (graph == nullptr)
                return;

            auto* node = graph->graph.getNodeForId (m.target.nodeID);
            auto* processor = node != nullptr ? node->getProcessor() : nullptr;
            auto* param = processor != nullptr ? m.target.findParameter (*processor) : nullptr;

            if (param == nullptr)
                return;

            float newValue = 0.0f;

            switch (m.mode)
            {
                case MidiMapping::Mode::absolute:
                    newValue = m.scale (controlValue);
                    break;

                case MidiMapping::Mode::toggle:
                    if (! pressed)
                        return;

                    newValue = param->getValue() >= 0.5f * (m.minValue + m.maxValue) ? m.minValue : m.maxValue;
                    break;

                case MidiMapping::Mode::follow:
                    newValue = (pressed != m.invert) ? m.maxValue : m.minValue;
                    break;
            }

            if (approximatelyEqual (param->getValue(), newValue))
                return;

            param->beginChangeGesture();
            param->setValueNotifyingHost (newValue);
            param->endChangeGesture();
            return;
        }
    }
}

void MidiControlEngine::stepPreset (int delta)
{
    const auto files = PluginGraph::getPresetFiles();

    if (files.isEmpty())
        return;

    const auto current = graph != nullptr ? files.indexOf (graph->getFile()) : -1;
    const auto index = current < 0 ? (delta > 0 ? 0 : files.size() - 1)
                                    : (current + delta + files.size()) % files.size();

    loadPresetAt (index);
}

void MidiControlEngine::loadPresetAt (int index)
{
    const auto files = PluginGraph::getPresetFiles();

    if (isPositiveAndBelow (index, files.size()) && onLoadPreset != nullptr)
    {
        onLoadPreset (files[index]);
        presetLoaded = true;
    }
}

//==============================================================================
void MidiControlEngine::setLearning (bool shouldLearn)
{
    if (learnMode == shouldLearn)
        return;

    learnMode = shouldLearn;
    learnTarget.reset();
    lastLearned.reset();

    updateCapturing();
    updateWatchers();
    rebuildTable();
    showLearnStatus();

    if (onLearnStateChanged != nullptr)
        onLearnStateChanged();
}

void MidiControlEngine::selectLearnTarget (const MidiMappingTarget& target)
{
    if (! learnMode || learnTarget == target)
        return;

    learnTarget = target;
    lastLearned.reset();
    showLearnStatus();

    if (onLearnStateChanged != nullptr)
        onLearnStateChanged();
}

void MidiControlEngine::parameterTouched (AudioProcessorGraph::NodeID nodeID, int paramIndex)
{
    if (! learnMode || graph == nullptr)
        return;

    if (auto* node = graph->graph.getNodeForId (nodeID))
        if (auto* processor = node->getProcessor(); processor != nullptr && isPositiveAndBelow (paramIndex, processor->getParameters().size()))
            selectLearnTarget (MidiMappingTarget::forParameter (nodeID, *processor, paramIndex));
}

bool MidiControlEngine::learnTargetHasMapping() const
{
    return graph != nullptr && learnTarget.has_value() && graph->getMidiMappings().findFor (*learnTarget) != nullptr;
}

void MidiControlEngine::removeMappingForLearnTarget()
{
    if (! learnTargetHasMapping())
        return;

    graph->removeMidiMappingFor (*learnTarget);
    lastLearned.reset();
    showLearnStatus ("MIDI Learn: removed the mapping for " + describeTarget (*learnTarget));

    if (onLearnStateChanged != nullptr)
        onLearnStateChanged();
}

void MidiControlEngine::handleLearnMessage (const MidiMessage& message)
{
    const auto trigger = MidiTrigger::fromMessage (message);

    if (! trigger.has_value())
        return;

    const auto value = MidiMapping::getControlValue (message);

    if (presetStepTarget.has_value())
    {
        // Wait for a press, so a button's release can't be learned on its own
        if (! MidiMapping::isPress (value))
            return;

        globalMappings.assign ({ *trigger, *presetStepTarget, MidiMapping::Mode::toggle });
        saveGlobalMappings();
        rebuildTable();

        presetStepTarget.reset();
        updateCapturing();

        if (auto callback = std::exchange (onPresetStepLearned, nullptr))
            callback (*trigger);

        return;
    }

    if (! learnMode || graph == nullptr)
        return;

    if (! learnTarget.has_value())
    {
        showLearnStatus ("MIDI Learn: first move a control in a plugin window, or click a bypass button");
        return;
    }

    // More messages from the control just learned
    if (lastLearned.has_value() && lastLearned->trigger == *trigger)
    {
        // A CC button that sends a value on press and 0 on release is momentary, so toggle on each press
        if (lastLearned->mode == MidiMapping::Mode::follow
            && value == 0
            && Time::getMillisecondCounter() - lastLearnedTime < momentaryReleaseWindowMs)
        {
            lastLearned->mode = MidiMapping::Mode::toggle;
            graph->assignMidiMapping (*lastLearned);
        }

        return;
    }

    if (message.isNoteOff())
        return;

    MidiMapping mapping;
    mapping.trigger = *trigger;
    mapping.target = *learnTarget;
    mapping.mode = MidiMapping::getDefaultMode (*trigger, *learnTarget);

    graph->assignMidiMapping (mapping);
    lastLearned = mapping;
    lastLearnedTime = Time::getMillisecondCounter();

    showLearnStatus ("MIDI Learn: mapped " + trigger->describe() + String::fromUTF8 (" \xe2\x86\x92 ") + describeTarget (*learnTarget));

    if (onLearnStateChanged != nullptr)
        onLearnStateChanged();
}

void MidiControlEngine::learnPresetStep (bool next, std::function<void (const MidiTrigger&)> onLearned)
{
    presetStepTarget = MidiMappingTarget::forPresetStep (next);
    onPresetStepLearned = std::move (onLearned);
    updateCapturing();
}

void MidiControlEngine::cancelPresetStepLearn()
{
    presetStepTarget.reset();
    onPresetStepLearned = nullptr;
    updateCapturing();
}

String MidiControlEngine::describeTarget (const MidiMappingTarget& target) const
{
    if (graph == nullptr)
        return {};

    if (! target.isNodeTarget())
        return graph->describeMidiMappingTarget (target);

    return graph->getNodeDisplayName (target.nodeID) + String::fromUTF8 (" \xe2\x80\xba ") + graph->describeMidiMappingTarget (target);
}

void MidiControlEngine::showLearnStatus (const String& detail)
{
    if (onStatusMessage == nullptr)
        return;

    if (! learnMode)
    {
        onStatusMessage ({});
        return;
    }

    if (detail.isNotEmpty())
    {
        onStatusMessage (detail);
        return;
    }

    if (! learnTarget.has_value())
    {
        onStatusMessage ("MIDI Learn: move a control in a plugin window, or click a bypass button  "
                         + String::fromUTF8 ("\xe2\x80\xa2") + "  Esc to finish");
        return;
    }

    String message = "MIDI Learn: " + describeTarget (*learnTarget) + " selected, now move a MIDI control";

    if (auto* existing = graph->getMidiMappings().findFor (*learnTarget))
        message << " (currently " << existing->trigger.describe() << ", Delete removes it)";

    onStatusMessage (message);
}

//==============================================================================
void MidiControlEngine::clearGlobalMappings()
{
    globalMappings.clear();
    saveGlobalMappings();
    rebuildTable();
}

void MidiControlEngine::saveGlobalMappings()
{
    if (auto* settings = getUserSettings())
    {
        settings->setValue (globalMappingsKey, globalMappings.toXml().get());
        settings->saveIfNeeded();
    }
}

bool MidiControlEngine::isProgramChangePresetSelectEnabled()
{
    if (auto* settings = getUserSettings())
        return settings->getBoolValue (programChangeSelectsPresetKey, false);

    return false;
}

void MidiControlEngine::setProgramChangePresetSelectEnabled (bool shouldBeEnabled)
{
    if (auto* settings = getUserSettings())
    {
        settings->setValue (programChangeSelectsPresetKey, shouldBeEnabled);
        settings->saveIfNeeded();
    }

    rebuildTable();
}
