# Analyse des ESPHome‑Bausteins „wmbus_radio“

## Hintergrund

In der Community um Home‑Assistant und ESPHome gibt es einen **wmbus_radio**‑Baustein, der Telegramme des Wireless‑M‑Bus mit einem CC1101‑Modul empfängt und an Home‑Assistant weiterleitet. Das Projekt befindet sich im Repository „esphome‑components“ von Szczepan Leon. Es implementiert die gesamte Funk‑ und Protokollschicht für T‑ und C‑Modus in C++ und nutzt FreeRTOS‑Queues sowie ESPHome‑Callbacks. Obwohl es kein komplettes Gateway wie die anderen Projekte darstellt, bietet dieser Baustein wertvolle Einblicke in die Architektur einer stromsparenden Implementierung.

### Architektur

* **Radio‑Klasse (`component.cpp`)**: Der zentrale Baustein `Radio` initialisiert den Transceiver, erstellt eine FreeRTOS‑Queue und setzt einen Interrupt‑Handler für den GDO‑Pin des CC1101. Wenn der Transceiver ein Byte empfängt, wird dieses in die Queue gelegt. Die Daten werden in einem Hintergrund‑Thread verarbeitet; für jedes vollständige wMBus‑Frame wird ein `Frame`‑Objekt erstellt und an ein Callback übergeben[^esphome_radio_component].

* **Transceiver‑Klasse (`transceiver.cpp`/`.h`)**: Diese Klasse kapselt den Zugriff auf den SPI‑Transceiver. Sie stellt Funktionen zum Zurücksetzen, Senden von Befehlsstrobes, Lesen und Schreiben von Registern, Auslesen des RX‑FIFOs und Abfragen von Status‑Bytes bereit[^esphome_transceiver]. Auf diese Weise lassen sich unterschiedliche Funkchips (z. B. CC1101 oder CC1120) einheitlich ansprechen.

* **3‑out‑of‑6‑Decoding (`decode3of6.cpp`)**: Für den T‑Modus implementiert das Projekt ein 3‑out‑of‑6‑Decoding. Eine statische Lookup‑Tabelle mit 64 Einträgen ordnet jedem 6‑Bit‑Code ein 4‑Bit‑Nibble zu. Die Dekodierfunktion zerlegt den Bitstrom in 6‑Bit‑Segmente, sucht die entsprechenden Nibbles in der Tabelle und fängt fehlerhafte Codes ab[^esphome_decode3of6].

* **Packet‑ und Frame‑Klassen (`packet.cpp`)**: Die Klasse `Packet` analysiert Daten aus dem RX‑FIFO, erkennt anhand der Präambel den Codierungsmodus (T‑Modus verwendet 0x54/0x3D) und bestimmt die Länge des Telegramms. Danach führt sie 3‑out‑of‑6‑Decoding durch, entfernt die CRC und erstellt ein `Frame`‑Objekt mit Feldern wie Hersteller, Adresse, CI‑Code und Nutzdaten[^esphome_packet]. Dieses Objekt wird an den Radio‑Handler übergeben.

### Besondere Merkmale

* **Automatische Moduserkennung:** Der Parser erkennt anhand der Präambel, ob das Telegramm im T‑ oder C‑Modus codiert ist, und wählt die entsprechende Decodiermethode.
* **Interrupt‑gesteuertes FIFO‑Lesen:** Der Transceiver liest immer nur das aktuell verfügbare Byte und legt es in die Queue. Dadurch werden FIFO‑Überläufe vermieden und die ISR vom Decoding entkoppelt.
* **Integration in ESPHome:** Die Komponente lässt sich in einer ESPHome‑Konfiguration einbinden. Anwender definieren Zähler‑IDs und Schlüssel in YAML; der Decoder erzeugt dann Sensorwerte (z. B. Messwert, RSSI, Link‑Quality) für Home‑Assistant.

### Codeausschnitt: 3‑out‑of‑6‑Decoding

