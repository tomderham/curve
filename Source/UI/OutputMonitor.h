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

//==============================================================================
/**
    Mute, Dim and Mono for what Curve plays, as a studio's monitor controller has: switches for
    checking what's heard, at the very end, after the graph and the volume, whatever feeds the
    graph. They apply to every output, each pair of outputs (1-2, 3-4...) made mono on its own.

    Not saved: Curve always starts with them off. Each change fades over 20 ms, and with all of
    them off the audio is left untouched, so it stays bit-perfect.

    Plain C++ so it can be tested without JUCE (tests/output-monitor).
*/
namespace OutputMonitor
{
    // Set on the message thread, read on the audio thread
    inline std::atomic<bool> muted { false }, dimmed { false }, mono { false };

    inline constexpr float dimGain = 0.1f; // -20 dB
    inline constexpr double fadeSeconds = 0.02;

    /** For the audio thread. */
    class Processor
    {
    public:
        void prepare (double sampleRate) noexcept
        {
            step = sampleRate > 0.0 ? (float) (1.0 / (sampleRate * fadeSeconds)) : 1.0f;
            gain = targetGain();
            monoAmount = mono.load (std::memory_order_relaxed) ? 1.0f : 0.0f;
        }

        void process (float* const* channels, int numChannels, int numSamples) noexcept
        {
            const auto toGain = targetGain();
            const auto toMono = mono.load (std::memory_order_relaxed) ? 1.0f : 0.0f;

            if (gain == 1.0f && toGain == 1.0f && monoAmount == 0.0f && toMono == 0.0f)
                return;

            for (int i = 0; i < numSamples; ++i)
            {
                gain = moveTowards (gain, toGain);
                monoAmount = moveTowards (monoAmount, toMono);

                for (int ch = 0; ch < numChannels; ch += 2)
                {
                    float* left = channels[ch];
                    float* right = ch + 1 < numChannels ? channels[ch + 1] : nullptr;

                    if (left != nullptr && right != nullptr)
                    {
                        const auto mid = 0.5f * (left[i] + right[i]);
                        left[i]  = (left[i]  + monoAmount * (mid - left[i]))  * gain;
                        right[i] = (right[i] + monoAmount * (mid - right[i])) * gain;
                    }
                    else if (left != nullptr || right != nullptr)
                    {
                        auto* only = left != nullptr ? left : right;
                        only[i] *= gain;
                    }
                }
            }
        }

    private:
        static float targetGain() noexcept
        {
            if (muted.load (std::memory_order_relaxed))
                return 0.0f;

            return dimmed.load (std::memory_order_relaxed) ? dimGain : 1.0f;
        }

        float moveTowards (float value, float target) const noexcept
        {
            return value < target ? std::min (target, value + step) : std::max (target, value - step);
        }

        float gain = 1.0f, monoAmount = 0.0f, step = 1.0f;
    };
}
