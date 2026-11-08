// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <array>
#include "Roles.h"

namespace mtm
{
// Frequenzregionen für Prioritäten
enum Region { Sub = 0, Low, Mid, High, NumRegions };   // <90 Hz, 90–300 Hz, 300 Hz–3 kHz, >3 kHz

struct RoleProfile
{
    std::array<float, NumRegions> priority {};   // 0..1, höher = setzt sich durch
    float levelDb = -8.0f;                       // Ziel-Lautheit relativ (0 = lautestes Element)
    float hpfMaxHz = 60.0f;                      // höchster sinnvoller Hochpass
};

struct GenreProfile
{
    float maxCutDb = 6.0f;      // maximale Absenkung je Oktavband
    float maxGainDb = 6.0f;     // maximale Lautstärke-Korrektur
};

const RoleProfile& getRoleProfile (Genre genre, Role role) noexcept;
GenreProfile getGenreProfile (Genre genre) noexcept;

// Priorität bei Frequenz (weich zwischen Regionen interpoliert)
float getPriorityAt (const RoleProfile& p, float hz) noexcept;

// Prioritäts-Offset der Spur-Einstellung (Parameter "priority")
float getPriorityOffset (int choiceIndex) noexcept;
} // namespace mtm
