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

#pragma once

#include <JuceHeader.h>
#include "DefaultPresets.h"
#include "AudioResilienceManager.h"

//==============================================================================
/** The Default Presets window, opened from Audio Settings: one row per output interface,
    connected or with a default saved, each with a menu of presets. */
class DefaultPresetsComponent final : public juce::Component
{
public:
    DefaultPresetsComponent()
    {
        intro.setText ("When macOS switches to an interface, Curve loads its default preset. "
                       "Interfaces set to None keep the preset that's loaded.",
                       juce::dontSendNotification);
        intro.setJustificationType (juce::Justification::topLeft);
        intro.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (intro);

        loadAtLaunchToggle.setButtonText ("Also load at launch");
        loadAtLaunchToggle.setTooltip ("When Curve starts, load the default preset of the interface macOS is using, instead of the preset used last.");
        loadAtLaunchToggle.setToggleState (DefaultPresets::shouldLoadAtLaunch(), juce::dontSendNotification);
        loadAtLaunchToggle.onClick = [this] { DefaultPresets::setLoadAtLaunch (loadAtLaunchToggle.getToggleState()); };
        addAndMakeVisible (loadAtLaunchToggle);

        viewport.setViewedComponent (&rowsHolder, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        buildRows();
        setSize (520, juce::jmin (480, introHeight + 12 + (int) rows.size() * rowHeight + 12 + 24 + 16));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 12);
        intro.setBounds (r.removeFromTop (introHeight));
        r.removeFromTop (8);
        loadAtLaunchToggle.setBounds (r.removeFromBottom (24));
        r.removeFromBottom (8);
        viewport.setBounds (r);

        const int width = viewport.getMaximumVisibleWidth();
        rowsHolder.setSize (width, (int) rows.size() * rowHeight);

        for (size_t i = 0; i < rows.size(); ++i)
        {
            auto row = juce::Rectangle<int> (0, (int) i * rowHeight, width, rowHeight).reduced (0, 3);
            rows[i]->label.setBounds (row.removeFromLeft (width * 45 / 100));
            row.removeFromLeft (8);
            rows[i]->menu.setBounds (row);
        }
    }

private:
    static constexpr int rowHeight = 30, introHeight = 40;
    static constexpr int noneId = 1, firstPresetId = 2, missingPresetId = 100000;

    struct Row
    {
        juce::String uid, name;
        juce::Label label;
        juce::ComboBox menu;
    };

    juce::Label intro;
    juce::ToggleButton loadAtLaunchToggle;
    juce::Viewport viewport;
    juce::Component rowsHolder;
    std::vector<std::unique_ptr<Row>> rows;
    juce::Array<juce::File> presetFiles;

    void buildRows()
    {
        presetFiles = PluginGraph::getPresetFiles();
        const auto assignments = DefaultPresets::getAssignments();

        // Connected interfaces first, in the order the Output menu lists them
        for (const auto& device : getMacOSOutputDevices())
        {
            if (device.uid.isEmpty() || device.name.containsIgnoreCase ("Curve Loopback"))
                continue;

            addRow (device.uid, device.name, device.isDefault ? " (in use)" : "");
        }

        // Then disconnected interfaces that have a default, so it can be seen and cleared
        for (const auto& a : assignments)
        {
            const bool isListed = std::any_of (rows.begin(), rows.end(), [&] (const auto& r) { return r->uid == a.deviceUID; });
            if (! isListed)
                addRow (a.deviceUID, a.deviceName, " (not connected)");
        }
    }

    void addRow (const juce::String& uid, const juce::String& name, const juce::String& suffix)
    {
        auto row = std::make_unique<Row>();
        row->uid = uid;
        row->name = name;
        row->label.setText (name + suffix, juce::dontSendNotification);
        row->label.setTooltip (name + suffix);

        row->menu.addItem ("None", noneId);
        for (int i = 0; i < presetFiles.size(); ++i)
            row->menu.addItem (presetFiles[i].getFileNameWithoutExtension(), firstPresetId + i);

        const auto assigned = DefaultPresets::getPresetFileName (uid);
        int selectedId = noneId;

        if (assigned.isNotEmpty())
        {
            const auto index = presetFiles.indexOf (DefaultPresets::getPresetFile (assigned));
            if (index >= 0)
            {
                selectedId = firstPresetId + index;
            }
            else
            {
                // Deleted or renamed outside Curve: show it, so it's clear why nothing loads
                row->menu.addSeparator();
                row->menu.addItem (juce::File (assigned).getFileNameWithoutExtension() + " (not found)", missingPresetId);
                selectedId = missingPresetId;
            }
        }

        row->menu.setSelectedId (selectedId, juce::dontSendNotification);

        auto* r = row.get();
        row->menu.onChange = [this, r]
        {
            const auto id = r->menu.getSelectedId();
            if (id == noneId)
                DefaultPresets::setPreset (r->uid, r->name, {});
            else if (id >= firstPresetId && id < firstPresetId + presetFiles.size())
                DefaultPresets::setPreset (r->uid, r->name, presetFiles[id - firstPresetId].getFileName());
        };

        rowsHolder.addAndMakeVisible (row->label);
        rowsHolder.addAndMakeVisible (row->menu);
        rows.push_back (std::move (row));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DefaultPresetsComponent)
};