Der folgende C++‑Ausschnitt stammt aus der Datei [`components/wmbus_radio/decode3of6.cpp`](https://github.com/SzczepanLeon/esphome-components/blob/main/components/wmbus_radio/decode3of6.cpp). Er zeigt, wie das 3‑out‑of‑6‑Decoding realisiert ist. Eine Lookup‑Tabelle ordnet jedem 6‑Bit‑Symbol ein 4‑Bit‑Nibble zu. Anschließend werden die kodierten Daten in 6‑Bit‑Blöcke zerlegt und mithilfe der Tabelle dekodiert. Fehlerhafte Codes werden abgefangen[^esphome_decode3of6].

```cpp
// Ausschnitt aus decode3of6.cpp (Zeilen 13–47)
static const std::map<uint8_t, uint8_t> lookupTable = {
    {0b010110, 0x0}, {0b001101, 0x1}, {0b001110, 0x2}, {0b001011, 0x3},
    {0b011100, 0x4}, {0b011001, 0x5}, {0b011010, 0x6}, {0b010011, 0x7},
    {0b101100, 0x8}, {0b100101, 0x9}, {0b100110, 0xA}, {0b100011, 0xB},
    {0b110100, 0xC}, {0b110001, 0xD}, {0b110010, 0xE}, {0b101001, 0xF},
};

std::optional<std::vector<uint8_t>> decode3of6(std::vector<uint8_t> &coded_data) {
  std::vector<uint8_t> decodedBytes;
  auto segments = coded_data.size() * 8 / 6;
  auto data = coded_data.data();

  for (size_t i = 0; i < segments; i++) {
    auto bit_idx = i * 6;
    auto byte_idx = bit_idx / 8;
    auto bit_offset = bit_idx % 8;

    uint8_t code = (data[byte_idx] << bit_offset);
    if (bit_offset > 0)
      code |= (data[byte_idx + 1] >> (8 - bit_offset));
    code >>= 2;

    auto it = lookupTable.find(code);
    if (it == lookupTable.end()) {
      return {};
    }

    if (i % 2 == 0)
      decodedBytes.push_back(it->second << 4);
    else
      decodedBytes.back() |= it->second;
  }
  return decodedBytes;
}
```

### Relevanz für den Leitfaden

Der **wmbus_radio**‑Baustein zeigt, wie man den Empfangsprozess auf einem Mikrocontroller strukturiert, um Ressourcen effizient zu nutzen. Wichtige Erkenntnisse sind:

1. **Entkopplung von ISR und Verarbeitung:** Mittels Queue wird das zeitkritische Lesen aus dem CC1101 getrennt von der rechenintensiven 3‑out‑of‑6‑Decodierung und CRC‑Prüfung.
2. **Moduserkennung:** Mechanismen zur Unterscheidung von T‑ und C‑Modus können direkt im Parser umgesetzt werden.
3. **Integration in Frameworks:** Der Code illustriert, wie der CC1101‑Empfang als Sensor in Home‑Assistant eingebunden wird.

[^esphome_radio_component]: Szczepan Leon. *esphome‑components: wmbus_radio/component.cpp*. GitHub‑Repository, Zeilen 29–72. <https://github.com/SzczepanLeon/esphome-components/blob/main/components/wmbus_radio/component.cpp> (abgerufen 2025).
[^esphome_transceiver]: Szczepan Leon. *esphome‑components: wmbus_radio/transceiver.cpp* und *transceiver.h*. GitHub‑Repository, Zeilen 10–79 bzw. 0–51. <https://github.com/SzczepanLeon/esphome-components/blob/main/components/wmbus_radio/transceiver.cpp>.
[^esphome_decode3of6]: Szczepan Leon. *esphome‑components: wmbus_radio/decode3of6.cpp*. GitHub‑Repository, Zeilen 10–49. <https://github.com/SzczepanLeon/esphome-components/blob/main/components/wmbus_radio/decode3of6.cpp>.
[^esphome_packet]: Szczepan Leon. *esphome‑components: wmbus_radio/packet.cpp*. GitHub‑Repository, Zeilen 20–120. <https://github.com/SzczepanLeon/esphome-components/blob/main/components/wmbus_radio/packet.cpp>.