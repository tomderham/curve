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

namespace SystemSleepManager
{
    /** Controls whether macOS CoreAudio is allowed to idle sleep even while audio I/O is active.
        If preventSleep is true, sleeping is disallowed (kAudioHardwarePropertySleepingIsAllowed = 0),
        causing coreaudiod to assert PreventUserIdleSystemSleep.
        If preventSleep is false, sleeping is allowed (kAudioHardwarePropertySleepingIsAllowed = 1),
        preventing coreaudiod from holding the sleep assertion.
    */
    void setPreventSystemSleep (bool preventSleep);

    /** Returns true if the user has opted to prevent system sleep in settings.
        Defaults to false (system sleep is allowed).
    */
    bool isPreventSystemSleepEnabled();

    /** Applies the current setting from user preferences to the CoreAudio HAL. */
    void applyCurrentSetting();
}

