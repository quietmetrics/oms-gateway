#include "cc1101.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_rom_sys.h"
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include "wmbus/wmbus.h"

static const char *TAG = "CC1101_DRIVER";

// SPI device handle
static spi_device_handle_t cc1101_spi_handle = NULL;

// SPI helper flags
#define CC1101_SPI_MAX_TRANSFER 130
#define CC1101_FIFO_SIZE 64
#define CC1101_SPI_READ_FLAG  0x80
#define CC1101_SPI_BURST_FLAG 0x40
#define CC1101_STATE_RX 0x0D
#define WMBUS_HEADER_ENCODED_LEN 3
#define CC1101_RX_FIFO_START_THRESHOLD 0x00
#define CC1101_RX_FIFO_ACTIVE_THRESHOLD 0x07
#define CC1101_RX_SAFE_CHUNK 31
#define CC1101_SPI_READY_TIMEOUT_US 5000

typedef enum {
    CC1101_GDO_EVENT_FIFO = 1,
    CC1101_GDO_EVENT_PACKET = 2
} cc1101_gdo_event_t;

typedef struct {
    uint8_t *buffer;
    size_t buffer_size;
    size_t bytes_read;
    size_t expected_len;
    size_t original_expected_len;
    bool expected_len_known;
    bool truncated;
    bool packet_done;
    bool complete;
    bool error;
} cc1101_rx_context_t;

static QueueHandle_t cc1101_gdo_event_queue = NULL;
static bool cc1101_isr_service_installed = false;
static bool cc1101_gdo_handlers_attached = false;
static volatile bool cc1101_rx_session_active = false;
static cc1101_rx_context_t cc1101_rx_ctx = {0};

static void cc1101_process_fifo_event(void);
static void cc1101_process_packet_event(void);
static esp_err_t cc1101_flush_rx_fifo(void);
static bool wmbus_calculate_expected_encoded_length(const uint8_t *header, size_t *encoded_len_out);

static void cc1101_trace_marcstate_sequence(const char *context, int samples, uint32_t delay_us)
{
    if (samples <= 0) {
        return;
    }

    for (int i = 0; i < samples; i++) {
        uint8_t state = cc1101_read_register(CC1101_MARCSTATE) & 0x1F;
        ESP_LOGI(TAG, "[%s] settle[%d]=0x%02X", context ? context : "marc", i, state);
        if (delay_us > 0) {
            esp_rom_delay_us(delay_us);
        }
    }
}

static void cc1101_log_state_snapshot(const char *context, uint8_t observed_marcstate)
{
    uint8_t current_state = cc1101_read_register(CC1101_MARCSTATE) & 0x1F;
    uint8_t rxbytes = cc1101_read_register(CC1101_RXBYTES);
    uint8_t pktstatus = cc1101_read_register(CC1101_PKTSTATUS);
    uint8_t mcsm2 = cc1101_read_register(CC1101_MCSM2);
    uint8_t mcsm1 = cc1101_read_register(CC1101_MCSM1);

    ESP_LOGW(TAG,
             "[%s] MARCSTATE observed=0x%02X current=0x%02X RXBYTES=0x%02X PKTSTATUS=0x%02X MCSM2=0x%02X MCSM1=0x%02X",
             context ? context : "diag",
             observed_marcstate & 0x1F,
             current_state,
             rxbytes,
             pktstatus,
             mcsm2,
             mcsm1);
}

