# Changelog

## 1.1.0 – 2026-11-09 (landscape layout, folding sections)
- **Landscape window**: base size 1400 × 760 instead of 1100 × 960 – fits 1080p screens, also with 125 % Windows scaling; still scalable 50–200 % with a fixed aspect ratio
- Line: ADAPTIVE moved next to the EQ (vertical column), status and applied values on two lines; VU meters and type plate more compact
- Exchange: patchbay, VU left/right and spectrogram in a right-hand column; line list and analyser on the left
- **Window size menu** in the header (50–200 %), usable even when the resize corner is off-screen
- **Fit to screen**: if the window is larger than the work area of the monitor it is on (e.g. size chosen on an ultrawide, then moved to a 1080p screen), it shrinks automatically – also after moving; this automatic size is not stored
- **Folding sections** (triangle): Line – ADAPTIVE and spectrogram; Exchange – line list and DETAILS column. The freed space goes to the EQ or analyser; state is global and remembered (user settings)
- Smoke test: folded screenshots, aspect ratio and stored fold state

## 1.0.2 – 2026-11-09 (export = monitor: audio-clocked engine, recording)
- **Engine on the audio clock**: the Exchange computes one engine step per 20 ms of audio in its audio thread (previously a 50 Hz wall-clock timer). Offline exports – faster or slower than real time – now behave like playback. The engine step is real-time safe (no locks, no allocations; registry copy per slot in the hub)
- **Recording** (new, per Line: Live / Record / Playback, parameter “Recording”, not automatable): captures the applied adaptive values (10 cuts, high-pass, level) along the DAW song position at 50 frames/s and replays them exactly – independent of engine, Exchange and render speed, also for single-track (stem) exports. Saved compressed with the project (up to 30 min); gaps up to 10 s are bridged
- Exchange: recording bar with **Record (all)**, **Playback (all)**, **Live (all)**, **Clear** and a status summary
- **Export warning**: a Line exported without a running engine and without a recording (detected via the host's offline flag or audio running faster than real time) shows “EXPORT WITHOUT EXCHANGE” for 5 minutes; the Exchange counts affected Lines
- Offline export: longer heartbeat tolerance (10 s) so slow renders do not drop the engine; host bypass of the Exchange clears the heartbeat immediately
- Hub layout v4
- Manual DE/EN: new chapter “Export and recording”, quick start, troubleshooting
- Smoke test: engine steps per audio time, offline flag, recording/playback without Exchange (identical values, offline and real time), project state round trip, export warning, clear; cross-process test keeps the Exchange running until the child finishes

## 1.0.1 – 2026-11-09 (housekeeping)
- README, CHANGELOG, contribution guide, issue and pull request templates in English
- Manual DE/EN: installation steps for any VST3 host (no longer Ableton-specific), “project” instead of “Live Set”
- Internal names cleaned up: classes `AmeProcessor` / `AmeEditor`, namespace `ame`, macros and CMake options `AME_*` (`AME_JUCE_PATH`, `AME_BUILD_TESTS`); bundle ID `com.batzebitbuilder.ame`
- Plug-in ID (manufacturer/plug-in code), parameter IDs and state format unchanged – existing projects keep loading
- Internal planning documents removed from the public repository

## 1.0.0 – 2026-11-09 (first release)
- First public version, functionally identical to 0.14.0
- Windows build (VST3, x64) via GitHub Actions; ZIP with plug-in, manual, LICENSE and THIRD_PARTY_NOTICES attached to the release

## 0.14.0 – 2026-11-09 (Exchange monitor, level match, Exchange deactivation fix)
- **Monitor in the Exchange** (replaces “Direct (all)”): **Full** / **No Engine** (manual channel strips only, engine off) / **Raw** (all Lines unprocessed – as without plug-ins); 30 ms crossfade, analysis keeps running; automatable (parameter “Monitor”), always reset to Full on load
- **Level match** for Raw: the Exchange compensates the loudness difference to the processed version at its output (power before/after every Line, ±12 dB, shown in the status line)
- **Fix**: Exchange deactivated in the DAW or host bypass → engine stops (acts like “No Engine”); previously the engine kept running. Detected via an audio heartbeat (300 ms) and `processBlockBypassed`; Lines show “Exchange deactivated”
- Lines drop engine processing immediately when the engine stops (heartbeat cleared)
- Status line and Line display show Raw / No Engine / Exchange deactivated
- Hub layout v3 (monitor, level match, power per Line)
- Manual DE/EN: Exchange chapter, Direct and Hold, quick start
- Smoke test: Raw = original (bit-exact), level match at the Exchange output, monitor not saved, host bypass and missing processing stop the engine

## 0.13.0 – 2026-11-08 (limiter/clipper, user presets, A/B)
- **Limiter / clipper** (new rack module, after the output gain): zero-latency limiter (instant attack, release), soft clip, hard clip; ceiling −12…0 dB, no sample above it; reduction meter
- **User presets**: save/load/delete in the preset bar, stored in `%APPDATA%\AME\Presets\*.amepreset` (XML, plain values, version tag), available in all projects; cover all strip settings incl. input, phase, output and limiter
- **A/B compare** of settings: slots A and B, “A→B” copies the active setting; saved with the project
- Manual DE/EN: new chapter “The Line” (signal flow, presets/A-B, module rack, EQ types, window size), workflow tips
- Disabled buttons are shown dimmed
- Smoke test: limiter/soft/hard clip (peak ≤ ceiling), user presets (save/load/delete), A/B incl. project state; screenshots of all rack modules

## 0.12.0 – 2026-11-08 (saturation/exciter, EQ extension, processing order)
- **Saturation / exciter** (new rack module): tube (asymmetric, even harmonics), tape (soft), transistor (harder); drive 0–24 dB with automatic gain compensation (small signals unchanged), mix; exciter (even harmonics from the high band only, 2–12 kHz)
- **EQ extended**: low and high band as shelf or bell, new controls Low Q / High Q (also via mouse wheel in the graph)
- **Filter slopes** HPF/LPF selectable 12 / 24 / 48 dB/oct (Butterworth)
- **Order** EQ → compressor or compressor → EQ
- EQ graph shows slopes and band types exactly (shared calculation with the DSP)
- Factory presets extended: saturation on kick, snare, drums, bass, lead vocal, keys; exciter on lead vocal and acoustic guitar; snare low band as bell
- Older projects: defaults match the previous sound (shelf, Q 0.71, 24 dB/oct, EQ → comp, saturation off)
- Smoke test: slopes, bell/shelf, order, saturation (small signal/limiting), exciter

## 0.11.0 – 2026-11-08 (factory presets per role)
- **Factory presets per role** (16): starting points for filter, EQ, gate, compressor and de-esser, e.g. kick (gate + punch EQ), lead vocal (3.5:1, auto release, de-esser), bass (4:1); input, phase and output stay unchanged
- **Preset bar** above the module rack: factory preset selection, name of the loaded preset (saved with the project)
- **“Role preset”**: loads the factory preset of the selected or auto-detected role with one click
- Presets set parameters with gestures → undoable in the DAW / recordable as automation
- Wider logo area (version number)
- Smoke test: all preset values in range, loading via the detected role

## 0.10.0 – 2026-11-08 (phase, gate/expander, de-esser, compressor extension, scalable window)
- **Phase invert** Ø L / Ø R in the INPUT section
- **Gate / expander** (new): modes gate (3 dB hysteresis), expander 1:2, 1:4; threshold, range, attack, hold, release, sidechain high-pass
- **De-esser** (new): frequency 2.5–12 kHz, threshold, range, mode split (band only, Linkwitz-Riley crossover) or wide (whole signal), listen
- **Compressor extended**: knee (0–18 dB), mix (parallel compression), sidechain high-pass, auto release (program-dependent)
- Signal chain: input → phase → HPF/LPF → gate → EQ → compressor → de-esser → [analysis] → adaptive → width/pan → output
- **Module rack** with tabs (gate, compressor, de-esser), lamp per tab = module on, LED reduction meter; last tab remembered in the project
- **Freely scalable window** (50–200 %, fixed aspect ratio, base size 1100 × 960, bottom-right corner); size stored globally (`uiScale` in AME.settings)
- OUTPUT section moved to the right column
- Parameter values read more efficiently (pointer cache `StripParamReader`)
- Central user settings (`Settings.*`)
- Older projects: new parameters with neutral defaults (modules off, knee 6 dB as before, mix 100 %)
- Hub layout v2 (gate/de-esser/limiter reduction) – v0.9 and v0.10 do not share a hub
- Smoke test: phase, gate, expander, compressor (mix, SC-HPF, auto release), de-esser

## 0.9.0 – 2026-11-08 (cross-process hub)
- MixHub now lives in named shared memory of the DAW session (Windows `CreateFileMapping` + named mutex, POSIX `shm_open` + robust process mutex) → instances see each other even in separate plug-in processes (Bitwig sandbox, Reaper “separate process”, FL bridge …)
- Session = DAW process: helper processes (plug-in host, audio engine, bridge) are traced back to the DAW in the process tree; several DAWs running at the same time stay separate; override via environment variable `AME_HUB_SESSION`
- Instances of crashed processes are unregistered automatically after ≤ 1 s
- Fallback: without shared memory, in-process only as before; different AME versions do not share memory (layout version in the name)
- Status line tooltip shows the connection (shared memory, session, number of processes)
- Manual: other DAWs, troubleshooting extended
- Smoke test: child process with its own Line (analysis, role, engine targets across the process boundary), crash cleanup

## 0.8.1 – 2026-11-08 (open source, AGPLv3)
- License: GNU AGPLv3 (`LICENSE`), JUCE under its AGPLv3 option
- `THIRD_PARTY_NOTICES.md`: JUCE (AGPLv3), VST 3 SDK (MIT), fonts (OFL), VST trademark notice
- Copyright/SPDX header in all source files (`AGPL-3.0-only`)
- Manual/README: license section, link to the source code

## 0.8.0 – 2026-11-08 (English interface, manual)
- UI language German/English, **DE / EN** switch in the header; applies to all instances immediately, stored in the user settings file (`AME/AME.settings`), first start = system language
- All UI texts and names of roles, programs, priorities and modes in both languages (i18n/Lang, `tr("de", "en")`)
- Parameter names in the DAW now always English (Mode, Role, Priority, Response, Direct, Program …); parameter IDs unchanged
- Built-in manual (**HANDBUCH / MANUAL** button): table of contents, 14 chapters, vintage style; source `docs/manual/handbuch_de.md` / `manual_en.md` (embedded, readable on GitHub)
- Developer/vendor: **Batze the bit builder**
- Tooltips (status line, manual, language)
- Header: master buttons slightly more compact
- Smoke test: language switch, parameter names, chapter count DE = EN; new screenshots

## 0.7.0 – 2026-11-08 (renamed to AME – Adaptive Mix Exchange)
- New name AME (Adaptive Mix Exchange), telephone-exchange metaphor
- Header: “AME” logo + “ADAPTIVE MIX EXCHANGE”, three lamps LINE / ENGINE / HOLD, version
- Name plate: “AME MODEL x.y.z · LINE nn”, name, role, program
- Terms: track instance → Line, master → Exchange, preset → Program, priority → Priority (Vorrang), reaction → Response, bypass → Direct, freeze → Hold, role auto → Auto-ID, masking matrix → Patchbay
- Default name of new instances “Line 01” …
- CMake: project/target AME, VST3 “AME Adaptive Mix Exchange.vst3”, smoke test AMESmokeTest
- Plug-in ID, parameter IDs and state format unchanged

## 0.6.0 – 2026-11-08 (vintage UI)
- New design: cream front with walnut cheeks, Bakelite header and knobs (knurled, scale, amber value arc), indicator lamps as switches, screws, brass name plate
- Embedded fonts (OFL): Abril Fatface (name plate), Bebas Neue (engraving), Courier Prime (values/displays)
- Analyser in measuring-instrument style (phosphor green, grid, glow lines)
- Frequency display: 240 points (~1/12 octave) instead of 32 bands; the engine still works with 32 bands
- Track: rainbow fill by frequency, line after engine, peak hold (dotted, 12 dB/s), spectrogram below the EQ
- Mix overview: tracks as coloured areas + glow lines, sum (master) as cream line, masking heat columns (yellow → red), peak hold and spectrogram of the selection, masking matrix (clickable), LED level chains
- Analogue VU meters (amber, VU ballistics, peak lamp): track = output + gain reduction, master = L/R
- Adaptive display as LED columns
- Window size 1000 × 880

## 0.5.0 – 2026-11-08 (automatic role detection)
- Automatic role detection for role “Auto” (manual choice wins)
  1. Track name (German/English, abbreviations such as BD, SD, OH, Vox, BGV, Gtr, Ac Gtr, 808, Rhodes …) – instant, 100 %
  2. Sound features after ≥ 3 s of signal: centroid, flatness, energy < 60 / < 150 Hz / > 4 kHz, onsets/s, harmonicity + fundamental (FFT autocorrelation), vibrato (cent deviation), crest (~1.5 s), stereo width → prototype scoring per role, softmax confidence
  3. Hysteresis: change only with a +15 % lead; below 25 % confidence → “Other”
- Engine and profiles use the detected role
- UI: “→ role (xx %)” or “(track name)” next to the role selector; mix overview marks detected roles with “(A)”
- Smoke test: 23 track names, 6 synthetic signals (kick, snare, hi-hats, bass, vocal, pad)

## 0.4.0 – 2026-11-08
- Level correction keeps the total power of all tracks constant (iterative, ±6 dB; acoustic ±4 dB)
- EQ graph in the strip view: own spectrum (area before / line after engine), manual EQ curve with draggable points (L, 1, 2, H, HP, LP; mouse wheel = Q, double-click = 0 dB), adaptive curve dashed
- A/B bypass of adaptive processing per track (automatable) and globally in the master; switching with 30 ms crossfade
- Freeze per track: holds the current engine values (independent of master/preset), saved with the project; master: “freeze all” / “release all”
- New programs: Hip-Hop, Acoustic / Singer-Songwriter, Metal
- Mix overview: markers [fix] / [bypass]
- Window size 1000 × 800

## 0.3.0 – 2026-11-08 (adaptive engine)
- MixEngine in the active master (own thread, 50 Hz) with heartbeat in the hub; no processing without a master
- Profiles per program × role: priority per region (sub/low/mid/high), target loudness, max. high-pass; max. cut per genre (Electronic 8 dB, otherwise 6 dB)
- Conflict logic: the lower priority makes room where two tracks compete in the same band (up to 18 dB difference); equal priority = both a little
- Adaptive stage per track (after compressor, before pan): high-pass 12 dB/oct, 10 octave peak bands (31 Hz–16 kHz, cuts only), level (−6 / +3 dB)
- Analysis now before the adaptive stage (no feedback)
- New track parameters: priority (program / very high … very low), response (slow = balance … fast = ducking), switches EQ / HPF / gain; amount in the “Adaptive” section
- Strip UI: “Adaptive” section showing current cuts, HPF, gain, engine status
- Mix overview: masking before→after, “Adaptive” column, spectra after engine, selected track additionally dashed before engine
- Window size 1000 × 700

## 0.2.0 – 2026-11-08 (analysis and masking)
- SpectrumAnalyzer: FFT 4096/hop 2048, Hann, 32 log bands 20 Hz–20 kHz (~1/3 oct), post-fader, allocation-free in the audio thread
- Hub: short-term spectrum (display) + long-term spectrum (~3 s, masking) per instance
- MaskingModel: matrix m(masker→track, band) from level difference, band relevance and neighbour-band spreading; masking degree, main masker and frequency range per track; hotspots per band
- Mix overview: columns “Masked” and “Masked by”, spectrum overlay of all tracks with hotspot markers; clicking a row shows the masking of that track
- Track colours: DAW colour, otherwise palette
- Smoke test: test signals (kick/bass/vocal), checks analysis and masking

## 0.1.0 – 2026-11-08 (project setup)
- Project skeleton: CMake + JUCE 8.0.10, VST3
- MixHub: process-wide registry of all instances (up to 128), lock-free level/GR data, slots prepared for analysis and adaptive data
- Modes track / master; with several masters the oldest is active
- Master writes the program (Rock/Band, Pop vocal focus, Electronic) and global amount to the hub
- ChannelStrip: input, HPF/LPF 24 dB/oct, EQ (low shelf, 2× peak, high shelf), compressor (soft knee, stereo-linked), width, pan, output; smoothed parameters
- Hook for adaptive EQ offsets
- Track name/colour from the DAW (if provided by the host)
- UI: strip view with meter/GR, overview of all instances, status line
- Smoke test without DAW
