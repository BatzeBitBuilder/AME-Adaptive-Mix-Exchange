// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cmath>
#include <array>

namespace mtm
{
// RBJ-Biquad (Transposed Direct Form II), bis 2 Kanäle.
class Biquad
{
public:
    enum class Type { HighPass, LowPass, LowShelf, Peak, HighShelf };

    void setup (Type type, double sampleRate, double freq, double q, double gainDb) noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        freq = std::fmin (freq, sampleRate * 0.49);
        const double w0 = 2.0 * pi * freq / sampleRate;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        const double A = std::pow (10.0, gainDb / 40.0);

        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

        switch (type)
        {
            case Type::HighPass:
                b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
                a0 = 1 + alpha;    a1 = -2 * cw;   a2 = 1 - alpha;
                break;
            case Type::LowPass:
                b0 = (1 - cw) / 2; b1 = 1 - cw;    b2 = (1 - cw) / 2;
                a0 = 1 + alpha;    a1 = -2 * cw;   a2 = 1 - alpha;
                break;
            case Type::Peak:
                b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
                a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
                break;
            case Type::LowShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) - (A - 1) * cw + sq);
                b1 = 2 * A * ((A - 1) - (A + 1) * cw);
                b2 = A * ((A + 1) - (A - 1) * cw - sq);
                a0 = (A + 1) + (A - 1) * cw + sq;
                a1 = -2 * ((A - 1) + (A + 1) * cw);
                a2 = (A + 1) + (A - 1) * cw - sq;
                break;
            }
            case Type::HighShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) + (A - 1) * cw + sq);
                b1 = -2 * A * ((A - 1) + (A + 1) * cw);
                b2 = A * ((A + 1) + (A - 1) * cw - sq);
                a0 = (A + 1) - (A - 1) * cw + sq;
                a1 = 2 * ((A - 1) - (A + 1) * cw);
                a2 = (A + 1) - (A - 1) * cw - sq;
                break;
            }
        }

        c0 = (float) (b0 / a0); c1 = (float) (b1 / a0); c2 = (float) (b2 / a0);
        d1 = (float) (a1 / a0); d2 = (float) (a2 / a0);
    }

    inline float process (float x, int ch) noexcept
    {
        auto& s = state[(size_t) ch];
        const float y = c0 * x + s[0];
        s[0] = c1 * x - d1 * y + s[1];
        s[1] = c2 * x - d2 * y;
        return y;
    }

    void reset() noexcept { state = {}; }

    // Betragsgang in dB (für Anzeige)
    float getMagnitudeDb (double freq, double sampleRate) const noexcept
    {
        const double w = 2.0 * 3.14159265358979323846 * freq / sampleRate;
        const double c1w = std::cos (w), s1w = std::sin (w), c2w = std::cos (2 * w), s2w = std::sin (2 * w);
        const double nr = c0 + c1 * c1w + c2 * c2w, ni = -(c1 * s1w + c2 * s2w);
        const double dr = 1.0 + d1 * c1w + d2 * c2w, di = -(d1 * s1w + d2 * s2w);
        const double mag2 = (nr * nr + ni * ni) / (dr * dr + di * di + 1.0e-30);
        return (float) (10.0 * std::log10 (mag2 + 1.0e-30));
    }

private:
    float c0 = 1, c1 = 0, c2 = 0, d1 = 0, d2 = 0;
    std::array<std::array<float, 2>, 2> state {};
};
} // namespace mtm
