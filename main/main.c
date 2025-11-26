#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "cc1101/cc1101.h"
#include "wmbus/rx.h"
#include "wmbus/packet.h"
#include "wmbus/encoding_3of6.h"
#include "esp_timer.h"
#include <stdlib.h>

static const char *TAG = "app";

static cc1101_handle_t g_cc1101;
static bool receiving = false;
static uint8_t rx_packet[291];
static uint8_t rx_bytes[584];
static wmbus_rx_result_t rx_result = {
    .rx_packet = rx_packet,
    .rx_bytes = rx_bytes,
    .packet_size = 0,
    .encoded_len = 0,
    .complete = false,
    .crc_ok = false,
    .rssi_raw = 0,
    .lqi_raw = 0,
    .marc_state = 0,
    .pkt_status = 0,
};

static void log_hex(const char *prefix, const uint8_t *buf, size_t len, size_t max_len)
{
    if (!buf || len == 0)
        return;
    size_t use_len = len;
    bool truncated = false;
    if (max_len > 0 && len > max_len)
    {
        use_len = max_len;
        truncated = true;
    }
    size_t hex_len = use_len * 2 + 1;
    char *hex = malloc(hex_len);
    if (!hex)
    {
        ESP_LOGW(TAG, "No mem to log %s", prefix);
        return;
    }
    for (size_t i = 0; i < use_len; i++)
    {
        snprintf(&hex[i * 2], 3, "%02X", buf[i]);
    }
    hex[hex_len - 1] = '\0';
    ESP_LOGI(TAG, "%s%s%s", prefix, hex, truncated ? "..." : "");
    if (truncated)
    {
        ESP_LOGI(TAG, "(truncated to %u/%u bytes)", (unsigned)use_len, (unsigned)len);
    }
    free(hex);
}

