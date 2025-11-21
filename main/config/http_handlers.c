/*
 * HTTP Request Handlers for Web Interface
 *
 * This file implements the HTTP request handlers for the wM-Bus Gateway
 * web configuration interface.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_http_client.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include "config.h"
#include "../nvs_manager/nvs_manager.h"
#include "../forwarder/forwarder.h"
#include "../system_status.h"
#include "cJSON.h"

static const char* TAG = "HTTP_HANDLERS";

#define BACKEND_TEST_MESSAGE_LEN 128

typedef struct {
    char status[32];
    char message[BACKEND_TEST_MESSAGE_LEN];
    int http_status;
} backend_test_state_t;

static backend_test_state_t backend_test_state = {
    .status = "not_tested",
    .message = "Not tested",
    .http_status = 0
};

static esp_err_t read_request_body(httpd_req_t *req, char *buf, size_t buf_size, size_t *out_len)
{
    if (!req || !buf || buf_size == 0 || !out_len) {
        return ESP_ERR_INVALID_ARG;
    }

    int remaining = req->content_len;
    if (remaining <= 0) {
        buf[0] = '\0';
        *out_len = 0;
        return ESP_OK;
    }

    size_t max_read = buf_size - 1;
    size_t to_read = remaining > max_read ? max_read : remaining;
    int ret = httpd_req_recv(req, buf, to_read);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if ((size_t)ret < buf_size) {
        buf[ret] = '\0';
    } else {
        buf[buf_size - 1] = '\0';
    }
    *out_len = ret;
    return ESP_OK;
}

static esp_err_t send_json_response(httpd_req_t *req, cJSON *json)
{
    if (!req || !json) {
        return ESP_ERR_INVALID_ARG;
    }

    char *payload = cJSON_PrintUnformatted(json);
    if (!payload) {
        return ESP_ERR_NO_MEM;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, payload);
    free(payload);
    return ESP_OK;
}

static cJSON* build_wifi_status_json(void)
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_err_t mode_err = esp_wifi_get_mode(&mode);

    wifi_config_t ap_config = {0};
    esp_err_t config_err = esp_wifi_get_config(WIFI_IF_AP, &ap_config);

    const char* mode_str = "unknown";
    const char* status_str = "disconnected";

    if (mode_err == ESP_OK) {
        switch (mode) {
            case WIFI_MODE_AP:
                mode_str = "AP";
                status_str = "running";
                break;
            case WIFI_MODE_STA:
                mode_str = "STA";
                status_str = "disconnected";
                break;
            case WIFI_MODE_APSTA:
                mode_str = "AP+STA";
                status_str = "running";
                break;
            default:
                break;
        }
    }

    const char* ssid_display = DEFAULT_AP_SSID;
    int channel = DEFAULT_AP_CHANNEL;

    if (config_err == ESP_OK && strlen((const char*)ap_config.ap.ssid) > 0) {
        ssid_display = (const char*)ap_config.ap.ssid;
        channel = ap_config.ap.channel;
    }

    cJSON* wifi = cJSON_CreateObject();
    if (!wifi) {
        return NULL;
    }

    cJSON_AddStringToObject(wifi, "mode", mode_str);
    cJSON_AddStringToObject(wifi, "status", status_str);
    cJSON_AddStringToObject(wifi, "ssid", ssid_display);
    cJSON_AddNumberToObject(wifi, "channel", channel);
    cJSON_AddNumberToObject(wifi, "rssi", 0);

    return wifi;
}

static cJSON* build_backend_status_json(void)
{
    cJSON* backend = cJSON_CreateObject();
    if (!backend) {
        return NULL;
    }

    const char* backend_url = get_saved_backend_server();
    bool configured = backend_url && backend_url[0] != '\0';

    cJSON_AddBoolToObject(backend, "configured", configured);
    cJSON_AddStringToObject(backend, "status", backend_test_state.status);
    cJSON_AddStringToObject(backend, "message", backend_test_state.message);
    cJSON_AddStringToObject(backend, "url", configured ? backend_url : "");

    if (backend_test_state.http_status > 0) {
        cJSON_AddNumberToObject(backend, "http_status", backend_test_state.http_status);
    }

    return backend;
}

static const char* radio_state_to_string(radio_status_state_t state)
{
    switch (state) {
        case RADIO_STATUS_READY:
            return "ready";
        case RADIO_STATUS_ERROR:
            return "error";
        case RADIO_STATUS_UNINITIALIZED:
        default:
            return "uninitialized";
    }
}

static cJSON* build_radio_status_json(void)
{
    const radio_status_info_t* info = system_status_get_radio();
    cJSON* radio = cJSON_CreateObject();
    if (!radio) {
        return NULL;
    }
    if (!info) {
        cJSON_Delete(radio);
        return NULL;
    }

    cJSON_AddStringToObject(radio, "state", radio_state_to_string(info->state));
    cJSON_AddBoolToObject(radio, "available", info->state == RADIO_STATUS_READY);
    cJSON_AddStringToObject(radio, "message", info->message);
    if (info->last_error != ESP_OK) {
        cJSON_AddStringToObject(radio, "error", esp_err_to_name(info->last_error));
    }

    return radio;
}

// Function declarations for HTML generators
extern char* generate_main_dashboard_html(void);
extern char* generate_wifi_config_html(void);
extern char* generate_whitelist_html(void);
extern char* generate_radio_settings_html(void);
extern char* generate_backend_settings_html(void);

/**
 * @brief Handler for the root path (main dashboard)
 */
