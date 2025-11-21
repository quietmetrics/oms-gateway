#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cc1101/cc1101.h"
#include "config/config.h"
#include "nvs_manager/nvs_manager.h"
#include "system_status.h"
#include "wmbus/wmbus_receiver.h"

static const char *TAG = "MAIN";

static void wmbus_frame_handler(const wmbus_decoded_frame_t *frame)
{
    if (!frame) {
        return;
    }
    ESP_LOGI(TAG, "wM-Bus frame len=%u type=0x%02X encrypted=%s",
             frame->raw_length,
             frame->frame.type,
             frame->frame.is_encrypted ? "yes" : "no");
    ESP_LOGI(TAG, "Device address: %02X%02X%02X%02X%02X%02X",
             frame->frame.address[0], frame->frame.address[1],
             frame->frame.address[2], frame->frame.address[3],
             frame->frame.address[4], frame->frame.address[5]);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting wM-Bus Gateway Application");

    // Initialize centralized NVS management system
    esp_err_t ret = nvs_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS manager: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "NVS manager initialized");

    // Initialize configuration manager
    ESP_LOGI(TAG, "Initializing configuration manager...");
    ret = init_config_manager();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize configuration manager: %s", esp_err_to_name(ret));
        return;
    }

    // Start WiFi in AP mode for initial configuration
    ESP_LOGI(TAG, "Starting WiFi in AP mode for configuration...");
    ret = start_wifi_ap_mode();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi in AP mode: %s", esp_err_to_name(ret));
        return;
    }

    // Start the web server for configuration
    ESP_LOGI(TAG, "Starting web server...");
    ret = start_web_server();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(ret));
        return;
    }

    if (wmbus_receiver_init(wmbus_frame_handler) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize wM-Bus receiver");
        return;
    }

    bool radio_ready = false;

    // Initialize CC1101 radio module
    ESP_LOGI(TAG, "Initializing CC1101 radio module...");
    ret = initialize_cc1101();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize CC1101: %s", esp_err_to_name(ret));
        system_status_set_radio(RADIO_STATUS_ERROR, "CC1101 initialization failed", ret);
    } else {
        system_status_set_radio(RADIO_STATUS_READY, "CC1101 reset complete", ESP_OK);
        radio_ready = true;
    }

    if (radio_ready) {
        // Configure CC1101 for wM-Bus operation
        ESP_LOGI(TAG, "Configuring CC1101 for wM-Bus operation...");
        cc1101_config_t wmbus_config = {
            .frequency_mhz = WM_BUS_FREQ_868_MHZ,
            .data_rate_bps = WM_BUS_DATA_RATE_38_KBAUD,
            .modulation_type = 0x02,  // GFSK
            .sync_mode = 0x06,        // 16/16 sync bits
            .packet_length = 0x3D,    // Max packet length
            .use_crc = true,
            .append_status = true
        };
        ret = configure_rf_parameters(&wmbus_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure CC1101 for wM-Bus: %s", esp_err_to_name(ret));
            system_status_set_radio(RADIO_STATUS_ERROR, "CC1101 configuration failed", ret);
            radio_ready = false;
        }
    }

    if (radio_ready) {
        // Set to RX mode
        ret = set_cc1101_mode(CC1101_MODE_RX);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set CC1101 to RX mode: %s", esp_err_to_name(ret));
            system_status_set_radio(RADIO_STATUS_ERROR, "Failed to enter RX mode", ret);
            radio_ready = false;
        } else {
            ESP_LOGI(TAG, "CC1101 initialized and configured for wM-Bus reception");
            system_status_set_radio(RADIO_STATUS_READY, "CC1101 ready (RX mode)", ESP_OK);
        }
    } else {
        ESP_LOGW(TAG, "CC1101 unavailable - running without radio functionality");
    }

    // Main loop - receive packets
    uint8_t rx_buffer[256];
    size_t packet_size;

    ESP_LOGI(TAG, "Starting packet reception loop...");

    while (1) {
        if (!radio_ready) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ret = receive_packet(rx_buffer, sizeof(rx_buffer), &packet_size, 5000); // 5 second timeout
        if (ret == ESP_OK) {
            wmbus_error_t decode_status = wmbus_receiver_process_encoded(rx_buffer, packet_size);
            if (decode_status != WMBUS_SUCCESS) {
                ESP_LOGD(TAG, "wM-Bus processing failed: %d", decode_status);
            }
        } else if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGD(TAG, "No packet received within timeout period");
        } else {
            ESP_LOGE(TAG, "Error receiving packet: %s", esp_err_to_name(ret));
        }

        // Small delay to prevent excessive CPU usage
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
