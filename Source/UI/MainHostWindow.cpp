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
#include "DockIconManager.h"
#include "CurveColours.h"
#include "AudioResilienceManager.h"
#include "IdleReleaseController.h"
#include "FilteredAudioDeviceSelectorComponent.h"
#include "MidiMappingsComponent.h"
#include "PresetSaveDialog.h"
#include "SessionRecovery.h"
#include "OnlineCalibrationDialog.h"
#include "../Calibration/AUNBandEQConverter.h"
#include "../Plugins/InternalPlugins.h"
#include "../Plugins/OutputInterfaceLoopbackNode.h"

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
                if (Time::getMillisecondCounter() - startTime > (uint32) pluginScanHardLimitMs)
                {
                    // Per-plugin timeout exceeded; kill worker and abort this plugin scan
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
        : PluginListComponent (manager, listToRepresent, pedal, props, async),
          formatManager (manager),
          list (listToRepresent),
          properties (props),
          rowsModel (*this, manager, listToRepresent, pedal)
    {

        // Use out-of-process plugin scanning for crash isolation
        getAppProperties().getUserSettings()->setValue (scanModeKey, juce::var (1));

        getTableListBox().setModel (&rowsModel);
        // 13 pt text, as Curve's other windows have (see FailedPluginRowsModel::paintCell())
        getTableListBox().setRowHeight (22);
        getTableListBox().getHeader().setSize (getTableListBox().getHeader().getWidth(), 24);
        getTableListBox().getHeader().setStretchToFitActive (true);

        // The things done most, scanning after installing plug-ins and removing them, as buttons,
        // and the rest in a menu, in place of JUCE's single Options menu
        getOptionsButton().setVisible (false);

        scanButton.setButtonText ("Scan for New Plug-ins");
        scanButton.setTooltip ("Finds plug-ins installed or updated since the last scan");
        scanButton.onClick = [this] { scanAllFormats(); };

        removeButton.setButtonText ("Remove");
        removeButton.setTooltip ("Removes the selected plug-ins from the list. Scanning finds them again.");
        removeButton.onClick = [this] { rowsModel.removeRows (rowsModel.getSelectedRows()); };

        moreButton.setButtonText ("More...");
        moreButton.onClick = [this] { showMoreMenu(); };

        countLabel.setColour (Label::textColourId, Curve::Colours::dimText);
        countLabel.setFont (FontOptions (12.5f));

        for (auto* c : std::initializer_list<Component*> { &scanButton, &removeButton, &moreButton, &countLabel })
            addAndMakeVisible (c);

        listListener.onChange = [this] { updateControls(); };
        listToRepresent.addChangeListener (&listListener);

        updateControls();
        handleResize();
    }

    ~CustomPluginListComponent() override
    {
        list.removeChangeListener (&listListener);
        getTableListBox().setModel (rowsModel.getJuceModel());
    }

    void resized() override
    {
        handleResize();
    }

    void updateControls()
    {
        // Curve's own plug-ins aren't shown or counted
        const auto types = list.getTypes();
        const auto numTypes = (int) std::count_if (types.begin(), types.end(), [] (const PluginDescription& d)
        {
            return d.pluginFormatName != InternalPluginFormat::getIdentifier();
        });
        const auto numFailed = list.getBlacklistedFiles().size();

        String count = String (numTypes) + (numTypes == 1 ? " plug-in" : " plug-ins");

        if (numFailed > 0)
            count << ", " << numFailed << " failed to scan";

        countLabel.setText (count, dontSendNotification);
        removeButton.setEnabled (getTableListBox().getNumSelectedRows() > 0);
        scanButton.setEnabled (! isBusyScanning());
    }

private:
    /*  Sits in front of JUCE's table model and takes over the rows for plug-ins that failed to
        scan. JUCE shows those by raw identifier, skips them on every rescan, and doesn't say
        that they must be removed before they can be tried again.

        It also leaves out Curve's own plug-ins, which are always in the list (Curve adds them
        at launch, for presets to find) and so would come back after being removed. Each table
        row is mapped to its row in JUCE's, which has the known plug-ins and then the failed ones.
    */
    class FailedPluginRowsModel final : public TableListBoxModel
    {
    public:
        FailedPluginRowsModel (CustomPluginListComponent& ownerIn,
                               AudioPluginFormatManager& formatManagerIn,
                               KnownPluginList& listIn,
                               const File& crashedPluginsFileIn)
            : owner (ownerIn),
              formatManager (formatManagerIn),
              list (listIn),
              crashedPluginsFile (crashedPluginsFileIn),
              juceModel (*ownerIn.getTableListBox().getTableListBoxModel())
        {
        }

        TableListBoxModel* getJuceModel() const         { return &juceModel; }

        int getNumRows() override
        {
            listRows.clear();
            const auto types = list.getTypes();

            for (int i = 0; i < types.size(); ++i)
                if (types.getReference (i).pluginFormatName != InternalPluginFormat::getIdentifier())
                    listRows.push_back (i);

            for (int i = 0; i < list.getBlacklistedFiles().size(); ++i)
                listRows.push_back (types.size() + i);

            return (int) listRows.size();
        }

        // The row in JUCE's table for a row of this one, or -1
        int toListRow (int row) const
        {
            return isPositiveAndBelow (row, (int) listRows.size()) ? listRows[(size_t) row] : -1;
        }

        void deleteKeyPressed (int) override            { removeRows (getTargetRows (-1)); }

        // Removes these rows of the table from the list. A failed one is cleared, so it's tried again
        void removeRows (const Array<int>& rows)
        {
            StringArray failed;
            Array<PluginDescription> known;
            const auto types = list.getTypes();

            for (const auto row : rows)
            {
                const auto listRow = toListRow (row);

                if (isPositiveAndBelow (listRow, types.size()))
                    known.add (types.getReference (listRow));
                else if (const auto identifier = getFailedPlugin (listRow); identifier.isNotEmpty())
                    failed.add (identifier);
            }

            for (const auto& identifier : failed)
                clearFailure (identifier);

            for (const auto& type : known)
                list.removeType (type);

            // Now, rather than when JUCE hears of it, so the rows aren't mapped as they were
            owner.getTableListBox().updateContent();
            owner.getTableListBox().deselectAllRows();
        }

        Array<int> getSelectedRows() const              { return getTargetRows (-1); }

        // Every plug-in that failed to scan cleared, so the next scan tries them again
        void clearFailures()
        {
            for (const auto& identifier : list.getBlacklistedFiles())
                clearFailure (identifier);
        }
        void sortOrderChanged (int column, bool forwards) override { juceModel.sortOrderChanged (column, forwards); }

        void paintRowBackground (Graphics& g, int row, int width, int height, bool selected) override
        {
            ignoreUnused (width, height);

            if (selected)
                g.fillAll (Curve::Colours::accent.withAlpha (0.35f));
            else if (row % 2 == 1)
                g.fillAll (Colours::white.withAlpha (0.025f));
        }

        void selectedRowsChanged (int lastRowSelected) override
        {
            juceModel.selectedRowsChanged (lastRowSelected);
            owner.updateControls();
        }

        // In Curve's own text, regular rather than JUCE's bold: the name bright, the rest dimmer
        void paintCell (Graphics& g, int row, int columnId, int width, int height, bool) override
        {
            const auto listRow = toListRow (row);
            const auto identifier = getFailedPlugin (listRow);
            const auto types = list.getTypes();
            String text;

            if (identifier.isNotEmpty())
            {
                text = getCellText (identifier, columnId);
                g.setColour (Curve::Colours::danger);
            }
            else if (isPositiveAndBelow (listRow, types.size()))
            {
                const auto& desc = types.getReference (listRow);

                // Column IDs as PluginListComponent's: name, format, category, manufacturer, description
                switch (columnId)
                {
                    case 1: text = desc.name; break;
                    case 2: text = desc.pluginFormatName; break;
                    case 3: text = desc.category.isNotEmpty() ? desc.category : "-"; break;
                    case 4: text = desc.manufacturerName; break;
                    case 5:
                    {
                        StringArray items;

                        if (desc.descriptiveName != desc.name)
                            items.add (desc.descriptiveName);

                        items.add (desc.version);
                        items.removeEmptyStrings();
                        text = items.joinIntoString (" - ");
                        break;
                    }
                    default: break;
                }

                g.setColour (columnId == 1 ? Curve::Colours::text : Curve::Colours::dimText);
            }

            if (text.isNotEmpty())
            {
                g.setFont (FontOptions (13.0f));
                g.drawText (text, 6, 0, width - 8, height, Justification::centredLeft, true);
            }
        }

        void cellClicked (int row, int columnId, const MouseEvent& e) override
        {
            const auto identifier = getFailedPlugin (toListRow (row));
            const auto rows = getTargetRows (row);

            // JUCE's own menu only acts on the clicked row
            if (! e.mods.isPopupMenu() || (identifier.isEmpty() && rows.size() < 2))
            {
                juceModel.cellClicked (toListRow (row), columnId, e);
                return;
            }

            StringArray targets;
            Array<PluginDescription> knownTargets;
            const auto types = list.getTypes();

            for (const auto tableRow : rows)
            {
                const auto targetRow = toListRow (tableRow);

                if (isPositiveAndBelow (targetRow, types.size()))
                    knownTargets.add (types.getReference (targetRow));
                else if (const auto failed = getFailedPlugin (targetRow); failed.isNotEmpty())
                    targets.add (failed);
            }

            PopupMenu menu;

            if (identifier.isNotEmpty())
            {
                auto* format = findFormat (identifier);

                menu.addItem (PopupMenu::Item ("Retry Scan")
                                .setEnabled (format != nullptr && ! owner.isBusyScanning())
                                .setAction ([this, targets, format]
                                            {
                                                // Only one scan can run at a time, so a selection
                                                // of several formats retries the clicked one's
                                                StringArray toScan;

                                                for (const auto& target : targets)
                                                    if (findFormat (target) == format)
                                                        toScan.add (target);

                                                for (const auto& target : toScan)
                                                    clearFailure (target);

                                                owner.startScan (*format, toScan);
                                            }));
            }

            // As the Remove button and the Delete key do, which refreshes the rows and selection
            menu.addItem (PopupMenu::Item ("Remove from List")
                            .setAction ([this, rows] { removeRows (rows); }));

            menu.showMenuAsync (PopupMenu::Options().withTargetComponent (e.originalComponent)
                                                    .withMousePosition()
                                                    .withDeletionCheck (owner));
        }

    private:
        // Failed plug-ins follow the known ones in JUCE's table. Empty for a known plug-in.
        String getFailedPlugin (int listRow) const
        {
            if (listRow < list.getNumTypes())
                return {};

            return list.getBlacklistedFiles()[listRow - list.getNumTypes()];
        }

        // The menu acts on the whole selection, unless the clicked row isn't part of it. -1 for the selection
        Array<int> getTargetRows (int clickedRow) const
        {
            auto& table = owner.getTableListBox();

            if (clickedRow >= 0 && ! table.isRowSelected (clickedRow))
                return { clickedRow };

            Array<int> rows;
            const auto selected = table.getSelectedRows();

            for (int i = 0; i < selected.size(); ++i)
                rows.add (selected[i]);

            return rows;
        }

        // JUCE blacklists everything in its crash list when a scan starts, so clear both
        void clearFailure (const String& identifier)
        {
            list.removeFromBlacklist (identifier);

            StringArray crashed;
            crashedPluginsFile.readLines (crashed);

            if (crashed.contains (identifier))
            {
                crashed.removeString (identifier);
                crashedPluginsFile.replaceWithText (crashed.joinIntoString ("\n"), true, true);
            }
        }

        AudioPluginFormat* findFormat (const String& identifier) const
        {
            for (auto* format : formatManager.getFormats())
                if (format->canScanForPlugins() && format->fileMightContainThisPluginType (identifier))
                    return format;

            return nullptr;
        }

        String getCellText (const String& identifier, int columnId)
        {
            // Column IDs match the ones PluginListComponent adds to its header
            enum { nameColumn = 1, formatColumn = 2, categoryColumn = 3, descriptionColumn = 5 };

            if (columnId == nameColumn)
                return getDisplayName (identifier);

            if (columnId == formatColumn)
                if (auto* format = findFormat (identifier))
                    return format->getName();

            // The default window is too narrow to show the description column
            if (columnId == categoryColumn)
                return "Scan failed";

            if (columnId == descriptionColumn)
                return "Failed to load during scan. Right-click to retry.";

            return {};
        }

        // Asking the format only reads system metadata, so this doesn't load the plug-in
        const String& getDisplayName (const String& identifier)
        {
            const auto cached = displayNames.find (identifier);

            if (cached != displayNames.end())
                return cached->second;

            auto name = identifier;

            if (auto* format = findFormat (identifier))
                name = format->getNameOfPluginFromIdentifier (identifier);

            if (name == identifier && File::isAbsolutePath (identifier))
                name = File (identifier).getFileNameWithoutExtension();

            return displayNames.emplace (identifier, name).first->second;
        }

        CustomPluginListComponent& owner;
        AudioPluginFormatManager& formatManager;
        KnownPluginList& list;
        File crashedPluginsFile;
        std::vector<int> listRows; // see toListRow()
        TableListBoxModel& juceModel;
        std::map<String, String> displayNames;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FailedPluginRowsModel)
    };

    void handleResize()
    {
        PluginListComponent::resized();

        auto r = getLocalBounds();
        auto bar = r.removeFromBottom (40).reduced (10, 7);

        scanButton.setBounds (bar.removeFromLeft (scanButton.getBestWidthForHeight (bar.getHeight()) + 16));
        bar.removeFromLeft (6);
        removeButton.setBounds (bar.removeFromLeft (removeButton.getBestWidthForHeight (bar.getHeight()) + 16));
        moreButton.setBounds (bar.removeFromRight (moreButton.getBestWidthForHeight (bar.getHeight()) + 16));
        bar.removeFromRight (8);
        countLabel.setBounds (bar.withTrimmedLeft (8));
        countLabel.setJustificationType (Justification::centredRight);

        getTableListBox().setBounds (r);
    }

    // Each format that can be scanned in turn, as only one scan can run at a time
    void scanAllFormats()
    {
        pendingScans.clear();

        for (auto* format : formatManager.getFormats())
            if (format->canScanForPlugins())
                pendingScans.add (format);

        startNextScan();
    }

    void startNextScan()
    {
        if (isScanning())
        {
            Timer::callAfterDelay (300, [safeThis = SafePointer<CustomPluginListComponent> (this)]
            {
                if (safeThis != nullptr)
                    safeThis->startNextScan();
            });
            return;
        }

        // A format that scans folders is given the files in the ones last chosen, or its default
        // ones, as JUCE otherwise asks which folders to scan every time. Unchanged ones are skipped
        if (! pendingScans.isEmpty())
        {
            auto& format = *pendingScans.removeAndReturn (0);

            if (format.getDefaultLocationsToSearch().getNumPaths() > 0 && properties != nullptr)
            {
                // None there, e.g. no LV2 plug-ins installed, is nothing to scan. JUCE would take
                // an empty list as a reason to ask for folders
                const auto files = format.searchPathsForPlugins (getLastSearchPath (*properties, format), true, true);

                if (! files.isEmpty())
                    scanFor (format, files);
            }
            else
            {
                scanFor (format);
            }
        }

        updateControls();

        if (! pendingScans.isEmpty() || isScanning())
            Timer::callAfterDelay (300, [safeThis = SafePointer<CustomPluginListComponent> (this)]
            {
                if (safeThis != nullptr)
                    safeThis->startNextScan();
            });
    }

    // A scan, or a queue of them from Scan for New Plug-ins, is running
    bool isBusyScanning() const     { return isScanning() || ! pendingScans.isEmpty(); }

    // Starts one scan, unless one's running, which would be cut short. The buttons follow it
    void startScan (AudioPluginFormat& format, const StringArray& files = {})
    {
        if (isBusyScanning())
            return;

        if (files.isEmpty())
            scanFor (format);
        else
            scanFor (format, files);

        startNextScan(); // which has none queued, so just waits for this one, then updates the buttons
    }

    void showMoreMenu()
    {
        PopupMenu menu;
        auto& table = getTableListBox();
        const auto selectedRow = rowsModel.toListRow (table.getSelectedRow());
        const auto types = list.getTypes();

        const auto selectedFile = isPositiveAndBelow (selectedRow, types.size())
                                    ? File::createFileWithoutCheckingPath (types.getReference (selectedRow).fileOrIdentifier)
                                    : File();

        menu.addItem (PopupMenu::Item ("Show in Finder")
                        .setEnabled (File::isAbsolutePath (selectedFile.getFullPathName()) && selectedFile.exists())
                        .setAction ([selectedFile] { selectedFile.revealToUser(); }));

        menu.addSeparator();

        for (auto* format : formatManager.getFormats())
            if (format->canScanForPlugins())
                menu.addItem (PopupMenu::Item ("Scan " + format->getName() + " Plug-ins Only"
                                               + (format->getDefaultLocationsToSearch().getNumPaths() > 0 ? String ("...") : String()))
                                .setEnabled (! isBusyScanning())
                                .setAction ([this, format] { startScan (*format); }));

        menu.addSeparator();

        menu.addItem (PopupMenu::Item ("Remove Plug-ins That No Longer Exist")
                        .setAction ([this] { removeMissing(); }));

        menu.addItem (PopupMenu::Item ("Clear List...")
                        .setAction ([this]
                        {
                            AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Clear the Plug-in List?",
                                                          "Every plug-in is removed from the list until you scan again. "
                                                          "Presets that use them can't load them meanwhile.",
                                                          "Clear List", "Cancel", this,
                                                          ModalCallbackFunction::create ([safeThis = SafePointer<CustomPluginListComponent> (this)] (int result)
                                                          {
                                                              // Curve's own stay, for presets to find (see FailedPluginRowsModel).
                                                              // Those that failed to scan go too, so a scan tries them again
                                                              if (result == 1 && safeThis != nullptr)
                                                              {
                                                                  safeThis->rowsModel.clearFailures();

                                                                  for (const auto& type : safeThis->list.getTypes())
                                                                      if (type.pluginFormatName != InternalPluginFormat::getIdentifier())
                                                                          safeThis->list.removeType (type);

                                                                  safeThis->getTableListBox().updateContent();
                                                                  safeThis->getTableListBox().deselectAllRows();
                                                              }
                                                          }));
                        }));

        menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&moreButton).withDeletionCheck (*this));
    }

    void removeMissing()
    {
        for (const auto& type : list.getTypes())
            for (auto* format : formatManager.getFormats())
                if (format->getName() == type.pluginFormatName && ! format->doesPluginStillExist (type))
                    list.removeType (type);
    }

    struct ListListener final : public juce::ChangeListener
    {
        std::function<void()> onChange;
        void changeListenerCallback (ChangeBroadcaster*) override { if (onChange != nullptr) onChange(); }
    };

    AudioPluginFormatManager& formatManager;
    KnownPluginList& list;
    PropertiesFile* properties;
    ListListener listListener;
    TextButton scanButton, removeButton, moreButton;
    Label countLabel;
    Array<AudioPluginFormat*> pendingScans;

    FailedPluginRowsModel rowsModel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomPluginListComponent)
};

