# Repository Structure - wM-Bus Gateway

## Current Structure
After organization, the repository has the following structure:

```
oms-hub/
├── .gitignore              # Git ignore file for ESP-IDF project
├── CMakeLists.txt          # ESP-IDF project configuration
├── partitions.csv          # Flash partition table
├── README.md               # Main project README
├── sdkconfig*              # [Generated during build - in .gitignore]
├── build/*                 # [Generated during build - in .gitignore]
├── docs/                   # Documentation directory
│   ├── README.md           # Documentation index
│   ├── Configuration_Access_Point.md
│   ├── IMPLEMENTATION_CHECKLIST.md
│   ├── PROJECT_STATUS.md
│   ├── Web_UI_Components.md
│   └── wM_Bus_Gateway_Implementation_Plan.md
├── Dev-Log/                # Development log and issue tracking
│   ├── build_error_directory.md
│   ├── directory_structure_correction.md
│   └── repository_cleanup_issue.md
├── Examples/               # Original example directories (empty)
│   ├── CC1101 Example/
│   └── wM-Bus Example/
├── Goals/                  # Project goals documentation
│   └── main_goals.md
├── main/                   # Main source code directory
│   ├── CMakeLists.txt      # Main component CMakeLists
│   ├── app_main.c          # Main application entry point
│   ├── main.h              # Main header file
│   ├── cc1101/             # CC1101 driver component
│   ├── wmbus/              # wM-Bus protocol component
│   ├── whitelist/          # Whitelist manager component
│   ├── forwarder/          # Backend forwarder component
│   └── config/             # Configuration web interface component
├── Research/               # Research documentation
│   ├── CC1101_Example_Research.md
│   └── wM-Bus_Example_Research.md
└── firmware/               # Firmware build outputs and documentation
    └── README.md           # Firmware build and flash instructions
```

## Directory Descriptions

### Source Code (main/)
Contains all the ESP-IDF source code organized by components:
- cc1101: CC1101 RF transceiver driver
- wmbus: wM-Bus protocol handler and 3-of-6 decoder
- whitelist: Device whitelist management
- forwarder: Backend communication module
- config: Web configuration interface

### Documentation (docs/)
All project documentation has been consolidated here for easy access.

### Development Log (Dev-Log/)
Issues encountered during development, including build problems and their resolutions.

### Examples/
Original empty example directories that were mentioned in the project requirements.

### Goals/ and Research/
Additional documentation about project goals and research findings.

### Firmware/
Directory for firmware build outputs and related documentation.

## Build Notes
- Use `idf.py build` from the project root directory
- Use `idf.py flash` to flash to ESP32-C3
- Build artifacts (build/, sdkconfig) are in .gitignore and should not be committed