esp_err_t root_get_handler(httpd_req_t *req)
{
    char* html_content = generate_main_dashboard_html();
    if (!html_content) {
        ESP_LOGE(TAG, "Failed to generate dashboard HTML");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, html_content);
    free(html_content);
    return ESP_OK;
}

/**
 * @brief Handler for WiFi configuration page
 */
esp_err_t wifi_config_get_handler(httpd_req_t *req)
{
    char* html_content = generate_wifi_config_html();
    if (!html_content) {
        ESP_LOGE(TAG, "Failed to generate WiFi config HTML");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, html_content);
    free(html_content);
    return ESP_OK;
}

/**
 * @brief Handler to save WiFi credentials
 */
esp_err_t wifi_config_post_handler(httpd_req_t *req)
{
    char buf[256];
    size_t len = 0;
    if (read_request_body(req, buf, sizeof(buf), &len) != ESP_OK) {
        return ESP_FAIL;
    }

    // Parse the form data (expected format: ssid=...&password=...)
    char ssid[64] = {0};
    char password[64] = {0};

    char* ssid_start = strstr(buf, "ssid=");
    char* pass_start = strstr(buf, "&password=");

    if (ssid_start && pass_start) {
        ssid_start += 5; // Skip "ssid="
        strncpy(ssid, ssid_start, pass_start - ssid_start);
        ssid[pass_start - ssid_start] = '\0';

        pass_start += 10; // Skip "&password="
        strncpy(password, pass_start, sizeof(password) - 1);
    }

    // URL decode the values (basic implementation)
    // In a real implementation, you'd want proper URL decoding
    // For now, we'll assume no special characters in SSID/password

    // Save the credentials
    esp_err_t result = save_wifi_credentials(ssid, password);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save WiFi credentials");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Respond with success
    httpd_resp_set_type(req, "application/json");
    const char* resp_str = "{\"status\":\"success\",\"message\":\"WiFi credentials saved successfully\"}";
    httpd_resp_sendstr(req, resp_str);

    return ESP_OK;
}

/**
 * @brief Handler for whitelist management page
 */
esp_err_t whitelist_get_handler(httpd_req_t *req)
{
    char* html_content = generate_whitelist_html();
    if (!html_content) {
        ESP_LOGE(TAG, "Failed to generate whitelist HTML");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, html_content);
    free(html_content);
    return ESP_OK;
}

/**
 * @brief Handler to update device whitelist
 */
esp_err_t whitelist_post_handler(httpd_req_t *req)
{
    char buf[256];
    size_t len = 0;
    if (read_request_body(req, buf, sizeof(buf), &len) != ESP_OK) {
        return ESP_FAIL;
    }

    // Parse the form data (expected format: device_address=...&action=...)
    char device_address[32] = {0};
    int action = -1; // 0 for remove, 1 for add

    char* addr_start = strstr(buf, "device_address=");
    char* action_start = strstr(buf, "&action=");

    if (addr_start && action_start) {
        addr_start += 15; // Skip "device_address="
        strncpy(device_address, addr_start, action_start - addr_start);
        device_address[action_start - addr_start] = '\0';

        action_start += 8; // Skip "&action="
        action = atoi(action_start);
    }

    // Update the whitelist
    esp_err_t result = update_device_whitelist(device_address, action);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update device whitelist");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Respond with success
    httpd_resp_set_type(req, "application/json");
    const char* resp_str = "{\"status\":\"success\",\"message\":\"Whitelist updated successfully\"}";
    httpd_resp_sendstr(req, resp_str);

    return ESP_OK;
}

