#pragma once
#include "esp_err.h"
#include <stdbool.h>

typedef struct {
    char ssid[33];
    char password[65];
    char hostname[33];
    char backend_host[64];
    int backend_port;
    bool backend_https;
} app_config_t;

esp_err_t config_manager_init(app_config_t *cfg);
esp_err_t config_manager_save(const app_config_t *cfg);
