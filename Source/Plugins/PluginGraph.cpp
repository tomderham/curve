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
*/

#include <JuceHeader.h>
#include "../UI/MainHostWindow.h"
#include "PluginGraph.h"
#include "InternalPlugins.h"
#include "MeterNode.h"
#include "LooperNode.h"
#include "PlayerNode.h"
#include "OutputInterfaceLoopbackNode.h"
#include "../UI/GraphEditorPanel.h"
#include "../AudioConstants.h"
#include "../AudioDiagnostics.h"
#include "../UI/CurveColours.h"


//==============================================================================
// JUCE invokes AUv3 creation callbacks directly from AudioComponentInstantiate's
// completion handler, which can run on an arbitrary queue. Everything our
// callbacks touch (WeakReference, the graph, UI) is message-thread only.
static AudioPluginFormat::PluginCreationCallback callOnMessageThread (AudioPluginFormat::PluginCreationCallback userCallback)
{
    return [callback = std::move (userCallback)] (std::unique_ptr<AudioPluginInstance> instance, const String& error)
    {
        if (MessageManager::getInstance()->isThisTheMessageThread())
        {
            callback (std::move (instance), error);
            return;
        }

        // std::function must be copyable, so the instance can't be captured as a unique_ptr
        auto heldInstance = std::make_shared<std::unique_ptr<AudioPluginInstance>> (std::move (instance));
        MessageManager::callAsync ([callback, heldInstance, error] { callback (std::move (*heldInstance), error); });
    };
}


//==============================================================================
PluginGraph::PluginGraph (AudioPluginFormatManager& fm, KnownPluginList& kpl)
    : FileBasedDocument (getFilenameSuffix(),
                         getFilenameWildcard(),
                         "Load a graph",
                         "Save a graph"),
      formatManager (fm),
      knownPlugins (kpl)
{
    newDocument();
    graph.addListener (this);

    displayChangeNotifier = createMacOSDisplayChangeNotifier ([weakSelf = WeakReference<PluginGraph> (this)]
    {
        if (auto* self = weakSelf.get())
        {
            for (auto* w : self->activePluginWindows)
                w->returnToSavedDisplay();

            self->updateBackdropSoon();
        }
    });
}

PluginGraph::~PluginGraph()
{
    graph.removeListener (this);
    graph.removeChangeListener (this);
    closeAnyOpenPluginWindows();
    graph.clear();
}

//==============================================================================
void PluginGraph::changeListenerCallback (ChangeBroadcaster*)
{
    changed();

    for (int i = activePluginWindows.size(); --i >= 0;)
    {
        if (! graph.getNodes().contains (activePluginWindows.getUnchecked (i)->node))
        {
            activePluginWindows.remove (i);
            updateBackdropSoon();
        }
    }
}

AudioProcessorGraph::Node::Ptr PluginGraph::getNodeForName (const String& name) const
{
    for (auto* node : graph.getNodes())
        if (auto p = node->getProcessor())
            if (p->getName().equalsIgnoreCase (name))
                return node;

    return nullptr;
}

void PluginGraph::addPlugin (const PluginDescriptionAndPreference& desc, Point<double> pos)
{
    juce::WeakReference<PluginGraph> weakSelf (this);

    formatManager.createPluginInstanceAsync (desc.pluginDescription,
                                             graph.getSampleRate(),
                                             graph.getBlockSize(),
                                             callOnMessageThread ([weakSelf, pos, useARA = desc.useARA] (std::unique_ptr<AudioPluginInstance> instance, const String& error)
                                             {
                                                 if (auto* self = weakSelf.get())
                                                     self->addPluginCallback (std::move (instance), error, pos, useARA);
                                             }));
}

void PluginGraph::addPluginCallback (std::unique_ptr<AudioPluginInstance> instance,
                                     const String& error,
                                     Point<double> pos,
                                     PluginDescriptionAndPreference::UseARA useARA)
{
    if (instance == nullptr)
    {
        auto options = MessageBoxOptions::makeOptionsOk (MessageBoxIconType::WarningIcon,
                                                         TRANS ("Couldn't create plugin"),
                                                         error);
        messageBox = AlertWindow::showScopedAsync (options, nullptr);
    }
    else
    {
       #if JUCE_PLUGINHOST_ARA
        if (useARA == PluginDescriptionAndPreference::UseARA::yes
            && instance->getPluginDescription().hasARAExtension)
        {
            instance = std::make_unique<ARAPluginInstanceWrapper> (std::move (instance));
        }
       #endif

        // A meter starts as a branch: its output stays off until I/O configuration enables it
        if (dynamic_cast<MeterNode*> (instance.get()) == nullptr)
            instance->enableAllBuses();

        // A plug-in with many outputs, such as a multi-output instrument, starts with only its main
        // output, as a DAW loads it, so it isn't a wide block of pins. Configure Audio I/O turns the
        // others on. Inputs stay on, so a sidechain can be connected. One that can't turn its extra
        // outputs off keeps them running, with their pins hidden
        bool hideExtraOutputs = false;

        if (instance->getPluginDescription().pluginFormatName != Curve::AudioConstants::internalPluginFormat
            && instance->getBusCount (false) > 1)
        {
            auto layout = instance->getBusesLayout();

            for (int i = 1; i < layout.outputBuses.size(); ++i)
                layout.outputBuses.getReference (i) = AudioChannelSet::disabled();

            hideExtraOutputs = ! instance->setBusesLayout (layout);
        }

        const auto numOutputBuses = instance->getBusCount (false);

        if (auto node = graph.addNode (std::move (instance)))
        {
            node->properties.set ("x", pos.x);
            node->properties.set ("y", pos.y);
            node->properties.set ("useARA", useARA == PluginDescriptionAndPreference::UseARA::yes);

            if (hideExtraOutputs)
                for (int i = 1; i < numOutputBuses; ++i)
                    setBusHidden (node->nodeID, false, i, true);

            changed();
        }
    }
}

void PluginGraph::setNodePosition (NodeID nodeID, Point<double> pos)
{
    if (auto* n = graph.getNodeForId (nodeID))
    {
        n->properties.set ("x", jlimit (0.0, 1.0, pos.x));
        n->properties.set ("y", jlimit (0.0, 1.0, pos.y));
    }
}

Point<double> PluginGraph::getNodePosition (NodeID nodeID) const
{
    if (auto* n = graph.getNodeForId (nodeID))
        return { static_cast<double> (n->properties ["x"]),
                 static_cast<double> (n->properties ["y"]) };

    return {};
}

void PluginGraph::setNodePositionFree (NodeID nodeID, bool isFree)
{
    if (auto* n = graph.getNodeForId (nodeID))
    {
        if (isFree)
            n->properties.set (freePositionProp, true);
        else
            n->properties.remove (freePositionProp);
    }
}

bool PluginGraph::isNodePositionFree (NodeID nodeID) const
{
    auto* n = graph.getNodeForId (nodeID);
    return n != nullptr && n->properties[freePositionProp];
}

// Hidden ports are stored as tokens such as "in1 outMidi"
static String getHiddenPortToken (bool isInput, int busIndex)
{
    return (isInput ? "in" : "out") + (busIndex == PluginGraph::midiBus ? String ("Midi") : String (busIndex));
}

void PluginGraph::setBusHidden (NodeID nodeID, bool isInput, int busIndex, bool shouldBeHidden)
{
    auto* n = graph.getNodeForId (nodeID);

    if (n == nullptr || isBusHidden (nodeID, isInput, busIndex) == shouldBeHidden)
        return;

    auto tokens = StringArray::fromTokens (n->properties[hiddenPortsProp].toString(), false);
    const auto token = getHiddenPortToken (isInput, busIndex);

    if (shouldBeHidden)
        tokens.add (token);
    else
        tokens.removeString (token);

    if (tokens.isEmpty())
        n->properties.remove (hiddenPortsProp);
    else
        n->properties.set (hiddenPortsProp, tokens.joinIntoString (" "));

    if (shouldBeHidden)
    {
        auto& processor = *n->getProcessor();
        int firstChannel = 0, numChannels = 1;

        if (busIndex == midiBus)
        {
            firstChannel = AudioProcessorGraph::midiChannelIndex;
        }
        else
        {
            for (int i = 0; i < busIndex; ++i)
                firstChannel += processor.getChannelCountOfBus (isInput, i);

            numChannels = processor.getChannelCountOfBus (isInput, busIndex);
        }

        for (const auto& c : graph.getConnections())
        {
            const auto& end = isInput ? c.destination : c.source;

            if (end.nodeID == nodeID && end.channelIndex >= firstChannel && end.channelIndex < firstChannel + numChannels)
                graph.removeConnection (c);
        }
    }

    changed();
}

bool PluginGraph::isBusHidden (NodeID nodeID, bool isInput, int busIndex) const
{
    auto* n = graph.getNodeForId (nodeID);

    return n != nullptr
        && StringArray::fromTokens (n->properties[hiddenPortsProp].toString(), false)
               .contains (getHiddenPortToken (isInput, busIndex));
}

//==============================================================================
// The graph deletes removed nodes up to 500ms later (with its old render sequence),
// so deactivate loopback nodes now, or the tap health check rebuilds the tap for
// nodes that are already gone.
static void deactivateIfLoopback (AudioProcessorGraph::Node& node)
{
    if (auto* loopback = dynamic_cast<OutputInterfaceLoopbackNode*> (node.getProcessor()))
        loopback->setActiveState (false);
}

void PluginGraph::removeNode (NodeID nodeID)
{
    if (auto node = graph.getNodeForId (nodeID))
        deactivateIfLoopback (*node);

    midiMappings.removeForNode (nodeID);
    statesAsSaved.erase (nodeID.uid);
    bypassedForComparison.erase (nodeID.uid);
    ringOutEnds.erase (nodeID.uid);
    graph.removeNode (nodeID);
}

