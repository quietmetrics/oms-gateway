// Small NVS convenience helpers to reduce boilerplate.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t storage_get_str(const char *ns, const char *key, char *out, size_t out_len);
esp_err_t storage_set_str(const char *ns, const char *key, const char *val);

esp_err_t storage_get_u8(const char *ns, const char *key, uint8_t *out);
esp_err_t storage_set_u8(const char *ns, const char *key, uint8_t val);

esp_err_t storage_get_blob(const char *ns, const char *key, void *out, size_t *len_inout);
esp_err_t storage_set_blob(const char *ns, const char *key, const void *data, size_t len);
