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

juce::PropertiesFile* getUserSettings();

/** The folders chosen in Preferences > Files: where the recorder saves, and where scores and backing
    tracks are opened from first. A folder that's gone falls back to the default. */
namespace CurveFolders
{
    enum class Kind { recordings, media };

    inline const char* getSettingKey (Kind kind)
    {
        return kind == Kind::recordings ? "recordingsFolder" : "mediaFolder";
    }

    inline juce::File getDefault (Kind kind)
    {
        return juce::File::getSpecialLocation (kind == Kind::recordings ? juce::File::userDesktopDirectory
                                                                        : juce::File::userDocumentsDirectory);
    }

    inline juce::File get (Kind kind)
    {
        if (auto* settings = getUserSettings())
        {
            const auto path = settings->getValue (getSettingKey (kind));

            if (juce::File::isAbsolutePath (path) && juce::File (path).isDirectory())
                return juce::File (path);
        }

        return getDefault (kind);
    }

    inline void set (Kind kind, const juce::File& folder)
    {
        if (auto* settings = getUserSettings())
        {
            settings->setValue (getSettingKey (kind), folder.getFullPathName());
            settings->saveIfNeeded();
        }
    }
}