void PluginGraph::clear()
{
    bypassedForComparison.clear();
    WindowLock::locked.store (false); // A new graph's windows are free, until its scene says otherwise
    ++restoreGeneration;
    pendingAsyncRestores = 0;
    pluginsFailedToRestore.clear();
    pluginsMissingFromLoad.clear();
    restoredConnections.clear();
    midiMappings.clear();
    sceneGroup.clear();
    loadedXml.reset();
    statesAsSaved.clear();
    ringOutEnds.clear();
    stopTimer();

    closeAnyOpenPluginWindows();

    for (auto* node : graph.getNodes())
        deactivateIfLoopback (*node);

    graph.clear();
    changed();
}

bool PluginGraph::canBeBypassed (AudioProcessor& processor)
{
    // Nor a node with no audio or MIDI, e.g. the Score Viewer, which has nothing to pass through
    return dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor*> (&processor) == nullptr
        && dynamic_cast<MeterNode*> (&processor) == nullptr
        && (processor.getTotalNumInputChannels() > 0 || processor.getTotalNumOutputChannels() > 0
            || processor.acceptsMidi() || processor.producesMidi());
}

bool PluginGraph::isConfiguredByAudioSettings (AudioProcessor& processor)
{
    using IONode = AudioProcessorGraph::AudioGraphIOProcessor;

    if (auto* ioNode = dynamic_cast<IONode*> (&processor))
        return ioNode->getType() == IONode::audioInputNode || ioNode->getType() == IONode::audioOutputNode;

    if (auto* plugin = dynamic_cast<AudioPluginInstance*> (&processor))
        return plugin->getPluginDescription().category == "Audio I/O";

    return false;
}

PluginWindow* PluginGraph::getOrCreateWindowFor (AudioProcessorGraph::Node* node, PluginWindow::Type type)
{
    jassert (node != nullptr);

    // One already open, which may be hidden with the others (see togglePluginWindows())
    for (auto* w : activePluginWindows)
    {
        if (w->node.get() == node && w->type == type)
        {
            if (w->isMinimised())
                w->setMinimised (false);

            if (! w->isVisible())
            {
                w->setVisible (true);
                sendChangeMessage();
                updateBackdropSoon();
            }

            return w;
        }
    }

    if (auto* processor = node->getProcessor())
    {
        if (isConfiguredByAudioSettings (*processor))
        {
            getCommandManager().invokeDirectly (CommandIDs::showAudioSettings, false);
            return nullptr;
        }

        if (auto* plugin = dynamic_cast<AudioPluginInstance*> (processor))
        {
            // A plug-in has only one editor. If one of these windows still has it, e.g. one of
            // another type, that's shown rather than opening another with JUCE's generic controls,
            // which it falls back to. Only these windows are looked in: the plug-in's own record of
            // its editor can outlive the editor if its editor doesn't clear it
            if (type == PluginWindow::Type::normal)
            {
                if (auto* editor = processor->getActiveEditor())
                {
                    for (auto* existing : activePluginWindows)
                    {
                        if (existing->getContentComponent() == editor)
                        {
                            if (existing->isMinimised())
                                existing->setMinimised (false);

                            existing->setVisible (true);
                            return existing;
                        }
                    }
                }
            }

            auto* window = activePluginWindows.add (new PluginWindow (node,
                                                                      type,
                                                                      activePluginWindows,
                                                                      getCommandManager().getKeyMappings()));
            window->setAlwaysOnTop (pluginWindowsFloating || window->isKeptOnTop());

            window->onClosed = [weakSelf = WeakReference<PluginGraph> (this)]
            {
                if (auto* self = weakSelf.get())
                {
                    self->sendChangeMessage();
                    self->updateBackdropSoon();
                }
            };

            window->onBoundsChanged = [weakSelf = WeakReference<PluginGraph> (this)]
            {
                if (auto* self = weakSelf.get())
                    self->updateBackdropSoon();
            };

            updateBackdropSoon();
            sendChangeMessage();

            // As its node is named, so a renamed one matches. Its format is in the node's tooltip
            window->setName (getNodeDisplayName (node->nodeID));
            return window;
        }
    }

    return nullptr;
}

void PluginGraph::updatePluginWindowTitles()
{
    for (auto* w : activePluginWindows)
        if (const auto name = getNodeDisplayName (w->node->nodeID); w->getName() != name)
            w->setName (name);
}

bool PluginGraph::closeAnyOpenPluginWindows()
{
    backdrop.hide();

    bool wasEmpty = activePluginWindows.isEmpty();
    activePluginWindows.clear();

    if (! wasEmpty)
        sendChangeMessage();

    return ! wasEmpty;
}

void PluginGraph::openPresetPluginWindows()
{
    // New windows open on the Space being shown. If those already open, which a scene switch
    // keeps, are all on another, they come to this one first, so the set isn't split between two
    const auto opensNew = [this]
    {
        for (auto* node : graph.getNodes())
            if (node->properties[openWithPresetProp] && ! hasWindowFor (*node, PluginWindow::Type::normal))
                return true;

        return false;
    }();

    if (opensNew && ! activePluginWindows.isEmpty())
    {
        const auto anyOnThisSpace = std::any_of (activePluginWindows.begin(), activePluginWindows.end(),
                                                 [] (PluginWindow* w) { return WindowArrangement::isOnActiveSpace (*w); });

        if (! anyOnThisSpace)
            for (auto* w : activePluginWindows)
                WindowArrangement::moveToActiveSpace (*w);
    }

    for (auto* node : graph.getNodes())
        if (node->properties[openWithPresetProp])
            getOrCreateWindowFor (node, PluginWindow::Type::normal);

    // Those already open too: a scene switch keeps them, wherever they've got to behind other apps,
    // or hidden
    showPluginWindows();
}

void PluginGraph::setBypassingAll (bool shouldBypass)
{
    if (shouldBypass == isBypassingAll())
        return;

    // As restoring does: the bypasses notify the graph, which would mark the preset as changed
    const bool wasChanged = hasChangedSinceSaved();
    graph.removeChangeListener (this);

    if (shouldBypass)
    {
        for (auto* node : graph.getNodes())
        {
            auto* processor = node->getProcessor();

            // Effects only: an instrument, with no audio in, would go silent, as would a loop or a
            // backing track, which are what's being played along to rather than an effect on it
            if (processor == nullptr || ! canBeBypassed (*processor) || isConfiguredByAudioSettings (*processor)
                || dynamic_cast<LooperNode*> (processor) != nullptr || dynamic_cast<PlayerNode*> (processor) != nullptr
                || processor->getTotalNumInputChannels() == 0 || node->isBypassed() || isNodeMuted (node->nodeID))
                continue;

            bypassedForComparison.insert (node->nodeID.uid);
            const ScopedValueSetter<bool> comparing (isApplyingComparison, true);
            setNodeBypassed (node->nodeID, true);
        }
    }
    else
    {
        const ScopedValueSetter<bool> comparing (isApplyingComparison, true);

        for (const auto uid : std::exchange (bypassedForComparison, {}))
            setNodeBypassed (NodeID (uid), false);
    }

    setChangedFlag (wasChanged);

    MessageManager::callAsync ([weakSelf = WeakReference<PluginGraph> (this), wasChanged, generation = restoreGeneration]
    {
        if (auto* self = weakSelf.get())
        {
            if (self->restoreGeneration == generation)
                self->setChangedFlag (wasChanged);

            self->graph.addChangeListener (self);
        }
    });

    sendChangeMessage();
}

void PluginGraph::arrangePluginWindows()
{
    using namespace Curve::WindowPlacement;

    showPluginWindows();

    // In the order they're in now, reading across and down, so the result is like the layout it tidies
    Array<PluginWindow*> windows;

    for (auto* w : activePluginWindows)
        if (w->isVisible())
            windows.add (w);

    if (windows.isEmpty())
        return;

    std::stable_sort (windows.begin(), windows.end(), [] (const PluginWindow* a, const PluginWindow* b)
    {
        return a->getY() / 40 != b->getY() / 40 ? a->getY() < b->getY() : a->getX() < b->getX();
    });

    const auto displays = WindowArrangement::getDisplays();
    const auto* target = findLayoutDisplay (displays, windows);

    if (target == nullptr)
        return;

    std::vector<Size> sizes;

    for (auto* w : windows)
        sizes.push_back ({ w->getWidth(), w->getHeight() });

    const auto positions = arrange (sizes, target->usable, windows.getFirst()->hasTitleBar() ? titleBarHeight : 0, 10);

    for (int i = 0; i < windows.size(); ++i)
        windows[i]->moveContentTo (positions[(size_t) i].x, positions[(size_t) i].y);

    bringPluginWindowsToFront();
}

const Curve::WindowPlacement::Display* PluginGraph::findLayoutDisplay (const std::vector<Curve::WindowPlacement::Display>& displays,
                                                                       const Array<PluginWindow*>& windows)
{
    using namespace Curve::WindowPlacement;
    std::map<std::string, int> counts;

    for (auto* w : windows)
        if (const auto* d = findDisplayFor (displays, { w->getX(), w->getY(), w->getWidth(), w->getHeight() }))
            ++counts[d->id];

    const Display* target = findMainDisplay (displays);

    for (const auto& [id, count] : counts)
        if (target == nullptr || count > counts[target->id])
            target = findDisplay (displays, id);

    return target;
}

void PluginGraph::updateBackdropSoon()
{
    if (std::exchange (backdropUpdatePending, true))
        return;

    MessageManager::callAsync ([weakSelf = WeakReference<PluginGraph> (this)]
    {
        if (auto* self = weakSelf.get())
        {
            self->backdropUpdatePending = false;
            self->updateBackdrop();
        }
    });
}

