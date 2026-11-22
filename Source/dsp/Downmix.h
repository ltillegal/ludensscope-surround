#pragma once

#include <cstddef>

namespace surround
{
    constexpr int channelCount = 6;

    // Decoded 5.1 arrives in SMPTE/ITU order: L, R, C, LFE, Ls, Rs.
    enum Channel
    {
        left = 0,
        right = 1,
        centre = 2,
        lfe = 3,
        leftSurround = 4,
        rightSurround = 5
    };

    constexpr float surroundGain = 0.7071067811865476f; // -3 dB

    // Each surround contributes equally to both needles of the master VU in
    // the balanced ALL view. Two shares of 0.5 carry the same total power as
    // one share of 0.707 in the strict ITU sum.
    constexpr float surroundShare = 0.5f;

    // Fixed ITU downmix used by FFT, waterfall, correlation and oscilloscope.
    // LFE is deliberately not mixed into the stereo sum. The master VU uses
    // the balanced sum below in ALL mode.
    inline void downmixToStereo (float* const* input,
                                 float* leftOut, float* rightOut,
                                 int numSamples) noexcept
    {
        if (input == nullptr || leftOut == nullptr || rightOut == nullptr)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            leftOut[i]  = input[left] [i]
                        + surroundGain * input[centre][i]
                        + surroundGain * input[leftSurround][i];
            rightOut[i] = input[right][i]
                        + surroundGain * input[centre][i]
                        + surroundGain * input[rightSurround][i];
        }
    }

    // Balanced monitor sum for the master VU in ALL mode. The front L/R pair
    // stays discrete, the centre feeds both needles, and Ls/Rs are shared
    // equally so a one-sided surround cannot leave one needle on its stop.
    // LFE is excluded, as in the strict ITU sum.
    inline void downmixToStereoBalanced (float* const* input,
                                         float* leftOut, float* rightOut,
                                         int numSamples) noexcept
    {
        if (input == nullptr || leftOut == nullptr || rightOut == nullptr)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            const float centreAndSurrounds =
                  surroundGain * input[centre][i]
                + surroundShare * (input[leftSurround][i] + input[rightSurround][i]);

            leftOut[i]  = input[left][i]  + centreAndSurrounds;
            rightOut[i] = input[right][i] + centreAndSurrounds;
        }
    }
}
