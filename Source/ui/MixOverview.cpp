// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "MixOverview.h"
#include "LookAndFeel.h"
#include "dsp/SpectrumAnalyzer.h"
#include "dsp/ChannelStrip.h"
#include "i18n/Lang.h"

namespace ame::ui
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
    listScroll.addListener (this);
    listScroll.setAutoHide (false);
    listScroll.setSingleStepSize (1.0);
    listScroll.setColour (juce::ScrollBar::thumbColourId, colours::engrave);
    addChildComponent (listScroll);
    matrix.onSelect = [this] (uint32_t id) { selectedId = id; slowDirty = true; idleTicks = 0; repaint(); };
    for (auto* f : { &listFold, &detailsFold })
    {
        f->onFoldChanged = [this] { resized(); repaint(); };
        addAndMakeVisible (*f);
    }
    updateTexts();
}

void MixOverview::updateTexts()
{
    vuLeft.setCaption (tr ("Links", "Left"));
    vuRight.setCaption (tr ("Rechts", "Right"));
    listFold.setTooltip (tr ("Line-Liste ein-/ausklappen", "Show/hide the line list"));
    detailsFold.setLabel ("DETAILS");
    detailsFold.setTooltip (tr ("Rechte Spalte ein-/ausklappen (Analyzer wird breiter)", "Show/hide the right column (the analyzer gets wider)"));
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

    ++tick;

    // Läuft überhaupt Audio? Sonst stehen alle Werte – dann genügt ein langsamer Takt.
    if (idleTicks > 45)
    {
        uint32_t live = 0;
        for (const auto& t : tracks)
            if (auto* d = hub.getAudioData (t.slot))
                live += d->blockCounter.load (std::memory_order_relaxed);
        if (live == blockSum && tick % 15 != 0)
            return;
    }

    auto old = std::move (tracks);
    tracks = hub.getSnapshot();

    uint32_t sum = 0;
    for (const auto& t : tracks)
        sum += t.blockCounter;
    idleTicks = sum != blockSum ? 0 : idleTicks + 1;
    blockSum = sum;

    // Frequenzgang der adaptiven Stufe je Line: nur neu, wenn sich die angewendeten Werte geändert haben
    bool sameLines = old.size() == tracks.size() && responseFine.size() == tracks.size();
    for (size_t i = 0; sameLines && i < tracks.size(); ++i)
        sameLines = old[i].instanceId == tracks[i].instanceId;
    if (! sameLines)
    {
        responseDb.assign (tracks.size(), {});
        responseFine.assign (tracks.size(), {});
        slowDirty = true;
    }

    static const auto bandCentres = []
    {
        std::array<float, kNumAnalysisBands> c {};
        for (int b = 0; b < kNumAnalysisBands; ++b)
            c[(size_t) b] = SpectrumAnalyzer::getBandCentreHz (b);
        return c;
    }();
    const auto& pointHz = getDisplayPointTable();

    for (size_t i = 0; i < tracks.size(); ++i)
    {
        const auto& t = tracks[i];
        if (sameLines && old[i].appliedCutDb == t.appliedCutDb && old[i].appliedHpfHz == t.appliedHpfHz
             && old[i].appliedGainDb == t.appliedGainDb)
            continue;

        const ChannelStrip::AdaptiveResponse response (t.appliedCutDb, t.appliedHpfHz, t.appliedGainDb);
        for (size_t b = 0; b < (size_t) kNumAnalysisBands; ++b)
            responseDb[i][b] = response.dbAt (bandCentres[b]);
        for (size_t d = 0; d < (size_t) kNumDisplayPoints; ++d)
            responseFine[i][d] = response.dbAt (pointHz[d]);
    }

    // Maskierung (vor / nach den adaptiven Eingriffen) arbeitet mit dem Langzeit-Spektrum (~3 s): 10 Hz genügen
    const bool slow = slowDirty || tick % 3 == 0;
    if (slow)
    {
        slowDirty = false;
        maskingPre = computeMasking (tracks);
        auto post = tracks;
        for (size_t i = 0; i < tracks.size(); ++i)
            for (size_t b = 0; b < (size_t) kNumAnalysisBands; ++b)
                post[i].bandAvgDb[b] += responseDb[i][b];
        masking = computeMasking (post);
    }

    if (tracks.size() != old.size())
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

    if (slow)
    {
        updateListScroll();
        std::vector<juce::Colour> cols;
        for (const auto& t : tracks) cols.push_back (colourFor (t));
        matrix.setData (tracks, masking, cols, selectedId);
        repaint();   // Liste + Analyzer
    }
    else
    {
        repaint (spectrumArea);
    }
}

