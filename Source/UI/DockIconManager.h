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

namespace DockIconManager
{
    /** Shows or hides Curve's Dock icon (and Cmd-Tab entry) by switching the app's
        activation policy. Curve is LSUIElement, so it starts without one.

        While the Dock icon is shown, the menu bar icon's menu can close straight
        away when clicked from another app; see TrayIconController::showTrayMenuWhenActive.
    */
    void setShowInDock (bool showInDock);

    /** Returns true if the user has opted to show Curve in the Dock.
        Defaults to false (menu bar only).
    */
    bool isShowInDockEnabled();

    /** Applies the current setting from user preferences. */
    void applyCurrentSetting();

    /** Calls onReopen when the user clicks the Dock icon while Curve is running.
        JUCE's app delegate doesn't handle this, so it's done via the
        kAEReopenApplication Apple Event.
    */
    void setReopenCallback (std::function<void()> onReopen);
}
