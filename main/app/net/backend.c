#include "app/net/backend.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "wmbus/pipeline.h"
#include "app/storage.h"

static const char *TAG = "backend";
static const char *NAMESPACE = "backend";
static const char *KEY_URL = "url";

static esp_err_t save_url(const backend_config_t *cfg)
{
    return storage_set_str(NAMESPACE, KEY_URL, cfg->url);
}

static esp_err_t load_url(backend_config_t *cfg)
{
    return storage_get_str(NAMESPACE, KEY_URL, cfg->url, sizeof(cfg->url));
}

esp_err_t backend_init(backend_config_t *cfg)
{
    if (!cfg)
    {
        return ESP_ERR_INVALID_ARG;
    }
    memset(cfg, 0, sizeof(*cfg));
    if (load_url(cfg) != ESP_OK)
    {
        cfg->url[0] = '\0';
    }
    return ESP_OK;
}

esp_err_t backend_set_url(backend_config_t *cfg, const char *url)
{
    if (!cfg)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!url || url[0] == '\0')
    {
        cfg->url[0] = '\0';
        return save_url(cfg);
    }

    size_t len = strnlen(url, sizeof(cfg->url));
    if (len >= sizeof(cfg->url))
    {
        return ESP_ERR_INVALID_SIZE;
    }

    memset(cfg->url, 0, sizeof(cfg->url));
    memcpy(cfg->url, url, len);
    return save_url(cfg);
}

esp_err_t backend_get_url(const backend_config_t *cfg, char *out, size_t out_len)
{
    if (!cfg || !out || out_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    size_t len = strnlen(cfg->url, sizeof(cfg->url));
    if (len >= out_len)
    {
        return ESP_ERR_NO_MEM;
    }
    memset(out, 0, out_len);
    memcpy(out, cfg->url, len);
    return ESP_OK;
}

esp_err_t backend_check_url(const char *url, int timeout_ms)
{
    if (!url || url[0] == '\0')
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = timeout_ms,
        .method = HTTP_METHOD_HEAD,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);
        if (status < 200 || status >= 400)
        {
            err = ESP_FAIL;
        }
    }
    esp_http_client_cleanup(client);
    return err;
}

static void hex_encode(const uint8_t *data, uint16_t len, char *out, size_t out_len)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t pos = 0;
    for (uint16_t i = 0; i < len && (pos + 2) < out_len; i++)
    {
        out[pos++] = hex[(data[i] >> 4) & 0xF];
        out[pos++] = hex[data[i] & 0xF];
    }
    if (pos < out_len)
    {
        out[pos] = '\0';
    }
}

esp_err_t backend_forward_packet(const backend_config_t *cfg, const WmbusPacketEvent *evt)
{
    if (!cfg || !evt)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->url[0] == '\0')
    {
        return ESP_ERR_INVALID_STATE;
    }

    // Build small JSON: header + payload_len + gateway + logical packet as hex (CRC-free)
    uint16_t logical_len = evt->logical_len ? evt->logical_len : evt->frame_info.logical_len;
    if (logical_len > WMBUS_MAX_PACKET_BYTES)
    {
        logical_len = WMBUS_MAX_PACKET_BYTES;
    }
    if (logical_len == 0)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    const uint8_t *logical_src = evt->logical_packet ? evt->logical_packet : evt->raw_packet;
    const size_t hex_cap = ((size_t)logical_len * 2) + 1;
    const size_t json_cap = 512;

    char *logical_hex = calloc(1, hex_cap);
    char *json = calloc(1, json_cap);
    if (!logical_hex || !json)
    {
        free(logical_hex);
        free(json);
        return ESP_ERR_NO_MEM;
    }

    hex_encode(logical_src, logical_len, logical_hex, hex_cap);

    const uint8_t *id = evt->frame_info.header.id;
    int written = snprintf(json, json_cap,
                           "{\"gateway\":\"%s\",\"status\":%u,\"rssi\":%.1f,\"lqi\":%u,"
                           "\"manuf\":%u,\"id\":\"%02X%02X%02X%02X\",\"dev_type\":%u,"
                           "\"version\":%u,\"ci\":%u,\"payload_len\":%u,"
                           "\"logical_hex\":\"%s\"}",
                           evt->gateway_name ? evt->gateway_name : "",
                           evt->status,
                           evt->rssi_dbm,
                           evt->lqi,
                           evt->frame_info.header.manufacturer_le,
                           id[3], id[2], id[1], id[0],
                           evt->frame_info.header.device_type,
                           evt->frame_info.header.version,
                           evt->frame_info.header.ci_field,
                           evt->frame_info.payload_len,
                           logical_hex);
    if (written <= 0 || written >= (int)json_cap)
    {
        free(logical_hex);
        free(json);
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_config_t cfg_http = {
        .url = cfg->url,
        .timeout_ms = 3000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg_http);
    if (!client)
    {
        free(logical_hex);
        free(json);
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, written);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);
        if (status < 200 || status >= 300)
        {
            ESP_LOGW(TAG, "backend HTTP status %d", status);
            err = ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGW(TAG, "backend post failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    free(logical_hex);
    free(json);
    return err;
}
