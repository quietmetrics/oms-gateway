# Leitfaden zur Implementierung eines Wireless‑M‑Bus‑Gateways mit dem CC1101 im T‑Modus

Dieser Leitfaden beschreibt detailliert, wie ein Microcontroller‑basiertes Gateway aufgebaut wird, das drahtlose M‑Bus‑Telegramme im **T‑Modus** empfängt, dekodiert und weiterverarbeitet. Er verbindet die Informationen aus offiziellen Dokumenten (TI‑Applikationsnote AN067, EN 13757‑4), Hersteller‑Guides und existierenden Open‑Source‑Implementierungen. **Die Beschreibung ist auf die Beispielbibliothek in `Example wM Bus Libary` abgestimmt (ESP32 + CC1101, Arduino‑Framework, Techem‑Decoder).** Das Ziel ist, die Konfiguration des **CC1101‑Transceivers** und die Verarbeitung der empfangenen Daten so aufzubereiten, dass ein vollständiges Gateway erstellt werden kann.

## 1 Überblick über Wireless‑M‑Bus und T‑Modus

Der Wireless‑M‑Bus (wMBus) ist ein europäischer Standard (EN 13757‑4) für die drahtlose Fernablesung von Zählern. Er definiert mehrere Betriebsmodi:

- **S‑Mode** für stationäre Geräte mit niedriger Datenrate (169 MHz).
 - **T‑Mode** für häufige Übertragungen bei 868,95 MHz mit 100 kBaud. Der Modus existiert als **T1** (unidirektional: Meter sendet an Gateway) und **T2** (bidirektional: Meter öffnet nach dem Senden ein kurzes Empfangsfenster)[^ti_an067_modes].
- **C‑Mode** (Compact) mit 50/100 kBaud NRZ‑Codierung.

Dieser Leitfaden konzentriert sich auf den **T1‑Modus**, der die meisten batteriebetriebenen Wasser‑ und Wärmezähler nutzt. Dabei werden die Daten mit **3‑out‑of‑6‑Codierung** übertragen und ein spezielles Synchronwort genutzt (im Beispiel `0x54 0x3D`; entspricht dem in AN067 angegebenen 0x3D54, aber in Byte‑Reihenfolge getauscht)[^ti_an067_sync].

## 2 Hardware – Komponenten und Verdrahtung

### 2.1 Benötigte Hardware

1. **Microcontroller** – z. B. ESP32, STM32, MSP430 oder ein anderes Board mit SPI‑Schnittstelle und genügend RAM. Für wMBus‑Decoding sind etwa 3–4 KiB RAM sinnvoll.
2. **CC1101‑Transceiver** – ein 868 MHz‑Modul (z. B. Modul im 868/915‑MHz‑Breakout). Er unterstützt 2‑FSK und wird über SPI angesteuert.
3. **Antenne** – eine 868‑MHz‑Helix‑Antenne oder auf das Modul abgestimmte Antenne, um Reichweite zu gewährleisten.
4. **Spannungsversorgung** – 3,3 V für den CC1101. Manche ESP32‑Boards liefern 3,3 V; ggf. Spannungsregler nutzen.
5. **Optionale Komponenten** – Pegelwandler (falls MCU mit 5 V arbeitet), Pegelwiderstände für Reset‑Pins, eventuell ein SAW‑Filter zur Verbesserung der Selektivität.

### 2.2 Pinbelegung

Der CC1101 wird über SPI verbunden. Typische Zuordnung (wie in `esp‑multical21` verwendet[^multical_readme]):

| CC1101‑Pin | Funktion           | MCU‑Pin (Beispiel ESP32) |
| ---------- | ------------------ | ------------------------ |
| GDO2       | Interrupt 1        | GPIO 3                   |
| GDO0       | Interrupt 2 (FIFO) | GPIO 2                   |
| CSN        | Chip‑Select        | GPIO 7                   |
| MOSI       | SPI MOSI           | GPIO 6                   |
| MISO       | SPI MISO           | GPIO 5                   |
| SCK        | SPI Clock          | GPIO 4                   |
| VCC        | 3,3 V Versorgung   | 3V3                      |
| GND        | Masse              | GND                      |

*Hinweis:* GDO2 wird oft als „Sync detected“ konfiguriert, GDO0 als „RX FIFO threshold reached“. Beide Pins können über Interrupts am Mikrocontroller eingelesen werden.

## 3 Initialisierung des CC1101 für wMBus T‑Modus

### 3.1 Reset und Kalibrierung

