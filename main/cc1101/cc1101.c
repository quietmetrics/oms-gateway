#include "cc1101.h"
#include "esp_log.h"
#include "esp_check.h"
#include <string.h>
#include <stdlib.h>

#define TAG "cc1101"

static inline void lock(cc1101_handle_t *h) { if (h->lock) xSemaphoreTake(h->lock, portMAX_DELAY); }
static inline void unlock(cc1101_handle_t *h) { if (h->lock) xSemaphoreGive(h->lock); }

static esp_err_t write_cmd(cc1101_handle_t *h, uint8_t cmd)
{
    uint8_t tx = cmd;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &tx,
    };
    lock(h);
    esp_err_t err = spi_device_polling_transmit(h->spi, &t);
    unlock(h);
    return err;
}

esp_err_t cc1101_write_reg(cc1101_handle_t *h, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = buf,
    };
    lock(h);
    esp_err_t err = spi_device_polling_transmit(h->spi, &t);
    unlock(h);
    return err;
}

esp_err_t cc1101_read_reg(cc1101_handle_t *h, uint8_t reg, uint8_t *out)
{
    uint8_t tx[2] = {(uint8_t)(reg | CC1101_READ_SINGLE), 0x00};
    uint8_t rx[2] = {0};
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    lock(h);
    esp_err_t err = spi_device_polling_transmit(h->spi, &t);
    unlock(h);
    if (err == ESP_OK) {
        *out = rx[1];
    }
    return err;
}

esp_err_t cc1101_read_burst(cc1101_handle_t *h, uint8_t reg, uint8_t *buf, size_t len)
{
    uint8_t addr = reg | CC1101_READ_BURST;
    spi_transaction_t t = {
        .flags = 0,
        .length = (len + 1) * 8,
        .tx_buffer = NULL,
        .rx_buffer = NULL,
    };
    uint8_t *tx = malloc(len + 1);
    uint8_t *rx = malloc(len + 1);
    if (!tx || !rx) {
        free(tx);
        free(rx);
        return ESP_ERR_NO_MEM;
    }
    tx[0] = addr;
    memset(tx + 1, 0, len);
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    lock(h);
    esp_err_t err = spi_device_polling_transmit(h->spi, &t);
    unlock(h);
    if (err == ESP_OK) {
        memcpy(buf, rx + 1, len);
    }
    free(tx);
    free(rx);
    return err;
}

esp_err_t cc1101_strobe(cc1101_handle_t *h, uint8_t cmd)
{
    return write_cmd(h, cmd);
}

esp_err_t cc1101_init(const cc1101_pins_t *pins, cc1101_handle_t *out)
{
    memset(out, 0, sizeof(*out));
    out->pins = *pins;
    out->lock = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "Init bus: MISO=%d MOSI=%d SCLK=%d CS=%d host=%d", pins->pin_miso, pins->pin_mosi, pins->pin_sclk, pins->pin_cs, pins->host);

    spi_bus_config_t buscfg = {
        .mosi_io_num = pins->pin_mosi,
        .miso_io_num = pins->pin_miso,
        .sclk_io_num = pins->pin_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(pins->host, &buscfg, SPI_DMA_CH_AUTO), TAG, "bus init");

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = pins->pin_cs,
        .queue_size = 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(pins->host, &devcfg, &out->spi), TAG, "add dev");

    // Reset chip
    ESP_RETURN_ON_ERROR(write_cmd(out, CC1101_SRES), TAG, "reset");
    ESP_LOGI(TAG, "Reset complete, SPI device added");
    return ESP_OK;
}

esp_err_t cc1101_get_status(cc1101_handle_t *handle, uint8_t *status_out)
{
    if (!handle || !status_out) return ESP_ERR_INVALID_ARG;
    uint8_t tx = CC1101_SNOP;
    uint8_t rx = 0;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &tx,
        .rx_buffer = &rx,
    };
    lock(handle);
    esp_err_t err = spi_device_polling_transmit(handle->spi, &t);
    unlock(handle);
    if (err == ESP_OK) {
        *status_out = rx;
    }
    return err;
}

