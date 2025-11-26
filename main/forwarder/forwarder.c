#include "forwarder.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "forwarder";

esp_err_t forwarder_send(const forwarder_cfg_t *cfg, const forwarder_frame_t *frame)
{
    if (!cfg || !frame) return ESP_ERR_INVALID_ARG;
    if (!cfg->backend_host || cfg->backend_host[0] == '\0') return ESP_ERR_INVALID_STATE;

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddStringToObject(root, "gateway_id", cfg->gateway_id ? cfg->gateway_id : "");
    cJSON_AddStringToObject(root, "radio_profile", "OMS-T-mode");
    cJSON_AddNumberToObject(root, "rssi_dbm", frame->rssi_dbm);
    cJSON_AddNumberToObject(root, "lqi", frame->lqi);
    cJSON_AddBoolToObject(root, "crc_ok", frame->crc_ok);
    cJSON_AddStringToObject(root, "device_address", frame->device_address_hex ? frame->device_address_hex : "");
    cJSON_AddStringToObject(root, "wmbus_frame_hex", frame->frame_hex ? frame->frame_hex : "");

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return ESP_ERR_NO_MEM;

    char url[128];
    snprintf(url, sizeof(url), "%s://%s:%d/api/v1/uplink",
             cfg->backend_https ? "https" : "http",
             cfg->backend_host,
             cfg->backend_port);

    esp_http_client_config_t http_cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (!client) {
        free(json);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (err == ESP_OK) {
        err = esp_http_client_set_post_field(client, json, strlen(json));
    }
    if (err == ESP_OK) {
        err = esp_http_client_perform(client);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "POST failed: %s", esp_err_to_name(err));
    } else {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "POST %s status=%d", url, status);
    }

    esp_http_client_cleanup(client);
    free(json);
    return err;
}
