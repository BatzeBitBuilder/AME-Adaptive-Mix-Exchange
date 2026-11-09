// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "hub/MixHub.h"

namespace ame::ui
{
// Spektrogramm (Wasserfall): x = Frequenz (log, 20 Hz – 20 kHz), y = Zeit (neu oben), Farbe = Pegel
class Waterfall : public juce::Component
{
public:
    Waterfall();

    // neue Zeile aus Anzeige-Punkten (dB) anhängen
    void pushRow (const std::array<float, kNumDisplayPoints>& db);
    void clear();

    void paint (juce::Graphics&) override;
    void resized() override;

    void setRange (float minDb, float maxDb) { lo = minDb; hi = maxDb; }
    void setCaption (juce::String c) { caption = std::move (c); repaint(); }

private:
    juce::Rectangle<int> imageArea() const;

    juce::Image image;
    std::vector<int> pointForColumn;
    std::vector<float> fracForColumn;
    float lo = -80.0f, hi = -10.0f;
    juce::String caption;
};
} // namespace ame::ui
