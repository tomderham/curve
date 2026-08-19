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
#include "MainHostWindow.h"
#include "AudioResilienceManager.h"
#include "../Plugins/InternalPlugins.h"
#include "../Plugins/SystemAudioCaptureNode.h"

constexpr const char* scanModeKey = "pluginScanMode";

//==============================================================================
class Superprocess final : private ChildProcessCoordinator
{
public:
    Superprocess()
    {
        launchWorkerProcess (File::getSpecialLocation (File::currentExecutableFile), processUID, 0, 0);
    }

    ~Superprocess() override
    {
        killWorkerProcess();
    }

    enum class State
    {
        timeout,
        gotResult,
        connectionLost,
    };

    struct Response
    {
        State state;
        std::unique_ptr<XmlElement> xml;
    };

    Response getResponse()
    {
        std::unique_lock<std::mutex> lock { mutex };

        if (! condvar.wait_for (lock, std::chrono::milliseconds { 50 }, [&] { return gotResult || connectionLost; }))
            return { State::timeout, nullptr };

        const auto state = connectionLost ? State::connectionLost : State::gotResult;
        connectionLost = false;
        gotResult = false;

        return { state, std::move (pluginDescription) };
    }

    using ChildProcessCoordinator::sendMessageToWorker;

private:
    void handleMessageFromWorker (const MemoryBlock& mb) override
    {
        const std::lock_guard<std::mutex> lock { mutex };
        pluginDescription = parseXML (mb.toString());
        gotResult = true;
        condvar.notify_one();
    }

    void handleConnectionLost() override
    {
        const std::lock_guard<std::mutex> lock { mutex };
        connectionLost = true;
        condvar.notify_one();
    }

    std::mutex mutex;
    std::condition_variable condvar;

    std::unique_ptr<XmlElement> pluginDescription;
    bool connectionLost = false;
    bool gotResult = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Superprocess)
};

//==============================================================================
class CustomPluginScanner final : public KnownPluginList::CustomScanner,
                                  private ChangeListener
{
public:
    CustomPluginScanner()
    {
        if (auto* file = getAppProperties().getUserSettings())
            file->addChangeListener (this);

        handleChange();
    }

    ~CustomPluginScanner() override
    {
        if (auto* file = getAppProperties().getUserSettings())
            file->removeChangeListener (this);
    }

    bool findPluginTypesFor (AudioPluginFormat& format,
                             OwnedArray<PluginDescription>& result,
                             const String& fileOrIdentifier) override
    {
        const std::lock_guard<std::mutex> lock (scanMutex);

        if (scanInProcess.load (std::memory_order_relaxed))
        {
            superprocess = nullptr;
            format.findAllTypesForFile (result, fileOrIdentifier);
            return true;
        }

        if (addPluginDescriptions (format.getName(), fileOrIdentifier, result))
            return true;

        superprocess = nullptr;
        return false;
    }

    void scanFinished() override
    {
        const std::lock_guard<std::mutex> lock (scanMutex);
        superprocess = nullptr;
    }

private:
    /*  Scans for a plugin with format 'formatName' and ID 'fileOrIdentifier' using a subprocess,
        and adds discovered plugin descriptions to 'result'.

        Returns true on success.

        Failure indicates that the subprocess is unrecoverable and should be terminated.
    */
    bool addPluginDescriptions (const String& formatName,
                                const String& fileOrIdentifier,
                                OwnedArray<PluginDescription>& result)
    {
        if (superprocess == nullptr)
            superprocess = std::make_unique<Superprocess>();

        MemoryBlock block;
        MemoryOutputStream stream { block, true };
        stream.writeString (formatName);
        stream.writeString (fileOrIdentifier);

        if (! superprocess->sendMessageToWorker (block))
        {
            superprocess = nullptr;
            return false;
        }

        auto startTime = Time::getMillisecondCounter();

        for (;;)
        {
            if (shouldExit())
                return true;

            const auto response = superprocess->getResponse();

            if (response.state == Superprocess::State::timeout)
            {
                if (Time::getMillisecondCounter() - startTime > 20000)
                {
                    // 20s per-plugin timeout exceeded; kill worker and abort this plugin scan
                    superprocess = nullptr;
                    return false;
                }
                continue;
            }

            if (response.xml != nullptr)
            {
                for (const auto* item : response.xml->getChildIterator())
                {
                    auto desc = std::make_unique<PluginDescription>();

                    if (desc->loadFromXml (*item))
                        result.add (std::move (desc));
                }
            }

            return (response.state == Superprocess::State::gotResult);
        }
    }

    void handleChange()
    {
        if (auto* file = getAppProperties().getUserSettings())
            scanInProcess = (file->getIntValue (scanModeKey) == 0);
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        handleChange();
    }

    std::mutex scanMutex;
    std::unique_ptr<Superprocess> superprocess;

    std::atomic<bool> scanInProcess { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomPluginScanner)
};