void PluginGraph::updateBackdrop()
{
    using namespace Curve::WindowPlacement;

    if (! PluginWindow::arePositionsLocked() || ! PluginWindow::isBackdropShownWhenLocked())
    {
        backdrop.hide();
        return;
    }

    Array<PluginWindow*> windows;

    for (auto* w : activePluginWindows)
        if (w->isVisible() && ! w->isMinimised())
            windows.add (w);

    const auto displays = WindowArrangement::getDisplays();
    const auto* display = windows.isEmpty() ? nullptr : findLayoutDisplay (displays, windows);

    if (display == nullptr)
    {
        backdrop.hide();
        return;
    }

    // Those on the layout's display, with their title bars if they show them
    std::vector<Rect> frames;
    Array<Component*> inFront;

    // Back to front, as they're stacked
    std::stable_sort (windows.begin(), windows.end(), [] (const PluginWindow* a, const PluginWindow* b)
    {
        return a->getStackPosition() < b->getStackPosition();
    });

    for (auto* w : windows)
    {
        // A window kept on top, or floating for MIDI Learn, would drop to the backdrop's level as its
        // child, so it's left free
        if (w->isAlwaysOnTop())
            continue;

        const auto titleBar = w->hasTitleBar() ? titleBarHeight : 0;
        const Rect frame { w->getX(), w->getY() - titleBar, w->getWidth(), w->getHeight() + titleBar };

        if (const auto* d = findDisplayFor (displays, frame); d != nullptr && d->id == display->id)
        {
            frames.push_back (frame);
            inFront.add (w);
        }
    }

    backdrop.show (backdropArea (frames, display->usable, 16), inFront, Curve::Colours::canvas, PluginWindow::isShownOnAllSpaces());
}

void PluginGraph::togglePluginWindows()
{
    if (activePluginWindows.isEmpty())
        return;

    const auto anyShowing = std::any_of (activePluginWindows.begin(), activePluginWindows.end(),
                                         [] (PluginWindow* w) { return w->isVisible() && ! w->isMinimised(); });

    if (anyShowing)
    {
        for (auto* w : activePluginWindows)
            w->setVisible (false);
    }
    else
    {
        showPluginWindows();
    }

    sendChangeMessage();
    updateBackdropSoon();
}

bool PluginGraph::arePluginWindowsHidden() const
{
    return ! activePluginWindows.isEmpty()
        && std::none_of (activePluginWindows.begin(), activePluginWindows.end(),
                         [] (PluginWindow* w) { return w->isVisible() && ! w->isMinimised(); });
}

void PluginGraph::showPluginWindows()
{
    for (auto* w : activePluginWindows)
    {
        if (w->isMinimised())
            w->setMinimised (false);

        w->setVisible (true);
    }

    bringPluginWindowsToFront();
    updateBackdropSoon();
}

void PluginGraph::bringPluginWindowsToFront()
{
    Array<PluginWindow*> windows;

    for (auto* w : activePluginWindows)
        windows.add (w);

    // As they were last stacked. Stable, so windows never brought forward keep the order they opened in
    std::stable_sort (windows.begin(), windows.end(), [] (const PluginWindow* a, const PluginWindow* b)
    {
        return a->getStackPosition() < b->getStackPosition();
    });

    Array<Component*> backToFront;

    for (auto* w : windows)
        backToFront.add (w);

    WindowArrangement::bringToFront (backToFront);
}

bool PluginGraph::hasWindowFor (const AudioProcessorGraph::Node& node, PluginWindow::Type type) const
{
    return std::any_of (activePluginWindows.begin(), activePluginWindows.end(),
                        [&] (const PluginWindow* w) { return w->node.get() == &node && w->type == type; });
}

bool PluginGraph::isWindowShowing (const AudioProcessorGraph::Node& node, PluginWindow::Type type) const
{
    return std::any_of (activePluginWindows.begin(), activePluginWindows.end(), [&] (const PluginWindow* w)
    {
        return w->node.get() == &node && w->type == type && w->isVisible() && ! w->isMinimised();
    });
}

void PluginGraph::closeWindowFor (const AudioProcessorGraph::Node& node, PluginWindow::Type type)
{
    for (int i = activePluginWindows.size(); --i >= 0;)
        if (activePluginWindows.getUnchecked (i)->node.get() == &node && activePluginWindows.getUnchecked (i)->type == type)
            activePluginWindows.remove (i);

    sendChangeMessage();
    updateBackdropSoon();
}

void PluginGraph::setWindowKeptOnTop (NodeID nodeID, bool shouldKeepOnTop)
{
    if (auto* node = graph.getNodeForId (nodeID))
    {
        const auto prop = PluginWindow::getOnTopProp (PluginWindow::Type::normal);

        if (shouldKeepOnTop)
            node->properties.set (prop, true);
        else
            node->properties.remove (prop);

        for (auto* w : activePluginWindows)
            if (w->node.get() == node)
                w->setAlwaysOnTop (pluginWindowsFloating || w->isKeptOnTop());

        sendChangeMessage();
        updateBackdropSoon();
    }
}

bool PluginGraph::isWindowKeptOnTop (NodeID nodeID) const
{
    if (auto* node = graph.getNodeForId (nodeID))
        return node->properties[PluginWindow::getOnTopProp (PluginWindow::Type::normal)];

    return false;
}

void PluginGraph::setPluginWindowPositionsLocked (bool shouldLock)
{
    if (shouldLock == PluginWindow::arePositionsLocked())
        return;

    applyWindowLock (shouldLock);

    // Part of the scene, as its window layout is, so saved with it
    changed();
}

void PluginGraph::applyWindowLock (bool shouldLock)
{
    WindowLock::locked.store (shouldLock);

    for (auto* w : activePluginWindows)
        w->applyLock (shouldLock);

    sendChangeMessage();
    updateBackdropSoon();
}

void PluginGraph::setPluginWindowBackdropShownWhenLocked (bool shouldShow)
{
    PluginWindow::setBackdropShownWhenLocked (shouldShow);
    updateBackdropSoon();
}

void PluginGraph::setPluginWindowTitleBarsHiddenWhenLocked (bool shouldHide)
{
    PluginWindow::setTitleBarsHiddenWhenLocked (shouldHide);

    for (auto* w : activePluginWindows)
        w->applyLock (PluginWindow::arePositionsLocked());

    updateBackdropSoon();
}

void PluginGraph::setPluginWindowsOnAllSpaces (bool shouldBeOnAll)
{
    PluginWindow::setShownOnAllSpaces (shouldBeOnAll);

    for (auto* w : activePluginWindows)
        WindowArrangement::setOnAllSpaces (*w, shouldBeOnAll);

    updateBackdropSoon();
}

void PluginGraph::setPluginWindowsFloating (bool shouldFloat)
{
    pluginWindowsFloating = shouldFloat;

    updateBackdropSoon();

    for (auto* w : activePluginWindows)
    {
        w->setAlwaysOnTop (shouldFloat || w->isKeptOnTop());

        // Bring back any that the editor covered when learning was switched on
        if (shouldFloat)
            w->toFront (false);
    }
}

void PluginGraph::setNodeBypassed (NodeID nodeID, bool shouldBeBypassed)
{
    // Changed other than by Bypass All, it's no longer only bypassed to compare
    if (! isApplyingComparison)
        bypassedForComparison.erase (nodeID.uid);

    if (auto* node = graph.getNodeForId (nodeID))
    {
        if (node->isBypassed() == shouldBeBypassed
            || (shouldBeBypassed && ! canBeBypassed (*node->getProcessor())))
            return;

        // A plug-in with a bypass parameter keeps it in its state. If that's unchanged since it was
        // loaded or saved apart from this, it still is: bypass is each scene's own
        auto saved = statesAsSaved.find (nodeID.uid);
        bool wasAsSaved = false;

        if (saved != statesAsSaved.end() && node->getProcessor()->getBypassParameter() != nullptr)
        {
            MemoryBlock before;
            node->getProcessor()->getStateInformation (before);
            wasAsSaved = before.toBase64Encoding() == saved->second.reported;
        }

        applyBypass (*node, shouldBeBypassed);

        if (wasAsSaved)
        {
            MemoryBlock after;
            node->getProcessor()->getStateInformation (after);
            saved->second.reported = after.toBase64Encoding();
        }

        graph.sendChangeMessage();
    }
}

String PluginGraph::getNodeDisplayName (NodeID nodeID) const
{
    if (auto* node = graph.getNodeForId (nodeID))
    {
        const auto customName = node->properties.getWithDefault ("customNodeName", String()).toString();

        if (customName.isNotEmpty())
            return customName;

        if (auto* processor = node->getProcessor())
            return processor->getName();
    }

    return {};
}

//==============================================================================
void PluginGraph::assignMidiMapping (const MidiMapping& mapping)
{
    midiMappings.assign (mapping);
    changed();
}

void PluginGraph::removeMidiMappingFor (const MidiMappingTarget& target)
{
    if (midiMappings.removeFor (target))
        changed();
}

void PluginGraph::removeMidiMappingsForNode (NodeID nodeID)
{
    if (midiMappings.removeForNode (nodeID))
        changed();
}

String PluginGraph::describeMidiMappingTarget (const MidiMappingTarget& target) const
{
    switch (target.kind)
    {
        case MidiMappingTarget::Kind::bypass:           return "Bypass";
        case MidiMappingTarget::Kind::mute:             return "Mute";
        case MidiMappingTarget::Kind::tempo:            return "Tempo";
        case MidiMappingTarget::Kind::tapTempo:         return "Tap Tempo";
        case MidiMappingTarget::Kind::transport:        return "Start/Stop";
        case MidiMappingTarget::Kind::nextPreset:       return "Next Preset";
        case MidiMappingTarget::Kind::previousPreset:   return "Previous Preset";
        case MidiMappingTarget::Kind::pluginWindows:    return "Show/Hide Plug-in Windows";
        case MidiMappingTarget::Kind::outputMono:       return "Mono";
        case MidiMappingTarget::Kind::outputDim:        return "Dim";
        case MidiMappingTarget::Kind::outputMute:       return "Mute Output";
        case MidiMappingTarget::Kind::parameter:        break;
    }

    if (auto* node = graph.getNodeForId (target.nodeID))
        if (auto* processor = node->getProcessor())
            if (auto* param = target.findParameter (*processor))
                return param->getName (64);

    return target.paramID.isNotEmpty() ? target.paramID : "Parameter " + String (target.paramIndex + 1);
}

String PluginGraph::describeMidiMappingsForNode (NodeID nodeID) const
{
    StringArray items;

    for (auto& m : midiMappings.getMappings())
        if (m.target.isNodeTarget() && m.target.nodeID == nodeID)
            items.add (m.trigger.describe() + String::fromUTF8 (" \xe2\x86\x92 ") + describeMidiMappingTarget (m.target));

    return items.joinIntoString (", ");
}

