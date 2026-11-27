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
#include "../Plugins/PluginGraph.h"
#include "SessionRecoveryCore.h"

juce::PropertiesFile* getUserSettings();
juce::String getMacOSOutputDeviceUID (const juce::String& juceDeviceName);

//==============================================================================
/** Keeps the loaded preset's unsaved changes, plug-ins' settings included, to restore when Curve
    opens again (see SessionRecoveryCore for when it asks first). Each setting is in General
    Preferences:

    - Keep unsaved changes when Curve quits: kept once, as it quits.
    - Save them every few minutes, in case Curve crashes: while there may be something new,
      which asks every plug-in for its state, as saving does. Some plug-ins hold up their audio
      while they report it, so it's off unless chosen. A clean quit discards what's been saved.

    The changes are kept in one file, written in place of the last, and discarded once there's
    nothing to keep: the preset saved, another loaded, or both settings off. With both off, it's
    never written, and Curve behaves as it always has.
*/
class SessionRecovery final : private juce::Timer,
                              private juce::ChangeListener
{
public:
    //==============================================================================
    static inline const char* keepOnQuitKey = "keepUnsavedChangesOnQuit";
    static inline const char* saveForCrashesKey = "saveUnsavedChangesForCrashes";

    // Set while Curve asks about kept changes: quitting before answering, it asks again next time
    static inline const char* askingKey = "askingAboutUnsavedChanges";

    static constexpr int snapshotIntervalSeconds = 5 * 60;

    static bool isKeepOnQuitEnabled()      { return getSetting (keepOnQuitKey, true); }
    static bool isSaveForCrashesEnabled()  { return getSetting (saveForCrashesKey, false); }
    static bool wasLeftUnanswered()        { return getSetting (askingKey, false); }

    static void setKeepOnQuitEnabled (bool shouldKeep)    { setSetting (keepOnQuitKey, shouldKeep); }
    static void setSaveForCrashesEnabled (bool shouldSave) { setSetting (saveForCrashesKey, shouldSave); }
    static void setAsking (bool isAsking)                  { setSetting (askingKey, isAsking); }

    static juce::File getFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Application Support")
                   .getChildFile (juce::JUCEApplication::getInstance()->getApplicationName())
                   .getChildFile ("Unsaved Changes.xml");
    }

    static void discard()   { getFile().deleteFile(); }

    //==============================================================================
    struct Kept
    {
        juce::File preset;              // none if they're to an untitled preset
        juce::String deviceUID, deviceName;
        juce::Time keptAt;
        juce::int64 presetModified = 0; // the preset file's modification time when they were kept
        std::unique_ptr<juce::XmlElement> graph;

        bool isForPreset() const         { return preset != juce::File(); }
        bool presetChangedSince() const  { return preset.existsAsFile() && preset.getLastModificationTime().toMilliseconds() != presetModified; }

        juce::String describePreset() const
        {
            return isForPreset() ? juce::String ("\"") + preset.getFileNameWithoutExtension()
                                     + juce::String ("\"")
                                 : juce::String ("an untitled preset");
        }
    };

    static std::unique_ptr<Kept> read()
    {
        auto xml = juce::parseXMLIfTagMatches (getFile(), rootTag);
        auto* graphXml = xml != nullptr ? xml->getChildByName ("FILTERGRAPH") : nullptr;

        if (graphXml == nullptr)
            return nullptr;

        auto kept = std::make_unique<Kept>();
        const auto path = xml->getStringAttribute ("preset");
        kept->preset = path.isNotEmpty() && juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
        kept->deviceUID = xml->getStringAttribute ("deviceUID");
        kept->deviceName = xml->getStringAttribute ("deviceName");
        kept->keptAt = juce::Time (xml->getStringAttribute ("keptAt").getLargeIntValue());
        kept->presetModified = xml->getStringAttribute ("presetModified").getLargeIntValue();
        kept->graph = std::make_unique<juce::XmlElement> (*graphXml);
        return kept;
    }

    //==============================================================================
    // The graph, which may be replaced or gone; the output interface's name; and whether a preset
    // is loading, when the graph is half built
    SessionRecovery (std::function<PluginGraph*()> graphGetter,
                     std::function<juce::String()> deviceNameGetter,
                     std::function<bool()> isLoadingGetter)
        : getGraph (std::move (graphGetter)),
          getDeviceName (std::move (deviceNameGetter)),
          isLoading (std::move (isLoadingGetter))
    {
    }

    ~SessionRecovery() override  { stopWatching(); }

    // Once Curve has opened its preset, and decided what to do with any changes kept: from then on,
    // what's loaded is what to keep
    void start()
    {
        hasStarted = true;
        settingsChanged();
    }

    void settingsChanged()
    {
        if (! hasStarted)
            return;

        if (! (isKeepOnQuitEnabled() || isSaveForCrashesEnabled()))
        {
            stopWatching();
            discard();
            return;
        }

        startWatching();

        if (isSaveForCrashesEnabled())
        {
            if (auto* g = watched.get())
                g->watchPluginStates();

            // Changes made before it was watching are only found by looking
            mayHaveChanged = true;
        }
        else if (auto* g = watched.get())
        {
            // Only saving for crashes asks: keeping them on quit looks at the graph then
            g->stopWatchingPluginStates();
        }
    }

    // As Curve quits, before the graph goes
    void quitting()
    {
        if (! hasStarted)
            return; // Still asking about changes kept from before: left as they are

        checkPresetReplaced();

        if (isKeepOnQuitEnabled())
            keep();
        else
            discard();

        stopWatching();
        hasStarted = false;
    }

