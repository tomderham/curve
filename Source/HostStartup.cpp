/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-8-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================

    Curve
   Copyright (c) Thomas Derham

   The modifications for Curve are licensed to you solely under the terms of the 
   AGPLv3 license terms as described above.

   CURVE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

#include <JuceHeader.h>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include "UI/MainHostWindow.h"
#include "Plugins/InternalPlugins.h"
#include "UI/TrayIconController.h"
#include "UI/AudioResilienceManager.h"
#include "UI/GitHubUpdater.h"

#if ! (JUCE_PLUGINHOST_VST || JUCE_PLUGINHOST_VST3 || JUCE_PLUGINHOST_AU)
 #error "If you're building the audio plugin host, you probably want to enable VST and/or AU support"
#endif

class PluginScannerSubprocess final : private ChildProcessWorker,
                                      private AsyncUpdater
{
public:
    PluginScannerSubprocess()
    {
        addDefaultFormatsToManager (formatManager);

        watchdogThread = std::thread ([this]
        {
            while (! shouldStopWatchdog.load (std::memory_order_relaxed))
            {
                std::unique_lock<std::mutex> lock (watchdogMutex);
                watchdogCv.wait_for (lock, std::chrono::milliseconds (500), [this]
                {
                    return shouldStopWatchdog.load (std::memory_order_relaxed)
                        || watchdogArmed.load (std::memory_order_relaxed);
                });

                if (shouldStopWatchdog.load (std::memory_order_relaxed))
                    break;

                if (watchdogArmed.load (std::memory_order_relaxed))
                {
                    auto now = juce::Time::getMillisecondCounter();
                    if (now >= watchdogDeadline.load (std::memory_order_relaxed))
                    {
                        // Plugin hung or timed out for >15s; terminate immediately to prevent zombie
                        std::_Exit (1);
                    }
                }
            }
        });
    }

    ~PluginScannerSubprocess() override
    {
        shouldStopWatchdog.store (true, std::memory_order_relaxed);
        watchdogCv.notify_all();
        if (watchdogThread.joinable())
            watchdogThread.join();
    }

    using ChildProcessWorker::initialiseFromCommandLine;

private:
    struct ScopedWatchdogArmer
    {
        ScopedWatchdogArmer (std::atomic<bool>& armed, std::atomic<juce::uint32>& deadline, std::condition_variable& cv, juce::uint32 timeoutMs = 15000)
            : isArmed (armed), cvRef (cv)
        {
            deadline.store (juce::Time::getMillisecondCounter() + timeoutMs, std::memory_order_relaxed);
            armed.store (true, std::memory_order_release);
            cvRef.notify_all();
        }

        ~ScopedWatchdogArmer()
        {
            isArmed.store (false, std::memory_order_release);
        }

        std::atomic<bool>& isArmed;
        std::condition_variable& cvRef;
    };

    void handleMessageFromCoordinator (const MemoryBlock& mb) override
    {
        if (mb.isEmpty())
            return;

        const std::lock_guard<std::mutex> lock (mutex);

        if (const auto results = doScan (mb); ! results.isEmpty())
        {
            sendResults (results);
        }
        else
        {
            pendingBlocks.emplace (mb);
            triggerAsyncUpdate();
        }
    }

    void handleConnectionLost() override
    {
        // Unconditional kernel-level exit so background threads spawned by plugins cannot keep the child process alive
        std::_Exit (0);
    }

    void handleAsyncUpdate() override
    {
        for (;;)
        {
            MemoryBlock block;
            {
                const std::lock_guard<std::mutex> lock (mutex);

                if (pendingBlocks.empty())
                    return;

                block = pendingBlocks.front();
                pendingBlocks.pop();
            }

            sendResults (doScan (block));
        }
    }

    OwnedArray<PluginDescription> doScan (const MemoryBlock& block)
    {
        MemoryInputStream stream { block, false };
        const auto formatName = stream.readString();
        const auto identifier = stream.readString();

        const auto matchingFormat = [&]() -> AudioPluginFormat*
        {
            for (auto* format : formatManager.getFormats())
                if (format->getName() == formatName)
                    return format;

            return nullptr;
        }();

        OwnedArray<PluginDescription> results;

        if (matchingFormat != nullptr)
        {
            ScopedWatchdogArmer armer (watchdogArmed, watchdogDeadline, watchdogCv, 15000);
            matchingFormat->findAllTypesForFile (results, identifier);
        }

        return results;
    }

    void sendResults (const OwnedArray<PluginDescription>& results)
    {
        XmlElement xml ("LIST");

        for (const auto& desc : results)
            xml.addChildElement (desc->createXml().release());

        const auto str = xml.toString();
        sendMessageToCoordinator ({ str.toRawUTF8(), str.getNumBytesAsUTF8() });
    }

    std::mutex mutex;
    std::queue<MemoryBlock> pendingBlocks;
    AudioPluginFormatManager formatManager;

    std::thread watchdogThread;
    std::mutex watchdogMutex;
    std::condition_variable watchdogCv;
    std::atomic<bool> shouldStopWatchdog { false };
    std::atomic<bool> watchdogArmed { false };
    std::atomic<juce::uint32> watchdogDeadline { 0 };
};

