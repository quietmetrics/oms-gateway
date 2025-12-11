#include "app/net/wifi.h"

#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi_types.h"
#include "esp_mac.h"
#include "nvs.h"
#include "app/storage.h"

static const char *TAG = "wifi";
static const char *NAMESPACE = "wifi";
static const char *KEY_SSID = "ssid";
static const char *KEY_PASS = "pass";
static const char *KEY_AP_SSID = "ap_ssid";
static const char *KEY_AP_PASS = "ap_pass";
static const char *KEY_AP_CH = "ap_ch";
static const char *KEY_HOST = "hostname";
static bool s_connected = false;
static esp_netif_t *s_netif = NULL;
static esp_netif_t *s_netif_ap = NULL;
static bool s_wifi_inited = false;
static char s_hostname[32] = {0};

static void apply_hostname_to_netif(esp_netif_t *netif)
{
    if (netif && s_hostname[0])
    {
        esp_netif_set_hostname(netif, s_hostname);
    }
}

static esp_err_t hostname_save(const char *hostname)
{
    return storage_set_str(NAMESPACE, KEY_HOST, hostname);
}

static esp_err_t hostname_generate_default(char *out, size_t out_len)
{
    if (!out || out_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t mac[6] = {0};
    esp_err_t err = esp_efuse_mac_get_default(mac);
    if (err != ESP_OK)
    {
        return err;
    }
    int written = snprintf(out, out_len, "OMS-Gateway-%02X%02X%02X", mac[3], mac[4], mac[5]);
    if (written <= 0 || (size_t)written >= out_len)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t wifi_hostname_init(void)
{
    esp_err_t err = storage_get_str(NAMESPACE, KEY_HOST, s_hostname, sizeof(s_hostname));
    if (err != ESP_OK || s_hostname[0] == '\0')
    {
        s_hostname[0] = '\0';
    }

    if (s_hostname[0] == '\0')
    {
        err = hostname_generate_default(s_hostname, sizeof(s_hostname));
        if (err != ESP_OK)
        {
            return err;
        }
        err = hostname_save(s_hostname);
        if (err != ESP_OK)
        {
            return err;
        }
    }
    apply_hostname_to_netif(s_netif);
    apply_hostname_to_netif(s_netif_ap);
    return ESP_OK;
}

esp_err_t wifi_hostname_set(const char *hostname)
{
    if (!hostname || hostname[0] == '\0')
    {
        return ESP_ERR_INVALID_ARG;
    }
    size_t len = strnlen(hostname, sizeof(s_hostname));
    if (len >= sizeof(s_hostname))
    {
        return ESP_ERR_INVALID_SIZE;
    }
    memset(s_hostname, 0, sizeof(s_hostname));
    memcpy(s_hostname, hostname, len);
    esp_err_t err = hostname_save(s_hostname);
    apply_hostname_to_netif(s_netif);
    apply_hostname_to_netif(s_netif_ap);
    return err;
}

esp_err_t wifi_hostname_get(char *out, size_t out_len)
{
    if (!out || out_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    size_t len = strnlen(s_hostname, sizeof(s_hostname));
    if (len == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (len >= out_len)
    {
        return ESP_ERR_NO_MEM;
    }
    memset(out, 0, out_len);
    memcpy(out, s_hostname, len);
    return ESP_OK;
}

static esp_err_t ensure_wifi_init(void)
{
    if (s_wifi_inited)
    {
        return ESP_OK;
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err == ESP_OK)
    {
        s_wifi_inited = true;
    }
    return err;
}

static esp_err_t wifi_set_mode_and_start(wifi_mode_t mode)
{
    esp_err_t err = ensure_wifi_init();
    if (err != ESP_OK)
    {
        return err;
    }
    esp_wifi_stop();
    err = esp_wifi_set_mode(mode);
    if (err != ESP_OK)
    {
        return err;
    }
    return esp_wifi_start();
}

esp_err_t wifi_sta_load_credentials(wifi_credentials_t *cred)
{
    if (!cred)
    {
        return ESP_ERR_INVALID_ARG;
    }
    memset(cred, 0, sizeof(*cred));

    if (storage_get_str(NAMESPACE, KEY_SSID, cred->ssid, sizeof(cred->ssid)) == ESP_ERR_NVS_NOT_FOUND)
    {
        cred->ssid[0] = '\0';
    }
    if (storage_get_str(NAMESPACE, KEY_PASS, cred->pass, sizeof(cred->pass)) == ESP_ERR_NVS_NOT_FOUND)
    {
        cred->pass[0] = '\0';
    }
    return ESP_OK;
}

esp_err_t wifi_sta_save_credentials(const wifi_credentials_t *cred)
{
    if (!cred)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = storage_set_str(NAMESPACE, KEY_SSID, cred->ssid);
    if (err == ESP_OK)
    {
        err = storage_set_str(NAMESPACE, KEY_PASS, cred->pass);
    }
    return err;
}

esp_err_t wifi_sta_set_credentials(const char *ssid, const char *pass)
{
    if (!ssid || !pass)
    {
        return ESP_ERR_INVALID_ARG;
    }
    wifi_credentials_t cred = {0};
    strncpy(cred.ssid, ssid, sizeof(cred.ssid) - 1);
    strncpy(cred.pass, pass, sizeof(cred.pass) - 1);
    return wifi_sta_save_credentials(&cred);
}

esp_err_t wifi_ap_load_config(wifi_ap_config_store_t *cfg)
{
    if (!cfg)
    {
        return ESP_ERR_INVALID_ARG;
    }
    memset(cfg, 0, sizeof(*cfg));

    if (storage_get_str(NAMESPACE, KEY_AP_SSID, cfg->ssid, sizeof(cfg->ssid)) == ESP_ERR_NVS_NOT_FOUND)
    {
        cfg->ssid[0] = '\0';
    }
    if (storage_get_str(NAMESPACE, KEY_AP_PASS, cfg->pass, sizeof(cfg->pass)) == ESP_ERR_NVS_NOT_FOUND)
    {
        cfg->pass[0] = '\0';
    }
    if (storage_get_u8(NAMESPACE, KEY_AP_CH, &cfg->channel) != ESP_OK)
    {
        cfg->channel = 0;
    }
    return ESP_OK;
}

esp_err_t wifi_ap_save_config(const wifi_ap_config_store_t *cfg)
{
    if (!cfg)
    {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = storage_set_str(NAMESPACE, KEY_AP_SSID, cfg->ssid);
    if (err == ESP_OK)
    {
        err = storage_set_str(NAMESPACE, KEY_AP_PASS, cfg->pass);
    }
    if (err == ESP_OK)
    {
        err = storage_set_u8(NAMESPACE, KEY_AP_CH, cfg->channel);
    }
    return err;
}

esp_err_t wifi_ap_set_config(const char *ssid, const char *pass, uint8_t channel)
{
    if (!ssid || !pass)
    {
        return ESP_ERR_INVALID_ARG;
    }
    wifi_ap_config_store_t cfg = {0};
    strncpy(cfg.ssid, ssid, sizeof(cfg.ssid) - 1);
    strncpy(cfg.pass, pass, sizeof(cfg.pass) - 1);
    cfg.channel = channel ? channel : 1; // default to channel 1 if not provided
    return wifi_ap_save_config(&cfg);
}

esp_err_t wifi_ap_start_default(const char *hostname)
{
    wifi_ap_config_store_t cfg = {0};
    wifi_ap_load_config(&cfg);
    if (cfg.ssid[0] == '\0')
    {
        const char *src = NULL;
        if (s_hostname[0])
        {
            src = s_hostname;
        }
        else if (hostname && hostname[0])
        {
            src = hostname;
        }
        else
        {
            src = "oms-ap";
        }
        strncpy(cfg.ssid, src, sizeof(cfg.ssid) - 1);
    }
    if (cfg.channel == 0)
    {
        cfg.channel = 1;
    }

    ESP_ERROR_CHECK(ensure_wifi_init());
    if (!s_netif_ap)
    {
        s_netif_ap = esp_netif_create_default_wifi_ap();
    }
    apply_hostname_to_netif(s_netif_ap);

    ESP_ERROR_CHECK(wifi_set_mode_and_start(WIFI_MODE_AP));

    wifi_config_t ap_cfg = {0};
    strncpy((char *)ap_cfg.ap.ssid, cfg.ssid, sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid_len = strlen(cfg.ssid);
    ap_cfg.ap.channel = cfg.channel;
    ap_cfg.ap.max_connection = 4;

    if (cfg.pass[0] == '\0')
    {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }
    else
    {
        strncpy((char *)ap_cfg.ap.password, cfg.pass, sizeof(ap_cfg.ap.password) - 1);
        ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_LOGI(TAG, "AP started SSID=\"%s\" channel=%u auth=%s", cfg.ssid, cfg.channel,
             (ap_cfg.ap.authmode == WIFI_AUTH_OPEN) ? "open" : "wpa2-psk");
    s_connected = false;
    return ESP_OK;
}

esp_err_t wifi_sta_connect(void)
{
    wifi_credentials_t cred = {0};
    esp_err_t load_err = wifi_sta_load_credentials(&cred);
    if (load_err != ESP_OK)
    {
        ESP_LOGW(TAG, "load cred failed: %s", esp_err_to_name(load_err));
        return load_err;
    }
    if (cred.ssid[0] == '\0')
    {
        ESP_LOGW(TAG, "No Wi-Fi credentials stored");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_ERROR_CHECK(ensure_wifi_init());
    if (!s_netif)
    {
        // Assume netif/event loop already initialized by app_main
        s_netif = esp_netif_create_default_wifi_sta();
        apply_hostname_to_netif(s_netif);
    }

    ESP_ERROR_CHECK(wifi_set_mode_and_start(WIFI_MODE_STA));

    wifi_config_t wifi_cfg = {0};
    strncpy((char *)wifi_cfg.sta.ssid, cred.ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, cred.pass, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());
    // Poll for IP (avoid heavy handlers on sys_evt stack)
    esp_netif_ip_info_t ip_info;
    for (int i = 0; i < 50; i++)
    {
        if (esp_netif_get_ip_info(s_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0)
        {
            s_connected = true;
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    s_connected = false;
    ESP_LOGW(TAG, "Wi-Fi connect timeout");
    return ESP_FAIL;
}

bool wifi_sta_is_connected(void)
{
    if (s_netif)
    {
        esp_netif_ip_info_t ip = {0};
        if (esp_netif_get_ip_info(s_netif, &ip) == ESP_OK && ip.ip.addr != 0)
        {
            s_connected = true;
        }
        else
        {
            s_connected = false;
        }
    }
    return s_connected;
}

esp_err_t wifi_get_status(wifi_status_t *out)
{
    if (!out)
    {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->connected = s_connected;

    wifi_config_t cfg = {0};
    if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK)
    {
        strncpy(out->ssid, (const char *)cfg.sta.ssid, sizeof(out->ssid) - 1);
    }
    wifi_credentials_t cred = {0};
    if (wifi_sta_load_credentials(&cred) == ESP_OK)
    {
        out->has_pass = (cred.pass[0] != '\0');
    }

    if (s_netif && s_connected)
    {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(s_netif, &ip) == ESP_OK && ip.ip.addr != 0)
        {
            esp_ip4addr_ntoa(&ip.ip, out->ip, sizeof(out->ip));
            esp_ip4addr_ntoa(&ip.gw, out->gateway, sizeof(out->gateway));
        }
        wifi_ap_record_t ap = {0};
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
        {
            out->rssi = ap.rssi;
        }
        esp_netif_dns_info_t dns_info = {0};
        if (esp_netif_get_dns_info(s_netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK && dns_info.ip.type == ESP_IPADDR_TYPE_V4)
        {
            esp_ip4addr_ntoa(&dns_info.ip.u_addr.ip4, out->dns, sizeof(out->dns));
        }
    }

    return ESP_OK;
}
