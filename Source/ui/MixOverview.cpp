// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "MixOverview.h"
#include "LookAndFeel.h"
#include "dsp/SpectrumAnalyzer.h"
#include "dsp/ChannelStrip.h"
#include "i18n/Lang.h"

namespace mtm::ui
{
using i18n::tr;

namespace
{
juce::String formatHz (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 0 : 1) + "k"
                         : juce::String (juce::roundToInt (hz));
}

const juce::Colour palette[] {
    juce::Colour (0xff4fc3f7), juce::Colour (0xffff8a50), juce::Colour (0xff9ccc65), juce::Colour (0xffffd54f),
    juce::Colour (0xffba68c8), juce::Colour (0xfff06292), juce::Colour (0xff4db6ac), juce::Colour (0xffe57373)
};

constexpr float specMinDb = -90.0f, specMaxDb = 0.0f;
} // namespace

MixOverview::MixOverview (MixHub& h) : hub (h)
{
    addAndMakeVisible (vuLeft);
    addAndMakeVisible (vuRight);
    addAndMakeVisible (waterfall);
    addAndMakeVisible (matrix);
    waterfall.setRange (-90.0f, -20.0f);
    peakDb.fill (-100.0f);
    matrix.onSelect = [this] (uint32_t id) { selectedId = id; repaint(); };
    updateTexts();
}

void MixOverview::updateTexts()
{
    vuLeft.setCaption (tr ("Links", "Left"));
    vuRight.setCaption (tr ("Rechts", "Right"));
    repaint();
}

juce::Colour MixOverview::colourFor (const TrackSnapshot& t) const
{
    if (! t.colour.isTransparent())
        return t.colour;
    return palette[(t.instanceId - 1) % (uint32_t) std::size (palette)];
}

int MixOverview::selectedSnapshotIndex() const
{
    for (int i = 0; i < (int) tracks.size(); ++i)
        if (tracks[(size_t) i].instanceId == selectedId)
            return i;
    return -1;
}

void MixOverview::refresh (uint32_t ownInstanceId, float dt)
{
    if (ownId != ownInstanceId)
    {
        ownId = ownInstanceId;
        selectedId = ownInstanceId;
    }

    const auto oldCount = tracks.size();
    tracks = hub.getSnapshot();
    maskingPre = computeMasking (tracks);

    // Spektren nach den adaptiven Eingriffen (analytischer Frequenzgang)
    responseDb.assign (tracks.size(), {});
    responseFine.assign (tracks.size(), {});
    auto post = tracks;
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        const auto& t = tracks[i];
        for (int b = 0; b < kNumAnalysisBands; ++b)
        {
            const float r = ChannelStrip::getAdaptiveResponseDb (t.appliedCutDb, t.appliedHpfHz, t.appliedGainDb,
                                                                 SpectrumAnalyzer::getBandCentreHz (b));
            responseDb[i][(size_t) b] = r;
            post[i].bandAvgDb[(size_t) b] += r;
        }
        for (int d = 0; d < kNumDisplayPoints; ++d)
            responseFine[i][(size_t) d] = ChannelStrip::getAdaptiveResponseDb (t.appliedCutDb, t.appliedHpfHz, t.appliedGainDb,
                                                                               getDisplayPointHz (d));
    }
    masking = computeMasking (post);

    if (tracks.size() != oldCount)
        resized();

    // VU L/R der eigenen Instanz (im Master = Summe)
    for (const auto& t : tracks)
        if (t.instanceId == ownId)
        {
            vuLeft.update (t.rmsLeftDb, t.peakDb, dt);
            vuRight.update (t.rmsRightDb, t.peakDb, dt);
        }

    // Auswahl: Peak-Hold + Spektrogramm (nach Engine)
    const int sel = selectedSnapshotIndex();
    if (sel >= 0)
    {
        if (peakForId != selectedId) { peakDb.fill (-100.0f); peakForId = selectedId; waterfall.clear(); }
        std::array<float, kNumDisplayPoints> row {};
        for (size_t d = 0; d < (size_t) kNumDisplayPoints; ++d)
        {
            row[d] = tracks[(size_t) sel].displayDb[d] + responseFine[(size_t) sel][d];
            peakDb[d] = juce::jmax (row[d], peakDb[d] - 12.0f * dt);
        }
        waterfall.pushRow (row);
        waterfall.setCaption (tr ("SPEKTROGRAMM · ", "SPECTROGRAM · ") + tracks[(size_t) sel].name.toUpperCase());
    }

    std::vector<juce::Colour> cols;
    for (const auto& t : tracks) cols.push_back (colourFor (t));
    matrix.setData (tracks, masking, cols, selectedId);

    repaint();
}

