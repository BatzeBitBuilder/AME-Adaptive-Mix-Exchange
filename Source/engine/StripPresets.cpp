// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "StripPresets.h"
#include "Parameters.h"
#include "Settings.h"

namespace ame::presets
{
using namespace pid;

const juce::StringArray& getSoundScope()
{
    static const juce::StringArray s {
        hpfOn, hpfFreq, lpfOn, lpfFreq, hpfSlope, lpfSlope,
        lsFreq, lsGain, lsQ, lsType, p1Freq, p1Gain, p1Q, p2Freq, p2Gain, p2Q, hsFreq, hsGain, hsQ, hsType, chainOrder,
        satOn, satType, satDrive, satMix, excAmount, excFreq,
        gateOn, gateMode, gateThresh, gateRange, gateAttack, gateHold, gateRelease, gateScHpf,
        compOn, compThresh, compRatio, compAttack, compRelease, compMakeup, compKnee, compMix, compScHpf, compAutoRel,
        deessOn, deessMode, deessFreq, deessThresh, deessRange, deessListen };
    return s;
}

const juce::StringArray& getUserScope()
{
    static const juce::StringArray s = []
    {
        juce::StringArray a (getSoundScope());
        a.addArray (juce::StringArray { inGain, phaseL, phaseR, pan, width, outGain, limOn, limMode, limCeiling, limRelease });
        return a;
    }();
    return s;
}

const std::vector<FactoryPreset>& getFactoryPresets()
{
    // Startwerte nach gängiger Mischpraxis; bewusst moderat (Engine greift zusätzlich ein)
    static const std::vector<FactoryPreset> list {
        { Role::Kick, {
            { hpfOn, 1 }, { hpfFreq, 30 },
            { lsFreq, 60 }, { lsGain, 3 }, { p1Freq, 350 }, { p1Gain, -4 }, { p1Q, 1.4f },
            { p2Freq, 3500 }, { p2Gain, 3 }, { p2Q, 1.2f },
            { gateOn, 1 }, { gateThresh, -35 }, { gateRange, 30 }, { gateAttack, 0.2f }, { gateHold, 40 }, { gateRelease, 120 },
            { compOn, 1 }, { compThresh, -18 }, { compRatio, 4 }, { compAttack, 15 }, { compRelease, 80 }, { compKnee, 3 }, { compMakeup, 3 },
            { satOn, 1 }, { satType, 0 }, { satDrive, 4 } } },
        { Role::Snare, {
            { hpfOn, 1 }, { hpfFreq, 80 },
            { lsFreq, 200 }, { lsGain, 2 }, { lsType, 1 }, { lsQ, 1.2f }, { p1Freq, 450 }, { p1Gain, -3 }, { p1Q, 1.5f },
            { p2Freq, 5000 }, { p2Gain, 3 }, { p2Q, 1.0f }, { hsFreq, 10000 }, { hsGain, 2 },
            { gateOn, 1 }, { gateThresh, -35 }, { gateRange, 20 }, { gateAttack, 0.2f }, { gateHold, 60 }, { gateRelease, 150 }, { gateScHpf, 150 },
            { compOn, 1 }, { compThresh, -20 }, { compRatio, 4 }, { compAttack, 8 }, { compRelease, 100 }, { compKnee, 3 }, { compMakeup, 3 },
            { satOn, 1 }, { satType, 2 }, { satDrive, 3 } } },
        { Role::Drums, {
            { hpfOn, 1 }, { hpfFreq, 120 },
            { p1Freq, 400 }, { p1Gain, -2 }, { p1Q, 1.0f }, { hsFreq, 10000 }, { hsGain, 3 },
            { compOn, 1 }, { compThresh, -20 }, { compRatio, 2.5f }, { compAttack, 20 }, { compRelease, 150 }, { compKnee, 6 },
            { compMakeup, 2 }, { compMix, 70 }, { compScHpf, 100 },
            { satOn, 1 }, { satType, 1 }, { satDrive, 4 }, { satMix, 50 } } },
        { Role::Percussion, {
            { hpfOn, 1 }, { hpfFreq, 120 },
            { p1Freq, 500 }, { p1Gain, -2 }, { hsFreq, 10000 }, { hsGain, 2 },
            { compOn, 1 }, { compThresh, -20 }, { compRatio, 3 }, { compAttack, 5 }, { compRelease, 100 }, { compMakeup, 2 } } },
        { Role::Bass, {
            { hpfOn, 1 }, { hpfFreq, 30 },
            { lsFreq, 80 }, { lsGain, 2 }, { p1Freq, 250 }, { p1Gain, -3 }, { p1Q, 1.2f }, { p2Freq, 800 }, { p2Gain, 2 }, { p2Q, 1.0f },
            { compOn, 1 }, { compThresh, -22 }, { compRatio, 4 }, { compAttack, 20 }, { compRelease, 120 }, { compKnee, 6 }, { compMakeup, 4 },
            { satOn, 1 }, { satType, 1 }, { satDrive, 6 }, { satMix, 60 } } },
        { Role::LeadVocal, {
            { hpfOn, 1 }, { hpfFreq, 90 },
            { p1Freq, 300 }, { p1Gain, -2 }, { p1Q, 1.0f }, { p2Freq, 3000 }, { p2Gain, 2 }, { p2Q, 1.0f },
            { hsFreq, 10000 }, { hsGain, 3 },
            { compOn, 1 }, { compThresh, -22 }, { compRatio, 3.5f }, { compAttack, 5 }, { compRelease, 80 }, { compAutoRel, 1 }, { compMakeup, 4 },
            { deessOn, 1 }, { deessFreq, 6500 }, { deessThresh, -30 }, { deessRange, 8 },
            { satOn, 1 }, { satType, 0 }, { satDrive, 3 }, { satMix, 40 }, { excAmount, 15 }, { excFreq, 6000 } } },
        { Role::BackingVocal, {
            { hpfOn, 1 }, { hpfFreq, 150 },
            { p1Freq, 400 }, { p1Gain, -3 }, { p1Q, 1.0f }, { p2Freq, 4000 }, { p2Gain, 1 }, { hsFreq, 10000 }, { hsGain, 2 },
            { compOn, 1 }, { compThresh, -24 }, { compRatio, 4 }, { compAttack, 5 }, { compRelease, 100 }, { compMakeup, 4 },
            { deessOn, 1 }, { deessFreq, 7000 }, { deessThresh, -32 }, { deessRange, 10 } } },
        { Role::ElectricGuitar, {
            { hpfOn, 1 }, { hpfFreq, 90 }, { lpfOn, 1 }, { lpfFreq, 12000 },
            { p1Freq, 400 }, { p1Gain, -2 }, { p1Q, 1.4f }, { p2Freq, 2500 }, { p2Gain, 2 }, { p2Q, 1.0f },
            { compOn, 1 }, { compThresh, -18 }, { compRatio, 2.5f }, { compAttack, 15 }, { compRelease, 120 }, { compMakeup, 2 } } },
        { Role::AcousticGuitar, {
            { hpfOn, 1 }, { hpfFreq, 80 },
            { p1Freq, 200 }, { p1Gain, -3 }, { p1Q, 1.0f }, { p2Freq, 5000 }, { p2Gain, 2 }, { hsFreq, 12000 }, { hsGain, 2 },
            { compOn, 1 }, { compThresh, -22 }, { compRatio, 3 }, { compAttack, 10 }, { compRelease, 120 }, { compMakeup, 3 },
            { satOn, 1 }, { satDrive, 0 }, { excAmount, 15 }, { excFreq, 5000 } } },
        { Role::Keys, {
            { hpfOn, 1 }, { hpfFreq, 60 },
            { p1Freq, 300 }, { p1Gain, -2 }, { hsFreq, 8000 }, { hsGain, 1 },
            { compOn, 1 }, { compThresh, -20 }, { compRatio, 2 }, { compAttack, 20 }, { compRelease, 150 }, { compMakeup, 1 },
            { satOn, 1 }, { satType, 1 }, { satDrive, 3 } } },
        { Role::SynthLead, {
            { hpfOn, 1 }, { hpfFreq, 100 },
            { p2Freq, 3000 }, { p2Gain, 2 },
            { compOn, 1 }, { compThresh, -18 }, { compRatio, 3 }, { compAttack, 10 }, { compRelease, 100 }, { compMakeup, 2 } } },
        { Role::Pad, {
            { hpfOn, 1 }, { hpfFreq, 150 }, { lpfOn, 1 }, { lpfFreq, 12000 },
            { p1Freq, 400 }, { p1Gain, -2 },
            { compOn, 1 }, { compThresh, -24 }, { compRatio, 2 }, { compAttack, 40 }, { compRelease, 300 }, { compMakeup, 2 } } },
        { Role::Strings, {
            { hpfOn, 1 }, { hpfFreq, 80 },
            { p1Freq, 300 }, { p1Gain, -2 }, { hsFreq, 8000 }, { hsGain, 1.5f },
            { compOn, 1 }, { compThresh, -24 }, { compRatio, 2 }, { compAttack, 30 }, { compRelease, 200 }, { compMakeup, 2 } } },
        { Role::Brass, {
            { hpfOn, 1 }, { hpfFreq, 100 },
            { p1Freq, 600 }, { p1Gain, -2 }, { hsFreq, 7000 }, { hsGain, 1 },
            { compOn, 1 }, { compThresh, -20 }, { compRatio, 3 }, { compAttack, 10 }, { compRelease, 120 }, { compMakeup, 2 } } },
        { Role::Fx, {
            { hpfOn, 1 }, { hpfFreq, 200 } } },
        { Role::Other, {
            { hpfOn, 1 }, { hpfFreq, 40 } } },
    };
    return list;
}

const FactoryPreset* findFactoryPreset (Role r)
{
    for (const auto& p : getFactoryPresets())
        if (p.role == r)
            return &p;
    return nullptr;
}

void applyValues (juce::AudioProcessorValueTreeState& apvts, const juce::StringArray& scope,
                  const std::vector<std::pair<juce::String, float>>& values, bool resetOthers)
{
    auto set = [&apvts] (const juce::String& id, float plain, bool isDefault)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            const float norm = isDefault ? p->getDefaultValue() : p->convertTo0to1 (plain);
            p->beginChangeGesture();
            p->setValueNotifyingHost (norm);
            p->endChangeGesture();
        }
    };

    for (const auto& id : scope)
    {
        auto it = std::find_if (values.begin(), values.end(), [&id] (const auto& v) { return v.first == id; });
        if (it != values.end())
            set (id, it->second, false);
        else if (resetOthers)
            set (id, 0.0f, true);
    }
}

