// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "MixEngine.h"
#include "Profiles.h"
#include "dsp/SpectrumAnalyzer.h"

namespace ame
{
namespace
{
constexpr int B = kNumAnalysisBands;
constexpr float floorDb = -70.0f;
constexpr float relevanceRangeDb = 24.0f;
// Konflikt: L stört H, wenn L im Band höchstens 18 dB unter H liegt (voll ab -6 dB)
constexpr float conflictFullDb = -6.0f;
constexpr float conflictZeroDb = -18.0f;
constexpr float spreadDb = 6.0f;

float dbToPow (float db) { return std::pow (10.0f, db * 0.1f); }
} // namespace

MixEngine::MixEngine (MixHub& h) : hub (h) {}

void MixEngine::prepare (double sampleRate) noexcept
{
    intervalSamples = juce::jmax (1, juce::roundToInt (sampleRate * kIntervalMs * 0.001));
    sampleCounter = 0;
}

void MixEngine::advance (int numSamples) noexcept
{
    sampleCounter += numSamples;
    while (sampleCounter >= intervalSamples)
    {
        sampleCounter -= intervalSamples;
        step();
    }
}

void MixEngine::step() noexcept
{
    const auto genre = (Genre) juce::jlimit (0, (int) Genre::NumGenres - 1, hub.globalGenre.load());
    const auto gp = getGenreProfile (genre);

    int n = 0;
    for (int slot = 0; slot < kMaxSlots; ++slot)
    {
        auto* s = hub.getAudioData (slot);
        if (s->engMode.load (std::memory_order_acquire) != (int) InstanceMode::Track
             || s->analysisCounter.load (std::memory_order_acquire) == 0)
            continue;

        auto& t = ts[(size_t) n++];
        t.slot = slot;
        t.profile = &getRoleProfile (genre, (Role) s->engRole.load (std::memory_order_relaxed));
        t.reaction = juce::jlimit (0.0f, 1.0f, s->reaction.load (std::memory_order_relaxed));
        t.maxDb = -200.0f;
        t.cut.fill (0.0f);
        const float prioOffset = s->priorityOffset.load (std::memory_order_relaxed);

        float pow = 0.0f;
        for (int b = 0; b < B; ++b)
        {
            t.avg[(size_t) b] = s->bandAvgDb[(size_t) b].load (std::memory_order_relaxed);
            t.fast[(size_t) b] = s->bandDb[(size_t) b].load (std::memory_order_relaxed);
            t.maxDb = juce::jmax (t.maxDb, t.avg[(size_t) b]);
            if (t.avg[(size_t) b] > floorDb) pow += dbToPow (t.avg[(size_t) b]);
            t.prio[(size_t) b] = juce::jlimit (0.0f, 1.5f, getPriorityAt (*t.profile, SpectrumAnalyzer::getBandCentreHz (b))
                                                          + prioOffset);
        }
        t.totalDb = 10.0f * std::log10 (pow + 1.0e-12f);

        for (int b = 0; b < B; ++b)
        {
            const float l = t.avg[(size_t) b];
            t.shapeRel[(size_t) b] = l <= floorDb ? 0.0f
                : juce::jlimit (0.0f, 1.0f, (l - (t.maxDb - relevanceRangeDb)) / (relevanceRangeDb * 0.75f));

            float e = l;
            if (b > 0)     e = juce::jmax (e, t.avg[(size_t) b - 1] - spreadDb);
            if (b < B - 1) e = juce::jmax (e, t.avg[(size_t) b + 1] - spreadDb);
            t.spreadAvg[(size_t) b] = e;
        }
    }

    // --- Absenkungen: L (niedrigere Priorität) weicht H aus, wo beide im selben Band konkurrieren ---
    for (int l = 0; l < n; ++l)
    {
        auto& L = ts[(size_t) l];
        for (int h = 0; h < n; ++h)
        {
            if (h == l) continue;
            const auto& H = ts[(size_t) h];

            for (int b = 0; b < B; ++b)
            {
                const auto ub = (size_t) b;
                if (H.shapeRel[ub] <= 0.0f || L.spreadAvg[ub] <= floorDb) continue;

                const float dp = H.prio[ub] - L.prio[ub];
                float w;
                if (dp > 0.1f)            w = juce::jlimit (0.0f, 1.0f, dp / 0.4f);
                else if (dp >= -0.1f)     w = 0.3f;   // gleichrangig: beide geben etwas nach
                else                      continue;

                // Aktivität von H: bei schneller Reaktion folgt sie dem Kurzzeitpegel (Ducking)
                const float hNow = H.avg[ub] + L.reaction * (H.fast[ub] - H.avg[ub]);
                const float gate = juce::jlimit (0.0f, 1.0f, (hNow - (H.avg[ub] - 20.0f)) / 20.0f);

                const float diff = L.spreadAvg[ub] - H.avg[ub];
                const float amount = juce::jlimit (0.0f, 1.0f, (diff - conflictZeroDb) / (conflictFullDb - conflictZeroDb));

                L.cut[ub] = juce::jmax (L.cut[ub], amount * H.shapeRel[ub] * gate * w);
            }
        }
    }

    // --- Lautstärke: Rollen-Verhältnisse herstellen, Summenleistung konstant halten ---
    // Ziel je Spur: ref + levelDb. ref so gewählt, dass Σ Leistung vorher = nachher;
    // wegen der Begrenzung iterativ nachgeführt.
    std::fill (gains.begin(), gains.begin() + n, 0.0f);
    {
        int numActive = 0;
        float powBefore = 0.0f;
        for (int i = 0; i < n; ++i)
            if (ts[(size_t) i].totalDb > -60.0f) { active[(size_t) numActive++] = i; powBefore += dbToPow (ts[(size_t) i].totalDb); }

        if (numActive > 1)
        {
            float targetPow = 0.0f;
            for (int a = 0; a < numActive; ++a) targetPow += dbToPow (ts[(size_t) active[(size_t) a]].profile->levelDb);
            float ref = 10.0f * std::log10 (powBefore / targetPow);

            for (int iter = 0; iter < 4; ++iter)
            {
                float powAfter = 0.0f;
                for (int a = 0; a < numActive; ++a)
                {
                    const int i = active[(size_t) a];
                    const auto& t = ts[(size_t) i];
                    gains[(size_t) i] = juce::jlimit (-gp.maxGainDb, gp.maxGainDb, ref + t.profile->levelDb - t.totalDb);
                    powAfter += dbToPow (t.totalDb + gains[(size_t) i]);
                }
                const float err = 10.0f * std::log10 (powBefore / juce::jmax (1.0e-12f, powAfter));
                if (std::abs (err) < 0.05f) break;
                ref += err;
            }
        }
    }

    // --- Ausgabe ---
    for (int ti = 0; ti < n; ++ti)
    {
        const auto& t = ts[(size_t) ti];
        auto* d = hub.getAudioData (t.slot);
        if (d == nullptr) continue;

        // über Bänder glätten
        std::array<float, B> sm {};
        for (int b = 0; b < B; ++b)
        {
            const float prev = t.cut[(size_t) juce::jmax (0, b - 1)];
            const float next = t.cut[(size_t) juce::jmin (B - 1, b + 1)];
            sm[(size_t) b] = 0.25f * prev + 0.5f * t.cut[(size_t) b] + 0.25f * next;
        }

        // auf Oktavbänder abbilden
        for (int k = 0; k < kNumAdaptiveBands; ++k)
        {
            const float fc = getAdaptiveBandHz (k);
            float sum = 0.0f; int cnt = 0;
            for (int b = 0; b < B; ++b)
            {
                const float f = SpectrumAnalyzer::getBandCentreHz (b);
                if (f >= fc / juce::MathConstants<float>::sqrt2 && f < fc * juce::MathConstants<float>::sqrt2)
                { sum += sm[(size_t) b]; ++cnt; }
            }
            const float c = cnt > 0 ? sum / (float) cnt : 0.0f;
            d->targetCutDb[(size_t) k].store (-gp.maxCutDb * c, std::memory_order_relaxed);
        }

        // Hochpass: unterhalb des Nutzsignals, höchstens Rollen-Maximum
        float hpf = 0.0f;
        for (int b = 0; b < B; ++b)
        {
            if (t.avg[(size_t) b] > t.maxDb - 30.0f)
            {
                hpf = juce::jmin (t.profile->hpfMaxHz, SpectrumAnalyzer::getBandEdgeHz (b) * 0.8f);
                break;
            }
        }
        d->targetHpfHz.store (hpf >= 25.0f ? hpf : 0.0f, std::memory_order_relaxed);

        // Lautstärke
        d->targetGainDb.store (gains[(size_t) ti], std::memory_order_relaxed);
    }

    // Pegelangleich für die Roh-Abhöre: Summenleistung bearbeitet / unbearbeitet (Spuren als unkorreliert genähert)
    {
        double dry = 0.0, wet = 0.0;
        for (int slot = 0; slot < kMaxSlots; ++slot)
            if (auto* d = hub.getAudioData (slot); d->engMode.load (std::memory_order_relaxed) == (int) InstanceMode::Track)
            {
                dry += d->dryPow.load (std::memory_order_relaxed);
                wet += d->wetPow.load (std::memory_order_relaxed);
            }
        const float db = (dry > 1.0e-9 && wet > 1.0e-9) ? (float) (10.0 * std::log10 (wet / dry)) : 0.0f;
        hub.rawMatchDb.store (juce::jlimit (-12.0f, 12.0f, db), std::memory_order_relaxed);
    }

    hub.engineHeartbeatMs.store (juce::jmax (1u, juce::Time::getMillisecondCounter()), std::memory_order_relaxed);
    stepCount.fetch_add (1, std::memory_order_relaxed);
}
} // namespace ame