//==============================================================================
class MainHostWindow::PluginListWindow final : public DocumentWindow
{
public:
    PluginListWindow (MainHostWindow& mw, AudioPluginFormatManager& pluginFormatManager)
        : DocumentWindow ("Plug-in Manager",
                          LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                          DocumentWindow::minimiseButton | DocumentWindow::closeButton),
          owner (mw)
    {
        // As Curve's other windows
        setUsingNativeTitleBar (true);

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
    // Follow mode: start on macOS's current output rather than the one used last time
    if (savedState != nullptr && AudioResilienceManager::isFollowingSystemOutput())
    {
        auto followedName = AudioResilienceManager::getFollowedOutputDeviceName();
        if (followedName.isNotEmpty() && followedName != savedState->getStringAttribute ("audioOutputDeviceName"))
        {
            AudioResilienceManager::applyFollowedDevice (*savedState, followedName);

            // Saved, so the resilience manager doesn't see a change of interface when it starts
            // (which would load that interface's default preset, even with Also load at launch off)
            getAppProperties().getUserSettings()->setValue ("audioDeviceState", savedState.get());
        }

        // An input that isn't connected (typically on the interface macOS has moved away from)
        // would stop the output opening at all. Open without it; it stays in the saved settings,
        // and the resilience manager adds it back when it reappears.
        auto inputName = savedState->getStringAttribute ("audioInputDeviceName");
        bool isInputAvailable = inputName.isEmpty();
        for (auto* type : deviceManager.getAvailableDeviceTypes())
        {
            type->scanForDevices();
            isInputAvailable = isInputAvailable || type->getDeviceNames (true).contains (inputName);
        }

        if (! isInputAvailable)
        {
            savedState->removeAttribute ("audioInputDeviceName");
            savedState->removeAttribute ("audioDeviceInChans");
        }
    }

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

    setResizable (true, false);
    setResizeLimits (500, 400, 10000, 10000);
    centreWithSize (800, 600);

    knownPluginList.setCustomScanner (std::make_unique<CustomPluginScanner>());

    graphHolder.reset (new GraphDocumentComponent (formatManager, deviceManager, knownPluginList));

    graphHolder->midiEngine->onLoadPreset = [safeThis = SafePointer<MainHostWindow> (this)] (const File& file)
    {
        if (safeThis != nullptr)
            safeThis->loadPreset (file);
    };

    setContentNonOwned (graphHolder.get(), false);

    setUsingNativeTitleBar (true);

    restoreWindowStateFromString (getAppProperties().getUserSettings()->getValue ("mainWindowPos"));

    InternalPluginFormat internalFormat;
    internalTypes = internalFormat.getAllTypes();

    if (auto savedPluginList = getAppProperties().getUserSettings()->getXmlValue ("pluginList"))
    {
        knownPluginList.recreateFromXml (*savedPluginList);
    }

    // Populate Apple built-in AudioUnit plugins if missing from pluginList
    AudioUnitPluginFormat auFormat;
    auto auIdentifiers = auFormat.searchPathsForPlugins (FileSearchPath(), false, false);
    
    std::unordered_set<juce::String> knownIdentifiers;
    for (const auto& knownDesc : knownPluginList.getTypes())
        knownIdentifiers.insert (knownDesc.fileOrIdentifier);

    for (const auto& identifier : auIdentifiers)
    {
        if (identifier.containsIgnoreCase (",appl") || identifier.containsIgnoreCase ("apple"))
        {
            if (knownIdentifiers.find (identifier) == knownIdentifiers.end())
            {
                OwnedArray<PluginDescription> found;
                auFormat.findAllTypesForFile (found, identifier);
                for (auto* desc : found)
                {
                    knownPluginList.addType (*desc);
                    knownIdentifiers.insert (identifier);
                }
            }
        }
    }

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

    // Started by PluginHostApp once the preset Curve opens with is decided
    sessionRecovery = std::make_unique<SessionRecovery> (
        [this]() -> PluginGraph* { return graphHolder != nullptr ? graphHolder->graph.get() : nullptr; },
        // As Curve's launch finds it: the interface followed, or set, while it's released when idle
        [this]
        {
            if (auto* device = deviceManager.getCurrentAudioDevice())
                return device->getName();

            if (AudioResilienceManager::isFollowingSystemOutput())
                return AudioResilienceManager::getFollowedOutputDeviceName();

            return deviceManager.getAudioDeviceSetup().outputDeviceName;
        },
        [this] { return isLoadingPreset || isPresetLoadScheduled; });

    addKeyListener (getCommandManager().getKeyMappings());

    getCommandManager().setFirstCommandTarget (this);
}

MainHostWindow::~MainHostWindow()
{
    sessionRecovery = nullptr;
    pluginListWindow = nullptr;
    midiMappingsWindow = nullptr;
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
    // As the graph still is, its plug-in windows open
    if (sessionRecovery != nullptr)
        sessionRecovery->quitting();

    if (graphHolder != nullptr)
        graphHolder->closeAnyOpenPluginWindows();

    ModalComponentManager::getInstance()->cancelAllModalComponents();

    // Refers to the graph, which is deleted here, and would otherwise stay open until shutdown
    midiMappingsWindow = nullptr;

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
        graphHolder->graph->updatePluginWindowTitles();

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

       #if JUCE_PLUGINHOST_ARA
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

        // By what they're for, each group under its name, so related ones are found together
        pluginsMenu.addSectionHeader ("EQ");
        auto auDesc = AUNBandEQConverter::findAUNBandEQ (knownPluginList);

        if (auDesc.has_value())
        {
            pluginDescriptionsAndPreference.add (PluginDescriptionAndPreference { *auDesc, PluginDescriptionAndPreference::UseARA::no });
            const auto menuID = pluginDescriptionsAndPreference.size() - 1 + menuIDBase;
            pluginsMenu.addItem (menuID, "Parametric EQ", true, false);
        }
        else
        {
            pluginsMenu.addItem (0, "Parametric EQ", false, false);
        }
        pluginsMenu.addItem (onlineCalibrationCreateMenuID, "Parametric EQ (import from AutoEQ...)", true, false);
        pluginsMenu.addItem (importCalibrationCreateMenuID, "Parametric EQ (import from file...)", true, false);

        std::set<size_t> addedInternalIndices;

        // Helper lambda to add internal plugin by exact name
        auto addInternalByName = [&] (const juce::String& name)
        {
            for (size_t idx = 0; idx < internalTypes.size(); ++idx)
            {
                if (internalTypes[idx].name.equalsIgnoreCase (name))
                {
                    pluginsMenu.addItem ((int) idx + 1, internalTypes[idx].name);
                    addedInternalIndices.insert (idx);
                    return;
                }
            }
        };

        // StringArrays, which own their names (an initializer_list in a pair wouldn't outlive the line)
        const std::vector<std::pair<juce::String, juce::StringArray>> groups {
            { "Headphones",      { "Headphone Crossfeed", "Headphone Speaker Emulation" } },
            { "Level",           { "Gain", "Clipper", "Meter" } },
            { "Stereo",          { "Pan", "Stereo Utility", "Mid/Side Encoder", "Mid/Side Decoder" } },
            { "Time and Phase",  { "Delay", "Invert Phase" } },
            { "Record and Play", { "Audio Recorder", "Looper", "Player" } },
            { "Performance",     { "Score Viewer", "Music Remote" } },
        };

        // Each heading starts its group, so no line between them
        for (const auto& [header, names] : groups)
        {
            pluginsMenu.addSectionHeader (header);

            for (const auto& name : names)
                addInternalByName (name);
        }

        // Catch-all: add any future internal plugins not explicitly categorized
        bool addedExtra = false;
        for (size_t idx = 0; idx < internalTypes.size(); ++idx)
        {
            if (addedInternalIndices.find (idx) != addedInternalIndices.end())
                continue;

            const auto& t = internalTypes[idx];
            if (t.category == "Plugins")
            {
                if (! addedExtra)
                {
                    pluginsMenu.addSeparator();
                    addedExtra = true;
                }
                pluginsMenu.addItem ((int) idx + 1, t.name);
                addedInternalIndices.insert (idx);
            }
        }

        // Populate Audio I/O and MIDI I/O submenus
        for (size_t idx = 0; idx < internalTypes.size(); ++idx)
        {
            const auto& t = internalTypes[idx];
            const int itemID = (int) idx + 1;

            if (t.name.containsIgnoreCase ("MIDI"))
            {
                midiIOMenu.addItem (itemID, t.name);
            }
            else if (t.category == "Audio I/O" || t.name.containsIgnoreCase ("Audio Input") || t.name.containsIgnoreCase ("Audio Output") || t.name.containsIgnoreCase ("Loopback"))
            {
                audioIOMenu.addItem (itemID, t.name);
            }
        }

        if (audioIOMenu.getNumItems() > 0)
            m.addSubMenu ("Audio I/O", audioIOMenu);

        if (midiIOMenu.getNumItems() > 0)
            m.addSubMenu ("MIDI I/O", midiIOMenu);

        if (pluginsMenu.getNumItems() > 0)
            m.addSubMenu ("Curve Plugins", pluginsMenu);
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
                              CommandIDs::showAudioSettings
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

    default:
        return false;
    }

