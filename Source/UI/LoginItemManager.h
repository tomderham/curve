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

//==============================================================================
/** Thin wrapper around macOS's SMAppService, used to register Curve.app itself
    as a login item. Implemented in the .mm file to keep Objective-C out of this
    header. Curve is LSUIElement, so registering the main bundle is enough --
    no separate login-item helper target needed.
*/
namespace LoginItemManager
{
    enum class Status
    {
        enabled,           // registered and will launch at login
        notRegistered,
        requiresApproval,  // registered, but needs user approval in System Settings
        notFound,          // registration record missing/invalid
        unavailable        // non-macOS, or below macOS 13
    };

    /** Live query -- always reflects the OS, e.g. reports notRegistered if the
        user removed Curve from Login Items via System Settings. */
    Status getStatus();

    /** Registers/unregisters Curve as a login item. Returns true on success;
        on failure, errorMessage holds a human-readable reason. */
    bool setEnabled (bool shouldEnable, juce::String& errorMessage);

    /** Opens System Settings to the Login Items & Extensions pane. */
    void openSystemSettingsLoginItemsPane();
}

#if JUCE_MAC
struct MacOSDisplayChangeNotifierBase
{
    virtual ~MacOSDisplayChangeNotifierBase() = default;
};
std::unique_ptr<MacOSDisplayChangeNotifierBase> createMacOSDisplayChangeNotifier (std::function<void()> callback);
void forceRefreshMacOSStatusItem (void* nativeHandle);
void removeMacOSStatusItem (void* nativeHandle);
#endif

