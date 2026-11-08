#pragma once

#include <vector>
#include "MixHub.h"

namespace mtm
{
// Masking-Analyse auf Basis der Langzeit-Spektren aller Spur-Instanzen.
// m(i -> j, b) in 0..1: wie stark Spur i die Spur j im Band b verdeckt.
struct MaskingResult
{
    std::vector<int> trackIndex;                 // Index in den Snapshot (nur Spur-Instanzen mit Analyse)
    std::vector<float> matrix;                   // [masker][masked][band]
    std::vector<float> maskedScore;              // pro Spur 0..1 (gewichtet nach eigener Bandrelevanz)
    std::vector<int> worstMasker;                // Index in trackIndex, -1 = keiner
    std::vector<int> worstBandLo, worstBandHi;   // Bandbereich der stärksten Überdeckung
    std::array<float, kNumAnalysisBands> hotspot {};  // Belegung je Band 0..1

    int numTracks() const noexcept { return (int) trackIndex.size(); }

    float get (int masker, int masked, int band) const noexcept
    {
        const auto n = (size_t) numTracks();
        return matrix[((size_t) masker * n + (size_t) masked) * kNumAnalysisBands + (size_t) band];
    }

    int findTrack (int snapshotIndex) const noexcept;
};

struct MaskingSettings
{
    float floorDb = -70.0f;        // darunter gilt ein Band als still
    float relevanceRangeDb = 24.0f; // Bänder bis X dB unter dem eigenen Maximum sind relevant
    float overlapRangeDb = 6.0f;    // Masker ab +X dB = volle Überdeckung, ab -X dB = keine
    float spreadDb = 6.0f;          // Mithörschwelle in Nachbarbänder (Abfall je Band)
};

MaskingResult computeMasking (const std::vector<TrackSnapshot>& tracks, const MaskingSettings& s = {});
} // namespace mtm
