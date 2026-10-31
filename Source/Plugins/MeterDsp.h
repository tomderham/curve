/*
==============================================================================
   Curve
   Copyright (c) Thomas Derham

   The modifications for Curve are licensed to you solely under the terms of the
   AGPLv3 license terms.

   CURVE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.
==============================================================================
*/

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

//==============================================================================
/**
    The measurements behind the Meter node, on one or two channels:

    - Sample peak, with a latched flag for samples over full scale.
    - True peak, from 4x oversampling as in BS.1770 Annex 2, which also finds peaks
      between samples.
    - RMS, averaged over about 300 ms. It's true RMS, so a 0 dBFS sine reads -3 dBFS.
    - Stereo correlation, from -1 (out of phase) to +1 (mono).
    - VU: full-wave rectified and smoothed to the standard 300 ms rise time.
      A sine reads its peak level, so a -18 dBFS sine reads 0 VU.
    - Loudness to ITU-R BS.1770-4 / EBU R128: momentary (400 ms), short-term (3 s)
      and gated integrated, in LUFS.

    It only measures between setMeasuring (true) and setMeasuring (false), so that a
    meter nobody is looking at costs one flag check per block. Each start begins
    afresh, including integrated loudness.

    process() is real-time safe and is called on the audio thread. Results are
    atomics that any thread can read. This has no JUCE dependency so that
    tests/meter-dsp can check it against the standards.
*/
class MeterDsp
{
public:
    static constexpr int maxChannels = 2;
    static constexpr float noLoudness = -200.0f;    // no reading yet, or silence

    /** Sets up for a sample rate. Only clears measurements if the rate has changed, so
        that integrated loudness survives the device stopping and restarting. */
    void prepare (double newSampleRate)
    {
        if (std::abs (sampleRate - newSampleRate) > 0.001)
        {
            sampleRate = newSampleRate;
            updateCoefficients();
            resetLoudness();
        }

        for (auto& c : channels)
            c = {};

        meanProduct = 0.0f;
        blockEnergy = 0.0;
        blockSamplesDone = 0;
    }

    /** Any thread. */
    void setMeasuring (bool shouldMeasure) noexcept
    {
        if (shouldMeasure)
        {
            clearPublishedLevels();
            clearPublishedLoudness();
            clearOvers();
        }

        measuring.store (shouldMeasure, std::memory_order_relaxed);
    }

    /** Audio thread only. */
    void process (const float* const* data, int numChannels, int numSamples) noexcept
    {
        if (! measuring.load (std::memory_order_relaxed))
        {
            wasMeasuring = false;
            return;
        }

        if (! wasMeasuring)
        {
            wasMeasuring = true;
            restart();
        }

        blocksProcessed.fetch_add (1, std::memory_order_relaxed);
        numChannels = std::min (numChannels, maxChannels);

        if (resetRequested.exchange (false, std::memory_order_relaxed))
            resetLoudness();

        for (int ch = 0; ch < numChannels; ++ch)
            measureLevels (ch, data[ch], numSamples);

        if (numChannels == 2)
            measureCorrelation (data[0], data[1], numSamples);

        for (int done = 0; done < numSamples;)
        {
            const auto count = std::min (numSamples - done, samplesPer100Ms - blockSamplesDone);

            for (int ch = 0; ch < numChannels; ++ch)
                blockEnergy += weightedEnergy (channels[(size_t) ch], data[ch] + done, count);

            done += count;
            blockSamplesDone += count;

            if (blockSamplesDone == samplesPer100Ms)
                finish100MsBlock();
        }
    }

    /** Any thread: restarts loudness, including integrated. */
    void requestLoudnessReset() noexcept
    {
        resetRequested.store (true, std::memory_order_relaxed);
        clearPublishedLoudness();
    }

    /** Any thread. */
    void clearOvers() noexcept
    {
        for (auto& flag : overFlags)
            flag.store (false, std::memory_order_relaxed);
    }

    /** Counts blocks measured, so that a display can tell when audio has stopped. */
    std::uint32_t getBlocksProcessed() const noexcept   { return blocksProcessed.load (std::memory_order_relaxed); }

    /** The highest absolute sample since the last call. */
    float consumePeak (int channel) noexcept        { return peaks[(size_t) channel].exchange (0.0f, std::memory_order_relaxed); }

    /** The highest true peak since the last call. */
    float consumeTruePeak (int channel) noexcept    { return truePeaks[(size_t) channel].exchange (0.0f, std::memory_order_relaxed); }

    /** From -1 to +1, or 0 for mono or silence. */
    float getCorrelation() const noexcept           { return correlation.load (std::memory_order_relaxed); }

