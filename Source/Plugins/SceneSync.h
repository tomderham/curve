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

#include <map>
#include <memory>

/*  Scenes: presets that share their plug-ins and connections.

    Each scene is an ordinary preset file holding its whole graph, as it sounds, so older
    versions of Curve open it as a preset. Scenes that share have the same sceneGroup on
    FILTERGRAPH, and are equals: none is the original. Each scene has its own bypass for every node, and its own state for nodes
    marked varyByScene; everything else is shared. Saving a scene rewrites the others of its
    group from it, keeping those parts of theirs.

    Needs juce_core only, so tests/preset-scenes can check it.
*/
namespace Curve::Scenes
{
inline constexpr const char* groupAttribute = "sceneGroup";
inline constexpr const char* varyAttribute  = "varyByScene";
inline constexpr const char* mutedAttribute = "muted";
inline constexpr const char* tempoAttribute = "tempo";

/** Removes what a plug-in's description records about when it was read, which is new each
    time the plug-in is created, and its version, which an update changes, so a preset just
    loaded and saved has the same description, even with an updated plug-in.
*/
inline void removeDescriptionTimes (juce::XmlElement& node)
{
    if (auto* plugin = node.getChildByName ("PLUGIN"))
    {
        plugin->removeAttribute ("infoUpdateTime");
        plugin->removeAttribute ("fileTime");
        plugin->removeAttribute ("version");
    }
}

/** Whether a node attribute is about its plug-in window: where it is, whether it's open, and
    whether it opens with the preset. Each scene has its own, so one scene can show the windows of
    the instruments it uses, and just opening or moving a window isn't a change to the shared graph.
*/
inline bool isWindowAttribute (const juce::String& name)
{
    return name.startsWith ("uiopen_") || name.startsWith ("uiLastX_") || name.startsWith ("uiLastY_")
        || name == "openWithPreset";
}

inline void removeWindowAttributes (juce::XmlElement& node)
{
    for (int i = node.getNumAttributes(); --i >= 0;)
        if (isWindowAttribute (node.getAttributeName (i)))
            node.removeAttribute (node.getAttributeName (i));
}

/** The Audio Input and Audio Output nodes, whose channel count is the audio interface's, not
    something set in the preset: it's whatever interface was in use when the scene was saved.
    It's in their LAYOUT, and in numInputs and numOutputs on their PLUGIN description.
*/
inline bool isAudioDeviceNode (const juce::XmlElement& node)
{
    auto* plugin = node.getChildByName ("PLUGIN");
    if (plugin == nullptr || ! plugin->getStringAttribute ("format").equalsIgnoreCase ("Internal"))
        return false;

    for (const auto* name : { "Audio Input", "Audio Output" })
        if (plugin->getStringAttribute ("name").equalsIgnoreCase (name)
            || plugin->getStringAttribute ("file").equalsIgnoreCase (name))
            return true;

    return false;
}

/** The graph's I/O nodes and meters, which can't be bypassed (see PluginGraph::canBeBypassed()),
    so are added to another scene as they are.
*/
inline bool isNeverBypassed (const juce::XmlElement& node)
{
    auto* plugin = node.getChildByName ("PLUGIN");
    if (plugin == nullptr || ! plugin->getStringAttribute ("format").equalsIgnoreCase ("Internal"))
        return false;

    for (const auto* name : { "Audio Input", "Audio Output", "MIDI Input", "MIDI Output", "Meter" })
        if (plugin->getStringAttribute ("name").equalsIgnoreCase (name)
            || plugin->getStringAttribute ("file").equalsIgnoreCase (name))
            return true;

    return false;
}

/** The part of a scene's graph that every scene of its group has: without its tempo, its bypass and mute flags,
    the state of nodes that vary by scene, or the Audio Input and Output nodes' channel layout.
    Connections, including those to those nodes, are shared.
*/
inline std::unique_ptr<juce::XmlElement> getSharedPart (const juce::XmlElement& graph)
{
    auto shared = std::make_unique<juce::XmlElement> (graph);
    shared->removeAttribute (groupAttribute);
    shared->removeAttribute (tempoAttribute);

    for (auto* node : shared->getChildWithTagNameIterator ("FILTER"))
    {
        node->removeAttribute ("bypassed");
        node->removeAttribute (mutedAttribute);
        removeWindowAttributes (*node);
        removeDescriptionTimes (*node);

        if (node->getBoolAttribute (varyAttribute))
            if (auto* state = node->getChildByName ("STATE"))
                node->removeChildElement (state, true);

        if (isAudioDeviceNode (*node))
        {
            if (auto* layout = node->getChildByName ("LAYOUT"))
                node->removeChildElement (layout, true);

            if (auto* plugin = node->getChildByName ("PLUGIN"))
            {
                plugin->removeAttribute ("numInputs");
                plugin->removeAttribute ("numOutputs");
            }
        }
    }

    return shared;
}

/** Whether two scenes of a group still share their graph. Saving writes it the same in every
    scene, so anything else means a file was changed elsewhere, e.g. copied from another Mac.
*/
inline bool shareGraph (const juce::XmlElement& a, const juce::XmlElement& b)
{
    return getSharedPart (a)->isEquivalentTo (getSharedPart (b).get(), true);
}

/** The node attributes a scene switched to in place sets as it has them, so they needn't match:
    where a node is on the graph, its name, and whether it varies by scene.
*/
inline constexpr const char* settableNodeAttributes[] = { "x", "y", "freePosition", "customNodeName", varyAttribute };

inline std::unique_ptr<juce::XmlElement> getStructure (const juce::XmlElement& graph)
{
    auto part = getSharedPart (graph);

    for (auto* node : part->getChildWithTagNameIterator ("FILTER"))
    {
        if (auto* state = node->getChildByName ("STATE"))
            node->removeChildElement (state, true);

        for (const auto* attribute : settableNodeAttributes)
            node->removeAttribute (attribute);
    }

    return part;
}

/** Whether two graphs share their structure: their nodes, connections, channel layouts and MIDI
    mappings, whatever their plug-ins' state, where the nodes are and what they're called. A scene
    with the same structure as the live graph can be switched to by setting the plug-ins' state,
    without loading them again.
*/
inline bool shareStructure (const juce::XmlElement& a, const juce::XmlElement& b)
{
    return getStructure (a)->isEquivalentTo (getStructure (b).get(), true);
}


/** Another scene of the group once `saved` has been saved: the saved graph, with the other
    scene's own bypass and mute for each node, its own state for each node that varies by scene, and its
    own Audio Input and Output layout (see isAudioDeviceNode()), and its own plug-in windows,
    including which open with it. Nodes
    it doesn't have yet are bypassed in it, so adding a node doesn't change how it sounds, except
    those that can't be bypassed (see isNeverBypassed()), which pass on nothing but what they're connected to.
*/
inline std::unique_ptr<juce::XmlElement> createOtherScene (const juce::XmlElement& saved, const juce::XmlElement& other)
{
    std::map<int, const juce::XmlElement*> otherNodes;

    for (auto* node : other.getChildWithTagNameIterator ("FILTER"))
        otherNodes[node->getIntAttribute ("uid")] = node;

    auto result = std::make_unique<juce::XmlElement> (saved);

    // Its own tempo
    if (other.hasAttribute (tempoAttribute))
        result->setAttribute (tempoAttribute, other.getStringAttribute (tempoAttribute));
    else
        result->removeAttribute (tempoAttribute);

    for (auto* node : result->getChildWithTagNameIterator ("FILTER"))
    {
        auto own = otherNodes.find (node->getIntAttribute ("uid"));

        if (own == otherNodes.end())
        {
            // Doesn't open with the other scenes, where it's bypassed
            node->removeAttribute (mutedAttribute);
            removeWindowAttributes (*node);

            if (! isNeverBypassed (*node))
                node->setAttribute ("bypassed", true);
            continue;
        }

        if (own->second->getBoolAttribute ("bypassed"))
            node->setAttribute ("bypassed", true);
        else
            node->removeAttribute ("bypassed");

        if (own->second->getBoolAttribute (mutedAttribute))
            node->setAttribute (mutedAttribute, true);
        else
            node->removeAttribute (mutedAttribute);

        // Its own plug-in windows
        removeWindowAttributes (*node);

        for (int i = 0; i < own->second->getNumAttributes(); ++i)
            if (const auto name = own->second->getAttributeName (i); isWindowAttribute (name))
                node->setAttribute (name, own->second->getAttributeValue (i));

        // Its own count of the interface's channels, as saved with whichever interface it was on
        if (isAudioDeviceNode (*node))
        {
            if (auto* layout = node->getChildByName ("LAYOUT"))
                node->removeChildElement (layout, true);

            if (auto* ownLayout = own->second->getChildByName ("LAYOUT"))
                node->addChildElement (new juce::XmlElement (*ownLayout));

            auto* plugin = node->getChildByName ("PLUGIN");
            auto* ownPlugin = own->second->getChildByName ("PLUGIN");

            if (plugin != nullptr && ownPlugin != nullptr)
                for (const auto* attribute : { "numInputs", "numOutputs" })
                    if (ownPlugin->hasAttribute (attribute))
                        plugin->setAttribute (attribute, ownPlugin->getStringAttribute (attribute));
        }

        // Its own state, or none if it had none. Curve always writes one, but a file may be edited
        if (node->getBoolAttribute (varyAttribute))
        {
            auto* state = node->getChildByName ("STATE");
            auto* ownState = own->second->getChildByName ("STATE");

            if (ownState == nullptr)
                node->removeChildElement (state, true);
            else if (state != nullptr)
                node->replaceChildElement (state, new juce::XmlElement (*ownState));
            else
                node->addChildElement (new juce::XmlElement (*ownState));
        }
    }

    return result;
}

/** What a node is called in the graph: its custom name, else its plug-in's. */
inline juce::String getNodeName (const juce::XmlElement& node)
{
    const auto custom = node.getStringAttribute ("customNodeName");

    if (custom.isNotEmpty())
        return custom;

    if (auto* plugin = node.getChildByName ("PLUGIN"))
        if (const auto name = plugin->getStringAttribute ("name"); name.isNotEmpty())
            return name;

    return "Node " + node.getStringAttribute ("uid");
}

/** How saving a scene changes how another scene of its group sounds, so it can be said first.
    Bypass and mute are each scene's own, and nodes are added bypassed, so neither is a change in sound.
*/
struct SharedChanges
{
    juce::StringArray changedNodes;   // settings or channel layout
    juce::Array<int> changedStateUIDs; // of those, the nodes whose settings change, which Vary by Scene would keep
    juce::StringArray addedNodes;     // bypassed in the other scene
    juce::StringArray addedIONodes;   // I/O nodes and meters, which can't be bypassed (see isNeverBypassed())
    juce::StringArray removedNodes;
    bool connectionsChanged = false;
    bool midiMappingsChanged = false;

