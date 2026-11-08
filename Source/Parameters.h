#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace mtm::pid
{
// Instanz
inline constexpr const char* mode   = "mode";
inline constexpr const char* role   = "role";
inline constexpr const char* amount = "amount";      // Eingriffsstärke dieser Spur

// Adaptiv (Spur)
inline constexpr const char* priority  = "priority";
inline constexpr const char* reaction  = "reaction";
inline constexpr const char* adaptEq   = "adaptEq";
inline constexpr const char* adaptHpf  = "adaptHpf";
inline constexpr const char* adaptGain = "adaptGain";
inline constexpr const char* adaptBypass = "adaptBypass";   // A/B: adaptive Eingriffe aus
inline constexpr const char* adaptFreeze = "adaptFreeze";   // aktuelle Werte festhalten

// Master (global)
inline constexpr const char* genre        = "genre";
inline constexpr const char* globalAmount = "globalAmount";
inline constexpr const char* globalBypass = "globalBypass";

// Strip
inline constexpr const char* inGain   = "inGain";
inline constexpr const char* hpfOn    = "hpfOn";
inline constexpr const char* hpfFreq  = "hpfFreq";
inline constexpr const char* lpfOn    = "lpfOn";
inline constexpr const char* lpfFreq  = "lpfFreq";

inline constexpr const char* lsFreq   = "lsFreq";
inline constexpr const char* lsGain   = "lsGain";
inline constexpr const char* p1Freq   = "p1Freq";
inline constexpr const char* p1Gain   = "p1Gain";
inline constexpr const char* p1Q      = "p1Q";
inline constexpr const char* p2Freq   = "p2Freq";
inline constexpr const char* p2Gain   = "p2Gain";
inline constexpr const char* p2Q      = "p2Q";
inline constexpr const char* hsFreq   = "hsFreq";
inline constexpr const char* hsGain   = "hsGain";

inline constexpr const char* compOn      = "compOn";
inline constexpr const char* compThresh  = "compThresh";
inline constexpr const char* compRatio   = "compRatio";
inline constexpr const char* compAttack  = "compAttack";
inline constexpr const char* compRelease = "compRelease";
inline constexpr const char* compMakeup  = "compMakeup";

inline constexpr const char* pan     = "pan";
inline constexpr const char* width   = "width";
inline constexpr const char* outGain = "outGain";
} // namespace mtm::pid

namespace mtm
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
