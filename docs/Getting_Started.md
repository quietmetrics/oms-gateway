# Getting Started with wM-Bus Gateway

## Overview
The wM-Bus Gateway is a device that intercepts encrypted wM-Bus packets from utility meters (water, gas, heat) and forwards them to your backend server. The device uses an ESP32-C3 microcontroller and CC1101 RF transceiver to capture signals in the 868 MHz band.

## What's in the Box
- wM-Bus Gateway device
- Power adapter (5V/2A recommended)
- Antenna for CC1101 module
- Quick start guide

## First-Time Setup

### 1. Physical Setup
1. Connect the antenna to the CC1101 module
2. Power on the device
3. Wait for the WiFi access point to appear

### 2. Connect to Configuration AP
1. The device creates an access point named `wM-Bus-Gateway-[unique_id]`
2. Connect your computer/smartphone to this WiFi network
3. The captive portal should automatically open in your browser
4. If not, navigate to `http://192.168.4.1`

### 3. Configure WiFi Connection
1. On the dashboard, go to "WiFi Config"
2. Enter your home WiFi network name (SSID) and password
3. Click "Save WiFi Credentials"
4. The device will attempt to connect to your network
5. The configuration AP will shut down after successful connection

### 4. Find the Device on Your Network
1. Check your router's device list for a device named `wM-Bus-Gateway`
2. Alternatively, the device may broadcast its IP address via serial output
3. You can access the web interface at the device's IP address

## Using the Web Interface

### Dashboard
- Shows connection status (WiFi and backend)
- Displays real-time packet statistics
- Lists detected wM-Bus devices with signal strength

### WiFi Configuration
- Connect to different WiFi networks
- View current connection status
- Test WiFi connectivity

### Whitelist Management
- Add specific device addresses to forward data
- Remove devices from the whitelist
- View all currently whitelisted devices

### Radio Settings
- Adjust sensitivity thresholds
- Configure signal quality filters
- Control automatic gain settings

### Backend Settings
- Configure where to forward captured data
- Set up API endpoints and authentication