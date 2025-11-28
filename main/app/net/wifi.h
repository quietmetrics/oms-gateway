// Minimal Wi-Fi STA helper with NVS-backed credentials.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

typedef struct
{
    char ssid[32];
    char pass[64];
} wifi_credentials_t;

// Load credentials from NVS; if none found, keep empty.
esp_err_t wifi_sta_load_credentials(wifi_credentials_t *cred);
// Persist credentials to NVS.
esp_err_t wifi_sta_save_credentials(const wifi_credentials_t *cred);
// Set credentials (saves to NVS).
esp_err_t wifi_sta_set_credentials(const char *ssid, const char *pass);
// Initialize Wi-Fi in STA mode and connect using stored credentials.
esp_err_t wifi_sta_connect(void);
bool wifi_sta_is_connected(void);

typedef struct
{
    char ssid[32];
    char pass[64];
    uint8_t channel;
} wifi_ap_config_store_t;

esp_err_t wifi_ap_load_config(wifi_ap_config_store_t *cfg);
esp_err_t wifi_ap_save_config(const wifi_ap_config_store_t *cfg);
esp_err_t wifi_ap_set_config(const char *ssid, const char *pass, uint8_t channel);
esp_err_t wifi_ap_start_default(const char *hostname);

typedef struct
{
    bool connected;
    char ssid[32];
    char ip[16];
    bool has_pass;
    int rssi;
    char gateway[16];
    char dns[16];
} wifi_status_t;

// Read current STA status (connected flag, SSID, IPv4 string if available).
esp_err_t wifi_get_status(wifi_status_t *out);

// Hostname helpers (persisted in NVS, applied to STA/AP netifs when available).
esp_err_t wifi_hostname_init(void); // load or generate default "OMS-Gateway-<CHIPID>"
esp_err_t wifi_hostname_set(const char *hostname);
esp_err_t wifi_hostname_get(char *out, size_t out_len);
