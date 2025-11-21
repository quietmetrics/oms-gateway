# wM-Bus Gateway Goals

## Primary Objective
Build a wireless Modbus (wM-Bus) Gateway using ESP32-C3 and CC1101 RF transceiver to intercept, decode, and forward encrypted wM-Bus packets.

## Core Features
1. RF Reception: Capture wM-Bus packets via CC1101 transceiver
2. 3-of-6 Decoding: Implement 3-of-6 decoding algorithm for frame validation
3. Address Whitelist: Only forward packets from whitelisted addresses
4. Backend Communication: Forward decrypted/encrypted payloads to backend
5. WiFi Configuration: Access point-based initial setup
6. Packet Monitoring: Real-time frame monitoring interface

## Technical Goals
- ESP-IDF based implementation for ESP32-C3
- Clean, readable code with clear function/variable naming
- Proper error handling and CRC validation
- Configurable radio parameters
- Secure credential storage

## Milestones
1. Hardware interface setup (CC1101 + ESP32-C3)
2. wM-Bus packet reception and validation
3. 3-of-6 decoding implementation
4. Whitelist filtering
5. WiFi connectivity and backend forwarding
6. Access point configuration interface
7. Monitoring and diagnostics