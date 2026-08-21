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
#include "GraphEditorPanel.h"
#include "LoginItemManager.h"
#include "MainHostWindow.h"

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

class TrayIconController : public juce::SystemTrayIconComponent {
public:
  // Pass a reference to the window so we can control it
  TrayIconController(MainHostWindow &windowToControl,
                     GitHubUpdater &gitHubUpdaterToControl)
      : mainWindow(windowToControl), gitHubUpdater(gitHubUpdaterToControl) {
    setIconImage(getImageFromAssets("juce_icon.png"),
                 getImageFromAssets("juce_icon_template.png"));
    setIconTooltip("Curve");
  }

  void mouseUp(const juce::MouseEvent &) override {
    // Leave this empty to prevent double-firing
  }

  void mouseDown(const juce::MouseEvent &event) override {
    if (event.mouseWasClicked()) {
      auto now = juce::Time::getMillisecondCounter();
      if (isMenuOpen || (now - lastMenuDismissTime < 250)) {
        isMenuOpen = false;
        lastMenuDismissTime = now;
        juce::PopupMenu::dismissAllActiveMenus();
        return;
      }

      juce::Process::makeForegroundProcess();
      auto currentMousePos = juce::Desktop::getInstance().getMousePosition();

      auto menu = buildAppMenu(mainWindow, gitHubUpdater, true);

      auto targetArea =
          juce::Rectangle<int>(currentMousePos.x, currentMousePos.y, 1, 1);
      juce::PopupMenu::Options options;
      options =
          options.withParentComponent(nullptr).withTargetScreenArea(targetArea);

      isMenuOpen = true;
      juce::Component::SafePointer<TrayIconController> safeSelf(this);
      menu.showMenuAsync(options, [safeSelf](int) {
        if (auto *self = safeSelf.getComponent()) {
          self->isMenuOpen = false;
          self->lastMenuDismissTime = juce::Time::getMillisecondCounter();
        }
      });
    }
  }

  ~TrayIconController() override = default;

  static void addPresetsToMenu(juce::PopupMenu &menu,
                               MainHostWindow &mainWindow) {
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
      return;
    }

    juce::Component::SafePointer<MainHostWindow> safeWindow (&mainWindow);

    for (const auto &file : files) {
      juce::String name = file.getFileNameWithoutExtension();
      bool isCurrent = (file == activeFile);

      juce::PopupMenu::Item item(name.replace("&", "&&"));
      item.setEnabled(true);
      item.setTicked(isCurrent);
      item.setAction([safeWindow, file] {
        if (auto* w = safeWindow.getComponent())
          w->loadPreset(file);
      });
      menu.addItem(item);
    }
  }

  static juce::PopupMenu buildAppMenu(MainHostWindow &mainWindow,
                                      GitHubUpdater &gitHubUpdater,
                                      bool includeVisibilityToggle = false) {
    bool isAutoAppUpdateCheckEnabled = true;
    bool isAutoSyncSoundEnabled = true;
    if (auto *settings = getAppProperties().getUserSettings()) {
      isAutoAppUpdateCheckEnabled =
          settings->getBoolValue("automaticUpdateChecks", true);
      isAutoSyncSoundEnabled =
          settings->getBoolValue("autoSyncSystemOutput", true);
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
#if JUCE_MAC
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
    settingsmenu.addSeparator();
#endif
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
    addPresetsToMenu(menu, mainWindow);
    menu.addSeparator();

    menu.addSubMenu("Settings", settingsmenu, true);

    return menu;
  }

private:
  MainHostWindow &mainWindow;
  GitHubUpdater &gitHubUpdater;
  bool isMenuOpen = false;
  juce::uint32 lastMenuDismissTime = 0;
};
