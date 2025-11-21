/*
 * whitelist_manager.c
 * 
 * Whitelist Manager Implementation for wM-Bus Gateway
 * 
 * This module handles device whitelisting for the wM-Bus gateway.
 * It provides functions to check if a device is authorized, update 
 * the whitelist, and retrieve radio settings.
 * 
 * Author: OMS-HUB Team
 * Date: 2025
 */

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "whitelist.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "../nvs_manager/nvs_manager.h"

static const char *TAG = "Whitelist";

// Current whitelist in memory
static whitelist_t current_whitelist = {0};

// Name of the NVS namespace for storing the whitelist
static const char *WHITELIST_NAMESPACE = "whitelist";

// Key for storing the whitelist data in NVS
static const char *WHITELIST_KEY = "devices";

/**
 * @brief Compares two device addresses for equality
 * 
 * @param addr1 First device address
 * @param addr2 Second device address
 * @return true if addresses match, false otherwise
 */
static bool compare_device_addresses(const uint8_t *addr1, const uint8_t *addr2) {
    return memcmp(addr1, addr2, DEVICE_ADDRESS_LENGTH) == 0;
}

/**
 * @brief Finds the index of a device in the whitelist
 * 
 * @param device_address Pointer to the device address to find
 * @return int Index of the device if found, -1 if not found
 */
static int find_device_index(const uint8_t *device_address) {
    for (int i = 0; i < current_whitelist.count; i++) {
        if (current_whitelist.devices[i].active && 
            compare_device_addresses(current_whitelist.devices[i].address, device_address)) {
            return i;
        }
    }
    return -1;
}

bool init_whitelist(void) {
    esp_err_t err;

    // Verify NVS is initialized
    if (!nvs_manager_is_initialized()) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return false;
    }

    nvs_handle_t nvs_handle;
    err = nvs_manager_safe_open(WHITELIST_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGD(TAG, "No existing whitelist found (whitelist namespace doesn't exist), starting with empty list");
            current_whitelist.count = 0;
            return true;
        } else {
            ESP_LOGE(TAG, "Error opening NVS handle for whitelist: %s", esp_err_to_name(err));
            return false;
        }
    }

    // Get the size of the stored data
    size_t required_size = 0;
    err = nvs_get_blob(nvs_handle, WHITELIST_KEY, NULL, &required_size);

    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No stored whitelist found, starting with empty list");
        current_whitelist.count = 0;
        nvs_manager_safe_close(nvs_handle);
        return true;
    }

    // Read the stored whitelist
    if (required_size == sizeof(whitelist_t)) {
        err = nvs_get_blob(nvs_handle, WHITELIST_KEY, &current_whitelist, &required_size);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Loaded whitelist with %d devices", current_whitelist.count);
        } else {
            ESP_LOGE(TAG, "Failed to read whitelist from storage: %s", esp_err_to_name(err));
            current_whitelist.count = 0;
        }
    } else {
        ESP_LOGE(TAG, "Stored whitelist size mismatch: expected %zu, got %zu",
                 sizeof(whitelist_t), required_size);
        current_whitelist.count = 0;
    }

    nvs_manager_safe_close(nvs_handle);
    return err == ESP_OK;
}

bool save_whitelist(void) {
    esp_err_t err;
    
    nvs_handle_t nvs_handle;
    err = nvs_open(WHITELIST_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle for writing: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_blob(nvs_handle, WHITELIST_KEY, &current_whitelist, sizeof(whitelist_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save whitelist to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit whitelist to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Whitelist saved successfully with %d devices", current_whitelist.count);
    return true;
}

bool check_device_whitelist(const uint8_t *device_address) {
    if (device_address == NULL) {
        ESP_LOGE(TAG, "Device address is NULL");
        return false;
    }
    
    int index = find_device_index(device_address);
    bool is_whitelisted = (index != -1);
    
    ESP_LOGD(TAG, "Device %02X%02X%02X%02X%02X%02X%02X%02X %s in whitelist", 
             device_address[0], device_address[1], device_address[2], device_address[3],
             device_address[4], device_address[5], device_address[6], device_address[7],
             is_whitelisted ? "is" : "is not");
    
    return is_whitelisted;
}

bool whitelist_manager_update_device(const uint8_t *device_address, bool add) {
    if (device_address == NULL) {
        ESP_LOGE(TAG, "Device address is NULL");
        return false;
    }
    
    int index = find_device_index(device_address);
    
    if (add) {
        // Adding a device
        if (index != -1) {
            // Device already exists in whitelist
            ESP_LOGI(TAG, "Device already exists in whitelist");
            return true;
        }
        
        // Check if we have space for more devices
        if (current_whitelist.count >= MAX_WHITELIST_DEVICES) {
            ESP_LOGE(TAG, "Whitelist is full, cannot add more devices");
            return false;
        }
        
        // Add the device to the whitelist
        memcpy(current_whitelist.devices[current_whitelist.count].address, device_address, DEVICE_ADDRESS_LENGTH);
        current_whitelist.devices[current_whitelist.count].active = true;
        current_whitelist.count++;
        
        ESP_LOGI(TAG, "Added device to whitelist, now %d devices", current_whitelist.count);
    } else {
        // Removing a device
        if (index == -1) {
            // Device not found in whitelist
            ESP_LOGW(TAG, "Device not found in whitelist, nothing to remove");
            return true; // Not an error, just nothing to do
        }
        
        // Mark the device as inactive
        current_whitelist.devices[index].active = false;
        
        // Compact the array by moving the last active device to the removed position
        for (int i = index; i < current_whitelist.count - 1; i++) {
            if (current_whitelist.devices[i+1].active) {
                current_whitelist.devices[i] = current_whitelist.devices[i+1];
            }
        }
        
        current_whitelist.count--;
        
        ESP_LOGI(TAG, "Removed device from whitelist, now %d devices", current_whitelist.count);
    }
    
    // Save the updated whitelist
    return save_whitelist();
}

uint32_t get_radio_settings(void) {
    if (!nvs_manager_is_initialized()) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return 0;
    }

    // This is a placeholder implementation
    // In a real implementation, this would read from NVS or another configuration source
    // For now, return a default value or a value based on stored configuration

    // Default radio settings could include:
    // - Frequency band (868 MHz for wM-Bus)
    // - Data rate
    // - Power level
    // - Modulation type

    // For now, return a default value
    // In a real implementation, this would come from stored configuration
    uint32_t default_radio_settings = 0x00000001; // Example: 868MHz band

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_manager_safe_open("radio_config", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        uint32_t stored_settings;
        err = nvs_get_u32(nvs_handle, "settings", &stored_settings);
        if (err == ESP_OK) {
            default_radio_settings = stored_settings;
        }
        nvs_manager_safe_close(nvs_handle);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Namespace doesn't exist, which is fine - just use default settings
        ESP_LOGD(TAG, "No stored radio settings found (radio_config namespace doesn't exist), using default");
    } else {
        ESP_LOGW(TAG, "Error accessing radio settings in NVS: %s", esp_err_to_name(err));
    }

    return default_radio_settings;
}

bool get_whitelist(whitelist_t *wl) {
    if (wl == NULL) {
        ESP_LOGE(TAG, "Whitelist pointer is NULL");
        return false;
    }
    
    memcpy(wl, &current_whitelist, sizeof(whitelist_t));
    return true;
}
