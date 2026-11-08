# Changelog

## 0.10.0 – 2026-11-08 (Phase, Gate/Expander, De-Esser, Kompressor-Erweiterung, skalierbares Fenster)
- **Phasenumkehr** Ø L / Ø R in der INPUT-Sektion
- **Gate / Expander** (neu): Modi Gate (3 dB Hysterese), Expander 1:2, 1:4; Threshold, Range, Attack, Hold, Release, Sidechain-Hochpass
- **De-Esser** (neu): Frequenz 2,5–12 kHz, Threshold, Range, Modus Split (nur Band, Linkwitz-Riley-Weiche) oder Wide (ganzes Signal), Abhören
- **Kompressor erweitert**: Knee (0–18 dB), Mix (Parallelkompression), Sidechain-Hochpass, Auto-Release (programmabhängig)
- Signalkette: Input → Phase → HPF/LPF → Gate → EQ → Kompressor → De-Esser → [Analyse] → Adaptiv → Width/Pan → Output
- **Modul-Rack** mit Reitern (Gate, Kompressor, De-Esser), Lampe je Reiter = Modul an, LED-Reduktionsanzeige; letzter Reiter wird im Projekt gemerkt
- **Fenster frei skalierbar** (50–200 %, festes Seitenverhältnis, Basisgröße 1100 × 960, Ecke unten rechts); Größe wird global gespeichert (`uiScale` in AME.settings)
- OUTPUT-Sektion in die rechte Spalte verlegt
- Parameter-Werte effizienter gelesen (Zeiger-Cache `StripParamReader`)
- Benutzer-Einstellungen zentral (`Settings.*`)
- Alte Live-Sets: neue Parameter mit neutralen Standardwerten (Module aus, Knee 6 dB wie bisher, Mix 100 %)
- Hub-Layout v2 (Reduktionswerte Gate/De-Esser/Limiter) – v0.9 und v0.10 teilen keinen Hub
- Smoke-Test: Phase, Gate, Expander, Kompressor (Mix, SC-HPF, Auto-Release), De-Esser

## 0.9.0 – 2026-11-08 (Roadmap Schritt 6: prozessübergreifender Hub)
- MixHub liegt jetzt im Named Shared Memory der DAW-Sitzung (Windows `CreateFileMapping` + benannter Mutex, POSIX `shm_open` + robuster Prozess-Mutex) → Instanzen sehen sich auch in getrennten Plug-in-Prozessen (Bitwig-Sandbox, Reaper „separate process“, FL-Bridge …)
- Sitzung = DAW-Prozess: Hilfsprozesse (Plugin-Host, Audio-Engine, Bridge) werden im Prozessbaum bis zur DAW zurückverfolgt; mehrere gleichzeitig laufende DAWs bleiben getrennt; Override per Umgebungsvariable `AME_HUB_SESSION`
- Instanzen abgestürzter Prozesse werden nach ≤ 1 s automatisch abgemeldet
- Fallback: ohne Shared Memory wie bisher nur innerhalb des Prozesses; verschiedene AME-Versionen teilen keinen Speicher (Layout-Version im Namen)
- Statuszeile: Tooltip zeigt die Verbindung (Shared Memory, Sitzung, Anzahl Prozesse)
- Handbuch: andere DAWs, Fehlersuche ergänzt
- Smoke-Test: Kindprozess mit eigener Line (Analyse, Rolle, Engine-Zielwerte über Prozessgrenze), Absturz-Aufräumen

## 0.8.1 – 2026-11-08 (Open Source, AGPLv3)
- Lizenz: GNU AGPLv3 (`LICENSE`), JUCE unter dessen AGPLv3-Option
- `THIRD_PARTY_NOTICES.md`: JUCE (AGPLv3), VST 3 SDK (MIT), Schriften (OFL), VST-Markenhinweis
- Copyright-/SPDX-Kopf in allen Quelldateien (`AGPL-3.0-only`)
- Handbuch/README: Kapitel Lizenz, Link zum Quellcode

