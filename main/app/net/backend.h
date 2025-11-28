// Backend configuration and forwarding utilities.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "app/wmbus/packet_router.h"

typedef struct
{
    char url[192]; // e.g., http://host:port/path (kept short on purpose)
} backend_config_t;

// Initialize backend config (load from NVS or keep empty).
esp_err_t backend_init(backend_config_t *cfg);

// Set/persist backend URL. Pass NULL/empty to clear.
esp_err_t backend_set_url(backend_config_t *cfg, const char *url);
// Read current URL into buffer; buffer is null-terminated.
esp_err_t backend_get_url(const backend_config_t *cfg, char *out, size_t out_len);

// Check reachability (HEAD); timeout in ms.
esp_err_t backend_check_url(const char *url, int timeout_ms);

// Forward packet to backend (POST JSON). Requires non-empty cfg->url.
esp_err_t backend_forward_packet(const backend_config_t *cfg, const WmbusPacketEvent *evt);
