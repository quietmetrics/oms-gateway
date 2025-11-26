#include "config_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_check.h"
#include <string.h>

#define NVS_NAMESPACE "oms"

esp_err_t config_manager_init(app_config_t *cfg)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;

    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->hostname, "OMS-Gateway-00000000", sizeof(cfg->hostname) - 1);
    cfg->backend_port = 80;
    cfg->backend_https = false;

    nvs_handle_t h;
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return ESP_OK;
    }
    size_t sz = sizeof(*cfg);
    err = nvs_get_blob(h, "cfg", cfg, &sz);
    nvs_close(h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    return err;
}

esp_err_t config_manager_save(const app_config_t *cfg)
{
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h), "cfg", "open");
    esp_err_t err = nvs_set_blob(h, "cfg", cfg, sizeof(*cfg));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}
