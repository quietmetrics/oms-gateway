// Centralized services facade for app configuration/state used by Web UI & main loop.
#pragma once

#include <stddef.h>
#include "esp_err.h"
#include "app/wmbus/whitelist.h"
#include "app/net/backend.h"
#include "app/net/wifi.h"
#include "app/radio/radio_config.h"
#include "app/config.h"

typedef struct
{
    wmbus_whitelist_t whitelist;
    backend_config_t backend;
    radio_config_t radio;
    char hostname[APP_HOSTNAME_MAX];
} services_state_t;

// Initialize persisted configs (whitelist, backend, radio, hostname).
esp_err_t services_init(services_state_t *svc);

// Accessors for composed services (non-owning).
static inline wmbus_whitelist_t *services_whitelist(services_state_t *svc)
{
    return svc ? &svc->whitelist : NULL;
}

static inline backend_config_t *services_backend(services_state_t *svc)
{
    return svc ? &svc->backend : NULL;
}

static inline radio_config_t *services_radio(services_state_t *svc)
{
    return svc ? &svc->radio : NULL;
}

// Returns stored hostname (never NULL); falls back to "oms-gateway".
const char *services_hostname(const services_state_t *svc);

// Setters/Getters as facade for UI/handlers (persistent).
esp_err_t services_set_backend_url(services_state_t *svc, const char *url);
esp_err_t services_get_backend_url(const services_state_t *svc, char *out, size_t out_len);

esp_err_t services_set_wifi_credentials(services_state_t *svc, const char *ssid, const char *pass);
esp_err_t services_get_wifi_status(app_wifi_status_t *out);

esp_err_t services_set_ap_config(services_state_t *svc, const char *ssid, const char *pass, uint8_t channel);

esp_err_t services_set_hostname(services_state_t *svc, const char *hostname);
esp_err_t services_get_hostname(char *out, size_t out_len);

esp_err_t services_set_radio_cs_level(services_state_t *svc, uint8_t level);
esp_err_t services_set_radio_sync_mode(services_state_t *svc, uint8_t mode);
esp_err_t services_get_radio_status(const services_state_t *svc, app_radio_status_t *out);
