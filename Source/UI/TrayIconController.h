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

#include "MainHostWindow.h"
#include "GraphEditorPanel.h"
#include "GitHubUpdater.h"

inline std::unique_ptr<InputStream> createAssetInputStream (const char* resourcePath)
{

    auto assetsDir = File::getSpecialLocation (File::currentExecutableFile)
                          .getParentDirectory().getSiblingFile ("Resources").getChildFile ("Assets");

    auto resourceFile = assetsDir.getChildFile (resourcePath);

    if (! resourceFile.existsAsFile())
        return {};

    return resourceFile.createInputStream();

}

inline Image getImageFromAssets (const char* assetName)
{
    auto hashCode = (String (assetName) + "@juce_demo_assets").hashCode64();
    auto img = ImageCache::getFromHashCode (hashCode);

    if (img.isNull())
    {
        std::unique_ptr<InputStream> juceIconStream (createAssetInputStream (assetName));

        if (juceIconStream == nullptr)
            return {};

        img = ImageFileFormat::loadFrom (*juceIconStream);

        ImageCache::addImageToCache (img, hashCode);
    }

    return img;
}

class TrayIconController : public juce::SystemTrayIconComponent
{
public:
    // Pass a reference to the window so we can control it
    TrayIconController(MainHostWindow& windowToControl, GitHubUpdater& gitHubUpdaterToControl)
        : mainWindow(windowToControl), gitHubUpdater(gitHubUpdaterToControl)
    {
        setIconImage (getImageFromAssets ("juce_icon.png"),
                      getImageFromAssets ("juce_icon_template.png"));
        setIconTooltip("Curve");

        if (auto* settings = getAppProperties().getUserSettings())
            isAutoAppUpdateCheckEnabled = settings->getBoolValue ("automaticUpdateChecks", true);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        // Leave this empty to prevent double-firing
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mouseWasClicked())
        {
            auto now = juce::Time::getMillisecondCounter();
            if (isMenuOpen || (now - lastMenuDismissTime < 250))
            {
                isMenuOpen = false;
                lastMenuDismissTime = now;
                juce::PopupMenu::dismissAllActiveMenus();
                return;
            }

            juce::Process::makeForegroundProcess();
            auto currentMousePos = juce::Desktop::getInstance().getMousePosition();

            juce::PopupMenu settingsmenu;
            settingsmenu.addItem("Audio Settings", [this] { mainWindow.showAudioSettings(); });
            settingsmenu.addItem("Plug-in Manager", [this] { mainWindow.showPluginListWindow(); });
            settingsmenu.addSeparator();
            settingsmenu.addItem("Auto-Check for App Updates", true, isAutoAppUpdateCheckEnabled, [this] { toggleAutoAppUpdateCheck(); });
            settingsmenu.addItem("About...", [this] { mainWindow.showAboutBox(); });
            settingsmenu.addSeparator();
            settingsmenu.addItem("Quit", [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); });

            juce::PopupMenu menu;

            // Toggle Visibility
            if (mainWindow.isVisible())
                menu.addItem("Hide Editor", [this] { mainWindow.hideWindow(); });
            else
                menu.addItem("Show Editor", [this] { mainWindow.showWindow(); });

            menu.addItem("Save As Preset...", [this] { mainWindow.saveAsPreset(); });
            menu.addSeparator();
            menu.addSectionHeader("Presets");
            addPresetsToMenu(menu);
            menu.addSeparator();

            menu.addSubMenu("Settings", settingsmenu, true);

            auto targetArea = juce::Rectangle<int>(currentMousePos.x, currentMousePos.y, 1, 1);
            juce::PopupMenu::Options options;
            options = options.withParentComponent(nullptr)
                .withTargetScreenArea(targetArea);

            isMenuOpen = true;
            juce::Component::SafePointer<TrayIconController> safeSelf (this);
            menu.showMenuAsync(options, [safeSelf] (int)
            {
                if (auto* self = safeSelf.getComponent())
                {
                    self->isMenuOpen = false;
                    self->lastMenuDismissTime = juce::Time::getMillisecondCounter();
                }
            });
        }
    }

    ~TrayIconController() override = default;

private:
    MainHostWindow& mainWindow;
    GitHubUpdater& gitHubUpdater;
    bool isAutoAppUpdateCheckEnabled = false;
    bool isMenuOpen = false;
    juce::uint32 lastMenuDismissTime = 0;

    void addPresetsToMenu(juce::PopupMenu& menu)
    {
        // Scan Presets folder for presets
        auto appDataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support").getChildFile(JUCEApplication::getInstance()->getApplicationName());
        auto presetsDir = appDataDir.getChildFile("Presets");
        auto files = presetsDir.findChildFiles(juce::File::findFiles, false, "*.filtergraph");

        // Sort presets alphabetically
        files.sort();

        juce::File activeFile;
        if (mainWindow.graphHolder != nullptr && mainWindow.graphHolder->graph != nullptr)
            activeFile = mainWindow.graphHolder->graph->getFile();

        if (files.isEmpty())
        {
            menu.addItem ("(No presets found)", false, false, nullptr);
            return;
        }

        for (const auto& file : files)
        {
            juce::String name = file.getFileNameWithoutExtension();
            bool isCurrent = (file == activeFile);

            juce::PopupMenu::Item item (name.replace ("&", "&&"));
            item.setEnabled (true);
            item.setTicked (isCurrent);
            item.setAction ([this, file] {
                mainWindow.loadPreset (file);
            });
            menu.addItem (item);
        }
    }

    void toggleAutoAppUpdateCheck()
    {
        if (isAutoAppUpdateCheckEnabled) 
        {
            isAutoAppUpdateCheckEnabled = false;
            if (auto* settings = getAppProperties().getUserSettings())
            {
                settings->setValue ("automaticUpdateChecks", false);
                settings->saveIfNeeded();
            }
        }
        else
        {
            isAutoAppUpdateCheckEnabled = true;
            if (auto* settings = getAppProperties().getUserSettings())
            {
                settings->setValue ("automaticUpdateChecks", true);
                settings->saveIfNeeded();
            }
            gitHubUpdater.doCheckNow();
        }
    }
};