//==============================================================================
class CustomPluginListComponent final : public PluginListComponent
{
public:
    CustomPluginListComponent (AudioPluginFormatManager& manager,
                               KnownPluginList& listToRepresent,
                               const File& pedal,
                               PropertiesFile* props,
                               bool async)
        : PluginListComponent (manager, listToRepresent, pedal, props, async)
    {

        // Use out-of-process plugin scanning for crash isolation
        getAppProperties().getUserSettings()->setValue (scanModeKey, juce::var (1));

        handleResize();
    }

    void resized() override
    {
        handleResize();
    }

private:
    void handleResize()
    {
        PluginListComponent::resized();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomPluginListComponent)
};

//==============================================================================
class MainHostWindow::PluginListWindow final : public DocumentWindow
{
public:
    PluginListWindow (MainHostWindow& mw, AudioPluginFormatManager& pluginFormatManager)
        : DocumentWindow ("Available Plugins",
                          LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                          DocumentWindow::minimiseButton | DocumentWindow::closeButton),
          owner (mw)
    {
        auto deadMansPedalFile = getAppProperties().getUserSettings()
                                   ->getFile().getSiblingFile ("RecentlyCrashedPluginsList");

        setContentOwned (new CustomPluginListComponent (pluginFormatManager,
                                                        owner.knownPluginList,
                                                        deadMansPedalFile,
                                                        getAppProperties().getUserSettings(),
                                                        true), true);

        setResizable (true, false);
        setResizeLimits (300, 400, 800, 1500);
        setTopLeftPosition (60, 60);

        restoreWindowStateFromString (getAppProperties().getUserSettings()->getValue ("listWindowPos"));
        setVisible (true);
    }

    ~PluginListWindow() override
    {
        getAppProperties().getUserSettings()->setValue ("listWindowPos", getWindowStateAsString());
        clearContentComponent();
    }

    void closeButtonPressed() override
    {
        owner.pluginListWindow = nullptr;
    }

private:
    MainHostWindow& owner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginListWindow)
};

//==============================================================================
MainHostWindow::MainHostWindow()
    : DocumentWindow (JUCEApplication::getInstance()->getApplicationName(),
                      LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                      DocumentWindow::allButtons)
{
    addDefaultFormatsToManager (formatManager);
    formatManager.addFormat (std::make_unique<InternalPluginFormat>());

    for (auto* format : formatManager.getFormats())
    {
        if (auto* props = getAppProperties().getUserSettings())
            format->searchPathsForPlugins (PluginListComponent::getLastSearchPath (*props, *format), false, false);
    }

    auto savedState = getAppProperties().getUserSettings()->getXmlValue ("audioDeviceState");
    if (savedState != nullptr && savedState->getStringAttribute ("audioOutputDeviceName").isNotEmpty())
    {
        juce::String inputDeviceName = savedState->getStringAttribute ("audioInputDeviceName");
        int numInputs = inputDeviceName.isNotEmpty() ? 256 : 0;
        deviceManager.initialise (numInputs, 256, savedState.get(), false);
    }
    else
    {
        // Initialize with empty setup until the user configures an interface in Audio Settings
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.setAudioDeviceSetup (setup, false);
    }

   #if JUCE_IOS || JUCE_ANDROID
    setFullScreen (true);
   #else
    setResizable (true, false);
    setResizeLimits (500, 400, 10000, 10000);
    centreWithSize (800, 600);
   #endif

    knownPluginList.setCustomScanner (std::make_unique<CustomPluginScanner>());

    graphHolder.reset (new GraphDocumentComponent (formatManager, deviceManager, knownPluginList));

    setContentNonOwned (graphHolder.get(), false);

    setUsingNativeTitleBar (true);

    restoreWindowStateFromString (getAppProperties().getUserSettings()->getValue ("mainWindowPos"));

    InternalPluginFormat internalFormat;
    internalTypes = internalFormat.getAllTypes();

    if (auto savedPluginList = getAppProperties().getUserSettings()->getXmlValue ("pluginList"))
    {
        knownPluginList.recreateFromXml (*savedPluginList);
    }

   #if JUCE_MAC
    // Ensure Apple's built-in AUNBandEQ is available on first launch or if missing from pluginList
    auto hasAUNBandEQ = [this]()
    {
        for (const auto& desc : knownPluginList.getTypes())
            if (desc.fileOrIdentifier.containsIgnoreCase (",nbeq,appl"))
                return true;
        return false;
    };

    if (! hasAUNBandEQ())
    {
        AudioUnitPluginFormat auFormat;
        OwnedArray<PluginDescription> found;
        auFormat.findAllTypesForFile (found, "AudioUnit:Effects/aufx,nbeq,appl");
        if (found.isEmpty())
            auFormat.findAllTypesForFile (found, "AudioUnit:aufx,nbeq,appl");

        for (auto* desc : found)
            knownPluginList.addType (*desc);
    }
   #endif

    // Remove any cached known (internal) plugins from list
    auto types = knownPluginList.getTypes();
    for (auto& desc : types)
        if (desc.pluginFormatName == InternalPluginFormat::getIdentifier())
            knownPluginList.removeType (desc);

    // Add current internal plugins to list
    for (auto& t : internalTypes)
        knownPluginList.addType (t);

    pluginSortMethod = (KnownPluginList::SortMethod) getAppProperties().getUserSettings()
                            ->getIntValue ("pluginSortMethod", KnownPluginList::sortByManufacturer);

    knownPluginList.addChangeListener (this);

    if (auto* g = graphHolder->graph.get())
        g->addChangeListener (this);

    addKeyListener (getCommandManager().getKeyMappings());

    getCommandManager().setFirstCommandTarget (this);
}