1. CSN kurz toggeln, um den CC1101 zu wecken, dann das **SRES‑Strobe** (Reset, `0x30`) senden; in der Beispielbibliothek erfolgt dies in `cc1101_reset()`.
2. Danach werden die Register gemäß Tabelle 1 geschrieben (`cc1101_initRegisters()`).
3. Eine separate `SCAL`‑Kalibrierung wird im Beispiel **nicht** ausgelöst; die Autokalibrierung greift beim Wechsel Idle→RX (siehe `MCSM0=0x18`).
4. Der Wechsel in den RX‑Modus erfolgt erst beim Start des Empfangs über `SRX` in `startReceiving()`.

### 3.2 Registereinstellungen

Die folgenden Werte stammen direkt aus `cc1101_initRegisters()` des Beispielcodes und sind auf den wMBus‑T1‑Empfang abgestimmt. Adressen sind hexadezimal.

**Tabelle 1 – CC1101‑Registerwerte (wie im Beispiel implementiert)**

| Register                | Wert | Erläuterung (aus dem Code abgeleitet)                                   |
| ----------------------- | ----:| ------------------------------------------------------------------------ |
| `IOCFG2`                | 0x06 | GDO2: Sync‑Interrupt (endet Empfang/Frame).                             |
| `IOCFG1`                | 0x2E | GDO1: Tristate.                                                         |
| `IOCFG0`                | 0x00 | GDO0: FIFO‑Threshold Interrupt.                                         |
| `FIFOTHR`               | 0x07 | FIFO‑Schwelle ca. 32 Byte (im Empfang wird erst auf 4 Byte, dann wieder erhöht). |
| `SYNC1`                 | 0x54 | Sync‑Word High‑Byte (Sync‑Word insgesamt 0x54 3D).                      |
| `SYNC0`                 | 0x3D | Sync‑Word Low‑Byte.                                                     |
| `PKTLEN`                | 0xFF | Maximale Packetlänge, wird dynamisch angepasst.                         |
| `PKTCTRL1`              | 0x00 | Keine Adressprüfung, Status‑Anhänge aus.                                |
| `PKTCTRL0`              | 0x00 | Normal Mode, Manchester/Whitening aus; Length Mode wird umgeschaltet.   |
| `ADDR`                  | 0x00 | Nicht genutzt.                                                          |
| `CHANNR`                | 0x00 | Kanal 0.                                                                |
| `FSCTRL1`               | 0x08 | Frequenzsynthese‑Regelung.                                              |
| `FSCTRL0`               | 0x00 | Frequenzfeinabstimmung.                                                 |
| `FREQ2`/`1`/`0`         |0x21/0x6B/0xD0| Trägerfrequenz im 868‑MHz‑Band.                                   |
| `MDMCFG4`               | 0x5C | Datenrate/Bandbreite für 100 kBaud T‑Mode.                              |
| `MDMCFG3`               | 0x04 | Datenraten‑Feineinstellung.                                             |
| `MDMCFG2`               | 0x05 | 2‑FSK, kein Manchester, Sync‑Detektion ein.                             |
| `MDMCFG1`               | 0x22 | Kanalbandbreite/Num. Preable Bytes.                                     |
| `MDMCFG0`               | 0xF8 | Kanalabstand.                                                           |
| `DEVIATN`               | 0x44 | Frequenzabweichung.                                                     |
| `MCSM2`                 | 0x07 | Main State Machine Konfiguration.                                       |
| `MCSM1`                 | 0x00 | Nach Empfang in IDLE gehen (ISR signalisiert Ende).                     |
| `MCSM0`                 | 0x18 | Autokalibrierung Idle→RX.                                               |
| `FOCCFG`                | 0x2E | Frequenz‑Offset‑Kompensation.                                           |
| `BSCFG`                 | 0xBF | Bit‑Synchronisation.                                                    |
| `AGCCTRL2`/`1`/`0`      |0x43/0x09/0xB5| AGC‑Parameter.                                                    |
| `WOREVT1`/`0`           |0x87/0x6B| Wake‑On‑Radio Timer (hier statisch gesetzt).                        |
| `WORCTRL`               | 0xFB | WOR‑Konfiguration.                                                      |
| `FREND1`/`0`            |0xB6/0x10| Frontend‑Config.                                                     |
| `FSCAL3`–`0`            |0xEA/0x2A/0x00/0x1F| Kalibrierung.                                                   |
| `RCCTRL1`/`0`           |0x41/0x00| RC‑Oszillator.                                                      |
| `FSTEST`                | 0x59 | Test‑Register (Empfehlung TI).                                         |
| `PTEST`                 | 0x7F | Produktions‑Test.                                                       |
| `AGCTEST`               | 0x3F | AGC‑Test.                                                               |
| `TEST2`/`1`/`0`         |0x81/0x35/0x09| Weitere Test‑Register.                                           |