    bool changesSound() const { return ! changedNodes.isEmpty() || ! removedNodes.isEmpty() || connectionsChanged; }

    void add (const SharedChanges& other)
    {
        changedNodes.addArray (other.changedNodes);
        for (auto uid : other.changedStateUIDs)
            changedStateUIDs.addIfNotAlreadyThere (uid);
        addedNodes.addArray (other.addedNodes);
        addedIONodes.addArray (other.addedIONodes);
        removedNodes.addArray (other.removedNodes);
        changedNodes.removeDuplicates (false);
        addedNodes.removeDuplicates (false);
        addedIONodes.removeDuplicates (false);
        removedNodes.removeDuplicates (false);
        connectionsChanged = connectionsChanged || other.connectionsChanged;
        midiMappingsChanged = midiMappingsChanged || other.midiMappingsChanged;
    }
};

inline juce::StringArray getConnections (const juce::XmlElement& graph)
{
    juce::StringArray connections;

    for (auto* c : graph.getChildWithTagNameIterator ("CONNECTION"))
        connections.add (c->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader()));

    connections.sort (false);
    return connections;
}

inline bool childDiffers (const juce::XmlElement& a, const juce::XmlElement& b, const char* tag)
{
    auto* ca = a.getChildByName (tag);
    auto* cb = b.getChildByName (tag);

    if (ca == nullptr || cb == nullptr)
        return ca != cb;

    return ! ca->isEquivalentTo (cb, true);
}

