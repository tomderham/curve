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
    void setShowInDock (bool showInDock, std::function<void()> onceDone = nullptr);

    /** When Curve has a Dock icon: never (menu bar only), while its editor is open, so the editor
        can be found with the Dock and Cmd-Tab, or always. */
    enum class Mode { never = 0, whileEditorOpen, always };

    /** As set in Preferences. Defaults to while the editor is open; before there was a choice,
        Show in Dock on was always. */
    Mode getMode();
    void setMode (Mode);

    /** Told as the editor opens and closes, for whileEditorOpen. onceDone runs when Curve is ready
        for the editor to be shown: at once, or once a Dock icon's been added, which hands the focus
        to the Dock and back, and would leave a window shown meanwhile behind other apps'. */
    void setEditorOpen (bool isOpen, std::function<void()> onceDone = nullptr);

    /** Applies the current setting from user preferences. */
    void applyCurrentSetting();

    /** Calls onReopen when the user clicks the Dock icon while Curve is running.
        JUCE's app delegate doesn't handle this, so it's done via the
        kAEReopenApplication Apple Event.
    */
    void setReopenCallback (std::function<void()> onReopen);
}
