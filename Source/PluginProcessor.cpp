#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "engine/Profiles.h"

using namespace mtm;

MultiTrackMixProcessor::MultiTrackMixProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MultiTrackMix", createParameterLayout())
{
    slot = hub->registerInstance (instanceId);
    lastFreezeCommandGen = hub->freezeCommandGen.load();
    engine = std::make_unique<MixEngine> (*hub);
    startTimerHz (10);
}

MultiTrackMixProcessor::~MultiTrackMixProcessor()
{
    stopTimer();
    engine.reset();
    hub->unregisterInstance (slot);
}

float MultiTrackMixProcessor::param (const char* id) const noexcept
{
    return apvts.getRawParameterValue (id)->load();
}

InstanceMode MultiTrackMixProcessor::getMode() const noexcept
{
    return (InstanceMode) juce::roundToInt (param (pid::mode));
}

bool MultiTrackMixProcessor::isActiveMaster() const
{
    return getMode() == InstanceMode::Master && hub->getActiveMasterId() == instanceId;
}

juce::String MultiTrackMixProcessor::getTrackName() const
{
    const juce::SpinLock::ScopedLockType sl (trackLock);
    return trackName;
}

void MultiTrackMixProcessor::updateTrackProperties (const TrackProperties& properties)
{
    const juce::SpinLock::ScopedLockType sl (trackLock);
    if (properties.name.has_value())   trackName = *properties.name;
    if (properties.colour.has_value()) trackColour = *properties.colour;
}

void MultiTrackMixProcessor::timerCallback()
{
    juce::String name;
    juce::Colour colour;
    {
        const juce::SpinLock::ScopedLockType sl (trackLock);
        name = trackName;
        colour = trackColour;
    }

    // Rolle: manuell oder automatisch (Spurname, sonst Klangmerkmale)
    const auto manualRole = (Role) juce::roundToInt (param (pid::role));
    Role effectiveRole = manualRole;
    bool isAuto = false;

    if (manualRole == Role::Auto && getMode() == InstanceMode::Track)
    {
        RoleFeatures feats;
        if (auto* d = hub->getAudioData (slot))
            feats = d->loadFeatures();
        autoGuess = roleDetector.update (name, feats);
        effectiveRole = autoGuess.role;
        isAuto = true;
    }
    else
    {
        autoGuess = {};
    }

    hub->updateInfo (slot, name, colour, effectiveRole, getMode(),
                     isAuto, autoGuess.confidence, autoGuess.fromName);

    const bool master = isActiveMaster();
    if (master)
    {
        hub->globalGenre.store (juce::roundToInt (param (pid::genre)));
        hub->globalAmount.store (param (pid::globalAmount) * 0.01f);
    }
    engine->setActive (master);

    if (master)
        hub->globalBypass.store (param (pid::globalBypass) > 0.5f);

    // Befehl "Alle einfrieren / lösen" vom Master
    const auto gen = hub->freezeCommandGen.load();
    if (gen != lastFreezeCommandGen)
    {
        lastFreezeCommandGen = gen;
        if (getMode() == InstanceMode::Track)
            if (auto* fp = apvts.getParameter (pid::adaptFreeze))
                fp->setValueNotifyingHost (hub->freezeCommandValue.load() ? 1.0f : 0.0f);
    }

    // Eingefrorene Werte im Projekt speichern
    if (frozenDirty.exchange (false))
        storeFrozenState();
}

void MultiTrackMixProcessor::prepareToPlay (double sampleRate, int)
{
    strip.prepare (sampleRate, getTotalNumOutputChannels());
    analyzer.prepare (sampleRate);
    meterRms = meterPeak = 0.0f;
    curCutDb.fill (0.0f);
    curHpfHz = curGainDb = 0.0f;

    if (frozenValid.load())   // eingefroren: direkt mit gespeicherten Werten starten
    {
        for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
            curCutDb[k] = frozenCutDb[k].load();
        curHpfHz = frozenHpfHz.load();
        curGainDb = frozenGainDb.load();
    }
}