MainHostWindow::~MainHostWindow()
{
    pluginListWindow = nullptr;
    knownPluginList.removeChangeListener (this);

    if (auto* g = graphHolder->graph.get())
        g->removeChangeListener (this);

    getAppProperties().getUserSettings()->setValue ("mainWindowPos", getWindowStateAsString());
    clearContentComponent();

    graphHolder = nullptr;
}

void MainHostWindow::closeButtonPressed()
{
    // hide main window when close button pressed
    hideWindow();
}

void MainHostWindow::tryToQuitApplication()
{
    if (graphHolder != nullptr)
        graphHolder->closeAnyOpenPluginWindows();

    ModalComponentManager::getInstance()->cancelAllModalComponents();

    if (graphHolder != nullptr)
        graphHolder->releaseGraph();

    JUCEApplication::quit();
}

void MainHostWindow::changeListenerCallback (ChangeBroadcaster* changed)
{
    if (changed == &knownPluginList)
    {
        // Persist the plugin list immediately to preserve discovered plugins across scans
        if (auto savedPluginList = std::unique_ptr<XmlElement> (knownPluginList.createXml()))
        {
            getAppProperties().getUserSettings()->setValue ("pluginList", savedPluginList.get());
            getAppProperties().saveIfNeeded();
        }
    }
    else if (graphHolder != nullptr && changed == graphHolder->graph.get())
    {
        graphHolder->propagateDeviceSettingsToNodes();

        auto title = JUCEApplication::getInstance()->getApplicationName();
        auto f = graphHolder->graph->getFile();

        if (f.existsAsFile())
            title = f.getFileNameWithoutExtension() + " - " + title;

        setName (title);
    }
}

void MainHostWindow::createPlugin (const PluginDescriptionAndPreference& desc, Point<int> pos)
{
    if (graphHolder != nullptr)
        graphHolder->createNewPlugin (desc, pos);
}

static bool containsDuplicateNames (const Array<PluginDescription>& plugins, const String& name)
{
    int matches = 0;

    for (auto& p : plugins)
        if (p.name == name && ++matches > 1)
            return true;

    return false;
}

static constexpr int menuIDBase = 0x324503f4;

