#include "rx.h"
#include "esp_log.h"
#include "esp_check.h"
#include "wmbus/packet.h"
#include "wmbus/encoding_3of6.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
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

typedef struct {
    uint32_t gpio_num;
} rx_event_t;

static QueueHandle_t s_rx_evt_queue = NULL;
static TaskHandle_t s_rx_task = NULL;

static void handle_gdo_event(uint32_t gpio_num)
{
    if (!s_cc || !s_result) return;

    if (gpio_num == s_cc->pins.pin_gdo0) {
        uint8_t bytesDecoded[2] = {0};
        if (RXinfo.start) {
            cc1101_read_burst(s_cc, CC1101_RXFIFO, RXinfo.pByteIndex, 3);
            // Example behavior: decode header but do not abort on error; final decode_tmode will decide
            wmbus_decode_3of6(RXinfo.pByteIndex, bytesDecoded, 0);

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
    rx_event_t evt;
    while (1) {
        if (xQueueReceive(s_rx_evt_queue, &evt, portMAX_DELAY) == pdTRUE) {
            handle_gdo_event(evt.gpio_num);
        }
    }
}

static void IRAM_ATTR gdo_isr_handler(void *arg)
{
    if (!s_rx_evt_queue) return;
    rx_event_t evt = {
        .gpio_num = (uint32_t)arg,
    };
    BaseType_t hp_task_woken = pdFALSE;
    xQueueSendFromISR(s_rx_evt_queue, &evt, &hp_task_woken);
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
    if (!s_rx_evt_queue) {
        s_rx_evt_queue = xQueueCreate(64, sizeof(rx_event_t));
    }
    if (!s_rx_evt_queue) {
        return ESP_ERR_NO_MEM;
    }
    if (!s_rx_task) {
        xTaskCreate(rx_worker_task, "wmbus_rx", 4096, NULL, 8, &s_rx_task);
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

    uint8_t status = 0;
    ESP_RETURN_ON_ERROR(cc1101_get_status(s_cc, &status), TAG, "get status");
    uint8_t state = (status >> 4) & 0x07;
    if (state != 0) { // 0 = IDLE per CC1101 datasheet
        ESP_LOGW(TAG, "Radio not idle (state=0x%02X), forcing IDLE", state);
        ESP_RETURN_ON_ERROR(cc1101_idle(s_cc), TAG, "idle");
        vTaskDelay(pdMS_TO_TICKS(1));
        if (state == 6) { // RX FIFO overflow
            cc1101_strobe(s_cc, CC1101_SFRX);
        } else if (state == 7) { // TX FIFO underflow
            cc1101_strobe(s_cc, CC1101_SFTX);
        }
    }

    // Ensure we are idle and FIFO is clean before RX (Example style)
    ESP_RETURN_ON_ERROR(cc1101_strobe(s_cc, CC1101_SFRX), TAG, "flush");

    cc1101_write_reg(s_cc, CC1101_FIFOTHR, RX_FIFO_START_THRESHOLD);
    cc1101_write_reg(s_cc, CC1101_PKTCTRL0, INFINITE_PACKET_LENGTH);

    ESP_RETURN_ON_ERROR(cc1101_enter_rx(s_cc), TAG, "enter rx");
    uint8_t marc_after = 0;
    if (cc1101_read_reg(s_cc, CC1101_MARCSTATE, &marc_after) == ESP_OK) {
        marc_after &= 0x1F;
        if (marc_after != 0x0D && marc_after != 0x0E) { // expected RX / RX_END
            ESP_LOGW(TAG, "After SRX, MARCSTATE=0x%02X (expected RX)", marc_after);
        }
    }
    return ESP_OK;
}

esp_err_t wmbus_rx_stop(void)
{
    if (!s_cc) return ESP_ERR_INVALID_STATE;
    cc1101_idle(s_cc);
    return ESP_OK;
}
