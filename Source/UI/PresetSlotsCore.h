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

#include <map>
#include <set>
#include <string>
#include <vector>

//==============================================================================
/** Which preset each key, or each MIDI program number, selects: the rule, without JUCE, so it can
    be tested on its own (see PresetSlots for where the assignments are kept).

    A slot is a key (0 to 29: 1-9 and 0, then with Option, then with Shift) or a program number
    (0 to 127). A preset assigned one has it; the others take the free slots in their order, which
    is alphabetical, so a preset without one gets the key it would have had before keys could be
    assigned, if nothing's been assigned since. An assignment names a preset that isn't there, e.g.
    one deleted in the Finder, keeps its slot free for it, so the others don't move up into it.
*/
namespace PresetSlotsCore
{
    /** The slot of each of the presets, in their order, or -1 for one with none (there are more
        presets than slots). */
    inline std::vector<int> resolve (const std::vector<std::string>& names,
                                     const std::map<int, std::string>& assigned,
                                     int numSlots)
    {
        std::vector<int> slots (names.size(), -1);
        std::set<int> taken;
        std::map<std::string, int> slotOfName;

        for (const auto& [slot, name] : assigned)
            if (slot >= 0 && slot < numSlots && ! name.empty())
            {
                taken.insert (slot);
                slotOfName.emplace (name, slot);
            }

        int next = 0;

        for (size_t i = 0; i < names.size(); ++i)
        {
            if (auto it = slotOfName.find (names[i]); it != slotOfName.end())
            {
                slots[i] = it->second;
                slotOfName.erase (it); // A name twice has its slot once
                continue;
            }

            while (next < numSlots && taken.count (next) > 0)
                ++next;

            if (next < numSlots)
                slots[i] = next++;
        }

        return slots;
    }

    /** The order presets are shown in, as indices into theirs (alphabetical): each scene group's
        scenes together, where its first scene comes. A preset's group is empty if it has none. */
    inline std::vector<size_t> groupedOrder (const std::vector<std::string>& groups)
    {
        std::vector<size_t> order;
        std::set<std::string> placed;

        for (size_t i = 0; i < groups.size(); ++i)
        {
            if (groups[i].empty())
                order.push_back (i);
            else if (placed.insert (groups[i]).second)
                for (size_t j = i; j < groups.size(); ++j)
                    if (groups[j] == groups[i])
                        order.push_back (j);
        }

        return order;
    }

    /** As resolve(), but with the automatic slots following the order shown (see groupedOrder()), so
        they run in order down the list. The slots are still given in the presets' own order. */
    inline std::vector<int> resolveInOrderShown (const std::vector<std::string>& names,
                                                 const std::vector<std::string>& groups,
                                                 const std::map<int, std::string>& assigned,
                                                 int numSlots)
    {
        const auto order = groupedOrder (groups);
        std::vector<std::string> shown;

        for (auto i : order)
            shown.push_back (names[i]);

        const auto shownSlots = resolve (shown, assigned, numSlots);
        std::vector<int> slots (names.size(), -1);

        for (size_t k = 0; k < order.size(); ++k)
            slots[order[k]] = shownSlots[k];

        return slots;
    }

    /** Gives a preset a slot, taking it from any other, and taking any other slot from the preset.
        A slot below 0 makes the preset's automatic again. */
    inline void assign (std::map<int, std::string>& assigned, const std::string& name, int slot)
    {
        for (auto it = assigned.begin(); it != assigned.end();)
            it = it->second == name ? assigned.erase (it) : std::next (it);

        if (slot >= 0)
            assigned[slot] = name;
    }

    /** A preset renamed keeps its slot; one removed gives it up. */
    inline void rename (std::map<int, std::string>& assigned, const std::string& from, const std::string& to)
    {
        if (from == to)
            return;

        // Onto the name of one that's gone, which kept its slot: that slot's given up, so a name has
        // one slot, the renamed preset's
        bool fromHasSlot = false;

        for (const auto& entry : assigned)
            fromHasSlot = fromHasSlot || entry.second == from;

        if (fromHasSlot)
            for (auto it = assigned.begin(); it != assigned.end();)
                it = it->second == to ? assigned.erase (it) : std::next (it);

        for (auto& entry : assigned)
            if (entry.second == from)
                entry.second = to;
    }

    inline void remove (std::map<int, std::string>& assigned, const std::string& name)
    {
        assign (assigned, name, -1);
    }
}