static void cc1101_log_rf_registers(const char *context)
{
    uint8_t fsctrl1 = cc1101_read_register(CC1101_FSCTRL1);
    uint8_t fsctrl0 = cc1101_read_register(CC1101_FSCTRL0);
    uint8_t freq2 = cc1101_read_register(CC1101_FREQ2);
    uint8_t freq1 = cc1101_read_register(CC1101_FREQ1);
    uint8_t freq0 = cc1101_read_register(CC1101_FREQ0);
    uint8_t mdmcfg4 = cc1101_read_register(CC1101_MDMCFG4);
    uint8_t mdmcfg3 = cc1101_read_register(CC1101_MDMCFG3);
    uint8_t mdmcfg2 = cc1101_read_register(CC1101_MDMCFG2);
    uint8_t mdmcfg1 = cc1101_read_register(CC1101_MDMCFG1);
    uint8_t mdmcfg0 = cc1101_read_register(CC1101_MDMCFG0);
    uint8_t deviatn = cc1101_read_register(CC1101_DEVIATN);
    uint8_t fscal3 = cc1101_read_register(CC1101_FSCAL3);
    uint8_t fscal2 = cc1101_read_register(CC1101_FSCAL2);
    uint8_t fscal1 = cc1101_read_register(CC1101_FSCAL1);
    uint8_t fscal0 = cc1101_read_register(CC1101_FSCAL0);

    ESP_LOGI(TAG, "[%s] FSCTRL1=0x%02X FSCTRL0=0x%02X FREQ=0x%02X%02X%02X",
             context ? context : "rf",
             fsctrl1,
             fsctrl0,
             freq2,
             freq1,
             freq0);
    ESP_LOGI(TAG, "[%s] MDMCFG4=0x%02X MDMCFG3=0x%02X MDMCFG2=0x%02X MDMCFG1=0x%02X MDMCFG0=0x%02X DEVIATN=0x%02X",
             context ? context : "rf",
             mdmcfg4,
             mdmcfg3,
             mdmcfg2,
             mdmcfg1,
             mdmcfg0,
             deviatn);
    ESP_LOGI(TAG, "[%s] FSCAL3=0x%02X FSCAL2=0x%02X FSCAL1=0x%02X FSCAL0=0x%02X",
             context ? context : "rf",
             fscal3,
             fscal2,
             fscal1,
             fscal0);
}

// Default wM-Bus configuration
static cc1101_config_t default_wmbus_config = {
    .frequency_mhz = WM_BUS_FREQ_868_MHZ,
    .data_rate_bps = WM_BUS_DATA_RATE_38_KBAUD,
    .modulation_type = 0x02,  // GFSK
    .sync_mode = 0x06,        // 16/16 sync bits
    .packet_length = 0x3D,    // Max packet length 61 bytes
    .use_crc = true,
    .append_status = true
};

static inline void cc1101_chip_select(void)
{
    gpio_set_level(CC1101_CS_GPIO, 0);
}

static inline void cc1101_chip_deselect(void)
{
    gpio_set_level(CC1101_CS_GPIO, 1);
}

static bool cc1101_wait_for_miso_low(uint32_t timeout_us)
{
    uint32_t waited = 0;
    while (gpio_get_level(CC1101_MISO_GPIO) == 1) {
        if (waited >= timeout_us) {
            return false;
        }
        esp_rom_delay_us(1);
        waited++;
    }
    return true;
}

