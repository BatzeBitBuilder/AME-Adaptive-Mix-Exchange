# AME – Adaptive Mix Exchange

*Every track finds its aim.*

A networked, adaptive channel strip plug-in (VST3, Windows x64) by **Batze the bit builder**.

Put AME on your tracks (**Line**) and one instance on the master bus (**Exchange**). All instances in the project find each other automatically. The Exchange listens to every line, detects which tracks mask each other and continuously gives every track its place in the frequency spectrum and in the level balance – while each Line remains a full channel strip you control by hand.

📖 **Manual:** [English](docs/manual/manual_en.md) · [Deutsch](docs/manual/handbuch_de.md) – also built into the plug-in (**MANUAL** button).
⬇️ **Download:** see [Releases](../../releases).

## Features
- **Full channel strip** per track: input and phase, HPF/LPF (12/24/48 dB/oct), 4-band EQ (low/high shelf or bell), gate/expander, compressor (knee, mix, sidechain HPF, auto release), de-esser, saturation/exciter, width/pan, output, limiter/clipper; EQ/compressor order selectable
- **Adaptive engine** with 6 programs (Rock/Band, Pop, Electronic, Hip-Hop, Acoustic/Singer-Songwriter, Metal), track roles, priority and response time per track; it only cuts and keeps the total level constant – your manual EQ is never touched
- **Auto role detection** from the track name and the sound
- **Exchange monitor**: Full / No Engine / Raw with level match; Direct (A/B) and Hold per track
- **Export = monitor**: engine on the audio clock; **Recording** captures the adaptive processing along the song and replays it exactly – reproducible exports, also for stems
- **Presets**: factory presets per role, user presets, A/B compare
- **Visualisation**: analyser, spectrogram, masking patchbay, mix overview, VU meters
- Vintage “valve gear” interface, freely scalable (50–200 %), **English/German** UI

## Compatibility
- Works in **all common VST3 hosts on Windows (64-bit)**, e.g. Ableton Live, Cubase/Nuendo, Studio One, Reaper, Bitwig Studio, FL Studio and Cakewalk.
- Tested with Ableton Live.
- Instances communicate via shared memory, so they also find each other when the DAW runs plug-ins in separate processes.
- Not available: AU (Logic), AAX (Pro Tools), macOS and Linux builds.

## Installation
1. Download the ZIP from [Releases](../../releases) and unzip it.
2. Copy `AME Adaptive Mix Exchange.vst3` to the system VST3 folder `C:\Program Files\Common Files\VST3\` (administrator rights required).
3. Rescan plug-ins in your DAW. AME appears under the vendor **Batze the bit builder**.

## Quick start
1. Load AME on every track that should take part – mode **Line (Track)** – and choose a **Role** (or leave it on **Auto-ID**).
2. Load one AME on the **master bus** and set its mode to **Exchange (Master)**.
3. Choose a **Program** in the Exchange and start playback.
4. Compare with the **Monitor** buttons (Full / No Engine / Raw).
5. Happy with it? **Record (all)**, play the song once, **Playback (all)** – exports now sound exactly like the monitor.

Without an Exchange every Line works as a normal channel strip without adaptive processing. Details: see the [manual](docs/manual/manual_en.md).

## Language and settings
Switch the interface language with **DE / EN** in the header; the choice applies to all instances immediately. Settings and user presets are stored per user in `%APPDATA%\AME\`. Parameter names shown in the DAW are always English.

## Building from source
Requirements: Windows, Visual Studio 2022 or newer with the “Desktop development with C++” workload, CMake ≥ 3.22, Git.

```bat
git clone https://github.com/BatzeBitBuilder/AME-Adaptive-Mix-Exchange.git
cd AME-Adaptive-Mix-Exchange
cmake -B build -A x64
cmake --build build --config Release
```

JUCE 8.0.10 is downloaded automatically on first configure (or use a local checkout: `-DAME_JUCE_PATH=<path-to-JUCE>`).
Result: `build\AME_artefacts\Release\VST3\AME Adaptive Mix Exchange.vst3`

### Smoke test (no DAW required)
```bat
cmake -B build -A x64 -DAME_BUILD_TESTS=ON
cmake --build build --config Release --target AMESmokeTest
build\AMESmokeTest_artefacts\Release\AMESmokeTest.exe
```

Release builds are produced by GitHub Actions (`.github/workflows/release.yml`).

## Project structure
```
Source/
  PluginProcessor.*      audio processing, hub connection
  PluginEditor.*         main window (header, strip / overview)
  Parameters.*           parameter IDs and layout
  Roles.h                roles, programs, modes (names EN/DE)
  Settings.*             per-user settings
  i18n/Lang.*            UI language, tr("de", "en")
  hub/MixHub.*           instance registry in shared memory (cross-process, lock-free audio data)
  hub/MaskingModel.*     masking matrix
  dsp/ChannelStrip.*     strip DSP: phase, filters, gate, EQ, compressor, de-esser, saturation, adaptive stage, width/pan, limiter
  dsp/StripParams.*      parameter cache
  dsp/SpectrumAnalyzer.* FFT analysis (32 bands)
  engine/MixEngine.*     adaptive engine (Exchange, 50 Hz)
  engine/Profiles.*      program × role: priorities, target loudness, HPF
  engine/RoleClassifier.*  role detection (name, sound features, hysteresis)
  engine/StripPresets.*  factory presets, user presets, A/B
  ui/                    look and feel, fonts, meters, spectrogram, EQ graph, patchbay, panels, manual viewer
Resources/Fonts/         embedded fonts (OFL)
docs/manual/             manual EN/DE (Markdown, embedded in the plug-in) and screenshots
Tests/                   smoke test
```

Changes: see [CHANGELOG.md](CHANGELOG.md) · Contributing: see [CONTRIBUTING.md](CONTRIBUTING.md).

## License
AME is free software under the **GNU Affero General Public License v3** – see [LICENSE](LICENSE).
Copyright (C) 2026 Batze the bit builder.

You may use, modify and share AME, also commercially. If you distribute a (modified) version, you must make its complete source code available under the AGPLv3. AME comes without any warranty.

JUCE is used under its AGPLv3 option; VST 3 SDK (MIT) and fonts (SIL OFL) – see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). VST is a trademark of Steinberg Media Technologies GmbH.
