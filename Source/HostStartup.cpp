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
   JUCE Website Terms of Service:
  https://juce.com/juce-website-terms-of-service/

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
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>
#include "UI/MainHostWindow.h"
#include "UI/CurveColours.h"
#include "Plugins/InternalPlugins.h"
#include "UI/AudioResilienceManager.h"
#include "UI/IdleReleaseController.h"
#include "Plugins/AudioRecorderNode.h"
#include "UI/GitHubUpdater.h"
#include "UI/LoginItemManager.h"
#include "UI/SystemSleepManager.h"
#include "UI/VolumeKeyManager.h"
#include "UI/DockIconManager.h"
#include "UI/TrayIconController.h"
#include "UI/DefaultPresets.h"
#include "AudioDiagnostics.h"


#if !(JUCE_PLUGINHOST_VST || JUCE_PLUGINHOST_VST3 || JUCE_PLUGINHOST_AU)
#error                                                                         \
    "If you're building the audio plugin host, you probably want to enable VST and/or AU support"
#endif

class PluginScannerSubprocess final : private ChildProcessWorker,
                                      private AsyncUpdater {
public:
  PluginScannerSubprocess() {
    addDefaultFormatsToManager(formatManager);

    watchdogThread = std::thread([this] {
      while (!shouldStopWatchdog.load(std::memory_order_relaxed)) {
        std::unique_lock<std::mutex> lock(watchdogMutex);

        if (!watchdogArmed.load(std::memory_order_acquire)) {
          // Idle: sleep until a scan arms the watchdog
          watchdogCv.wait_for(lock, std::chrono::milliseconds(500), [this] {
            return shouldStopWatchdog.load(std::memory_order_relaxed) ||
                   watchdogArmed.load(std::memory_order_acquire);
          });
          continue;
        }

        auto now = juce::Time::getMillisecondCounter();
        auto remainingMs = static_cast<juce::int32>(
            watchdogDeadline.load(std::memory_order_relaxed) - now);

        if (remainingMs <= 0) {
          // Plugin hung past the scan timeout; exit immediately to avoid a zombie
          std::_Exit(1);
        }

        // Armed: flags are set without the mutex, so cap the wait in case a
        // notification is missed.
        watchdogCv.wait_for(lock,
                            std::chrono::milliseconds(juce::jmin(remainingMs, 100)),
                            [this] {
          return shouldStopWatchdog.load(std::memory_order_relaxed) ||
                 !watchdogArmed.load(std::memory_order_acquire);
        });
      }
    });
  }

  ~PluginScannerSubprocess() override {
    shouldStopWatchdog.store(true, std::memory_order_relaxed);
    watchdogCv.notify_all();
    if (watchdogThread.joinable())
      watchdogThread.join();
  }

  using ChildProcessWorker::initialiseFromCommandLine;

private:
  struct ScopedWatchdogArmer {
    ScopedWatchdogArmer(std::atomic<bool> &armed,
                        std::atomic<juce::uint32> &deadline,
                        std::condition_variable &cv,
                        juce::uint32 timeoutMs = pluginScanTimeoutMs)
        : isArmed(armed), cvRef(cv) {
      deadline.store(juce::Time::getMillisecondCounter() + timeoutMs,
                     std::memory_order_relaxed);
      armed.store(true, std::memory_order_release);
      cvRef.notify_all();
    }

    ~ScopedWatchdogArmer() {
      isArmed.store(false, std::memory_order_release);
      cvRef.notify_all();
    }

    std::atomic<bool> &isArmed;
    std::condition_variable &cvRef;
  };

  void handleMessageFromCoordinator(const MemoryBlock &mb) override {
    if (mb.isEmpty())
      return;

    const std::lock_guard<std::mutex> lock(mutex);

    if (!needsMessageThread(mb)) {
      if (const auto results = doScan(mb); !results.isEmpty()) {
        sendResults(results);
        return;
      }
    }

    pendingBlocks.emplace(mb);
    triggerAsyncUpdate();
  }

  // VST3 modules may touch AppKit as they load, which throws on other threads
  static bool needsMessageThread(const MemoryBlock &block) {
    MemoryInputStream stream{block, false};
    return stream.readString() == VST3PluginFormat::getFormatName();
  }

  void handleConnectionLost() override {
    // Unconditional kernel-level exit so background threads spawned by plugins
    // cannot keep the child process alive
    std::_Exit(0);
  }

  void handleAsyncUpdate() override {
    for (;;) {
      MemoryBlock block;
      {
        const std::lock_guard<std::mutex> lock(mutex);

        if (pendingBlocks.empty())
          return;

        block = pendingBlocks.front();
        pendingBlocks.pop();
      }

      sendResults(doScan(block));
    }
  }

  OwnedArray<PluginDescription> doScan(const MemoryBlock &block) {
    MemoryInputStream stream{block, false};
    const auto formatName = stream.readString();
    const auto identifier = stream.readString();

    const auto matchingFormat = [&]() -> AudioPluginFormat * {
      for (auto *format : formatManager.getFormats())
        if (format->getName() == formatName)
          return format;

      return nullptr;
    }();

    OwnedArray<PluginDescription> results;

    if (matchingFormat != nullptr) {
      ScopedWatchdogArmer armer(watchdogArmed, watchdogDeadline, watchdogCv);

      // An exception left to reach this thread's entry point would end the
      // thread silently, leaving a worker that can't read messages
      try {
        matchingFormat->findAllTypesForFile(results, identifier);
      } catch (...) {
        results.clear();
      }
    }

    return results;
  }

  void sendResults(const OwnedArray<PluginDescription> &results) {
    XmlElement xml("LIST");

    for (const auto &desc : results)
      xml.addChildElement(desc->createXml().release());

    const auto str = xml.toString();
    sendMessageToCoordinator({str.toRawUTF8(), str.getNumBytesAsUTF8()});
  }

  std::mutex mutex;
  std::queue<MemoryBlock> pendingBlocks;
  AudioPluginFormatManager formatManager;

  std::thread watchdogThread;
  std::mutex watchdogMutex;
  std::condition_variable watchdogCv;
  std::atomic<bool> shouldStopWatchdog{false};
  std::atomic<bool> watchdogArmed{false};
  std::atomic<juce::uint32> watchdogDeadline{0};
};