/** The changes that saving `saved` makes to `other`, another scene of its group. */
inline SharedChanges describeChanges (const juce::XmlElement& saved, const juce::XmlElement& other)
{
    SharedChanges changes;
    const auto updated = createOtherScene (saved, other);

    std::map<int, const juce::XmlElement*> otherNodes;

    for (auto* node : other.getChildWithTagNameIterator ("FILTER"))
        otherNodes[node->getIntAttribute ("uid")] = node;

    for (auto* node : updated->getChildWithTagNameIterator ("FILTER"))
    {
        auto own = otherNodes.find (node->getIntAttribute ("uid"));

        if (own == otherNodes.end())
        {
            (isNeverBypassed (*node) ? changes.addedIONodes : changes.addedNodes).add (getNodeName (*node));
            continue;
        }

        juce::XmlElement nodeWithoutTimes (*node), ownWithoutTimes (*own->second);
        removeDescriptionTimes (nodeWithoutTimes);
        removeDescriptionTimes (ownWithoutTimes);

        const bool stateDiffers = childDiffers (*node, *own->second, "STATE");

        if (childDiffers (nodeWithoutTimes, ownWithoutTimes, "PLUGIN")
            || stateDiffers
            || childDiffers (*node, *own->second, "LAYOUT"))
            changes.changedNodes.add (getNodeName (*node));

        if (stateDiffers)
            changes.changedStateUIDs.addIfNotAlreadyThere (node->getIntAttribute ("uid"));

        otherNodes.erase (own);
    }

    for (const auto& [uid, node] : otherNodes)
        changes.removedNodes.add (getNodeName (*node));

    changes.connectionsChanged = getConnections (*updated) != getConnections (other);

    // Shared too, and not a change in sound until a control is used
    const auto* mappings = updated->getChildByName ("MIDIMAPPINGS");
    const auto* ownMappings = other.getChildByName ("MIDIMAPPINGS");
    changes.midiMappingsChanged = (mappings == nullptr) != (ownMappings == nullptr)
                               || (mappings != nullptr && ! mappings->isEquivalentTo (ownMappings, true));
    return changes;
}
} // namespace Curve::Scenes
