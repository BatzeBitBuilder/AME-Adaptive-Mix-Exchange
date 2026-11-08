// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "LookAndFeel.h"

namespace mtm::ui
{
//==============================================================================
juce::Colour heatColour (float t)
{
    t = juce::jlimit (0.0f, 1.0f, t);
    static const juce::Colour stops[] { juce::Colour (0xff06120a), juce::Colour (0xff12306a), juce::Colour (0xff1f8a8a),
                                        juce::Colour (0xff4fc35a), juce::Colour (0xffe8d23a), juce::Colour (0xfff08a2a),
                                        juce::Colour (0xffe03a2a), juce::Colour (0xfffff0e0) };
    const float x = t * (float) (std::size (stops) - 1);
    const int i = juce::jmin ((int) x, (int) std::size (stops) - 2);
    return stops[i].interpolatedWith (stops[i + 1], x - (float) i);
}

juce::Colour rainbowForFrequency (float hz)
{
    const float t = juce::jlimit (0.0f, 1.0f, std::log (juce::jmax (20.0f, hz) / 20.0f) / std::log (1000.0f));
    return juce::Colour::fromHSV (0.0f + 0.78f * t, 0.75f, 0.95f, 1.0f);   // rot -> violett
}

juce::Colour maskingColour (float t)
{
    t = juce::jlimit (0.0f, 1.0f, t);
    return t < 0.5f ? juce::Colour (0xffe8d23a).interpolatedWith (juce::Colour (0xfff08a2a), t * 2.0f)
                    : juce::Colour (0xfff08a2a).interpolatedWith (juce::Colour (0xffe03a2a), (t - 0.5f) * 2.0f);
}

void drawPhosphorGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c, float thickness)
{
    g.setColour (c.withAlpha (0.12f));
    g.strokePath (p, juce::PathStrokeType (thickness * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (0.25f));
    g.strokePath (p, juce::PathStrokeType (thickness * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void drawScrew (juce::Graphics& g, juce::Point<float> c, float r)
{
    juce::ColourGradient grad (juce::Colour (0xffd8d2c4), c.x - r, c.y - r, juce::Colour (0xff6f6656), c.x + r, c.y + r, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    g.setColour (juce::Colour (0xff4a4236));
    g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 0.8f);
    g.drawLine (c.x - r * 0.7f, c.y + r * 0.2f, c.x + r * 0.7f, c.y - r * 0.2f, 1.2f);
}

void drawEngravedPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    g.setColour (colours::creamDark);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::engrave);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (1.5f), 3.5f, 0.8f);

    if (title.isNotEmpty())
    {
        g.setFont (fonts::label (17.0f));
        g.setColour (colours::ink);
        g.drawText (title, r.reduced (10.0f, 5.0f).removeFromTop (18.0f), juce::Justification::centredLeft);
    }
}

void drawDisplayBezel (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (colours::bakelite);
    g.fillRoundedRectangle (r.expanded (3.0f), 5.0f);
    g.setColour (colours::display);
    g.fillRoundedRectangle (r, 3.0f);
}

//==============================================================================
VintageLookAndFeel::VintageLookAndFeel()
{
    setDefaultSansSerifTypeface (fonts::monoTypeface());

    setColour (juce::ResizableWindow::backgroundColourId, colours::walnut);
    setColour (juce::Slider::textBoxTextColourId, colours::ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::ink);
    setColour (juce::ComboBox::backgroundColourId, colours::cream);
    setColour (juce::ComboBox::outlineColourId, colours::engrave);
    setColour (juce::ComboBox::textColourId, colours::ink);
    setColour (juce::ComboBox::arrowColourId, colours::ink);
    setColour (juce::PopupMenu::backgroundColourId, colours::cream);
    setColour (juce::PopupMenu::textColourId, colours::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::amber);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::ink);
    setColour (juce::TextButton::buttonColourId, colours::bakelite);
    setColour (juce::TextButton::buttonOnColourId, colours::bakelite);
    setColour (juce::TextButton::textColourOffId, colours::cream);
    setColour (juce::TextButton::textColourOnId, colours::amberLamp);
    setColour (juce::ToggleButton::textColourId, colours::ink);
    setColour (juce::TextEditor::backgroundColourId, colours::cream);
    setColour (juce::TextEditor::textColourId, colours::ink);
    setColour (juce::TextEditor::highlightColourId, colours::amber.withAlpha (0.5f));
    setColour (juce::CaretComponent::caretColourId, colours::ink);
}

void VintageLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Skala (gravierte Striche auf der Front)
    g.setColour (colours::ink.withAlpha (0.75f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const float r1 = radius * (i % 5 == 0 ? 0.80f : 0.86f), r2 = radius * 0.98f;
        g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), c.x + r2 * std::sin (a), c.y - r2 * std::cos (a),
                    i % 5 == 0 ? 1.4f : 0.9f);
    }

    // Bernstein-Bogen für den Wert (dezent)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, radius * 0.72f, radius * 0.72f, 0.0f, startAngle, angle, true);
        g.setColour (colours::amber.withAlpha (0.9f));
        g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Bakelit-Knopf mit Riffelung
    const float kr = radius * 0.62f;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - kr + 1.5f, c.y - kr + 2.5f, kr * 2, kr * 2);

    juce::Path skirt;
    constexpr int ridges = 24;
    for (int i = 0; i <= ridges * 2; ++i)
    {
        const float a = (float) i / (float) (ridges * 2) * juce::MathConstants<float>::twoPi;
        const float rr = (i % 2 == 0) ? kr : kr * 0.94f;
        const auto p = c.getPointOnCircumference (rr, a);
        if (i == 0) skirt.startNewSubPath (p); else skirt.lineTo (p);
    }
    skirt.closeSubPath();
    g.setColour (colours::bakelite);
    g.fillPath (skirt);

    juce::ColourGradient cap (juce::Colour (0xff4a443c), c.x - kr * 0.6f, c.y - kr * 0.7f,
                              juce::Colour (0xff0e0d0b), c.x + kr * 0.5f, c.y + kr * 0.7f, true);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - kr * 0.8f, c.y - kr * 0.8f, kr * 1.6f, kr * 1.6f);

    // Zeiger
    const auto p1 = c.getPointOnCircumference (kr * 0.25f, angle);
    const auto p2 = c.getPointOnCircumference (kr * 0.92f, angle);
    g.setColour (colours::cream);
    g.drawLine ({ p1, p2 }, 2.2f);
}

void VintageLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                           juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }

    const float cy = (float) y + (float) h * 0.5f;
    auto groove = juce::Rectangle<float> ((float) x, cy - 3.0f, (float) w, 6.0f);
    g.setColour (colours::bakelite);
    g.fillRoundedRectangle (groove, 3.0f);
    g.setColour (colours::amber);
    g.fillRoundedRectangle (groove.withRight (pos).reduced (1.0f), 2.0f);

    auto cap = juce::Rectangle<float> (12.0f, (float) h * 0.8f).withCentre ({ pos, cy });
    g.setColour (colours::bakelite);
    g.fillRoundedRectangle (cap, 2.0f);
    g.setColour (colours::cream);
    g.drawLine (pos, cap.getY() + 3.0f, pos, cap.getBottom() - 3.0f, 1.5f);
}

void VintageLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float d = juce::jmin (14.0f, r.getHeight() - 4.0f);
    auto lamp = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + d * 0.5f + 2.0f, r.getCentreY() });

    // Kontrolllampe mit Fassung
    g.setColour (juce::Colour (0xff8a7a5a));
    g.fillEllipse (lamp.expanded (2.0f));
    if (b.getToggleState())
    {
        g.setColour (colours::amberLamp.withAlpha (0.35f));
        g.fillEllipse (lamp.expanded (4.0f));
        juce::ColourGradient lit (juce::Colour (0xfffff2c0), lamp.getCentreX() - d * 0.2f, lamp.getCentreY() - d * 0.2f,
                                  juce::Colour (0xffd88a1a), lamp.getRight(), lamp.getBottom(), true);
        g.setGradientFill (lit);
    }
    else
    {
        g.setColour (juce::Colour (0xff4a3a26));
    }
    g.fillEllipse (lamp);

    g.setColour (highlighted ? colours::rust : b.findColour (juce::ToggleButton::textColourId));
    g.setFont (fonts::label (16.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (d + 8.0f), juce::Justification::centredLeft);
}

void VintageLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (r.translated (0.0f, down ? 0.5f : 1.5f), 3.0f);
    g.setColour (highlighted ? juce::Colour (0xff2e2a24) : colours::bakelite);
    g.fillRoundedRectangle (r.translated (0.0f, down ? 1.0f : 0.0f), 3.0f);
    if (b.getToggleState())
    {
        g.setColour (colours::amberLamp);
        g.drawRoundedRectangle (r.reduced (1.0f), 3.0f, 1.2f);
    }
}

void VintageLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    auto r = b.getLocalBounds().translated (0, down ? 1 : 0);

    // optionale Kontrolllampe (Reiter im Modul-Rack: Modul an/aus)
    if (b.getProperties().contains ("lamp"))
    {
        const bool on = b.getProperties()["lamp"];
        auto lamp = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ (float) r.getX() + 11.0f, (float) r.getCentreY() });
        if (on)
        {
            g.setColour (colours::amberLamp.withAlpha (0.35f));
            g.fillEllipse (lamp.expanded (2.5f));
        }
        g.setColour (on ? colours::amberLamp : juce::Colour (0xff4a3a26));
        g.fillEllipse (lamp);
        r.removeFromLeft (16);
    }

    g.setFont (fonts::label (17.0f));
    g.setColour ((b.getToggleState() ? colours::amberLamp : colours::cream).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.35f));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void VintageLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (colours::cream);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (colours::ink.withAlpha (0.5f));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.drawLine (2.0f, 1.5f, (float) w - 2.0f, 1.5f, 1.5f);

    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5.0f, ay - 3.0f, ax + 5.0f, ay - 3.0f, ax, ay + 4.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

juce::Font VintageLookAndFeel::getComboBoxFont (juce::ComboBox&) { return juce::Font (fonts::mono (15.0f, true)); }

void VintageLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void VintageLookAndFeel::drawComboBoxTextWhenNothingSelected (juce::Graphics& g, juce::ComboBox& box, juce::Label& label)
{
    // z. B. Name des geladenen Presets: normal lesbar, nicht ausgegraut
    g.setColour (box.findColour (juce::ComboBox::textColourId));
    g.setFont (getComboBoxFont (box));
    g.drawFittedText (box.getTextWhenNothingSelected(), label.getBounds(), juce::Justification::centredLeft,
                      1, 0.8f);
}

juce::Font VintageLookAndFeel::getPopupMenuFont() { return juce::Font (fonts::mono (15.0f)); }
juce::Font VintageLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return juce::Font (fonts::label (17.0f)); }
juce::Font VintageLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

juce::Label* VintageLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::Font (fonts::mono (13.0f, true)));
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

void VintageLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (colours::cream);
    g.setColour (colours::engrave);
    g.drawRect (0, 0, w, h, 1);
}
} // namespace mtm::ui
