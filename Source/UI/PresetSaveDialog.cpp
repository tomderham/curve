#include "PresetSaveDialog.h"
#include "DefaultPresets.h"

// Appends the preset suffix rather than using File::withFileExtension, which
// would treat everything after the last dot in the name ("EQ v1.2") as an extension.
static juce::File presetFileForName (const juce::File& directory, juce::String legalName)
{
    const juce::String suffix (PluginGraph::getFilenameSuffix());

    if (legalName.endsWithIgnoreCase (suffix))
        legalName = legalName.dropLastCharacters (suffix.length());

    return directory.getChildFile (legalName + suffix);
}

static juce::String quotedName (const juce::String& name)
{
    return juce::String::fromUTF8 ("\xe2\x80\x9c") + name + juce::String::fromUTF8 ("\xe2\x80\x9d");
}

// "A", "A and B", "A, B and C"
static juce::String joinWithAnd (const juce::StringArray& items)
{
    if (items.size() <= 1)
        return items.joinIntoString ({});

    return items.joinIntoString (", ", 0, items.size() - 1) + " and " + items[items.size() - 1];
}

// Where a row draws its SCENE badge, within the row
static juce::Rectangle<float> getSceneBadgeBounds (int width, int height)
{
    return { (float) width - 138.0f, ((float) height - 18.0f) * 0.5f, 60.0f, 18.0f };
}

// Each group's colour, from its id, so it's the same each time the window opens
static juce::Colour getSceneGroupColour (const juce::String& group)
{
    static const juce::uint32 palette[] = { 0xff4aa3ff, 0xffff9f43, 0xff5fd38d, 0xffe066ff,
                                            0xffffd23f, 0xff42d6d6, 0xffff6b81, 0xffa3a8ff };

    return juce::Colour (palette[(size_t) (group.hashCode64() & 0x7fffffff) % std::size (palette)]);
}

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

    // Saving under a new name: a preset of its own, or a scene sharing the loaded preset's graph
    separateToggle.setButtonText ("Standalone preset");

    for (auto* t : { &separateToggle, &sceneToggle })
    {
        t->setRadioGroupId (1);
        t->setColour (juce::ToggleButton::textColourId, juce::Colours::white);
        t->onClick = [this] { updateSceneChoice(); };
        addChildComponent (t);
    }

    sceneHintLabel.setFont (juce::FontOptions (12.0f));
    sceneHintLabel.setJustificationType (juce::Justification::topLeft);
    sceneHintLabel.setBorderSize ({});
    sceneHintLabel.setMinimumHorizontalScale (1.0f);
    addChildComponent (sceneHintLabel);

    // A scene only when chosen, as saving one can change other presets
    separateToggle.setToggleState (true, juce::dontSendNotification);

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
    startTimer (500);
}

void PresetSaveDialog::timerCallback()
{
    // Another preset was loaded under the dialog: what saving does is now different
    if (graph.getGraphGeneration() != shownGraphGeneration)
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

    auto sceneSection = bounds.removeFromBottom (126);
    bounds.removeFromBottom (6);

    auto nameSection = bounds.removeFromBottom (28);
    bounds.removeFromBottom (12);

    // Below the name, lined up with its editor
    // One option per line, so a long name still has the full width
    sceneSection.removeFromLeft (90);
    // The hint goes under the options, or in their place when saving over the loaded scene
    separateToggle.setBounds (sceneSection.removeFromTop (22));
    sceneToggle.setBounds (sceneSection.removeFromTop (22));
    fitSceneToggleText();

    if (! sceneToggle.isVisible())
        sceneSection = separateToggle.getBounds().getUnion (sceneSection);
    else
        sceneSection = sceneSection.withTrimmedLeft (22).withTrimmedTop (4);

    sceneHintLabel.setBounds (sceneSection);

    // Preset list fills remaining middle space
    presetListBox.setBounds (bounds);

    // Name editor line: label on left (90px), text editor fills rest
    namePromptLabel.setBounds (nameSection.removeFromLeft (90));
    nameEditor.setBounds (nameSection);

    // Bottom buttons
    openFolderButton.setBounds (bottomSection.removeFromLeft (150));

    layOutSaveButton();

    // Once the list has a size. Not on every layout: updateSceneChoice() lays out again on each
    // keystroke and row click, and the selection would keep jumping back to the loaded preset.
    if (! hasScrolledToActivePreset && presetListBox.getHeight() > 0)
    {
        hasScrolledToActivePreset = true;
        scrollToActivePreset();
    }
}

