#pragma once
// Framework-independent lookahead peak limiter (no JUCE dependency).
//
// Per sample: required gain -> sliding minimum over the lookahead window ->
// release smoothing -> moving average over the window. Because the gain
// starts dropping one full window before the peak arrives and the minimum
// stage guarantees every averaged value is <= the required gain, the limited
// signal never exceeds the limit level (no clipping, no overshoot).

#include <algorithm>
#include <cmath>
#include <vector>

class LimiterCore
{
public:
    void prepare (double sampleRate, int numChannels, double lookaheadMs = 5.0)
    {
        sr = sampleRate;
        window = std::max (2, (int) std::lround (sampleRate * lookaheadMs * 0.001));
        delay.assign ((size_t) numChannels, std::vector<float> ((size_t) window, 0.0f));
        targetHist.assign ((size_t) window, 1.0f);
        avgHist.assign ((size_t) window, 1.0f);
        avgSum = (double) window;
        env = 1.0f;
        pos = 0;
        setReleaseMs (100.0f);
    }

    int getLatencySamples() const { return window - 1; }

    void setReleaseMs (float ms)
    {
        releaseCoeff = (float) std::exp (-1.0 / (std::max (1.0f, ms) * 0.001 * sr));
    }

    // limitLin:  level the signal is limited to (before makeup gain)
    // makeupLin: gain applied after limiting
    // ceilingLin: final safety clamp (never reached when limitLin * makeupLin <= ceilingLin)
    // Returns the lowest gain applied in this block (1.0 = no reduction).
    float process (float* const* data, int numChannels, int numSamples,
                   float limitLin, float makeupLin, float ceilingLin, bool bypass)
    {
        float blockMinGain = 1.0f;
        limitLin = std::max (limitLin, 1.0e-6f);

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = std::max (peak, std::abs (data[ch][i]));

            const float target = peak > limitLin ? limitLin / peak : 1.0f;
            targetHist[(size_t) pos] = target;

            float minG = 1.0f;
            for (float t : targetHist)
                minG = std::min (minG, t);

            if (minG < env)
                env = minG;
            else
                env = minG + (env - minG) * releaseCoeff;

            avgSum += (double) env - (double) avgHist[(size_t) pos];
            avgHist[(size_t) pos] = env;
            const float gain = (float) (avgSum / (double) window);
            blockMinGain = std::min (blockMinGain, gain);

            const int rd = (pos + 1) % window;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& d = delay[(size_t) ch];
                d[(size_t) pos] = data[ch][i];
                const float delayed = d[(size_t) rd];

                if (bypass)
                    data[ch][i] = delayed;
                else
                    data[ch][i] = std::clamp (delayed * gain * makeupLin, -ceilingLin, ceilingLin);
            }
            pos = rd;
        }
        return blockMinGain;
    }

private:
    double sr = 44100.0;
    int window = 2, pos = 0;
    float env = 1.0f, releaseCoeff = 0.999f;
    double avgSum = 2.0;
    std::vector<std::vector<float>> delay;
    std::vector<float> targetHist, avgHist;
};
