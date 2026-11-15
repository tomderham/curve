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
#include <vector>
#include "../Plugins/PluginGraph.h"

juce::PropertiesFile* getUserSettings();

//==============================================================================
/** Default preset per output interface: with "Use output interface selected in macOS" on,
    Curve loads an interface's default preset when macOS switches to it.

    Stored in Curve's settings, never in preset files, so shared or copied presets carry
    no device assignments. Interfaces are identified by CoreAudio UID, which stays the same
    across reconnections; the name is kept only to show disconnected interfaces. Presets are
    stored by file name within the presets folder.
*/
namespace DefaultPresets
{
    struct Assignment
    {
        juce::String deviceUID, deviceName, presetFileName;
    };

    inline const char* settingsKey = "defaultPresetsByInterface";

    inline std::unique_ptr<juce::XmlElement> loadXml()
    {
        if (auto* settings = getUserSettings())
            if (auto xml = settings->getXmlValue (settingsKey))
                return xml;
        return std::make_unique<juce::XmlElement> ("DEFAULTPRESETS");
    }

    inline void saveXml (const juce::XmlElement& xml)
    {
        if (auto* settings = getUserSettings())
        {
            settings->setValue (settingsKey, &xml);
            settings->saveIfNeeded();
        }
    }

    inline std::vector<Assignment> getAssignments()
    {
        std::vector<Assignment> result;
        auto xml = loadXml();
        for (auto* e : xml->getChildWithTagNameIterator ("INTERFACE"))
            result.push_back ({ e->getStringAttribute ("uid"), e->getStringAttribute ("name"), e->getStringAttribute ("preset") });
        return result;
    }

    /** Empty if the interface has no default (keep the loaded preset). */
    inline juce::String getPresetFileName (const juce::String& deviceUID)
    {
        for (const auto& a : getAssignments())
            if (a.deviceUID == deviceUID)
                return a.presetFileName;
        return {};
    }

    /** An empty presetFileName removes the assignment. */
    inline void setPreset (const juce::String& deviceUID, const juce::String& deviceName, const juce::String& presetFileName)
    {
        if (deviceUID.isEmpty())
            return;

        auto xml = loadXml();
        auto* entry = xml->getChildByAttribute ("uid", deviceUID);

        if (presetFileName.isEmpty())
        {
            if (entry != nullptr)
                xml->removeChildElement (entry, true);
        }
        else
        {
            if (entry == nullptr)
            {
                entry = xml->createNewChildElement ("INTERFACE");
                entry->setAttribute ("uid", deviceUID);
            }

            entry->setAttribute ("name", deviceName);
            entry->setAttribute ("preset", presetFileName);
        }

        saveXml (*xml);
    }

    /** Keeps assignments pointing at a preset renamed in Curve. */
    inline void presetRenamed (const juce::File& oldFile, const juce::File& newFile)
    {
        auto xml = loadXml();
        bool changed = false;

        for (auto* e : xml->getChildWithTagNameIterator ("INTERFACE"))
        {
            if (e->getStringAttribute ("preset") == oldFile.getFileName())
            {
                e->setAttribute ("preset", newFile.getFileName());
                changed = true;
            }
        }

        if (changed)
            saveXml (*xml);
    }

    /** Off by default: launch keeps the preset used last. On: launch loads the default for
        the interface macOS is using, if it has one. */
    inline bool shouldLoadAtLaunch()
    {
        return loadXml()->getBoolAttribute ("loadAtLaunch", false);
    }

    inline void setLoadAtLaunch (bool shouldLoad)
    {
        auto xml = loadXml();
        xml->setAttribute ("loadAtLaunch", shouldLoad);
        saveXml (*xml);
    }

    inline juce::File getPresetFile (const juce::String& presetFileName)
    {
        return presetFileName.isEmpty() ? juce::File() : PluginGraph::getPresetsDirectory().getChildFile (presetFileName);
    }
}

/** Implemented in HostStartup.cpp: in follow mode, loads the default preset for the output
    interface Curve has just switched to, if it has one and it isn't already loaded. */
void loadDefaultPresetForOutputDevice (const juce::String& juceDeviceName);