//==============================================================================
class CurveLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CurveLookAndFeel()
    {
        namespace C = Curve::Colours;

        // The system's font, San Francisco, as AppKit's own controls and Curve's menu bar panel use,
        // rather than JUCE's default on macOS, Lucida Grande
        setDefaultSansSerifTypefaceName (".AppleSystemUIFont");

        setColourScheme ({ C::canvas,   // window background
                           C::control,  // widget background
                           C::menu,     // menu background
                           C::edge,     // outline
                           C::text,     // default text
                           C::accent,   // default fill
                           juce::Colours::white, // highlighted text
                           C::accent,   // highlighted fill
                           C::text });  // menu text

        // Where the scheme's colours don't suit: lists are panels rather than controls, scrollbars
        // and tooltips are quiet rather than in the accent, and a focused field shows the accent
        setColour (juce::ListBox::backgroundColourId, C::surface);
        setColour (juce::ScrollBar::thumbColourId, juce::Colours::white.withAlpha (0.25f));
        setColour (juce::TooltipWindow::backgroundColourId, C::menu);
        setColour (juce::TooltipWindow::textColourId, C::text);
        setColour (juce::TooltipWindow::outlineColourId, C::edge);
        setColour (juce::TextEditor::focusedOutlineColourId, C::accent);
        setColour (juce::ComboBox::focusedOutlineColourId, C::accent);
        setColour (juce::ComboBox::buttonColourId, C::control);
        setColour (juce::HyperlinkButton::textColourId, C::link);
        setColour (juce::AlertWindow::backgroundColourId, C::surface);
        setColour (juce::PopupMenu::headerTextColourId, C::dimText);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            const bool isSeparator, const bool isActive,
                            const bool isHighlighted, const bool isTicked,
                            const bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* const textColourToUse) override
    {
        if (isSeparator)
        {
            auto r = area.reduced (8, 0);
            g.setColour (Curve::Colours::edge); // Solid, crisp, visible separator line
            g.fillRect (r.getX(), area.getCentreY(), r.getWidth(), 1);
            return;
        }

        // An item's colour marks the scenes of a group (see TrayIconController::addPresetsToMenu()),
        // as a stripe down its left edge like the Save Preset window's, rather than as its text colour
        juce::LookAndFeel_V4::drawPopupMenuItem (g, area, isSeparator, isActive, isHighlighted, isTicked,
                                                hasSubMenu, text, shortcutKeyText, icon, nullptr);

        // After the highlight, which would cover it
        if (textColourToUse != nullptr)
        {
            g.setColour (*textColourToUse);
            g.fillRoundedRectangle ((float) area.getX() + 1.0f, (float) area.getY() + 4.0f, 2.5f, (float) area.getHeight() - 8.0f, 1.25f);
        }
    }

    // A menu's section headings small and quiet, in capitals, as the menu bar panel's are, so a menu
    // of several sections, e.g. Curve Plugins, isn't made much taller by them
    void getIdealPopupMenuSectionHeaderSizeWithOptions (const juce::String& text, int standardMenuItemHeight,
                                                        int& idealWidth, int& idealHeight,
                                                        const juce::PopupMenu::Options& options) override
    {
        getIdealPopupMenuItemSizeWithOptions (text, false, standardMenuItemHeight, idealWidth, idealHeight, options);
        idealHeight = 19;
    }

    void drawPopupMenuSectionHeaderWithOptions (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                const juce::String& sectionName, const juce::PopupMenu::Options&) override
    {
        g.setColour (findColour (juce::PopupMenu::headerTextColourId));
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (sectionName.toUpperCase(), area.withTrimmedLeft (12).withTrimmedRight (8).withTrimmedBottom (2),
                    juce::Justification::bottomLeft, true);
    }

    //==============================================================================
    // Controls: flat and rounded, with a quiet edge, and the accent for what's on or focused

    static constexpr float corner = 6.0f;

    // No larger than Curve's other text, however tall the button
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::FontOptions (juce::jmin (14.0f, (float) buttonHeight * 0.6f));
    }

    // As switches' text, rather than JUCE's 85% of the box's height, which is larger than the text
    // around it
    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return juce::FontOptions (juce::jmin (13.5f, (float) box.getHeight() * 0.6f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& background,
                               bool isHighlighted, bool isDown) override
    {
        const auto r = button.getLocalBounds().toFloat().reduced (0.5f);
        auto fill = background.withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f);

        if (isDown)
            fill = fill.brighter (0.18f);
        else if (isHighlighted)
            fill = fill.brighter (0.09f);

        g.setColour (fill);
        g.fillRoundedRectangle (r, corner);

        // A coloured button, e.g. a dialog's default action, needs no edge
        if (background == findColour (juce::TextButton::buttonColourId))
        {
            g.setColour (Curve::Colours::edge);
            g.drawRoundedRectangle (r, corner, 1.0f);
        }
    }

    // A switch, rather than a tick box
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool isHighlighted, bool) override
    {
        const auto bounds = button.getLocalBounds().toFloat();
        const float height = 14.0f, width = 24.0f;
        const auto track = juce::Rectangle<float> (bounds.getX() + 2.0f, bounds.getCentreY() - height * 0.5f, width, height);
        const bool on = button.getToggleState();
        const float alpha = button.isEnabled() ? 1.0f : 0.45f;

        g.setColour ((on ? Curve::Colours::accent : Curve::Colours::control.brighter (isHighlighted ? 0.2f : 0.1f)).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (track, height * 0.5f);

        if (! on)
        {
            g.setColour (Curve::Colours::edge.withMultipliedAlpha (alpha));
            g.drawRoundedRectangle (track.reduced (0.5f), height * 0.5f, 1.0f);
        }

        const float knob = height - 4.0f;
        const auto knobArea = juce::Rectangle<float> (knob, knob).withCentre ({ on ? track.getRight() - height * 0.5f : track.getX() + height * 0.5f,
                                                                                track.getCentreY() });
        g.setColour (juce::Colours::white.withAlpha (alpha * (on ? 1.0f : 0.85f)));
        g.fillEllipse (knobArea);

        g.setColour (button.findColour (juce::ToggleButton::textColourId).withMultipliedAlpha (alpha));
        g.setFont (juce::FontOptions (13.5f));
        g.drawFittedText (button.getButtonText(),
                          button.getLocalBounds().withTrimmedLeft (juce::roundToInt (track.getRight()) + 7).withTrimmedRight (2),
                          juce::Justification::centredLeft, 10);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

        g.setColour (box.findColour (juce::ComboBox::backgroundColourId).withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.5f));
        g.fillRoundedRectangle (r, corner);
        g.setColour (box.hasKeyboardFocus (false) ? Curve::Colours::accent : Curve::Colours::edge);
        g.drawRoundedRectangle (r, corner, 1.0f);

        // A chevron
        const auto centre = juce::Point<float> ((float) width - 13.0f, (float) height * 0.5f);
        juce::Path chevron;
        chevron.startNewSubPath (centre.translated (-4.0f, -2.0f));
        chevron.lineTo (centre.translated (0.0f, 2.0f));
        chevron.lineTo (centre.translated (4.0f, -2.0f));

        g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 0.8f : 0.3f));
        g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar& bar, int x, int y, int width, int height, bool isVertical,
                        int thumbStart, int thumbSize, bool isMouseOver, bool isMouseDown) override
    {
        auto thumb = isVertical ? juce::Rectangle<float> ((float) x, (float) thumbStart, (float) width, (float) thumbSize)
                                : juce::Rectangle<float> ((float) thumbStart, (float) y, (float) thumbSize, (float) height);

        // Slim, and wider while in use
        const float across = isMouseOver || isMouseDown ? 7.0f : 5.0f;
        thumb = isVertical ? thumb.withSizeKeepingCentre (across, thumb.getHeight() - 4.0f)
                           : thumb.withSizeKeepingCentre (thumb.getWidth() - 4.0f, across);

        g.setColour (bar.findColour (juce::ScrollBar::thumbColourId).withMultipliedAlpha (isMouseOver || isMouseDown ? 1.6f : 1.0f));
        g.fillRoundedRectangle (thumb, across * 0.5f);
    }

    void fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
    {
        g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) width, (float) height), 5.0f);
    }

    void drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
    {
        if (! editor.isEnabled())
            return;

        const auto focused = editor.hasKeyboardFocus (true) && ! editor.isReadOnly();
        g.setColour (editor.findColour (focused ? juce::TextEditor::focusedOutlineColourId : juce::TextEditor::outlineColourId));
        g.drawRoundedRectangle (juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f), 5.0f, focused ? 1.5f : 1.0f);
    }

    // Tabs as a segmented control: the front one raised, the others plain text
    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool) override
    {
        const auto area = button.getActiveArea().toFloat().reduced (2.0f, 3.0f);

        if (button.isFrontTab())
        {
            g.setColour (Curve::Colours::control.brighter (0.08f));
            g.fillRoundedRectangle (area, corner);
            g.setColour (Curve::Colours::edge);
            g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
        }
        else if (isMouseOver)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (area, corner);
        }

        g.setColour (button.isFrontTab() ? Curve::Colours::text : Curve::Colours::dimText);
        g.setFont (juce::FontOptions (13.0f, button.isFrontTab() ? juce::Font::bold : juce::Font::plain));
        g.drawFittedText (button.getButtonText().trim(), area.toNearestInt(), juce::Justification::centred, 1);
    }

    // Table headers, e.g. the Plug-in Manager's: quiet column names, with an edge below
    void drawTableHeaderBackground (juce::Graphics& g, juce::TableHeaderComponent& header) override
    {
        g.fillAll (Curve::Colours::surface);
        g.setColour (Curve::Colours::edge);
        g.fillRect (header.getLocalBounds().removeFromBottom (1));

        // Between the columns
        for (int i = header.getNumColumns (true) - 1; --i >= 0;)
        {
            const auto x = header.getColumnPosition (i).getRight() - 1;
            g.fillRect (x, 6, 1, header.getHeight() - 12);
        }
    }

    void drawTableHeaderColumn (juce::Graphics& g, juce::TableHeaderComponent&, const juce::String& columnName, int,
                                int width, int height, bool isMouseOver, bool isMouseDown, int columnFlags) override
    {
        if (isMouseDown || isMouseOver)
        {
            g.setColour (juce::Colours::white.withAlpha (isMouseDown ? 0.08f : 0.04f));
            g.fillRect (0, 0, width, height);
        }

        auto area = juce::Rectangle<int> (width, height).reduced (8, 0);

        if ((columnFlags & (juce::TableHeaderComponent::sortedForwards | juce::TableHeaderComponent::sortedBackwards)) != 0)
        {
            const auto arrowArea = area.removeFromRight (10).toFloat().withSizeKeepingCentre (8.0f, 5.0f);
            const bool forwards = (columnFlags & juce::TableHeaderComponent::sortedForwards) != 0;

            juce::Path arrow;
            arrow.addTriangle (arrowArea.getX(), forwards ? arrowArea.getBottom() : arrowArea.getY(),
                               arrowArea.getRight(), forwards ? arrowArea.getBottom() : arrowArea.getY(),
                               arrowArea.getCentreX(), forwards ? arrowArea.getY() : arrowArea.getBottom());
            g.setColour (Curve::Colours::dimText);
            g.fillPath (arrow);
        }

        g.setColour (Curve::Colours::dimText);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawFittedText (columnName, area, juce::Justification::centredLeft, 1);
    }

    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override {}
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}
};

