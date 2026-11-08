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
    for (auto* s : { &inGain, &outGain, &panS, &widthS, &makeup, &compMixS })
        s->reset (sr, rampSec);
    compMixS.setCurrentAndTargetValue (1.0f);
    deessF.reset (sr, 0.05);
    deessF.setCurrentAndTargetValue (6500.0f);

    for (auto& s : aCut) { s.reset (sr, 0.02); s.setCurrentAndTargetValue (0.0f); }
    aHpfF.reset (sr, 0.05);
    aHpfF.setCurrentAndTargetValue (20.0f);

    reset();
}

void ChannelStrip::reset()
{
    for (auto& b : hpf) b.reset();
    for (auto& b : lpf) b.reset();
    excHp.reset(); excHp2.reset(); excApplied = -1.0f;
    dcX = {}; dcY = {};
    limGain = 1.0f; limRedDb = 0.0f;
    for (auto& b : eq) b.reset();
    aHpf.reset();
    for (auto& b : aEq) { b.reset(); b.setup (Biquad::Type::Peak, sr, 1000.0, kAdaptiveQ, 0.0); }
    aCutApplied.fill (0.0f);
    aHpfApplied = -1.0f;
    compEnvDb = compFastDb = compSlowDb = 0.0f;
    compSc.reset(); compScApplied = -1.0f;
    gateSc.reset(); gateScApplied = -1.0f;
    gateDetEnv = 0.0f; gateGainDb = 0.0f; gateHoldLeft = 0; gateOpen = false;
    deessHp.reset(); deessHp2.reset(); deessLp.reset(); deessLp2.reset(); deessDetEnv = 0.0f; deessRedDb = 0.0f;
}

namespace
{
// Butterworth-Güten je Kaskade: 12 / 24 / 48 dB/Okt.
constexpr double kBwQ[3][4] { { 0.7071068, 0, 0, 0 },
                              { 0.5411961, 1.3065630, 0, 0 },
                              { 0.5097956, 0.6013449, 0.8999762, 2.5629154 } };
constexpr int kBwStages[3] { 1, 2, 4 };

Biquad::Type lowType (bool bell)  { return bell ? Biquad::Type::Peak : Biquad::Type::LowShelf; }
Biquad::Type highType (bool bell) { return bell ? Biquad::Type::Peak : Biquad::Type::HighShelf; }
}

void ChannelStrip::updateFilters (const Params& p) noexcept
{
    const auto hf = hpfF.getNextValue(), lf = lpfF.getNextValue();
    const int hs = juce::jlimit (0, 2, p.hpfSlope), ls = juce::jlimit (0, 2, p.lpfSlope);

    if (kBwStages[hs] != hpfStages) { hpfStages = kBwStages[hs]; for (auto& b : hpf) b.reset(); }
    if (kBwStages[ls] != lpfStages) { lpfStages = kBwStages[ls]; for (auto& b : lpf) b.reset(); }

    if (p.hpfOn)
        for (int i = 0; i < hpfStages; ++i)
            hpf[(size_t) i].setup (Biquad::Type::HighPass, sr, hf, kBwQ[hs][i], 0);
    if (p.lpfOn)
        for (int i = 0; i < lpfStages; ++i)
            lpf[(size_t) i].setup (Biquad::Type::LowPass, sr, lf, kBwQ[ls][i], 0);

    const Biquad::Type types[4] { lowType (p.lowBell), Biquad::Type::Peak, Biquad::Type::Peak, highType (p.highBell) };
    for (size_t b = 0; b < 4; ++b)
        eq[b].setup (types[b], sr, eqF[b].getNextValue(), eqQ[b].getNextValue(), eqG[b].getNextValue());
}

float ChannelStrip::getManualResponseDb (const Params& p, float hz, double rate) noexcept
{
    float db = 0.0f;
    Biquad b;
    const int hs = juce::jlimit (0, 2, p.hpfSlope), ls = juce::jlimit (0, 2, p.lpfSlope);
    if (p.hpfOn)
        for (int i = 0; i < kBwStages[hs]; ++i)
        {
            b.setup (Biquad::Type::HighPass, rate, p.hpfFreq, kBwQ[hs][i], 0);
            db += b.getMagnitudeDb (hz, rate);
        }
    if (p.lpfOn)
        for (int i = 0; i < kBwStages[ls]; ++i)
        {
            b.setup (Biquad::Type::LowPass, rate, p.lpfFreq, kBwQ[ls][i], 0);
            db += b.getMagnitudeDb (hz, rate);
        }

    const Biquad::Type types[4] { lowType (p.lowBell), Biquad::Type::Peak, Biquad::Type::Peak, highType (p.highBell) };
    for (size_t k = 0; k < 4; ++k)
    {
        b.setup (types[k], rate, p.eqFreq[k], p.eqQ[k], p.eqGainDb[k]);
        db += b.getMagnitudeDb (hz, rate);
    }
    return db;
}

