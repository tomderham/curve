#pragma once

#include <JuceHeader.h>
#include "../Plugins/PluginGraph.h"

class PresetSaveDialog final : public juce::Component,
                               public juce::ListBoxModel,
                               public juce::TextEditor::Listener
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

    // TextEditor::Listener overrides
    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void textEditorEscapeKeyPressed (juce::TextEditor&) override;

    static juce::File getPresetsDirectory();

private:
    void refreshPresetList();
    void scrollToActivePreset();
    void closeDialog();
    void attemptSave();
    void doSave (const juce::File& file);
    void promptRename (const juce::File& file);
    void promptMoveToTrash (const juce::File& file);
    void showRowContextMenu (const juce::File& file);

    PluginGraph& graph;
    std::function<void()> closeCallback;

    juce::Array<juce::File> presetFiles;
    juce::File currentActivePreset;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label listHeaderLabel;
    juce::ListBox presetListBox;
    juce::Label namePromptLabel;
    juce::TextEditor nameEditor;
    juce::TextButton openFolderButton;
    juce::TextButton cancelButton;
    juce::TextButton saveButton;

    juce::ScopedMessageBox scopedAlert;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetSaveDialog)
};