/**
 * @brief Initialize SPI for CC1101 communication
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
static esp_err_t init_spi(void)
{
    esp_err_t ret;

    gpio_config_t cs_cfg = {
        .pin_bit_mask = 1ULL << CC1101_CS_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cs_cfg);
    cc1101_chip_deselect();

    // Configuration for the SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = CC1101_MOSI_GPIO,
        .miso_io_num = CC1101_MISO_GPIO,
        .sclk_io_num = CC1101_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = CC1101_SPI_MAX_TRANSFER,  // Allow header + FIFO reads
    };

    // Configuration for the SPI device
    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .mode = CC1101_SPI_MODE,
        .clock_speed_hz = CC1101_SPI_CLOCK_SPEED_HZ,
        .spics_io_num = -1,
        .queue_size = 7,
        .flags = SPI_DEVICE_NO_DUMMY,  // CC1101 doesn't need dummy cycles
    };

    // Initialize the SPI bus
    ret = spi_bus_initialize(CC1101_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Attach the CC1101 to the SPI bus
    ret = spi_bus_add_device(CC1101_SPI_HOST, &devcfg, &cc1101_spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add CC1101 device to SPI bus: %s", esp_err_to_name(ret));
        spi_bus_free(CC1101_SPI_HOST);
        return ret;
    }

    ESP_LOGD(TAG, "SPI initialized successfully for CC1101");
    return ESP_OK;
}

static void IRAM_ATTR cc1101_gdo_isr(void *arg)
{
    uint32_t event = (uint32_t)arg;
    BaseType_t hp_task_woken = pdFALSE;
    if (cc1101_gdo_event_queue) {
        xQueueSendFromISR(cc1101_gdo_event_queue, &event, &hp_task_woken);
    }
    if (hp_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static esp_err_t cc1101_prepare_gdo_resources(void)
{
    gpio_config_t gdo_cfg = {
        .pin_bit_mask = (1ULL << CC1101_GDO0_GPIO) | (1ULL << CC1101_GDO2_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&gdo_cfg);

    if (!cc1101_gdo_event_queue) {
        cc1101_gdo_event_queue = xQueueCreate(32, sizeof(uint32_t));
        if (!cc1101_gdo_event_queue) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (!cc1101_isr_service_installed) {
        esp_err_t ret = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            return ret;
        }
        cc1101_isr_service_installed = true;
    }

    if (!cc1101_gdo_handlers_attached) {
        gpio_isr_handler_add(CC1101_GDO0_GPIO, cc1101_gdo_isr, (void *)CC1101_GDO_EVENT_FIFO);
        gpio_isr_handler_add(CC1101_GDO2_GPIO, cc1101_gdo_isr, (void *)CC1101_GDO_EVENT_PACKET);
        cc1101_gdo_handlers_attached = true;
    }

    gpio_intr_disable(CC1101_GDO0_GPIO);
    gpio_intr_disable(CC1101_GDO2_GPIO);
    return ESP_OK;
}

static inline void cc1101_enable_gdo_interrupts(void)
{
    if (cc1101_gdo_event_queue) {
        xQueueReset(cc1101_gdo_event_queue);
    }
    gpio_set_intr_type(CC1101_GDO0_GPIO, GPIO_INTR_POSEDGE);
    gpio_set_intr_type(CC1101_GDO2_GPIO, GPIO_INTR_NEGEDGE);
    gpio_intr_enable(CC1101_GDO0_GPIO);
    gpio_intr_enable(CC1101_GDO2_GPIO);
}

static inline void cc1101_disable_gdo_interrupts(void)
{
    gpio_intr_disable(CC1101_GDO0_GPIO);
    gpio_intr_disable(CC1101_GDO2_GPIO);
}

static void cc1101_reset_rx_context(uint8_t *buffer, size_t buffer_size)
{
    memset(&cc1101_rx_ctx, 0, sizeof(cc1101_rx_ctx));
    cc1101_rx_ctx.buffer = buffer;
    cc1101_rx_ctx.buffer_size = buffer_size;
}

static void cc1101_process_gdo_event(cc1101_gdo_event_t event)
{
    if (!cc1101_rx_session_active || cc1101_rx_ctx.error) {
        return;
    }

    switch (event) {
        case CC1101_GDO_EVENT_FIFO:
            cc1101_process_fifo_event();
            break;
        case CC1101_GDO_EVENT_PACKET:
            cc1101_process_packet_event();
            break;
        default:
            break;
    }
}

/**
 * @brief Build the 7-bit address byte that includes the burst flag
 */
static inline uint8_t cc1101_build_header(uint8_t reg_addr, bool read, bool burst)
{
    uint8_t header = reg_addr & 0x3F;
    if (read) {
        header |= CC1101_SPI_READ_FLAG;
    }
    if (burst) {
        header |= CC1101_SPI_BURST_FLAG;
    }
    return header;
}

