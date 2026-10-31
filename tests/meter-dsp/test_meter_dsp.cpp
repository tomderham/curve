// Checks MeterDsp against EBU Tech 3341 / BS.1770 and the VU standard.
// Build and run: tests/meter-dsp/run.sh

#include "../../Source/Plugins/MeterDsp.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace
{
constexpr double pi = 3.14159265358979323846;
int failures = 0;

void check (const char* name, double actual, double expected, double tolerance)
{
    const bool ok = std::abs (actual - expected) <= tolerance;
    std::printf ("%-4s %-46s %9.3f (expected %.3f +/- %.3f)\n", ok ? "ok" : "FAIL", name, actual, expected, tolerance);
    failures += ok ? 0 : 1;
}

// Feeds stereo sine in uneven block sizes, as a device would
void feed (MeterDsp& meter, double sampleRate, double frequency, double dbfs, double seconds, double right = 1.0, int channels = 2)
{
    static double phase = 0.0;
    const auto gain = std::pow (10.0, dbfs / 20.0);
    auto remaining = (long) (seconds * sampleRate);
    const int sizes[] = { 64, 128, 37, 512, 1024 };
    std::vector<float> left (1024), rightBuffer (1024);

    for (int block = 0; remaining > 0; ++block)
    {
        const auto n = (int) std::min<long> (remaining, sizes[block % 5]);

        for (int i = 0; i < n; ++i)
        {
            const auto sample = gain * std::sin (phase);
            left[(size_t) i] = (float) sample;
            rightBuffer[(size_t) i] = (float) (sample * right);
            phase = std::fmod (phase + 2.0 * pi * frequency / sampleRate, 2.0 * pi);
        }

        const float* data[] = { left.data(), rightBuffer.data() };
        meter.process (data, channels, n);
        remaining -= n;
    }
}

std::unique_ptr<MeterDsp> makeMeter (double sampleRate)
{
    auto meter = std::make_unique<MeterDsp>();
    meter->prepare (sampleRate);
    meter->setMeasuring (true);
    return meter;
}
}

