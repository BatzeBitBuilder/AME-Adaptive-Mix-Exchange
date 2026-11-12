// AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
// SPDX-License-Identifier: AGPL-3.0-only

// Benchmark (AMESmokeTest --bench <folder>): CPU cost of the audio path, engine, project state and UI,
// plus raw audio/engine dumps (*.f32) to compare two builds sample by sample.
#pragma once

#include <ctime>
#include <functional>

namespace bench
{
using namespace ame;

inline void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

inline void setP (AmeProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

// One second of stereo test signal (harmonic tone with level modulation + noise), deterministic per seed
inline juce::AudioBuffer<float> makeSignal (int seed)
{
    constexpr int n = 48000;
    juce::AudioBuffer<float> b (2, n);
    juce::Random rnd (seed * 7919 + 13);
    const double base = 40.0 * std::pow (2.0, rnd.nextDouble() * 6.0);
    const double f0 = std::round (base);   // whole Hz: loops without a click
    const float noise = 0.01f + 0.05f * rnd.nextFloat();
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / n;
        float v = 0.0f;
        for (int h = 1; h <= 6; ++h)
            if (f0 * h < 20000.0)
                v += (0.3f / (float) h) * (float) std::sin (juce::MathConstants<double>::twoPi * f0 * h * t);
        const float env = 0.15f + 0.85f * (float) std::pow (0.5 + 0.5 * std::sin (juce::MathConstants<double>::twoPi * 2.0 * t), 4.0);
        b.setSample (0, i, env * v + noise * (rnd.nextFloat() * 2.0f - 1.0f));
        b.setSample (1, i, env * v * 0.8f + noise * (rnd.nextFloat() * 2.0f - 1.0f));
    }
    return b;
}

struct Runner
{
    juce::AudioBuffer<float> buf { 2, 512 };
    juce::MidiBuffer midi;
    int pos = 0;

    void run (AmeProcessor& p, const juce::AudioBuffer<float>& sig, int blocks, std::vector<float>* dump = nullptr)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    buf.setSample (ch, i, sig.getSample (ch, (pos + i) % sig.getNumSamples()));
            pos = (pos + 512) % sig.getNumSamples();
            p.processBlock (buf, midi);
            if (dump != nullptr)
                for (int i = 0; i < 512; ++i)
                    for (int ch = 0; ch < 2; ++ch)
                        dump->push_back (buf.getSample (ch, i));
        }
    }
};

inline double bestOf (int runs, const std::function<void()>& fn)
{
    double best = 1.0e30;
    for (int r = 0; r < runs; ++r)
    {
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        fn();
        best = juce::jmin (best, juce::Time::getMillisecondCounterHiRes() - t0);
    }
    return best;
}

inline void writeDump (const juce::File& dir, const char* name, const std::vector<float>& v)
{
    const auto f = dir.getChildFile (name);
    f.deleteFile();
    juce::FileOutputStream os (f);
    os.write (v.data(), v.size() * sizeof (float));
}

inline void fullStrip (AmeProcessor& p)
{
    setP (p, pid::role, (float) Role::LeadVocal);
    for (auto* id : { pid::hpfOn, pid::lpfOn, pid::gateOn, pid::compOn, pid::deessOn, pid::satOn, pid::limOn })
        setP (p, id, 1.0f);
    setP (p, pid::hpfFreq, 90.0f);     setP (p, pid::lpfFreq, 16000.0f);
    setP (p, pid::lsGain, 2.0f);       setP (p, pid::p1Gain, -3.0f);
    setP (p, pid::p2Gain, 4.0f);       setP (p, pid::hsGain, 1.5f);
    setP (p, pid::gateThresh, -45.0f); setP (p, pid::compThresh, -28.0f);
    setP (p, pid::compRatio, 4.0f);    setP (p, pid::compMakeup, 3.0f);
    setP (p, pid::deessThresh, -40.0f);
    setP (p, pid::excAmount, 30.0f);   setP (p, pid::pan, 0.2f);
}

