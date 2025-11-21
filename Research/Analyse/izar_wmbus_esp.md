# Analyse des GitHub‑Projekts „izar‑wmbus‑esp“

## Zweck und Funktionsweise

Das Repository **maciekn/izar‑wmbus‑esp** implementiert einen Gateway für Izar‑Wasserzähler, die im Wireless‑M‑Bus‑T‑Modus (T1) senden. Der Quellcode basiert auf der Elechouse‑CC1101‑Bibliothek und enthält eine vollständige Registerkonfiguration für den CC1101 sowie Funktionen zum Empfangen, Dekodieren, Prüfen und Interpretieren von WMBus‑T‑Frames. Das Projekt ist besonders nützlich, weil es die relevanten Einstellungen aus der TI‑Applikationsnote SWRA234 in Form eines Arrays bereitstellt und damit das Funkmodul sehr einfach initialisieren lässt.

### CC1101‑Konfiguration

In der Datei `wmbus_t_cc1101_config.h` wird ein Array `WMBUS_T_CC1101_CONFIG_BYTES` definiert, das Paare aus Register‑Adresse und Wert enthält. Diese Paare werden in `izar_wmbus.cpp` in einer Schleife geschrieben. Die Werte entsprechen den Einstellungen für den WMBus‑T‑Mode: Sync‑Word 0x3D54, 868,95 MHz Trägerfrequenz, Datenrate 100 kBaud, Frequenzabweichung 50 kHz und 2‑FSK. Einige Beispiele aus der Liste:

* **IOCFG2 (0x00, 0x06)** – GDO2 als Interrupt, wenn ein Synchronwort erkannt wurde.
* **FIFOTHR (0x03, 0x07)** – RX‑FIFO‑Schwellwert.
* **SYNC1/SYNC0 (0x04, 0x3D; 0x05, 0x54)** – Synchronisationswort 0x3D54.
* **PKTLEN (0x06, 0xFF)** – Maximale Packet‑Länge.
* **FSCTRL1 / FSCTRL0 (0x0B, 0x08; 0x0C, 0x00)** – Frequenzsynthesizer‑Regelung.
* **FREQ2/1/0 (0x0D–0x0F, 0x21 / 0x65 / 0x6A)** – Frequenzeinstellung 868,95 MHz.
* **MDMCFG4 – MDMCFG0 (0x10–0x14, 0x5C / 0x0F / 0x05 / 0x22 / 0xF8)** – Datenrate ≈100 kBaud, 2‑FSK, Kanalabstand 200 kHz[^izar_register].
* **DEVIATN (0x15, 0x50)** – Frequenzabweichung ≈ 50 kHz.
* Weitere Einträge konfigurieren AGC, Automatic Calibration, Wake‑on‑Radio und Test‑Register.

Die Liste orientiert sich direkt an der TI‑App‑Note und bildet die Grundlage für die Funkparameter des T‑Modus.

#### Codeausschnitt: Konfiguration des CC1101 für den T‑Modus

