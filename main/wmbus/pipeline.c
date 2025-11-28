#include "wmbus/pipeline.h"

#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "radio/cc1101_regs.h"
#include "radio/radio_rx.h"
#include "wmbus/packet.h"
#include "wmbus/3of6.h"
#include "radio/cc1101_hal.h"

// This file mirrors the TI SWRA234A RX logic for CC1101 T-mode, with minimal deviations.

static const char *TAG = "wmbus_rx";

#define RX_FIFO_THRESHOLD 0x07
#define RX_FIFO_START_THRESHOLD 0x00
#define RX_FIFO_SIZE 64
#define RX_AVAILABLE_FIFO 32 // half FIFO

#define FIXED_PACKET_LENGTH 0x00
#define INFINITE_PACKET_LENGTH 0x02
#define INFINITE 0
#define FIXED 1
#define MAX_FIXED_LENGTH 256

// Optional RF robustness tweak for CC1101 AGC behaviour.
// false (default):
//   - Use the “high sensitivity” AGC profile (higher LNA/DVGA gain, softer
//     attack/release).
//   - Maximises receive range and ability to decode very weak OMS frames.
//   - More susceptible to spurious wakeups and false packets in very noisy bands.
//
// true:
//   - Use a conservative / low-sensitivity AGC profile (reduced max gain,
//     faster attack, more aggressive gain back-off).
//   - Shorter range, but the receiver is less affected by continuous narrow-band
//     interferers and strong nearby transmitters (e.g. other 868 MHz devices).
//   - Recommended for “dirty” RF environments or when you see many false wakes.
static bool wmbus_rx_low_sensitivity = false;

// Carrier-sense (CS) threshold preset used for CCA / “channel busy” decisions.
// This is mapped internally to different RSSI thresholds on the CC1101 and
// influences when the radio reports the channel as occupied:
//
//   CC1101_CS_LEVEL_DEFAULT
//     - Default tuning for typical OMS deployments.
//     - Balanced between range (detect weak frames) and avoiding “always busy”.
//
//   CC1101_CS_LEVEL_LOW
//     - Lowest CS threshold (most sensitive).
//     - Channel is already “busy” for very weak signals.
//     - Good for long range, but in urban/noisy bands the channel may appear
//       permanently occupied.
//
//   CC1101_CS_LEVEL_MEDIUM
//     - Slightly higher threshold than DEFAULT.
//     - Ignores very weak background noise but still sees normal OMS meters.
//     - Often a good compromise for multi-tenant buildings.
//
//   CC1101_CS_LEVEL_HIGH
//     - Highest CS threshold (least sensitive for CS).
//     - Only treats strong signals as “busy”; weak interferers are ignored.
//     - Reduces false CCA blocking, but may transmit while a distant meter
//       is active (hidden-node risk increases).
static cc1101_cs_level_t wmbus_rx_cs_level = CC1101_CS_LEVEL_DEFAULT;

// Sync-word correlation policy for packet detection (MDMCFG2.SYNC_MODE equivalent).
// All variants require carrier sense (CS) to be asserted; the number notation
// describes how many bits of the 16-bit sync word must match:
//
//   CC1101_SYNC_MODE_DEFAULT  (15/16 + CS)
//     - Sync is detected when at least 15 of 16 sync bits correlate AND
//       carrier sense is high.
//     - Good tolerance against bit errors / jitter in the sync word.
//     - Slightly higher probability of false sync in heavy interference.
//
//   CC1101_SYNC_MODE_TIGHT    (16/16 + CS)
//     - Full 16/16 sync match required plus carrier sense.
//     - Fewer false positives, better resilience against random noise bursts.
//     - Frames with a single bit error in the sync word are rejected
//       (slightly reduced robustness at very low SNR).
//
//   CC1101_SYNC_MODE_STRICT   (30/32 + CS)
//     - Extended correlation window (e.g. preamble+sync, 30 of 32 bits must match)
//       plus carrier sense.
//     - Very selective: minimizes false syncs and ghost packets, useful in
//       high-density RF environments with many different protocols.
//     - Requires clean preamble/sync sequence; more sensitive to timing / drift,
//       so marginal links may no longer be detected.
static cc1101_sync_mode_t wmbus_rx_sync_mode = CC1101_SYNC_MODE_TIGHT;

// Setters for runtime adjustment (call wmbus_rx_apply_settings to write to radio)
void wmbus_rx_set_low_sensitivity(bool enable)
{
    wmbus_rx_low_sensitivity = enable;
}

void wmbus_rx_set_cs_level(cc1101_cs_level_t level)
{
    wmbus_rx_cs_level = level;
}