static void addToMenu (const KnownPluginList::PluginTree& tree,
                       PopupMenu& m,
                       const Array<PluginDescription>& allPlugins,
                       Array<PluginDescriptionAndPreference>& addedPlugins)
{
    for (auto* sub : tree.subFolders)
    {
        PopupMenu subMenu;
        addToMenu (*sub, subMenu, allPlugins, addedPlugins);

        m.addSubMenu (sub->folder, subMenu, true, nullptr, false, 0);
    }

    auto addPlugin = [&] (const auto& descriptionAndPreference, const auto& pluginName)
    {
        addedPlugins.add (descriptionAndPreference);
        const auto menuID = addedPlugins.size() - 1 + menuIDBase;
        m.addItem (menuID, pluginName, true, false);
    };

    for (auto& plugin : tree.plugins)
    {
        auto name = plugin.name;

        if (containsDuplicateNames (tree.plugins, name))
            name << " (" << plugin.pluginFormatName << ')';

        addPlugin (PluginDescriptionAndPreference { plugin, PluginDescriptionAndPreference::UseARA::no }, name);

       #if JUCE_PLUGINHOST_ARA && (JUCE_MAC || JUCE_WINDOWS || JUCE_LINUX)
        if (plugin.hasARAExtension)
        {
            name << " (ARA)";
            addPlugin (PluginDescriptionAndPreference { plugin }, name);
        }
       #endif
    }
}

void MainHostWindow::addPluginsToMenu (PopupMenu& m)
{
    pluginDescriptionsAndPreference = {};

    if (graphHolder != nullptr)
    {
        PopupMenu audioIOMenu;
        PopupMenu midiIOMenu;
        PopupMenu pluginsMenu;

        // Add Parametric EQ (AUNBandEQ) shortcut first in Plugins
        for (const auto& desc : knownPluginList.getTypes())
        {
            if (desc.fileOrIdentifier.containsIgnoreCase (",nbeq,appl"))
            {
                pluginDescriptionsAndPreference.add (PluginDescriptionAndPreference { desc, PluginDescriptionAndPreference::UseARA::no });
                const auto menuID = pluginDescriptionsAndPreference.size() - 1 + menuIDBase;
                pluginsMenu.addItem (menuID, "Parametric EQ (AUNBandEQ)", true, false);
                break;
            }
        }

        for (size_t idx = 0; idx < internalTypes.size(); ++idx)
        {
            const auto& t = internalTypes[idx];
            const int itemID = (int) idx + 1;

            if (t.category == "Plugins")
                pluginsMenu.addItem (itemID, t.name);
            else if (t.name.containsIgnoreCase ("MIDI"))
                midiIOMenu.addItem (itemID, t.name);
            else
                audioIOMenu.addItem (itemID, t.name);
        }

        if (audioIOMenu.getNumItems() > 0)
            m.addSubMenu ("Audio I/O", audioIOMenu);

        if (midiIOMenu.getNumItems() > 0)
            m.addSubMenu ("MIDI I/O", midiIOMenu);

        if (pluginsMenu.getNumItems() > 0)
            m.addSubMenu ("Plugins", pluginsMenu);
    }

    m.addSeparator();

    auto pluginDescriptions = knownPluginList.getTypes();

    // This avoids showing the internal types again later on in the list
    pluginDescriptions.removeIf ([] (PluginDescription& desc)
    {
        return desc.pluginFormatName == InternalPluginFormat::getIdentifier();
    });

    auto tree = KnownPluginList::createTree (pluginDescriptions, pluginSortMethod);
    addToMenu (*tree, m, pluginDescriptions, pluginDescriptionsAndPreference);
}

std::optional<PluginDescriptionAndPreference> MainHostWindow::getChosenType (const int menuID) const
{
    const auto internalIndex = menuID - 1;

    if (isPositiveAndBelow (internalIndex, internalTypes.size()))
        return PluginDescriptionAndPreference { internalTypes[(size_t) internalIndex] };

    const auto externalIndex = menuID - menuIDBase;

    if (isPositiveAndBelow (externalIndex, pluginDescriptionsAndPreference.size()))
        return pluginDescriptionsAndPreference[externalIndex];

    return {};
}

//==============================================================================
ApplicationCommandTarget* MainHostWindow::getNextCommandTarget()
{
    return findFirstTargetParentComponent();
}

void MainHostWindow::getAllCommands (Array<CommandID>& commands)
{
    const CommandID ids[] = {
                              CommandIDs::showPluginListEditor,
                              CommandIDs::showAudioSettings,
                              CommandIDs::allWindowsForward,
                              CommandIDs::autoScalePluginWindows
                            };

    commands.addArray (ids, numElementsInArray (ids));
}

