# wM-Bus Gateway

A wireless M-Bus (wM-Bus) Gateway using ESP32-C3 and CC1101 RF transceiver to intercept, decode, and forward encrypted wM-Bus packets from utility meters.

## Documentation Structure

### User Documentation (docs/)
- [Getting_Started.md](docs/Getting_Started.md) - First-time setup and basic usage
- [User_Configuration_Guide.md](docs/User_Configuration_Guide.md) - Detailed configuration instructions
- [User_Interface_Guide.md](docs/User_Interface_Guide.md) - Web interface usage guide

### Technical Documentation & Progress (Progress/)
- [Technical_Implementation_Plan.md](Progress/Technical_Implementation_Plan.md) - System architecture and implementation plan
- [Hardware_and_Technical_Details.md](Progress/Hardware_and_Technical_Details.md) - Wiring, signal flow, and technical implementation
- [Project_Status.md](Progress/Project_Status.md) - Current implementation status
- [Main_Goals.md](Progress/Main_Goals.md) - Original project goals and objectives
- [Implementation_Checklist.md](Progress/Implementation_Checklist.md) - Progress tracking checklist

### Research Notes (Research/)
- [Leitfaden.md](Research/Leitfaden.md) - Detailed CC1101/wM-Bus implementation guide (register sets, FIFO handling, T-mode specifics)
- [CC1101_Example_Research.md](Research/CC1101_Example_Research.md) - Expected CC1101 example structure and missing artefacts
- [wM-Bus_Example_Research.md](Research/wM-Bus_Example_Research.md) - Protocol behaviour, sample frames, decoding expectations
- `Analyse/` and other subfolders capture raw notes, datasheets, and vendor references used during development.

### Development Log (Dev-Log/)
A growing set of engineering journals describing issues, fixes, and decisions (e.g. `cc1101_leitfaden_adaptation.md`, `web_ui_improvement_plan.md`). Each entry documents what was changed, why, and any outstanding risks—useful when tracing regressions or revisiting design choices.

## Features

- **RF Reception**: Capture wM-Bus packets via CC1101 transceiver on 868.95 MHz
- **3-of-6 Decoding**: Implement 3-of-6 decoding algorithm for frame validation
- **Address Whitelist**: Only forward packets from whitelisted addresses
- **Backend Communication**: Forward encrypted payloads to configurable backend
- **WiFi Configuration**: Access point-based initial setup with web interface
- **Real-time Monitoring**: Live frame monitoring and statistics dashboard

## Hardware Requirements

- ESP32-C3 development board
- CC1101 RF transceiver module
- Appropriate antenna for 868 MHz
- 5V/2A power supply

## Building and Flashing

This is an ESP-IDF project. To build and flash:

```bash
# Install ESP-IDF (v4.4 or later)
# Set up your development environment

# Clone the repository
git clone <repository-url>
cd oms-hub

# Configure the project
idf.py menuconfig

# Build the project
idf.py build

# Flash to device
idf.py flash
```

## License

[Specify your license here]
