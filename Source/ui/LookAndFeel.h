// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Fonts.h"

namespace ame::ui
{
// Vintage-Palette: Creme-Front, Walnuss, Bakelit, Bernstein; Analyzer im Messgeräte-Stil (Phosphor)
namespace colours
{
// Gehäuse / Front
inline const juce::Colour walnut      { 0xff4a2c18 };
inline const juce::Colour walnutLight { 0xff7a4a28 };
inline const juce::Colour cream       { 0xffe8dcc0 };
inline const juce::Colour creamDark   { 0xffdccdab };
inline const juce::Colour ink         { 0xff2e2218 };
inline const juce::Colour inkDim      { 0xff75644d };
inline const juce::Colour engrave     { 0xffa8946c };
inline const juce::Colour bakelite    { 0xff1b1916 };
inline const juce::Colour amber       { 0xffe8b45a };
inline const juce::Colour amberLamp   { 0xfff2c56b };
inline const juce::Colour rust        { 0xffa4471c };

// Anzeigen
inline const juce::Colour display     { 0xff06120a };
inline const juce::Colour displayGrid { 0xff1c4426 };
inline const juce::Colour phosphor    { 0xff7dff9a };
inline const juce::Colour phosphorDim { 0xff3f9a55 };

// Kompatible Namen für bestehenden Code
inline const juce::Colour background = walnut;
inline const juce::Colour panel      = creamDark;
inline const juce::Colour outline    = engrave;
inline const juce::Colour text       = ink;
inline const juce::Colour textDim    = inkDim;
inline const juce::Colour accent     = rust;
inline const juce::Colour warn       { 0xffc0392b };
inline const juce::Colour meterLow   { 0xff4f8a3a };
inline const juce::Colour meterHigh  { 0xffc0392b };
} // namespace colours

// Farbhilfen für die Anzeigen
juce::Colour heatColour (float t);               // 0..1: dunkelblau -> grün -> gelb -> orange -> rot
juce::Colour rainbowForFrequency (float hz);     // tief = rot ... hoch = violett
juce::Colour maskingColour (float t);            // 0..1: gelb -> orange -> rot

// Zeichenhilfen
void drawPhosphorGlow (juce::Graphics&, const juce::Path&, juce::Colour, float thickness);
void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius);
void drawEngravedPanel (juce::Graphics&, juce::Rectangle<float>, const juce::String& title);
void drawDisplayBezel (juce::Graphics&, juce::Rectangle<float>);

// Statischer Hintergrund (Gehäuse, Platten, Skalen): einmal je Größe und Skalierung in ein Bild gerendert,
// danach nur noch kopiert. Ohne das würde er bei jedem Teil-Repaint der Anzeigen (30-mal je Sekunde) neu gezeichnet.
class BackdropCache
{
public:
    void invalidate() noexcept { image = {}; }

    template <typename PaintFn>
    void draw (juce::Graphics& g, juce::Rectangle<int> bounds, bool opaque, PaintFn&& paintFn)
    {
        if (bounds.isEmpty())
            return;
        const float scale = juce::jlimit (0.25f, 4.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const int w = juce::jmax (1, (int) std::ceil ((float) bounds.getWidth() * scale)),
                  h = juce::jmax (1, (int) std::ceil ((float) bounds.getHeight() * scale));
        if (image.isNull() || image.getWidth() != w || image.getHeight() != h || std::abs (scale - imageScale) > 1.0e-4f)
        {
            image = juce::Image (opaque ? juce::Image::RGB : juce::Image::ARGB, w, h, true);
            imageScale = scale;
            juce::Graphics ig (image);
            ig.addTransform (juce::AffineTransform::scale (scale).translated (-(float) bounds.getX() * scale, -(float) bounds.getY() * scale));
            paintFn (ig);
        }
        g.setOpacity (1.0f);
        g.drawImageTransformed (image, juce::AffineTransform::scale (1.0f / scale).translated ((float) bounds.getX(), (float) bounds.getY()), false);
    }

private:
    juce::Image image;
    float imageScale = 0.0f;
};

class VintageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VintageLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawComboBoxTextWhenNothingSelected (juce::Graphics&, juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
};

using AmeLookAndFeel = VintageLookAndFeel;
} // namespace ame::ui