void MainHostWindow::getCommandInfo (const CommandID commandID, ApplicationCommandInfo& result)
{
    const String category ("General");

    switch (commandID)
    {
    case CommandIDs::showPluginListEditor:
        result.setInfo ("Edit the List of Available Plug-ins...", {}, category, 0);
        result.addDefaultKeypress ('p', ModifierKeys::commandModifier);
        break;

    case CommandIDs::showAudioSettings:
        result.setInfo ("Change the Audio Device Settings", {}, category, 0);
        result.addDefaultKeypress ('a', ModifierKeys::commandModifier);
        break;

    case CommandIDs::allWindowsForward:
        result.setInfo ("All Windows Forward", "Bring all plug-in windows forward", category, 0);
        result.addDefaultKeypress ('w', ModifierKeys::commandModifier);
        break;

    case CommandIDs::autoScalePluginWindows:
        updateAutoScaleMenuItem (result);
        break;

    default:
        break;
    }
}

bool MainHostWindow::perform (const InvocationInfo& info)
{
    switch (info.commandID)
    {
    case CommandIDs::showPluginListEditor:
        if (pluginListWindow == nullptr)
            pluginListWindow.reset (new PluginListWindow (*this, formatManager));

        pluginListWindow->toFront (true);
        break;

    case CommandIDs::showAudioSettings:
        showAudioSettings();
        break;

    case CommandIDs::autoScalePluginWindows:
        if (auto* props = getAppProperties().getUserSettings())
        {
            auto newAutoScale = ! isAutoScalePluginWindowsEnabled();
            props->setValue ("autoScalePluginWindows", var (newAutoScale));

            ApplicationCommandInfo cmdInfo (info.commandID);
            updateAutoScaleMenuItem (cmdInfo);
        }
        break;

    case CommandIDs::allWindowsForward:
    {
        auto& desktop = Desktop::getInstance();

        for (int i = 0; i < desktop.getNumComponents(); ++i)
            desktop.getComponent (i)->toBehind (this);

        break;
    }

    default:
        return false;
    }

    return true;
}

void MainHostWindow::showAudioSettings()
{
    auto* audioSettingsComp = new AudioDeviceSelectorComponent (deviceManager,
                                                                0, 256,
                                                                0, 256,
                                                                true, true,
                                                                true, false);

    audioSettingsComp->setSize (500, 450);

    DialogWindow::LaunchOptions o;
    o.content.setOwned (audioSettingsComp);
    o.dialogTitle                   = "Audio Settings";
    o.componentToCentreAround       = (isOnDesktop() && isVisible()) ? this : nullptr;
    o.dialogBackgroundColour        = getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
    o.escapeKeyTriggersCloseButton  = true;
    o.useNativeTitleBar             = false;
    o.resizable                     = false;

     auto* w = o.create();
     auto safeThis = SafePointer<MainHostWindow> (this);

     w->enterModalState (true,
                         ModalCallbackFunction::create
                         ([safeThis] (int)
                         {
                             if (safeThis != nullptr)
                             {
                                 auto audioState = safeThis->deviceManager.createStateXml();

                                 getAppProperties().getUserSettings()->setValue ("audioDeviceState", audioState.get());
                                 getAppProperties().getUserSettings()->saveIfNeeded();

                                 if (auto* rm = getResilienceManager())
                                     rm->updateTargetSettings();

                                 if (safeThis->graphHolder != nullptr)
                                 {
                                     safeThis->graphHolder->propagateDeviceSettingsToNodes();
                                 }
                             }
                         }), true);
}

bool MainHostWindow::isInterestedInFileDrag (const StringArray&)
{
    return true;
}

void MainHostWindow::fileDragEnter (const StringArray&, int, int)
{
}

void MainHostWindow::fileDragMove (const StringArray&, int, int)
{
}

void MainHostWindow::fileDragExit (const StringArray&)
{
}

void MainHostWindow::filesDropped (const StringArray& files, int x, int y)
{
    if (files.isEmpty())
        return;

    if (graphHolder != nullptr)
    {
       #if ! (JUCE_ANDROID || JUCE_IOS)
        File firstFile { files[0] };

        if (files.size() == 1 && firstFile.hasFileExtension (PluginGraph::getFilenameSuffix()))
        {
            if (auto* g = graphHolder->graph.get())
            {
                SafePointer<MainHostWindow> parent { this };
                g->saveIfNeededAndUserAgreesAsync ([parent, firstFile] (FileBasedDocument::SaveResult r)
                {
                    if (parent == nullptr)
                        return;

                    if (r == FileBasedDocument::savedOk)
                        parent->loadPreset (firstFile);
                });
            }
        }
        else
       #endif
        {
            OwnedArray<PluginDescription> typesFound;
            knownPluginList.scanAndAddDragAndDroppedFiles (formatManager, files, typesFound);

            auto pos = graphHolder->getLocalPoint (this, Point<int> (x, y));

            for (int i = 0; i < jmin (5, typesFound.size()); ++i)
                if (auto* desc = typesFound.getUnchecked (i))
                    createPlugin (PluginDescriptionAndPreference { *desc }, pos);
        }
    }
}

