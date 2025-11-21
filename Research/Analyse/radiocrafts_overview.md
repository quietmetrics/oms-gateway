# Zusammenfassung des Radiocrafts‑Artikels „Wireless M‑Bus Technology Overview“

Der Hersteller Radiocrafts stellt auf seiner Website einen Technologie‑Überblick über das Wireless‑M‑Bus‑System bereit. Der Artikel fasst die verschiedenen Funkmodi, deren Frequenzen sowie die Datenraten zusammen. Die Informationen sind hilfreich, um den T‑Modus mit anderen Modi zu vergleichen.

## Kommunikationsmodi und Datenraten

Der Artikel listet die gebräuchlichen Wireless‑M‑Bus‑Modi **S**, **T** und **C** auf und gibt deren Eigenschaften an. In der folgenden Tabelle sind die wesentlichen Merkmale zusammengefasst:

| Modus     | Richtung                              | Frequenzband                                 | Datenrate/Line Coding                                           | Anmerkungen                                                                 |
|-----------|---------------------------------------|----------------------------------------------|-----------------------------------------------------------------|-------------------------------------------------------------------------------|
| **T1**    | Zähler → Gateway                      | 868,95 MHz oder 433 MHz                      | 100 kBit/s mit 3‑out‑of‑6‑Codierung                             | Unidirektionaler Modus für batteriebetriebene Zähler; Geräte senden in festen Intervallen an den Gateway[^radiocrafts_modes]. |
| **T2**    | Bidirektional                         | 868,95 MHz (alternativ 868,3 MHz oder 433 MHz) | Uplink: 67 kBit/s (3‑out‑of‑6), Downlink: 32,768 kBit/s Manchester‑codiert | Bidirektional; der Gateway kann Rückmeldungen senden, aber mit geringerer Datenrate[^radiocrafts_modes]. |
| **C1/C2** | Zähler → Gateway, optional bidirektional | 868,95 MHz                                  | 100 kBit/s oder 50 kBit/s, NRZ (kein 3‑out‑of‑6)                | Kompaktmodus; C2 unterstützt einen Rückkanal auf 50 kBit/s.                                   |
| **S**     | Verschiedene                          | 169 MHz oder 433 MHz                         | Sehr niedrige Datenraten (≈ 2,4 kBit/s)                         | Stationäre Geräte mit großer Reichweite.                                                         |

Aus den Angaben ergibt sich, dass die T‑Modi die hohe Datenrate von 100 kBit/s nutzen und 3‑out‑of‑6‑codierte Telegramme senden. Die C‑Modi verwenden NRZ‑Codierung mit 100 kBit/s (Uplink) bzw. 50 kBit/s (Downlink). Die S‑Modi arbeiten auf schmalen Frequenzbändern mit wesentlich geringerer Datenrate.

## Datenraten‑Matrix

Die Datenrate‑Matrix des Radiocrafts‑Artikels zeigt, dass Zähler im T‑Modus Daten mit 100 kBit/s (3‑out‑of‑6) zum Gateway übertragen, während der Collector im Rückkanal auf 32,768 kBit/s mit Manchester‑Codierung sendet. Im C‑Modus wird auf beiden Seiten NRZ‑Codierung verwendet; der Zähler sendet mit 100 kBit/s und der Collector mit 50 kBit/s[^radiocrafts_matrix].

## Wichtige Erkenntnisse

* **3‑out‑of‑6‑Codierung nur im T‑Modus:** Die T‑Modi T1 und T2 verwenden die 3‑out‑of‑6‑Codierung, um die Taktsynchronisation zu erleichtern. C‑Modi setzen auf NRZ‑Codierung.
* **Unidirektionaler vs. bidirektionaler Betrieb:** T1 und C1 sind unidirektional (nur der Zähler sendet), während T2 und C2 einen Rückkanal ermöglichen. Der Rückkanal arbeitet mit deutlich niedrigerer Datenrate und Manchester‑Codierung.
* **Anwendungskontext:** T1‑Geräte senden regelmäßig (alle paar Sekunden) Messwerte. T2 und C2 werden eher in Szenarien genutzt, in denen eine kurze Interaktion zwischen Zähler und Gateway erforderlich ist.

Diese Übersicht ergänzt die technischen Parameter aus der TI‑Applikationsnote und hilft, die Wahl des Betriebsmodus für das zu entwickelnde Gateway abzuschätzen.

[^radiocrafts_modes]: Radiocrafts. *Wireless M‑Bus Technology Overview*. Abschnitt „Modes & their uses“. Abrufbar unter: <https://radiocrafts.com/technologies/wireless-m-bus-technology-overview/>.
[^radiocrafts_matrix]: Radiocrafts. *Wireless M‑Bus Technology Overview*. Abschnitt „Data rates“. Abrufbar unter: <https://radiocrafts.com/technologies/wireless-m-bus-technology-overview/>.