Diese Tabelle spiegelt exakt die im Beispiel genutzten Werte wider. Abweichungen zu anderen Projekten sind möglich; wenn Register geändert werden, müssen auch die Annahmen der Empfangslogik überprüft werden.

### 3.3 Umschalten zwischen Fixed‑ und Infinite‑Length (wie im Beispiel umgesetzt)

Der Code arbeitet standardmäßig im **Infinite‑Length‑Modus**, bis die tatsächliche Länge aus dem L‑Feld bekannt ist. Ablauf (`rxFifoISR`):

1. GDO0 löst bei 4 Byte FIFO‑Füllstand aus. Die ISR liest die ersten 3 Byte und decodiert den 3‑out‑of‑6‑codierten L‑Wert.  
2. Aus `L` wird die paketweite Bytezahl (`packetSize` → `byteSize`) berechnet.  
3. Wenn die Gesamtlänge < 256 Byte ist, wird `PKTLEN` auf diese Länge gesetzt und `PKTCTRL0` auf **Fixed Length** umgeschaltet. Ist sie größer, bleibt Infinite‑Length aktiv; `PKTLEN` wird auf den Rest modulo 256 gesetzt, um den Empfang fortzuführen.  
4. Danach wird die FIFO‑Schwelle auf 32 Byte angehoben, damit der FIFO seltener ausgelesen werden muss.  

Dieses Verhalten erklärt die Wertewechsel von `FIFOTHR` und `PKTCTRL0` in den Interrupt‑Handlern.

## 4 Empfangsablauf und FIFO‑Management

Der CC1101 verfügt über einen 64‑Byte‑RX‑FIFO. Bei wMBus‑T‑Frames (typisch 100–300 Bytes nach Decodierung) muss der FIFO periodisch ausgelesen werden. Ein möglicher Ablauf:

1. **GDO0 (FIFO‑Interrupt):** Startet bei 4 Byte im FIFO. Die ISR liest die ersten 3 Byte, decodiert das L‑Feld und wählt Fixed‑ oder Infinite‑Mode (Abschnitt 3.3). Danach wird die FIFO‑Schwelle auf 32 Byte gesetzt und der FIFO blockweise geleert.  
2. **GDO2 (Packet‑Complete):** Signalisiert das Paketende. Die ISR liest die restlichen Bytes aus dem FIFO und markiert das Paket als vollständig.  
3. **Überlauf vermeiden:** Der Code nutzt `SFRX` vor jedem Empfang, prüft aber den Status während des Empfangs nicht erneut. Bei Anpassungen sollte das RXBYTES‑Statusbit geprüft und bei Overflow ein Reset erfolgen.  
4. **Ende des Frames:** Nach dem GDO2‑Interrupt wechselt die Hauptschleife in `stopReceiving`, setzt den Transceiver in IDLE und decodiert/prüft das Paket.

Dieser Ablauf entspricht exakt den beiden ISR‑Funktionen `rxFifoISR` und `rxPacketRecvdISR` im Beispielcode; die eigentliche Decodierung passiert anschließend im Hauptloop.

## 5 3‑out‑of‑6‑Decodierung

Im T‑Modus sind die Nutzdaten 3‑out‑of‑6 codiert. Jeder 6‑Bit‑Block enthält genau drei Einsen. Dadurch wird eine bessere Gleichstrombalance erreicht und der Empfänger kann die Taktsynchronisation aufrechterhalten. Die Beispielbibliothek nutzt folgende Zuordnung (aus `3outof6.cpp`):

| Nibble (hex) | Code (hex) | Codebits (b5…b0) |
| ------------ | ---------- | ---------------- |
| 0            | 0x16       | 010110           |
| 1            | 0x0D       | 001101           |
| 2            | 0x0E       | 001110           |
| 3            | 0x0B       | 001011           |
| 4            | 0x1C       | 011100           |
| 5            | 0x19       | 011001           |
| 6            | 0x1A       | 011010           |
| 7            | 0x13       | 010011           |
| 8            | 0x2C       | 101100           |
| 9            | 0x25       | 100101           |
| A            | 0x26       | 100110           |
| B            | 0x23       | 100011           |
| C            | 0x34       | 110100           |
| D            | 0x31       | 110001           |
| E            | 0x32       | 110010           |
| F            | 0x29       | 101001           |

Nicht abgedeckte 6‑Bit‑Werte werden beim Decodieren als `0xFF` markiert und führen zu `PACKET_CODING_ERROR`.

### 5.1 Decodieralgorithmus (Pseudocode)

