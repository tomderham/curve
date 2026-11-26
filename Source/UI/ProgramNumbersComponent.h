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
#include "CurveColours.h"
#include "PresetSlots.h"
#include "../Plugins/PluginGraph.h"

//==============================================================================
/** Which MIDI program change selects each preset, from Preferences > MIDI: each preset's number, its
    own or from its place in the list, as controllers number them, from 1. Set once, so it's here
    rather than with the presets.
*/
class ProgramNumbersComponent final : public juce::Component,
                                      private juce::ListBoxModel
{
public:
    ProgramNumbersComponent()
    {
        note.setText ("Program changes select presets in alphabetical order, unless one is given its own number. "
                      "Numbers are as most controllers show them, from 1; MIDI sends one less.",
                      juce::dontSendNotification);
        note.setFont (juce::FontOptions (12.5f));
        note.setColour (juce::Label::textColourId, Curve::Colours::dimText);
        note.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (note);

        list.setModel (this);
        list.setRowHeight (32);
        list.setOutlineThickness (0);
        addAndMakeVisible (list);

        refresh();
        setSize (460, 480);
    }

    ~ProgramNumbersComponent() override
    {
        list.setModel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Curve::Colours::canvas);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 14);
        note.setBounds (r.removeFromTop (40));
        r.removeFromTop (8);
        list.setBounds (r);
    }

private:
    // A preset and its number
    struct Row final : public juce::Component
    {
        explicit Row (ProgramNumbersComponent& o) : owner (o)
        {
            name.setFont (juce::FontOptions (13.5f));
            name.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (name);

            number.onChange = [this]
            {
                if (updating || number.getSelectedId() == 0)
                    return;

                const auto id = number.getSelectedId();
                PresetSlots::assign (PresetSlots::Kind::program, file, id == automaticId ? -1 : id - firstId);

                // The others' automatic numbers may have moved, after this one's turn has
                juce::Component::SafePointer<ProgramNumbersComponent> safeOwner (&owner);
                juce::MessageManager::callAsync ([safeOwner] { if (safeOwner != nullptr) safeOwner->refresh(); });
            };
            addAndMakeVisible (number);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8, 3);
            number.setBounds (r.removeFromRight (190));
            r.removeFromRight (8);
            name.setBounds (r);
        }

        void show (const juce::File& f, int slot, bool assigned, const std::map<int, juce::String>& users)
        {
            const juce::ScopedValueSetter<bool> svs (updating, true);
            file = f;
            name.setText (f.getFileNameWithoutExtension(), juce::dontSendNotification);

            number.clear (juce::dontSendNotification);
            number.addItem ("Automatic" + (slot >= 0 && ! assigned ? ": " + PresetSlots::describeProgram (slot) : juce::String()), automaticId);
            number.addSeparator();

            for (int p = 0; p < PresetSlots::getNumSlots (PresetSlots::Kind::program); ++p)
            {
                juce::String text = PresetSlots::describeProgram (p);

                // Whose it is now, as giving it to this one takes it from them
                if (auto user = users.find (p); user != users.end() && user->second != name.getText())
                    text << "    " << user->second;

                number.addItem (text, firstId + p);
            }

            number.setSelectedId (assigned ? firstId + slot : automaticId, juce::dontSendNotification);
        }

        static constexpr int automaticId = 1, firstId = 2;

        ProgramNumbersComponent& owner;
        juce::File file;
        juce::Label name;
        juce::ComboBox number;
        bool updating = false;
    };

    void refresh()
    {
        presets = PluginGraph::getPresetFiles();
        slots = PresetSlots::resolve (PresetSlots::Kind::program, presets);
        assigned = PresetSlots::load (PresetSlots::Kind::program);

        users.clear();

        for (size_t i = 0; i < slots.size(); ++i)
            if (slots[i] >= 0)
                users[slots[i]] = presets[(int) i].getFileNameWithoutExtension();

        list.updateContent();

        for (int i = 0; i < presets.size(); ++i)
            if (auto* row = dynamic_cast<Row*> (list.getComponentForRowNumber (i)))
                showRow (*row, i);
    }

    void showRow (Row& row, int index)
    {
        const auto slot = slots[(size_t) index];
        const auto file = presets[index];
        const auto isAssigned = slot >= 0 && assigned.count (slot) > 0
                             && juce::String (assigned.at (slot)) == file.getFileNameWithoutExtension();
        row.show (file, slot, isAssigned, users);
    }

    int getNumRows() override   { return presets.size(); }
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}

    juce::Component* refreshComponentForRow (int index, bool, juce::Component* existing) override
    {
        if (! juce::isPositiveAndBelow (index, presets.size()))
        {
            delete existing;
            return nullptr;
        }

        auto* row = dynamic_cast<Row*> (existing);

        if (row == nullptr)
        {
            delete existing;
            row = new Row (*this);
        }

        showRow (*row, index);
        return row;
    }

    juce::Label note;
    juce::ListBox list;
    juce::Array<juce::File> presets;
    std::vector<int> slots;
    std::map<int, std::string> assigned;
    std::map<int, juce::String> users;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgramNumbersComponent)
};
