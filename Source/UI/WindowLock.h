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

#include <atomic>

/** Whether the loaded scene's plug-in windows are locked in place. Each scene has its own, saved
    with it as its window layout is (see PluginGraph::setPluginWindowPositionsLocked()), so this is
    the one loaded, for whatever needs to know, e.g. the Score Viewer hiding its toolbar. */
namespace WindowLock
{
    inline std::atomic<bool> locked { false };
}
