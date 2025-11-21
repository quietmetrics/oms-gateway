/*
 * whitelist.h
 * 
 * Whitelist Manager for wM-Bus Gateway
 * 
 * This module handles device whitelisting for the wM-Bus gateway.
 * It provides functions to check if a device is authorized, update 
 * the whitelist, and retrieve radio settings.
 * 
 * Author: OMS-HUB Team
 * Date: 2025
 */

#ifndef WHITELIST_H
#define WHITELIST_H

#include <stdbool.h>
#include <stdint.h>

// Maximum number of whitelisted devices
#define MAX_WHITELIST_DEVICES 50

// Device address length for wM-Bus (typically 8 bytes: 4 for ID + 1 for Type + 1 for Version + 2 for Manufacturer)
#define DEVICE_ADDRESS_LENGTH 8

// Structure to hold device information
typedef struct {
    uint8_t address[DEVICE_ADDRESS_LENGTH];  // Device address
    bool active;                             // Whether this entry is active
} device_entry_t;

// Structure to hold all whitelisted devices
typedef struct {
    device_entry_t devices[MAX_WHITELIST_DEVICES];
    int count;  // Number of active devices in the whitelist
} whitelist_t;

/**
 * @brief Checks if a device is in the whitelist
 * 
 * @param device_address Pointer to the device address to check
 * @return true if device is in whitelist, false otherwise
 */
bool check_device_whitelist(const uint8_t *device_address);

/**
 * @brief Updates the whitelist by adding or removing a device
 * 
 * @param device_address Pointer to the device address to update
 * @param add If true, adds the device to whitelist; if false, removes it
 * @return true on success, false on failure
 */
bool whitelist_manager_update_device(const uint8_t *device_address, bool add);

/**
 * @brief Gets the current radio settings
 * 
 * @return uint32_t representing the current radio configuration
 */
uint32_t get_radio_settings(void);

/**
 * @brief Initializes the whitelist from storage
 * 
 * @return true on success, false on failure
 */
bool init_whitelist(void);

/**
 * @brief Saves the current whitelist to storage
 * 
 * @return true on success, false on failure
 */
bool save_whitelist(void);

/**
 * @brief Gets a copy of the current whitelist
 * 
 * @param wl Pointer to a whitelist_t structure to fill with current data
 * @return true on success, false on failure
 */
bool get_whitelist(whitelist_t *wl);

#endif // WHITELIST_H