void PresetSaveDialog::layOutSaveButton()
{
    // Right aligned, wide enough for "Save and Update 2 Scenes"
    auto bottomSection = getLocalBounds().reduced (20, 16).removeFromBottom (32);
    bottomSection.removeFromLeft (158);

    const auto textWidth = juce::GlyphArrangement::getStringWidthInt (getLookAndFeel().getTextButtonFont (saveButton, 32),
                                                                       saveButton.getButtonText());

    saveButton.setBounds (bottomSection.removeFromRight (juce::jlimit (80, juce::jmax (80, bottomSection.getWidth() - 88), textWidth + 28)));
    bottomSection.removeFromRight (8);
    cancelButton.setBounds (bottomSection.removeFromRight (80));
}

void PresetSaveDialog::refreshPresetList()
{
    auto presetsDir = getPresetsDirectory();
    presetFiles = presetsDir.findChildFiles (juce::File::findFiles, false, "*.filtergraph");
    presetFiles.sort();
    currentActivePreset = graph.getFile();
    shownGraphGeneration = graph.getGraphGeneration();

    sceneGroups.clear();
    sceneGroupSizes.clear();

    // A preset renamed, trashed or made standalone changes which scenes a save updates
    newScenePlan.reset();
    sceneUpdatePlans.clear();
    sharesLoadedGraph.clear();

    for (const auto& file : presetFiles)
    {
        const auto group = PluginGraph::readSceneGroup (file);
        sceneGroups[file] = group;

        if (group.isNotEmpty())
            ++sceneGroupSizes[group];
    }

    updateSceneNames();
    updateSceneChoice();

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

    const auto group = getSceneGroupOf (file);
    const auto groupColour = getSceneGroupColour (group);
    const auto selectedRow = presetListBox.getSelectedRow();
    const bool inSelectedGroup = group.isNotEmpty() && ! rowIsSelected
                              && juce::isPositiveAndBelow (selectedRow, presetFiles.size())
                              && getSceneGroupOf (presetFiles.getReference (selectedRow)) == group;

    if (rowIsSelected)
    {
        g.setColour (juce::Colour (0xff005fb8).withAlpha (0.45f));
        g.fillRoundedRectangle (2.0f, 2.0f, (float) width - 4.0f, (float) height - 4.0f, 4.0f);
        g.setColour (juce::Colour (0xff0078d4));
        g.drawRoundedRectangle (2.0f, 2.0f, (float) width - 4.0f, (float) height - 4.0f, 4.0f, 1.0f);
    }
    else if (inSelectedGroup)
    {
        // The other scenes of the selected scene's group
        g.setColour (groupColour.withAlpha (0.12f));
        g.fillRoundedRectangle (2.0f, 2.0f, (float) width - 4.0f, (float) height - 4.0f, 4.0f);
    }
    else if (rowNumber % 2 == 1)
    {
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        g.fillRect (0, 0, width, height);
    }

    // Scenes of a group share a colour, as a stripe down the row's left edge
    if (group.isNotEmpty())
    {
        g.setColour (groupColour);
        g.fillRoundedRectangle (3.0f, 5.0f, 3.0f, (float) height - 10.0f, 1.5f);
    }

    // Preset Name (bright, clear typography)
    g.setFont (juce::FontOptions (13.0f));
    g.setColour (juce::Colours::white);
    auto name = file.getFileNameWithoutExtension();
    auto textBounds = juce::Rectangle<int> (10, 0, width - 150, height);
    g.drawFittedText (name, textBounds, juce::Justification::centredLeft, 1);

    // Scene Badge: shares its graph with other presets
    if (group.isNotEmpty())
    {
        auto badgeBounds = getSceneBadgeBounds (width, height);
        g.setColour (groupColour.withAlpha (0.18f));
        g.fillRoundedRectangle (badgeBounds, 4.0f);
        g.setColour (groupColour);
        g.drawRoundedRectangle (badgeBounds, 4.0f, 1.0f);

        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawFittedText ("SCENE", badgeBounds.toNearestInt(), juce::Justification::centred, 1);
    }

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

        // Not sent as a text change, so update what saving under this name does here
        updateSceneChoice();
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

    if (isSceneWithOthers (file))
        menu.addItem (4, "Convert to Standalone Preset");

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
        else if (result == 4)
            safeThis->promptMakeSeparate (file);
    });
}

