/*
 * Configuration Manager Implementation
 * 
 * This file implements the configuration management functionality
 * for the wM-Bus Gateway project, including WiFi setup, credential storage,
 * and device whitelist management.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <esp_http_server.h>
#include <esp_netif.h>
#include "config.h"
#include "cJSON.h"
#include "../nvs_manager/nvs_manager.h"

static const char* TAG = "CONFIG_MANAGER";

// HTTP server configuration
#define HTTP_PORT 80
#define MAX_URI_HANDLERS 20

// Global variables for configuration
static char current_wifi_ssid[64] = {0};
static char current_wifi_password[64] = {0};
static char current_backend_server[256] = {0};
static esp_netif_t* wifi_ap_netif = NULL;
static esp_netif_t* wifi_sta_netif = NULL;
static httpd_handle_t server = NULL;

// External URI handlers from http_handlers.c
extern httpd_uri_t uri_handlers[];
extern int num_uri_handlers;

// Function declarations for internal use
static esp_err_t load_wifi_credentials(void);
static esp_err_t save_wifi_credentials_internal(const char* ssid, const char* password);
static esp_err_t load_backend_settings(void);
static esp_err_t init_wifi(void);

/**
 * @brief Initialize the configuration manager
 *
 * This function initializes the configuration manager, setting up
 * the web server and loading stored configuration values.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t init_config_manager(void)
{
    ESP_LOGI(TAG, "Initializing configuration manager");

    // Verify NVS is initialized
    if (!nvs_manager_is_initialized()) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Load stored WiFi credentials
    esp_err_t ret = load_wifi_credentials();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No stored WiFi credentials found");
    }

    // Load stored backend settings
    ret = load_backend_settings();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No stored backend settings found");
    }

    // Initialize WiFi
    ret = init_wifi();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi");
        return ret;
    }

    ESP_LOGI(TAG, "Configuration manager initialized successfully");
    return ESP_OK;
}

/**
 * @brief Initialize WiFi
 *
 * This function initializes the WiFi driver and creates the AP and STA interfaces.
 *
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t init_wifi(void)
{
    // Initialize the network interface subsystem
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create the default event loop if it doesn't exist yet
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means the default event loop already exists
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize WiFi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure WiFi power save mode
    ret = esp_wifi_set_ps(WIFI_PS_NONE); // Disable power save for better performance
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi power save mode: %s", esp_err_to_name(ret));
    }

    // Create AP and STA network interfaces
    if (wifi_ap_netif == NULL) {
        wifi_ap_netif = esp_netif_create_default_wifi_ap();
        if (wifi_ap_netif == NULL) {
            ESP_LOGE(TAG, "Failed to create default AP netif");
            esp_wifi_deinit(); // Clean up WiFi driver if netif creation fails
            return ESP_FAIL;
        }
    }

    if (wifi_sta_netif == NULL) {
        wifi_sta_netif = esp_netif_create_default_wifi_sta();
        if (wifi_sta_netif == NULL) {
            ESP_LOGE(TAG, "Failed to create default STA netif");
            esp_wifi_deinit(); // Clean up WiFi driver if netif creation fails
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

/**
 * @brief Load stored WiFi credentials from NVS
 *
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t load_wifi_credentials(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_manager_safe_open("storage", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            // Namespace doesn't exist, which means no data has been stored yet
            ESP_LOGD(TAG, "No stored WiFi credentials found (storage namespace doesn't exist)");
            current_wifi_ssid[0] = '\0';
            current_wifi_password[0] = '\0';
            return ESP_OK; // This is not an error condition
        } else {
            ESP_LOGE(TAG, "Error opening NVS handle for WiFi credentials: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    size_t ssid_len = sizeof(current_wifi_ssid);
    ret = nvs_manager_get_string(nvs_handle, NVS_WIFI_SSID, current_wifi_ssid, &ssid_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No stored SSID found: %s", esp_err_to_name(ret));
        // Reset the variable to empty string if no value found
        current_wifi_ssid[0] = '\0';
    } else {
        ESP_LOGI(TAG, "Loaded stored SSID: %s", current_wifi_ssid);
    }

    size_t pass_len = sizeof(current_wifi_password);
    ret = nvs_manager_get_string(nvs_handle, NVS_WIFI_PASS, current_wifi_password, &pass_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No stored password found: %s", esp_err_to_name(ret));
        // Reset the variable to empty string if no value found
        current_wifi_password[0] = '\0';
    } else {
        ESP_LOGI(TAG, "Loaded stored password (length: %d)", (int)pass_len);
    }

    nvs_manager_safe_close(nvs_handle);
    return ESP_OK;
}

/**
 * @brief Save WiFi credentials to NVS
 *
 * @param ssid WiFi network SSID
 * @param password WiFi network password
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t save_wifi_credentials_internal(const char* ssid, const char* password)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_manager_safe_open("storage", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle for WiFi credentials");
        return ret;
    }

    // Save SSID
    ret = nvs_manager_set_string(nvs_handle, NVS_WIFI_SSID, ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error saving SSID to NVS: %s", esp_err_to_name(ret));
        nvs_manager_safe_close(nvs_handle);
        return ret;
    }

    // Save password
    ret = nvs_manager_set_string(nvs_handle, NVS_WIFI_PASS, password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error saving password to NVS: %s", esp_err_to_name(ret));
        nvs_manager_safe_close(nvs_handle);
        return ret;
    }

    nvs_manager_safe_close(nvs_handle);

    // Update global variables
    strncpy(current_wifi_ssid, ssid, sizeof(current_wifi_ssid) - 1);
    strncpy(current_wifi_password, password, sizeof(current_wifi_password) - 1);

    ESP_LOGI(TAG, "WiFi credentials saved successfully");
    return ESP_OK;
}

/**
 * @brief Load backend settings from NVS
 *
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t load_backend_settings(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_manager_safe_open("storage", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            // Namespace doesn't exist, which means no data has been stored yet
            ESP_LOGD(TAG, "No stored backend settings found (storage namespace doesn't exist)");
            current_backend_server[0] = '\0';
            return ESP_OK; // This is not an error condition
        } else {
            ESP_LOGE(TAG, "Error opening NVS handle for backend settings: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    size_t server_len = sizeof(current_backend_server);
    ret = nvs_manager_get_string(nvs_handle, NVS_BACKEND_SERVER, current_backend_server, &server_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No stored backend server found: %s", esp_err_to_name(ret));
        // Reset the variable to empty string if no value found
        current_backend_server[0] = '\0';
    } else {
        ESP_LOGI(TAG, "Loaded stored backend server: %s", current_backend_server);
    }

    nvs_manager_safe_close(nvs_handle);
    return ESP_OK;
}

/**
 * @brief Save backend settings to NVS
 *
 * @param server Backend server URL
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t save_backend_settings(const char* server)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_manager_safe_open("storage", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle for backend settings");
        return ret;
    }

    // Save backend server
    ret = nvs_manager_set_string(nvs_handle, NVS_BACKEND_SERVER, server);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error saving backend server to NVS: %s", esp_err_to_name(ret));
        nvs_manager_safe_close(nvs_handle);
        return ret;
    }

    nvs_manager_safe_close(nvs_handle);

    // Update global variable
    strncpy(current_backend_server, server, sizeof(current_backend_server) - 1);

    ESP_LOGI(TAG, "Backend settings saved successfully");
    return ESP_OK;
}

const char* get_saved_wifi_ssid(void)
{
    return current_wifi_ssid;
}

const char* get_saved_backend_server(void)
{
    return current_backend_server;
}

/**
 * @brief Start WiFi in Access Point mode for configuration
 *
 * This function starts the ESP32 in WiFi Access Point mode to allow
 * users to connect and configure the device through the web interface.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t start_wifi_ap_mode(void)
{
    ESP_LOGI(TAG, "Starting WiFi in AP mode");

    // The netif interfaces should already be created during initialization
    if (wifi_ap_netif == NULL) {
        ESP_LOGE(TAG, "AP netif is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Configure AP
    wifi_config_t ap_config = {
        .ap = {
            .ssid = DEFAULT_AP_SSID,
            .password = DEFAULT_AP_PASSWORD,
            .ssid_len = strlen(DEFAULT_AP_SSID),
            .channel = DEFAULT_AP_CHANNEL,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .ssid_hidden = 0,
            .max_connection = DEFAULT_AP_MAX_CONN,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    // Set password length to 0 if authmode is open
    if (strlen(DEFAULT_AP_PASSWORD) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Set the AP configuration
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode to AP: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set AP configuration: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start WiFi
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi AP mode started successfully. SSID: %s, Password: %s",
             DEFAULT_AP_SSID, DEFAULT_AP_PASSWORD);

    return ESP_OK;
}

/**
 * @brief Save WiFi credentials to secure storage
 * 
 * This function securely stores the provided WiFi credentials
 * for use when connecting to the network in station mode.
 * 
 * @param ssid WiFi network SSID
 * @param password WiFi network password
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t save_wifi_credentials(const char* ssid, const char* password)
{
    if (!ssid || !password) {
        ESP_LOGE(TAG, "Invalid parameters: SSID or password is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(ssid) == 0) {
        ESP_LOGE(TAG, "SSID cannot be empty");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Saving WiFi credentials for SSID: %s", ssid);

    return save_wifi_credentials_internal(ssid, password);
}

/**
 * @brief Update the device whitelist with new addresses
 *
 * This function adds or removes device addresses from the whitelist
 * that determines which wM-Bus devices are allowed to be forwarded.
 *
 * @param device_address Device address to add/remove
 * @param action 0 to remove, 1 to add
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t update_device_whitelist(const char* device_address, int action)
{
    if (!device_address) {
        ESP_LOGE(TAG, "Device address is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_manager_safe_open("whitelist", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle for whitelist: %s", esp_err_to_name(ret));
        return ret;
    }

    // Get current whitelist size
    size_t whitelist_size = 0;
    ret = nvs_get_blob(nvs_handle, NVS_WHITELIST, NULL, &whitelist_size);

    // Allocate memory for current whitelist
    char* current_whitelist = NULL;
    if (whitelist_size > 0) {
        current_whitelist = malloc(whitelist_size);
        if (!current_whitelist) {
            ESP_LOGE(TAG, "Failed to allocate memory for whitelist");
            nvs_manager_safe_close(nvs_handle);
            return ESP_ERR_NO_MEM;
        }

        ret = nvs_get_blob(nvs_handle, NVS_WHITELIST, current_whitelist, &whitelist_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get current whitelist: %s", esp_err_to_name(ret));
            free(current_whitelist);
            nvs_manager_safe_close(nvs_handle);
            return ret;
        }
    }

    // Create new whitelist based on action
    char* new_whitelist = NULL;
    size_t new_size = 0;
    
    if (action == 1) { // Add device
        // Calculate new size (current + new address + separator + null terminator)
        size_t addr_len = strlen(device_address);
        new_size = whitelist_size + addr_len + 2; // +1 for separator, +1 for null
        new_whitelist = malloc(new_size);
        if (!new_whitelist) {
            ESP_LOGE(TAG, "Failed to allocate memory for new whitelist");
            free(current_whitelist);
            nvs_close(nvs_handle);
            return ESP_ERR_NO_MEM;
        }
        
        // Copy current whitelist
        if (current_whitelist && whitelist_size > 0) {
            strcpy(new_whitelist, current_whitelist);
            strcat(new_whitelist, ";"); // Separator
        } else {
            new_whitelist[0] = '\0';
        }
        
        // Add new device address
        strcat(new_whitelist, device_address);
        
    } else if (action == 0) { // Remove device
        if (!current_whitelist || whitelist_size == 0) {
            ESP_LOGW(TAG, "Whitelist is empty, nothing to remove");
            free(current_whitelist);
            nvs_close(nvs_handle);
            return ESP_OK;
        }
        
        // Find and remove the device address
        char* new_list = malloc(whitelist_size); // Potentially smaller, but safe to allocate same size
        if (!new_list) {
            ESP_LOGE(TAG, "Failed to allocate memory for new whitelist");
            free(current_whitelist);
            nvs_close(nvs_handle);
            return ESP_ERR_NO_MEM;
        }
        
        // Parse current whitelist and rebuild without the specified address
        char* token = strtok(current_whitelist, ";");
        new_list[0] = '\0'; // Initialize as empty string
        bool found = false;
        
        while (token != NULL) {
            if (strcmp(token, device_address) != 0) {
                if (strlen(new_list) > 0) {
                    strcat(new_list, ";");
                }
                strcat(new_list, token);
            } else {
                found = true;
            }
            token = strtok(NULL, ";");
        }
        
        if (!found) {
            ESP_LOGW(TAG, "Device address %s not found in whitelist", device_address);
        }
        
        new_whitelist = new_list;
        new_size = strlen(new_whitelist) + 1;
    } else {
        ESP_LOGE(TAG, "Invalid action: %d. Use 0 to remove, 1 to add", action);
        free(current_whitelist);
        nvs_close(nvs_handle);
        return ESP_ERR_INVALID_ARG;
    }

    // Save new whitelist
    ret = nvs_set_blob(nvs_handle, NVS_WHITELIST, new_whitelist, new_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save new whitelist: %s", esp_err_to_name(ret));
        free(current_whitelist);
        free(new_whitelist);
        nvs_manager_safe_close(nvs_handle);
        return ret;
    }

    // Commit changes
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit whitelist changes: %s", esp_err_to_name(ret));
    }

    free(current_whitelist);
    free(new_whitelist);
    nvs_manager_safe_close(nvs_handle);

    if (action == 1) {
        ESP_LOGI(TAG, "Device address %s added to whitelist", device_address);
    } else {
        ESP_LOGI(TAG, "Device address %s removed from whitelist", device_address);
    }

    return ESP_OK;
}

/**
 * @brief Get current radio settings
 *
 * This function retrieves the current radio configuration parameters
 * including frequency, data rate, and other RF settings.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t get_radio_settings(void)
{
    if (!nvs_manager_is_initialized()) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // This is a placeholder implementation - actual implementation would depend on
    // the CC1101 driver and radio configuration system
    ESP_LOGI(TAG, "Getting current radio settings");

    // In a real implementation, this would interact with the CC1101 driver
    // to retrieve current settings like frequency, data rate, etc.
    // For now, we'll just log that the function was called
    ESP_LOGI(TAG, "Radio settings retrieval completed (placeholder implementation)");

    return ESP_OK;
}

/**
 * @brief Start the HTTP server for web interface
 *
 * This function starts the HTTP server that serves the web UI
 * for configuring the wM-Bus gateway.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t start_web_server(void)
{
    ESP_LOGI(TAG, "Starting web server on port %d", HTTP_PORT);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_PORT;
    config.max_uri_handlers = MAX_URI_HANDLERS;

    if (num_uri_handlers > MAX_URI_HANDLERS) {
        ESP_LOGE(TAG, "Configured %d URI handlers exceeds supported limit of %d", num_uri_handlers, MAX_URI_HANDLERS);
        return ESP_ERR_INVALID_ARG;
    }

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    // Register URI handlers
    for (int i = 0; i < num_uri_handlers; i++) {
        esp_err_t ret = httpd_register_uri_handler(server, &uri_handlers[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register URI handler for %s: %s",
                     uri_handlers[i].uri, esp_err_to_name(ret));
            return ret;
        }
    }

    ESP_LOGI(TAG, "Web server started successfully");
    return ESP_OK;
}
