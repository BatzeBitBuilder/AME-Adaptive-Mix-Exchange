// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

#include "Biquad.h"
#include "hub/MixHub.h"

namespace mtm
{
// Signalkette:
//   processPre:  Input -> HPF/LPF -> EQ (LS, P1, P2, HS) -> Kompressor
//   [Analyse-Abgriff]
//   processPost: Adaptiv (HPF, 10 Oktav-Bänder, Gain) -> Width/Pan -> Output
class ChannelStrip
{
public:
    struct Params
    {
        float inGainDb = 0.0f;
        bool  hpfOn = false;  float hpfFreq = 20.0f;
        bool  lpfOn = false;  float lpfFreq = 20000.0f;

        // EQ-Bänder: 0 = Low Shelf, 1 = Mid1, 2 = Mid2, 3 = High Shelf
        std::array<float, 4> eqFreq { 100.0f, 400.0f, 2500.0f, 8000.0f };
        std::array<float, 4> eqGainDb {};
        std::array<float, 4> eqQ { 0.707f, 1.0f, 1.0f, 0.707f };

        bool  compOn = false;
        float compThreshDb = -18.0f, compRatio = 3.0f, compAttackMs = 10.0f,
              compReleaseMs = 120.0f, compMakeupDb = 0.0f;

        // Adaptiv (bereits geglättet und mit Amount skaliert)
        std::array<float, kNumAdaptiveBands> adaptiveCutDb {};
        float adaptiveHpfHz = 0.0f;     // < 20 = aus
        float adaptiveGainDb = 0.0f;

        float pan = 0.0f;        // -1 .. 1
        float width = 1.0f;      // 0 .. 2
        float outGainDb = 0.0f;
    };

    void prepare (double sampleRate, int numChannels);
    void reset();
    void processPre (juce::AudioBuffer<float>& buffer, const Params& p) noexcept;
    void processPost (juce::AudioBuffer<float>& buffer, const Params& p) noexcept;

    float getGainReductionDb() const noexcept { return compEnvDb; }

    // Frequenzgang der adaptiven Stufe (für die Anzeige)
    static float getAdaptiveResponseDb (const std::array<float, kNumAdaptiveBands>& cutDb,
                                        float hpfHz, float gainDb, float freq) noexcept;

private:
    static constexpr int kSubBlock = 32;
    static constexpr double kAdaptiveQ = 1.41;

    void updateFilters (const Params& p) noexcept;
    void updateAdaptiveFilters() noexcept;

    double sr = 44100.0;
    int channels = 2;

    // Manuell
    Biquad hpf, hpf2, lpf, lpf2;            // 2 x 12 dB = 24 dB/Okt.
    std::array<Biquad, 4> eq;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> hpfF, lpfF;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>, 4> eqF;
    std::array<juce::SmoothedValue<float>, 4> eqG, eqQ;
    juce::SmoothedValue<float> inGain, makeup;
    float compEnvDb = 0.0f;

    // Adaptiv
    Biquad aHpf;
    std::array<Biquad, kNumAdaptiveBands> aEq;
    std::array<juce::SmoothedValue<float>, kNumAdaptiveBands> aCut;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> aHpfF;
    bool aHpfActive = false;
    std::array<float, kNumAdaptiveBands> aCutApplied {};
    float aHpfApplied = -1.0f;

    // Ausgang
    juce::SmoothedValue<float> outGain, panS, widthS;
};
} // namespace mtm