juce::String PresetSaveDialog::getSceneGroupOf (const juce::File& file) const
{
    return isSceneWithOthers (file) ? sceneGroups.at (file) : juce::String();
}

void PresetSaveDialog::selectedRowsChanged (int)
{
    // The selected scene's group is highlighted
    presetListBox.repaint();
}

juce::String PresetSaveDialog::getTooltipForRow (int row)
{
    if (! juce::isPositiveAndBelow (row, presetFiles.size()))
        return {};

    const auto& file = presetFiles.getReference (row);
    const auto group = getSceneGroupOf (file);

    if (group.isEmpty())
        return {};

    // Only over the SCENE badge. The tooltip window asks again as the mouse moves, so it hides off it
    const auto rowBounds = presetListBox.getRowPosition (row, true);
    const auto badge = getSceneBadgeBounds (rowBounds.getWidth(), rowBounds.getHeight()).translated ((float) rowBounds.getX(), (float) rowBounds.getY());

    if (! badge.contains (presetListBox.getMouseXYRelative().toFloat()))
        return {};

    juce::StringArray others;

    for (const auto& f : presetFiles)
        if (f != file && getSceneGroupOf (f) == group)
            others.add (quotedName (f.getFileNameWithoutExtension()));

    return "Scene sharing with " + joinWithAnd (others);
}

bool PresetSaveDialog::isSceneWithOthers (const juce::File& file) const
{
    auto group = sceneGroups.find (file);

    if (group == sceneGroups.end() || group->second.isEmpty())
        return false;

    auto size = sceneGroupSizes.find (group->second);
    return size != sceneGroupSizes.end() && size->second > 1;
}

void PresetSaveDialog::promptMakeSeparate (const juce::File& file)
{
    juce::Component::SafePointer<PresetSaveDialog> safeThis (this);
    auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
        juce::MessageBoxIconType::QuestionIcon,
        "Convert to Standalone Preset",
        "'" + file.getFileNameWithoutExtension() + "' will no longer share its plug-ins and connections with other scenes. "
        "It keeps its current settings, and saving it no longer changes the other scenes, or the other way round.",
        "Convert",
        "Cancel");

    scopedAlert = juce::AlertWindow::showScopedAsync (options, [safeThis, file] (int result)
    {
        if (result != 1 || safeThis == nullptr)
            return;

        const auto res = PluginGraph::makeSeparatePreset (file, &safeThis->graph);

        if (res.failed())
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                         "Couldn't Convert to Standalone Preset", res.getErrorMessage());

        safeThis->refreshPresetList();
    });
}

juce::File PresetSaveDialog::getTargetFile() const
{
    auto legalName = juce::File::createLegalFileName (nameEditor.getText().trim());

    if (legalName.isEmpty())
        return {};

    return presetFileForName (getPresetsDirectory(), legalName);
}

// The scenes a save changes, by name up to three
static juce::String describeScenes (const juce::Array<juce::File>& files)
{
    juce::StringArray names;

    for (const auto& f : files)
        names.add (quotedName (f.getFileNameWithoutExtension()));

    if (names.size() > 3)
    {
        const auto numMore = names.size() - 2;
        names.removeRange (2, numMore);
        names.add (juce::String (numMore) + " other scenes");
    }

    return joinWithAnd (names);
}