void applyFactory (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset)
{
    std::vector<std::pair<juce::String, float>> v;
    for (const auto& x : preset.values)
        v.emplace_back (x.id, x.value);
    applyValues (apvts, getSoundScope(), v, true);
}
//==============================================================================
namespace
{
const juce::Identifier presetTag ("AMEPreset"), paramTag ("Param"), abTag ("ABCompare"), activeId ("active");

std::vector<std::pair<juce::String, float>> capture (juce::AudioProcessorValueTreeState& apvts)
{
    std::vector<std::pair<juce::String, float>> v;
    for (const auto& id : getUserScope())
        if (auto* raw = apvts.getRawParameterValue (id))
            v.emplace_back (id, raw->load());
    return v;
}

juce::ValueTree toTree (const juce::Identifier& type, const std::vector<std::pair<juce::String, float>>& values)
{
    juce::ValueTree t (type);
    for (const auto& [id, value] : values)
        t.setProperty (juce::Identifier (id), value, nullptr);
    return t;
}

std::vector<std::pair<juce::String, float>> fromTree (const juce::ValueTree& t)
{
    std::vector<std::pair<juce::String, float>> v;
    for (int i = 0; i < t.getNumProperties(); ++i)
    {
        const auto name = t.getPropertyName (i);
        v.emplace_back (name.toString(), (float) t.getProperty (name));
    }
    return v;
}
} // namespace

