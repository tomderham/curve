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

#include "WindowPlacement.h"

/** The macOS side of arranging plug-in windows, which JUCE doesn't cover. */
namespace WindowArrangement
{
    /** The connected displays, with the stable IDs that WindowPlacement saves windows against. */
    std::vector<Curve::WindowPlacement::Display> getDisplays();

    /** Brings the windows to the front in this order, so the last ends up frontmost, even while
        Curve isn't the active app, without making it active. If Curve is active and none of its
        other windows has keyboard focus, the frontmost takes it. */
    void bringToFront (const juce::Array<juce::Component*>& backToFront);

    /** Whether the window is on the Space being shown, or on every Space. */
    bool isOnActiveSpace (juce::Component&);

    /** Moves the window onto the Space being shown, so it isn't left on another while windows
        opened with it appear on this one, which is where macOS opens new windows. */
    void moveToActiveSpace (juce::Component&);

    /** Shows the window on every Space, or only on the one it's on. */
    void setOnAllSpaces (juce::Component&, bool shouldBeOnAll);

    /** Whether a mouse button is down anywhere, e.g. dragging a window by its title bar, which
        macOS handles without telling the window's view. */
    bool isMouseButtonDown();
}
