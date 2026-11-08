// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "StripParams.h"
#include "Parameters.h"

namespace mtm
{
namespace
{
// Reihenfolge = Index in ptrs
enum Idx
{
    iInGain, iPhaseL, iPhaseR, iHpfOn, iHpfFreq, iLpfOn, iLpfFreq,
    iLsFreq, iLsGain, iP1Freq, iP1Gain, iP1Q, iP2Freq, iP2Gain, iP2Q, iHsFreq, iHsGain,
    iCompOn, iCompThresh, iCompRatio, iCompAttack, iCompRelease, iCompMakeup, iCompKnee, iCompMix, iCompScHpf, iCompAutoRel,
    iGateOn, iGateMode, iGateThresh, iGateRange, iGateAttack, iGateHold, iGateRelease, iGateScHpf,
    iDeessOn, iDeessMode, iDeessFreq, iDeessThresh, iDeessRange, iDeessListen,
    iPan, iWidth, iOutGain,
    iCount
};

const char* const ids[iCount] {
    pid::inGain, pid::phaseL, pid::phaseR, pid::hpfOn, pid::hpfFreq, pid::lpfOn, pid::lpfFreq,
    pid::lsFreq, pid::lsGain, pid::p1Freq, pid::p1Gain, pid::p1Q, pid::p2Freq, pid::p2Gain, pid::p2Q, pid::hsFreq, pid::hsGain,
    pid::compOn, pid::compThresh, pid::compRatio, pid::compAttack, pid::compRelease, pid::compMakeup, pid::compKnee, pid::compMix,
    pid::compScHpf, pid::compAutoRel,
    pid::gateOn, pid::gateMode, pid::gateThresh, pid::gateRange, pid::gateAttack, pid::gateHold, pid::gateRelease, pid::gateScHpf,
    pid::deessOn, pid::deessMode, pid::deessFreq, pid::deessThresh, pid::deessRange, pid::deessListen,
    pid::pan, pid::width, pid::outGain
};
} // namespace

StripParamReader::StripParamReader (juce::AudioProcessorValueTreeState& apvts)
{
    ptrs.reserve (iCount);
    for (auto* id : ids)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        ptrs.push_back (p);
    }
}

void StripParamReader::read (ChannelStrip::Params& p) const noexcept
{
    p.inGainDb = get (iInGain);
    p.phaseL = on (iPhaseL);  p.phaseR = on (iPhaseR);
    p.hpfOn = on (iHpfOn);  p.hpfFreq = get (iHpfFreq);
    p.lpfOn = on (iLpfOn);  p.lpfFreq = get (iLpfFreq);

    p.eqFreq   = { get (iLsFreq), get (iP1Freq), get (iP2Freq), get (iHsFreq) };
    p.eqGainDb = { get (iLsGain), get (iP1Gain), get (iP2Gain), get (iHsGain) };
    p.eqQ      = { 0.707f, get (iP1Q), get (iP2Q), 0.707f };

    p.compOn = on (iCompOn);
    p.compThreshDb = get (iCompThresh);
    p.compRatio = get (iCompRatio);
    p.compAttackMs = get (iCompAttack);
    p.compReleaseMs = get (iCompRelease);
    p.compMakeupDb = get (iCompMakeup);
    p.compKneeDb = get (iCompKnee);
    p.compMix = get (iCompMix) * 0.01f;
    p.compScHpfHz = get (iCompScHpf);
    p.compAutoRelease = on (iCompAutoRel);

    p.gateOn = on (iGateOn);
    p.gateMode = juce::roundToInt (get (iGateMode));
    p.gateThreshDb = get (iGateThresh);
    p.gateRangeDb = get (iGateRange);
    p.gateAttackMs = get (iGateAttack);
    p.gateHoldMs = get (iGateHold);
    p.gateReleaseMs = get (iGateRelease);
    p.gateScHpfHz = get (iGateScHpf);

    p.deessOn = on (iDeessOn);
    p.deessWide = juce::roundToInt (get (iDeessMode)) == 1;
    p.deessFreq = get (iDeessFreq);
    p.deessThreshDb = get (iDeessThresh);
    p.deessRangeDb = get (iDeessRange);
    p.deessListen = on (iDeessListen);

    p.pan = get (iPan);
    p.width = get (iWidth) * 0.01f;
    p.outGainDb = get (iOutGain);
}
} // namespace mtm
