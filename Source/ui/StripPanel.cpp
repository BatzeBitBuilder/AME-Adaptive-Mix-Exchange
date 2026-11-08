// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "StripPanel.h"
#include "LookAndFeel.h"
#include "Parameters.h"
#include "i18n/Lang.h"

namespace mtm::ui
{
using i18n::tr;

StripPanel::StripPanel (juce::AudioProcessorValueTreeState& state, MixHub& h, int s)
    : apvts (state), hub (h), slot (s), eqGraph (state, h, s)
{
    addAndMakeVisible (eqGraph);
    addAndMakeVisible (levelVu);
    addAndMakeVisible (grVu);
    addAndMakeVisible (waterfall);
    sections.reserve (6);   // Zeiger auf Sektionen bleiben gültig

    sections.emplace_back ("INPUT", std::vector<Knob*> { addKnob (pid::inGain, "Gain") });

    sections.emplace_back ("FILTER", std::vector<Knob*> { addKnob (pid::hpfFreq, "HPF"), addKnob (pid::lpfFreq, "LPF") });
    hpfToggle = addToggle (pid::hpfOn, "HPF");
    lpfToggle = addToggle (pid::lpfOn, "LPF");

    sections.emplace_back ("EQ", std::vector<Knob*> {
        addKnob (pid::lsFreq, "Low Hz"),  addKnob (pid::lsGain, "Low dB"),
        addKnob (pid::p1Freq, "Mid1 Hz"), addKnob (pid::p1Gain, "Mid1 dB"), addKnob (pid::p1Q, "Mid1 Q"),
        addKnob (pid::p2Freq, "Mid2 Hz"), addKnob (pid::p2Gain, "Mid2 dB"), addKnob (pid::p2Q, "Mid2 Q"),
        addKnob (pid::hsFreq, "High Hz"), addKnob (pid::hsGain, "High dB") });

    std::vector<Knob*> compKnobs { addKnob (pid::compThresh, "Thresh"), addKnob (pid::compRatio, "Ratio"),
                                   addKnob (pid::compAttack, "Attack"), addKnob (pid::compRelease, "Release"),
                                   addKnob (pid::compMakeup, "Makeup") };
    compToggle = addToggle (pid::compOn, {});
    sections.emplace_back ("", std::move (compKnobs), compToggle);

    sections.emplace_back ("OUTPUT", std::vector<Knob*> {
        addKnob (pid::pan, "Pan"), addKnob (pid::width, "Width"), addKnob (pid::outGain, "Gain") });

    reactionKnob = addKnob (pid::reaction, {});
    sections.emplace_back ("", std::vector<Knob*> { addKnob (pid::amount, "Amount"), reactionKnob });
    adaptive = &sections.back();
    aEqToggle   = addToggle (pid::adaptEq, "EQ");
    aHpfToggle  = addToggle (pid::adaptHpf, "HPF");
    aGainToggle = addToggle (pid::adaptGain, "Gain");
    aBypassToggle = addToggle (pid::adaptBypass, {});
    aFreezeToggle = addToggle (pid::adaptFreeze, "Hold");

    priorityLabel.setFont (fonts::label (15.0f));
    priorityLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (priorityLabel);
    priorityBox.addItemList (getPriorityNames(), 1);
    addAndMakeVisible (priorityBox);
    priorityAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, pid::priority, priorityBox);

    updateTexts();
}

void StripPanel::updateTexts()
{
    sections[3].title = tr ("KOMPRESSOR", "COMPRESSOR");
    sections[5].title = tr ("ADAPTIV", "ADAPTIVE");
    compToggle->button.setButtonText (tr ("An", "On"));
    reactionKnob->label.setText (tr ("Ansprechzeit", "Response"), juce::dontSendNotification);
    aBypassToggle->button.setButtonText (tr ("Direkt", "Direct"));
    priorityLabel.setText (tr ("Vorrang", "Priority"), juce::dontSendNotification);

    // Einträge neu, Auswahl bleibt (Parameter unverändert)
    const int sel = priorityBox.getSelectedItemIndex();
    priorityBox.clear (juce::dontSendNotification);
    priorityBox.addItemList (getPriorityNames(), 1);
    priorityBox.setSelectedItemIndex (sel, juce::dontSendNotification);

    levelVu.setCaption (tr ("Ausgang", "Output"));
    grVu.setCaption (tr ("Kompressor", "Compressor"));
    waterfall.setCaption (tr ("SPEKTROGRAMM", "SPECTROGRAM"));
    plateCounter = 14;   // Typenschild beim nächsten Meter-Update neu
    repaint();
}

