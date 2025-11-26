#pragma once
#include "esp_err.h"
#include "whitelist/whitelist.h"
#include "config/config_manager.h"

typedef struct {
    const app_config_t *cfg;
} web_server_ctx_t;

esp_err_t web_server_start(const web_server_ctx_t *ctx);
void web_server_stop(void);
