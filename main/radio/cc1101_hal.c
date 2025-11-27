#include "radio/cc1101_hal.h"

#include <string.h>
#include <stdbool.h>
#include "esp_log.h"

static const char *TAG = "cc1101_hal";
static bool s_bus_initialized = false;

static esp_err_t cc1101_spi_transfer(cc1101_hal_t *dev, const uint8_t *tx, uint8_t *rx, size_t len_bits)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len_bits;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    return spi_device_polling_transmit(dev->spi, &t);
}

esp_err_t cc1101_hal_init(const cc1101_pin_config_t *pins, cc1101_hal_t *out)
{
    if (!pins || !out)
    {
        return ESP_ERR_INVALID_ARG;
    }

    spi_bus_config_t buscfg = {
        .mosi_io_num = pins->mosi,
        .miso_io_num = pins->miso,
        .sclk_io_num = pins->sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 128,
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 6 * 1000 * 1000, // lower speed to improve margin
        .mode = 0,
        .spics_io_num = pins->cs,
        .queue_size = 2,
        .flags = 0, // full duplex (required for tx+rx in one transaction)
    };

    if (!s_bus_initialized)
    {
        esp_err_t bus_err = spi_bus_initialize(pins->host, &buscfg, SPI_DMA_CH_AUTO);
        if (bus_err != ESP_OK && bus_err != ESP_ERR_INVALID_STATE)
        {
            ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(bus_err));
            return bus_err;
        }
        s_bus_initialized = true;
    }

    esp_err_t dev_err = spi_bus_add_device(pins->host, &devcfg, &out->spi);
    if (dev_err != ESP_OK)
    {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(dev_err));
        return dev_err;
    }

    out->pins = *pins;

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << pins->gdo0) | (1ULL << pins->gdo2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    return ESP_OK;
}

void cc1101_hal_deinit(cc1101_hal_t *dev)
{
    if (!dev || !dev->spi)
    {
        return;
    }
    spi_device_handle_t handle = dev->spi;
    dev->spi = NULL;
    spi_bus_remove_device(handle);
}

esp_err_t cc1101_hal_strobe(cc1101_hal_t *dev, uint8_t strobe, uint8_t *status_out)
{
    if (!dev)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx_data[1] = {strobe};
    uint8_t rx_data[1] = {0};
    esp_err_t err = cc1101_spi_transfer(dev, tx_data, rx_data, 8);
    if (err == ESP_OK && status_out)
    {
        *status_out = rx_data[0];
    }
    return err;
}

esp_err_t cc1101_hal_write_reg(cc1101_hal_t *dev, uint8_t addr, uint8_t value)
{
    if (!dev)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx_data[2] = {addr, value};
    return cc1101_spi_transfer(dev, tx_data, NULL, 16);
}

esp_err_t cc1101_hal_read_reg(cc1101_hal_t *dev, uint8_t addr, uint8_t *value)
{
    if (!dev || !value)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx_data[2] = {addr | CC1101_READ_SINGLE, 0x00};
    uint8_t rx_data[2] = {0};
    esp_err_t err = cc1101_spi_transfer(dev, tx_data, rx_data, 16);
    if (err == ESP_OK)
    {
        *value = rx_data[1];
    }
    return err;
}

