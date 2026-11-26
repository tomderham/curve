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

#include "MenuBarPanelState.h"

//==============================================================================
/**
    The panel that opens from Curve's menu bar icon: presets to switch between, the preset's
    plug-ins with their bypass and windows, the output volume, and the app's settings. Native
    AppKit, in a popover attached to the icon. What it shows and does comes from MenuBarPanelModel.

    The Plug-ins and Volume sections can each be turned off in Preferences, which leaves a panel
    of presets much like the menu it replaced.
*/
class MenuBarPanel final
{
public:
    /** The app's settings and windows, from the panel's gear menu. */
    struct Callbacks
    {
        std::function<void()> showAudioSettings, showPluginManager, showPreferences,
                              showHelp, showAbout, quit;
    };

    MenuBarPanel (MenuBarPanelState::Source&, Callbacks);
    ~MenuBarPanel();

    /** Opens the panel below the status item (an NSStatusItem*), or closes it if it's open. */
    void toggle (void* statusItem);
    void close();
    bool isOpen() const;

    // Settings for which sections the panel shows
    static constexpr const char* showPluginsSetting = "menuBarPanelShowsPlugins";
    static constexpr const char* showVolumeSetting  = "menuBarPanelShowsVolume";
    static constexpr const char* showMeterSetting   = "menuBarPanelShowsMeter";
    static constexpr const char* showOutputSetting  = "menuBarPanelShowsOutputSwitches";
    static constexpr const char* showTempoSetting   = "menuBarPanelShowsTempo"; // the tempo, Tap, and Play/Stop
    static constexpr const char* solidBackgroundSetting = "menuBarPanelSolidBackground"; // rather than macOS's glass
    static constexpr const char* useMenuSetting     = "menuBarIconOpensMenu"; // the menu instead of the panel

    static bool getSetting (const char* key, bool defaultValue);
    static void setSetting (const char* key, bool value);

    /** The panel's background: frosted, as macOS's menus and the menu bar's own panels (e.g.
        Wi-Fi) are, which stays light over a dark window; the popover's clearer glass, which takes
        on what's behind it; or solid. Before there was a choice, it was clear glass or solid. */
    enum class Background { frosted = 0, clearGlass, solid };
    static Background getBackground();
    static void setBackground (Background);

    /** Light or dark, or as macOS is set. Curve's own windows are always dark. */
    enum class Appearance { system = 0, light, dark };
    static Appearance getAppearance();
    static void setAppearance (Appearance);

    struct Impl;

private:
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE (MenuBarPanel)
};
