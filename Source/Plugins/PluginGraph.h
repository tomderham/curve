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

#pragma once

#include "../UI/PluginWindow.h"
#include "../UI/LoginItemManager.h"
#include "../Midi/MidiMapping.h"
#include "SceneSync.h"
#include "HostTransport.h"

//==============================================================================
/** A type that encapsulates a PluginDescription and some preferences regarding
    how plugins of that description should be instantiated.
*/
struct PluginDescriptionAndPreference
{
    enum class UseARA { no, yes };

    PluginDescriptionAndPreference() = default;

    explicit PluginDescriptionAndPreference (PluginDescription pd)
        : pluginDescription (std::move (pd)),
          useARA (pluginDescription.hasARAExtension ? PluginDescriptionAndPreference::UseARA::yes
                                                    : PluginDescriptionAndPreference::UseARA::no)
    {}

    PluginDescriptionAndPreference (PluginDescription pd, UseARA ara)
        : pluginDescription (std::move (pd)), useARA (ara)
    {}

    PluginDescription pluginDescription;
    UseARA useARA = UseARA::no;
};

//==============================================================================
/**
    A collection of plugins and some connections between them.
*/
class PluginGraph final : public FileBasedDocument,
                          public AudioProcessorListener,
                          private ChangeListener,
                          private Timer
{
public:
    //==============================================================================
    PluginGraph (AudioPluginFormatManager&, KnownPluginList&);
    ~PluginGraph() override;

    //==============================================================================
    using NodeID = AudioProcessorGraph::NodeID;

    void addPlugin (const PluginDescriptionAndPreference&, Point<double>);

    AudioProcessorGraph::Node::Ptr getNodeForName (const String& name) const;

    void setNodePosition (NodeID, Point<double>);
    Point<double> getNodePosition (NodeID) const;

    /** A node with a free position ignores the graph editor's grid. */
    void setNodePositionFree (NodeID, bool);
    bool isNodePositionFree (NodeID) const;

    //==============================================================================
    // Remove nodes through these rather than graph.removeNode()/clear() directly,
    // so loopback nodes stop holding the shared tap open as soon as they leave the graph.
    void removeNode (NodeID);
    void clear();

    PluginWindow* getOrCreateWindowFor (AudioProcessorGraph::Node*, PluginWindow::Type);

    // Open plug-in windows are titled as their nodes are named, so after a rename too
    void updatePluginWindowTitles();

    // Device audio I/O and loopback nodes are configured in Audio Settings; any
    // window requested for them opens that instead.
    static bool isConfiguredByAudioSettings (AudioProcessor&);

    // Not the graph's I/O nodes, or meters, which don't change the audio
    static bool canBeBypassed (AudioProcessor&);
    bool closeAnyOpenPluginWindows();

    // Opens the windows of nodes set to open with the preset, and brings them to the front together.
    // Called once the preset's audio is running, because plugin editors can be slow to open.
    // Doesn't take keyboard focus from the current app.
    void openPresetPluginWindows();

    // Brings the open plug-in windows in front of other apps' windows, in the order they were last
    // in, without making Curve the active app
    void bringPluginWindowsToFront();

    // Hides the open plug-in windows, or shows and brings them to the front if they're all hidden,
    // from the menu bar panel, a shortcut or a MIDI control. They stay open while hidden
    void togglePluginWindows();

    // Tidies the open plug-in windows into rows on the display most of them are on
    void arrangePluginWindows();
    void showPluginWindows();
    bool arePluginWindowsHidden() const;
    int getNumPluginWindows() const   { return activePluginWindows.size(); }

    // Shows plug-in windows on every Space, or each only on its own, and remembers the choice
    void setPluginWindowsOnAllSpaces (bool shouldBeOnAll);

    // Keeps a node's plug-in window above other apps' windows. Saved with the preset, as its place is
    void setWindowKeptOnTop (NodeID, bool shouldKeepOnTop);
    bool isWindowKeptOnTop (NodeID) const;

    // Stops the plug-in windows being dragged, and hides their title bars if that's set. Each scene has
    // its own, saved with it as its window layout is, so changing it is a change to the preset
    void setPluginWindowPositionsLocked (bool shouldLock);
    void setPluginWindowTitleBarsHiddenWhenLocked (bool shouldHide);
    void setPluginWindowBackdropShownWhenLocked (bool shouldShow);

    // Puts the backdrop behind the plug-in windows while they're locked, if that's set, covering them
    // with a margin, or takes it away. Soon, once for any number of changes
    void updateBackdropSoon();

    bool hasWindowFor (const AudioProcessorGraph::Node&, PluginWindow::Type) const;
    bool isWindowShowing (const AudioProcessorGraph::Node&, PluginWindow::Type) const; // open, and not hidden or minimised
    void closeWindowFor (const AudioProcessorGraph::Node&, PluginWindow::Type);

    static inline const Identifier openWithPresetProp { "openWithPreset" };
    static inline const Identifier freePositionProp { "freePosition" };

    // Left out of the menu bar panel's plug-in list. Shared by a preset's scenes, as its graph is
    static inline const Identifier hiddenFromPanelProp { "hiddenFromPanel" };
    static inline const Identifier varyBySceneProp { Curve::Scenes::varyAttribute };
    static inline const Identifier mutedProp { Curve::Scenes::mutedAttribute };

    // A hidden bus keeps running but shows no pins in the graph editor, for buses that can't be
    // disabled. Hiding one removes its connections. Pass midiBus for the node's MIDI port.
    static constexpr int midiBus = -1;
    void setBusHidden (NodeID, bool isInput, int busIndex, bool shouldBeHidden);
    bool isBusHidden (NodeID, bool isInput, int busIndex) const;
    static inline const Identifier hiddenPortsProp { "hiddenPorts" };

    // While MIDI learning, plugin windows float above the editor, so clicking the
    // graph (e.g. a bypass button) doesn't hide the controls still to be touched.
    void setPluginWindowsFloating (bool shouldFloat);

    // Sets the node's bypass and notifies the graph, so the UI and the loopback tap follow.
    void setNodeBypassed (NodeID, bool shouldBeBypassed);

    // Bypasses every effect that's on, to hear the preset's sources without them, and turns them back
    // on. Doesn't change the preset: it isn't marked as changed, and a save keeps them on. Ends when
    // another preset or scene loads. The graph's I/O, loopback, instruments and meters are left alone
    void setBypassingAll (bool shouldBypass);
    bool isBypassingAll() const   { return ! bypassedForComparison.empty(); }

    // A node that varies by scene keeps its own settings in each scene of the preset, rather
    // than sharing them. Bypass always varies by scene.
    void setNodeVariesByScene (NodeID, bool);
    bool doesNodeVaryByScene (NodeID) const;

    // Not the graph's I/O nodes, or those set in Audio Settings, which have no settings of their own
    static bool canVaryByScene (AudioProcessor&);

    // A muted node passes on nothing and isn't processed: unlike bypass, which passes its input
    // straight through, it cuts the path it's on. Like bypass, each scene has its own.
    void setNodeMuted (NodeID, bool shouldBeMuted);
    bool isNodeMuted (NodeID) const;

    // The node's custom name if it has one, otherwise its processor's name.
    String getNodeDisplayName (NodeID) const;

    //==============================================================================
    // MIDI mappings for this preset's nodes. Changing them marks the preset as edited.
    const MidiMappingSet& getMidiMappings() const { return midiMappings; }
    void assignMidiMapping (const MidiMapping&);
    void removeMidiMappingFor (const MidiMappingTarget&);
    void removeMidiMappingsForNode (NodeID);

    // e.g. "CC 21 · Ch 1 → Gain 1, Note C3 · Ch 1 → Bypass"
    String describeMidiMappingsForNode (NodeID) const;
    String describeMidiMappingTarget (const MidiMappingTarget&) const;

    //==============================================================================
    void audioProcessorParameterChanged (AudioProcessor*, int, float) override {}
    void audioProcessorChanged (AudioProcessor*, const ChangeDetails&) override { changed(); }

    //==============================================================================
    // Without the plug-ins' states, it's quick, e.g. to compare a graph's structure
    std::unique_ptr<XmlElement> createXml (bool includeStates = true) const;
    void restoreFromXml (const XmlElement&, bool restorePluginWindows = false);

    void setRestorePluginWindowsOnLoad (bool shouldRestore) { restorePluginWindowsOnLoad = shouldRestore; }
    bool getRestorePluginWindowsOnLoad() const             { return restorePluginWindowsOnLoad; }

    static const char* getFilenameSuffix()      { return ".filtergraph"; }
    static const char* getFilenameWildcard()    { return "*.filtergraph"; }

    static File getPresetsDirectory();

    // Sorted alphabetically: the order of the menu, its number shortcuts and MIDI program changes.
    static Array<File> getPresetFiles();

    //==============================================================================
    // Scenes: presets sharing their plug-ins and connections (see SceneSync.h)

    // The file's scene group, read from its outer element only; empty if it isn't a scene
    static String readSceneGroup (const File&);

    // The presets in a scene group, sorted like getPresetFiles()
    static Array<File> getScenesInGroup (const String& group);

    // The loaded preset's scene group, or empty
    const String& getSceneGroup() const { return sceneGroup; }

    // The colour that marks a group's scenes, the same wherever they're shown
    static Colour getSceneGroupColour (const String& group);

    // Switching to another scene of the loaded group with the same structure as the live graph,
    // e.g. from the menu, sets the plug-ins' state to the scene's where it differs, along with what
    // each scene has of its own (bypass, mute, tempo, windows), rather than loading every plug-in
    // again, so the scene is as saved. Prepare returns the scene to switch to, or nothing if it
    // has to be loaded: not a scene of the group, nodes or connections changed since loading, or a
    // plug-in still loading or missing. Apply it with the audio stopped.
    // whyLoadedInFull: for a scene of the group that has to be loaded in full, why, to show the user
    std::unique_ptr<XmlElement> prepareSceneSwitch (const File&, String* whyLoadedInFull = nullptr) const;
    void switchScene (const File&, const XmlElement& scene);

    // Takes a preset out of its scene group. It sounds the same.
    static Result makeSeparatePreset (const File&, PluginGraph* loadedGraph);

    struct SceneSave
    {
        File file;
        Array<File> otherScenes;   // rewritten with the shared changes
        Array<File> leftOut;       // in the group, but no longer sharing the graph, so left as they are
        Array<File> unchanged;     // in the group and already sharing what's saved, so not rewritten
        Curve::Scenes::SharedChanges changes;   // what saving changes in the scenes rewritten
        uint32 graphGeneration = 0;             // the loaded preset it was prepared for
    };

    // Saving the live graph as a scene: over the loaded scene, or as a new scene of the loaded
    // preset, which becomes a scene too if it isn't one yet
    SceneSave prepareSceneSave (const File&, bool isNewScene) const;

    // Whether a preset is a scene of the loaded preset's group that still shares its graph
    bool sharesLoadedGraph (const File&) const;

    // Fails without writing anything if another preset has been loaded since the save was
    // prepared (e.g. a default preset, or a MIDI program change, while a dialog was open):
    // its scene files are the old preset's, and would all be rewritten from the new graph.
    // Also if it would rewrite other scenes while a plug-in failed to load or is still loading,
    // as the live graph doesn't have it, and every scene would lose it.
    // wasPresetSaved: whether save.file was written, even if updating another scene failed
    Result saveScene (const SceneSave&, bool* wasPresetSaved = nullptr);

    // Changes whenever another preset is loaded or started (not when saving)
    uint32 getGraphGeneration() const { return restoreGeneration; }

    // Saves the live graph as a preset of its own
    Result saveSeparatePreset (const File&);

    // The loaded preset's file is gone (e.g. moved to the Trash): it's no longer a scene of its
    // group either, so a later save can't rewrite the scenes it shared with
    void forgetFile() { setFile ({}); sceneGroup.clear(); loadedXml.reset(); }

    //==============================================================================
    void newDocument();
    String getDocumentTitle() override;
    Result loadDocument (const File& file) override;
    Result saveDocument (const File& file) override;
    File getLastDocumentOpened() override;
    void setLastDocumentOpened (const File& file) override;
    File getSuggestedSaveAsFile (const File& defaultFile) override;

    //==============================================================================
    TransportGraph graph;

    //==============================================================================
    // The preset's tempo, which each scene has its own of, and the transport plug-ins follow.
    // Changing the tempo marks the preset as changed; starting and stopping doesn't.
    HostTransport& getTransport()                { return graph.transport; }
    void setTempo (double bpm);
    void tapTempo();
    void setTransportPlaying (bool);

private:
    //==============================================================================
    AudioPluginFormatManager& formatManager;
    KnownPluginList& knownPlugins;
    OwnedArray<PluginWindow> activePluginWindows;
    ScopedMessageBox messageBox;
    bool restorePluginWindowsOnLoad = false;
    bool pluginWindowsFloating = false;
    MidiMappingSet midiMappings;


    std::set<uint32> bypassedForComparison; // by setBypassingAll(), to turn back on
    bool isApplyingComparison = false;      // while setBypassingAll() sets them

    WindowArrangement::Backdrop backdrop;
    void applyWindowLock (bool shouldLock); // As a scene has it, without changing the preset
    bool backdropUpdatePending = false;
    void updateBackdrop();

    // The display most of these windows are on, or the main one
    static const Curve::WindowPlacement::Display* findLayoutDisplay (const std::vector<Curve::WindowPlacement::Display>&,
                                                                     const Array<PluginWindow*>&);

    // Moves plug-in windows back onto a display when it's connected again
    std::unique_ptr<MacOSDisplayChangeNotifierBase> displayChangeNotifier;

    // State for the preset restore in progress. Plugins that can't be created
    // synchronously (e.g. AUv3) finish restoring after restoreFromXml() returns;
    // restoreGeneration lets those callbacks detect that the graph was replaced.
    uint32 restoreGeneration = 0;
    int pendingAsyncRestores = 0;
    StringArray pluginsFailedToRestore;
    StringArray pluginsMissingFromLoad;   // as above, but kept until the next load, for scene saves
    std::vector<AudioProcessorGraph::Connection> restoredConnections;

    // The loaded preset's scene group, and the preset as loaded or last saved, to check that the
    // other scenes still share its graph
    String sceneGroup;
    std::unique_ptr<XmlElement> loadedXml;

    // Each node's state as in the file it was loaded from or saved to, and as the plug-in reported
    // it just after. Many plug-ins don't report back exactly the state they were given, and those
    // with a bypass parameter keep the node's bypass in it, which each scene has its own of. While
    // a plug-in still reports the same, its state is saved and compared as in the file, so an
    // unchanged scene isn't a change to the others (see createXml())
    struct StateAsSaved { String inFile, reported; };
    std::map<uint32, StateAsSaved> statesAsSaved;
    void rememberStateAsSaved (AudioProcessorGraph::Node&, const String& inFile);

    // Configure Audio I/O suspends the plug-in and releases its resources while open, then
    // prepares it again and applies its mute when it closes (see IOConfigurationWindow)
    bool isConfiguringAudioIO (NodeID) const;

    // Mutes or unmutes a node. A muted node first rings out: it's still processed, with no new
    // notes and its output discarded, so its tails, including a plug-in's built-in effects, die
    // away out of earshot. Then it's suspended, so it isn't processed. Unmuted while ringing out,
    // it just carries on; unmuted once suspended, it's reset first, and given all notes off (see
    // the JUCE fork's AudioProcessorGraph), so nothing held while suspended comes back
    void applyMute (AudioProcessorGraph::Node&, bool shouldBeMuted);

    static constexpr uint32 ringOutMs = 5000;
    std::map<uint32, uint32> ringOutEnds;   // node uid -> Time::getMillisecondCounter() to suspend at
    void timerCallback() override;

    // Bypasses a node or not. A node without its own bypass parameter isn't processed while
    // bypassed, so it's reset when unbypassed, for the same reason
    void applyBypass (AudioProcessorGraph::Node&, bool shouldBeBypassed);

    // Under its callback lock, as the audio thread may be processing it
    static void resetProcessor (AudioProcessor&);
    void rememberSavedStates (const XmlElement& saved);

    void createNodeFromXml (const XmlElement&, bool restorePluginWindows = false);
    void createNodeFromXmlAsync (std::shared_ptr<const XmlElement> nodeXml,
                                 std::vector<PluginDescriptionAndPreference> candidates,
                                 bool restorePluginWindows);
    void asyncNodeRestoreFinished (std::unique_ptr<AudioPluginInstance>,
                                   const PluginDescriptionAndPreference&,
                                   const XmlElement& nodeXml,
                                   bool restorePluginWindows);
    bool finishNodeFromXml (std::unique_ptr<AudioPluginInstance>,
                            const XmlElement&,
                            bool restorePluginWindows);
    void showRestoreFailures();
    void addPluginCallback (std::unique_ptr<AudioPluginInstance>,
                            const String& error,
                            Point<double>,
                            PluginDescriptionAndPreference::UseARA useARA);
    void changeListenerCallback (ChangeBroadcaster*) override;

    JUCE_DECLARE_WEAK_REFERENCEABLE (PluginGraph)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginGraph)
};