    return true;
}

static void keepMissingInputDevice (juce::XmlElement& newState, const juce::XmlElement& previousState,
                                    juce::AudioDeviceManager& deviceManager)
{
    const auto inputName = previousState.getStringAttribute ("audioInputDeviceName");
    if (inputName.isEmpty() || newState.getStringAttribute ("audioInputDeviceName").isNotEmpty())
        return;

    for (auto* type : deviceManager.getAvailableDeviceTypes())
        if (type->getDeviceNames (true).contains (inputName))
            return; // connected, so the dialog left it out on purpose

    newState.setAttribute ("audioInputDeviceName", inputName);
    if (previousState.hasAttribute ("audioDeviceInChans"))
        newState.setAttribute ("audioDeviceInChans", previousState.getStringAttribute ("audioDeviceInChans"));
}

void MainHostWindow::showAudioSettings()
{
    // The selector needs an open device to show and edit the current configuration
    if (auto* idle = getIdleReleaseController())
        idle->resumeIfReleased();

    if (auto* rm = getResilienceManager())
        rm->setSuspended (true);

    auto* audioSettingsComp = new FilteredAudioDeviceSelectorComponent (deviceManager,
                                                                        0, 256,
                                                                        0, 256,
                                                                        true, true,
                                                                        true, false);
    audioSettingsComp->setSize (500, 520);

    DialogWindow::LaunchOptions o;
    o.content.setOwned (audioSettingsComp);
    o.dialogTitle                   = "Audio Settings";
    o.componentToCentreAround       = (isOnDesktop() && isVisible()) ? this : nullptr;
    o.dialogBackgroundColour        = getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
    o.escapeKeyTriggersCloseButton  = true;
    o.useNativeTitleBar             = true;
    o.resizable                     = false;

     auto* w = o.create();
     auto safeThis = SafePointer<MainHostWindow> (this);

     w->enterModalState (true,
                         ModalCallbackFunction::create
                         ([safeThis] (int)
                         {
                             if (safeThis != nullptr)
                             {
                                 // Only save audio device state if a device is actually active.
                                 // When the target interface is disconnected, createStateXml()
                                 // returns stale JUCE internal state (the fallback device),
                                 // which would permanently clobber the configured target in settings.
                                 if (safeThis->deviceManager.getCurrentAudioDevice() != nullptr)
                                 {
                                     auto audioState = safeThis->deviceManager.createStateXml();

                                     // Following macOS, Curve runs without an input that isn't
                                     // connected but keeps it in its settings: don't lose it here
                                     if (audioState != nullptr && AudioResilienceManager::isFollowingSystemOutput())
                                         if (auto previous = getUserSettings()->getXmlValue ("audioDeviceState"))
                                             keepMissingInputDevice (*audioState, *previous, safeThis->deviceManager);

                                     if (auto* settings = getUserSettings())
                                     {
                                         settings->setValue ("audioDeviceState", audioState.get());
                                         settings->saveIfNeeded();
                                     }
                                 }

                                 if (auto* rm = getResilienceManager())
                                 {
                                     rm->userChangedSettings();
                                     rm->setSuspended (false);
                                 }

                                 if (safeThis->graphHolder != nullptr)
                                 {
                                     safeThis->graphHolder->propagateDeviceSettingsToNodes();
                                 }
                             }
                             else
                             {
                                 if (auto* rm = getResilienceManager())
                                     rm->setSuspended (false);
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
        File firstFile { files[0] };

        if (files.size() == 1 && firstFile.hasFileExtension (PluginGraph::getFilenameSuffix()))
        {
            if (auto* g = graphHolder->graph.get())
            {
                SafePointer<MainHostWindow> parent { this };

                // Saving a scene also updates the others, so say so rather than only offering to save this one
                const auto plan = g->hasChangedSinceSaved() && g->getSceneGroup().isNotEmpty()
                                    ? g->prepareSceneSave (g->getFile(), false) : PluginGraph::SceneSave();

                if (! plan.otherScenes.isEmpty())
                {

                    auto options = MessageBoxOptions::makeOptionsYesNoCancel (
                        MessageBoxIconType::QuestionIcon,
                        "Save changes to \"" + g->getDocumentTitle() + "\"?",
                        PresetSaveDialog::describeSceneUpdate (plan),
                        PresetSaveDialog::getSceneUpdateButtonText (plan),
                        "Discard Changes",
                        "Cancel");

                    // Plain button indices: Save, Discard Changes, Cancel
                    NativeMessageBox::showAsync (options, [parent, firstFile, plan] (int button)
                    {
                        if (parent == nullptr || parent->graphHolder == nullptr || parent->graphHolder->graph == nullptr)
                            return;

                        if (button == 0)
                        {
                            const auto res = parent->graphHolder->graph->saveScene (plan);

                            if (res.failed())
                            {
                                NativeMessageBox::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Save Failed",
                                                                       "Could not save preset:\n" + res.getErrorMessage());
                                return;
                            }
                        }

                        if (button == 0 || button == 1)
                            parent->loadPreset (firstFile);
                    });

                    return;
                }

                g->saveIfNeededAndUserAgreesAsync ([parent, firstFile] (FileBasedDocument::SaveResult r)
                {
                    if (parent == nullptr)
                        return;

                    if (r == FileBasedDocument::savedOk)
                        parent->loadPreset (firstFile);
                });
            }
            return;
        }

        // Check for Calibration / Parametric EQ / Measurement text files (.txt, .csv, .tsv, .eq)
        if (files.size() == 1 && firstFile.hasFileExtension ("txt;csv;tsv;eq"))
        {
            auto profile = AutoEQDataManager::parseCalibrationFile (firstFile);
            if (profile.has_value() && ! profile->bands.empty())
            {
                auto pos = graphHolder->graphPanel != nullptr
                             ? graphHolder->graphPanel->getLocalPoint (this, Point<int> (x, y))
                             : graphHolder->getLocalPoint (this, Point<int> (x, y));
                applyCalibrationProfileAtLocation (*profile, pos);
                return;
            }
        }

        {
            OwnedArray<PluginDescription> typesFound;
            knownPluginList.scanAndAddDragAndDroppedFiles (formatManager, files, typesFound);

            auto pos = graphHolder->graphPanel != nullptr
                         ? graphHolder->graphPanel->getLocalPoint (this, Point<int> (x, y))
                         : graphHolder->getLocalPoint (this, Point<int> (x, y));

            for (int i = 0; i < jmin (5, typesFound.size()); ++i)
                if (auto* desc = typesFound.getUnchecked (i))
                    createPlugin (PluginDescriptionAndPreference { *desc }, pos);
        }
    }
}

// Loads at most this often. Flipping through presets faster than this loads the first and then the
// last, not every one between: plug-ins created and destroyed in quick succession can be destroyed
// with work still queued on the message thread, which some (e.g. Two Notes Genome) then crash running
static constexpr juce::uint32 minPresetLoadIntervalMs = 250;

// While switching quickly, a preset's windows open once it has settled for this long, rather than
// each preset's plug-in editors opening and closing in turn
static constexpr juce::uint32 rapidSwitchingMs = 1000, windowsSettleMs = 500;

void MainHostWindow::loadPreset (juce::File file)
{
    pendingPresetFile = file;

    // A load in progress, e.g. a plug-in running the event loop as it loads, takes this on after
    if (isLoadingPreset || isPresetLoadScheduled)
        return;

    const auto sinceLast = juce::Time::getMillisecondCounter() - lastPresetLoadTime;

    if (lastPresetLoadTime != 0 && sinceLast < minPresetLoadIntervalMs)
    {
        isPresetLoadScheduled = true;

        juce::Timer::callAfterDelay ((int) (minPresetLoadIntervalMs - sinceLast), [safeThis = SafePointer<MainHostWindow> (this)]
        {
            if (safeThis != nullptr)
            {
                safeThis->isPresetLoadScheduled = false;
                safeThis->loadPendingPreset();
            }
        });

        return;
    }

    loadPendingPreset();
}

void MainHostWindow::loadPendingPreset()
{
    const auto file = std::exchange (pendingPresetFile, juce::File());

    if (file == juce::File())
        return;

    const auto now = juce::Time::getMillisecondCounter();
    const bool switchingQuickly = lastPresetLoadTime != 0 && now - lastPresetLoadTime < rapidSwitchingMs;

    {
        const juce::ScopedValueSetter<bool> loading (isLoadingPreset, true);
        loadPresetNow (file, switchingQuickly);
    }

    lastPresetLoadTime = juce::Time::getMillisecondCounter();

    // Asked for while it loaded: the latest, in its turn
    if (pendingPresetFile != juce::File())
        loadPreset (pendingPresetFile);
}

void MainHostWindow::openPresetPluginWindows (bool afterSettling)
{
    if (graphHolder == nullptr || graphHolder->graph == nullptr)
        return;

    graphHolder->graph->setPresetWindowsDeferred (afterSettling);

    if (! afterSettling)
    {
        graphHolder->graph->openPresetPluginWindows();
        return;
    }

    const auto generation = ++windowsOpenGeneration;

    juce::Timer::callAfterDelay ((int) windowsSettleMs, [safeThis = SafePointer<MainHostWindow> (this), generation]
    {
        // Only for the last preset loaded, once nothing else has loaded since
        if (safeThis != nullptr && safeThis->windowsOpenGeneration == generation
            && safeThis->graphHolder != nullptr && safeThis->graphHolder->graph != nullptr)
        {
            safeThis->graphHolder->graph->setPresetWindowsDeferred (false);
            safeThis->graphHolder->graph->openPresetPluginWindows();
        }
    });
}

void MainHostWindow::loadPresetNow (juce::File file, bool switchingQuickly)
{
    ++windowsOpenGeneration; // A preset's windows waiting to open no longer will

    // Another scene with the live graph's structure: its plug-ins are set as it has them, without loading them
    String whyLoadedInFull;

    if (graphHolder != nullptr && graphHolder->graph != nullptr)
    {
        if (auto scene = graphHolder->graph->prepareSceneSwitch (file, &whyLoadedInFull))
        {
            replaceGraph ([this, file, &scene]
            {
                graphHolder->graph->switchScene (file, *scene);

                // As loading does, so the loopback tap follows mute and bypass before audio resumes
                graphHolder->propagateDeviceSettingsToNodes();
            });

            openPresetPluginWindows (switchingQuickly);
            return;
        }
    }

    bool loaded = false;

    replaceGraph ([this, file, &loaded]
    {
        graphHolder->graph->setRestorePluginWindowsOnLoad (false);
        auto result = graphHolder->graph->loadFrom (file, true);
        loaded = result.wasOk();

        if (result.wasOk())
        {
            graphHolder->propagateDeviceSettingsToNodes();
        }
        else
        {
            juce::NativeMessageBox::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Failed to Load Preset",
                "Could not load the preset file:\n" + result.getErrorMessage());
        }
    });

    openPresetPluginWindows (switchingQuickly);

    // A scene of the loaded group that couldn't switch in place: say why, as it's slower
    if (loaded && whyLoadedInFull.isNotEmpty() && graphHolder != nullptr)
        graphHolder->showStatusMessage ("Loaded \"" + file.getFileNameWithoutExtension() + "\" in full, as " + whyLoadedInFull, 8000);
}

void MainHostWindow::restoreUnsavedChanges (const juce::XmlElement& kept, const juce::File& preset)
{
    if (graphHolder == nullptr || graphHolder->graph == nullptr)
        return;

    ++windowsOpenGeneration; // A preset's windows waiting to open no longer will

    {
        const juce::ScopedValueSetter<bool> loading (isLoadingPreset, true);

        replaceGraph ([this, &kept, &preset]
        {
            graphHolder->graph->setRestorePluginWindowsOnLoad (false);
            graphHolder->graph->restoreUnsavedChanges (kept, preset);
            graphHolder->propagateDeviceSettingsToNodes();
        });
    }

    lastPresetLoadTime = juce::Time::getMillisecondCounter();
    openPresetPluginWindows (false);

    // Asked for while it loaded, e.g. a MIDI program change: it replaces the restored changes
    if (pendingPresetFile != juce::File())
    {
        loadPreset (pendingPresetFile);
        return;
    }

    if (graphHolder != nullptr && graphHolder->graph != nullptr)
    {
        const auto name = graphHolder->graph->getFile().existsAsFile()
                            ? "\"" + graphHolder->graph->getFile().getFileNameWithoutExtension() + "\""
                            : juce::String ("an untitled preset");
        graphHolder->showStatusMessage ("Restored the unsaved changes to " + name + ".", 8000);
    }
}

void MainHostWindow::newPreset()
{
    if (graphHolder == nullptr || graphHolder->graph == nullptr)
        return;

    auto startNewPreset = [safeThis = SafePointer<MainHostWindow> (this)]
    {
        if (auto* w = safeThis.getComponent())
        {
            // A preset still to load from switching quickly, or windows still to open for one, would
            // replace the new preset, or open over it
            w->pendingPresetFile = juce::File();
            ++w->windowsOpenGeneration;

            if (w->graphHolder != nullptr && w->graphHolder->graph != nullptr)
                w->graphHolder->graph->setPresetWindowsDeferred (false);

            w->replaceGraph ([w]
            {
                w->graphHolder->graph->newDocument();
                w->graphHolder->propagateDeviceSettingsToNodes();
            });
        }
    };

    // Settings changed inside a plug-in don't mark the preset as changed, so always ask,
    // unless there's nothing yet beyond the default audio nodes
    auto& audioGraph = graphHolder->graph->graph;
    const auto isDefaultGraph = audioGraph.getConnections().empty()
                             && std::all_of (audioGraph.getNodes().begin(), audioGraph.getNodes().end(), [] (auto* node)
                                {
                                    return PluginGraph::isConfiguredByAudioSettings (*node->getProcessor());
                                });

    if (isDefaultGraph)
    {
        startNewPreset();
        return;
    }

    juce::Process::makeForegroundProcess();

    const auto current = graphHolder->graph->getFile();
    const auto message = current.existsAsFile()
                           ? "This removes every node from the graph. Anything changed since \"" + current.getFileNameWithoutExtension()
                               + "\" was last saved, including settings inside plug-ins, will be lost."
                           : String ("This removes every node from the graph. Your current setup hasn't been saved as a preset and will be lost.");

    auto options = MessageBoxOptions()
                       .withIconType (MessageBoxIconType::WarningIcon)
                       .withTitle ("Start a New Preset?")
                       .withMessage (message)
                       .withButton ("Save As Preset...")
                       .withButton ("Discard and Start New")
                       .withButton ("Cancel")
                       .withAssociatedComponent ((isOnDesktop() && isVisible()) ? this : nullptr);

    messageBox = AlertWindow::showScopedAsync (options, [safeThis = SafePointer<MainHostWindow> (this), startNewPreset] (int result)
    {
        // Buttons are numbered 1, 2, then 0 for the last
        if (result == 1)
        {
            if (auto* w = safeThis.getComponent())
                w->saveAsPreset();
        }
        else if (result == 2)
        {
            startNewPreset();
        }
    });
}

void MainHostWindow::replaceGraph (const std::function<void()>& replace)
{
    // Load into a running device, so the new graph is prepared and checked for idle release normally
    if (auto* idle = getIdleReleaseController())
        idle->resumeIfReleased();

    // Restored afterwards rather than cleared: Audio Settings suspends it while open, and a
    // preset can load meanwhile (a default preset, or MIDI program change)
    bool wasResilienceSuspended = false;

    if (auto* rm = getResilienceManager())
    {
        wasResilienceSuspended = rm->isSuspendedNow();
        rm->setSuspended (true);
    }

    if (graphHolder != nullptr && graphHolder->graph != nullptr)
    {
        const bool hasActiveDevice = (deviceManager.getCurrentAudioDevice() != nullptr);

        // Smoothly fade out active audio before modifying the graph
        if (hasActiveDevice)
        {
            graphHolder->startPresetTransition();

            for (int i = 0; i < 6 && ! graphHolder->isTransitionFadedOut(); ++i)
                juce::Thread::sleep (5);
        }

        auto& audioGraph = graphHolder->graph->graph;
        audioGraph.suspendProcessing (true);

        replace();

        audioGraph.suspendProcessing (false);

        // Smoothly fade in the new graph's audio
        if (hasActiveDevice)
            graphHolder->endPresetTransition();
    }

    if (auto* rm = getResilienceManager())
    {
        rm->setSuspended (wasResilienceSuspended);
    }
}

void MainHostWindow::saveAsPreset()
{
    if (graphHolder == nullptr || graphHolder->graph == nullptr)
        return;

    juce::Process::makeForegroundProcess();

    auto* dialogComp = new PresetSaveDialog (*graphHolder->graph);
    dialogComp->setSize (520, 554);

    DialogWindow::LaunchOptions o;
    o.content.setOwned (dialogComp);
    o.dialogTitle                   = "Save Preset";
    o.componentToCentreAround       = (isOnDesktop() && isVisible()) ? this : nullptr;
    o.dialogBackgroundColour        = getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
    o.escapeKeyTriggersCloseButton  = true;
    o.useNativeTitleBar             = true;
    o.resizable                     = false;

    auto* w = o.launchAsync();
    juce::Component::SafePointer<juce::DialogWindow> safeWindow (w);
    dialogComp->setCloseCallback ([safeWindow]
    {
        if (safeWindow != nullptr)
        {
            safeWindow->exitModalState (0);
            safeWindow->setVisible (false);
        }
    });
}

void MainHostWindow::showOnlineCalibrationDialogForCreation (Point<int> pos)
{
    auto auDesc = AUNBandEQConverter::findAUNBandEQ (knownPluginList);
    if (! auDesc.has_value())
    {
        juce::NativeMessageBox::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "AUNBandEQ Not Found",
            "Apple's Parametric EQ (AUNBandEQ) audio unit was not found on this system.");
        return;
    }

    if (graphHolder == nullptr || graphHolder->graph == nullptr)
        return;

    auto& pluginGraph = *(graphHolder->graph);

    double normX = 0.5;
    double normY = 0.5;
    if (graphHolder->graphPanel != nullptr)
    {
        normX = juce::jlimit (0.05, 0.95, (double) pos.x / (double) graphHolder->graphPanel->getWidth());
        normY = juce::jlimit (0.05, 0.95, (double) pos.y / (double) graphHolder->graphPanel->getHeight());
    }

    String errorMessage;
    auto instance = formatManager.createPluginInstance (*auDesc,
                                                        pluginGraph.graph.getSampleRate(),
                                                        pluginGraph.graph.getBlockSize(),
                                                        errorMessage);
    if (instance == nullptr)
    {
        juce::NativeMessageBox::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Failed to Create Parametric EQ",
            errorMessage);
        return;
    }

    instance->enableAllBuses();
    auto node = pluginGraph.graph.addNode (std::move (instance));
    if (node == nullptr)
        return;

    node->properties.set ("x", normX);
    node->properties.set ("y", normY);
    node->properties.set ("useARA", false);
    pluginGraph.changed();

    AudioProcessorGraph::NodeID newNodeId = node->nodeID;

    juce::Process::makeForegroundProcess();

    auto* dialogComp = new OnlineCalibrationDialog (OnlineCalibrationDialog::Mode::createNewNode);
    dialogComp->setSize (760, 520);

    DialogWindow::LaunchOptions o;
    o.content.setOwned (dialogComp);
    o.dialogTitle                   = "Online Headphone Calibration - Create EQ";
    o.componentToCentreAround       = (isOnDesktop() && isVisible()) ? this : nullptr;
    o.dialogBackgroundColour        = getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
    o.escapeKeyTriggersCloseButton  = true;
    o.useNativeTitleBar             = true;
    o.resizable                     = true;

    o.launchAsync();
    SafePointer<MainHostWindow> safeThis (this);

    // Instant Audition: update newly created node each time a profile loads
    dialogComp->setAuditionCallback ([safeThis, newNodeId] (const CalibrationProfile& profile)
    {
        if (safeThis == nullptr || safeThis->graphHolder == nullptr || safeThis->graphHolder->graph == nullptr)
            return;

        auto targetNode = safeThis->graphHolder->graph->graph.getNodeForId (newNodeId);
        if (targetNode != nullptr && targetNode->getProcessor() != nullptr)
        {
            AUNBandEQConverter::applyProfileToAUNBandEQ (targetNode->getProcessor(), profile);
            safeThis->graphHolder->graph->changed();
        }
    });

    dialogComp->setCompletionCallback ([safeThis, newNodeId] (bool confirmed, const std::optional<CalibrationProfile>& finalProfile)
    {
        if (safeThis != nullptr && safeThis->graphHolder != nullptr && safeThis->graphHolder->graph != nullptr)
        {
            if (! confirmed)
            {
                // User cancelled: delete the newly created node
                safeThis->graphHolder->graph->removeNode (newNodeId);
                safeThis->graphHolder->graph->changed();
            }
            else if (finalProfile.has_value())
            {
                auto targetNode = safeThis->graphHolder->graph->graph.getNodeForId (newNodeId);
                if (targetNode != nullptr && targetNode->getProcessor() != nullptr)
                {
                    if (finalProfile->name.isNotEmpty())
                        targetNode->properties.set ("customNodeName", finalProfile->name);
                    AUNBandEQConverter::applyProfileToAUNBandEQ (targetNode->getProcessor(), *finalProfile);
                    safeThis->graphHolder->graph->changed();
                }
            }
        }
    });
}