```pseudo
function decode3of6(input_bytes):
    // input_bytes: Array mit 6‑Bit‑Blöcken (kodiert)
    output_nibbles = []
    for each 6‑Bit‑symbol in input_bytes:
        if symbol not in lookup_table:
            return error // ungültiger Code
        nibble = lookup_table[symbol]
        append nibble to output_nibbles
    // Kombiniere jeweils zwei Nibbles zu einem Byte
    output_bytes = []
    for i in 0..length(output_nibbles)/2 - 1:
        output_bytes.append((output_nibbles[2*i] << 4) | output_nibbles[2*i+1])
    return output_bytes
```

Die Lookup‑Tabelle kann als 64‑Elemente‑Array implementiert werden, wie es der ESPHome‑Baustein `decode3of6.cpp` zeigt[^esphome_decode3of6].

## 6 CRC‑Berechnung

Für jeden Block (Block 1, 2 oder optionale Blöcke) wird eine 16‑Bit‑CRC berechnet. Das Polynom lautet \(x^{16} + x^{13} + x^{12} + x^{11} + x^{10} + x^8 + x^6 + x^5 + x^2 + 1\)[^ti_an067_frame] (hexadezimal 0x3D65). Die Beispielbibliothek startet den CRC‑Akkumulator bei `0x0000`, schiebt pro Bit und vergleicht anschließend das bitweise invertierte Ergebnis mit den empfangenen CRC‑Bytes. Das entspricht `crc.cpp`:

```c
// crcReg: aktueller CRC-Wert, crcData: Datenbyte
uint16_t crcCalc(uint16_t crcReg, uint8_t crcData) {
    for (int i = 0; i < 8; i++) {
        if (((crcReg & 0x8000) >> 8) ^ (crcData & 0x80))
            crcReg = (crcReg << 1) ^ 0x3D65;
        else
            crcReg = (crcReg << 1);
        crcData <<= 1;
    }
    return crcReg;
}
```

`decodeRXBytesTmode()` baut den CRC fortlaufend auf, erwartet im Telegramm die Zweierkomplement‑Darstellung (`~crc`) und bricht bei Abweichungen mit `PACKET_CRC_ERROR` ab.

## 7 Rahmenstruktur und Felder

Nach der 3‑out‑of‑6‑Decodierung lässt sich das Telegramm in Felder zerlegen[^ti_an067_frame]:

| Feld                 | Länge                   | Beschreibung                                                                                                           |
| -------------------- | ----------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| **L‑Feld**           | 1 Byte (codiert)        | Gibt die Länge des nachfolgenden Blocks in Bytes an (ohne L‑Feld und CRC). Wird zweimal im codierten Block übertragen. |
| **C‑Feld**           | 1 Byte                  | Steuerung: u. a. Definition der Übertragung (z. B. Access Number, Funktion etc.).                                      |
| **M‑Feld**           | 2 Byte                  | Herstellerkennung (z. B. 0x7046 = „Kamstrup“).                                                                         |
| **A‑Feld**           | 4–8 Byte                | Geräteadresse (ID), Version und Medium.                                                                                |
| **CRC1**             | 2 Byte                  | 16‑Bit‑CRC über Block 1.                                                                                               |
| **CI‑Feld**          | 1 Byte                  | Application Information Code. Für OMS‑Telegramme z. B. 0x78 (long application header).                                 |
| **User Data**        | variabel                | Messwerte und Statusinformationen. Können in weitere Subfelder (DIF/VIF/VIFE) unterteilt sein.                         |
| **CRC2**             | 2 Byte                  | CRC über CI‑Feld und User Data (Block 2).                                                                              |
| **Optionale Blöcke** | 16 Byte + CRC pro Block | Dienen zur Fragmentierung großer Nachrichten.                                                                          |

Beim Parsen muss nach dem L‑Feld das C‑Feld, das M‑Feld und die Adresse extrahiert werden. In vielen Fällen genügt es, anhand des A‑Felds (Geräte‑ID) zu entscheiden, ob das Telegramm relevant ist. Der CI‑Code bestimmt das Protokoll der Nutzdaten (z. B. standardmäßige M‑Bus‑Rahmen, OMS, SML oder hersteller­spezifische Daten).

**Beispielauswertung im Projekt:** `decodeTechemWaterMeters()` prüft ausschließlich auf Hersteller `0x6850` (Techem), nutzt `RXpacket[0] == 0x2f` als L‑Feld und interpretiert die Typ‑Bytes `0x72` (Kalt) bzw. `0x62` (Warm). Die Verbrauchswerte werden aus den Bytes 16/17 (bisheriger Wert) und 20/21 (aktueller Wert) gelesen, zu Kubikmetern skaliert und mit festen Geräte‑IDs (`cold_meter_id`, `warm_meter_id`) verglichen. Andere Hersteller/Protokolle werden nicht analysiert.

## 8 Umgang mit Verschlüsselung

