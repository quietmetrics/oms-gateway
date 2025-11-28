#include "app/storage.h"

#include "nvs.h"
#include "nvs_flash.h"

esp_err_t storage_get_str(const char *ns, const char *key, char *out, size_t out_len)
{
    if (!ns || !key || !out || out_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }
    size_t required = out_len;
    err = nvs_get_str(nvs, key, out, &required);
    nvs_close(nvs);
    return err;
}

esp_err_t storage_set_str(const char *ns, const char *key, const char *val)
{
    if (!ns || !key || !val)
    {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_set_str(nvs, key, val);
    if (err == ESP_OK)
    {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t storage_get_u8(const char *ns, const char *key, uint8_t *out)
{
    if (!ns || !key || !out)
    {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }
    size_t len = sizeof(uint8_t);
    err = nvs_get_blob(nvs, key, out, &len);
    nvs_close(nvs);
    return err;
}

esp_err_t storage_set_u8(const char *ns, const char *key, uint8_t val)
{
    if (!ns || !key)
    {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_set_blob(nvs, key, &val, sizeof(val));
    if (err == ESP_OK)
    {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t storage_get_blob(const char *ns, const char *key, void *out, size_t *len_inout)
{
    if (!ns || !key || !out || !len_inout)
    {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_get_blob(nvs, key, out, len_inout);
    nvs_close(nvs);
    return err;
}

esp_err_t storage_set_blob(const char *ns, const char *key, const void *data, size_t len)
{
    if (!ns || !key || !data)
    {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_set_blob(nvs, key, data, len);
    if (err == ESP_OK)
    {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}
