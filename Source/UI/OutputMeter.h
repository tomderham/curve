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

//==============================================================================
/**
    The peak level of Curve's output, for the menu bar panel's meter.

    Measured by the output player at the end of each audio callback, but only while something
    asks for it (enabled), so it costs one atomic read per block otherwise. Left is the peak of
    the even outputs (1, 3, ...) and right of the odd ones, so a preset playing to speakers on
    1-2 and headphones on 3-4 shows on one stereo meter.

    Plain C++ so it can be tested without JUCE.
*/
namespace OutputMeter
{
    inline std::atomic<bool> enabled { false };
    inline std::atomic<float> peaks[2] { { 0.0f }, { 0.0f } };

    /** Audio thread: raises the held peaks to this block's. */
    inline void measure (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        if (! enabled.load (std::memory_order_relaxed))
            return;

        float blockPeaks[2] = { 0.0f, 0.0f };

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (const auto* data = channels[ch])
            {
                auto& peak = blockPeaks[ch % 2];

                // A sample that isn't a number, e.g. from a misbehaving plug-in, isn't a level
                for (int i = 0; i < numSamples; ++i)
                    if (const auto level = std::abs (data[i]); std::isfinite (level))
                        peak = std::max (peak, level);
            }
        }

        // A single-channel output shows on both sides
        if (numChannels == 1)
            blockPeaks[1] = blockPeaks[0];

        for (int side = 0; side < 2; ++side)
        {
            auto held = peaks[side].load (std::memory_order_relaxed);

            while (blockPeaks[side] > held
                   && ! peaks[side].compare_exchange_weak (held, blockPeaks[side], std::memory_order_relaxed))
            {}
        }
    }

    /** Turns measuring on or off. Either way, the next read starts from silence, not from whatever
        was held from the last time. */
    inline void setEnabled (bool shouldMeasure) noexcept
    {
        peaks[0].store (0.0f);
        peaks[1].store (0.0f);
        enabled.store (shouldMeasure);
    }

    /** Reader: the peaks since the last read, which then start again from silence. */
    inline void read (float& left, float& right) noexcept
    {
        left  = peaks[0].exchange (0.0f, std::memory_order_relaxed);
        right = peaks[1].exchange (0.0f, std::memory_order_relaxed);
    }
}