Die folgenden Registerpaare sind im Projekt [`wmbus_t_cc1101_config.h`](https://github.com/maciekn/izar-wmbus-esp/blob/master/wmbus_t_cc1101_config.h) hinterlegt und definieren alle notwendigen Einstellungen für den CC1101. Jede Zeile enthält die Registeradresse gefolgt vom zu schreibenden Wert. Der Ausschnitt deckt sowohl die wichtigen Basis‑Register (Synchronwort, Paketlänge, Frequenz und Modemparameter) als auch AGC‑, Wake‑on‑Radio‑ und Test‑Register ab[^izar_register]. Durch diese Tabelle kann man die CC1101‑Initialisierung einfach in eigene Projekte übernehmen.

```c
// Ausschnitt aus wmbus_t_cc1101_config.h (Zeilen 8–55)
const uint8_t WMBUS_T_CC1101_CONFIG_BYTES[] = {
    CC1101_IOCFG2,0x06,
    CC1101_IOCFG1,0x2E,
    CC1101_IOCFG0,0x00,
    CC1101_FIFOTHR,0x07,
    CC1101_SYNC1,0x54,
    CC1101_SYNC0,0x3D,
    CC1101_PKTLEN,0xFF,
    CC1101_PKTCTRL1,0x00, // disable APPEND_STATUS
    CC1101_PKTCTRL0,0x00,
    CC1101_ADDR,0x00,
    CC1101_CHANNR,0x00,
    CC1101_FSCTRL1,0x08,
    CC1101_FSCTRL0,0x00,
    CC1101_FREQ2,0x21,
    CC1101_FREQ1,0x6B,
    CC1101_FREQ0,0xD0,
    CC1101_MDMCFG4,0x5C,
    CC1101_MDMCFG3,0x04,
    CC1101_MDMCFG2,0x05,
    CC1101_MDMCFG1,0x22,
    CC1101_MDMCFG0,0xF8,
    CC1101_DEVIATN,0x44,
    CC1101_MCSM2,0x07,
    CC1101_MCSM1,0x00,
    CC1101_MCSM0,0x18,
    CC1101_FOCCFG,0x2E,
    CC1101_BSCFG,0xBF,
    CC1101_AGCCTRL2,0x43,
    CC1101_AGCCTRL1,0x09,
    CC1101_AGCCTRL0,0xB5,
    CC1101_WOREVT1,0x87,
    CC1101_WOREVT0,0x6B,
    CC1101_WORCTRL,0xFB,
    CC1101_FREND1,0xB6,
    CC1101_FREND0,0x10,
    CC1101_FSCAL3,0xEA,
    CC1101_FSCAL2,0x2A,
    CC1101_FSCAL1,0x00,
    CC1101_FSCAL0,0x1F,
    CC1101_RCCTRL1,0x41,
    CC1101_RCCTRL0,0x00,
    CC1101_FSTEST,0x59,
    CC1101_PTEST,0x7F,
    CC1101_AGCTEST,0x3F,
    CC1101_TEST2,0x81,
    CC1101_TEST1,0x35,
    CC1101_TEST0,0x09
};
```

### Empfang und Dekodierung

Die Datei `izar_wmbus.cpp` enthält die Hauptlogik für das Empfangen der Frames:

* **Initialisierung:** Der Konstruktor lädt die Register aus dem oben beschriebenen Array in den CC1101, führt ein Hardware‑Reset und eine Kalibrierung durch und setzt das Funkmodul in den Empfangsmodus[^izar_wmbus_code].
* **3‑out‑of‑6‑Decoding:** Nachdem ein Frame empfangen wurde, wird die Funktion `decode3of6()` aufgerufen, um die 3‑out‑of‑6‑kodierten Bits in nibbles und anschließend in Bytes umzuwandeln. Diese Funktion orientiert sich an der Tabelle aus EN 13757‑4 und ist in `izar_utils.cpp` implementiert.
* **Rahmenaufbau:** Die Klasse `WMBusFrame` enthält Felder für die L‑Feld‑Länge, die Kontroll‑ und Hersteller‑IDs, die Adresse, den CI‑Code und die Nutzdaten. Nach dem Decoding wird das CRC überprüft und das Ergebnis in ein `WMBusFrame`‑Objekt geschrieben[^izar_wmbus_code].
* **Callback:** Das Projekt ermöglicht das Setzen von Callback‑Funktionen, so dass Anwender den dekodierten Telegramm‑Inhalt auswerten können (z. B. für MQTT).

### Relevanz für den Leitfaden

Dieses Projekt liefert eine einsatzbereite Registerkonfiguration für den CC1101 im T‑Modus sowie eine durchdachte Implementierung des 3‑out‑of‑6‑Decodings und des Frame‑Parsers. Im Gegensatz zum C‑Modus müssen im T‑Modus das Synchronwort und die Datenkodierung beachtet werden; `izar-wmbus-esp` zeigt, wie man nach dem Empfang dynamisch zwischen Fixed‑ und Infinite‑Length‑Modus wechselt, den RX‑FIFO regelmäßig ausliest und die Bytes decodiert. Durch die Integration dieser Erkenntnisse kann ein eigener WMBus‑T‑Gateway auf Basis eines Mikrokontrollers erstellt werden.

[^izar_register]: Die Datei `wmbus_t_cc1101_config.h` im GitHub‑Projekt „izar‑wmbus‑esp“ listet die CC1101‑Register und ihre Werte für den Wireless‑M‑Bus‑T‑Modus (z. B. Sync‑Wort, Frequenz, Modem‑Parameter). Link: https://github.com/maciekn/izar-wmbus-esp/blob/master/wmbus_t_cc1101_config.h

[^izar_wmbus_code]: Die Datei `izar_wmbus.cpp` im Projekt „izar‑wmbus‑esp“ zeigt die Initialisierung und den Ablauf zum Empfangen und Decodieren der T‑Frames (Lines 23–38 für die Initialisierung, 64–139 für das Frame‑Parsing). Link: https://github.com/maciekn/izar-wmbus-esp/blob/master/izar_wmbus.cpp