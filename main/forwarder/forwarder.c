/*
 * forwarder.c - Backend communication implementation
 * 
 * This module handles forwarding of whitelisted wM-Bus device data to the backend server.
 * It ensures secure communication and proper payload packaging according to ESP-IDF conventions.
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>

#include "forwarder.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "esp_tls.h"

// Logging tag for this module
static const char *TAG = "Forwarder";

// Backend server configuration
static char backend_url[256] = {0};
static bool is_initialized = false;

// Maximum size for HTTP request body
#define MAX_HTTP_REQUEST_SIZE 2048

/**
 * @brief Convert binary data to hexadecimal string
 * 
 * @param data Binary data to convert
 * @param size Size of the binary data
 * @param hex_str Output buffer for hexadecimal string
 * @param hex_str_size Size of the output buffer
 */
static void bin_to_hex_str(const uint8_t *data, size_t size, char *hex_str, size_t hex_str_size) {
    if (hex_str_size < (size * 2 + 1)) {
        ESP_LOGE(TAG, "Hex string buffer too small");
        return;
    }
    
    for (size_t i = 0; i < size; i++) {
        sprintf(&hex_str[i * 2], "%02x", data[i]);
    }
    hex_str[size * 2] = '\0';
}


/**
 * @brief Initialize the forwarder module
 * 
 * This function initializes the forwarder module, setting up network connections
 * and any required resources for backend communication.
 * 
 * @return ESP_OK on success, ESP_ERR_XXX on failure
 */
esp_err_t forwarder_init(void) {
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing forwarder module");
    
    // Initialize default backend URL if not set
    if (strlen(backend_url) == 0) {
        strcpy(backend_url, "https://example.com/api/wmbus");
        ESP_LOGI(TAG, "Using default backend URL: %s", backend_url);
    }
    
    // Initialize NVS for secure storage if needed
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs reformatting");
        ret = nvs_flash_erase();
        if (ret == ESP_OK) {
            ret = nvs_flash_init();
        }
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    is_initialized = true;
    ESP_LOGI(TAG, "Forwarder module initialized successfully");
    
    return ESP_OK;
}

/**
 * @brief Check if a device is whitelisted
 * 
 * This function verifies if the provided device address is in the whitelist
 * before allowing data forwarding to the backend.
 * 
 * @param device_address Pointer to the 8-byte device address
 * @return true if device is whitelisted, false otherwise
 */
bool is_device_whitelisted(const uint8_t *device_address) {
    // This function would typically check against a stored whitelist
    // For now, we'll implement a basic check - in a real implementation
    // this would query the whitelist manager module
    
    // Convert device address to hex string for comparison
    char device_addr_hex[17]; // 8 bytes * 2 chars + null terminator
    bin_to_hex_str(device_address, 8, device_addr_hex, sizeof(device_addr_hex));
    
    ESP_LOGD(TAG, "Checking if device %s is whitelisted", device_addr_hex);
    
    // In a real implementation, this would query the whitelist manager
    // For now, we'll return true for any device (placeholder implementation)
    // A proper implementation would check against stored whitelist entries
    return true; // Placeholder - actual implementation would check whitelist
}

/**
 * @brief Package payload for forwarding to backend
 * 
 * This function prepares the payload data for secure transmission to the backend,
 * including any necessary encryption or formatting.
 * 
 * @param frame Pointer to the wM-Bus frame to package
 * @param[out] output_buffer Buffer to store the packaged payload
 * @param[out] output_size Size of the packaged payload
 * @return ESP_OK on success, ESP_ERR_XXX on failure
 */
