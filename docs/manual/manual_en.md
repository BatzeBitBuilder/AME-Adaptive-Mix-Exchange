# AME – Adaptive Mix Exchange

**Operating Manual · Version 0.10.0** · built by **Batze the bit builder**

*Every track finds its aim.*

![Line view](images/ui_track_en.png)

## Overview

AME is a networked channel strip (VST3). Every instance on a track is called a **Line**. A single instance on the master bus works as the **Exchange** – like the switchboard of an old telephone exchange: it listens to all lines, notices who is talking over whom, and continuously assigns every track its place in the frequency spectrum and in the level balance.

- **Classic channel strip** per track: input, high/low-pass, 4-band EQ, compressor, pan, stereo width, output.
- **Analysis**: every Line measures its spectrum and shares it with all other instances in the project.
- **Adaptive engine** in the Exchange: works out from program (genre), roles and priority which track should make room where.
- **Dedicated adaptive stage** per Line: 10 octave cuts, high-pass and level. Your manual EQ settings are never touched.
- **Full control**: Direct (A/B comparison), Hold (freeze values), amount and response per track.

> AME helps you clean up a mix – it does not replace your ears. Treat its moves as suggestions, compare with **Direct** and freeze good results with **Hold**.

## Installation

1. Copy `AME Adaptive Mix Exchange.vst3` to `C:\Program Files\Common Files\VST3\` (administrator rights required).
2. If an older `MultiTrackMix.vst3` is there, delete it – both share the same plug-in ID.
3. Ableton Live → Settings → Plug-Ins → enable "VST3 Plug-In System Folders" → **Rescan**.
4. AME appears under the vendor **Batze the bit builder**.

Live Sets saved with earlier versions keep loading: parameter IDs and the state format are unchanged.

**Other DAWs:** AME runs in any Windows DAW with VST3 support (e.g. Cubase, Studio One, Reaper, FL Studio, Bitwig). The instances find each other through shared memory of the DAW session – even when the DAW runs plug-ins in separate processes (sandbox, bridge, e.g. Bitwig "Individually"). Tested so far with Ableton Live.

## Quick start

1. **Load AME on every track** that should take part. Mode: **Line (Track)**.
2. Choose a **Role** (e.g. Kick, Bass, Lead Vocal) – or leave it on **Auto-ID**.
3. **Put one AME instance on the master bus** and set its mode to **Exchange (Master)**.
4. In the Exchange, choose a **Program** (e.g. Rock / Band, Pop, Electronic).
5. Start playback. After a few seconds the **ENGINE** lamp lights up and the Lines start making room for each other.
6. Use **Direct (all)** in the Exchange to compare before/after. Happy with the result? Press **Hold (all)**.

> Without an Exchange every Line runs "solo": the channel strip works normally, but there is no adaptive processing.

## Concept: Lines and Exchange

| Term | Meaning |
|---|---|
| Line | AME instance on a track (mode "Line (Track)") |
| Exchange | AME instance on the master bus (mode "Exchange (Master)"), runs the engine |
| Program | target profile / genre preset for the whole mix |
| Role | function of a track in the arrangement (kick, bass, vocal …) |
| Priority | how important a track is compared with its program default |
| Response | how fast the processing follows (slow = balance, fast = ducking) |
| Direct | A/B bypass of the adaptive processing |
| Hold | freeze the current adaptive values |
| Patchbay | matrix showing which track masks which |

**How it works:**

- All instances in the same project find each other automatically (process-wide registry inside the plug-in DLL). Nothing needs to be connected.
- Every Line analyses its signal (FFT, 32 bands, short- and long-term spectrum) and shares it with the others.
- The Exchange computes target values for each Line 50 times per second and sends them back.
- Each Line moves smoothly towards these targets – at the response speed you set.

If there are several Exchange instances, only the **oldest** one is active; the others show "[EX–]" and a note in the status line.

## Header

The header looks the same in Line and Exchange mode:

- **AME logo with three lamps**:
  - **LINE** (green) – an Exchange is present.
  - **ENGINE** (amber) – the engine is running (heartbeat younger than 0.5 s).
  - **HOLD** (blue) – this Line is holding its values.
- **Status line**: number of Lines, Exchange status and hints.
- **DE / EN**: interface language. The choice applies immediately to all open AME windows and is remembered for future sessions. On the very first start AME follows the system language.
- **MANUAL**: opens this manual. Click a chapter on the left; **CLOSE** returns to the plug-in.
- **Mode**: Line (Track) or Exchange (Master).
- **Line**: role selector, the detected role when Auto-ID is active, **Mix Overview** button.
- **Exchange**: program, global amount, Direct (all), Hold (all), Release.

> Parameter names in the DAW (automation) are always English – regardless of the selected language.

## The Line (track view)

### Input
**Gain** (±24 dB) – input level before all stages.

### Filter
**HPF** (20 Hz–1 kHz) and **LPF** (1–20 kHz), 24 dB/octave each. The **HPF** / **LPF** lamps in the title switch the filters on.

### Compressor
Soft knee, stereo-linked. **Thresh**, **Ratio** (1:1–20:1), **Attack** (0.1–100 ms), **Release** (10–1000 ms), **Makeup** (0–24 dB). The **On** lamp enables it. The right-hand VU meter shows the gain reduction.

### Output
**Pan**, **Width** (0–200 %, 100 % = unchanged, 0 % = mono) and **Gain** (±24 dB).

### EQ with analyser
Four bands: low shelf, Mid 1, Mid 2 (peak with Q) and high shelf (±18 dB).

The display shows:

- **Spectrum** of the track as a rainbow area (before the engine),
- **After engine** as a green glowing line,
- **Peak** dotted (falls at 12 dB/s),
- the **EQ** curve (cream) with draggable points L, 1, 2, H, HP, LP,
- the **Engine** curve dashed (amber).

Handling: **drag** a point = frequency/gain, **mouse wheel** = Q, **double-click** = 0 dB. The **spectrogram** runs underneath.

### Adaptive
- **Amount** – strength of the engine's processing for this Line (0–100 %).
- **Response** – 0 % = slow (cut in about 1.5 s, release in about 3 s; for long-term balance), 100 % = fast (about 20 ms / 150 ms; ducking, e.g. bass making room for the kick). Default 30 %.
- **Priority** – Program (role default) or Very high / High / Low / Very low. Higher priority = other tracks make room for this one.
- **EQ / HPF / Gain** lamps – which kinds of processing are allowed.
- **Direct** – processing of this Line off (A/B, with a 30 ms crossfade).
- **Hold** – freeze the current values.

The LED display on the right shows the current cut per octave band (31 Hz–16 kHz, scale down to −9 dB), the adaptive high-pass and level correction, and the status (EXCHANGE CONNECTED, NO EXCHANGE, DIRECT, HOLD).

### VU meters and name plate
- **Output**: VU ballistics (0 VU = −18 dBFS RMS), peak lamp from −1 dBFS.
- **Compressor**: gain reduction 0 to −20 dB.
- **Name plate**: version, Line number, track name (from the DAW), role and active program.

## The Exchange (master)

![Exchange](images/ui_master_en.png)

The Exchange passes audio through unchanged and shows the **Mix Overview**. On Lines you can show it too with the **Mix Overview** button.

- **Program** – genre profile for the whole mix (see chapter Programs).
- **Amount** (global) – scales the processing of all Lines.
- **Direct (all)** – A/B comparison for the whole mix.
- **Hold (all) / Release** – switches Hold on all Lines.

### Mix Overview
The table lists all Lines:

| Column | Content |
|---|---|
| Line | name, track colour; [EX] = active Exchange, [EX–] = inactive |
| Role | selected role; "(A)" = detected automatically |
| Level | LED level chain |
| GR | compressor gain reduction |
| Masked | masking before > after (in %) |
| Masked by | main culprit and frequency range |
| Adaptive | current processing: gain, HP (high-pass), EQ (deepest cut), [DIR] / [HOLD] |

**Click a row** to select a track: the analyser then shows its masking as heat columns (yellow → red), its spectrum before the engine dashed, its peak dotted and its spectrogram.

### Analyser, patchbay, VU
- **Analyser**: all tracks as coloured areas, the **sum** as a cream line; with no selection the heat columns show the total occupancy per frequency.
- **Patchbay**: matrix of all Lines – the row is masked by the column. Brighter = stronger. Click to select a track.
- **VU Left/Right**: master level.

## Programs

A program defines, for every role, the priority per frequency region (sub / low / mid / high), target loudness and maximum high-pass – plus the limits of the engine:

| Program | max. cut | max. level correction | Character |
|---|---|---|---|
| Rock / Band | 6 dB | ±6 dB | vocal and snare up front, guitars in the mids |
| Pop (Vocal Focus) | 6 dB | ±6 dB | lead vocal clearly dominant |
| Electronic | 8 dB | ±6 dB | kick and bass carry the track, strong ducking possible |
| Hip-Hop | 8 dB | ±6 dB | kick/808 and voice in the centre |
| Acoustic / Singer-Songwriter | 4 dB | ±4 dB | gentle, natural balance |
| Metal | 7 dB | ±6 dB | wall of guitars, kick cuts through |

## Roles and Auto-ID

Available roles: Kick, Snare, Drums/Overheads, Percussion, Bass, Lead Vocal, Backing Vocals, Electric Guitar, Acoustic Guitar, Keys/Piano, Synth Lead, Pad, Strings, Brass, FX, Other.

With **Auto-ID** AME detects the role by itself:

1. **Track name** (instant, 100 %): German and English names plus abbreviations such as BD, SD, OH, Vox, BGV, Gtr, Ac Gtr, 808, Rhodes …
2. **Sound** (after about 3 s of signal): centroid, tonality, fundamental, transients, vibrato, crest factor, stereo width. Shown as "→ Role (xx %)".
3. **Hysteresis**: the role only changes with a clear lead (+15 %); below 25 % confidence it falls back to "Other".

A manually selected role always wins.

## How the engine works

1. **Find conflicts**: in each of the 32 bands the engine checks where two tracks carry energy at the same time (up to 18 dB level difference, fully from 6 dB).
2. **Compare priority**: the track with the lower priority makes room. If both are equal, both give a little.
3. **Ducking**: with a fast response the cut follows the short-term level of the more important track – it only happens while that track is actually playing.
4. **High-pass**: below a track's useful signal (30 dB under its maximum) the engine sets a high-pass, at most up to the role's limit.
5. **Level**: tracks are brought to the program's role ratios – **the total power stays constant**, so the mix does not simply get louder or quieter.
6. **Output**: the cuts are mapped to 10 octave bands (31 Hz–16 kHz); each Line follows them with its response (HPF and gain with about 2 s).

The engine **only cuts** – it never boosts frequencies. Your manual EQ is never changed; the adaptive stage sits after compressor and EQ, before pan/width.

> Data from other tracks is at least one audio block old (the DAW processes tracks in parallel). That is why AME works smoothed rather than sample-accurate.

## Direct and Hold

- **Direct** (Line) or **Direct (all)** (Exchange) switches off the adaptive processing only. The manual channel strip keeps working. Ideal for A/B comparisons – automatable.
- **Hold** freezes the current adaptive values. They are saved with the Live Set and stay in effect even without an Exchange or with a different program.
- **Hold (all) / Release** in the Exchange switches Hold on all Lines.

Typical workflow: choose a program → let the song play a few times → compare with Direct → Hold (all) → remove the Exchange if you like.

## Workflow tips

- **Setting roles** pays off: Auto-ID is good, a deliberate choice is better.
- Change **Priority** only where the program does not fit (e.g. guitar solo: Very high).
- **Slow response** for pads, guitars, keys; **fast** for bass against kick or pads against vocals.
- Lower **Amount** per track if a track is processed too much; globally in the Exchange for the whole mix.
- In the **patchbay**, look at the brightest cells first – that is where the biggest conflicts are.
- On buses (e.g. a drum bus) AME can work as one Line instead of on every single track.

## Troubleshooting

| Symptom | Cause / solution |
|---|---|
| "No Exchange – line runs solo" | No AME instance in Exchange mode on the master bus. |
| ENGINE lamp off | The Exchange is loaded but not being processed (device deactivated, master track frozen). |
| "n exchanges – only the oldest is active" | Several Exchange instances: switch the extra ones to Line or remove them. |
| No processing although the engine runs | Direct on? Amount at 0? EQ/HPF/Gain lamps off? Hold on? |
| Track missing from the overview | The status line tooltip shows the link: "this process only" = shared memory not available → run plug-ins in the same process. If the instance runs in a helper process AME does not attribute to the DAW, set the same environment variable `AME_HUB_SESSION` for all processes. |
| Track stays listed after a plug-in process crashed | Removed automatically within one second. |
| Auto-ID wrong | Give tracks clear names or set the role manually. |
| Track names "Line 01" … | The DAW does not send a name – set the role manually. |

## About AME

**AME – Adaptive Mix Exchange**, version 0.10.0

Built by **Batze the bit builder**.

- Framework: JUCE 8 (C++20), VST3 format, Windows (tested with Ableton Live).
- Fonts: Abril Fatface, Bebas Neue, Courier Prime – SIL Open Font License.
- **License: GNU Affero General Public License v3 (AGPLv3)** – free, open software. You may use, modify and share AME, also commercially. Anyone distributing a modified version must publish its source code under the AGPLv3 as well. AME comes without any warranty.
- Source code: [github.com/BatzeBitBuilder/AME-Adaptive-Mix-Exchange](https://github.com/BatzeBitBuilder/AME-Adaptive-Mix-Exchange)
- Third-party components: JUCE (AGPLv3), VST 3 SDK (MIT, Steinberg), fonts (OFL) – see THIRD_PARTY_NOTICES.md. VST is a trademark of Steinberg Media Technologies GmbH.
- Name and imagery: a telephone exchange from the valve era – every track is a line, the Exchange is the switchboard.

*Every track finds its aim.*
