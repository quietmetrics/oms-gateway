# Analyse des GitHub‑Projekts „esp‑multical21“

## Ziel des Projekts

Das Repository **chester4444/esp‑multical21** implementiert einen Empfänger für Multical‑21‑Wasserzähler, die im Wireless‑M‑Bus‑Modus C1 telegramme aussenden. Es nutzt eine ESP8266‑/ESP32‑Plattform und einen CC1101‑Transceiver. Neben dem Empfang erfolgt die Entschlüsselung der Daten (AES‑128‑CTR). Obwohl der Fokus dieses Projekts nicht der T‑Modus ist, liefert es wertvolle Einblicke in die allgemeine Verwendung des CC1101 für WMBus sowie in die AES‑basierte Verschlüsselung.

### Hardware‑Verdrahtung

Die README‑Datei enthält eine Tabelle, die die Pins des CC1101 mit den Pins des ESP8266 bzw. ESP32 verbindet. GDO2 (Pin 1) geht an D2 bzw. IO36, GDO0 (Pin 2) an D1 bzw. IO39, CS (Pin 3) an D8 bzw. IO5, MOSI/MISO/SCLK entsprechend an D7/D6/D5 bzw. IO23/IO19/IO18. Diese Verkabelung verdeutlicht, wie die Interrupt‑Pins des CC1101 an GPIO‑Pins des Mikrocontrollers angeschlossen werden[^multical_readme].
Die README listet alternative Anschlüsse (z. B. IO32/IO36 für GDO2 und GPIO 4 oder 5 für CS) ebenfalls auf. Diese Details finden sich in den Zeilen 101–115 der README[^multical_readme].

### CC1101‑Registerwerte für Modus C1

Die Datei `WaterMeter.h` definiert ein Array mit standardmäßigen CC1101‑Einstellungen für den WMBus‑Modus C1. Einige wichtige Registerwerte:

| Register            | Wert  | Bedeutung |
|---------------------|------:|-----------|
| `IOCFG2`            | `0x06`| GDO2 Interrupt für Sync‑Signal |
| `IOCFG1`            | `0x2E`| GDO1 High‑Impedanz (nicht benutzt) |
| `IOCFG0`            | `0x00`| GDO0 Interrupt für „RX FIFO Schwelle erreicht“ |
| `FREQ2`/`FREQ1`/`FREQ0`| `0x21`/`0x65`/`0x6B`| Trägerfrequenz 868,95 MHz (leicht verschoben) |
| `MDMCFG4`           | `0x8C`| Datenrate 50 kBaud (C‑Mode) |
| `MDMCFG3`           | `0x22`| Feinjustierung der Datenrate |
| `MDMCFG2`           | `0x00`| 2‑FSK, NRZ; keine Whitening |
| `DEVIATN`           | `0x50`| Frequenzabweichung 50 kHz |
| `MCSM0`/`MCSM1`     | `0x18`/`0x30`| Steuerung des State‑Machines |
| Weitere Register    | …     | AGC‑ und Kalibrier‑Register sowie die Test‑Register sind ebenfalls definiert. Eine vollständige Liste der Default‑Werte (inklusive AGC‑ und Kalibrier‑Register) findet sich in den Zeilen 184 bis 242 der Datei `WaterMeter.h`[^multical_watermeter]. |

Im Gegensatz zum T‑Mode setzt der C‑Mode keine 3‑out‑of‑6‑Kodierung ein; daher wird der CC1101 im normalen NRZ‑Modus betrieben und die Paket‑Engine kann die Paketlänge verarbeiten. Die Datenrate von 50 kBaud wird über `MDMCFG4`/`MDMCFG3` eingestellt. Da C‑Mode kompatibel mit T‑Mode ist, sind viele Werte identisch (z. B. das Sync‑Wort 0x3D54). 

#### Codeausschnitt aus `WaterMeter.h`