    /** True once a sample has gone over full scale, until cleared. */
    bool hasOver (int channel) const noexcept       { return overFlags[(size_t) channel].load (std::memory_order_relaxed); }

    /** Linear, so 1.0 is a square wave at 0 dBFS. */
    float getRms (int channel) const noexcept       { return std::sqrt (rmsLevels[(size_t) channel].load (std::memory_order_relaxed)); }

    /** Linear, so 1.0 is a 0 dBFS sine. */
    float getVu (int channel) const noexcept        { return vuLevels[(size_t) channel].load (std::memory_order_relaxed); }

    float getMomentaryLufs() const noexcept         { return momentary.load (std::memory_order_relaxed); }
    float getShortTermLufs() const noexcept         { return shortTerm.load (std::memory_order_relaxed); }
    float getIntegratedLufs() const noexcept        { return integrated.load (std::memory_order_relaxed); }

private:
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

    static constexpr int truePeakPhases = 3;    // the fourth is the sample itself
    static constexpr int truePeakTaps = 12;

    struct ChannelState
    {
        double shelfZ1 = 0.0, shelfZ2 = 0.0, highPassZ1 = 0.0, highPassZ2 = 0.0;
        float vu1 = 0.0f, vu2 = 0.0f, meanSquare = 0.0f;
        std::array<float, 2 * truePeakTaps> truePeakHistory {};
        int truePeakIndex = 0;
    };

    static double energyToLufs (double energy) noexcept
    {
        return -0.691 + 10.0 * std::log10 (energy);
    }

    void updateCoefficients() noexcept
    {
        constexpr double pi = 3.14159265358979323846;

        // A 48-point Hann-windowed sinc for 4x oversampling, split into its fractional
        // delays. It doesn't depend on the sample rate. Each is normalised to unity gain.
        for (int phase = 0; phase < truePeakPhases; ++phase)
        {
            double sum = 0.0;
            std::array<double, truePeakTaps> taps {};

            for (int tap = 0; tap < truePeakTaps; ++tap)
            {
                const auto position = tap - (truePeakTaps / 2 - 1) - (phase + 1) / 4.0;     // in samples
                const auto sinc = std::sin (pi * position) / (pi * position);
                const auto window = 0.5 + 0.5 * std::cos (pi * position / (truePeakTaps / 2));

                taps[(size_t) tap] = sinc * window;
                sum += taps[(size_t) tap];
            }

            for (int tap = 0; tap < truePeakTaps; ++tap)
                truePeakFilters[(size_t) phase][(size_t) tap] = (float) (taps[(size_t) tap] / sum);
        }

        // BS.1770 defines both filters at 48 kHz. These are the analogue prototypes,
        // so that they're right at every sample rate.
        {
            constexpr double f0 = 1681.974450955533, gainDb = 3.999843853973347, q = 0.7071752369554196;

            const auto k = std::tan (pi * f0 / sampleRate);
            const auto vh = std::pow (10.0, gainDb / 20.0);
            const auto vb = std::pow (vh, 0.4996667741545416);
            const auto a0 = 1.0 + k / q + k * k;

            shelf = { (vh + vb * k / q + k * k) / a0,
                      2.0 * (k * k - vh) / a0,
                      (vh - vb * k / q + k * k) / a0,
                      2.0 * (k * k - 1.0) / a0,
                      (1.0 - k / q + k * k) / a0 };
        }

        {
            constexpr double f0 = 38.13547087602444, q = 0.5003270373238773;

            const auto k = std::tan (pi * f0 / sampleRate);
            const auto a0 = 1.0 + k / q + k * k;

            highPass = { 1.0, -2.0, 1.0,
                         2.0 * (k * k - 1.0) / a0,
                         (1.0 - k / q + k * k) / a0 };
        }

        // Two equal one-pole stages reach 99% of a step after 6.64 time constants
        const auto timeConstantSamples = 0.3 / 6.638 * sampleRate;
        vuCoefficient = (float) (1.0 - std::exp (-1.0 / timeConstantSamples));

        // 100 ms time constant: settles to 95% in 300 ms
        rmsCoefficient = (float) (1.0 - std::exp (-1.0 / (0.1 * sampleRate)));

        samplesPer100Ms = std::max (1, (int) std::lround (sampleRate * 0.1));
    }

    void measureLevels (int ch, const float* samples, int numSamples) noexcept
    {
        auto& state = channels[(size_t) ch];
        const auto coefficient = vuCoefficient, rmsCoeff = rmsCoefficient;
        auto vu1 = state.vu1, vu2 = state.vu2, meanSquare = state.meanSquare, peak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto rectified = std::abs (samples[i]);
            peak = std::max (peak, rectified);
            vu1 += coefficient * (rectified - vu1);
            vu2 += coefficient * (vu1 - vu2);
            meanSquare += rmsCoeff * (samples[i] * samples[i] - meanSquare);
        }