StripPanel::~StripPanel() = default;

StripPanel::Knob* StripPanel::addKnob (const char* paramId, const juce::String& label)
{
    auto k = std::make_unique<Knob>();
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k->label.setText (label, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (fonts::label (15.0f));
    k->label.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, k->slider);

    auto* raw = k.get();
    knobs.push_back (std::move (k));
    return raw;
}

StripPanel::Toggle* StripPanel::addToggle (const char* paramId, const juce::String& label)
{
    auto t = std::make_unique<Toggle>();
    t->button.setButtonText (label);
    addAndMakeVisible (t->button);
    t->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramId, t->button);

    auto* raw = t.get();
    toggles.push_back (std::move (t));
    return raw;
}

void StripPanel::layoutKnobs (Section& s, juce::Rectangle<int> r)
{
    s.bounds = r;
    auto inner = r.reduced (6);
    inner.removeFromTop (20);   // Titel
    const int w = s.knobs.empty() ? 0 : inner.getWidth() / (int) s.knobs.size();
    for (auto* k : s.knobs)
    {
        auto cell = inner.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    }
}

void StripPanel::resized()
{
    auto area = getLocalBounds().reduced (8);

    // Rechte Spalte: VU-Meter
    auto meterCol = area.removeFromRight (176);
    area.removeFromRight (8);
    levelVu.setBounds (meterCol.removeFromTop (128));
    meterCol.removeFromTop (8);
    grVu.setBounds (meterCol.removeFromTop (128));
    meterCol.removeFromTop (10);
    plateArea = meterCol.removeFromTop (150);

    constexpr int gap = 6;
    auto row3 = area.removeFromBottom (150);
    area.removeFromBottom (gap);
    auto row1 = area.removeFromTop (juce::jmin (200, (area.getHeight() - gap) / 2));
    area.removeFromTop (gap);
    auto row2 = area;

    auto& input = sections[0];
    auto& filter = sections[1];
    auto& eq = sections[2];
    auto& comp = sections[3];
    auto& out = sections[4];

    // Zeile 1: Anteile 1 / 2.4 / 5 / 3
    const float unit = (float) (row1.getWidth() - 3 * gap) / 11.4f;
    layoutKnobs (input,  row1.removeFromLeft (juce::roundToInt (unit * 1.0f)));  row1.removeFromLeft (gap);
    layoutKnobs (filter, row1.removeFromLeft (juce::roundToInt (unit * 2.4f)));  row1.removeFromLeft (gap);
    layoutKnobs (comp,   row1.removeFromLeft (juce::roundToInt (unit * 5.0f)));  row1.removeFromLeft (gap);
    layoutKnobs (out,    row1);

    // EQ: Analyzer, Spektrogramm, Regler
    {
        eq.bounds = row2;
        auto inner = row2.reduced (6);
        inner.removeFromTop (22);
        auto knobRow = inner.removeFromBottom (108);
        auto wf = inner.removeFromBottom (62);
        inner.removeFromBottom (4);
        eqGraph.setBounds (inner);
        waterfall.setBounds (wf.withTrimmedLeft (24));   // an Analyzer-Achse ausrichten
        Section tmp ("", eq.knobs);
        layoutKnobs (tmp, knobRow.withY (knobRow.getY() - 26).withHeight (knobRow.getHeight() + 26));
    }

    auto fTitle = filter.bounds.reduced (6).removeFromTop (20);
    lpfToggle->button.setBounds (fTitle.removeFromRight (54));
    hpfToggle->button.setBounds (fTitle.removeFromRight (54));

    auto cTitle = comp.bounds.reduced (6).removeFromTop (20);
    comp.toggle->button.setBounds (cTitle.removeFromRight (56));

    // Zeile 3: Adaptiv
    adaptive->bounds = row3;
    auto inner = row3.reduced (6);
    auto title = inner.removeFromTop (22);
    title.removeFromLeft (84);
    aEqToggle->button.setBounds (title.removeFromLeft (52));
    aHpfToggle->button.setBounds (title.removeFromLeft (58));
    aGainToggle->button.setBounds (title.removeFromLeft (64));
    title.removeFromLeft (16);
    aBypassToggle->button.setBounds (title.removeFromLeft (104));
    aFreezeToggle->button.setBounds (title.removeFromLeft (104));

    auto knobArea = inner.removeFromLeft (160);
    for (auto* k : adaptive->knobs)
    {
        auto cell = knobArea.removeFromLeft (80);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    }

    inner.removeFromLeft (10);
    auto prioArea = inner.removeFromLeft (150);
    prioArea.removeFromTop (8);
    priorityLabel.setBounds (prioArea.removeFromTop (18));
    priorityBox.setBounds (prioArea.removeFromTop (26));

    inner.removeFromLeft (16);
    adaptiveDisplay = inner.reduced (0, 2);
}

