// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

#include "hub/MixHub.h"

namespace ame
{
// FFT-Analyse (4096 Punkte, Hop 2048, Hann):
//  - 32 log. Bänder 20 Hz – 20 kHz (~1/3 Oktave), kurz + lang gemittelt
//  - Klangmerkmale für die Rollenerkennung (Schwerpunkt, Flatness, Bandanteile,
//    Onsets, Harmonizität/Grundton per Autokorrelation, Crest, Stereobreite)
// Läuft im Audio-Thread, keine Allokationen nach prepare().
class SpectrumAnalyzer
{
public:
    static constexpr int kOrder = 12;
    static constexpr int kSize  = 1 << kOrder;
    static constexpr int kHop   = kSize / 2;

    void prepare (double sampleRate);
    void reset();

    // Buffer (mono/stereo) einspeisen; schreibt bei jedem Hop in 'out'
    void process (const juce::AudioBuffer<float>& buffer, SlotAudioData* out) noexcept;

    static float getBandCentreHz (int band) noexcept;
    static float getBandEdgeHz (int edge) noexcept;   // edge 0..kNumAnalysisBands

private:
    void performAnalysis (SlotAudioData* out) noexcept;
    void updateFeatures (SlotAudioData* out, float frameRms, float framePeak) noexcept;

    double sr = 48000.0;
    juce::dsp::FFT fft { kOrder };
    std::vector<float> window, fifo, fftData, acData, prevMag, windowAc;
    int fifoPos = 0;

    // Pro Band: Bin-Bereich [first, last], Gewichtung, falls Band schmaler als ein Bin
    std::array<int, kNumAnalysisBands> firstBin {}, lastBin {};
    std::array<float, kNumAnalysisBands> fractionalWeight {};

    std::array<float, kNumAnalysisBands> fastDb {}, slowDb {};

    // Anzeige-Punkte: Bin-Bereich (fractional) je Punkt
    std::array<float, kNumDisplayPoints> dispLoBin {}, dispHiBin {}, dispDb {};
    float dispRel = 0.0f;
    float fastAtt = 0, fastRel = 0, slowCoef = 0;
    float normalisation = 1.0f;

    // Merkmale
    RoleFeatures feat;
    float featCoef = 0.0f, hopSec = 0.0f;
    double midEnergy = 0.0, sideEnergy = 0.0;
    float fluxMean = 0.0f, lastFlux = 0.0f, lastFrameDb = -120.0f;
    float peakHold = 0.0f, rmsLong = 0.0f, crestPeakCoef = 0.0f, crestRmsCoef = 0.0f;
    float lastPitch = 0.0f;
    bool pitchVarInit = false;
    bool lastWasOnset = false;
    int minLag = 48, maxLag = 960;
};
} // namespace ame
