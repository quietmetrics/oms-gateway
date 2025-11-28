#include "app/services.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "services";

const char *services_hostname(const services_state_t *svc)
{
    if (svc && svc->hostname[0])
    {
        return svc->hostname;
    }
    return "oms-gateway";
}

esp_err_t services_init(services_state_t *svc)
{
    if (!svc)
    {
        return ESP_ERR_INVALID_ARG;
    }

    memset(svc, 0, sizeof(*svc));

    ESP_ERROR_CHECK(wmbus_whitelist_init(&svc->whitelist));
    ESP_ERROR_CHECK(backend_init(&svc->backend));
    ESP_ERROR_CHECK(radio_config_init(&svc->radio));

    esp_err_t err = wifi_hostname_init();
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "hostname init failed: %s", esp_err_to_name(err));
    }
    if (wifi_hostname_get(svc->hostname, sizeof(svc->hostname)) != ESP_OK)
    {
        strncpy(svc->hostname, "oms-gateway", sizeof(svc->hostname) - 1);
    }

    return ESP_OK;
}

esp_err_t services_set_backend_url(services_state_t *svc, const char *url)
{
    return backend_set_url(services_backend(svc), url);
}

esp_err_t services_get_backend_url(const services_state_t *svc, char *out, size_t out_len)
{
    return backend_get_url(services_backend((services_state_t *)svc), out, out_len);
}

esp_err_t services_set_wifi_credentials(services_state_t *svc, const char *ssid, const char *pass)
{
    (void)svc;
    return wifi_sta_set_credentials(ssid, pass);
}

esp_err_t services_get_wifi_status(app_wifi_status_t *out)
{
    if (!out)
    {
        return ESP_ERR_INVALID_ARG;
    }
    wifi_status_t wifi = {0};
    esp_err_t err = wifi_get_status(&wifi);
    if (err != ESP_OK)
    {
        return err;
    }
    memset(out, 0, sizeof(*out));
    out->connected = wifi.connected;
    strncpy(out->ssid, wifi.ssid, sizeof(out->ssid) - 1);
    strncpy(out->ip, wifi.ip, sizeof(out->ip) - 1);
    return ESP_OK;
}

esp_err_t services_set_ap_config(services_state_t *svc, const char *ssid, const char *pass, uint8_t channel)
{
    (void)svc;
    return wifi_ap_set_config(ssid, pass, channel);
}

esp_err_t services_set_hostname(services_state_t *svc, const char *hostname)
{
    esp_err_t err = wifi_hostname_set(hostname);
    if (err == ESP_OK && svc)
    {
        memset(svc->hostname, 0, sizeof(svc->hostname));
        strncpy(svc->hostname, hostname, sizeof(svc->hostname) - 1);
    }
    return err;
}

esp_err_t services_get_hostname(char *out, size_t out_len)
{
    return wifi_hostname_get(out, out_len);
}

esp_err_t services_set_radio_cs_level(services_state_t *svc, uint8_t level)
{
    return radio_config_set_cs_level(services_radio(svc), (cc1101_cs_level_t)level);
}

esp_err_t services_set_radio_sync_mode(services_state_t *svc, uint8_t mode)
{
    return radio_config_set_sync_mode(services_radio(svc), (cc1101_sync_mode_t)mode);
}

esp_err_t services_get_radio_status(const services_state_t *svc, app_radio_status_t *out)
{
    if (!svc || !out)
    {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->cs_level = (uint8_t)svc->radio.cs_level;
    out->sync_mode = (uint8_t)svc->radio.sync_mode;
    return ESP_OK;
}