void app_main(void)
{
    // Ensure stdout is unbuffered and logging uses default vprintf
    setvbuf(stdout, NULL, _IONBF, 0);
    esp_log_set_vprintf(&vprintf);

    // Force global log level to INFO (temporary while validating radio path)
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set("cc1101", ESP_LOG_DEBUG);
    esp_log_level_set("wmbus_rx", ESP_LOG_DEBUG);

    ESP_ERROR_CHECK(nvs_flash_init());

    // CC1101 setup
    cc1101_pins_t pins = {
        .host = SPI2_HOST,
        .pin_miso = GPIO_NUM_5, // MISO per Leitfaden
        .pin_mosi = GPIO_NUM_6, // MOSI per Leitfaden
        .pin_sclk = GPIO_NUM_4, // SCK per Leitfaden
        .pin_cs = GPIO_NUM_7,   // CSN per Leitfaden
        .pin_gdo0 = GPIO_NUM_2, // FIFO interrupt
        .pin_gdo2 = GPIO_NUM_3, // Sync/packet done
    };
    ESP_ERROR_CHECK(cc1101_init(&pins, &g_cc1101));
    ESP_ERROR_CHECK(cc1101_init_registers(&g_cc1101, NULL)); // use default filter config
    ESP_ERROR_CHECK(wmbus_rx_init(&g_cc1101));

    // Main loop: receive, decode, (later) forward
    while (1)
    {
        if (!receiving)
        {
            rx_result.complete = false;
            rx_result.packet_size = 0;
            rx_result.encoded_len = 0;
            rx_result.crc_ok = false;
            ESP_ERROR_CHECK(wmbus_rx_start(&rx_result));
            receiving = true;
        }
        else if (rx_result.complete)
        {
            receiving = false;

            // Skip empty/aborted packets signaled by RX layer
            if (rx_result.packet_size == 0 || rx_result.encoded_len == 0)
            {
                continue;
            }

            // Read RSSI/LQI before idling to keep last RX value
            float rssi_dbm = rx_result.rssi_dbm;
            uint8_t lqi = rx_result.lqi;
            // Fallback read if the ISR-side read did not populate (e.g. timeout case)
            if (rssi_dbm == 0 && lqi == 0)
            {
                uint8_t rssi_raw_fallback = 0;
                uint8_t lqi_raw_fallback = 0;
                cc1101_read_reg(&g_cc1101, CC1101_RSSI, &rssi_raw_fallback);
                cc1101_read_reg(&g_cc1101, CC1101_LQI, &lqi_raw_fallback);
                cc1101_read_rssi_lqi(&g_cc1101, &rssi_dbm, &lqi);
                rx_result.rssi_dbm = rssi_dbm;
                rx_result.lqi = lqi;
                ESP_LOGD(TAG, "Fallback RSSI/LQI read: raw rssi=0x%02X lqi=0x%02X -> %.1f/%u",
                         rssi_raw_fallback, lqi_raw_fallback, rssi_dbm, lqi);
            }

            uint8_t state = 0;
            cc1101_get_status(&g_cc1101, &state);

            cc1101_idle(&g_cc1101);

            wmbus_packet_status_t status = wmbus_decode_tmode(rx_result.rx_bytes, rx_result.rx_packet, rx_result.packet_size);
            rx_result.crc_ok = (status == WMBUS_PKT_OK);

            if (!rx_result.crc_ok)
            {
                ESP_LOGW(TAG, "Packet CRC/coding error (status=%d) enc_len=%u dec_len=%u RSSI=%.1f LQI=%u state=0x%02X raw_rssi=0x%02X raw_lqi=0x%02X marc=0x%02X pkt=0x%02X",
                         status, rx_result.encoded_len, rx_result.packet_size, rssi_dbm, lqi, (state >> 4) & 0x07,
                         rx_result.rssi_raw, rx_result.lqi_raw, rx_result.marc_state, rx_result.pkt_status);
                log_hex("RAW(enc): ", rx_result.rx_bytes, rx_result.encoded_len ? rx_result.encoded_len : 3, 64);
                log_hex("RAW(dec): ", rx_result.rx_packet, rx_result.packet_size, 64);
                continue;
            }

            // Build device address (manufacturer + ID) from packet bytes
            if (rx_result.packet_size < 8)
            {
                ESP_LOGW(TAG, "Packet too short");
                continue;
            }
            char manuf_hex[7] = {0}; // 3 bytes manufacturer (reverse per wM-Bus) -> 6 hex chars
            char id_hex[9] = {0};    // 4 bytes ID -> 8 hex chars
            snprintf(manuf_hex, sizeof(manuf_hex), "%02X%02X%02X",
                     rx_result.rx_packet[3], rx_result.rx_packet[2], rx_result.rx_packet[1]); // OMS uses reverse order for manuf code
            snprintf(id_hex, sizeof(id_hex), "%02X%02X%02X%02X",
                     rx_result.rx_packet[7], rx_result.rx_packet[6], rx_result.rx_packet[5], rx_result.rx_packet[4]);
            char device_address[17] = {0};
            snprintf(device_address, sizeof(device_address), "%s%s", manuf_hex, id_hex);

            // Frame hex for forwarding/monitor
            size_t plen = rx_result.packet_size;
            char frame_hex[600] = {0};
            size_t hex_len = plen * 2;
            if (hex_len >= sizeof(frame_hex))
                hex_len = sizeof(frame_hex) - 1;
            for (size_t i = 0; i < plen && (i * 2 + 1) < sizeof(frame_hex); i++)
            {
                snprintf(&frame_hex[i * 2], 3, "%02X", rx_result.rx_packet[i]);
            }

            ESP_LOGI(TAG, "Packet OK: L=%u len(dec)=%u len(enc)=%u RSSI=%.1f LQI=%u CRC=OK Manuf=%s ID=%s Addr=%s state=0x%02X",
                     rx_result.rx_packet[0], rx_result.packet_size, rx_result.encoded_len, rssi_dbm, lqi, manuf_hex, id_hex, device_address, (state >> 4) & 0x07);
            ESP_LOGD(TAG, "raw_rssi=0x%02X raw_lqi=0x%02X marc=0x%02X pkt=0x%02X", rx_result.rssi_raw, rx_result.lqi_raw, rx_result.marc_state, rx_result.pkt_status);
            ESP_LOGD(TAG, "RAW(enc): len=%u", rx_result.encoded_len ? rx_result.encoded_len : 3);
            ESP_LOGD(TAG, "RAW(dec): len=%u", rx_result.packet_size);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
