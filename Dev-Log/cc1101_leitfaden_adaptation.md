# Anpassung der CC1101-Konfiguration gemäß Leitfaden

## Kontext
- Datum: 2025-??-??
- Quelle: `Research/Leitfaden.md` (T‑Mode-Konfigurationsanleitung für CC1101)

Der Leitfaden beschreibt eine vollständige Registerkonfiguration für wM‑Bus T1 (100 kBaud, 868.95 MHz) inklusive Synchronwort, AGC‑Parametern und MCSM‑Einstellungen. Darüber hinaus wird empfohlen, den Empfängerzustand permanent auf RX zu halten, FIFO‑Überläufe zu vermeiden und eine regelmäßige Rekalibration (SCAL) durchzuführen.

## Codeänderungen
1. **Register-Setup nach Tabelle 1 umgesetzt**
   - `main/cc1101/cc1101_driver.c:205-276`: `configure_rf_parameters()` schreibt nun die in der Tabelle aufgeführten Werte (IOCFGx, FREQx, MDMCFGx, DEVIATN, AGCCTRLx, MCSMx, FSCALx, TESTx). Das zuvor generische Set wurde vollständig ersetzt.
   - Der Frequenzsynthesizer wird nach dem Schreiben aller Register mit `SCAL` kalibriert und hat ein kurzes Delay zur Stabilisierung.
2. **RX-Zustand absichern**
   - Neuer Helper `cc1101_flush_rx_fifo()` (Zeilen 120‑138) setzt SIDLE → SFRX → SRX, um den Empfänger sauber neu zu starten.
   - `receive_packet()` prüft nun konsequent `MARCSTATE`. Ist der Transceiver nicht im RX-State (`0x0D`), wird ein Flush ausgeführt und der Status erneut geprüft (Zeilen ~310‑322). Damit folgen wir Abschnitt 4 des Leitfadens („FIFO-Management“).
3. **SPI-Transfers für FIFO-Burst**
   - Die maximale Transfergröße wurde auf 130 Bytes erhöht, sodass Header + kompletter FIFO in einer Transaktion gelesen werden können.
   - Paketlängen werden jetzt in FIFO‑Chunks gelesen; der Treiber wartet, bis Bytes verfügbar sind, liest bis zu 64 Bytes pro Burst und entsorgt überschüssige Daten, um den FIFO sauber zu halten (vgl. Abschnitt 4 „FIFO-Management“).
4. **Synthese-Kalibrierung**
   - Nach dem Schreiben der Register wird `SCAL` ausgelöst und eine kurze Pause eingelegt, wie im Leitfaden (Abschnitt 3.1) empfohlen.
5. **wM-Bus Verarbeitungspipeline**
   - Neues Modul `wmbus_receiver` integriert 3-of-6-Dekodierung (`three_of_six.c`) und ruft anschließend den bestehenden Parser auf. Damit folgt die Software jetzt Abschnitt 3.3/5 des Leitfadens.
   - `app_main` leitet die empfangenen Bytes direkt an `wmbus_receiver_process_encoded()` weiter; erfolgreiche Frames lösen einen Callback aus, der Adresse, Länge und Verschlüsselungsstatus meldet.
6. **Warnungen & RX-Stabilität**
   - Die `deliver_frame`-Routine vergleicht nun mit einem `size_t`, um GCCs „comparison is always false“-Warnung zu vermeiden.
   - `receive_packet()` prüft pro Loop erneut `MARCSTATE`, flush’t den FIFO (SIDLE→SFRX→SRX) und wartet kurz, wenn der CC1101 in WOR/XOFF gelandet ist. Falls er nicht zurück in RX kommt, wird ein vollständiger Re-Init (`SRES` + Register-Setup + SRX) durchgeführt, um den Leitfaden-Schritt „Reset und Kalibrierung“ zur Laufzeit zu wiederholen.
