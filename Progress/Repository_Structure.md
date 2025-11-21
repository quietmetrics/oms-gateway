# Repository Structure - wM-Bus Gateway

## Current Structure
After organization, the repository has the following structure:

```
oms-hub/
├── .gitignore              # Git ignore file for ESP-IDF project
├── CMakeLists.txt          # ESP-IDF project configuration
├── partitions.csv          # Flash partition table
├── README.md               # Main project README
├── PROJECT_BUILD_STATUS.md # Build status tracking
├── sdkconfig*              # [Generated during build - in .gitignore]
├── build/*                 # [Generated during build - in .gitignore]
├── docs/                   # User documentation directory
│   ├── README.md           # User documentation index
│   ├── Getting_Started.md  # First-time setup guide
│   ├── User_Configuration_Guide.md  # Configuration instructions
│   └── User_Interface_Guide.md      # Web interface guide
├── Progress/               # Technical documentation and progress tracking
│   ├── README.md           # Technical documentation index
│   ├── Technical_Implementation_Plan.md     # System architecture
│   ├── Hardware_and_Technical_Details.md    # Wiring and technical details
│   ├── Project_Status.md   # Implementation status
│   ├── Main_Goals.md       # Original project goals
│   └── Implementation_Checklist.md          # Progress checklist
├── Dev-Log/                # Development log and issue tracking
├── Examples/               # Example directories (empty, from requirements)
├── main/                   # Main source code directory
│   ├── CMakeLists.txt      # Main component CMakeLists
│   ├── app_main.c          # Main application entry point
│   ├── cc1101/             # CC1101 driver component
│   ├── wmbus/              # wM-Bus protocol component
│   ├── whitelist/          # Whitelist manager component
│   ├── forwarder/          # Backend forwarder component
│   └── config/             # Configuration web interface component
├── Research/               # Research documentation
└── firmware/               # Firmware build outputs and documentation
```

## Directory Descriptions

### User Documentation (docs/)
Contains all user-focused documentation:
- Getting started guides
- Configuration instructions
- Web interface usage

### Technical Documentation & Progress (Progress/)
Contains developer-focused documentation:
- Technical implementation details
- Hardware wiring and signal flow
- Project goals and status
- Implementation checklist

### Source Code (main/)
Contains all the ESP-IDF source code organized by components:
- cc1101: CC1101 RF transceiver driver
- wmbus: wM-Bus protocol handler and 3-of-6 decoder
- whitelist: Device whitelist management
- forwarder: Backend communication module
- config: Web configuration interface

### Development Log (Dev-Log/)
Issues encountered during development, including build problems and their resolutions.

### Research/
Research documentation about wM-Bus protocol, CC1101, and implementation approaches.

## Build Notes
- Use `idf.py build` from the project root directory
- Use `idf.py flash` to flash to ESP32-C3
- Build artifacts (build/, sdkconfig) are in .gitignore and should not be committed