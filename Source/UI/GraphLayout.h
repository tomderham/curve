/*
==============================================================================
   Curve - Graph auto-layout
   Arranges nodes into top-to-bottom rows by signal depth, ordering each row to
   reduce wire crossings. Free of JUCE so it can be tested on its own.
==============================================================================
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace Curve
{
namespace GraphLayout
{
    struct NodeInfo
    {
        int width = 0, height = 0;
        bool pinnedToBottom = false; // Kept on the last row, e.g. the audio output
        int anchor = -1;             // Distance from the left edge of the point that lines up
                                     // with the node's neighbours; by default its centre

        double getAnchor() const { return anchor >= 0 ? anchor : width * 0.5; }
    };

    using Edge = std::pair<int, int>; // Source and destination indices into the nodes

    struct Options
    {
        int minRowGap = 20, maxRowGap = 80;
        int nodeGap = 20;
        int areaWidth = 800, areaHeight = 600;
        int margin = 20; // Kept clear around the edges of the area
    };

    struct Centre { double x = 0.0, y = 0.0; };
    struct Extent { int width = 0, height = 0; };

    namespace detail
    {
        // Feedback loops are legal in the graph, but layering needs a DAG
        inline void collectForwardEdges (int node, const std::vector<std::vector<int>>& outputs,
                                         std::vector<int>& state, std::vector<Edge>& forward)
        {
            state[(size_t) node] = 1;

            for (auto next : outputs[(size_t) node])
            {
                if (state[(size_t) next] == 1)
                    continue; // Back edge

                forward.emplace_back (node, next);

                if (state[(size_t) next] == 0)
                    collectForwardEdges (next, outputs, state, forward);
            }

            state[(size_t) node] = 2;
        }

        inline double mean (const std::vector<double>& values, double fallback)
        {
            if (values.empty())
                return fallback;

            double sum = 0.0;

            for (auto v : values)
                sum += v;

            return sum / (double) values.size();
        }
    }

    /** Returns each node's centre. If the nodes don't fit the area, they overflow to
        the right and bottom, keeping the top left visible. The area they need,
        margins included, goes in requiredArea.
    */
    inline std::vector<Centre> arrange (const std::vector<NodeInfo>& nodes,
                                        const std::vector<Edge>& edges,
                                        const Options& options,
                                        Extent* requiredArea = nullptr)
    {
        const auto numNodes = nodes.size();
        std::vector<Centre> result (numNodes);

        if (numNodes == 0)
            return result;

        std::vector<std::vector<int>> outputs (numNodes);
        std::vector<int> numInputs (numNodes, 0);

        for (const auto& e : edges)
        {
            if (e.first == e.second)
                continue;

            auto& out = outputs[(size_t) e.first];

            if (std::find (out.begin(), out.end(), e.second) == out.end())
            {
                out.push_back (e.second);
                ++numInputs[(size_t) e.second];
            }
        }

        // Start from the sources, so that the edges dropped are the ones feeding back
        std::vector<Edge> forward;
        std::vector<int> state (numNodes, 0);

        for (size_t i = 0; i < numNodes; ++i)
            if (numInputs[i] == 0 && state[i] == 0)
                detail::collectForwardEdges ((int) i, outputs, state, forward);

        for (size_t i = 0; i < numNodes; ++i)
            if (state[i] == 0)
                detail::collectForwardEdges ((int) i, outputs, state, forward);

        // Each node's row is its longest path from a source
        std::vector<int> row (numNodes, 0);

        for (size_t pass = 0; pass < numNodes; ++pass)
        {
            bool changed = false;

            for (const auto& e : forward)
            {
                if (row[(size_t) e.second] < row[(size_t) e.first] + 1)
                {
                    row[(size_t) e.second] = row[(size_t) e.first] + 1;
                    changed = true;
                }
            }

            if (! changed)
                break;
        }

        const auto numRows = *std::max_element (row.begin(), row.end()) + 1;

        for (size_t i = 0; i < numNodes; ++i)
            if (nodes[i].pinnedToBottom)
                row[i] = numRows - 1;

        std::vector<std::vector<int>> rows ((size_t) numRows);

        for (size_t i = 0; i < numNodes; ++i)
            rows[(size_t) row[i]].push_back ((int) i);

        // Sets a row's x positions as close to those desired as its order allows,
        // pushing nodes apart where they would overlap
        std::vector<double> x (numNodes, 0.0);

        const auto placeRow = [&] (const std::vector<int>& indices, const std::vector<double>& desired)
        {
            std::vector<double> placed (desired);

            for (size_t i = 1; i < indices.size(); ++i)
            {
                const auto& left = nodes[(size_t) indices[i - 1]];
                const auto& right = nodes[(size_t) indices[i]];
                const auto minGap = left.width - left.getAnchor() + right.getAnchor() + options.nodeGap;
                placed[i] = std::max (placed[i], placed[i - 1] + minGap);
            }

            // Pushing only goes rightwards, so recentre the row on what it wanted
            double shift = 0.0;

            for (size_t i = 0; i < indices.size(); ++i)
                shift += desired[i] - placed[i];

            shift /= (double) indices.size();

            for (size_t i = 0; i < indices.size(); ++i)
                x[(size_t) indices[i]] = placed[i] + shift;
        };

        const auto neighbourPositions = [&] (int node, bool usePredecessors)
        {
            std::vector<double> positions;

            for (const auto& e : forward)
            {
                if (usePredecessors && e.second == node)
                    positions.push_back (x[(size_t) e.first]);
                else if (! usePredecessors && e.first == node)
                    positions.push_back (x[(size_t) e.second]);
            }

            return positions;
        };

        // Sweeping down then up settles the order (barycentre heuristic) and the positions
        const auto sweep = [&] (std::vector<int>& indices, bool usePredecessors)
        {
            std::vector<std::pair<double, int>> order;

            for (auto node : indices)
                order.emplace_back (detail::mean (neighbourPositions (node, usePredecessors), x[(size_t) node]), node);

            std::stable_sort (order.begin(), order.end(),
                              [] (const auto& a, const auto& b) { return a.first < b.first; });

            std::vector<double> desired;

            for (size_t i = 0; i < order.size(); ++i)
            {
                indices[i] = order[i].second;
                desired.push_back (order[i].first);
            }

            placeRow (indices, desired);
        };

        for (auto& r : rows)
        {
            std::vector<double> spaced (r.size());

            for (size_t i = 0; i < r.size(); ++i)
                spaced[i] = (double) i;

            if (! r.empty())
                placeRow (r, spaced);
        }

        for (int iteration = 0; iteration < 4; ++iteration)
        {
            for (size_t i = 1; i < rows.size(); ++i)
                if (! rows[i].empty())
                    sweep (rows[i], true);

            for (size_t i = rows.size() - 1; i-- > 0;)
                if (! rows[i].empty())
                    sweep (rows[i], false);
        }

        // Finish top-down, so that chains hang straight below the first row
        for (size_t i = 1; i < rows.size(); ++i)
            if (! rows[i].empty())
                sweep (rows[i], true);

        // Rows are spaced to fill the area, within limits
        std::vector<int> rowHeights;

        for (const auto& r : rows)
        {
            int height = 0;

            for (auto node : r)
                height = std::max (height, nodes[(size_t) node].height);

            if (! r.empty())
                rowHeights.push_back (height);
        }

        int totalHeight = 0;

        for (auto h : rowHeights)
            totalHeight += h;

        const auto numGaps = (int) rowHeights.size() - 1;
        const auto rowGap = numGaps > 0 ? std::clamp ((options.areaHeight - totalHeight) / (numGaps + 1),
                                                      options.minRowGap, options.maxRowGap)
                                        : 0;

        const auto contentHeight = totalHeight + numGaps * rowGap;
        double top = std::max ((double) options.margin, (options.areaHeight - contentHeight) * 0.5);
        size_t heightIndex = 0;

        for (const auto& r : rows)
        {
            if (r.empty())
                continue;

            const auto rowHeight = rowHeights[heightIndex++];

            for (auto node : r)
                result[(size_t) node].y = top + rowHeight * 0.5;

            top += rowHeight + rowGap;
        }

        // Centre the whole arrangement horizontally
        double left = x[0] - nodes[0].getAnchor(), right = left + nodes[0].width;

        for (size_t i = 0; i < numNodes; ++i)
        {
            left  = std::min (left,  x[i] - nodes[i].getAnchor());
            right = std::max (right, x[i] - nodes[i].getAnchor() + nodes[i].width);
        }

        const auto contentWidth = right - left;
        const auto offset = std::max ((double) options.margin, (options.areaWidth - contentWidth) * 0.5) - left;

        if (requiredArea != nullptr)
            *requiredArea = { (int) std::ceil (contentWidth) + options.margin * 2,
                              contentHeight + options.margin * 2 };

        for (size_t i = 0; i < numNodes; ++i)
            result[i].x = x[i] - nodes[i].getAnchor() + nodes[i].width * 0.5 + offset;

        return result;
    }
}
}