/**
 * @brief Send SPI command to CC1101
 * 
 * @param header Command/register header byte
 * @param data_out Data to write (for write operations)
 * @param data_in Buffer to store read data (for read operations)
 * @param len Number of bytes to transfer
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
static esp_err_t cc1101_spi_transfer(uint8_t header, const uint8_t *data_out, uint8_t *data_in, size_t len)
{
    if (!cc1101_spi_handle) {
        ESP_LOGE(TAG, "SPI handle not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (len + 1 > CC1101_SPI_MAX_TRANSFER) {
        ESP_LOGE(TAG, "SPI transfer length %d exceeds configured maximum", (int)(len + 1));
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t tx_buffer[CC1101_SPI_MAX_TRANSFER] = {0};
    uint8_t rx_buffer[CC1101_SPI_MAX_TRANSFER] = {0};

    tx_buffer[0] = header;
    if (data_out && len > 0) {
        memcpy(&tx_buffer[1], data_out, len);
    }

    spi_transaction_t t = {
        .flags = 0,
        .cmd = 0,
        .addr = 0,
        .length = (len + 1) * 8,
        .tx_buffer = tx_buffer,
        .rx_buffer = data_in ? rx_buffer : NULL,
    };

    spi_device_acquire_bus(cc1101_spi_handle, portMAX_DELAY);
    cc1101_chip_select();
    if (!cc1101_wait_for_miso_low(CC1101_SPI_READY_TIMEOUT_US)) {
        ESP_LOGW(TAG, "Timeout waiting for CC1101 ready after CS assert");
    }

    esp_err_t ret = spi_device_polling_transmit(cc1101_spi_handle, &t);
    cc1101_chip_deselect();
    spi_device_release_bus(cc1101_spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI transfer failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (data_in && len > 0) {
        memcpy(data_in, &rx_buffer[1], len);
    }

    return ESP_OK;
}

static void cc1101_process_fifo_event(void)
{
    if (!cc1101_rx_session_active || cc1101_rx_ctx.complete || cc1101_rx_ctx.error) {
        return;
    }

    while (true) {
        uint8_t rxbytes = cc1101_read_register(CC1101_RXBYTES);
        uint8_t available = rxbytes & 0x7F;
        if (available == 0) {
            break;
        }

        size_t chunk = available;
        if (!cc1101_rx_ctx.expected_len_known && cc1101_rx_ctx.bytes_read < WMBUS_HEADER_ENCODED_LEN) {
            size_t needed = WMBUS_HEADER_ENCODED_LEN - cc1101_rx_ctx.bytes_read;
            if (chunk > needed) {
                chunk = needed;
            }
        } else if (cc1101_rx_ctx.expected_len_known) {
            if (cc1101_rx_ctx.bytes_read >= cc1101_rx_ctx.expected_len) {
                break;
            }
            size_t remaining = cc1101_rx_ctx.expected_len - cc1101_rx_ctx.bytes_read;
            if (chunk > remaining) {
                chunk = remaining;
            }
        }

        if (chunk > CC1101_RX_SAFE_CHUNK) {
            chunk = CC1101_RX_SAFE_CHUNK;
        }

        if (cc1101_rx_ctx.bytes_read + chunk > cc1101_rx_ctx.buffer_size) {
            chunk = cc1101_rx_ctx.buffer_size - cc1101_rx_ctx.bytes_read;
            cc1101_rx_ctx.truncated = true;
        }

        if (chunk == 0) {
            break;
        }

        if (cc1101_spi_transfer(cc1101_build_header(CC1101_RXFIFO, true, true),
                                NULL,
                                cc1101_rx_ctx.buffer + cc1101_rx_ctx.bytes_read,
                                chunk) != ESP_OK) {
            cc1101_rx_ctx.error = true;
            return;
        }
        cc1101_rx_ctx.bytes_read += chunk;

        if (!cc1101_rx_ctx.expected_len_known && cc1101_rx_ctx.bytes_read >= WMBUS_HEADER_ENCODED_LEN) {
            size_t computed_encoded = 0;
            if (!wmbus_calculate_expected_encoded_length(cc1101_rx_ctx.buffer, &computed_encoded)) {
                ESP_LOGD(TAG, "Invalid wM-Bus header, discarding frame");
                cc1101_flush_rx_fifo();
                cc1101_rx_ctx.bytes_read = 0;
                cc1101_rx_ctx.expected_len_known = false;
                cc1101_rx_ctx.truncated = false;
                cc1101_write_register(CC1101_FIFOTHR, CC1101_RX_FIFO_START_THRESHOLD);
                continue;
            }
            cc1101_rx_ctx.original_expected_len = computed_encoded;
            cc1101_rx_ctx.expected_len = computed_encoded;
            if (cc1101_rx_ctx.expected_len > cc1101_rx_ctx.buffer_size) {
                cc1101_rx_ctx.expected_len = cc1101_rx_ctx.buffer_size;
                cc1101_rx_ctx.truncated = true;
            }
            cc1101_rx_ctx.expected_len_known = true;
            cc1101_write_register(CC1101_FIFOTHR, CC1101_RX_FIFO_ACTIVE_THRESHOLD);
        }

        if (cc1101_rx_ctx.expected_len_known && cc1101_rx_ctx.bytes_read >= cc1101_rx_ctx.expected_len) {
            cc1101_rx_ctx.complete = true;
            break;
        }

        if (chunk < available) {
            continue;
        }
    }

    if (cc1101_rx_ctx.packet_done && cc1101_rx_ctx.expected_len_known &&
        cc1101_rx_ctx.bytes_read >= cc1101_rx_ctx.expected_len) {
        cc1101_rx_ctx.complete = true;
    }
}

static void cc1101_process_packet_event(void)
{
    if (!cc1101_rx_session_active) {
        return;
    }
    cc1101_rx_ctx.packet_done = true;
    cc1101_process_fifo_event();
}

static void cc1101_discard_remaining_fifo(size_t remaining)
{
    uint8_t discard[CC1101_FIFO_SIZE];
    while (remaining > 0) {
        uint8_t avail = cc1101_read_register(CC1101_RXBYTES) & 0x7F;
        if (avail == 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        size_t drop = remaining;
        if (drop > avail) {
            drop = avail;
        }
        if (drop > CC1101_FIFO_SIZE) {
            drop = CC1101_FIFO_SIZE;
        }
        cc1101_spi_transfer(cc1101_build_header(CC1101_RXFIFO, true, true),
                            NULL, discard, drop);
        remaining -= drop;
    }
}

static esp_err_t cc1101_flush_rx_fifo(void)
{
    esp_err_t ret = cc1101_send_command_strobe(CC1101_SIDLE);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = cc1101_send_command_strobe(CC1101_SFRX);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = cc1101_send_command_strobe(CC1101_SRX);
    if (ret == ESP_OK) {
        cc1101_trace_marcstate_sequence("srx-after-flush", 8, 200);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ret;
}

static bool wmbus_calculate_expected_encoded_length(const uint8_t *header, size_t *encoded_len_out)
{
    if (!header || !encoded_len_out) {
        return false;
    }

    uint8_t decoded[8];
    uint8_t decoded_len = 0;
    wmbus_error_t res = decode_three_of_six(header, WMBUS_HEADER_ENCODED_LEN, decoded, &decoded_len);
    if (res != WMBUS_SUCCESS || decoded_len < 2) {
        return false;
    }

    if (decoded[0] != decoded[1]) {
        return false;
    }

    uint16_t length_field = decoded[0];
    size_t decoded_total = length_field + 1 /*L byte*/ + 2 /*CRC*/;
    if (decoded_total < 4) {
        decoded_total = 4;
    }
    size_t encoded_total = (decoded_total * 12 + 7) / 8;
    if (encoded_total < WMBUS_HEADER_ENCODED_LEN) {
        encoded_total = WMBUS_HEADER_ENCODED_LEN;
    }
    *encoded_len_out = encoded_total;
    return true;
}