Die folgende Liste stammt aus der Datei [`include/WaterMeter.h`](https://github.com/chester4444/esp-multical21/blob/master/include/WaterMeter.h) des Projekts. Sie zeigt die standardmäßigen Registerwerte, die im Projekt für den CC1101 im C1‑Modus verwendet werden. Die Werte sind in Form von `#define`‑Direktiven angegeben und decken das Synchronisationswort, die Frequenz‑Register, Modem‑Konfigurationen, die Abweichung (deviation) sowie AGC‑ und Test‑Register ab. Durch diesen Codeausschnitt lässt sich leicht nachvollziehen, welche Startparameter für den CC1101 gesetzt werden[^multical_watermeter].

```c
// Ausschnitt aus include/WaterMeter.h (Zeilen 184–242)
#define CC1101_DEFVAL_SYNC1      0x54  // Synchronization word, high byte
#define CC1101_DEFVAL_SYNC0      0x3D  // Synchronization word, low byte
#define CC1101_DEFVAL_MCSM1      0x00  // Main Radio Control State Machine Configuration
#define CC1101_DEFVAL_IOCFG2     0x2E  // GDO2 Output Pin configuration
#define CC1101_DEFVAL_IOCFG0     0x06  // GDO0 Output Pin configuration
#define CC1101_DEFVAL_FSCTRL1    0x08  // Frequency synthesizer control
#define CC1101_DEFVAL_FSCTRL0    0x00  // Frequency synthesizer control
#define CC1101_DEFVAL_FREQ2      0x21  // Frequency control word (high byte)
#define CC1101_DEFVAL_FREQ1      0x6B  // Frequency control word (middle byte)
#define CC1101_DEFVAL_FREQ0      0xD0  // Frequency control word (low byte)
#define CC1101_DEFVAL_MDMCFG4    0x5C  // Modem configuration. Speed ≈103 kbps (C‑Mode)
#define CC1101_DEFVAL_MDMCFG3    0x04  // Modem configuration
#define CC1101_DEFVAL_MDMCFG2    0x06  // Modem configuration (2‑FSK, NRZ)
#define CC1101_DEFVAL_MDMCFG1    0x22  // Modem configuration
#define CC1101_DEFVAL_MDMCFG0    0xF8  // Modem configuration
#define CC1101_DEFVAL_CHANNR     0x00  // Channel number
#define CC1101_DEFVAL_DEVIATN    0x44  // Modem deviation setting
#define CC1101_DEFVAL_FREND1     0xB6  // Front end RX configuration
#define CC1101_DEFVAL_FREND0     0x10  // Front end TX configuration
#define CC1101_DEFVAL_MCSM0      0x18  // Main Radio Control State Machine configuration
#define CC1101_DEFVAL_FOCCFG     0x2E  // Frequency Offset Compensation configuration
#define CC1101_DEFVAL_BSCFG      0xBF  // Bit Synchronization configuration
#define CC1101_DEFVAL_AGCCTRL2   0x43  // AGC control
#define CC1101_DEFVAL_AGCCTRL1   0x09  // AGC control
#define CC1101_DEFVAL_AGCCTRL0   0xB5  // AGC control
#define CC1101_DEFVAL_FSCAL3     0xEA  // Frequency synthesizer calibration
#define CC1101_DEFVAL_FSCAL2     0x2A  // Frequency synthesizer calibration
#define CC1101_DEFVAL_FSCAL1     0x00  // Frequency synthesizer calibration
#define CC1101_DEFVAL_FSCAL0     0x1F  // Frequency synthesizer calibration
#define CC1101_DEFVAL_FSTEST     0x59  // Frequency synthesizer calibration control
#define CC1101_DEFVAL_TEST2      0x81  // Various test settings
#define CC1101_DEFVAL_TEST1      0x35  // Various test settings
#define CC1101_DEFVAL_TEST0      0x09  // Various test settings
#define CC1101_DEFVAL_PKTCTRL1   0x00  // Packet automation control
#define CC1101_DEFVAL_PKTCTRL0   0x02  // Infinite length mode (C‑Mode uses NRZ)
#define CC1101_DEFVAL_ADDR       0x00  // Device address
#define CC1101_DEFVAL_PKTLEN     0x30  // Packet length
#define CC1101_DEFVAL_FIFOTHR    0x00  // RX 4 bytes and TX 61 bytes thresholds
```

### Entschlüsselung

Multical‑21‑Zähler versenden ihre Daten verschlüsselt. Das Projekt verwendet AES‑128 im CTR‑Modus, um die Nutzdaten zu entschlüsseln. In der Datei `WaterMeter.cpp` wird nach dem Empfang jedes Datenblocks über den AES‑Algorithmus iteriert. Das Projekt erklärt, dass das „initial vector“ (IV) und der Zähler im Telegramm enthalten sind und der AES‑Schlüssel im Code hinterlegt sein muss. Dank dieser Implementierung kann man nachvollziehen, wie verschlüsselte WMBus‑Telegramme aufgebaut sind und wie man sie entschlüsselt.

### Bedeutung für den Leitfaden

Für unsere Anleitung zum WMBus‑T‑Modus liefert dieses Projekt einen Vergleich zu Modus C. Es bestätigt, dass der CC1101 als vielseitiger Transceiver genutzt wird und demonstriert eine funktionierende AES‑Entschlüsselung (CTR‑Modus). Der T‑Modus nutzt andere Datenraten und erfordert 3‑out‑of‑6‑Decoding, doch viele Register‑ und Hardware‑Aspekte sind ähnlich. Damit eignet sich das Projekt als Referenz für Pinbelegung, Registerzugriffe und für die Integration einer AES‑128‑Bibliothek zur optionalen Entschlüsselung der Nutzdaten.

[^multical_readme]: Die README im Projekt „esp‑multical21“ enthält eine Tabelle mit der Pinbelegung des CC1101 für ESP8266/ESP32‑Boards (Zeilen 101–115). Link: https://github.com/chester4444/esp-multical21#readme

[^multical_watermeter]: Die Datei `include/WaterMeter.h` im Projekt „esp‑multical21“ definiert alle CC1101‑Standardregister für den C1‑Modus (Zeilen 184–242). Link: https://github.com/chester4444/esp-multical21/blob/master/include/WaterMeter.h