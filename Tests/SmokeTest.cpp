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
#include "Settings.h"
#include "engine/StripPresets.h"

using namespace ame;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { std::cerr << "FAIL: " #cond " (Zeile " << __LINE__ << ")\n"; ++failures; } } while (0)

static void setParam (AmeProcessor& p, const char* id, float plain)
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

static void runBlocks (AmeProcessor& p, TestSignal& sig, int blocks)
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

static juce::Button* findButton (juce::Component& root, const juce::String& textStart)
{
    for (auto* c : root.getChildren())
    {
        if (auto* b = dynamic_cast<juce::Button*> (c))
            if (b->getButtonText().startsWith (textStart))
                return b;
        if (auto* b = findButton (*c, textStart))
            return b;
    }
    return nullptr;
}

static void snapshot (juce::Component& c, const juce::File& f)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds());
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
}

// --- Channelstrip-Module (ab 0.10) ---
// Pegel (RMS dB) eines Sinus nach dem Strip; optional zweiter Ton
static float stripRmsDb (ChannelStrip& cs, const ChannelStrip::Params& p, float hz, float amp,
                         float hz2 = 0.0f, float amp2 = 0.0f, int blocks = 200, float* outLeftSign = nullptr)
{
    cs.prepare (48000.0, 2);
    juce::AudioBuffer<float> buf (2, 512);
    double ph = 0.0;
    double sum = 0.0; int n = 0; double corr = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float v = amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * ph)
                          + amp2 * (float) std::sin (juce::MathConstants<double>::twoPi * hz2 * ph);
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
            ph += 1.0 / 48000.0;
        }
        juce::AudioBuffer<float> in (buf);
        cs.processPre (buf, p);
        cs.processPost (buf, p);
        if (b >= blocks / 2)
            for (int i = 0; i < 512; ++i)
            {
                const float y = buf.getSample (0, i);
                CHECK (std::isfinite (y));
                sum += (double) y * y; ++n;
                corr += (double) y * in.getSample (0, i);
            }
    }
    if (outLeftSign != nullptr) *outLeftSign = corr >= 0.0 ? 1.0f : -1.0f;
    return (float) (10.0 * std::log10 (sum / juce::jmax (1, n) + 1.0e-20));
}