static juce::String describeLeftOut (const juce::Array<juce::File>& leftOut)
{
    if (leftOut.isEmpty())
        return {};

    return "Not updated, as plug-ins or connections differ: " + describeScenes (leftOut);
}

/*  What saving does to the other scenes, said before saving rather than after, one line each.
    Settings of nodes that aren't set to Vary by Scene are shared, so a setting made for this
    scene alone would also change in the others: say which nodes.
*/
static juce::String describeSharedChanges (const Curve::Scenes::SharedChanges& changes, const juce::String& scenes)
{
    juce::StringArray lines;

    if (changes.changesSound())
    {
        juce::StringArray items;
        items.addArray (changes.changedNodes);

        for (const auto& removed : changes.removedNodes)
            items.add (removed + " (removed)");

        if (changes.connectionsChanged)
            items.add ("connections");

        lines.add ("Also changes: " + joinWithAnd (items) + " in " + scenes);
    }

    if (! changes.addedNodes.isEmpty())
        lines.add ("Also adds, bypassed: " + joinWithAnd (changes.addedNodes) + " in " + scenes);

    return lines.joinIntoString ("\n");
}

juce::String PresetSaveDialog::describeSceneUpdate (const PluginGraph::SceneSave& plan)
{
    juce::StringArray lines;

    // Scenes already sharing what's saved aren't rewritten, so aren't mentioned
    if (! plan.otherScenes.isEmpty())
    {
        const auto scenes = describeScenes (plan.otherScenes);

        if (plan.changes.changesSound() || ! plan.changes.addedNodes.isEmpty())
            lines.add (describeSharedChanges (plan.changes, scenes));
        else
            lines.add ("Also updates " + scenes + ", without changing how "
                       + (plan.otherScenes.size() == 1 ? "it sounds" : "they sound"));
    }

    lines.add (describeLeftOut (plan.leftOut));
    lines.removeEmptyStrings();
    return lines.joinIntoString ("\n");
}

juce::String PresetSaveDialog::getSceneUpdateButtonText (const PluginGraph::SceneSave& plan, bool replaces)
{
    const auto num = plan.otherScenes.size();
    return juce::String (replaces ? "Replace and Update " : "Save and Update ") + juce::String (num) + (num == 1 ? " Scene" : " Scenes");
}

const PluginGraph::SceneSave& PresetSaveDialog::getScenePlan (bool isNewScene, const juce::File& target)
{
    // A new scene is the same for any new name, but mustn't count its own file among the others
    if (isNewScene)
    {
        if (! newScenePlan.has_value())
            newScenePlan = graph.prepareSceneSave ({}, true);

        return *newScenePlan;
    }

    auto plan = sceneUpdatePlans.find (target);

    if (plan == sceneUpdatePlans.end())
        plan = sceneUpdatePlans.emplace (target, graph.prepareSceneSave (target, false)).first;

    return plan->second;
}

bool PresetSaveDialog::isSceneOfLoadedGroup (const juce::File& target)
{
    if (target == juce::File() || ! target.existsAsFile() || graph.getSceneGroup().isEmpty())
        return false;

    if (target == graph.getFile())
        return true;

    // Another scene of the group: saving over it keeps it a scene, as it's most likely meant to
    auto shares = sharesLoadedGraph.find (target);

    if (shares == sharesLoadedGraph.end())
        shares = sharesLoadedGraph.emplace (target, graph.sharesLoadedGraph (target)).first;

    return shares->second;
}

void PresetSaveDialog::updateSceneNames()
{
    // As the list is now: the loaded preset may have been renamed or trashed, and another scene
    // renamed, trashed or made standalone
    const auto current = graph.getFile();
    canSaveAsScene = current.existsAsFile() && current.getParentDirectory() == getPresetsDirectory();

    sceneNames.clear();

    if (! canSaveAsScene)
        return;

    sceneNames.add (current.getFileNameWithoutExtension());

    // Only the scenes still sharing its graph, so not one that would be left out
    if (graph.getSceneGroup().isNotEmpty())
    {
        const auto& plan = getScenePlan (true);
        auto sharing = plan.otherScenes;
        sharing.addArray (plan.unchanged);

        for (const auto& f : sharing)
            if (f != current)
                sceneNames.add (f.getFileNameWithoutExtension());
    }

    fitSceneToggleText();
}

