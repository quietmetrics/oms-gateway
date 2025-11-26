#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

typedef struct {
    char address_hex[17]; // up to 16 hex chars + null
} whitelist_entry_t;

esp_err_t whitelist_init(void);
esp_err_t whitelist_add(const char *addr_hex);
esp_err_t whitelist_remove(const char *addr_hex);
bool whitelist_contains(const char *addr_hex);
size_t whitelist_list(whitelist_entry_t *out, size_t max_entries);
