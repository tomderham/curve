#include "PresetSaveDialog.h"

juce::File PresetSaveDialog::getPresetsDirectory()
{
    auto appDataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                        .getChildFile ("Application Support")
                        .getChildFile (juce::JUCEApplication::getInstance()->getApplicationName());
    auto presetsDir = appDataDir.getChildFile ("Presets");
    if (! presetsDir.exists())
        presetsDir.createDirectory();

    return presetsDir;
}

PresetSaveDialog::PresetSaveDialog (PluginGraph& g)
    : graph (g)
{
    // Title & subtitle
    titleLabel.setText ("Save Preset", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Select an existing preset to overwrite, or type a new name below.", juce::dontSendNotification);
    subtitleLabel.setFont (juce::FontOptions (12.0f));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.65f));
    addAndMakeVisible (subtitleLabel);

    // List header
    listHeaderLabel.setText ("Existing Presets:", juce::dontSendNotification);
    listHeaderLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    listHeaderLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    addAndMakeVisible (listHeaderLabel);

    // List Box
    presetListBox.setModel (this);
    presetListBox.setRowHeight (28);
    presetListBox.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1e1e1e));
    presetListBox.setColour (juce::ListBox::outlineColourId, juce::Colour (0xff383838));
    addAndMakeVisible (presetListBox);

    // Name Prompt & Editor
    namePromptLabel.setText ("Preset Name:", juce::dontSendNotification);
    namePromptLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    namePromptLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    addAndMakeVisible (namePromptLabel);

    auto currentTitle = graph.getDocumentTitle();
    nameEditor.setText (currentTitle.isEmpty() || currentTitle == "Unnamed" ? "" : currentTitle);
    nameEditor.setFont (juce::FontOptions (13.0f));
    nameEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff282828));
    nameEditor.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    nameEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff484848));
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff0078d4));
    nameEditor.addListener (this);
    addAndMakeVisible (nameEditor);

    // Buttons
    openFolderButton.setButtonText ("Open Presets Folder");
    openFolderButton.onClick = [] { getPresetsDirectory().startAsProcess(); };
    addAndMakeVisible (openFolderButton);

    cancelButton.setButtonText ("Cancel");
    cancelButton.onClick = [this] { closeDialog(); };
    addAndMakeVisible (cancelButton);

    saveButton.setButtonText ("Save");
    saveButton.onClick = [this] { attemptSave(); };
    saveButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0066cc));
    saveButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible (saveButton);

    refreshPresetList();
}

PresetSaveDialog::~PresetSaveDialog()
{
    nameEditor.removeListener (this);
    presetListBox.setModel (nullptr);
}

void PresetSaveDialog::parentHierarchyChanged()
{
    nameEditor.grabKeyboardFocus();
    nameEditor.selectAll();
    scrollToActivePreset();
}

void PresetSaveDialog::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void PresetSaveDialog::resized()
{
    auto bounds = getLocalBounds().reduced (20, 16);

    titleLabel.setBounds (bounds.removeFromTop (24));
    subtitleLabel.setBounds (bounds.removeFromTop (18));

    bounds.removeFromTop (10);

    listHeaderLabel.setBounds (bounds.removeFromTop (18));
    bounds.removeFromTop (4);

    auto bottomSection = bounds.removeFromBottom (32);
    bounds.removeFromBottom (12);

    auto nameSection = bounds.removeFromBottom (28);
    bounds.removeFromBottom (12);

    // Preset list fills remaining middle space
    presetListBox.setBounds (bounds);

    // Name editor line: label on left (90px), text editor fills rest
    namePromptLabel.setBounds (nameSection.removeFromLeft (90));
    nameEditor.setBounds (nameSection);

    // Bottom buttons
    openFolderButton.setBounds (bottomSection.removeFromLeft (150));

    saveButton.setBounds (bottomSection.removeFromRight (80));
    bottomSection.removeFromRight (8);
    cancelButton.setBounds (bottomSection.removeFromRight (80));

    scrollToActivePreset();
}

