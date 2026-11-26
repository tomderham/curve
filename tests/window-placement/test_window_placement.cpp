// Checks where plug-in windows reopen as displays come, go and move.
// Build and run: tests/window-placement/run.sh

#include "../../Source/UI/WindowPlacement.h"

#include <cstdio>

namespace
{
using namespace Curve::WindowPlacement;

int failures = 0;

void check (const char* name, bool ok)
{
    std::printf ("%-4s %s\n", ok ? "ok" : "FAIL", name);
    failures += ok ? 0 : 1;
}

bool at (const Result& r, Outcome outcome, int x, int y)
{
    if (r.outcome == outcome && r.x == x && r.y == y)
        return true;

    std::printf ("     got outcome %d at (%d, %d)\n", (int) r.outcome, r.x, r.y);
    return false;
}

// A 1512x982 laptop as main, menu bar 33 high
Display laptop()
{
    return { "LAPTOP", { 0, 0, 1512, 982 }, { 0, 33, 1512, 949 }, true };
}

// A 2560x1440 external display, placed to the laptop's right
Display external (int x = 1512, int y = -200)
{
    return { "EXTERNAL", { x, y, 2560, 1440 }, { x, y + 25, 2560, 1415 }, false };
}
}

int main()
{
    const std::vector<Display> both { laptop(), external() };
    const std::vector<Display> laptopOnly { laptop() };

    Saved onExternal;
    onExternal.displayId = "EXTERNAL";
    onExternal.offsetX = 300;
    onExternal.offsetY = 200;

    check ("reopens on its display", at (place (onExternal, 600, 400, both), Outcome::onSavedDisplay, 1812, 0));

    // The external display moved to the left of the laptop, and up
    const std::vector<Display> rearranged { laptop(), external (-2560, -400) };
    check ("follows its display when rearranged", at (place (onExternal, 600, 400, rearranged), Outcome::onSavedDisplay, -2260, -200));

    check ("on the main display while its own is away", at (place (onExternal, 600, 400, laptopOnly), Outcome::displaced, 300, 200));

    Saved farOnExternal = onExternal;
    farOnExternal.offsetX = 2200;
    farOnExternal.offsetY = 1300;
    check ("displaced window kept wholly on screen", at (place (farOnExternal, 600, 400, laptopOnly), Outcome::displaced, 912, 582));
    check ("displaced window's title bar is reachable",
           isTitleBarReachable (laptopOnly, place (farOnExternal, 600, 400, laptopOnly).x, place (farOnExternal, 600, 400, laptopOnly).y, 600));

    // The external display at a lower resolution than when saved
    const std::vector<Display> smaller { laptop(), { "EXTERNAL", { 1512, 0, 1280, 720 }, { 1512, 25, 1280, 695 }, false } };
    const auto shrunk = place (farOnExternal, 600, 400, smaller);
    check ("kept on its display at a lower resolution", shrunk.outcome == Outcome::onSavedDisplay
                                                         && isTitleBarReachable (smaller, shrunk.x, shrunk.y, 600)
                                                         && shrunk.x >= 1512);

    Saved older;
    older.hasAbsolute = true;
    older.x = 100;
    older.y = 300;
    Saved huge = onExternal;
    check ("window larger than the main display keeps its title bar there",
           at (place (huge, 2000, 1200, laptopOnly), Outcome::displaced, 0, 61));

    check ("older presets keep their absolute position", at (place (older, 400, 300, both), Outcome::absolute, 100, 300));

    older.x = 5000;
    check ("older presets off every display are left to the caller", place (older, 400, 300, both).outcome == Outcome::unsaved);

    check ("nothing saved is left to the caller", place (Saved {}, 400, 300, both).outcome == Outcome::unsaved);

    check ("title bar under the menu bar isn't reachable", ! isTitleBarReachable (laptopOnly, 100, 40, 400));
    check ("title bar just below the menu bar is reachable", isTitleBarReachable (laptopOnly, 100, 61, 400));

    check ("finds the display most of a window is on",
           findDisplayFor (both, { 1400, 100, 800, 600 })->id == "EXTERNAL");
    check ("finds the nearest display for a window off them all",
           findDisplayFor (both, { -3000, 100, 400, 300 })->id == "LAPTOP");

    // Arranging: a 1000x700 area at (0, 30), 28 high title bars, 10 apart
    {
        const Rect area { 0, 30, 1000, 700 };
        const auto p = arrange ({ { 400, 300 }, { 400, 200 }, { 400, 250 }, { 300, 100 } }, area, 28, 10);

        check ("the first is in the top left, below its title bar", p[0].x == 10 && p[0].y == 68);
        check ("the next is beside it", p[1].x == 420 && p[1].y == 68);
        check ("one too wide for the row starts the next, below the tallest", p[2].x == 10 && p[2].y == 30 + 10 + 328 + 10 + 28);
        check ("and the row goes on", p[3].x == 420 && p[3].y == p[2].y);

        const auto hidden = arrange ({ { 400, 300 } }, area, 0, 10);
        check ("without title bars, the content goes at the top", hidden[0].y == 40);

        const auto tooMany = arrange ({ { 900, 500 }, { 900, 500 } }, area, 28, 10);
        check ("those that don't fit start again from the top left, offset", tooMany[1].x == 40 && tooMany[1].y == 30 + 10 + 30 + 28);

        const auto wide = arrange ({ { 1200, 200 } }, area, 28, 10);
        check ("one wider than the area still goes in at the left", wide[0].x == 10);
    }

    // The backdrop behind a layout
    {
        const Rect usable { 0, 33, 1512, 949 };
        const auto area = backdropArea ({ { 100, 100, 400, 300 }, { 520, 120, 300, 500 } }, usable, 16);
        check ("the backdrop covers every window, with a margin", area.x == 84 && area.y == 84 && area.right() == 836 && area.bottom() == 636);

        const auto edge = backdropArea ({ { 0, 40, 400, 300 } }, usable, 16);
        check ("the backdrop stays on the display", edge.x == 0 && edge.y == 33);

        check ("no windows, no backdrop", backdropArea ({}, usable, 16).w == 0);
    }

    std::printf ("\n%s\n", failures == 0 ? "All window placement tests passed" : "Some window placement tests FAILED");
    return failures == 0 ? 0 : 1;
}