void MixOverview::resized()
{
    auto area = getLocalBounds().reduced (8);

    // rechts: Patchfeld, VU L/R, Spektrogramm (eingeklappt: schmale Leiste)
    const bool detailsFolded = detailsFold.isFolded();
    detailsArea = area.removeFromRight (detailsFolded ? 30 : 330);
    area.removeFromRight (8);
    for (juce::Component* c : { (juce::Component*) &matrix, (juce::Component*) &vuLeft, (juce::Component*) &vuRight, (juce::Component*) &waterfall })
        c->setVisible (! detailsFolded);
    if (detailsFolded)
    {
        detailsFold.setBounds (detailsArea.reduced (4, 6).removeFromTop (22));
    }
    else
    {
        auto d = detailsArea;
        detailsFold.setBounds (d.removeFromTop (22).withTrimmedLeft (4).withWidth (detailsFold.getIdealWidth()));
        d.removeFromTop (4);
        matrix.setBounds (d.removeFromTop (juce::jmin (282, d.getHeight() / 2)));
        d.removeFromTop (8);
        auto vuRow = d.removeFromTop (116);
        vuLeft.setBounds (vuRow.removeFromLeft (vuRow.getWidth() / 2).withTrimmedRight (4));
        vuRight.setBounds (vuRow.withTrimmedLeft (4));
        d.removeFromTop (8);
        waterfall.setBounds (d);
    }

    // links oben: Liste (eingeklappt: nur Kopfzeile)
    const int wantedH = headH + juce::jmax (4, (int) tracks.size()) * rowH + 16;
    listArea = area.removeFromTop (listFold.isFolded() ? headH + 12 : juce::jlimit (128, 230, wantedH));
    listFold.setBounds (listArea.getRight() - 32, listArea.getY() + 6, 22, headH);
    area.removeFromTop (8);

    spectrumArea = area;

    const auto rows = rowsArea();
    listScroll.setBounds (rows.getRight() - 11, rows.getY() + 3, 10, juce::jmax (0, rows.getHeight() - 4));
    updateListScroll();
}

//==============================================================================
// Liste: Geometrie
MixOverview::RowCells MixOverview::cellsFor (juce::Rectangle<int> r) const
{
    RowCells c;
    r.removeFromLeft (12);
    c.name = r.removeFromLeft (132);
    c.role = r.removeFromLeft (118);
    r.removeFromRight (26);            // Rollbalken / Klapp-Dreieck
    c.prio = r.removeFromRight (46);
    r.removeFromRight (4);
    c.hold = r.removeFromRight (22);
    r.removeFromRight (3);
    c.direct = r.removeFromRight (22);
    r.removeFromRight (8);
    c.adapt = r.removeFromRight (150);
    c.info  = r.removeFromRight (160);
    c.mask  = r.removeFromRight (112);
    c.gr    = r.removeFromRight (40);
    r.removeFromRight (8);
    c.level = r;
    return c;
}

juce::Rectangle<int> MixOverview::rowsArea() const
{
    auto a = listArea.reduced (8, 6);
    a.removeFromTop (headH);
    return a;
}

