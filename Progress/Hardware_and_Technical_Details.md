# Hardware Wiring and Connections

## ESP32-C3 to CC1101 Connections

The CC1101 RF transceiver connects to the ESP32-C3 via SPI interface with the following pin assignments:

| ESP32-C3 GPIO | CC1101 Pin | Function |
|---------------|------------|----------|
| GPIO 7        | MOSI       | Master Out Slave In |
| GPIO 6        | MISO       | Master In Slave Out |
| GPIO 5        | SCLK       | Serial Clock |
| GPIO 10       | CSN        | Chip Select (active low) |
| GPIO 4        | GDO0       | Data Input/Output |
| GPIO 8        | GDO2       | Data Input/Output |
| 3.3V          | VCC        | Power Supply |
| GND           | GND        | Ground |

## Power Requirements
- ESP32-C3: 3.3V (500mA recommended)
- CC1101: 3.3V (15.2mA operating, 16.5mA receiving)
- Combined current: ~600mA peak during transmission

## Signal Flow and Data Processing

### Reception Flow
```
Antenna -> CC1101 RF Front-End -> 38.4 kbps GFSK Demodulation -> 
3-of-6 Decoder -> Frame Validation -> Address Extraction -> 
Whitelist Check -> Backend Forwarding
```

### 1. RF Reception
- CC1101 configured for 868.95 MHz (wM-Bus Mode C1)
- Data rate: 38.4 kbps
- Modulation: GFSK
- Sync word: 0x2DD4 (standard wM-Bus)

### 2. 3-of-6 Decoding
The received data is 3-of-6 encoded where each 4-bit nibble is represented by a 6-bit code with exactly 3 ones and 3 zeros:
- 0x0 -> 0x16 (010110)
- 0x1 -> 0x0D (001101)
- 0x2 -> 0x0E (001110)
- 0x3 -> 0x15 (010101)
- And so on...

### 3. Frame Processing
- Frame validation includes length check and CRC verification
- Note: CRC check may fail for encrypted packets (expected behavior)
- Device address extraction from A-field (6 bytes at positions 2-7)

### 4. Filtering and Forwarding
- Whitelist check determines if packet should be forwarded
- Only packets from whitelisted device addresses are forwarded to backend
- Encrypted payloads are forwarded as-is (no decryption)

## Software Architecture

### Main Components
- `app_main.c`: Entry point and main event loop
- `cc1101/`: Hardware abstraction layer for CC1101 communication
- `wmbus/`: Protocol implementation including 3-of-6 decoding
- `whitelist/`: Device address management with NVS storage
- `forwarder/`: Backend communication logic
- `config/`: Web interface and configuration management

### Data Structures
- `cc1101_config_t`: Radio configuration parameters
- `wmbus_frame_t`: Parsed wM-Bus frame data
- `device_entry_t`: Whitelist entry structure

### Memory Usage
- SPIFFS partition not used (all HTML served dynamically)
- NVS partition for configuration storage
- Packet buffers allocated in main application loop

## Configuration Storage

### Non-Volatile Storage (NVS) Usage
- WiFi credentials (ssid, password)
- Device whitelist entries
- Backend server configuration
- Radio parameter preferences

### Configuration Validation
- Input validation on all web interface form submissions
- Range checking for radio parameters
- Format validation for device addresses (16-character hex)

## Troubleshooting

### Common Issues
1. **No packets detected**
   - Check antenna connection
   - Verify radio frequency configuration (868.95 MHz)
   - Adjust signal strength threshold

2. **Poor range**
   - Verify antenna type and quality
   - Ensure proper CC1101 power supply
   - Check for interference sources

3. **Connection issues**
   - Verify WiFi credentials
   - Check backend server accessibility
   - Confirm network firewall settings

### Debugging
- Serial output via USB-UART interface (115200 baud)
- Web interface shows real-time packet statistics
- RSSI and LQI values for signal quality assessment

## Performance Considerations

### Processing Limitations
- ESP32-C3 single-core processor
- Limited RAM for buffering large amounts of data
- SPI communication speed limitations (1 MHz for CC1101)

### Optimizations
- Dynamic HTML generation to save flash space
- Efficient 3-of-6 decoding algorithm
- Selective forwarding to reduce network traffic
- Adaptive filtering to reduce noise