// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ChannelStrip.h"

namespace mtm
{
// Liest die manuellen Strip-Parameter aus dem APVTS (Zeiger einmalig ermittelt, echtzeitsicher)
class StripParamReader
{
public:
    explicit StripParamReader (juce::AudioProcessorValueTreeState&);

    // füllt alle manuellen Werte; adaptive Felder bleiben unberührt
    void read (ChannelStrip::Params&) const noexcept;

private:
    float get (int i) const noexcept { return ptrs[(size_t) i]->load (std::memory_order_relaxed); }
    bool  on  (int i) const noexcept { return get (i) > 0.5f; }
    std::vector<std::atomic<float>*> ptrs;
};
} // namespace mtm
