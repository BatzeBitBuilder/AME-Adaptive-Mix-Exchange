// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "EqGraph.h"
#include "LookAndFeel.h"
#include "i18n/Lang.h"
#include "Parameters.h"
#include "dsp/Biquad.h"
#include "dsp/ChannelStrip.h"
#include "dsp/SpectrumAnalyzer.h"

namespace ame::ui
{
namespace
{
constexpr float fMin = 20.0f, fMax = 20000.0f;
constexpr double displaySr = 48000.0;
constexpr float nodeRadius = 7.0f;
}

EqGraph::EqGraph (juce::AudioProcessorValueTreeState& state, MixHub& h, int s)
    : apvts (state), hub (h), slot (s),
      reader (state),
      nodes { { { pid::lsFreq,  pid::lsGain, pid::lsQ, nullptr,     "L"  },
                { pid::p1Freq,  pid::p1Gain, pid::p1Q, nullptr,     "1"  },
                { pid::p2Freq,  pid::p2Gain, pid::p2Q, nullptr,     "2"  },
                { pid::hsFreq,  pid::hsGain, pid::hsQ, nullptr,     "H"  },
                { pid::hpfFreq, nullptr,     nullptr,  pid::hpfOn,  "HP" },
                { pid::lpfFreq, nullptr,     nullptr,  pid::lpfOn,  "LP" } } }
{
    spectrumDb.fill (-100.0f);
    peakDb.fill (-100.0f);
}

juce::Rectangle<float> EqGraph::plotArea() const
{
    return getLocalBounds().toFloat().reduced (5.0f).withTrimmedLeft (24.0f).withTrimmedBottom (13.0f);
}

float EqGraph::xForFreq (float hz) const
{
    const auto p = plotArea();
    return p.getX() + p.getWidth() * std::log (juce::jlimit (fMin, fMax, hz) / fMin) / std::log (fMax / fMin);
}

float EqGraph::freqForX (float x) const
{
    const auto p = plotArea();
    return fMin * std::pow (fMax / fMin, juce::jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth()));
}

float EqGraph::yForDb (float db) const
{
    const auto p = plotArea();
    return juce::jmap (juce::jlimit (-rangeDb, rangeDb, db), -rangeDb, rangeDb, p.getBottom(), p.getY());
}

float EqGraph::dbForY (float y) const
{
    const auto p = plotArea();
    return juce::jmap (y, p.getBottom(), p.getY(), -rangeDb, rangeDb);
}

float EqGraph::ySpectrum (float db) const
{
    const auto p = plotArea();
    return juce::jmap (juce::jlimit (-80.0f, 0.0f, db), -80.0f, 0.0f, p.getBottom(), p.getY());
}

float EqGraph::value (const char* id) const
{
    return apvts.getRawParameterValue (id)->load();
}

void EqGraph::setValue (const char* id, float v)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

bool EqGraph::nodeEnabled (const Node& n) const
{
    return n.onId == nullptr || value (n.onId) > 0.5f;
}

juce::Point<float> EqGraph::nodePosition (const Node& n) const
{
    const float x = xForFreq (value (n.freqId));
    const float y = n.gainId != nullptr ? yForDb (value (n.gainId)) : yForDb (0.0f);
    return { x, y };
}

