# Build Error - Directory Structure Correction
**Date:** 2025-11-19
**Description:** Found that the ESP-IDF project structure was created in the firmware subdirectory instead of the project root
**Issue:** ESP-IDF projects must have the main CMakeLists.txt and main/ directory in the project root
**Resolution:** Moved the project files from firmware/main/, firmware/CMakeLists.txt, and firmware/partitions.csv to the project root directory

**Corrected Structure:**
- / (project root)
  - CMakeLists.txt (project configuration)
  - partitions.csv (partition table)
  - main/ (all source code)
    - app_main.c
    - cc1101/ (CC1101 driver)
    - wmbus/ (wM-Bus protocol)
    - whitelist/ (whitelist manager)
    - forwarder/ (backend forwarder)
    - config/ (web interface and configuration)

The firmware directory now only contains documentation about building and flashing, while the actual project files are in the correct location for ESP-IDF.