// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_core/juce_core.h>
#include "hub/MixHub.h"

namespace ame
{
// Adaptive Engine: läuft in der aktiven Master-Instanz (eigener Thread, 50 Hz).
// Liest Spektren (vor den adaptiven Eingriffen) aller Spuren und schreibt Zielwerte
// (Oktavband-Absenkungen, Hochpass, Lautstärke) in den Hub.
class MixEngine : private juce::HighResolutionTimer
{
public:
    static constexpr int kIntervalMs = 20;

    explicit MixEngine (MixHub& hub);
    ~MixEngine() override;

    void setActive (bool shouldBeActive);
    bool isActive() const { return isTimerRunning(); }

    void step();   // ein Rechenschritt (auch synchron für Tests)

private:
    void hiResTimerCallback() override { step(); }

    MixHub& hub;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixEngine)
};
} // namespace ame