//==============================================================================
class PluginHostApp final : public JUCEApplication, private AsyncUpdater {
public:
  PluginHostApp() = default;

  void initialise(const String &commandLine) override {
    auto scannerSubprocess = std::make_unique<PluginScannerSubprocess>();

    if (scannerSubprocess->initialiseFromCommandLine(commandLine, processUID,
                                                     pluginScanHardLimitMs)) {
      storedScannerSubprocess = std::move(scannerSubprocess);
      return;
    }

    // Early initialization of audio diagnostics on the message thread before any audio hardware or devices start
    AudioDiagnostics::initializeEarly();

    // Text that doesn't fit is cut short, with an ellipsis, rather than squashed narrower
    juce::Font::setDefaultMinimumHorizontalScaleFactor (1.0f);

    customLookAndFeel = std::make_unique<CurveLookAndFeel>();
    juce::LookAndFeel::setDefaultLookAndFeel (customLookAndFeel.get());

    // initialise our settings file..

    PropertiesFile::Options options;
    options.applicationName = "Curve";
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Preferences";

    appProperties.reset(new ApplicationProperties());
    appProperties->setStorageParameters(options);

    lastShutdownClean = appProperties->getUserSettings()->getBoolValue(
        "lastShutdownClean", true);
    appProperties->getUserSettings()->setValue("lastShutdownClean", false);
    appProperties->getUserSettings()->saveIfNeeded();

    SystemSleepManager::applyCurrentSetting();
    VolumeKeyManager::start();

    // create presets folder if it doesn't exist
    auto appDataDir =
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Application Support")
            .getChildFile(JUCEApplication::getInstance()->getApplicationName());
    auto presetsDir = appDataDir.getChildFile("Presets");
    if (!appDataDir.exists())
      appDataDir.createDirectory();
    if (!presetsDir.exists())
      presetsDir.createDirectory();

    mainWindow.reset(new MainHostWindow());

    // Show editor window on first launch; hide initially on subsequent launches
    bool isFirstLaunch = false;
    if (auto *settings = getAppProperties().getUserSettings()) {
      if (!settings->getBoolValue("hasRunBefore", false)) {
        isFirstLaunch = true;
        settings->setValue("hasRunBefore", true);
        settings->saveIfNeeded();
      }
    }

    if (isFirstLaunch) {
      juce::MessageManager::callAsync(
          [weakWin =
               juce::Component::SafePointer<MainHostWindow>(mainWindow.get())] {
            if (auto *w = weakWin.getComponent())
              w->showWindow();
          });
    } else {
      mainWindow->hideWindow();
    }

    // initialize app Github updater
    githubUpdater.reset(new GitHubUpdater());
    githubUpdater->onUpdateAvailable =
        [weakWindow =
             juce::Component::SafePointer<MainHostWindow>(mainWindow.get())](
            const juce::String &ver, const juce::String &url) {
          if (auto *w = weakWindow.getComponent())
            w->showUpdateBanner(ver, url);
        };
    githubUpdater->start("tomderham", "curve", options.applicationName);

    // initialize menu bar controller
    trayIcon.reset(new TrayIconController(*mainWindow, *githubUpdater));

    // optional Dock icon; clicking it shows the editor
    DockIconManager::applyCurrentSetting();
    DockIconManager::setReopenCallback(
        [weakWin =
             juce::Component::SafePointer<MainHostWindow>(mainWindow.get())] {
          if (auto *w = weakWin.getComponent())
            w->showWindow();
        });

    mainWindow->onShowAppMenu = [this](juce::Component *target) {
      if (mainWindow != nullptr && githubUpdater != nullptr) {
        auto menu = TrayIconController::buildAppMenu(*mainWindow,
                                                     *githubUpdater, false, nullptr,
                                                     [this] {
                                                       if (trayIcon != nullptr)
                                                         trayIcon->showPreferences();
                                                     });
        juce::PopupMenu::Options menuOptions;
        if (target != nullptr) {
          menuOptions = menuOptions.withTargetComponent(target);
        } else {
          auto mousePos = juce::Desktop::getInstance().getMousePosition();
          menuOptions = menuOptions.withTargetScreenArea(
              juce::Rectangle<int>(mousePos.x, mousePos.y, 1, 1));
        }

        menu.showMenuAsync(menuOptions);
      }
    };

    // initialize audio resilience manager
    auto &deviceManager = mainWindow->getDeviceManager();
    resilienceManager.reset(new AudioResilienceManager(deviceManager, [] {
      if (auto *app = dynamic_cast<PluginHostApp *>(
              juce::JUCEApplication::getInstance())) {
        if (app->mainWindow != nullptr &&
            app->mainWindow->graphHolder != nullptr) {
          app->mainWindow->graphHolder->propagateDeviceSettingsToNodes();
        }
      }
    }));

    idleReleaseController.reset(new IdleReleaseController(deviceManager, [] {
      // Never release the interface mid-recording
      if (auto *app = dynamic_cast<PluginHostApp *>(
              juce::JUCEApplication::getInstance()))
        if (app->mainWindow != nullptr && app->mainWindow->graphHolder != nullptr &&
            app->mainWindow->graphHolder->graph != nullptr)
          for (auto *node : app->mainWindow->graphHolder->graph->graph.getNodes())
            if (auto *recorder = dynamic_cast<AudioRecorderNode *>(node->getProcessor()))
              if (recorder->isRecording())
                return true;
      return false;
    }, [] {
      // Whether the preset routes the Audio Input node to anything
      using IONode = juce::AudioProcessorGraph::AudioGraphIOProcessor;
      if (auto *app = dynamic_cast<PluginHostApp *>(
              juce::JUCEApplication::getInstance()))
        if (app->mainWindow != nullptr && app->mainWindow->graphHolder != nullptr &&
            app->mainWindow->graphHolder->graph != nullptr) {
          auto &graph = app->mainWindow->graphHolder->graph->graph;
          for (auto &connection : graph.getConnections())
            if (auto *node = graph.getNodeForId(connection.source.nodeID))
              if (auto *io = dynamic_cast<IONode *>(node->getProcessor()))
                if (io->getType() == IONode::audioInputNode)
                  return true;
        }
      return false;
    }, [](bool shouldHold) {
      if (auto *app = dynamic_cast<PluginHostApp *>(
              juce::JUCEApplication::getInstance()))
        if (app->mainWindow != nullptr && app->mainWindow->graphHolder != nullptr) {
          if (shouldHold)
            app->mainWindow->graphHolder->holdOutputUntilFadeIn();
          else
            app->mainWindow->graphHolder->fadeInOutput();
        }
    }));

    commandManager.registerAllCommandsForTarget(mainWindow.get());

    // Defer plugin loading to an async update so instantiation happens once the
    // application event loop is fully initialized.
    triggerAsyncUpdate();
  }