void MixOverview::resized()
{
    auto area = getLocalBounds().reduced (8);

    // oben: Liste (volle Breite)
    const int wantedH = headH + juce::jmax (4, (int) tracks.size()) * rowH + 16;
    listArea = area.removeFromTop (juce::jlimit (128, 230, wantedH));
    area.removeFromTop (8);

    // unten: Spektrogramm | VU L/R | Matrix
    auto bottom = area.removeFromBottom (juce::jmin (220, area.getHeight() / 2));
    area.removeFromBottom (8);
    matrix.setBounds (bottom.removeFromRight (290));
    bottom.removeFromRight (8);
    auto vuCol = bottom.removeFromRight (150);
    bottom.removeFromRight (8);
    vuLeft.setBounds (vuCol.removeFromTop (vuCol.getHeight() / 2).withTrimmedBottom (3));
    vuRight.setBounds (vuCol.withTrimmedTop (3));
    waterfall.setBounds (bottom);

    spectrumArea = area;
}

void MixOverview::mouseDown (const juce::MouseEvent& e)
{
    if (! listArea.contains (e.getPosition()))
        return;

    const int row = (e.y - listArea.getY() - 6 - headH) / rowH;
    if (juce::isPositiveAndBelow (row, (int) tracks.size()))
    {
        const auto id = tracks[(size_t) row].instanceId;
        selectedId = (selectedId == id ? 0 : id);
        repaint();
    }
}

void MixOverview::paint (juce::Graphics& g)
{
    paintList (g, listArea);
    paintSpectrum (g, spectrumArea);
}