esp_err_t initialize_cc1101(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing CC1101 radio module");

    // Initialize SPI
    ret = init_spi();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI");
        return ret;
    }

    ret = cc1101_prepare_gdo_resources();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to prepare GDO resources: %s", esp_err_to_name(ret));
        return ret;
    }

    // Reset the CC1101
    ret = cc1101_send_command_strobe(CC1101_SRES);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset CC1101");
        return ret;
    }

    // Small delay after reset
    vTaskDelay(pdMS_TO_TICKS(10));

    // Verify that CC1101 is responding by reading the version register
    uint8_t version = cc1101_read_register(CC1101_VERSION);
    if (version == 0x00 || version == 0xFF) {
        ESP_LOGE(TAG, "CC1101 not responding, version register: 0x%02X", version);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGD(TAG, "CC1101 found, version: 0x%02X", version);

    // Configure with default wM-Bus settings
    ret = configure_rf_parameters(&default_wmbus_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure RF parameters");
        return ret;
    }

    // Set to RX mode by default
    ret = set_cc1101_mode(CC1101_MODE_RX);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set RX mode");
        return ret;
    }

    ESP_LOGD(TAG, "CC1101 initialized successfully");
    return ESP_OK;
}

esp_err_t configure_rf_parameters(const cc1101_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Configuration pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Configuring RF parameters for %.2f MHz, %"PRIu32" bps",
             config->frequency_mhz, config->data_rate_bps);

    // Calculate frequency registers (868.95 MHz)
    // Formula: FREQ[23:0] = freq_carrier * 2^16 / f_xtal
    // With 26 MHz crystal: FREQ = 868.95 * 2^16 / 26 = 0x6D4353
    uint32_t freq_value = (uint32_t)(config->frequency_mhz * 65536.0 / 26.0);
    uint8_t freq2 = (freq_value >> 16) & 0xFF;
    uint8_t freq1 = (freq_value >> 8) & 0xFF;
    uint8_t freq0 = freq_value & 0xFF;

    typedef struct {
        uint8_t reg;
        uint8_t value;
    } cc1101_reg_setting_t;

    const cc1101_reg_setting_t settings[] = {
        {CC1101_IOCFG2, 0x06},
        {CC1101_IOCFG1, 0x2E},
        {CC1101_IOCFG0, 0x00},
        {CC1101_FIFOTHR, 0x07},
        {CC1101_SYNC1, 0x54},
        {CC1101_SYNC0, 0x3D},
        {CC1101_PKTLEN, 0xFF},
        {CC1101_PKTCTRL1, 0x00},
        {CC1101_PKTCTRL0, 0x00},
        {CC1101_ADDR, 0x00},
        {CC1101_CHANNR, 0x00},
        {CC1101_FSCTRL1, 0x08},
        {CC1101_FSCTRL0, 0x00},
        {CC1101_FREQ2, freq2},
        {CC1101_FREQ1, freq1},
        {CC1101_FREQ0, freq0},
        {CC1101_MDMCFG4, 0x5C},
        {CC1101_MDMCFG3, 0x04},
        {CC1101_MDMCFG2, 0x05},
        {CC1101_MDMCFG1, 0x22},
        {CC1101_MDMCFG0, 0xF8},
        {CC1101_DEVIATN, 0x44},
        {CC1101_MCSM2, 0x07},
        {CC1101_MCSM1, 0x00},
        {CC1101_MCSM0, 0x18},
        {CC1101_FOCCFG, 0x2E},
        {CC1101_BSCFG, 0xBF},
        {CC1101_AGCCTRL2, 0x43},
        {CC1101_AGCCTRL1, 0x09},
        {CC1101_AGCCTRL0, 0xB5},
        {CC1101_WOREVT1, 0x87},
        {CC1101_WOREVT0, 0x6B},
        {CC1101_WORCTRL, 0xFB},
        {CC1101_FREND1, 0xB6},
        {CC1101_FREND0, 0x10},
        {CC1101_FSCAL3, 0xEA},
        {CC1101_FSCAL2, 0x2A},
        {CC1101_FSCAL1, 0x00},
        {CC1101_FSCAL0, 0x1F},
        {CC1101_RCCTRL1, 0x41},
        {CC1101_RCCTRL0, 0x00},
        {CC1101_FSTEST, 0x59},
        {CC1101_PTEST, 0x7F},
        {CC1101_AGCTEST, 0x3F},
        {CC1101_TEST2, 0x81},
        {CC1101_TEST1, 0x35},
        {CC1101_TEST0, 0x09},
    };

    for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
        esp_err_t ret = cc1101_write_register(settings[i].reg, settings[i].value);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write CC1101 register 0x%02X", settings[i].reg);
            return ret;
        }
    }

    esp_err_t ret = cc1101_send_command_strobe(CC1101_SCAL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to calibrate frequency synthesizer");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(1));

    cc1101_log_rf_registers("configure-rf");
    ESP_LOGD(TAG, "RF parameters configured successfully");
    return ESP_OK;
}

