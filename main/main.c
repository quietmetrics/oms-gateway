#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "radio/pins.h"
#include "radio/cc1101_hal.h"
#include "wmbus/pipeline.h"
#include "wmbus/packet.h"

static const char *TAG = "app";

static void log_hex(const char *prefix, const uint8_t *buf, size_t len, size_t max_len)
{
    if (!buf || len == 0)
    {
        return;
    }

    size_t use_len = len;
    bool truncated = false;
    if (max_len > 0 && len > max_len)
    {
        use_len = max_len;
        truncated = true;
    }

    size_t hex_cap = (use_len * 2) + 1;
    char *hex = malloc(hex_cap);
    if (!hex)
    {
        ESP_LOGW(TAG, "No mem to log hex (len=%u)", (unsigned)use_len);
        return;
    }

    size_t out_len = 0;
    for (size_t i = 0; i < use_len && (out_len + 2) < hex_cap; i++)
    {
        int written = snprintf(&hex[out_len], hex_cap - out_len, "%02X", buf[i]);
        if (written <= 0)
        {
            break;
        }
        out_len += (size_t)written;
    }

    ESP_LOGI(TAG, "%s%s%s", prefix, hex, truncated ? "..." : "");
    if (truncated)
    {
        ESP_LOGI(TAG, "(truncated to %u/%u bytes)", (unsigned)use_len, (unsigned)len);
    }
    free(hex);
}

static void log_packet_summary(const wmbus_rx_result_t *res, const WmbusFrameInfo *info)
{
    if (!res)
    {
        return;
    }

    const bool ok = (res->status == WMBUS_PKT_OK) && res->packet_size >= 8;
    ESP_LOGI(TAG, "Packet %s: status=%u L=%u len(dec)=%u len(enc)=%u logical=%u payload_len=%u RSSI=%.1f LQI=%u marc=0x%02X pkt=0x%02X",
             ok ? "OK" : "ERR",
             res->status,
             res->packet_size ? res->rx_packet[0] : 0,
             res->packet_size,
             res->encoded_len,
             info ? info->logical_len : 0,
             info ? info->payload_len : 0,
             res->rssi_dbm,
             res->lqi,
             res->marc_state,
             res->pkt_status);

    if (info && info->parsed)
    {
        char manuf_hex[5] = {0};
        char id_hex[9] = {0};
        snprintf(manuf_hex, sizeof(manuf_hex), "%02X%02X", (uint8_t)(info->header.manufacturer_le & 0xFF), (uint8_t)((info->header.manufacturer_le >> 8) & 0xFF));
        snprintf(id_hex, sizeof(id_hex), "%02X%02X%02X%02X", info->header.id[3], info->header.id[2], info->header.id[1], info->header.id[0]);

        ESP_LOGI(TAG, "Header: Manuf=%s ID=%s CI=0x%02X Ver=0x%02X Dev=0x%02X", manuf_hex, id_hex, info->header.ci_field, info->header.version, info->header.device_type);
    }
}

static void log_diag(const wmbus_rx_result_t *res)
{
    if (!res)
    {
        return;
    }

    log_hex("RAW(enc): ", res->rx_bytes, res->encoded_len, 16);
    log_hex("RAW(dec): ", res->rx_packet, res->packet_size, 16);
    ESP_LOGI(TAG, "Diag: marc=0x%02X pkt=0x%02X raw_rssi=0x%02X raw_lqi=0x%02X",
             res->marc_state, res->pkt_status, res->rssi_raw, res->lqi_raw);
}

void app_main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set("wmbus_rx", ESP_LOG_DEBUG);

    ESP_ERROR_CHECK(nvs_flash_init());

    cc1101_hal_t cc1101 = {0};
    cc1101_pin_config_t pins = cc1101_default_pins();
    ESP_ERROR_CHECK(cc1101_hal_init(&pins, &cc1101));
    ESP_ERROR_CHECK(wmbus_pipeline_init(&cc1101));

    static uint8_t rx_packet[WMBUS_MAX_PACKET_BYTES];
    static uint8_t rx_bytes[WMBUS_MAX_ENCODED_BYTES];
    static uint8_t rx_packet_stripped[WMBUS_MAX_PACKET_BYTES];
    wmbus_rx_result_t res = {
        .rx_packet = rx_packet,
        .rx_bytes = rx_bytes,
    };

    while (true)
    {
        ESP_ERROR_CHECK(wmbus_pipeline_receive(&cc1101, &res, 1500));

        if (!res.complete || res.packet_size == 0 || res.encoded_len == 0)
        {
            ESP_LOGD(TAG, "RX incomplete: complete=%d packet_size=%u encoded_len=%u status=%u", res.complete, res.packet_size, res.encoded_len, res.status);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        WmbusFrameInfo info = {0};
        wmbus_extract_frame_info(res.rx_packet, res.packet_size, rx_packet_stripped, sizeof(rx_packet_stripped), &info);
        log_packet_summary(&res, &info);

        if (!info.parsed)
        {
            ESP_LOGW(TAG, "Header parse failed");
        }
        if (res.status != WMBUS_PKT_OK)
        {
            log_diag(&res);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