int EqGraph::hitNode (juce::Point<float> pos) const
{
    int best = -1;
    float bestDist = nodeRadius * 2.0f;
    for (int i = 0; i < (int) nodes.size(); ++i)
    {
        if (! nodeEnabled (nodes[(size_t) i])) continue;
        const float d = nodePosition (nodes[(size_t) i]).getDistanceFrom (pos);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

EqGraph::ManualKey EqGraph::manualKey (const ChannelStrip::Params& p) noexcept
{
    return { (float) p.hpfOn, p.hpfFreq, (float) p.hpfSlope, (float) p.lpfOn, p.lpfFreq, (float) p.lpfSlope,
             p.eqFreq[0], p.eqFreq[1], p.eqFreq[2], p.eqFreq[3],
             p.eqGainDb[0], p.eqGainDb[1], p.eqGainDb[2], p.eqGainDb[3],
             p.eqQ[0], p.eqQ[1], p.eqQ[2], p.eqQ[3], (float) p.lowBell, (float) p.highBell };
}

void EqGraph::refresh (float dtSec, bool audioActive)
{
    bool changed = audioActive;

    if (auto* d = hub.getAudioData (slot))
    {
        if (audioActive)
        {
            const float fall = 12.0f * dtSec;   // Peak-Hold fällt mit 12 dB/s
            for (size_t i = 0; i < (size_t) kNumDisplayPoints; ++i)
            {
                spectrumDb[i] = d->displayDb[i].load (std::memory_order_relaxed);
                peakDb[i] = juce::jmax (spectrumDb[i], peakDb[i] - fall);
            }
        }

        auto cut = cutDb;
        for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
            cut[k] = d->appliedCutDb[k].load (std::memory_order_relaxed);
        const float hpf = d->appliedHpfHz.load(), gain = d->appliedGainDb.load();
        if (cut != cutDb || hpf != hpfHz || gain != gainDb)
        {
            cutDb = cut; hpfHz = hpf; gainDb = gain;
            adaptiveDirty = true;
            changed = true;
        }

        const bool has = d->analysisCounter.load() > 0;
        changed = changed || has != hasSpectrum;
        hasSpectrum = has;
    }

    // Parameter (auch Automation / Presets): Kurve und Punkte nur bei Änderung neu
    ChannelStrip::Params p;
    reader.read (p);
    if (const auto key = manualKey (p); ! keyValid || key != shownKey)
    {
        shown = p; shownKey = key; keyValid = true;
        manualDirty = true;
        changed = true;
    }

    if (changed)
        repaint();
}

void EqGraph::updateCurves (int steps)
{
    if (! keyValid)   // gezeichnet vor dem ersten refresh()
    {
        reader.read (shown);
        shownKey = manualKey (shown);
        keyValid = true;
        manualDirty = true;
    }

    if ((int) curveHz.size() != steps + 1)
    {
        const auto p = plotArea();
        curveHz.resize ((size_t) steps + 1);
        manualDb.resize (curveHz.size());
        adaptiveDb.resize (curveHz.size());
        for (int i = 0; i <= steps; ++i)
            curveHz[(size_t) i] = freqForX (p.getX() + p.getWidth() * (float) i / (float) steps);
        manualDirty = adaptiveDirty = true;
    }

    if (manualDirty)
    {
        const ChannelStrip::ManualResponse response (shown, displaySr);
        for (size_t i = 0; i < curveHz.size(); ++i)
            manualDb[i] = response.dbAt (curveHz[i]);
        manualDirty = false;
    }

    if (adaptiveDirty)
    {
        const ChannelStrip::AdaptiveResponse full (cutDb, hpfHz, gainDb), shape (cutDb, hpfHz, 0.0f);
        const auto& hz = getDisplayPointTable();
        for (size_t i = 0; i < (size_t) kNumDisplayPoints; ++i)
            responseFine[i] = full.dbAt (hz[i]);
        for (size_t i = 0; i < curveHz.size(); ++i)
            adaptiveDb[i] = shape.dbAt (curveHz[i]);
        adaptiveDirty = false;
    }
}

void EqGraph::paint (juce::Graphics& g)
{
    const auto p = plotArea();
    drawDisplayBezel (g, p);

    const int steps = juce::jmax (8, juce::roundToInt (p.getWidth() / 2.0f));
    updateCurves (steps);
    // Anzeige-Punkte liegen logarithmisch gleichmäßig: x ist linear im Index
    auto xForPoint = [&p] (int i) { return p.getX() + p.getWidth() * (float) i / (float) (kNumDisplayPoints - 1); };

    // Raster (Phosphor)
    g.setFont (fonts::mono (10.0f, true));
    for (float hz : { 30.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (hz);
        g.setColour (colours::displayGrid);
        g.drawVerticalLine (juce::roundToInt (x), p.getY(), p.getBottom());
        g.setColour (colours::inkDim);
        g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 20.0f, p.getBottom() + 3.0f, 40.0f, 11.0f), juce::Justification::centred);
    }
    for (int db : { -12, -6, 0, 6, 12 })
    {
        const float y = yForDb ((float) db);
        g.setColour (db == 0 ? colours::phosphorDim.withAlpha (0.6f) : colours::displayGrid);
        g.drawHorizontalLine (juce::roundToInt (y), p.getX(), p.getRight());
        g.setColour (colours::inkDim);
        g.drawText (juce::String (db), juce::Rectangle<float> (p.getX() - 27.0f, y - 6.0f, 20.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    if (hasSpectrum)
    {
        juce::Path fill, post, peak;
        fill.startNewSubPath (xForPoint (0), p.getBottom());
        for (int i = 0; i < kNumDisplayPoints; ++i)
        {
            const float x = xForPoint (i);
            const float pre = spectrumDb[(size_t) i];
            const float response = responseFine[(size_t) i];
            fill.lineTo (x, ySpectrum (pre));
            const float yPost = ySpectrum (pre + response), yPeak = ySpectrum (peakDb[(size_t) i] + response);
            if (i == 0) { post.startNewSubPath (x, yPost); peak.startNewSubPath (x, yPeak); }
            else        { post.lineTo (x, yPost);          peak.lineTo (x, yPeak); }
        }
        fill.lineTo (xForPoint (kNumDisplayPoints - 1), p.getBottom());
        fill.closeSubPath();

        // Regenbogen-Füllung nach Frequenz (vor Engine)
        juce::ColourGradient rainbow (rainbowForFrequency (20.0f).withAlpha (0.55f), p.getX(), 0.0f,
                                      rainbowForFrequency (20000.0f).withAlpha (0.55f), p.getRight(), 0.0f, false);
        for (float hz : { 50.0f, 120.0f, 300.0f, 800.0f, 2000.0f, 5000.0f, 10000.0f })
            rainbow.addColour ((xForFreq (hz) - p.getX()) / p.getWidth(), rainbowForFrequency (hz).withAlpha (0.55f));
        g.setGradientFill (rainbow);
        g.fillPath (fill);

        // nach Engine: Phosphor-Linie, Peak-Hold gepunktet
        drawPhosphorGlow (g, post, colours::phosphor, 1.3f);
        juce::Path dotted;
        const float dots[] { 1.5f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dotted, peak, dots, 2);
        g.setColour (colours::amberLamp.withAlpha (0.85f));
        g.fillPath (dotted);
    }

    // Kurven
    juce::Path manual, adaptive;
    bool anyAdaptive = std::abs (gainDb) > 0.05f || hpfHz >= 20.0f;
    for (auto cdb : cutDb) anyAdaptive |= cdb < -0.05f;

    for (int i = 0; i <= steps; ++i)
    {
        const float x = p.getX() + p.getWidth() * (float) i / (float) steps;
        const float ym = yForDb (manualDb[(size_t) i]);
        const float ya = yForDb (adaptiveDb[(size_t) i]);
        if (i == 0) { manual.startNewSubPath (x, ym); adaptive.startNewSubPath (x, ya); }
        else        { manual.lineTo (x, ym);          adaptive.lineTo (x, ya); }
    }

    if (anyAdaptive)
    {
        juce::Path dashed;
        const float dashes[] { 6.0f, 4.0f };
        juce::PathStrokeType (1.8f).createDashedStroke (dashed, adaptive, dashes, 2);
        g.setColour (colours::amber);
        g.fillPath (dashed);
    }

    drawPhosphorGlow (g, manual, colours::cream, 1.8f);

    // Punkte (Bernstein-Lampen)
    g.setFont (fonts::label (13.0f));
    for (int i = 0; i < (int) nodes.size(); ++i)
    {
        const auto& n = nodes[(size_t) i];
        if (! nodeEnabled (n)) continue;
        const auto pos = nodePosition (n);
        const bool active = i == dragNode || i == hoverNode;
        auto r = juce::Rectangle<float> (nodeRadius * 2.0f, nodeRadius * 2.0f).withCentre (pos);
        if (active)
        {
            g.setColour (colours::amberLamp.withAlpha (0.3f));
            g.fillEllipse (r.expanded (5.0f));
        }
        g.setColour (active ? juce::Colour (0xfffff2c0) : colours::amberLamp);
        g.fillEllipse (r);
        g.setColour (colours::bakelite);
        g.drawEllipse (r, 1.0f);
        g.drawText (n.label, r.expanded (4.0f), juce::Justification::centred);
    }

    // Legende
    g.setFont (fonts::mono (10.0f, true));
    auto legend = p.reduced (8.0f, 5.0f).removeFromTop (12.0f);
    g.setColour (colours::cream);
    g.drawText ("EQ", legend.removeFromRight (24.0f), juce::Justification::centredRight);
    g.setColour (colours::amber);
    g.drawText ("ENGINE", legend.removeFromRight (56.0f), juce::Justification::centredRight);
    g.setColour (colours::amberLamp);
    g.drawText ("PEAK", legend.removeFromRight (44.0f), juce::Justification::centredRight);
    g.setColour (colours::phosphor);
    g.drawText (i18n::tr ("NACH ENGINE", "AFTER ENGINE"), legend.removeFromRight (90.0f), juce::Justification::centredRight);
    g.setColour (rainbowForFrequency (500.0f));
    g.drawText (i18n::tr ("SPEKTRUM", "SPECTRUM"), legend.removeFromRight (70.0f), juce::Justification::centredRight);
}

void EqGraph::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h != hoverNode) { hoverNode = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void EqGraph::mouseDown (const juce::MouseEvent& e)
{
    dragNode = hitNode (e.position);
    if (dragNode < 0) return;

    const auto& n = nodes[(size_t) dragNode];
    for (auto* id : { n.freqId, n.gainId })
        if (id != nullptr)
            if (auto* prm = apvts.getParameter (id))
                prm->beginChangeGesture();
}

void EqGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNode < 0) return;
    const auto& n = nodes[(size_t) dragNode];
    setValue (n.freqId, freqForX (e.position.x));
    if (n.gainId != nullptr)
        setValue (n.gainId, dbForY (e.position.y));
    keyValid = false;   // Kurve sofort mit den neuen Werten
    repaint();
}

void EqGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragNode >= 0)
    {
        const auto& n = nodes[(size_t) dragNode];
        for (auto* id : { n.freqId, n.gainId })
            if (id != nullptr)
                if (auto* prm = apvts.getParameter (id))
                    prm->endChangeGesture();
    }
    dragNode = -1;
    repaint();
}

void EqGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h >= 0 && nodes[(size_t) h].gainId != nullptr)
    {
        if (auto* prm = apvts.getParameter (nodes[(size_t) h].gainId))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (0.0f));
            prm->endChangeGesture();
        }
    }
}

void EqGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int h = hitNode (e.position);
    if (h < 0 || nodes[(size_t) h].qId == nullptr) return;

    if (auto* prm = apvts.getParameter (nodes[(size_t) h].qId))
    {
        const float q = value (nodes[(size_t) h].qId) * std::pow (1.15f, w.deltaY * 4.0f);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (q));
        prm->endChangeGesture();
    }
}
} // namespace ame::ui
