#include "rx.h"
#include "esp_log.h"
#include "esp_check.h"
#include "wmbus/packet.h"
#include "wmbus/encoding_3of6.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "wmbus_rx";

typedef struct {
    uint8_t lengthField;
    uint16_t length;
    uint16_t bytesLeft;
    uint8_t *pByteIndex;
    uint8_t format;
    bool start;
    bool complete;
} RXinfoDescr;

static cc1101_handle_t *s_cc = NULL;
static wmbus_rx_result_t *s_result = NULL;
static RXinfoDescr RXinfo;

#define RX_FIFO_THRESHOLD 0x07
#define RX_FIFO_START_THRESHOLD 0x00
#define MAX_FIXED_LENGTH 256
#define FIXED_PACKET_LENGTH 0x00
#define INFINITE_PACKET_LENGTH 0x02

#define RX_AVAILABLE_FIFO 32

static TaskHandle_t s_rx_task = NULL;

// Start RX once: clean idle, flush, set thresholds, enter RX.
static esp_err_t start_rx_once(void)
{
    cc1101_idle(s_cc);
    cc1101_strobe(s_cc, CC1101_SFRX);
    cc1101_strobe(s_cc, CC1101_SFTX);
    cc1101_write_reg(s_cc, CC1101_FIFOTHR, RX_FIFO_START_THRESHOLD);
    cc1101_write_reg(s_cc, CC1101_PKTCTRL0, INFINITE_PACKET_LENGTH);
    return cc1101_enter_rx(s_cc);
}

static void handle_gdo_event(uint32_t gpio_num)
{
    if (!s_cc || !s_result) return;

    if (gpio_num == s_cc->pins.pin_gdo0) {
        uint8_t bytesDecoded[2] = {0};
        if (RXinfo.start) {
            cc1101_read_burst(s_cc, CC1101_RXFIFO, RXinfo.pByteIndex, 3);
            wmbus_dec_status_t dec = wmbus_decode_3of6(RXinfo.pByteIndex, bytesDecoded, 0);
            if (dec != WMBUS_DEC_OK) {
                static uint32_t bad_lfield_log = 0;
                if ((bad_lfield_log++ % 10) == 0) {
                    ESP_LOGD(TAG, "Invalid 3of6 in L-field, flushing RX");
                }
                // Flush, signal abort to main loop, and let it re-arm
                RXinfo.complete = false;
                RXinfo.start = true;
                if (s_result) {
                    s_result->complete = true;
                    s_result->packet_size = 0;
                    s_result->encoded_len = 0;
                    s_result->crc_ok = false;
                }
                cc1101_idle(s_cc);
                cc1101_strobe(s_cc, CC1101_SFRX);
                cc1101_strobe(s_cc, CC1101_SFTX);
                return;
            }

            RXinfo.lengthField = bytesDecoded[0];
            RXinfo.length = wmbus_encoded_size_from_packet(wmbus_packet_size_from_l(RXinfo.lengthField));
            if (s_result) {
                s_result->packet_size = wmbus_packet_size_from_l(RXinfo.lengthField);
                s_result->encoded_len = RXinfo.length;
            }

            if (RXinfo.length < MAX_FIXED_LENGTH) {
                cc1101_write_reg(s_cc, CC1101_PKTLEN, (uint8_t)(RXinfo.length));
                cc1101_write_reg(s_cc, CC1101_PKTCTRL0, FIXED_PACKET_LENGTH);
                RXinfo.format = 1;
            } else {
                uint8_t fixedLength = RXinfo.length % MAX_FIXED_LENGTH;
                cc1101_write_reg(s_cc, CC1101_PKTLEN, fixedLength);
            }

            RXinfo.pByteIndex += 3;
            RXinfo.bytesLeft = RXinfo.length - 3;
            RXinfo.start = false;
            cc1101_write_reg(s_cc, CC1101_FIFOTHR, RX_FIFO_THRESHOLD);
        } else {
            if ((RXinfo.bytesLeft < MAX_FIXED_LENGTH) && (RXinfo.format == 0)) {
                cc1101_write_reg(s_cc, CC1101_PKTCTRL0, FIXED_PACKET_LENGTH);
                RXinfo.format = 1;
            }
            // Example: read RX_AVAILABLE_FIFO-1 bytes each time
            cc1101_read_burst(s_cc, CC1101_RXFIFO, RXinfo.pByteIndex, RX_AVAILABLE_FIFO - 1);
            RXinfo.bytesLeft -= (RX_AVAILABLE_FIFO - 1);
            RXinfo.pByteIndex += (RX_AVAILABLE_FIFO - 1);
        }
    } else if (gpio_num == s_cc->pins.pin_gdo2) {
        cc1101_read_burst(s_cc, CC1101_RXFIFO, RXinfo.pByteIndex, RXinfo.bytesLeft);
        RXinfo.complete = true;
        if (s_result) {
            s_result->complete = true;
            float rssi_dbm = 0;
            uint8_t lqi = 0;
            uint8_t status = 0;
            uint8_t rssi_raw = 0;
            uint8_t lqi_raw = 0;
            uint8_t marcstate = 0;
            uint8_t pktstatus = 0;
            if (cc1101_get_status(s_cc, &status) == ESP_OK &&
                cc1101_read_reg(s_cc, CC1101_RSSI, &rssi_raw) == ESP_OK &&
                cc1101_read_reg(s_cc, CC1101_LQI, &lqi_raw) == ESP_OK &&
                cc1101_read_reg(s_cc, CC1101_MARCSTATE, &marcstate) == ESP_OK &&
                cc1101_read_reg(s_cc, CC1101_PKTSTATUS, &pktstatus) == ESP_OK) {
                int16_t rssi_dec = (rssi_raw >= 128) ? (int16_t)(rssi_raw - 256) : (int16_t)rssi_raw;
                rssi_dbm = ((float)rssi_dec / 2.0f) - 74.0f;
                lqi = lqi_raw & 0x7F;
                s_result->rssi_dbm = rssi_dbm;
                s_result->lqi = lqi;
                s_result->rssi_raw = rssi_raw;
                s_result->lqi_raw = lqi_raw;
                s_result->marc_state = marcstate & 0x1F;
                s_result->pkt_status = pktstatus;
                ESP_LOGD(TAG, "GDO2 done: state=0x%02X RSSI=%.1f LQI=%u (raw rssi=0x%02X lqi=0x%02X marc=0x%02X pkt=0x%02X)",
                         (status >> 4) & 0x07, rssi_dbm, lqi, rssi_raw, lqi_raw, marcstate & 0x1F, pktstatus);
            } else {
                ESP_LOGW(TAG, "GDO2: failed to read RSSI/LQI");
            }
        }
    }
}

