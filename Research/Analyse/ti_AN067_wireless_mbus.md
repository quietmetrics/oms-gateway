# Analyse der Texas‑Instruments‑Applikationsnote „AN067 – Wireless M‑Bus with CC1101“ (SWRA234)

Die TI‑Applikationsnote AN067 (Dokument‑Nummer SWRA234) beschreibt die Implementierung des Wireless‑M‑Bus‑Protokolls auf einem MSP430‑Mikrocontroller in Kombination mit dem Transceiver CC1101. Sie dient als maßgebliche Grundlage für den physikalischen und den Link‑Layer des WMBus‑T‑Modus. Nachfolgend sind die wichtigsten Punkte zusammengefasst.

## Kommunikationsmodi

Die EN 13757‑4 definiert verschiedene Funkmodi. Für unsere Anwendung sind relevant:

* **S‑Mode (Stationary/Service)**: niedrige Datenrate und große Reichweite.
* **T‑Mode (Transmit/Frequent Transmit)**: hohe Datenrate (etwa 100 kBaud) bei 868,95 MHz. Es gibt die Varianten **T1** (unidirektional) und **T2** (bidirektional). Der T‑Modus nutzt **Radio‑Link B** für die Aussendung der Zählerdaten und – bei T2 – **Radio‑Link A** für die Rückrichtung[^ti_an067_modes].
* **R‑Mode (Frequent Receive)**: stromsparender Empfangsmodus, den batteriebetriebene Geräte verwenden können.

## Physikalische Parameter des T‑Modus

Laut App‑Note gelten für Radio‑Link B (T‑Modus) folgende Werte[^ti_an067_params]:

| Parameter | Wert |
|---|---|
| Frequenzband | 868,90–869,20 MHz |
| Modulation | 2‑FSK |
| Datenrate | 100 kBaud (100 kSymbols pro Sekunde) |
| Frequenzabweichung | ca. 50 kHz |
| Kanalabstand und Anzahl Kanäle | 200 kHz Abstände, 12 Kanäle |
| Preamble | wiederholtes Byte 01 (langes Low‑Pegel‑Muster) |
| Synchronwort | Bitfolge 0000 1111 1110 1101 0101 0100 (hexadezimal 3D54)[^ti_an067_sync] |
| Leitungs‑Codierung | 3‑out‑of‑6‑Codierung |

Die Synchronisation erfolgt somit durch ein Preamble‑Feld aus vielen Eins‑/Null‑Sequenzen und ein fester 16‑Bit‑Sync‑Wort. Die Daten sind mit 3‑out‑of‑6 codiert.

## 3‑out‑of‑6‑Codierung

Beim T‑Modus werden jeweils vier Datenbits zu sechs Codebits mit genau drei Einsen abgebildet. Die App‑Note listet die 16 möglichen 6‑Bit‑Codes für die Nibbles 0 bis 15[^ti_an067_sync]. Diese Codierung wird im Transceiver als NRZ empfangen; die Decodierung muss im Mikrocontroller durchgeführt werden.

## Rahmenaufbau und CRC

Ein WMBus‑Frame besteht laut AN067 aus mehreren Blöcken. Block 1 enthält die Länge (L‑Feld), das Control‑Feld (C‑Feld), den Hersteller (M‑Feld), die Adresse (A‑Feld) und eine CRC. Block 2 beginnt mit einem CI‑Feld (Application Identifier), gefolgt von Nutzdaten und einer CRC[^ti_an067_frame]. Weitere optionale Blöcke enthalten jeweils 16 Byte Daten und eine CRC. Die CRC wird mit dem Polynom \(x^{16} + x^{13} + x^{12} + x^{11} + x^{10} + x^8 + x^6 + x^5 + x^2 + 1\) berechnet und am Ende invertiert[^ti_an067_frame].

## Einschränkungen des CC1101

Die App‑Note weist darauf hin, dass der CC1101 einige Funktionen des WMBus‑Protokolls nicht direkt unterstützt. Wichtige Punkte:

1. **Kein 3‑out‑of‑6‑Decoder:** Der CC1101 muss im NRZ‑Modus betrieben werden; die Codierung und Decodierung erfolgt in der Firmware[^ti_an067_limitations].
2. **Paketlänge:** Das Register `PKTLEN` kann nur Werte bis 255 verarbeiten. Da T‑Frames größer sein können und die Länge im 3‑out‑of‑6‑Code übertragen wird, muss die Software zwischen Fixed‑Length‑ und Infinite‑Length‑Modus umschalten[^ti_an067_limitations].
3. **RX‑FIFO‑Größe:** Der FIFO speichert maximal 64 Byte; die Firmware muss den FIFO regelmäßig auslesen, um Überläufe zu verhindern.

## Bedeutung für den Leitfaden

Die TI‑Applikationsnote liefert alle relevanten physikalischen Parameter für den T‑Modus (Frequenz, Datenrate, Modulation, Preamble, Synchronwort, 3‑out‑of‑6‑Codierung) und beschreibt den Rahmenaufbau sowie das CRC‑Polynom. Zudem weist sie auf die Beschränkungen des CC1101 hin, wodurch klar wird, dass Decodierung und Längensteuerung in der Firmware stattfinden müssen. Diese Informationen bilden die Grundlage für die hardwareseitige Konfiguration und die Software‑Struktur des Leitfadens.

[^ti_an067_modes]: Texas Instruments, *AN067 – Wireless M‑Bus with CC1101 (SWRA234)*, Abschnitt „Operation Modes and Radio Links“. Dieser beschreibt die WMBus‑Modi (S, T, R) und ordnet sie den Radio‑Links A/B zu. Online verfügbar unter: https://www.ti.com/lit/an/swra234/swra234.pdf

[^ti_an067_params]: Texas Instruments, *AN067 – Wireless M‑Bus with CC1101 (SWRA234)*, Tabelle 5 „Radio Link B“ und begleitender Text. Enthält Frequenzband, Modulation, Datenrate und Kanalparameter. Online verfügbar unter: https://www.ti.com/lit/an/swra234/swra234.pdf

[^ti_an067_sync]: Texas Instruments, *AN067 – Wireless M‑Bus with CC1101 (SWRA234)*, Tabelle 9 „Preamble, Sync Word and Postamble“ und Tabelle 8 „3‑Out‑of‑6 Code“. Beschreibt das 16‑Bit‑Synchronwort (0x3D54) und die 3‑out‑of‑6‑Codierung. Online verfügbar unter: https://www.ti.com/lit/an/swra234/swra234.pdf

[^ti_an067_frame]: Texas Instruments, *AN067 – Wireless M‑Bus with CC1101 (SWRA234)*, Abschnitt „Packet Format“ und Tabelle 10. Enthält die Struktur von Block 1 und Block 2, das CRC‑Polynom und Details zu optionalen Blöcken. Online verfügbar unter: https://www.ti.com/lit/an/swra234/swra234.pdf

[^ti_an067_limitations]: Texas Instruments, *AN067 – Wireless M‑Bus with CC1101 (SWRA234)*, Abschnitt „Limitations of the CC1101“. Erläutert, dass der Transceiver keinen 3‑out‑of‑6‑Decoder besitzt, dass das `PKTLEN`‑Register auf 255 Bytes begrenzt ist und wie die Firmware zwischen Fixed‑ und Infinite‑Length umschalten muss. Online verfügbar unter: https://www.ti.com/lit/an/swra234/swra234.pdf