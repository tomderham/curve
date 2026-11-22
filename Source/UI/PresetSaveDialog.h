#pragma once

#include <JuceHeader.h>
#include "../Plugins/PluginGraph.h"

/** A toggle drawn as a radio button, for a choice of one of several. */
struct RadioToggle final : public juce::ToggleButton
{
    using juce::ToggleButton::ToggleButton;
    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class PresetSaveDialog final : public juce::Component,
                               public juce::ListBoxModel,
                               public juce::TextEditor::Listener,
                               private juce::Timer
{
public:
    PresetSaveDialog (PluginGraph& graph);
    ~PresetSaveDialog() override;

    void setCloseCallback (std::function<void()> callback) { closeCallback = std::move (callback); }

    // Component overrides
    void paint (juce::Graphics& g) override;
    void resized() override;
    void parentHierarchyChanged() override;

    // ListBoxModel overrides
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent& e) override;
    void selectedRowsChanged (int lastRowSelected) override;
    juce::String getTooltipForRow (int row) override;

    // TextEditor::Listener overrides
    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void textEditorEscapeKeyPressed (juce::TextEditor&) override;
    void textEditorTextChanged (juce::TextEditor&) override;

    static juce::File getPresetsDirectory();

    // What saving over a scene does to the others of its group, for asking before saving
    static juce::String describeSceneUpdate (const PluginGraph::SceneSave&);
    static juce::String getSceneUpdateButtonText (const PluginGraph::SceneSave&, bool replaces = false);

private:
    void refreshPresetList();
    void scrollToActivePreset();
    void closeDialog();
    void attemptSave();
    void doSave (const juce::File& file);
    void promptRename (const juce::File& file);
    void promptMoveToTrash (const juce::File& file);
    void showRowContextMenu (const juce::File& file);
    void promptMakeSeparate (const juce::File& file);
    void saveScene (bool isNewScene, const juce::File& file);
    const PluginGraph::SceneSave& getScenePlan (bool isNewScene, const juce::File& target = {});
    bool isSceneOfLoadedGroup (const juce::File& target);
    void updateSceneNames();
    juce::File getTargetFile() const;
    void updateSceneChoice();
    void layOutSaveButton();
    bool isSceneWithOthers (const juce::File& file) const;
    void timerCallback() override;
    juce::String getSceneGroupOf (const juce::File& file) const;

    PluginGraph& graph;
    std::function<void()> closeCallback;

    juce::Array<juce::File> presetFiles;
    juce::File currentActivePreset;
    juce::uint32 shownGraphGeneration = 0;   // the loaded preset the list, plans and hints are for

    // Each preset's scene group, and how many presets share each group
    std::map<juce::File, juce::String> sceneGroups;
    std::map<juce::String, int> sceneGroupSizes;

    // A new name can be a scene of the loaded preset
    bool canSaveAsScene = false;
    bool hasScrolledToActivePreset = false;

    // What saving as a new scene, or over a scene of the loaded group, does to the other scenes.
    // Made once each, as it reads every plug-in's state, and edits can't be made while this is
    // open; along with which presets are scenes still sharing the loaded graph. All made again
    // if another preset loads meanwhile (e.g. a MIDI program change): see timerCallback()
    std::optional<PluginGraph::SceneSave> newScenePlan;
    std::map<juce::File, PluginGraph::SceneSave> sceneUpdatePlans;
    std::map<juce::File, bool> sharesLoadedGraph;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label listHeaderLabel;
    juce::ListBox presetListBox;
    juce::Label namePromptLabel;
    juce::TextEditor nameEditor;
    RadioToggle separateToggle, sceneToggle;
    juce::Label sharesLabel;      // the scenes it shares with
    juce::Label sceneHintLabel;   // what each scene keeps of its own, and scenes left out
    juce::Label changesLabel;     // what saving changes in the other scenes

    void layOutSceneSection (juce::Rectangle<int>);
    int getSceneSectionHeight (int width) const;
    juce::TextButton openFolderButton;
    juce::TextButton cancelButton;
    juce::TextButton saveButton;

    juce::ScopedMessageBox scopedAlert;
    juce::TooltipWindow tooltipWindow { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetSaveDialog)
};