/**
 * @brief Handler for radio settings page
 */
esp_err_t radio_settings_get_handler(httpd_req_t *req)
{
    char* html_content = generate_radio_settings_html();
    if (!html_content) {
        ESP_LOGE(TAG, "Failed to generate radio settings HTML");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, html_content);
    free(html_content);
    return ESP_OK;
}

/**
 * @brief Handler for backend settings page
 */
esp_err_t backend_settings_get_handler(httpd_req_t *req)
{
    char* html_content = generate_backend_settings_html();
    if (!html_content) {
        ESP_LOGE(TAG, "Failed to generate backend settings HTML");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, html_content);
    free(html_content);
    return ESP_OK;
}

/**
 * @brief API handler to get current radio settings
 */
esp_err_t api_radio_settings_get_handler(httpd_req_t *req)
{
    esp_err_t result = get_radio_settings();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get radio settings");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // For now, return a placeholder response
    // In a real implementation, this would return actual radio settings
    httpd_resp_set_type(req, "application/json");
    const char* resp_str = "{"
        "\"frequency\": 868.3, "
        "\"data_rate\": 32768, "
        "\"signal_threshold\": -80, "
        "\"sync_quality_threshold\": 0.7, "
        "\"agc_enabled\": true"
    "}";
    httpd_resp_sendstr(req, resp_str);

    return ESP_OK;
}

/**
 * @brief API handler to update radio settings
 */
esp_err_t api_radio_settings_post_handler(httpd_req_t *req)
{
    char buf[256];
    size_t len = 0;
    if (read_request_body(req, buf, sizeof(buf), &len) != ESP_OK) {
        return ESP_FAIL;
    }

    // Parse the JSON request body
    cJSON* json = cJSON_Parse(buf);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON request");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    // Extract values from JSON (placeholder implementation)
    cJSON* freq = cJSON_GetObjectItem(json, "frequency");
    cJSON* data_rate = cJSON_GetObjectItem(json, "data_rate");
    cJSON* signal_threshold = cJSON_GetObjectItem(json, "signal_threshold");
    cJSON* sync_quality_threshold = cJSON_GetObjectItem(json, "sync_quality_threshold");
    cJSON* agc_enabled = cJSON_GetObjectItem(json, "agc_enabled");

    // In a real implementation, these values would be used to configure the CC1101
    if (freq) {
        ESP_LOGI(TAG, "Setting frequency to: %f", freq->valuedouble);
    }
    if (data_rate) {
        ESP_LOGI(TAG, "Setting data rate to: %d", data_rate->valueint);
    }
    if (signal_threshold) {
        ESP_LOGI(TAG, "Setting signal threshold to: %d", signal_threshold->valueint);
    }
    if (sync_quality_threshold) {
        ESP_LOGI(TAG, "Setting sync quality threshold to: %f", sync_quality_threshold->valuedouble);
    }
    if (agc_enabled) {
        ESP_LOGI(TAG, "Setting AGC to: %s", agc_enabled->valueint ? "enabled" : "disabled");
    }

    cJSON_Delete(json);

    // Respond with success
    httpd_resp_set_type(req, "application/json");
    const char* resp_str = "{\"status\":\"success\",\"message\":\"Radio settings updated successfully\"}";
    httpd_resp_sendstr(req, resp_str);

    return ESP_OK;
}

/**
 * @brief API handler to get current WiFi status
 */
esp_err_t api_wifi_status_get_handler(httpd_req_t *req)
{
    cJSON* wifi = build_wifi_status_json();
    if (!wifi) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = send_json_response(req, wifi);
    cJSON_Delete(wifi);
    return ret;
}

/**
 * @brief API handler to get current whitelist
 */