float ChannelStrip::saturate (int type, float x) noexcept
{
    switch (type)
    {
        case 0:   // Röhre: asymmetrisch (gerade Obertöne), Steigung 1 im Nullpunkt
        {
            constexpr float bias = 0.25f;
            static const float t0 = std::tanh (bias), norm = 1.0f / (1.0f - t0 * t0);
            return (std::tanh (x + bias) - t0) * norm;
        }
        case 2:   // Transistor: kubische Kennlinie, härterer Knick
        {
            const float u = juce::jlimit (-1.0f, 1.0f, x * (2.0f / 3.0f));
            return 1.5f * (u - u * u * u / 3.0f) * 1.0f;
        }
        default:  // Band: weich symmetrisch
            return std::tanh (x);
    }
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

namespace
{
inline float dbToGain (float db) noexcept  { return std::exp (db * 0.11512925465f); }      // ln(10)/20
inline float gainToDb (float g) noexcept   { return 8.6858896381f * std::log (juce::jmax (g, 1.0e-6f)); }
inline float coefFor (float ms, double sr) noexcept
{
    return std::exp (-1.0f / (0.001f * juce::jmax (0.01f, ms) * (float) sr));
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
    compMixS.setTargetValue (juce::jlimit (0.0f, 1.0f, p.compMix));
    deessF.setTargetValue (juce::jlimit (1000.0f, (float) sr * 0.45f, p.deessFreq));

    const int numSamples = buffer.getNumSamples();
    const int nch = juce::jmin (channels, buffer.getNumChannels());
    float* L = buffer.getWritePointer (0);
    float* R = nch > 1 ? buffer.getWritePointer (1) : nullptr;

    // Sidechain-Filter (nur Detektor) bei Änderung neu
    const bool compScOn = p.compScHpfHz > 20.5f;
    if (compScOn && std::abs (p.compScHpfHz - compScApplied) > 0.5f)
    {
        compSc.setup (Biquad::Type::HighPass, sr, p.compScHpfHz, 0.7071, 0);
        compScApplied = p.compScHpfHz;
    }
    const bool gateScOn = p.gateScHpfHz > 20.5f;
    if (gateScOn && std::abs (p.gateScHpfHz - gateScApplied) > 0.5f)
    {
        gateSc.setup (Biquad::Type::HighPass, sr, p.gateScHpfHz, 0.7071, 0);
        gateScApplied = p.gateScHpfHz;
    }

    // Kompressor
    const float attCoef = coefFor (juce::jmax (0.05f, p.compAttackMs), sr);
    const float relCoef = coefFor (juce::jmax (5.0f, p.compReleaseMs), sr);
    const float relFast = coefFor (40.0f, sr), relSlow = coefFor (1500.0f, sr), attSlow = coefFor (300.0f, sr);
    const float kneeDb = juce::jmax (0.0f, p.compKneeDb);
    const float slope = 1.0f / juce::jmax (1.0f, p.compRatio) - 1.0f;   // <= 0

    // Gate
    const float gDetRel = coefFor (8.0f, sr);
    const float gAtt = coefFor (p.gateAttackMs, sr), gRel = coefFor (p.gateReleaseMs, sr);
    const int gHold = (int) (p.gateHoldMs * 0.001f * (float) sr);
    const float gRatio = p.gateMode == 2 ? 4.0f : 2.0f;

    // Sättigung
    const float satG = dbToGain (juce::jlimit (0.0f, 36.0f, p.satDriveDb)), satInv = 1.0f / satG;
    const float satMix = juce::jlimit (0.0f, 1.0f, p.satMix);
    const bool excOn = p.satOn && p.excAmount > 0.001f;
    if (excOn && std::abs (p.excFreq - excApplied) > 1.0f)
    {
        excHp.setup (Biquad::Type::HighPass, sr, p.excFreq, 0.7071, 0);
        excHp2.setup (Biquad::Type::HighPass, sr, p.excFreq * 0.8f, 0.7071, 0);
        excApplied = p.excFreq;
    }

    // De-Esser
    const float dDetRel = coefFor (15.0f, sr);
    const float dAtt = coefFor (1.0f, sr), dRel = coefFor (80.0f, sr);

    for (int start = 0; start < numSamples; start += kSubBlock)
    {
        const int len = juce::jmin (kSubBlock, numSamples - start);

        updateFilters (p);
        hpfF.skip (len - 1); lpfF.skip (len - 1);
        for (size_t b = 0; b < 4; ++b) { eqF[b].skip (len - 1); eqG[b].skip (len - 1); eqQ[b].skip (len - 1); }
        if (p.deessOn)
        {
            const float f = deessF.getNextValue();
            deessHp.setup (Biquad::Type::HighPass, sr, f, 0.7071, 0);
            deessHp2.setup (Biquad::Type::HighPass, sr, f, 0.7071, 0);
            deessLp.setup (Biquad::Type::LowPass, sr, f, 0.7071, 0);
            deessLp2.setup (Biquad::Type::LowPass, sr, f, 0.7071, 0);
        }
        deessF.skip (len - 1);

        for (int i = start; i < start + len; ++i)
        {
            const float gIn = inGain.getNextValue();
            float l = L[i] * gIn * (p.phaseL ? -1.0f : 1.0f);
            float r = R != nullptr ? R[i] * gIn * (p.phaseR ? -1.0f : 1.0f) : 0.0f;

            if (p.hpfOn)
                for (int k = 0; k < hpfStages; ++k) { l = hpf[(size_t) k].process (l, 0); if (R) r = hpf[(size_t) k].process (r, 1); }
            if (p.lpfOn)
                for (int k = 0; k < lpfStages; ++k) { l = lpf[(size_t) k].process (l, 0); if (R) r = lpf[(size_t) k].process (r, 1); }

            // --- Gate / Expander ---
            if (p.gateOn)
            {
                float dl = l, dr = r;
                if (gateScOn) { dl = gateSc.process (l, 0); if (R) dr = gateSc.process (r, 1); }
                const float det = juce::jmax (std::abs (dl), R ? std::abs (dr) : 0.0f);
                gateDetEnv = juce::jmax (det, gateDetEnv * gDetRel);
                const float levelDb = gainToDb (gateDetEnv);

                float target = 0.0f;
                bool above;
                if (p.gateMode == 0)
                {
                    gateOpen = levelDb > p.gateThreshDb - (gateOpen ? 3.0f : 0.0f);   // 3 dB Hysterese
                    above = gateOpen;
                    target = gateOpen ? 0.0f : -p.gateRangeDb;
                }
                else
                {
                    const float below = p.gateThreshDb - levelDb;
                    above = below <= 0.0f;
                    target = above ? 0.0f : -juce::jmin (p.gateRangeDb, below * (gRatio - 1.0f));
                }

                if (above)
                    gateHoldLeft = gHold;

                if (target > gateGainDb)
                    gateGainDb = gAtt * gateGainDb + (1.0f - gAtt) * target;
                else if (gateHoldLeft > 0)
                    --gateHoldLeft;
                else
                    gateGainDb = gRel * gateGainDb + (1.0f - gRel) * target;

                const float g = dbToGain (gateGainDb);
                l *= g; r *= g;
            }
            else
            {
                gateGainDb = 0.0f;
            }

            auto runEq = [&]
            {
                for (auto& b : eq)
                {
                    l = b.process (l, 0);
                    if (R) r = b.process (r, 1);
                }
            };
            if (! p.compFirst)
                runEq();

            // --- Kompressor ---
            const float mk = dbToGain (makeup.getNextValue());
            const float mix = compMixS.getNextValue();

            if (p.compOn)
            {
                float dl = l, dr = r;
                if (compScOn) { dl = compSc.process (l, 0); if (R) dr = compSc.process (r, 1); }
                const float level = juce::jmax (std::abs (dl), R ? std::abs (dr) : 0.0f);
                const float over = gainToDb (level) - p.compThreshDb;

                float target = 0.0f;   // positive Gain Reduction in dB
                if (kneeDb < 0.1f)
                    target = over > 0.0f ? -slope * over : 0.0f;
                else if (2.0f * over > kneeDb)
                    target = -slope * over;
                else if (2.0f * over > -kneeDb)
                    target = -slope * (over + kneeDb * 0.5f) * (over + kneeDb * 0.5f) / (2.0f * kneeDb);

                if (p.compAutoRelease)
                {
                    // Programmabhängig: schnelle Erholung nach Spitzen, langsame bei Dauerkompression
                    compFastDb = (target > compFastDb ? attCoef : relFast) * (compFastDb - target) + target;
                    compSlowDb = (target > compSlowDb ? attSlow : relSlow) * (compSlowDb - target) + target;
                    compEnvDb = juce::jmax (compFastDb, compSlowDb);
                }
                else
                {
                    const float c = target > compEnvDb ? attCoef : relCoef;
                    compEnvDb = c * compEnvDb + (1.0f - c) * target;
                    compFastDb = compSlowDb = compEnvDb;
                }

                const float g = dbToGain (-compEnvDb) * mk;
                const float wl = l * g, wr = r * g;
                l += mix * (wl - l);
                r += mix * (wr - r);
            }
            else
            {
                compEnvDb *= relCoef;
                compFastDb = compSlowDb = compEnvDb;
            }

            if (p.compFirst)
                runEq();

            // --- De-Esser ---
            if (p.deessOn)
            {
                // Frequenzweiche LR4: tief + hoch = Allpass (flacher Betrag)
                const float hl = deessHp2.process (deessHp.process (l, 0), 0);
                const float hr = R ? deessHp2.process (deessHp.process (r, 1), 1) : 0.0f;
                const float det = juce::jmax (std::abs (hl), std::abs (hr));
                deessDetEnv = juce::jmax (det, deessDetEnv * dDetRel);
                const float over = gainToDb (deessDetEnv) - p.deessThreshDb;
                const float target = over > 0.0f ? juce::jmin (p.deessRangeDb, over * 0.75f) : 0.0f;   // 4:1
                const float c = target > deessRedDb ? dAtt : dRel;
                deessRedDb = c * deessRedDb + (1.0f - c) * target;
                const float g = dbToGain (-deessRedDb);

                if (p.deessListen)      { l = hl; r = hr; }
                else if (p.deessWide)   { l *= g; r *= g; }
                else
                {
                    // nur das Band oberhalb der Frequenz absenken
                    const float ll = deessLp2.process (deessLp.process (l, 0), 0);
                    const float lr = R ? deessLp2.process (deessLp.process (r, 1), 1) : 0.0f;
                    l = ll + hl * g;
                    r = lr + hr * g;
                }
            }
            else
            {
                deessRedDb = 0.0f;
            }

            // --- Sättigung / Exciter ---
            if (p.satOn)
            {
                const float in[2] { l, r };
                float out[2] {};
                for (int ch = 0; ch < (R ? 2 : 1); ++ch)
                {
                    const float x = in[ch];
                    float wet = saturate (p.satType, x * satG) * satInv;
                    if (p.satType == 0)   // Gleichanteil der Röhre entfernen
                    {
                        const float y = wet - dcX[(size_t) ch] + 0.9995f * dcY[(size_t) ch];
                        dcX[(size_t) ch] = wet; dcY[(size_t) ch] = y;
                        wet = y;
                    }
                    float y = x + satMix * (wet - x);

                    if (excOn)
                    {
                        // Exciter: Obertöne nur aus dem Höhenband, danach erneut hochpassgefiltert
                        const float h = excHp.process (x, ch);
                        // gerade Obertöne (Quadrat: 2. Harmonische + Gleichanteil, Hochpass entfernt ihn), begrenzt
                        const float e = std::tanh (4.0f * h);
                        y += p.excAmount * 0.5f * excHp2.process (e * e, ch);
                    }
                    out[ch] = y;
                }
                l = out[0];
                if (R) r = out[1];
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
    const float ceiling = dbToGain (juce::jlimit (-24.0f, 0.0f, p.limCeilingDb));
    const float limRel = coefFor (p.limReleaseMs, sr);
    float blockPeak = 0.0f, blockMinGain = 1.0f;
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
                l *= gl * gOut;
                r *= gr * gOut;
            }
            else
            {
                l *= gOut;
            }

            // --- Limiter / Clipper ---
            if (p.limOn)
            {
                const float peak = juce::jmax (std::abs (l), R ? std::abs (r) : 0.0f);
                blockPeak = juce::jmax (blockPeak, peak);
                if (p.limMode == 0)
                {
                    // Spitzenbegrenzer ohne Latenz: sofortiger Eingriff, Rücklauf mit Release
                    const float need = peak > ceiling ? ceiling / peak : 1.0f;
                    limGain = need < limGain ? need : limRel * limGain + (1.0f - limRel);
                    l *= limGain; r *= limGain;
                    l = juce::jlimit (-ceiling, ceiling, l);   // Sicherheit
                    r = juce::jlimit (-ceiling, ceiling, r);
                    blockMinGain = juce::jmin (blockMinGain, limGain);
                }
                else
                {
                    l = clip (p.limMode, l, ceiling);
                    r = clip (p.limMode, r, ceiling);
                }
            }

            L[i] = l;
            if (R) R[i] = r;
        }
    }

    if (! p.limOn)
    {
        limGain = 1.0f;
        limRedDb = 0.0f;
    }
    else if (p.limMode == 0)
        limRedDb = -gainToDb (blockMinGain);
    else
        limRedDb = juce::jmax (0.0f, gainToDb (blockPeak) - p.limCeilingDb);
}

float ChannelStrip::clip (int mode, float x, float c) noexcept
{
    if (mode == 2)
        return juce::jlimit (-c, c, x);

    // weich: linear bis 50 % des Ceilings, darüber tanh-Übergang (stetige Steigung)
    const float k = 0.5f * c, ax = std::abs (x);
    if (ax <= k)
        return x;
    const float y = k + (c - k) * std::tanh ((ax - k) / (c - k));
    return x < 0.0f ? -y : y;
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
