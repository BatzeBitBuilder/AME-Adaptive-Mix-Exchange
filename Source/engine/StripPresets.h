// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

#include "Roles.h"

namespace ame::presets
{
struct Value
{
    const char* id;
    float value;     // Klarwert (Hz, dB, ms, Index …)
};

// Werkspreset je Rolle: Startwerte für Filter, EQ, Gate, Kompressor, De-Esser (Klang-Module)
struct FactoryPreset
{
    Role role;
    std::vector<Value> values;
};

const std::vector<FactoryPreset>& getFactoryPresets();
const FactoryPreset* findFactoryPreset (Role);

// Parameter, die ein Werkspreset setzt (alle übrigen Klang-Parameter gehen auf Standard).
// Input, Phase, Pan, Width, Output und alle Adaptiv-/Instanz-Parameter bleiben unberührt.
const juce::StringArray& getSoundScope();

// Parameter, die ein eigenes Preset speichert: Klang-Module + Input, Phase, Output-Sektion, Limiter
const juce::StringArray& getUserScope();

// Werte setzen (Message-Thread, mit Gesten für Undo/Automation der DAW)
void applyFactory (juce::AudioProcessorValueTreeState&, const FactoryPreset&);
void applyValues (juce::AudioProcessorValueTreeState&, const juce::StringArray& scope,
                  const std::vector<std::pair<juce::String, float>>& values, bool resetOthers);
// --- Eigene Presets: %APPDATA%\AME\Presets\<Name>.amepreset (XML, Klarwerte) ---
juce::File getUserPresetFolder();
juce::StringArray getUserPresetNames();                       // alphabetisch
bool saveUserPreset (juce::AudioProcessorValueTreeState&, const juce::String& name);
bool loadUserPreset (juce::AudioProcessorValueTreeState&, const juce::String& name);
bool deleteUserPreset (const juce::String& name);

// --- A/B-Vergleich der Einstellungen (im Projekt gespeichert) ---
juce::String getActiveAB (const juce::AudioProcessorValueTreeState&);      // "A" oder "B"
void selectAB (juce::AudioProcessorValueTreeState&, const juce::String& slot);
void copyActiveToOther (juce::AudioProcessorValueTreeState&);
} // namespace ame::presets