void PresetSaveDialog::updateSceneChoice()
{
    const auto target = getTargetFile();
    const bool isNewName = canSaveAsScene && target != juce::File() && ! target.existsAsFile();
    const bool isGroupScene = isSceneOfLoadedGroup (target);

    separateToggle.setVisible (isNewName);
    sceneToggle.setVisible (isNewName);

    juce::String hint, saveText ("Save");
    bool isWarning = false;

    if (isNewName && sceneToggle.getToggleState())
    {
        const auto& plan = getScenePlan (true);

        auto sharing = plan.otherScenes;
        sharing.addArray (plan.unchanged);

        juce::StringArray lines;

        if (! sharing.isEmpty())
        {
            lines.add ("Specific to this scene: Bypass on all nodes, all settings on nodes with Vary by Scene enabled");
            lines.add ("Shared with " + describeScenes (sharing) + ": All other settings");
        }

        lines.add (describeSharedChanges (plan.changes, describeScenes (plan.otherScenes)));
        lines.add (describeLeftOut (plan.leftOut));
        lines.removeEmptyStrings();
        hint = lines.joinIntoString ("\n");

        isWarning = plan.changes.changesSound();
    }
    else if (isGroupScene)
    {
        const auto& plan = getScenePlan (false, target);
        const bool replaces = target != graph.getFile();

        if (replaces)
            hint << "Replaces " << quotedName (target.getFileNameWithoutExtension())
                 << ", including its bypass settings. It stays a scene.\n";

        hint << describeSceneUpdate (plan);
        isWarning = plan.changes.changesSound();

        if (! plan.otherScenes.isEmpty())
            saveText = getSceneUpdateButtonText (plan, replaces);
        else if (replaces)
            saveText = "Replace";
    }

    sceneHintLabel.setText (hint.trim(), juce::dontSendNotification);
    sceneHintLabel.setColour (juce::Label::textColourId, isWarning ? juce::Colour (0xffffc04d)
                                                                   : juce::Colours::white.withAlpha (0.6f));
    sceneHintLabel.setVisible (hint.isNotEmpty());

    if (saveButton.getButtonText() != saveText)
        saveButton.setButtonText (saveText);

    resized();
}

// e.g. Scene sharing with "Studio", or with "Studio", "Late" and 2 more. Not "Scene of", as the
// scenes are equals: none is the original
static juce::String describeSceneOption (const juce::StringArray& names, int numShown)
{
    if (names.size() == 1)
        return "Scene sharing with " + quotedName (names[0]);

    juce::StringArray shown;

    for (int i = 0; i < numShown; ++i)
        shown.add (quotedName (names[i]));

    const auto numMore = names.size() - numShown;

    if (numMore > 0)
        shown.add (juce::String (numMore) + " more");

    const auto last = shown[shown.size() - 1];
    shown.remove (shown.size() - 1);

    return "Scene sharing with " + (shown.isEmpty() ? last : shown.joinIntoString (", ") + " and " + last);
}

void PresetSaveDialog::fitSceneToggleText()
{
    if (sceneNames.isEmpty() || sceneToggle.getWidth() <= 0)
        return;

    // As LookAndFeel_V4::drawToggleButton lays out its text, after the tick box
    const auto fontSize = juce::jmin (15.0f, (float) sceneToggle.getHeight() * 0.75f);
    const juce::Font font (juce::FontOptions { fontSize });
    const auto available = (float) (sceneToggle.getWidth() - juce::roundToInt (fontSize * 1.1f) - 12);

    const auto fits = [&] (const juce::String& text)
    {
        return juce::GlyphArrangement::getStringWidth (font, text) <= available;
    };

    // Fewer names, then the first one shortened
    for (int numShown = juce::jmin (3, sceneNames.size()); numShown >= 1; --numShown)
    {
        const auto text = describeSceneOption (sceneNames, numShown);

        if (fits (text))
        {
            sceneToggle.setButtonText (text);
            return;
        }
    }

    auto names = sceneNames;
    auto first = names[0];

    while (first.length() > 1)
    {
        first = first.dropLastCharacters (1).trimEnd();
        names.set (0, first + juce::String::fromUTF8 ("\xe2\x80\xa6"));

        if (fits (describeSceneOption (names, 1)))
            break;
    }

    sceneToggle.setButtonText (describeSceneOption (names, 1));
}

