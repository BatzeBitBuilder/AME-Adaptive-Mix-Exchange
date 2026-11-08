// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

// Kurztest ohne DAW: mehrere Instanzen im selben Prozess, Hub-Kommunikation, DSP, UI-Snapshots.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "hub/MaskingModel.h"
#include "engine/Profiles.h"
#include "engine/RoleClassifier.h"
#include "RoleSignals.h"

using namespace mtm;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { std::cerr << "FAIL: " #cond " (Zeile " << __LINE__ << ")\n"; ++failures; } } while (0)

static void setParam (MultiTrackMixProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

// Testsignal-Generator: Summe von Sinustönen + optional Rauschen
struct TestSignal
{
    std::vector<std::pair<float, float>> partials;   // Frequenz, Amplitude
    float noise = 0.0f;
    double phase = 0.0;
    juce::Random rnd { 7 };

    float next (double sr)
    {
        float v = 0.0f;
        for (auto& [f, a] : partials)
            v += a * (float) std::sin (juce::MathConstants<double>::twoPi * f * phase);
        phase += 1.0 / sr;
        return v + noise * (rnd.nextFloat() * 2.0f - 1.0f);
    }
};

static void runBlocks (MultiTrackMixProcessor& p, TestSignal& sig, int blocks)
{
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float v = sig.next (48000.0);
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        p.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                CHECK (std::isfinite (buf.getSample (ch, i)));
    }
}

static void snapshot (juce::Component& c, const juce::File& f)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds());
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
}

