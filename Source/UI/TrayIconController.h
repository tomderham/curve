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
    TrayIconController(MainHostWindow& windowToControl)
        : mainWindow(windowToControl)
    {
        setIconImage (getImageFromAssets ("juce_icon.png"),
                      getImageFromAssets ("juce_icon_template.png"));
        setIconTooltip("Curve");

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
                juce::PopupMenu menu;
                
                // Toggle Visibility
                if (mainWindow.isVisible())
                    menu.addItem("Hide Editor", [this] { mainWindow.setVisible(false); });
                else
                    menu.addItem("Show Editor", [this] { mainWindow.setVisible(true); mainWindow.toFront(true); });

                menu.addSeparator();
                menu.addSectionHeader("Presets");
                addPresetsToMenu(menu);

                menu.addSeparator();
                menu.addItem("Save as preset", [this] { mainWindow.saveAsPreset(); });
                menu.addItem("Audio settings", [this] { mainWindow.showAudioSettings(); });
                menu.addItem("Plugin manager", [this] { mainWindow.showPluginListWindow(); });

                menu.addSeparator();
                menu.addItem("About", [this] { mainWindow.showAboutBox(); });
                menu.addItem("Quit", [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); });

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
    juce::File currentLoadedPreset;

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
};
