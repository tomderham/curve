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

//==============================================================================
/**
    A wrapper around juce::AudioDeviceSelectorComponent that filters internal
    virtual loopback devices (such as "Curve Loopback Tap") out of the input and
    output device selection drop-down lists to prevent internal feedback loops,
    and places the "Always enforce sample rate" option cleanly between Sample Rate
    and Audio Buffer Size.
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
                rm->updateTargetSettings();
            }
        };

        filterLoopbackItems();
        startTimer (100);
    }

    ~FilteredAudioDeviceSelectorComponent() override
    {
        stopTimer();
    }

    void timerCallback() override
    {
        filterLoopbackItems();
    }

    void resized() override
    {
        if (isAdjustingLayout)
            return;

        isAdjustingLayout = true;
        juce::AudioDeviceSelectorComponent::resized();
        applyCustomLayout();
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
    bool isAdjustingLayout = false;

    struct SelectorChildren
    {
        juce::Component* settingsComp = nullptr;
        juce::Component* sampleRateComp = nullptr;
        juce::Component* bufferSizeComp = nullptr;
        juce::Label* bufferSizeLabel = nullptr;
        juce::Component* midiInputsList = nullptr;
        juce::Component* bluetoothButton = nullptr;
        juce::Component* midiOutputSelector = nullptr;
    };

    SelectorChildren findSelectorChildren()
    {
        SelectorChildren c;

        for (int i = 0; i < getNumChildComponents(); ++i)
        {
            auto* child = getChildComponent (i);

            if (auto* lb = dynamic_cast<juce::ListBox*> (child))
                c.midiInputsList = lb;
            else if (auto* btn = dynamic_cast<juce::Button*> (child))
            {
                if (btn->getButtonText().containsIgnoreCase ("Bluetooth"))
                    c.bluetoothButton = btn;
            }
            else if (auto* label = dynamic_cast<juce::Label*> (child))
            {
                if (label->getText().containsIgnoreCase ("buffer size"))
                {
                    c.bufferSizeLabel = label;
                    c.bufferSizeComp = label->getAttachedComponent();
                }
                else if (label->getText().containsIgnoreCase ("MIDI Output"))
                {
                    c.midiOutputSelector = label->getAttachedComponent();
                }
            }

            for (int j = 0; j < child->getNumChildComponents(); ++j)
            {
                if (auto* label = dynamic_cast<juce::Label*> (child->getChildComponent (j)))
                {
                    auto text = label->getText();
                    if (text.containsIgnoreCase ("Sample rate"))
                    {
                        c.settingsComp = child;
                        c.sampleRateComp = label->getAttachedComponent();
                    }
                    else if (text.containsIgnoreCase ("buffer size"))
                    {
                        c.bufferSizeLabel = label;
                        c.bufferSizeComp = label->getAttachedComponent();
                    }
                    else if (text.containsIgnoreCase ("MIDI Output"))
                    {
                        c.midiOutputSelector = label->getAttachedComponent();
                    }
                }
            }
        }

        return c;
    }

    void applyCustomLayout()
    {
        auto c = findSelectorChildren();
        if (c.sampleRateComp == nullptr || c.bufferSizeComp == nullptr || c.settingsComp == nullptr)
        {
            enforceRateToggle.setVisible (false);
            return;
        }

        // Reparent controls to this component so settingsComp cannot clip them
        if (enforceRateToggle.getParentComponent() != this)
            addAndMakeVisible (enforceRateToggle);

        if (c.bufferSizeComp->getParentComponent() != this)
            addAndMakeVisible (c.bufferSizeComp);

        if (c.bufferSizeLabel != nullptr && c.bufferSizeLabel->getParentComponent() != this)
            addAndMakeVisible (c.bufferSizeLabel);

        const bool isAdvancedVisible = c.sampleRateComp->isVisible();
        enforceRateToggle.setVisible (isAdvancedVisible);
        c.bufferSizeComp->setVisible (isAdvancedVisible);
        if (c.bufferSizeLabel != nullptr)
            c.bufferSizeLabel->setVisible (isAdvancedVisible);

        const int space = 6;

        if (! isAdvancedVisible)
        {
            int curY = c.settingsComp->getBottom() + space;
            if (c.midiInputsList != nullptr)
            {
                c.midiInputsList->setTopLeftPosition (c.midiInputsList->getX(), curY);
                curY = c.midiInputsList->getBottom() + space;
            }
            if (c.bluetoothButton != nullptr)
            {
                c.bluetoothButton->setTopLeftPosition (c.bluetoothButton->getX(), curY);
                curY = c.bluetoothButton->getBottom() + space;
            }
            if (c.midiOutputSelector != nullptr)
            {
                c.midiOutputSelector->setTopLeftPosition (c.midiOutputSelector->getX(), curY);
                curY = c.midiOutputSelector->getBottom() + space;
            }
            setSize (getWidth(), curY + 15);
            return;
        }

        const int toggleHeight = 22;
        auto srBounds = getLocalArea (c.sampleRateComp->getParentComponent(), c.sampleRateComp->getBounds());

        // 1. Position toggle directly below Sample Rate dropdown
        enforceRateToggle.setBounds (srBounds.getX(),
                                     srBounds.getBottom() + space,
                                     srBounds.getWidth(),
                                     toggleHeight);

        // 2. Position Audio Buffer Size dropdown directly below toggle
        c.bufferSizeComp->setBounds (srBounds.getX(),
                                     enforceRateToggle.getBottom() + space,
                                     srBounds.getWidth(),
                                     c.sampleRateComp->getHeight());

        if (c.bufferSizeLabel != nullptr)
            c.bufferSizeLabel->attachToComponent (c.bufferSizeComp, true);

        // 3. Position MIDI inputs list below Audio Buffer Size
        int curY = c.bufferSizeComp->getBottom() + (space * 2);

        if (c.midiInputsList != nullptr)
        {
            c.midiInputsList->setTopLeftPosition (c.midiInputsList->getX(), curY);
            curY = c.midiInputsList->getBottom() + space;
        }

        // 4. Position bluetooth button
        if (c.bluetoothButton != nullptr)
        {
            c.bluetoothButton->setTopLeftPosition (c.bluetoothButton->getX(), curY);
            curY = c.bluetoothButton->getBottom() + space;
        }

        // 5. Position MIDI output selector
        if (c.midiOutputSelector != nullptr)
        {
            c.midiOutputSelector->setTopLeftPosition (c.midiOutputSelector->getX(), curY);
            curY = c.midiOutputSelector->getBottom() + space;
        }

        // 6. Set total height
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
