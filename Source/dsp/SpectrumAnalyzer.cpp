// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "SpectrumAnalyzer.h"

namespace ame
{
float SpectrumAnalyzer::getBandEdgeHz (int edge) noexcept
{
    return 20.0f * std::pow (1000.0f, (float) edge / (float) kNumAnalysisBands);
}

float SpectrumAnalyzer::getBandCentreHz (int band) noexcept
{
    return std::sqrt (getBandEdgeHz (band) * getBandEdgeHz (band + 1));
}

void SpectrumAnalyzer::prepare (double sampleRate)
{
    sr = sampleRate;
    window.resize ((size_t) kSize);
    fifo.assign ((size_t) kSize, 0.0f);
    fftData.assign ((size_t) kSize * 2, 0.0f);
    acData.assign ((size_t) kSize * 2, 0.0f);
    prevMag.assign ((size_t) kSize / 2 + 1, 0.0f);

    juce::dsp::WindowingFunction<float>::fillWindowingTables (window.data(), (size_t) kSize,
                                                              juce::dsp::WindowingFunction<float>::hann, false);
    float sum = 0.0f;
    for (auto w : window) sum += w;
    normalisation = 2.0f / sum;   // Sinus mit Amplitude 1 -> ~0 dB

    const float binHz = (float) sr / (float) kSize;
    const int maxBin = kSize / 2 - 1;

    for (int b = 0; b < kNumAnalysisBands; ++b)
    {
        const float lo = getBandEdgeHz (b), hi = getBandEdgeHz (b + 1);
        int f = (int) std::ceil (lo / binHz);
        int l = (int) std::ceil (hi / binHz) - 1;
        f = juce::jlimit (1, maxBin, f);
        l = juce::jlimit (0, maxBin, l);

        if (l < f)   // Band schmaler als ein Bin -> nächster Bin, anteilig
        {
            f = l = juce::jlimit (1, maxBin, juce::roundToInt (getBandCentreHz (b) / binHz));
            fractionalWeight[(size_t) b] = (hi - lo) / binHz;
        }
        else
        {
            fractionalWeight[(size_t) b] = 1.0f;
        }

        if (lo >= (float) sr * 0.5f)
            f = l = -1;

        firstBin[(size_t) b] = f;
        lastBin[(size_t) b] = l;
    }

    // Anzeige: ±1/24 Oktave um jeden Punkt
    for (int i = 0; i < kNumDisplayPoints; ++i)
    {
        const float f = getDisplayPointHz (i);
        dispLoBin[(size_t) i] = f * std::pow (2.0f, -1.0f / 24.0f) / binHz;
        dispHiBin[(size_t) i] = f * std::pow (2.0f,  1.0f / 24.0f) / binHz;
    }

    hopSec = (float) kHop / (float) sr;
    dispRel = std::exp (-hopSec / 0.25f);
    fastAtt  = std::exp (-hopSec / 0.08f);
    fastRel  = std::exp (-hopSec / 0.35f);
    slowCoef = std::exp (-hopSec / 3.0f);   // Langzeit-Spektrum für Masking
    featCoef = std::exp (-hopSec / 4.0f);   // Merkmale ~4 s
    crestPeakCoef = std::exp (-hopSec / 1.5f);
    crestRmsCoef  = std::exp (-hopSec / 1.0f);

    // Grundton-Suchbereich 50 Hz – 1 kHz; Autokorrelation des Fensters zur Normierung
    minLag = juce::jmax (2, (int) (sr / 1000.0));
    maxLag = juce::jmin (kSize / 2 - 2, (int) (sr / 50.0));
    windowAc.assign ((size_t) maxLag + 2, 0.0f);
    double w0 = 0.0;
    for (int i = 0; i < kSize; ++i) w0 += (double) window[(size_t) i] * window[(size_t) i];
    for (int lag = 0; lag <= maxLag + 1; ++lag)
    {
        double acc = 0.0;
        for (int i = 0; i + lag < kSize; ++i)
            acc += (double) window[(size_t) i] * window[(size_t) (i + lag)];
        windowAc[(size_t) lag] = (float) (acc / w0);
    }

    reset();
}

void SpectrumAnalyzer::reset()
{
    std::fill (fifo.begin(), fifo.end(), 0.0f);
    std::fill (prevMag.begin(), prevMag.end(), 0.0f);
    fifoPos = 0;
    fastDb.fill (-100.0f);
    slowDb.fill (-100.0f);
    dispDb.fill (-100.0f);
    feat = {};
    midEnergy = sideEnergy = 0.0;
    fluxMean = lastFlux = 0.0f;
    lastFrameDb = -120.0f;
    peakHold = rmsLong = 0.0f;
    lastPitch = 0.0f;
    pitchVarInit = false;
    lastWasOnset = false;
}

void SpectrumAnalyzer::setFeaturesEnabled (bool shouldCompute) noexcept
{
    if (shouldCompute && ! featuresEnabled)
        featuresRestart = true;
    featuresEnabled = shouldCompute;
}

void SpectrumAnalyzer::process (const juce::AudioBuffer<float>& buffer, SlotAudioData* out) noexcept
{
    if (out == nullptr || fifo.empty())
        return;

    if (featuresRestart)   // Merkmale waren aus: nicht mit alten Mittelwerten weiterrechnen
    {
        featuresRestart = false;
        std::fill (prevMag.begin(), prevMag.end(), 0.0f);
        feat = {};
        midEnergy = sideEnergy = 0.0;
        lastFrameDb = -120.0f;
        peakHold = rmsLong = 0.0f;
        lastPitch = 0.0f;
        pitchVarInit = false;
        lastWasOnset = false;
        out->fActiveSec.store (0.0f, std::memory_order_relaxed);
    }

    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (nch == 0)
        return;

    const float* L = buffer.getReadPointer (0);
    const float* R = nch > 1 ? buffer.getReadPointer (1) : nullptr;
    const bool features = featuresEnabled;

    for (int i = 0; i < n; ++i)
    {
        const float l = L[i];
        const float r = R != nullptr ? R[i] : l;
        const float m = 0.5f * (l + r);
        if (features)
        {
            const float s = 0.5f * (l - r);
            midEnergy += (double) (m * m);
            sideEnergy += (double) (s * s);
        }

        fifo[(size_t) fifoPos++] = m;

        if (fifoPos == kSize)
        {
            performAnalysis (out);
            std::copy (fifo.begin() + kHop, fifo.end(), fifo.begin());
            fifoPos = kSize - kHop;
        }
    }
}

void SpectrumAnalyzer::performAnalysis (SlotAudioData* out) noexcept
{
    // Frame-Pegel (ungefenstert, Hop-Bereich)
    float sumSq = 0.0f, peak = 0.0f;
    for (int i = kSize - kHop; i < kSize; ++i)
    {
        const float x = fifo[(size_t) i];
        sumSq += x * x;
        peak = juce::jmax (peak, std::abs (x));
    }
    const float frameRms = std::sqrt (sumSq / (float) kHop);

    for (int i = 0; i < kSize; ++i)
        fftData[(size_t) i] = fifo[(size_t) i] * window[(size_t) i];
    std::fill (fftData.begin() + kSize, fftData.end(), 0.0f);

    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    for (int b = 0; b < kNumAnalysisBands; ++b)
    {
        const auto ub = (size_t) b;
        float powerSum = 0.0f;

        if (firstBin[ub] >= 0)
        {
            for (int k = firstBin[ub]; k <= lastBin[ub]; ++k)
            {
                const float m = fftData[(size_t) k] * normalisation;
                powerSum += m * m;
            }
            powerSum *= fractionalWeight[ub];
        }

        const float db = 10.0f * std::log10 (powerSum + 1.0e-12f);
        const float clamped = juce::jlimit (-100.0f, 12.0f, db);

        const float c = clamped > fastDb[ub] ? fastAtt : fastRel;
        fastDb[ub] = c * fastDb[ub] + (1.0f - c) * clamped;

        if (clamped > -90.0f)
        {
            const float slowPow = std::pow (10.0f, slowDb[ub] * 0.1f);
            const float newPow = slowCoef * slowPow + (1.0f - slowCoef) * powerSum;
            slowDb[ub] = juce::jlimit (-100.0f, 12.0f, 10.0f * std::log10 (newPow + 1.0e-12f));
        }

        out->bandDb[ub].store (fastDb[ub], std::memory_order_relaxed);
        out->bandAvgDb[ub].store (slowDb[ub], std::memory_order_relaxed);
    }

    // Hochauflösende Anzeige
    {
        const int maxBin = kSize / 2;
        for (int i = 0; i < kNumDisplayPoints; ++i)
        {
            const float lo = dispLoBin[(size_t) i], hi = dispHiBin[(size_t) i];
            const int k0 = (int) std::ceil (lo), k1 = (int) std::floor (hi);
            float p = 0.0f;

            if (k1 >= k0)
            {
                for (int k = juce::jmax (1, k0); k <= juce::jmin (maxBin, k1); ++k)
                {
                    const float m = fftData[(size_t) k] * normalisation;
                    p += m * m;
                }
            }
            else   // schmaler als ein Bin: linear zwischen Nachbar-Bins interpolieren
            {
                const float centre = 0.5f * (lo + hi);
                const int kb = juce::jlimit (1, maxBin - 1, (int) centre);
                const float t = juce::jlimit (0.0f, 1.0f, centre - (float) kb);
                const float m = (1.0f - t) * fftData[(size_t) kb] + t * fftData[(size_t) kb + 1];
                p = m * m * normalisation * normalisation;
            }

            const float db = juce::jlimit (-100.0f, 12.0f, 10.0f * std::log10 (p + 1.0e-12f));
            auto& d = dispDb[(size_t) i];
            d = db > d ? db : dispRel * d + (1.0f - dispRel) * db;   // sofortiger Anstieg, Abfall ~250 ms
            out->displayDb[(size_t) i].store (d, std::memory_order_relaxed);
        }
    }

    if (featuresEnabled)
        updateFeatures (out, frameRms, peak);
    out->analysisCounter.fetch_add (1, std::memory_order_release);
}

void SpectrumAnalyzer::updateFeatures (SlotAudioData* out, float frameRms, float framePeak) noexcept
{
    const int numBins = kSize / 2 + 1;
    const float binHz = (float) sr / (float) kSize;

    // Spektral-Fluss (immer, damit Onsets nach Pausen erkannt werden)
    float flux = 0.0f, magSum = 0.0f;
    for (int k = 1; k < numBins; ++k)
    {
        const float m = fftData[(size_t) k];
        flux += juce::jmax (0.0f, m - prevMag[(size_t) k]);
        magSum += m;
        prevMag[(size_t) k] = m;
    }
    const float normFlux = magSum > 1.0e-6f ? flux / magSum : 0.0f;

    const float frameDb = juce::Decibels::gainToDecibels (frameRms, -120.0f);
    const bool active = frameDb > -55.0f;

    const float ms = (float) midEnergy, ss = (float) sideEnergy;
    midEnergy = sideEnergy = 0.0;

    // Onset: Pegelsprung >= 6 dB gegenüber dem vorigen Hop, mit spektraler Änderung
    const bool onset = active && frameDb - lastFrameDb >= 6.0f && normFlux > 0.1f && ! lastWasOnset;
    lastFrameDb = frameDb;
    lastWasOnset = onset;
    juce::ignoreUnused (fluxMean, lastFlux);

    if (! active)
        return;

    // Spektrale Merkmale
    double pTotal = 0.0, pSub = 0.0, pLow = 0.0, pHigh = 0.0, fWeighted = 0.0;
    double logSum = 0.0, linSum = 0.0;
    int flatCount = 0;

    for (int k = 1; k < numBins; ++k)
    {
        const double m = fftData[(size_t) k];
        const double p = m * m;
        const double f = (double) k * binHz;
        pTotal += p;
        fWeighted += p * f;
        if (f < 60.0)   pSub += p;
        if (f < 150.0)  pLow += p;
        if (f > 4000.0) pHigh += p;
        if (f >= 100.0 && f <= 10000.0)
        {
            logSum += std::log (p + 1.0e-20);
            linSum += p;
            ++flatCount;
        }
    }

    if (pTotal <= 0.0)
        return;

    const float centroid = (float) (fWeighted / pTotal);
    const float flatness = flatCount > 0 && linSum > 0.0
                         ? (float) (std::exp (logSum / flatCount) / (linSum / flatCount)) : 0.0f;

    // Harmonizität: Autokorrelation = IFFT(|X|^2)
    for (int k = 0; k < numBins; ++k)
    {
        const float m = fftData[(size_t) k];
        acData[(size_t) (2 * k)] = m * m;
        acData[(size_t) (2 * k + 1)] = 0.0f;
    }
    std::fill (acData.begin() + 2 * numBins, acData.end(), 0.0f);
    fft.performRealOnlyInverseTransform (acData.data());

    float harmonicity = 0.0f, pitch = 0.0f;
    const float r0 = acData[0];
    if (r0 > 0.0f)
    {
        int bestLag = -1;
        float best = 0.0f;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            const float v = acData[(size_t) lag] / (r0 * juce::jmax (0.05f, windowAc[(size_t) lag]));
            const float prev = acData[(size_t) lag - 1], next = acData[(size_t) lag + 1];
            if (v > best && acData[(size_t) lag] >= prev && acData[(size_t) lag] >= next)
            {
                best = v;
                bestLag = lag;
            }
        }
        harmonicity = juce::jlimit (0.0f, 1.0f, best);
        if (bestLag > 0)
        {
            // parabolische Interpolation für genauere Tonhöhe
            const float y0 = acData[(size_t) bestLag - 1], y1 = acData[(size_t) bestLag], y2 = acData[(size_t) bestLag + 1];
            const float den = y0 - 2.0f * y1 + y2;
            const float delta = std::abs (den) > 1.0e-12f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (y0 - y2) / den) : 0.0f;
            pitch = (float) sr / ((float) bestLag + delta);
        }
    }

    // Crest über ~1,5 s: Spitzenwert (Hold) gegen Langzeit-RMS -> Impulshaftigkeit
    peakHold = juce::jmax (framePeak, peakHold * crestPeakCoef);
    rmsLong = rmsLong <= 0.0f ? frameRms * frameRms : crestRmsCoef * rmsLong + (1.0f - crestRmsCoef) * frameRms * frameRms;
    const float crest = juce::Decibels::gainToDecibels (peakHold, -120.0f)
                      - 10.0f * std::log10 (juce::jmax (1.0e-12f, rmsLong));
    const float width = ms > 1.0e-12f ? juce::jlimit (0.0f, 2.0f, std::sqrt (ss / ms)) : 0.0f;

    // Langzeit-Mittel; Startwert = erster Frame
    const bool first = feat.activeSec <= 0.0f;
    auto avg = [this, first] (float& dst, float v) { dst = first ? v : featCoef * dst + (1.0f - featCoef) * v; };

    avg (feat.centroidHz, std::log2 (juce::jmax (20.0f, centroid)));   // log-Domäne mitteln
    avg (feat.flatness, flatness);
    avg (feat.subRatio, (float) (pSub / pTotal));
    avg (feat.lowRatio, (float) (pLow / pTotal));
    avg (feat.highRatio, (float) (pHigh / pTotal));
    feat.onsetRate = featCoef * feat.onsetRate + (1.0f - featCoef) * (onset ? 1.0f / hopSec : 0.0f);
    avg (feat.harmonicity, harmonicity);
    // Tonhöhen-Schwankung: Änderung zum vorigen Frame, Tonwechsel (> 80 Cent) ausgenommen
    if (harmonicity > 0.6f && pitch > 0.0f)
    {
        if (lastPitch > 0.0f)
        {
            const float cents = std::abs (1200.0f * std::log2 (pitch / lastPitch));
            if (cents < 80.0f)
                feat.pitchVarCents = pitchVarInit ? featCoef * feat.pitchVarCents + (1.0f - featCoef) * cents : cents;
            pitchVarInit = pitchVarInit || cents < 80.0f;
        }
        lastPitch = pitch;
    }
    else
    {
        lastPitch = 0.0f;
    }

    if (harmonicity > 0.5f && pitch > 0.0f)
    {
        const float lp = std::log2 (pitch);
        feat.pitchHz = feat.pitchHz <= 0.0f ? lp : featCoef * feat.pitchHz + (1.0f - featCoef) * lp;
    }
    avg (feat.crestDb, crest);
    avg (feat.width, width);
    feat.activeSec += hopSec;

    out->fCentroid.store (std::exp2 (feat.centroidHz), std::memory_order_relaxed);
    out->fFlatness.store (feat.flatness, std::memory_order_relaxed);
    out->fSub.store (feat.subRatio, std::memory_order_relaxed);
    out->fLow.store (feat.lowRatio, std::memory_order_relaxed);
    out->fHigh.store (feat.highRatio, std::memory_order_relaxed);
    out->fOnsetRate.store (feat.onsetRate, std::memory_order_relaxed);
    out->fHarmonicity.store (feat.harmonicity, std::memory_order_relaxed);
    out->fPitch.store (feat.pitchHz > 0.0f ? std::exp2 (feat.pitchHz) : 0.0f, std::memory_order_relaxed);
    out->fPitchVar.store (feat.pitchVarCents, std::memory_order_relaxed);
    out->fCrest.store (feat.crestDb, std::memory_order_relaxed);
    out->fWidth.store (feat.width, std::memory_order_relaxed);
    out->fActiveSec.store (feat.activeSec, std::memory_order_relaxed);
}
} // namespace ame
