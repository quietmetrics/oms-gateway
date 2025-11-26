#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char *backend_host;
    int backend_port;
    bool backend_https;
    const char *gateway_id;
} forwarder_cfg_t;

typedef struct {
    float rssi_dbm;
    uint8_t lqi;
    bool crc_ok;
    const char *device_address_hex;
    const char *frame_hex;
} forwarder_frame_t;

esp_err_t forwarder_send(const forwarder_cfg_t *cfg, const forwarder_frame_t *frame);
