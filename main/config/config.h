/*
 * Configuration Manager Header
 *
 * This file contains function declarations for the configuration manager
 * of the wM-Bus Gateway project.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <esp_err.h>
#include <esp_http_server.h>

// Defaults for Access Point mode used during configuration
#define DEFAULT_AP_SSID "wM-Bus-Gateway-Setup"
#define DEFAULT_AP_PASSWORD "123456789"
#define DEFAULT_AP_CHANNEL 1
#define DEFAULT_AP_MAX_CONN 4

// NVS keys used by configuration storage
#define NVS_WIFI_SSID "wifi_ssid"
#define NVS_WIFI_PASS "wifi_pass"
#define NVS_WHITELIST "whitelist"
#define NVS_BACKEND_SERVER "backend_server"

/**
 * @brief Start WiFi in Access Point mode for configuration
 *
 * This function starts the ESP32 in WiFi Access Point mode to allow
 * users to connect and configure the device through the web interface.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t start_wifi_ap_mode(void);

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
esp_err_t save_wifi_credentials(const char* ssid, const char* password);

/**
 * @brief Get the currently saved WiFi SSID (station credentials)
 *
 * @return Pointer to the stored SSID string (read-only)
 */
const char* get_saved_wifi_ssid(void);

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
esp_err_t update_device_whitelist(const char* device_address, int action);

/**
 * @brief Get current radio settings
 *
 * This function retrieves the current radio configuration parameters
 * including frequency, data rate, and other RF settings.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t get_radio_settings(void);

/**
 * @brief Initialize the configuration manager
 *
 * This function initializes the configuration manager, setting up
 * the web server and loading stored configuration values.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t init_config_manager(void);

/**
 * @brief Start the HTTP server for web interface
 *
 * This function starts the HTTP server that serves the web UI
 * for configuring the wM-Bus gateway.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t start_web_server(void);

/**
 * @brief Save backend settings to NVS
 *
 * This function securely stores the backend server configuration
 * for forwarding wM-Bus data.
 *
 * @param server Backend server URL
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t save_backend_settings(const char* server);

/**
 * @brief Get the currently configured backend server URL
 *
 * @return Pointer to the stored backend server string (read-only)
 */
const char* get_saved_backend_server(void);

#endif // CONFIG_H
