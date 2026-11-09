// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "LookAndFeel.h"
#include "Fonts.h"
#include "Settings.h"

namespace ame::ui
{
// Klappschalter für einklappbare Bereiche: Dreieck (▼ offen, ▶ eingeklappt) + optionaler Text.
// Zustand (eingeklappt) global in den Benutzer-Einstellungen unter settingsKey.
class FoldButton : public juce::Button
{
public:
    explicit FoldButton (const juce::String& key) : juce::Button ({}), settingsKey (key)
    {
        setClickingTogglesState (true);
        setToggleState (settings::get (settingsKey, "0") == "1", juce::dontSendNotification);
        onClick = [this]
        {
            settings::set (settingsKey, isFolded() ? "1" : "0");
            if (onFoldChanged)
                onFoldChanged();
        };
    }

    bool isFolded() const { return getToggleState(); }
    void setLabel (const juce::String& t) { label = t; repaint(); }
    int getIdealWidth() const
    {
        return label.isEmpty() ? 22 : 26 + juce::roundToInt (juce::GlyphArrangement::getStringWidth (fonts::label (16.0f), label));
    }

    std::function<void()> onFoldChanged;

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto r = getLocalBounds().toFloat();
        const auto c = highlighted || down ? colours::rust : colours::ink;
        auto tri = r.removeFromLeft (18.0f).withSizeKeepingCentre (10.0f, 10.0f);
        juce::Path p;
        if (isFolded())
            p.addTriangle (tri.getX() + 1.0f, tri.getY(), tri.getRight() - 1.0f, tri.getCentreY(), tri.getX() + 1.0f, tri.getBottom());
        else
            p.addTriangle (tri.getX(), tri.getY() + 1.0f, tri.getRight(), tri.getY() + 1.0f, tri.getCentreX(), tri.getBottom() - 1.0f);
        g.setColour (c);
        g.fillPath (p);
        if (label.isNotEmpty())
        {
            g.setFont (fonts::label (16.0f));
            g.drawText (label, r.withTrimmedLeft (4.0f), juce::Justification::centredLeft, false);
        }
    }

private:
    juce::String settingsKey, label;
};

// Eingeklappte Spalte: schmale Platte mit senkrechtem Titel
inline void drawFoldedColumn (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    drawEngravedPanel (g, r, {});
    g.saveState();
    const auto centre = r.getCentre();
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, centre.x, centre.y));
    g.setFont (juce::Font (fonts::label (16.0f)).withExtraKerningFactor (0.12f));
    g.setColour (colours::inkDim);
    auto rotated = juce::Rectangle<float> (r.getHeight() - 40.0f, r.getWidth()).withCentre (centre).translated (-14.0f, 0.0f);
    g.drawText (title, rotated, juce::Justification::centredRight, false);
    g.restoreState();
}
} // namespace ame::ui
