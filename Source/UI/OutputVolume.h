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

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

//==============================================================================
/**
    Curve's output volume, set by the volume keys (see VolumeKeyManager).

    The level is held in quarter steps, as macOS does: a volume key moves 4 of them
    (16 steps from silent to full) and Shift-Option with a volume key moves 1.
    The curve copies macOS's own for the built-in speakers: linear in dB from
    -63.5 dB at the first step to 0 dB at the top, and silent at 0.

    Plain C++ so it can be tested without JUCE (tests/output-volume).
*/
namespace OutputVolume
{
    constexpr int maxSteps = 64;
    constexpr int stepsPerKey = 4;
    constexpr float rangeDb = 63.5f;

    inline int clampSteps (int steps) noexcept { return std::clamp (steps, 0, maxSteps); }

    // dB for a level above 0, linear in the 0..1 scalar as in macOS: the first whole step
    // (4/64) is -59.5 dB, each whole step adds 3.97 dB, and step 0 alone is silent
    inline float stepsToDb (int steps) noexcept
    {
        return rangeDb * ((float) clampSteps (steps) / (float) maxSteps - 1.0f);
    }

    inline float stepsToGain (int steps, bool muted) noexcept
    {
        steps = clampSteps (steps);

        if (muted || steps == 0)
            return 0.0f;

        if (steps == maxSteps)
            return 1.0f; // exactly, so the output stays bit-perfect at full volume

        return std::pow (10.0f, stepsToDb (steps) / 20.0f);
    }

    /** The level after one key press. A whole step lands on the next multiple of 4, as in macOS,
        so a level set by fine steps rejoins the 16-step scale. */
    inline int stepVolume (int steps, bool up, bool fine) noexcept
    {
        steps = clampSteps (steps);

        if (fine)
            return clampSteps (steps + (up ? 1 : -1));

        if (up)
            return clampSteps ((steps / stepsPerKey + 1) * stepsPerKey);

        return clampSteps (((steps + stepsPerKey - 1) / stepsPerKey - 1) * stepsPerKey);
    }

    /** Where the volume keys' gain applies: nowhere while Curve doesn't take the keys, to
        Curve's whole output, or only to the Interface Loopback (In) nodes. */
    enum Target : std::uint32_t { none = 0, output = 1, loopback = 2 };

    /** The volume keys' gain and its target. One atomic, so the audio thread never sees the
        gain of one target with the other target. Written on the message thread and the key
        tap's thread. */
    struct Setting
    {
        float gain = 1.0f;
        std::uint32_t target = none;

        float outputGain() const noexcept   { return target == output ? gain : 1.0f; }
        float loopbackGain() const noexcept { return target == loopback ? gain : 1.0f; }
    };

    inline std::atomic<Setting> setting {};
    static_assert (std::atomic<Setting>::is_always_lock_free, "read on the audio thread");

    /** For the loopback nodes: the output player copies the setting here at the start of each
        audio callback. When the volume moves straight between the output and the loopback
        nodes, the player counts a switch, and both jump to their new gain in that callback, so
        the loopback audio's level doesn't change. Any other change ramps. Written and read on
        the audio thread. */
    inline std::atomic<float> blockLoopbackGain { 1.0f };
    inline std::atomic<std::uint32_t> blockTargetSwitches { 0 };
}