        state.vu1 = vu1;
        state.vu2 = vu2;
        state.meanSquare = meanSquare;
        rmsLevels[(size_t) ch].store (meanSquare, std::memory_order_relaxed);

        // A sine's rectified mean is 2/pi of its peak
        vuLevels[(size_t) ch].store (vu2 * 1.5707963f, std::memory_order_relaxed);

        const auto truePeak = std::max (peak, measureTruePeak (state, samples, numSamples));
        auto& storedTruePeak = truePeaks[(size_t) ch];

        if (truePeak > storedTruePeak.load (std::memory_order_relaxed))
            storedTruePeak.store (truePeak, std::memory_order_relaxed);

        auto& storedPeak = peaks[(size_t) ch];

        if (peak > storedPeak.load (std::memory_order_relaxed))
            storedPeak.store (peak, std::memory_order_relaxed);

        if (peak > 1.0f)
            overFlags[(size_t) ch].store (true, std::memory_order_relaxed);
    }

    // The history is written twice, so that the last 12 samples are always contiguous
    float measureTruePeak (ChannelState& state, const float* samples, int numSamples) const noexcept
    {
        auto index = state.truePeakIndex;
        auto peak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            state.truePeakHistory[(size_t) index] = state.truePeakHistory[(size_t) index + truePeakTaps] = samples[i];
            index = (index + 1) % truePeakTaps;

            const auto* oldest = state.truePeakHistory.data() + index;

            for (const auto& filter : truePeakFilters)
            {
                auto sum = 0.0f;

                for (int tap = 0; tap < truePeakTaps; ++tap)
                    sum += filter[(size_t) tap] * oldest[tap];

                peak = std::max (peak, std::abs (sum));
            }
        }

        state.truePeakIndex = index;
        return peak;
    }

    void measureCorrelation (const float* left, const float* right, int numSamples) noexcept
    {
        const auto coefficient = rmsCoefficient;
        auto product = meanProduct;

        for (int i = 0; i < numSamples; ++i)
            product += coefficient * (left[i] * right[i] - product);

        meanProduct = product;

        const auto powers = channels[0].meanSquare * channels[1].meanSquare;
        const auto value = powers > 1.0e-14f ? product / std::sqrt (powers) : 0.0f;

        correlation.store (std::clamp (value, -1.0f, 1.0f), std::memory_order_relaxed);
    }

    double weightedEnergy (ChannelState& state, const float* samples, int numSamples) const noexcept
    {
        const auto s = shelf, h = highPass;
        auto s1 = state.shelfZ1, s2 = state.shelfZ2, h1 = state.highPassZ1, h2 = state.highPassZ2;
        double energy = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto in = (double) samples[i];

            const auto shelved = s.b0 * in + s1;
            s1 = s.b1 * in - s.a1 * shelved + s2;
            s2 = s.b2 * in - s.a2 * shelved;

            const auto out = h.b0 * shelved + h1;
            h1 = h.b1 * shelved - h.a1 * out + h2;
            h2 = h.b2 * shelved - h.a2 * out;

            energy += out * out;
        }

        state.shelfZ1 = s1;
        state.shelfZ2 = s2;
        state.highPassZ1 = h1;
        state.highPassZ2 = h2;

        return energy;
    }

    void finish100MsBlock() noexcept
    {
        recentBlocks[(size_t) recentWriteIndex] = blockEnergy / samplesPer100Ms;
        recentWriteIndex = (recentWriteIndex + 1) % blocksPerShortTerm;
        recentCount = std::min (recentCount + 1, blocksPerShortTerm);
        blockEnergy = 0.0;
        blockSamplesDone = 0;

        if (recentCount < blocksPerGate)
            return;

        const auto momentaryEnergy = meanOfRecent (blocksPerGate);

        momentary.store (toStoredLufs (momentaryEnergy), std::memory_order_relaxed);
        shortTerm.store (toStoredLufs (meanOfRecent (recentCount)), std::memory_order_relaxed);

        // Each momentary window is one 400 ms gating block, overlapping by 75%
        addGatingBlock (momentaryEnergy);
    }

    double meanOfRecent (int count) const noexcept
    {
        double sum = 0.0;

        for (int i = 1; i <= count; ++i)
            sum += recentBlocks[(size_t) ((recentWriteIndex - i + blocksPerShortTerm) % blocksPerShortTerm)];

        return sum / count;
    }

    static float toStoredLufs (double energy) noexcept
    {
        return energy > 0.0 ? std::max (noLoudness, (float) energyToLufs (energy)) : noLoudness;
    }

    // Keeps gating blocks in a fixed histogram, so a long session needs no allocation.
    // Each bin keeps its exact energy, so only the relative gate is quantised, to 0.02 LU.
    void addGatingBlock (double energy) noexcept
    {
        if (energy <= 0.0)
            return;

        const auto lufs = energyToLufs (energy);

        if (lufs <= absoluteGateLufs)
            return;

        const auto bin = std::min ((int) ((lufs - absoluteGateLufs) / lufsPerBin), numBins - 1);
        histogramCounts[(size_t) bin] += 1;
        histogramEnergy[(size_t) bin] += energy;
        aboveAbsoluteGateCount += 1;
        aboveAbsoluteGateEnergy += energy;

        const auto relativeGate = energyToLufs (aboveAbsoluteGateEnergy / (double) aboveAbsoluteGateCount) - 10.0;
        const auto firstBin = std::clamp ((int) std::ceil ((relativeGate - absoluteGateLufs) / lufsPerBin), 0, numBins - 1);

        std::uint64_t count = 0;
        double sum = 0.0;

        for (int i = firstBin; i < numBins; ++i)
        {
            count += histogramCounts[(size_t) i];
            sum += histogramEnergy[(size_t) i];
        }

        if (count > 0)
            integrated.store (toStoredLufs (sum / (double) count), std::memory_order_relaxed);
    }

    void restart() noexcept
    {
        for (auto& c : channels)
            c = {};

        meanProduct = 0.0f;
        blockEnergy = 0.0;
        blockSamplesDone = 0;
        resetRequested.store (false, std::memory_order_relaxed);
        resetLoudness();
    }

    void clearPublishedLevels() noexcept
    {
        correlation.store (0.0f, std::memory_order_relaxed);

        for (size_t ch = 0; ch < maxChannels; ++ch)
        {
            peaks[ch].store (0.0f, std::memory_order_relaxed);
            truePeaks[ch].store (0.0f, std::memory_order_relaxed);
            rmsLevels[ch].store (0.0f, std::memory_order_relaxed);
            vuLevels[ch].store (0.0f, std::memory_order_relaxed);
        }
    }

    void clearPublishedLoudness() noexcept
    {
        momentary.store (noLoudness, std::memory_order_relaxed);
        shortTerm.store (noLoudness, std::memory_order_relaxed);
        integrated.store (noLoudness, std::memory_order_relaxed);
    }

    void resetLoudness() noexcept
    {
        recentBlocks.fill (0.0);
        recentWriteIndex = 0;
        recentCount = 0;
        histogramCounts.fill (0);
        histogramEnergy.fill (0.0);
        aboveAbsoluteGateCount = 0;
        aboveAbsoluteGateEnergy = 0.0;
        clearPublishedLoudness();
    }

    static constexpr int blocksPerGate = 4;
    static constexpr int blocksPerShortTerm = 30;
    static constexpr double absoluteGateLufs = -70.0;
    static constexpr double lufsPerBin = 0.02;
    static constexpr int numBins = 4000;            // up to +10 LUFS

    double sampleRate = 0.0;
    Biquad shelf, highPass;
    float vuCoefficient = 1.0f, rmsCoefficient = 1.0f, meanProduct = 0.0f;
    std::array<std::array<float, truePeakTaps>, truePeakPhases> truePeakFilters {};
    bool wasMeasuring = false;
    int samplesPer100Ms = 4800;

    std::array<ChannelState, maxChannels> channels {};

    double blockEnergy = 0.0;
    int blockSamplesDone = 0;
    std::array<double, blocksPerShortTerm> recentBlocks {};
    int recentWriteIndex = 0, recentCount = 0;

    std::array<std::uint32_t, numBins> histogramCounts {};
    std::array<double, numBins> histogramEnergy {};
    std::uint64_t aboveAbsoluteGateCount = 0;
    double aboveAbsoluteGateEnergy = 0.0;

    std::array<std::atomic<float>, maxChannels> peaks {}, truePeaks {}, rmsLevels {}, vuLevels {};
    std::array<std::atomic<bool>, maxChannels> overFlags {};
    std::atomic<float> correlation { 0.0f };
    std::atomic<float> momentary { noLoudness }, shortTerm { noLoudness }, integrated { noLoudness };
    std::atomic<bool> measuring { false }, resetRequested { false };
    std::atomic<std::uint32_t> blocksProcessed { 0 };
};