int MixOverview::visibleRows() const
{
    return juce::jmax (1, rowsArea().getHeight() / rowH);
}

int MixOverview::firstRow() const
{
    return juce::jlimit (0, juce::jmax (0, (int) tracks.size() - visibleRows()),
                         juce::roundToInt (listScroll.getCurrentRangeStart()));
}

juce::Rectangle<int> MixOverview::rowBounds (int row) const
{
    const auto a = rowsArea();
    return { a.getX(), a.getY() + (row - firstRow()) * rowH, a.getWidth(), rowH };
}

int MixOverview::rowAt (juce::Point<int> p) const
{
    const auto a = rowsArea();
    if (listFold.isFolded() || ! a.contains (p))
        return -1;
    const int line = (p.y - a.getY()) / rowH;
    const int row = firstRow() + line;
    return line < visibleRows() && juce::isPositiveAndBelow (row, (int) tracks.size()) ? row : -1;
}

void MixOverview::updateListScroll()
{
    const int rows = visibleRows(), n = (int) tracks.size();
    const int first = firstRow();
    listScroll.setRangeLimits (0.0, (double) juce::jmax (n, rows), juce::dontSendNotification);
    listScroll.setCurrentRange ((double) first, (double) rows, juce::dontSendNotification);
    listScroll.setVisible (! listFold.isFolded() && n > rows);
}

void MixOverview::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! listArea.contains (e.getPosition()) || (int) tracks.size() <= visibleRows())
        return;
    // eine Rasterung des Mausrads = etwa zwei Zeilen
    const double step = w.deltaY > 0.0f ? -juce::jmax (1.0, (double) w.deltaY * 8.0) : juce::jmax (1.0, (double) -w.deltaY * 8.0);
    if (w.deltaY != 0.0f)
        listScroll.setCurrentRangeStart (juce::jlimit (0.0, (double) ((int) tracks.size() - visibleRows()),
                                                       (double) firstRow() + std::round (step)));
}

void MixOverview::showPriorityMenu (int row)
{
    const auto& t = tracks[(size_t) row];
    juce::PopupMenu m;
    m.addSectionHeader (tr ("Vorrang", "Priority") + AME_TXT (" – ") + t.name);
    const auto names = getPriorityNames();
    for (int i = 0; i < names.size(); ++i)
        m.addItem (i + 1, names[i], true, i == t.priorityIndex);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                               .withTargetScreenArea (localAreaToGlobal (cellsFor (rowBounds (row)).prio)),
                     [safe = juce::Component::SafePointer<MixOverview> (this), slot = t.slot] (int r)
                     {
                         if (safe != nullptr && r > 0)
                         {
                             safe->hub.sendRemote (slot, MixHub::remotePriority, (float) (r - 1));
                             safe->slowDirty = true;
                         }
                     });
}

juce::String MixOverview::getTooltip()
{
    const auto p = getMouseXYRelative();
    const int row = rowAt (p);
    if (row < 0)
        return {};

    const auto& t = tracks[(size_t) row];
    const auto c = cellsFor (rowBounds (row));
    if ((t.clipIn || t.clipOut) && c.level.contains (p))
        return (t.clipIn && t.clipOut ? tr ("Übersteuerung an Eingang und Ausgang", "Overload at input and output")
                : t.clipIn            ? tr ("Übersteuerung am Eingang", "Input overload")
                                      : tr ("Übersteuerung am Ausgang", "Output overload"))
               + tr (" (0 dBFS erreicht) – Klick setzt zurück", " (0 dBFS reached) – click to reset");
    if (t.mode != InstanceMode::Track)
        return {};
    if (c.direct.contains (p))
        return tr ("Direkt: adaptive Eingriffe dieser Line aus/ein", "Direct: adaptive processing of this line off/on");
    if (c.hold.contains (p))
        return tr ("Hold: aktuelle adaptive Werte dieser Line halten/lösen", "Hold: freeze/release the current adaptive values of this line");
    if (c.prio.contains (p))
        return tr ("Vorrang dieser Line: ", "Priority of this line: ") + getPriorityNames()[juce::jlimit (0, 4, t.priorityIndex)]
               + tr (" – Klick wählt", " – click to choose");
    return {};
}

