#include "app/radio/radio_config.h"

#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "app/storage.h"

static const char *TAG = "radio_cfg";
static const char *NAMESPACE = "radio";
static const char *KEY_CS = "cs_level";
static const char *KEY_SYNC = "sync_mode";

static bool is_valid_cs(uint8_t v)
{
    return v <= CC1101_CS_LEVEL_LOW;
}

static bool is_valid_sync(uint8_t v)
{
    return v <= CC1101_SYNC_MODE_STRICT;
}

static esp_err_t save_cfg(const radio_config_t *cfg)
{
    esp_err_t err = storage_set_u8(NAMESPACE, KEY_CS, (uint8_t)cfg->cs_level);
    if (err == ESP_OK)
    {
        err = storage_set_u8(NAMESPACE, KEY_SYNC, (uint8_t)cfg->sync_mode);
    }
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "radio cfg save failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t load_cfg(radio_config_t *cfg)
{
    uint8_t cs = 0;
    uint8_t sync = 0;
    esp_err_t err = storage_get_u8(NAMESPACE, KEY_CS, &cs);
    if (err == ESP_OK && is_valid_cs(cs))
    {
        cfg->cs_level = (cc1101_cs_level_t)cs;
    }
    if (err == ESP_OK && storage_get_u8(NAMESPACE, KEY_SYNC, &sync) == ESP_OK && is_valid_sync(sync))
    {
        cfg->sync_mode = (cc1101_sync_mode_t)sync;
    }
    return err;
}

static void set_defaults(radio_config_t *cfg)
{
    cfg->cs_level = CC1101_CS_LEVEL_DEFAULT;
    cfg->sync_mode = CC1101_SYNC_MODE_DEFAULT;
}

esp_err_t radio_config_init(radio_config_t *cfg)
{
    if (!cfg)
    {
        return ESP_ERR_INVALID_ARG;
    }
    set_defaults(cfg);
    esp_err_t err = load_cfg(cfg);
    if (err == ESP_OK)
    {
        return ESP_OK;
    }
    if (err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "load cfg failed: %s", esp_err_to_name(err));
    }
    return save_cfg(cfg);
}

esp_err_t radio_config_set_cs_level(radio_config_t *cfg, cc1101_cs_level_t level)
{
    if (!cfg || !is_valid_cs(level))
    {
        return ESP_ERR_INVALID_ARG;
    }
    cfg->cs_level = level;
    return save_cfg(cfg);
}

esp_err_t radio_config_set_sync_mode(radio_config_t *cfg, cc1101_sync_mode_t mode)
{
    if (!cfg || !is_valid_sync(mode))
    {
        return ESP_ERR_INVALID_ARG;
    }
    cfg->sync_mode = mode;
    return save_cfg(cfg);
}

esp_err_t radio_config_apply(const radio_config_t *cfg, cc1101_hal_t *dev)
{
    if (!cfg || !dev)
    {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = cc1101_hal_set_cs_threshold(dev, cfg->cs_level);
    if (err != ESP_OK)
    {
        return err;
    }
    return cc1101_hal_set_sync_mode(dev, cfg->sync_mode);
}