  void handleAsyncUpdate() override {
    File fileToOpen;

    for (int i = 0; i < getCommandLineParameterArray().size(); ++i) {
      fileToOpen = File::getCurrentWorkingDirectory().getChildFile(
          getCommandLineParameterArray()[i]);

      if (fileToOpen.existsAsFile())
        break;
    }

    const bool isOpeningRequestedFile = fileToOpen.existsAsFile();

    if (!fileToOpen.existsAsFile()) {
      RecentlyOpenedFilesList recentFiles;
      recentFiles.restoreFromString(
          getAppProperties().getUserSettings()->getValue(
              "recentFilterGraphFiles"));

      if (recentFiles.getNumFiles() > 0)
        fileToOpen = recentFiles.getFile(0);
    }

    // Following macOS with "Also load at launch": the default preset of the interface macOS is
    // using replaces the preset used last (but not a preset Curve was asked to open)
    if (!isOpeningRequestedFile && AudioResilienceManager::isFollowingSystemOutput() &&
        DefaultPresets::shouldLoadAtLaunch()) {
      juce::String deviceName;
      if (auto *device = mainWindow->getDeviceManager().getCurrentAudioDevice())
        deviceName = device->getName();
      else
        deviceName = AudioResilienceManager::getFollowedOutputDeviceName();

      auto defaultPreset = DefaultPresets::getPresetFile(
          DefaultPresets::getPresetFileName(getMacOSOutputDeviceUID(deviceName)));
      if (defaultPreset.existsAsFile())
        fileToOpen = defaultPreset;
    }

    if (fileToOpen.existsAsFile()) {
      if (!lastShutdownClean) {
        juce::NativeMessageBox::showOkCancelBox(
            juce::MessageBoxIconType::WarningIcon, "Curve Startup Recovery",
            "It looks like Curve didn't shut down cleanly last time. Would you "
            "like to load your last active preset (" +
                fileToOpen.getFileNameWithoutExtension() + ") anyway?",
            nullptr,
            juce::ModalCallbackFunction::create([this, fileToOpen](int result) {
              if (result == 1 && mainWindow != nullptr) // OK / Load
              {
                mainWindow->loadPreset(fileToOpen);
              }
            }));
      } else {
        mainWindow->loadPreset(fileToOpen);
      }
    }

    maybeShowLoginItemFirstRunPrompt();
  }

