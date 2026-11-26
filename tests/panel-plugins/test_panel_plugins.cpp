// Checks the order and names of the plug-ins the menu bar panel lists.
// Build and run: tests/panel-plugins/run.sh

#include "../../Source/UI/PanelPlugins.h"

#include <cstdio>

namespace
{
using namespace Curve::PanelPlugins;

int failures = 0;

void check (const char* name, bool ok)
{
    std::printf ("%-4s %s\n", ok ? "ok" : "FAIL", name);
    failures += ok ? 0 : 1;
}

std::string labels (const std::vector<Entry>& entries)
{
    std::string s;

    for (const auto& e : entries)
        s += (s.empty() ? "" : ", ") + e.label;

    return s;
}

bool is (const std::vector<Entry>& entries, const std::string& expected)
{
    const auto actual = labels (entries);

    if (actual != expected)
        std::printf ("     got: %s\n", actual.c_str());

    return actual == expected;
}
}

int main()
{
    // Loopback -> Crossfeed -> EQ -> Output, added to the graph in a different order
    {
        const std::vector<Node> nodes { { 4, "Audio Output", false, 0.5, 0.9 },
                                        { 3, "EQ", true, 0.5, 0.6 },
                                        { 1, "Loopback", false, 0.5, 0.1 },
                                        { 2, "Crossfeed", true, 0.5, 0.3 } };
        const std::vector<Edge> edges { { 1, 2 }, { 2, 3 }, { 3, 4 } };

        check ("a chain is listed in signal order, without the I/O", is (list (nodes, edges), "Crossfeed, EQ"));
    }

    // The multi-output example: four EQs, one after Crossfeed
    {
        const std::vector<Node> nodes { { 1, "Loopback", false, 0.5, 0.1 },
                                        { 2, "AUNBandEQ", true, 0.1, 0.4 },
                                        { 3, "AUNBandEQ", true, 0.3, 0.4 },
                                        { 4, "AUNBandEQ", true, 0.6, 0.4 },
                                        { 5, "Headphone Crossfeed", true, 0.9, 0.35 },
                                        { 6, "AUNBandEQ", true, 0.9, 0.6 },
                                        { 7, "Audio Output", false, 0.5, 0.9 } };
        const std::vector<Edge> edges { { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 }, { 5, 6 },
                                        { 2, 7 }, { 3, 7 }, { 4, 7 }, { 6, 7 } };

        check ("parallel branches go by depth, then down, then across, and repeated names are numbered",
               is (list (nodes, edges), "Headphone Crossfeed, AUNBandEQ 1, AUNBandEQ 2, AUNBandEQ 3, AUNBandEQ 4"));
    }

    // An instrument with no inputs, and an effect not yet connected
    {
        const std::vector<Node> nodes { { 1, "Synth", true, 0.2, 0.2 },
                                        { 2, "Reverb", true, 0.2, 0.5 },
                                        { 3, "Delay", true, 0.8, 0.1 } };
        const std::vector<Edge> edges { { 1, 2 } };

        check ("unconnected nodes come first, by position", is (list (nodes, edges), "Delay, Synth, Reverb"));
    }

    // A loop, which the graph refuses, mustn't hang or crash
    {
        const std::vector<Node> nodes { { 1, "A", true, 0.5, 0.1 }, { 2, "B", true, 0.5, 0.2 } };
        const std::vector<Edge> edges { { 1, 2 }, { 2, 1 } };

        check ("a loop still lists every node", list (nodes, edges).size() == 2);
    }

    check ("an empty graph lists nothing", list ({}, {}).empty());

    std::printf ("\n%s\n", failures == 0 ? "All panel plug-in list tests passed" : "Some panel plug-in list tests FAILED");
    return failures == 0 ? 0 : 1;
}
