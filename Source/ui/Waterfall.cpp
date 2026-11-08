// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "Waterfall.h"
#include "LookAndFeel.h"

namespace mtm::ui
{
Waterfall::Waterfall()
{
    setOpaque (false);
}

juce::Rectangle<int> Waterfall::imageArea() const
{
    return getLocalBounds().reduced (4);
}

void Waterfall::resized()
{
    const auto a = imageArea();
    if (a.isEmpty())
        return;

    image = juce::Image (juce::Image::RGB, a.getWidth(), a.getHeight(), true);
    juce::Graphics (image).fillAll (colours::display);

    // Spalte -> Anzeige-Punkt (log. Frequenzachse, identisch zu den Analyzern)
    pointForColumn.resize ((size_t) a.getWidth());
    fracForColumn.resize ((size_t) a.getWidth());
    for (int x = 0; x < a.getWidth(); ++x)
    {
        const float pos = (float) x / (float) juce::jmax (1, a.getWidth() - 1) * (float) (kNumDisplayPoints - 1);
        pointForColumn[(size_t) x] = juce::jmin ((int) pos, kNumDisplayPoints - 2);
        fracForColumn[(size_t) x] = pos - (float) pointForColumn[(size_t) x];
    }
}

void Waterfall::clear()
{
    if (image.isValid())
        juce::Graphics (image).fillAll (colours::display);
    repaint();
}

void Waterfall::pushRow (const std::array<float, kNumDisplayPoints>& db)
{
    if (! image.isValid())
        return;

    const int w = image.getWidth(), h = image.getHeight();
    image.moveImageSection (0, 1, 0, 0, w, h - 1);

    juce::Image::BitmapData bd (image, 0, 0, w, 1, juce::Image::BitmapData::writeOnly);
    for (int x = 0; x < w; ++x)
    {
        const int i = pointForColumn[(size_t) x];
        const float f = fracForColumn[(size_t) x];
        const float v = (1.0f - f) * db[(size_t) i] + f * db[(size_t) i + 1];
        bd.setPixelColour (x, 0, heatColour ((v - lo) / (hi - lo)));
    }
    repaint();
}

void Waterfall::paint (juce::Graphics& g)
{
    const auto a = imageArea().toFloat();
    drawDisplayBezel (g, a);
    if (image.isValid())
        g.drawImageAt (image, (int) a.getX(), (int) a.getY());

    // Scanlines für Röhrenoptik
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    for (float y = a.getY(); y < a.getBottom(); y += 3.0f)
        g.drawHorizontalLine ((int) y, a.getX(), a.getRight());

    if (caption.isNotEmpty())
    {
        g.setFont (fonts::mono (11.0f, true));
        const float tw = (float) caption.length() * 7.0f + 12.0f;
        auto cap = juce::Rectangle<float> (a.getX(), a.getY(), juce::jmin (tw, a.getWidth()), 16.0f);
        g.setColour (colours::display.withAlpha (0.8f));
        g.fillRect (cap);
        g.setColour (colours::phosphor);
        g.drawText (caption, cap.reduced (6.0f, 0.0f), juce::Justification::centredLeft, true);
    }
}
} // namespace mtm::ui