  void maybeShowLoginItemFirstRunPrompt() {
    auto *settings = getAppProperties().getUserSettings();
    if (settings == nullptr)
      return;

    if (settings->getBoolValue("hasPromptedLoginItem", false)) {
      // macOS is the source of truth: notRegistered is what we see after the
      // user removes Curve from Login Items, so never re-register silently.
      if (settings->getBoolValue("openAtLogin", false) &&
          LoginItemManager::getStatus() ==
              LoginItemManager::Status::notRegistered) {
        settings->setValue("openAtLogin", false);
        settings->saveIfNeeded();
      }

      return;
    }

    // Delay so this doesn't stack on the startup-recovery dialog above.
    juce::Timer::callAfterDelay(1500, [] {
      auto *userSettings = getUserSettings();
      if (userSettings == nullptr ||
          userSettings->getBoolValue("hasPromptedLoginItem", false))
        return;

      juce::NativeMessageBox::showOkCancelBox(
          juce::MessageBoxIconType::QuestionIcon, "Open Curve at Login?",
          "Would you like Curve to open automatically each time you log in? "
          "You can change this later from the tray menu under Settings.",
          nullptr, juce::ModalCallbackFunction::create([](int result) {
            auto *s = getUserSettings();
            if (s == nullptr)
              return;

            if (result == 1 && attemptToEnableLoginItem()) // OK
              juce::NativeMessageBox::showMessageBoxAsync(
                  juce::MessageBoxIconType::InfoIcon, "Open at Login",
                  "Curve will now open automatically at login.");

            s->setValue("hasPromptedLoginItem", true);
            s->saveIfNeeded();
          }));
    });
  }

