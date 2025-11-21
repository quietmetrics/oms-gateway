# Analyse des GitHub‑Projekts „esp32_cc1101_wmbus“ von alex‑icesoft

## Überblick

Das Repository **alex‑icesoft/esp32_cc1101_wmbus** implementiert einen Empfänger für drahtlose M‑Bus‑Telegramme (T‑Mode/T1) auf Basis eines ESP32‑Mikrocontrollers und des TI‑Transceivers CC1101. Es handelt sich um ein PlatformIO‑Projekt, das 3‑out‑of‑6 codierte Meter‑Telegramme empfängt, dekodiert und als M‑Bus‑Rahmen ausgibt. Das Projekt ist interessant, weil es alle CC1101‑Register eigenständig konfiguriert und das 3‑out‑of‑6‑Decoding, CRC‑Prüfung und die Zerlegung des M‑Bus‑Rahmens implementiert.

### Hardware

- **Mikrocontroller**: ESP32 (Plattform IO, Arduino‑Core).
- **Funkmodul**: CC1101 auf 868 MHz, verbunden über SPI. Die Pins GDO0/GDO2 werden als Interrupt‑Eingänge genutzt und lassen sich im Code konfigurieren.

### WMBus‑Empfangslogik

Das Empfangsprogramm befindet sich in `src/main.cpp`. Es richtet die CC1101‑Interrupts ein, empfängt Daten in einem ISR und dekodiert sie anschließend im Hauptloop. Die Länge des Pakets wird anhand der ersten drei erhaltenen Byte berechnet. Wenn weniger als 256 Byte erwartet werden, schaltet das Programm den CC1101 in den Fixed‑Length‑Modus, andernfalls in den Infinite‑Packet‑Modus. Nachdem der vollständige Rahmen empfangen wurde, werden die 3‑out‑of‑6‑codierten Byte mit der Funktion `decode3outof6()` in normale Nutzdaten umgewandelt. Anschließend wird die Prüfsumme geprüft und das Ergebnis über WLAN veröffentlicht.

#### Codeausschnitt: Dynamisches Umschalten zwischen Fixed‑ und Infinite‑Packet‑Modus