void StripPanel::paint (juce::Graphics& g)
{
    for (auto& s : sections)
        drawEngravedPanel (g, s.bounds.toFloat(), s.title);

    paintAdaptive (g);

    // Typenschild
    if (! plateArea.isEmpty())
    {
        auto r = plateArea.toFloat();
        g.setColour (colours::bakelite);
        g.fillRoundedRectangle (r, 5.0f);
        auto inner = r.reduced (5.0f);
        juce::ColourGradient brass (juce::Colour (0xffd9b56a), inner.getX(), inner.getY(),
                                    juce::Colour (0xff9c7a3a), inner.getRight(), inner.getBottom(), false);
        g.setGradientFill (brass);
        g.fillRoundedRectangle (inner, 3.0f);
        for (auto p : { inner.getTopLeft().translated (7.0f, 7.0f), inner.getTopRight().translated (-7.0f, 7.0f),
                        inner.getBottomLeft().translated (7.0f, -7.0f), inner.getBottomRight().translated (-7.0f, -7.0f) })
            drawScrew (g, p, 3.5f);

        auto t = inner.reduced (14.0f, 12.0f);
        g.setColour (colours::bakelite);

        // Kopfzeile: Hersteller-Gravur + Line-Nummer
        auto head = t.removeFromTop (14.0f);
        g.setFont (juce::Font (fonts::label (13.0f)).withExtraKerningFactor (0.12f));
        g.drawText ("AME  MODEL " JucePlugin_VersionString, head, juce::Justification::centredLeft);
        g.drawText (plateLine, head, juce::Justification::centredRight);
        t.removeFromTop (3.0f);
        g.setColour (colours::bakelite.withAlpha (0.45f));
        g.fillRect (t.removeFromTop (1.0f));
        t.removeFromTop (3.0f);

        g.setColour (colours::bakelite);
        g.setFont (fonts::display (18.0f));
        g.drawText (plateName, t.removeFromTop (24.0f), juce::Justification::centredLeft, true);
        t.removeFromTop (2.0f);
        g.setFont (fonts::label (13.0f));
        g.drawText (tr ("ROLLE", "ROLE"), t.removeFromTop (13.0f), juce::Justification::centredLeft);
        g.setFont (fonts::mono (13.0f, true));
        g.drawText (plateRole, t.removeFromTop (17.0f), juce::Justification::centredLeft, true);
        t.removeFromTop (2.0f);
        g.setFont (fonts::label (13.0f));
        g.drawText (tr ("PROGRAMM", "PROGRAM"), t.removeFromTop (13.0f), juce::Justification::centredLeft);
        g.setFont (fonts::mono (13.0f, true));
        g.drawText (plateGenre, t.removeFromTop (17.0f), juce::Justification::centredLeft, true);
    }
}

