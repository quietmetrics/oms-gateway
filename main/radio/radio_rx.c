#include "radio/radio_rx.h"

#include "radio/cc1101_regs.h"

esp_err_t radio_rx_configure_tmode(cc1101_hal_t *dev)
{
    if (!dev)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_ERROR_CHECK(cc1101_hal_reset(dev));
    ESP_ERROR_CHECK(cc1101_hal_configure_tmode(dev));
    ESP_ERROR_CHECK(cc1101_hal_load_pa_table(dev, cc1101_tmode_pa_table, sizeof(cc1101_tmode_pa_table)));

    // IDLE after RX/TX
    ESP_ERROR_CHECK(cc1101_hal_write_reg(dev, CC1101_MCSM1, 0x00));

    // T-mode sync word 0x543D
    ESP_ERROR_CHECK(cc1101_hal_write_reg(dev, CC1101_SYNC1, 0x54));
    ESP_ERROR_CHECK(cc1101_hal_write_reg(dev, CC1101_SYNC0, 0x3D));

    // FIFO thresholds: start with 4 bytes (0), can be raised later if needed
    ESP_ERROR_CHECK(cc1101_hal_write_reg(dev, CC1101_FIFOTHR, 0x00));

    // Infinite length mode by default; PKTLEN ignored
    ESP_ERROR_CHECK(cc1101_hal_write_reg(dev, CC1101_PKTCTRL0, 0x02));

    return ESP_OK;
}

esp_err_t radio_rx_read_rssi_lqi(cc1101_hal_t *dev, float *rssi_dbm, uint8_t *lqi_raw)
{
    if (!dev)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t rssi_raw = 0;
    uint8_t lqi = 0;
    ESP_ERROR_CHECK(cc1101_hal_read_reg(dev, CC1101_RSSI, &rssi_raw));
    ESP_ERROR_CHECK(cc1101_hal_read_reg(dev, CC1101_LQI, &lqi));

    int16_t rssi_dec = (rssi_raw >= 128) ? (int16_t)rssi_raw - 256 : rssi_raw;
    if (rssi_dbm)
    {
        *rssi_dbm = ((float)rssi_dec / 2.0f) - 74.0f;
    }
    if (lqi_raw)
    {
        *lqi_raw = lqi;
    }
    return ESP_OK;
}
