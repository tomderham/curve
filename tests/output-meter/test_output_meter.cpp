// Checks the output meter's peaks.
// Build and run: tests/output-meter/run.sh

#include "../../Source/UI/OutputMeter.h"

#include <cstdio>
#include <limits>
#include <vector>

namespace
{
int failures = 0;

void check (const char* name, bool ok)
{
    std::printf ("%-4s %s\n", ok ? "ok" : "FAIL", name);
    failures += ok ? 0 : 1;
}
}

int main()
{
    std::vector<float> a (64, 0.1f), b (64, -0.5f), c (64, 0.8f), d (64, 0.2f);
    a[10] = -0.3f;
    const float* stereo[] = { a.data(), b.data() };
    const float* four[] = { a.data(), b.data(), c.data(), d.data() };
    float left = 0, right = 0;

    OutputMeter::enabled = false;
    OutputMeter::measure (stereo, 2, 64);
    OutputMeter::read (left, right);
    check ("nothing measured while not enabled", left == 0.0f && right == 0.0f);

    OutputMeter::enabled = true;
    OutputMeter::measure (stereo, 2, 64);
    OutputMeter::read (left, right);
    check ("stereo peaks, negative samples included", left == 0.3f && right == 0.5f);

    OutputMeter::read (left, right);
    check ("reading starts again from silence", left == 0.0f && right == 0.0f);

    OutputMeter::measure (four, 4, 64);
    OutputMeter::read (left, right);
    check ("outputs 3-4 count with 1-2", left == 0.8f && right == 0.5f);

    OutputMeter::measure (stereo, 2, 64);
    OutputMeter::measure (four, 4, 64);
    OutputMeter::read (left, right);
    check ("the highest peak since the last read is kept", left == 0.8f && right == 0.5f);

    const float* mono[] = { b.data() };
    OutputMeter::measure (mono, 1, 64);
    OutputMeter::read (left, right);
    check ("mono shows on both sides", left == 0.5f && right == 0.5f);

    const float* missing[] = { nullptr, b.data() };
    OutputMeter::measure (missing, 2, 64);
    OutputMeter::read (left, right);
    check ("a missing channel is skipped", left == 0.0f && right == 0.5f);

    std::vector<float> broken (64, 0.25f);
    broken[3] = std::numeric_limits<float>::infinity();
    broken[4] = std::numeric_limits<float>::quiet_NaN();
    const float* withBroken[] = { broken.data(), broken.data() };
    OutputMeter::measure (withBroken, 2, 64);
    OutputMeter::read (left, right);
    check ("samples that aren't numbers are left out", left == 0.25f && right == 0.25f);

    OutputMeter::measure (stereo, 2, 64);
    OutputMeter::setEnabled (true);
    OutputMeter::read (left, right);
    check ("turning it on again starts from silence", left == 0.0f && right == 0.0f);

    std::printf ("\n%s\n", failures == 0 ? "All passed" : "Some output meter tests FAILED");
    return failures == 0 ? 0 : 1;
}
