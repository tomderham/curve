// Tests PresetSlotsCore: which preset each key, or program number, selects.

#include "PresetSlotsCore.h"
#include <cstdio>
#include <cstdlib>

static int failures = 0;

static void expect (bool ok, const char* what)
{
    if (! ok)
    {
        std::printf ("FAILED: %s\n", what);
        ++failures;
    }
}

int main()
{
    using namespace PresetSlotsCore;
    const std::vector<std::string> names { "Acoustic", "Clean", "Crunch", "Lead" };

    // Nothing assigned: in order, as before keys could be assigned
    expect ((resolve (names, {}, 30) == std::vector<int> { 0, 1, 2, 3 }), "automatic slots follow the order");

    // An assigned one keeps its slot; the others fill the free ones in order
    std::map<int, std::string> assigned;
    assign (assigned, "Lead", 0);
    expect ((resolve (names, assigned, 30) == std::vector<int> { 1, 2, 3, 0 }), "assigned slot, others fill in");

    // Assigning a taken slot takes it from the other; assigning again moves the preset's
    assign (assigned, "Clean", 0);
    expect (assigned.size() == 1 && assigned[0] == "Clean", "a slot has one preset");
    assign (assigned, "Clean", 5);
    expect (assigned.size() == 1 && assigned[5] == "Clean", "a preset has one slot");
    expect ((resolve (names, assigned, 30) == std::vector<int> { 0, 5, 1, 2 }), "others skip an assigned slot");

    // Back to automatic
    assign (assigned, "Clean", -1);
    expect (assigned.empty(), "automatic again");

    // A preset that isn't there keeps its slot free, so the others don't move into it
    assigned = { { 1, "Gone" } };
    expect ((resolve (names, assigned, 30) == std::vector<int> { 0, 2, 3, 4 }), "a missing preset's slot stays free");

    // Renamed keeps it, removed gives it up
    assigned = { { 7, "Lead" } };
    rename (assigned, "Lead", "Solo");
    expect (assigned.size() == 1 && assigned[7] == "Solo", "renamed keeps its slot");
    remove (assigned, "Solo");
    expect (assigned.empty(), "removed gives it up");

    // More presets than slots: the last have none, and an assigned one past them still has its own
    std::vector<std::string> many;
    for (int i = 0; i < 5; ++i)
        many.push_back ("P" + std::to_string (i));

    assigned = { { 0, "P4" } };
    expect ((resolve (many, assigned, 3) == std::vector<int> { 1, 2, -1, -1, 0 }), "more presets than slots");

    // Slots out of range are ignored
    assigned = { { 40, "Clean" } };
    expect ((resolve (names, assigned, 30) == std::vector<int> { 0, 1, 2, 3 }), "out-of-range slot ignored");

    // Grouped: a group's scenes together where its first comes, and automatic keys down that order
    {
        const std::vector<std::string> presets { "A", "B", "C", "D", "E" };
        const std::vector<std::string> groups  { "",  "g", "",  "g", ""  };
        expect ((groupedOrder (groups) == std::vector<size_t> { 0, 1, 3, 2, 4 }), "a group's scenes together");
        expect ((resolveInOrderShown (presets, groups, {}, 30) == std::vector<int> { 0, 1, 3, 2, 4 }), "keys run down the order shown");

        const std::map<int, std::string> lead { { 0, "E" } };
        expect ((resolveInOrderShown (presets, groups, lead, 30) == std::vector<int> { 1, 2, 4, 3, 0 }), "assigned, others down the order shown");
        expect ((groupedOrder ({ "x", "", "x", "y", "y" }) == std::vector<size_t> { 0, 2, 1, 3, 4 }), "several groups");
    }

    if (failures == 0)
        std::printf ("All preset slot tests passed\n");

    return failures == 0 ? 0 : 1;
}