esp_err_t api_whitelist_get_handler(httpd_req_t *req)
{
    cJSON* array = cJSON_CreateArray();
    if (!array) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_manager_safe_open("whitelist", NVS_READONLY, &nvs_handle);
    if (ret == ESP_OK) {
        size_t whitelist_size = 0;
        ret = nvs_get_blob(nvs_handle, NVS_WHITELIST, NULL, &whitelist_size);
        if (ret == ESP_OK && whitelist_size > 0) {
            char* buffer = malloc(whitelist_size + 1);
            if (buffer) {
                ret = nvs_get_blob(nvs_handle, NVS_WHITELIST, buffer, &whitelist_size);
                if (ret == ESP_OK) {
                    buffer[whitelist_size] = '\0';
                    char* token = strtok(buffer, ";");
                    while (token != NULL) {
                        if (strlen(token) > 0) {
                            cJSON_AddItemToArray(array, cJSON_CreateString(token));
                        }
                        token = strtok(NULL, ";");
                    }
                } else {
                    ESP_LOGW(TAG, "Unable to read whitelist blob: %s", esp_err_to_name(ret));
                }
                free(buffer);
            } else {
                ESP_LOGE(TAG, "Failed to allocate buffer for whitelist read");
            }
        }
        nvs_manager_safe_close(nvs_handle);
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to open whitelist namespace: %s", esp_err_to_name(ret));
    }

    esp_err_t send_ret = send_json_response(req, array);
    cJSON_Delete(array);
    return send_ret;
}

/**
 * @brief API handler to get dashboard statistics
 */
