## OMS Hub (ESP32-C3 + CC1101)

Lightweight OMS/W-MBus gateway using ESP-IDF 5.5.x on ESP32-C3 with a CC1101 sub-GHz transceiver. Wi-Fi provides a path to configure the device and forward decoded frames.

### Status
- RF receive path operational; CC1101 SPI/interrupts stable.
- Work-in-progress web UI for Wi-Fi onboarding and packet/whitelist handling.
- Backend pipeline (FastAPI → MQTT + InfluxDB) planned; see dev notes for details.

### Quick build/flash
Run from repo root with ESP-IDF environment sourced:
```sh
idf.py set-target esp32c3    # one-time
idf.py build
idf.py -p /dev/ttyUSB0 flash
idf.py monitor
```
Adjust serial port as needed. Use `idf.py erase-flash` if NVS/config needs resetting.

### Hardware
- ESP32-C3 (SuperMini form factor) + CC1101 868/886 MHz module.
- See `doc/img` for pinout references and assembly photos.

![ESP32-C3 and CC1101](doc/img/ESP32C3_CC1101.jpg)

*ESP32-C3 and CC1101 hardware baseline*

### Documentation
- Development notes, pictures, and open tasks: [doc/readme.md](doc/readme.md)

### Components (main/)
- `app/`:
  - `runtime.*`: init + RX loop wiring, registers packet sinks.
  - `services.*`: Facade for persisted config (backend URL, Wi-Fi creds/hostname, radio CS/sync, whitelist).
  - `net/`: Wi-Fi helpers (STA/AP + hostname) and backend URL + JSON forwarder.
  - `wmbus/`: packet router (observer) and NVS whitelist CRUD.
  - `radio/`: persisted CS threshold + sync mode presets for CC1101.
  - `storage.*`: small NVS helpers.
- `radio/`: CC1101 HAL, register presets, and RX pipeline glue to bring raw sub-GHz frames into the system.
- `wmbus/`: OMS/W-MBus framing (3-of-6 encoding, CRC16), packet parsing, and pipeline utilities to prepare frames for higher layers.

### Packet Handling Flow (T-mode)
RX path (CC1101 → decoded packet):
- CC1101 strips preamble/sync and exposes 3-of-6 coded bytes in its RX FIFO.
- `wmbus_pipeline_receive` (`main/wmbus/pipeline.c`) starts in infinite-length mode, reads the first 3 coded bytes, decodes them with `wmbus_decode_3of6`, and extracts the L-field.
- With `wmbus_packet_size`, it derives how many on-air bytes (incl. interleaved CRC16 blocks) to expect, switches to fixed-length when possible, and drains the FIFO until the expected encoded length is reached.
- `wmbus_decode_rx_bytes_tmode` decodes the 3-of-6 stream back to raw bytes and checks each CRC16 block; any coding/CRC error sets `res->status` to `WMBUS_PKT_CODING_ERROR`/`WMBUS_PKT_CRC_ERROR`.
- If you provide `res->rx_logical` in `wmbus_rx_result_t`, the pipeline strips CRCs and fills `res->frame_info` (header + payload length) for direct use in logs/UI; `res->rx_packet` still carries the on-air bytes with CRCs for diagnostics if needed.

TX path (app → CC1101):
- Prepare a header with `wmbus_build_default_header` or fill `WmbusFrameHeaderRaw` manually, then call `wmbus_encode_tx_packet_with_header` to assemble L/C/M/ID/version/device-type/CI, payload, and CRC blocks.
- `wmbus_encode_tx_bytes_tmode` converts the raw packet into 3-of-6 coded bytes for transmission; `wmbus_byte_size_tmode` gives the encoded length for buffer sizing.