//==============================================================================
class PluginHostApp final : public JUCEApplication,
                            private AsyncUpdater
{
public:
    PluginHostApp() = default;

    void initialise (const String& commandLine) override
    {
        auto scannerSubprocess = std::make_unique<PluginScannerSubprocess>();

        if (scannerSubprocess->initialiseFromCommandLine (commandLine, processUID))
        {
            storedScannerSubprocess = std::move (scannerSubprocess);
            return;
        }

        // initialise our settings file..

        PropertiesFile::Options options;
        options.applicationName     = "Curve";
        options.filenameSuffix      = "settings";
        options.osxLibrarySubFolder = "Preferences";

        appProperties.reset (new ApplicationProperties());
        appProperties->setStorageParameters (options);

        lastShutdownClean = appProperties->getUserSettings()->getBoolValue ("lastShutdownClean", true);
        appProperties->getUserSettings()->setValue ("lastShutdownClean", false);
        appProperties->getUserSettings()->saveIfNeeded();

        // create presets folder if it doesn't exist
        auto appDataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support").getChildFile(JUCEApplication::getInstance()->getApplicationName());
        auto presetsDir = appDataDir.getChildFile("Presets");
        if (! appDataDir.exists())
            appDataDir.createDirectory();
        if (! presetsDir.exists())
            presetsDir.createDirectory();

        mainWindow.reset (new MainHostWindow());

        // hide editor window initially
        mainWindow->hideWindow();

        // initialize app Github updater
        githubUpdater.reset(new GitHubUpdater());
        githubUpdater->start("tomderham", "curve", options.applicationName);

        // initialize menu bar controller
        trayIcon.reset(new TrayIconController(*mainWindow, *githubUpdater));

        // initialize audio resilience manager
        auto& deviceManager = mainWindow->getDeviceManager();
        resilienceManager.reset(new AudioResilienceManager(deviceManager, []
        {
            if (auto* app = dynamic_cast<PluginHostApp*> (juce::JUCEApplication::getInstance()))
            {
                if (app->mainWindow != nullptr && app->mainWindow->graphHolder != nullptr)
                {
                    app->mainWindow->graphHolder->propagateDeviceSettingsToNodes();
                }
            }
        }));

        commandManager.registerAllCommandsForTarget (this);
        commandManager.registerAllCommandsForTarget (mainWindow.get());

        // Defer plugin loading to an async update so instantiation happens once the
        // application event loop is fully initialized.
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        File fileToOpen;

       #if JUCE_ANDROID || JUCE_IOS
        fileToOpen = PluginGraph::getDefaultGraphDocumentOnMobile();
       #else
        for (int i = 0; i < getCommandLineParameterArray().size(); ++i)
        {
            fileToOpen = File::getCurrentWorkingDirectory().getChildFile (getCommandLineParameterArray()[i]);

            if (fileToOpen.existsAsFile())
                break;
        }
       #endif

        if (! fileToOpen.existsAsFile())
        {
            RecentlyOpenedFilesList recentFiles;
            recentFiles.restoreFromString (getAppProperties().getUserSettings()->getValue ("recentFilterGraphFiles"));

            if (recentFiles.getNumFiles() > 0)
                fileToOpen = recentFiles.getFile (0);
        }

        if (fileToOpen.existsAsFile())
        {
            if (! lastShutdownClean)
            {
                juce::NativeMessageBox::showOkCancelBox (
                    juce::MessageBoxIconType::WarningIcon,
                    "Curve Startup Recovery",
                    "It looks like Curve didn't shut down cleanly last time. Would you like to load your last active preset (" + fileToOpen.getFileNameWithoutExtension() + ") anyway?",
                    nullptr,
                    juce::ModalCallbackFunction::create ([this, fileToOpen] (int result) {
                        if (result == 1) // OK / Load
                        {
                            mainWindow->loadPreset (fileToOpen);
                        }
                    })
                );
            }
            else
            {
                mainWindow->loadPreset (fileToOpen);
            }
        }
    }

    void shutdown() override
    {
        // Scanner subprocesses exit early without initializing appProperties
        if (appProperties != nullptr)
        {
            appProperties->getUserSettings()->setValue ("lastShutdownClean", true);
            appProperties->getUserSettings()->saveIfNeeded();
        }

        trayIcon = nullptr;
        resilienceManager = nullptr;
        githubUpdater = nullptr;
        mainWindow = nullptr;
        appProperties = nullptr;
        LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void suspended() override
    {
       #if JUCE_ANDROID || JUCE_IOS
        if (auto graph = mainWindow->graphHolder.get())
            if (auto ioGraph = graph->graph.get())
                ioGraph->saveDocument (PluginGraph::getDefaultGraphDocumentOnMobile());
       #endif
    }

    void systemRequestedQuit() override
    {
        if (appProperties != nullptr)
        {
            appProperties->getUserSettings()->setValue ("lastShutdownClean", true);
            appProperties->getUserSettings()->saveIfNeeded();
        }

        if (mainWindow != nullptr)
            mainWindow->tryToQuitApplication();
        else
            JUCEApplicationBase::quit();
    }

    bool backButtonPressed() override
    {
        if (mainWindow->graphHolder != nullptr)
            mainWindow->graphHolder->hideLastSidePanel();

        return true;
    }

    const String getApplicationName() override       { return ProjectInfo::projectName; }
    const String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override
    {
        return getCommandLineParameters().contains (processUID);
    }

    ApplicationCommandManager commandManager;
    std::unique_ptr<ApplicationProperties> appProperties;

    AudioResilienceManager* getResilienceManager() const { return resilienceManager.get(); }

private:
    std::unique_ptr<MainHostWindow> mainWindow;
    std::unique_ptr<PluginScannerSubprocess> storedScannerSubprocess;
    std::unique_ptr<TrayIconController> trayIcon;
    std::unique_ptr<AudioResilienceManager> resilienceManager;
    std::unique_ptr<GitHubUpdater> githubUpdater;
    bool lastShutdownClean = true;
};

static PluginHostApp& getApp()                    { return *dynamic_cast<PluginHostApp*> (JUCEApplication::getInstance()); }

ApplicationProperties& getAppProperties()         { return *getApp().appProperties; }
PropertiesFile* getUserSettings()
{
    if (auto* app = dynamic_cast<PluginHostApp*> (JUCEApplication::getInstance()))
        if (app->appProperties != nullptr)
            return app->appProperties->getUserSettings();
    return nullptr;
}
ApplicationCommandManager& getCommandManager()    { return getApp().commandManager; }

AudioResilienceManager* getResilienceManager()
{
    if (auto* app = dynamic_cast<PluginHostApp*> (JUCEApplication::getInstance()))
        return app->getResilienceManager();
    return nullptr;
}

//==============================================================================
static AutoScale autoScaleFromString (StringRef str)
{
    if (str.isEmpty())                     return AutoScale::useDefault;
    if (str == CharPointer_ASCII { "0" })  return AutoScale::scaled;
    if (str == CharPointer_ASCII { "1" })  return AutoScale::unscaled;

    jassertfalse;
    return AutoScale::useDefault;
}

static const char* autoScaleToString (AutoScale autoScale)
{
    if (autoScale == AutoScale::scaled)    return "0";
    if (autoScale == AutoScale::unscaled)  return "1";

    return {};
}

AutoScale getAutoScaleValueForPlugin (const String& identifier)
{
    if (identifier.isNotEmpty())
    {
        if (auto* settings = getUserSettings())
        {
            auto plugins = StringArray::fromLines (settings->getValue ("autoScalePlugins"));
            plugins.removeEmptyStrings();

            for (auto& plugin : plugins)
            {
                auto fromIdentifier = plugin.fromFirstOccurrenceOf (identifier, false, false);

                if (fromIdentifier.isNotEmpty())
                    return autoScaleFromString (fromIdentifier.fromFirstOccurrenceOf (":", false, false));
            }
        }
    }

    return AutoScale::useDefault;
}

void setAutoScaleValueForPlugin (const String& identifier, AutoScale s)
{
    auto* settings = getUserSettings();
    if (settings == nullptr)
        return;

    auto plugins = StringArray::fromLines (settings->getValue ("autoScalePlugins"));
    plugins.removeEmptyStrings();

    auto index = [identifier, plugins]
    {
        auto it = std::find_if (plugins.begin(), plugins.end(),
                                [&] (const String& str) { return str.startsWith (identifier); });

        return (int) std::distance (plugins.begin(), it);
    }();

    if (s == AutoScale::useDefault && index != plugins.size())
    {
        plugins.remove (index);
    }
    else
    {
        auto str = identifier + ":" + autoScaleToString (s);

        if (index != plugins.size())
            plugins.getReference (index) = str;
        else
            plugins.add (str);
    }

    settings->setValue ("autoScalePlugins", plugins.joinIntoString ("\n"));
    settings->saveIfNeeded();
}

static bool isAutoScaleAvailableForPlugin (const PluginDescription& description)
{
    return autoScaleOptionAvailable
          && (description.pluginFormatName.containsIgnoreCase ("VST")
              || description.pluginFormatName.containsIgnoreCase ("LV2"));
}

bool shouldAutoScalePlugin (const PluginDescription& description)
{
    if (! isAutoScaleAvailableForPlugin (description))
        return false;

    const auto scaleValue = getAutoScaleValueForPlugin (description.fileOrIdentifier);
    auto* settings = getUserSettings();

    return (scaleValue == AutoScale::scaled
              || (scaleValue == AutoScale::useDefault
                    && settings != nullptr && settings->getBoolValue ("autoScalePluginWindows")));
}

void addPluginAutoScaleOptionsSubMenu (AudioPluginInstance* pluginInstance,
                                       PopupMenu& menu)
{
    if (pluginInstance == nullptr)
        return;

    auto description = pluginInstance->getPluginDescription();

    if (! isAutoScaleAvailableForPlugin (description))
        return;

    auto identifier = description.fileOrIdentifier;

    PopupMenu autoScaleMenu;

    autoScaleMenu.addItem ("Default",
                           true,
                           getAutoScaleValueForPlugin (identifier) == AutoScale::useDefault,
                           [identifier] { setAutoScaleValueForPlugin (identifier, AutoScale::useDefault); });

    autoScaleMenu.addItem ("Enabled",
                           true,
                           getAutoScaleValueForPlugin (identifier) == AutoScale::scaled,
                           [identifier] { setAutoScaleValueForPlugin (identifier, AutoScale::scaled); });

    autoScaleMenu.addItem ("Disabled",
                           true,
                           getAutoScaleValueForPlugin (identifier) == AutoScale::unscaled,
                           [identifier] { setAutoScaleValueForPlugin (identifier, AutoScale::unscaled); });

    menu.addSubMenu ("Auto-scale window", autoScaleMenu);
}

// Application entry point
START_JUCE_APPLICATION (PluginHostApp)