  void shutdown() override {
    // Scanner subprocesses exit early without initializing appProperties
    if (appProperties != nullptr) {
      appProperties->getUserSettings()->setValue("lastShutdownClean", true);
      appProperties->getUserSettings()->saveIfNeeded();
    }

    trayIcon = nullptr;
    VolumeKeyManager::stop();
    idleReleaseController = nullptr;
    resilienceManager = nullptr;
    githubUpdater = nullptr;
    mainWindow = nullptr;
    appProperties = nullptr;
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    customLookAndFeel = nullptr;
  }

  void suspended() override {}

  void systemRequestedQuit() override {
    if (appProperties != nullptr) {
      appProperties->getUserSettings()->setValue("lastShutdownClean", true);
      appProperties->getUserSettings()->saveIfNeeded();
    }

    if (mainWindow != nullptr)
      mainWindow->tryToQuitApplication();
    else
      JUCEApplicationBase::quit();
  }


  const String getApplicationName() override {
    return ProjectInfo::projectName;
  }
  const String getApplicationVersion() override {
    return ProjectInfo::versionString;
  }
  bool moreThanOneInstanceAllowed() override {
    return getCommandLineParameters().contains(processUID);
  }

  ApplicationCommandManager commandManager;
  std::unique_ptr<ApplicationProperties> appProperties;