7. **L-Feld-Auswertung (Abschnitt 3.3)**
    - Die ersten drei kodierten Bytes werden gelesen, 3-of-6 dekodiert und auf die doppelte L-Angabe geprüft. Daraus berechnen wir die erwartete dekodierte Länge und lesen nur noch so viele codierte Bytes vom CC1101.
    - Der CC1101 läuft dauerhaft im „Infinite Length“-Stil; das Ende des Rahmens wird jetzt softwareseitig bestimmt. Ungültige 3-of-6-Sequenzen führen sofort zum Abbruch und Flush, analog zur Leitfaden-Anforderung, nur echte wM-Bus-Telegramme weiterzuverarbeiten.
8. **Logging-Politik**
    - Routinemäßige RF-Vorgänge (Re-Init, RX-Recovery, FIFO-Leerläufe, L-Header-Drops) protokollieren wir nur noch auf `ESP_LOGD`, um laute Logs in lauten Funkumgebungen zu vermeiden.
    - Warnungen erscheinen nur noch, wenn der CC1101 auch nach mehreren Recovery-Versuchen nicht in RX gelangt; erfolgreiche Re-Initialisierungen laufen still.
    - Unerwartete Fehler (SPI init, CC1101 Reset/Config, RX-Recovery-Fehlschlag) bleiben auf Fehlerniveau (`ESP_LOGE`), sodass echte Probleme weiterhin sichtbar sind.

9. **SPI-Handshakes & Interrupt-Flow**
    - Die SPI-Schicht zieht CS jetzt manuell, wartet wie im TI-Datenblatt beschrieben auf `MISO=LOW` und kapselt jede Transaktion mit `spi_device_acquire/release`. Damit spiegeln wir das Verhalten der Referenz-Bibliothek und stellen sicher, dass der CC1101 wirklich bereit ist, bevor wir ein Header-Byte senden.
    - GDO0/GDO2 werden wie im Arduino-Beispiel genutzt: ein FreeRTOS-Queue sammelt FIFO- und Paket-Events, `receive_packet()` läuft blockierend auf diese Interrupts und liest den FIFO in kleinen Burst-Blöcken. Die bisherige Polling-Schleife (MARCSTATE/RXBYTES) entfällt, wodurch wir keine Race-Conditions mit WOR/Settling mehr erzeugen.
    - Während der Headerphase halten wir den FIFO-Schwellenwert auf 4 Bytes, schalten nach erfolgreicher L-Feld-Dekodierung auf 32 Bytes und aktivieren den End-of-Packet-Interrupt. Das entspricht exakt dem Flow aus `Example wM Bus Library` und erfüllt die Empfehlungen aus dem TI-Leitfaden/Errata.

## Noch offen / Nächste Schritte
1. **Interrupt-basierter RX-Handler**  
   Leitfaden Abschnitt 4 empfiehlt Sync‑ und FIFO‑Interrupts (GDO2/GDO0). Aktuell wird der FIFO noch im Polling gelesen. Die Integration einer ISR steht weiterhin aus.
2. **3-of-6-Decoding & L-Feld-Handling**  
   Abschnitt 3.3/5 beschreibt das Umschalten zwischen Fixed/Infinite Length sowie die Codierung. Der Decoder muss noch implementiert werden, sobald echte Telegramme empfangen werden.
3. **Dokumentierte Pinbelegung prüfen**  
   Im Leitfaden sind Beispiel-Pins genannt (GDO2→GPIO3, GDO0→GPIO2). Die aktuelle Hardware sollte gegen diese Zuordnung getestet und ggf. in `cc1101.h` angepasst werden.

Damit ist die Registerkonfiguration jetzt deckungsgleich mit dem Leitfaden, und der Treiber hält den Empfänger zuverlässiger im RX-Modus. Weitere Schritte konzentrieren sich auf ISR-basierte Verarbeitung und das Dekodieren der T‑Mode-Rahmen.