void MixOverview::paintList (juce::Graphics& g, juce::Rectangle<int> area)
{
    drawEngravedPanel (g, area.toFloat(), {});
    area.reduce (8, 6);

    const auto roleNames = getRoleNames();
    const uint32_t masterId = hub.getActiveMasterId();

    constexpr int cName = 140, cRole = 130, cGr = 44, cMask = 120, cInfo = 190, cAdapt = 190;

    auto columns = [&] (juce::Rectangle<int> r)
    {
        struct Cols { juce::Rectangle<int> name, role, level, gr, mask, info, adapt; } c;
        r.removeFromLeft (12);
        c.name = r.removeFromLeft (cName);
        c.role = r.removeFromLeft (cRole);
        c.adapt = r.removeFromRight (cAdapt);
        c.info = r.removeFromRight (cInfo);
        c.mask = r.removeFromRight (cMask);
        c.gr   = r.removeFromRight (cGr);
        r.removeFromRight (8);
        c.level = r;
        return c;
    };

    g.setFont (fonts::label (16.0f));
    g.setColour (colours::ink);
    {
        auto c = columns (area.removeFromTop (headH));
        g.drawText ("LINE", c.name, juce::Justification::centredLeft);
        g.drawText (tr ("ROLLE", "ROLE"), c.role, juce::Justification::centredLeft);
        g.drawText (tr ("PEGEL", "LEVEL"), c.level, juce::Justification::centredLeft);
        g.drawText ("GR", c.gr, juce::Justification::centredRight);
        g.drawText (tr ("  MASKIERT", "  MASKED"), c.mask, juce::Justification::centredLeft);
        g.drawText (tr ("ÜBERDECKT VON", "MASKED BY"), c.info, juce::Justification::centredLeft);
        g.drawText (tr ("ADAPTIV", "ADAPTIVE"), c.adapt, juce::Justification::centredLeft);
    }
    g.setColour (colours::engrave);
    g.drawHorizontalLine (area.getY(), (float) area.getX(), (float) area.getRight());

    if (tracks.empty())
    {
        g.setFont (fonts::mono (13.0f));
        g.drawText (tr ("Keine Lines verbunden.", "No lines connected."), area, juce::Justification::centred);
        return;
    }

    const int maxRows = area.getHeight() / rowH;

    for (int idx = 0; idx < (int) tracks.size(); ++idx)
    {
        const auto& t = tracks[(size_t) idx];

        if (idx >= maxRows)
        {
            g.setColour (colours::inkDim);
            g.setFont (fonts::mono (12.0f));
            g.drawText ("+ " + juce::String ((int) tracks.size() - idx) + tr (" weitere", " more"),
                        area.removeFromTop (rowH), juce::Justification::centredLeft);
            break;
        }

        auto rowArea = area.removeFromTop (rowH).reduced (0, 2);

        if (t.instanceId == selectedId)
        {
            g.setColour (colours::amber.withAlpha (0.35f));
            g.fillRoundedRectangle (rowArea.toFloat(), 3.0f);
        }

        g.setColour (colourFor (t));
        g.fillRoundedRectangle (rowArea.withWidth (6).toFloat(), 2.0f);
        auto c = columns (rowArea);

        g.setFont (fonts::mono (13.0f, true));
        g.setColour (colours::ink);
        juce::String name = t.name;
        if (t.mode == InstanceMode::Master)
            name += t.instanceId == masterId ? juce::String (" [EX]") : MTM_TXT (" [EX–]");
        g.drawText (name, c.name, juce::Justification::centredLeft, true);

        g.setFont (fonts::mono (12.0f));
        g.setColour (colours::inkDim);
        juce::String roleText = t.mode == InstanceMode::Master ? MTM_TXT ("–") : roleNames[(int) t.role];
        if (t.mode == InstanceMode::Track && t.autoRole)
            roleText = t.role == Role::Auto ? MTM_TXT ("Auto …") : roleText + " (A)";
        g.drawText (roleText, c.role, juce::Justification::centredLeft, true);

        g.drawText (t.grDb > 0.05f ? juce::String (-t.grDb, 1) : juce::String ("0.0"),
                    c.gr, juce::Justification::centredRight);

        // Pegel: LED-Kette
        {
            auto bar = c.level.reduced (0, 4).toFloat();
            g.setColour (colours::bakelite);
            g.fillRoundedRectangle (bar, 2.0f);
            bar.reduce (2.0f, 2.0f);
            const int leds = juce::jmax (8, (int) (bar.getWidth() / 6.0f));
            const float lw = bar.getWidth() / (float) leds;
            const float level = juce::jmap (juce::jlimit (-60.0f, 6.0f, t.rmsDb), -60.0f, 6.0f, 0.0f, 1.0f);
            const float peak = juce::jmap (juce::jlimit (-60.0f, 6.0f, t.peakDb), -60.0f, 6.0f, 0.0f, 1.0f);
            for (int l = 0; l < leds; ++l)
            {
                const float pos = (float) l / (float) leds;
                const bool on = pos < level || std::abs (pos - peak) < 0.5f / (float) leds;
                const auto col = pos > 0.9f ? juce::Colour (0xffe03a2a) : (pos > 0.75f ? colours::amberLamp : colours::phosphor);
                g.setColour (on ? col : col.withAlpha (0.12f));
                g.fillRect (bar.getX() + lw * (float) l + 0.5f, bar.getY(), lw - 1.5f, bar.getHeight());
            }
        }

        // Maskierung
        const int mt = masking.findTrack (idx);
        if (mt >= 0)
        {
            const float score = masking.maskedScore[(size_t) mt];
            auto mArea = c.mask.reduced (8, 0);
            auto mBar = mArea.removeFromLeft (36).reduced (0, 5).toFloat();
            g.setColour (colours::bakelite);
            g.fillRect (mBar);
            g.setColour (maskingColour (score * 1.5f));
            g.fillRect (mBar.reduced (1.0f).withWidth ((mBar.getWidth() - 2.0f) * juce::jlimit (0.0f, 1.0f, score)));
            g.setColour (colours::inkDim);
            g.setFont (fonts::mono (11.0f, true));
            const int pre = maskingPre.findTrack (idx);
            const int preScore = pre >= 0 ? juce::roundToInt (maskingPre.maskedScore[(size_t) pre] * 100.0f) : 0;
            const int postScore = juce::roundToInt (score * 100.0f);
            g.drawText (preScore != postScore ? juce::String (preScore) + ">" + juce::String (postScore) + "%"
                                              : juce::String (postScore) + "%",
                        mArea.withTrimmedLeft (4), juce::Justification::centredLeft);

            const int w = masking.worstMasker[(size_t) mt];
            if (w >= 0 && score > 0.05f)
            {
                const auto& masker = tracks[(size_t) masking.trackIndex[(size_t) w]];
                const float lo = SpectrumAnalyzer::getBandEdgeHz (masking.worstBandLo[(size_t) mt]);
                const float hi = SpectrumAnalyzer::getBandEdgeHz (masking.worstBandHi[(size_t) mt] + 1);
                g.setFont (fonts::mono (12.0f));
                g.drawText (masker.name + " " + formatHz (lo) + "-" + formatHz (hi),
                            c.info, juce::Justification::centredLeft, true);
            }
        }
        else if (t.mode == InstanceMode::Track)
        {
            g.setColour (colours::inkDim);
            g.drawText (tr ("kein Signal", "no signal"), c.mask.withTrimmedLeft (8), juce::Justification::centredLeft);
        }

        if (t.mode == InstanceMode::Track)
        {
            float maxCut = 0.0f;
            for (auto v : t.appliedCutDb) maxCut = juce::jmin (maxCut, v);
            juce::String a;
            if (t.bypassed)    a << "[DIR] ";
            else if (t.frozen) a << "[HOLD] ";
            a << (t.appliedGainDb >= 0.0f ? "+" : "") << juce::String (t.appliedGainDb, 1) << "dB";
            if (t.appliedHpfHz >= 20.0f) a << " HP" << juce::roundToInt (t.appliedHpfHz);
            if (maxCut < -0.1f)          a << " EQ" << juce::String (maxCut, 1);
            g.setColour (colours::inkDim);
            g.drawText (a, c.adapt, juce::Justification::centredLeft, true);
        }
    }
}

