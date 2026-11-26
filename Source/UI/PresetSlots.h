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
#include "PresetSlotsCore.h"

juce::ApplicationProperties& getAppProperties();

//==============================================================================
/** The keys (1-9 and 0, then with Option, then with Shift) and MIDI program numbers that select
    presets, as assigned to them, with the rest following the presets' alphabetical order (see
    PresetSlotsCore for the rule).

    Assignments are kept in Curve's settings, by preset name, rather than in the presets: a key
    then belongs to one preset, so two can't claim it. Renaming or deleting a preset in Curve
    keeps them up to date; one renamed in the Finder goes back to automatic.
*/
namespace PresetSlots
{
    enum class Kind { key, program };

    inline int getNumSlots (Kind k)                 { return k == Kind::key ? 30 : 128; }
    inline const char* getSettingsKey (Kind k)      { return k == Kind::key ? "presetKeys" : "presetPrograms"; }

    inline std::map<int, std::string> load (Kind k)
    {
        std::map<int, std::string> assigned;

        if (auto* settings = getAppProperties().getUserSettings())
            for (const auto& line : juce::StringArray::fromLines (settings->getValue (getSettingsKey (k))))
                if (const auto tab = line.indexOfChar ('\t'); tab > 0)
                    assigned[line.substring (0, tab).getIntValue()] = line.substring (tab + 1).toStdString();

        return assigned;
    }

    inline void save (Kind k, const std::map<int, std::string>& assigned)
    {
        if (auto* settings = getAppProperties().getUserSettings())
        {
            juce::StringArray lines;

            for (const auto& [slot, name] : assigned)
                lines.add (juce::String (slot) + "\t" + juce::String (name));

            settings->setValue (getSettingsKey (k), lines.joinIntoString ("\n"));
            settings->saveIfNeeded();
        }
    }

    inline juce::String getName (const juce::File& preset)   { return preset.getFileNameWithoutExtension(); }

    /** Each preset's key, in the order given (alphabetical), or -1 if it has none: the automatic
        ones run down the order the presets are shown in, with each scene group's scenes together.
        groups has each preset's group, empty for none or for a group of one. */
    inline std::vector<int> resolveKeys (const juce::Array<juce::File>& presets, const juce::StringArray& groups)
    {
        std::vector<std::string> names, groupNames;

        for (int i = 0; i < presets.size(); ++i)
        {
            names.push_back (getName (presets[i]).toStdString());
            groupNames.push_back (groups[i].toStdString());
        }

        return PresetSlotsCore::resolveInOrderShown (names, groupNames, load (Kind::key), getNumSlots (Kind::key));
    }

    /** Each preset's program number, in the order given (alphabetical), or -1 if it has none. The
        automatic ones follow that order, as program changes always have. */
    inline std::vector<int> resolve (Kind k, const juce::Array<juce::File>& presets)
    {
        std::vector<std::string> names;

        for (const auto& f : presets)
            names.push_back (getName (f).toStdString());

        return PresetSlotsCore::resolve (names, load (k), getNumSlots (k));
    }

    /** The preset a slot selects, if any. */
    inline juce::File findPreset (Kind k, int slot, const juce::Array<juce::File>& presets)
    {
        const auto slots = resolve (k, presets);

        for (size_t i = 0; i < slots.size(); ++i)
            if (slots[i] == slot)
                return presets[(int) i];

        return {};
    }

    /** The slot assigned to a preset, or -1 if it's automatic. */
    inline int getAssignedSlot (Kind k, const juce::File& preset)
    {
        for (const auto& [slot, name] : load (k))
            if (juce::String (name) == getName (preset))
                return slot;

        return -1;
    }

    /** Gives a preset a slot, taking it from any other; -1 makes it automatic again. */
    inline void assign (Kind k, const juce::File& preset, int slot)
    {
        auto assigned = load (k);
        PresetSlotsCore::assign (assigned, getName (preset).toStdString(), slot);
        save (k, assigned);
    }

    inline void presetRenamed (const juce::File& from, const juce::File& to)
    {
        for (auto k : { Kind::key, Kind::program })
        {
            auto assigned = load (k);
            PresetSlotsCore::rename (assigned, getName (from).toStdString(), getName (to).toStdString());
            save (k, assigned);
        }
    }

    inline void presetRemoved (const juce::File& preset)
    {
        for (auto k : { Kind::key, Kind::program })
        {
            auto assigned = load (k);
            PresetSlotsCore::remove (assigned, getName (preset).toStdString());
            save (k, assigned);
        }
    }

    //==============================================================================
    /** A key slot's key: 0-9 are 1-9 and 0, 10-19 the same with Option, 20-29 with Shift. */
    inline int getKeyBank (int slot)                { return slot / 10; } // 0 none, 1 Option, 2 Shift
    inline char getKeyChar (int slot)               { return slot % 10 == 9 ? '0' : (char) ('1' + slot % 10); }

    inline int getKeySlot (int bank, char keyChar)
    {
        if (keyChar < '0' || keyChar > '9' || bank < 0 || bank > 2)
            return -1;

        return bank * 10 + (keyChar == '0' ? 9 : keyChar - '1');
    }

    inline juce::String describeKey (int slot)
    {
        if (slot < 0 || slot >= 30)
            return {};

        const auto key = juce::String::charToString (getKeyChar (slot));

        switch (getKeyBank (slot))
        {
            case 1:  return juce::String::fromUTF8 ("\xe2\x8c\xa5") + key; // ⌥
            case 2:  return juce::String::fromUTF8 ("\xe2\x87\xa7") + key; // ⇧
            default: return key;
        }
    }

    /** As controllers number them, from 1. MIDI sends this less one. */
    inline juce::String describeProgram (int slot)  { return slot >= 0 ? juce::String (slot + 1) : juce::String(); }
}
