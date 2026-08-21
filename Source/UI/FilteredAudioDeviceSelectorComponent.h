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

//==============================================================================
/**
    A wrapper around juce::AudioDeviceSelectorComponent that filters internal
    virtual loopback devices (such as "Curve Loopback Tap") out of the input and
    output device selection drop-down lists to prevent internal feedback loops.
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

    void childrenChanged() override
    {
        juce::AudioDeviceSelectorComponent::childrenChanged();
        filterLoopbackItems();
    }

    void parentHierarchyChanged() override
    {
        juce::AudioDeviceSelectorComponent::parentHierarchyChanged();
        filterLoopbackItems();
    }

    void visibilityChanged() override
    {
        juce::AudioDeviceSelectorComponent::visibilityChanged();
        if (isVisible())
            filterLoopbackItems();
    }

    void filterLoopbackItems()
    {
        filterComponent (this);
    }

private:
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
