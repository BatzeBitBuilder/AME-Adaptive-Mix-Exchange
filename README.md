# AME – Adaptive Mix Exchange

*Every track finds its aim.* (Branding: BRANDING.md) · Entwickler: **Batze the bit builder**

📖 Handbuch: [Deutsch](docs/manual/handbuch_de.md) · [English](docs/manual/manual_en.md) – auch im Plugin über **HANDBUCH / MANUAL** aufrufbar.

Vernetzter Channelstrip (VST3, JUCE). Alle Instanzen (Lines) im Projekt kennen sich; eine Exchange-Instanz auf dem Master-Bus steuert Programm und globale Eingriffsstärke.

Konzept & Roadmap: siehe `KONZEPT.md` · Änderungen: `CHANGELOG.md`

## Voraussetzungen (Windows)
- Visual Studio 2022 (Workload „Desktopentwicklung mit C++“)
- CMake ≥ 3.22 (in VS enthalten), Git

## Build
```bat
cd C:\Users\thors\Documents\Coding\MultiTrackMix
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
JUCE 8.0.10 wird beim ersten Konfigurieren automatisch geladen (alternativ: `-DMTM_JUCE_PATH=C:\pfad\zu\JUCE`).

Ergebnis: `build\AME_artefacts\Release\VST3\AME Adaptive Mix Exchange.vst3`

## Installation (Ableton Live)
1. Alte `MultiTrackMix.vst3` aus dem VST3-Ordner löschen (gleiche Plugin-ID). `AME Adaptive Mix Exchange.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren (Admin-Rechte).
2. Live → Einstellungen → Plug-Ins → „VST3 Plug-In System Folders“ an → Neu scannen.

## Verwendung
- Auf jede Spur laden, **Modus: Spur**, **Rolle** wählen.
- Eine Instanz auf den **Master-Bus**, **Modus: Exchange (Master)**, Programm wählen.
- Exchange zeigt die Mix-Übersicht aller Lines; in Spur-Instanzen über „Mix-Übersicht“ umschaltbar.
- EQ-Grafik: Punkte ziehen (Frequenz/Gain), Mausrad = Q, Doppelklick = 0 dB.
- Direkt (A/B): Eingriffe der Engine aus (Line bzw. im Exchange für alle). Hold: aktuelle Engine-Werte festhalten (bleiben beim Speichern erhalten, gelten auch ohne Exchange); im Exchange „Hold (alle) / Hold lösen“.
- Rolle „Auto“: Erkennung über den Spurnamen (z. B. „Kick“, „Vox“, „Gtr“), sonst nach einigen Sekunden über den Klang; Anzeige „→ Rolle (Sicherheit)“. Manuell gewählte Rollen haben Vorrang.
- Adaptiv (Spur): Amount = Eingriffsstärke, Ansprechzeit = langsam (Mix-Balance) bis schnell (Ducking), Vorrang = Abweichung vom Programm, Schalter EQ/HPF/Gain. Ohne aktives Exchange greift nichts ein („No Exchange – Line läuft solo“).
- Mix-Übersicht: Spektren aller Spuren, Maskierungsgrad, Hauptüberdeckung; Zeile anklicken = Überdeckung dieser Spur (rot hinterlegt).

## Sprache
Oberfläche Deutsch/Englisch, umschaltbar über **DE / EN** im Kopf. Gilt sofort für alle Instanzen, gespeichert in `%APPDATA%\AME\AME.settings` (erster Start: Systemsprache). Parameter-Namen in der DAW sind immer englisch.

## Smoke-Test (ohne DAW)
```bat
cmake -B build -DMTM_BUILD_TESTS=ON
cmake --build build --config Release --target AMESmokeTest
build\AMESmokeTest_artefacts\Release\AMESmokeTest.exe
```

## Schriften
`Resources/Fonts`: Abril Fatface, Bebas Neue, Courier Prime – SIL Open Font License (Lizenztexte liegen bei), per `juce_add_binary_data` eingebettet.

## Struktur
```
Source/
  PluginProcessor.*   Audio-Verarbeitung, Hub-Anbindung
  PluginEditor.*      Hauptfenster (Kopfzeile, Umschaltung Strip/Übersicht)
  Parameters.*        Parameter-IDs und -Layout
  Roles.h             Rollen, Genres, Modi (Namen DE/EN)
  i18n/Lang.*         UI-Sprache (global, gespeichert), tr("de", "en")
  hub/MixHub.*        Prozessweites Instanz-Register (lock-free Audio-Daten)
  dsp/ChannelStrip.*  HPF/LPF, 4-Band-EQ, Kompressor, Width/Pan
  dsp/Biquad.h        RBJ-Biquad
  dsp/SpectrumAnalyzer.*  FFT-Analyse -> 32 Bänder
  hub/MaskingModel.*  Masking-Matrix, Hotspots
  engine/Profiles.*   Preset × Rolle: Prioritäten, Ziel-Lautheit, HPF
  engine/MixEngine.*  Adaptive Engine (Master, 50 Hz)
  engine/RoleClassifier.*  Rollenerkennung (Name, Klangmerkmale, Hysterese)
  ui/                 VintageLookAndFeel (Palette, Knöpfe, Lampen), Fonts, VuMeter, Waterfall,
                      MaskingMatrix, EqGraph, StripPanel, MixOverview, HandbookView (Markdown-Renderer)
Resources/Fonts/      eingebettete Schriften (OFL)
docs/manual/          Handbuch DE/EN (Markdown, ins Plugin eingebettet) + Screenshots
Tests/SmokeTest.cpp, Tests/RoleSignals.h
```
