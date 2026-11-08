#include "Profiles.h"

#include <cmath>

namespace mtm
{
namespace
{
constexpr int kRoles = (int) Role::NumRoles;
using Table = std::array<RoleProfile, kRoles>;

//                              Sub   Low   Mid   High   Level  HPF
#define P(s, l, m, h, lv, hp) RoleProfile { { s, l, m, h }, lv, hp }

const Table rockTable {
    P (0.5f, 0.4f, 0.5f, 0.5f,  -8.0f,  40.0f),   // Auto
    P (1.0f, 0.6f, 0.4f, 0.6f,  -3.0f,  25.0f),   // Kick
    P (0.2f, 0.7f, 0.8f, 0.7f,  -3.0f,  70.0f),   // Snare
    P (0.1f, 0.3f, 0.5f, 0.7f,  -8.0f, 100.0f),   // Drums/Overheads
    P (0.1f, 0.2f, 0.4f, 0.5f, -12.0f, 150.0f),   // Percussion
    P (0.8f, 0.9f, 0.5f, 0.3f,  -4.0f,  30.0f),   // Bass
    P (0.1f, 0.6f, 1.0f, 0.9f,   0.0f,  90.0f),   // Lead Vocal
    P (0.1f, 0.3f, 0.6f, 0.6f,  -8.0f, 120.0f),   // Backing Vocals
    P (0.1f, 0.5f, 0.8f, 0.6f,  -4.0f,  90.0f),   // E-Gitarre
    P (0.1f, 0.4f, 0.6f, 0.6f,  -8.0f, 100.0f),   // Akustikgitarre
    P (0.2f, 0.4f, 0.5f, 0.4f,  -8.0f,  60.0f),   // Keys
    P (0.1f, 0.3f, 0.7f, 0.7f,  -6.0f, 100.0f),   // Synth Lead
    P (0.1f, 0.2f, 0.3f, 0.3f, -12.0f, 150.0f),   // Pad
    P (0.1f, 0.3f, 0.5f, 0.5f, -10.0f, 100.0f),   // Streicher
    P (0.1f, 0.3f, 0.6f, 0.5f,  -8.0f, 100.0f),   // Bläser
    P (0.0f, 0.1f, 0.2f, 0.2f, -14.0f, 150.0f),   // FX
    P (0.3f, 0.4f, 0.5f, 0.5f,  -8.0f,  60.0f),   // Sonstiges
};

const Table popTable {
    P (0.5f, 0.4f, 0.4f, 0.4f,  -9.0f,  40.0f),   // Auto
    P (1.0f, 0.6f, 0.3f, 0.5f,  -4.0f,  25.0f),   // Kick
    P (0.2f, 0.6f, 0.6f, 0.6f,  -5.0f,  80.0f),   // Snare
    P (0.1f, 0.2f, 0.3f, 0.5f, -10.0f, 120.0f),   // Drums/Overheads
    P (0.1f, 0.2f, 0.3f, 0.4f, -12.0f, 150.0f),   // Percussion
    P (0.8f, 0.9f, 0.4f, 0.2f,  -5.0f,  30.0f),   // Bass
    P (0.1f, 0.8f, 1.0f, 1.0f,   0.0f,  80.0f),   // Lead Vocal
    P (0.1f, 0.3f, 0.6f, 0.7f,  -7.0f, 120.0f),   // Backing Vocals
    P (0.1f, 0.4f, 0.5f, 0.4f,  -8.0f, 100.0f),   // E-Gitarre
    P (0.1f, 0.3f, 0.5f, 0.5f,  -8.0f, 100.0f),   // Akustikgitarre
    P (0.2f, 0.4f, 0.5f, 0.4f,  -7.0f,  70.0f),   // Keys
    P (0.1f, 0.3f, 0.6f, 0.6f,  -7.0f, 100.0f),   // Synth Lead
    P (0.1f, 0.2f, 0.2f, 0.3f, -12.0f, 150.0f),   // Pad
    P (0.1f, 0.3f, 0.4f, 0.5f, -10.0f, 100.0f),   // Streicher
    P (0.1f, 0.3f, 0.5f, 0.5f,  -9.0f, 100.0f),   // Bläser
    P (0.0f, 0.1f, 0.2f, 0.2f, -14.0f, 150.0f),   // FX
    P (0.3f, 0.4f, 0.4f, 0.4f,  -9.0f,  60.0f),   // Sonstiges
};

const Table electronicTable {
    P (0.5f, 0.4f, 0.5f, 0.5f,  -8.0f,  40.0f),   // Auto
    P (1.0f, 0.9f, 0.5f, 0.6f,   0.0f,  20.0f),   // Kick
    P (0.2f, 0.6f, 0.7f, 0.7f,  -4.0f,  90.0f),   // Snare/Clap
    P (0.1f, 0.2f, 0.5f, 0.7f,  -8.0f, 150.0f),   // Drums/Hats
    P (0.1f, 0.2f, 0.4f, 0.6f, -10.0f, 200.0f),   // Percussion
    P (0.7f, 1.0f, 0.5f, 0.3f,  -2.0f,  25.0f),   // Bass
    P (0.1f, 0.6f, 0.9f, 0.9f,  -2.0f, 100.0f),   // Lead Vocal
    P (0.1f, 0.3f, 0.5f, 0.6f,  -8.0f, 150.0f),   // Backing Vocals
    P (0.1f, 0.3f, 0.6f, 0.5f,  -8.0f, 120.0f),   // E-Gitarre
    P (0.1f, 0.3f, 0.5f, 0.5f,  -9.0f, 120.0f),   // Akustikgitarre
    P (0.1f, 0.3f, 0.6f, 0.5f,  -7.0f, 100.0f),   // Keys
    P (0.1f, 0.4f, 0.9f, 0.8f,  -4.0f, 100.0f),   // Synth Lead
    P (0.1f, 0.2f, 0.4f, 0.4f, -10.0f, 150.0f),   // Pad
    P (0.1f, 0.2f, 0.4f, 0.4f, -10.0f, 120.0f),   // Streicher
    P (0.1f, 0.3f, 0.5f, 0.5f,  -9.0f, 120.0f),   // Bläser
    P (0.0f, 0.1f, 0.3f, 0.4f, -12.0f, 200.0f),   // FX
    P (0.3f, 0.4f, 0.5f, 0.5f,  -8.0f,  60.0f),   // Sonstiges
};

const Table hipHopTable {
    P (0.5f, 0.4f, 0.4f, 0.4f,  -9.0f,  40.0f),   // Auto
    P (1.0f, 0.8f, 0.4f, 0.6f,  -2.0f,  20.0f),   // Kick
    P (0.2f, 0.6f, 0.7f, 0.7f,  -4.0f,  90.0f),   // Snare/Clap
    P (0.1f, 0.2f, 0.3f, 0.6f, -10.0f, 200.0f),   // Drums/Hats
    P (0.1f, 0.2f, 0.3f, 0.5f, -12.0f, 200.0f),   // Percussion
    P (0.9f, 1.0f, 0.4f, 0.2f,  -3.0f,  25.0f),   // Bass/808
    P (0.1f, 0.8f, 1.0f, 1.0f,   0.0f,  80.0f),   // Lead Vocal
    P (0.1f, 0.3f, 0.6f, 0.6f,  -7.0f, 130.0f),   // Backing Vocals / Adlibs
    P (0.1f, 0.3f, 0.4f, 0.4f, -10.0f, 120.0f),   // E-Gitarre
    P (0.1f, 0.3f, 0.4f, 0.4f, -10.0f, 120.0f),   // Akustikgitarre
    P (0.1f, 0.3f, 0.5f, 0.4f,  -9.0f, 100.0f),   // Keys/Sample
    P (0.1f, 0.3f, 0.6f, 0.6f,  -7.0f, 120.0f),   // Synth Lead
    P (0.1f, 0.2f, 0.3f, 0.3f, -12.0f, 150.0f),   // Pad
    P (0.1f, 0.2f, 0.4f, 0.4f, -10.0f, 120.0f),   // Streicher
    P (0.1f, 0.3f, 0.5f, 0.5f,  -9.0f, 120.0f),   // Bläser
    P (0.0f, 0.1f, 0.2f, 0.3f, -14.0f, 200.0f),   // FX
    P (0.3f, 0.4f, 0.4f, 0.4f,  -8.0f,  60.0f),   // Sonstiges / Sample
};

const Table acousticTable {
    P (0.4f, 0.5f, 0.5f, 0.5f,  -8.0f,  50.0f),   // Auto
    P (0.9f, 0.5f, 0.3f, 0.4f,  -8.0f,  30.0f),   // Kick/Cajon-Bass
    P (0.2f, 0.5f, 0.6f, 0.6f,  -8.0f,  80.0f),   // Snare
    P (0.1f, 0.3f, 0.4f, 0.6f, -10.0f,  80.0f),   // Drums/Overheads
    P (0.1f, 0.3f, 0.4f, 0.6f, -12.0f, 120.0f),   // Percussion/Shaker
    P (0.8f, 0.8f, 0.4f, 0.2f,  -8.0f,  35.0f),   // Bass
    P (0.2f, 0.8f, 1.0f, 1.0f,   0.0f,  80.0f),   // Lead Vocal
    P (0.1f, 0.4f, 0.6f, 0.7f,  -7.0f, 110.0f),   // Backing Vocals
    P (0.1f, 0.5f, 0.6f, 0.6f,  -6.0f,  90.0f),   // E-Gitarre
    P (0.3f, 0.8f, 0.8f, 0.8f,  -3.0f,  70.0f),   // Akustikgitarre
    P (0.4f, 0.7f, 0.7f, 0.6f,  -5.0f,  50.0f),   // Keys/Piano
    P (0.1f, 0.3f, 0.5f, 0.5f, -10.0f, 100.0f),   // Synth Lead
    P (0.1f, 0.2f, 0.3f, 0.3f, -14.0f, 120.0f),   // Pad
    P (0.2f, 0.5f, 0.6f, 0.6f,  -9.0f,  70.0f),   // Streicher
    P (0.1f, 0.4f, 0.6f, 0.5f,  -9.0f,  90.0f),   // Bläser
    P (0.0f, 0.1f, 0.2f, 0.2f, -16.0f, 150.0f),   // FX
    P (0.3f, 0.5f, 0.5f, 0.5f,  -8.0f,  50.0f),   // Sonstiges
};

const Table metalTable {
    P (0.5f, 0.4f, 0.5f, 0.5f,  -8.0f,  40.0f),   // Auto
    P (1.0f, 0.6f, 0.3f, 0.9f,  -3.0f,  30.0f),   // Kick (Sub + Klick)
    P (0.2f, 0.7f, 0.8f, 0.7f,  -3.0f,  90.0f),   // Snare
    P (0.1f, 0.2f, 0.4f, 0.7f,  -8.0f, 150.0f),   // Drums/Overheads
    P (0.1f, 0.2f, 0.3f, 0.5f, -12.0f, 150.0f),   // Percussion
    P (0.8f, 0.8f, 0.6f, 0.3f,  -6.0f,  35.0f),   // Bass
    P (0.1f, 0.5f, 0.9f, 0.8f,  -2.0f, 100.0f),   // Lead Vocal
    P (0.1f, 0.3f, 0.6f, 0.6f,  -8.0f, 150.0f),   // Backing Vocals
    P (0.1f, 0.7f, 0.9f, 0.6f,  -2.0f,  90.0f),   // E-Gitarre
    P (0.1f, 0.4f, 0.5f, 0.5f, -10.0f, 120.0f),   // Akustikgitarre
    P (0.1f, 0.3f, 0.5f, 0.4f, -10.0f, 100.0f),   // Keys
    P (0.1f, 0.3f, 0.6f, 0.6f,  -8.0f, 120.0f),   // Synth Lead
    P (0.1f, 0.2f, 0.3f, 0.3f, -12.0f, 150.0f),   // Pad
    P (0.1f, 0.3f, 0.5f, 0.5f, -10.0f, 120.0f),   // Streicher
    P (0.1f, 0.3f, 0.5f, 0.5f, -10.0f, 120.0f),   // Bläser
    P (0.0f, 0.1f, 0.2f, 0.3f, -14.0f, 200.0f),   // FX
    P (0.3f, 0.4f, 0.5f, 0.5f,  -8.0f,  60.0f),   // Sonstiges
};
#undef P

constexpr float regionCentreHz[NumRegions] { 50.0f, 165.0f, 950.0f, 6000.0f };
} // namespace

const RoleProfile& getRoleProfile (Genre genre, Role role) noexcept
{
    const auto idx = (size_t) juce::jlimit (0, kRoles - 1, (int) role);
    switch (genre)
    {
        case Genre::PopVocal:   return popTable[idx];
        case Genre::Electronic: return electronicTable[idx];
        case Genre::HipHop:     return hipHopTable[idx];
        case Genre::Acoustic:   return acousticTable[idx];
        case Genre::Metal:      return metalTable[idx];
        case Genre::RockBand:
        case Genre::NumGenres:
        default:                return rockTable[idx];
    }
}

GenreProfile getGenreProfile (Genre genre) noexcept
{
    switch (genre)
    {
        case Genre::Electronic: return { 8.0f, 6.0f };
        case Genre::HipHop:     return { 8.0f, 6.0f };
        case Genre::Acoustic:   return { 4.0f, 4.0f };   // behutsam
        case Genre::Metal:      return { 7.0f, 6.0f };
        case Genre::PopVocal:   return { 6.0f, 6.0f };
        case Genre::RockBand:
        case Genre::NumGenres:
        default:                return { 6.0f, 6.0f };
    }
}

float getPriorityAt (const RoleProfile& p, float hz) noexcept
{
    if (hz <= regionCentreHz[0]) return p.priority[0];
    if (hz >= regionCentreHz[NumRegions - 1]) return p.priority[NumRegions - 1];

    for (int r = 0; r < NumRegions - 1; ++r)
    {
        if (hz < regionCentreHz[r + 1])
        {
            const float t = std::log (hz / regionCentreHz[r]) / std::log (regionCentreHz[r + 1] / regionCentreHz[r]);
            return p.priority[(size_t) r] + t * (p.priority[(size_t) r + 1] - p.priority[(size_t) r]);
        }
    }
    return p.priority[NumRegions - 1];
}

float getPriorityOffset (int choiceIndex) noexcept
{
    // "Preset", "Sehr hoch", "Hoch", "Niedrig", "Sehr niedrig"
    static constexpr float offsets[] { 0.0f, 0.5f, 0.25f, -0.25f, -0.5f };
    return offsets[juce::jlimit (0, 4, choiceIndex)];
}
} // namespace mtm
