// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "hub/MixHub.h"
#include "hub/MaskingModel.h"
#include "VuMeter.h"
#include "Waterfall.h"
#include "MaskingMatrix.h"
#include "FoldButton.h"

namespace ame::ui
{
// Mix-Übersicht im Vintage-Stil:
//  links oben:   Spurliste (Rolle, Pegel, Maskierung, Hauptüberdeckung, Adaptiv) – einklappbar
//  links unten:  Phosphor-Analyzer aller Spuren (Spurfarben, Masking-Wärmesäulen, Peak-Hold der Auswahl)
//  rechts:       Patchfeld (Masking-Matrix), VU L/R der eigenen Instanz, Spektrogramm der Auswahl – einklappbar
// Klick auf Zeile / Matrixzeile / Analyzer-Kurve wählt eine Spur aus. Die Liste ist rollbar (Mausrad, Rollbalken);
// je Line lassen sich Direkt, Hold und Vorrang direkt in der Zeile schalten (Fernbedienung über den Hub).
class MixOverview : public juce::Component,
                    public juce::TooltipClient,
                    private juce::ScrollBar::Listener
{
public:
    explicit MixOverview (MixHub& hub);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    juce::String getTooltip() override;
    void refresh (uint32_t ownInstanceId, float dtSec = 1.0f / 30.0f);
    void updateTexts();   // nach Sprachwechsel

    const MaskingResult& getMasking() const noexcept { return masking; }

private:
    void paintList (juce::Graphics&, juce::Rectangle<int>);
    void paintSpectrum (juce::Graphics&, juce::Rectangle<int>);
    juce::Colour colourFor (const TrackSnapshot&) const;
    int selectedSnapshotIndex() const;

    // Liste: Zeilen-Geometrie (gemeinsam für Zeichnen, Klick und Tooltip)
    struct RowCells { juce::Rectangle<int> name, role, level, gr, mask, info, adapt, direct, hold, prio; };
    RowCells cellsFor (juce::Rectangle<int> row) const;
    juce::Rectangle<int> rowsArea() const;
    juce::Rectangle<int> rowBounds (int row) const;
    int visibleRows() const;
    int firstRow() const;
    int rowAt (juce::Point<int>) const;      // Index in tracks oder -1
    void updateListScroll();
    void showPriorityMenu (int row);
    void scrollBarMoved (juce::ScrollBar*, double) override { repaint (listArea); }

    // Analyzer: Kurve unter der Maus
    juce::Rectangle<float> spectrumPlot() const;
    int curveAt (juce::Point<float>) const;  // Index in tracks oder -1

    MixHub& hub;
    std::vector<TrackSnapshot> tracks;
    MaskingResult masking, maskingPre;   // nach / vor den adaptiven Eingriffen
    std::vector<std::array<float, kNumAnalysisBands>> responseDb;   // adaptiver Frequenzgang je Snapshot-Eintrag (Bänder)
    std::vector<std::array<float, kNumDisplayPoints>> responseFine; // ... je Anzeige-Punkt
    std::array<float, kNumDisplayPoints> peakDb {};
    uint32_t ownId = 0, selectedId = 0, peakForId = 0;
    uint32_t hoverId = 0;                    // Line, deren Kurve im Analyzer unter der Maus liegt
    juce::Point<float> hoverPos;
    juce::ScrollBar listScroll { true };

    // Takt: Analyzer mit jedem Aufruf (30 Hz), Maskierung und Liste mit 10 Hz;
    // ohne laufendes Audio (kein Host-Block seit ~1,5 s) nur noch 2-mal je Sekunde
    int tick = 0, idleTicks = 0;
    uint32_t blockSum = 0;
    bool slowDirty = true;

    VuMeter vuLeft { {}, VuMeter::Mode::Level }, vuRight { {}, VuMeter::Mode::Level };
    Waterfall waterfall;
    MaskingMatrix matrix;

    juce::Rectangle<int> listArea, spectrumArea, detailsArea;
    FoldButton listFold { "foldExList" }, detailsFold { "foldExDetails" };
    static constexpr int rowH = 22, headH = 22;
    static constexpr int maxGlowCurves = 12;   // darüber: Analyzer-Kurven als einfache Linien
};
} // namespace ame::ui
