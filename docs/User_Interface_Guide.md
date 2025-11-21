# Web UI Components for wM-Bus Gateway

## Essential UI Components

### Main Dashboard
- `device_detection_list` - Shows devices currently detected with addresses, signal strength, and last seen time
- `connection_status_indicator` - Shows WiFi and backend connection status
- `packet_statistics` - Displays total packets received and forwarded

### WiFi Configuration
- `wifi_ssid_input` - Text field for WiFi network name
- `wifi_password_input` - Password field for WiFi network
- `wifi_connect_button` - Save credentials and connect to network

### Whitelist Management
- `whitelist_table` - Shows currently whitelisted device addresses
- `add_device_input` - Input field to add new device addresses to whitelist
- `add_device_button` - Add device to whitelist
- `remove_device_button` - Remove device from whitelist

### Radio Noise Filtering
- `signal_strength_threshold` - Minimum RSSI to accept frames (reduce noise)
- `sync_quality_threshold` - Minimum sync quality for frame validation
- `agc_settings` - Automatic Gain Control to prevent overload

### Backend Settings
- `backend_server_input` - URL/IP of backend server
- `backend_connect_button` - Test connection to backend