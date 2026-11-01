// Tests for the graph auto-layout in Source/UI/GraphLayout.h

#include "../../Source/UI/GraphLayout.h"

#include <cmath>
#include <cstdio>

using namespace Curve::GraphLayout;

static int failures = 0;

#define CHECK(condition) \
    do { if (! (condition)) { std::printf ("FAILED line %d: %s\n", __LINE__, #condition); ++failures; } } while (false)

static bool overlaps (const std::vector<NodeInfo>& nodes, const std::vector<Centre>& c)
{
    for (size_t a = 0; a < nodes.size(); ++a)
        for (size_t b = a + 1; b < nodes.size(); ++b)
            if (std::abs (c[a].x - c[b].x) < (nodes[a].width + nodes[b].width) * 0.5
                && std::abs (c[a].y - c[b].y) < (nodes[a].height + nodes[b].height) * 0.5)
                return true;

    return false;
}

static void testEmpty()
{
    CHECK (arrange ({}, {}, {}).empty());
}

static void testChainHangsStraight()
{
    const std::vector<NodeInfo> nodes (4, { 140, 60 });
    const auto c = arrange (nodes, { { 0, 1 }, { 1, 2 }, { 2, 3 } }, {});

    for (size_t i = 1; i < nodes.size(); ++i)
    {
        CHECK (std::abs (c[i].x - c[0].x) < 0.001);
        CHECK (c[i].y > c[i - 1].y);
    }

    CHECK (std::abs (c[0].x - 400.0) < 0.001);
}

static void testParallelBranches()
{
    // The screenshot's shape: one source, four branches, one output
    const std::vector<NodeInfo> nodes { { 200, 60 }, { 140, 60 }, { 140, 60 }, { 140, 60 },
                                        { 180, 60 }, { 140, 60 }, { 160, 60, true } };
    const auto c = arrange (nodes, { { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 4, 5 },
                                     { 1, 6 }, { 2, 6 }, { 3, 6 }, { 5, 6 } }, {});

    CHECK (! overlaps (nodes, c));
    CHECK (c[6].y > c[5].y);
    CHECK (c[5].y > c[4].y);
    CHECK (c[1].y > c[0].y);
    CHECK (std::abs (c[1].y - c[4].y) < 0.001);
}

static void testFeedbackLoopTerminates()
{
    const std::vector<NodeInfo> nodes (3, { 140, 60 });
    const auto c = arrange (nodes, { { 0, 1 }, { 1, 2 }, { 2, 1 } }, {});

    CHECK (! overlaps (nodes, c));
    CHECK (c[1].y > c[0].y);
    CHECK (c[2].y > c[1].y);

    // No source at all: a pure cycle must still place every node
    const auto ring = arrange (nodes, { { 0, 1 }, { 1, 2 }, { 2, 0 } }, {});
    CHECK (! overlaps (nodes, ring));
}

static void testUnconnectedNodesDoNotOverlap()
{
    const std::vector<NodeInfo> nodes (6, { 160, 100 });
    CHECK (! overlaps (nodes, arrange (nodes, {}, {})));
}

static void testPinnedNodeStaysBelowItsInputs()
{
    // The output's only input is on the row that the pin would otherwise use
    const std::vector<NodeInfo> nodes { { 140, 60 }, { 140, 60 }, { 140, 60 }, { 140, 60, true } };
    const auto c = arrange (nodes, { { 0, 1 }, { 1, 2 }, { 0, 3 } }, {});

    CHECK (c[3].y > c[0].y);
    CHECK (c[3].y >= c[2].y);
    CHECK (! overlaps (nodes, c));
}

static void testDuplicateAndSelfEdgesIgnored()
{
    // A stereo pair is two connections between the same two nodes
    const std::vector<NodeInfo> nodes (2, { 140, 60 });
    const auto c = arrange (nodes, { { 0, 1 }, { 0, 1 }, { 1, 1 } }, {});

    CHECK (c[1].y > c[0].y);
    CHECK (std::abs (c[1].x - c[0].x) < 0.001);
}

static void testAnchorsAlignNodesOfDifferentWidths()
{
    // With pins at a fixed inset, a chain lines up on its left edges
    const std::vector<NodeInfo> nodes { { 140, 60, false, 60 }, { 220, 60, false, 60 }, { 180, 60, false, 60 } };
    const auto c = arrange (nodes, { { 0, 1 }, { 1, 2 } }, {});

    for (size_t i = 0; i < nodes.size(); ++i)
        CHECK (std::abs ((c[i].x - nodes[i].width * 0.5) - (c[0].x - nodes[0].width * 0.5)) < 0.001);

    // ...and nodes side by side still keep clear of each other
    const auto row = arrange (nodes, { { 0, 1 }, { 0, 2 } }, {});
    CHECK (! overlaps (nodes, row));
}

static bool fits (const std::vector<NodeInfo>& nodes, const std::vector<Centre>& c, int width, int height)
{
    for (size_t i = 0; i < nodes.size(); ++i)
        if (c[i].x - nodes[i].width * 0.5 < 0 || c[i].x + nodes[i].width * 0.5 > width
            || c[i].y - nodes[i].height * 0.5 < 0 || c[i].y + nodes[i].height * 0.5 > height)
            return false;

    return true;
}

static void testLargeGraphFitsItsRequiredArea()
{
    // The shape of a large preset: 8 rows, 5 nodes wide
    const std::vector<NodeInfo> nodes { { 140, 60 }, { 140, 60 }, { 200, 60 }, { 140, 60 }, { 140, 60 }, { 140, 60 },
                                        { 140, 60 }, { 140, 60 }, { 140, 60 }, { 140, 60 }, { 140, 60 }, { 200, 60 },
                                        { 140, 60 }, { 140, 60 }, { 140, 60 }, { 220, 60, true } };
    const std::vector<Edge> edges { { 0, 3 }, { 0, 4 }, { 1, 5 }, { 5, 6 }, { 3, 7 }, { 7, 8 }, { 2, 9 }, { 6, 9 }, { 8, 9 },
                                    { 9, 10 }, { 9, 11 }, { 9, 12 }, { 9, 13 }, { 9, 14 }, { 11, 15 }, { 10, 15 }, { 12, 15 }, { 13, 15 } };

    Extent required;
    const auto small = arrange (nodes, edges, {}, &required);

    CHECK (required.width > 800 || required.height > 600);
    CHECK (! overlaps (nodes, small));

    // Too small: the top left stays visible
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        CHECK (small[i].x - nodes[i].width * 0.5 >= 19.9);
        CHECK (small[i].y - nodes[i].height * 0.5 >= 19.9);
    }

    // Given what it asked for, everything is on screen
    Options big;
    big.areaWidth = required.width;
    big.areaHeight = required.height;

    const auto c = arrange (nodes, edges, big);
    CHECK (fits (nodes, c, big.areaWidth, big.areaHeight));
    CHECK (! overlaps (nodes, c));
}

int main()
{
    testEmpty();
    testChainHangsStraight();
    testParallelBranches();
    testFeedbackLoopTerminates();
    testUnconnectedNodesDoNotOverlap();
    testPinnedNodeStaysBelowItsInputs();
    testDuplicateAndSelfEdgesIgnored();
    testAnchorsAlignNodesOfDifferentWidths();
    testLargeGraphFitsItsRequiredArea();

    std::printf (failures == 0 ? "All graph layout tests passed\n" : "%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