bool MainHostWindow::isAutoScalePluginWindowsEnabled()
{
    // always auto-scale plugin windows
    return true;
}

void MainHostWindow::updateAutoScaleMenuItem (ApplicationCommandInfo& info)
{
    info.setInfo ("Auto-Scale Plug-in Windows", {}, "General", 0);
    info.setTicked (isAutoScalePluginWindowsEnabled());
}

void MainHostWindow::loadPreset(juce::File file)
{
    if (auto* rm = getResilienceManager())
    {
        rm->setSuspended (true);
    }

    if (graphHolder != nullptr && graphHolder->graph != nullptr)
    {
        // Smoothly fade out active audio before modifying the graph
        graphHolder->startPresetTransition();

        for (int i = 0; i < 6 && ! graphHolder->isTransitionFadedOut(); ++i)
            juce::Thread::sleep (5);

        auto& audioGraph = graphHolder->graph->graph;
        audioGraph.suspendProcessing (true);

        graphHolder->graph->setRestorePluginWindowsOnLoad (false);
        graphHolder->graph->loadFrom (file, true);
        graphHolder->propagateDeviceSettingsToNodes();

        audioGraph.suspendProcessing (false);

        // Smoothly fade in new preset audio
        graphHolder->endPresetTransition();
    }

    if (auto* rm = getResilienceManager())
    {
        rm->setSuspended (false);
    }
}

void MainHostWindow::saveAsPreset()
{
    if (graphHolder != nullptr && graphHolder->graph != nullptr)
        graphHolder->graph->saveAsInteractiveAsync (true, nullptr);
}

void MainHostWindow::showPluginListWindow()
{
    if (pluginListWindow == nullptr)
        pluginListWindow.reset (new PluginListWindow (*this, formatManager));
    pluginListWindow->toFront (true);
}

void MainHostWindow::showAboutBox()
{
    juce::String msg =
        "Version " + juce::JUCEApplication::getInstance()->getApplicationVersion() + "\n"
        + "Copyright " + juce::String(juce::CharPointer_UTF8 ("\xc2\xa9")) + " 2026 Thomas Derham.\n\n"
        + "Portions Copyright " + juce::String (juce::CharPointer_UTF8 ("\xc2\xa9")) + " 2004-2025 Raw Material Software.\n"
        + "All Rights Reserved.\n\n"
        + "This program is free software: you can redistribute it and/or modify "
        + "it under the terms of the GNU Affero General Public License as published by "
        + "the Free Software Foundation, either version 3 of the License, or "
        + "(at your option) any later version.\n\n"
        + "Made with JUCE.\n"
        + "Audio Unit is a trademark of Apple Inc.\n"
        + "VST is a registered trademark of Steinberg Media Technologies GmbH.";

    juce::NativeMessageBox::showMessageBoxAsync (juce::AlertWindow::InfoIcon, "About " + juce::JUCEApplication::getInstance()->getApplicationName(), msg);
}

void MainHostWindow::showWindow()
{
    juce::Process::makeForegroundProcess();

    if (! isOnDesktop())
        addToDesktop (getDesktopWindowStyleFlags());

    restoreWindowStateFromString (getAppProperties().getUserSettings()->getValue ("mainWindowPos"));

    auto totalBounds = Desktop::getInstance().getDisplays().getTotalBounds (true);
    if (! totalBounds.isEmpty() && ! totalBounds.intersects (getBounds()))
    {
        centreWithSize (getWidth(), getHeight());
    }

    setVisible (true);
    toFront (true);
}

void MainHostWindow::hideWindow()
{
    getAppProperties().getUserSettings()->setValue ("mainWindowPos", getWindowStateAsString());
    getAppProperties().getUserSettings()->saveIfNeeded();
    setVisible (false);
}

void MainHostWindow::showUpdateBanner (const juce::String& version, const juce::String& downloadUrl)
{
    if (graphHolder != nullptr)
        graphHolder->showUpdateBanner (version, downloadUrl);
}

void MainHostWindow::showAppMenu (juce::Component* target)
{
    if (onShowAppMenu)
        onShowAppMenu (target);
}