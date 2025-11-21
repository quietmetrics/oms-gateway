# Build Environment Not Sourced
**Date:** 2025-11-19
**Description:** Attempted to build project without sourcing ESP-IDF environment
**Error:** CMake cannot find the ESP toolchain compilers (xtensa-esp32-elf-gcc, etc.)
**Resolution:** The ESP-IDF environment must be sourced before building

**Required Steps to Build:**
1. Navigate to project root: `cd /mnt/d/Nextcloud/02 - Projekte/GitHub/oms-hub`
2. Source ESP-IDF: `. $IDF_PATH/export.sh`
3. Set target: `idf.py set-target esp32c3`
4. Build: `idf.py build`

The error occurred because the ESP toolchain compilers are not in the system PATH until the ESP-IDF environment is sourced.