// Checks Mute, Dim and Mono.
// Build and run: tests/output-monitor/run.sh

#include "../../Source/UI/OutputMonitor.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
int failures = 0;

void check (const char* name, bool ok)
{
    std::printf ("%-4s %s\n", ok ? "ok" : "FAIL", name);
    failures += ok ? 0 : 1;
}

bool near (float a, float b) { return std::abs (a - b) < 1.0e-5f; }

struct Block
{
    std::vector<float> l, r;
    Block (int n, float left, float right) : l (n, left), r (n, right) {}
    float* channels[2] = {};
    float* const* get() { channels[0] = l.data(); channels[1] = r.data(); return channels; }
};
}

int main()
{
    using namespace OutputMonitor;
    constexpr double rate = 48000.0;
    const int fade = (int) (rate * fadeSeconds);

    Processor p;
    p.prepare (rate);

    {
        Block b (64, 0.3f, -0.7f);
        p.process (b.get(), 2, 64);
        check ("all off leaves the audio untouched", b.l[10] == 0.3f && b.r[10] == -0.7f);
    }

    muted = true;
    {
        Block b (fade + 64, 1.0f, 1.0f);
        p.process (b.get(), 2, fade + 64);
        bool smooth = true;
        for (size_t i = 1; i < b.l.size(); ++i)
            smooth = smooth && std::abs (b.l[i] - b.l[i - 1]) <= 1.0f / (float) fade + 1.0e-5f;
        check ("mute fades out without a step", smooth);
        check ("then is silent", b.l.back() == 0.0f && b.r.back() == 0.0f);
    }

    muted = false;
    dimmed = true;
    {
        Block b (fade + 64, 1.0f, 1.0f);
        p.process (b.get(), 2, fade + 64);
        check ("dim settles 20 dB down", near (b.l.back(), dimGain));
    }

    dimmed = false;
    mono = true;
    {
        Block b (fade + 64, 1.0f, 0.0f);
        p.process (b.get(), 2, fade + 64);
        check ("mono settles on the average of the pair", near (b.l.back(), 0.5f) && near (b.r.back(), 0.5f));
    }

    {
        std::vector<float> c1 (fade, 0.2f), c2 (fade, 0.4f), c3 (fade, 1.0f), c4 (fade, -1.0f);
        float* four[] = { c1.data(), c2.data(), c3.data(), c4.data() };
        p.process (four, 4, fade);
        check ("each pair of outputs is made mono on its own", near (c1.back(), 0.3f) && near (c3.back(), 0.0f) && near (c4.back(), 0.0f));
    }

    mono = false;
    {
        Block b (fade + 64, 1.0f, 0.0f);
        p.process (b.get(), 2, fade + 64);
        Block after (64, 0.25f, 0.5f);
        p.process (after.get(), 2, 64);
        check ("back to untouched once everything is off again", after.l[5] == 0.25f && after.r[5] == 0.5f);
    }

    {
        std::vector<float> only (64, 0.5f);
        float* three[] = { only.data(), nullptr, only.data() };
        muted = true;
        p.prepare (rate);
        p.process (three, 3, 64);
        check ("an odd last output is muted too", only[10] == 0.0f);
        muted = false;
    }

    std::printf ("\n%s\n", failures == 0 ? "All passed" : "Some output monitor tests FAILED");
    return failures == 0 ? 0 : 1;
}
