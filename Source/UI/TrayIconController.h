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

#include "GitHubUpdater.h"
#include "GlobalShortcutDialog.h"
#include "GlobalShortcutManager.h"
#include "GraphEditorPanel.h"
#include "DockIconManager.h"
#include "HelpBookManager.h"
#include "LoginItemManager.h"
#include "MainHostWindow.h"
#include "SystemSleepManager.h"
#include "IdleReleaseController.h"
#include "VolumeKeyManager.h"
#include "PreferencesWindow.h"
#include "MenuBarPanel.h"
#include "MenuBarPanelModel.h"

inline std::unique_ptr<InputStream>
createAssetInputStream(const char *resourcePath) {

  auto assetsDir = File::getSpecialLocation(File::currentExecutableFile)
                       .getParentDirectory()
                       .getSiblingFile("Resources")
                       .getChildFile("Assets");

  auto resourceFile = assetsDir.getChildFile(resourcePath);

  if (!resourceFile.existsAsFile())
    return {};

  return resourceFile.createInputStream();
}

inline Image getImageFromAssets(const char *assetName) {
  auto hashCode = (String(assetName) + "@juce_demo_assets").hashCode64();
  auto img = ImageCache::getFromHashCode(hashCode);

  if (img.isNull()) {
    std::unique_ptr<InputStream> juceIconStream(
        createAssetInputStream(assetName));

    if (juceIconStream == nullptr)
      return {};

    img = ImageFileFormat::loadFrom(*juceIconStream);

    ImageCache::addImageToCache(img, hashCode);
  }

  return img;
}