juce::File getUserPresetFolder()
{
    return settings::getUserDataFolder().getChildFile ("Presets");
}

juce::StringArray getUserPresetNames()
{
    juce::StringArray names;
    for (const auto& f : getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.amepreset"))
    {
        juce::String name = f.getFileNameWithoutExtension();
        if (auto xml = juce::XmlDocument::parse (f))
            name = xml->getStringAttribute ("name", name);
        names.add (name);
    }
    names.sortNatural();
    return names;
}

static juce::File fileFor (const juce::String& name)
{
    return getUserPresetFolder().getChildFile (juce::File::createLegalFileName (name.trim()) + ".amepreset");
}

bool saveUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::String& name)
{
    if (name.trim().isEmpty())
        return false;

    juce::XmlElement xml (presetTag.toString());
    xml.setAttribute ("name", name.trim());
    xml.setAttribute ("version", JucePlugin_VersionString);
    for (const auto& [id, value] : capture (apvts))
    {
        auto* e = xml.createNewChildElement (paramTag.toString());
        e->setAttribute ("id", id);
        e->setAttribute ("value", value);
    }

    getUserPresetFolder().createDirectory();
    return xml.writeTo (fileFor (name));
}

bool loadUserPreset (juce::AudioProcessorValueTreeState& apvts, const juce::String& name)
{
    auto xml = juce::XmlDocument::parse (fileFor (name));
    if (xml == nullptr || ! xml->hasTagName (presetTag.toString()))
        return false;

    std::vector<std::pair<juce::String, float>> v;
    for (auto* e : xml->getChildWithTagNameIterator (paramTag.toString()))
        v.emplace_back (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));

    applyValues (apvts, getUserScope(), v, true);   // in älteren Presets fehlende Parameter → Standard
    return true;
}