void wmbus_rx_set_sync_mode(cc1101_sync_mode_t mode)
{
    wmbus_rx_sync_mode = mode;
}

esp_err_t wmbus_rx_apply_settings(cc1101_hal_t *dev)
{
    if (!dev)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // AGC tweak
    if (wmbus_rx_low_sensitivity)
    {
        cc1101_hal_write_reg(dev, CC1101_AGCCTRL2, 0x03);
    }

    // Carrier sense threshold preset
    ESP_RETURN_ON_ERROR(cc1101_hal_set_cs_threshold(dev, wmbus_rx_cs_level), TAG, "set CS level");

    // Sync mode preset
    ESP_RETURN_ON_ERROR(cc1101_hal_set_sync_mode(dev, wmbus_rx_sync_mode), TAG, "set sync mode");

    return ESP_OK;
}

typedef struct RXinfoDescr
{
    uint8_t lengthField;
    uint16_t length;
    uint16_t bytesLeft;
    uint8_t *pByteIndex;
    uint8_t format;
    uint8_t start;
    uint8_t complete;
    uint8_t mode;
} RXinfoDescr;

static RXinfoDescr s_rxinfo;
static EventGroupHandle_t s_rx_events;
static bool s_isr_installed = false;
static cc1101_hal_t *s_dev;
static wmbus_rx_result_t *s_res;

static const uint32_t RX_EVT_FIFO = (1 << 0);
static const uint32_t RX_EVT_PKT = (1 << 1);

