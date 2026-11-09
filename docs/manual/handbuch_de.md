# AME – Adaptive Mix Exchange

**Handbuch · Version 1.1.0** · entwickelt von **Batze the bit builder**

*Every track finds its aim.* – Jede Spur bekommt ihren Platz.

![Line-Ansicht](images/ui_track.png)

## Überblick

AME ist ein vernetzter Channelstrip (VST3). Jede Instanz auf einer Spur heißt **Line**. Eine einzelne Instanz auf dem Master-Bus arbeitet als **Exchange** – wie das Amt einer alten Telefonvermittlung: Es hört alle Leitungen ab, erkennt, wer wem ins Wort fällt, und weist jeder Spur laufend ihren Platz im Frequenzspektrum und in der Lautstärke zu.

- **Klassischer Channelstrip** pro Spur: Input, Hoch-/Tiefpass, 4-Band-EQ, Kompressor, Pan, Stereobreite, Output.
- **Analyse**: Jede Line misst ihr Spektrum und meldet es an alle anderen Instanzen im Projekt.
- **Adaptive Engine** im Exchange: berechnet aus Programm (Genre), Rollen und Vorrang, welche Spur wo ausweicht.
- **Eigene adaptive Stufe** pro Line: 10 Oktav-Absenkungen, Hochpass und Lautstärke. Deine manuellen EQ-Einstellungen bleiben unangetastet.
- **Volle Kontrolle**: Direkt (A/B-Vergleich), Hold (Werte einfrieren), Stärke und Ansprechzeit pro Spur.
- **Export = Abhöre**: Die Engine rechnet im Takt des Audios, und der **Mitschnitt** zeichnet die Eingriffe über den Song auf und spielt sie exakt wieder ab – auch beim Export einzelner Spuren.

> AME hilft beim Aufräumen eines Mixes – es ersetzt nicht dein Gehör. Nutze die Eingriffe als Vorschlag, vergleiche mit **Direkt** und friere gute Ergebnisse mit **Hold** ein.

## Installation

