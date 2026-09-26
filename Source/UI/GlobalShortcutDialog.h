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
#include "GlobalShortcutManager.h"

class GlobalShortcutDialog : public juce::Component {
public:
    using SaveCallback = std::function<void(bool enabled, uint32_t keyCode, uint32_t modifiers, const juce::String& text)>;

    GlobalShortcutDialog(bool initialEnabled,
                         uint32_t initialKeyCode,
                         uint32_t initialModifiers,
                         const juce::String& initialText,
                         SaveCallback onSaveCallback)
        : isEnabled(initialEnabled),
          currentKeyCode(initialKeyCode),
          currentModifiers(initialModifiers),
          currentText(initialText),
          onSave(std::move(onSaveCallback))
    {
        setSize(420, 260);

        titleLabel.setText("Global Menu Shortcut", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);

        descLabel.setText("Open Curve's menu bar popup from anywhere on macOS, even when running in the background.",
                          juce::dontSendNotification);
        descLabel.setFont(juce::FontOptions(12.0f));
        descLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));
        descLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(descLabel);

        updateShortcutDisplay();
        shortcutBox.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        shortcutBox.setColour(juce::Label::textColourId, juce::Colours::white);
        shortcutBox.setColour(juce::Label::backgroundColourId, juce::Colour(0xff222222));
        shortcutBox.setColour(juce::Label::outlineColourId, juce::Colour(0xff444444));
        shortcutBox.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(shortcutBox);

        hintLabel.setText("Click 'Record Shortcut' below, then press your desired key combination.",
                          juce::dontSendNotification);
        hintLabel.setFont(juce::FontOptions(11.0f));
        hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff999999));
        hintLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(hintLabel);

        recordButton.setButtonText("Record Shortcut");
        recordButton.onClick = [this] { startRecording(); };
        addAndMakeVisible(recordButton);

        resetButton.setButtonText("Reset to Default");
        resetButton.onClick = [this] {
            isEnabled = true;
            currentKeyCode = GlobalShortcutManager::defaultCarbonKeyCode;
            currentModifiers = GlobalShortcutManager::defaultCarbonModifiers;
            currentText = GlobalShortcutManager::defaultShortcutText();
            isRecording = false;
            hintLabel.setText("Reset to default (⌥⇧C).", juce::dontSendNotification);
            updateShortcutDisplay();
        };
        addAndMakeVisible(resetButton);

        disableButton.setButtonText("Disable");
        disableButton.onClick = [this] {
            isEnabled = false;
            isRecording = false;
            hintLabel.setText("Global shortcut disabled.", juce::dontSendNotification);
            updateShortcutDisplay();
        };
        addAndMakeVisible(disableButton);

        saveButton.setButtonText("Save");
        saveButton.onClick = [this] {
            if (onSave)
                onSave(isEnabled, currentKeyCode, currentModifiers, currentText);
            closeDialog();
        };
        addAndMakeVisible(saveButton);

        cancelButton.setButtonText("Cancel");
        cancelButton.onClick = [this] { closeDialog(); };
        addAndMakeVisible(cancelButton);

        setWantsKeyboardFocus(true);
    }

    void startRecording() {
        isRecording = true;
        shortcutBox.setText("Press key combination...", juce::dontSendNotification);
        shortcutBox.setColour(juce::Label::outlineColourId, juce::Colour(0xff007acc));
        hintLabel.setText("Press keys now (e.g. ⌥⇧C). Press Esc to cancel.", juce::dontSendNotification);
        grabKeyboardFocus();
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (!isRecording)
            return false;

        if (key.isKeyCode(juce::KeyPress::escapeKey)) {
            isRecording = false;
            hintLabel.setText("Recording cancelled.", juce::dontSendNotification);
            updateShortcutDisplay();
            return true;
        }

        auto mods = key.getModifiers();
        // Require at least one non-shift modifier (Command, Alt/Option, or Ctrl)
        if (!mods.isCommandDown() && !mods.isAltDown() && !mods.isCtrlDown()) {
            hintLabel.setText("Please include at least Command (⌘), Option (⌥), or Control (⌃).",
                              juce::dontSendNotification);
            return true;
        }

        uint32_t carbonCode = 0;
        uint32_t carbonMods = 0;
        if (GlobalShortcutManager::juceKeyPressToCarbon(key, carbonCode, carbonMods)) {
            currentKeyCode = carbonCode;
            currentModifiers = carbonMods;
            currentText = GlobalShortcutManager::carbonShortcutToString(carbonCode, carbonMods);
            isEnabled = true;
            isRecording = false;
            hintLabel.setText("Recorded! Click 'Save' to apply.", juce::dontSendNotification);
            updateShortcutDisplay();
            return true;
        }

        return false;
    }

    void resized() override {
        auto r = getLocalBounds().reduced(20, 15);

        titleLabel.setBounds(r.removeFromTop(26));
        descLabel.setBounds(r.removeFromTop(34));

        r.removeFromTop(8);
        shortcutBox.setBounds(r.removeFromTop(48).reduced(40, 0));
        r.removeFromTop(6);
        hintLabel.setBounds(r.removeFromTop(18));

        r.removeFromTop(12);
        auto actionRow = r.removeFromTop(28);
        int actionBtnW = (actionRow.getWidth() - 16) / 3;
        recordButton.setBounds(actionRow.removeFromLeft(actionBtnW));
        actionRow.removeFromLeft(8);
        resetButton.setBounds(actionRow.removeFromLeft(actionBtnW));
        actionRow.removeFromLeft(8);
        disableButton.setBounds(actionRow);

        r.removeFromTop(14);
        auto bottomRow = r.removeFromBottom(28);
        cancelButton.setBounds(bottomRow.removeFromRight(80));
        bottomRow.removeFromRight(10);
        saveButton.setBounds(bottomRow.removeFromRight(80));
    }

    static void showDialog(bool enabled, uint32_t keyCode, uint32_t modifiers, const juce::String& text, SaveCallback onSave) {
        auto* dialogComp = new GlobalShortcutDialog(enabled, keyCode, modifiers, text, std::move(onSave));

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "Global Menu Shortcut";
        options.content.setOwned(dialogComp);
        options.dialogBackgroundColour = juce::Colour(0xff2d2d2d);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;

        options.launchAsync();
    }

private:
    void updateShortcutDisplay() {
        if (!isEnabled) {
            shortcutBox.setText("(Disabled)", juce::dontSendNotification);
            shortcutBox.setColour(juce::Label::outlineColourId, juce::Colour(0xff555555));
        } else {
            shortcutBox.setText(currentText.isNotEmpty() ? currentText : GlobalShortcutManager::defaultShortcutText(),
                                juce::dontSendNotification);
            shortcutBox.setColour(juce::Label::outlineColourId, juce::Colour(0xff444444));
        }
    }

    void closeDialog() {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    }

    bool isEnabled = true;
    uint32_t currentKeyCode = GlobalShortcutManager::defaultCarbonKeyCode;
    uint32_t currentModifiers = GlobalShortcutManager::defaultCarbonModifiers;
    juce::String currentText = GlobalShortcutManager::defaultShortcutText();
    bool isRecording = false;
    SaveCallback onSave;

    juce::Label titleLabel;
    juce::Label descLabel;
    juce::Label shortcutBox;
    juce::Label hintLabel;
    juce::TextButton recordButton;
    juce::TextButton resetButton;
    juce::TextButton disableButton;
    juce::TextButton saveButton;
    juce::TextButton cancelButton;
};