void MainHostWindow::showOnlineCalibrationDialogForNode (AudioProcessorGraph::Node* node)
{
    if (node == nullptr || node->getProcessor() == nullptr)
        return;

    AudioProcessorGraph::NodeID nodeID = node->nodeID;

    // Snapshot initial processor state for rollback on Cancel
    juce::MemoryBlock initialPluginState;
    node->getProcessor()->getStateInformation (initialPluginState);

    juce::Process::makeForegroundProcess();

    auto* dialogComp = new OnlineCalibrationDialog (OnlineCalibrationDialog::Mode::configureExistingNode);
    dialogComp->setSize (760, 520);

    DialogWindow::LaunchOptions o;
    o.content.setOwned (dialogComp);
    o.dialogTitle                   = "Online Headphone Calibration - Configure EQ";
    o.componentToCentreAround       = (isOnDesktop() && isVisible()) ? this : nullptr;
    o.dialogBackgroundColour        = getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
    o.escapeKeyTriggersCloseButton  = true;
    o.useNativeTitleBar             = true;
    o.resizable                     = true;

    o.launchAsync();
    SafePointer<MainHostWindow> safeThis (this);

    // Instant Audition: update existing node each time a profile loads
    dialogComp->setAuditionCallback ([safeThis, nodeID] (const CalibrationProfile& profile)
    {
        if (safeThis == nullptr || safeThis->graphHolder == nullptr || safeThis->graphHolder->graph == nullptr)
            return;

        auto targetNode = safeThis->graphHolder->graph->graph.getNodeForId (nodeID);
        if (targetNode != nullptr && targetNode->getProcessor() != nullptr)
        {
            AUNBandEQConverter::applyProfileToAUNBandEQ (targetNode->getProcessor(), profile);
            safeThis->graphHolder->graph->changed();
        }
    });

    dialogComp->setCompletionCallback ([safeThis, nodeID, initialPluginState] (bool confirmed, const std::optional<CalibrationProfile>& finalProfile)
    {
        if (safeThis != nullptr && safeThis->graphHolder != nullptr && safeThis->graphHolder->graph != nullptr)
        {
            auto targetNode = safeThis->graphHolder->graph->graph.getNodeForId (nodeID);
            if (targetNode != nullptr && targetNode->getProcessor() != nullptr)
            {
                if (! confirmed)
                {
                    // User cancelled: restore original node configuration and update GUI with smooth crossfade
                    safeThis->graphHolder->startPresetTransition();
                    AUNBandEQConverter::restoreAUNBandEQState (targetNode->getProcessor(), initialPluginState);
                    safeThis->graphHolder->graph->changed();
                    safeThis->graphHolder->endPresetTransition();
                }
                else if (finalProfile.has_value())
                {
                    safeThis->graphHolder->startPresetTransition();
                    if (finalProfile->name.isNotEmpty())
                        targetNode->properties.set ("customNodeName", finalProfile->name);
                    AUNBandEQConverter::applyProfileToAUNBandEQ (targetNode->getProcessor(), *finalProfile);
                    safeThis->graphHolder->graph->changed();
                    safeThis->graphHolder->endPresetTransition();
                }
            }
        }
    });
}

