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

class TrayIconController : public juce::SystemTrayIconComponent, public juce::ChangeListener
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
            isAutoAppUpdateCheckEnabled = settings->getBoolValue ("automaticUpdateChecks");

        if (auto* g = mainWindow.graphHolder->graph.get())
            g->addChangeListener (this);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        // Leave this empty to prevent double-firing
    }

    void mouseDown(const juce::MouseEvent& event) override
    {

        if (event.mouseWasClicked()) {
            juce::Process::makeForegroundProcess();
            auto currentMousePos = juce::Desktop::getInstance().getMousePosition();

            juce::Timer::callAfterDelay(100, [this, currentMousePos]()
            {
                juce::PopupMenu settingsmenu;
                settingsmenu.addItem("Audio settings", [this] { mainWindow.showAudioSettings(); });
                settingsmenu.addItem("Plugin manager", [this] { mainWindow.showPluginListWindow(); });
                settingsmenu.addItem("Auto check for app updates", true, isAutoAppUpdateCheckEnabled, [this] { toggleAutoAppUpdateCheck(); });
                settingsmenu.addItem("About", [this] { mainWindow.showAboutBox(); });
                settingsmenu.addItem("Quit", [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); });

                juce::PopupMenu menu;
                
                // Toggle Visibility
                if (mainWindow.isVisible())
                    menu.addItem("Hide Editor", [this] { mainWindow.setVisible(false); });
                else
                    menu.addItem("Show Editor", [this] { mainWindow.setVisible(true); mainWindow.toFront(true); });

                menu.addItem("Save as preset", [this] { mainWindow.saveAsPreset(); });
                menu.addSeparator();
                menu.addSectionHeader("Presets");
                addPresetsToMenu(menu);
                menu.addSeparator();

                menu.addSubMenu("Settings", settingsmenu, true);

                auto targetArea = juce::Rectangle<int>(currentMousePos.x, currentMousePos.y, 1, 1);
                juce::PopupMenu::Options options;
                options = options.withParentComponent(nullptr)
                    .withTargetScreenArea(targetArea);
                menu.showMenuAsync(options);
            });
        }
    }

    void changeListenerCallback(juce::ChangeBroadcaster* source) override
        {
            if (auto* g = mainWindow.graphHolder->graph.get()) {
                if (source == g)
                    currentLoadedPreset = g->getFile(); // update local currentLoadedPreset when graph change occurs
            }
        }

    ~TrayIconController() override
        {
            if (auto* g = mainWindow.graphHolder->graph.get())
                g->removeChangeListener (this);
        }

private:
    MainHostWindow& mainWindow;
    GitHubUpdater& gitHubUpdater;
    juce::File currentLoadedPreset;
    bool isAutoAppUpdateCheckEnabled = false;

    void addPresetsToMenu(juce::PopupMenu& menu)
        {
            // Scan Presets folder for presets
            auto appDataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support").getChildFile(JUCEApplication::getInstance()->getApplicationName());
            auto presetsDir = appDataDir.getChildFile("Presets");
            auto files = presetsDir.findChildFiles(juce::File::findFiles, false, "*.filtergraph");

            for (const auto& file : files)
            {
                juce::String name = file.getFileNameWithoutExtension();
                bool isCurrent = (file == currentLoadedPreset);

                menu.addItem(name,
                     true,              // Enabled
                     isCurrent,         // Checked or not
                     [this, file] {     // Action
                         mainWindow.loadPreset(file);
                     });
            }
        }

    void toggleAutoAppUpdateCheck()
    {
        if(isAutoAppUpdateCheckEnabled) 
        {
            // auto update currently enabled, so toggle to disabled
            isAutoAppUpdateCheckEnabled = false;
            if (auto* settings = getAppProperties().getUserSettings())
            {
                settings->setValue ("automaticUpdateChecks", false);
                settings->saveIfNeeded();
            }
        }
        else
        {
            // auto update currently disabled, so toggle to enable and trigger update check now
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