bool MultiTrackMixProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void MultiTrackMixProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto* data = hub->getAudioData (slot);
    const int numSamples = buffer.getNumSamples();

    if (getMode() == InstanceMode::Track)
    {
        ChannelStrip::Params p;
        p.inGainDb = param (pid::inGain);
        p.hpfOn = param (pid::hpfOn) > 0.5f;  p.hpfFreq = param (pid::hpfFreq);
        p.lpfOn = param (pid::lpfOn) > 0.5f;  p.lpfFreq = param (pid::lpfFreq);

        p.eqFreq   = { param (pid::lsFreq), param (pid::p1Freq), param (pid::p2Freq), param (pid::hsFreq) };
        p.eqGainDb = { param (pid::lsGain), param (pid::p1Gain), param (pid::p2Gain), param (pid::hsGain) };
        p.eqQ      = { 0.707f, param (pid::p1Q), param (pid::p2Q), 0.707f };

        p.compOn        = param (pid::compOn) > 0.5f;
        p.compThreshDb  = param (pid::compThresh);
        p.compRatio     = param (pid::compRatio);
        p.compAttackMs  = param (pid::compAttack);
        p.compReleaseMs = param (pid::compRelease);
        p.compMakeupDb  = param (pid::compMakeup);

        p.pan       = param (pid::pan);
        p.width     = param (pid::width) * 0.01f;
        p.outGainDb = param (pid::outGain);

        strip.processPre (buffer, p);

        // Analyse vor den adaptiven Eingriffen (keine Rückkopplung in die Engine)
        analyzer.process (buffer, data);

        if (data != nullptr)
        {
            const float reaction = param (pid::reaction) * 0.01f;
            data->reaction.store (reaction, std::memory_order_relaxed);
            data->priorityOffset.store (getPriorityOffset (juce::roundToInt (param (pid::priority))), std::memory_order_relaxed);

            const bool alive = hub->isEngineAlive();
            const float amt = alive ? param (pid::amount) * 0.01f * hub->globalAmount.load() : 0.0f;
            const bool eqOn = param (pid::adaptEq) > 0.5f, hpfOn = param (pid::adaptHpf) > 0.5f,
                       gainOn = param (pid::adaptGain) > 0.5f;
            const bool bypass = param (pid::adaptBypass) > 0.5f || hub->globalBypass.load (std::memory_order_relaxed);
            const bool freeze = param (pid::adaptFreeze) > 0.5f;

            const float blockSec = (float) numSamples / (float) getSampleRate();
            auto coef = [blockSec] (float tau) { return std::exp (-blockSec / tau); };

            // Einfrieren: aktuelle Werte übernehmen (einmalig beim Einschalten)
            if (freeze && ! frozenValid.load())
            {
                for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
                    frozenCutDb[k].store (curCutDb[k]);
                frozenHpfHz.store (curHpfHz);
                frozenGainDb.store (curGainDb);
                frozenValid.store (true);
                frozenDirty.store (true);
            }
            else if (! freeze && frozenValid.load())
            {
                frozenValid.store (false);
            }

            // A/B-Umschaltung schnell überblenden
            if (bypass != lastBypass || freeze != lastFreeze)
            {
                fastBlendSec = 0.3f;
                lastBypass = bypass;
                lastFreeze = freeze;
            }
            const bool fast = fastBlendSec > 0.0f;
            fastBlendSec -= blockSec;
            const float fastCoef = coef (0.03f);

            data->frozen.store (freeze, std::memory_order_relaxed);
            data->bypassed.store (bypass, std::memory_order_relaxed);

            // EQ: Eingriff (tiefer) mit Attack, Rücknahme mit Release – beide abhängig von der Reaktion
            const float att = fast ? fastCoef : coef (juce::jmap (reaction, 1.5f, 0.02f));
            const float rel = fast ? fastCoef : coef (juce::jmap (reaction, 3.0f, 0.15f));
            for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
            {
                float target = 0.0f;
                if (! bypass)
                    target = freeze ? frozenCutDb[k].load (std::memory_order_relaxed)
                                    : (eqOn ? data->targetCutDb[k].load (std::memory_order_relaxed) * amt : 0.0f);
                const float c = target < curCutDb[k] ? att : rel;
                curCutDb[k] = c * curCutDb[k] + (1.0f - c) * target;
                p.adaptiveCutDb[k] = curCutDb[k];
                data->appliedCutDb[k].store (curCutDb[k], std::memory_order_relaxed);
            }

            // HPF und Gain langsam (außer beim Umschalten)
            const float slow = fast ? fastCoef : coef (2.0f);

            float hpfTarget = 0.0f;
            if (! bypass)
            {
                if (freeze)
                    hpfTarget = frozenHpfHz.load (std::memory_order_relaxed);
                else
                {
                    const float hpfT = data->targetHpfHz.load (std::memory_order_relaxed);
                    hpfTarget = (hpfOn && hpfT >= 20.0f) ? 20.0f * std::pow (hpfT / 20.0f, amt) : 0.0f;
                }
            }
            curHpfHz = slow * curHpfHz + (1.0f - slow) * hpfTarget;
            p.adaptiveHpfHz = curHpfHz >= 21.0f ? curHpfHz : 0.0f;
            data->appliedHpfHz.store (p.adaptiveHpfHz, std::memory_order_relaxed);

            float gainTarget = 0.0f;
            if (! bypass)
                gainTarget = freeze ? frozenGainDb.load (std::memory_order_relaxed)
                                    : (gainOn ? data->targetGainDb.load (std::memory_order_relaxed) * amt : 0.0f);
            curGainDb = slow * curGainDb + (1.0f - slow) * gainTarget;
            p.adaptiveGainDb = curGainDb;
            data->appliedGainDb.store (curGainDb, std::memory_order_relaxed);
        }

        strip.processPost (buffer, p);
    }
    else
    {
        // Master: Durchschleifen, nur Messung
        analyzer.process (buffer, data);
    }

    // Metering (Post-Fader)
    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n == 0 || nch == 0 || data == nullptr)
        return;

    float sumSq = 0.0f, peak = 0.0f;
    float chSq[2] {};
    for (int ch = 0; ch < nch; ++ch)
    {
        const float* x = buffer.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
        {
            sumSq += x[i] * x[i];
            chSq[juce::jmin (ch, 1)] += x[i] * x[i];
            peak = juce::jmax (peak, std::abs (x[i]));
        }
    }
    if (nch == 1) chSq[1] = chSq[0];
    data->rmsLeftDb.store (juce::Decibels::gainToDecibels (std::sqrt (chSq[0] / (float) n), -100.0f), std::memory_order_relaxed);
    data->rmsRightDb.store (juce::Decibels::gainToDecibels (std::sqrt (chSq[1] / (float) n), -100.0f), std::memory_order_relaxed);

    const float blockMs = (float) sumSq / (float) (n * nch);
    const float blockSec = (float) n / (float) getSampleRate();
    const float a = std::exp (-blockSec / 0.3f);                // ~300 ms RMS-Fenster
    meterRms = a * meterRms + (1.0f - a) * blockMs;
    meterPeak = juce::jmax (peak, meterPeak * std::exp (-blockSec / 0.5f));

    data->rmsDb.store (juce::Decibels::gainToDecibels (std::sqrt (meterRms), -100.0f));
    data->peakDb.store (juce::Decibels::gainToDecibels (meterPeak, -100.0f));
    data->gainReductionDb.store (getMode() == InstanceMode::Track ? strip.getGainReductionDb() : 0.0f);
    data->blockCounter.fetch_add (1);
}