Viele wMBus‑Zähler verschlüsseln ihre Nutzdaten. Die Beispielbibliothek implementiert **keine Entschlüsselung** – sie erkennt lediglich Techem‑Telegramme an Hersteller/ID und wertet unverschlüsselte Felder aus. Für vollständige Gateways ist oft **Mode 5** (AES‑128‑CBC) relevant[^radiocrafts_security]. Der Schlüssel ist zähler­spezifisch und muss dem Gateway bekannt sein (oft in den Unterlagen des Versorgers aufgeführt). Bei Mode 5 wird vor der verschlüsselten Nutzlast ein 2‑Byte‑Verifikationswert eingefügt. Die Initialisierungs‑Vektoren werden aus Adresse, Hersteller und Nachrichtenzähler gebildet. Eine grobe Vorgehensweise zur Entschlüsselung:

1. Nach dem Decoding der Daten den Bereich ab dem CI‑Feld identifizieren, der verschlüsselt ist. Das CI‑Feld zeigt die Verschlüsselungsart (z. B. 0x8A = encrypted application layer).  
2. Den 2‑Byte‑Verifikationswert und die verschlüsselte Nutzlast in 16‑Byte‑Blöcke aufteilen. Fehlende Byte werden mit 0x2F aufgefüllt (Filler).  
3. Den Initialisierungsvektor (IV) aus dem Zähleridentifikationsfeld und dem Nachrichten‑Zähler generieren (spezifisch pro Hersteller).  
4. Mit dem AES‑128‑CBC‑Algorithmus den Datenblock entschlüsseln.  
5. Den Verifikationswert prüfen, Nutzdaten extrahieren und weitere DIF/VIF‑Felder auswerten.  

Die Applikationsnote AN043 beschreibt daneben Mode 7 (AES‑CBC + CMAC) und Mode 13 (TLS) sowie die drei OMS‑Sicherheitsprofile[^radiocrafts_security]. Diese sind komplexer und erfordern MAC‑Berechnung oder TLS‑Stack. In der Praxis wird Mode 5 am häufigsten eingesetzt.

## 9 Softwarearchitektur des Gateways

Angelehnt an die Open‑Source‑Implementierungen ergibt sich folgende modulare Architektur:

1. **Transceiver‑Abstraktion (`cc1101.cpp/h`):** Kapselt SPI‑Zugriffe, Strobes und Registerwerte. Wird im Sketch direkt genutzt, keine Objekt‑Klasse.  
2. **RX‑Handler (`startReceiving`/ISR):** Zwei Interrupts lesen den FIFO blockweise, schalten zwischen Infinite/Fixed Length um und markieren Paketende.  
3. **Decoder (`3outof6.cpp`, `mbus_packet.cpp`):** 3‑out‑of‑6‑Decodierung, Paketgrößenberechnung und CRC‑Prüfung. Liefert `PACKET_OK`, `PACKET_CODING_ERROR` oder `PACKET_CRC_ERROR`.  
4. **Parser/Anwendung (`main.cpp`):** Identifiziert Techem‑Telegramme an Hersteller 0x6850 und vergleicht Geräte‑IDs (`cold_meter_id`, `warm_meter_id`). Weitere Protokollinterpretation oder Entschlüsselung ist nicht implementiert.  
5. **Erweiterungsschicht (optional):** Für MQTT/HTTP‑Weitergabe oder Entschlüsselung müsste zusätzlicher Code ergänzt werden; derzeit werden die Pakete nur über die serielle Konsole ausgegeben.

## 10 Herausforderungen und Tipps

* **FIFO‑Überlauf:** Da der CC1101 nur 64 Byte puffern kann, muss der Mikrocontroller den FIFO regelmäßig leeren und den Empfangsbuffer verarbeiten. Ein ringförmiger Software‑Puffer ist sinnvoll.
* **Timing und Interrupt‑Last:** Das 100‑kBaud‑Signal erzeugt ca. 16,6 kByte/s (nach Codierung). Die ISR darf nicht zu viel Zeit benötigen; nutze deshalb nur das Lesen des FIFO in der ISR und verschiebe die Decodierung in den Hauptthread.
* **Umschalten von Fixed‑ auf Infinite‑Length:** Beachte, dass vor dem Ändern des `PKTCTRL0` der Empfänger in IDLE versetzt werden muss. Beispiele zeigen, dass die ersten Bytes gelesen, der Modus eingestellt und anschließend der Empfang fortgesetzt wird[^esp32_length_switch].
* **Frequenz‑Kalibrierung:** Nutze das `SCAL`‑Strobe nach dem Setzen der Register, um die Frequenzsynthese zu kalibrieren. Bei Temperaturschwankungen oder langer Betriebszeit kann eine regelmäßige Rekalibrierung nötig sein.
* **Empfangsfilter:** Die wMBus‑Standard‑Telegramme enthalten je zwei identische L‑Byte in 3‑out‑of‑6‑Codierung zur Fehlererkennung. Prüfe, ob beide identisch sind, bevor du die Paketlänge akzeptierst.
* **Key‑Management:** Hinterlege die AES‑Schlüssel sicher im Flash/Secure‑Element. Implementiere eine Möglichkeit, Schlüssel zu aktualisieren (OTA‑Update, Konfig‑Datei). Beachte, dass die OMS‑Profile B/C MAC‑Berechnungen erfordern.

