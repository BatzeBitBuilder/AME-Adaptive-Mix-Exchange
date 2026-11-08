// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "ChannelStrip.h"

namespace mtm
{
void ChannelStrip::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);

    constexpr double rampSec = 0.03;
    hpfF.reset (sr, rampSec);  lpfF.reset (sr, rampSec);
    for (auto& s : eqF) s.reset (sr, rampSec);
    for (auto& s : eqG) s.reset (sr, rampSec);
    for (auto& s : eqQ) s.reset (sr, rampSec);
    for (auto* s : { &inGain, &outGain, &panS, &widthS, &makeup })
        s->reset (sr, rampSec);

    for (auto& s : aCut) { s.reset (sr, 0.02); s.setCurrentAndTargetValue (0.0f); }
    aHpfF.reset (sr, 0.05);
    aHpfF.setCurrentAndTargetValue (20.0f);

    reset();
}

void ChannelStrip::reset()
{
    hpf.reset(); hpf2.reset(); lpf.reset(); lpf2.reset();
    for (auto& b : eq) b.reset();
    aHpf.reset();
    for (auto& b : aEq) { b.reset(); b.setup (Biquad::Type::Peak, sr, 1000.0, kAdaptiveQ, 0.0); }
    aCutApplied.fill (0.0f);
    aHpfApplied = -1.0f;
    compEnvDb = 0.0f;
}

void ChannelStrip::updateFilters (const Params& p) noexcept
{
    constexpr double q1 = 0.5411961, q2 = 1.3065630;  // Butterworth 4. Ordnung

    const auto hf = hpfF.getNextValue(), lf = lpfF.getNextValue();
    if (p.hpfOn) { hpf.setup (Biquad::Type::HighPass, sr, hf, q1, 0); hpf2.setup (Biquad::Type::HighPass, sr, hf, q2, 0); }
    if (p.lpfOn) { lpf.setup (Biquad::Type::LowPass,  sr, lf, q1, 0); lpf2.setup (Biquad::Type::LowPass,  sr, lf, q2, 0); }

    static constexpr Biquad::Type types[4] { Biquad::Type::LowShelf, Biquad::Type::Peak,
                                             Biquad::Type::Peak, Biquad::Type::HighShelf };
    for (size_t b = 0; b < 4; ++b)
        eq[b].setup (types[b], sr, eqF[b].getNextValue(), eqQ[b].getNextValue(), eqG[b].getNextValue());
}

void ChannelStrip::updateAdaptiveFilters() noexcept
{
    for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
    {
        const float g = aCut[k].getNextValue();
        if (std::abs (g - aCutApplied[k]) > 0.005f)
        {
            aEq[k].setup (Biquad::Type::Peak, sr, getAdaptiveBandHz ((int) k), kAdaptiveQ, g);
            aCutApplied[k] = g;
        }
    }

    const float f = aHpfF.getNextValue();
    if (aHpfActive && std::abs (f - aHpfApplied) > 0.05f)
    {
        aHpf.setup (Biquad::Type::HighPass, sr, f, 0.7071, 0);
        aHpfApplied = f;
    }
}

void ChannelStrip::processPre (juce::AudioBuffer<float>& buffer, const Params& p) noexcept
{
    hpfF.setTargetValue (p.hpfFreq);
    lpfF.setTargetValue (p.lpfFreq);
    for (size_t b = 0; b < 4; ++b)
    {
        eqF[b].setTargetValue (p.eqFreq[b]);
        eqG[b].setTargetValue (p.eqGainDb[b]);
        eqQ[b].setTargetValue (p.eqQ[b]);
    }
    inGain.setTargetValue (juce::Decibels::decibelsToGain (p.inGainDb));
    makeup.setTargetValue (p.compOn ? p.compMakeupDb : 0.0f);

    const int numSamples = buffer.getNumSamples();
    const int nch = juce::jmin (channels, buffer.getNumChannels());
    float* L = buffer.getWritePointer (0);
    float* R = nch > 1 ? buffer.getWritePointer (1) : nullptr;

    const float attCoef = std::exp (-1.0f / (0.001f * juce::jmax (0.05f, p.compAttackMs) * (float) sr));
    const float relCoef = std::exp (-1.0f / (0.001f * juce::jmax (5.0f, p.compReleaseMs) * (float) sr));
    constexpr float kneeDb = 6.0f;
    const float slope = 1.0f / juce::jmax (1.0f, p.compRatio) - 1.0f;   // <= 0

    for (int start = 0; start < numSamples; start += kSubBlock)
    {
        const int len = juce::jmin (kSubBlock, numSamples - start);

        updateFilters (p);
        hpfF.skip (len - 1); lpfF.skip (len - 1);
        for (size_t b = 0; b < 4; ++b) { eqF[b].skip (len - 1); eqG[b].skip (len - 1); eqQ[b].skip (len - 1); }

        for (int i = start; i < start + len; ++i)
        {
            const float gIn = inGain.getNextValue();
            float l = L[i] * gIn;
            float r = R != nullptr ? R[i] * gIn : 0.0f;

            if (p.hpfOn) { l = hpf2.process (hpf.process (l, 0), 0); if (R) r = hpf2.process (hpf.process (r, 1), 1); }
            if (p.lpfOn) { l = lpf2.process (lpf.process (l, 0), 0); if (R) r = lpf2.process (lpf.process (r, 1), 1); }

            for (auto& b : eq)
            {
                l = b.process (l, 0);
                if (R) r = b.process (r, 1);
            }

            const float mk = juce::Decibels::decibelsToGain (makeup.getNextValue());

            if (p.compOn)
            {
                const float level = juce::jmax (std::abs (l), R ? std::abs (r) : 0.0f);
                const float levelDb = juce::Decibels::gainToDecibels (level, -120.0f);
                const float over = levelDb - p.compThreshDb;

                float target = 0.0f;   // positive Gain Reduction in dB
                if (2.0f * over > kneeDb)
                    target = -slope * over;
                else if (2.0f * over > -kneeDb)
                    target = -slope * (over + kneeDb * 0.5f) * (over + kneeDb * 0.5f) / (2.0f * kneeDb);

                const float c = target > compEnvDb ? attCoef : relCoef;
                compEnvDb = c * compEnvDb + (1.0f - c) * target;

                const float g = juce::Decibels::decibelsToGain (-compEnvDb) * mk;
                l *= g; r *= g;
            }
            else
            {
                compEnvDb *= relCoef;
            }

            L[i] = l;
            if (R) R[i] = r;
        }
    }
}

