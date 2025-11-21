# Build Error - Incorrect Directory
**Date:** 2025-11-19
**Description:** Attempted to run idf.py build from /firmware directory instead of project root
**Error:** CMake could not find the ESP toolchain compilers
**Resolution:** Build command should be run from project root directory where CMakeLists.txt and main/ directory exist, not from the /firmware documentation directory.

**Correct Build Process:**
1. Navigate to project root: `cd /mnt/d/Nextcloud/02 - Projekte/GitHub/oms-hub`
2. Source ESP-IDF: `. $IDF_PATH/export.sh`
3. Set target: `idf.py set-target esp32c3`
4. Build: `idf.py build`