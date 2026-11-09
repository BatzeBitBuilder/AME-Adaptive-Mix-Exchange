// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <memory>

#include "hub/MixHub.h"

namespace ame
{
// Mitschnitt (Lernlauf): die tatsächlich angewendeten adaptiven Werte einer Line, aufgezeichnet über die
// Songposition (50 Werte/s). Wiedergabe liefert exakt dieselben Werte an derselben Stelle – unabhängig von
// Engine, Exchange und Render-Geschwindigkeit (Export = Abhöre, auch für Einzelspuren).
//
// Threads: write()/read() im Audio-Thread (lock-frei), allocate()/clear()/toBase64()/fromBase64() im Message-Thread.
// Speicher wird einmalig angelegt (allocate) und erst im Destruktor freigegeben.
class AdaptiveRecording
{
public:
    static constexpr int kFramesPerSecond = 50;
    static constexpr int kMaxSeconds = 30 * 60;
    static constexpr int kMaxFrames = kFramesPerSecond * kMaxSeconds;
    static constexpr int kValues = kNumAdaptiveBands + 2;   // Absenkungen, Hochpass, Lautstärke

    struct Values
    {
        std::array<float, kNumAdaptiveBands> cutDb {};
        float hpfHz = 0.0f, gainDb = 0.0f;
    };

    // Message-Thread
    void allocate();
    bool isAllocated() const noexcept { return data.load (std::memory_order_acquire) != nullptr; }
    void clear();
    juce::String toBase64() const;            // leer, wenn nichts aufgezeichnet
    bool fromBase64 (const juce::String&);

    // Audio-Thread
    void write (double startSec, double durationSec, const Values&) noexcept;   // Block ab Songposition
    bool read (double timeSec, Values&) const noexcept;   // false = keine Daten

    bool hasData() const noexcept { return numFrames.load (std::memory_order_relaxed) > 0; }
    float getLengthSec() const noexcept { return (float) numFrames.load (std::memory_order_relaxed) / (float) kFramesPerSecond; }

private:
    static constexpr int16_t kInvalid = INT16_MIN;
    bool frameValid (int f) const noexcept;
    void decode (int f, Values&) const noexcept;

    std::unique_ptr<std::atomic<int16_t>[]> storage;
    std::atomic<std::atomic<int16_t>*> data { nullptr };
    std::atomic<int> numFrames { 0 };   // höchster gültiger Frame + 1
};
} // namespace ame
