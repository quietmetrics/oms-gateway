// Minimal CC1101 HAL for ESP-IDF, supporting RX-only T-mode pipeline
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "radio/pins.h"
#include "radio/cc1101_regs.h"
#include "radio/rf_config_tmode.h"

typedef struct
{
    spi_device_handle_t spi;
    cc1101_pin_config_t pins;
} cc1101_hal_t;

esp_err_t cc1101_hal_init(const cc1101_pin_config_t *pins, cc1101_hal_t *out);
void cc1101_hal_deinit(cc1101_hal_t *dev);

esp_err_t cc1101_hal_strobe(cc1101_hal_t *dev, uint8_t strobe, uint8_t *status_out);
esp_err_t cc1101_hal_write_reg(cc1101_hal_t *dev, uint8_t addr, uint8_t value);
esp_err_t cc1101_hal_read_reg(cc1101_hal_t *dev, uint8_t addr, uint8_t *value);
esp_err_t cc1101_hal_write_fifo(cc1101_hal_t *dev, const uint8_t *data, size_t len);
esp_err_t cc1101_hal_read_fifo(cc1101_hal_t *dev, uint8_t *data, size_t len);

esp_err_t cc1101_hal_configure_tmode(cc1101_hal_t *dev);
esp_err_t cc1101_hal_load_pa_table(cc1101_hal_t *dev, const uint8_t *table, size_t len);
// Carrier-sense threshold presets (adjust AGCCTRL0 CS bits at runtime)
typedef enum
{
    CC1101_CS_LEVEL_DEFAULT = 0, // default CS bits (AGCCTRL0=0xB5, rel=01 abs=01)
    CC1101_CS_LEVEL_MEDIUM,      // Raises absolute CS threshold (more selective vs noise)
    CC1101_CS_LEVEL_HIGH,        // Highest CS thresholds (most selective, may miss weak signals)
    CC1101_CS_LEVEL_LOW          // Lowers CS threshold (more sensitive, may trigger on noise)
} cc1101_cs_level_t;
esp_err_t cc1101_hal_set_cs_threshold(cc1101_hal_t *dev, cc1101_cs_level_t level);

// Sync mode presets (MDMCFG2.SYNC_MODE) for wM-Bus T-mode
typedef enum
{
    CC1101_SYNC_MODE_DEFAULT = 0, // 15/16 + CS (default)
    CC1101_SYNC_MODE_TIGHT,       // 16/16 + CS
    CC1101_SYNC_MODE_STRICT       // 30/32 + CS (most selective)
} cc1101_sync_mode_t;
esp_err_t cc1101_hal_set_sync_mode(cc1101_hal_t *dev, cc1101_sync_mode_t mode);
static inline esp_err_t cc1101_hal_idle(cc1101_hal_t *dev)
{
    return cc1101_hal_strobe(dev, CC1101_SIDLE, NULL);
}

static inline esp_err_t cc1101_hal_enter_rx(cc1101_hal_t *dev)
{
    return cc1101_hal_strobe(dev, CC1101_SRX, NULL);
}

static inline esp_err_t cc1101_hal_flush_rx(cc1101_hal_t *dev)
{
    return cc1101_hal_strobe(dev, CC1101_SFRX, NULL);
}

static inline esp_err_t cc1101_hal_reset(cc1101_hal_t *dev)
{
    return cc1101_hal_strobe(dev, CC1101_SRES, NULL);
}