// --- Schritt 4: Rollenerkennung ---
static void testRoleDetection()
{
    // Spurnamen
    struct NameCase { const char* name; Role expected; };
    const NameCase names[] {
        { "Kick In", Role::Kick }, { "BD 1", Role::Kick }, { "Bass Drum", Role::Kick }, { "Snare Top", Role::Snare },
        { "OH L", Role::Drums }, { "Hi-Hat", Role::Drums }, { "Lead Vox", Role::LeadVocal }, { "BGV 2", Role::BackingVocal },
        { "Chorus Gtr", Role::ElectricGuitar }, { "Ac. Gtr", Role::AcousticGuitar }, { "808", Role::Bass },
        { "Sub Bass", Role::Bass }, { "Rhodes", Role::Keys }, { "Lead Synth", Role::SynthLead }, { "Strings", Role::Strings },
        { "Riser FX", Role::Fx }, { "Gesang", Role::LeadVocal }, { "E-Git", Role::ElectricGuitar },
        { "Akustikgitarre", Role::AcousticGuitar }, { "Shaker", Role::Percussion }, { "Pad Wide", Role::Pad },
        { "1-Audio", Role::Auto }, { "2-MIDI", Role::Auto }
    };
    for (const auto& c : names)
    {
        const auto g = classifyByName (c.name);
        if (g.role != c.expected)
            std::cerr << "  Name \"" << c.name << "\" -> " << getRoleNames()[(int) g.role] << " (erwartet " << getRoleNames()[(int) c.expected] << ")\n";
        CHECK (g.role == c.expected);
    }
    {
        const auto g = classifyByName (MTM_TXT ("Bläser"));
        CHECK (g.role == Role::Brass);
    }

    // Klangmerkmale
    struct SigCase { const char* label; StereoGen gen; std::vector<Role> accepted; };
    std::vector<SigCase> cases {
        { "Kick",   testsig::kick(),  { Role::Kick } },
        { "Snare",  testsig::snare(), { Role::Snare } },
        { "Hats",   testsig::hats(),  { Role::Drums, Role::Percussion } },
        { "Bass",   testsig::bass(),  { Role::Bass } },
        { "Vocal",  testsig::vocal(), { Role::LeadVocal, Role::BackingVocal } },
        { "Pad",    testsig::pad(),   { Role::Pad, Role::Strings } },
    };

    for (auto& c : cases)
    {
        MultiTrackMixProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        double t = 0.0;
        for (int b = 0; b < 940; ++b)   // ~10 s
        {
            for (int i = 0; i < 512; ++i)
            {
                const auto [l, r] = c.gen.fn (t);
                buf.setSample (0, i, l);
                buf.setSample (1, i, r);
                t += 1.0 / 48000.0;
            }
            p.processBlock (buf, midi);
        }

        const auto f = p.getHub().getAudioData (p.getSlot())->loadFeatures();
        const auto g = classifyByFeatures (f);
        const bool ok = std::find (c.accepted.begin(), c.accepted.end(), g.role) != c.accepted.end();
        std::cout << "  " << c.label << ": " << getRoleNames()[(int) g.role] << " " << juce::roundToInt (g.confidence * 100.0f) << "%"
                  << "  [C=" << juce::roundToInt (f.centroidHz) << " FL=" << juce::String (f.flatness, 2)
                  << " SUB=" << juce::String (f.subRatio, 2) << " LOW=" << juce::String (f.lowRatio, 2)
                  << " HIGH=" << juce::String (f.highRatio, 2) << " ON=" << juce::String (f.onsetRate, 1)
                  << " HA=" << juce::String (f.harmonicity, 2) << " P=" << juce::roundToInt (f.pitchHz)
                  << " PV=" << juce::String (f.pitchVarCents, 1)
                  << " CR=" << juce::String (f.crestDb, 1) << " W=" << juce::String (f.width, 2) << "]"
                  << (ok ? "" : "  <-- FALSCH") << "\n";
        {
            std::vector<std::pair<float, int>> top;
            for (int r = 1; r < kNumRoles; ++r) top.push_back ({ g.probabilities[(size_t) r], r });
            std::sort (top.rbegin(), top.rend());
            std::cout << "     ";
            for (int i = 0; i < 3; ++i) std::cout << getRoleNames()[top[(size_t) i].second] << " " << juce::roundToInt (top[(size_t) i].first * 100.0f) << "%  ";
            std::cout << "\n";
        }
        CHECK (ok);

        // Über das Plugin: Rolle "Auto" -> Hub zeigt erkannte Rolle
        juce::MessageManager::getInstance()->runDispatchLoopUntil (250);
        const auto snap = p.getHub().getSnapshot();
        for (const auto& t : snap)
            if (t.instanceId == p.getInstanceId())
            {
                CHECK (t.autoRole);
                CHECK (t.role == g.role || t.role == Role::Other);
            }

        if (juce::String (c.label) == "Vocal")
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            snapshot (*ed, juce::File::getCurrentWorkingDirectory().getChildFile ("ui_auto_role.png"));
        }
    }

    // Spurname aus der DAW hat Vorrang vor Klangmerkmalen
    {
        MultiTrackMixProcessor p;
        juce::AudioProcessor::TrackProperties props;
        props.name = juce::String ("Snare Top");
        p.updateTrackProperties (props);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (250);
        bool found = false;
        for (const auto& t : p.getHub().getSnapshot())
            if (t.instanceId == p.getInstanceId())
            {
                found = true;
                CHECK (t.role == Role::Snare && t.autoFromName);
            }
        CHECK (found);

        // manuelle Rolle überschreibt
        setParam (p, pid::role, (float) Role::Keys);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (250);
        for (const auto& t : p.getHub().getSnapshot())
            if (t.instanceId == p.getInstanceId())
                CHECK (t.role == Role::Keys && ! t.autoRole);
    }
}

