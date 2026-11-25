#include "../Source/LimiterCore.h"
#include <cstdio>
#include <cmath>
#include <random>

static float dbToLin (float db) { return std::pow (10.0f, db / 20.0f); }

int main()
{
    const double sr = 48000.0;
    const int N = 48000 * 4, blockSize = 512;
    int failures = 0;

    for (int testCase = 0; testCase < 3; ++testCase)
    {
        LimiterCore lim;
        lim.prepare (sr, 2, 5.0);
        lim.setReleaseMs (testCase == 1 ? 10.0f : 100.0f);

        std::vector<float> L (N), R (N);
        std::mt19937 rng (42 + testCase);
        std::uniform_real_distribution<float> noise (-1.0f, 1.0f);

        for (int i = 0; i < N; ++i)
        {
            float t = (float) i / (float) sr;
            float env = (testCase == 2) ? 1.0f : (0.3f + 0.7f * (std::sin (t * 3.0f) > 0.5f ? 1.0f : 0.0f));
            L[i] = 1.8f * env * std::sin (2.0f * 3.14159265f * 220.0f * t);
            R[i] = (testCase == 2 ? 3.0f : 1.2f) * env * noise (rng);
            if (i % 20000 == 0) { L[i] = 4.0f; R[i] = -4.0f; }   // isolated spikes
        }

        const float limit = dbToLin (-6.0f);
        const float makeup = dbToLin (3.0f);
        const float ceiling = dbToLin (-0.3f);

        float maxOut = 0.0f, minGain = 1.0f;
        for (int s = 0; s < N; s += blockSize)
        {
            int n = std::min (blockSize, N - s);
            float* ch[2] = { L.data() + s, R.data() + s };
            minGain = std::min (minGain, lim.process (ch, 2, n, limit, makeup, ceiling, false));
        }
        for (int i = lim.getLatencySamples(); i < N; ++i)
            maxOut = std::max ({ maxOut, std::abs (L[i]), std::abs (R[i]) });

        // Without the final clamp the limiter alone must hold limit*makeup (with tiny float tolerance)
        printf ("case %d: max out = %.4f (ceiling %.4f), min gain = %.4f\n", testCase, maxOut, ceiling, minGain);
        if (maxOut > ceiling + 1e-6f) { printf ("  FAIL: exceeded ceiling\n"); ++failures; }
    }

    // Check the limiter itself (not the clamp) is doing the work: set ceiling very high.
    {
        LimiterCore lim;
        lim.prepare (sr, 1, 5.0);
        std::vector<float> x (48000);
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = 2.5f * std::sin (2.0f * 3.14159265f * 100.0f * (float) i / 48000.0f);
        float limit = dbToLin (-6.0f);
        for (size_t s = 0; s < x.size(); s += 256)
        {
            float* ch[1] = { x.data() + s };
            lim.process (ch, 1, (int) std::min<size_t> (256, x.size() - s), limit, 1.0f, 100.0f, false);
        }
        float m = 0.0f;
        for (size_t i = lim.getLatencySamples(); i < x.size(); ++i) m = std::max (m, std::abs (x[i]));
        printf ("clamp-free peak = %.5f (limit %.5f)\n", m, limit);
        if (m > limit * 1.001f) { printf ("  FAIL: limiter overshoots without clamp\n"); ++failures; }
    }

    // Bypass: output must equal input delayed by latency.
    {
        LimiterCore lim;
        lim.prepare (sr, 1, 5.0);
        std::vector<float> x (4096), ref;
        for (size_t i = 0; i < x.size(); ++i) x[i] = std::sin ((float) i * 0.05f) * 2.0f;
        ref = x;
        for (size_t s = 0; s < x.size(); s += 256)
        {
            float* ch[1] = { x.data() + s };
            lim.process (ch, 1, 256, 0.5f, 1.0f, 1.0f, true);
        }
        int lat = lim.getLatencySamples();
        float err = 0.0f;
        for (size_t i = lat; i < x.size(); ++i) err = std::max (err, std::abs (x[i] - ref[i - lat]));
        printf ("bypass max error vs delayed input = %.8f\n", err);
        if (err > 1e-6f) { printf ("  FAIL: bypass not transparent\n"); ++failures; }
    }

    printf (failures ? "FAILED (%d)\n" : "ALL PASSED\n", failures);
    return failures != 0;
}
