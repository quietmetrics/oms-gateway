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
- `radio/`: CC1101 HAL, register presets, and RX pipeline glue to bring raw sub-GHz frames into the system.
- `wmbus/`: OMS/W-MBus framing (3-of-6 encoding, CRC16), packet parsing, and pipeline utilities to prepare frames for higher layers.
