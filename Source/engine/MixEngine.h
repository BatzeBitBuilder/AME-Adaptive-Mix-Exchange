// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_core/juce_core.h>
#include "hub/MixHub.h"
#include "engine/Profiles.h"

namespace ame
{
// Adaptive Engine: läuft in der aktiven Master-Instanz (Exchange), getaktet vom Audio des Exchange:
// ein Rechenschritt je 20 ms Audio – im Echtzeitbetrieb wie beim Offline-Export gleich (Export = Abhöre).
// Liest Spektren (vor den adaptiven Eingriffen) aller Spuren und schreibt Zielwerte
// (Oktavband-Absenkungen, Hochpass, Lautstärke) in den Hub. Echtzeit-sicher: keine Sperren, keine Allokationen.
class MixEngine
{
public:
    static constexpr int kIntervalMs = 20;

    explicit MixEngine (MixHub& hub);

    void prepare (double sampleRate) noexcept;
    void reset() noexcept { sampleCounter = 0; }

    // Audio-Thread des Exchange: Audio-Zeit weiterzählen, je 20 ms einen Schritt rechnen
    void advance (int numSamples) noexcept;

    void step() noexcept;   // ein Rechenschritt (auch synchron für Tests)
    uint64_t getStepCount() const noexcept { return stepCount.load (std::memory_order_relaxed); }

private:
    struct T
    {
        int slot = -1;
        const RoleProfile* profile = nullptr;
        std::array<float, kNumAnalysisBands> avg {}, fast {}, shapeRel {}, spreadAvg {}, prio {}, cut {};
        float maxDb = -200.0f, totalDb = -100.0f, reaction = 0.3f;
    };

    MixHub& hub;
    std::array<T, kMaxSlots> ts;            // Arbeitsspeicher (vorab angelegt)
    std::array<float, kMaxSlots> gains {};
    std::array<int, kMaxSlots> active {};
    int intervalSamples = 960;
    int sampleCounter = 0;
    std::atomic<uint64_t> stepCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixEngine)
};
} // namespace ame
