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
#include "LoginItemManager.h"
#include "MainHostWindow.h"
#include "SystemSleepManager.h"

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

inline bool attemptToEnableLoginItem() {
  juce::String errorMessage;
  LoginItemManager::setEnabled(true, errorMessage);

  bool enabled =
      (LoginItemManager::getStatus() == LoginItemManager::Status::enabled);

  if (enabled) {
    if (auto *settings = getAppProperties().getUserSettings()) {
      settings->setValue("openAtLogin", true);
      settings->saveIfNeeded();
    }
  } else {
    LoginItemManager::openSystemSettingsLoginItemsPane();
  }

  return enabled;
}

class TrayIconController : public juce::SystemTrayIconComponent,
                           private juce::Timer {
public:
  // Pass a reference to the window so we can control it
  TrayIconController(MainHostWindow &windowToControl,
                     GitHubUpdater &gitHubUpdaterToControl)
      : mainWindow(windowToControl), gitHubUpdater(gitHubUpdaterToControl) {
    refreshIcon();

    displayChangeNotifier = createMacOSDisplayChangeNotifier([this] {
      handleDisplayOrWakeChange();
    });

    initGlobalShortcut();
  }

  void refreshIcon() {
    setIconImage(getImageFromAssets("juce_icon.png"),
                 getImageFromAssets("juce_icon_template.png"));
    setIconTooltip("Curve");
  }

  void recreateStatusItem() {
    if (isMenuOpen)
      return;

    // Explicitly unregister old NSStatusItem from NSStatusBar so it is cleanly removed
    // from all menu bars and does not linger as a greyed-out/zombie placeholder.
    removeMacOSStatusItem (getNativeHandle());

    // Reset JUCE's internal Pimpl holder
    setIconImage({}, {});

    // Allocates a fresh NSStatusItem, which macOS registers across all currently active display menu bars.
    refreshIcon();
  }

  void handleDisplayOrWakeChange() {
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

    if (isMenuOpen) {
      // If user is currently interacting with the menu, retry shortly after
      startTimer(1500);
      return;
    }

    recreateStatusItem();
  }

  void mouseUp(const juce::MouseEvent &) override {
    // Leave this empty to prevent double-firing
  }

  void toggleMenuFromShortcut() {
    juce::MessageManager::callAsync([safeSelf = juce::Component::SafePointer<TrayIconController>(this)] {
      if (auto *self = safeSelf.getComponent()) {
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

    GlobalShortcutManager::LocalActionMap localActions;
    int initialSelectedPresetId = 0;
    juce::Component::SafePointer<TrayIconController> safeSelf(this);
    auto menu = buildAppMenu(mainWindow, gitHubUpdater, true, &localActions,
                             [safeSelf] {
                               if (auto *self = safeSelf.getComponent())
                                 self->showGlobalShortcutDialog();
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
    if (activeMenuKeyMonitor != nullptr) {
      GlobalShortcutManager::removeMenuKeyMonitor(activeMenuKeyMonitor);
      activeMenuKeyMonitor = nullptr;
    }
  }

  static int addPresetsToMenu(juce::PopupMenu &menu,
                              MainHostWindow &mainWindow,
                              GlobalShortcutManager::LocalActionMap *outLocalActions = nullptr) {
    // Scan Presets folder for presets
    auto appDataDir =
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Application Support")
            .getChildFile(
                juce::JUCEApplication::getInstance()->getApplicationName());
    auto presetsDir = appDataDir.getChildFile("Presets");
    auto files = presetsDir.findChildFiles(juce::File::findFiles, false,
                                           "*.filtergraph");

    // Sort presets alphabetically
    files.sort();

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

    for (int i = 0; i < files.size(); ++i) {
      const auto &file = files[i];
      juce::String name = file.getFileNameWithoutExtension();
      bool isCurrent = (file == activeFile);
      int itemId = 1000 + i;

      juce::PopupMenu::Item item(name.replace("&", "&&"));
      item.setID(itemId);
      item.setEnabled(true);
      item.setTicked(isCurrent);

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

  static juce::String getGlobalShortcutDisplayString() {
    if (auto *settings = getAppProperties().getUserSettings()) {
      if (!settings->getBoolValue("globalShortcutEnabled", false))
        return "(Disabled)";
      return settings->getValue("globalShortcutText", GlobalShortcutManager::defaultShortcutText());
    }
    return "(Disabled)";
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

  void initGlobalShortcut() {
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
                                      std::function<void()> onOpenShortcutSettings = nullptr,
                                      int *outInitialSelectedId = nullptr) {
    bool isAutoAppUpdateCheckEnabled = true;
    bool isAutoSyncSoundEnabled = true;
    bool isPreventSleepEnabled = false;
    if (auto *settings = getAppProperties().getUserSettings()) {
      isAutoAppUpdateCheckEnabled =
          settings->getBoolValue("automaticUpdateChecks", true);
      isAutoSyncSoundEnabled =
          settings->getBoolValue("autoSyncSystemOutput", true);
      isPreventSleepEnabled =
          settings->getBoolValue("preventSystemSleep", false);
    }

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
    settingsmenu.addSeparator();
    settingsmenu.addItem(
        "Force macOS System Audio to loopback", true, isAutoSyncSoundEnabled,
        [safeWindow, isAutoSyncSoundEnabled] {
          bool newState = !isAutoSyncSoundEnabled;
          if (auto *settings = getAppProperties().getUserSettings()) {
            settings->setValue("autoSyncSystemOutput", newState);
            settings->saveIfNeeded();
          }
          if (auto* w = safeWindow.getComponent())
            if (w->graphHolder != nullptr)
              w->graphHolder->propagateDeviceSettingsToNodes();
        });
    settingsmenu.addItem(
        "Prevent macOS from automatically sleeping", true, isPreventSleepEnabled,
        [isPreventSleepEnabled] {
          bool newState = !isPreventSleepEnabled;
          if (auto *settings = getAppProperties().getUserSettings()) {
            settings->setValue("preventSystemSleep", newState);
            settings->saveIfNeeded();
          }
          SystemSleepManager::setPreventSystemSleep(newState);
        });
    settingsmenu.addSeparator();
    auto updaterToken = gitHubUpdater.getLifetimeToken();
    settingsmenu.addItem("Check for Updates...", [updaterToken] {
      if (updaterToken != nullptr)
        if (auto* u = updaterToken->load())
          u->checkForUpdates(true);
    });
    settingsmenu.addItem(
        "Auto-Check for App Updates", true, isAutoAppUpdateCheckEnabled,
        [updaterToken, isAutoAppUpdateCheckEnabled] {
          bool newState = !isAutoAppUpdateCheckEnabled;
          if (auto *settings = getAppProperties().getUserSettings()) {
            settings->setValue("automaticUpdateChecks", newState);
            settings->saveIfNeeded();
          }
          if (newState && updaterToken != nullptr)
            if (auto* u = updaterToken->load())
              u->doCheckNow();
        });
    settingsmenu.addItem("Open at Login...", [] {
      if (attemptToEnableLoginItem())
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon, "Open at Login",
            "Curve will now open automatically at login.");
    });
    juce::String shortcutLabel = "Global Menu Shortcut: " + getGlobalShortcutDisplayString() + "...";
    settingsmenu.addItem(shortcutLabel, [onOpenShortcutSettings] {
      if (onOpenShortcutSettings)
        onOpenShortcutSettings();
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

    menu.addSubMenu("Settings", settingsmenu, true);

    return menu;
  }

private:
  MainHostWindow &mainWindow;
  GitHubUpdater &gitHubUpdater;
  bool isMenuOpen = false;
  juce::uint32 lastMenuDismissTime = 0;
  void *activeMenuKeyMonitor = nullptr;
  std::unique_ptr<MacOSDisplayChangeNotifierBase> displayChangeNotifier;
};

