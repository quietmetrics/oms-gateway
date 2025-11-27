// CC1101 RX helpers tailored for wM-Bus T-mode
#pragma once

#include "esp_err.h"
#include "radio/cc1101_hal.h"

esp_err_t radio_rx_configure_tmode(cc1101_hal_t *dev);
esp_err_t radio_rx_read_rssi_lqi(cc1101_hal_t *dev, float *rssi_dbm, uint8_t *lqi_raw);
