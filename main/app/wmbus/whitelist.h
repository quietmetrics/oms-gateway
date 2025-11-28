// Simple whitelist manager for wM-Bus devices (manufacturer + ID), persisted in NVS.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#define WMBUS_WHITELIST_MAX 32

typedef struct
{
    uint16_t manufacturer_le;
    uint8_t id[4]; // BCD LSB first as received
} wmbus_address_t;

typedef struct
{
    wmbus_address_t entries[WMBUS_WHITELIST_MAX];
    size_t count;
} wmbus_whitelist_t;

esp_err_t wmbus_whitelist_init(wmbus_whitelist_t *wl);
bool wmbus_whitelist_contains(const wmbus_whitelist_t *wl, uint16_t manufacturer_le, const uint8_t id[4]);
esp_err_t wmbus_whitelist_add(wmbus_whitelist_t *wl, uint16_t manufacturer_le, const uint8_t id[4]);
esp_err_t wmbus_whitelist_remove(wmbus_whitelist_t *wl, uint16_t manufacturer_le, const uint8_t id[4]);
// Copy out entries; returns count copied (up to max_entries).
size_t wmbus_whitelist_list(const wmbus_whitelist_t *wl, wmbus_address_t *out, size_t max_entries);
