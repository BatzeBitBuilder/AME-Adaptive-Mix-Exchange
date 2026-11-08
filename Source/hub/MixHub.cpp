// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "MixHub.h"

namespace mtm
{
void SlotAudioData::reset() noexcept
{
    rmsDb.store (-100.0f);
    peakDb.store (-100.0f);
    gainReductionDb.store (0.0f);
    for (auto& b : bandDb)             b.store (-100.0f);
    for (auto& b : bandAvgDb)          b.store (-100.0f);
    for (auto& d : displayDb)          d.store (-100.0f);
    rmsLeftDb.store (-100.0f);
    rmsRightDb.store (-100.0f);
    analysisCounter.store (0);
    blockCounter.store (0);
    priorityOffset.store (0.0f);
    reaction.store (0.3f);
    for (auto& v : targetCutDb)  v.store (0.0f);
    for (auto& v : appliedCutDb) v.store (0.0f);
    targetHpfHz.store (0.0f);  targetGainDb.store (0.0f);
    appliedHpfHz.store (0.0f); appliedGainDb.store (0.0f);
    frozen.store (false); bypassed.store (false);
    for (auto* f : { &fCentroid, &fFlatness, &fSub, &fLow, &fHigh, &fOnsetRate, &fHarmonicity, &fPitch, &fPitchVar, &fCrest, &fWidth, &fActiveSec })
        f->store (0.0f);
}

MixHub::MixHub()
{
    for (auto& a : audio)
        a.reset();
}

MixHub::~MixHub() = default;

int MixHub::registerInstance (uint32_t& outInstanceId)
{
    const juce::ScopedLock sl (lock);

    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (! infos[(size_t) i].used)
        {
            auto& info = infos[(size_t) i];
            info = {};
            info.used = true;
            info.instanceId = nextInstanceId++;
            info.name = "Line " + juce::String (info.instanceId).paddedLeft ('0', 2);
            audio[(size_t) i].reset();
            outInstanceId = info.instanceId;
            return i;
        }
    }

    outInstanceId = 0;
    return -1;
}

void MixHub::unregisterInstance (int slot)
{
    if (! juce::isPositiveAndBelow (slot, kMaxSlots))
        return;

    const juce::ScopedLock sl (lock);
    infos[(size_t) slot] = {};
    audio[(size_t) slot].reset();
}

void MixHub::updateInfo (int slot, const juce::String& name, juce::Colour colour, Role role, InstanceMode mode,
                         bool autoRole, float autoConfidence, bool autoFromName)
{
    if (! juce::isPositiveAndBelow (slot, kMaxSlots))
        return;

    const juce::ScopedLock sl (lock);
    auto& info = infos[(size_t) slot];
    if (! info.used)
        return;

    if (name.isNotEmpty())
        info.name = name;

    info.colour = colour;
    info.role = role;
    info.mode = mode;
    info.autoRole = autoRole;
    info.autoConfidence = autoConfidence;
    info.autoFromName = autoFromName;
}

std::vector<TrackSnapshot> MixHub::getSnapshot() const
{
    std::vector<TrackSnapshot> result;
    const juce::ScopedLock sl (lock);

    for (int i = 0; i < kMaxSlots; ++i)
    {
        const auto& info = infos[(size_t) i];
        if (! info.used)
            continue;

        const auto& a = audio[(size_t) i];
        TrackSnapshot s;
        s.slot = i;
        s.instanceId = info.instanceId;
        s.name = info.name;
        s.colour = info.colour;
        s.role = info.role;
        s.mode = info.mode;
        s.autoRole = info.autoRole;
        s.autoConfidence = info.autoConfidence;
        s.autoFromName = info.autoFromName;
        s.rmsDb = a.rmsDb.load();
        s.peakDb = a.peakDb.load();
        s.grDb = a.gainReductionDb.load();
        for (size_t b = 0; b < (size_t) kNumAnalysisBands; ++b)
        {
            s.bandDb[b] = a.bandDb[b].load (std::memory_order_relaxed);
            s.bandAvgDb[b] = a.bandAvgDb[b].load (std::memory_order_relaxed);
        }
        s.hasAnalysis = a.analysisCounter.load (std::memory_order_acquire) > 0;
        for (size_t d = 0; d < (size_t) kNumDisplayPoints; ++d)
            s.displayDb[d] = a.displayDb[d].load (std::memory_order_relaxed);
        s.rmsLeftDb = a.rmsLeftDb.load();
        s.rmsRightDb = a.rmsRightDb.load();
        for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
            s.appliedCutDb[k] = a.appliedCutDb[k].load (std::memory_order_relaxed);
        s.appliedHpfHz = a.appliedHpfHz.load();
        s.appliedGainDb = a.appliedGainDb.load();
        s.priorityOffset = a.priorityOffset.load();
        s.reaction = a.reaction.load();
        s.frozen = a.frozen.load();
        s.bypassed = a.bypassed.load();
        result.push_back (std::move (s));
    }

    std::sort (result.begin(), result.end(),
               [] (const auto& a, const auto& b) { return a.instanceId < b.instanceId; });
    return result;
}

int MixHub::getNumInstances() const
{
    const juce::ScopedLock sl (lock);
    return (int) std::count_if (infos.begin(), infos.end(), [] (const auto& i) { return i.used; });
}

int MixHub::getNumMasters() const
{
    const juce::ScopedLock sl (lock);
    return (int) std::count_if (infos.begin(), infos.end(),
                                [] (const auto& i) { return i.used && i.mode == InstanceMode::Master; });
}

uint32_t MixHub::getActiveMasterId() const
{
    const juce::ScopedLock sl (lock);
    uint32_t best = 0;

    for (const auto& i : infos)
        if (i.used && i.mode == InstanceMode::Master && (best == 0 || i.instanceId < best))
            best = i.instanceId;

    return best;
}

SlotAudioData* MixHub::getAudioData (int slot) noexcept
{
    return juce::isPositiveAndBelow (slot, kMaxSlots) ? &audio[(size_t) slot] : nullptr;
}
} // namespace mtm
