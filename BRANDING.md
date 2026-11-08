# AME – Adaptive Mix Exchange

## Idee
„Exchange“ = Vermittlungsstelle aus der Zeit der Röhren und Klappenschränke.
Jede Spur ist ein **Anschluss (Line)**, die Master-Instanz ist das **Amt (Exchange)**.
Das Amt hört alle Leitungen ab, erkennt, wer wem ins Wort fällt, und weist
jeder Spur ihren Platz im Frequenzspektrum zu – laufend, adaptiv.

Aussprache: englisch „aim“ → **Every track finds its aim.** (Ziel = Preset/Zielprofil)

## Claims
- Every track finds its aim.
- Your tracks, talking.
- The mix that listens to itself.
- DE: Jede Spur bekommt ihren Platz.

## Entwickler
**Batze the bit builder** – VST3-Hersteller (COMPANY_NAME), Handbuch, README, Inhaltsverzeichnis des Handbuchs.

## Begriffswelt (UI)
| Technisch | AME-Begriff DE | AME-Begriff EN |
|---|---|---|
| Spur-Instanz | Line / Anschluss („Line 07“) | Line |
| Master-Instanz | Exchange / Amt | Exchange |
| Preset | Programm („Programm: Rock / Band“) | Program |
| Masking-Matrix | Patchfeld (Switchboard) | Patchbay |
| Priorität | Vorrang | Priority |
| Rolle „Auto“ | Auto-Kennung | Auto-ID |
| Einfrieren | Hold | Hold (Release = lösen) |
| Bypass | Direkt | Direct |
| Reaktion | Ansprechzeit | Response |
| Instanz verbunden | Lampe „LINE“ | Lampe „LINE“ |
| kein Master | „No Exchange – Line läuft solo“ | „No Exchange – line runs solo“ |

## Gestaltung
- Kopf: „AME“ groß in Abril Fatface, bernstein auf Bakelit; daneben „ADAPTIVE MIX EXCHANGE“ in Bebas Neue, gesperrt.
- Drei Lampen unter dem Untertitel: LINE (Exchange vorhanden, grün), ENGINE (Engine aktiv, bernstein), HOLD (blau).
- Messing-Typenschild: `AME MODEL 0.8.0 · LINE 07`, Name, Rolle, Programm.
- Kopf rechts: DE / EN, HANDBUCH / MANUAL.

## Technische Namen
- VST3-Name: „AME Adaptive Mix Exchange“, Hersteller „Batze the bit builder“ (in DAW-Liste vollständig → besser auffindbar)
- CMake-Target: `AME`, Plugin-Code bleibt `Mtmx` (Kompatibilität mit gespeicherten Live-Sets)
- Hinweis: Kürzel AME auch bei Adobe Media Encoder belegt → im Plugin-Kontext unkritisch, Vollname immer mitführen.

## Status
Umgesetzt in v0.7.0 (2026-11-08): Name, Kopf-Logo mit Lampen, Typenschild, alle UI-Begriffe.
v0.8.0: englische Begriffe (umschaltbar), Hersteller „Batze the bit builder“.
