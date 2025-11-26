# wM-Bus Gateway Implementation Plan

## System Architecture

### Hardware Components
- ESP32-C3 (main processor)
- CC1101 RF transceiver
- SPI interface for CC1101 communication
- GPIOs for RF control signals

### Software Components
1. ESP-IDF Project Structure
2. CC1101 Driver
3. wM-Bus Protocol Handler
4. 3-of-6 Decoder
5. Whitelist Manager
6. WiFi/Backend Connector
7. Configuration Manager
8. Web/HTTP Server for AP interface

## Implementation Steps

### Phase 1: Basic RF Reception
1. Set up ESP-IDF project for ESP32-C3
2. Implement CC1101 SPI driver from "./Example wM Bus Libary" + "./Research/Leitfaden.md"
3. Configure CC1101 for wM-Bus frequencies also from "./Example wM Bus Libary" + "./Research/Leitfaden.md"
4. Implement basic packet reception from "./Example wM Bus Libary"  + "./Research/Leitfaden.md"

### Phase 2: wM-Bus Protocol Handling
1. Implement 3-of-6 decoding algorithm from "./Example wM Bus Libary" + "./Research/Leitfaden.md"
2. Validate frame structure from "./Example wM Bus Libary" + "./Research/Leitfaden.md"
3. Extract device address (OMS-Adresse, M- + A-Feld) from frames from "./Example wM Bus Libary" + "./Research/Leitfaden.md"
4. Add raw valid packet logging (for the monitor described in Phase 4.4); "valid" = 3-of-6 decode OK + doppeltes L-Feld konsistent + CRC OK

### Phase 3: Filtering and Forwarding
1. Implement address whitelist
   - Format: OMS-Adresse (M- + A-Feld), hex, Endianness wie im Frame
2. Create credential storage (NVS, unverschlüsselt)
3. Implement backend communication (HTTP/HTTPS POST /api/v1/uplink to configured host:port, Content-Type application/json, with TLS cert check)
   - Payload example (ohne Timestamp): `{"gateway_id":"<from hostname>","radio_profile":"OMS-T-mode","rssi_dbm":-78.0,"lqi":92,"crc_ok":true,"device_address":"12345678","wmbus_frame_hex":"A55A6B..."}`
   - RSSI in dBm aus CC1101-Register konvertieren; LQI direkt aus Statusbyte
   - Backend target may be empty initially; when saved, perform immediate connectivity check
4. Forward only whitelisted device data; kein Offline-Puffer, Pakete werden verworfen wenn Backend nicht erreichbar
5. Show the status of the backend (Connected / Disconnected) in the WEB UI (next phase), status based on last transmission attempt

### Phase 4: Configuration Interface (Web UI)
1. Set up WiFi access point and station modes (SSID = Hostname = OMS-Gateway-{CHIP_ID}, CHIP_ID hex, 8 chars if available); start in AP, then STA; fallback to AP if STA connection fails
2. Create web configuration interface to configure:
   1. WIFI settings
   2. change Hostname
   3. Whitelist elements (wM Bus Device addresses)
   4. Backend URL (to send the recives and whitelisted packets to)
3. Implement settings storage (SSID, password, whitelist, backend url, hostname, gateway_id derived from hostname)
4. Add real-time frame monitoring dashboard to show ALL valid wM-Bus packes (show only the last recived packed from each device)
   1. should show all relevant packet infos like rssi, lqi, lenght, addres, manufacturer; optional raw hex; CI not required
   2. add a "add whitelist" button to easily add devices to whitelist
   3. show local timestamp in monitor; no timestamp in backend payload
5. Whitelist UI: table of hex device addresses with add/remove buttons; normalize input as hex string
