#pragma once

#include <array>
#include "Roles.h"
#include "hub/MixHub.h"

namespace mtm
{
constexpr int kNumRoles = (int) Role::NumRoles;

struct RoleGuess
{
    Role role = Role::Auto;      // Auto = nichts erkannt
    float confidence = 0.0f;     // 0..1
    bool fromName = false;
    std::array<float, kNumRoles> probabilities {};   // nur bei Merkmals-Erkennung
};

// Erkennung über den Spurnamen (deutsch/englisch, gängige Abkürzungen)
RoleGuess classifyByName (const juce::String& trackName);

// Erkennung über Klangmerkmale (regelbasiert, Prototyp je Rolle)
RoleGuess classifyByFeatures (const RoleFeatures& f);

// Mindest-Auswertezeit, bevor Merkmale genutzt werden
constexpr float kMinAnalysisSec = 3.0f;

// Zustand mit Hysterese (pro Instanz)
class RoleDetector
{
public:
    // liefert die effektive Auto-Rolle (Role::Auto = noch unbekannt)
    RoleGuess update (const juce::String& trackName, const RoleFeatures& f);
    void reset() { current = {}; }

private:
    RoleGuess current;
};
} // namespace mtm
