# wM-Bus Gateway Implementation Plan

## System Architecture

### Hardware Components
- ESP32-C3 (main processor)
- CC1101 RF transceiver
- SPI interface for CC1101 communication
- GPIOs for RF control signals

### Software Components
1. ESP-IDF Project Structure
2. CC1101 Driver
3. wM-Bus Protocol Handler
4. 3-of-6 Decoder
5. Whitelist Manager
6. WiFi/Backend Connector
7. Configuration Manager
8. Web/HTTP Server for AP interface

## Implementation Steps

### Phase 1: Basic RF Reception
1. Set up ESP-IDF project for ESP32-C3
2. Implement CC1101 SPI driver
3. Configure CC1101 for wM-Bus frequencies (868MHz)
4. Implement basic packet reception
5. Add raw packet logging

### Phase 2: wM-Bus Protocol Handling
1. Implement 3-of-6 decoding algorithm
2. Validate frame structure
3. Extract device address from frames
4. Handle encrypted payloads (CRC may fail due to encryption)

### Phase 3: Filtering and Forwarding
1. Implement address whitelist
2. Create secure credential storage
3. Implement backend communication (HTTP/MQTT)
4. Forward only whitelisted device data

### Phase 4: Configuration Interface
1. Set up WiFi access point and station modes
2. Create web configuration interface
3. Implement settings storage (SSID, password, whitelist)
4. Add real-time frame monitoring dashboard

## Code Structure

```
project_root/
├── main/
│   ├── app_main.c                 # Main application entry point
│   ├── cc1101/
│   │   ├── cc1101_driver.c        # CC1101 SPI communication
│   │   ├── cc1101_config.c        # CC1101 configuration
│   │   └── cc1101.h               # CC1101 header
│   ├── wmbus/
│   │   ├── wmbus_protocol.c       # wM-Bus frame handling
│   │   ├── three_of_six.c         # 3-of-6 decoding
│   │   └── wmbus.h                # wM-Bus header
│   ├── forwarder/
│   │   ├── forwarder.c            # Backend communication
│   │   └── forwarder.h            # Forwarder header
│   ├── whitelist/
│   │   ├── whitelist_manager.c    # Whitelist operations
│   │   └── whitelist.h            # Whitelist header
│   ├── config/
│   │   ├── config_manager.c       # Configuration handling
│   │   └── config.h               # Configuration header
│   └── nvs_manager/
│       ├── nvs_manager.c          # Centralized NVS management
│       ├── nvs_manager.h          # NVS management API
│       └── CMakeLists.txt         # Component build file
├── components/
├── CMakeLists.txt
└── partitions.csv
```

## NVS Management Architecture

### Problem
Multiple modules were initializing NVS independently, causing handle errors and conflicts.

### Solution
Implemented centralized NVS manager with thread-safe operations and comprehensive error handling.

### Key Changes
1. **Single Initialization Point**: NVS initialization now happens only in app_main()
2. **Safe API Functions**: All modules use safe wrapper functions for NVS operations
3. **Thread Safety**: Mutex-protected access to NVS operations
4. **Error Handling**: Comprehensive error checking and recovery
5. **Monitoring**: Statistics and health check functions

## Key Functions (Descriptive Names)

### CC1101 Driver
- `initialize_cc1101()` - Initialize SPI and configure CC1101
- `configure_rf_parameters()` - Set frequency, data rate, modulation
- `receive_packet()` - Receive raw packet from CC1101
- `set_cc1101_mode()` - Switch between receive/transmit modes

### wM-Bus Handler
- `decode_three_of_six()` - Decode 3-of-6 encoded data
- `extract_device_address()` - Extract address from wM-Bus frame
- `validate_frame_structure()` - Check frame format
- `parse_wmbus_frame()` - Parse complete frame

### Forwarder
- `forward_to_backend()` - Send data to backend server
- `check_device_whitelist()` - Verify device is allowed to forward
- `package_payload_for_forwarding()` - Prepare encrypted payload

### Configuration Manager
- `start_wifi_ap_mode()` - Start configuration access point
- `save_wifi_credentials()` - Store WiFi credentials securely
- `update_device_whitelist()` - Modify whitelisted devices
- `get_radio_settings()` - Get current radio configuration

## Data Flow
```
RF Reception -> Packet Validation -> 3-of-6 Decoding -> Address Extraction 
-> Whitelist Check -> Backend Forwarding (if whitelisted)
```

## Error Handling
- Handle CRC failures due to encrypted packets
- Graceful degradation when decryption fails
- Radio configuration validation
- Network connection fallback mechanisms

## Security Considerations
- Secure storage of WiFi credentials
- Encrypted communication with backend
- Proper validation of configuration inputs
- Prevent unauthorized access to device configuration