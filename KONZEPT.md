# AME (Adaptive Mix Exchange) – Konzept

Stand: 2026-11-08 · Version 0.13.0 (Roadmap 1–4, 6a + Vintage-UI, AME, DE/EN, Handbuch, AGPLv3, Shared-Memory-Hub, vollständiger Channelstrip mit Presets)
Entwickler: Batze the bit builder

## Ziel
VST3-Channelstrip, dessen Instanzen sich gegenseitig kennen. Anhand eines globalen Presets (Zielprofil) werden alle Spuren analysiert und adaptiv so bearbeitet, dass jede ihren Platz im Mix bekommt.

## Festgelegte Entscheidungen
| Thema | Entscheidung |
|---|---|
| Name | AME – Adaptive Mix Exchange (Vermittlungs-Metapher, siehe BRANDING.md); UI-Begriffe Line/Exchange/Programm/Vorrang/Ansprechzeit/Direkt/Hold |
| Entwickler | Batze the bit builder (VST3-Hersteller) |
| Lizenz | Open Source, GNU AGPLv3 (AGPL-3.0-only); JUCE unter dessen AGPLv3-Option, VST3 SDK MIT, Schriften OFL (THIRD_PARTY_NOTICES.md) |
| Repository | GitHub BatzeBitBuilder/AME-Adaptive-Mix-Exchange (derzeit privat) |
| Framework | JUCE (C++), CMake, Format VST3 |
| Plattform / DAW | Windows, jede VST3-DAW (getestet: Ableton Live) |
| Kommunikation | Hub im Named Shared Memory der DAW-Sitzung (prozessübergreifend), Fallback In-Process |
| Modus | Adaptive (kontinuierliche, dynamische Anpassung) |
| Rollen | manuell wählbar oder „Auto“: Spurname → Klangmerkmale (regelbasiert); ML später optional |
| Presets | Rock / Band, Pop (Vocal-Fokus), Electronic, Hip-Hop, Akustik / Singer-Songwriter, Metal |
| Konfliktauflösung | Priorität aus Preset × Rolle × Frequenzregion, pro Spur verschiebbar (Sehr hoch … Sehr niedrig) |
| Eingriffe | eigene adaptive Stufe: 10 Oktav-Absenkungen + Hochpass + Lautstärke; manueller EQ bleibt unberührt |
| Channelstrip | Input + Phase, HPF/LPF 12/24/48, Gate/Expander, 4-Band-EQ (Low/High Shelf/Glocke), Kompressor (Knee, Mix, SC-HPF, Auto-Release), De-Esser (Split/Wide), Sättigung (Röhre/Band/Transistor) + Exciter, Limiter/Soft/Hard Clip; Reihenfolge EQ/Comp wählbar; Modul-Rack mit Reitern |
| Presets | Werkspresets je Rolle (Klang-Module), „Preset der Rolle“ (Auto-Kennung), eigene Presets (%APPDATA%\AME\Presets), A/B im Projekt |
| Fenster | frei skalierbar 50–200 % (Basis 1100 × 960), Größe global gespeichert |
| Lautstärke | Rollen-Verhältnisse, Summenleistung bleibt konstant |
| Kontrolle | A/B-Bypass (Spur/global), Einfrieren (Spur/alle, im Projekt gespeichert), EQ-Grafik |
| Reaktion | pro Spur stufenlos: langsam (Langzeit-Balance) … schnell (Ducking) |
| Design | Vintage „Röhren-Gear“: Creme/Walnuss/Bakelit/Bernstein; Analyzer im Messgeräte-Stil (Phosphor); Schriften Abril Fatface, Bebas Neue, Courier Prime (OFL) |
| Analyse-Anzeige | 240 Punkte (~1/12 Okt.), Regenbogen-/Spurfarben, Peak-Hold, Spektrogramm, Masking-Wärmesäulen + Matrix, Zeiger-VU |
| Engine-Master | eigene Instanz auf dem Master-Bus (Modus „Exchange“), zeigt Mix-Übersicht + Programmwahl; bei mehreren gilt die älteste |
| Sprache | UI Deutsch/Englisch, global umschaltbar (DE/EN), gespeichert in Benutzer-Einstellungen, Start = Systemsprache; DAW-Parameternamen fest englisch |
| Handbuch | Markdown DE/EN in docs/manual, ins Plugin eingebettet (HANDBUCH/MANUAL), auf GitHub lesbar |