//==============================================================================
// Analyzer: Geometrie und Kurve unter der Maus
namespace
{
float specY (juce::Rectangle<float> plot, float db)
{
    return juce::jmap (juce::jlimit (specMinDb, specMaxDb, db), specMinDb, specMaxDb, plot.getBottom(), plot.getY());
}
}

juce::Rectangle<float> MixOverview::spectrumPlot() const
{
    return spectrumArea.reduced (10, 6).withTrimmedTop (22).withTrimmedBottom (14).withTrimmedLeft (24).toFloat();
}

int MixOverview::curveAt (juce::Point<float> pos) const
{
    const auto plot = spectrumPlot();
    if (! plot.contains (pos) || responseFine.size() != tracks.size())
        return -1;

    const float fd = (pos.x - plot.getX()) / plot.getWidth() * (float) (kNumDisplayPoints - 1);
    const int d0 = juce::jlimit (0, kNumDisplayPoints - 2, (int) fd);
    const float frac = juce::jlimit (0.0f, 1.0f, fd - (float) d0);

    int best = -1;
    float bestDist = 9.0f;   // Fangbereich in Pixeln
    for (int i = 0; i < (int) tracks.size(); ++i)
    {
        const auto& t = tracks[(size_t) i];
        if (t.mode != InstanceMode::Track || ! t.hasAnalysis)
            continue;
        const float v0 = t.displayDb[(size_t) d0] + responseFine[(size_t) i][(size_t) d0];
        const float v1 = t.displayDb[(size_t) d0 + 1] + responseFine[(size_t) i][(size_t) d0 + 1];
        const float v = v0 + frac * (v1 - v0);
        if (v <= specMinDb + 1.0f)
            continue;   // am Boden liegen alle Kurven übereinander
        const float dist = std::abs (specY (plot, v) - pos.y);
        if (dist < bestDist) { bestDist = dist; best = i; }
    }
    return best;
}