// --- Schritt 6: prozessübergreifender Hub (Shared Memory) ---
// Kindprozess: eine Bass-Line in einem eigenen Prozess (wie Plugin-Sandbox/Bridge), endet ohne Abmeldung (Absturz)
static int runHubChild()
{
    auto line = std::make_unique<MultiTrackMixProcessor>();
    line->setPlayConfigDetails (2, 2, 48000.0, 512);
    line->prepareToPlay (48000.0, 512);
    juce::AudioProcessor::TrackProperties tp;
    tp.name = juce::String ("Child Bass");
    line->updateTrackProperties (tp);

    TestSignal sig { { { 55.0f, 0.3f }, { 82.0f, 0.3f } }, 0.0f };
    bool engine = false, targets = false;
    for (int i = 0; i < 60; ++i)    // ~3 s
    {
        runBlocks (*line, sig, 5);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        engine = engine || line->getHub().isEngineAlive();
        if (auto* d = line->getHub().getAudioData (line->getSlot()))
            targets = targets || d->targetHpfHz.load() > 0.0f || std::abs (d->targetGainDb.load()) > 0.01f;
    }

    auto& hub = line->getHub();
    std::cout << "CHILD shared=" << (hub.isShared() ? 1 : 0) << " seen=" << hub.getNumInstances()
              << " masters=" << hub.getNumMasters() << " engine=" << (engine ? 1 : 0)
              << " targets=" << (targets ? 1 : 0) << std::endl;
    std::_Exit (0);   // Absturz simulieren: keine Abmeldung
}

