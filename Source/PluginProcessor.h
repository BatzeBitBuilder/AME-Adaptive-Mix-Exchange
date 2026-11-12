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
#include "engine/Recording.h"

class AmeProcessor : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    AmeProcessor();
    ~AmeProcessor() override;

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
    ame::MixHub& getHub() noexcept { return *hub; }
    int getSlot() const noexcept { return slot; }
    uint32_t getInstanceId() const noexcept { return instanceId; }
    ame::InstanceMode getMode() const noexcept;
    bool isActiveMaster() const;
    // Engine rechnet (aktives Exchange, vom Host verarbeitet); getaktet vom Audio des Exchange
    bool isEngineRunning() const noexcept { return activeMasterFlag.load() && ! exchangeSuspended.load(); }
    uint64_t getEngineSteps() const noexcept { return engine->getStepCount(); }
    // Exchange ist Master, wird vom Host aber nicht verarbeitet (deaktiviert / Bypass)
    bool isExchangeSuspended() const noexcept { return exchangeSuspended.load(); }
    void setMonitor (int mode);   // Abhöre 0/1/2 (Message-Thread, mit Geste)
    juce::String getTrackName() const;
    ame::RoleGuess getAutoRole() const { return autoGuess; }   // nur Message-Thread

    // Mitschnitt (Lernlauf)
    ame::AdaptiveRecording& getRecording() noexcept { return recording; }
    void setLearnMode (int mode);   // 0 = Live, 1 = Aufnahme, 2 = Wiedergabe (Message-Thread, mit Geste)

private:
    void timerCallback() override;
    float param (const char* id) const noexcept;

    // Parameter des Audio-Threads: Zeiger einmal holen statt je Block per Namen suchen
    struct AudioParams
    {
        std::atomic<float> *mode = nullptr, *role = nullptr, *amount = nullptr, *priority = nullptr, *reaction = nullptr,
                           *adaptEq = nullptr, *adaptHpf = nullptr, *adaptGain = nullptr, *adaptBypass = nullptr,
                           *adaptFreeze = nullptr, *learnMode = nullptr, *levelMatch = nullptr;
    } prm;

    juce::SharedResourcePointer<ame::MixHub> hub;
    int slot = -1;
    uint32_t instanceId = 0;

    ame::StripParamReader stripParams;   // nach apvts deklariert
    ame::ChannelStrip strip;
    ame::SpectrumAnalyzer analyzer;
    std::unique_ptr<ame::MixEngine> engine;

    // Geglättete adaptive Werte (Audio-Thread)
    std::array<float, ame::kNumAdaptiveBands> curCutDb {};
    float curHpfHz = 0.0f, curGainDb = 0.0f;

    // Bypass / Einfrieren
    std::array<std::atomic<float>, ame::kNumAdaptiveBands> frozenCutDb {};
    std::atomic<float> frozenHpfHz { 0.0f }, frozenGainDb { 0.0f };
    std::atomic<bool> frozenValid { false }, frozenDirty { false };
    bool lastBypass = false, lastFreeze = false;
    float fastBlendSec = 0.0f;
    uint32_t lastFreezeCommandGen = 0;

    ame::RoleDetector roleDetector;
    ame::RoleGuess autoGuess;   // Message-Thread

    // Mitschnitt
    ame::AdaptiveRecording recording;
    uint32_t lastLearnCommandGen = 0;
    bool lastReplay = false;

    // Export erkennen, auch ohne Offline-Kennung des Hosts: Audio läuft deutlich schneller als Echtzeit
    struct RenderSpeed
    {
        bool update (int numSamples, double sampleRate) noexcept
        {
            const double now = juce::Time::getMillisecondCounterHiRes();
            if (lastMs <= 0.0 || now - lastMs > 250.0) { winStart = now; winAudioSec = 0.0; }
            lastMs = now;
            winAudioSec += (double) numSamples / sampleRate;
            if (winAudioSec >= 0.5)
            {
                fast = winAudioSec * 1000.0 > (now - winStart) * 1.5;
                winStart = now;
                winAudioSec = 0.0;
            }
            return fast;
        }
        double lastMs = 0.0, winStart = 0.0, winAudioSec = 0.0;
        bool fast = false;
    } renderSpeed;
    bool engineEnabled = false;   // Message-Thread

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmeProcessor)
};