void PresetSaveDialog::textEditorTextChanged (juce::TextEditor&)
{
    updateSceneChoice();
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
                auto newFile = presetFileForName (file.getParentDirectory(), legalName);
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
                        DefaultPresets::presetRenamed (file, newFile);

                        if (safeThis->graph.getFile() == file)
                            safeThis->graph.setFile (newFile);

                        // Keep the name field on the renamed preset, so it isn't taken for a new one
                        if (safeThis->nameEditor.getText().trim() == file.getFileNameWithoutExtension())
                            safeThis->nameEditor.setText (newFile.getFileNameWithoutExtension(), false);

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
                    safeThis->graph.forgetFile();

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

    auto targetFile = presetFileForName (getPresetsDirectory(), legalName);

    // Saving over the loaded scene, or another of its group, also updates the other scenes, as
    // the hint says
    if (isSceneOfLoadedGroup (targetFile))
    {
        if (targetFile == graph.getFile())
        {
            saveScene (false, targetFile);
            return;
        }

        // Another scene of the group: replacing it loses its own bypass and Vary by Scene settings,
        // so ask, as for overwriting any other preset (double-click and Return skip the button)
        juce::Component::SafePointer<PresetSaveDialog> safeThis (this);
        auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
            juce::MessageBoxIconType::WarningIcon,
            "Replace Scene?",
            "Replace the scene '" + legalName + "' with this one? It loses its own bypass settings, and its "
            "settings on nodes with Vary by Scene enabled. It stays a scene of the group.",
            "Replace",
            "Cancel");

        scopedAlert = juce::AlertWindow::showScopedAsync (options, [safeThis, targetFile] (int result)
        {
            if (result == 1 && safeThis != nullptr)
                safeThis->saveScene (false, targetFile);
        });
        return;
    }

    if (targetFile.existsAsFile())
    {
        auto message = "There is already a preset named '" + legalName + "'.\n\nDo you want to overwrite it?";

        if (isSceneWithOthers (targetFile))
            message << "\n\nIt's a scene, sharing its plug-ins and connections with other presets. "
                       "Overwriting it makes it a standalone preset and doesn't change the others.";

        juce::Component::SafePointer<PresetSaveDialog> safeThis (this);
        auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
            juce::MessageBoxIconType::WarningIcon,
            "Preset Already Exists",
            message,
            "Overwrite",
            "Cancel");

        scopedAlert = juce::AlertWindow::showScopedAsync (options, [safeThis, targetFile] (int result)
        {
            if (result == 1 && safeThis != nullptr)
                safeThis->doSave (targetFile);
        });
    }
    else if (canSaveAsScene && sceneToggle.getToggleState())
    {
        saveScene (true, targetFile);
    }
    else
    {
        doSave (targetFile);
    }
}

void PresetSaveDialog::saveScene (bool isNewScene, const juce::File& file)
{
    // The hint has already said what this does to the other scenes
    auto sceneSave = getScenePlan (isNewScene, file);
    sceneSave.file = file;

    // Stays open if the preset itself wasn't saved, as doSave() does, to retry or save another way.
    // Saving it can succeed while rewriting some of the other scenes fails: still saved, so close
    bool isSaved = false;
    const auto res = graph.saveScene (sceneSave, &isSaved);

    if (res.failed())
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     "Save Failed", "Could not save preset:\n" + res.getErrorMessage());

    if (! isSaved)
        return;

    graph.setLastDocumentOpened (file);
    closeDialog();
}

void PresetSaveDialog::doSave (const juce::File& file)
{
    auto res = graph.saveSeparatePreset (file);
    if (res.wasOk())
    {
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