esp_err_t cc1101_init_registers(cc1101_handle_t *h, const cc1101_filter_cfg_t *filter_cfg)
{
    cc1101_filter_cfg_t cfg = filter_cfg ? *filter_cfg : CC1101_FILTER_CFG_DEFAULT;
    ESP_LOGD(TAG, "Configuring registers for T-mode OMS");
    ESP_LOGI(TAG, "Filter cfg: MDMCFG4=0x%02X MDMCFG2=0x%02X PKTCTRL1=0x%02X AGCCTRL2=0x%02X AGCCTRL1=0x%02X",
             cfg.mdmcfg4, cfg.mdmcfg2, cfg.pktctrl1, cfg.agcctrl2, cfg.agcctrl1);
    // Register values taken from Example wM Bus Library
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_IOCFG2, 0x06), TAG, "IOCFG2");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_IOCFG1, 0x2E), TAG, "IOCFG1");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_IOCFG0, 0x00), TAG, "IOCFG0");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FIFOTHR, 0x07), TAG, "FIFOTHR");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_SYNC1, 0x54), TAG, "SYNC1");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_SYNC0, 0x3D), TAG, "SYNC0");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_PKTLEN, 0xFF), TAG, "PKTLEN");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_PKTCTRL1, cfg.pktctrl1), TAG, "PKTCTRL1");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_PKTCTRL0, 0x00), TAG, "PKTCTRL0");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_ADDR, 0x00), TAG, "ADDR");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_CHANNR, 0x00), TAG, "CHANNR");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FSCTRL1, 0x08), TAG, "FSCTRL1");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FSCTRL0, 0x00), TAG, "FSCTRL0");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FREQ2, 0x21), TAG, "FREQ2");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FREQ1, 0x6B), TAG, "FREQ1");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FREQ0, 0xD0), TAG, "FREQ0");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_MDMCFG4, cfg.mdmcfg4), TAG, "MDMCFG4");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_MDMCFG3, 0x04), TAG, "MDMCFG3");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_MDMCFG2, cfg.mdmcfg2), TAG, "MDMCFG2");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_MDMCFG1, 0x22), TAG, "MDMCFG1");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_MDMCFG0, 0xF8), TAG, "MDMCFG0");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_DEVIATN, 0x44), TAG, "DEVIATN");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_MCSM2, 0x07), TAG, "MCSM2");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_MCSM1, 0x00), TAG, "MCSM1");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_MCSM0, 0x18), TAG, "MCSM0");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FOCCFG, 0x2E), TAG, "FOCCFG");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_BSCFG, 0xBF), TAG, "BSCFG");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_AGCCTRL2, cfg.agcctrl2), TAG, "AGCCTRL2");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_AGCCTRL1, cfg.agcctrl1), TAG, "AGCCTRL1");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_AGCCTRL0, 0xB5), TAG, "AGCCTRL0");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_WOREVT1, 0x87), TAG, "WOREVT1");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_WOREVT0, 0x6B), TAG, "WOREVT0");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_WORCTRL, 0xFB), TAG, "WORCTRL");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FREND1, 0xB6), TAG, "FREND1");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FREND0, 0x10), TAG, "FREND0");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FSCAL3, 0xEA), TAG, "FSCAL3");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FSCAL2, 0x2A), TAG, "FSCAL2");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FSCAL1, 0x00), TAG, "FSCAL1");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FSCAL0, 0x1F), TAG, "FSCAL0");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_RCCTRL1, 0x41), TAG, "RCCTRL1");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_RCCTRL0, 0x00), TAG, "RCCTRL0");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_FSTEST, 0x59), TAG, "FSTEST");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_PTEST, 0x7F), TAG, "PTEST");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_AGCTEST, 0x3F), TAG, "AGCTEST");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_TEST2, 0x81), TAG, "TEST2");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_TEST1, 0x35), TAG, "TEST1");
    ESP_RETURN_ON_ERROR(cc1101_write_reg(h, CC1101_TEST0, 0x09), TAG, "TEST0");
    ESP_LOGI(TAG, "Register init complete");
    return ESP_OK;
}

esp_err_t cc1101_set_filter(cc1101_handle_t *h, const cc1101_filter_cfg_t *filter_cfg)
{
    if (!h) return ESP_ERR_INVALID_ARG;
    ESP_LOGI(TAG, "Applying new filter settings");
    return cc1101_init_registers(h, filter_cfg);
}

esp_err_t cc1101_enter_rx(cc1101_handle_t *handle)
{
    ESP_RETURN_ON_ERROR(write_cmd(handle, CC1101_SFRX), TAG, "flush");
    esp_err_t err = write_cmd(handle, CC1101_SRX);
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Entered RX");
    }
    return err;
}

esp_err_t cc1101_idle(cc1101_handle_t *handle)
{
    esp_err_t err = write_cmd(handle, CC1101_SIDLE);
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Entered IDLE");
    }
    return err;
}

esp_err_t cc1101_read_rssi_lqi(cc1101_handle_t *handle, float *rssi_dbm, uint8_t *lqi)
{
    uint8_t rssi_raw = 0;
    uint8_t lqi_raw = 0;
    ESP_RETURN_ON_ERROR(cc1101_read_reg(handle, CC1101_RSSI, &rssi_raw), TAG, "rssi");
    ESP_RETURN_ON_ERROR(cc1101_read_reg(handle, CC1101_LQI, &lqi_raw), TAG, "lqi");

    int16_t rssi_dec = (rssi_raw >= 128) ? (int16_t)(rssi_raw - 256) : (int16_t)rssi_raw;
    *rssi_dbm = ((float)rssi_dec / 2.0f) - 74.0f;
    *lqi = lqi_raw & 0x7F;
    ESP_LOGD(TAG, "RSSI %.1f dBm LQI %u (raw rssi=0x%02X lqi=0x%02X)", *rssi_dbm, *lqi, rssi_raw, lqi_raw);
    return ESP_OK;
}
