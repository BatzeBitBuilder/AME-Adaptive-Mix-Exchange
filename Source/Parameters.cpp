// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "Parameters.h"
#include "Roles.h"

namespace ame
{
namespace
{
using Range = juce::NormalisableRange<float>;

Range freqRange (float lo, float hi, float centre)
{
    Range r { lo, hi };
    r.setSkewForCentre (centre);
    return r;
}

juce::String formatValue (float v, const juce::String& unit)
{
    if (unit == "Hz")
        return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                            : juce::String (juce::roundToInt (v)) + " Hz";
    if (unit == "dB")  return juce::String (v, 1) + " dB";
    if (unit == "%")   return juce::String (juce::roundToInt (v)) + " %";
    if (unit == "ms")  return juce::String (v, v < 10.0f ? 1 : 0) + " ms";
    if (unit == ":1")  return juce::String (v, 1) + ":1";
    if (unit == "pan")
    {
        const int p = juce::roundToInt (v * 100.0f);
        return p == 0 ? juce::String ("C") : (p < 0 ? "L" + juce::String (-p) : "R" + juce::String (p));
    }
    return juce::String (v, 2);
}

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name,
                                                      Range range, float def, const juce::String& unit = {})
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, range, def,
        juce::AudioParameterFloatAttributes()
            .withLabel (unit == "pan" ? juce::String() : unit)
            .withStringFromValueFunction ([unit] (float v, int) { return formatValue (v, unit); })
            .withValueFromStringFunction ([unit] (const juce::String& s)
            {
                if (unit == "pan")
                {
                    const auto t = s.trim().toUpperCase();
                    if (t.startsWith ("L")) return -t.substring (1).getFloatValue() / 100.0f;
                    if (t.startsWith ("R")) return  t.substring (1).getFloatValue() / 100.0f;
                    return t.getFloatValue();
                }
                const float v = s.getFloatValue();
                return (unit == "Hz" && s.containsIgnoreCase ("k")) ? v * 1000.0f : v;
            }));
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id, const juce::String& name, bool def,
                                                    bool automatable = true)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def,
                                                       juce::AudioParameterBoolAttributes().withAutomatable (automatable));
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice (const char* id, const juce::String& name,
                                                        const juce::StringArray& choices, int def,
                                                        bool automatable)
{
    return std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { id, 1 }, name, choices, def,
        juce::AudioParameterChoiceAttributes().withAutomatable (automatable));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    // Instanz / global – Namen in der DAW immer Englisch (IDs unverändert)
    l.add (makeChoice (pid::mode,  "Mode", getModeNames (Language::English), 0, false));
    l.add (makeChoice (pid::role,  "Role", getRoleNames (Language::English), 0, false));
    l.add (makeFloat (pid::amount, "Amount", { 0.0f, 100.0f }, 100.0f, "%"));
    l.add (makeChoice (pid::priority, "Priority", getPriorityNames (Language::English), 0, false));
    l.add (makeFloat (pid::reaction, "Response", { 0.0f, 100.0f }, 30.0f, "%"));
    l.add (makeBool (pid::adaptEq,   "Adaptive EQ",   true));
    l.add (makeBool (pid::adaptHpf,  "Adaptive HPF",  true));
    l.add (makeBool (pid::adaptGain, "Adaptive Gain", true));
    l.add (makeBool (pid::adaptBypass, "Direct", false));
    l.add (makeBool (pid::adaptFreeze, "Hold", false, false));
    l.add (makeChoice (pid::genre, "Program", getGenreNames (Language::English), 0, false));
    l.add (makeFloat (pid::globalAmount, "Global Amount", { 0.0f, 100.0f }, 100.0f, "%"));
    l.add (makeBool (pid::globalBypass, "Direct (all)", false));
    l.add (makeChoice (pid::monitor, "Monitor", { "Full", "No Engine", "Raw" }, 0, true));
    l.add (makeBool (pid::levelMatch, "Level Match", true));

    // Input / Filter
    l.add (makeFloat (pid::inGain,  "Input",   { -24.0f, 24.0f, 0.01f }, 0.0f, "dB"));
    l.add (makeBool  (pid::phaseL,  "Phase L", false));
    l.add (makeBool  (pid::phaseR,  "Phase R", false));
    l.add (makeBool  (pid::hpfOn,   "HPF On",  false));
    l.add (makeFloat (pid::hpfFreq, "HPF",     freqRange (20.0f, 1000.0f, 120.0f), 20.0f, "Hz"));
    l.add (makeBool  (pid::lpfOn,   "LPF On",  false));
    l.add (makeFloat (pid::lpfFreq, "LPF",     freqRange (1000.0f, 20000.0f, 6000.0f), 20000.0f, "Hz"));
    l.add (makeChoice (pid::hpfSlope, "HPF Slope", { "12 dB/oct", "24 dB/oct", "48 dB/oct" }, 1, true));
    l.add (makeChoice (pid::lpfSlope, "LPF Slope", { "12 dB/oct", "24 dB/oct", "48 dB/oct" }, 1, true));

    // EQ
    const Range gainR { -18.0f, 18.0f, 0.01f };
    const Range qR    = [] { Range r { 0.2f, 10.0f }; r.setSkewForCentre (1.0f); return r; }();

    l.add (makeFloat (pid::lsFreq, "Low Freq",   freqRange (30.0f, 500.0f, 120.0f), 100.0f, "Hz"));
    l.add (makeFloat (pid::lsGain, "Low Gain",   gainR, 0.0f, "dB"));
    l.add (makeFloat (pid::p1Freq, "Mid1 Freq",  freqRange (100.0f, 2000.0f, 500.0f), 400.0f, "Hz"));
    l.add (makeFloat (pid::p1Gain, "Mid1 Gain",  gainR, 0.0f, "dB"));
    l.add (makeFloat (pid::p1Q,    "Mid1 Q",     qR, 1.0f));
    l.add (makeFloat (pid::p2Freq, "Mid2 Freq",  freqRange (500.0f, 10000.0f, 2500.0f), 2500.0f, "Hz"));
    l.add (makeFloat (pid::p2Gain, "Mid2 Gain",  gainR, 0.0f, "dB"));
    l.add (makeFloat (pid::p2Q,    "Mid2 Q",     qR, 1.0f));
    l.add (makeFloat (pid::hsFreq, "High Freq",  freqRange (2000.0f, 18000.0f, 8000.0f), 8000.0f, "Hz"));
    l.add (makeFloat (pid::hsGain, "High Gain",  gainR, 0.0f, "dB"));
    l.add (makeChoice (pid::lsType, "Low Type",  { "Shelf", "Bell" }, 0, true));
    l.add (makeFloat  (pid::lsQ,    "Low Q",     qR, 0.707f));
    l.add (makeChoice (pid::hsType, "High Type", { "Shelf", "Bell" }, 0, true));
    l.add (makeFloat  (pid::hsQ,    "High Q",    qR, 0.707f));
    l.add (makeChoice (pid::chainOrder, "Order", { "EQ > Comp", "Comp > EQ" }, 0, true));

    // Kompressor
    l.add (makeBool  (pid::compOn,      "Comp On",   false));
    l.add (makeFloat (pid::compThresh,  "Threshold", { -60.0f, 0.0f, 0.1f }, -18.0f, "dB"));
    l.add (makeFloat (pid::compRatio,   "Ratio",     [] { Range r { 1.0f, 20.0f }; r.setSkewForCentre (4.0f); return r; }(), 3.0f, ":1"));
    l.add (makeFloat (pid::compAttack,  "Attack",    [] { Range r { 0.1f, 100.0f }; r.setSkewForCentre (10.0f); return r; }(), 10.0f, "ms"));
    l.add (makeFloat (pid::compRelease, "Release",   [] { Range r { 10.0f, 1000.0f }; r.setSkewForCentre (150.0f); return r; }(), 120.0f, "ms"));
    l.add (makeFloat (pid::compMakeup,  "Makeup",    { 0.0f, 24.0f, 0.01f }, 0.0f, "dB"));
    l.add (makeFloat (pid::compKnee,    "Knee",      { 0.0f, 18.0f, 0.1f }, 6.0f, "dB"));
    l.add (makeFloat (pid::compMix,     "Comp Mix",  { 0.0f, 100.0f, 0.1f }, 100.0f, "%"));
    l.add (makeFloat (pid::compScHpf,   "Comp SC HPF", freqRange (20.0f, 500.0f, 100.0f), 20.0f, "Hz"));
    l.add (makeBool  (pid::compAutoRel, "Auto Release", false));

    // Gate / Expander
    l.add (makeBool   (pid::gateOn,      "Gate On", false));
    l.add (makeChoice (pid::gateMode,    "Gate Mode", { "Gate", "Expander 1:2", "Expander 1:4" }, 0, true));
    l.add (makeFloat  (pid::gateThresh,  "Gate Threshold", { -80.0f, 0.0f, 0.1f }, -50.0f, "dB"));
    l.add (makeFloat  (pid::gateRange,   "Gate Range", { 0.0f, 80.0f, 0.1f }, 40.0f, "dB"));
    l.add (makeFloat  (pid::gateAttack,  "Gate Attack", [] { Range r { 0.05f, 50.0f }; r.setSkewForCentre (2.0f); return r; }(), 0.5f, "ms"));
    l.add (makeFloat  (pid::gateHold,    "Gate Hold", [] { Range r { 0.0f, 500.0f }; r.setSkewForCentre (50.0f); return r; }(), 30.0f, "ms"));
    l.add (makeFloat  (pid::gateRelease, "Gate Release", [] { Range r { 5.0f, 2000.0f }; r.setSkewForCentre (150.0f); return r; }(), 150.0f, "ms"));
    l.add (makeFloat  (pid::gateScHpf,   "Gate SC HPF", freqRange (20.0f, 2000.0f, 150.0f), 20.0f, "Hz"));

    // Sättigung / Exciter
    l.add (makeBool   (pid::satOn,     "Saturation On", false));
    l.add (makeChoice (pid::satType,   "Saturation Type", { "Tube", "Tape", "Transistor" }, 0, true));
    l.add (makeFloat  (pid::satDrive,  "Drive", { 0.0f, 24.0f, 0.1f }, 6.0f, "dB"));
    l.add (makeFloat  (pid::satMix,    "Saturation Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, "%"));
    l.add (makeFloat  (pid::excAmount, "Exciter", { 0.0f, 100.0f, 0.1f }, 0.0f, "%"));
    l.add (makeFloat  (pid::excFreq,   "Exciter Freq", freqRange (2000.0f, 12000.0f, 5000.0f), 5000.0f, "Hz"));

    // De-Esser
    l.add (makeBool   (pid::deessOn,     "De-Esser On", false));
    l.add (makeChoice (pid::deessMode,   "De-Esser Mode", { "Split", "Wide" }, 0, true));
    l.add (makeFloat  (pid::deessFreq,   "De-Esser Freq", freqRange (2500.0f, 12000.0f, 6000.0f), 6500.0f, "Hz"));
    l.add (makeFloat  (pid::deessThresh, "De-Esser Threshold", { -60.0f, 0.0f, 0.1f }, -30.0f, "dB"));
    l.add (makeFloat  (pid::deessRange,  "De-Esser Range", { 0.0f, 18.0f, 0.1f }, 8.0f, "dB"));
    l.add (makeBool   (pid::deessListen, "De-Esser Listen", false));

    // Output
    l.add (makeFloat (pid::pan,     "Pan",    { -1.0f, 1.0f, 0.01f }, 0.0f, "pan"));
    l.add (makeFloat (pid::width,   "Width",  { 0.0f, 200.0f, 0.1f }, 100.0f, "%"));
    l.add (makeFloat (pid::outGain, "Output", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB"));

    // Limiter / Clipper am Ausgang
    l.add (makeBool   (pid::limOn,      "Limiter On", false));
    l.add (makeChoice (pid::limMode,    "Limiter Mode", { "Limiter", "Soft Clip", "Hard Clip" }, 0, true));
    l.add (makeFloat  (pid::limCeiling, "Ceiling", { -12.0f, 0.0f, 0.1f }, -0.3f, "dB"));
    l.add (makeFloat  (pid::limRelease, "Limiter Release", [] { Range r { 5.0f, 500.0f }; r.setSkewForCentre (60.0f); return r; }(), 60.0f, "ms"));

    return l;
}
} // namespace ame