  // See loadDefaultPresetForOutputDevice() in DefaultPresets.h
  void loadDefaultPresetFor(const juce::String &deviceName) {
    if (mainWindow == nullptr || !AudioResilienceManager::isFollowingSystemOutput())
      return;

    // Not under an open dialog or menu: a Save Preset window or prompt may be about to save
    // scenes planned for the loaded preset. Load once it's closed, unless a later switch
    // has replaced this one.
    const auto request = ++defaultPresetRequest;
    if (juce::Component::getNumCurrentlyModalComponents() > 0) {
      waitToLoadDefaultPreset(deviceName, request);
      return;
    }

    auto presetFileName =
        DefaultPresets::getPresetFileName(getMacOSOutputDeviceUID(deviceName));
    if (presetFileName.isEmpty())
      return; // no default: keep the loaded preset

    auto showStatus = [this](const juce::String &message) {
      if (mainWindow->graphHolder != nullptr)
        mainWindow->graphHolder->showStatusMessage(message, 5000);
    };

    auto file = DefaultPresets::getPresetFile(presetFileName);
    if (!file.existsAsFile()) {
      showStatus("The default preset for " + deviceName + ", " +
                 juce::File(presetFileName).getFileNameWithoutExtension() +
                 ", wasn't found, so the current preset is still loaded.");
      return;
    }

    // Loaded as if picked from the preset menu, which doesn't ask about unsaved changes either:
    // the changed flag isn't reliable enough to guard with (a device switch sets it, and changes
    // made inside a plug-in don't)
    if (mainWindow->graphHolder != nullptr &&
        mainWindow->graphHolder->graph != nullptr &&
        mainWindow->graphHolder->graph->getFile() == file)
      return;

    mainWindow->loadPreset(file);
    showStatus("Loaded " + file.getFileNameWithoutExtension() +
               ", the default preset for " + deviceName + ".");
  }