static void testStripModules()
{
    std::cout << "Channelstrip-Module:\n";
    ChannelStrip cs;
    ChannelStrip::Params base;
    const float ref = stripRmsDb (cs, base, 1000.0f, 0.1f);

    // Phase
    {
        auto p = base; p.phaseL = true;
        float sign = 0.0f;
        const float db = stripRmsDb (cs, p, 1000.0f, 0.1f, 0, 0, 100, &sign);
        CHECK (sign < 0.0f && std::abs (db - ref) < 0.1f);
    }
    // Gate: leises Signal (-40 dBFS) unter Threshold -30 → um Range (40 dB) gedämpft, lautes bleibt
    {
        auto p = base; p.gateOn = true; p.gateThreshDb = -30.0f; p.gateRangeDb = 40.0f;
        const float quietRef = stripRmsDb (cs, base, 1000.0f, 0.01f);
        const float quiet = stripRmsDb (cs, p, 1000.0f, 0.01f);
        const float loud = stripRmsDb (cs, p, 1000.0f, 0.3f);
        const float loudRef = stripRmsDb (cs, base, 1000.0f, 0.3f);
        std::cout << "  Gate: leise " << (quiet - quietRef) << " dB, laut " << (loud - loudRef) << " dB\n";
        CHECK (quiet - quietRef < -35.0f);
        CHECK (std::abs (loud - loudRef) < 0.2f);
        p.gateMode = 1;   // Expander 1:2, 10 dB unter Threshold → ca. -10 dB
        const float exp = stripRmsDb (cs, p, 1000.0f, 0.01f);      // Spitze −40 dBFS = 10 dB unter Threshold
        const float expRef = stripRmsDb (cs, base, 1000.0f, 0.01f);
        std::cout << "  Expander 1:2 (−10 dB unter Thr.): " << (exp - expRef) << " dB\n";
        CHECK (exp - expRef < -6.0f && exp - expRef > -14.0f);
    }
    // Kompressor: Mix 0 = unverändert, Mix 100 = komprimiert; SC-HPF lässt Bass durch
    {
        auto p = base; p.compOn = true; p.compThreshDb = -30.0f; p.compRatio = 4.0f;
        const float wet = stripRmsDb (cs, p, 1000.0f, 0.3f);
        p.compMix = 0.0f;
        const float dry = stripRmsDb (cs, p, 1000.0f, 0.3f);
        const float dryRef = stripRmsDb (cs, base, 1000.0f, 0.3f);
        p.compMix = 1.0f; p.compScHpfHz = 400.0f;
        const float bassSc = stripRmsDb (cs, p, 60.0f, 0.3f);
        p.compScHpfHz = 20.0f;
        const float bassNoSc = stripRmsDb (cs, p, 60.0f, 0.3f);
        p.compAutoRelease = true; p.compKneeDb = 0.0f;
        const float auto_ = stripRmsDb (cs, p, 1000.0f, 0.3f);
        std::cout << "  Comp: wet " << (wet - dryRef) << " dB, Mix 0 " << (dry - dryRef) << " dB, Bass mit/ohne SC-HPF "
                  << bassSc << " / " << bassNoSc << " dB\n";
        CHECK (wet - dryRef < -10.0f);
        CHECK (std::abs (dry - dryRef) < 0.1f);
        CHECK (bassSc > bassNoSc + 6.0f);
        CHECK (auto_ < dryRef - 10.0f);
    }
    // De-Esser: 8 kHz wird abgesenkt, 500 Hz (Split) bleibt
    {
        auto p = base; p.deessOn = true; p.deessFreq = 6000.0f; p.deessThreshDb = -40.0f; p.deessRangeDb = 10.0f;
        const float s = stripRmsDb (cs, p, 8000.0f, 0.2f);
        const float red = cs.getDeessReductionDb();
        const float sRef = stripRmsDb (cs, base, 8000.0f, 0.2f);
        const float low = stripRmsDb (cs, p, 500.0f, 0.2f, 8000.0f, 0.2f) ;
        const float lowRef = stripRmsDb (cs, base, 500.0f, 0.2f, 8000.0f, 0.2f);
        std::cout << "  De-Esser: 8 kHz " << (s - sRef) << " dB, Mischung " << (low - lowRef) << " dB\n";
        CHECK (s - sRef < -6.0f && s - sRef > -10.5f);
        CHECK (low - lowRef > -4.0f && low - lowRef < -0.5f);
        CHECK (red > 9.0f);
    }

    // Filterflanken: eine Oktave unter 400 Hz (HPF)
    {
        auto p = base; p.hpfOn = true; p.hpfFreq = 400.0f;
        p.hpfSlope = 0; const float s12 = stripRmsDb (cs, p, 200.0f, 0.1f) - stripRmsDb (cs, base, 200.0f, 0.1f);
        p.hpfSlope = 2; const float s48 = stripRmsDb (cs, p, 200.0f, 0.1f) - stripRmsDb (cs, base, 200.0f, 0.1f);
        std::cout << "  HPF 400 Hz bei 200 Hz: 12 dB/Okt " << s12 << " dB, 48 dB/Okt " << s48 << " dB\n";
        CHECK (s12 < -9.0f && s12 > -15.0f);
        CHECK (s48 < -40.0f);
        CHECK (std::abs (ChannelStrip::getManualResponseDb (p, 200.0f) - s48) < 1.5f);
    }
    // Low als Glocke: 1 kHz unbeeinflusst, bei Shelf angehoben
    {
        auto p = base; p.eqFreq[0] = 100.0f; p.eqGainDb[0] = 6.0f;
        const float shelf = ChannelStrip::getManualResponseDb (p, 30.0f);
        p.lowBell = true; p.eqQ[0] = 2.0f;
        const float bell30 = ChannelStrip::getManualResponseDb (p, 30.0f), bell100 = ChannelStrip::getManualResponseDb (p, 100.0f);
        CHECK (shelf > 5.0f && bell30 < 2.0f && std::abs (bell100 - 6.0f) < 0.2f);
    }
    // Reihenfolge: +12 dB EQ vor dem Kompressor wird stärker komprimiert als danach
    {
        auto p = base; p.compOn = true; p.compThreshDb = -30.0f; p.compRatio = 10.0f; p.eqGainDb[1] = 12.0f;
        const float eqFirst = stripRmsDb (cs, p, 400.0f, 0.1f);
        p.compFirst = true;
        const float compFirst = stripRmsDb (cs, p, 400.0f, 0.1f);
        std::cout << "  Reihenfolge EQ>Comp " << eqFirst << " dB, Comp>EQ " << compFirst << " dB\n";
        CHECK (compFirst > eqFirst + 6.0f);
    }
    // Sättigung: Kleinsignal unverändert, laut begrenzt; Exciter erzeugt Höhen
    {
        for (int type = 0; type < 3; ++type)
        {
            CHECK (std::abs (ChannelStrip::saturate (type, 0.001f) - 0.001f) < 2.0e-5f);
            CHECK (std::abs (ChannelStrip::saturate (type, 10.0f)) < 1.6f);
        }
        auto p = base; p.satOn = true; p.satDriveDb = 18.0f;
        const float quiet = stripRmsDb (cs, p, 1000.0f, 0.001f) - stripRmsDb (cs, base, 1000.0f, 0.001f);
        const float loud = stripRmsDb (cs, p, 1000.0f, 0.5f) - stripRmsDb (cs, base, 1000.0f, 0.5f);
        p.satDriveDb = 0.0f; p.satMix = 1.0f; p.excAmount = 1.0f; p.excFreq = 3000.0f;
        // 4 kHz-Ton: Obertöne bei 8/12 kHz → mehr Energie oberhalb; Ton allein ändert sich kaum
        const float withExc = stripRmsDb (cs, p, 4000.0f, 0.3f);
        p.excAmount = 0.0f;
        const float exc = withExc - stripRmsDb (cs, p, 4000.0f, 0.3f);
        std::cout << "  Sättigung 18 dB: leise " << quiet << " dB, laut " << loud << " dB; Exciter " << exc << " dB\n";
        CHECK (std::abs (quiet) < 0.5f);
        CHECK (loud < -3.0f);
        CHECK (exc > 0.1f && exc < 6.0f);
    }

    // Limiter / Clipper: +6 dBFS-Sinus → Spitzen <= Ceiling
    for (int mode = 0; mode < 3; ++mode)
    {
        auto p = base; p.limOn = true; p.limMode = mode; p.limCeilingDb = -1.0f;
        cs.prepare (48000.0, 2);
        juce::AudioBuffer<float> buf (2, 512);
        float peak = 0.0f; double ph = 0.0;
        for (int b = 0; b < 50; ++b)
        {
            for (int i = 0; i < 512; ++i, ph += 1.0 / 48000.0)
                for (int ch = 0; ch < 2; ++ch)
                    buf.setSample (ch, i, 2.0f * (float) std::sin (juce::MathConstants<double>::twoPi * 200.0 * ph));
            cs.processPre (buf, p);
            cs.processPost (buf, p);
            peak = juce::jmax (peak, buf.getMagnitude (0, 512));
        }
        std::cout << "  Limiter-Modus " << mode << ": Spitze " << juce::Decibels::gainToDecibels (peak)
                  << " dBFS, Reduktion " << cs.getLimiterReductionDb() << " dB\n";
        CHECK (peak <= juce::Decibels::decibelsToGain (-1.0f) + 1.0e-4f);
        CHECK (cs.getLimiterReductionDb() > 5.0f);
    }
    CHECK (std::abs (ChannelStrip::clip (1, 0.3f, 1.0f) - 0.3f) < 1.0e-6f);   // unter 50 % linear
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
        const auto g = classifyByName (AME_TXT ("Bläser"));
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
        AmeProcessor p;
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
        AmeProcessor p;
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
    auto line = std::make_unique<AmeProcessor>();
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
    // weiter takten, bis das Kind fertig ist: die Engine rechnet im Audio-Takt des Exchange (hier: tick)
    for (int i = 0; i < 400 && (! (seen && analysed) || child.isRunning()); ++i)
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

    // Einstellungen (Sprache, Fenstergröße, Presets) nicht im echten Benutzerordner
    const auto settingsDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("AMESmokeTestSettings");
    settingsDir.createDirectory();
    ame::settings::setUserDataFolderForTesting (settingsDir);
    ame::settings::set ("uiScale", "1.0");   // Screenshots in Basisgröße

    if (argc > 1 && juce::String (argv[1]) == "--hub-child")
        return runHubChild();

    const juce::File outDir = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory();
    ame::i18n::setLanguageForTesting (ame::i18n::Language::German);

    auto kick   = std::make_unique<AmeProcessor>();
    auto bass   = std::make_unique<AmeProcessor>();
    auto vocal  = std::make_unique<AmeProcessor>();
    auto master = std::make_unique<AmeProcessor>();

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
        auto restored = std::make_unique<AmeProcessor>();
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
    auto master2 = std::make_unique<AmeProcessor>();
    setParam (*master2, pid::mode, (float) InstanceMode::Master);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    CHECK (master->isActiveMaster());
    CHECK (! master2->isActiveMaster());
    master2.reset();
    CHECK (kick->getHub().getNumInstances() == 4);

    testCrossProcessHub (kick->getHub(), [&] { runAll (2); juce::MessageManager::getInstance()->runDispatchLoopUntil (20); });

    // --- Abhöre im Exchange: Roh, Pegelangleich, Host-Bypass, nicht im Projekt gespeichert ---
    {
        std::cout << "Abhöre:\n";
        auto tick = [&] (int n) { for (int i = 0; i < n; ++i) { runAll (2); juce::MessageManager::getInstance()->runDispatchLoopUntil (20); } };
        auto& hub = kick->getHub();
        master->setMonitor (2);
        tick (10);
        CHECK (hub.isRawMonitor());

        // Line gibt das Original aus
        juce::AudioBuffer<float> buf (2, 512), in (2, 512);
        for (int i = 0; i < 512; ++i)
            for (int ch = 0; ch < 2; ++ch)
                buf.setSample (ch, i, kickSig.next (48000.0) * (ch == 0 ? 1.0f : 0.9f));
        in.makeCopyOf (buf);
        juce::MidiBuffer midi;
        kick->processBlock (buf, midi);
        float maxDiff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (buf.getSample (ch, i) - in.getSample (ch, i)));
        CHECK (maxDiff < 1.0e-6f);

        // Exchange gleicht den Pegelunterschied aus
        const float matchDb = hub.rawMatchDb.load();
        juce::AudioBuffer<float> mb (2, 512);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                mb.setSample (ch, i, 0.1f);
        master->processBlock (mb, midi);
        master->processBlock (mb, midi);   // Glättung (50 ms) abgeschlossen
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                mb.setSample (ch, i, 0.1f);
        master->processBlock (mb, midi);
        master->processBlock (mb, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                mb.setSample (ch, i, 0.1f);
        master->processBlock (mb, midi);
        const float gotDb = juce::Decibels::gainToDecibels (mb.getSample (0, 511) / 0.1f);
        std::cout << "  Roh: Line = Original (max. Abw. " << maxDiff << "), Pegelangleich " << matchDb << " dB, Exchange-Ausgang " << gotDb << " dB\n";
        CHECK (std::abs (matchDb) > 0.1f);
        CHECK (std::abs (gotDb - matchDb) < 0.1f);

        // Nicht im Projekt gespeichert
        juce::MemoryBlock state;
        master->getStateInformation (state);
        AmeProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (juce::roundToInt (restored.apvts.getRawParameterValue (pid::monitor)->load()) == 0);
        CHECK (juce::roundToInt (master->apvts.getRawParameterValue (pid::monitor)->load()) == 2);

        // Ohne Amt
        master->setMonitor (1);
        tick (5);
        CHECK (! hub.isRawMonitor() && hub.globalBypass.load());
        master->setMonitor (0);
        tick (5);
        CHECK (! hub.globalBypass.load());

        // Host-Bypass bzw. keine Verarbeitung des Exchange → Engine aus
        for (int i = 0; i < 25; ++i)
        {
            runBlocks (*kick, kickSig, 2);
            juce::AudioBuffer<float> b2 (2, 512);
            b2.clear();
            master->processBlockBypassed (b2, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        }
        CHECK (master->isExchangeSuspended());
        CHECK (! master->isEngineRunning());
        CHECK (! hub.isEngineAlive());
        tick (10);
        CHECK (! master->isExchangeSuspended() && master->isEngineRunning() && hub.isEngineAlive());

        // Ableton "Gerät aus": Host ruft gar nichts mehr auf
        for (int i = 0; i < 40; ++i)
        {
            runBlocks (*kick, kickSig, 2);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        }
        CHECK (master->isExchangeSuspended() && ! hub.isEngineAlive());
        tick (10);
        CHECK (hub.isEngineAlive());
        std::cout << "  Host-Bypass / keine Verarbeitung: Engine aus und wieder an\n";
    }
    CHECK (kick->getHub().getNumInstances() == 4);

    // --- Export = Abhöre: Engine im Audio-Takt, Mitschnitt (1.0.2) ---
    {
        auto& hub = kick->getHub();

        // Engine rechnet je 20 ms Audio (unabhängig von der Uhrzeit / Render-Geschwindigkeit)
        const auto steps0 = master->getEngineSteps();
        runAll (250);   // 250 × 512 @ 48 kHz = 2,667 s → 133 Schritte
        const auto steps = (int64_t) (master->getEngineSteps() - steps0);
        std::cout << "  Engine-Schritte fuer 2,667 s Audio: " << steps << "\n";
        CHECK (std::abs (steps - 133) <= 1);

        // Offline-Kennung des Hosts → großzügige Lebenszeichen
        master->setNonRealtime (true);
        runAll (2);
        CHECK (hub.engineOffline.load());
        master->setNonRealtime (false);
        runAll (2);
        CHECK (! hub.engineOffline.load());

        // Songposition für die Lines
        struct FakePlayHead : juce::AudioPlayHead
        {
            double sec = 0.0;
            bool playing = true;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo p;
                p.setIsPlaying (playing);
                p.setTimeInSeconds (sec);
                p.setTimeInSamples ((int64_t) (sec * 48000.0));
                return p;
            }
        } ph;
        for (auto* p : { kick.get(), bass.get(), vocal.get() })
            p->setPlayHead (&ph);

        auto runSong = [&] (int rounds, bool withExchange, const std::function<void (int)>& each = {})
        {
            for (int r = 0; r < rounds; ++r)
            {
                runBlocks (*kick, kickSig, 1);
                runBlocks (*bass, bassSig, 1);
                runBlocks (*vocal, vocalSig, 1);
                if (withExchange)
                    runBlocks (*master, masterSig, 1);
                if (each) each (r);
                ph.sec += 512.0 / 48000.0;
            }
        };

        // Aufnahme (alle) vom Exchange
        hub.sendLearnCommand (MixHub::learnRecord);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        CHECK (juce::roundToInt (bass->apvts.getRawParameterValue (pid::learnMode)->load()) == 1);
        CHECK (bass->getRecording().isAllocated());
        CHECK (juce::roundToInt (master->apvts.getRawParameterValue (pid::learnMode)->load()) == 0);   // Exchange nimmt nicht teil

        ph.sec = 0.0;
        runSong (375, true);   // 4 s
        std::cout << "  Mitschnitt Bass: " << bass->getRecording().getLengthSec() << " s\n";
        CHECK (std::abs (bass->getRecording().getLengthSec() - 4.0f) < 0.05f);
        CHECK (hub.getAudioData (bass->getSlot())->learnSec.load() > 3.9f);

        AdaptiveRecording::Values at2;
        CHECK (bass->getRecording().read (2.0, at2));
        CHECK (at2.cutDb[1] < -1.0f);   // Bass weicht der Kick (wie oben)

        // Wiedergabe ohne Exchange (wie Stem-Export): exakt die aufgezeichneten Werte an derselben Stelle
        hub.sendLearnCommand (MixHub::learnPlay);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        CHECK (juce::roundToInt (bass->apvts.getRawParameterValue (pid::learnMode)->load()) == 2);

        auto replayRun = [&] (std::vector<float>& out)
        {
            ph.sec = 0.0;
            runSong (375, false, [&] (int r)
            {
                if (r >= 60)   // nach der Überblendung (0,3 s)
                {
                    auto* d = hub.getAudioData (bass->getSlot());
                    out.push_back (d->appliedCutDb[1].load());
                    out.push_back (d->appliedGainDb.load());
                    AdaptiveRecording::Values v;
                    bass->getRecording().read (ph.sec, v);
                    CHECK (std::abs (v.cutDb[1] - d->appliedCutDb[1].load()) < 1.0e-4f);
                }
            });
        };
        hub.getAudioData (bass->getSlot())->renderWarnMs.store (0);   // Warnungen vom Testbeginn (Engine lief noch nicht)
        juce::Thread::sleep (600);   // Engine-Lebenszeichen sicher abgelaufen
        CHECK (! hub.isEngineAlive());
        std::vector<float> runA, runB;
        replayRun (runA);
        bass->setNonRealtime (true);   // Export-Kennung ändert nichts
        replayRun (runB);
        bass->setNonRealtime (false);
        CHECK (runA == runB);
        CHECK (hub.getAudioData (bass->getSlot())->renderWarnMs.load() == 0);   // Wiedergabe: keine Warnung
        std::cout << "  Wiedergabe ohne Exchange: " << runA.size() / 2 << " Bloecke identisch zum Mitschnitt\n";

        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (bass->createEditor());
            ph.sec = 2.0;
            for (int f = 0; f < 10; ++f) { runSong (3, false); juce::MessageManager::getInstance()->runDispatchLoopUntil (33); }
            snapshot (*ed, outDir.getChildFile ("ui_track_playback.png"));
        }

        // im Projekt gespeichert (komprimiert) und wieder geladen
        {
            juce::MemoryBlock state;
            bass->getStateInformation (state);
            std::cout << "  Projektzustand Bass mit Mitschnitt: " << state.getSize() << " Bytes\n";
            AmeProcessor restored;
            restored.setStateInformation (state.getData(), (int) state.getSize());
            CHECK (std::abs (restored.getRecording().getLengthSec() - bass->getRecording().getLengthSec()) < 1.0e-6f);
            AdaptiveRecording::Values r2;
            CHECK (restored.getRecording().read (2.0, r2));
            CHECK (r2.cutDb == at2.cutDb && r2.hpfHz == at2.hpfHz && r2.gainDb == at2.gainDb);
            CHECK (juce::roundToInt (restored.apvts.getRawParameterValue (pid::learnMode)->load()) == 2);
        }

        // Export ohne Exchange und ohne Mitschnitt → Warnung an der Line
        hub.sendLearnCommand (MixHub::learnLive);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        CHECK (juce::roundToInt (kick->apvts.getRawParameterValue (pid::learnMode)->load()) == 0);
        juce::Thread::sleep (600);
        runBlocks (*kick, kickSig, 200);   // schneller als Echtzeit
        CHECK (hub.getAudioData (kick->getSlot())->renderWarnMs.load() != 0);

        // Löschen
        hub.sendLearnCommand (MixHub::learnClear);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        CHECK (! bass->getRecording().hasData());

        for (auto* p : { kick.get(), bass.get(), vocal.get() })
        {
            p->setPlayHead (nullptr);
            hub.getAudioData (p->getSlot())->renderWarnMs.store (0);
        }
        // Engine wieder anlaufen lassen
        for (int i = 0; i < 10; ++i) { runAll (2); juce::MessageManager::getInstance()->runDispatchLoopUntil (20); }
        CHECK (hub.isEngineAlive());
        for (auto* p : { kick.get(), bass.get(), vocal.get() })
            hub.getAudioData (p->getSlot())->renderWarnMs.store (0);
    }

    // UI-Snapshots
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (vocal->createEditor());
        for (int f = 0; f < 70; ++f) { runAll (3); juce::MessageManager::getInstance()->runDispatchLoopUntil (33); }
        snapshot (*ed, outDir.getChildFile ("ui_track.png"));
    }
    {
        // Bass-Instanz, Mix-Übersicht aktiv -> Überdeckung des Bass
        std::unique_ptr<juce::AudioProcessorEditor> ed (bass->createEditor());
        if (auto* b = findButton (*ed, "Mix"))
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
        CHECK (getRoleNames (ame::i18n::Language::English)[(int) Role::ElectricGuitar] == "Electric Guitar");
        CHECK (getRoleNames (ame::i18n::Language::German).size() == getRoleNames (ame::i18n::Language::English).size());
        CHECK (getGenreNames (ame::i18n::Language::German).size() == getGenreNames (ame::i18n::Language::English).size());
        CHECK (getPriorityNames (ame::i18n::Language::German).size() == getPriorityNames (ame::i18n::Language::English).size());

        auto* masterEd = dynamic_cast<AmeEditor*> (master->createEditor());
        std::unique_ptr<juce::AudioProcessorEditor> edM (masterEd);
        auto* trackEd = dynamic_cast<AmeEditor*> (bass->createEditor());
        std::unique_ptr<juce::AudioProcessorEditor> edT (trackEd);
        CHECK (masterEd != nullptr && trackEd != nullptr);

        const int chaptersDe = trackEd->getHandbook().getNumChapters();
        trackEd->showHandbook (true);
        trackEd->getHandbook().showChapter (2);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        snapshot (*edT, outDir.getChildFile ("ui_manual_de.png"));

        ame::i18n::setLanguageForTesting (ame::i18n::Language::English);   // beide Editoren folgen
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
        ame::i18n::setLanguageForTesting (ame::i18n::Language::German);
    }

    // Werkspresets: alle IDs gültig, Werte im Bereich; Laden über die Rolle
    {
        std::cout << "Presets:\n";
        for (const auto& fp : presets::getFactoryPresets())
            for (const auto& v : fp.values)
            {
                auto* prm = dynamic_cast<juce::RangedAudioParameter*> (bass->apvts.getParameter (v.id));
                CHECK (prm != nullptr && presets::getSoundScope().contains (v.id));
                if (prm != nullptr)
                {
                    const auto& r = prm->getNormalisableRange();
                    if (v.value < r.start || v.value > r.end)
                        std::cerr << "  Wert außerhalb: " << v.id << " " << v.value << "\n";
                    CHECK (v.value >= r.start && v.value <= r.end);
                }
            }

        setParam (*bass, pid::deessOn, 1.0f);
        setParam (*bass, pid::pan, -0.5f);
        std::unique_ptr<juce::AudioProcessorEditor> ed (bass->createEditor());
        auto* med = dynamic_cast<AmeEditor*> (ed.get());
        for (int f = 0; f < 20; ++f) { runAll (1); juce::MessageManager::getInstance()->runDispatchLoopUntil (33); }
        CHECK (med->getStrip().getCurrentRole() == Role::Bass);
        med->getStrip().loadPresetForCurrentRole();
        auto val = [&] (const char* id) { return bass->apvts.getRawParameterValue (id)->load(); };
        CHECK (val (pid::compOn) > 0.5f && std::abs (val (pid::compRatio) - 4.0f) < 0.01f);
        CHECK (val (pid::hpfOn) > 0.5f && std::abs (val (pid::hpfFreq) - 30.0f) < 0.5f);
        CHECK (val (pid::deessOn) < 0.5f);                 // nicht im Preset → Standard
        CHECK (std::abs (val (pid::pan) + 0.5f) < 0.01f);  // außerhalb des Klang-Umfangs → bleibt
        std::cout << "  " << presets::getFactoryPresets().size() << " Werkspresets, Bass geladen\n";
        med->getStrip().showModule (ui::StripPanel::Module::Compressor);
        for (int f = 0; f < 20; ++f) { runAll (1); juce::MessageManager::getInstance()->runDispatchLoopUntil (33); }
        snapshot (*ed, outDir.getChildFile ("ui_track_preset.png"));
    }

    // Eigene Presets + A/B
    {
        auto val = [&] (const char* id) { return vocal->apvts.getRawParameterValue (id)->load(); };
        setParam (*vocal, pid::compRatio, 7.0f);
        setParam (*vocal, pid::limOn, 1.0f);
        setParam (*vocal, pid::outGain, -3.0f);
        CHECK (presets::saveUserPreset (vocal->apvts, "Test Vox / 1"));
        CHECK (presets::getUserPresetNames().contains ("Test Vox / 1"));
        setParam (*vocal, pid::compRatio, 2.0f);
        setParam (*vocal, pid::limOn, 0.0f);
        setParam (*vocal, pid::outGain, 0.0f);
        CHECK (presets::loadUserPreset (vocal->apvts, "Test Vox / 1"));
        CHECK (std::abs (val (pid::compRatio) - 7.0f) < 0.01f && val (pid::limOn) > 0.5f && std::abs (val (pid::outGain) + 3.0f) < 0.01f);

        // A/B: B startet als Kopie, Änderungen in B, zurück zu A = alter Wert
        CHECK (presets::getActiveAB (vocal->apvts) == "A");
        presets::selectAB (vocal->apvts, "B");
        CHECK (std::abs (val (pid::compRatio) - 7.0f) < 0.01f);
        setParam (*vocal, pid::compRatio, 12.0f);
        presets::selectAB (vocal->apvts, "A");
        CHECK (std::abs (val (pid::compRatio) - 7.0f) < 0.01f);
        presets::selectAB (vocal->apvts, "B");
        CHECK (std::abs (val (pid::compRatio) - 12.0f) < 0.01f);
        presets::copyActiveToOther (vocal->apvts);
        presets::selectAB (vocal->apvts, "A");
        CHECK (std::abs (val (pid::compRatio) - 12.0f) < 0.01f);

        // A/B-Zustand wird mit dem Projekt gespeichert
        juce::MemoryBlock state;
        vocal->getStateInformation (state);
        AmeProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (presets::getActiveAB (restored.apvts) == "A");
        CHECK (restored.apvts.state.getChildWithName ("ABCompare").getChildWithName ("B").isValid());

        CHECK (presets::deleteUserPreset ("Test Vox / 1"));
        CHECK (! presets::getUserPresetNames().contains ("Test Vox / 1"));
        std::cout << "  eigene Presets + A/B ok\n";

        // Screenshots: Limiter-Reiter
        std::unique_ptr<juce::AudioProcessorEditor> ed (vocal->createEditor());
        auto* med = dynamic_cast<AmeEditor*> (ed.get());
        med->getStrip().showModule (ui::StripPanel::Module::Limiter);
        for (int f = 0; f < 15; ++f) { runAll (1); juce::MessageManager::getInstance()->runDispatchLoopUntil (33); }
        snapshot (*ed, outDir.getChildFile ("ui_rack_limiter.png"));
        med->getStrip().showModule (ui::StripPanel::Module::Saturation);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        snapshot (*ed, outDir.getChildFile ("ui_rack_saturation.png"));
        med->getStrip().showModule (ui::StripPanel::Module::Gate);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        snapshot (*ed, outDir.getChildFile ("ui_rack_gate.png"));
        med->getStrip().showModule (ui::StripPanel::Module::DeEsser);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        snapshot (*ed, outDir.getChildFile ("ui_rack_deesser.png"));
    }

    vocal.reset();
    CHECK (kick->getHub().getNumInstances() == 3);

    std::cout << "Rollenerkennung:\n";
    testRoleDetection();
    testStripModules();

    std::cout << (failures == 0 ? "OK" : "FEHLER: " + std::to_string (failures)) << "\n";
    return failures == 0 ? 0 : 1;
}