esp_err_t api_dashboard_stats_get_handler(httpd_req_t *req)
{
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(root, "total_packets", 0);
    cJSON_AddNumberToObject(root, "forwarded_packets", 0);
    cJSON_AddNumberToObject(root, "detected_devices", 0);

    cJSON* connection_status = cJSON_CreateObject();
    if (!connection_status) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    cJSON* wifi = build_wifi_status_json();
    if (!wifi) {
        cJSON_Delete(connection_status);
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddItemToObject(connection_status, "wifi", wifi);

    cJSON* backend = build_backend_status_json();
    if (!backend) {
        cJSON_Delete(connection_status);
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddItemToObject(connection_status, "backend", backend);

    cJSON* radio = build_radio_status_json();
    if (!radio) {
        cJSON_Delete(connection_status);
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddItemToObject(connection_status, "radio", radio);

    cJSON_AddItemToObject(root, "connection_status", connection_status);

    esp_err_t ret = send_json_response(req, root);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t backend_test_http_event_handler(esp_http_client_event_t *evt)
{
    // No special handling required for the simple connectivity check
    (void)evt;
    return ESP_OK;
}

esp_err_t api_backend_status_get_handler(httpd_req_t *req)
{
    cJSON* backend = build_backend_status_json();
    if (!backend) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = send_json_response(req, backend);
    cJSON_Delete(backend);
    return ret;
}

esp_err_t api_backend_save_handler(httpd_req_t *req)
{
    char buf[512];
    size_t len = 0;
    if (read_request_body(req, buf, sizeof(buf), &len) != ESP_OK) {
        return ESP_FAIL;
    }

    cJSON* json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    cJSON* url_item = cJSON_GetObjectItem(json, "server_url");
    if (!cJSON_IsString(url_item) || strlen(url_item->valuestring) == 0) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing backend URL");
        return ESP_FAIL;
    }

    esp_err_t ret = save_backend_settings(url_item->valuestring);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save backend settings: %s", esp_err_to_name(ret));
        cJSON_Delete(json);
        httpd_resp_send_500(req);
        return ret;
    }

    esp_err_t forwarder_ret = set_backend_url(url_item->valuestring);
    if (forwarder_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update forwarder backend URL: %s", esp_err_to_name(forwarder_ret));
    }

    backend_test_state.http_status = 0;
    snprintf(backend_test_state.status, sizeof(backend_test_state.status), "not_tested");
    snprintf(backend_test_state.message, sizeof(backend_test_state.message), "Backend URL stored, run a test to verify connectivity.");

    cJSON_Delete(json);

    cJSON* response = cJSON_CreateObject();
    if (!response) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(response, "status", "success");
    cJSON_AddStringToObject(response, "message", "Backend settings saved");

    esp_err_t send_ret = send_json_response(req, response);
    cJSON_Delete(response);
    return send_ret;
}

esp_err_t api_backend_test_handler(httpd_req_t *req)
{
    char buf[512];
    size_t len = 0;
    if (read_request_body(req, buf, sizeof(buf), &len) != ESP_OK) {
        return ESP_FAIL;
    }

    cJSON* json = cJSON_Parse(buf);
    const char* test_url = NULL;

    if (json) {
        cJSON* url_item = cJSON_GetObjectItem(json, "server_url");
        if (cJSON_IsString(url_item) && strlen(url_item->valuestring) > 0) {
            test_url = url_item->valuestring;
        }
    }

    if (!test_url) {
        test_url = get_saved_backend_server();
    }

    if (!test_url || strlen(test_url) == 0) {
        if (json) {
            cJSON_Delete(json);
        }
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No backend URL configured");
        return ESP_FAIL;
    }

    esp_http_client_config_t config = {
        .url = test_url,
        .event_handler = backend_test_http_event_handler,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for backend test");
        if (json) {
            cJSON_Delete(json);
        }
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    esp_http_client_set_method(client, HTTP_METHOD_HEAD);
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    if (err == ESP_OK) {
        if (status_code >= 200 && status_code < 400) {
            snprintf(backend_test_state.status, sizeof(backend_test_state.status), "success");
            snprintf(backend_test_state.message, sizeof(backend_test_state.message), "Backend reachable (HTTP %d)", status_code);
        } else {
            snprintf(backend_test_state.status, sizeof(backend_test_state.status), "failure");
            snprintf(backend_test_state.message, sizeof(backend_test_state.message), "Backend returned HTTP %d", status_code);
        }
    } else {
        snprintf(backend_test_state.status, sizeof(backend_test_state.status), "failure");
        snprintf(backend_test_state.message, sizeof(backend_test_state.message), "Connection error: %s", esp_err_to_name(err));
    }

    backend_test_state.http_status = status_code > 0 ? status_code : -1;

    esp_http_client_cleanup(client);

    cJSON_Delete(json);

    cJSON* response = cJSON_CreateObject();
    if (!response) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(response, "status", backend_test_state.status);
    cJSON_AddStringToObject(response, "message", backend_test_state.message);
    cJSON_AddNumberToObject(response, "http_status", backend_test_state.http_status);
    cJSON_AddStringToObject(response, "url", test_url);

    esp_err_t send_ret = send_json_response(req, response);
    cJSON_Delete(response);
    return send_ret;
}

/**
 * @brief API handler to get detected devices
 */
esp_err_t api_detected_devices_get_handler(httpd_req_t *req)
{
    // Placeholder implementation - in a real system this would return
    // actual detected devices from the wM-Bus receiver
    httpd_resp_set_type(req, "application/json");
    const char* resp_str = "["
        "{"
            "\"address\": \"12345678\", "
            "\"rssi\": -65, "
            "\"last_seen\": \"2023-06-15T10:30:45Z\", "
            "\"type\": \"Water Meter\""
        "},"
        "{"
            "\"address\": \"87654321\", "
            "\"rssi\": -72, "
            "\"last_seen\": \"2023-06-15T10:29:30Z\", "
            "\"type\": \"Heat Meter\""
        "},"
        "{"
            "\"address\": \"AABBCCDD\", "
            "\"rssi\": -68, "
            "\"last_seen\": \"2023-06-15T10:28:15Z\", "
            "\"type\": \"Gas Meter\""
        "}"
    "]";
    httpd_resp_sendstr(req, resp_str);

    return ESP_OK;
}

// Define the HTTP URI handlers
httpd_uri_t uri_handlers[] = {
    {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_get_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/wifi",
        .method    = HTTP_GET,
        .handler   = wifi_config_get_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/wifi",
        .method    = HTTP_POST,
        .handler   = wifi_config_post_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/whitelist",
        .method    = HTTP_GET,
        .handler   = whitelist_get_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/whitelist",
        .method    = HTTP_POST,
        .handler   = whitelist_post_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/radio",
        .method    = HTTP_GET,
        .handler   = radio_settings_get_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/backend",
        .method    = HTTP_GET,
        .handler   = backend_settings_get_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/api/radio",
        .method    = HTTP_GET,
        .handler   = api_radio_settings_get_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/api/radio",
        .method    = HTTP_POST,
        .handler   = api_radio_settings_post_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/api/wifi_status",
        .method    = HTTP_GET,
        .handler   = api_wifi_status_get_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/api/whitelist",
        .method    = HTTP_GET,
        .handler   = api_whitelist_get_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/api/dashboard_stats",
        .method    = HTTP_GET,
        .handler   = api_dashboard_stats_get_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/api/backend_status",
        .method    = HTTP_GET,
        .handler   = api_backend_status_get_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/api/backend",
        .method    = HTTP_POST,
        .handler   = api_backend_save_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/api/backend/test",
        .method    = HTTP_POST,
        .handler   = api_backend_test_handler,
        .user_ctx  = NULL
    },
    {
        .uri       = "/api/detected_devices",
        .method    = HTTP_GET,
        .handler   = api_detected_devices_get_handler,
        .user_ctx  = NULL
    }
};

int num_uri_handlers = sizeof(uri_handlers) / sizeof(httpd_uri_t);
