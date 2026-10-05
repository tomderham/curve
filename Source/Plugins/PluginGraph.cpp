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
#include "OutputInterfaceLoopbackNode.h"
#include "../UI/GraphEditorPanel.h"
#include "../AudioConstants.h"
#include "../AudioDiagnostics.h"


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
        if (! graph.getNodes().contains (activePluginWindows.getUnchecked (i)->node))
            activePluginWindows.remove (i);
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

        instance->enableAllBuses();

        if (auto node = graph.addNode (std::move (instance)))
        {
            node->properties.set ("x", pos.x);
            node->properties.set ("y", pos.y);
            node->properties.set ("useARA", useARA == PluginDescriptionAndPreference::UseARA::yes);
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

    graph.removeNode (nodeID);
}

void PluginGraph::clear()
{
    ++restoreGeneration;
    pendingAsyncRestores = 0;
    pluginsFailedToRestore.clear();
    restoredConnections.clear();

    closeAnyOpenPluginWindows();

    for (auto* node : graph.getNodes())
        deactivateIfLoopback (*node);

    graph.clear();
    changed();
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

    for (auto* w : activePluginWindows)
        if (w->node.get() == node && w->type == type)
            return w;

    if (auto* processor = node->getProcessor())
    {
        if (isConfiguredByAudioSettings (*processor))
        {
            getCommandManager().invokeDirectly (CommandIDs::showAudioSettings, false);
            return nullptr;
        }

        if (auto* plugin = dynamic_cast<AudioPluginInstance*> (processor))
        {
            return activePluginWindows.add (new PluginWindow (node,
                                                              type,
                                                              activePluginWindows,
                                                              getCommandManager().getKeyMappings()));
        }
    }

    return nullptr;
}

bool PluginGraph::closeAnyOpenPluginWindows()
{
    bool wasEmpty = activePluginWindows.isEmpty();
    activePluginWindows.clear();
    return ! wasEmpty;
}

//==============================================================================
String PluginGraph::getDocumentTitle()
{
    if (! getFile().exists())
        return "Unnamed";

    return getFile().getFileNameWithoutExtension();
}

File PluginGraph::getSuggestedSaveAsFile (const File& defaultFile)
{
    auto appDataDir = File::getSpecialLocation (File::userApplicationDataDirectory)
                        .getChildFile ("Application Support")
                        .getChildFile (JUCEApplication::getInstance()->getApplicationName());
    auto presetsDir = appDataDir.getChildFile ("Presets");

    return FileBasedDocument::getSuggestedSaveAsFile (presetsDir.getChildFile (defaultFile.getFileName()));
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
    auto xml = createXml();

    if (! xml->writeTo (file, {}))
        return Result::fail ("Couldn't write to the file");

    return Result::ok();
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

static XmlElement* createNodeXml (AudioProcessorGraph::Node* const node) noexcept
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

        for (int i = 0; i < (int) PluginWindow::Type::numTypes; ++i)
        {
            auto type = (PluginWindow::Type) i;

            if (node->properties.contains (PluginWindow::getOpenProp (type)))
            {
                e->setAttribute (PluginWindow::getLastXProp (type), node->properties[PluginWindow::getLastXProp (type)].toString());
                e->setAttribute (PluginWindow::getLastYProp (type), node->properties[PluginWindow::getLastYProp (type)].toString());
                e->setAttribute (PluginWindow::getOpenProp (type),  node->properties[PluginWindow::getOpenProp (type)].toString());
            }
        }

        {
            PluginDescription pd;
            plugin->fillInPluginDescription (pd);
            e->addChildElement (pd.createXml().release());
        }

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
                pluginsFailedToRestore.add (getRestoreFailureName (pd.pluginDescription));

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

            for (const auto& connection : restoredConnections)
                if (connection.source.nodeID == nodeID || connection.destination.nodeID == nodeID)
                    graph.addConnection (connection);

            graph.removeIllegalConnections();

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
        showRestoreFailures();
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

        // After STATE, so it overrides any bypass in the plugin's own state
        if (xml.getBoolAttribute ("bypassed"))
            node->setBypassed (true);

        const double posX = xml.hasAttribute ("x") ? xml.getDoubleAttribute ("x") : 0.5;
        const double posY = xml.hasAttribute ("y") ? xml.getDoubleAttribute ("y") : 0.5;
        node->properties.set ("x", jlimit (0.0, 1.0, posX));
        node->properties.set ("y", jlimit (0.0, 1.0, posY));
        node->properties.set ("useARA", xml.getBoolAttribute ("useARA"));

        if (const auto customName = xml.getStringAttribute ("customNodeName"); customName.isNotEmpty())
            node->properties.set ("customNodeName", customName);

        for (int i = 0; i < (int) PluginWindow::Type::numTypes; ++i)
        {
            auto type = (PluginWindow::Type) i;

            if (xml.hasAttribute (PluginWindow::getOpenProp (type)))
            {
                node->properties.set (PluginWindow::getLastXProp (type), xml.getIntAttribute (PluginWindow::getLastXProp (type)));
                node->properties.set (PluginWindow::getLastYProp (type), xml.getIntAttribute (PluginWindow::getLastYProp (type)));
                node->properties.set (PluginWindow::getOpenProp  (type), xml.getIntAttribute (PluginWindow::getOpenProp (type)));

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

std::unique_ptr<XmlElement> PluginGraph::createXml() const
{
    auto xml = std::make_unique<XmlElement> ("FILTERGRAPH");

    for (auto* node : graph.getNodes())
        xml->addChildElement (createNodeXml (node));

    for (auto& connection : graph.getConnections())
    {
        auto e = xml->createNewChildElement ("CONNECTION");

        e->setAttribute ("srcFilter", (int) connection.source.nodeID.uid);
        e->setAttribute ("srcChannel", connection.source.channelIndex);
        e->setAttribute ("dstFilter", (int) connection.destination.nodeID.uid);
        e->setAttribute ("dstChannel", connection.destination.channelIndex);
    }

    return xml;
}

void PluginGraph::restoreFromXml (const XmlElement& xml, bool restorePluginWindows)
{
    clear();

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
    changed();

    if (pendingAsyncRestores == 0)
        showRestoreFailures();
}