  void waitToLoadDefaultPreset(const juce::String &deviceName, juce::uint32 request) {
    juce::Timer::callAfterDelay(300, [this, deviceName, request] {
      if (request != defaultPresetRequest)
        return; // a later switch replaced it

      if (juce::Component::getNumCurrentlyModalComponents() > 0) {
        waitToLoadDefaultPreset(deviceName, request);
        return;
      }

      // Still on that interface?
      if (mainWindow != nullptr)
        if (auto *device = mainWindow->getDeviceManager().getCurrentAudioDevice())
          if (device->getName() == deviceName)
            loadDefaultPresetFor(deviceName);
    });
  }

  juce::uint32 defaultPresetRequest = 0;

  AudioResilienceManager *getResilienceManager() const {
    return resilienceManager.get();
  }

  IdleReleaseController *getIdleReleaseController() const {
    return idleReleaseController.get();
  }

private:
  std::unique_ptr<MainHostWindow> mainWindow;
  std::unique_ptr<CurveLookAndFeel> customLookAndFeel;
  std::unique_ptr<PluginScannerSubprocess> storedScannerSubprocess;
  std::unique_ptr<GitHubUpdater> githubUpdater;
  std::unique_ptr<AudioResilienceManager> resilienceManager;
  std::unique_ptr<IdleReleaseController> idleReleaseController;
  std::unique_ptr<TrayIconController> trayIcon;
  bool lastShutdownClean = true;
};

static PluginHostApp &getApp() {
  return *dynamic_cast<PluginHostApp *>(JUCEApplication::getInstance());
}

ApplicationProperties &getAppProperties() { return *getApp().appProperties; }
PropertiesFile *getUserSettings() {
  if (auto *app = dynamic_cast<PluginHostApp *>(JUCEApplication::getInstance()))
    if (app->appProperties != nullptr)
      return app->appProperties->getUserSettings();
  return nullptr;
}
ApplicationCommandManager &getCommandManager() {
  return getApp().commandManager;
}

AudioResilienceManager *getResilienceManager() {
  if (auto *app = dynamic_cast<PluginHostApp *>(JUCEApplication::getInstance()))
    return app->getResilienceManager();
  return nullptr;
}

void loadDefaultPresetForOutputDevice(const juce::String &juceDeviceName) {
  if (auto *app = dynamic_cast<PluginHostApp *>(JUCEApplication::getInstance()))
    app->loadDefaultPresetFor(juceDeviceName);
}

IdleReleaseController *getIdleReleaseController() {
  if (auto *app = dynamic_cast<PluginHostApp *>(JUCEApplication::getInstance()))
    return app->getIdleReleaseController();
  return nullptr;
}

// Application entry point
START_JUCE_APPLICATION(PluginHostApp)
