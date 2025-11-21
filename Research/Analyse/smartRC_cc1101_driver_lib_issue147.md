# Analyse der Diskussion „Receive long(er) telegrams e.g. wMBUS“ (Issue #147) im Projekt SmartRC‑CC1101‑Driver‑Lib

## Hintergrund

Im GitHub‑Projekt **LSatan/SmartRC‑CC1101‑Driver‑Lib** wurde in Issue #147 die Frage gestellt, wie sich längere Wireless‑M‑Bus‑Telegramme (T‑Modus) mit dem CC1101 empfangen lassen. In der Diskussion veröffentlichte ein Nutzer eine komplette Registerkonfiguration für den CC1101 sowie einen Beispielcode zum Auslesen des RX‑FIFOs. Obwohl das Issue kein eigenständiges Projekt beschreibt, liefert es wertvolle Hinweise für die Konfiguration der Elechouse‑Bibliothek.

### Registerkonfiguration

Die bereitgestellte Konfiguration setzt das Synchronwort auf `0x3D 54`, wählt 2‑FSK mit 100 kBaud und NRZ‑Codierung und stellt die Kanalbandbreite auf 200 kHz ein. Außerdem werden die Frequenzregister auf 868,95 MHz gesetzt und die Abweichung (deviation) auf 50 kHz. AGC‑ und Test‑Register entsprechen weitgehend den Vorgaben aus der TI‑App‑Note. Wichtig ist, dass `PKTCTRL0` und `PKTCTRL1` auf 0 gesetzt werden, sodass die Paketlänge vom Mikrocontroller gesteuert wird[^smartrc_config].

### Beispielcode

Im Issue wird ein Beispielcode vorgestellt, der die Register aus einem Array (`WMBUS_T_CC1101_CONFIG_BYTES`) in den CC1101 schreibt. Anschließend wird der Synthesizer kalibriert (`SCAL`) und der Empfänger gestartet (`SRX`). In einer Schleife wird das RX‑FIFO ausgelesen und der Inhalt über die serielle Schnittstelle ausgegeben. Der Ausschnitt demonstriert, dass der CC1101 selbst keine 3‑out‑of‑6‑Decodierung vornimmt; das erledigt später der Mikrocontroller[^smartrc_code].

```cpp
// vereinfachter Ausschnitt aus Issue #147 (Initialisierung)
ELECHOUSE_cc1101.Init();
for (uint8_t i = 0; i < CONFIG_LEN; i++) {
  ELECHOUSE_cc1101.SpiWriteReg(CONFIG_BYTES[i << 1], CONFIG_BYTES[(i << 1) + 1]);
}
ELECHOUSE_cc1101.SpiStrobe(CC1101_SCAL); // calibrate synthesizer
ELECHOUSE_cc1101.SetRx();                // enter RX mode

while (true) {
  if (ELECHOUSE_cc1101.CheckRxFifo()) {
    uint8_t data = ELECHOUSE_cc1101.SpiReadReg(CC1101_RXFIFO);
    Serial.print(data, HEX);
  }
}
```

### Erkenntnisse

* **RX‑FIFO begrenzt:** Der CC1101 verfügt nur über ein 64‑Byte‑RX‑FIFO. Bei längeren T‑Mode‑Telegrammen muss der FIFO während des Empfangs regelmäßig geleert werden.
* **Deckungsgleiche Registerwerte:** Die Registerwerte stimmen mit den Vorgaben der TI‑App‑Note überein. Dadurch lässt sich das Issue als Referenz für die Konfiguration des CC1101 nutzen.
* **Keine Decodierung im Transceiver:** Der Code zeigt nur das Auslesen der Rohdaten. Die Dekodierung (3‑out‑of‑6, CRC) muss im Mikrocontroller erfolgen.

### Nutzen für den Leitfaden

Die Diskussion liefert eine minimalistische Implementierung zum Empfang von wMBus‑T‑Telegrammen mit der Elechouse‑Bibliothek. Die Registerwerte und das Beispiel zeigen, wie der CC1101 initialisiert und der FIFO ausgelesen werden. Diese Erkenntnisse fließen in den Leitfaden ein, insbesondere bei der Beschreibung der Registerkonfiguration und des FIFO‑Managements.

[^smartrc_config]: LSatan. *SmartRC‑CC1101‑Driver‑Lib Issue #147 – Receive long(er) telegrams e.g. wMBUS.* GitHub‑Issue. Kommentar mit Registerkonfiguration für den wMBus‑T‑Modus. <https://github.com/LSatan/SmartRC-CC1101-Driver-Lib/issues/147> (abgerufen 2025).
[^smartrc_code]: LSatan. *SmartRC‑CC1101‑Driver‑Lib Issue #147 – Receive long(er) telegrams e.g. wMBUS.* GitHub‑Issue. Codebeispiel zur Initialisierung und zum Auslesen des CC1101 im T‑Modus. <https://github.com/LSatan/SmartRC-CC1101-Driver-Lib/issues/147>.