void MainHostWindow::applyCalibrationProfileAtLocation (const CalibrationProfile& profile, Point<int> pos)
{
    if (graphHolder == nullptr || graphHolder->graph == nullptr)
        return;

    auto& pluginGraph = *(graphHolder->graph);

    // 1. Check if dropped directly on an existing AUNBandEQ node
    AudioProcessorGraph::Node::Ptr targetNode = nullptr;
    if (graphHolder->graphPanel != nullptr)
    {
        targetNode = graphHolder->graphPanel->getNodeAt (pos);
        if (targetNode != nullptr && ! AUNBandEQConverter::isAUNBandEQ (targetNode->getProcessor()))
            targetNode = nullptr; // Only apply EQ directly if dropped onto an EQ node
    }

    if (targetNode != nullptr)
    {
        graphHolder->startPresetTransition();
        AUNBandEQConverter::applyProfileToAUNBandEQ (targetNode->getProcessor(), profile);
        if (profile.name.isNotEmpty())
            targetNode->properties.set ("customNodeName", profile.name);
        pluginGraph.changed();
        graphHolder->endPresetTransition();
        return;
    }

    // 2. Otherwise create a new AUNBandEQ node at pos
    auto auDesc = AUNBandEQConverter::findAUNBandEQ (knownPluginList);
    if (! auDesc.has_value())
    {
        juce::NativeMessageBox::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Parametric EQ Not Found",
            "Apple's Parametric EQ (AUNBandEQ) audio unit was not found on this system.");
        return;
    }

    double normX = 0.5, normY = 0.5;
    if (graphHolder->graphPanel != nullptr && graphHolder->graphPanel->getWidth() > 0)
    {
        normX = juce::jlimit (0.05, 0.95, (double) pos.x / (double) graphHolder->graphPanel->getWidth());
        normY = juce::jlimit (0.05, 0.95, (double) pos.y / (double) graphHolder->graphPanel->getHeight());
    }

    String errorMessage;
    auto instance = formatManager.createPluginInstance (*auDesc,
                                                        pluginGraph.graph.getSampleRate(),
                                                        pluginGraph.graph.getBlockSize(),
                                                        errorMessage);
    if (instance != nullptr)
    {
        instance->enableAllBuses();
        auto newNode = pluginGraph.graph.addNode (std::move (instance));
        if (newNode != nullptr)
        {
            newNode->properties.set ("x", normX);
            newNode->properties.set ("y", normY);
            newNode->properties.set ("useARA", false);
            if (profile.name.isNotEmpty())
                newNode->properties.set ("customNodeName", profile.name);
            AUNBandEQConverter::applyProfileToAUNBandEQ (newNode->getProcessor(), profile);
            pluginGraph.changed();
        }
    }
}