private:
    static inline const char* rootTag = "CURVEUNSAVEDCHANGES";

    std::function<PluginGraph*()> getGraph;
    std::function<juce::String()> getDeviceName;
    std::function<bool()> isLoading;

    juce::WeakReference<PluginGraph> watched;
    juce::uint32 lastGeneration = 0, lastSaveGeneration = 0;
    bool hasStarted = false, mayHaveChanged = false;
    int secondsToSnapshot = snapshotIntervalSeconds;

    static bool getSetting (const char* key, bool defaultValue)
    {
        if (auto* s = getUserSettings())
            return s->getBoolValue (key, defaultValue);
        return defaultValue;
    }

    static void setSetting (const char* key, bool value)
    {
        if (auto* s = getUserSettings())
        {
            s->setValue (key, value);
            s->saveIfNeeded();
        }
    }

    void startWatching()
    {
        auto* g = getGraph();

        if (g == nullptr || g == watched.get())
            return;

        stopWatching();
        watched = g;
        g->addChangeListener (this);
        lastGeneration = g->getGraphGeneration();
        lastSaveGeneration = g->getSaveGeneration();
        mayHaveChanged = g->hasChangedSinceSaved();
        secondsToSnapshot = snapshotIntervalSeconds;
        startTimer (1000);
    }

    void stopWatching()
    {
        stopTimer();

        if (auto* g = watched.get())
            g->removeChangeListener (this);

        watched = nullptr;
    }

    // Saved, or another preset loaded: what's kept is out of date
    void checkPresetReplaced()
    {
        auto* g = watched.get();

        if (g == nullptr || (g->getGraphGeneration() == lastGeneration && g->getSaveGeneration() == lastSaveGeneration))
            return;

        lastGeneration = g->getGraphGeneration();
        lastSaveGeneration = g->getSaveGeneration();
        mayHaveChanged = false;
        g->takePluginStateTouched();
        discard();
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        checkPresetReplaced();

        if (auto* g = watched.get())
        {
            // Not as a load completes, which clears the flag
            mayHaveChanged = mayHaveChanged || g->hasChangedSinceSaved();

            // Any plug-ins added
            if (isSaveForCrashesEnabled())
                g->watchPluginStates();
        }
    }

    void timerCallback() override
    {
        checkPresetReplaced();

        if (! isSaveForCrashesEnabled() || --secondsToSnapshot > 0)
            return;

        secondsToSnapshot = snapshotIntervalSeconds;

        auto* g = watched.get();

        if (g == nullptr)
            return;

        // Only asking the plug-ins for their states when something may have changed since last time
        const bool touched = g->takePluginStateTouched();

        if (mayHaveChanged || touched)
            mayHaveChanged = ! keep();
    }

    // Writes the graph as it is now, if it isn't as saved, or discards what's kept if it is.
    // False if it can't tell yet: a preset loading, or a plug-in still loading or missing, which
    // the graph doesn't have, so what's kept already is left as it is
    bool keep()
    {
        auto* g = watched.get();

        if (g == nullptr || isLoading() || ! g->isFullyLoaded())
            return false;

        auto current = g->createXml();

        if (! g->differsFromSaved (*current))
        {
            discard();
            return true;
        }

        const auto preset = g->getFile().existsAsFile() ? g->getFile() : juce::File();
        const auto deviceName = getDeviceName();

        juce::XmlElement root (rootTag);
        root.setAttribute ("preset", preset.getFullPathName());
        root.setAttribute ("presetModified", juce::String (preset.existsAsFile() ? preset.getLastModificationTime().toMilliseconds() : 0));
        root.setAttribute ("deviceName", deviceName);
        root.setAttribute ("deviceUID", deviceName.isNotEmpty() ? getMacOSOutputDeviceUID (deviceName) : juce::String());
        root.setAttribute ("keptAt", juce::String (juce::Time::currentTimeMillis()));
        root.addChildElement (current.release());

        // In place of the last, through a temporary file, so a crash while writing leaves that
        getFile().getParentDirectory().createDirectory();
        return root.writeTo (getFile(), {});
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionRecovery)
};
