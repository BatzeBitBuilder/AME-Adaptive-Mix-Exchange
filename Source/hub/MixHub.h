#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <vector>

#include "Roles.h"

namespace mtm
{
constexpr int kMaxSlots         = 128;
constexpr int kNumAnalysisBands = 32;
constexpr int kNumAdaptiveBands = 10;   // Oktavbänder 31 Hz – 16 kHz

inline float getAdaptiveBandHz (int k) noexcept { return 31.25f * (float) (1 << k); }

// Hochauflösende Anzeige: 240 log. Punkte 20 Hz – 20 kHz, je ~1/12 Oktave geglättet
constexpr int kNumDisplayPoints = 240;
inline float getDisplayPointHz (int i) noexcept
{
    return 20.0f * std::pow (1000.0f, (float) i / (float) (kNumDisplayPoints - 1));
}

// Klangmerkmale für die Rollenerkennung (Langzeit-Mittel über aktive Frames)
struct RoleFeatures
{
    float centroidHz = 0.0f;   // spektraler Schwerpunkt
    float flatness = 0.0f;     // 0 = tonal, 1 = Rauschen
    float subRatio = 0.0f;     // Anteil < 60 Hz
    float lowRatio = 0.0f;     // Anteil < 150 Hz
    float highRatio = 0.0f;    // Anteil > 4 kHz
    float onsetRate = 0.0f;    // Anschläge pro Sekunde
    float harmonicity = 0.0f;  // 0..1 (Autokorrelation)
    float pitchHz = 0.0f;      // Grundton-Schätzung
    float pitchVarCents = 0.0f; // Tonhöhen-Schwankung innerhalb von Tönen (Vibrato)
    float crestDb = 0.0f;      // Spitze / RMS je Frame
    float width = 0.0f;        // Seite / Mitte (0 = mono)
    float activeSec = 0.0f;    // ausgewertete Zeit mit Signal
};

// Echtzeitdaten einer Instanz. Geschrieben vom Audio-Thread der Instanz,
// gelesen von allen anderen (lock-free).
struct SlotAudioData
{
    std::atomic<float> rmsDb { -100.0f };
    std::atomic<float> peakDb { -100.0f };
    std::atomic<float> gainReductionDb { 0.0f };
    std::array<std::atomic<float>, kNumAnalysisBands> bandDb {};       // Kurzzeit-Spektrum (Anzeige, adaptiv)
    std::array<std::atomic<float>, kNumAnalysisBands> bandAvgDb {};    // Langzeit-Spektrum (~3 s, Masking)
    std::atomic<uint32_t> analysisCounter { 0 };
    std::array<std::atomic<float>, kNumDisplayPoints> displayDb {};    // Anzeige (fein, schnell)
    std::atomic<float> rmsLeftDb { -100.0f }, rmsRightDb { -100.0f }; // für VU L/R
    std::atomic<uint32_t> blockCounter { 0 };

    // Spur-Einstellungen (für die Engine)
    std::atomic<float> priorityOffset { 0.0f };
    std::atomic<float> reaction { 0.3f };          // 0 = langsam (Balance), 1 = schnell (Ducking)

    // Engine -> Spur: Zielwerte (ohne Amount)
    std::array<std::atomic<float>, kNumAdaptiveBands> targetCutDb {};
    std::atomic<float> targetHpfHz { 0.0f };       // 0 = aus
    std::atomic<float> targetGainDb { 0.0f };

    // Spur -> UI: tatsächlich angewendet (geglättet, inkl. Amount/Schalter)
    std::array<std::atomic<float>, kNumAdaptiveBands> appliedCutDb {};
    std::atomic<float> appliedHpfHz { 0.0f };
    std::atomic<float> appliedGainDb { 0.0f };
    std::atomic<bool> frozen { false }, bypassed { false };

    // Klangmerkmale (SpectrumAnalyzer)
    std::atomic<float> fCentroid { 0 }, fFlatness { 0 }, fSub { 0 }, fLow { 0 }, fHigh { 0 }, fOnsetRate { 0 },
                       fHarmonicity { 0 }, fPitch { 0 }, fPitchVar { 0 }, fCrest { 0 }, fWidth { 0 }, fActiveSec { 0 };

