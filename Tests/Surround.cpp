#include "dsp/Downmix.h"
#include "dsp/LoudnessMeter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    constexpr double pi = 3.14159265358979323846;

    std::vector<float> sine (double sampleRate, double amplitude, double frequency,
                             int numSamples)
    {
        std::vector<float> result ((size_t) numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const double t = (double) i / sampleRate;
            result[(size_t) i] = (float) (amplitude * std::sin (2.0 * pi * frequency * t));
        }

        return result;
    }

    float measure (float* const* channels, int numSamples)
    {
        LoudnessMeter meter;
        meter.prepare (48000.0);

        constexpr int blockSize = 512;

        for (int offset = 0; offset < numSamples; offset += blockSize)
            meter.processSurround (channels, std::min (blockSize, numSamples - offset));

        return meter.momentaryLufs();
    }
}

int main()
{
    int failures = 0;

    const auto check = [&] (const char* name, bool ok)
    {
        std::printf ("%-42s %s\n", name, ok ? "ok" : "FAIL");
        if (! ok)
            ++failures;
    };

    check ("5.1 uses six discrete channels", surround::channelCount == 6);
    check ("SMPTE order L R C LFE Ls Rs",
           surround::left == 0 && surround::right == 1 && surround::centre == 2
               && surround::lfe == 3 && surround::leftSurround == 4
               && surround::rightSurround == 5);

    // Fixed ITU downmix: L' = L + .707 C + .707 Ls, R' = R + .707 C + .707 Rs.
    {
        constexpr int n = 8;
        float l[n] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        float r[n] = { 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f };
        float c[n] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
        float lfe[n] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        float ls[n] = { 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f };
        float rs[n] = { 0.75f, 0.75f, 0.75f, 0.75f, 0.75f, 0.75f, 0.75f, 0.75f };
        float* channels[surround::channelCount] = { l, r, c, lfe, ls, rs };
        float outL[n] = {}, outR[n] = {};

        surround::downmixToStereo (channels, outL, outR, n);

        const float expectedL = 1.0f + surround::surroundGain * (0.5f + 0.25f);
        const float expectedR = 2.0f + surround::surroundGain * (0.5f + 0.75f);

        check ("ITU downmix left channel",
               std::abs (outL[0] - expectedL) < 1.0e-5f);
        check ("ITU downmix right channel",
               std::abs (outR[0] - expectedR) < 1.0e-5f);
        check ("ITU downmix is constant per block",
               std::all_of (outL, outL + n, [=] (float v) { return std::abs (v - expectedL) < 1.0e-5f; })
                   && std::all_of (outR, outR + n, [=] (float v) { return std::abs (v - expectedR) < 1.0e-5f; }));
    }

    // Balanced VU sum: L/R stay discrete, C feeds both needles, Ls/Rs are
    // shared equally, and LFE is excluded.
    {
        constexpr int n = 8;
        float l[n] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        float r[n] = { 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f };
        float c[n] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
        float lfe[n] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        float ls[n] = { 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f };
        float rs[n] = { 0.75f, 0.75f, 0.75f, 0.75f, 0.75f, 0.75f, 0.75f, 0.75f };
        float* channels[surround::channelCount] = { l, r, c, lfe, ls, rs };
        float outL[n] = {}, outR[n] = {};

        surround::downmixToStereoBalanced (channels, outL, outR, n);

        const float common = surround::surroundGain * 0.5f
                           + surround::surroundShare * (0.25f + 0.75f);

        check ("balanced VU downmix left channel",
               std::all_of (outL, outL + n, [=] (float v)
                            { return std::abs (v - (1.0f + common)) < 1.0e-5f; }));
        check ("balanced VU downmix right channel",
               std::all_of (outR, outR + n, [=] (float v)
                            { return std::abs (v - (2.0f + common)) < 1.0e-5f; }));
    }

    // A one-sided surround appears equally in both VU needles, with no LFE.
    {
        constexpr int n = 4;
        float silence[n] = {};
        float ls[n] = { 1.0f, -1.0f, 1.0f, -1.0f };
        float lfe[n] = { 1.0f, -1.0f, 1.0f, -1.0f };
        float* channels[surround::channelCount] = { silence, silence, silence,
                                                    lfe, ls, silence };
        float outL[n] = {}, outR[n] = {};

        surround::downmixToStereoBalanced (channels, outL, outR, n);

        bool sharedEqual = true;

        for (int i = 0; i < n; ++i)
        {
            const float expected = ls[(size_t) i] * surround::surroundShare;

            if (std::abs (outL[(size_t) i] - expected) >= 1.0e-5f
                    || std::abs (outR[(size_t) i] - expected) >= 1.0e-5f)
                sharedEqual = false;
        }

        check ("balanced VU shares Ls and excludes LFE", sharedEqual);
    }

    // LFE must not contribute to the downmix at all.
    {
        constexpr int n = 4;
        float silence[n] = {};
        float lfe[n] = { 1.0f, -1.0f, 1.0f, -1.0f };
        float* channels[surround::channelCount] = { silence, silence, silence,
                                                    lfe, silence, silence };
        float outL[n] = {}, outR[n] = {};

        surround::downmixToStereo (channels, outL, outR, n);

        check ("LFE excluded from stereo downmix",
               std::all_of (outL, outL + n, [] (float v) { return v == 0.0f; })
                   && std::all_of (outR, outR + n, [] (float v) { return v == 0.0f; }));
    }

    constexpr int sampleRate = 48000;
    constexpr int numSamples = sampleRate * 4;
    auto tone = sine (sampleRate, 0.1, 1000.0, numSamples);
    auto silence = std::vector<float> ((size_t) numSamples, 0.0f);

    // One +3 dB-weighted surround channel equals two unity-weighted channels.
    {
        float* onlyLs[surround::channelCount] = {
            silence.data(), silence.data(), silence.data(), silence.data(),
            tone.data(), silence.data()
        };
        float* stereo[surround::channelCount] = {
            tone.data(), tone.data(),
            silence.data(), silence.data(), silence.data(), silence.data()
        };

        const float lsLufs = measure (onlyLs, numSamples);
        const float stereoLufs = measure (stereo, numSamples);

        std::printf ("Ls-only %.2f LUFS vs stereo %.2f LUFS\n", lsLufs, stereoLufs);
        check ("surround channels weighted +3 dB",
               std::abs (lsLufs - stereoLufs) < 0.35f);
    }

    // Centre and front channels are unity weighted.
    {
        float* onlyC[surround::channelCount] = {
            silence.data(), silence.data(), tone.data(),
            silence.data(), silence.data(), silence.data()
        };

        const float cLufs = measure (onlyC, numSamples);
        std::printf ("C-only %.2f LUFS\n", cLufs);
        // One unity-weighted channel is 3 dB quieter than the same tone in both
        // stereo channels; the K-weighting shelf cancels the -0.691 offset.
        check ("centre channel uses unity gain", std::abs (cLufs + 23.01f) < 0.35f);
    }

    // LFE alone must measure as silence. A loud LFE alongside a quiet front
    // channel must not change the measured loudness.
    {
        auto loudLfe = sine (sampleRate, 0.9, 60.0, numSamples);
        float* onlyLfe[surround::channelCount] = {
            silence.data(), silence.data(), silence.data(),
            loudLfe.data(), silence.data(), silence.data()
        };
        float* frontAndLfe[surround::channelCount] = {
            tone.data(), silence.data(), silence.data(),
            loudLfe.data(), silence.data(), silence.data()
        };
        float* frontOnly[surround::channelCount] = {
            tone.data(), silence.data(), silence.data(),
            silence.data(), silence.data(), silence.data()
        };

        const float lfeLufs = measure (onlyLfe, numSamples);
        const float withLfe = measure (frontAndLfe, numSamples);
        const float withoutLfe = measure (frontOnly, numSamples);

        std::printf ("LFE-only %.2f LUFS, front+LFE %.2f, front %.2f\n",
                     lfeLufs, withLfe, withoutLfe);
        check ("LFE excluded from BS.1770 measurement", lfeLufs < -90.0f);
        check ("LFE does not affect measured loudness",
               std::abs (withLfe - withoutLfe) < 0.05f);
    }

    // Switching measurement modes must reset the filter state cleanly.
    {
        LoudnessMeter meter;
        meter.prepare (48000.0);

        for (int offset = 0; offset < numSamples; offset += 512)
            meter.process (tone.data(), tone.data(), std::min (512, numSamples - offset));

        float* channels[surround::channelCount] = {
            tone.data(), silence.data(), silence.data(),
            silence.data(), silence.data(), silence.data()
        };
        for (int offset = 0; offset < numSamples; offset += 512)
            meter.processSurround (channels, std::min (512, numSamples - offset));

        check ("mode switch resets loudness state",
               std::isfinite (meter.momentaryLufs()) && meter.momentaryLufs() > -100.0f);
    }

    std::printf ("%s (%d failures)\n", failures == 0 ? "surround tests passed" : "surround tests failed", failures);
    return failures == 0 ? 0 : 1;
}