class TrayIconController : public juce::SystemTrayIconComponent,
                           private juce::Timer {
public:
  // Pass a reference to the window so we can control it
  TrayIconController(MainHostWindow &windowToControl,
                     GitHubUpdater &gitHubUpdaterToControl)
      : mainWindow(windowToControl), gitHubUpdater(gitHubUpdaterToControl) {
    refreshIcon();
    knownDisplayIDs = getMacOSActiveDisplayIDs();

    // For testing only: CURVE_DEBUG_RECREATE_TRAY_ICON=<seconds> recreates the status item at that
    // interval, to check it keeps its menu bar position (macOS 26 and earlier lose it by default).
    if (auto seconds = juce::SystemStats::getEnvironmentVariable("CURVE_DEBUG_RECREATE_TRAY_ICON", {}).getIntValue();
        seconds > 0) {
      debugRecreateTimer.callback = [this] { recreateStatusItem(); };
      debugRecreateTimer.startTimer(seconds * 1000);
    }

    displayChangeNotifier = createMacOSDisplayChangeNotifier([this] {
      handleDisplayChange();
    });

    panelModel = std::make_unique<MenuBarPanelModel>(mainWindow);

    juce::Component::SafePointer<TrayIconController> safeSelf(this);
    juce::Component::SafePointer<MainHostWindow> safeWindow(&mainWindow);
    MenuBarPanel::Callbacks callbacks;
    callbacks.showAudioSettings = [safeWindow] { if (auto *w = safeWindow.getComponent()) w->showAudioSettings(); };
    callbacks.showPluginManager = [safeWindow] { if (auto *w = safeWindow.getComponent()) w->showPluginListWindow(); };
    callbacks.showPreferences = [safeSelf] { if (auto *self = safeSelf.getComponent()) self->showPreferences(); };
    callbacks.showHelp = [] { HelpBookManager::showHelp(); };
    callbacks.showAbout = [safeWindow] { if (auto *w = safeWindow.getComponent()) w->showAboutBox(); };
    callbacks.quit = [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); };
    panel = std::make_unique<MenuBarPanel>(*panelModel, std::move(callbacks));

    initGlobalShortcut();
  }

  // The panel, unless the menu was asked for: by right-clicking or Control-clicking the icon, or
  // in Preferences instead of the panel
  static bool opensPanel(bool isPopupMenuClick) {
    return !isPopupMenuClick && !MenuBarPanel::getSetting(MenuBarPanel::useMenuSetting, false);
  }

  void refreshIcon() {
    setIconImage(getImageFromAssets("juce_icon.png"),
                 getImageFromAssets("juce_icon_template.png"));
    setMacOSStatusItemAutosaveName(getNativeHandle());
    setIconTooltip("Curve");
  }

  void recreateStatusItem() {
    if (isMenuOpen || (panel != nullptr && panel->isOpen()))
      return;

    // Explicitly unregister old NSStatusItem from NSStatusBar so it is cleanly removed
    // from all menu bars and does not linger as a greyed-out/zombie placeholder.
    removeMacOSStatusItem (getNativeHandle());

    // Reset JUCE's internal Pimpl holder
    setIconImage({}, {});

    // Allocates a fresh NSStatusItem, which macOS registers across all currently active display menu bars.
    refreshIcon();
  }

  void handleDisplayChange() {
    juce::MessageManager::callAsync([safeSelf = juce::Component::SafePointer<TrayIconController>(this)] {
      if (auto *self = safeSelf.getComponent()) {
        // Debounce: wait 3.5s for display geometry and Thunderbolt link training to stabilize,
        // then recreate the status item ONCE. Any subsequent display events in this window
        // reset the timer, ensuring a single clean update with zero icon flashing.
        self->startTimer(3500);
      }
    });
  }

  void timerCallback() override {
    stopTimer();

    if (isMenuOpen || (panel != nullptr && panel->isOpen())) {
      // If user is currently interacting with the menu or panel, retry shortly after
      startTimer(1500);
      return;
    }

    // Recreate only when a display was added or removed. macOS reports a reconfiguration on
    // every wake too, and a recreated item moves to the far left of the menu bar on macOS 26
    // and earlier. An empty list means the displays are still asleep, so wait for the next event.
    auto displayIDs = getMacOSActiveDisplayIDs();
    if (displayIDs.empty() || displayIDs == knownDisplayIDs)
      return;

    knownDisplayIDs = std::move(displayIDs);
    recreateStatusItem();
  }

  void mouseUp(const juce::MouseEvent &) override {
    // Leave this empty to prevent double-firing
  }

  void toggleMenuFromShortcut() {
    juce::MessageManager::callAsync([safeSelf = juce::Component::SafePointer<TrayIconController>(this)] {
      if (auto *self = safeSelf.getComponent()) {
        if (opensPanel(false) && !self->isMenuOpen) {
          self->panel->toggle(self->getNativeHandle());
          return;
        }

        auto now = juce::Time::getMillisecondCounter();
        if (self->isMenuOpen || (now - self->lastMenuDismissTime < 250)) {
          self->isMenuOpen = false;
          self->lastMenuDismissTime = now;
          if (self->activeMenuKeyMonitor != nullptr) {
            GlobalShortcutManager::removeMenuKeyMonitor(self->activeMenuKeyMonitor);
            self->activeMenuKeyMonitor = nullptr;
          }
          juce::PopupMenu::dismissAllActiveMenus();
          return;
        }

        self->openTrayMenu(true);
      }
    });
  }

  void mouseDown(const juce::MouseEvent &event) override {
    if (event.mouseWasClicked()) {
      if (opensPanel(event.mods.isPopupMenu()) && !isMenuOpen) {
        panel->toggle(getNativeHandle());
        return;
      }

      panel->close();

      auto now = juce::Time::getMillisecondCounter();
      if (isMenuOpen || (now - lastMenuDismissTime < 250)) {
        isMenuOpen = false;
        lastMenuDismissTime = now;
        if (activeMenuKeyMonitor != nullptr) {
          GlobalShortcutManager::removeMenuKeyMonitor(activeMenuKeyMonitor);
          activeMenuKeyMonitor = nullptr;
        }
        juce::PopupMenu::dismissAllActiveMenus();
        return;
      }

      openTrayMenu(false);
    }
  }

  void openTrayMenu(bool fromShortcut) {
    juce::Process::makeForegroundProcess();

    juce::Rectangle<int> targetArea;
    if (fromShortcut) {
      targetArea = GlobalShortcutManager::getStatusItemScreenBounds(getNativeHandle());
      if (targetArea.isEmpty()) {
        auto mousePos = juce::Desktop::getInstance().getMousePosition();
        targetArea = juce::Rectangle<int>(mousePos.x, mousePos.y, 1, 1);
      }
    } else {
      auto mousePos = juce::Desktop::getInstance().getMousePosition();
      targetArea = juce::Rectangle<int>(mousePos.x, mousePos.y, 1, 1);
    }

    // Set now so a second click while waiting for activation cancels the open
    isMenuOpen = true;
    showTrayMenuWhenActive(targetArea, ++menuRequestId, juce::Time::getMillisecondCounter());
  }

  // App activation is asynchronous, and JUCE's PopupMenu dismisses itself within
  // ~10ms if the app isn't foreground while the mouse is outside the menu. When
  // Curve isn't already active (e.g. the first click on the icon), the menu would
  // flash and close, so wait for activation before showing it.
  //
  // Known limitation with "Show in Dock" on (activation policy Regular, see
  // DockIconManager): clicking the icon while another app is active never
  // activates Curve on macOS 14+ (cooperative activation). Neither
  // activateIgnoringOtherApps: (Process::makeForegroundProcess) nor [NSApp activate]
  // works; as an Accessory app, Curve does get activated. So the wait below times
  // out and the menu opens while Curve is inactive. JUCE's PopupMenu then closes it
  // within ~10ms unless the pointer is already inside it (the
  // doesAnyJuceCompHaveFocus check in PopupMenu's MouseSourceState), so the menu
  // sometimes flashes and disappears. Clicks while Curve is already active work.
  void showTrayMenuWhenActive(juce::Rectangle<int> targetArea, juce::uint32 requestId, juce::uint32 startTime) {
    if (requestId != menuRequestId || !isMenuOpen)
      return;

    constexpr juce::uint32 activationTimeoutMs = 500;

    if (!juce::Process::isForegroundProcess()
        && juce::Time::getMillisecondCounter() - startTime < activationTimeoutMs) {
      juce::Component::SafePointer<TrayIconController> safeSelf(this);
      juce::Timer::callAfterDelay(10, [safeSelf, targetArea, requestId, startTime] {
        if (auto *self = safeSelf.getComponent())
          self->showTrayMenuWhenActive(targetArea, requestId, startTime);
      });
      return;
    }

    showTrayMenu(targetArea);
  }

  void showTrayMenu(juce::Rectangle<int> targetArea) {
    GlobalShortcutManager::LocalActionMap localActions;
    int initialSelectedPresetId = 0;
    juce::Component::SafePointer<TrayIconController> safeSelf(this);
    auto menu = buildAppMenu(mainWindow, gitHubUpdater, true, &localActions,
                             [safeSelf] {
                               if (auto *self = safeSelf.getComponent())
                                 self->showPreferences();
                             },
                             &initialSelectedPresetId);

    juce::PopupMenu::Options options;
    options = options.withParentComponent(nullptr).withTargetScreenArea(targetArea);
    if (initialSelectedPresetId != 0)
      options = options.withInitiallySelectedItem(initialSelectedPresetId);

    isMenuOpen = true;

    if (activeMenuKeyMonitor != nullptr) {
      GlobalShortcutManager::removeMenuKeyMonitor(activeMenuKeyMonitor);
      activeMenuKeyMonitor = nullptr;
    }

    if (!localActions.empty()) {
      activeMenuKeyMonitor = GlobalShortcutManager::installMenuKeyMonitor(
          std::move(localActions),
          [safeSelf] {
            if (auto *self = safeSelf.getComponent()) {
              self->isMenuOpen = false;
              self->lastMenuDismissTime = juce::Time::getMillisecondCounter();
              if (self->activeMenuKeyMonitor != nullptr) {
                GlobalShortcutManager::removeMenuKeyMonitor(self->activeMenuKeyMonitor);
                self->activeMenuKeyMonitor = nullptr;
              }
            }
          });
    }

    menu.showMenuAsync(options, [safeSelf](int) {
      if (auto *self = safeSelf.getComponent()) {
        self->isMenuOpen = false;
        self->lastMenuDismissTime = juce::Time::getMillisecondCounter();
        if (self->activeMenuKeyMonitor != nullptr) {
          GlobalShortcutManager::removeMenuKeyMonitor(self->activeMenuKeyMonitor);
          self->activeMenuKeyMonitor = nullptr;
        }
      }
    });
  }

  ~TrayIconController() override {
    GlobalShortcutManager::unregisterGlobalShortcut();
    GlobalShortcutManager::unregisterGlobalShortcut(GlobalShortcutManager::Shortcut::pluginWindows);
    if (activeMenuKeyMonitor != nullptr) {
      GlobalShortcutManager::removeMenuKeyMonitor(activeMenuKeyMonitor);
      activeMenuKeyMonitor = nullptr;
    }
  }

  static int addPresetsToMenu(juce::PopupMenu &menu,
                              MainHostWindow &mainWindow,
                              GlobalShortcutManager::LocalActionMap *outLocalActions = nullptr) {
    // Sorted alphabetically, matching MIDI program change numbers
    auto files = PluginGraph::getPresetFiles();

    juce::File activeFile;
    if (mainWindow.graphHolder != nullptr &&
        mainWindow.graphHolder->graph != nullptr)
      activeFile = mainWindow.graphHolder->graph->getFile();

    if (files.isEmpty()) {
      menu.addItem("(No presets found)", false, false, nullptr);
      return 0;
    }

    juce::Component::SafePointer<MainHostWindow> safeWindow (&mainWindow);
    int activePresetItemId = 0;

    // Scenes of a group are marked in its colour, as in the Save Preset window, so it's clear
    // which presets share their plug-ins before saving one changes the others. The item colour
    // is drawn as a stripe (see CurveLookAndFeel::drawPopupMenuItem())
    juce::StringArray groups;
    std::map<juce::String, int> groupSizes;
    for (const auto &file : files) {
      groups.add(PluginGraph::readSceneGroup(file));
      if (groups.strings.getLast().isNotEmpty())
        ++groupSizes[groups.strings.getLast()];
    }

    for (int i = 0; i < files.size(); ++i) {
      const auto &file = files[i];
      juce::String name = file.getFileNameWithoutExtension();
      bool isCurrent = (file == activeFile);
      int itemId = 1000 + i;

      juce::PopupMenu::Item item(name.replace("&", "&&"));
      item.setID(itemId);
      item.setEnabled(true);
      item.setTicked(isCurrent);

      if (groups[i].isNotEmpty() && groupSizes[groups[i]] > 1)
        item.setColour(PluginGraph::getSceneGroupColour(groups[i]));

      if (isCurrent)
        activePresetItemId = itemId;

      auto action = [safeWindow, file] {
        if (auto* w = safeWindow.getComponent())
          w->loadPreset(file);
      };
      item.setAction(action);

      // Support up to 30 fast presets:
      // Index 0..9   -> Keys 1..9, 0 (Bank_None, badges "1".."9", "0")
      // Index 10..19 -> Keys Option + 1..9, 0 (Bank_Option, badges "⌥1".."⌥0")
      // Index 20..29 -> Keys Shift + 1..9, 0 (Bank_Shift, badges "⇧1".."⇧0")
      if (i < 30) {
        GlobalShortcutManager::ModifierBank bank = GlobalShortcutManager::Bank_None;
        juce::String badgePrefix;
        if (i >= 10 && i < 20) {
          bank = GlobalShortcutManager::Bank_Option;
          badgePrefix = juce::String::fromUTF8("\xe2\x8c\xa5"); // ⌥
        } else if (i >= 20) {
          bank = GlobalShortcutManager::Bank_Shift;
          badgePrefix = juce::String::fromUTF8("\xe2\x87\xa7"); // ⇧
        }

        int digitIndex = (i % 10) + 1; // 1..10
        char keyChar = (digitIndex == 10) ? '0' : static_cast<char>('0' + digitIndex);

        item.shortcutKeyDescription = badgePrefix + juce::String::charToString(keyChar);

        if (outLocalActions != nullptr) {
          GlobalShortcutManager::LocalShortcutKey key { bank, keyChar };
          (*outLocalActions)[key] = action;
        }
      }

      menu.addItem(item);
    }

    // If no preset is currently active, fallback to highlighting the first preset
    if (activePresetItemId == 0 && !files.isEmpty())
      activePresetItemId = 1000;

    return activePresetItemId;
  }

  void initWindowsShortcut() {
    auto *settings = getAppProperties().getUserSettings();

    if (settings == nullptr || !settings->getBoolValue("windowsShortcutEnabled", false)) {
      GlobalShortcutManager::unregisterGlobalShortcut(GlobalShortcutManager::Shortcut::pluginWindows);
      return;
    }

    juce::Component::SafePointer<MainHostWindow> safeWindow(&mainWindow);
    GlobalShortcutManager::registerGlobalShortcut(
        static_cast<uint32_t>(settings->getIntValue("windowsShortcutKeyCode", 0)),
        static_cast<uint32_t>(settings->getIntValue("windowsShortcutModifiers", 0)),
        [safeWindow] {
          if (auto *w = safeWindow.getComponent())
            if (w->graphHolder != nullptr && w->graphHolder->graph != nullptr)
              w->graphHolder->graph->togglePluginWindows();
        },
        GlobalShortcutManager::Shortcut::pluginWindows);
  }

  static juce::String getGlobalShortcutDisplayString() {
    if (auto *settings = getAppProperties().getUserSettings()) {
      if (!settings->getBoolValue("globalShortcutEnabled", false))
        return "(Disabled)";
      return settings->getValue("globalShortcutText", GlobalShortcutManager::defaultShortcutText());
    }
    return "(Disabled)";
  }

  void showPreferences() {
    if (preferencesWindow == nullptr) {
      juce::Component::SafePointer<TrayIconController> safeSelf(this);
      PreferencesComponent::Callbacks callbacks;
      callbacks.changeGlobalShortcut = [safeSelf] {
        if (auto *self = safeSelf.getComponent())
          self->showGlobalShortcutDialog();
      };
      callbacks.describeGlobalShortcut = [] { return getGlobalShortcutDisplayString(); };
      callbacks.changeWindowsShortcut = [safeSelf] {
        if (auto *self = safeSelf.getComponent())
          self->showWindowsShortcutDialog();
      };
      callbacks.describeWindowsShortcut = [] {
        if (auto *settings = getAppProperties().getUserSettings())
          if (settings->getBoolValue("windowsShortcutEnabled", false))
            return settings->getValue("windowsShortcutText");
        return juce::String("(Disabled)");
      };
      preferencesWindow = std::make_unique<PreferencesWindow>(mainWindow, gitHubUpdater, std::move(callbacks));
    }

    juce::Process::makeForegroundProcess();
    preferencesWindow->setVisible(true);
    preferencesWindow->toFront(true);
  }

  void showGlobalShortcutDialog() {
    bool enabled = false;
    uint32_t keyCode = GlobalShortcutManager::defaultCarbonKeyCode;
    uint32_t modifiers = GlobalShortcutManager::defaultCarbonModifiers;
    juce::String text = GlobalShortcutManager::defaultShortcutText();

    if (auto *settings = getAppProperties().getUserSettings()) {
      enabled = settings->getBoolValue("globalShortcutEnabled", false);
      keyCode = static_cast<uint32_t>(settings->getIntValue("globalShortcutKeyCode", (int) GlobalShortcutManager::defaultCarbonKeyCode));
      modifiers = static_cast<uint32_t>(settings->getIntValue("globalShortcutModifiers", (int) GlobalShortcutManager::defaultCarbonModifiers));
      text = settings->getValue("globalShortcutText", GlobalShortcutManager::defaultShortcutText());
    }

    juce::Component::SafePointer<TrayIconController> safeSelf(this);
    GlobalShortcutDialog::showDialog(enabled, keyCode, modifiers, text,
      [safeSelf](bool newEnabled, uint32_t newKeyCode, uint32_t newMods, const juce::String &newText) {
        if (auto *self = safeSelf.getComponent()) {
          if (auto *settings = getAppProperties().getUserSettings()) {
            settings->setValue("globalShortcutEnabled", newEnabled);
            settings->setValue("globalShortcutKeyCode", (int) newKeyCode);
            settings->setValue("globalShortcutModifiers", (int) newMods);
            settings->setValue("globalShortcutText", newText);
            settings->saveIfNeeded();
          }
          self->initGlobalShortcut();
        }
      });
  }

  // Showing and hiding the plug-in windows: off until set, as it has no default
  void showWindowsShortcutDialog() {
    bool enabled = false;
    uint32_t keyCode = 0, modifiers = 0;
    juce::String text;

    if (auto *settings = getAppProperties().getUserSettings()) {
      enabled = settings->getBoolValue("windowsShortcutEnabled", false);
      keyCode = static_cast<uint32_t>(settings->getIntValue("windowsShortcutKeyCode", 0));
      modifiers = static_cast<uint32_t>(settings->getIntValue("windowsShortcutModifiers", 0));
      text = settings->getValue("windowsShortcutText");
    }

    juce::Component::SafePointer<TrayIconController> safeSelf(this);
    GlobalShortcutDialog::showDialog(enabled, keyCode, modifiers, text,
      [safeSelf](bool newEnabled, uint32_t newKeyCode, uint32_t newMods, const juce::String &newText) {
        if (auto *self = safeSelf.getComponent()) {
          if (auto *settings = getAppProperties().getUserSettings()) {
            // Enabled without recording one keeps nothing to register
            settings->setValue("windowsShortcutEnabled", newEnabled && newText.isNotEmpty());
            settings->setValue("windowsShortcutKeyCode", (int) newKeyCode);
            settings->setValue("windowsShortcutModifiers", (int) newMods);
            settings->setValue("windowsShortcutText", newText);
            settings->saveIfNeeded();
          }
          self->initGlobalShortcut();
        }
      },
      GlobalShortcutDialog::pluginWindowsPurpose());
  }

  void initGlobalShortcut() {
    initWindowsShortcut();

    bool enabled = false;
    uint32_t keyCode = GlobalShortcutManager::defaultCarbonKeyCode;
    uint32_t modifiers = GlobalShortcutManager::defaultCarbonModifiers;

    if (auto *settings = getAppProperties().getUserSettings()) {
      enabled = settings->getBoolValue("globalShortcutEnabled", false);
      keyCode = static_cast<uint32_t>(settings->getIntValue("globalShortcutKeyCode", (int) GlobalShortcutManager::defaultCarbonKeyCode));
      modifiers = static_cast<uint32_t>(settings->getIntValue("globalShortcutModifiers", (int) GlobalShortcutManager::defaultCarbonModifiers));
    }

    if (enabled) {
      GlobalShortcutManager::registerGlobalShortcut(keyCode, modifiers, [this] {
        toggleMenuFromShortcut();
      });
    } else {
      GlobalShortcutManager::unregisterGlobalShortcut();
    }
  }

  static juce::PopupMenu buildAppMenu(MainHostWindow &mainWindow,
                                      GitHubUpdater &gitHubUpdater,
                                      bool includeVisibilityToggle = false,
                                      GlobalShortcutManager::LocalActionMap *outLocalActions = nullptr,
                                      std::function<void()> onOpenPreferences = nullptr,
                                      int *outInitialSelectedId = nullptr) {
    juce::ignoreUnused(gitHubUpdater);
    juce::Component::SafePointer<MainHostWindow> safeWindow (&mainWindow);

    juce::PopupMenu settingsmenu;
    settingsmenu.addItem("Audio Settings", [safeWindow] {
      if (auto* w = safeWindow.getComponent())
        w->showAudioSettings();
    });
    settingsmenu.addItem("Plug-in Manager", [safeWindow] {
      if (auto* w = safeWindow.getComponent())
        w->showPluginListWindow();
    });
    // Everything else: General, Keyboard and MIDI tabs
    settingsmenu.addItem("Preferences...", onOpenPreferences != nullptr, false, [onOpenPreferences] {
      if (onOpenPreferences)
        onOpenPreferences();
    });
    settingsmenu.addSeparator();
    settingsmenu.addItem("Curve Help", [] {
      HelpBookManager::showHelp();
    });
    settingsmenu.addItem("About...", [safeWindow] {
      if (auto* w = safeWindow.getComponent())
        w->showAboutBox();
    });
    settingsmenu.addSeparator();
    settingsmenu.addItem("Quit", [] {
      juce::JUCEApplication::getInstance()->systemRequestedQuit();
    });

    juce::PopupMenu menu;

    if (includeVisibilityToggle) {
      if (mainWindow.isVisible())
        menu.addItem("Hide Editor", [safeWindow] {
          if (auto* w = safeWindow.getComponent())
            w->hideWindow();
        });
      else
        menu.addItem("Show Editor", [safeWindow] {
          if (auto* w = safeWindow.getComponent())
            w->showWindow();
        });
    }

    // Only in the editor's own menu, where the graph it clears is in view
    if (!includeVisibilityToggle)
      menu.addItem("New Preset", [safeWindow] {
        if (auto* w = safeWindow.getComponent())
          w->newPreset();
      });
    menu.addItem("Save As Preset...", [safeWindow] {
      if (auto* w = safeWindow.getComponent())
        w->saveAsPreset();
    });
    menu.addSeparator();
    menu.addSectionHeader("Presets");
    int initialId = addPresetsToMenu(menu, mainWindow, outLocalActions);
    if (outInitialSelectedId != nullptr)
      *outInitialSelectedId = initialId;
    menu.addSeparator();

    // The mappings belong to the loaded preset, so they sit with the editor's preset items. MIDI
    // Learn maps any node, the tempo and the transport, so it's here rather than on a node
    if (!includeVisibilityToggle) {
      auto* engine = mainWindow.graphHolder != nullptr ? mainWindow.graphHolder->midiEngine.get() : nullptr;
      menu.addItem("MIDI Learn", engine != nullptr, engine != nullptr && engine->isLearning(), [safeWindow] {
        if (auto* w = safeWindow.getComponent())
          if (w->graphHolder != nullptr && w->graphHolder->midiEngine != nullptr)
            w->graphHolder->midiEngine->setLearning(!w->graphHolder->midiEngine->isLearning());
      });
      menu.addItem("MIDI Mappings for Current Preset...", [safeWindow] {
        if (auto* w = safeWindow.getComponent())
          w->showMidiMappingsWindow();
      });
    }
    // The preset's plug-in windows, tidied into rows
    {
      auto *graph = mainWindow.graphHolder != nullptr ? mainWindow.graphHolder->graph.get() : nullptr;
      menu.addItem("Arrange Plug-in Windows", graph != nullptr && graph->getNumPluginWindows() > 0, false, [safeWindow] {
        if (auto *w = safeWindow.getComponent())
          if (w->graphHolder != nullptr && w->graphHolder->graph != nullptr)
            w->graphHolder->graph->arrangePluginWindows();
      });

      // Saved with the scene, as its window layout is
      menu.addItem("Lock Plug-in Windows (This Scene)", graph != nullptr, PluginWindow::arePositionsLocked(), [safeWindow] {
        if (auto *w = safeWindow.getComponent())
          if (w->graphHolder != nullptr && w->graphHolder->graph != nullptr)
            w->graphHolder->graph->setPluginWindowPositionsLocked(!PluginWindow::arePositionsLocked());
      });
    }
    menu.addSubMenu("Settings", settingsmenu, true);

    return menu;
  }

private:
  MainHostWindow &mainWindow;
  GitHubUpdater &gitHubUpdater;
  bool isMenuOpen = false;
  juce::uint32 menuRequestId = 0;
  juce::uint32 lastMenuDismissTime = 0;
  void *activeMenuKeyMonitor = nullptr;
  std::unique_ptr<MacOSDisplayChangeNotifierBase> displayChangeNotifier;
  std::unique_ptr<PreferencesWindow> preferencesWindow;
  std::unique_ptr<MenuBarPanelModel> panelModel; // before the panel, which uses it
  std::unique_ptr<MenuBarPanel> panel;
  std::vector<juce::uint32> knownDisplayIDs;

  struct CallbackTimer : juce::Timer {
    std::function<void()> callback;
    void timerCallback() override {
      if (callback)
        callback();
    }
  };
  CallbackTimer debugRecreateTimer;
};

