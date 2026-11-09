// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "Roles.h"
#include "hub/MixHub.h"
#include "dsp/ChannelStrip.h"
#include "dsp/StripParams.h"
#include "dsp/SpectrumAnalyzer.h"
#include "engine/MixEngine.h"
#include "engine/RoleClassifier.h"

class MultiTrackMixProcessor : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    MultiTrackMixProcessor();
    ~MultiTrackMixProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void updateTrackProperties (const TrackProperties& properties) override;

    // Für den Editor
    juce::AudioProcessorValueTreeState apvts;
    mtm::MixHub& getHub() noexcept { return *hub; }
    int getSlot() const noexcept { return slot; }
    uint32_t getInstanceId() const noexcept { return instanceId; }
    mtm::InstanceMode getMode() const noexcept;
    bool isActiveMaster() const;
    bool isEngineRunning() const { return engine != nullptr && engine->isActive(); }
    // Exchange ist Master, wird vom Host aber nicht verarbeitet (deaktiviert / Bypass)
    bool isExchangeSuspended() const noexcept { return exchangeSuspended.load(); }
    void setMonitor (int mode);   // Abhöre 0/1/2 (Message-Thread, mit Geste)
    juce::String getTrackName() const;
    mtm::RoleGuess getAutoRole() const { return autoGuess; }   // nur Message-Thread

private:
    void timerCallback() override;
    float param (const char* id) const noexcept;

    juce::SharedResourcePointer<mtm::MixHub> hub;
    int slot = -1;
    uint32_t instanceId = 0;

    mtm::StripParamReader stripParams;   // nach apvts deklariert
    mtm::ChannelStrip strip;
    mtm::SpectrumAnalyzer analyzer;
    std::unique_ptr<mtm::MixEngine> engine;

    // Geglättete adaptive Werte (Audio-Thread)
    std::array<float, mtm::kNumAdaptiveBands> curCutDb {};
    float curHpfHz = 0.0f, curGainDb = 0.0f;

    // Bypass / Einfrieren
    std::array<std::atomic<float>, mtm::kNumAdaptiveBands> frozenCutDb {};
    std::atomic<float> frozenHpfHz { 0.0f }, frozenGainDb { 0.0f };
    std::atomic<bool> frozenValid { false }, frozenDirty { false };
    bool lastBypass = false, lastFreeze = false;
    float fastBlendSec = 0.0f;
    uint32_t lastFreezeCommandGen = 0;

    mtm::RoleDetector roleDetector;
    mtm::RoleGuess autoGuess;   // Message-Thread

    void storeFrozenState();
    void loadFrozenState();

    mutable juce::SpinLock trackLock;
    juce::String trackName;
    juce::Colour trackColour { juce::Colours::transparentBlack };   // transparent = Palette im UI

    float meterRms = 0.0f;   // Mittelwert der Quadrate, geglättet

    // Exchange: Audio-Lebenszeichen, Host-Bypass
    std::atomic<uint32_t> lastAudioMs { 0 };
    std::atomic<bool> hostBypassed { false }, exchangeSuspended { false }, activeMasterFlag { false };

    // Roh-Abhöre (Line): unbearbeitetes Signal, Überblendung, Leistungen
    juce::AudioBuffer<float> dryBuffer;
    float rawMix = 0.0f, dryPowS = 0.0f, wetPowS = 0.0f;
    juce::SmoothedValue<float> matchGain { 1.0f };   // Exchange: Pegelangleich
    float meterPeak = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiTrackMixProcessor)
};