esp_err_t receive_packet(uint8_t *buffer, size_t buffer_size, size_t *packet_size, uint32_t timeout_ms)
{
    if (!buffer || !packet_size) {
        ESP_LOGE(TAG, "Invalid buffer or packet_size pointer");
        return ESP_ERR_INVALID_ARG;
    }

    if (buffer_size == 0) {
        ESP_LOGE(TAG, "Buffer size is zero");
        return ESP_ERR_INVALID_ARG;
    }

    if (!cc1101_gdo_event_queue) {
        ESP_LOGE(TAG, "GDO event queue not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    cc1101_reset_rx_context(buffer, buffer_size);
    *packet_size = 0;

    esp_err_t ret = cc1101_write_register(CC1101_FIFOTHR, CC1101_RX_FIFO_START_THRESHOLD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set start FIFO threshold");
        return ret;
    }

    ret = cc1101_write_register(CC1101_PKTCTRL0, 0x02); // Infinite packet length mode
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable infinite length mode");
        return ret;
    }

    cc1101_enable_gdo_interrupts();
    cc1101_rx_session_active = true;

    ret = cc1101_flush_rx_fifo();
    if (ret != ESP_OK) {
        cc1101_disable_gdo_interrupts();
        cc1101_rx_session_active = false;
        return ret;
    }

    ret = cc1101_send_command_strobe(CC1101_SRX);
    if (ret != ESP_OK) {
        cc1101_disable_gdo_interrupts();
        cc1101_rx_session_active = false;
        return ret;
    }

    TickType_t start_ticks = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    esp_err_t wait_status = ESP_ERR_TIMEOUT;

    while (true) {
        if (cc1101_rx_ctx.complete || cc1101_rx_ctx.error) {
            wait_status = ESP_OK;
            break;
        }

        TickType_t elapsed = xTaskGetTickCount() - start_ticks;
        if (elapsed >= timeout_ticks) {
            break;
        }

        TickType_t remaining = timeout_ticks - elapsed;
        uint32_t event = 0;
        if (xQueueReceive(cc1101_gdo_event_queue, &event, remaining) == pdTRUE) {
            cc1101_process_gdo_event((cc1101_gdo_event_t)event);
            while (uxQueueMessagesWaiting(cc1101_gdo_event_queue) > 0) {
                if (xQueueReceive(cc1101_gdo_event_queue, &event, 0) == pdTRUE) {
                    cc1101_process_gdo_event((cc1101_gdo_event_t)event);
                } else {
                    break;
                }
            }
        } else {
            break;
        }
    }

    cc1101_disable_gdo_interrupts();
    cc1101_rx_session_active = false;

    if (cc1101_rx_ctx.error) {
        ESP_LOGE(TAG, "RX session aborted due to error");
        return ESP_FAIL;
    }

    if (!cc1101_rx_ctx.complete) {
        ESP_LOGD(TAG, "Timeout waiting for packet (timeout: %"PRIu32" ms)", timeout_ms);
        return wait_status;
    }

    if (cc1101_rx_ctx.truncated && cc1101_rx_ctx.original_expected_len > cc1101_rx_ctx.expected_len) {
        size_t remaining = cc1101_rx_ctx.original_expected_len - cc1101_rx_ctx.expected_len;
        cc1101_discard_remaining_fifo(remaining);
    }

    *packet_size = cc1101_rx_ctx.bytes_read;
    ESP_LOGD(TAG, "Received packet of %d bytes", (int)*packet_size);
    return ESP_OK;
}

esp_err_t set_cc1101_mode(cc1101_mode_t mode)
{
    esp_err_t ret = ESP_OK;

    switch (mode) {
        case CC1101_MODE_IDLE:
            ret = cc1101_send_command_strobe(CC1101_SIDLE);
            ESP_LOGI(TAG, "Set CC1101 to IDLE mode");
            break;
            
        case CC1101_MODE_RX:
            // Flush RX FIFO first
            ret = cc1101_send_command_strobe(CC1101_SFRX);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to flush RX FIFO");
                return ret;
            }
            // Enter RX mode
            ret = cc1101_send_command_strobe(CC1101_SRX);
            ESP_LOGI(TAG, "Set CC1101 to RX mode");
            if (ret == ESP_OK) {
                cc1101_trace_marcstate_sequence("srx-after-mode-set", 8, 200);
            }
            break;
            
        case CC1101_MODE_TX:
            ret = cc1101_send_command_strobe(CC1101_STX);
            ESP_LOGI(TAG, "Set CC1101 to TX mode");
            break;
            
        case CC1101_MODE_FSTXON:
            ret = cc1101_send_command_strobe(CC1101_SFSTXON);
            ESP_LOGI(TAG, "Set CC1101 to FSTXON mode");
            break;
            
        case CC1101_MODE_CALIBRATE:
            ret = cc1101_send_command_strobe(CC1101_SCAL);
            ESP_LOGI(TAG, "Set CC1101 to CALIBRATE mode");
            break;
            
        case CC1101_MODE_POWER_DOWN:
            ret = cc1101_send_command_strobe(CC1101_SPWD);
            ESP_LOGI(TAG, "Set CC1101 to POWER DOWN mode");
            break;
            
        default:
            ESP_LOGE(TAG, "Invalid mode: %d", mode);
            return ESP_ERR_INVALID_ARG;
    }

    if (ret == ESP_OK) {
        // Small delay after mode change for stability
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return ret;
}

uint8_t cc1101_read_register(uint8_t reg_addr)
{
    uint8_t value = 0;

    esp_err_t ret = cc1101_spi_transfer(cc1101_build_header(reg_addr, true, false),
                                        NULL, &value, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read register 0x%02X: %s", reg_addr, esp_err_to_name(ret));
    }

    return value;
}

esp_err_t cc1101_write_register(uint8_t reg_addr, uint8_t value)
{
    esp_err_t ret = cc1101_spi_transfer(cc1101_build_header(reg_addr, false, false),
                                        &value, NULL, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write register 0x%02X with value 0x%02X: %s",
                 reg_addr, value, esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t cc1101_send_command_strobe(uint8_t strobe_cmd)
{
    // Synchronize strobe commands have address in range 0x30-0x3D
    if (strobe_cmd < 0x30 || strobe_cmd > 0x3D) {
        ESP_LOGE(TAG, "Invalid strobe command: 0x%02X", strobe_cmd);
        return ESP_ERR_INVALID_ARG;
    }

    return cc1101_spi_transfer(strobe_cmd, NULL, NULL, 0);
}

int8_t cc1101_read_rssi(void)
{
    uint8_t raw_rssi = cc1101_read_register(CC1101_RSSI);
    
    // Convert raw RSSI to dBm
    // Formula from CC1101 datasheet: RSSI_dBm = (RSSI - 256)/2 if RSSI >= 128
    //                                RSSI_dBm = RSSI/2 - 74 if RSSI < 128
    if (raw_rssi >= 128) {
        return (int8_t)((int16_t)raw_rssi - 256) / 2;
    } else {
        return (int8_t)raw_rssi / 2 - 74;
    }
}

uint8_t cc1101_read_lqi(void)
{
    uint8_t lqi = cc1101_read_register(CC1101_LQI);
    return lqi & 0x7F;  // LQI is in the lower 7 bits
}