void MixOverview::mouseMove (const juce::MouseEvent& e)
{
    const int idx = curveAt (e.position);
    const uint32_t id = idx >= 0 ? tracks[(size_t) idx].instanceId : 0;
    if (id != hoverId || id != 0)
    {
        hoverId = id;
        hoverPos = e.position;
        repaint (spectrumArea);
    }
    setMouseCursor (id != 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void MixOverview::mouseExit (const juce::MouseEvent&)
{
    if (hoverId != 0)
    {
        hoverId = 0;
        repaint (spectrumArea);
    }
}

void MixOverview::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    auto select = [this] (uint32_t id)
    {
        selectedId = (selectedId == id ? 0 : id);
        slowDirty = true;   // Patchfeld-Auswahl sofort nachführen
        idleTicks = 0;
        repaint();
    };

    // Analyzer: Klick auf eine Kurve wählt die Line
    if (spectrumArea.contains (p))
    {
        if (const int idx = curveAt (e.position); idx >= 0)
            select (tracks[(size_t) idx].instanceId);
        return;
    }

    const int row = rowAt (p);
    if (row < 0)
        return;

    auto& t = tracks[(size_t) row];
    const auto c = cellsFor (rowBounds (row));

    // Clip-Merker zurücksetzen
    if ((t.clipIn || t.clipOut) && c.level.contains (p))
    {
        hub.resetClip (t.slot);
        t.clipIn = t.clipOut = false;
        repaint (listArea);
        return;
    }

    // Fernbedienung der Line: Direkt / Hold / Vorrang
    if (t.mode == InstanceMode::Track)
    {
        if (c.direct.contains (p) || c.hold.contains (p))
        {
            const bool isDirect = c.direct.contains (p);
            auto& state = isDirect ? t.direct : t.frozen;
            state = ! state;   // sofort anzeigen; die Line bestätigt mit ihrem nächsten Timer-Takt
            hub.sendRemote (t.slot, isDirect ? MixHub::remoteDirect : MixHub::remoteHold, state ? 1.0f : 0.0f);
            idleTicks = 0;
            repaint (listArea);
            return;
        }
        if (c.prio.contains (p))
        {
            showPriorityMenu (row);
            return;
        }
    }

    select (t.instanceId);
}

void MixOverview::paint (juce::Graphics& g)
{
    const auto clip = g.getClipBounds();
    if (clip.intersects (listArea))
        paintList (g, listArea);
    if (clip.intersects (spectrumArea))
        paintSpectrum (g, spectrumArea);
    if (detailsFold.isFolded() && clip.intersects (detailsArea))
        drawFoldedColumn (g, detailsArea.toFloat(), tr ("PATCHFELD · PEGEL · SPEKTROGRAMM", "PATCHBAY · LEVEL · SPECTROGRAM"));
}

void MixOverview::paintList (juce::Graphics& g, juce::Rectangle<int> area)
{
    drawEngravedPanel (g, area.toFloat(), {});
    area.reduce (8, 6);

    const auto roleNames = getRoleNames();
    const uint32_t masterId = hub.getActiveMasterId();

    g.setFont (fonts::label (16.0f));
    g.setColour (colours::ink);
    {
        const auto c = cellsFor (area.removeFromTop (headH));
        g.drawText ("LINE", c.name, juce::Justification::centredLeft);
        g.drawText (tr ("ROLLE", "ROLE"), c.role, juce::Justification::centredLeft);
        g.drawText (tr ("PEGEL", "LEVEL"), c.level, juce::Justification::centredLeft);
        g.drawText ("GR", c.gr, juce::Justification::centredRight);
        g.drawText (tr ("  MASKIERT", "  MASKED"), c.mask, juce::Justification::centredLeft);
        g.drawText (tr ("ÜBERDECKT VON", "MASKED BY"), c.info, juce::Justification::centredLeft);
        g.drawText (tr ("ADAPTIV", "ADAPTIVE"), c.adapt, juce::Justification::centredLeft);
        g.drawText ("D", c.direct, juce::Justification::centred);
        g.drawText ("H", c.hold, juce::Justification::centred);
        g.drawText (tr ("VORR.", "PRIO"), c.prio, juce::Justification::centredLeft);
    }
    if (listFold.isFolded())
        return;

    g.setColour (colours::engrave);
    g.drawHorizontalLine (area.getY(), (float) area.getX(), (float) area.getRight());

    if (tracks.empty())
    {
        g.setFont (fonts::mono (13.0f));
        g.drawText (tr ("Keine Lines verbunden.", "No lines connected."), area, juce::Justification::centred);
        return;
    }

    // Schaltfeld in der Zeile (Direkt, Hold, Vorrang)
    auto cell = [&g] (juce::Rectangle<int> r, const juce::String& text, bool on, juce::Colour onColour)
    {
        const auto b = r.reduced (1, 1).toFloat();
        g.setColour (on ? onColour : colours::cream);
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (colours::engrave);
        g.drawRoundedRectangle (b, 3.0f, 1.0f);
        g.setColour (on ? colours::bakelite : colours::inkDim);
        g.setFont (fonts::label (13.0f));
        g.drawText (text, b, juce::Justification::centred);
    };

    const int first = firstRow();
    const int last = juce::jmin ((int) tracks.size(), first + visibleRows());

    for (int idx = first; idx < last; ++idx)
    {
        const auto& t = tracks[(size_t) idx];
        const auto rowArea = rowBounds (idx).reduced (0, 2);

        if (t.instanceId == selectedId)
        {
            g.setColour (colours::amber.withAlpha (0.35f));
            g.fillRoundedRectangle (rowArea.withTrimmedRight (14).toFloat(), 3.0f);
        }

        g.setColour (colourFor (t));
        g.fillRoundedRectangle (rowArea.withWidth (6).toFloat(), 2.0f);
        const auto c = cellsFor (rowArea);

        g.setFont (fonts::mono (13.0f, true));
        g.setColour (colours::ink);
        juce::String name = t.name;
        if (t.mode == InstanceMode::Master)
            name += t.instanceId == masterId ? juce::String (" [EX]") : AME_TXT (" [EX–]");
        g.drawText (name, c.name, juce::Justification::centredLeft, true);

        g.setFont (fonts::mono (12.0f));
        g.setColour (colours::inkDim);
        juce::String roleText = t.mode == InstanceMode::Master ? AME_TXT ("–") : roleNames[(int) t.role];
        if (t.mode == InstanceMode::Track && t.autoRole)
            roleText = t.role == Role::Auto ? AME_TXT ("Auto …") : roleText + " (A)";
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

        // Clip-Merker: rotes Feld am Ende der Pegelkette (Klick setzt zurück)
        if (t.clipIn || t.clipOut)
        {
            const auto clipBox = c.level.reduced (0, 4).removeFromRight (9).toFloat();
            g.setColour (juce::Colour (0xffff3a2a));
            g.fillRoundedRectangle (clipBox, 2.0f);
            g.setColour (colours::bakelite);
            g.drawRoundedRectangle (clipBox, 2.0f, 1.0f);
        }

        if (t.mode == InstanceMode::Track)
        {
            float maxCut = 0.0f;
            for (auto v : t.appliedCutDb) maxCut = juce::jmin (maxCut, v);
            juce::String a;
            a << (t.appliedGainDb >= 0.0f ? "+" : "") << juce::String (t.appliedGainDb, 1) << "dB";
            if (t.appliedHpfHz >= 20.0f) a << " HP" << juce::roundToInt (t.appliedHpfHz);
            if (maxCut < -0.1f)          a << " EQ" << juce::String (maxCut, 1);
            g.setFont (fonts::mono (12.0f));
            g.setColour (t.bypassed ? colours::inkDim.withAlpha (0.55f) : colours::inkDim);
            g.drawText (a, c.adapt, juce::Justification::centredLeft, true);

            static const char* const prioShort[] { "PROG", "++", "+", "-", "- -" };
            cell (c.direct, "D", t.direct, colours::amberLamp);
            cell (c.hold, "H", t.frozen, juce::Colour (0xff8fd0ff));
            cell (c.prio, prioShort[juce::jlimit (0, 4, t.priorityIndex)], t.priorityIndex != 0, colours::amber);
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

    const auto plot = spectrumPlot();
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

    // Kurven nach Engine, je Spur in ihrer Farbe (Fläche + Leuchtlinie).
    // Anzeige-Punkte liegen logarithmisch gleichmäßig: x ist linear im Index.
    auto xAt = [&plot] (int d) { return plot.getX() + plot.getWidth() * (float) d / (float) (kNumDisplayPoints - 1); };
    auto curve = [&] (int idx, const float* values, bool closed)
    {
        juce::Path p;
        p.preallocateSpace (3 * kNumDisplayPoints + 16);
        if (closed) p.startNewSubPath (xAt (0), plot.getBottom());
        for (int d = 0; d < kNumDisplayPoints; ++d)
        {
            const float x = xAt (d);
            const float y = yOf (values[d] + responseFine[(size_t) idx][(size_t) d]);
            if (d == 0 && ! closed) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (closed) { p.lineTo (xAt (kNumDisplayPoints - 1), plot.getBottom()); p.closeSubPath(); }
        return p;
    };

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    // Viele Lines: Fläche und Leuchtsaum je Line kosten am meisten und überdecken sich ohnehin –
    // dann je Line nur eine Linie (die gewählte Line und die Summe behalten Fläche und Saum)
    int numCurves = 0;
    for (const auto& t : tracks)
        numCurves += (t.mode == InstanceMode::Track && t.hasAnalysis) ? 1 : 0;
    const bool many = numCurves > maxGlowCurves;
    const uint32_t activeMasterId = hub.getActiveMasterId();

    for (int i = 0; i < (int) tracks.size(); ++i)
    {
        const auto& t = tracks[(size_t) i];
        if (t.mode != InstanceMode::Track || ! t.hasAnalysis || i == selIdx) continue;
        const auto c = colourFor (t);
        const bool dim = selTrack >= 0;
        if (many)
        {
            g.setColour (c.withAlpha (dim ? 0.4f : 0.85f));
            g.strokePath (curve (i, t.displayDb.data(), false), juce::PathStrokeType (1.0f));
            continue;
        }
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
        if (t.mode != InstanceMode::Master || t.instanceId != activeMasterId || ! t.hasAnalysis) continue;
        juce::Path sum;
        for (int d = 0; d < kNumDisplayPoints; ++d)
        {
            const float x = xAt (d), y = yOf (t.displayDb[(size_t) d]);
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
            const float x = xAt (d), y = yOf (peakDb[(size_t) d]);
            if (d == 0) pk.startNewSubPath (x, y); else pk.lineTo (x, y);
        }
        juce::Path dotted;
        const float dots[] { 1.5f, 3.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dotted, pk, dots, 2);
        g.setColour (colours::amberLamp);
        g.fillPath (dotted);
    }

    // Kurve unter der Maus hervorheben
    int hoverIdx = -1;
    for (int i = 0; i < (int) tracks.size() && hoverId != 0; ++i)
        if (tracks[(size_t) i].instanceId == hoverId && tracks[(size_t) i].mode == InstanceMode::Track && tracks[(size_t) i].hasAnalysis)
            hoverIdx = i;
    if (hoverIdx >= 0 && hoverIdx != selIdx)
        drawPhosphorGlow (g, curve (hoverIdx, tracks[(size_t) hoverIdx].displayDb.data(), false),
                          colourFor (tracks[(size_t) hoverIdx]).brighter (0.3f), 2.0f);

    g.restoreState();

    // Namensschild an der Maus
    if (hoverIdx >= 0)
    {
        const auto& t = tracks[(size_t) hoverIdx];
        juce::String text = t.name;
        if (t.role != Role::Auto)
            text << AME_TXT (" · ") << getRoleNames()[(int) t.role];
        g.setFont (fonts::mono (12.0f, true));
        const float w = juce::jmin (plot.getWidth() - 8.0f, (float) text.length() * 7.4f + 22.0f);
        auto box = juce::Rectangle<float> (w, 18.0f).withPosition (hoverPos.x + 12.0f, hoverPos.y - 24.0f);
        box.setX (juce::jlimit (plot.getX() + 4.0f, plot.getRight() - 4.0f - w, box.getX()));
        box.setY (juce::jlimit (plot.getY() + 20.0f, plot.getBottom() - 22.0f, box.getY()));
        g.setColour (colours::display.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (colourFor (t));
        g.drawRoundedRectangle (box, 3.0f, 1.0f);
        g.fillRect (box.reduced (5.0f, 5.0f).withWidth (8.0f));
        g.setColour (colours::cream);
        g.drawText (text, box.withTrimmedLeft (18.0f).withTrimmedRight (5.0f), juce::Justification::centredLeft, true);
    }

    // Legende: Spurfarben + Summe
    g.setFont (fonts::mono (10.0f, true));
    auto legend = plot.reduced (8.0f, 5.0f).removeFromTop (12.0f);
    if (activeMasterId != 0)
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
} // namespace ame::ui