inline std::unique_ptr<AmeProcessor> makeProc()
{
    auto p = std::make_unique<AmeProcessor>();
    p->setPlayConfigDetails (2, 2, 48000.0, 512);
    p->prepareToPlay (48000.0, 512);
    return p;
}

// CPU time of the message thread per second while the editor's timers run (audio is fed in between, untimed)
inline double uiTickCpuMsPerSec (const std::function<void()>& feedAudio)
{
    double cpu = 0.0;
    constexpr int rounds = 50, ms = 40;
    for (int i = 0; i < rounds; ++i)
    {
        feedAudio();
        const auto c0 = std::clock();
        pump (ms);
        cpu += (double) (std::clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
    }
    return cpu * 1000.0 / (rounds * ms);
}

inline double paintMs (juce::Component& ed, juce::Rectangle<int> clip, int frames)
{
    juce::Image img (juce::Image::ARGB, ed.getWidth(), ed.getHeight(), true);
    return bestOf (3, [&]
    {
        for (int i = 0; i < frames; ++i)
        {
            juce::Graphics g (img);
            if (! clip.isEmpty())
                g.reduceClipRegion (clip);
            ed.paintEntireComponent (g, false);
        }
    }) / frames;
}

inline int run (const juce::File& outDir)
{
    outDir.createDirectory();
    juce::StringArray report;
    auto add = [&] (const juce::String& name, double value, const juce::String& unit)
    {
        const auto line = name + ";" + juce::String (value, 3) + ";" + unit;
        report.add (line);
        std::cout << "BENCH;" << line << std::endl;
    };

    constexpr int blocks30s = 2812;   // 30 s at 48 kHz / 512

    add ("msg_thread_empty", uiTickCpuMsPerSec ([] {}), "CPU ms per s");   // reference: message loop without any instance

    //--- 1. ChannelStrip alone: moving parameters and adaptive values (dump for the null test) ---
    {
        const auto sig = makeSignal (1);
        ChannelStrip cs;
        cs.prepare (48000.0, 2);
        ChannelStrip::Params p;
        p.hpfOn = p.lpfOn = p.gateOn = p.compOn = p.deessOn = p.satOn = p.limOn = true;
        p.lpfFreq = 15000.0f; p.gateThreshDb = -45.0f; p.compThreshDb = -28.0f; p.compRatio = 4.0f;
        p.deessThreshDb = -40.0f; p.excAmount = 0.3f;
        std::vector<float> dump;
        juce::AudioBuffer<float> buf (2, 512);
        int pos = 0;
        for (int b = 0; b < 700; ++b)
        {
            const float s = std::sin ((float) b / 20.0f);
            if (b >= 100)
            {
                p.hpfFreq = 80.0f + 40.0f * s;
                p.eqGainDb = { 3.0f * s, -4.0f * s, 5.0f * s, 2.0f * s };
                p.eqFreq[1] = 400.0f + 200.0f * s;
                p.compMakeupDb = 2.0f + 2.0f * s;
                p.deessFreq = 6500.0f + 1500.0f * s;
                p.pan = 0.5f * s; p.width = 1.0f + 0.5f * s; p.outGainDb = -1.0f + s; p.inGainDb = s;
            }
            if (b >= 200)
            {
                for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
                    p.adaptiveCutDb[k] = -6.0f * (0.5f + 0.5f * std::sin ((float) b / 15.0f + (float) k));
                p.adaptiveHpfHz = b < 500 ? 60.0f + 20.0f * s : 0.0f;
                p.adaptiveGainDb = 2.0f * s;
            }
            if (b == 350) { p.hpfSlope = 2; p.lowBell = true; p.compFirst = true; p.satType = 2; p.deessWide = true; }
            if (b == 450) { p.limMode = 1; p.gateMode = 1; p.compAutoRelease = true; p.satType = 1; }
            if (b == 550) { p.compOn = false; p.gateOn = false; p.deessOn = false; p.satOn = false; p.hpfOn = false; }
            if (b == 620) { p.compOn = true; p.deessOn = true; p.deessWide = false; p.hpfOn = true; p.hpfSlope = 0; }

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    buf.setSample (ch, i, sig.getSample (ch, (pos + i) % 48000));
            pos = (pos + 512) % 48000;
            cs.processPre (buf, p);
            cs.processPost (buf, p);
            for (int i = 0; i < 512; ++i)
                for (int ch = 0; ch < 2; ++ch)
                    dump.push_back (buf.getSample (ch, i));
        }
        writeDump (outDir, "bench_strip.f32", dump);
    }

    //--- 2. Line: full strip, manual role (no Exchange) ---
    {
        const auto sig = makeSignal (2);
        auto line = makeProc();
        fullStrip (*line);
        Runner r;
        std::vector<float> dump;
        r.run (*line, sig, 150, &dump);
        setP (*line, pid::hpfFreq, 140.0f); setP (*line, pid::p1Gain, 5.0f); setP (*line, pid::pan, -0.4f);
        r.run (*line, sig, 150, &dump);
        writeDump (outDir, "bench_line.f32", dump);
        const double ms = bestOf (3, [&] { r.run (*line, sig, blocks30s); });
        add ("line_full_strip", ms / 300.0, "% of one core");
    }

    //--- 3. Line: default strip (all modules off), manual role / Auto role ---
    for (int autoRole = 0; autoRole < 2; ++autoRole)
    {
        const auto sig = makeSignal (3);
        auto line = makeProc();
        setP (*line, pid::role, autoRole ? (float) Role::Auto : (float) Role::Bass);
        pump (250);
        Runner r;
        r.run (*line, sig, 100);
        const double ms = bestOf (3, [&] { r.run (*line, sig, blocks30s); });
        add (autoRole ? "line_default_auto_role" : "line_default_manual_role", ms / 300.0, "% of one core");
    }

    //--- 4. Project state with a 10 min recording ---
    {
        auto line = makeProc();
        auto& rec = line->getRecording();
        rec.allocate();
        AdaptiveRecording::Values v;
        for (int f = 0; f < 600 * AdaptiveRecording::kFramesPerSecond; ++f)
        {
            for (size_t k = 0; k < (size_t) kNumAdaptiveBands; ++k)
                v.cutDb[k] = -4.0f * (0.5f + 0.5f * std::sin ((float) f / 90.0f + (float) k * 0.7f));
            v.hpfHz = 60.0f + 10.0f * std::sin ((float) f / 300.0f);
            v.gainDb = std::sin ((float) f / 200.0f);
            rec.write ((double) f / AdaptiveRecording::kFramesPerSecond, 0.02, v);
        }
        juce::MemoryBlock mb;
        const double first = bestOf (1, [&] { line->getStateInformation (mb); });
        const double repeat = bestOf (3, [&] { mb.reset(); line->getStateInformation (mb); });
        add ("state_save_first", first, "ms");
        add ("state_save_repeat", repeat, "ms");
        add ("state_size", (double) mb.getSize() / 1024.0, "kB");

        auto other = makeProc();
        other->setStateInformation (mb.getData(), (int) mb.getSize());
        AdaptiveRecording::Values a, b;
        const bool okA = rec.read (123.456, a), okB = other->getRecording().read (123.456, b);
        add ("state_roundtrip_ok", okA && okB && std::abs (a.cutDb[3] - b.cutDb[3]) < 1.0e-6f && std::abs (a.gainDb - b.gainDb) < 1.0e-6f ? 1.0 : 0.0, "bool");
    }

    //--- 5. Engine with 48 lines ---
    constexpr int numLines = 48;
    std::vector<std::unique_ptr<AmeProcessor>> lines;
    std::vector<juce::AudioBuffer<float>> sigs;
    std::vector<Runner> runners ((size_t) numLines);
    for (int i = 0; i < numLines; ++i)
    {
        lines.push_back (makeProc());
        sigs.push_back (makeSignal (100 + i));
        setP (*lines.back(), pid::role, (float) (1 + i % ((int) Role::NumRoles - 1)));
        setP (*lines.back(), pid::reaction, (float) (i * 2 % 100));
    }
    pump (300);
    for (int i = 0; i < numLines; ++i)
        runners[(size_t) i].run (*lines[(size_t) i], sigs[(size_t) i], 300);

    auto& hub = lines[0]->getHub();
    {
        MixEngine eng (hub);
        eng.prepare (48000.0);
        eng.step();
        std::vector<float> dump;
        for (auto& l : lines)
        {
            auto* d = hub.getAudioData (l->getSlot());
            for (auto& c : d->targetCutDb) dump.push_back (c.load());
            dump.push_back (d->targetHpfHz.load());
            dump.push_back (d->targetGainDb.load());
            for (auto& c : d->bandDb) dump.push_back (c.load());
            for (auto& c : d->bandAvgDb) dump.push_back (c.load());
            for (auto& c : d->displayDb) dump.push_back (c.load());
        }
        writeDump (outDir, "bench_engine.f32", dump);
        hub.engineHeartbeatMs.store (0);

        // 30 s of audio: 50 steps/s, the spectra of half the lines change per step (~25 analysis hops/s per line)
        const double ms = bestOf (3, [&]
        {
            for (int i = 0; i < 1500; ++i)
            {
                for (int l = i % 2; l < numLines; l += 2)
                    hub.getAudioData (lines[(size_t) l]->getSlot())->analysisCounter.fetch_add (1);
                eng.step();
            }
        });
        add ("engine_48_lines", ms / 300.0, "% of one core");
        add ("engine_step_48_lines", ms * 1000.0 / 1500.0, "us");
        hub.engineHeartbeatMs.store (0);
    }

    //--- 6. UI ---
    auto master = makeProc();
    setP (*master, pid::mode, (float) InstanceMode::Master);
    const auto masterSig = makeSignal (999);
    Runner masterRunner;
    auto feed = [&]
    {
        for (int i = 0; i < numLines; ++i)
            runners[(size_t) i].run (*lines[(size_t) i], sigs[(size_t) i], 1);
        masterRunner.run (*master, masterSig, 4);
    };
    for (int i = 0; i < 20; ++i) { feed(); pump (30); }
    add ("engine_alive", hub.isEngineAlive() ? 1.0 : 0.0, "bool");

    add ("msg_thread_49_instances_no_editor", uiTickCpuMsPerSec (feed), "CPU ms per s");

    fullStrip (*lines[0]);
    const juce::Rectangle<int> eqClip (20, 330, 800, 380);   // roughly the EQ display of a Line

    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (lines[0]->createEditor());
        ed->setSize (1400, 760);
        for (int i = 0; i < 10; ++i) { feed(); pump (40); }
        add ("ui_line_timers", uiTickCpuMsPerSec (feed), "CPU ms per s");
        add ("ui_line_paint_full", paintMs (*ed, {}, 20), "ms per frame");
        add ("ui_line_paint_eq_area", paintMs (*ed, eqClip, 40), "ms per frame");
        const auto f = outDir.getChildFile ("bench_ui_line.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (ed->createComponentSnapshot (ed->getLocalBounds()), os);
    }
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (master->createEditor());
        ed->setSize (1400, 760);
        for (int i = 0; i < 10; ++i) { feed(); pump (40); }
        add ("ui_exchange_48_timers", uiTickCpuMsPerSec (feed), "CPU ms per s");
        add ("ui_exchange_48_paint_full", paintMs (*ed, {}, 10), "ms per frame");
        add ("ui_exchange_48_paint_analyzer_area", paintMs (*ed, juce::Rectangle<int> (30, 380, 1000, 360), 10), "ms per frame");

        // without audio (transport stopped, host idle)
        pump (2500);
        add ("ui_exchange_48_timers_idle", uiTickCpuMsPerSec ([] {}), "CPU ms per s");
        const auto f = outDir.getChildFile ("bench_ui_exchange.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (ed->createComponentSnapshot (ed->getLocalBounds()), os);
    }

    outDir.getChildFile ("bench.csv").replaceWithText (report.joinIntoString ("\n") + "\n");
    return 0;
}
} // namespace bench