void ChannelStrip::processPost (juce::AudioBuffer<float>& buffer, const Params& p) noexcept
{
    for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
        aCut[k].setTargetValue (juce::jlimit (-24.0f, 0.0f, p.adaptiveCutDb[k]));

    const bool hpfWanted = p.adaptiveHpfHz >= 20.0f;
    if (hpfWanted && ! aHpfActive)
    {
        aHpfActive = true;
        aHpfF.setCurrentAndTargetValue (20.0f);
        aHpfApplied = -1.0f;
        aHpf.reset();
    }
    aHpfF.setTargetValue (hpfWanted ? p.adaptiveHpfHz : 20.0f);
    if (! hpfWanted && aHpfActive && ! aHpfF.isSmoothing() && aHpfF.getCurrentValue() <= 20.5f)
        aHpfActive = false;

    outGain.setTargetValue (juce::Decibels::decibelsToGain (p.outGainDb + p.adaptiveGainDb));
    panS.setTargetValue (p.pan);
    widthS.setTargetValue (p.width);

    const int numSamples = buffer.getNumSamples();
    const int nch = juce::jmin (channels, buffer.getNumChannels());
    float* L = buffer.getWritePointer (0);
    float* R = nch > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int start = 0; start < numSamples; start += kSubBlock)
    {
        const int len = juce::jmin (kSubBlock, numSamples - start);

        updateAdaptiveFilters();
        for (auto& s : aCut) s.skip (len - 1);
        aHpfF.skip (len - 1);

        bool anyCut = false;
        for (auto g : aCutApplied) anyCut |= (g < -0.01f);

        for (int i = start; i < start + len; ++i)
        {
            float l = L[i];
            float r = R != nullptr ? R[i] : 0.0f;

            if (aHpfActive)
            {
                l = aHpf.process (l, 0);
                if (R) r = aHpf.process (r, 1);
            }

            if (anyCut)
            {
                for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
                {
                    if (aCutApplied[k] < -0.01f)
                    {
                        l = aEq[k].process (l, 0);
                        if (R) r = aEq[k].process (r, 1);
                    }
                }
            }

            const float gOut = outGain.getNextValue();
            const float pan = panS.getNextValue();
            const float wid = widthS.getNextValue();

            if (R != nullptr)
            {
                const float m = 0.5f * (l + r);
                const float s = 0.5f * (l - r) * wid;
                l = m + s;
                r = m - s;

                // Pan-Law: Mitte 0 dB, außen -inf / 0 dB
                const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                const float gl = juce::jmin (1.0f, juce::MathConstants<float>::sqrt2 * std::cos (angle));
                const float gr = juce::jmin (1.0f, juce::MathConstants<float>::sqrt2 * std::sin (angle));
                L[i] = l * gl * gOut;
                R[i] = r * gr * gOut;
            }
            else
            {
                L[i] = l * gOut;
            }
        }
    }
}

float ChannelStrip::getAdaptiveResponseDb (const std::array<float, kNumAdaptiveBands>& cutDb,
                                           float hpfHz, float gainDb, float freq) noexcept
{
    // Analytischer Betragsgang (Peak-Filter nach RBJ, HPF 2. Ordnung)
    double db = gainDb;
    const double w = freq;

    for (int k = 0; k < kNumAdaptiveBands; ++k)
    {
        const double g = cutDb[(size_t) k];
        if (g > -0.01) continue;

        const double fc = getAdaptiveBandHz (k);
        const double A2 = std::pow (10.0, g / 20.0);     // A^2
        const double x = w / fc - fc / w;
        const double q2 = kAdaptiveQ * kAdaptiveQ;
        // Analoger Prototyp: |H|^2 = (x^2 Q^2 + A^2) / (x^2 Q^2 + 1/A^2), A = 10^(g/40), x = f/fc - fc/f
        const double num = x * x * q2 + A2;
        const double den = x * x * q2 + 1.0 / A2;
        db += 10.0 * std::log10 (num / den);
    }

    if (hpfHz >= 20.0f)
    {
        const double r = hpfHz / w;
        db += -10.0 * std::log10 (1.0 + r * r * r * r);
    }

    return (float) db;
}
} // namespace mtm