## 0.8.0 – 2026-11-08 (Englische Oberfläche, Handbuch)
- UI-Sprache Deutsch/Englisch, Umschalter **DE / EN** im Kopf; gilt sofort für alle Instanzen, gespeichert in der Benutzer-Einstellungsdatei (`AME/AME.settings`), erster Start = Systemsprache
- Alle UI-Texte, Rollen-, Programm-, Vorrang- und Modusnamen zweisprachig (i18n/Lang, `tr("de", "en")`)
- Parameter-Namen in der DAW jetzt fest englisch (Mode, Role, Priority, Response, Direct, Program …); Parameter-IDs unverändert → alte Live-Sets kompatibel
- Eingebautes Handbuch (Schalter **HANDBUCH / MANUAL**): Inhaltsverzeichnis, 14 Kapitel, Vintage-Darstellung; Quelle `docs/manual/handbuch_de.md` / `manual_en.md` (eingebettet, auf GitHub lesbar)
- Entwickler/Hersteller: **Batze the bit builder** (VST3-Hersteller, Handbuch, README)
- Tooltips (Statuszeile, Handbuch, Sprache)
- Kopfzeile: Master-Schalter etwas kompakter
- Smoke-Test: Sprachwechsel, Parameter-Namen, Kapitelzahl DE = EN; neue Bilder ui_track_en, ui_master_en, ui_manual_de, ui_manual_en

## 0.7.0 – 2026-11-08 (Umbenennung: AME – Adaptive Mix Exchange)
- Neuer Name AME (Adaptive Mix Exchange), Vermittlungs-Metapher (siehe BRANDING.md)
- Kopf: Schriftzug „AME“ + „ADAPTIVE MIX EXCHANGE“, drei Lampen LINE / ENGINE / HOLD, Version
- Typenschild: „AME MODEL x.y.z · LINE nn“, Name, Rolle, Programm
- Begriffe: Spur-Instanz → Line, Master → Exchange, Preset → Programm, Priorität → Vorrang, Reaktion → Ansprechzeit, Bypass → Direkt, Einfrieren → Hold, Rolle Auto → Auto-Kennung, Masking-Matrix → Patchfeld
- Standardname neuer Instanzen „Line 01“ …
- CMake: Projekt/Target AME, VST3 „AME Adaptive Mix Exchange.vst3“, Smoke-Test AMESmokeTest
- Plugin-Code (Tbnn/Mtmx), Parameter-IDs und State-Format unverändert → bestehende Live-Sets laden weiter

## 0.6.0 – 2026-11-08 (Vintage-UI)
- Neues Design: Creme-Front mit Walnuss-Gehäuse, Bakelit-Kopfleiste und -Knöpfe (geriffelt, Skala, Bernstein-Wertbogen), Kontrolllampen als Schalter, Schrauben, Messing-Typenschild
- Eingebettete Schriften (OFL): Abril Fatface (Namensschild), Bebas Neue (Gravur), Courier Prime (Werte/Anzeigen)
- Analyzer im Messgeräte-Stil (Phosphor-Grün, Raster, Leuchtlinien)
- Frequenzanalyse: 240 Anzeige-Punkte (~1/12 Oktave) statt 32 Bänder; Engine rechnet weiter mit 32 Bändern
- Spur: Regenbogen-Füllung nach Frequenz, Linie nach Engine, Peak-Hold (gepunktet, 12 dB/s), Spektrogramm unter dem EQ
- Mix-Übersicht: Spuren als farbige Flächen + Leuchtlinien, Summe (Master) als Creme-Linie, Masking-Wärmesäulen (gelb → rot), Peak-Hold der Auswahl, Spektrogramm der Auswahl, Masking-Matrix (klickbar), LED-Pegelketten
- Analoge VU-Meter (Bernstein, VU-Ballistik, Peak-Lampe): Spur = Ausgang + Gain Reduction, Master = L/R
- Adaptiv-Anzeige als LED-Säulen
- Fenstergröße 1000 × 880

## 0.5.0 – 2026-11-08 (Roadmap Schritt 4)
- Automatische Rollenerkennung bei Rolle „Auto“ (manuelle Wahl hat Vorrang)
  1. Spurname (deutsch/englisch, Abkürzungen wie BD, SD, OH, Vox, BGV, Gtr, Ac Gtr, 808, Rhodes …) – sofort, 100 %
  2. Klangmerkmale nach ≥ 3 s Signal: Schwerpunkt, Flatness, Anteile < 60 / < 150 Hz / > 4 kHz, Onsets/s, Harmonizität + Grundton (Autokorrelation per FFT), Vibrato (Cent-Schwankung), Crest (~1,5 s), Stereobreite → Prototyp-Bewertung je Rolle, Softmax-Sicherheit
  3. Hysterese: Wechsel nur bei +15 % Vorsprung; unter 25 % Sicherheit → „Sonstiges“
- Engine und Profile nutzen die erkannte Rolle
- UI: „→ Rolle (xx %)“ bzw. „(Spurname)“ neben der Rollenauswahl; Mix-Übersicht kennzeichnet erkannte Rollen mit „(A)“
- Smoke-Test: 23 Spurnamen, 6 synthetische Signale (Kick, Snare, Hi-Hats, Bass, Gesang, Pad)

