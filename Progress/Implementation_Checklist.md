# Implementation Checklist for wM-Bus Gateway

This checklist ensures implementation follows the specifications in:
- wM_Bus_Gateway_Implementation_Plan.md
- Web_UI_Components.md

## Phase 1: Basic RF Reception
- [x] Set up ESP-IDF project for ESP32-C3
- [x] Implement CC1101 SPI driver
  - [x] `initialize_cc1101()` - Initialize SPI and configure CC1101
  - [x] `configure_rf_parameters()` - Set frequency, data rate, modulation
  - [x] `receive_packet()` - Receive raw packet from CC1101
  - [x] `set_cc1101_mode()` - Switch between receive/transmit modes
- [x] Configure CC1101 for wM-Bus frequencies (868MHz)
- [x] Implement basic packet reception
- [x] Add raw packet logging

## Phase 2: wM-Bus Protocol Handling
- [x] Implement 3-of-6 decoding algorithm
  - [x] `decode_three_of_six()` - Decode 3-of-6 encoded data
- [x] Validate frame structure
  - [x] `validate_frame_structure()` - Check frame format
  - [x] `parse_wmbus_frame()` - Parse complete frame
- [x] Extract device address from frames
  - [x] `extract_device_address()` - Extract address from wM-Bus frame
- [x] Handle encrypted payloads (CRC may fail due to encryption)

## Phase 3: Filtering and Forwarding
- [x] Implement address whitelist
  - [x] `check_device_whitelist()` - Verify device is allowed to forward
  - [x] `update_device_whitelist()` - Modify whitelisted devices
- [x] Create secure credential storage
- [x] Implement backend communication (HTTP/MQTT)
  - [x] `forward_to_backend()` - Send data to backend server
  - [x] `package_payload_for_forwarding()` - Prepare encrypted payload
- [x] Forward only whitelisted device data

## Phase 4: Configuration Interface
- [x] Set up WiFi access point and station modes
  - [x] `start_wifi_ap_mode()` - Start configuration access point
  - [x] `save_wifi_credentials()` - Store WiFi credentials securely
- [x] Implement settings storage (SSID, password, whitelist)
  - [x] `get_radio_settings()` - Get current radio configuration
- [x] Create web configuration interface following Web_UI_Components.md:
  - [x] Main Dashboard (device_detection_list, connection_status_indicator, packet_statistics)
  - [x] WiFi Configuration (wifi_ssid_input, wifi_password_input, wifi_connect_button)
  - [x] Whitelist Management (whitelist_table, add_device_input, add_device_button, remove_device_button)
  - [x] Radio Noise Filtering (signal_strength_threshold, sync_quality_threshold, agc_settings)
  - [x] Backend Settings (backend_server_input, backend_connect_button)

## Code Structure Implementation
- [x] Create code structure as specified in wM_Bus_Gateway_Implementation_Plan.md
  - [x] main/app_main.c
  - [x] main/cc1101/cc1101_driver.c
  - [x] main/cc1101/cc1101_config.c
  - [x] main/cc1101/cc1101.h
  - [x] main/wmbus/wmbus_protocol.c
  - [x] main/wmbus/three_of_six.c
  - [x] main/wmbus/wmbus.h
  - [x] main/forwarder/forwarder.c
  - [x] main/forwarder/forwarder.h
  - [x] main/whitelist/whitelist_manager.c
  - [x] main/whitelist/whitelist.h
  - [x] main/config/config_manager.c
  - [x] main/config/config.h
  - [x] main/config/http_handlers.c
  - [x] main/config/web_ui.c
  - [x] main/cc1101/CMakeLists.txt
  - [x] main/wmbus/CMakeLists.txt
  - [x] main/forwarder/CMakeLists.txt
  - [x] main/whitelist/CMakeLists.txt
  - [x] main/config/CMakeLists.txt
  - [x] CMakeLists.txt
  - [x] partitions.csv

## Data Flow Validation
- [x] Ensure data flow follows: RF Reception -> Packet Validation -> 3-of-6 Decoding -> Address Extraction -> Whitelist Check -> Backend Forwarding

## Error Handling
- [x] Handle CRC failures due to encrypted packets
- [x] Implement graceful degradation when decryption fails
- [x] Validate radio configuration
- [x] Implement network connection fallback mechanisms

## Security Considerations
- [x] Secure storage of WiFi credentials
- [x] Encrypted communication with backend
- [x] Validate configuration inputs
- [x] Prevent unauthorized access to device configuration