static void IRAM_ATTR gdo0_isr(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(s_rx_events, RX_EVT_FIFO, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

static void IRAM_ATTR gdo2_isr(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(s_rx_events, RX_EVT_PKT, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

static void rx_handle_fifo_event(void)
{
    if (!s_dev || !s_res)
    {
        return;
    }

    // Abort if RX overflow is indicated and get available bytes
    uint8_t rxbytes = 0;
    if (cc1101_hal_read_reg(s_dev, CC1101_RXBYTES, &rxbytes) == ESP_OK && (rxbytes & CC1101_RX_OVERFLOW_BM))
    {
        cc1101_hal_flush_rx(s_dev);
        s_rxinfo.complete = true;
        s_res->status = WMBUS_PKT_CODING_ERROR;
        s_rxinfo.bytesLeft = 1;
        return;
    }
    uint8_t available = rxbytes & CC1101_RXBYTES_NUM_MASK;

    if (s_rxinfo.start)
    {
        // Read the first 3 bytes
        const size_t to_read = 3;
        if (available < to_read)
        {
            return;
        }
        if (to_read > RX_FIFO_SIZE)
        {
            return;
        }
        cc1101_hal_read_fifo(s_dev, s_rxinfo.pByteIndex, to_read);

        // Decode length
        uint8_t decoded[2] = {0};
        if (wmbus_decode_3of6(s_rxinfo.pByteIndex, decoded, 0) != WMBUS_3OF6_OK)
        {
            s_rxinfo.complete = true;
            s_res->status = WMBUS_PKT_CODING_ERROR;
            s_rxinfo.bytesLeft = 1; // force incomplete exit
            return;
        }

        s_rxinfo.lengthField = decoded[0];
        s_res->l_field = decoded[0];
        uint16_t pkt_size = wmbus_packet_size(s_rxinfo.lengthField);
        if (pkt_size == 0 || pkt_size > WMBUS_MAX_PACKET_BYTES)
        {
            s_rxinfo.complete = true;
            s_res->status = WMBUS_PKT_CODING_ERROR;
            s_rxinfo.bytesLeft = 1;
            return;
        }

        s_rxinfo.length = wmbus_byte_size_tmode(false, pkt_size);
        if (s_rxinfo.length > WMBUS_MAX_ENCODED_BYTES)
        {
            s_rxinfo.complete = true;
            s_res->status = WMBUS_PKT_CODING_ERROR;
            s_rxinfo.bytesLeft = 1;
            return;
        }

        s_rxinfo.bytesLeft = s_rxinfo.length - to_read;
        s_rxinfo.pByteIndex += to_read;
        s_res->encoded_len = to_read;

        // Switch to fixed length if less than 256 bytes remain
        if (s_rxinfo.length < MAX_FIXED_LENGTH)
        {
            cc1101_hal_write_reg(s_dev, CC1101_PKTLEN, (uint8_t)(s_rxinfo.length));
            cc1101_hal_write_reg(s_dev, CC1101_PKTCTRL0, FIXED_PACKET_LENGTH);
            s_rxinfo.format = FIXED;
        }
        else
        {
            uint16_t fixedLength = s_rxinfo.length % MAX_FIXED_LENGTH;
            cc1101_hal_write_reg(s_dev, CC1101_PKTLEN, (uint8_t)fixedLength);
        }

        // Threshold to half FIFO
        cc1101_hal_write_reg(s_dev, CC1101_FIFOTHR, RX_FIFO_THRESHOLD);
        s_rxinfo.start = false;
    }
    else
    {
        // Switch to fixed if appropriate
        if ((s_rxinfo.bytesLeft < MAX_FIXED_LENGTH) && (s_rxinfo.format == INFINITE))
        {
            cc1101_hal_write_reg(s_dev, CC1101_PKTCTRL0, FIXED_PACKET_LENGTH);
            s_rxinfo.format = FIXED;
        }

        if (available == 0)
        {
            return;
        }

        size_t chunk = (s_rxinfo.bytesLeft > (RX_AVAILABLE_FIFO - 1)) ? (RX_AVAILABLE_FIFO - 1) : s_rxinfo.bytesLeft;
        if (chunk > RX_FIFO_SIZE)
        {
            chunk = RX_FIFO_SIZE;
        }
        if (available < chunk)
        {
            chunk = available;
        }
        if (chunk == 0)
        {
            return;
        }

        cc1101_hal_read_fifo(s_dev, s_rxinfo.pByteIndex, chunk);
        s_rxinfo.bytesLeft -= chunk;
        s_rxinfo.pByteIndex += chunk;
        s_res->encoded_len += chunk;
    }
}

static void rx_handle_packet_event(void)
{
    if (!s_dev || !s_res)
    {
        return;
    }

    // Abort if RX overflow is indicated
    uint8_t rxbytes = 0;
    if (cc1101_hal_read_reg(s_dev, CC1101_RXBYTES, &rxbytes) == ESP_OK && (rxbytes & CC1101_RX_OVERFLOW_BM))
    {
        cc1101_hal_flush_rx(s_dev);
        s_rxinfo.complete = true;
        s_res->status = WMBUS_PKT_CODING_ERROR;
        s_rxinfo.bytesLeft = 1;
        return;
    }

    while (s_rxinfo.bytesLeft)
    {
        uint8_t available = rxbytes & CC1101_RXBYTES_NUM_MASK;
        if (available == 0)
        {
            // Refresh available
            if (cc1101_hal_read_reg(s_dev, CC1101_RXBYTES, &rxbytes) != ESP_OK)
            {
                break;
            }
            available = rxbytes & CC1101_RXBYTES_NUM_MASK;
            if (rxbytes & CC1101_RX_OVERFLOW_BM)
            {
                cc1101_hal_flush_rx(s_dev);
                s_rxinfo.complete = true;
                s_res->status = WMBUS_PKT_CODING_ERROR;
                s_rxinfo.bytesLeft = 1;
                return;
            }
            if (available == 0)
            {
                break;
            }
        }

        size_t to_read = s_rxinfo.bytesLeft;
        if (to_read > RX_FIFO_SIZE)
        {
            to_read = RX_FIFO_SIZE;
        }
        if (available < to_read)
        {
            to_read = available;
        }
        cc1101_hal_read_fifo(s_dev, s_rxinfo.pByteIndex, to_read);
        s_res->encoded_len += to_read;
        s_rxinfo.bytesLeft -= to_read;
        s_rxinfo.pByteIndex += to_read;

        // Refresh rxbytes for next loop
        if (cc1101_hal_read_reg(s_dev, CC1101_RXBYTES, &rxbytes) != ESP_OK)
        {
            break;
        }
        if (rxbytes & 0x80)
        {
            cc1101_hal_flush_rx(s_dev);
            s_rxinfo.complete = true;
            s_res->status = WMBUS_PKT_CODING_ERROR;
            s_rxinfo.bytesLeft = 1;
            return;
        }
    }
    s_rxinfo.complete = true;
}

esp_err_t wmbus_pipeline_init(cc1101_hal_t *dev)
{
    ESP_RETURN_ON_ERROR(radio_rx_configure_tmode(dev), TAG, "config T-mode");

    if (!s_rx_events)
    {
        s_rx_events = xEventGroupCreate();
    }

    if (!s_isr_installed)
    {
        ESP_ERROR_CHECK(gpio_install_isr_service(0));
        s_isr_installed = true;
    }

    gpio_set_intr_type(dev->pins.gdo0, GPIO_INTR_POSEDGE);
    gpio_set_intr_type(dev->pins.gdo2, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(dev->pins.gdo0, gdo0_isr, NULL);
    gpio_isr_handler_add(dev->pins.gdo2, gdo2_isr, NULL);

    return ESP_OK;
}

esp_err_t wmbus_pipeline_receive(cc1101_hal_t *dev, wmbus_rx_result_t *res, uint32_t timeout_ms)
{
    if (!dev || !res || !res->rx_packet || !res->rx_bytes)
    {
        return ESP_ERR_INVALID_ARG;
    }

    s_dev = dev;
    s_res = res;

    memset(res->rx_packet, 0, WMBUS_MAX_PACKET_BYTES);
    memset(res->rx_bytes, 0, WMBUS_MAX_ENCODED_BYTES);
    if (res->rx_logical)
    {
        memset(res->rx_logical, 0, WMBUS_MAX_PACKET_BYTES);
    }
    res->encoded_len = 0;
    res->packet_size = 0;
    res->logical_len = 0;
    memset(&res->frame_info, 0, sizeof(res->frame_info));
    res->l_field = 0;
    res->status = WMBUS_PKT_CODING_ERROR;
    res->complete = false;

    // Initialize RX info
    s_rxinfo.lengthField = 0;
    s_rxinfo.length = 0;
    s_rxinfo.bytesLeft = 0;
    s_rxinfo.pByteIndex = res->rx_bytes;
    s_rxinfo.format = INFINITE;
    s_rxinfo.start = true;
    s_rxinfo.complete = false;
    s_rxinfo.mode = 0; // T-mode

    ESP_ERROR_CHECK(cc1101_hal_idle(dev));
    ESP_ERROR_CHECK(cc1101_hal_flush_rx(dev));

    // Configure thresholds for start
    cc1101_hal_write_reg(dev, CC1101_FIFOTHR, RX_FIFO_START_THRESHOLD);
    cc1101_hal_write_reg(dev, CC1101_PKTCTRL0, INFINITE_PACKET_LENGTH);

    // Apply current RX knobs (AGC/CS/Sync)
    wmbus_rx_apply_settings(dev);

    xEventGroupClearBits(s_rx_events, RX_EVT_FIFO | RX_EVT_PKT);

    // Enable interrupts
    gpio_intr_enable(dev->pins.gdo0);
    gpio_intr_enable(dev->pins.gdo2);

    cc1101_hal_enter_rx(dev);

    int64_t start_us = esp_timer_get_time();
    while (!s_rxinfo.complete)
    {
        EventBits_t bits = xEventGroupWaitBits(s_rx_events, RX_EVT_FIFO | RX_EVT_PKT, pdTRUE, pdFALSE, pdMS_TO_TICKS(500));

        if (bits & RX_EVT_FIFO)
        {
            rx_handle_fifo_event();
        }
        if (bits & RX_EVT_PKT)
        {
            rx_handle_packet_event();
        }

        if (timeout_ms)
        {
            int64_t elapsed_ms = (esp_timer_get_time() - start_us) / 1000;
            if (elapsed_ms > (int64_t)timeout_ms)
            {
                break;
            }
        }
    }

    // Disable interrupts for this session
    gpio_intr_disable(dev->pins.gdo0);
    gpio_intr_disable(dev->pins.gdo2);

    cc1101_hal_idle(dev);

    if (!s_rxinfo.complete || s_rxinfo.bytesLeft != 0 || res->encoded_len < 3)
    {
        cc1101_hal_flush_rx(dev);
        res->complete = false;
        return ESP_OK;
    }

    uint16_t pkt_size = wmbus_packet_size(s_rxinfo.lengthField);
    res->packet_size = pkt_size;
    res->status = wmbus_decode_rx_bytes_tmode(res->rx_bytes, res->rx_packet, res->packet_size);
    res->complete = true;

    if (res->status == WMBUS_PKT_OK && res->rx_logical)
    {
        wmbus_extract_frame_info(res->rx_packet, res->packet_size, res->rx_logical, WMBUS_MAX_PACKET_BYTES, &res->frame_info);
        res->logical_len = res->frame_info.logical_len;
    }

    // Capture status registers for diagnostics
    cc1101_hal_read_reg(dev, CC1101_RSSI, &res->rssi_raw);
    cc1101_hal_read_reg(dev, CC1101_LQI, &res->lqi_raw);
    cc1101_hal_read_reg(dev, CC1101_MARCSTATE, &res->marc_state);
    cc1101_hal_read_reg(dev, CC1101_PKTSTATUS, &res->pkt_status);

    float rssi_dbm = 0;
    radio_rx_read_rssi_lqi(dev, &rssi_dbm, &res->lqi);
    res->rssi_dbm = rssi_dbm;

    cc1101_hal_flush_rx(dev);
    return ESP_OK;
}