## 0.4.0 – 2026-11-08
- Lautstärke-Korrektur hält die Summenleistung aller Spuren konstant (iterativ, ±6 dB; Akustik ±4 dB)
- EQ-Grafik in der Strip-Ansicht: eigenes Spektrum (Fläche vor / Linie nach Engine), manuelle EQ-Kurve mit ziehbaren Punkten (L, 1, 2, H, HP, LP; Mausrad = Q, Doppelklick = 0 dB), adaptive Kurve gestrichelt
- A/B-Bypass der adaptiven Eingriffe pro Spur (automatisierbar) und global im Master; Umschalten mit 30-ms-Überblendung
- Einfrieren pro Spur: hält die aktuellen Engine-Werte fest (unabhängig von Master/Preset), wird im Live-Set gespeichert; Master: „Alle einfrieren“ / „Alle lösen“
- Neue Presets: Hip-Hop, Akustik / Singer-Songwriter, Metal
- Mix-Übersicht: Kennzeichnung [fix] / [Bypass]
- Fenstergröße 1000 × 800

## 0.3.0 – 2026-11-08 (Roadmap Schritt 3)
- MixEngine im aktiven Master (eigener Thread, 50 Hz) mit Lebenszeichen im Hub; ohne Master keine Eingriffe
- Profile je Preset × Rolle: Priorität pro Region (Sub/Low/Mid/High), Ziel-Lautheit, max. Hochpass; max. Absenkung je Genre (Electronic 8 dB, sonst 6 dB)
- Konflikt-Logik: niedrigere Priorität weicht aus, wo zwei Spuren im selben Band konkurrieren (bis 18 dB Abstand); gleichrangig = beide etwas
- Adaptive Stufe pro Spur (nach Kompressor, vor Pan): Hochpass 12 dB/Okt., 10 Oktav-Peak-Bänder (31 Hz–16 kHz, nur Absenkung), Lautstärke (−6 / +3 dB)
- Analyse jetzt vor der adaptiven Stufe (keine Rückkopplung)
- Neue Spur-Parameter: Priorität (Preset / Sehr hoch … Sehr niedrig), Reaktion (langsam = Balance … schnell = Ducking), Schalter EQ / HPF / Gain; Amount in Sektion „Adaptiv“
- Strip-UI: Sektion „Adaptiv“ mit Anzeige der aktuellen Absenkungen, HPF, Gain, Engine-Status
- Mix-Übersicht: Maskierung vorher→nachher, Spalte „Adaptiv“, Spektren nach Engine, ausgewählte Spur zusätzlich gestrichelt vor Engine
- Fenstergröße 1000 × 700

## 0.2.0 – 2026-11-08 (Roadmap Schritt 2)
- SpectrumAnalyzer: FFT 4096/Hop 2048, Hann, 32 log. Bänder 20 Hz–20 kHz (~1/3 Okt.), Post-Fader, im Audio-Thread ohne Allokation
- Hub: Kurzzeit-Spektrum (Anzeige) + Langzeit-Spektrum (~3 s, Masking) je Instanz
- MaskingModel: Matrix m(Masker→Spur, Band) aus Pegeldifferenz, Bandrelevanz und Nachbarband-Spreizung; Maskierungsgrad, Hauptmasker und Frequenzbereich je Spur; Hotspots je Band
- Mix-Übersicht: Spalten „Maskiert“ und „Hauptüberdeckung“, Spektrum-Overlay aller Spuren mit Hotspot-Markierung; Klick auf Zeile zeigt Überdeckung der Einzelspur
- Spurfarben: DAW-Farbe, sonst Palette
- Smoke-Test: Testsignale (Kick/Bass/Vocal), prüft Analyse und Masking

## 0.1.0 – 2026-11-08 (Roadmap Schritt 1)
- Projektgerüst: CMake + JUCE 8.0.10, VST3
- MixHub: prozessweites Register aller Instanzen (bis 128), lock-free Pegel/GR-Daten, Slots für Analyse- und Adaptiv-Daten vorbereitet
- Modi Spur / Master; bei mehreren Mastern ist der älteste aktiv
- Master schreibt Preset (Rock/Band, Pop Vocal-Fokus, Electronic) und globalen Amount in den Hub
- ChannelStrip: Input, HPF/LPF 24 dB/Okt., EQ (Low Shelf, 2× Peak, High Shelf), Kompressor (Soft Knee, stereo-linked), Width, Pan, Output; geglättete Parameter
- Hook für adaptive EQ-Offsets (Engine ab Schritt 3)
- Spurname/-farbe aus der DAW (falls vom Host übermittelt)
- UI: Strip-Ansicht mit Meter/GR, Mix-Übersicht aller Instanzen, Statuszeile
- Smoke-Test ohne DAW
