// Checks OutputVolume against macOS's volume curve and key steps.
// Build and run: tests/output-volume/run.sh

#include "../../Source/UI/OutputVolume.h"

#include <cstdio>

namespace
{
int failures = 0;

void check (const char* name, double actual, double expected, double tolerance = 0.0)
{
    const bool ok = std::abs (actual - expected) <= tolerance;
    std::printf ("%-4s %-48s %9.3f (expected %.3f)\n", ok ? "ok" : "FAIL", name, actual, expected);
    failures += ok ? 0 : 1;
}

void checkGainMatchesDb (int steps)
{
    const double db = 20.0 * std::log10 ((double) OutputVolume::stepsToGain (steps, false));
    check ("gain matches dB", db, OutputVolume::stepsToDb (steps), 0.001);
}
}

int main()
{
    using namespace OutputVolume;

    // macOS's curve for the MacBook Pro speakers (kAudioDevicePropertyVolumeScalarToDecibels):
    // -59.53 dB at 1/16, -31.75 dB at 1/2, -3.97 dB at 15/16, 0 dB at full
    check ("1 key step is -59.53 dB", stepsToDb (4), -59.53, 0.01);
    check ("8 key steps is -31.75 dB", stepsToDb (32), -31.75, 0.01);
    check ("15 key steps is -3.97 dB", stepsToDb (60), -3.97, 0.01);
    check ("full is 0 dB", stepsToDb (maxSteps), 0.0);

    check ("full is exactly unity", stepsToGain (maxSteps, false), 1.0);
    check ("0 is silent", stepsToGain (0, false), 0.0);
    check ("muted is silent", stepsToGain (maxSteps, true), 0.0);
    check ("out of range clamps to full", stepsToGain (maxSteps + 10, false), 1.0);
    check ("first fine step is audible", stepsToGain (1, false) > 0.0f ? 1.0 : 0.0, 1.0);
    checkGainMatchesDb (4);
    checkGainMatchesDb (37);

    check ("up from 0", stepVolume (0, true, false), 4);
    check ("up from full stays full", stepVolume (maxSteps, true, false), maxSteps);
    check ("down from full", stepVolume (maxSteps, false, false), 60);
    check ("down from 0 stays 0", stepVolume (0, false, false), 0);
    check ("up from a fine level rounds up", stepVolume (5, true, false), 8);
    check ("down from a fine level rounds down", stepVolume (5, false, false), 4);
    check ("down from a whole step", stepVolume (8, false, false), 4);
    check ("fine up", stepVolume (8, true, true), 9);
    check ("fine down", stepVolume (8, false, true), 7);
    check ("fine down from 0 stays 0", stepVolume (0, false, true), 0);

    int steps = 0;
    for (int i = 0; i < 16; ++i)
        steps = stepVolume (steps, true, false);
    check ("16 key presses from silent reach full", steps, maxSteps);

    std::printf (failures == 0 ? "\nAll passed\n" : "\n%d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
