# Configuration via Access Point

## Overview
The wM-Bus Gateway device will create a WiFi access point on first boot or when needed, allowing users to configure essential settings through a web interface.

## Access Point Behavior
- On boot, if no WiFi credentials are stored or if forced into AP mode, the ESP32-C3 will create an access point
- Default AP name: "wM-Bus-Gateway-[unique_id]" 
- No password by default for initial configuration
- AP will remain active until configuration is saved or timeout occurs (e.g., 10 minutes)

## Configuration Interface Elements

### WiFi Connection Settings
- `wifi_ssid_input` - Text field for WiFi network name
- `wifi_password_input` - Password field for WiFi network
- `wifi_save_button` - Save WiFi credentials and attempt connection
- `wifi_status_indicator` - Show current WiFi connection status

### Device Whitelist Management
- `whitelist_add_form` - Form to add new device addresses to whitelist
- `whitelist_remove_button` - Buttons to remove specific devices from whitelist
- `whitelist_clear_button` - Button to clear entire whitelist
- `whitelist_display_area` - List of currently whitelisted device addresses

### Radio and Filtering Settings
- `frequency_configuration` - Select wM-Bus mode frequencies (868.95 MHz, etc.)
- `sensitivity_control` - Adjust RF sensitivity/detection threshold
- `packet_filtering_options` - Configure packet filtering options
- `crc_validation_toggle` - Enable/disable CRC validation (may fail for encrypted packets)

### Monitoring Interface
- `live_frame_display` - Real-time display of captured wM-Bus frames
- `signal_strength_indicator` - Show signal strength of received frames
- `frame_type_display` - Show type of captured frames (S, T, C mode)
- `capture_statistics` - Statistics about captured frames (count, rate, etc.)

### General Settings
- `device_name_field` - Configurable device name
- `backend_server_input` - Backend server address for data forwarding
- `ntp_server_input` - NTP server for time synchronization
- `reboot_device_button` - Reboot the device
- `reset_configuration_button` - Reset to factory defaults

## Web Interface Implementation
- Lightweight HTML/CSS/JavaScript served from ESP32-C3
- Single-page application to reduce memory usage
- JavaScript-based form validation
- AJAX requests for configuration updates
- WebSocket connection for real-time frame monitoring

## Configuration Storage
- WiFi credentials stored in NVS (Non-Volatile Storage)
- Device whitelist stored in NVS with configurable persistence
- Radio settings stored in NVS
- Settings validation before storage

## Security Considerations
- After initial configuration, the AP should be password-protected or disabled
- Input validation to prevent injection attacks
- HTTPS support if possible with ESP32-C3 limitations
- Rate limiting to prevent brute force on AP password

## User Experience Flow
1. Connect to "wM-Bus-Gateway-[unique_id]" WiFi network
2. Browser automatically redirects to configuration page (captive portal)
3. Fill in WiFi credentials and save
4. Device connects to WiFi and configuration AP shuts down
5. Access configuration later via device IP address when on same network

## Advanced Configuration Options
- Over-the-Air (OTA) update settings
- Logging level configuration
- Frame format selection (different wM-Bus modes)
- Timezone configuration
- Custom backend endpoint settings

## Fallback Mechanisms
- Manual AP activation via physical button press
- Configuration restore from backup settings
- Factory reset option
- Safe mode that forces AP access even with bad configuration