static void testCrossProcessHub (MixHub& hub, const std::function<void()>& tick)
{
    std::cout << "Hub prozessübergreifend:\n";
    CHECK (hub.isShared());
    const int before = hub.getNumInstances();

    juce::ChildProcess child;
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName();
    CHECK (child.start (juce::StringArray { exe, "--hub-child" }, juce::ChildProcess::wantStdOut));

    bool seen = false, analysed = false;
    int procs = 0;
    for (int i = 0; i < 200 && ! (seen && analysed); ++i)
    {
        tick();
        for (const auto& t : hub.getSnapshot())
            if (t.name == "Child Bass")
            {
                seen = true;
                analysed = t.hasAnalysis && t.role == Role::Bass;
            }
        procs = juce::jmax (procs, hub.getNumProcesses());
    }

    const auto out = child.readAllProcessOutput();
    child.waitForProcessToFinish (5000);   // Prozess abholen (sonst Zombie = "lebt")
    std::cout << "  " << out.trim() << "  (Sitzung " << hub.getSessionKey() << ", Prozesse " << procs << ")\n";
    CHECK (seen && analysed);
    CHECK (procs == 2);
    CHECK (out.contains ("shared=1"));
    CHECK (out.contains ("seen=" + juce::String (before + 1)));
    CHECK (out.contains ("masters=1"));
    CHECK (out.contains ("engine=1"));
    CHECK (out.contains ("targets=1"));

    // Kind ist ohne Abmeldung beendet → Slot wird nach spätestens ~1 s freigegeben
    int after = -1;
    for (int i = 0; i < 60 && after != before; ++i)
    {
        tick();
        after = hub.getNumInstances();
    }
    CHECK (after == before);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    // eigene Hub-Sitzung, damit der Test nie mit einer laufenden DAW kollidiert (Kindprozess erbt sie)
    if (juce::SystemStats::getEnvironmentVariable ("AME_HUB_SESSION", {}).isEmpty())
    {
        const auto key = "smoketest" + juce::String (juce::Time::currentTimeMillis());
       #if JUCE_WINDOWS
        _putenv_s ("AME_HUB_SESSION", key.toRawUTF8());
       #else
        setenv ("AME_HUB_SESSION", key.toRawUTF8(), 1);
       #endif
    }

    if (argc > 1 && juce::String (argv[1]) == "--hub-child")
        return runHubChild();

    const juce::File outDir = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory();
    mtm::i18n::setLanguageForTesting (mtm::i18n::Language::German);

    auto kick   = std::make_unique<MultiTrackMixProcessor>();
    auto bass   = std::make_unique<MultiTrackMixProcessor>();
    auto vocal  = std::make_unique<MultiTrackMixProcessor>();
    auto master = std::make_unique<MultiTrackMixProcessor>();

    for (auto* p : { kick.get(), bass.get(), vocal.get(), master.get() })
    {
        p->setPlayConfigDetails (2, 2, 48000.0, 512);
        p->prepareToPlay (48000.0, 512);
    }

    CHECK (&kick->getHub() == &master->getHub());
    CHECK (kick->getHub().getNumInstances() == 4);

    setParam (*kick,  pid::role, (float) Role::Kick);
    setParam (*bass,  pid::role, (float) Role::Bass);
    setParam (*vocal, pid::role, (float) Role::LeadVocal);
    setParam (*master, pid::mode, (float) InstanceMode::Master);
    setParam (*master, pid::genre, (float) Genre::PopVocal);

    // Kompressor + EQ auf Vocal
    setParam (*vocal, pid::compOn, 1.0f);
    setParam (*vocal, pid::compThresh, -30.0f);
    setParam (*vocal, pid::compRatio, 6.0f);
    setParam (*vocal, pid::hpfOn, 1.0f);
    setParam (*vocal, pid::hpfFreq, 120.0f);
    setParam (*vocal, pid::p2Gain, 4.0f);

    // ~10 s Signal, damit das Langzeit-Spektrum (3 s) eingeschwungen ist
    constexpr int blocks = 940;
    TestSignal kickSig  { { { 55.0f, 0.5f }, { 110.0f, 0.2f } }, 0.02f };
    TestSignal bassSig  { { { 55.0f, 0.3f }, { 82.0f, 0.3f }, { 165.0f, 0.15f } }, 0.0f };
    TestSignal vocalSig { { { 220.0f, 0.2f }, { 440.0f, 0.15f }, { 880.0f, 0.1f }, { 1760.0f, 0.06f }, { 3000.0f, 0.04f } }, 0.0f };
    TestSignal masterSig { {}, 0.1f };
    auto runAll = [&] (int n)
    {
        for (int b = 0; b < n; ++b)
        {
            runBlocks (*kick, kickSig, 1);
            runBlocks (*bass, bassSig, 1);
            runBlocks (*vocal, vocalSig, 1);
            runBlocks (*master, masterSig, 1);
        }
    };
    runAll (blocks);

    // Timer laufen nicht ohne Message-Loop -> Hub direkt per Message-Loop-Durchlauf aktualisieren
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

    auto snap = kick->getHub().getSnapshot();
    CHECK (snap.size() == 4);
    CHECK (kick->getHub().getNumMasters() == 1);
    CHECK (master->isActiveMaster());
    CHECK (kick->getHub().globalGenre.load() == (int) Genre::PopVocal);

    for (auto& s : snap)
        std::cout << s.name << "  role=" << (int) s.role << "  mode=" << (int) s.mode
                  << "  rms=" << s.rmsDb << "  peak=" << s.peakDb << "  gr=" << s.grDb << "\n";

    CHECK (snap[2].grDb > 1.0f);   // Vocal komprimiert
    CHECK (snap[0].rmsDb > -20.0f && snap[0].rmsDb < 0.0f);

    // Analyse: Kick hat Energie bei 55 Hz, kaum bei 5 kHz
    CHECK (snap[0].hasAnalysis);
    int band55 = 0, band5k = 0;
    for (int b = 0; b < kNumAnalysisBands; ++b)
    {
        if (SpectrumAnalyzer::getBandEdgeHz (b) <= 55.0f && SpectrumAnalyzer::getBandEdgeHz (b + 1) > 55.0f) band55 = b;
        if (SpectrumAnalyzer::getBandEdgeHz (b) <= 5000.0f && SpectrumAnalyzer::getBandEdgeHz (b + 1) > 5000.0f) band5k = b;
    }
    std::cout << "Kick @55Hz: " << snap[0].bandAvgDb[(size_t) band55] << " dB, @5kHz: " << snap[0].bandAvgDb[(size_t) band5k] << " dB\n";
    CHECK (snap[0].bandAvgDb[(size_t) band55] > -15.0f);
    CHECK (snap[0].bandAvgDb[(size_t) band55] - snap[0].bandAvgDb[(size_t) band5k] > 30.0f);

    // Masking: Bass wird von Kick verdeckt, Vocal kaum
    auto mr = computeMasking (snap);
    CHECK (mr.numTracks() == 3);   // Master zählt nicht
    const int tKick = mr.findTrack (0), tBass = mr.findTrack (1), tVocal = mr.findTrack (2);
    for (int t = 0; t < mr.numTracks(); ++t)
        std::cout << snap[(size_t) mr.trackIndex[(size_t) t]].name << ": maskiert " << mr.maskedScore[(size_t) t]
                  << ", durch " << mr.worstMasker[(size_t) t] << " Baender " << mr.worstBandLo[(size_t) t] << "-" << mr.worstBandHi[(size_t) t] << "\n";
    CHECK (mr.worstMasker[(size_t) tBass] == tKick);
    CHECK (mr.maskedScore[(size_t) tBass] > mr.maskedScore[(size_t) tVocal]);
    CHECK (mr.get (tKick, tBass, band55) > 0.5f);

    // --- Adaptive Engine (Schritt 3) ---
    CHECK (master->isEngineRunning());
    CHECK (! kick->isEngineRunning());
    // Engine läuft in Echtzeit (50 Hz), Audio hier schneller -> verschachtelt weiterlaufen lassen
    for (int round = 0; round < 20; ++round)
    {
        runAll (50);
        juce::Thread::sleep (25);
    }
    CHECK (kick->getHub().isEngineAlive());

    snap = kick->getHub().getSnapshot();
    auto printAdaptive = [] (const TrackSnapshot& t)
    {
        std::cout << t.name << ": gain=" << t.appliedGainDb << " hpf=" << t.appliedHpfHz << " cuts=";
        for (auto c : t.appliedCutDb) std::cout << juce::String (c, 1) << " ";
        std::cout << "\n";
    };
    for (int i = 0; i < 3; ++i) printAdaptive (snap[(size_t) i]);

    // Pop: Kick hat im Sub Vorrang vor Bass -> Bass wird bei 31/63 Hz abgesenkt, Kick dort nicht
    CHECK (juce::jmin (snap[1].appliedCutDb[0], snap[1].appliedCutDb[1]) < -1.0f);
    CHECK (snap[0].appliedCutDb[1] > snap[1].appliedCutDb[1]);
    // Vocal bekommt einen Hochpass, Kick nicht
    CHECK (snap[2].appliedHpfHz > 40.0f);
    CHECK (snap[0].appliedHpfHz < 30.0f);

    // Masking nach Engine geringer als vorher
    {
        auto post = snap;
        for (auto& t : post)
            for (int b = 0; b < kNumAnalysisBands; ++b)
                t.bandAvgDb[(size_t) b] += ChannelStrip::getAdaptiveResponseDb (t.appliedCutDb, t.appliedHpfHz, t.appliedGainDb,
                                                                               SpectrumAnalyzer::getBandCentreHz (b));
        auto pre = computeMasking (snap), after = computeMasking (post);
        const int tv = pre.findTrack (2);   // Pop: Vocal im Fokus
        std::cout << "Vocal maskiert vorher " << pre.maskedScore[(size_t) tv] << ", nachher " << after.maskedScore[(size_t) tv] << "\n";
        CHECK (after.maskedScore[(size_t) tv] < pre.maskedScore[(size_t) tv] * 0.7f);
    }

    // Frequenzgang-Formel: -6 dB bei 1 kHz -> ca. -6 dB Antwort
    {
        std::array<float, kNumAdaptiveBands> c {};
        c[5] = -6.0f;
        const float r = ChannelStrip::getAdaptiveResponseDb (c, 0.0f, 0.0f, 1000.0f);
        CHECK (std::abs (r + 6.0f) < 0.1f);
        CHECK (std::abs (ChannelStrip::getAdaptiveResponseDb (c, 0.0f, 0.0f, 8000.0f)) < 0.5f);
    }


    // --- 0.4.0: Lautstärke hält Summenleistung konstant ---
    {
        float before = 0.0f, after = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            float pw = 0.0f;
            for (auto db : snap[(size_t) i].bandAvgDb) if (db > -70.0f) pw += std::pow (10.0f, db * 0.1f);
            before += pw;
            after  += pw * std::pow (10.0f, snap[(size_t) i].appliedGainDb * 0.1f);
        }
        const float diffDb = 10.0f * std::log10 (after / before);
        std::cout << "Summenleistung vorher/nachher: " << diffDb << " dB\n";
        CHECK (std::abs (diffDb) < 1.0f);
    }

    // --- Bypass ---
    setParam (*vocal, pid::adaptBypass, 1.0f);
    runAll (100);
    snap = kick->getHub().getSnapshot();
    CHECK (snap[2].bypassed);
    CHECK (snap[2].appliedHpfHz < 21.0f && std::abs (snap[2].appliedGainDb) < 0.1f);
    setParam (*vocal, pid::adaptBypass, 0.0f);
    runAll (100);
    snap = kick->getHub().getSnapshot();
    CHECK (snap[2].appliedHpfHz > 40.0f);   // nach Umschalten schnell wieder da

    // --- Einfrieren + Speichern/Laden ---
    {
        const auto beforeFreeze = kick->getHub().getSnapshot()[1];
        setParam (*bass, pid::adaptFreeze, 1.0f);
        runAll (20);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);   // Timer speichert Zustand

        // Preset wechseln: eingefrorene Spur bleibt
        setParam (*master, pid::genre, (float) Genre::Metal);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        for (int round = 0; round < 10; ++round) { runAll (50); juce::Thread::sleep (25); }
        const auto frozenSnap = kick->getHub().getSnapshot()[1];
        CHECK (frozenSnap.frozen);
        CHECK (std::abs (frozenSnap.appliedGainDb - beforeFreeze.appliedGainDb) < 0.3f);
        CHECK (std::abs (frozenSnap.appliedCutDb[1] - beforeFreeze.appliedCutDb[1]) < 0.3f);

        juce::MemoryBlock state;
        bass->getStateInformation (state);
        auto restored = std::make_unique<MultiTrackMixProcessor>();
        restored->setPlayConfigDetails (2, 2, 48000.0, 512);
        restored->setStateInformation (state.getData(), (int) state.getSize());
        restored->prepareToPlay (48000.0, 512);
        TestSignal sig = bassSig;
        runBlocks (*restored, sig, 5);
        auto* rd = restored->getHub().getAudioData (restored->getSlot());
        CHECK (rd->frozen.load());
        CHECK (std::abs (rd->appliedGainDb.load() - beforeFreeze.appliedGainDb) < 0.3f);
        std::cout << "Eingefroren: gain " << beforeFreeze.appliedGainDb << " -> wiederhergestellt " << rd->appliedGainDb.load() << "\n";
    }

    // --- Alle einfrieren / lösen über Master ---
    kick->getHub().sendFreezeCommand (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    CHECK (kick->apvts.getRawParameterValue (pid::adaptFreeze)->load() > 0.5f);
    CHECK (vocal->apvts.getRawParameterValue (pid::adaptFreeze)->load() > 0.5f);
    kick->getHub().sendFreezeCommand (false);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    CHECK (kick->apvts.getRawParameterValue (pid::adaptFreeze)->load() < 0.5f);
    CHECK (bass->apvts.getRawParameterValue (pid::adaptFreeze)->load() < 0.5f);
    setParam (*master, pid::genre, (float) Genre::PopVocal);

    // --- Neue Presets vorhanden und unterschiedlich ---
    CHECK (getGenreNames().size() == (int) Genre::NumGenres);
    CHECK (getRoleProfile (Genre::Metal, Role::ElectricGuitar).levelDb > getRoleProfile (Genre::HipHop, Role::ElectricGuitar).levelDb);
    CHECK (getGenreProfile (Genre::Acoustic).maxCutDb < getGenreProfile (Genre::HipHop).maxCutDb);

    // Zweiter Master -> inaktiv
    auto master2 = std::make_unique<MultiTrackMixProcessor>();
    setParam (*master2, pid::mode, (float) InstanceMode::Master);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    CHECK (master->isActiveMaster());
    CHECK (! master2->isActiveMaster());
    master2.reset();
    CHECK (kick->getHub().getNumInstances() == 4);

    testCrossProcessHub (kick->getHub(), [&] { runAll (2); juce::MessageManager::getInstance()->runDispatchLoopUntil (20); });
    CHECK (kick->getHub().getNumInstances() == 4);

    // UI-Snapshots
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (vocal->createEditor());
        for (int f = 0; f < 70; ++f) { runAll (3); juce::MessageManager::getInstance()->runDispatchLoopUntil (33); }
        snapshot (*ed, outDir.getChildFile ("ui_track.png"));
    }
    {
        // Bass-Instanz, Mix-Übersicht aktiv -> Überdeckung des Bass
        std::unique_ptr<juce::AudioProcessorEditor> ed (bass->createEditor());
        for (auto* c : ed->getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c))
                if (b->getButtonText().startsWith ("Mix"))
                    b->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        snapshot (*ed, outDir.getChildFile ("ui_overview_bass.png"));
    }
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (master->createEditor());
        for (int f = 0; f < 120; ++f) { runAll (3); juce::MessageManager::getInstance()->runDispatchLoopUntil (33); }
        snapshot (*ed, outDir.getChildFile ("ui_master.png"));
    }

    // Sprache, Parameter-Namen, Handbuch
    {
        std::cout << "Sprache/Handbuch:\n";
        CHECK (master->getParameters()[0]->getName (32) == "Mode");
        CHECK (getRoleNames (mtm::i18n::Language::English)[(int) Role::ElectricGuitar] == "Electric Guitar");
        CHECK (getRoleNames (mtm::i18n::Language::German).size() == getRoleNames (mtm::i18n::Language::English).size());
        CHECK (getGenreNames (mtm::i18n::Language::German).size() == getGenreNames (mtm::i18n::Language::English).size());
        CHECK (getPriorityNames (mtm::i18n::Language::German).size() == getPriorityNames (mtm::i18n::Language::English).size());

        auto* masterEd = dynamic_cast<MultiTrackMixEditor*> (master->createEditor());
        std::unique_ptr<juce::AudioProcessorEditor> edM (masterEd);
        auto* trackEd = dynamic_cast<MultiTrackMixEditor*> (bass->createEditor());
        std::unique_ptr<juce::AudioProcessorEditor> edT (trackEd);
        CHECK (masterEd != nullptr && trackEd != nullptr);

        const int chaptersDe = trackEd->getHandbook().getNumChapters();
        trackEd->showHandbook (true);
        trackEd->getHandbook().showChapter (2);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        snapshot (*edT, outDir.getChildFile ("ui_manual_de.png"));

        mtm::i18n::setLanguageForTesting (mtm::i18n::Language::English);   // beide Editoren folgen
        const int chaptersEn = trackEd->getHandbook().getNumChapters();
        std::cout << "  Kapitel DE " << chaptersDe << " / EN " << chaptersEn << "\n";
        CHECK (chaptersDe >= 8 && chaptersDe == chaptersEn);
        trackEd->getHandbook().showChapter (2);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        snapshot (*edT, outDir.getChildFile ("ui_manual_en.png"));

        trackEd->showHandbook (false);
        for (int f = 0; f < 40; ++f) { runAll (3); juce::MessageManager::getInstance()->runDispatchLoopUntil (33); }
        snapshot (*edT, outDir.getChildFile ("ui_track_en.png"));
        snapshot (*edM, outDir.getChildFile ("ui_master_en.png"));
        mtm::i18n::setLanguageForTesting (mtm::i18n::Language::German);
    }

    vocal.reset();
    CHECK (kick->getHub().getNumInstances() == 3);

    std::cout << "Rollenerkennung:\n";
    testRoleDetection();

    std::cout << (failures == 0 ? "OK" : "FEHLER: " + std::to_string (failures)) << "\n";
    return failures == 0 ? 0 : 1;
}