void PresetSaveDialog::refreshPresetList()
{
    auto presetsDir = getPresetsDirectory();
    presetFiles = presetsDir.findChildFiles (juce::File::findFiles, false, "*.filtergraph");
    presetFiles.sort();
    currentActivePreset = graph.getFile();

    listHeaderLabel.setText ("Existing Presets (" + juce::String (presetFiles.size()) + "):",
                             juce::dontSendNotification);

    presetListBox.updateContent();
    presetListBox.repaint();
    scrollToActivePreset();
}

void PresetSaveDialog::scrollToActivePreset()
{
    if (presetFiles.isEmpty())
        return;

    int activeRow = -1;
    for (int i = 0; i < presetFiles.size(); ++i)
    {
        if (presetFiles.getReference (i) == currentActivePreset)
        {
            activeRow = i;
            break;
        }
    }

    if (activeRow >= 0)
    {
        presetListBox.selectRow (activeRow, false);
        presetListBox.scrollToEnsureRowIsOnscreen (activeRow);
    }
}

void PresetSaveDialog::closeDialog()
{
    if (closeCallback)
    {
        closeCallback();
    }

    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
    {
        dw->exitModalState (0);
        dw->setVisible (false);
    }
    else if (auto* top = getTopLevelComponent())
    {
        top->setVisible (false);
    }
}

int PresetSaveDialog::getNumRows()
{
    return presetFiles.size();
}

void PresetSaveDialog::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow (rowNumber, presetFiles.size()))
        return;

    const auto& file = presetFiles.getReference (rowNumber);
    bool isActive = (file == currentActivePreset);

    if (rowIsSelected)
    {
        g.setColour (juce::Colour (0xff005fb8).withAlpha (0.45f));
        g.fillRoundedRectangle (2.0f, 2.0f, (float) width - 4.0f, (float) height - 4.0f, 4.0f);
        g.setColour (juce::Colour (0xff0078d4));
        g.drawRoundedRectangle (2.0f, 2.0f, (float) width - 4.0f, (float) height - 4.0f, 4.0f, 1.0f);
    }
    else if (rowNumber % 2 == 1)
    {
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        g.fillRect (0, 0, width, height);
    }

    // Preset Name (bright, clear typography)
    g.setFont (juce::FontOptions (13.0f));
    g.setColour (juce::Colours::white);
    auto name = file.getFileNameWithoutExtension();
    auto textBounds = juce::Rectangle<int> (10, 0, width - 85, height);
    g.drawFittedText (name, textBounds, juce::Justification::centredLeft, 1);

    // Active Badge
    if (isActive)
    {
        auto badgeBounds = juce::Rectangle<float> ((float) width - 72.0f, ((float) height - 18.0f) * 0.5f, 60.0f, 18.0f);
        g.setColour (juce::Colour (0xff28a745).withAlpha (0.25f));
        g.fillRoundedRectangle (badgeBounds, 4.0f);
        g.setColour (juce::Colour (0xff4cd964));
        g.drawRoundedRectangle (badgeBounds, 4.0f, 1.0f);

        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawFittedText ("ACTIVE", badgeBounds.toNearestInt(), juce::Justification::centred, 1);
    }
}

void PresetSaveDialog::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, presetFiles.size()))
        return;

    const auto& file = presetFiles.getReference (row);

    if (e.mods.isPopupMenu())
    {
        showRowContextMenu (file);
    }
    else
    {
        nameEditor.setText (file.getFileNameWithoutExtension(), false);
        nameEditor.selectAll();
    }
}

void PresetSaveDialog::listBoxItemDoubleClicked (int row, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    if (juce::isPositiveAndBelow (row, presetFiles.size()))
    {
        nameEditor.setText (presetFiles.getReference (row).getFileNameWithoutExtension(), false);
        attemptSave();
    }
}

void PresetSaveDialog::showRowContextMenu (const juce::File& file)
{
    juce::PopupMenu menu;
    menu.addItem (1, "Rename...");
    menu.addItem (2, "Move to Trash");
    menu.addSeparator();
    menu.addItem (3, "Reveal in Finder");

    juce::Component::SafePointer<PresetSaveDialog> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safeThis, file] (int result)
    {
        if (safeThis == nullptr)
            return;

        if (result == 1)
            safeThis->promptRename (file);
        else if (result == 2)
            safeThis->promptMoveToTrash (file);
        else if (result == 3)
            file.revealToUser();
    });
}

