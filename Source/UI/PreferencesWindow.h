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
#include "MenuBarPanel.h"
#include "CurveFolders.h"
#include "ProgramNumbersComponent.h"

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
        std::function<void()> changeWindowsShortcut;          // the same, for showing and hiding plug-in windows
        std::function<juce::String()> describeWindowsShortcut;
    };

    PreferencesComponent (MainHostWindow& window, GitHubUpdater& updater, Callbacks callbacksIn)
        : callbacks (std::move (callbacksIn)), general (window, updater), keyboard (callbacks), midi (window)
    {
        const auto background = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
        tabs.addTab ("General", background, &general, false);
        tabs.addTab ("Menu Bar", background, &menuBar, false);
        tabs.addTab ("Files", background, &files, false);
        tabs.addTab ("Keyboard", background, &keyboard, false);
        tabs.addTab ("MIDI", background, &midi, false);
        tabs.setTabBarDepth (30);
        tabs.setOutline (0);
        addAndMakeVisible (tabs);

        refresh();
        startTimer (500);
        setSize (560, 450);
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
        GeneralPage (MainHostWindow& w, GitHubUpdater& updater) : window (w), updaterToken (updater.getLifetimeToken())
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

            dockLabel.setText ("Dock Icon:", juce::dontSendNotification);
            dockMode.addItem ("Never", 1);
            dockMode.addItem ("While the Editor Is Open", 2);
            dockMode.addItem ("Always", 3);
            dockMode.setTooltip ("While the editor is open, a Dock icon and a place in Cmd-Tab make it easy to find");
            dockMode.onChange = [this]
            {
                DockIconManager::setMode ((DockIconManager::Mode) (dockMode.getSelectedId() - 1));
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

            pluginWindowsOnAllSpaces.setButtonText ("Show plug-in windows on all Spaces");
            pluginWindowsOnAllSpaces.onClick = [this]
            {
                const bool newState = pluginWindowsOnAllSpaces.getToggleState();

                if (window.graphHolder != nullptr && window.graphHolder->graph != nullptr)
                    window.graphHolder->graph->setPluginWindowsOnAllSpaces (newState);
                else
                    PluginWindow::setShownOnAllSpaces (newState);
            };

            hideTitleBars.setButtonText ("Hide their title bars");
            hideTitleBars.onClick = [this]
            {
                const bool newState = hideTitleBars.getToggleState();

                if (window.graphHolder != nullptr && window.graphHolder->graph != nullptr)
                    window.graphHolder->graph->setPluginWindowTitleBarsHiddenWhenLocked (newState);
                else
                    PluginWindow::setTitleBarsHiddenWhenLocked (newState);
            };

            showBackdrop.setButtonText ("Show a backdrop behind them");
            showBackdrop.onClick = [this]
            {
                const bool newState = showBackdrop.getToggleState();

                if (window.graphHolder != nullptr && window.graphHolder->graph != nullptr)
                    window.graphHolder->graph->setPluginWindowBackdropShownWhenLocked (newState);
                else
                    PluginWindow::setBackdropShownWhenLocked (newState);
            };

            // Locking is per scene, from the editor and the menus; this is how it looks
            lockedLabel.setText ("When a scene's plug-in windows are locked:", juce::dontSendNotification);

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

            for (auto* c : std::initializer_list<juce::Component*> { &openAtLogin, &dockLabel, &dockMode, &preventSleep, &releaseWhenIdle,
                                                                    &pluginWindowsOnAllSpaces, &lockedLabel, &hideTitleBars, &showBackdrop, &updatesLabel, &autoCheck, &checkNow })
                addAndMakeVisible (c);
        }

        void refresh()
        {
            openAtLogin.setToggleState (LoginItemManager::getStatus() == LoginItemManager::Status::enabled, juce::dontSendNotification);
            dockMode.setSelectedId ((int) DockIconManager::getMode() + 1, juce::dontSendNotification);
            releaseWhenIdle.setToggleState (IdleReleaseController::isEnabledInSettings(), juce::dontSendNotification);
            pluginWindowsOnAllSpaces.setToggleState (PluginWindow::isShownOnAllSpaces(), juce::dontSendNotification);
            hideTitleBars.setToggleState (PluginWindow::areTitleBarsHiddenWhenLocked(), juce::dontSendNotification);
            showBackdrop.setToggleState (PluginWindow::isBackdropShownWhenLocked(), juce::dontSendNotification);

            if (auto* settings = getAppProperties().getUserSettings())
            {
                preventSleep.setToggleState (settings->getBoolValue ("preventSystemSleep", false), juce::dontSendNotification);
                autoCheck.setToggleState (settings->getBoolValue ("automaticUpdateChecks", true), juce::dontSendNotification);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (margin, 12);

            openAtLogin.setBounds (r.removeFromTop (rowHeight));

            {
                auto row = r.removeFromTop (rowHeight);
                dockLabel.setBounds (row.removeFromLeft (80));
                dockMode.setBounds (row.removeFromLeft (220).reduced (0, 3));
            }

            for (auto* t : { &preventSleep, &releaseWhenIdle, &pluginWindowsOnAllSpaces })
                t->setBounds (r.removeFromTop (rowHeight));

            lockedLabel.setBounds (r.removeFromTop (rowHeight));

            // Under the lock, which it goes with
            hideTitleBars.setBounds (r.removeFromTop (rowHeight).withTrimmedLeft (24));
            showBackdrop.setBounds (r.removeFromTop (rowHeight).withTrimmedLeft (24));

            r.removeFromTop (12);
            updatesLabel.setBounds (r.removeFromTop (rowHeight));
            autoCheck.setBounds (r.removeFromTop (rowHeight));
            r.removeFromTop (4);
            checkNow.changeWidthToFitText (26);
            checkNow.setTopLeftPosition (r.getX(), r.getY());
        }

        MainHostWindow& window;
        std::shared_ptr<std::atomic<GitHubUpdater*>> updaterToken;
        juce::Label dockLabel;
        juce::ComboBox dockMode;
        juce::ToggleButton openAtLogin, preventSleep, releaseWhenIdle, pluginWindowsOnAllSpaces, hideTitleBars, showBackdrop, autoCheck;
        juce::Label lockedLabel;
        juce::Label updatesLabel;
        juce::TextButton checkNow;
    };

    //==============================================================================
    // The folders the recorder saves to, and scores and backing tracks are opened from first
    struct FilesPage final : public juce::Component
    {
        FilesPage()
        {
            addRow (CurveFolders::Kind::recordings, "Recordings:");
            addRow (CurveFolders::Kind::media, "Scores and Backing Tracks:");

            styleNote (note);
            note.setText ("The Audio Recorder saves here, and the Score Viewer opens files from here first.",
                          juce::dontSendNotification);
            addAndMakeVisible (note);
        }

        void refresh()
        {
            for (auto* row : rows)
                row->path.setText (CurveFolders::get (row->kind).getFullPathName(), juce::dontSendNotification);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (margin, 12);

            for (auto* row : rows)
            {
                auto area = r.removeFromTop (rowHeight);
                row->label.setBounds (area.removeFromLeft (labelWidth));
                row->choose.setBounds (area.removeFromRight (90).reduced (0, 3));
                area.removeFromRight (6);
                row->path.setBounds (area);
            }

            r.removeFromTop (12);
            note.setBounds (r.removeFromTop (40));
        }

    private:
        struct Row
        {
            CurveFolders::Kind kind;
            juce::Label label, path;
            juce::TextButton choose;
        };

        void addRow (CurveFolders::Kind kind, const juce::String& name)
        {
            auto* row = rows.add (new Row());
            row->kind = kind;
            row->label.setText (name, juce::dontSendNotification);
            row->path.setMinimumHorizontalScale (1.0f);
            row->path.setColour (juce::Label::textColourId, Curve::Colours::dimText);
            row->choose.setButtonText ("Choose...");
            row->choose.onClick = [this, kind]
            {
                chooser = std::make_unique<juce::FileChooser> ("Choose a Folder", CurveFolders::get (kind));
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                      [this, kind] (const juce::FileChooser& fc)
                                      {
                                          if (fc.getResult().isDirectory())
                                              CurveFolders::set (kind, fc.getResult());

                                          refresh();
                                      });
            };

            for (auto* c : std::initializer_list<juce::Component*> { &row->label, &row->path, &row->choose })
                addAndMakeVisible (c);
        }

        juce::OwnedArray<Row> rows;
        juce::Label note;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    //==============================================================================
    // What clicking Curve's menu bar icon opens, and what its panel shows
    struct MenuBarPage final : public juce::Component
    {
        MenuBarPage()
        {
            opensLabel.setText ("Clicking the Icon Opens:", juce::dontSendNotification);
            opens.addItem ("The Panel", 1);
            opens.addItem ("A Menu", 2);
            opens.onChange = [this]
            {
                MenuBarPanel::setSetting (MenuBarPanel::useMenuSetting, opens.getSelectedId() == 2);
                refresh();
            };

            showsLabel.setText ("The Panel Shows:", juce::dontSendNotification);

            showPlugins.setButtonText ("Plug-ins, to bypass them and open their windows");
            showPlugins.onClick = [this] { MenuBarPanel::setSetting (MenuBarPanel::showPluginsSetting, showPlugins.getToggleState()); };

            showVolume.setButtonText ("Output volume: the interface's, or Curve's if it has none");
            showVolume.onClick = [this] { MenuBarPanel::setSetting (MenuBarPanel::showVolumeSetting, showVolume.getToggleState()); };

            showOutput.setButtonText ("Output switches: Mono, Dim and Mute");
            showOutput.onClick = [this] { MenuBarPanel::setSetting (MenuBarPanel::showOutputSetting, showOutput.getToggleState()); };

            solidBackground.setButtonText ("Solid background, rather than macOS's glass");
            solidBackground.onClick = [this] { MenuBarPanel::setSetting (MenuBarPanel::solidBackgroundSetting, solidBackground.getToggleState()); };

            showMeter.setButtonText ("Output level meter");
            showMeter.onClick = [this] { MenuBarPanel::setSetting (MenuBarPanel::showMeterSetting, showMeter.getToggleState()); };

            showTempo.setButtonText ("Tempo, with Tap and Play/Stop, for tempo-synced plug-ins");
            showTempo.onClick = [this] { MenuBarPanel::setSetting (MenuBarPanel::showTempoSetting, showTempo.getToggleState()); };

            styleNote (note);
            note.setText ("The panel always shows the presets. Right-click or Control-click the icon for the menu.",
                          juce::dontSendNotification);

            for (auto* c : std::initializer_list<juce::Component*> { &opensLabel, &opens, &showsLabel, &showPlugins, &showOutput, &showVolume, &showMeter, &showTempo, &solidBackground, &note })
                addAndMakeVisible (c);
        }

        void refresh()
        {
            const bool usesMenu = MenuBarPanel::getSetting (MenuBarPanel::useMenuSetting, false);
            opens.setSelectedId (usesMenu ? 2 : 1, juce::dontSendNotification);
            showPlugins.setToggleState (MenuBarPanel::getSetting (MenuBarPanel::showPluginsSetting, true), juce::dontSendNotification);
            showVolume.setToggleState (MenuBarPanel::getSetting (MenuBarPanel::showVolumeSetting, true), juce::dontSendNotification);
            showMeter.setToggleState (MenuBarPanel::getSetting (MenuBarPanel::showMeterSetting, true), juce::dontSendNotification);
            showTempo.setToggleState (MenuBarPanel::getSetting (MenuBarPanel::showTempoSetting, false), juce::dontSendNotification);
            showTempo.setEnabled (! usesMenu);
            showOutput.setToggleState (MenuBarPanel::getSetting (MenuBarPanel::showOutputSetting, true), juce::dontSendNotification);
            showOutput.setEnabled (! usesMenu);
            solidBackground.setToggleState (MenuBarPanel::getSetting (MenuBarPanel::solidBackgroundSetting, false), juce::dontSendNotification);
            solidBackground.setEnabled (! usesMenu);
            showPlugins.setEnabled (! usesMenu);
            showVolume.setEnabled (! usesMenu);
            showMeter.setEnabled (! usesMenu);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (margin, 12);

            auto row = r.removeFromTop (rowHeight);
            opensLabel.setBounds (row.removeFromLeft (labelWidth));
            opens.setBounds (row.removeFromLeft (160).reduced (0, 3));

            r.removeFromTop (8);
            row = r.removeFromTop (rowHeight);
            showsLabel.setBounds (row.removeFromLeft (labelWidth));
            showPlugins.setBounds (row);

            row = r.removeFromTop (rowHeight);
            row.removeFromLeft (labelWidth);
            showOutput.setBounds (row);

            row = r.removeFromTop (rowHeight);
            row.removeFromLeft (labelWidth);
            showVolume.setBounds (row);

            row = r.removeFromTop (rowHeight);
            row.removeFromLeft (labelWidth);
            showMeter.setBounds (row);

            row = r.removeFromTop (rowHeight);
            row.removeFromLeft (labelWidth);
            showTempo.setBounds (row);

            r.removeFromTop (8);
            row = r.removeFromTop (rowHeight);
            row.removeFromLeft (labelWidth);
            solidBackground.setBounds (row);

            r.removeFromTop (12);
            note.setBounds (r.removeFromTop (40));
        }

        juce::Label opensLabel, showsLabel, note;
        juce::ComboBox opens;
        juce::ToggleButton showPlugins, showOutput, showVolume, showMeter, showTempo, solidBackground;
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

            windowsShortcutLabel.setText ("Plug-in Windows Shortcut:", juce::dontSendNotification);
            changeWindowsShortcut.setButtonText ("Change...");
            changeWindowsShortcut.onClick = [this] { if (callbacks.changeWindowsShortcut) callbacks.changeWindowsShortcut(); };
            addAndMakeVisible (windowsShortcutLabel);
            addAndMakeVisible (windowsShortcutText);
            addAndMakeVisible (changeWindowsShortcut);

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
            windowsShortcutText.setText (callbacks.describeWindowsShortcut ? callbacks.describeWindowsShortcut() : juce::String(),
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

            row = r.removeFromTop (rowHeight);
            windowsShortcutLabel.setBounds (row.removeFromLeft (labelWidth));
            changeWindowsShortcut.setBounds (row.removeFromRight (90).reduced (0, 3));
            windowsShortcutText.setBounds (row);
        }

        const Callbacks& callbacks;
        juce::Label volumeKeysLabel, volumeStatus, shortcutLabel, shortcutText;
        juce::Label windowsShortcutLabel, windowsShortcutText;
        juce::ComboBox volumeKeysMode, volumeKeysTarget;
        juce::ToggleButton overlay;
        juce::TextButton allowAccessibility, changeShortcut, changeWindowsShortcut;
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

            programNumbers.setButtonText ("Program Numbers...");
            programNumbers.setTooltip ("Which program change selects each preset, and giving a preset a number of its own");
            programNumbers.onClick = []
            {
                juce::DialogWindow::LaunchOptions options;
                options.content.setOwned (new ProgramNumbersComponent());
                options.dialogTitle = "Program Numbers";
                options.dialogBackgroundColour = Curve::Colours::canvas;
                options.useNativeTitleBar = true;
                options.resizable = true;
                options.launchAsync();
            };
            addAndMakeVisible (programNumbers);

            switchingHeader.setText ("Changing Presets", juce::dontSendNotification);
            switchingHeader.setFont (juce::FontOptions (14.0f, juce::Font::bold));

            for (const bool next : { true, false })
            {
                auto& label = next ? nextLabel : previousLabel;
                label.setText (next ? "Next Preset Control:" : "Previous Preset Control:", juce::dontSendNotification);

                auto& learn = next ? learnNext : learnPrevious;
                learn.setButtonText ("MIDI Learn...");
                learn.onClick = [this, next]
                {
                    learnControl (MidiMappingTarget::forPresetStep (next), next ? "Next Preset Control" : "Previous Preset Control");
                };
            }

            clear.setButtonText ("Clear Preset Controls");
            clear.onClick = [this]
            {
                if (auto* engine = getEngine())
                {
                    engine->clearGlobalMapping (MidiMappingTarget::forPresetStep (true));
                    engine->clearGlobalMapping (MidiMappingTarget::forPresetStep (false));
                }
                refresh();
            };

            // App-wide controls, each with its own row
            controlsHeader.setText ("Plug-in Windows and Output", juce::dontSendNotification);
            controlsHeader.setFont (juce::FontOptions (14.0f, juce::Font::bold));

            using K = MidiMappingTarget::Kind;
            addRow (K::pluginWindows, "Show/Hide Windows:", "Show/Hide Plug-in Windows Control");
            addRow (K::outputMono,    "Mono:",              "Mono Control");
            addRow (K::outputDim,     "Dim:",               "Dim Control");
            addRow (K::outputMute,    "Mute Output:",       "Mute Output Control");

            note.setText ("Controllers must be enabled under MIDI Inputs in Audio Settings.", juce::dontSendNotification);
            styleNote (note);

            for (auto* c : std::initializer_list<juce::Component*> { &switchingHeader, &programChange, &nextLabel, &nextControl, &learnNext,
                                                                    &previousLabel, &previousControl, &learnPrevious, &clear,
                                                                    &controlsHeader, &note })
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

            const auto& global = engine != nullptr ? engine->getGlobalMappings() : MidiMappingSet();
            const auto describe = [&global] (const MidiMappingTarget& t)
            {
                const auto* m = global.findFor (t);
                return m != nullptr ? m->trigger.describe() : juce::String ("None");
            };

            nextControl.setText (describe (MidiMappingTarget::forPresetStep (true)), juce::dontSendNotification);
            previousControl.setText (describe (MidiMappingTarget::forPresetStep (false)), juce::dontSendNotification);
            clear.setEnabled (global.findFor (MidiMappingTarget::forPresetStep (true)) != nullptr
                           || global.findFor (MidiMappingTarget::forPresetStep (false)) != nullptr);

            for (auto* row : rows)
            {
                row->control.setText (describe (row->target), juce::dontSendNotification);
                row->clear.setEnabled (global.findFor (row->target) != nullptr);
            }
        }

        // Waits for the next MIDI control for an app-wide control, with a window the user can cancel
        void learnControl (const MidiMappingTarget& target, const juce::String& title)
        {
            auto* engine = getEngine();
            if (engine == nullptr)
                return;

            auto* alert = new juce::AlertWindow (title,
                                                 "Press a button or pad on your MIDI controller.\n\n"
                                                 "Controllers must be enabled under MIDI Inputs in Audio Settings.",
                                                 juce::MessageBoxIconType::NoIcon);
            alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

            juce::Component::SafePointer<juce::AlertWindow> safeAlert (alert);
            juce::Component::SafePointer<MidiPage> safeThis (this);

            engine->learnGlobalControl (target, [safeAlert] (const MidiTrigger&)
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
            {
                auto row = r.removeFromTop (rowHeight);
                programNumbers.setBounds (row.removeFromRight (150).reduced (0, 3));
                programChange.setBounds (row);
            }

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
            r.removeFromTop (rowHeight + 8);

            controlsHeader.setBounds (r.removeFromTop (24));

            for (auto* row : rows)
            {
                auto area = r.removeFromTop (rowHeight);
                row->label.setBounds (area.removeFromLeft (labelWidth));
                row->clear.setBounds (area.removeFromRight (60).reduced (0, 3));
                area.removeFromRight (6);
                row->learn.setBounds (area.removeFromRight (110).reduced (0, 3));
                row->control.setBounds (area);
            }

            r.removeFromTop (8);
            note.setBounds (r.removeFromTop (rowHeight));
        }

        // An app-wide control: what it's mapped to, and buttons to learn and clear it
        struct ControlRow
        {
            MidiMappingTarget target;
            juce::Label label, control;
            juce::TextButton learn, clear;
        };

        void addRow (MidiMappingTarget::Kind kind, const juce::String& name, const juce::String& title)
        {
            auto* row = rows.add (new ControlRow());
            row->target = MidiMappingTarget::forGlobal (kind);
            row->label.setText (name, juce::dontSendNotification);
            row->learn.setButtonText ("MIDI Learn...");
            row->learn.onClick = [this, target = row->target, title] { learnControl (target, title); };
            row->clear.setButtonText ("Clear");
            row->clear.onClick = [this, target = row->target]
            {
                if (auto* engine = getEngine())
                    engine->clearGlobalMapping (target);
                refresh();
            };

            for (auto* c : std::initializer_list<juce::Component*> { &row->label, &row->control, &row->learn, &row->clear })
                addAndMakeVisible (c);
        }

        juce::Component::SafePointer<MainHostWindow> window;
        juce::ToggleButton programChange;
        juce::TextButton programNumbers;
        juce::TextButton learnNext, learnPrevious, clear;
        juce::Label switchingHeader, nextLabel, nextControl, previousLabel, previousControl, controlsHeader, note;
        juce::OwnedArray<ControlRow> rows;
    };

    //==============================================================================
    void refresh()
    {
        general.refresh();
        menuBar.refresh();
        files.refresh();
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
    MenuBarPage menuBar;
    FilesPage files;
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
