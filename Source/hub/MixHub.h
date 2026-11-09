// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "Roles.h"

namespace ame
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
    std::atomic<float> gateReductionDb { 0.0f }, deessReductionDb { 0.0f }, limiterReductionDb { 0.0f };
    std::atomic<float> dryPow { 0.0f }, wetPow { 0.0f };   // Leistung Eingang / Ausgang (geglättet, für Pegelangleich)
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

    // Registry-Kopie für die Engine (lock-frei lesbar im Audio-Thread): -1 = Slot frei
    std::atomic<int> engMode { -1 }, engRole { 0 };

    // Mitschnitt (Lernlauf): 0 = Live, 1 = Aufnahme, 2 = Wiedergabe; aufgezeichnete Länge; nimmt gerade auf
    std::atomic<int> learnMode { 0 };
    std::atomic<float> learnSec { 0.0f };
    std::atomic<bool> learnActive { false };
    std::atomic<uint32_t> renderWarnMs { 0 };   // Zeitpunkt (ms-Zähler): schneller Export ohne Engine-Daten

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

// Anmeldedaten einer Instanz (im gemeinsamen Speicher: nur feste Größen, keine juce::String)
struct HubSlotInfo
{
    bool used = false;
    uint32_t instanceId = 0;
    uint32_t ownerPid = 0;          // Prozess der Instanz (Aufräumen nach Absturz)
    char name[128] {};              // UTF-8
    uint32_t colourArgb = 0;
    int32_t role = 0, mode = 0;
    bool autoRole = false, autoFromName = false;
    float autoConfidence = 0.0f;
};

// Gesamter Hub-Zustand. Liegt im Named Shared Memory (prozessübergreifend) oder – falls das
// nicht möglich ist – im Prozess. Nur lock-freie Atomics und POD, damit er zwischen Prozessen teilbar ist.
struct HubShared
{
    std::atomic<int>      globalGenre { 0 };
    std::atomic<float>    globalAmount { 1.0f };
    std::atomic<bool>     globalBypass { false };
    std::atomic<uint32_t> freezeCommandGen { 0 };
    std::atomic<bool>     freezeCommandValue { false };
    std::atomic<uint32_t> engineHeartbeatMs { 0 };
    std::atomic<int>      monitorMode { 0 };     // Abhöre: 0 = Voll, 1 = Ohne Amt, 2 = Roh
    std::atomic<float>    rawMatchDb { 0.0f };   // Summe bearbeitet minus roh (dB), von der Engine
    std::atomic<bool>     engineOffline { false };   // Exchange rendert offline (Export) → großzügigere Lebenszeichen
    std::atomic<uint32_t> learnCommandGen { 0 };     // Befehl "Mitschnitt" vom Exchange an alle Lines
    std::atomic<int>      learnCommandValue { 0 };   // 0 = Live, 1 = Aufnahme, 2 = Wiedergabe, 3 = Löschen

    uint32_t nextInstanceId = 1;    // nur unter Hub-Sperre
    std::array<HubSlotInfo, kMaxSlots> infos {};
    std::array<SlotAudioData, kMaxSlots> audio;
};

static_assert (std::atomic<float>::is_always_lock_free && std::atomic<uint32_t>::is_always_lock_free
                && std::atomic<int>::is_always_lock_free && std::atomic<bool>::is_always_lock_free,
               "Hub-Atomics müssen lock-frei sein (Shared Memory)");

// Register aller Plugin-Instanzen. Pro Prozess ein Objekt (juce::SharedResourcePointer); die Daten liegen im
// Named Shared Memory der DAW-Sitzung, damit sich auch Instanzen in getrennten Prozessen sehen
// (Plugin-Sandbox, Bridge). Sitzung = DAW-Prozess (Hilfsprozesse werden bis zur DAW zurückverfolgt),
// überschreibbar mit der Umgebungsvariable AME_HUB_SESSION. Fallback: nur im eigenen Prozess.
class MixHub
{
public:
    class Connection;   // Shared Memory / lokaler Fallback (MixHub.cpp)

private:
    std::unique_ptr<Connection> conn;
    HubShared& shared;

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

    // Verbindung
    bool isShared() const noexcept;                 // true = Shared Memory (prozessübergreifend)
    juce::String getSessionKey() const;             // z. B. PID der DAW
    int  getNumProcesses() const;                   // Prozesse mit angemeldeten Instanzen

    // Echtzeit-sicher
    SlotAudioData* getAudioData (int slot) noexcept;

    // Globale Einstellungen, geschrieben vom aktiven Master
    std::atomic<int>&   globalGenre  { shared.globalGenre };
    std::atomic<float>& globalAmount { shared.globalAmount };
    std::atomic<bool>&  globalBypass { shared.globalBypass };

    // Befehl "Alle einfrieren / lösen" vom Master: Spuren reagieren, wenn sich die Generation ändert
    std::atomic<uint32_t>& freezeCommandGen   { shared.freezeCommandGen };
    std::atomic<bool>&     freezeCommandValue { shared.freezeCommandValue };
    void sendFreezeCommand (bool freeze) noexcept
    {
        freezeCommandValue.store (freeze);
        freezeCommandGen.fetch_add (1);
    }

    // Lebenszeichen der Engine (Millisekunden-Zähler, systemweit); veraltet = keine adaptiven Eingriffe
    std::atomic<uint32_t>& engineHeartbeatMs { shared.engineHeartbeatMs };
    std::atomic<bool>&     engineOffline     { shared.engineOffline };

    // Befehl "Mitschnitt" vom Master an alle Lines (wie Hold: Lines reagieren auf die neue Generation)
    enum LearnCommand { learnLive = 0, learnRecord = 1, learnPlay = 2, learnClear = 3 };
    std::atomic<uint32_t>& learnCommandGen   { shared.learnCommandGen };
    std::atomic<int>&      learnCommandValue { shared.learnCommandValue };
    void sendLearnCommand (int cmd) noexcept
    {
        learnCommandValue.store (cmd);
        learnCommandGen.fetch_add (1);
    }

    // Abhöre des Exchange (gilt nur, solange die Engine läuft) und Pegelunterschied roh/bearbeitet
    std::atomic<int>&   monitorMode { shared.monitorMode };
    std::atomic<float>& rawMatchDb  { shared.rawMatchDb };
    bool isRawMonitor() const noexcept { return monitorMode.load (std::memory_order_relaxed) == 2 && isEngineAlive(); }
    bool isEngineAlive() const noexcept
    {
        const auto hb = engineHeartbeatMs.load (std::memory_order_relaxed);
        // Offline-Export: Blöcke können langsamer als Echtzeit kommen → längere Frist
        const uint32_t limit = engineOffline.load (std::memory_order_relaxed) ? 10000u : 500u;
        return hb != 0 && juce::Time::getMillisecondCounter() - hb < limit;
    }

private:
    void purgeDeadOwners (bool force) const;       // unter Sperre aufrufen
    mutable uint32_t lastPurgeMs = 0;
    uint32_t ownPid = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixHub)
};
} // namespace ame
