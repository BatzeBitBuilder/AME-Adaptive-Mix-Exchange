#pragma once

#include <juce_core/juce_core.h>

#include "i18n/Lang.h"   // MTM_TXT, Sprache

namespace mtm
{
enum class Role
{
    Auto = 0,
    Kick,
    Snare,
    Drums,
    Percussion,
    Bass,
    LeadVocal,
    BackingVocal,
    ElectricGuitar,
    AcousticGuitar,
    Keys,
    SynthLead,
    Pad,
    Strings,
    Brass,
    Fx,
    Other,
    NumRoles
};

// Anzeigenamen in der gewünschten Sprache. Parameter (DAW) nutzen immer Englisch.
using i18n::Language;

inline juce::StringArray getRoleNames (Language lang = i18n::current())
{
    if (lang == Language::English)
        return { "Auto-ID", "Kick", "Snare", "Drums/Overheads", "Percussion", "Bass",
                 "Lead Vocal", "Backing Vocals", "Electric Guitar", "Acoustic Guitar",
                 "Keys/Piano", "Synth Lead", "Pad", "Strings", "Brass", "FX", "Other" };

    return { "Auto-Kennung", "Kick", "Snare", "Drums/Overheads", "Percussion", "Bass",
             "Lead Vocal", "Backing Vocals", "E-Gitarre", "Akustikgitarre",
             "Keys/Piano", "Synth Lead", "Pad", "Streicher", MTM_TXT ("Bläser"), "FX", "Sonstiges" };
}

enum class Genre
{
    RockBand = 0,
    PopVocal,
    Electronic,
    HipHop,
    Acoustic,
    Metal,
    NumGenres
};

inline juce::StringArray getGenreNames (Language lang = i18n::current())
{
    if (lang == Language::English)
        return { "Rock / Band", "Pop (Vocal Focus)", "Electronic", "Hip-Hop", "Acoustic / Singer-Songwriter", "Metal" };
    return { "Rock / Band", "Pop (Vocal-Fokus)", "Electronic", "Hip-Hop", "Akustik / Singer-Songwriter", "Metal" };
}

inline juce::StringArray getPriorityNames (Language lang = i18n::current())
{
    if (lang == Language::English)
        return { "Program", "Very high", "High", "Low", "Very low" };
    return { "Programm", "Sehr hoch", "Hoch", "Niedrig", "Sehr niedrig" };
}

enum class InstanceMode
{
    Track = 0,
    Master
};

inline juce::StringArray getModeNames (Language lang = i18n::current())
{
    if (lang == Language::English)
        return { "Line (Track)", "Exchange (Master)" };
    return { "Line (Spur)", "Exchange (Master)" };
}
} // namespace mtm
