// Persisted radio configuration helpers (CS threshold + sync mode) for CC1101.
#pragma once

#include "esp_err.h"
#include "radio/cc1101_hal.h"

typedef struct
{
    cc1101_cs_level_t cs_level;
    cc1101_sync_mode_t sync_mode;
} radio_config_t;

// Load config from NVS (or defaults) into cfg.
esp_err_t radio_config_init(radio_config_t *cfg);
// Persist and update CS threshold selection.
esp_err_t radio_config_set_cs_level(radio_config_t *cfg, cc1101_cs_level_t level);
// Persist and update sync mode selection.
esp_err_t radio_config_set_sync_mode(radio_config_t *cfg, cc1101_sync_mode_t mode);
// Apply current config to a CC1101 instance.
esp_err_t radio_config_apply(const radio_config_t *cfg, cc1101_hal_t *dev);
