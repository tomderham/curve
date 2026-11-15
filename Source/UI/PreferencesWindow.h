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
#include "GitHubUpdater.h"
#include "GlobalShortcutManager.h"
#include "GraphEditorPanel.h"
#include "DockIconManager.h"
#include "LoginItemManager.h"
#include "MainHostWindow.h"
#include "SystemSleepManager.h"
#include "IdleReleaseController.h"
#include "VolumeKeyManager.h"

inline bool attemptToEnableLoginItem()
{
    juce::String errorMessage;
    LoginItemManager::setEnabled (true, errorMessage);

    const bool enabled = LoginItemManager::getStatus() == LoginItemManager::Status::enabled;

    if (enabled)
    {
        if (auto* settings = getAppProperties().getUserSettings())
        {
            settings->setValue ("openAtLogin", true);
            settings->saveIfNeeded();
        }
    }
    else
    {
        LoginItemManager::openSystemSettingsLoginItemsPane();
    }

    return enabled;
}

//==============================================================================
/** The Preferences window, from Settings > Preferences...: General, Keyboard and MIDI tabs.
    Every control reads its value from where it's stored, and a timer keeps them up to date,
    so changes made elsewhere (e.g. MIDI Learn, or System Settings' Login Items) show here. */
class PreferencesComponent final : public juce::Component,
                                   private juce::Timer
{
public:
    struct Callbacks
    {
        std::function<void()> changeGlobalShortcut;          // opens the shortcut dialog
        std::function<juce::String()> describeGlobalShortcut; // e.g. "⌥⌘C", or "(Disabled)"
    };

    PreferencesComponent (MainHostWindow& window, GitHubUpdater& updater, Callbacks callbacksIn)
        : callbacks (std::move (callbacksIn)), general (updater), keyboard (callbacks), midi (window)
    {
        const auto background = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
        tabs.addTab ("General", background, &general, false);
        tabs.addTab ("Keyboard", background, &keyboard, false);
        tabs.addTab ("MIDI", background, &midi, false);
        tabs.setTabBarDepth (30);
        tabs.setOutline (0);
        addAndMakeVisible (tabs);

        refresh();
        startTimer (500);
        setSize (560, 380);
    }

    void resized() override { tabs.setBounds (getLocalBounds().reduced (8)); }

private:
    //==============================================================================
    // A row with a label on the left and a control (or two) on the right
    static constexpr int rowHeight = 30, labelWidth = 170, margin = 16;

    static void styleNote (juce::Label& label)
    {
        label.setFont (juce::FontOptions (13.0f));
        label.setJustificationType (juce::Justification::topLeft);
        label.setMinimumHorizontalScale (1.0f);
        label.setColour (juce::Label::textColourId,
                         label.getLookAndFeel().findColour (juce::Label::textColourId).withAlpha (0.6f));
    }

    //==============================================================================
    struct GeneralPage final : public juce::Component
    {
        explicit GeneralPage (GitHubUpdater& updater) : updaterToken (updater.getLifetimeToken())
        {
            openAtLogin.setButtonText ("Open at Login");
            openAtLogin.onClick = [this]
            {
                if (openAtLogin.getToggleState())
                {
                    if (attemptToEnableLoginItem())
                        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Open at Login",
                                                                     "Curve will now open automatically at login.");
                }
                else
                {
                    juce::String errorMessage;
                    if (LoginItemManager::setEnabled (false, errorMessage))
                    {
                        if (auto* settings = getAppProperties().getUserSettings())
                        {
                            settings->setValue ("openAtLogin", false);
                            settings->saveIfNeeded();
                        }
                    }
                    else
                    {
                        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Open at Login",
                                                                     "Could not remove Curve from Login Items:\n" + errorMessage);
                    }
                }

                refresh();
            };

            showInDock.setButtonText ("Show in Dock");
            showInDock.onClick = [this]
            {
                const bool newState = showInDock.getToggleState();
                if (auto* settings = getAppProperties().getUserSettings())
                {
                    settings->setValue ("showInDock", newState);
                    settings->saveIfNeeded();
                }
                DockIconManager::setShowInDock (newState);
            };

            preventSleep.setButtonText ("Prevent macOS from automatically sleeping");
            preventSleep.onClick = [this]
            {
                const bool newState = preventSleep.getToggleState();
                if (auto* settings = getAppProperties().getUserSettings())
                {
                    settings->setValue ("preventSystemSleep", newState);
                    settings->saveIfNeeded();
                }
                SystemSleepManager::setPreventSystemSleep (newState);
            };

            releaseWhenIdle.setButtonText ("Release audio interface when idle");
            releaseWhenIdle.onClick = [this] { IdleReleaseController::setEnabledInSettings (releaseWhenIdle.getToggleState()); };

            updatesLabel.setText ("Updates:", juce::dontSendNotification);

            autoCheck.setButtonText ("Auto-Check for App Updates");
            autoCheck.onClick = [this]
            {
                const bool newState = autoCheck.getToggleState();
                if (auto* settings = getAppProperties().getUserSettings())
                {
                    settings->setValue ("automaticUpdateChecks", newState);
                    settings->saveIfNeeded();
                }
                if (newState && updaterToken != nullptr)
                    if (auto* u = updaterToken->load())
                        u->doCheckNow();
            };

            checkNow.setButtonText ("Check for Updates...");
            checkNow.onClick = [this]
            {
                if (updaterToken != nullptr)
                    if (auto* u = updaterToken->load())
                        u->checkForUpdates (true);
            };

            for (auto* c : std::initializer_list<juce::Component*> { &openAtLogin, &showInDock, &preventSleep, &releaseWhenIdle,
                                                                    &updatesLabel, &autoCheck, &checkNow })
                addAndMakeVisible (c);
        }

        void refresh()
        {
            openAtLogin.setToggleState (LoginItemManager::getStatus() == LoginItemManager::Status::enabled, juce::dontSendNotification);
            showInDock.setToggleState (DockIconManager::isShowInDockEnabled(), juce::dontSendNotification);
            releaseWhenIdle.setToggleState (IdleReleaseController::isEnabledInSettings(), juce::dontSendNotification);

            if (auto* settings = getAppProperties().getUserSettings())
            {
                preventSleep.setToggleState (settings->getBoolValue ("preventSystemSleep", false), juce::dontSendNotification);
                autoCheck.setToggleState (settings->getBoolValue ("automaticUpdateChecks", true), juce::dontSendNotification);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (margin, 12);

            for (auto* t : { &openAtLogin, &showInDock, &preventSleep, &releaseWhenIdle })
                t->setBounds (r.removeFromTop (rowHeight));

            r.removeFromTop (12);
            updatesLabel.setBounds (r.removeFromTop (rowHeight));
            autoCheck.setBounds (r.removeFromTop (rowHeight));
            r.removeFromTop (4);
            checkNow.changeWidthToFitText (26);
            checkNow.setTopLeftPosition (r.getX(), r.getY());
        }

        std::shared_ptr<std::atomic<GitHubUpdater*>> updaterToken;
        juce::ToggleButton openAtLogin, showInDock, preventSleep, releaseWhenIdle, autoCheck;
        juce::Label updatesLabel;
        juce::TextButton checkNow;
    };

    //==============================================================================
    struct KeyboardPage final : public juce::Component
    {
        explicit KeyboardPage (const Callbacks& c) : callbacks (c)
        {
            volumeKeysLabel.setText ("Volume Keys:", juce::dontSendNotification);
            volumeKeysMode.addItem ("Off", 1 + (int) VolumeKeyManager::Mode::off);
            volumeKeysMode.addItem ("When macOS Can't Control the Interface's Volume", 1 + (int) VolumeKeyManager::Mode::automatic);
            volumeKeysMode.addItem ("Always", 1 + (int) VolumeKeyManager::Mode::always);
            volumeKeysMode.onChange = [this]
            {
                VolumeKeyManager::setMode ((VolumeKeyManager::Mode) (volumeKeysMode.getSelectedId() - 1));
                refresh();
            };

            volumeKeysTarget.addItem ("Control Curve's Output Volume", 1 + (int) VolumeKeyManager::Target::output);
            volumeKeysTarget.addItem ("Control Interface Loopback Volume", 1 + (int) VolumeKeyManager::Target::loopback);
            volumeKeysTarget.onChange = [this]
            {
                VolumeKeyManager::setTarget ((VolumeKeyManager::Target) (volumeKeysTarget.getSelectedId() - 1));
                refresh();
            };

            overlay.setButtonText ("Show Volume Overlay");
            overlay.onClick = [this] { VolumeKeyManager::setOverlayEnabled (overlay.getToggleState()); };

            styleNote (volumeStatus);

            allowAccessibility.setButtonText ("Allow Accessibility Access...");
            allowAccessibility.onClick = [] { VolumeKeyManager::openAccessibilitySettings(); };

            shortcutLabel.setText ("Global Menu Shortcut:", juce::dontSendNotification);
            changeShortcut.setButtonText ("Change...");
            changeShortcut.onClick = [this] { if (callbacks.changeGlobalShortcut) callbacks.changeGlobalShortcut(); };

            for (auto* comp : std::initializer_list<juce::Component*> { &volumeKeysLabel, &volumeKeysMode, &volumeKeysTarget,
                                                                       &overlay, &shortcutLabel, &shortcutText, &changeShortcut })
                addAndMakeVisible (comp);

            addChildComponent (volumeStatus);
            addChildComponent (allowAccessibility);
        }

        void refresh()
        {
            const auto mode = VolumeKeyManager::getMode();
            volumeKeysMode.setSelectedId (1 + (int) mode, juce::dontSendNotification);
            volumeKeysTarget.setSelectedId (1 + (int) VolumeKeyManager::getTarget(), juce::dontSendNotification);
            overlay.setToggleState (VolumeKeyManager::isOverlayEnabled(), juce::dontSendNotification);

            // As in the menu used to: only while the keys are on
            const bool isOn = mode != VolumeKeyManager::Mode::off;
            const bool needsAccess = isOn && VolumeKeyManager::needsAccessibilityAccess();
            allowAccessibility.setVisible (needsAccess);
            volumeStatus.setVisible (isOn && ! needsAccess);
            volumeStatus.setText (describeVolumeStatus(), juce::dontSendNotification);

            shortcutText.setText (callbacks.describeGlobalShortcut ? callbacks.describeGlobalShortcut() : juce::String(),
                                  juce::dontSendNotification);
        }

        static juce::String describeVolumeStatus()
        {
            const bool loopback = VolumeKeyManager::getTarget() == VolumeKeyManager::Target::loopback;

            if (VolumeKeyManager::isControllingVolume())
                return (loopback ? "Interface Loopback Volume: " : "Output Volume: ") + VolumeKeyManager::describeVolume();

            if (loopback && ! VolumeKeyManager::hasConnectedLoopback())
                return "No Interface Loopback in Preset";

            return "Interface Uses macOS Volume";
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (margin, 12);

            auto row = r.removeFromTop (rowHeight);
            volumeKeysLabel.setBounds (row.removeFromLeft (labelWidth));
            volumeKeysMode.setBounds (row.reduced (0, 3));

            // What the keys control, under the mode
            row = r.removeFromTop (rowHeight);
            row.removeFromLeft (labelWidth);
            volumeKeysTarget.setBounds (row.reduced (0, 3));

            row = r.removeFromTop (rowHeight);
            row.removeFromLeft (labelWidth);
            overlay.setBounds (row);

            row = r.removeFromTop (rowHeight);
            row.removeFromLeft (labelWidth);
            volumeStatus.setBounds (row);
            allowAccessibility.changeWidthToFitText (24);
            allowAccessibility.setTopLeftPosition (row.getX(), row.getY() + 3);

            r.removeFromTop (16);
            row = r.removeFromTop (rowHeight);
            shortcutLabel.setBounds (row.removeFromLeft (labelWidth));
            changeShortcut.setBounds (row.removeFromRight (90).reduced (0, 3));
            shortcutText.setBounds (row);
        }

        const Callbacks& callbacks;
        juce::Label volumeKeysLabel, volumeStatus, shortcutLabel, shortcutText;
        juce::ComboBox volumeKeysMode, volumeKeysTarget;
        juce::ToggleButton overlay;
        juce::TextButton allowAccessibility, changeShortcut;
    };

    //==============================================================================
    struct MidiPage final : public juce::Component
    {
        explicit MidiPage (MainHostWindow& w) : window (&w)
        {
            programChange.setButtonText ("Program Change Selects Preset");
            programChange.onClick = [this]
            {
                if (auto* engine = getEngine())
                    engine->setProgramChangePresetSelectEnabled (programChange.getToggleState());
            };

            switchingHeader.setText ("Changing Presets", juce::dontSendNotification);
            currentHeader.setText ("Current Preset", juce::dontSendNotification);
            for (auto* header : { &switchingHeader, &currentHeader })
                header->setFont (juce::FontOptions (14.0f, juce::Font::bold));

            mappings.setButtonText ("MIDI Mappings for Current Preset...");
            mappings.setTooltip ("The MIDI controls saved in the loaded preset, and each one's range");
            mappings.onClick = [this]
            {
                if (auto* w = window.getComponent())
                    w->showMidiMappingsWindow();
            };

            for (const bool next : { true, false })
            {
                auto& label = next ? nextLabel : previousLabel;
                label.setText (next ? "Next Preset Control:" : "Previous Preset Control:", juce::dontSendNotification);

                auto& learn = next ? learnNext : learnPrevious;
                learn.setButtonText ("MIDI Learn...");
                learn.onClick = [this, next] { learnPresetStep (next); };
            }

            clear.setButtonText ("Clear Preset Controls");
            clear.onClick = [this]
            {
                if (auto* engine = getEngine())
                    engine->clearGlobalMappings();
                refresh();
            };

            note.setText ("Controllers must be enabled under MIDI Inputs in Audio Settings.", juce::dontSendNotification);
            styleNote (note);

            for (auto* c : std::initializer_list<juce::Component*> { &switchingHeader, &programChange, &nextLabel, &nextControl, &learnNext,
                                                                    &previousLabel, &previousControl, &learnPrevious, &clear,
                                                                    &currentHeader, &mappings, &note })
                addAndMakeVisible (c);
        }

        MidiControlEngine* getEngine() const
        {
            auto* w = window.getComponent();
            return w != nullptr && w->graphHolder != nullptr ? w->graphHolder->midiEngine.get() : nullptr;
        }

        void refresh()
        {
            auto* engine = getEngine();
            setEnabled (engine != nullptr);

            programChange.setToggleState (MidiControlEngine::isProgramChangePresetSelectEnabled(), juce::dontSendNotification);

            for (const bool next : { true, false })
            {
                juce::String text = "None";
                if (engine != nullptr)
                    if (auto* mapping = engine->getGlobalMappings().findFor (MidiMappingTarget::forPresetStep (next)))
                        text = mapping->trigger.describe();

                (next ? nextControl : previousControl).setText (text, juce::dontSendNotification);
            }

            clear.setEnabled (engine != nullptr && ! engine->getGlobalMappings().isEmpty());
        }

        // Waits for the next MIDI control, with a window the user can cancel
        void learnPresetStep (bool next)
        {
            auto* engine = getEngine();
            if (engine == nullptr)
                return;

            auto* alert = new juce::AlertWindow (next ? "Next Preset Control" : "Previous Preset Control",
                                                 "Press a button or move a control on your MIDI controller.\n\n"
                                                 "Controllers must be enabled under MIDI Inputs in Audio Settings.",
                                                 juce::MessageBoxIconType::NoIcon);
            alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

            juce::Component::SafePointer<juce::AlertWindow> safeAlert (alert);
            juce::Component::SafePointer<MidiPage> safeThis (this);

            engine->learnPresetStep (next, [safeAlert] (const MidiTrigger&)
            {
                if (auto* a = safeAlert.getComponent())
                    a->exitModalState (1);
            });

            alert->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis] (int result)
            {
                if (safeThis == nullptr)
                    return;

                if (result == 0)
                    if (auto* e = safeThis->getEngine())
                        e->cancelPresetStepLearn();

                safeThis->refresh();
            }), true);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (margin, 12);

            // Program Change and the Next/Previous controls are all app-wide ways to change preset
            switchingHeader.setBounds (r.removeFromTop (24));
            programChange.setBounds (r.removeFromTop (rowHeight));

            for (const bool next : { true, false })
            {
                auto row = r.removeFromTop (rowHeight);
                (next ? nextLabel : previousLabel).setBounds (row.removeFromLeft (labelWidth));
                (next ? learnNext : learnPrevious).setBounds (row.removeFromRight (110).reduced (0, 3));
                (next ? nextControl : previousControl).setBounds (row);
            }

            r.removeFromTop (4);
            clear.changeWidthToFitText (26);
            clear.setTopLeftPosition (r.getX(), r.getY());
            r.removeFromTop (rowHeight + 12);

            // MIDI mappings are saved in each preset
            currentHeader.setBounds (r.removeFromTop (24));
            mappings.changeWidthToFitText (26);
            mappings.setTopLeftPosition (r.getX(), r.getY() + 2);
            r.removeFromTop (rowHeight + 12);
            note.setBounds (r.removeFromTop (rowHeight));
        }

        juce::Component::SafePointer<MainHostWindow> window;
        juce::ToggleButton programChange;
        juce::TextButton mappings, learnNext, learnPrevious, clear;
        juce::Label switchingHeader, currentHeader, nextLabel, nextControl, previousLabel, previousControl, note;
    };

    //==============================================================================
    void refresh()
    {
        general.refresh();
        keyboard.refresh();
        midi.refresh();
    }

    void timerCallback() override
    {
        if (isShowing())
            refresh();
    }

    Callbacks callbacks; // before the pages, which refer to it
    GeneralPage general;
    KeyboardPage keyboard;
    MidiPage midi;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreferencesComponent)
};

//==============================================================================
class PreferencesWindow final : public juce::DocumentWindow
{
public:
    PreferencesWindow (MainHostWindow& window, GitHubUpdater& updater, PreferencesComponent::Callbacks callbacks)
        : DocumentWindow ("Preferences",
                          juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
                          DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new PreferencesComponent (window, updater, std::move (callbacks)), true);
        setResizable (false, false);
        centreWithSize (getWidth(), getHeight());
    }

    void closeButtonPressed() override { setVisible (false); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreferencesWindow)
};