## Architektur
### 1. Kommunikation (SharedHub)
- `MixHub` pro Prozess (SharedResourcePointer), Daten (Registry + Analyse) im Named Shared Memory `AME_Hub_v<Layout>_<Größe>_<Sitzung>` (Windows `CreateFileMapping` + benannter Mutex; POSIX `shm_open` + robuster Mutex).
- Sitzung = DAW-PID (Hilfsprozesse wie Plugin-Host/Audio-Engine/Bridge werden zurückverfolgt), Override `AME_HUB_SESSION`; Slots toter Prozesse werden aufgeräumt; Fallback lokal.
- Datenaustausch lock-free (Atomics), kein Locking im Audio-Thread; Sperre nur für die Registry (Message-Thread).
- Daten anderer Spuren sind ≥1 Block alt (DAW verarbeitet Spuren parallel) → Regelung mit Glättung (Attack/Release), kein samplegenaues Ducking.

### 2. Analyse (pro Instanz)
- FFT → Bandenergien (1/3-Oktave bzw. ~32 Mel-Bänder), Lautheit (LUFS short-term), Crest-Faktor, Transientendichte, Stereobreite/Korrelation.
- Rollenerkennung: Start mit Feature-Heuristik, später ML-Klassifikator (z. B. ONNX Runtime).

### 3. Engine (Hintergrund-Thread, eine Instanz = Master)
- Masking-Matrix: pro Band, welche Spur welche verdeckt.
- Preset = Zielprofil: Zielspektrum je Rolle, Lautheitsverhältnisse, Prioritäten, Pan-/Breitenregeln.
- Ausgabe je Spur: Zielwerte für Gain, Filter, dynamische EQ-Absenkungen (bei niedrigerer Priorität), Kompression, Pan/Breite.
- Adaptive: dynamische EQ-Bänder folgen dem Pegel der maskierenden Spuren (geglättet).

### 4. DSP-Kette pro Strip
Input → Phase → HPF/LPF → Gate/Expander → EQ ⇄ Kompressor → De-Esser → Sättigung/Exciter → [Analyse] → adaptive Stufe → Width/Pan → Output → Limiter/Clipper

### 5. UI
- Line: Input/Filter/Kompressor/Output, EQ mit Analyzer + Spektrogramm, Adaptiv-Sektion, VU Ausgang + GR, Typenschild.
- Exchange/Mix-Übersicht: Line-Liste, Analyzer aller Spuren + Summe, Spektrogramm, VU L/R, Patchfeld; Programm, Amount, Direkt, Hold.
- Kopf: Sprache DE/EN, HANDBUCH (Overlay mit Inhaltsverzeichnis).

## Roadmap
1. ✅ Projektgerüst (JUCE/CMake), Strip-DSP, MixHub (In-Process), Instanzen in Übersicht sichtbar (v0.1.0)
2. ✅ Analyse + manuelle Rollen + Masking-Matrix + Visualisierung (v0.2.0)
3. ✅ Adaptive Engine mit 3 Presets (v0.3.0)
   ✅ Summenpegel konstant, EQ-Grafik, Bypass/Einfrieren, 3 weitere Presets (v0.4.0)
4. ✅ Automatische Rollenerkennung: Spurname + Klangmerkmale (v0.5.0)
   ✅ Vintage-UI (v0.6.0), Umbenennung AME (v0.7.0)
   ✅ Englische Oberfläche (umschaltbar) + Handbuch DE/EN (v0.8.0)
5. Veröffentlichung auf GitHub: ✅ privates Repo (v0.8.0), ✅ Lizenz AGPLv3 (v0.8.1); offen: Repo öffentlich, Release-Build, ggf. GitHub Actions/Pages
6. ✅ Shared-Memory-Hub prozessübergreifend (v0.9.0); offen: Feinschliff, Performance
7. ✅ Vollständiger Channelstrip: Phase, Gate, De-Esser, Comp-Erweiterung, skalierbares Fenster (v0.10.0), Werkspresets je Rolle (v0.11.0), Sättigung/Exciter, EQ-Typen, Flanken, Reihenfolge (v0.12.0), Limiter/Clipper, eigene Presets, A/B (v0.13.0)

## Offene Punkte
- v0.9.0 in Ableton getestet ✅; v0.13.0 unter Windows bauen und testen (neue Module, Presets, A/B, Skalierung)
- Werkspresets mit echtem Material feinjustieren; ggf. Oversampling für Sättigung/Clipper
- Nächster Schritt: weitere Entwicklung; später Repo öffentlich + Release
- Profilwerte weiter mit echtem Material feinjustieren
- Rollenerkennung mit echtem Material prüfen (Prototypen in RoleClassifier.cpp justieren); optional ML-Klassifikator
- Später: LUFS-Anzeige, Stereo-Korrelation/Goniometer, skalierbares Fenster
