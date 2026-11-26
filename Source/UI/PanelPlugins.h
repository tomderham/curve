/*
==============================================================================
   Curve - Plug-ins as the menu bar panel lists them
   In signal order, inputs first, with repeated names told apart. Free of JUCE
   so it can be tested on its own (tests/panel-plugins).
==============================================================================
*/

#pragma once

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Curve
{
namespace PanelPlugins
{
    struct Node
    {
        unsigned id = 0;
        std::string name;    // As the graph shows it: its custom name, if it has one
        bool listed = true;  // False for nodes the panel leaves out, e.g. the graph's I/O
        double x = 0.5, y = 0.5; // Where it is on the graph, each 0 to 1
    };

    using Edge = std::pair<unsigned, unsigned>; // Source and destination node IDs

    struct Entry
    {
        unsigned id = 0;
        std::string label;
    };

    /** How many connections the longest path from a node with no inputs takes to reach each node.
        Nodes in a feedback loop, which the graph doesn't allow, count from where the loop is entered. */
    inline std::map<unsigned, int> getDepths (const std::vector<Node>& nodes, const std::vector<Edge>& edges)
    {
        std::map<unsigned, int> depth;

        for (const auto& n : nodes)
            depth[n.id] = 0;

        // Relaxing every edge once per node gives the longest paths in an acyclic graph
        for (size_t pass = 0; pass < nodes.size(); ++pass)
        {
            bool changed = false;

            for (const auto& [from, to] : edges)
            {
                const auto f = depth.find (from);
                const auto t = depth.find (to);

                if (f != depth.end() && t != depth.end() && t->second < f->second + 1
                    && f->second + 1 <= (int) nodes.size())
                {
                    t->second = f->second + 1;
                    changed = true;
                }
            }

            if (! changed)
                break;
        }

        return depth;
    }

    /** The listed nodes in the order the audio passes through them: by depth, then down the graph,
        then across it. A name used by more than one gets a number, in that order. */
    inline std::vector<Entry> list (const std::vector<Node>& nodes, const std::vector<Edge>& edges)
    {
        const auto depth = getDepths (nodes, edges);

        std::vector<const Node*> listed;

        for (const auto& n : nodes)
            if (n.listed)
                listed.push_back (&n);

        std::stable_sort (listed.begin(), listed.end(), [&] (const Node* a, const Node* b)
        {
            const auto da = depth.at (a->id), db = depth.at (b->id);

            if (da != db)   return da < db;
            if (a->y != b->y) return a->y < b->y;
            return a->x < b->x;
        });

        std::map<std::string, int> uses;

        for (const auto* n : listed)
            ++uses[n->name];

        std::map<std::string, int> numbered;
        std::vector<Entry> entries;

        for (const auto* n : listed)
        {
            auto label = n->name;

            if (uses[n->name] > 1)
                label += " " + std::to_string (++numbered[n->name]);

            entries.push_back ({ n->id, label });
        }

        return entries;
    }
}
}
