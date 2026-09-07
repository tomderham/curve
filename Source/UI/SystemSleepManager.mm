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

#if defined (__APPLE__)
#include <CoreAudio/CoreAudio.h>
#endif

#include "SystemSleepManager.h"

juce::PropertiesFile* getUserSettings();

namespace SystemSleepManager
{
    void setPreventSystemSleep (bool preventSleep)
    {
    #if JUCE_MAC
        AudioObjectPropertyAddress address = {
            kAudioHardwarePropertySleepingIsAllowed,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        UInt32 sleepingAllowed = preventSleep ? 0 : 1;
        AudioObjectSetPropertyData (kAudioObjectSystemObject, &address, 0, nullptr,
                                    static_cast<UInt32> (sizeof (sleepingAllowed)), &sleepingAllowed);
    #else
        juce::ignoreUnused (preventSleep);
    #endif
    }

    bool isPreventSystemSleepEnabled()
    {
        if (auto* settings = getUserSettings())
            return settings->getBoolValue ("preventSystemSleep", false);

        return false;
    }

    void applyCurrentSetting()
    {
        setPreventSystemSleep (isPreventSystemSleepEnabled());
    }
}

