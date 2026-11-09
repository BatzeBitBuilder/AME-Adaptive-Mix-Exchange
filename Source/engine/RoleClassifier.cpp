// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "RoleClassifier.h"

#include <cmath>
#include <initializer_list>

namespace ame
{
namespace
{
struct NameRule
{
    Role role;
    std::initializer_list<const char*> keys;   // <= 3 Zeichen oder "=..": exaktes Token, sonst Teilstring eines Tokens
    std::initializer_list<const char*> phrases; // Wortfolgen
};

// Reihenfolge = Vorrang (z. B. "bass drum" vor "bass", "lead vox" vor "lead")
const NameRule nameRules[] {
    { Role::Kick,           { "kick", "kik", "bd", "kd", "bassdrum" }, { "bass drum" } },
    { Role::Snare,          { "snare", "snr", "sd", "clap", "rim" }, {} },
    { Role::Percussion,     { "perc", "shaker", "conga", "bongo", "tamb", "cajon", "clave", "cowbell", "timbale", "guiro" }, {} },
    { Role::Drums,          { "drum", "overhead", "oh", "room", "kit", "hat", "hats", "hh", "hihat", "cymbal", "=ride", "crash", "tom", "toms" }, {} },
    { Role::BackingVocal,   { "bgv", "bv", "bvs", "backing", "backs", "choir", "=chor", "harmony", "harm", "double", "dbl", "adlib" }, {} },
    { Role::LeadVocal,      { "vox", "vocal", "voc", "gesang", "stimme", "voice", "singer", "rap", "lv" }, {} },
    { Role::AcousticGuitar, { "acoustic", "akustik", "agtr", "ag", "nylon", "western" }, { "ac gtr", "a gtr", "ak git" } },
    { Role::ElectricGuitar, { "gtr", "guitar", "gitarre", "git", "egtr", "eg", "elec", "dist", "crunch", "riff" }, {} },
    { Role::Bass,           { "bass", "bs", "808", "subbass", "kontrabass", "upright" }, { "sub bass" } },
    { Role::Keys,           { "piano", "keys", "key", "rhodes", "wurli", "organ", "orgel", "klavier", "epiano", "ep", "clav", "keyboard", "pno" }, {} },
    { Role::Pad,            { "pad", "atmo", "ambient", "drone", "texture", "flaeche" }, {} },
    { Role::Strings,        { "string", "streicher", "violin", "geige", "cello", "viola", "orch", "str" }, {} },
    { Role::Brass,          { "brass", "blaeser", "trumpet", "trompete", "tpt", "sax", "trombone", "posaune", "horn" }, {} },
    { Role::SynthLead,      { "synth", "syn", "lead", "pluck", "arp", "saw", "seq" }, {} },
    { Role::Fx,             { "fx", "sfx", "riser", "impact", "sweep", "noise", "whoosh", "reverse", "uplifter", "downlifter", "foley" }, {} },
};

juce::String normaliseName (const juce::String& name)
{
    auto s = name.toLowerCase()
                 .replace (AME_TXT ("ä"), "ae").replace (AME_TXT ("ö"), "oe")
                 .replace (AME_TXT ("ü"), "ue").replace (AME_TXT ("ß"), "ss");

    // Nicht-alphanumerisch -> Leerzeichen, Grenzen Buchstabe/Ziffer trennen ("Kick1" -> "kick 1"),
    // ausser bei "808"
    juce::String out;
    juce::juce_wchar prev = 0;
    for (auto c : s)
    {
        const bool alnum = juce::CharacterFunctions::isLetterOrDigit (c);
        if (! alnum) { out << ' '; prev = 0; continue; }
        if (prev != 0 && juce::CharacterFunctions::isDigit (c) != juce::CharacterFunctions::isDigit (prev))
            out << ' ';
        out << juce::String::charToString (c);
        prev = c;
    }
    return " " + out.trim().replace ("  ", " ") + " ";
}

float gauss (float x, float c, float s)
{
    const float d = (x - c) / s;
    return std::exp (-0.5f * d * d);
}

enum Feature { C, FL, SUB, LOW, HIGH, ON, HA, P, PV, CR, W };

struct Criterion { Feature f; float centre, sigma, weight; };

struct Prototype
{
    Role role;
    std::initializer_list<Criterion> criteria;
    float scale = 1.0f;
};

// Zentren: C/P als log2(Hz) (100 Hz = 6.6, 300 Hz = 8.2, 1 kHz = 10, 3 kHz = 11.6, 6 kHz = 12.6)
const Prototype prototypes[] {
    { Role::Kick,           { { C, 7.0f, 1.2f, 2.0f }, { SUB, 0.5f, 0.3f, 1.5f }, { LOW, 0.8f, 0.25f, 1.5f }, { ON, 2.0f, 1.5f, 1.5f }, { CR, 13.0f, 4.0f, 2.0f }, { HA, 0.5f, 0.4f, 0.3f } } },
    { Role::Snare,          { { C, 11.2f, 1.0f, 2.0f }, { FL, 0.3f, 0.2f, 1.5f }, { LOW, 0.05f, 0.1f, 1.0f }, { ON, 1.5f, 1.0f, 1.5f }, { CR, 14.0f, 4.0f, 1.0f }, { HA, 0.25f, 0.25f, 1.0f } } },
    { Role::Drums,          { { C, 12.8f, 1.0f, 2.0f }, { FL, 0.45f, 0.2f, 1.5f }, { HIGH, 0.75f, 0.25f, 1.5f }, { ON, 5.0f, 3.0f, 1.0f }, { W, 0.5f, 0.4f, 0.5f }, { HA, 0.15f, 0.2f, 1.0f } } },
    { Role::Percussion,     { { C, 11.8f, 1.0f, 1.5f }, { FL, 0.35f, 0.2f, 1.0f }, { ON, 5.0f, 3.0f, 1.5f }, { CR, 14.0f, 5.0f, 1.0f }, { HA, 0.2f, 0.2f, 1.0f }, { LOW, 0.05f, 0.1f, 0.5f } }, 0.95f },
    { Role::Bass,           { { C, 7.3f, 1.0f, 2.0f }, { LOW, 0.7f, 0.25f, 2.0f }, { HA, 0.85f, 0.2f, 1.5f }, { CR, 7.0f, 3.0f, 1.5f }, { P, 6.3f, 1.0f, 1.0f }, { ON, 1.0f, 1.5f, 0.5f }, { HIGH, 0.01f, 0.05f, 0.5f } } },
    { Role::LeadVocal,      { { C, 10.2f, 0.8f, 1.5f }, { HA, 0.7f, 0.2f, 1.5f }, { P, 8.0f, 1.0f, 1.5f }, { PV, 20.0f, 12.0f, 2.0f }, { LOW, 0.05f, 0.08f, 1.0f }, { W, 0.1f, 0.2f, 1.0f }, { ON, 1.5f, 1.5f, 0.5f }, { FL, 0.1f, 0.1f, 0.5f } } },
    { Role::BackingVocal,   { { C, 10.2f, 0.8f, 1.5f }, { HA, 0.65f, 0.2f, 1.5f }, { P, 8.0f, 1.0f, 1.0f }, { PV, 20.0f, 12.0f, 1.5f }, { LOW, 0.05f, 0.08f, 1.0f }, { W, 0.5f, 0.3f, 1.5f }, { FL, 0.1f, 0.1f, 0.5f } }, 0.9f },
    { Role::ElectricGuitar, { { C, 10.0f, 0.8f, 1.5f }, { HA, 0.55f, 0.25f, 1.0f }, { CR, 9.0f, 3.0f, 1.5f }, { FL, 0.15f, 0.1f, 0.5f }, { LOW, 0.1f, 0.1f, 0.5f }, { P, 7.8f, 1.2f, 0.5f } } },
    { Role::AcousticGuitar, { { C, 10.5f, 0.8f, 1.5f }, { HA, 0.6f, 0.2f, 1.0f }, { CR, 15.0f, 4.0f, 1.0f }, { ON, 3.0f, 2.0f, 1.0f }, { LOW, 0.08f, 0.1f, 0.5f } } },
    { Role::Keys,           { { C, 9.3f, 1.0f, 1.5f }, { HA, 0.75f, 0.2f, 1.0f }, { PV, 2.0f, 6.0f, 1.0f }, { LOW, 0.2f, 0.15f, 1.0f }, { CR, 13.0f, 4.0f, 0.5f }, { W, 0.3f, 0.3f, 0.5f }, { ON, 2.0f, 2.0f, 0.5f } } },
    { Role::SynthLead,      { { C, 10.5f, 1.0f, 1.0f }, { HA, 0.85f, 0.15f, 1.5f }, { PV, 2.0f, 5.0f, 1.5f }, { CR, 8.0f, 3.0f, 1.5f }, { FL, 0.05f, 0.08f, 0.5f }, { ON, 2.0f, 2.0f, 0.5f } } },
    { Role::Pad,            { { ON, 0.3f, 0.5f, 2.0f }, { PV, 2.0f, 6.0f, 0.5f }, { CR, 7.0f, 3.0f, 1.5f }, { W, 0.6f, 0.4f, 1.0f }, { HA, 0.6f, 0.3f, 0.5f }, { C, 9.5f, 1.2f, 0.5f } } },
    { Role::Strings,        { { ON, 0.5f, 0.7f, 1.5f }, { PV, 12.0f, 10.0f, 0.5f }, { CR, 8.0f, 3.0f, 1.0f }, { HA, 0.75f, 0.2f, 1.0f }, { C, 9.8f, 1.0f, 1.0f }, { W, 0.4f, 0.4f, 0.5f } }, 0.95f },
    { Role::Brass,          { { C, 10.2f, 0.8f, 1.0f }, { HA, 0.8f, 0.15f, 1.5f }, { CR, 10.0f, 3.0f, 1.0f }, { ON, 1.5f, 1.5f, 0.5f } }, 0.9f },
    { Role::Fx,             { { FL, 0.5f, 0.2f, 1.5f }, { ON, 0.5f, 1.0f, 1.0f }, { HA, 0.2f, 0.3f, 1.0f }, { C, 11.0f, 2.0f, 0.5f } }, 0.9f },
};

constexpr float otherBaseline = 0.35f;
constexpr float softmaxK = 10.0f;
} // namespace

RoleGuess classifyByName (const juce::String& trackName)
{
    RoleGuess g;
    const auto n = normaliseName (trackName);
    if (n.trim().isEmpty())
        return g;

    juce::StringArray tokens;
    tokens.addTokens (n.trim(), " ", {});

    for (const auto& rule : nameRules)
    {
        for (auto* phrase : rule.phrases)
            if (n.contains (" " + juce::String (phrase) + " "))
                return { rule.role, 1.0f, true, {} };

        for (auto* key : rule.keys)
        {
            juce::String k (key);
            const bool exact = k.startsWithChar ('=') || k.length() <= 3;
            k = k.trimCharactersAtStart ("=");
            for (const auto& t : tokens)
            {
                const bool match = exact ? t == k : t.contains (k);
                if (match)
                    return { rule.role, 1.0f, true, {} };
            }
        }
    }
    return g;
}

RoleGuess classifyByFeatures (const RoleFeatures& f)
{
    RoleGuess g;
    if (f.activeSec < kMinAnalysisSec)
        return g;

    auto value = [&f] (Feature id) -> float
    {
        switch (id)
        {
            case C:    return std::log2 (juce::jmax (20.0f, f.centroidHz));
            case FL:   return f.flatness;
            case SUB:  return f.subRatio;
            case LOW:  return f.lowRatio;
            case HIGH: return f.highRatio;
            case ON:   return f.onsetRate;
            case HA:   return f.harmonicity;
            case P:    return f.pitchHz > 0.0f ? std::log2 (f.pitchHz) : 0.0f;
            case PV:   return f.pitchVarCents;
            case CR:   return f.crestDb;
            case W:    return f.width;
        }
        return 0.0f;
    };

    std::array<float, kNumRoles> scores {};
    scores.fill (-1.0f);
    scores[(size_t) Role::Other] = otherBaseline;

    for (const auto& proto : prototypes)
    {
        float sum = 0.0f, wsum = 0.0f;
        for (const auto& c : proto.criteria)
        {
            if ((c.f == P || c.f == PV) && (f.harmonicity < 0.5f || f.pitchHz <= 0.0f))
                continue;   // Grundton/Vibrato nur bei tonalem Material
            sum += c.weight * gauss (value (c.f), c.centre, c.sigma);
            wsum += c.weight;
        }
        scores[(size_t) proto.role] = wsum > 0.0f ? proto.scale * sum / wsum : 0.0f;
    }

    // Softmax -> Wahrscheinlichkeiten
    float total = 0.0f;
    for (int r = 1; r < kNumRoles; ++r)
    {
        const float e = scores[(size_t) r] < 0.0f ? 0.0f : std::exp (softmaxK * scores[(size_t) r]);
        g.probabilities[(size_t) r] = e;
        total += e;
    }

    for (int r = 1; r < kNumRoles; ++r)
        g.probabilities[(size_t) r] /= juce::jmax (1.0e-9f, total);

    int best = (int) Role::Other;
    for (int r = 1; r < kNumRoles; ++r)
        if (g.probabilities[(size_t) r] > g.probabilities[(size_t) best])
            best = r;

    g.role = (Role) best;
    g.confidence = g.probabilities[(size_t) best];
    return g;
}

RoleGuess RoleDetector::update (const juce::String& trackName, const RoleFeatures& f)
{
    // 1. Spurname
    const auto byName = classifyByName (trackName);
    if (byName.role != Role::Auto)
    {
        current = byName;
        return current;
    }

    if (current.fromName)   // Name passt nicht mehr -> neu bewerten
        current = {};

    // 2. Klangmerkmale mit Hysterese
    const auto g = classifyByFeatures (f);
    if (g.role == Role::Auto)
        return current;

    if (current.role == Role::Auto)
    {
        current = g;
    }
    else if (g.role != current.role)
    {
        const float currentProb = g.probabilities[(size_t) current.role];
        if (g.confidence > currentProb + 0.15f)
            current = g;
        else
        {
            current.confidence = currentProb;
            current.probabilities = g.probabilities;
        }
    }
    else
    {
        current = g;
    }

    // Unsicher -> "Sonstiges"
    if (current.confidence < 0.25f)
    {
        RoleGuess other = current;
        other.role = Role::Other;
        return other;
    }
    return current;
}
} // namespace ame