void MainHostWindow::importCalibrationFileForCreation (Point<int> pos)
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Import Calibration / EQ File",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.txt;*.csv;*.tsv;*.eq");

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    SafePointer<MainHostWindow> safeThis (this);

    chooser->launchAsync (flags, [safeThis, chooser, pos] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile() && safeThis != nullptr)
        {
            auto profile = AutoEQDataManager::parseCalibrationFile (file);
            if (profile.has_value())
            {
                safeThis->applyCalibrationProfileAtLocation (*profile, pos);
            }
            else
            {
                juce::NativeMessageBox::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Invalid Calibration File",
                    "Could not parse parametric EQ filters or frequency response measurement points from this file.");
            }
        }
    });
}

void MainHostWindow::importCalibrationFileForNode (juce::AudioProcessorGraph::Node* node)
{
    if (node == nullptr)
        return;

    auto chooser = std::make_shared<juce::FileChooser> (
        "Import Calibration / EQ File",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.txt;*.csv;*.tsv;*.eq");

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    SafePointer<MainHostWindow> safeThis (this);
    auto nodeId = node->nodeID;

    chooser->launchAsync (flags, [safeThis, chooser, nodeId] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile() && safeThis != nullptr && safeThis->graphHolder != nullptr && safeThis->graphHolder->graph != nullptr)
        {
            auto profile = AutoEQDataManager::parseCalibrationFile (file);
            if (profile.has_value())
            {
                auto& pluginGraph = *(safeThis->graphHolder->graph);
                if (auto* targetNode = pluginGraph.graph.getNodeForId (nodeId))
                {
                    safeThis->graphHolder->startPresetTransition();
                    AUNBandEQConverter::applyProfileToAUNBandEQ (targetNode->getProcessor(), *profile);
                    if (profile->name.isNotEmpty())
                        targetNode->properties.set ("customNodeName", profile->name);
                    pluginGraph.changed();
                    safeThis->graphHolder->endPresetTransition();
                }
            }
            else
            {
                juce::NativeMessageBox::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Invalid Calibration File",
                    "Could not parse parametric EQ filters or frequency response measurement points from this file.");
            }
        }
    });
}