    RoleFeatures loadFeatures() const noexcept
    {
        RoleFeatures f;
        f.centroidHz = fCentroid.load(); f.flatness = fFlatness.load(); f.subRatio = fSub.load();
        f.lowRatio = fLow.load(); f.highRatio = fHigh.load(); f.onsetRate = fOnsetRate.load();
        f.harmonicity = fHarmonicity.load(); f.pitchHz = fPitch.load(); f.pitchVarCents = fPitchVar.load();
        f.crestDb = fCrest.load();
        f.width = fWidth.load(); f.activeSec = fActiveSec.load();
        return f;
    }

    void reset() noexcept;
};

struct TrackSnapshot
{
    int slot = -1;
    uint32_t instanceId = 0;
    juce::String name;
    juce::Colour colour;
    Role role = Role::Auto;
    InstanceMode mode = InstanceMode::Track;
    float rmsDb = -100.0f, peakDb = -100.0f, grDb = 0.0f;
    std::array<float, kNumAnalysisBands> bandDb {};
    std::array<float, kNumAnalysisBands> bandAvgDb {};
    std::array<float, kNumDisplayPoints> displayDb {};
    float rmsLeftDb = -100.0f, rmsRightDb = -100.0f;
    bool hasAnalysis = false;

    std::array<float, kNumAdaptiveBands> appliedCutDb {};
    float appliedHpfHz = 0.0f, appliedGainDb = 0.0f;
    float priorityOffset = 0.0f, reaction = 0.3f;
    bool frozen = false, bypassed = false;
    bool autoRole = false;          // Rolle automatisch erkannt
    float autoConfidence = 0.0f;
    bool autoFromName = false;
};

// Prozessweites Register aller Plugin-Instanzen (über juce::SharedResourcePointer geteilt).
// Fallback über Shared Memory für getrennte Prozesse folgt in Schritt 5.
class MixHub
{
public:
    MixHub();
    ~MixHub();

    // Message-Thread
    int  registerInstance (uint32_t& outInstanceId);   // -1 = kein Slot frei
    void unregisterInstance (int slot);
    void updateInfo (int slot, const juce::String& name, juce::Colour colour, Role role, InstanceMode mode,
                     bool autoRole = false, float autoConfidence = 0.0f, bool autoFromName = false);
    std::vector<TrackSnapshot> getSnapshot() const;
    int  getNumInstances() const;
    int  getNumMasters() const;
    uint32_t getActiveMasterId() const;  // 0 = kein Master; bei mehreren gilt der älteste

    // Echtzeit-sicher
    SlotAudioData* getAudioData (int slot) noexcept;

    // Globale Einstellungen, geschrieben vom aktiven Master
    std::atomic<int>   globalGenre  { 0 };
    std::atomic<float> globalAmount { 1.0f };
    std::atomic<bool>  globalBypass { false };

    // Befehl "Alle einfrieren / lösen" vom Master: Spuren reagieren, wenn sich die Generation ändert
    std::atomic<uint32_t> freezeCommandGen { 0 };
    std::atomic<bool>     freezeCommandValue { false };
    void sendFreezeCommand (bool freeze) noexcept
    {
        freezeCommandValue.store (freeze);
        freezeCommandGen.fetch_add (1);
    }

    // Lebenszeichen der Engine (Millisekunden-Zähler); veraltet = keine adaptiven Eingriffe
    std::atomic<uint32_t> engineHeartbeatMs { 0 };
    bool isEngineAlive() const noexcept
    {
        const auto hb = engineHeartbeatMs.load (std::memory_order_relaxed);
        return hb != 0 && juce::Time::getMillisecondCounter() - hb < 500;
    }

private:
    struct SlotInfo
    {
        bool used = false;
        uint32_t instanceId = 0;
        juce::String name;
        juce::Colour colour;
        Role role = Role::Auto;
        InstanceMode mode = InstanceMode::Track;
        bool autoRole = false, autoFromName = false;
        float autoConfidence = 0.0f;
    };

    mutable juce::CriticalSection lock;
    std::array<SlotInfo, kMaxSlots> infos;
    std::array<SlotAudioData, kMaxSlots> audio;
    uint32_t nextInstanceId = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixHub)
};
} // namespace mtm