int main()
{
    for (const auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        std::printf ("-- %.0f Hz\n", rate);

        // Tech 3341 case 1: stereo -23 dBFS 1 kHz sine for 20 s
        auto meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -23.0, 20.0);
        check ("3341-1 momentary", meter->getMomentaryLufs(), -23.0, 0.1);
        check ("3341-1 short-term", meter->getShortTermLufs(), -23.0, 0.1);
        check ("3341-1 integrated", meter->getIntegratedLufs(), -23.0, 0.1);

        // Case 2: the same at -33 dBFS
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -33.0, 20.0);
        check ("3341-2 integrated", meter->getIntegratedLufs(), -33.0, 0.1);

        // Case 3: -36, -23, -36 dBFS for 10, 60, 10 s (the quiet parts fall to the relative gate)
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -36.0, 10.0);
        feed (*meter, rate, 1000.0, -23.0, 60.0);
        feed (*meter, rate, 1000.0, -36.0, 10.0);
        check ("3341-3 integrated", meter->getIntegratedLufs(), -23.0, 0.1);

        // Case 4: -72, -36, -23, -36, -72 dBFS for 10, 10, 60, 10, 10 s
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -72.0, 10.0);
        feed (*meter, rate, 1000.0, -36.0, 10.0);
        feed (*meter, rate, 1000.0, -23.0, 60.0);
        feed (*meter, rate, 1000.0, -36.0, 10.0);
        feed (*meter, rate, 1000.0, -72.0, 10.0);
        check ("3341-4 integrated", meter->getIntegratedLufs(), -23.0, 0.1);

        // Case 5: -26, -20, -26 dBFS for 20, 20.1, 20 s
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -26.0, 20.0);
        feed (*meter, rate, 1000.0, -20.0, 20.1);
        feed (*meter, rate, 1000.0, -26.0, 20.0);
        check ("3341-5 integrated", meter->getIntegratedLufs(), -23.0, 0.1);

        // VU: a -18 dBFS sine reads 0 VU, in either channel, and the peak is the sine's
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -18.0, 1.0);
        check ("VU of -18 dBFS sine (dB)", 20.0 * std::log10 (meter->getVu (0)), -18.0, 0.1);
        check ("sample peak (dB)", 20.0 * std::log10 (meter->consumePeak (1)), -18.0, 0.05);
        check ("peak is cleared by reading", meter->consumePeak (1), 0.0, 0.0);

        // RMS: a sine reads 3 dB below its peak, and settles to 95% of its power in 300 ms
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -18.0, 2.0);
        check ("RMS of -18 dBFS sine (dB)", 20.0 * std::log10 (meter->getRms (0)), -21.01, 0.05);
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, 0.0, 0.3);
        check ("RMS power after 300 ms (fraction)", 2.0 * meter->getRms (0) * meter->getRms (0), 0.95, 0.02);

        // True peak: at a quarter of the sample rate and 45 degrees, every sample is at
        // 0.707 of the real peak. The reading must find the 3 dB that sample peak misses.
        meter = makeMeter (rate);
        {
            std::vector<float> samples (4096);

            for (size_t i = 0; i < samples.size(); ++i)
                samples[i] = (float) (0.5 * std::sin (pi / 2.0 * (double) i + pi / 4.0));

            const float* data[] = { samples.data(), samples.data() };
            meter->process (data, 2, (int) samples.size());
            check ("sample peak misses it (dB)", 20.0 * std::log10 (meter->consumePeak (0)), -9.03, 0.05);
            check ("true peak finds it (dB)", 20.0 * std::log10 (meter->consumeTruePeak (0)), -6.02, 0.25);
        }

        // At low frequencies samples already show the peak, so true peak must agree
        meter = makeMeter (rate);
        feed (*meter, rate, 997.0, -6.0, 1.0);
        check ("true peak of a low sine (dB)", 20.0 * std::log10 (meter->consumeTruePeak (0)), -6.0, 0.05);

        // Correlation
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -12.0, 1.0);
        check ("correlation of identical channels", meter->getCorrelation(), 1.0, 0.01);
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -12.0, 1.0, -1.0);
        check ("correlation of inverted channels", meter->getCorrelation(), -1.0, 0.01);
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -12.0, 1.0, 0.0);
        check ("correlation with one silent channel", meter->getCorrelation(), 0.0, 0.01);

        // Partial correlation. Two sines 72 degrees apart read cos 72 degrees, and
        // two equal noises sharing half their power read 0.5.
        {
            const auto numSamples = (int) rate;
            std::vector<float> left ((size_t) numSamples), right ((size_t) numSamples);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto phase = 2.0 * pi * 1000.0 * i / rate;
                left[(size_t) i] = (float) (0.25 * std::sin (phase));
                right[(size_t) i] = (float) (0.25 * std::sin (phase + 2.0 * pi / 5.0));
            }

            const float* data[] = { left.data(), right.data() };
            meter = makeMeter (rate);
            meter->process (data, 2, numSamples);
            check ("correlation of sines 72 degrees apart", meter->getCorrelation(), 0.309, 0.01);

            const auto noise = [] { return (float) std::rand() / (float) RAND_MAX - 0.5f; };

            for (int i = 0; i < numSamples; ++i)
            {
                const auto shared = noise();
                left[(size_t) i] = 0.25f * (shared + noise());
                right[(size_t) i] = 0.25f * (shared + noise());
            }

            meter = makeMeter (rate);
            meter->process (data, 2, numSamples);
            check ("correlation of half-shared noise", meter->getCorrelation(), 0.5, 0.05);
        }

        // Stopped: nothing is measured or counted, and starting again begins afresh
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -23.0, 5.0);
        const auto blocksBefore = meter->getBlocksProcessed();
        meter->setMeasuring (false);
        feed (*meter, rate, 1000.0, -6.0, 1.0);
        check ("no blocks counted while stopped", meter->getBlocksProcessed() - blocksBefore, 0.0, 0.0);
        check ("nothing measured while stopped", meter->getIntegratedLufs(), -23.0, 0.1);
        meter->setMeasuring (true);
        check ("readings clear on start", meter->getIntegratedLufs(), MeterDsp::noLoudness, 0.0);
        feed (*meter, rate, 1000.0, -30.0, 5.0);
        check ("integrated restarts on start", meter->getIntegratedLufs(), -30.0, 0.1);

        // VU ballistics: 99% of the final reading 300 ms after a step
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -18.0, 0.3);
        check ("VU rise after 300 ms (fraction)", meter->getVu (0) / std::pow (10.0, -18.0 / 20.0), 0.99, 0.015);

        // Overs: full scale exactly isn't over, anything above is, and it latches until reset
        meter = makeMeter (rate);
        float sample = 1.0f;
        const float* fullScale[] = { &sample, &sample };
        meter->process (fullScale, 2, 1);
        check ("no over at exactly full scale", meter->hasOver (0), 0.0, 0.0);
        float over = 1.001f;
        const float* overData[] = { &sample, &over };
        meter->process (overData, 2, 1);
        check ("over on the right", meter->hasOver (1), 1.0, 0.0);
        check ("no over on the left", meter->hasOver (0), 0.0, 0.0);
        feed (*meter, rate, 1000.0, -60.0, 0.1);
        check ("over stays latched", meter->hasOver (1), 1.0, 0.0);
        meter->clearOvers();
        check ("over clears", meter->hasOver (1), 0.0, 0.0);

        // Reset clears integrated loudness, and silence doesn't count towards it
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -23.0, 5.0);
        feed (*meter, rate, 1000.0, -200.0, 5.0);
        // Three blocks overlap the end of the tone and pass the gate, at 3/4, 1/2 and 1/4 energy
        check ("silence is gated out", meter->getIntegratedLufs(), -23.135, 0.03);
        meter->requestLoudnessReset();
        feed (*meter, rate, 1000.0, -30.0, 5.0);
        check ("integrated restarts after reset", meter->getIntegratedLufs(), -30.0, 0.1);

        // The device restarting at the same rate keeps it; a new rate clears it
        meter->prepare (rate);
        check ("integrated survives restart", meter->getIntegratedLufs(), -30.0, 0.1);
        meter->prepare (rate * 2.0);
        check ("integrated clears on new rate", meter->getIntegratedLufs(), MeterDsp::noLoudness, 0.0);

        // Mono counts one channel, so reads 3 dB below the same signal on both
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -23.0, 5.0, 1.0, 1);
        check ("mono integrated", meter->getIntegratedLufs(), -26.01, 0.1);

        // Anti-phase channels must still read in full
        meter = makeMeter (rate);
        feed (*meter, rate, 1000.0, -23.0, 5.0, -1.0);
        check ("anti-phase integrated", meter->getIntegratedLufs(), -23.0, 0.1);
    }

    std::printf (failures == 0 ? "\nAll passed\n" : "\n%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