Der folgende Abschnitt aus der Datei [`src/main.cpp`](https://github.com/alex-icesoft/esp32_cc1101_wmbus/blob/main/src/main.cpp) demonstriert, wie das Projekt beim ersten Empfangs‑Interrupt die Länge des Telegramms ermittelt und je nach Größe den CC1101‑Paket‑Modus umschaltet. Dazu werden zunächst drei Byte aus dem RX‑FIFO gelesen, mithilfe der 3‑out‑of‑6‑Dekodierung das L‑Feld bestimmt und daraus die erwartete Byte‑Anzahl berechnet. Bei Paketen unter 256 Byte wird der PKTCTRL0‑Registerwert auf Fixed‑Length gesetzt; bei längeren Paketen verbleibt das Modul im Infinite‑Length‑Modus. Der Ausschnitt verdeutlicht den Umgang mit der Beschränkung des CC1101‑Paket‑Engines[^esp32_length_switch].

```cpp
// Ausschnitt aus src/main.cpp (Zeilen 30–62)
// 3‑out‑of‑6‑Decoding der ersten drei Bytes und Wahl des Packet‑Modus
if (RXinfo.start == true) {
  // 3 erste Bytes aus dem RX FIFO lesen
  cc1101_readBurstReg(RXinfo.pByteIndex, CC1101_RXFIFO, 3);
  // Länge berechnen (L‑Feld decodieren)
  decode3outof6(RXinfo.pByteIndex, bytesDecoded, 0);
  RXinfo.lengthField = bytesDecoded[0];
  RXinfo.length = byteSize((packetSize(RXinfo.lengthField)));
  // Fixed‑Length‑Modus aktivieren, falls das Paket < 256 Byte lang ist
  if (RXinfo.length < (MAX_FIXED_LENGTH)) {
    cc1101_writeReg(CC1101_PKTLEN, (uint8_t)(RXinfo.length));
    cc1101_writeReg(CC1101_PKTCTRL0, FIXED_PACKET_LENGTH);
    RXinfo.format = FIXED;
  }
  // Für längere Pakete: Infinite‑Length‑Modus beibehalten und PKTLEN auf Rest setzen
  else {
    fixedLength = RXinfo.length % (MAX_FIXED_LENGTH);
    cc1101_writeReg(CC1101_PKTLEN, (uint8_t)(fixedLength));
  }
  RXinfo.pByteIndex += 3;
  RXinfo.bytesLeft = RXinfo.length - 3;
  // RX FIFO Threshold anpassen und Start‑Flag zurücksetzen
  cc1101_writeReg(CC1101_FIFOTHR, RX_FIFO_THRESHOLD);
  RXinfo.start = false;
}
```

### CC1101‑Registerkonfiguration

Die Datei `lib/wmbus/cc1101.cpp` kapselt die CC1101‑Register‑Zugriffe (SPI‑Lese‑/Schreibfunktionen) und enthält eine Routine `cc1101_initRegisters()`, die den CC1101 für den WMBus‑T‑Mode konfiguriert. Die Werte orientieren sich an der TI‑Applikationsnotiz für Radio‑Link B (T‑Mode) und stellen 868,95 MHz mit 100 kBaud sowie eine Frequenzabweichung von 50 kHz ein. Besonders wichtige Registerwerte:

| Register            | Wert  | Bedeutung |
|---------------------|------:|-----------|
| `IOCFG2`            | `0x06`| GDO2‐Pin als „assert when sync word sent/received“ |
| `IOCFG1`            | `0x2E`| GDO1 High‑Impedanz (nicht genutzt) |
| `IOCFG0`            | `0x00`| GDO0‑Pin als „RX FIFO Threshold“ |
| `FIFOTHR`           | `0x07`| RX‑ und TX‑FIFO‑Schwellwerte |
| `SYNC1`/`SYNC0`     | `0x54`/`0x3D`| Synchronisationswort für WMBus‑Link B (0x3D54) |
| `PKTLEN`            | `0xFF`| Maximale Paketlänge (255 Byte) |
| `PKTCTRL1`          | `0x00`| Kein Address‑Check, normaler FIFO-Modus |
| `PKTCTRL0`          | `0x00`| Fixed/variable length durch Software umgeschaltet |
| `FSCTRL1`           | `0x08`| Frequenzsynthesizer‑Regelung |
| `FSCTRL0`           | `0x00`| Frequenzabstimmung |
| `FREQ2`/`FREQ1`/`FREQ0`| `0x21`/`0x65`/`0x6A`| 868,95 MHz Trägerfrequenz |
| `MDMCFG4`           | `0x5C`| Datenrate ≈ 100 kBaud und Filterbandbreite 203 kHz |
| `MDMCFG3`           | `0x0F`| Datenrate‑Teilung |
| `MDMCFG2`           | `0x05`| 2‑FSK, keine Datenverarbeitung (NRZ) |
| `MDMCFG1`           | `0x22`| ADR_DEVIATION, etc. |
| `MDMCFG0`           | `0xF8`| Channel spacing 200 kHz |
| `DEVIATN`           | `0x50`| Frequenzabweichung ≈ 50 kHz |
| Weitere Register    | …     | AGC‑, TEST‑ und Kalibrier‑Register werden entsprechend gesetzt. Die vollständige Liste der Registerwerte für den T‑Modus befindet sich in der Datei `wmbus_t_cc1101_config.h` des Projekts „izar‑wmbus‑esp“[^izar_register]. |

Die Funktion schreibt die Werte nacheinander in die CC1101‑Register und führt anschließend ein Reset sowie eine Kalibrierung durch. Die Werte entsprechen genau den Registereinstellungen für WMBus‑Mode T (Link B) aus der TI‑Applikationsnotiz.

### 3‑out‑of‑6‑Decoding und CRC

Die WMBus‑T‑Frames sind 3‑out‑of‑6 codiert. In der Datei `lib/wmbus/3outof6.cpp` ist eine Funktion `decode3outof6()` implementiert, die mithilfe eines Lookup‑Arrays 6‑Bit‑Symbole in 4‑Bit‑Nibbles zurückwandelt. Das Programm liest das kodierte Byte‑Array aus dem RX‑FIFO, decodiert es und entfernt anschließend die CRC. Für die CRC wird ein 16‑Bit‑Polynom verwendet.

### Bewertung

Dieses Projekt zeigt eine funktionsfähige Implementierung des Wireless‑M‑Bus‑Empfangs mit einem CC1101‑Modul. Besonders nützlich ist die vollständige Registertabelle für den CC1101 im T‑Mode sowie die Software‑Umsetzung des 3‑out‑of‑6‑Decodings. Die Verwendung des Fixed‑Length‑Modus für kurze Pakete und des Infinite‑Length‑Modus für lange Pakete verdeutlicht, dass der CC1101‑Paket‑Motor nicht die gesamte Rahmendekodierung übernehmen kann. Als Grundlage für einen Leitfaden eignet sich dieses Projekt, da sowohl Hardware‑Anschluss (ESP32 mit CC1101) als auch die notwendigen Software‑Bausteine für die Initialisierung, das 3‑out‑of‑6‑Decoding und das Handling von WMBus‑Rahmen bereitgestellt werden.

[^esp32_length_switch]: In `src/main.cpp` des Projekts „esp32_cc1101_wmbus“ wird beim ersten Empfangs‑Interrupt die Paketlänge decodiert. Die Software entscheidet je nach Länge (<256 Byte vs. ≥256 Byte), ob der CC1101 im Fixed‑Length‑ oder Infinite‑Length‑Modus betrieben wird. Der relevante Codeabschnitt ist in Zeilen 30–62 auf GitHub zu finden: https://github.com/alex-icesoft/esp32_cc1101_wmbus/blob/main/src/main.cpp#L30-L62

[^izar_register]: Siehe Analyse zum Projekt „izar‑wmbus‑esp“: Die Datei `wmbus_t_cc1101_config.h` listet alle Registerwerte für den CC1101 im T‑Modus. Link: https://github.com/maciekn/izar-wmbus-esp/blob/master/wmbus_t_cc1101_config.h