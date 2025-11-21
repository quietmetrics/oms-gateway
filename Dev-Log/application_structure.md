# wM-Bus Gateway Main Application Structure

## Issue 1: Component Integration
**Date:** 2025-11-19
**Description:** Multiple agents implemented different components, needed to ensure they work together in main application
**Resolution:** All components properly integrated in main/app_main.c with appropriate initialization calls

## Application Flow
The main application in app_main.c follows this sequence:
1. Initialize NVS (Non-Volatile Storage)
2. Initialize CC1101 driver
3. Configure CC1101 for wM-Bus reception
4. Initialize wM-Bus protocol handler
5. Initialize whitelist manager
6. Initialize forwarder
7. Initialize configuration manager (starts web server and WiFi)
8. Start packet reception loop

## Component Integration Points
- CC1101 driver receives raw packets
- wM-Bus handler decodes 3-of-6 and extracts addresses
- Whitelist manager validates device addresses
- Forwarder sends approved data to backend
- Config manager handles WiFi and web interface

## Main Dependencies
The main application depends on all the implemented components:
- CC1101 driver for RF communication
- wM-Bus protocol for frame handling
- Whitelist manager for device filtering
- Forwarder for backend communication
- Config manager for web interface