bool deleteUserPreset (const juce::String& name)
{
    const auto f = fileFor (name);
    return f.existsAsFile() && f.deleteFile();
}

juce::String getActiveAB (const juce::AudioProcessorValueTreeState& apvts)
{
    const auto t = apvts.state.getChildWithName (abTag);
    return t.isValid() && t.getProperty (activeId).toString() == "B" ? "B" : "A";
}

void selectAB (juce::AudioProcessorValueTreeState& apvts, const juce::String& slot)
{
    const auto current = getActiveAB (apvts);
    if (slot == current)
        return;

    auto ab = apvts.state.getOrCreateChildWithName (abTag, nullptr);

    // aktuelle Einstellungen im bisherigen Platz sichern
    const juce::Identifier curId (current), nextId (slot);
    ab.removeChild (ab.getChildWithName (curId), nullptr);
    ab.appendChild (toTree (curId, capture (apvts)), nullptr);

    // Zielplatz laden (leer = bisherige Einstellungen übernehmen)
    const auto target = ab.getChildWithName (nextId);
    if (target.isValid())
        applyValues (apvts, getUserScope(), fromTree (target), false);

    ab.setProperty (activeId, slot, nullptr);
}

void copyActiveToOther (juce::AudioProcessorValueTreeState& apvts)
{
    auto ab = apvts.state.getOrCreateChildWithName (abTag, nullptr);
    const juce::Identifier other (getActiveAB (apvts) == "A" ? "B" : "A");
    ab.removeChild (ab.getChildWithName (other), nullptr);
    ab.appendChild (toTree (other, capture (apvts)), nullptr);
}
} // namespace ame::presets
