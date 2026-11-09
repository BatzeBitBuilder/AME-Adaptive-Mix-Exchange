// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "VuMeter.h"
#include "LookAndFeel.h"

namespace ame::ui
{
namespace
{
constexpr float vuMin = -20.0f, vuMax = 3.0f, refDbfs = -18.0f;
constexpr float arcStart = -0.82f, arcEnd = 0.82f;   // Bogen in Radiant (0 = oben)
}

VuMeter::VuMeter (juce::String c, Mode m) : caption (std::move (c)), mode (m)
{
    needle = mode == Mode::GainReduction ? 1.0f : 0.0f;
    setOpaque (false);
}

float VuMeter::positionForVu (float vu) const
{
    // klassische VU-Skala: linear in Spannung
    auto lin = [] (float db) { return std::pow (10.0f, db / 20.0f); };
    return juce::jlimit (0.0f, 1.0f, (lin (vu) - lin (vuMin)) / (lin (vuMax) - lin (vuMin)));
}

float VuMeter::positionForGr (float gr) const
{
    return juce::jlimit (0.0f, 1.0f, 1.0f - gr / 20.0f);
}

void VuMeter::update (float rmsDb, float peakDb, float dt)
{
    const float target = positionForVu (rmsDb - refDbfs);
    const float c = std::exp (-dt / 0.09f);   // ~300 ms Anstiegszeit (VU-Ballistik)
    needle = c * needle + (1.0f - c) * target;
    peakHold = peakDb > -1.0f ? 0.6f : juce::jmax (0.0f, peakHold - dt);
    repaint();
}

void VuMeter::updateGainReduction (float grDb, float dt)
{
    const float target = positionForGr (grDb);
    const float c = std::exp (-dt / 0.06f);
    needle = c * needle + (1.0f - c) * target;
    repaint();
}

void VuMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);

    // Gehäuse
    g.setColour (colours::bakelite);
    g.fillRoundedRectangle (r, 6.0f);
    auto face = r.reduced (6.0f);
    juce::ColourGradient glow (juce::Colour (0xfffff0c8), face.getCentreX(), face.getBottom(),
                               juce::Colour (0xffe0a848), face.getX(), face.getY(), true);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (face, 3.0f);

    // Geometrie: Drehpunkt unterhalb der Skala
    auto scaleArea = face.withTrimmedBottom (16.0f);   // unten: Beschriftungsleiste
    const auto pivot = juce::Point<float> (scaleArea.getCentreX(), scaleArea.getBottom() + scaleArea.getHeight() * 0.22f);
    const float radius = juce::jmin (scaleArea.getHeight() * 1.02f, scaleArea.getWidth() * 0.56f);
    auto angleFor = [] (float pos) { return arcStart + pos * (arcEnd - arcStart); };
    auto pointAt = [&] (float pos, float rr) { return pivot.getPointOnCircumference (rr, angleFor (pos)); };

    const auto ink = juce::Colour (0xff2a1c10);

    // Skalenbogen + roter Bereich
    {
        juce::Path arc;
        arc.addCentredArc (pivot.x, pivot.y, radius * 0.78f, radius * 0.78f, 0.0f, arcStart, arcEnd, true);
        g.setColour (ink);
        g.strokePath (arc, juce::PathStrokeType (1.2f));

        const float redFrom = mode == Mode::Level ? positionForVu (0.0f) : 0.0f;
        const float redTo   = mode == Mode::Level ? 1.0f : positionForGr (10.0f);
        juce::Path red;
        red.addCentredArc (pivot.x, pivot.y, radius * 0.81f, radius * 0.81f, 0.0f, angleFor (redFrom), angleFor (redTo), true);
        g.setColour (juce::Colour (0xffb8281c));
        g.strokePath (red, juce::PathStrokeType (4.0f));
    }

    // Teilstriche + Beschriftung
    g.setFont (fonts::mono (juce::jlimit (9.0f, 11.0f, face.getHeight() * 0.11f), true));
    auto tick = [&] (float pos, const juce::String& text, bool major)
    {
        g.setColour (pos > 0.999f && mode == Mode::Level ? juce::Colour (0xffb8281c) : ink);
        g.drawLine ({ pointAt (pos, radius * 0.78f), pointAt (pos, radius * (major ? 0.90f : 0.85f)) }, major ? 1.4f : 1.0f);
        if (text.isNotEmpty())
        {
            const auto tp = pointAt (pos, radius * 1.0f);
            g.drawText (text, juce::Rectangle<float> (26.0f, 11.0f).withCentre (tp), juce::Justification::centred);
        }
    };

    if (mode == Mode::Level)
    {
        for (int v : { -20, -10, -7, -5, -3, -1, 0, 1, 2, 3 })
        {
            const bool label = v == -20 || v == -10 || v == -5 || v == -3 || v == 0 || v == 3;
            tick (positionForVu ((float) v), label ? (v > 0 ? "+" : "") + juce::String (v) : juce::String(), label);
        }
    }
    else
    {
        for (int gr : { 0, 2, 4, 6, 10, 15, 20 })
            tick (positionForGr ((float) gr), juce::String (gr), true);
    }

    // Beschriftung
    g.setColour (ink);
    g.setFont (fonts::display (juce::jlimit (12.0f, 20.0f, face.getHeight() * 0.19f)));
    g.drawText (mode == Mode::Level ? "VU" : "GR", scaleArea.withTrimmedTop (scaleArea.getHeight() * 0.50f).withHeight (scaleArea.getHeight() * 0.26f),
                juce::Justification::centred);

    // Zeiger
    {
        g.saveState();
        g.reduceClipRegion (face.toNearestInt());
        const auto tip = pointAt (needle, radius * 0.9f);
        g.setColour (juce::Colours::black.withAlpha (0.2f));
        g.drawLine ({ pivot.translated (2.0f, 1.0f), tip.translated (2.0f, 1.0f) }, 1.6f);
        g.setColour (ink);
        g.drawLine ({ pivot, tip }, 1.6f);
        g.restoreState();
    }

    // Abdeckung des Drehpunkts mit Beschriftung
    auto plate = face.withTop (scaleArea.getBottom());
    g.setColour (colours::bakelite);
    g.fillRect (plate);
    g.setColour (colours::cream);
    g.setFont (fonts::label (14.0f));
    g.drawText (caption.toUpperCase(), plate, juce::Justification::centred);

    // Peak-Lampe
    if (mode == Mode::Level)
    {
        auto lamp = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ face.getRight() - 10.0f, face.getY() + 10.0f });
        g.setColour (peakHold > 0.0f ? juce::Colour (0xffff3a2a) : juce::Colour (0xff5a2a1a));
        g.fillEllipse (lamp);
        if (peakHold > 0.0f)
        {
            g.setColour (juce::Colour (0x55ff3a2a));
            g.fillEllipse (lamp.expanded (3.0f));
        }
    }

    // Glas-Reflex
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.35f).reduced (2.0f, 2.0f), 3.0f);
}
} // namespace ame::ui
