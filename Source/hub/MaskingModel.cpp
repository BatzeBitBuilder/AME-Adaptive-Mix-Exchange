// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "MaskingModel.h"

namespace ame
{
int MaskingResult::findTrack (int snapshotIndex) const noexcept
{
    for (int t = 0; t < numTracks(); ++t)
        if (trackIndex[(size_t) t] == snapshotIndex)
            return t;
    return -1;
}

MaskingResult computeMasking (const std::vector<TrackSnapshot>& tracks, const MaskingSettings& s)
{
    constexpr int B = kNumAnalysisBands;
    MaskingResult r;

    for (int i = 0; i < (int) tracks.size(); ++i)
        if (tracks[(size_t) i].mode == InstanceMode::Track && tracks[(size_t) i].hasAnalysis)
            r.trackIndex.push_back (i);

    const int n = r.numTracks();
    r.matrix.assign ((size_t) (n * n * B), 0.0f);
    r.maskedScore.assign ((size_t) n, 0.0f);
    r.worstMasker.assign ((size_t) n, -1);
    r.worstBandLo.assign ((size_t) n, -1);
    r.worstBandHi.assign ((size_t) n, -1);

    if (n < 2)
        return r;

    // Pegel, Relevanz und gespreizte Masker-Pegel vorberechnen
    std::vector<std::array<float, B>> level ((size_t) n), relevance ((size_t) n), spread ((size_t) n);

    for (int t = 0; t < n; ++t)
    {
        const auto& src = tracks[(size_t) r.trackIndex[(size_t) t]].bandAvgDb;
        auto& L = level[(size_t) t];
        float maxDb = -200.0f;

        for (int b = 0; b < B; ++b)
        {
            L[(size_t) b] = src[(size_t) b];
            maxDb = juce::jmax (maxDb, L[(size_t) b]);
        }

        for (int b = 0; b < B; ++b)
        {
            const float l = L[(size_t) b];
            relevance[(size_t) t][(size_t) b] =
                l <= s.floorDb ? 0.0f
                               : juce::jlimit (0.0f, 1.0f, (l - (maxDb - s.relevanceRangeDb)) / (s.relevanceRangeDb * 0.75f));

            float e = l;
            if (b > 0)     e = juce::jmax (e, L[(size_t) b - 1] - s.spreadDb);
            if (b < B - 1) e = juce::jmax (e, L[(size_t) b + 1] - s.spreadDb);
            spread[(size_t) t][(size_t) b] = e;
        }
    }

    // Matrix
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
        {
            if (i == j) continue;

            for (int b = 0; b < B; ++b)
            {
                const float masker = spread[(size_t) i][(size_t) b];
                if (masker <= s.floorDb) continue;

                const float rel = relevance[(size_t) j][(size_t) b];
                if (rel <= 0.0f) continue;

                const float diff = masker - level[(size_t) j][(size_t) b];
                const float amount = juce::jlimit (0.0f, 1.0f, (diff + s.overlapRangeDb) / (2.0f * s.overlapRangeDb));
                r.matrix[((size_t) i * (size_t) n + (size_t) j) * B + (size_t) b] = amount * rel;
            }
        }

    // Auswertung pro Spur
    for (int j = 0; j < n; ++j)
    {
        float relSum = 0.0f, maskedSum = 0.0f;
        std::array<float, B> maxPerBand {};

        for (int b = 0; b < B; ++b)
        {
            relSum += relevance[(size_t) j][(size_t) b];
            for (int i = 0; i < n; ++i)
                if (i != j)
                    maxPerBand[(size_t) b] = juce::jmax (maxPerBand[(size_t) b], r.get (i, j, b));
            maskedSum += maxPerBand[(size_t) b];
            r.hotspot[(size_t) b] += maxPerBand[(size_t) b];
        }

        r.maskedScore[(size_t) j] = relSum > 0.0f ? maskedSum / relSum : 0.0f;

        // Stärkster Masker
        float best = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            if (i == j) continue;
            float sum = 0.0f;
            for (int b = 0; b < B; ++b) sum += r.get (i, j, b);
            if (sum > best) { best = sum; r.worstMasker[(size_t) j] = i; }
        }

        if (const int w = r.worstMasker[(size_t) j]; w >= 0)
        {
            int peakBand = 0;
            for (int b = 1; b < B; ++b)
                if (r.get (w, j, b) > r.get (w, j, peakBand)) peakBand = b;

            // zusammenhängender Bereich um das Maximum (>= 50 % davon)
            const float thr = 0.5f * r.get (w, j, peakBand);
            int lo = peakBand, hi = peakBand;
            while (lo > 0 && r.get (w, j, lo - 1) >= thr) --lo;
            while (hi < B - 1 && r.get (w, j, hi + 1) >= thr) ++hi;
            r.worstBandLo[(size_t) j] = lo;
            r.worstBandHi[(size_t) j] = hi;
        }
    }

    // Hotspot normieren: ab 2 überdeckten Spuren im Band = voll
    for (auto& h : r.hotspot)
        h = juce::jlimit (0.0f, 1.0f, h * 0.5f);

    return r;
}
} // namespace ame