void StripPanel::paintAdaptive (juce::Graphics& g)
{
    auto area = adaptiveDisplay.toFloat();
    if (area.isEmpty())
        return;

    drawDisplayBezel (g, area);
    area.reduce (8.0f, 5.0f);

    // Statuszeile
    auto status = area.removeFromTop (16.0f);
    juce::String st;
    juce::Colour sc = colours::phosphor;
    if (bypassed)          { st = tr ("DIREKT – EINGRIFFE AUS", "DIRECT – PROCESSING OFF"); sc = colours::amberLamp; }
    else if (frozen)       { st = tr ("HOLD – WERTE GEHALTEN", "HOLD – VALUES FROZEN");     sc = juce::Colour (0xff8fd0ff); }
    else if (engineAlive)  { st = tr ("EXCHANGE VERBUNDEN", "EXCHANGE CONNECTED"); }
    else                   { st = tr ("NO EXCHANGE – LINE LÄUFT SOLO", "NO EXCHANGE – LINE RUNS SOLO"); sc = colours::amberLamp; }
    g.setFont (fonts::mono (12.0f, true));
    g.setColour (sc);
    g.drawText (st, status, juce::Justification::centredLeft);

    juce::String info;
    info << "HPF " << (hpfHz >= 20.0f ? juce::String (juce::roundToInt (hpfHz)) + " Hz" : tr ("AUS", "OFF"))
         << "   GAIN " << (gainDb >= 0.0f ? "+" : "") << juce::String (gainDb, 1) << " dB";
    g.setColour (colours::phosphor);
    g.drawText (info, status, juce::Justification::centredRight);

    area.removeFromTop (4.0f);
    auto labels = area.removeFromBottom (13.0f);
    auto plot = area;

    constexpr float rangeDb = 9.0f;
    static const char* names[kNumAdaptiveBands] { "31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };
    const float w = plot.getWidth() / (float) kNumAdaptiveBands;

    g.setColour (colours::displayGrid);
    for (int i = 1; i < 3; ++i)
        g.drawHorizontalLine (juce::roundToInt (plot.getY() + plot.getHeight() * (float) i / 3.0f), plot.getX(), plot.getRight());

    for (int k = 0; k < kNumAdaptiveBands; ++k)
    {
        const float cut = juce::jlimit (-rangeDb, 0.0f, cutDb[(size_t) k]);
        auto cell = juce::Rectangle<float> (plot.getX() + w * (float) k, plot.getY(), w, plot.getHeight()).reduced (4.0f, 0.0f);
        const float frac = -cut / rangeDb;

        // segmentierte LED-Säule von oben (Absenkung)
        const int segs = 9;
        const float segH = cell.getHeight() / (float) segs;
        for (int sgm = 0; sgm < segs; ++sgm)
        {
            auto sr = cell.withY (cell.getY() + segH * (float) sgm).withHeight (segH).reduced (0.0f, 1.0f);
            const bool on = (float) sgm < frac * (float) segs - 0.01f;
            g.setColour (on ? maskingColour ((float) sgm / (float) (segs - 1)) : colours::displayGrid.withAlpha (0.6f));
            g.fillRect (sr);
        }

        g.setColour (colours::phosphorDim);
        g.setFont (fonts::mono (10.0f, true));
        g.drawText (names[k], juce::Rectangle<float> (cell.getX() - 4.0f, labels.getY(), cell.getWidth() + 8.0f, 13.0f),
                    juce::Justification::centred);
    }
}

void StripPanel::refreshMeters()
{
    constexpr float dt = 1.0f / 30.0f;
    if (auto* d = hub.getAudioData (slot))
    {
        levelVu.update (d->rmsDb.load(), d->peakDb.load(), dt);
        grVu.updateGainReduction (d->gainReductionDb.load(), dt);
        for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
            cutDb[k] = d->appliedCutDb[k].load();
        hpfHz = d->appliedHpfHz.load();
        gainDb = d->appliedGainDb.load();
        engineAlive = hub.isEngineAlive();
        frozen = d->frozen.load();
        bypassed = d->bypassed.load();
        eqGraph.refresh (dt);

        // Typenschild (2 Hz genügt)
        if (++plateCounter % 15 == 0)
        {
            for (const auto& t : hub.getSnapshot())
                if (t.slot == slot)
                {
                    plateName = t.name;
                    plateRole = getRoleNames()[(int) t.role] + (t.autoRole ? tr (" (Kennung)", " (auto)") : juce::String());
                    plateLine = "LINE " + juce::String (t.instanceId).paddedLeft ('0', 2);
                }
            plateGenre = hub.isEngineAlive() ? getGenreNames()[hub.globalGenre.load()] : MTM_TXT ("– no exchange –");
            repaint (plateArea);
        }
        waterfall.pushRow (eqGraph.getSpectrum());
        repaint (adaptiveDisplay.expanded (6));
    }
}
} // namespace mtm::ui