esp_err_t cc1101_hal_write_fifo(cc1101_hal_t *dev, const uint8_t *data, size_t len)
{
    if (!dev || !data || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[65]; // FIFO is 64 bytes; header + payload fits
    if (len > 64)
    {
        return ESP_ERR_INVALID_ARG;
    }

    buffer[0] = CC1101_TXFIFO | CC1101_WRITE_BURST;
    memcpy(&buffer[1], data, len);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * (len + 1);
    t.tx_buffer = buffer;
    return spi_device_polling_transmit(dev->spi, &t);
}

esp_err_t cc1101_hal_read_fifo(cc1101_hal_t *dev, uint8_t *data, size_t len)
{
    if (!dev || !data || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[65];
    if (len > 64)
    {
        return ESP_ERR_INVALID_ARG;
    }

    buffer[0] = CC1101_RXFIFO | CC1101_READ_BURST;
    memset(&buffer[1], 0, len);

    uint8_t rx_buffer[65] = {0};

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * (len + 1);
    t.tx_buffer = buffer;
    t.rx_buffer = rx_buffer;
    esp_err_t err = spi_device_polling_transmit(dev->spi, &t);
    if (err == ESP_OK)
    {
        memcpy(data, &rx_buffer[1], len);
    }
    return err;
}

esp_err_t cc1101_hal_configure_tmode(cc1101_hal_t *dev)
{
    if (!dev)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < (sizeof(cc1101_tmode_reg_config) / sizeof(cc1101_tmode_reg_config[0])); i++)
    {
        const cc1101_reg_value_t *reg = &cc1101_tmode_reg_config[i];
        ESP_ERROR_CHECK(cc1101_hal_write_reg(dev, reg->addr, reg->value));
    }

    // Default PKTCTRL0 to infinite length; caller may override.
    ESP_ERROR_CHECK(cc1101_hal_write_reg(dev, CC1101_PKTCTRL0, 0x02));
    return ESP_OK;
}

esp_err_t cc1101_hal_load_pa_table(cc1101_hal_t *dev, const uint8_t *table, size_t len)
{
    if (!dev || !table || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (len > 8)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[9];
    buffer[0] = CC1101_PATABLE | CC1101_WRITE_BURST;
    memcpy(&buffer[1], table, len);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 * (len + 1);
    t.tx_buffer = buffer;
    return spi_device_polling_transmit(dev->spi, &t);
}

esp_err_t cc1101_hal_set_cs_threshold(cc1101_hal_t *dev, cc1101_cs_level_t level)
{
    if (!dev)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Base value from TI T-mode profile (AGCCTRL0 = 0xB5)
    uint8_t base = 0xB5;
    // Modify carrier sense relative/absolute bits (bits 3:0)
    // Mapping (empirical, higher nibble increases threshold):
    // DEFAULT: 0xB5 -> rel=01 abs=01
    // MEDIUM:  0xB7 -> rel=01 abs=11 (higher abs threshold)
    // HIGH:    0xBF -> rel=11 abs=11 (highest thresholds)
    // LOW:     0xB1 -> rel=00 abs=01 (more sensitive)
    uint8_t val = base;
    switch (level)
    {
    case CC1101_CS_LEVEL_MEDIUM:
        val = (base & 0xF0) | 0x07;
        break;
    case CC1101_CS_LEVEL_HIGH:
        val = (base & 0xF0) | 0x0F;
        break;
    case CC1101_CS_LEVEL_LOW:
        val = (base & 0xF0) | 0x01;
        break;
    case CC1101_CS_LEVEL_DEFAULT:
    default:
        val = base;
        break;
    }

    return cc1101_hal_write_reg(dev, CC1101_AGCCTRL0, val);
}

esp_err_t cc1101_hal_set_sync_mode(cc1101_hal_t *dev, cc1101_sync_mode_t mode)
{
    if (!dev)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Base from TI T-mode profile: MDMCFG2 = 0x05 (15/16 + CS)
    uint8_t base = 0x05; // default 15/16 + CS
    uint8_t val = base;
    switch (mode)
    {
    case CC1101_SYNC_MODE_TIGHT:
        // Sync mode 6: 16/16 + CS
        val = (base & 0xF8) | 0x06;
        break;
    case CC1101_SYNC_MODE_STRICT:
        // Sync mode 7: 30/32 sync word bits + carrier sense
        val = (base & 0xF8) | 0x07;
        break;
    case CC1101_SYNC_MODE_DEFAULT:
    default:
        val = base;
        break;
    }
    return cc1101_hal_write_reg(dev, CC1101_MDMCFG2, val);
}