## 11 Zusammenfassung

Mit einem CC1101 und einem geeigneten Mikrocontroller lässt sich ein leistungsfähiger wMBus‑Gateway im T‑Modus realisieren. Die wichtigsten Schritte sind:

1. **Hardware verbinden** und GDO‑Pins korrekt konfigurieren.
2. **CC1101‑Register** gemäß Tabelle 1 programmieren und den Transceiver kalibrieren.
3. **Empfangslogik** implementieren: Synchronwort erkennen, L‑Feld ermitteln, FIFO auslesen, Decodieren und CRC prüfen.
4. **Rahmen analysieren:** Felder extrahieren, optional verschlüsselte Nutzdaten mit AES‑128‑CBC entschlüsseln.
5. **Daten weitergeben** an ein übergeordnetes System (z. B. MQTT/BACnet).  

Offene Implementierungen wie `esp32_cc1101_wmbus`, `izar‑wmbus‑esp` und der ESPHome‑Baustein liefern praktische Beispiele für Registerwerte und Code. Die TI‑Applikationsnote stellt die theoretischen Grundlagen bereit, während Radiocrafts‑ und Silicon‑Labs‑Dokumente die Betriebsmodi und Datenraten vergleichen. Durch Kombination dieser Quellen entsteht ein umfassender Leitfaden für den Aufbau eines Wireless‑M‑Bus‑Gateways.

## 12 Umgang mit ungültigen Paketen und Funkstörungen

Bei starkem Störpegel (z. B. in städtischen Gebieten) können aus dem CC1101 auch **Rauschpakete** kommen, die zufällig das Synchronwort enthalten. Um diese zu unterdrücken, sollte sowohl die Hardware als auch die Software feinjustiert werden:

1. **Vier‑Byte‑Preamble und Sync‑Word:** Eine längere Preamble und ein längeres Synchronwort verbessern die Bit‑ und Byte‑Synchronisation und reduzieren die Wahrscheinlichkeit, dass zufällige Rauschsequenzen als gültiger Rahmen erkannt werden. TI empfiehlt daher 4 Bytes für Preamble und Sync‑Word im CC1101[^ti_forum_preamble].
2. **Preamble‑Quality‑Threshold (PQT):** Der CC1101 verfügt über einen Pre­amble‑Qualitätszähler. Über `PKTCTRL1.PQT` wird die minimale Qualität definiert, bevor ein Synchronwort akzeptiert wird. Ein Wert von 1–3 ist sinnvoll; `PQT=0` deaktiviert die Prüfung[^cc1101_datasheet_pqt].
3. **Sync‑Qualifier mit Carrier‑Sense:** Der Sync‑Qualifier‑Modus in `MDMCFG2.SYNC_MODE` legt fest, unter welchen Bedingungen das Synchronwort gültig ist. Für wMBus bietet sich `16/16 + CS` oder `30/32 + CS` an, wodurch nur bei voller Übereinstimmung und ausreichendem RSSI ein Paket akzeptiert wird[^cc1101_datasheet_syncqual]. Das Carrier‑Sense‑Signal kann auf einem GDO‑Pin ausgegeben werden; erst wenn es high ist, beginnt die Software mit dem Empfang[^ti_forum_cs].
4. **Kanalfilter‑Bandbreite reduzieren:** Die digitale Filterbandbreite lässt sich über `MDMCFG4.CHANBW_E/M` anpassen. Eine schmalere Bandbreite (z. B. 162 kHz statt 203 kHz) unterdrückt Rauschen außerhalb des gewünschten Kanals, erfordert aber eine genauere Frequenz‑Offset‑Kompensation[^cc1101_datasheet_bandwidth].
5. **Frequenz‑Offset‑Kompensation:** Im Register `FOCCFG` kann der AFC‑Algorithmus so konfiguriert werden, dass er erst nach Carrier‑Sense aktiv wird (`FOC_BS_CS_GATE`), um Drift auf Rauschen zu vermeiden[^cc1101_datasheet_foccfg].
6. **Externer SAW‑Filter:** In stark verrauschten Umgebungen hilft ein vorgeschalteter SAW‑Bandpass‑Filter, Störungen außerhalb des 868‑MHz‑Bandes zu unterdrücken[^ti_forum_saw].
7. **AGC‑Parameter optimieren oder einfrieren:** Die AGC reduziert die Verstärkung deutlich schneller als sie sie erhöht. Eine kurze `AGCCTRL0.WAIT_TIME` reduziert die Einschwingzeit. Für spezielle Anwendungen kann die AGC durch Setzen von `AGCCTRL0.AGC_FREEZE` eingefroren und die Verstärkung in `AGCTEST` manuell eingestellt werden[^ti_forum_agc]. Diese Methode erfordert jedoch eine genaue Kenntnis des Eingangssignalpegels und wird von TI nur für Spezialfälle empfohlen.
8. **CRC‑Auto‑Flush und Statusauswertung:** `PKTCTRL1.CRC_AUTOFLUSH` leert das RX‑FIFO automatisch bei fehlerhafter CRC. Mit `APPEND_STATUS=1` werden RSSI/LQI an das Ende jedes Pakets angehängt; Pakete mit geringer LQI können verworfen werden[^cc1101_datasheet_crc].
9. **Software‑Checks:** Implementiere Prüfungen für die 3‑out‑of‑6‑Decodierung, kontrolliere die doppelte L‑Feld‑Übertragung und verwerfe Pakete mit falscher CRC oder ungewöhnlich niedriger RSSI/LQI. Speichere die `Access‑Number` aus dem C‑Feld, um Duplikate zu erkennen.

