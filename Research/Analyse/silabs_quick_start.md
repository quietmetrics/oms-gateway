# Analyse der „Wireless M‑Bus Quick Start Guide“ von Silicon Labs (2014)

Silicon Labs veröffentlichte 2014 eine Kurzanleitung für Wireless‑M‑Bus‑Systeme. Die Broschüre beschreibt die Betriebsarten, deren Datenraten sowie die Kompatibilität zwischen Zähler‑ und Sammlergeräten. Einige wichtige Punkte für das Verständnis des T‑Modus sind unten zusammengefasst.

## Betriebsarten und Verwendungszweck

 Tabelle 4 der Broschüre ordnet die verschiedenen WMBus‑Modi ein[^silabs_brochure]:

| Modus | Beschreibung |
|------|--------------|
| **S1** | Stationäre Zähler senden sporadisch; verwendet für Gaszähler (lange Batterielaufzeit). |
| **S2** | Bidirektionale Variante zu S1. |
| **T1** | Unidirektionaler Modus für häufiges Senden; Meter sendet in konfigurierbaren Intervallen. |
| **T2** | Bidirektionale Variante zu T1; Zähler empfängt Nachrichten nur kurz nach dem Senden. |
| **C1** | Kompaktmodus: Meter sendet A‑Format‑Telegramme (100 kBaud NRZ). |
| **C2** | Bidirektionaler Kompaktmodus mit geringerer Datenrate im Rückkanal. |

 Tabelle 5 (Kompatibilitätsmatrix) zeigt die physikalischen Parameter für die Modes[^silabs_brochure]:

* **T1/T2:** Meter‑Geräte senden bei 868,95 MHz mit 100 kcps (kchips/s) unter Verwendung der 3‑out‑of‑6‑Codierung; Sammlergeräte senden auf 868,30 MHz mit 32,768 kcps (Manchester‑Codierung). Somit werden die A‑ und B‑Links realisiert.
* **C1/C2:** Meter senden bei 868,95 MHz mit NRZ‑Codierung und 100 bzw. 50 kcps; Sammler senden bei 868,95 MHz mit 50 kcps NRZ und sind zudem kompatibel zu T‑Mode‑Empfang[^silabs_brochure].

Die Broschüre bestätigt, dass T‑Mode‑Meter standardmäßig 3‑out‑of‑6 codieren und 100 kBaud übertragen. Für T2‑Geräte wird der Rückkanal mit Manchester‑Codierung und geringerer Datenrate betrieben, um die Empfängerdauer zu reduzieren.

## Nutzen für den Leitfaden

Die Kurzanleitung von Silicon Labs hilft, die Betriebsarten einzuordnen und liefert zusätzliche Bestätigung für die Datenraten und Modulationsarten im T‑Modus. Sie betont außerdem die zeitliche Abstimmung im bidirektionalen Betrieb (T2), was bei der Gateway‑Entwicklung zu berücksichtigen ist. Für eine reine T1‑Implementierung sind jedoch nur die 100‑kBaud‑Parametrierung und die 3‑out‑of‑6‑Codierung relevant.

[^silabs_brochure]: Silicon Labs. *Wireless M‑Bus Quick Start Guide* (2014). Die Broschüre definiert die WMBus‑Betriebsarten S1/S2, T1/T2 und C1/C2, listet deren Datenraten und Modulationsarten auf und zeigt eine Kompatibilitätsmatrix für Meter‑ und Sampler‑Geräte. Online abrufbar z. B. unter: https://www.silabs.com/documents/public/application-notes/an0798-wireless-mbus.pdf