esp_err_t package_payload_for_forwarding(const wmbus_frame_t *frame, 
                                        uint8_t **output_buffer, 
                                        size_t *output_size) {
    if (!frame || !output_buffer || !output_size) {
        ESP_LOGE(TAG, "Invalid parameters provided to package_payload_for_forwarding");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Create a JSON object to package the frame data
    cJSON *json_root = cJSON_CreateObject();
    if (!json_root) {
        ESP_LOGE(TAG, "Failed to create JSON root object");
        return ESP_ERR_NO_MEM;
    }
    
    // Convert device address to hex string
    char device_addr_hex[17]; // 8 bytes * 2 chars + null terminator
    bin_to_hex_str(frame->device_address, 8, device_addr_hex, sizeof(device_addr_hex));
    
    // Add device address to JSON
    cJSON_AddStringToObject(json_root, "device_address", device_addr_hex);
    
    // Add frame type to JSON
    cJSON_AddNumberToObject(json_root, "frame_type", frame->frame_type);
    
    // Add timestamp to JSON
    cJSON_AddNumberToObject(json_root, "timestamp", frame->timestamp);
    
    // Add payload to JSON as hex string
    if (frame->payload && frame->payload_size > 0) {
        char *payload_hex = malloc(frame->payload_size * 2 + 1);
        if (!payload_hex) {
            cJSON_Delete(json_root);
            ESP_LOGE(TAG, "Failed to allocate memory for payload hex string");
            return ESP_ERR_NO_MEM;
        }
        
        bin_to_hex_str(frame->payload, frame->payload_size, payload_hex, frame->payload_size * 2 + 1);
        cJSON_AddStringToObject(json_root, "payload", payload_hex);
        free(payload_hex);
    } else {
        cJSON_AddStringToObject(json_root, "payload", "");
    }
    
    // Convert JSON object to string
    char *json_string = cJSON_Print(json_root);
    if (!json_string) {
        cJSON_Delete(json_root);
        ESP_LOGE(TAG, "Failed to convert JSON to string");
        return ESP_ERR_NO_MEM;
    }
    
    // Allocate output buffer and copy JSON string
    size_t json_len = strlen(json_string);
    *output_buffer = malloc(json_len + 1);
    if (!(*output_buffer)) {
        cJSON_Delete(json_root);
        free(json_string);
        ESP_LOGE(TAG, "Failed to allocate output buffer");
        return ESP_ERR_NO_MEM;
    }
    
    memcpy(*output_buffer, json_string, json_len + 1);
    *output_size = json_len;
    
    // Clean up
    cJSON_Delete(json_root);
    free(json_string);
    
    ESP_LOGD(TAG, "Successfully packaged payload for forwarding, size: %d", *output_size);
    return ESP_OK;
}

/**
 * @brief HTTP event handler for the client
 * 
 * @param evt HTTP event data
 * @return ESP_OK on success
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
                     (char*)evt->header_key, (char*)evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                ESP_LOG_BUFFER_HEXDUMP(TAG, evt->data, evt->data_len, ESP_LOG_DEBUG);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

/**
 * @brief Forward data to the backend server
 * 
 * This function sends the provided wM-Bus frame data to the backend server
 * if the device is whitelisted. It handles secure communication with the backend.
 * 
 * @param frame Pointer to the wM-Bus frame structure containing data to forward
 * @return ESP_OK on success, ESP_ERR_XXX on failure
 */
esp_err_t forward_to_backend(const wmbus_frame_t *frame) {
    if (!frame) {
        ESP_LOGE(TAG, "Invalid frame parameter provided to forward_to_backend");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!is_initialized) {
        ESP_LOGE(TAG, "Forwarder module not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Check if the device is whitelisted before forwarding
    if (!is_device_whitelisted(frame->device_address)) {
        ESP_LOGW(TAG, "Device not whitelisted, skipping forward");
        return ESP_OK; // Not an error, just skip forwarding
    }
    
    // Package the payload for forwarding
    uint8_t *packed_payload = NULL;
    size_t packed_size = 0;
    
    esp_err_t ret = package_payload_for_forwarding(frame, &packed_payload, &packed_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to package payload for forwarding");
        return ret;
    }
    
    // Initialize HTTP client configuration
    esp_http_client_config_t config = {
        .url = backend_url,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_POST,
        .cert_pem = NULL, // Use esp_crt_bundle for verification
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(packed_payload);
        return ESP_FAIL;
    }
    
    // Set headers
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "User-Agent", "OMS-Hub/1.0");
    
    // Perform the HTTP POST request
    esp_http_client_set_post_field(client, (const char*)packed_payload, packed_size);
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST request completed successfully, status=%d", status_code);
        
        if (status_code >= 200 && status_code < 300) {
            ret = ESP_OK;
        } else {
            ESP_LOGW(TAG, "Backend returned error status: %d", status_code);
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        ret = err;
    }
    
    // Clean up
    esp_http_client_cleanup(client);
    free(packed_payload);
    
    return ret;
}

/**
 * @brief Set the backend server URL
 * 
 * This function allows configuration of the backend server endpoint
 * 
 * @param url The URL of the backend server
 * @return ESP_OK on success, ESP_ERR_XXX on failure
 */
esp_err_t set_backend_url(const char *url) {
    if (!url) {
        ESP_LOGE(TAG, "Invalid URL parameter provided to set_backend_url");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(url) >= sizeof(backend_url)) {
        ESP_LOGE(TAG, "URL too long for storage");
        return ESP_ERR_INVALID_ARG;
    }
    
    strcpy(backend_url, url);
    ESP_LOGI(TAG, "Backend URL set to: %s", backend_url);
    
    return ESP_OK;
}

/**
 * @brief Deinitialize the forwarder module
 * 
 * This function cleans up resources used by the forwarder module
 * 
 * @return ESP_OK on success, ESP_ERR_XXX on failure
 */
esp_err_t forwarder_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing forwarder module");
    is_initialized = false;
    
    // In a real implementation, we would clean up any allocated resources
    // For now, we just reset the initialized flag
    
    return ESP_OK;
}