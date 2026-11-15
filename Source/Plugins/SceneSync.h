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

/** Removes what a plug-in's description records about when it was read, which is new each
    time the plug-in is created, so a preset just loaded and saved has the same description.
*/
inline void removeDescriptionTimes (juce::XmlElement& node)
{
    if (auto* plugin = node.getChildByName ("PLUGIN"))
    {
        plugin->removeAttribute ("infoUpdateTime");
        plugin->removeAttribute ("fileTime");
    }
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

/** The part of a scene's graph that every scene of its group has: without its bypass flags,
    the state of nodes that vary by scene, or the Audio Input and Output nodes' channel layout.
    Connections, including those to those nodes, are shared.
*/
inline std::unique_ptr<juce::XmlElement> getSharedPart (const juce::XmlElement& graph)
{
    auto shared = std::make_unique<juce::XmlElement> (graph);
    shared->removeAttribute (groupAttribute);

    for (auto* node : shared->getChildWithTagNameIterator ("FILTER"))
    {
        node->removeAttribute ("bypassed");
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

/** Another scene of the group once `saved` has been saved: the saved graph, with the other
    scene's own bypass for each node, its own state for each node that varies by scene, and its
    own Audio Input and Output layout (see isAudioDeviceNode()). Nodes it doesn't have yet are
    bypassed in it, so adding a node doesn't change how it sounds.
*/
inline std::unique_ptr<juce::XmlElement> createOtherScene (const juce::XmlElement& saved, const juce::XmlElement& other)
{
    std::map<int, const juce::XmlElement*> otherNodes;

    for (auto* node : other.getChildWithTagNameIterator ("FILTER"))
        otherNodes[node->getIntAttribute ("uid")] = node;

    auto result = std::make_unique<juce::XmlElement> (saved);

    for (auto* node : result->getChildWithTagNameIterator ("FILTER"))
    {
        auto own = otherNodes.find (node->getIntAttribute ("uid"));

        if (own == otherNodes.end())
        {
            node->setAttribute ("bypassed", true);
            continue;
        }

        if (own->second->getBoolAttribute ("bypassed"))
            node->setAttribute ("bypassed", true);
        else
            node->removeAttribute ("bypassed");

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
    Bypass is each scene's own, and nodes are added bypassed, so neither is a change in sound.
*/
struct SharedChanges
{
    juce::StringArray changedNodes;   // settings or channel layout
    juce::StringArray addedNodes;     // bypassed in the other scene
    juce::StringArray removedNodes;
    bool connectionsChanged = false;

    bool changesSound() const { return ! changedNodes.isEmpty() || ! removedNodes.isEmpty() || connectionsChanged; }

    void add (const SharedChanges& other)
    {
        changedNodes.addArray (other.changedNodes);
        addedNodes.addArray (other.addedNodes);
        removedNodes.addArray (other.removedNodes);
        changedNodes.removeDuplicates (false);
        addedNodes.removeDuplicates (false);
        removedNodes.removeDuplicates (false);
        connectionsChanged = connectionsChanged || other.connectionsChanged;
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
            changes.addedNodes.add (getNodeName (*node));
            continue;
        }

        juce::XmlElement nodeWithoutTimes (*node), ownWithoutTimes (*own->second);
        removeDescriptionTimes (nodeWithoutTimes);
        removeDescriptionTimes (ownWithoutTimes);

        if (childDiffers (nodeWithoutTimes, ownWithoutTimes, "PLUGIN")
            || childDiffers (*node, *own->second, "STATE")
            || childDiffers (*node, *own->second, "LAYOUT"))
            changes.changedNodes.add (getNodeName (*node));

        otherNodes.erase (own);
    }

    for (const auto& [uid, node] : otherNodes)
        changes.removedNodes.add (getNodeName (*node));

    changes.connectionsChanged = getConnections (*updated) != getConnections (other);
    return changes;
}
} // namespace Curve::Scenes