void MixOverview::paintSpectrum (juce::Graphics& g, juce::Rectangle<int> area)
{
    drawEngravedPanel (g, area.toFloat(), {});

    const int selIdx = selectedSnapshotIndex();
    const int selTrack = selIdx >= 0 ? masking.findTrack (selIdx) : -1;

    // Titel
    auto titleArea = area.reduced (10, 5).removeFromTop (18).toFloat();
    g.setFont (fonts::label (17.0f));
    g.setColour (colours::ink);
    g.drawText ("ANALYZER", titleArea, juce::Justification::centredLeft);
    g.setFont (fonts::mono (11.0f, true));
    g.setColour (colours::inkDim);
    g.drawText (selTrack >= 0 ? tr ("Wärmesäulen = Überdeckung von \"", "Heat columns = masking of \"") + tracks[(size_t) selIdx].name
                                  + tr ("\" · gepunktet = Peak", "\" · dotted = peak")
                              : tr ("Wärmesäulen = Gesamtbelegung · Zeile anklicken für Einzelspur",
                                    "Heat columns = total occupancy · click a row for a single track"),
                titleArea.withTrimmedLeft (90.0f), juce::Justification::centredLeft);

    auto plot = area.reduced (10, 6).withTrimmedTop (22).withTrimmedBottom (14).withTrimmedLeft (24).toFloat();
    drawDisplayBezel (g, plot);

    const float fMin = 20.0f, fMax = 20000.0f;
    auto xOf = [&] (float hz) { return plot.getX() + plot.getWidth() * std::log (juce::jlimit (fMin, fMax, hz) / fMin) / std::log (fMax / fMin); };
    auto yOf = [&] (float db) { return juce::jmap (juce::jlimit (specMinDb, specMaxDb, db), specMinDb, specMaxDb, plot.getBottom(), plot.getY()); };

    // Masking-Wärmesäulen
    for (int b = 0; b < kNumAnalysisBands; ++b)
    {
        float h = 0.0f;
        if (selTrack >= 0)
        {
            for (int i = 0; i < masking.numTracks(); ++i)
                if (i != selTrack)
                    h = juce::jmax (h, masking.get (i, selTrack, b));
        }
        else if (masking.numTracks() >= 2)
        {
            h = masking.hotspot[(size_t) b];
        }

        if (h > 0.03f)
        {
            const float x0 = xOf (SpectrumAnalyzer::getBandEdgeHz (b)), x1 = xOf (SpectrumAnalyzer::getBandEdgeHz (b + 1));
            const auto col = maskingColour (h);
            juce::ColourGradient grad (col.withAlpha (0.05f), 0.0f, plot.getY(), col.withAlpha (0.15f + 0.45f * h), 0.0f, plot.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (x0 + 1.0f, plot.getY(), x1 - x0 - 2.0f, plot.getHeight()));
            g.setColour (col);
            g.fillRect (juce::Rectangle<float> (x0 + 1.0f, plot.getBottom() - 4.0f * (1.0f + 3.0f * h), x1 - x0 - 2.0f, 4.0f * (1.0f + 3.0f * h)));
        }
    }

    // Raster
    g.setFont (fonts::mono (10.0f, true));
    for (float hz : { 30.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xOf (hz);
        g.setColour (colours::displayGrid);
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        g.setColour (colours::inkDim);
        g.drawText (formatHz (hz), juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 3.0f, 40.0f, 11.0f), juce::Justification::centred);
    }
    for (int db = (int) specMinDb + 10; db <= (int) specMaxDb; db += 20)
    {
        const float y = yOf ((float) db);
        g.setColour (colours::displayGrid);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        g.setColour (colours::inkDim);
        g.drawText (juce::String (db), juce::Rectangle<float> (plot.getX() - 27.0f, y - 6.0f, 22.0f, 12.0f), juce::Justification::centredRight);
    }

    // Kurven nach Engine, je Spur in ihrer Farbe (Fläche + Leuchtlinie)
    auto curve = [&] (int idx, const float* values, bool closed)
    {
        juce::Path p;
        if (closed) p.startNewSubPath (xOf (getDisplayPointHz (0)), plot.getBottom());
        for (int d = 0; d < kNumDisplayPoints; ++d)
        {
            const float x = xOf (getDisplayPointHz (d));
            const float y = yOf (values[d] + responseFine[(size_t) idx][(size_t) d]);
            if (d == 0 && ! closed) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (closed) { p.lineTo (xOf (getDisplayPointHz (kNumDisplayPoints - 1)), plot.getBottom()); p.closeSubPath(); }
        return p;
    };

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    for (int i = 0; i < (int) tracks.size(); ++i)
    {
        const auto& t = tracks[(size_t) i];
        if (t.mode != InstanceMode::Track || ! t.hasAnalysis || i == selIdx) continue;
        const auto c = colourFor (t);
        const bool dim = selTrack >= 0;
        g.setColour (c.withAlpha (dim ? 0.06f : 0.16f));
        g.fillPath (curve (i, t.displayDb.data(), true));
        if (dim)
        {
            g.setColour (c.withAlpha (0.45f));
            g.strokePath (curve (i, t.displayDb.data(), false), juce::PathStrokeType (1.0f));
        }
        else
        {
            drawPhosphorGlow (g, curve (i, t.displayDb.data(), false), c, 1.2f);
        }
    }

    // Summe (aktiver Master) als cremefarbene Linie
    for (int i = 0; i < (int) tracks.size(); ++i)
    {
        const auto& t = tracks[(size_t) i];
        if (t.mode != InstanceMode::Master || t.instanceId != hub.getActiveMasterId() || ! t.hasAnalysis) continue;
        juce::Path sum;
        for (int d = 0; d < kNumDisplayPoints; ++d)
        {
            const float x = xOf (getDisplayPointHz (d)), y = yOf (t.displayDb[(size_t) d]);
            if (d == 0) sum.startNewSubPath (x, y); else sum.lineTo (x, y);
        }
        drawPhosphorGlow (g, sum, colours::cream.withAlpha (0.8f), i == selIdx ? 1.8f : 1.0f);
    }

    if (selIdx >= 0 && tracks[(size_t) selIdx].mode == InstanceMode::Track && tracks[(size_t) selIdx].hasAnalysis)
    {
        const auto& t = tracks[(size_t) selIdx];
        const auto c = colourFor (t);
        g.setColour (c.withAlpha (0.25f));
        g.fillPath (curve (selIdx, t.displayDb.data(), true));
        drawPhosphorGlow (g, curve (selIdx, t.displayDb.data(), false), c, 2.0f);

        // Peak-Hold (enthält bereits den Frequenzgang)
        juce::Path pk;
        for (int d = 0; d < kNumDisplayPoints; ++d)
        {
            const float x = xOf (getDisplayPointHz (d)), y = yOf (peakDb[(size_t) d]);
            if (d == 0) pk.startNewSubPath (x, y); else pk.lineTo (x, y);
        }
        juce::Path dotted;
        const float dots[] { 1.5f, 3.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dotted, pk, dots, 2);
        g.setColour (colours::amberLamp);
        g.fillPath (dotted);
    }

    g.restoreState();

    // Legende: Spurfarben + Summe
    g.setFont (fonts::mono (10.0f, true));
    auto legend = plot.reduced (8.0f, 5.0f).removeFromTop (12.0f);
    if (hub.getActiveMasterId() != 0)
    {
        auto r = legend.removeFromRight (60.0f);
        g.setColour (colours::cream);
        g.fillRect (r.removeFromLeft (8.0f).reduced (0.0f, 5.0f));
        g.drawText (tr ("SUMME", "SUM"), r.withTrimmedLeft (3.0f), juce::Justification::centredLeft);
    }
    for (int i = (int) tracks.size() - 1; i >= 0; --i)
    {
        const auto& t = tracks[(size_t) i];
        if (t.mode != InstanceMode::Track) continue;
        const auto name = t.name.toUpperCase().substring (0, 12);
        const float w = (float) name.length() * 6.5f + 16.0f;
        auto r = legend.removeFromRight (w);
        g.setColour (colourFor (t));
        g.fillRect (r.removeFromLeft (8.0f).reduced (0.0f, 3.0f));
        g.drawText (name, r.withTrimmedLeft (3.0f), juce::Justification::centredLeft);
        if (legend.getWidth() < 80.0f) break;
    }
}
} // namespace mtm::ui
