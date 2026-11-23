/*
  ==============================================================================

    Curve
    Copyright (c) Thomas Derham

    The modifications for Curve are licensed to you solely under the terms of the 
    AGPLv3 license terms.

    CURVE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
    WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "AudioResilienceManager.h"
#include "DefaultPresetsComponent.h"

//==============================================================================
/**
    A wrapper around juce::AudioDeviceSelectorComponent that filters internal
    virtual loopback devices (such as "Curve Loopback Tap") out of the input and
    output device selection drop-down lists to prevent internal feedback loops,
    and places the "Always enforce sample rate" option cleanly between Sample Rate
    and Audio Buffer Size.

    Above JUCE's controls it adds "Use output interface selected in macOS" (follow
    mode, see AudioResilienceManager) and "Force macOS system output to this interface",
    which has nothing to do while following, so it's greyed out then.
*/
class FilteredAudioDeviceSelectorComponent : public juce::AudioDeviceSelectorComponent,
                                             private juce::Timer
{
public:
    using juce::AudioDeviceSelectorComponent::AudioDeviceSelectorComponent;

    FilteredAudioDeviceSelectorComponent (juce::AudioDeviceManager& dm,
                                          int minInputChannels, int maxInputChannels,
                                          int minOutputChannels, int maxOutputChannels,
                                          bool showMidiInputOptions, bool showMidiOutputSelector,
                                          bool showChannelsAsStereoPairs, bool hideAdvancedOptionsWithButton)
        : juce::AudioDeviceSelectorComponent (dm, minInputChannels, maxInputChannels,
                                              minOutputChannels, maxOutputChannels,
                                              showMidiInputOptions, showMidiOutputSelector,
                                              showChannelsAsStereoPairs, hideAdvancedOptionsWithButton)
    {
        enforceRateToggle.setButtonText ("Always enforce sample rate");
        enforceRateToggle.setTooltip ("When disabled, Curve adapts when other applications change the hardware sample rate instead of forcing it back.");

        if (auto* settings = getUserSettings())
            enforceRateToggle.setToggleState (settings->getBoolValue ("enforceSampleRate", false), juce::dontSendNotification);

        enforceRateToggle.onClick = [this]
        {
            auto isEnforced = enforceRateToggle.getToggleState();
            if (auto* settings = getUserSettings())
            {
                settings->setValue ("enforceSampleRate", isEnforced);
                settings->saveIfNeeded();
            }

            if (auto* rm = getResilienceManager())
            {
                rm->setAlwaysEnforceSampleRate (isEnforced);
                rm->userChangedSettings();
            }
        };

        followToggle.setButtonText ("Use output interface selected in macOS");
        followToggle.setTooltip ("Curve's output switches whenever you choose a different output in macOS: in System Settings, Control Center or the menu bar.");
        followToggle.setToggleState (AudioResilienceManager::isFollowingSystemOutput(), juce::dontSendNotification);
        followToggle.onClick = [this]
        {
            if (auto* settings = getUserSettings())
            {
                settings->setValue ("followSystemOutput", followToggle.getToggleState());
                settings->saveIfNeeded();
            }

            if (followToggle.getToggleState())
                switchToFollowedDevice();

            updateFollowControls();
            resized();
        };

        followWarning.setText ("Curve's output follows macOS. If macOS switches to a device with fewer channels, "
                               "outputs that device doesn't have are silent.",
                               juce::dontSendNotification);
        followWarning.setFont (juce::FontOptions (13.0f));
        followWarning.setJustificationType (juce::Justification::topLeft);
        followWarning.setMinimumHorizontalScale (1.0f);
        followWarning.setBorderSize ({ 0, 2, 0, 0 });
        followWarning.setColour (juce::Label::textColourId,
                                 getLookAndFeel().findColour (juce::Label::textColourId).withAlpha (0.7f));

        defaultPresetsButton.setButtonText ("Default Presets for each output interface...");
        defaultPresetsButton.setTooltip ("Choose a preset for Curve to load when macOS switches to each interface.");
        defaultPresetsButton.onClick = [this] { showDefaultPresets(); };

        forceToggle.setButtonText ("Force macOS system output to this interface");
        forceToggle.setTooltip ("Keeps macOS's output set to Curve's output interface, so Interface Loopback captures all system audio. "
                                "Not used while Curve follows the interface selected in macOS.");
        if (auto* settings = getUserSettings())
            forceToggle.setToggleState (settings->getBoolValue ("autoSyncSystemOutput", true), juce::dontSendNotification);
        forceToggle.onClick = [this]
        {
            if (auto* settings = getUserSettings())
            {
                settings->setValue ("autoSyncSystemOutput", forceToggle.getToggleState());
                settings->saveIfNeeded();
            }
        };

        // Picking a device returns it to the rate and buffer size it last used, as follow mode does
        rememberedSettings = AudioResilienceManager::getAllRememberedDeviceSettings();
        if (auto* device = deviceManager.getCurrentAudioDevice())
            lastDeviceName = deviceNameAtOpen = device->getName();
        deviceWatcher.onChange = [this] { handleDeviceChange(); };
        deviceManager.addChangeListener (&deviceWatcher);

        // The resilience manager is suspended while this is open, so follow macOS from here
        juce::Component::SafePointer<FilteredAudioDeviceSelectorComponent> safeThis (this);
        defaultOutputNotifier = createMacOSDefaultOutputChangeNotifier ([safeThis]
        {
            if (safeThis != nullptr && safeThis->followToggle.getToggleState())
                safeThis->switchToFollowedDevice();
        });

        addAndMakeVisible (followToggle);
        addChildComponent (followWarning);
        addChildComponent (defaultPresetsButton);
        addAndMakeVisible (forceToggle);
        updateFollowControls();

        filterLoopbackItems();
        startTimer (100);
    }

    ~FilteredAudioDeviceSelectorComponent() override
    {
        defaultOutputNotifier.reset();
        deviceManager.removeChangeListener (&deviceWatcher);
        stopTimer();

        // JUCE's panel owns these, so hand them back before it deletes them
        returnAdoptedControls();

        // Followed onto another interface while open: load its default preset now, once Audio
        // Settings has saved the new device (the preset's channels are sized from it) and closed
        // (so plug-in windows the preset opens don't appear over it)
        if (followedDeviceToLoadPresetFor.isNotEmpty() && followedDeviceToLoadPresetFor != deviceNameAtOpen)
            juce::MessageManager::callAsync ([name = followedDeviceToLoadPresetFor] { loadDefaultPresetForOutputDevice (name); });
    }

    void timerCallback() override
    {
        filterLoopbackItems();
        updateFollowControls();

        if (isLayoutStale())
            resized();

    }

    void resized() override
    {
        if (isAdjustingLayout)
            return;

        isAdjustingLayout = true;
        returnAdoptedControls();
        juce::AudioDeviceSelectorComponent::resized();
        layoutControls();
        isAdjustingLayout = false;
    }

    void childBoundsChanged (juce::Component* child) override
    {
        if (isAdjustingLayout)
            return;

        juce::AudioDeviceSelectorComponent::childBoundsChanged (child);
    }

    void filterLoopbackItems()
    {
        filterComponent (this);
    }

private:
    juce::ToggleButton enforceRateToggle;
    juce::ToggleButton followToggle, forceToggle;
    juce::TextButton defaultPresetsButton;

    void showDefaultPresets()
    {
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (new DefaultPresetsComponent());
        o.dialogTitle = "Default Presets";
        o.componentToCentreAround = getTopLevelComponent();
        o.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }
    juce::Label followWarning;
    std::unique_ptr<MacOSDefaultOutputChangeNotifierBase> defaultOutputNotifier;

    struct DeviceWatcher : juce::ChangeListener
    {
        std::function<void()> onChange;
        void changeListenerCallback (juce::ChangeBroadcaster*) override { if (onChange) onChange(); }
    };
    DeviceWatcher deviceWatcher;

    // Own copy while the dialog is open: JUCE first opens a newly picked device at the old
    // device's rate, and the resilience manager records that before it can be corrected here
    std::map<juce::String, AudioResilienceManager::DeviceSettings> rememberedSettings;
    juce::String lastDeviceName, deviceNameAtOpen, followedDeviceToLoadPresetFor;
    juce::Component::SafePointer<juce::ComboBox> hookedOutputMenu;

    void handleDeviceChange()
    {
        auto* device = deviceManager.getCurrentAudioDevice();
        if (device == nullptr || ! device->isPlaying())
            return;

        const auto name = device->getName();
        const auto current = AudioResilienceManager::DeviceSettings::of (*device);

        if (name == lastDeviceName)
        {
            rememberedSettings[name] = current; // the user changed the rate, buffer size or outputs
            return;
        }

        // With Always enforce sample rate, the previous interface's rate carries over, as in switchOutputDevice()
        double enforcedRate = 0.0;
        if (AudioResilienceManager::isSampleRateEnforcedSetting())
            if (auto previous = rememberedSettings.find (lastDeviceName); previous != rememberedSettings.end())
                enforcedRate = previous->second.sampleRate;

        lastDeviceName = name;
        auto it = rememberedSettings.find (name);
        if (it == rememberedSettings.end() || it->second.sampleRate <= 0.0)
        {
            rememberedSettings[name] = current;

            if (enforcedRate <= 0.0)
                return;

            it = rememberedSettings.find (name);
        }

        auto wanted = it->second;
        if (enforcedRate > 0.0)
            wanted.sampleRate = enforcedRate;

        if (std::abs (wanted.sampleRate - current.sampleRate) < 0.001
            && (wanted.bufferSize <= 0 || wanted.bufferSize == current.bufferSize)
            && (wanted.outputChannels.isZero() || wanted.outputChannels == current.outputChannels))
            return;

        juce::Component::SafePointer<FilteredAudioDeviceSelectorComponent> safeThis (this);
        juce::MessageManager::callAsync ([safeThis, name, wanted]
        {
            if (safeThis == nullptr)
                return;

            auto& dm = safeThis->deviceManager;
            auto* device = dm.getCurrentAudioDevice();
            if (device == nullptr || device->getName() != name)
                return;

            juce::AudioDeviceManager::AudioDeviceSetup setup;
            dm.getAudioDeviceSetup (setup);
            setup.sampleRate = wanted.sampleRate;
            if (wanted.bufferSize > 0)
                setup.bufferSize = wanted.bufferSize;
            if (! wanted.outputChannels.isZero())
            {
                setup.outputChannels = wanted.outputChannels;
                setup.useDefaultOutputChannels = false;
            }
            dm.setAudioDeviceSetup (setup, true);
        });
    }
    bool isAdjustingLayout = false;

    // JUCE's output device menu: the combo box the "Output:" label is attached to
    juce::Component* findOutputDeviceMenu (juce::Component* comp)
    {
        for (int i = 0; i < comp->getNumChildComponents(); ++i)
        {
            auto* child = comp->getChildComponent (i);
            if (auto* label = dynamic_cast<juce::Label*> (child))
                if (label->getText().trim() == "Output:" && label->getAttachedComponent() != nullptr)
                    return label->getAttachedComponent();

            if (auto* found = findOutputDeviceMenu (child))
                return found;
        }
        return nullptr;
    }

    void updateFollowControls()
    {
        const bool isFollowing = followToggle.getToggleState();
        forceToggle.setEnabled (! isFollowing);
        followWarning.setVisible (isFollowing);
        defaultPresetsButton.setVisible (isFollowing); // only used while following

        // JUCE can recreate the menu when the device changes, so this runs from the timer too
        if (auto* outputMenu = findOutputDeviceMenu (this))
        {
            if (outputMenu->isEnabled() == isFollowing)
                outputMenu->setEnabled (! isFollowing);

            hookOutputMenu (dynamic_cast<juce::ComboBox*> (outputMenu));
        }
    }

    // Picking a device in JUCE's Output menu would open it with the previous device's sample rate
    // and buffer size. Open it with its own remembered settings instead, or the first time, with
    // the hardware's current rate and all outputs, as follow mode does.
    void hookOutputMenu (juce::ComboBox* outputMenu)
    {
        if (outputMenu == nullptr || outputMenu == hookedOutputMenu.getComponent())
            return;

        hookedOutputMenu = outputMenu;
        auto juceHandler = outputMenu->onChange;

        outputMenu->onChange = [this, outputMenu, juceHandler]
        {
            auto* type = deviceManager.getCurrentDeviceTypeObject();
            const auto name = outputMenu->getText();

            if (outputMenu->getSelectedId() > 0 && type != nullptr && type->getDeviceNames (false).contains (name))
                switchOutputDevice (name);
            else if (juceHandler)
                juceHandler(); // e.g. "<< none >>"
        };
    }

    void switchToFollowedDevice()
    {
        auto name = AudioResilienceManager::getFollowedOutputDeviceName();
        if (name.isNotEmpty() && switchOutputDevice (name))
            followedDeviceToLoadPresetFor = name;
    }

    static void showOpenError (const juce::String& error)
    {
        // As JUCE's own Output menu handler reports it
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                TRANS ("Error when trying to open audio device!"), error);
    }

    /** Returns true if it switched. */
    bool switchOutputDevice (const juce::String& name)
    {
        auto* type = deviceManager.getCurrentDeviceTypeObject();
        if (name.isEmpty() || type == nullptr)
            return false;

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.getAudioDeviceSetup (setup);
        if (setup.outputDeviceName == name || ! type->getDeviceNames (false).contains (name))
            return false;

        setup.outputDeviceName = name;

        // Not the old device's settings: the ones last used on this device, or with nothing
        // remembered, the hardware's current rate (JUCE's choice for 0) and all outputs
        const auto it = rememberedSettings.find (name);
        const auto remembered = it != rememberedSettings.end() ? it->second : AudioResilienceManager::DeviceSettings {};
        if (! (AudioResilienceManager::isSampleRateEnforcedSetting() && setup.sampleRate > 0.0))
            setup.sampleRate = remembered.sampleRate; // else the enforced rate, kept on every interface
        setup.bufferSize = remembered.bufferSize;
        setup.outputChannels = remembered.outputChannels;
        setup.useDefaultOutputChannels = remembered.outputChannels.isZero();

        if (setup.inputDeviceName.isNotEmpty() && ! type->getDeviceNames (true).contains (setup.inputDeviceName))
        {
            setup.inputDeviceName = {};
            setup.useDefaultInputChannels = true;
        }

        const auto error = deviceManager.setAudioDeviceSetup (setup, true);
        if (error.isNotEmpty())
            showOpenError (error);
        return error.isEmpty();
    }

    //==============================================================================
    // Layout. JUCE's settings panel lays out the device menus, channel lists, sample rate and
    // buffer size inside itself and clips them to its own height, so nothing can be inserted
    // between them. After JUCE's layout, its controls are moved into this component, where the
    // follow options go below the Output menu and Always enforce sample rate below Sample Rate.
    // Before each layout they go back, so JUCE always lays them out in its own coordinates.
    struct AdoptedControl
    {
        juce::Component::SafePointer<juce::Component> component;
        juce::Rectangle<int> boundsInPanel, boundsHere;
    };
    std::vector<AdoptedControl> adopted;
    juce::Component::SafePointer<juce::Component> settingsPanel;

    static bool isAttachedLabel (juce::Component* c)
    {
        auto* label = dynamic_cast<juce::Label*> (c);
        return label != nullptr && label->getAttachedComponent() != nullptr;
    }

    juce::Component* findSettingsPanel()
    {
        for (int i = 0; i < getNumChildComponents(); ++i)
        {
            auto* child = getChildComponent (i);
            if (child->getX() == 0 && child->getWidth() == getWidth()
                && dynamic_cast<juce::ListBox*> (child) == nullptr
                && dynamic_cast<juce::Button*> (child) == nullptr
                && dynamic_cast<juce::ComboBox*> (child) == nullptr
                && dynamic_cast<juce::Label*> (child) == nullptr
                && std::none_of (adopted.begin(), adopted.end(), [child] (const auto& a) { return a.component == child; }))
                return child;
        }
        return nullptr;
    }

    void returnAdoptedControls()
    {
        for (auto& a : adopted)
        {
            if (a.component == nullptr || settingsPanel == nullptr || a.component->getParentComponent() != this)
                continue;

            settingsPanel->addChildComponent (a.component.getComponent());
            a.component->setBounds (a.boundsInPanel);
        }
        adopted.clear();

        // Taken away while its controls were out of it (see layoutControls())
        if (settingsPanel != nullptr)
            settingsPanel->setInterceptsMouseClicks (true, true);
    }

    // JUCE also re-lays the panel by itself when the device changes, sometimes after this
    // component's layout has run
    bool isLayoutStale()
    {
        if (settingsPanel != nullptr)
            for (int i = 0; i < settingsPanel->getNumChildComponents(); ++i)
                if (! isAttachedLabel (settingsPanel->getChildComponent (i)))
                    return true;

        for (auto& a : adopted)
            if (a.component != nullptr && a.component->getBounds() != a.boundsHere)
                return true;

        return false;
    }

    void layoutControls()
    {
        settingsPanel = findSettingsPanel();
        auto* panel = settingsPanel.getComponent();
        auto* outputMenu = panel != nullptr ? findOutputDeviceMenu (panel) : nullptr;

        const int x = proportionOfWidth (0.35f);
        const int w = proportionOfWidth (0.6f);
        const int toggleHeight = 22, space = 6;

        if (panel == nullptr || outputMenu == nullptr)
        {
            followToggle.setVisible (false);
            followWarning.setVisible (false);
            defaultPresetsButton.setVisible (false);
            forceToggle.setVisible (false);
            enforceRateToggle.setVisible (false);
            return;
        }

        followToggle.setVisible (true);
        forceToggle.setVisible (true);
        followWarning.setVisible (followToggle.getToggleState());
        defaultPresetsButton.setVisible (followToggle.getToggleState());

        const auto offset = panel->getPosition();
        const int outputRowTop = outputMenu->getY();
        const int outputRowBottom = outputMenu->getBottom() + offset.y;

        // Follow options, below the Output menu
        followToggle.setBounds (x, outputRowBottom + space, w, toggleHeight);
        int y = followToggle.getBottom() + 2;

        if (followWarning.isVisible())
        {
            juce::AttributedString text;
            text.append (followWarning.getText(), followWarning.getFont());
            juce::TextLayout layout;
            layout.createLayout (text, (float) (w - 22 - 4));
            followWarning.setBounds (x + 22, y, w - 22, (int) std::ceil (layout.getHeight()) + 4);
            y = followWarning.getBottom() + 2;
        }

        if (defaultPresetsButton.isVisible())
        {
            defaultPresetsButton.changeWidthToFitText (24);
            defaultPresetsButton.setTopLeftPosition (x + 22, y + 4);
            y = defaultPresetsButton.getBottom() + 2;
        }

        forceToggle.setBounds (x, y + 2, w, toggleHeight);
        const int belowOutputShift = forceToggle.getBottom() + space - outputRowBottom;

        // Move JUCE's controls out of the panel; those below the Output row make room for the above
        juce::Array<juce::Component*> children;
        for (int i = 0; i < panel->getNumChildComponents(); ++i)
            children.add (panel->getChildComponent (i));

        juce::Component* sampleRateMenu = nullptr;
        for (auto* child : children)
            if (auto* label = dynamic_cast<juce::Label*> (child))
                if (label->getText().containsIgnoreCase ("Sample rate"))
                    sampleRateMenu = label->getAttachedComponent();

        const int sampleRateTop = sampleRateMenu != nullptr ? sampleRateMenu->getY() : std::numeric_limits<int>::max();
        const bool showEnforceToggle = sampleRateMenu != nullptr && sampleRateMenu->isVisible();

        for (auto* child : children)
        {
            if (isAttachedLabel (child))
                continue; // follows its component

            auto bounds = child->getBounds();
            const auto boundsInPanel = bounds;
            bounds += offset;

            if (child->getY() > outputRowTop)
                bounds.translate (0, belowOutputShift);

            // Always enforce sample rate goes directly below Sample Rate
            if (showEnforceToggle && child->getY() > sampleRateTop)
                bounds.translate (0, toggleHeight + space);

            addChildComponent (child);
            child->setBounds (bounds);
            adopted.push_back ({ child, boundsInPanel, bounds });
        }

        int contentBottom = forceToggle.getBottom();
        for (auto& a : adopted)
            if (a.component != nullptr && a.component->isVisible())
                contentBottom = juce::jmax (contentBottom, a.component->getBottom());

        if (showEnforceToggle)
        {
            if (enforceRateToggle.getParentComponent() != this)
                addAndMakeVisible (enforceRateToggle);

            enforceRateToggle.setVisible (true);
            enforceRateToggle.setBounds (sampleRateMenu->getX(), sampleRateMenu->getBottom() + space,
                                         sampleRateMenu->getWidth(), toggleHeight);
            contentBottom = juce::jmax (contentBottom, enforceRateToggle.getBottom());
        }
        else
        {
            enforceRateToggle.setVisible (false);
        }

        panel->setInterceptsMouseClicks (false, false);
        panel->toBack();
        followToggle.toFront (false);
        followWarning.toFront (false);
        defaultPresetsButton.toFront (false);
        forceToggle.toFront (false);

        // The rest of JUCE's controls (MIDI inputs, Bluetooth, MIDI output) follow underneath
        int curY = contentBottom + space * 2;
        juce::Array<juce::Component*> below;
        for (int i = 0; i < getNumChildComponents(); ++i)
            below.add (getChildComponent (i));
        std::stable_sort (below.begin(), below.end(), [] (auto* l, auto* r) { return l->getY() < r->getY(); });

        for (auto* child : below)
        {
            const bool isOurs = child == panel || child == &followToggle || child == &followWarning || child == &defaultPresetsButton
                             || child == &forceToggle || child == &enforceRateToggle || isAttachedLabel (child)
                             || std::any_of (adopted.begin(), adopted.end(), [child] (const auto& a) { return a.component == child; });

            if (! isOurs && child->isVisible() && child->getY() >= panel->getY())
            {
                child->setTopLeftPosition (child->getX(), curY);
                curY = child->getBottom() + space;
            }
        }

        setSize (getWidth(), curY + 15);
    }

    void filterComponent (juce::Component* comp)
    {
        if (comp == nullptr)
            return;

        if (auto* combo = dynamic_cast<juce::ComboBox*> (comp))
        {
            if (combo->isPopupActive())
                return;

            if (auto* rootMenu = combo->getRootMenu())
            {
                juce::PopupMenu filteredMenu;
                juce::PopupMenu::MenuItemIterator iterator (*rootMenu);
                bool hasFiltered = false;

                while (iterator.next())
                {
                    const auto& item = iterator.getItem();
                    if (item.text.containsIgnoreCase ("Curve Loopback"))
                    {
                        hasFiltered = true;
                    }
                    else
                    {
                        filteredMenu.addItem (item);
                    }
                }

                if (hasFiltered)
                {
                    *rootMenu = filteredMenu;
                    combo->repaint();
                }
            }
        }

        for (int i = 0; i < comp->getNumChildComponents(); ++i)
            filterComponent (comp->getChildComponent (i));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilteredAudioDeviceSelectorComponent)
};