void MainHostWindow::showMidiMappingsWindow()
{
    if (graphHolder == nullptr || graphHolder->graph == nullptr)
        return;

    if (midiMappingsWindow == nullptr)
        midiMappingsWindow = std::make_unique<MidiMappingsWindow> (*graphHolder->graph);

    juce::Process::makeForegroundProcess();
    midiMappingsWindow->setVisible (true);
    midiMappingsWindow->toFront (true);
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
    // A Dock icon first, while it's open, if that's set, so it can be found again. Adding one hands
    // the focus to the Dock and back, so the editor's shown once that's done, or it'd be left behind
    // other apps' windows
    DockIconManager::setEditorOpen (true, [safeThis = SafePointer<MainHostWindow> (this)]
    {
        if (safeThis != nullptr)
            safeThis->showNow();
    });
}

void MainHostWindow::showNow()
{
    juce::Process::makeForegroundProcess();

    if (! isOnDesktop())
        addToDesktop (getDesktopWindowStyleFlags());

    // Only restore the saved position when reopening; if the editor is already
    // showing (e.g. a Dock click), keep it where the user has moved it.
    if (! isVisible())
    {
        restoreWindowStateFromString (getAppProperties().getUserSettings()->getValue ("mainWindowPos"));

        auto totalBounds = Desktop::getInstance().getDisplays().getTotalBounds (true);
        if (! totalBounds.isEmpty() && ! totalBounds.intersects (getBounds()))
        {
            centreWithSize (getWidth(), getHeight());
        }
    }

    setVisible (true);
    toFront (true);
}

void MainHostWindow::hideWindow()
{
    getAppProperties().getUserSettings()->setValue ("mainWindowPos", getWindowStateAsString());
    getAppProperties().getUserSettings()->saveIfNeeded();
    setVisible (false);
    DockIconManager::setEditorOpen (false);
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