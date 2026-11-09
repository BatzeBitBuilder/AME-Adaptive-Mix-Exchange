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

namespace ame::ui
{
// Mix-Übersicht im Vintage-Stil:
//  oben:  Spurliste (Rolle, Pegel, Maskierung, Hauptüberdeckung, Adaptiv) + VU L/R der eigenen Instanz
//  Mitte: Phosphor-Analyzer aller Spuren (Spurfarben, Masking-Wärmesäulen, Peak-Hold der Auswahl)
//  unten: Spektrogramm der Auswahl + Masking-Matrix
// Klick auf Zeile / Matrixzeile wählt eine Spur aus.
class MixOverview : public juce::Component
{
public:
    explicit MixOverview (MixHub& hub);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh (uint32_t ownInstanceId, float dtSec = 1.0f / 30.0f);
    void updateTexts();   // nach Sprachwechsel

    const MaskingResult& getMasking() const noexcept { return masking; }

private:
    void paintList (juce::Graphics&, juce::Rectangle<int>);
    void paintSpectrum (juce::Graphics&, juce::Rectangle<int>);
    juce::Colour colourFor (const TrackSnapshot&) const;
    int selectedSnapshotIndex() const;

    MixHub& hub;
    std::vector<TrackSnapshot> tracks;
    MaskingResult masking, maskingPre;   // nach / vor den adaptiven Eingriffen
    std::vector<std::array<float, kNumAnalysisBands>> responseDb;   // adaptiver Frequenzgang je Snapshot-Eintrag (Bänder)
    std::vector<std::array<float, kNumDisplayPoints>> responseFine; // ... je Anzeige-Punkt
    std::array<float, kNumDisplayPoints> peakDb {};
    uint32_t ownId = 0, selectedId = 0, peakForId = 0;

    VuMeter vuLeft { {}, VuMeter::Mode::Level }, vuRight { {}, VuMeter::Mode::Level };
    Waterfall waterfall;
    MaskingMatrix matrix;

    juce::Rectangle<int> listArea, spectrumArea;
    static constexpr int rowH = 22, headH = 22;
};
} // namespace ame::ui