//==============================================================================
String PluginGraph::getDocumentTitle()
{
    if (! getFile().exists())
        return "Unnamed";

    return getFile().getFileNameWithoutExtension();
}

File PluginGraph::getPresetsDirectory()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
             .getChildFile ("Application Support")
             .getChildFile (JUCEApplication::getInstance()->getApplicationName())
             .getChildFile ("Presets");
}

Array<File> PluginGraph::getPresetFiles()
{
    auto files = getPresetsDirectory().findChildFiles (File::findFiles, false, getFilenameWildcard());
    files.sort();
    return files;
}

File PluginGraph::getSuggestedSaveAsFile (const File& defaultFile)
{
    return FileBasedDocument::getSuggestedSaveAsFile (getPresetsDirectory().getChildFile (defaultFile.getFileName()));
}

//==============================================================================
static void preConfigureGraphChannels (AudioProcessorGraph& graph, const XmlElement* xml = nullptr)
{
    using namespace Curve::AudioConstants;

    int numIns = defaultNumChannels;
    int numOuts = defaultNumChannels;
    double targetSampleRate = graph.getSampleRate();
    int targetBlockSize = graph.getBlockSize();

    if (auto* settings = getUserSettings())
    {
        if (auto state = settings->getXmlValue ("audioDeviceState"))
        {
            juce::BigInteger inChans, outChans;
            inChans.parseString (state->getStringAttribute ("audioDeviceInChans", defaultStereoBitmask), 2);
            outChans.parseString (state->getStringAttribute ("audioDeviceOutChans", defaultStereoBitmask), 2);

            numIns  = jmax (numIns,  inChans.countNumberOfSetBits());
            numOuts = jmax (numOuts, outChans.countNumberOfSetBits());

            if (targetSampleRate <= 0.0)
                targetSampleRate = state->getDoubleAttribute ("audioDeviceRate", state->getDoubleAttribute ("sampleRate", defaultSampleRate));

            if (targetBlockSize <= 0)
                targetBlockSize = state->getIntAttribute ("audioDeviceBufferSize", state->getIntAttribute ("bufferSize", defaultBufferSize));
        }
    }

    if (xml != nullptr)
    {
        int audioOutputUid = -1;
        int audioInputUid = -1;

        for (auto* filterXml : xml->getChildWithTagNameIterator ("FILTER"))
        {
            int uid = filterXml->getIntAttribute ("uid", -1);

            for (auto* child : filterXml->getChildIterator())
            {
                if (child->hasTagName ("PLUGIN"))
                {
                    PluginDescription desc;
                    const bool loaded = desc.loadFromXml (*child);
                    const bool isInternal = loaded && desc.pluginFormatName.equalsIgnoreCase (internalPluginFormat);

                    const bool isAudioOutput = isInternal && (desc.fileOrIdentifier.equalsIgnoreCase (audioOutputName)
                                                           || desc.name.equalsIgnoreCase (audioOutputName)
                                                           || desc.uniqueId == audioOutputUniqueId);

                    const bool isAudioInput  = isInternal && (desc.fileOrIdentifier.equalsIgnoreCase (audioInputName)
                                                           || desc.name.equalsIgnoreCase (audioInputName)
                                                           || desc.uniqueId == audioInputUniqueId);

                    if (isAudioOutput)
                    {
                        audioOutputUid = uid;

                        if (auto* layoutEntity = filterXml->getChildByName ("LAYOUT"))
                        {
                            if (auto* inputs = layoutEntity->getChildByName ("INPUTS"))
                            {
                                for (auto* bus : inputs->getChildWithTagNameIterator ("BUS"))
                                {
                                    auto layoutStr = bus->getStringAttribute ("layout");
                                    if (layoutStr.isNotEmpty() && ! layoutStr.equalsIgnoreCase ("disabled"))
                                    {
                                        auto set = AudioChannelSet::fromAbbreviatedString (layoutStr);
                                        if (set.size() > 0)
                                            numOuts = jmax (numOuts, set.size());
                                    }
                                }
                            }
                        }
                    }
                    else if (isAudioInput)
                    {
                        audioInputUid = uid;

                        if (auto* layoutEntity = filterXml->getChildByName ("LAYOUT"))
                        {
                            if (auto* outputs = layoutEntity->getChildByName ("OUTPUTS"))
                            {
                                for (auto* bus : outputs->getChildWithTagNameIterator ("BUS"))
                                {
                                    auto layoutStr = bus->getStringAttribute ("layout");
                                    if (layoutStr.isNotEmpty() && ! layoutStr.equalsIgnoreCase ("disabled"))
                                    {
                                        auto set = AudioChannelSet::fromAbbreviatedString (layoutStr);
                                        if (set.size() > 0)
                                            numIns = jmax (numIns, set.size());
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }

        // Also check connection indices as a safeguard against sparse channels or missing LAYOUT tags
        for (auto* connXml : xml->getChildWithTagNameIterator ("CONNECTION"))
        {
            if (audioOutputUid >= 0 && connXml->getIntAttribute ("dstFilter") == audioOutputUid)
            {
                int dstChan = connXml->getIntAttribute ("dstChannel");
                numOuts = jmax (numOuts, dstChan + 1);
            }

            if (audioInputUid >= 0 && connXml->getIntAttribute ("srcFilter") == audioInputUid)
            {
                int srcChan = connXml->getIntAttribute ("srcChannel");
                numIns = jmax (numIns, srcChan + 1);
            }
        }
    }

    if (targetSampleRate <= 0.0)
        targetSampleRate = defaultSampleRate;

    if (targetBlockSize <= 0)
        targetBlockSize = defaultBufferSize;

    graph.setPlayConfigDetails (numIns, numOuts, targetSampleRate, targetBlockSize);
}

void PluginGraph::newDocument()
{
    clear();
    setFile ({});
    graph.transport.setTempo (HostTransport::defaultTempo);

    graph.removeChangeListener (this);

    preConfigureGraphChannels (graph, nullptr);

    InternalPluginFormat internalFormat;
    String errorMessage;

    for (const auto& desc : internalFormat.getAllTypes())
    {
        bool isAudioInput  = (desc.fileOrIdentifier == Curve::AudioConstants::audioInputName || desc.name == Curve::AudioConstants::audioInputName);
        bool isLoopback    = (desc.fileOrIdentifier == Curve::AudioConstants::loopbackIdentifier || desc.name == Curve::AudioConstants::loopbackName || desc.name == "Output Interface Loopback");
        bool isAudioOutput = (desc.fileOrIdentifier == Curve::AudioConstants::audioOutputName || desc.name == Curve::AudioConstants::audioOutputName);

        if (isAudioInput || isLoopback || isAudioOutput)
        {
            if (auto instance = formatManager.createPluginInstance (desc, graph.getSampleRate(), graph.getBlockSize(), errorMessage))
            {
                instance->enableAllBuses();
                if (auto node = graph.addNode (std::move (instance)))
                {
                    double xPos = 0.5;
                    double yPos = 0.9;

                    if (isAudioInput)
                    {
                        xPos = 0.35;
                        yPos = 0.1;
                    }
                    else if (isLoopback)
                    {
                        xPos = 0.65;
                        yPos = 0.1;
                    }

                    node->properties.set ("x", xPos);
                    node->properties.set ("y", yPos);
                    node->properties.set ("useARA", false);
                }
            }
        }
    }

    setChangedFlag (false);
    graph.addChangeListener (this);
}

Result PluginGraph::loadDocument (const File& file)
{
    if (auto xml = parseXMLIfTagMatches (file, "FILTERGRAPH"))
    {
        graph.removeChangeListener (this);
        restoreFromXml (*xml, restorePluginWindowsOnLoad);

        juce::WeakReference<PluginGraph> weakSelf (this);
        MessageManager::callAsync ([weakSelf]
        {
            if (auto* self = weakSelf.get())
            {
                self->setChangedFlag (false);
                self->graph.addChangeListener (self);
            }
        });

        return Result::ok();
    }

    return Result::fail ("Not a valid graph file");
}

Result PluginGraph::saveDocument (const File& file)
{
    if (file == getFile() && sceneGroup.isNotEmpty())
        return saveScene (prepareSceneSave (file, false));

    return saveSeparatePreset (file);
}

File PluginGraph::getLastDocumentOpened()
{
    RecentlyOpenedFilesList recentFiles;
    if (auto* settings = getUserSettings())
        recentFiles.restoreFromString (settings->getValue ("recentFilterGraphFiles"));

    return recentFiles.getFile (0);
}

void PluginGraph::setLastDocumentOpened (const File& file)
{
    RecentlyOpenedFilesList recentFiles;
    if (auto* settings = getUserSettings())
    {
        recentFiles.restoreFromString (settings->getValue ("recentFilterGraphFiles"));
        recentFiles.addFile (file);
        settings->setValue ("recentFilterGraphFiles", recentFiles.toString());
        settings->saveIfNeeded();
    }
}

//==============================================================================
static void readBusLayoutFromXml (AudioProcessor::BusesLayout& busesLayout, AudioProcessor& plugin,
                                  const XmlElement& xml, bool isInput)
{
    auto& targetBuses = (isInput ? busesLayout.inputBuses
                                 : busesLayout.outputBuses);
    int maxNumBuses = 0;

    if (auto* buses = xml.getChildByName (isInput ? "INPUTS" : "OUTPUTS"))
    {
        for (auto* e : buses->getChildWithTagNameIterator ("BUS"))
        {
            const int busIdx = e->getIntAttribute ("index");
            maxNumBuses = jmax (maxNumBuses, busIdx + 1);

            // the number of buses on busesLayout may not be in sync with the plugin after adding buses
            // because adding an input bus could also add an output bus
            for (int actualIdx = plugin.getBusCount (isInput) - 1; actualIdx < busIdx; ++actualIdx)
                if (! plugin.addBus (isInput))
                    return;

            for (int actualIdx = targetBuses.size() - 1; actualIdx < busIdx; ++actualIdx)
                targetBuses.add (plugin.getChannelLayoutOfBus (isInput, busIdx));

            auto layout = e->getStringAttribute ("layout");

            if (layout.isNotEmpty() && isPositiveAndBelow (busIdx, targetBuses.size()))
                targetBuses.getReference (busIdx) = AudioChannelSet::fromAbbreviatedString (layout);
        }
    }

    // if the plugin has more buses than specified in the xml, then try to remove them!
    while (maxNumBuses < targetBuses.size())
    {
        if (! plugin.removeBus (isInput))
            return;

        targetBuses.removeLast();
    }
}

//==============================================================================
static XmlElement* createBusLayoutXml (const AudioProcessor::BusesLayout& layout, const bool isInput)
{
    auto& buses = isInput ? layout.inputBuses
                          : layout.outputBuses;

    auto* xml = new XmlElement (isInput ? "INPUTS" : "OUTPUTS");

    for (int busIdx = 0; busIdx < buses.size(); ++busIdx)
    {
        auto& set = buses.getReference (busIdx);

        auto* bus = xml->createNewChildElement ("BUS");
        bus->setAttribute ("index", busIdx);
        bus->setAttribute ("layout", set.isDisabled() ? "disabled" : set.getSpeakerArrangementAsString());
    }

    return xml;
}

static XmlElement* createNodeXml (AudioProcessorGraph::Node* const node, bool includeState) noexcept
{
    if (auto* plugin = dynamic_cast<AudioPluginInstance*> (node->getProcessor()))
    {
        auto e = new XmlElement ("FILTER");

        e->setAttribute ("uid",      (int) node->nodeID.uid);
        e->setAttribute ("x",        node->properties ["x"].toString());
        e->setAttribute ("y",        node->properties ["y"].toString());
        e->setAttribute ("useARA",   node->properties ["useARA"].toString());

        if (node->properties.contains ("customNodeName"))
            e->setAttribute ("customNodeName", node->properties ["customNodeName"].toString());

        if (node->isBypassed())
            e->setAttribute ("bypassed", true);

        if (node->properties[PluginGraph::mutedProp])
            e->setAttribute (PluginGraph::mutedProp, true);

        if (node->properties[PluginGraph::openWithPresetProp])
            e->setAttribute (PluginGraph::openWithPresetProp, true);

        if (node->properties[PluginGraph::freePositionProp])
            e->setAttribute (PluginGraph::freePositionProp, true);

        if (node->properties[PluginGraph::hiddenFromPanelProp])
            e->setAttribute (PluginGraph::hiddenFromPanelProp, true);

        if (node->properties[PluginGraph::varyBySceneProp])
            e->setAttribute (PluginGraph::varyBySceneProp, true);

        if (node->properties.contains (PluginGraph::hiddenPortsProp))
            e->setAttribute (PluginGraph::hiddenPortsProp, node->properties[PluginGraph::hiddenPortsProp].toString());

        for (int i = 0; i < (int) PluginWindow::Type::numTypes; ++i)
        {
            auto type = (PluginWindow::Type) i;

            // Whichever it has: Keep on Top can be set before its window has ever opened
            PluginWindow::saveArrangement (node->properties, *e, type);
        }

        {
            PluginDescription pd;
            plugin->fillInPluginDescription (pd);
            e->addChildElement (pd.createXml().release());
        }

        if (includeState)
        {
            MemoryBlock m;
            node->getProcessor()->getStateInformation (m);
            e->createNewChildElement ("STATE")->addTextElement (m.toBase64Encoding());
        }

        auto layout = plugin->getBusesLayout();

        auto layouts = e->createNewChildElement ("LAYOUT");
        layouts->addChildElement (createBusLayoutXml (layout, true));
        layouts->addChildElement (createBusLayoutXml (layout, false));

        return e;
    }

    jassertfalse;
    return nullptr;
}

#if JUCE_PLUGINHOST_ARA
static std::unique_ptr<AudioPluginInstance> wrapInstanceForARA (std::unique_ptr<AudioPluginInstance> instance,
                                                                const PluginDescriptionAndPreference& description)
{
    if (instance
        && description.useARA == PluginDescriptionAndPreference::UseARA::yes
        && description.pluginDescription.hasARAExtension)
    {
        return std::make_unique<ARAPluginInstanceWrapper> (std::move (instance));
    }

    return instance;
}
#else
static std::unique_ptr<AudioPluginInstance> wrapInstanceForARA (std::unique_ptr<AudioPluginInstance> instance,
                                                                const PluginDescriptionAndPreference&)
{
    return instance;
}
#endif

static String getRestoreFailureName (const PluginDescription& description)
{
    return description.name.isNotEmpty() ? description.name : TRANS ("Unknown plugin");
}

void PluginGraph::createNodeFromXml (const XmlElement& xml, bool restorePluginWindows)
{
    PluginDescriptionAndPreference pd;
    const auto nodeUsesARA = xml.getBoolAttribute ("useARA");

    for (auto* e : xml.getChildIterator())
    {
        if (pd.pluginDescription.loadFromXml (*e))
        {
            pd.useARA = nodeUsesARA ? PluginDescriptionAndPreference::UseARA::yes
                                    : PluginDescriptionAndPreference::UseARA::no;
            break;
        }
    }

    // The saved description first, then the scanned plugin with the same ID as a fallback
    std::vector<PluginDescriptionAndPreference> candidates { pd };

    {
        const auto allFormats = formatManager.getFormats();
        const auto matchingFormat = std::find_if (allFormats.begin(), allFormats.end(),
                                                  [&] (const AudioPluginFormat* f) { return f->getName() == pd.pluginDescription.pluginFormatName; });

        if (matchingFormat != allFormats.end())
        {
            const auto plugins = knownPlugins.getTypesForFormat (**matchingFormat);
            const auto matchingPlugin = std::find_if (plugins.begin(), plugins.end(),
                                                      [&] (const PluginDescription& desc) { return pd.pluginDescription.uniqueId == desc.uniqueId; });

            if (matchingPlugin != plugins.end())
                candidates.push_back (PluginDescriptionAndPreference { *matchingPlugin });
        }
    }

    for (const auto& description : candidates)
    {
        String errorMessage;

        auto instance = formatManager.createPluginInstance (description.pluginDescription,
                                                            graph.getSampleRate(),
                                                            graph.getBlockSize(),
                                                            errorMessage);

        if (instance != nullptr)
        {
            if (! finishNodeFromXml (wrapInstanceForARA (std::move (instance), description), xml, restorePluginWindows))
            {
                pluginsFailedToRestore.add (getRestoreFailureName (pd.pluginDescription));
                pluginsMissingFromLoad.add (getRestoreFailureName (pd.pluginDescription));
            }

            return;
        }
    }

    // Some formats (e.g. AUv3) can't be created synchronously; retry async.
    CURVE_AUDIO_LOG ("[PresetRestore] '%s' (%s) could not be created synchronously; retrying asynchronously",
                     pd.pluginDescription.name.toRawUTF8(), pd.pluginDescription.pluginFormatName.toRawUTF8());

    ++pendingAsyncRestores;
    createNodeFromXmlAsync (std::make_shared<const XmlElement> (xml), std::move (candidates), restorePluginWindows);
}

void PluginGraph::createNodeFromXmlAsync (std::shared_ptr<const XmlElement> nodeXml,
                                          std::vector<PluginDescriptionAndPreference> candidates,
                                          bool restorePluginWindows)
{
    jassert (! candidates.empty());

    const auto description = candidates.front();
    candidates.erase (candidates.begin());

    juce::WeakReference<PluginGraph> weakSelf (this);

    formatManager.createPluginInstanceAsync (description.pluginDescription,
                                             graph.getSampleRate(),
                                             graph.getBlockSize(),
                                             callOnMessageThread ([weakSelf, nodeXml, description, candidates, restorePluginWindows, generation = restoreGeneration]
                                                                  (std::unique_ptr<AudioPluginInstance> instance, const String&)
                                             {
                                                 auto* self = weakSelf.get();

                                                 // The graph was cleared or another preset was loaded in the meantime
                                                 if (self == nullptr || self->restoreGeneration != generation)
                                                     return;

                                                 if (instance == nullptr && ! candidates.empty())
                                                     self->createNodeFromXmlAsync (nodeXml, candidates, restorePluginWindows);
                                                 else
                                                     self->asyncNodeRestoreFinished (std::move (instance), description, *nodeXml, restorePluginWindows);
                                             }));
}

void PluginGraph::asyncNodeRestoreFinished (std::unique_ptr<AudioPluginInstance> instance,
                                            const PluginDescriptionAndPreference& description,
                                            const XmlElement& nodeXml,
                                            bool restorePluginWindows)
{
    --pendingAsyncRestores;

    const auto addFailure = [&]
    {
        PluginDescription saved;

        for (auto* e : nodeXml.getChildIterator())
            if (saved.loadFromXml (*e))
                break;

        pluginsFailedToRestore.add (getRestoreFailureName (saved));
        pluginsMissingFromLoad.add (getRestoreFailureName (saved));
        CURVE_AUDIO_LOG ("[PresetRestore] Async restore failed for '%s'", getRestoreFailureName (saved).toRawUTF8());
    };

    if (instance == nullptr)
    {
        addFailure();
    }
    else
    {
        // As in loadDocument(): completing the load mustn't mark the preset as edited
        const bool wasChanged = hasChangedSinceSaved();
        graph.removeChangeListener (this);

        if (finishNodeFromXml (wrapInstanceForARA (std::move (instance), description), nodeXml, restorePluginWindows))
        {
            const NodeID nodeID ((uint32) nodeXml.getIntAttribute ("uid"));

            // If the device changed to one with fewer outputs while this plug-in was loading,
            // the Audio Output node has shrunk, and its connections to the higher outputs would be
            // refused. Size the graph to fit them, as preConfigureGraphChannels() does on load.
            using IOProcessor = AudioProcessorGraph::AudioGraphIOProcessor;
            int outputsNeeded = 0;
            IOProcessor* audioOutputNode = nullptr;

            for (const auto& connection : restoredConnections)
                if (connection.source.nodeID == nodeID && connection.destination.channelIndex != AudioProcessorGraph::midiChannelIndex)
                    if (auto* dest = graph.getNodeForId (connection.destination.nodeID))
                        if (auto* io = dynamic_cast<IOProcessor*> (dest->getProcessor()))
                            if (io->getType() == IOProcessor::audioOutputNode)
                            {
                                audioOutputNode = io;
                                outputsNeeded = jmax (outputsNeeded, connection.destination.channelIndex + 1);
                            }

            if (audioOutputNode != nullptr && outputsNeeded > audioOutputNode->getTotalNumInputChannels())
            {
                graph.suspendProcessing (true);
                graph.setPlayConfigDetails (graph.getTotalNumInputChannels(),
                                            jmax (outputsNeeded, graph.getTotalNumOutputChannels()),
                                            graph.getSampleRate(), graph.getBlockSize());
                // No prepareToPlay needed: the graph renders into a buffer as wide as the one the
                // player passes, and the Audio Output node copies only the channels both have, so
                // the extra outputs are silent until the device has them
                // The existing Audio Output node only takes the graph's new size from this
                audioOutputNode->setParentGraph (&graph);
                graph.suspendProcessing (false);
            }

            for (const auto& connection : restoredConnections)
                if (connection.source.nodeID == nodeID || connection.destination.nodeID == nodeID)
                    graph.addConnection (connection);

            // Only this node's connections: pruning the whole graph would also delete connections
            // to Audio Output channels the current device lacks (e.g. outputs 3-4 while following
            // macOS onto 2-channel headphones), which should be kept for when the device returns
            for (const auto& connection : graph.getConnections())
                if ((connection.source.nodeID == nodeID || connection.destination.nodeID == nodeID)
                    && ! graph.isConnectionLegal (connection))
                    graph.removeConnection (connection);

            CURVE_AUDIO_LOG ("[PresetRestore] Async restore succeeded for '%s' (node uid=%u)",
                             description.pluginDescription.name.toRawUTF8(), (unsigned) nodeID.uid);
        }
        else
        {
            addFailure();
        }

        changed();
        setChangedFlag (wasChanged);

        juce::WeakReference<PluginGraph> weakSelf (this);
        MessageManager::callAsync ([weakSelf, wasChanged, generation = restoreGeneration]
        {
            if (auto* self = weakSelf.get())
            {
                if (self->restoreGeneration == generation)
                    self->setChangedFlag (wasChanged);

                self->graph.addChangeListener (self);
            }
        });
    }

    if (pendingAsyncRestores == 0)
    {
        showRestoreFailures();

        // Their nodes didn't exist yet when MainHostWindow::loadPreset() opened the others. Unless
        // it's holding them back while presets are switched quickly, and opens them all later
        if (! arePresetWindowsDeferred)
            openPresetPluginWindows();
    }
}

bool PluginGraph::finishNodeFromXml (std::unique_ptr<AudioPluginInstance> instance,
                                     const XmlElement& xml,
                                     bool restorePluginWindows)
{
    if (auto* layoutEntity = xml.getChildByName ("LAYOUT"))
    {
        auto layout = instance->getBusesLayout();

        readBusLayoutFromXml (layout, *instance, *layoutEntity, true);
        readBusLayoutFromXml (layout, *instance, *layoutEntity, false);

        if (! instance->setBusesLayout (layout))
            DBG ("Failed to apply saved bus layout to " + instance->getName());
    }

    if (auto node = graph.addNode (std::move (instance), NodeID ((uint32) xml.getIntAttribute ("uid"))))
    {
        if (auto* state = xml.getChildByName ("STATE"))
        {
            MemoryBlock m;
            m.fromBase64Encoding (state->getAllSubText());

            node->getProcessor()->setStateInformation (m.getData(), (int) m.getSize());
        }

        // After STATE, so it overrides any bypass in the plugin's own state, either way: a plug-in
        // with a bypass parameter keeps it in its state, which may be another scene's. Not for a
        // node that can't be bypassed, which another scene's save once added bypassed
        node->setBypassed (xml.getBoolAttribute ("bypassed") && canBeBypassed (*node->getProcessor()));

        if (auto* state = xml.getChildByName ("STATE"))
            rememberStateAsSaved (*node, state->getAllSubText());

        const double posX = xml.hasAttribute ("x") ? xml.getDoubleAttribute ("x") : 0.5;
        const double posY = xml.hasAttribute ("y") ? xml.getDoubleAttribute ("y") : 0.5;
        node->properties.set ("x", jlimit (0.0, 1.0, posX));
        node->properties.set ("y", jlimit (0.0, 1.0, posY));
        node->properties.set ("useARA", xml.getBoolAttribute ("useARA"));

        if (xml.getBoolAttribute (openWithPresetProp))
            node->properties.set (openWithPresetProp, true);

        if (xml.getBoolAttribute (freePositionProp))
            node->properties.set (freePositionProp, true);

        if (xml.getBoolAttribute (hiddenFromPanelProp))
            node->properties.set (hiddenFromPanelProp, true);

        if (xml.getBoolAttribute (varyBySceneProp))
            node->properties.set (varyBySceneProp, true);

        if (xml.getBoolAttribute (mutedProp))
        {
            // Nothing to ring out yet
            node->properties.set (mutedProp, true);
            node->setOutputMuted (true);
            node->getProcessor()->suspendProcessing (true);
        }

        if (const auto hiddenPorts = xml.getStringAttribute (hiddenPortsProp); hiddenPorts.isNotEmpty())
            node->properties.set (hiddenPortsProp, hiddenPorts);

        if (const auto customName = xml.getStringAttribute ("customNodeName"); customName.isNotEmpty())
            node->properties.set ("customNodeName", customName);

        for (int i = 0; i < (int) PluginWindow::Type::numTypes; ++i)
        {
            auto type = (PluginWindow::Type) i;

            PluginWindow::restoreArrangement (xml, node->properties, type, false);

            if (xml.hasAttribute (PluginWindow::getOpenProp (type)))
            {
                if (restorePluginWindows && node->properties[PluginWindow::getOpenProp (type)])
                {
                    jassert (node->getProcessor() != nullptr);

                    if (auto w = getOrCreateWindowFor (node, type))
                        w->toFront (true);
                }
            }
        }

        return true;
    }

    return false;
}

void PluginGraph::showRestoreFailures()
{
    if (pluginsFailedToRestore.isEmpty())
        return;

    auto options = MessageBoxOptions::makeOptionsOk (MessageBoxIconType::WarningIcon,
                                                     TRANS ("Couldn't load plugins"),
                                                     TRANS ("These plugins could not be loaded and are missing from the preset:")
                                                         + "\n\n" + pluginsFailedToRestore.joinIntoString ("\n"));
    messageBox = AlertWindow::showScopedAsync (options, nullptr);
    pluginsFailedToRestore.clear();
}

std::unique_ptr<XmlElement> PluginGraph::createXml (bool includeStates) const
{
    auto xml = std::make_unique<XmlElement> ("FILTERGRAPH");
    xml->setAttribute (Curve::Scenes::tempoAttribute, graph.transport.getTempo());

    if (PluginWindow::arePositionsLocked())
        xml->setAttribute (Curve::Scenes::windowsLockedAttribute, true);

    for (auto* node : graph.getNodes())
    {
        auto* e = createNodeXml (node, includeStates);

        // Bypassed only to compare, so saved as it was
        if (bypassedForComparison.count (node->nodeID.uid) > 0)
            e->removeAttribute ("bypassed");

        // As in the file, while the plug-in reports what it did when that was loaded or saved
        if (auto saved = statesAsSaved.find (node->nodeID.uid); saved != statesAsSaved.end())
        {
            if (auto* state = e->getChildByName ("STATE"); state != nullptr && state->getAllSubText() == saved->second.reported)
            {
                state->deleteAllTextElements();
                state->addTextElement (saved->second.inFile);
            }
        }

        xml->addChildElement (e);
    }

    for (auto& connection : graph.getConnections())
    {
        auto e = xml->createNewChildElement ("CONNECTION");

        e->setAttribute ("srcFilter", (int) connection.source.nodeID.uid);
        e->setAttribute ("srcChannel", connection.source.channelIndex);
        e->setAttribute ("dstFilter", (int) connection.destination.nodeID.uid);
        e->setAttribute ("dstChannel", connection.destination.channelIndex);
    }

    // Leave out mappings whose node is gone, e.g. a plugin that failed to load
    MidiMappingSet mappingsToSave;

    for (auto& m : midiMappings.getMappings())
        if (! m.target.isNodeTarget() || graph.getNodeForId (m.target.nodeID) != nullptr)
            mappingsToSave.assign (m);

    if (! mappingsToSave.isEmpty())
        xml->addChildElement (mappingsToSave.toXml().release());

    return xml;
}

void PluginGraph::restoreFromXml (const XmlElement& xml, bool restorePluginWindows)
{
    clear();

    sceneGroup = xml.getStringAttribute (Curve::Scenes::groupAttribute);
    loadedXml = std::make_unique<XmlElement> (xml);
    graph.transport.setTempo (xml.getDoubleAttribute (Curve::Scenes::tempoAttribute, HostTransport::defaultTempo));

    // Before its windows open, so they open locked. Presets from before had none, and load unlocked
    applyWindowLock (xml.getBoolAttribute (Curve::Scenes::windowsLockedAttribute));

    preConfigureGraphChannels (graph, &xml);

    for (auto* e : xml.getChildWithTagNameIterator ("CONNECTION"))
    {
        restoredConnections.push_back ({ { NodeID ((uint32) e->getIntAttribute ("srcFilter")), e->getIntAttribute ("srcChannel") },
                                         { NodeID ((uint32) e->getIntAttribute ("dstFilter")), e->getIntAttribute ("dstChannel") } });
    }

    for (auto* e : xml.getChildWithTagNameIterator ("FILTER"))
        createNodeFromXml (*e, restorePluginWindows);

    for (const auto& connection : restoredConnections)
        graph.addConnection (connection);

    graph.removeIllegalConnections();

    // Targets are looked up when a message arrives, so nodes still restoring asynchronously are fine
    if (auto* mappingsXml = xml.getChildByName (MidiMappingSet::xmlTag))
        midiMappings.restoreFromXml (*mappingsXml);

    changed();

    if (pendingAsyncRestores == 0)
        showRestoreFailures();
}

//==============================================================================
// Scenes

void PluginGraph::setNodeVariesByScene (NodeID nodeID, bool shouldVary)
{
    if (auto* node = graph.getNodeForId (nodeID))
    {
        if ((bool) node->properties[varyBySceneProp] == shouldVary)
            return;

        if (shouldVary)
            node->properties.set (varyBySceneProp, true);
        else
            node->properties.remove (varyBySceneProp);

        changed();
    }
}

bool PluginGraph::doesNodeVaryByScene (NodeID nodeID) const
{
    if (auto* node = graph.getNodeForId (nodeID))
        return node->properties[varyBySceneProp];

    return false;
}

bool PluginGraph::canVaryByScene (AudioProcessor& processor)
{
    return dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor*> (&processor) == nullptr
        && ! isConfiguredByAudioSettings (processor);
}

void PluginGraph::setNodeMuted (NodeID nodeID, bool shouldBeMuted)
{
    if (auto* node = graph.getNodeForId (nodeID))
    {
        if ((bool) node->properties[mutedProp] == shouldBeMuted)
            return;

        applyMute (*node, shouldBeMuted);
        graph.sendChangeMessage();
    }
}

void PluginGraph::setTempo (double bpm)
{
    const auto previous = graph.transport.getTempo();
    graph.transport.setTempo (bpm);

    if (! approximatelyEqual (previous, graph.transport.getTempo()))
        changed();
}

void PluginGraph::tapTempo()
{
    const auto previous = graph.transport.getTempo();

    if (graph.transport.tap() && ! approximatelyEqual (previous, graph.transport.getTempo()))
        changed();
}

void PluginGraph::setTransportPlaying (bool shouldPlay)
{
    graph.transport.setPlaying (shouldPlay);
    sendChangeMessage();
}

void PluginGraph::resetProcessor (AudioProcessor& processor)
{
    const ScopedLock sl (processor.getCallbackLock());
    processor.reset();
}

void PluginGraph::applyBypass (AudioProcessorGraph::Node& node, bool shouldBeBypassed)
{
    auto& processor = *node.getProcessor();
    const bool resetsWhenUnbypassed = node.isBypassed() && ! shouldBeBypassed && processor.getBypassParameter() == nullptr;

    // Reset first, as it's not yet being processed again
    if (resetsWhenUnbypassed)
        resetProcessor (processor);

    node.setBypassed (shouldBeBypassed);
}

bool PluginGraph::isConfiguringAudioIO (NodeID nodeID) const
{
    for (auto* w : activePluginWindows)
        if (w->type == PluginWindow::Type::audioIO && w->node->nodeID == nodeID)
            return true;

    return false;
}

void PluginGraph::applyMute (AudioProcessorGraph::Node& node, bool shouldBeMuted)
{
    auto* processor = node.getProcessor();
    const bool wasMuted = node.properties[mutedProp];

    // Already so, e.g. the same in the scene switched to: a ring-out carries on
    if (wasMuted == shouldBeMuted)
        return;

    ringOutEnds.erase (node.nodeID.uid);

    if (shouldBeMuted)
    {
        node.properties.set (mutedProp, true);
        node.setOutputMuted (true);

        // The graph's I/O nodes pass straight to the device as they're processed, so stop at once
        const bool ringsOut = dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor*> (processor) == nullptr;

        if (ringsOut && ! processor->isSuspended())
        {
            ringOutEnds[node.nodeID.uid] = Time::getMillisecondCounter() + ringOutMs;
            startTimer (250);
            return;
        }

        // Left to Configure Audio I/O while it's open, which suspends it as it closes
        if (! isConfiguringAudioIO (node.nodeID))
            processor->suspendProcessing (true);

        return;
    }

    node.properties.remove (mutedProp);

    // Left suspended while Configure Audio I/O is open: its resources are released, and the
    // window prepares it again and unmutes it when it closes
    if (isConfiguringAudioIO (node.nodeID))
    {
        node.setOutputMuted (false);
        return;
    }

    // Suspended after ringing out: reset while still suspended, so nothing is processing it.
    // Still ringing out, it was processed all along, so it just carries on
    if (processor->isSuspended())
    {
        resetProcessor (*processor);
        node.setOutputMuted (false);
        processor->suspendProcessing (false);
    }
    else
    {
        node.setOutputMuted (false);
    }
}

void PluginGraph::timerCallback()
{
    const auto now = Time::getMillisecondCounter();

    for (auto it = ringOutEnds.begin(); it != ringOutEnds.end();)
    {
        auto* node = graph.getNodeForId (NodeID (it->first));

        // Gone, or unmuted meanwhile
        if (node == nullptr || ! (bool) node->properties[mutedProp])
        {
            it = ringOutEnds.erase (it);
            continue;
        }

        // Rung out: suspend it, so it isn't processed. Left to Configure Audio I/O while it's open
        if ((int32) (now - it->second) >= 0)
        {
            if (! isConfiguringAudioIO (node->nodeID))
                node->getProcessor()->suspendProcessing (true);

            it = ringOutEnds.erase (it);
            continue;
        }

        ++it;
    }

    if (ringOutEnds.empty())
        stopTimer();
}

bool PluginGraph::isNodeMuted (NodeID nodeID) const
{
    if (auto* node = graph.getNodeForId (nodeID))
        return node->properties[mutedProp];

    return false;
}

Colour PluginGraph::getSceneGroupColour (const String& group)
{
    // From its id, so it's the same each time
    static const uint32 palette[] = { 0xff4aa3ff, 0xffff9f43, 0xff5fd38d, 0xffe066ff,
                                      0xffffd23f, 0xff42d6d6, 0xffff6b81, 0xffa3a8ff };

    return Colour (palette[(size_t) (group.hashCode64() & 0x7fffffff) % std::size (palette)]);
}

std::unique_ptr<XmlElement> PluginGraph::prepareSceneSwitch (const File& file, String* whyLoadedInFull) const
{
    if (sceneGroup.isEmpty() || file == getFile())
        return nullptr;

    auto scene = parseXMLIfTagMatches (file, "FILTERGRAPH");

    if (scene == nullptr || scene->getStringAttribute (Curve::Scenes::groupAttribute) != sceneGroup)
        return nullptr;

    const auto loadInFull = [&] (const String& why) -> std::unique_ptr<XmlElement>
    {
        if (whyLoadedInFull != nullptr)
            *whyLoadedInFull = why;

        return nullptr;
    };

    if (pendingAsyncRestores > 0)
        return loadInFull ("a plug-in was still loading");

    if (! pluginsMissingFromLoad.isEmpty() || ! pluginsFailedToRestore.isEmpty())
        return loadInFull ("a plug-in didn't load");

    // The live graph's structure: plug-ins' state is set to the scene's as it switches (see
    // switchScene()), but nodes, connections, channel layouts and MIDI mappings changed since
    // loading or saving need the scene loaded in full
    if (! Curve::Scenes::shareStructure (*createXml (false), *scene))
        return loadInFull ("nodes, connections, channel layouts or MIDI mappings changed since it was loaded or saved");

    return scene;
}

void PluginGraph::switchScene (const File& file, const XmlElement& scene)
{
    // The scene sets each node's bypass as it has it
    bypassedForComparison.clear();

    // Setting plug-ins' state changes them, which isn't an edit to this scene
    graph.removeChangeListener (this);

    // As loading does, close the windows that don't open with this scene; openPresetPluginWindows()
    // opens the rest. First, as a closing window records where it was in its node
    std::set<uint32> opensWithScene;
    for (auto* e : scene.getChildWithTagNameIterator ("FILTER"))
        if (e->getBoolAttribute (openWithPresetProp))
            opensWithScene.insert ((uint32) e->getIntAttribute ("uid"));

    // Always the others, such as Configure Audio I/O, which prepares its plug-in again as it closes
    for (int i = activePluginWindows.size(); --i >= 0;)
    {
        auto* w = activePluginWindows.getUnchecked (i);

        if (w->type != PluginWindow::Type::normal || opensWithScene.count (w->node->nodeID.uid) == 0)
            activePluginWindows.remove (i);
    }

    for (auto* e : scene.getChildWithTagNameIterator ("FILTER"))
    {
        auto* node = graph.getNodeForId (NodeID ((uint32) e->getIntAttribute ("uid")));

        if (node == nullptr)
            continue;

        auto* processor = node->getProcessor();

        // Where it is on the graph, its name, and whether it varies by scene, as the scene has
        // them (see Curve::Scenes::getStructure()). First, as varying decides which state it gets
        node->properties.set ("x", jlimit (0.0, 1.0, e->hasAttribute ("x") ? e->getDoubleAttribute ("x") : 0.5));
        node->properties.set ("y", jlimit (0.0, 1.0, e->hasAttribute ("y") ? e->getDoubleAttribute ("y") : 0.5));

        for (const auto& prop : { freePositionProp, hiddenFromPanelProp, varyBySceneProp })
        {
            if (e->getBoolAttribute (prop))
                node->properties.set (prop, true);
            else
                node->properties.remove (prop);
        }

        if (const auto customName = e->getStringAttribute ("customNodeName"); customName.isNotEmpty())
            node->properties.set ("customNodeName", customName);
        else
            node->properties.remove ("customNodeName");

        // Each node's state as the scene has it: its own for a node that varies by scene, and for
        // a shared one, the saved one, so the scene is as saved, whatever was changed since. A
        // shared node still as loaded or saved is left alone, so most plug-ins aren't touched
        if (auto* state = e->getChildByName ("STATE"))
        {
            const auto sceneState = state->getAllSubText();
            bool isAsScene = false;

            if (! node->properties[varyBySceneProp])
            {
                MemoryBlock now;
                processor->getStateInformation (now);
                const auto nowText = now.toBase64Encoding();
                const auto saved = statesAsSaved.find (node->nodeID.uid);

                isAsScene = nowText == sceneState
                         || (saved != statesAsSaved.end() && saved->second.reported == nowText && saved->second.inFile == sceneState);
            }

            if (! isAsScene)
            {
                MemoryBlock m;
                m.fromBase64Encoding (sceneState);
                processor->setStateInformation (m.getData(), (int) m.getSize());
            }
        }

        // After the state, as when loading
        applyBypass (*node, e->getBoolAttribute ("bypassed") && canBeBypassed (*processor));

        if (auto* state = e->getChildByName ("STATE"))
            rememberStateAsSaved (*node, state->getAllSubText());

        applyMute (*node, e->getBoolAttribute (mutedProp));

        if (opensWithScene.count (node->nodeID.uid) > 0)
            node->properties.set (openWithPresetProp, true);
        else
            node->properties.remove (openWithPresetProp);

        // Where this scene's plug-in windows were. Open ones stay where they are
        for (int i = 0; i < (int) PluginWindow::Type::numTypes; ++i)
            PluginWindow::restoreArrangement (*e, node->properties, (PluginWindow::Type) i, true);
    }

    // Open windows kept on top as this scene has them
    for (auto* w : activePluginWindows)
        w->setAlwaysOnTop (pluginWindowsFloating || w->isKeptOnTop());

    graph.transport.setTempo (scene.getDoubleAttribute (Curve::Scenes::tempoAttribute, HostTransport::defaultTempo));
    applyWindowLock (scene.getBoolAttribute (Curve::Scenes::windowsLockedAttribute));

    // Another preset is loaded, for anything planned for the previous one, e.g. a save
    ++restoreGeneration;
    loadedXml = std::make_unique<XmlElement> (scene);
    setFile (file);
    setLastDocumentOpened (file);

    graph.sendChangeMessage();
    sendChangeMessage();

    // As loadDocument(): after the change messages, which would mark it as changed
    juce::WeakReference<PluginGraph> weakSelf (this);
    MessageManager::callAsync ([weakSelf]
    {
        if (auto* self = weakSelf.get())
        {
            self->setChangedFlag (false);
            self->graph.addChangeListener (self);
        }
    });
}

String PluginGraph::readSceneGroup (const File& file)
{
    // Kept until the file changes, as reading it reads the whole file, and Curve's menu asks for
    // every preset's each time it opens (message thread only)
    JUCE_ASSERT_MESSAGE_THREAD
    struct Read { Time modified; int64 size = 0; String group; };
    static std::map<String, Read> cache;

    const auto modified = file.getLastModificationTime();
    const auto size = file.getSize();
    auto& read = cache[file.getFullPathName()];

    if (read.modified == modified && read.size == size && modified != Time())
        return read.group;

    read = { modified, size, {} };
    XmlDocument doc (file);

    if (auto outer = doc.getDocumentElement (true))
        if (outer->hasTagName ("FILTERGRAPH"))
            read.group = outer->getStringAttribute (Curve::Scenes::groupAttribute);

    return read.group;
}

Array<File> PluginGraph::getScenesInGroup (const String& group)
{
    Array<File> scenes;

    if (group.isEmpty())
        return scenes;

    for (const auto& file : getPresetFiles())
        if (readSceneGroup (file) == group)
            scenes.add (file);

    return scenes;
}

Result PluginGraph::makeSeparatePreset (const File& file, PluginGraph* loadedGraph)
{
    auto xml = parseXMLIfTagMatches (file, "FILTERGRAPH");

    if (xml == nullptr)
        return Result::fail ("Not a valid graph file");

    // Each scene's file is the whole preset as it sounds, so this is all it takes
    xml->removeAttribute (Curve::Scenes::groupAttribute);

    if (! xml->writeTo (file, {}))
        return Result::fail ("Couldn't write to the file");

    if (loadedGraph != nullptr && loadedGraph->getFile() == file)
        loadedGraph->sceneGroup.clear();

    return Result::ok();
}

PluginGraph::SceneSave PluginGraph::prepareSceneSave (const File& file, bool isNewScene) const
{
    SceneSave save;
    save.file = file;
    save.graphGeneration = restoreGeneration;

    auto candidates = getScenesInGroup (sceneGroup);

    // A new scene of a preset that isn't a scene yet: the preset joins the new group
    if (isNewScene && getFile().existsAsFile())
        candidates.addIfNotAlreadyThere (getFile());

    candidates.removeAllInstancesOf (file);

    Array<File> otherFiles;
    OwnedArray<XmlElement> otherXmls;
    const auto loadedShared = loadedXml != nullptr ? Curve::Scenes::getSharedPart (*loadedXml) : nullptr;

    // Only rewrite scenes that still share the loaded preset's graph, as every save writes it.
    // A file that merely has the same group, e.g. copied from another Mac, would otherwise lose
    // its plug-ins, and have its own bypass and settings applied to whichever nodes have its uids.
    for (const auto& other : candidates)
    {
        auto xml = parseXMLIfTagMatches (other, "FILTERGRAPH");

        if (xml != nullptr && loadedShared != nullptr
            && Curve::Scenes::getSharedPart (*xml)->isEquivalentTo (loadedShared.get(), true))
        {
            otherFiles.add (other);
            otherXmls.add (std::move (xml));
        }
        else
        {
            save.leftOut.add (other);
        }
    }

    if (otherXmls.isEmpty())
        return save;

    const auto saved = createXml();

    // A scene of the group already sharing what's saved needs no rewriting. One joining the
    // group does, to be given the group
    for (int i = 0; i < otherXmls.size(); ++i)
    {
        const auto& other = *otherXmls[i];

        if (other.getStringAttribute (Curve::Scenes::groupAttribute).isNotEmpty() && Curve::Scenes::shareGraph (*saved, other))
        {
            save.unchanged.add (otherFiles[i]);
            continue;
        }

        save.otherScenes.add (otherFiles[i]);
        save.changes.add (Curve::Scenes::describeChanges (*saved, other));
    }

    return save;
}

bool PluginGraph::sharesLoadedGraph (const File& file) const
{
    if (sceneGroup.isEmpty() || loadedXml == nullptr || readSceneGroup (file) != sceneGroup)
        return false;

    auto xml = parseXMLIfTagMatches (file, "FILTERGRAPH");
    return xml != nullptr && Curve::Scenes::shareGraph (*xml, *loadedXml);
}

Result PluginGraph::saveScene (const SceneSave& save, bool* wasPresetSaved)
{
    if (wasPresetSaved != nullptr)
        *wasPresetSaved = false;

    if (save.graphGeneration != restoreGeneration)
        return Result::fail ("Another preset was loaded while this save was being prepared, so nothing was saved.");

    auto toUpdate = save.otherScenes;
    toUpdate.addArray (save.unchanged);

    if (! toUpdate.isEmpty())
    {
        if (pendingAsyncRestores > 0)
            return Result::fail ("Some plug-ins are still loading. Try again once they've loaded, "
                                 "so the other scenes keep them.");

        if (! pluginsMissingFromLoad.isEmpty())
            return Result::fail ("These plug-ins didn't load, and saving as a scene would remove them from the other scenes too:\n\n"
                                 + pluginsMissingFromLoad.joinIntoString ("\n")
                                 + "\n\nSave it as a standalone preset instead, or install the plug-ins and load the preset again.");
    }

    auto saved = createXml();
    const auto group = sceneGroup.isNotEmpty() ? sceneGroup : Uuid().toString();
    saved->setAttribute (Curve::Scenes::groupAttribute, group);

    if (! saved->writeTo (save.file, {}))
        return Result::fail ("Couldn't write to the file");

    if (wasPresetSaved != nullptr)
        *wasPresetSaved = true;

    StringArray failed, changedElsewhere;

    for (const auto& other : toUpdate)
    {
        auto otherXml = parseXMLIfTagMatches (other, "FILTERGRAPH");

        // One that shared what's saved when the save was prepared may not by now
        if (otherXml != nullptr && save.unchanged.contains (other) && Curve::Scenes::shareGraph (*saved, *otherXml))
            continue;

        // Changed elsewhere since the save was prepared, e.g. synced from another Mac: left as it
        // is, as prepareSceneSave() leaves out one that already differed
        if (otherXml != nullptr && loadedXml != nullptr && ! Curve::Scenes::shareGraph (*otherXml, *loadedXml))
        {
            changedElsewhere.add (other.getFileNameWithoutExtension());
            continue;
        }

        if (otherXml == nullptr || ! Curve::Scenes::createOtherScene (*saved, *otherXml)->writeTo (other, {}))
            failed.add (other.getFileNameWithoutExtension());
    }

    setFile (save.file);
    sceneGroup = group;

    // As read back from a file, like the other scenes it's compared with on the next save
    loadedXml = parseXML (saved->toString());
    rememberSavedStates (*saved);
    setChangedFlag (false);

    StringArray problems;

    if (! failed.isEmpty())
        problems.add ("Couldn't update these scenes: " + failed.joinIntoString (", "));

    if (! changedElsewhere.isEmpty())
        problems.add ("Didn't update these scenes, as they were changed elsewhere while saving: " + changedElsewhere.joinIntoString (", "));

    if (! problems.isEmpty())
        return Result::fail (problems.joinIntoString ("\n\n"));

    return Result::ok();
}

void PluginGraph::rememberStateAsSaved (AudioProcessorGraph::Node& node, const String& inFile)
{
    MemoryBlock reported;
    node.getProcessor()->getStateInformation (reported);
    statesAsSaved[node.nodeID.uid] = { inFile, reported.toBase64Encoding() };
}

void PluginGraph::rememberSavedStates (const XmlElement& saved)
{
    // A state written as the plug-in reports it is now the file's. One written as remembered still is
    for (auto* e : saved.getChildWithTagNameIterator ("FILTER"))
    {
        if (auto* state = e->getChildByName ("STATE"))
        {
            const auto uid = (uint32) e->getIntAttribute ("uid");
            const auto text = state->getAllSubText();
            auto it = statesAsSaved.find (uid);

            if (it == statesAsSaved.end() || it->second.inFile != text)
                statesAsSaved[uid] = { text, text };
        }
    }
}

Result PluginGraph::saveSeparatePreset (const File& file)
{
    auto xml = createXml();

    if (! xml->writeTo (file, {}))
        return Result::fail ("Couldn't write to the file");

    setFile (file);
    sceneGroup.clear();
    loadedXml = parseXML (xml->toString());
    rememberSavedStates (*xml);
    setChangedFlag (false);
    return Result::ok();
}
