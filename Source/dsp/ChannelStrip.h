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
//   processPre:  Input -> Phase -> HPF/LPF -> Gate/Expander -> EQ <-> Kompressor (Reihenfolge wählbar)
//                -> De-Esser -> Sättigung/Exciter
//   [Analyse-Abgriff]
//   processPost: Adaptiv (HPF, 10 Oktav-Bänder, Gain) -> Width/Pan -> Output -> Limiter/Clipper
class ChannelStrip
{
public:
    struct Params
    {
        float inGainDb = 0.0f;
        bool  phaseL = false, phaseR = false;
        bool  hpfOn = false;  float hpfFreq = 20.0f;
        bool  lpfOn = false;  float lpfFreq = 20000.0f;
        int   hpfSlope = 1, lpfSlope = 1;      // 0 = 12, 1 = 24, 2 = 48 dB/Okt.

        // EQ-Bänder: 0 = Low Shelf, 1 = Mid1, 2 = Mid2, 3 = High Shelf
        std::array<float, 4> eqFreq { 100.0f, 400.0f, 2500.0f, 8000.0f };
        std::array<float, 4> eqGainDb {};
        std::array<float, 4> eqQ { 0.707f, 1.0f, 1.0f, 0.707f };
        bool  lowBell = false, highBell = false;   // Low/High als Glocke statt Shelf
        bool  compFirst = false;                   // Reihenfolge Comp -> EQ

        bool  compOn = false;
        float compThreshDb = -18.0f, compRatio = 3.0f, compAttackMs = 10.0f,
              compReleaseMs = 120.0f, compMakeupDb = 0.0f;
        float compKneeDb = 6.0f, compMix = 1.0f, compScHpfHz = 20.0f;   // Sidechain-HPF <= 20 Hz = aus
        bool  compAutoRelease = false;

        bool  gateOn = false;
        int   gateMode = 0;          // 0 = Gate, 1 = Expander 1:2, 2 = Expander 1:4
        float gateThreshDb = -50.0f, gateRangeDb = 40.0f, gateAttackMs = 0.5f, gateHoldMs = 30.0f,
              gateReleaseMs = 150.0f, gateScHpfHz = 20.0f;

        bool  satOn = false;
        int   satType = 0;           // 0 = Röhre, 1 = Band, 2 = Transistor
        float satDriveDb = 6.0f, satMix = 1.0f, excAmount = 0.0f, excFreq = 5000.0f;

        bool  deessOn = false, deessWide = false, deessListen = false;
        float deessFreq = 6500.0f, deessThreshDb = -30.0f, deessRangeDb = 8.0f;

        // Adaptiv (bereits geglättet und mit Amount skaliert)
        std::array<float, kNumAdaptiveBands> adaptiveCutDb {};
        float adaptiveHpfHz = 0.0f;     // < 20 = aus
        float adaptiveGainDb = 0.0f;

        float pan = 0.0f;        // -1 .. 1
        float width = 1.0f;      // 0 .. 2
        float outGainDb = 0.0f;

        bool  limOn = false;
        int   limMode = 0;           // 0 = Limiter, 1 = Soft Clip, 2 = Hard Clip
        float limCeilingDb = -0.3f, limReleaseMs = 60.0f;
    };

    void prepare (double sampleRate, int numChannels);
    void reset();
    void processPre (juce::AudioBuffer<float>& buffer, const Params& p) noexcept;
    void processPost (juce::AudioBuffer<float>& buffer, const Params& p) noexcept;

    float getGainReductionDb() const noexcept { return compEnvDb; }
    float getGateReductionDb() const noexcept { return -gateGainDb; }      // >= 0
    float getDeessReductionDb() const noexcept { return deessRedDb; }      // >= 0
    float getLimiterReductionDb() const noexcept { return limRedDb; }      // >= 0

    // Clipper-Kennlinie (für Tests): weich bzw. hart auf ceiling begrenzt
    static float clip (int mode, float x, float ceiling) noexcept;

    // Frequenzgang von Filtern + EQ (für die Anzeige)
    static float getManualResponseDb (const Params&, float freq, double sampleRate = 48000.0) noexcept;

    // Kennlinie der Sättigung (Kleinsignal-Verstärkung 1)
    static float saturate (int type, float x) noexcept;

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
    static constexpr int kMaxFilterStages = 4;          // bis 48 dB/Okt.
    std::array<Biquad, kMaxFilterStages> hpf, lpf;
    int hpfStages = 2, lpfStages = 2;
    std::array<Biquad, 4> eq;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> hpfF, lpfF;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>, 4> eqF;
    std::array<juce::SmoothedValue<float>, 4> eqG, eqQ;
    juce::SmoothedValue<float> inGain, makeup, compMixS;
    float compEnvDb = 0.0f, compFastDb = 0.0f, compSlowDb = 0.0f;
    Biquad compSc;
    float compScApplied = -1.0f;

    // Gate / Expander
    Biquad gateSc;
    float gateScApplied = -1.0f;
    float gateDetEnv = 0.0f, gateGainDb = 0.0f;
    int gateHoldLeft = 0;
    bool gateOpen = false;

    // Sättigung / Exciter
    Biquad excHp, excHp2;
    float excApplied = -1.0f;
    std::array<float, 2> dcX {}, dcY {};

    // De-Esser
    Biquad deessHp, deessHp2, deessLp, deessLp2;   // Linkwitz-Riley 4. Ordnung (Split)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> deessF;
    float deessDetEnv = 0.0f, deessRedDb = 0.0f;

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
    float limGain = 1.0f, limRedDb = 0.0f;
};
} // namespace mtm