void PresetSaveDialog::promptRename (const juce::File& file)
{
    auto* alert = new juce::AlertWindow ("Rename Preset",
                                         "Enter a new name for preset '" + file.getFileNameWithoutExtension() + "':",
                                         juce::AlertWindow::QuestionIcon);
    alert->addTextEditor ("newName", file.getFileNameWithoutExtension());
    alert->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetSaveDialog> safeThis (this);
    alert->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, file, alert] (int result)
    {
        if (result == 1 && safeThis != nullptr)
        {
            auto rawName = alert->getTextEditorContents ("newName").trim();
            auto legalName = juce::File::createLegalFileName (rawName);
            if (legalName.isNotEmpty())
            {
                auto newFile = file.getSiblingFile (legalName).withFileExtension ("filtergraph");
                if (newFile != file)
                {
                    if (newFile.existsAsFile())
                    {
                        juce::NativeMessageBox::showMessageBoxAsync (
                            juce::MessageBoxIconType::WarningIcon,
                            "Cannot Rename",
                            "A preset named '" + legalName + "' already exists.");
                    }
                    else if (file.moveFileTo (newFile))
                    {
                        if (safeThis->graph.getFile() == file)
                            safeThis->graph.setFile (newFile);

                        safeThis->refreshPresetList();
                    }
                }
            }
        }
    }), true);
}

void PresetSaveDialog::promptMoveToTrash (const juce::File& file)
{
    juce::Component::SafePointer<PresetSaveDialog> safeThis (this);
    auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
        juce::MessageBoxIconType::QuestionIcon,
        "Move to Trash",
        "Are you sure you want to move preset '" + file.getFileNameWithoutExtension() + "' to the Trash?",
        "Move to Trash",
        "Cancel");

    scopedAlert = juce::AlertWindow::showScopedAsync (options, [safeThis, file] (int result)
    {
        if (result == 1 && safeThis != nullptr)
        {
            if (file.moveToTrash())
            {
                if (safeThis->graph.getFile() == file)
                    safeThis->graph.setFile ({});

                safeThis->refreshPresetList();
            }
        }
    });
}

void PresetSaveDialog::textEditorReturnKeyPressed (juce::TextEditor&)
{
    attemptSave();
}

void PresetSaveDialog::textEditorEscapeKeyPressed (juce::TextEditor&)
{
    closeDialog();
}

void PresetSaveDialog::attemptSave()
{
    auto rawName = nameEditor.getText().trim();
    if (rawName.isEmpty())
    {
        nameEditor.grabKeyboardFocus();
        return;
    }

    auto legalName = juce::File::createLegalFileName (rawName);
    if (legalName.isEmpty())
        legalName = "unnamed";

    auto targetFile = getPresetsDirectory().getChildFile (legalName).withFileExtension ("filtergraph");

    if (targetFile.existsAsFile())
    {
        juce::Component::SafePointer<PresetSaveDialog> safeThis (this);
        auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
            juce::MessageBoxIconType::WarningIcon,
            "Preset Already Exists",
            "There is already a preset named '" + legalName + "'.\n\nDo you want to overwrite it?",
            "Overwrite",
            "Cancel");

        scopedAlert = juce::AlertWindow::showScopedAsync (options, [safeThis, targetFile] (int result)
        {
            if (result == 1 && safeThis != nullptr)
                safeThis->doSave (targetFile);
        });
    }
    else
    {
        doSave (targetFile);
    }
}

void PresetSaveDialog::doSave (const juce::File& file)
{
    auto res = graph.saveDocument (file);
    if (res.wasOk())
    {
        graph.setFile (file);
        graph.setLastDocumentOpened (file);
        closeDialog();
    }
    else
    {
        juce::NativeMessageBox::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Save Failed",
            "Could not save preset:\n" + res.getErrorMessage());
    }
}
