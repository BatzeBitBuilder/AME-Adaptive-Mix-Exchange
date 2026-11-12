// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "engine/Profiles.h"

using namespace ame;

AmeProcessor::AmeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MultiTrackMix", createParameterLayout()),   // state type: keep for compatibility with saved projects
      stripParams (apvts)
{
    prm.mode = apvts.getRawParameterValue (pid::mode);
    prm.role = apvts.getRawParameterValue (pid::role);
    prm.amount = apvts.getRawParameterValue (pid::amount);
    prm.priority = apvts.getRawParameterValue (pid::priority);
    prm.reaction = apvts.getRawParameterValue (pid::reaction);
    prm.adaptEq = apvts.getRawParameterValue (pid::adaptEq);
    prm.adaptHpf = apvts.getRawParameterValue (pid::adaptHpf);
    prm.adaptGain = apvts.getRawParameterValue (pid::adaptGain);
    prm.adaptBypass = apvts.getRawParameterValue (pid::adaptBypass);
    prm.adaptFreeze = apvts.getRawParameterValue (pid::adaptFreeze);
    prm.learnMode = apvts.getRawParameterValue (pid::learnMode);
    prm.levelMatch = apvts.getRawParameterValue (pid::levelMatch);

    slot = hub->registerInstance (instanceId);
    lastFreezeCommandGen = hub->freezeCommandGen.load();
    lastLearnCommandGen = hub->learnCommandGen.load();
    engine = std::make_unique<MixEngine> (*hub);
    startTimerHz (10);
}

AmeProcessor::~AmeProcessor()
{
    stopTimer();
    engine.reset();
    hub->unregisterInstance (slot);
}

float AmeProcessor::param (const char* id) const noexcept
{
    return apvts.getRawParameterValue (id)->load();
}

InstanceMode AmeProcessor::getMode() const noexcept
{
    return (InstanceMode) juce::roundToInt (prm.mode->load (std::memory_order_relaxed));
}

bool AmeProcessor::isActiveMaster() const
{
    return getMode() == InstanceMode::Master && hub->getActiveMasterId() == instanceId;
}

juce::String AmeProcessor::getTrackName() const
{
    const juce::SpinLock::ScopedLockType sl (trackLock);
    return trackName;
}

void AmeProcessor::updateTrackProperties (const TrackProperties& properties)
{
    const juce::SpinLock::ScopedLockType sl (trackLock);
    if (properties.name.has_value())   trackName = *properties.name;
    if (properties.colour.has_value()) trackColour = *properties.colour;
}

void AmeProcessor::timerCallback()
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
    // Engine nur, solange der Host das Exchange auch verarbeitet (deaktiviert/Bypass = wie "Ohne Amt").
    // Gerechnet wird im Audio-Thread des Exchange (je 20 ms Audio), hier nur die Überwachung.
    const auto now = juce::Time::getMillisecondCounter();
    const uint32_t audioTimeout = (master && hub->engineOffline.load()) ? 5000u : 300u;   // Export: Blöcke ggf. langsamer
    const bool processing = ! hostBypassed.load() && lastAudioMs.load() != 0 && now - lastAudioMs.load() < audioTimeout;
    const bool enabled = master && processing;
    exchangeSuspended.store (master && ! processing);
    activeMasterFlag.store (master);
    if (engineEnabled && ! enabled)
    {
        hub->engineHeartbeatMs.store (0);   // Lines sofort ohne Engine (nicht erst nach Ablauf des Lebenszeichens)
        hub->engineOffline.store (false);
    }
    engineEnabled = enabled;

    if (master)
    {
        const int mon = juce::roundToInt (param (pid::monitor));
        hub->monitorMode.store (mon);
        hub->globalBypass.store (mon == 1 || param (pid::globalBypass) > 0.5f);
    }

    // Befehl "Alle einfrieren / lösen" vom Master
    const auto gen = hub->freezeCommandGen.load();
    if (gen != lastFreezeCommandGen)
    {
        lastFreezeCommandGen = gen;
        if (getMode() == InstanceMode::Track)
            if (auto* fp = apvts.getParameter (pid::adaptFreeze))
                fp->setValueNotifyingHost (hub->freezeCommandValue.load() ? 1.0f : 0.0f);
    }

    // Befehl "Mitschnitt" vom Exchange
    const auto lgen = hub->learnCommandGen.load();
    if (lgen != lastLearnCommandGen)
    {
        lastLearnCommandGen = lgen;
        if (getMode() == InstanceMode::Track)
        {
            const int cmd = hub->learnCommandValue.load();
            if (cmd == MixHub::learnClear)
                recording.clear();
            else
                setLearnMode (cmd);
        }
    }
    if (juce::roundToInt (param (pid::learnMode)) == 1)
        recording.allocate();   // Speicher erst bei Bedarf (einmalig)

    // Fernbedienung aus der Mix-Übersicht: Direkt / Hold / Vorrang dieser Line
    if (auto* d = hub->getAudioData (slot))
    {
        const auto rgen = d->remoteGen.load();
        if (rgen != lastRemoteGen)
        {
            lastRemoteGen = rgen;
            const int what = d->remoteParam.load();
            const float value = d->remoteValue.load();
            const char* id = what == MixHub::remoteDirect   ? pid::adaptBypass
                           : what == MixHub::remoteHold     ? pid::adaptFreeze
                           : what == MixHub::remotePriority ? pid::priority : nullptr;
            if (id != nullptr && getMode() == InstanceMode::Track)
                if (auto* p = apvts.getParameter (id))
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (p->convertTo0to1 (what == MixHub::remotePriority ? juce::jlimit (0.0f, 4.0f, value)
                                                                                               : (value > 0.5f ? 1.0f : 0.0f)));
                    p->endChangeGesture();
                }
        }
    }

    // Eingefrorene Werte im Projekt speichern
    if (frozenDirty.exchange (false))
        storeFrozenState();
}

void AmeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // großzügig anlegen: manche Hosts liefern größere Blöcke als angekündigt (keine Allokation im Audio-Thread)
    dryBuffer.setSize (2, juce::jmax (4096, samplesPerBlock), false, true, false);
    rawMix = 0.0f;
    matchGain.reset (sampleRate, 0.05);
    matchGain.setCurrentAndTargetValue (1.0f);
    strip.prepare (sampleRate, getTotalNumOutputChannels());
    analyzer.prepare (sampleRate);
    engine->prepare (sampleRate);
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

bool AmeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void AmeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto* data = hub->getAudioData (slot);
    const int numSamples = buffer.getNumSamples();
    lastAudioMs.store (juce::jmax (1u, juce::Time::getMillisecondCounter()), std::memory_order_relaxed);
    hostBypassed.store (false, std::memory_order_relaxed);

    if (getMode() == InstanceMode::Track)
    {
        ChannelStrip::Params p;
        stripParams.read (p);

        // Original für die Roh-Abhöre sichern
        const int nch = juce::jmin (2, buffer.getNumChannels());
        if (dryBuffer.getNumSamples() < numSamples)
            dryBuffer.setSize (2, numSamples, false, true, true);
        float dryPow = 0.0f, inPeak = 0.0f;
        for (int ch = 0; ch < nch; ++ch)
        {
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
            const float* x = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                dryPow += x[i] * x[i];
                inPeak = juce::jmax (inPeak, std::abs (x[i]));
            }
        }
        if (inPeak >= 1.0f && data != nullptr)
            data->clipIn.store (true, std::memory_order_relaxed);   // Eingang >= 0 dBFS (Merker, die Anzeige setzt zurück)

        strip.processPre (buffer, p);

        // Analyse vor den adaptiven Eingriffen (keine Rückkopplung in die Engine);
        // Klangmerkmale nur, solange die Rolle automatisch erkannt wird
        analyzer.setFeaturesEnabled (juce::roundToInt (prm.role->load (std::memory_order_relaxed)) == (int) Role::Auto);
        analyzer.process (buffer, data);

        if (data != nullptr)
        {
            const float reaction = prm.reaction->load (std::memory_order_relaxed) * 0.01f;
            data->reaction.store (reaction, std::memory_order_relaxed);
            const int prioIndex = juce::roundToInt (prm.priority->load (std::memory_order_relaxed));
            data->priorityOffset.store (getPriorityOffset (prioIndex), std::memory_order_relaxed);
            data->priorityIndex.store (prioIndex, std::memory_order_relaxed);
            data->directSwitch.store (prm.adaptBypass->load (std::memory_order_relaxed) > 0.5f, std::memory_order_relaxed);

            const bool alive = hub->isEngineAlive();
            const float amt = alive ? prm.amount->load (std::memory_order_relaxed) * 0.01f * hub->globalAmount.load() : 0.0f;
            const bool eqOn = prm.adaptEq->load (std::memory_order_relaxed) > 0.5f, hpfOn = prm.adaptHpf->load (std::memory_order_relaxed) > 0.5f,
                       gainOn = prm.adaptGain->load (std::memory_order_relaxed) > 0.5f;
            const bool bypass = prm.adaptBypass->load (std::memory_order_relaxed) > 0.5f || hub->globalBypass.load (std::memory_order_relaxed);
            const bool freeze = prm.adaptFreeze->load (std::memory_order_relaxed) > 0.5f;

            const float blockSec = (float) numSamples / (float) getSampleRate();

            // Mitschnitt: Songposition der DAW
            const int learn = juce::roundToInt (prm.learnMode->load (std::memory_order_relaxed));
            double posSec = -1.0;
            bool playing = false;
            if (auto* ph = getPlayHead())
                if (const auto pos = ph->getPosition())
                {
                    playing = pos->getIsPlaying();
                    if (const auto t = pos->getTimeInSeconds())
                        posSec = *t;
                    else if (const auto smp = pos->getTimeInSamples())
                        posSec = (double) *smp / getSampleRate();
                }
            AdaptiveRecording::Values rv;
            const bool replay = learn == 2 && posSec >= 0.0 && recording.read (posSec, rv);
            data->learnMode.store (learn, std::memory_order_relaxed);
            data->learnSec.store (recording.getLengthSec(), std::memory_order_relaxed);

            // Export erkannt (Host-Kennung oder schneller als Echtzeit), aber weder Engine noch Mitschnitt → Warnung
            const bool fastRender = renderSpeed.update (numSamples, getSampleRate()) || isNonRealtime();
            if (fastRender && ! replay && ! bypass && ! freeze && ! alive && prm.amount->load (std::memory_order_relaxed) > 0.0f)
                data->renderWarnMs.store (juce::jmax (1u, juce::Time::getMillisecondCounter()), std::memory_order_relaxed);
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
            if (bypass != lastBypass || freeze != lastFreeze || replay != lastReplay)
            {
                fastBlendSec = 0.3f;
                lastBypass = bypass;
                lastFreeze = freeze;
                lastReplay = replay;
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
                    target = replay ? rv.cutDb[k]
                           : freeze ? frozenCutDb[k].load (std::memory_order_relaxed)
                                    : (eqOn ? data->targetCutDb[k].load (std::memory_order_relaxed) * amt : 0.0f);
                if (replay && ! bypass && ! fast)
                    curCutDb[k] = target;   // Mitschnitt enthält bereits die geglätteten Werte
                else
                {
                    const float c = target < curCutDb[k] ? att : rel;
                    curCutDb[k] = c * curCutDb[k] + (1.0f - c) * target;
                }
                p.adaptiveCutDb[k] = curCutDb[k];
                data->appliedCutDb[k].store (curCutDb[k], std::memory_order_relaxed);
            }

            // HPF und Gain langsam (außer beim Umschalten)
            const float slow = fast ? fastCoef : coef (2.0f);

            float hpfTarget = 0.0f;
            if (! bypass)
            {
                if (replay)
                    hpfTarget = rv.hpfHz;
                else if (freeze)
                    hpfTarget = frozenHpfHz.load (std::memory_order_relaxed);
                else
                {
                    const float hpfT = data->targetHpfHz.load (std::memory_order_relaxed);
                    hpfTarget = (hpfOn && hpfT >= 20.0f) ? 20.0f * std::pow (hpfT / 20.0f, amt) : 0.0f;
                }
            }
            curHpfHz = (replay && ! bypass && ! fast) ? hpfTarget : slow * curHpfHz + (1.0f - slow) * hpfTarget;
            p.adaptiveHpfHz = curHpfHz >= 21.0f ? curHpfHz : 0.0f;
            data->appliedHpfHz.store (p.adaptiveHpfHz, std::memory_order_relaxed);

            float gainTarget = 0.0f;
            if (! bypass)
                gainTarget = replay ? rv.gainDb
                           : freeze ? frozenGainDb.load (std::memory_order_relaxed)
                                    : (gainOn ? data->targetGainDb.load (std::memory_order_relaxed) * amt : 0.0f);
            curGainDb = (replay && ! bypass && ! fast) ? gainTarget : slow * curGainDb + (1.0f - slow) * gainTarget;
            p.adaptiveGainDb = curGainDb;
            data->appliedGainDb.store (curGainDb, std::memory_order_relaxed);

            // Mitschnitt aufnehmen: genau das, was gerade angewendet wird, an der Songposition
            const bool rec = learn == 1 && playing && posSec >= 0.0 && recording.isAllocated();
            if (rec)
            {
                AdaptiveRecording::Values v;
                for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
                    v.cutDb[k] = curCutDb[k];
                v.hpfHz = curHpfHz;
                v.gainDb = curGainDb;
                recording.write (posSec, blockSec, v);
            }
            data->learnActive.store (rec, std::memory_order_relaxed);
        }

        strip.processPost (buffer, p);

        // Leistung bearbeitet / roh (Pegelangleich im Exchange), ~1 s geglättet
        float wetPow = 0.0f;
        for (int ch = 0; ch < nch; ++ch)
        {
            const float* x = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                wetPow += x[i] * x[i];
        }
        const float norm = 1.0f / (float) juce::jmax (1, numSamples * nch);
        const float pc = std::exp (-(float) numSamples / (float) getSampleRate());
        dryPowS = pc * dryPowS + (1.0f - pc) * dryPow * norm;
        wetPowS = pc * wetPowS + (1.0f - pc) * wetPow * norm;
        if (data != nullptr)
        {
            data->dryPow.store (dryPowS, std::memory_order_relaxed);
            data->wetPow.store (wetPowS, std::memory_order_relaxed);
        }

        // Roh-Abhöre des Exchange: auf das Original überblenden (30 ms)
        const float rawTarget = hub->isRawMonitor() ? 1.0f : 0.0f;
        if (rawTarget > 0.0f || rawMix > 0.0f)
        {
            const float step = 1.0f / (0.03f * (float) getSampleRate());
            for (int i = 0; i < numSamples; ++i)
            {
                rawMix = rawTarget > rawMix ? juce::jmin (rawTarget, rawMix + step) : juce::jmax (rawTarget, rawMix - step);
                for (int ch = 0; ch < nch; ++ch)
                {
                    auto* y = buffer.getWritePointer (ch);
                    y[i] += rawMix * (dryBuffer.getSample (ch, i) - y[i]);
                }
            }
        }
    }
    else
    {
        // Master: Durchschleifen, Messung (vor dem Pegelangleich)
        analyzer.setFeaturesEnabled (false);
        analyzer.process (buffer, data);

        // Engine im Takt des Audios: je 20 ms ein Schritt – Echtzeit und Export verhalten sich gleich
        if (activeMasterFlag.load (std::memory_order_relaxed))
        {
            hub->engineOffline.store (isNonRealtime(), std::memory_order_relaxed);
            engine->advance (numSamples);
        }

        // Roh-Abhöre: Lautheitsunterschied zur bearbeiteten Fassung am Ausgang ausgleichen
        const bool raw = activeMasterFlag.load (std::memory_order_relaxed) && hub->isRawMonitor() && prm.levelMatch->load (std::memory_order_relaxed) > 0.5f;
        matchGain.setTargetValue (raw ? juce::Decibels::decibelsToGain (hub->rawMatchDb.load (std::memory_order_relaxed)) : 1.0f);
        if (matchGain.isSmoothing() || std::abs (matchGain.getTargetValue() - 1.0f) > 1.0e-4f)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float g = matchGain.getNextValue();
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.getWritePointer (ch)[i] *= g;
            }
        }
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
    if (peak >= 1.0f)
        data->clipOut.store (true, std::memory_order_relaxed);   // Ausgang >= 0 dBFS
    data->rmsLeftDb.store (juce::Decibels::gainToDecibels (std::sqrt (chSq[0] / (float) n), -100.0f), std::memory_order_relaxed);
    data->rmsRightDb.store (juce::Decibels::gainToDecibels (std::sqrt (chSq[1] / (float) n), -100.0f), std::memory_order_relaxed);

    const float blockMs = (float) sumSq / (float) (n * nch);
    const float blockSec = (float) n / (float) getSampleRate();
    const float a = std::exp (-blockSec / 0.3f);                // ~300 ms RMS-Fenster
    meterRms = a * meterRms + (1.0f - a) * blockMs;
    meterPeak = juce::jmax (peak, meterPeak * std::exp (-blockSec / 0.5f));

    data->rmsDb.store (juce::Decibels::gainToDecibels (std::sqrt (meterRms), -100.0f));
    data->peakDb.store (juce::Decibels::gainToDecibels (meterPeak, -100.0f));
    const bool track = getMode() == InstanceMode::Track;
    data->gainReductionDb.store (track ? strip.getGainReductionDb() : 0.0f);
    data->gateReductionDb.store (track ? strip.getGateReductionDb() : 0.0f, std::memory_order_relaxed);
    data->deessReductionDb.store (track ? strip.getDeessReductionDb() : 0.0f, std::memory_order_relaxed);
    data->limiterReductionDb.store (track ? strip.getLimiterReductionDb() : 0.0f, std::memory_order_relaxed);
    data->blockCounter.fetch_add (1);
}

void AmeProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Host-Bypass: Exchange gilt als deaktiviert (Engine aus), Lines laufen ohne Eingriffe weiter
    hostBypassed.store (true, std::memory_order_relaxed);
    if (activeMasterFlag.load (std::memory_order_relaxed))
        hub->engineHeartbeatMs.store (0, std::memory_order_relaxed);   // Lines sofort ohne Engine
    lastAudioMs.store (juce::jmax (1u, juce::Time::getMillisecondCounter()), std::memory_order_relaxed);
    AudioProcessor::processBlockBypassed (buffer, midi);
}

void AmeProcessor::setMonitor (int mode)
{
    if (auto* prm = apvts.getParameter (pid::monitor))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) juce::jlimit (0, 2, mode)));
        prm->endChangeGesture();
    }
}

void AmeProcessor::setLearnMode (int mode)
{
    if (juce::jlimit (0, 2, mode) == 1)
        recording.allocate();
    if (auto* prm = apvts.getParameter (pid::learnMode))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) juce::jlimit (0, 2, mode)));
        prm->endChangeGesture();
    }
}

juce::AudioProcessorEditor* AmeProcessor::createEditor()
{
    return new AmeEditor (*this);
}

void AmeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        // Mitschnitt (komprimiert) nur im Projekt, nicht im ValueTree (kein Undo, keine A/B-Kopie)
        const auto rec = recording.toBase64();
        if (rec.isNotEmpty())
        {
            auto* e = xml->createNewChildElement ("AdaptiveRecording");
            e->setAttribute ("fps", AdaptiveRecording::kFramesPerSecond);
            e->setAttribute ("data", rec);
        }
        copyXmlToBinary (*xml, destData);
    }
}

void AmeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            if (auto* e = xml->getChildByName ("AdaptiveRecording"))
            {
                recording.fromBase64 (e->getStringAttribute ("data"));
                xml->removeChildElement (e, true);
            }
            else
            {
                recording.clear();
            }

            auto tree = juce::ValueTree::fromXml (*xml);

            // Abhöre nicht aus dem Projekt übernehmen (nie versehentlich "roh" weiterarbeiten)
            for (auto child : tree)
                if (child.hasType ("PARAM"))
                    for (auto* id : { pid::monitor, pid::globalBypass })
                        if (child.getProperty ("id").toString() == id)
                            child.setProperty ("value", 0.0f, nullptr);

            apvts.replaceState (tree);
            loadFrozenState();
        }
}

static const juce::Identifier frozenId ("Frozen");

void AmeProcessor::storeFrozenState()
{
    auto tree = apvts.state.getOrCreateChildWithName (frozenId, nullptr);
    for (int k = 0; k < kNumAdaptiveBands; ++k)
        tree.setProperty ("cut" + juce::String (k), frozenCutDb[(size_t) k].load(), nullptr);
    tree.setProperty ("hpf", frozenHpfHz.load(), nullptr);
    tree.setProperty ("gain", frozenGainDb.load(), nullptr);
}

void AmeProcessor::loadFrozenState()
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
    return new AmeProcessor();
}