juce::AudioProcessorEditor* MultiTrackMixProcessor::createEditor()
{
    return new MultiTrackMixEditor (*this);
}

void MultiTrackMixProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MultiTrackMixProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            loadFrozenState();
        }
}

static const juce::Identifier frozenId ("Frozen");

void MultiTrackMixProcessor::storeFrozenState()
{
    auto tree = apvts.state.getOrCreateChildWithName (frozenId, nullptr);
    for (int k = 0; k < kNumAdaptiveBands; ++k)
        tree.setProperty ("cut" + juce::String (k), frozenCutDb[(size_t) k].load(), nullptr);
    tree.setProperty ("hpf", frozenHpfHz.load(), nullptr);
    tree.setProperty ("gain", frozenGainDb.load(), nullptr);
}

void MultiTrackMixProcessor::loadFrozenState()
{
    const auto tree = apvts.state.getChildWithName (frozenId);
    const bool freeze = param (pid::adaptFreeze) > 0.5f;

    if (tree.isValid() && freeze)
    {
        for (int k = 0; k < kNumAdaptiveBands; ++k)
            frozenCutDb[(size_t) k].store ((float) tree.getProperty ("cut" + juce::String (k), 0.0f));
        frozenHpfHz.store ((float) tree.getProperty ("hpf", 0.0f));
        frozenGainDb.store ((float) tree.getProperty ("gain", 0.0f));
        frozenValid.store (true);

        // sofort mit den gespeicherten Werten starten
        lastFreeze = true;
        for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
            curCutDb[k] = frozenCutDb[k].load();
        curHpfHz = frozenHpfHz.load();
        curGainDb = frozenGainDb.load();
    }
    else
    {
        frozenValid.store (false);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultiTrackMixProcessor();
}