Weitere Details und Beispielcode zur Konfiguration dieser Filtermechanismen sind in den Analyse‑Dokumenten **cc1101_noise_and_filter.md** und **cc1101_filter_options_noise.md** beschrieben. Durch die Anpassung der oben genannten Parameter lässt sich die Robustheit des Gateways bei hohem Rauschen signifikant erhöhen.

[^ti_an067_modes]: Texas Instruments. *AN067 – Wireless M‑Bus with CC1101 (SWRA234)*. Dieses Dokument beschreibt die Betriebsmodi von Wireless‑M‑Bus (S, T, R) und ordnet den T‑Modus dem Radio‑Link B zu. PDF: https://www.ti.com/lit/an/swra234/swra234.pdf

[^ti_an067_sync]: Texas Instruments. *AN067 – Wireless M‑Bus with CC1101 (SWRA234)*. Tabelle 9 enthält das 16‑Bit‑Synchronwort (0x3D54) sowie die 3‑out‑of‑6‑Codierung der 16 Nibbles. PDF: https://www.ti.com/lit/an/swra234/swra234.pdf

[^izar_register]: GitHub‑Projekt „izar‑wmbus‑esp“. Die Datei `wmbus_t_cc1101_config.h` listet Register‑Wert‑Paare für den CC1101 im WMBus‑T‑Modus. URL: https://github.com/maciekn/izar-wmbus-esp/blob/master/wmbus_t_cc1101_config.h

[^esp32_length_switch]: GitHub‑Projekt „esp32_cc1101_wmbus“. In `src/main.cpp` wird beim ersten Empfangs‑Interrupt die Paketlänge decodiert und je nach Länge der Fixed‑ oder Infinite‑Length‑Modus ausgewählt (Zeilen 30–62). URL: https://github.com/alex-icesoft/esp32_cc1101_wmbus/blob/main/src/main.cpp#L30-L62

[^esphome_decode3of6]: GitHub‑Repository „esphome-components/wmbus_radio“. Die Datei `decode3of6.cpp` enthält eine Lookup‑Tabelle und Funktion zum Decodieren des 3‑out‑of‑6‑Codes (Zeilen 10–49). URL: https://github.com/SzczepanLeon/esphome-components/blob/main/components/wmbus_radio/decode3of6.cpp

[^ti_an067_frame]: Texas Instruments. *AN067 – Wireless M‑Bus with CC1101 (SWRA234)*. Abschnitt „Packet Format“ und Tabelle 10 beschreiben die Struktur von Block 1 und Block 2 sowie das CRC‑Polynom. PDF: https://www.ti.com/lit/an/swra234/swra234.pdf

[^radiocrafts_security]: Radiocrafts AS. *AN043 – Wireless M‑Bus Security* (2020). Die Applikationsnote erläutert die Sicherheitsmodi 5, 7, 8 und 13 sowie die OMS‑Sicherheitsprofile A–C. URL: https://radiocrafts.com/uploads/rc232-wmbus-security-application-note.pdf

