/*
==============================================================================
   Curve - Plug-in window placement
   Where a plug-in window reopens: on the display it was saved on, at the same
   place relative to it, wherever that display now is in the arrangement. Free
   of JUCE so it can be tested on its own (tests/window-placement).
==============================================================================
*/

#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace Curve
{
namespace WindowPlacement
{
    struct Rect
    {
        int x = 0, y = 0, w = 0, h = 0;

        int right() const  { return x + w; }
        int bottom() const { return y + h; }

        int overlapArea (const Rect& o) const
        {
            const int ow = std::min (right(), o.right()) - std::max (x, o.x);
            const int oh = std::min (bottom(), o.bottom()) - std::max (y, o.y);
            return ow > 0 && oh > 0 ? ow * oh : 0;
        }
    };

    struct Display
    {
        std::string id;  // Stable across restarts and rearrangements (the display's UUID)
        Rect frame;      // The whole display, in desktop coordinates with y down
        Rect usable;     // Without the menu bar and Dock
        bool isMain = false;
    };

    /** Where a window was saved. Positions are of the window's content, below its title bar. */
    struct Saved
    {
        std::string displayId;     // Empty for presets saved before displays were remembered
        int offsetX = 0, offsetY = 0; // From the top left of that display's frame
        bool hasAbsolute = false;  // The older absolute desktop position
        int x = 0, y = 0;
    };

    enum class Outcome
    {
        onSavedDisplay, // Where it was saved
        displaced,      // Its display isn't connected, so on the main one for now
        absolute,       // An older preset's absolute position
        unsaved         // Nothing usable saved: the caller places it
    };

    struct Result
    {
        Outcome outcome = Outcome::unsaved;
        int x = 0, y = 0;
    };

    inline constexpr int titleBarHeight = 28; // macOS's standard; JUCE reports a native one as 0
    inline constexpr int minVisibleWidth = 100;

    inline const Display* findDisplay (const std::vector<Display>& displays, const std::string& id)
    {
        for (const auto& d : displays)
            if (d.id == id)
                return &d;

        return nullptr;
    }

    inline const Display* findMainDisplay (const std::vector<Display>& displays)
    {
        for (const auto& d : displays)
            if (d.isMain)
                return &d;

        return displays.empty() ? nullptr : &displays.front();
    }

    /** The display that most of a window is on, or failing that the nearest one to it. */
    inline const Display* findDisplayFor (const std::vector<Display>& displays, const Rect& window)
    {
        const Display* best = nullptr;
        int bestArea = 0;

        for (const auto& d : displays)
            if (const int area = d.frame.overlapArea (window); area > bestArea)
            {
                best = &d;
                bestArea = area;
            }

        if (best != nullptr)
            return best;

        // Off every display: the one whose centre is nearest the window's
        const auto distance = [&] (const Rect& r)
        {
            const long dx = (long) (r.x + r.w / 2) - (window.x + window.w / 2);
            const long dy = (long) (r.y + r.h / 2) - (window.y + window.h / 2);
            return dx * dx + dy * dy;
        };

        for (const auto& d : displays)
            if (best == nullptr || distance (d.frame) < distance (best->frame))
                best = &d;

        return best;
    }

    /** Whether enough of the title bar of a window whose content is at (x, y) is on a display for it to be dragged. */
    inline bool isTitleBarReachable (const std::vector<Display>& displays, int x, int y, int width, int titleBar_ = titleBarHeight)
    {
        // Without a title bar (a locked window), the top of its content
        const auto height = std::max (titleBar_, 1);
        const Rect titleBar { x, y - titleBar_, width, height };
        const int needed = std::min (minVisibleWidth, width);

        for (const auto& d : displays)
        {
            const int ow = std::min (titleBar.right(), d.usable.right()) - std::max (titleBar.x, d.usable.x);
            const bool fullHeight = titleBar.y >= d.usable.y && titleBar.bottom() <= d.usable.bottom();

            if (fullHeight && ow >= needed)
                return true;
        }

        return false;
    }

    /** Moves a window just enough to be wholly on the display's usable area, or if it's larger, to
        have its title bar and top left there. (x, y) and the size are of its content. */
    inline void keepOn (const Display& display, int width, int height, int& x, int& y, int titleBar = titleBarHeight)
    {
        const auto& u = display.usable;
        const int top = u.y + titleBar;

        x = std::max (u.x, std::min (x, u.right() - width));
        y = std::max (top, std::min (y, u.bottom() - height));
    }

    struct Size { int width = 0, height = 0; };
    struct Position { int x = 0, y = 0; };

    /** Arranges windows, given their content sizes in the order to place them, in rows across the
        area, left to right and top to bottom, with a gap around each. The title bar is above each
        window's content (0 if hidden). Windows that don't fit go on again from the top left,
        offset, so they overlap rather than leave the area. Returns where each one's content goes. */
    inline std::vector<Position> arrange (const std::vector<Size>& sizes, const Rect& area, int titleBar, int gap)
    {
        std::vector<Position> positions;
        int x = area.x + gap, y = area.y + gap, rowHeight = 0, layer = 0;

        for (const auto& size : sizes)
        {
            const int width = size.width, height = size.height + titleBar;

            // On to the next row, unless this is the first in its row, which goes in however wide
            if (x > area.x + gap && x + width > area.right() - gap)
            {
                x = area.x + gap + layer * 30;
                y += rowHeight + gap;
                rowHeight = 0;
            }

            // Below the area: start again from the top left, a little further in each time
            if (y > area.y + gap && y + height > area.bottom() - gap)
            {
                ++layer;
                x = area.x + gap + layer * 30;
                y = area.y + gap + layer * 30;
                rowHeight = 0;
            }

            positions.push_back ({ x, y + titleBar });
            x += width + gap;
            rowHeight = std::max (rowHeight, height);
        }

        return positions;
    }

    /** The area a backdrop behind these windows covers: all of them, with a margin around, kept to
        the display's usable area. Empty without any windows. Frames include title bars. */
    inline Rect backdropArea (const std::vector<Rect>& windows, const Rect& usable, int margin)
    {
        if (windows.empty())
            return {};

        int left = windows.front().x, top = windows.front().y;
        int right = windows.front().right(), bottom = windows.front().bottom();

        for (const auto& w : windows)
        {
            left = std::min (left, w.x);
            top = std::min (top, w.y);
            right = std::max (right, w.right());
            bottom = std::max (bottom, w.bottom());
        }

        left   = std::max (usable.x, left - margin);
        top    = std::max (usable.y, top - margin);
        right  = std::min (usable.right(), right + margin);
        bottom = std::min (usable.bottom(), bottom + margin);

        return right > left && bottom > top ? Rect { left, top, right - left, bottom - top } : Rect {};
    }

    /** Where a window with content of the given size, and a title bar this high (0 if hidden), should reopen. */
    inline Result place (const Saved& saved, int width, int height, const std::vector<Display>& displays,
                         int titleBar = titleBarHeight)
    {
        Result result;

        if (! saved.displayId.empty())
        {
            if (const auto* display = findDisplay (displays, saved.displayId))
            {
                result.outcome = Outcome::onSavedDisplay;
                result.x = display->frame.x + saved.offsetX;
                result.y = display->frame.y + saved.offsetY;

                // The display may be at a lower resolution than it was
                if (! isTitleBarReachable (displays, result.x, result.y, width, titleBar))
                    keepOn (*display, width, height, result.x, result.y, titleBar);

                return result;
            }

            if (const auto* main = findMainDisplay (displays))
            {
                result.outcome = Outcome::displaced;
                result.x = main->frame.x + saved.offsetX;
                result.y = main->frame.y + saved.offsetY;

                // Wholly on it, as it may have been anywhere on a larger display
                keepOn (*main, width, height, result.x, result.y, titleBar);

                return result;
            }
        }

        if (saved.hasAbsolute && isTitleBarReachable (displays, saved.x, saved.y, width, titleBar))
        {
            result.outcome = Outcome::absolute;
            result.x = saved.x;
            result.y = saved.y;
        }

        return result;
    }
}
}
