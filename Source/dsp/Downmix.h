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

    // Fixed ITU downmix used by the legacy stereo views (FFT, waterfall,
    // VU, oscilloscope). LFE is deliberately not mixed into the stereo sum.
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
}