static void rx_worker_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t flags = 0;
        xTaskNotifyWait(0, 0, &flags, pdMS_TO_TICKS(100));
        if (flags & BIT0) {
            handle_gdo_event(s_cc->pins.pin_gdo0);
        }
        if (flags & BIT1) {
            handle_gdo_event(s_cc->pins.pin_gdo2);
        }
        vTaskDelay(1);
    }
}

static void IRAM_ATTR gdo_isr_handler(void *arg)
{
    uint32_t gpio = (uint32_t)arg;
    BaseType_t hp_task_woken = pdFALSE;
    if (gpio == s_cc->pins.pin_gdo0) {
        xTaskNotifyFromISR(s_rx_task, BIT0, eSetBits, &hp_task_woken);
    } else if (gpio == s_cc->pins.pin_gdo2) {
        xTaskNotifyFromISR(s_rx_task, BIT1, eSetBits, &hp_task_woken);
    }
    if (hp_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t wmbus_rx_init(cc1101_handle_t *cc)
{
    s_cc = cc;
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE, // set per-pin below
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << cc->pins.pin_gdo0) | (1ULL << cc->pins.pin_gdo2),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "gpio cfg");
    // Use edge types matching the example: GDO0 rises on FIFO threshold, GDO2 falls on packet done
    ESP_RETURN_ON_ERROR(gpio_set_intr_type(cc->pins.pin_gdo0, GPIO_INTR_POSEDGE), TAG, "intr0");
    ESP_RETURN_ON_ERROR(gpio_set_intr_type(cc->pins.pin_gdo2, GPIO_INTR_NEGEDGE), TAG, "intr2");
    if (!s_rx_task) {
        BaseType_t res = xTaskCreatePinnedToCore(rx_worker_task, "wmbus_rx", 4096, NULL, 14, &s_rx_task, 0);
        if (res != pdPASS || !s_rx_task) {
            ESP_LOGE(TAG, "Failed to create rx task");
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(cc->pins.pin_gdo0, gdo_isr_handler, (void *)cc->pins.pin_gdo0), TAG, "isr0");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(cc->pins.pin_gdo2, gdo_isr_handler, (void *)cc->pins.pin_gdo2), TAG, "isr2");
    return ESP_OK;
}

esp_err_t wmbus_rx_start(wmbus_rx_result_t *result)
{
    if (!s_cc || !result) return ESP_ERR_INVALID_ARG;
    s_result = result;
    memset(&RXinfo, 0, sizeof(RXinfo));
    RXinfo.pByteIndex = result->rx_bytes;
    RXinfo.start = true;
    RXinfo.complete = false;
    RXinfo.format = 0;
    result->complete = false;
    result->encoded_len = 0;
    result->rssi_dbm = 0;
    result->lqi = 0;
    result->rssi_raw = 0;
    result->lqi_raw = 0;
    result->marc_state = 0;
    result->pkt_status = 0;

    // Just start RX; don't hard-fail on MARC to avoid churn
    esp_err_t err = start_rx_once();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "RX start warning (continuing despite status)");
    }
    return ESP_OK;
}

esp_err_t wmbus_rx_stop(void)
{
    if (!s_cc) return ESP_ERR_INVALID_STATE;
    cc1101_idle(s_cc);
    return ESP_OK;
}