[^esphome_radio_component]: Szczepan Leon. *esphome-components/wmbus_radio* – Die Datei `component.cpp` (Zeilen 29–72) zeigt, wie der Radio‑Task Daten aus dem CC1101 empfängt, in eine Queue legt und als Frames verarbeitet. URL: https://github.com/SzczepanLeon/esphome-components/blob/main/components/wmbus_radio/component.cpp

[^multical_readme]: GitHub‑Projekt „esp‑multical21“. Die README (Zeilen 101–115) beschreibt die Pinbelegung des CC1101 für ESP8266/ESP32‑Boards. URL: https://github.com/chester4444/esp-multical21#readme

[^cc1101_datasheet_pqt]: Texas Instruments. *CC1101 Low‑Power Sub‑1 GHz RF Transceiver* – Datenblatt. Abschnitt 17.2 (Preamble Quality Threshold) erklärt, wie über `PKTCTRL1.PQT` der Schwellwert definiert wird und wie `PQT_REACHED` abgefragt werden kann.

[^cc1101_datasheet_syncqual]: Texas Instruments. *CC1101 Low‑Power Sub‑1 GHz RF Transceiver* – Datenblatt. Tabelle 29 erläutert die Sync‑Word‑Qualifier‑Modi (15/16, 16/16, 30/32) und die Kombination mit Carrier‑Sense.

[^ti_forum_cs]: Texas Instruments. **E2E Support Forum** – Thread „CC1101: How to filter noise in asynchronous mode“. TI‑Ingenieure empfehlen, das Carrier‑Sense‑Signal auf einen GDO‑Pin zu legen und die Paketverarbeitung erst zu starten, wenn CS high ist.

[^cc1101_datasheet_bandwidth]: Texas Instruments. *CC1101 Low‑Power Sub‑1 GHz RF Transceiver* – Datenblatt. Tabelle 26 listet die programmierbaren RX‑Filterbandbreiten und deren passende Datenraten.

[^cc1101_datasheet_foccfg]: Texas Instruments. *CC1101 Low‑Power Sub‑1 GHz RF Transceiver* – Datenblatt. Abschnitt 14.1–14.3 erklärt die Frequenz‑Offset‑Kompensation und das Gate‑Bit `FOC_BS_CS_GATE`.

[^cc1101_datasheet_crc]: Texas Instruments. *CC1101 Low‑Power Sub‑1 GHz RF Transceiver* – Datenblatt. Kapitel 9.2.10 beschreibt das `PKTCTRL1`‑Register und insbesondere das Bit `CRC_AUTOFLUSH`, mit dem der RX‑FIFO bei CRC‑Fehlern automatisch geleert wird, sowie `APPEND_STATUS` für RSSI/LQI‑Anhang.

[^ti_forum_preamble]: Texas Instruments. **E2E Support Forum – CC1101 PREAMBLE, SYNC WORD QUALITY** (2020).  In dieser Diskussion erklären TI‑Ingenieure, dass das CC1101 für die Bit‑ und Byte‑Synchronisation eine Preamble und ein Sync‑Word benötigt und empfehlen für die meisten Anwendungen jeweils vier Bytes, um Fehl‑Erkennungen zu reduzieren.  URL: <https://e2e.ti.com/support/wireless-connectivity/sub-1-ghz-group/sub-1-ghz/f/sub-1-ghz-forum/1027627/cc1101-preamble-sync-word-quality>

[^ti_forum_saw]: Texas Instruments. **E2E Support Forum – CC1101 433.92 MHz ASK reception**.  In diesem Thread wird vorgeschlagen, einen SAW‑Bandpass‑Filter (z. B. ACTR433A) vor den CC1101 zu schalten, um das Empfangsband auf 433,92 MHz ± 75 kHz zu begrenzen.  URL: <https://e2e.ti.com/support/wireless-connectivity/other-wireless-group/other-wireless/f/other-wireless-technologies-forum/137497/cc1101-433-92mhz-ask-reception-remote-controlled-switches>

[^ti_forum_agc]: Texas Instruments. **E2E Support Forum – CC1101 433.92 MHz ASK reception**.  Ein TI‑Experte erläutert, dass die AGC des CC1101 die Verstärkung schneller verringert als erhöht, und dass eine kurze `AGCCTRL0.WAIT_TIME` die Einschwingzeit verringert.  Er beschreibt außerdem, wie sich die AGC einfrieren lässt (`AGCCTRL0.AGC_FREEZE`) und über das Register `AGCTEST` manuell die LNA‑ und DVGA‑Verstärkung eingestellt werden kann.  URL: <https://e2e.ti.com/support/wireless-connectivity/other-wireless-group/other-wireless/f/other-wireless-technologies-forum/137497/cc1101-433-92mhz-ask-reception-remote-controlled-switches>