1. `AME Adaptive Mix Exchange.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren (Administratorrechte nötig).
2. Plug-ins in der DAW neu scannen lassen (der System-VST3-Ordner muss in den Plug-in-Einstellungen enthalten sein).
3. AME erscheint unter dem Hersteller **Batze the bit builder**.

| DAW | Neu scannen (Menünamen der englischen DAW-Oberfläche) |
|---|---|
| Ableton Live | Settings → Plug-Ins → enable "VST3 Plug-In System Folders" → Rescan |
| Cubase / Nuendo | Studio → VST Plug-in Manager → Rescan |
| Studio One | Options → Locations → VST Plug-ins → Reset Blocklist, dann neu starten |
| Reaper | Options → Preferences → Plug-ins → VST → Re-scan |
| Bitwig Studio | Settings → Locations → Plug-ins (scannt automatisch) |
| FL Studio | Options → Manage plugins → Find installed plugins |

**Kompatibilität:** AME läuft in allen gängigen VST3-Hosts unter Windows 64-bit. Die Instanzen finden sich über gemeinsamen Speicher der DAW-Sitzung – auch dann, wenn die DAW Plug-ins in getrennten Prozessen betreibt (Sandbox, Bridge, z. B. Bitwig „Individually“). Getestet mit Ableton Live.

## Schnellstart

1. **AME auf jede Spur** laden, die mitgemischt werden soll. Modus: **Line (Spur)**.
2. **Rolle** wählen (z. B. Kick, Bass, Lead Vocal) – oder auf **Auto-Kennung** lassen.
3. **Eine AME-Instanz auf den Master-Bus** legen und den Modus auf **Exchange (Master)** stellen.
4. Im Exchange das **Programm** wählen (z. B. Rock / Band, Pop, Electronic).
5. Wiedergabe starten. Nach wenigen Sekunden leuchtet die Lampe **ENGINE**, die Lines weichen sich gegenseitig aus.
6. Mit der **Abhöre** im Exchange vergleichen: **Voll** / **Ohne Amt** / **Roh**.
7. Gefällt das Ergebnis: im Exchange **Aufnahme (alle)**, den Song einmal komplett abspielen, dann **Wiedergabe (alle)**. Der Export klingt jetzt genau wie die Abhöre (siehe Kapitel „Export und Mitschnitt“).

> Ohne Exchange laufen alle Lines „solo": Der Channelstrip funktioniert normal, es gibt aber keine adaptiven Eingriffe.

## Prinzip: Lines und Exchange

| Begriff | Bedeutung |
|---|---|
| Line | AME-Instanz auf einer Spur (Modus „Line (Spur)") |
| Exchange | AME-Instanz auf dem Master-Bus (Modus „Exchange (Master)"), steuert die Engine |
| Programm | Zielprofil / Genre-Preset des gesamten Mixes |
| Rolle | Funktion einer Spur im Arrangement (Kick, Bass, Gesang …) |
| Vorrang | Wie wichtig eine Spur gegenüber ihrem Programm-Standard ist |
| Ansprechzeit | Wie schnell die Eingriffe folgen (langsam = Balance, schnell = Ducking) |
| Direkt | A/B-Bypass der adaptiven Eingriffe |
| Hold | Aktuelle adaptive Werte festhalten |
| Patchfeld | Matrix: welche Spur welche verdeckt |

**So läuft es ab:**

- Alle Instanzen im selben Projekt finden sich automatisch (gemeinsamer Speicher der DAW-Sitzung, auch über Plug-in-Prozesse hinweg). Es gibt nichts zu verbinden.
- Jede Line analysiert ihr Signal (FFT, 32 Bänder, Kurz- und Langzeitspektrum) und stellt es den anderen bereit.
- Das Exchange berechnet je 20 ms Audio (50-mal pro Sekunde) Zielwerte für jede Line und schickt sie zurück.
- Jede Line nähert sich diesen Zielen geglättet an – mit der eingestellten Ansprechzeit.

Gibt es mehrere Exchange-Instanzen, ist nur die **älteste** aktiv; die anderen zeigen „[EX–]" und einen Hinweis in der Statuszeile.

## Kopfzeile

Die Kopfzeile ist in Line und Exchange gleich aufgebaut:

- **AME-Schriftzug mit drei Lampen**:
  - **LINE** (grün) – ein Exchange ist vorhanden.
  - **ENGINE** (bernstein) – die Engine rechnet gerade (Lebenszeichen jünger als 0,5 s).
  - **HOLD** (blau) – diese Line hält ihre Werte fest.
- **Statuszeile**: Anzahl der Lines, Exchange-Status und Hinweise.
- **DE / EN**: Sprache der Oberfläche. Die Wahl gilt sofort für alle offenen AME-Fenster und wird für künftige Sitzungen gespeichert. Beim allerersten Start folgt AME der Systemsprache.
- **HANDBUCH**: öffnet dieses Handbuch. Kapitel links anklicken, **SCHLIESSEN** kehrt zurück.
- **Modus**: Line (Spur) oder Exchange (Master).
- **Line**: Rollenauswahl, bei Auto-Kennung die erkannte Rolle, Schalter **Mix-Übersicht**.
- **Exchange**: Programm, globaler Amount, Abhöre (Voll / Ohne Amt / Roh), Pegelangleich, Hold (alle), Hold lösen.

> Parameter-Namen in der DAW (Automation) sind immer englisch – unabhängig von der gewählten Sprache.

## Die Line (Spur-Ansicht)

Signalweg: **Input → Phase → Filter → Gate → EQ ⇄ Kompressor → De-Esser → Sättigung → [Analyse] → Adaptiv → Pan/Width → Output → Limiter**

### Presets und A/B
Die Leiste oben:

- **Preset**: Werkspresets je Rolle (Startwerte für Filter, EQ, Gate, Kompressor, De-Esser, Sättigung) und **eigene Presets**. Werkspresets lassen Input, Phase, Pan, Width, Output und Limiter unverändert.
- **Preset der Rolle**: lädt das Werkspreset der eingestellten bzw. per Auto-Kennung erkannten Rolle.
- **Speichern**: aktuelle Einstellungen als eigenes Preset ablegen (gleicher Name überschreibt). Eigene Presets liegen in `%APPDATA%\AME\Presets` und stehen in allen Projekten zur Verfügung.
- **Löschen**: das geladene eigene Preset löschen.
- **A / B**: zwei Einstellungen vergleichen. Beim ersten Wechsel startet B als Kopie von A. **A→B** kopiert die aktive Einstellung in den anderen Platz. Beide werden im Projekt gespeichert.

Presets setzen die Parameter wie eine Bedienung von Hand – die DAW kann sie rückgängig machen.

### Input
**Gain** (±24 dB). **Ø L / Ø R** kehren die Phase (Polarität) des linken bzw. rechten Kanals um – z. B. für Snare unten oder Kick Out.

### Filter
**HPF** (20 Hz–1 kHz) und **LPF** (1–20 kHz), Lampen im Titel schalten sie ein. Darunter die **Flankensteilheit**: 12, 24 oder 48 dB/Oktave.

### Modul-Rack
Fünf Module auf Reitern; die Lampe im Reiter zeigt, ob das Modul eingeschaltet ist. Rechts zeigt die LED-Kette die aktuelle Pegelreduktion des sichtbaren Moduls (0–24 dB). Der zuletzt gewählte Reiter wird im Projekt gemerkt.

**Gate / Expander** – **Gate** schließt unterhalb des Thresholds um **Range** (3 dB Hysterese gegen Flattern), **Expander 1:2 / 1:4** senkt leise Anteile im Verhältnis ab (höchstens um Range). **Attack**, **Hold**, **Release**; **SC HPF** filtert nur das Steuersignal (z. B. damit Bass-Übersprechen das Gate nicht öffnet).

**Kompressor** – **Thresh**, **Ratio** (1:1–20:1), **Attack**, **Release**, **Knee** (0 = hart, bis 18 dB weich), **Makeup**, **Mix** (Parallelkompression: 0 % = unbearbeitet), **SC HPF** (Bässe lösen weniger Kompression aus). **Auto-Release**: schnelle Erholung nach kurzen Spitzen, langsame bei Dauerkompression. Die Gain Reduction zeigt zusätzlich das VU-Meter rechts.

**De-Esser** – **Freq** (2,5–12 kHz), **Thresh**, **Range** (maximale Absenkung). **Split**: nur der Bereich oberhalb der Frequenz wird abgesenkt (Frequenzweiche), **Wide**: das ganze Signal. **Abhören** gibt nur das Zischband aus – zum Einstellen der Frequenz.

**Sättigung / Exciter** – **Röhre** (weich, gerade Obertöne), **Band** (weich, symmetrisch), **Transistor** (härter). **Drive** 0–24 dB mit automatischem Pegelausgleich: leise Signale bleiben unverändert, laute werden verdichtet. **Mix** mischt das Original bei. **Exciter** fügt nur aus dem Höhenband (ab **Exc Freq**) Obertöne hinzu – für Präsenz und Glanz.

**Limiter** – sitzt ganz am Ende hinter dem Output-Gain. **Limiter**: Spitzenbegrenzer ohne Latenz mit **Release**. **Soft Clip**: weiche Sättigung zum **Ceiling**. **Hard Clip**: hart abgeschnitten. Kein Signal überschreitet das Ceiling.

### EQ mit Analyzer
Vier Bänder (±18 dB): **Low** und **High** wahlweise **Shelf** oder **Glocke** (Auswahl im Titel), Mid 1 und Mid 2 als Glocke. Alle Bänder mit **Q** (bei Shelf = Steilheit des Übergangs). **Reihenfolge**: EQ vor oder hinter dem Kompressor.

Die Grafik zeigt:

- **Spektrum** der Spur als Regenbogenfläche (vor der Engine),
- **Nach Engine** als grüne Leuchtlinie,
- **Peak** gepunktet (fällt mit 12 dB/s),
- **EQ**-Kurve (creme, inkl. Filter) mit ziehbaren Punkten L, 1, 2, H, HP, LP,
- **Engine**-Kurve gestrichelt (bernstein).

Bedienung: Punkt **ziehen** = Frequenz/Gain, **Mausrad** = Q, **Doppelklick** = 0 dB. Darunter läuft das **Spektrogramm**; der Klappschalter (Dreieck) **SPEKTROGRAMM** im EQ-Titel blendet es aus (die Grafik wird höher).

### Output
In der rechten Spalte: **Pan**, **Width** (0–200 %, 100 % = unverändert, 0 % = Mono) und **Gain** (±24 dB).

### Adaptiv
- **Amount** – Stärke der Engine-Eingriffe für diese Line (0–100 %).
- **Ansprechzeit** – 0 % = langsam (Eingriff ca. 1,5 s, Rücknahme ca. 3 s; für die Langzeit-Balance), 100 % = schnell (ca. 20 ms / 150 ms; Ducking, z. B. Bass weicht der Kick aus). Standard 30 %.
- **Vorrang** – Programm (Standard der Rolle) oder Sehr hoch / Hoch / Niedrig / Sehr niedrig. Höherer Vorrang = andere Spuren weichen dieser Spur aus.
- Lampen **EQ / HPF / Gain** – welche Eingriffsarten erlaubt sind.
- **Direkt** – Eingriffe dieser Line aus (A/B, mit 30 ms Überblendung).
- **Hold** – aktuelle Werte festhalten.
- **Mitschnitt** – **Live** (Engine regelt laufend), **Aufnahme** (angewendete Werte über die Songposition aufzeichnen) oder **Wiedergabe** (Aufzeichnung exakt abspielen). Siehe Kapitel „Export und Mitschnitt“.

Der Bereich ADAPTIV sitzt rechts neben dem EQ. Mit dem Klappschalter (Dreieck) oben rechts lässt er sich zu einer schmalen Leiste einklappen – der EQ wird dann breiter.

Die LED-Anzeige unten zeigt die aktuelle Absenkung je Oktavband (31 Hz–16 kHz, bis −9 dB Skala), dazu adaptiven Hochpass und Lautstärkekorrektur sowie den Status (EXCHANGE VERBUNDEN, NO EXCHANGE, DIREKT, HOLD, AUFNAHME LÄUFT, WIEDERGABE MITSCHNITT, EXPORT OHNE EXCHANGE).

### VU-Meter und Typenschild
- **Ausgang**: VU-Ballistik (0 VU = −18 dBFS RMS), Peak-Lampe ab −1 dBFS.
- **Kompressor**: Gain Reduction 0 bis −20 dB.
- **Typenschild**: Version, Line-Nummer, Spurname (aus der DAW), Rolle und aktives Programm.

### Fenstergröße und Klappbereiche
Das Fenster ist im Querformat angelegt (Basis 1400 × 760) und passt damit auch auf Full-HD-Bildschirme (1080p, auch mit 125 % Windows-Skalierung). An der Ecke unten rechts lässt es sich stufenlos von 50 bis 200 % skalieren. Die zuletzt gewählte Größe gilt für alle AME-Fenster.

Einklappbar (Dreieck nach unten = offen, nach rechts = eingeklappt): an der Line **ADAPTIV** und das **Spektrogramm**, im Exchange die **Line-Liste** und die rechte Spalte **DETAILS** (Patchfeld, VU, Spektrogramm). Der Platz geht an EQ bzw. Analyzer. Der Zustand gilt für alle AME-Fenster und wird gespeichert.

## Das Exchange (Master)

![Exchange](images/ui_master.png)

Das Exchange schleift das Audiosignal unverändert durch und zeigt die **Mix-Übersicht**. Auf Lines lässt sie sich über den Schalter **Mix-Übersicht** ebenfalls einblenden.

- **Programm** – Genre-Profil für den ganzen Mix (siehe Kapitel Programme).
- **Amount** (global) – skaliert die Eingriffe aller Lines.
- **Abhöre** – Vergleich für den gesamten Mix:
  - **Voll** – alles aktiv (Channelstrips + Engine).
  - **Ohne Amt** – nur die manuellen Channelstrips, die Engine-Eingriffe sind aus (früher „Direkt (alle)“).
  - **Roh** – alle Lines geben ihr unbearbeitetes Eingangssignal aus, als wären keine Plugins geladen. Umschalten mit 30 ms Überblendung, die Analyse läuft weiter – beim Zurückschalten ist sofort alles wieder da.
  - Die Abhöre ist automatisierbar (Parameter „Monitor“, z. B. per MIDI Map auf einen Taster) und wird **nicht** im Projekt gespeichert: Beim Laden steht sie immer auf Voll.
- **Pegelangleich** – gleicht in der Stellung Roh den Lautheitsunterschied zur bearbeiteten Fassung am Exchange-Ausgang aus (±12 dB, Wert in der Statuszeile). So klingt nicht einfach die lautere Variante „besser“. Grundlage: Leistung vor und nach jeder Line (Näherung, Spuren als unkorreliert betrachtet).
- **Exchange deaktivieren** (Gerät in der DAW aus bzw. Bypass) wirkt wie **Ohne Amt**: Die Engine stoppt, die Lines blenden ihre Eingriffe aus. Statuszeile und Lines zeigen „Exchange deaktiviert“.
- **Hold (alle) / Hold lösen** – schaltet Hold auf allen Lines.
- **Mitschnitt-Leiste** (über der Tabelle): **Aufnahme (alle)**, **Wiedergabe (alle)**, **Live (alle)** und **Löschen** (mit Rückfrage) für alle Lines. Daneben: wie viele Lines live, in Aufnahme oder Wiedergabe sind, die Länge des längsten Mitschnitts, „NIMMT AUF“ während der Aufnahme und eine Warnung, wenn Lines ohne Exchange exportiert wurden.

### Mix-Übersicht
Die Tabelle listet alle Lines:

| Spalte | Inhalt |
|---|---|
| Line | Name, Spurfarbe; [EX] = aktives Exchange, [EX–] = inaktives |
| Rolle | gewählte Rolle; „(A)" = automatisch erkannt |
| Pegel | LED-Kette des Pegels |
| GR | Gain Reduction des Kompressors |
| Maskiert | Maskierungsgrad vorher > nachher (in %) |
| Überdeckt von | Hauptverursacher und Frequenzbereich |
| Adaptiv | aktuelle Eingriffe: Gain, HP (Hochpass), EQ (stärkste Absenkung), [DIR] / [HOLD] |

**Zeile anklicken** wählt eine Spur aus: Der Analyzer zeigt dann ihre Überdeckung als Wärmesäulen (gelb → rot), ihr Spektrum vor der Engine gestrichelt, den Peak gepunktet und ihr Spektrogramm.

### Analyzer, Patchfeld, VU
Links Line-Liste und Analyzer, rechts die Spalte **DETAILS** mit Patchfeld, VU Links/Rechts und dem Spektrogramm der ausgewählten Spur. Liste und DETAILS lassen sich einklappen (Dreieck), der Analyzer nutzt dann den Platz.

- **Analyzer**: alle Spuren als farbige Flächen, die **Summe** als Creme-Linie; ohne Auswahl zeigen die Wärmesäulen die Gesamtbelegung je Frequenz.
- **Patchfeld**: Matrix aller Lines – Zeile wird von Spalte verdeckt. Je heller, desto stärker. Klick wählt die Spur.
- **VU Links/Rechts**: Summenpegel.

## Programme

Ein Programm legt für jede Rolle Vorrang je Frequenzregion (Sub / Low / Mid / High), Ziel-Lautheit und maximalen Hochpass fest – dazu die Grenzen der Engine:

| Programm | max. Absenkung | max. Lautstärke-Korrektur | Charakter |
|---|---|---|---|
| Rock / Band | 6 dB | ±6 dB | Gesang und Snare vorne, Gitarren im Mittenband |
| Pop (Vocal-Fokus) | 6 dB | ±6 dB | Lead Vocal klar dominant |
| Electronic | 8 dB | ±6 dB | Kick und Bass tragen, kräftiges Ducking möglich |
| Hip-Hop | 8 dB | ±6 dB | Kick/808 und Stimme im Zentrum |
| Akustik / Singer-Songwriter | 4 dB | ±4 dB | behutsam, natürliche Balance |
| Metal | 7 dB | ±6 dB | Gitarrenwand, Kick-Durchsetzung |

## Rollen und Auto-Kennung

Verfügbare Rollen: Kick, Snare, Drums/Overheads, Percussion, Bass, Lead Vocal, Backing Vocals, E-Gitarre, Akustikgitarre, Keys/Piano, Synth Lead, Pad, Streicher, Bläser, FX, Sonstiges.

Bei **Auto-Kennung** erkennt AME die Rolle selbst:

1. **Spurname** (sofort, 100 %): deutsche und englische Namen sowie Abkürzungen wie BD, SD, OH, Vox, BGV, Gtr, Ac Gtr, 808, Rhodes …
2. **Klang** (nach ca. 3 s Signal): Schwerpunkt, Tonalität, Grundton, Transienten, Vibrato, Crest-Faktor, Stereobreite. Anzeige „→ Rolle (xx %)".
3. **Hysterese**: Ein Wechsel erfolgt nur bei deutlichem Vorsprung (+15 %); unter 25 % Sicherheit gilt „Sonstiges".

Eine manuell gewählte Rolle hat immer Vorrang.

## So arbeitet die Engine

1. **Konflikte finden**: In jedem der 32 Bänder prüft die Engine, wo zwei Spuren gleichzeitig Energie haben (bis 18 dB Pegelabstand, voll ab 6 dB).
2. **Vorrang vergleichen**: Die Spur mit niedrigerem Vorrang weicht aus. Sind beide gleichrangig, geben beide ein wenig nach.
3. **Ducking**: Mit schneller Ansprechzeit folgt das Ausweichen dem Kurzzeitpegel der wichtigeren Spur – die Absenkung kommt nur, wenn diese gerade spielt.
4. **Hochpass**: Unterhalb des Nutzsignals einer Spur (30 dB unter ihrem Maximum) setzt die Engine einen Hochpass, höchstens bis zum Rollen-Maximum.
5. **Lautstärke**: Die Spuren werden auf die Rollen-Verhältnisse des Programms gebracht – die **Summenleistung bleibt dabei konstant**, der Mix wird also nicht einfach lauter oder leiser.
6. **Ausgabe**: Die Absenkungen werden auf 10 Oktavbänder (31 Hz–16 kHz) abgebildet; die Line nähert sich ihnen mit ihrer Ansprechzeit (HPF und Gain mit ca. 2 s).

Die Engine rechnet im **Takt des Audios**: ein Schritt je 20 ms Audio, ausgelöst vom Exchange. Ob die DAW in Echtzeit abspielt oder schneller bzw. langsamer exportiert, ändert also nichts am Verhalten.

Die Engine **senkt nur ab** – sie hebt keine Frequenzen an. Dein manueller EQ wird nie verändert; die adaptive Stufe sitzt hinter Kompressor und EQ, vor Pan/Width.

> Daten anderer Spuren sind mindestens einen Audio-Block alt (die DAW rechnet Spuren parallel). Deshalb arbeitet AME geglättet und nicht samplegenau.

## Direkt und Hold

- **Direkt** (Line) bzw. die Abhöre **Ohne Amt** (Exchange) schaltet nur die adaptiven Eingriffe aus. Der manuelle Channelstrip läuft weiter. Ideal für A/B-Vergleiche – automatisierbar. **Roh** (Exchange) umgeht zusätzlich alle Channelstrips.
- **Hold** friert die aktuellen adaptiven Werte ein. Sie werden im Projekt gespeichert und gelten auch ohne Exchange oder bei anderem Programm.
- **Hold (alle) / Hold lösen** im Exchange schaltet Hold auf allen Lines.

Hold hält **einen** Zustand für den ganzen Song fest (den des Moments, in dem du Hold drückst) – der Mix ist dann nicht mehr adaptiv. Soll der Song adaptiv bleiben und trotzdem reproduzierbar sein, nutze den **Mitschnitt**.

## Export und Mitschnitt

Ziel: **Der Export klingt wie die Abhöre in der DAW.** AME sorgt dafür auf zwei Ebenen.

**1. Engine im Takt des Audios.** Die Engine rechnet je 20 ms Audio einen Schritt – beim Abspielen genauso wie beim Offline-Export, egal wie schnell die DAW rendert. Ein Export im Live-Modus entspricht damit weitgehend der Abhöre. Kleine Abweichungen bleiben möglich: Die Regelung hängt vom Vorlauf ab (Startposition, vorherige Durchläufe), und die DAW rechnet Spuren parallel.

**2. Mitschnitt – exakt und auch für Einzelspuren.** Jede Line zeichnet die tatsächlich angewendeten Werte (10 Absenkungen, Hochpass, Lautstärke) über die **Songposition** der DAW auf, 50 Werte pro Sekunde. In der Wiedergabe spielt sie genau diese Werte an genau dieser Stelle ab – ohne Engine, ohne Exchange, unabhängig von Render-Geschwindigkeit und Vorlauf. Der Song bleibt dabei adaptiv: Refrain und Strophe bekommen ihre eigenen Eingriffe.

So geht's:
1. Programm, Rollen, Vorrang und Amount einstellen, Song einige Male abspielen und mit der Abhöre vergleichen, bis es passt.
2. Abhöre auf **Voll**. Im Exchange **Aufnahme (alle)**.
3. Den Song **einmal komplett** abspielen (vom Anfang, oder nur die Abschnitte, die neu aufgenommen werden sollen – Aufnahme überschreibt nur die gespielten Stellen). Die Lines zeigen „AUFNAHME LÄUFT“ und die Länge.
4. **Wiedergabe (alle)**. Ab jetzt klingt jede Wiedergabe und jeder Export identisch.
5. Exportieren – Gesamtmix oder Einzelspuren (Stems), in Echtzeit oder offline. Das Exchange wird dafür nicht mehr gebraucht.

Wissenswert:
- Der Mitschnitt wird komprimiert **im Projekt gespeichert** (pro Line, bis 30 Minuten Songlänge).
- In der Wiedergabe wirken Änderungen an Programm, Amount, Vorrang oder Ansprechzeit **nicht** – dafür neu aufnehmen oder auf **Live** schalten. Der manuelle Channelstrip und **Direkt** wirken weiter.
- Nicht aufgenommene Stellen: Lücken bis 10 s werden überbrückt (nächster aufgenommener Wert), darüber hinaus keine Eingriffe.
- Nach Änderungen am Arrangement (Teile verschoben, Tempo geändert) die betroffenen Stellen neu aufnehmen.
- Pro Line wählbar im Bereich ADAPTIV (**Mitschnitt**), für alle Lines im Exchange. **Live (alle)** behält den Mitschnitt, **Löschen** entfernt ihn.
- Mitschnitt hat Vorrang vor Hold.

**Export ohne Mitschnitt – worauf achten:**
- Das Exchange muss beim Export **mitlaufen**. Exportiert die DAW nur einzelne Spuren, ohne den Master-Bus zu verarbeiten, fehlen der Engine die Daten – die Lines laufen dann ohne adaptive Eingriffe. AME erkennt das (Export-Kennung der DAW oder Audio schneller als Echtzeit) und zeigt danach an der Line **EXPORT OHNE EXCHANGE – ADAPTIV FEHLTE**, im Exchange „× EXPORT OHNE EXCHANGE“ (5 Minuten lang). Abhilfe: Mitschnitt verwenden.
- Ohne Mitschnitt hängt das Ergebnis vom Vorlauf ab: Export am besten vom Songanfang, nach einigen Durchläufen.

Typischer Ablauf: Programm wählen → Song einige Male durchlaufen lassen → mit der Abhöre vergleichen → Aufnahme (alle) → Song einmal abspielen → Wiedergabe (alle) → exportieren. Alternativ, für einen festen Zustand: Hold (alle).

## Tipps für den Workflow

- **Rollen setzen** lohnt sich: Die Auto-Kennung ist gut, eine bewusste Wahl ist besser.
- **Preset der Rolle** als Startpunkt laden, danach Gate-Threshold und Kompressor-Threshold an den Pegel der Spur anpassen.
- **A/B** nutzen, um zwei Ansätze (z. B. mit und ohne Sättigung) direkt zu vergleichen; gelungene Einstellungen als **eigenes Preset** speichern.
- **Vorrang** nur dort ändern, wo das Programm nicht passt (z. B. Gitarrensolo: Vorrang Sehr hoch).
- **Langsame Ansprechzeit** für Flächen, Gitarren, Keys; **schnell** für Bass gegen Kick oder Pads gegen Gesang.
- **Amount** pro Spur zurücknehmen, wenn eine Spur zu stark bearbeitet wird; global im Exchange für den ganzen Mix.
- Im **Patchfeld** zuerst die hellsten Felder ansehen – dort liegen die größten Konflikte.
- Auf Bussen (z. B. Drum-Bus) kann AME als eine Line arbeiten statt auf jeder Einzelspur.

## Fehlersuche

| Symptom | Ursache / Lösung |
|---|---|
| „No Exchange – Line läuft solo" | Keine AME-Instanz im Modus Exchange auf dem Master-Bus. |
| Lampe ENGINE aus | Exchange ist geladen, wird aber nicht verarbeitet (Gerät deaktiviert, Master-Spur eingefroren). |
| „n Exchanges – nur das älteste ist aktiv" | Mehrere Exchange-Instanzen: überzählige auf Line stellen oder entfernen. |
| Keine Eingriffe trotz Engine | Direkt aktiv? Amount auf 0? Lampen EQ/HPF/Gain aus? Hold aktiv? |
| Spur fehlt in der Übersicht | Tooltip der Statuszeile zeigt die Verbindung: „nur innerhalb dieses Prozesses“ = gemeinsamer Speicher nicht verfügbar → Plug-ins im selben Prozess betreiben. Läuft die Instanz in einem Hilfsprozess, den AME nicht der DAW zuordnet, in allen Prozessen dieselbe Umgebungsvariable `AME_HUB_SESSION` setzen. |
| Spur bleibt nach Absturz eines Plug-in-Prozesses stehen | Wird nach spätestens einer Sekunde automatisch entfernt. |
| Export klingt anders als die Abhöre | Mitschnitt verwenden (Aufnahme → Song abspielen → Wiedergabe). Ohne Mitschnitt muss das Exchange beim Export mitlaufen. |
| „EXPORT OHNE EXCHANGE – ADAPTIV FEHLTE“ | Die Line wurde exportiert, während kein Exchange rechnete (z. B. Einzelspur-Export). Mitschnitt aufnehmen und auf Wiedergabe stellen. |
| „WIEDERGABE – KEIN MITSCHNITT“ | Die Line steht auf Wiedergabe, hat aber nichts aufgezeichnet: Aufnahme wählen und den Song abspielen. |
| „AUFNAHME – KEIN EXCHANGE AKTIV“ | Ohne laufende Engine würde nur „keine Eingriffe“ aufgezeichnet: Exchange aktivieren, Abhöre auf Voll. |
| Auto-Kennung falsch | Spurnamen eindeutig benennen oder Rolle manuell setzen. |
| Spurnamen „Line 01" … | Die DAW übermittelt keinen Namen – Rolle manuell setzen. |

## Über AME

**AME – Adaptive Mix Exchange**, Version 1.1.0

Entwickelt von **Batze the bit builder**.

- Framework: JUCE 8 (C++20), Format VST3, Windows (getestet mit Ableton Live).
- Schriften: Abril Fatface, Bebas Neue, Courier Prime – SIL Open Font License.
- **Lizenz: GNU Affero General Public License v3 (AGPLv3)** – freie, offene Software. Du darfst AME nutzen, verändern und weitergeben, auch kommerziell. Wer eine veränderte Fassung weitergibt, muss deren Quellcode ebenfalls unter AGPLv3 offenlegen. AME wird ohne jede Gewährleistung bereitgestellt.
- Quellcode: [github.com/BatzeBitBuilder/AME-Adaptive-Mix-Exchange](https://github.com/BatzeBitBuilder/AME-Adaptive-Mix-Exchange)
- Fremdkomponenten: JUCE (AGPLv3), VST 3 SDK (MIT, Steinberg), Schriften (OFL) – Details in THIRD_PARTY_NOTICES.md. VST ist eine Marke der Steinberg Media Technologies GmbH.
- Name und Bildsprache: Telefonvermittlung aus der Röhrenzeit – jede Spur ist ein Anschluss, das Exchange ist das Amt.

*Every track finds its aim.*
