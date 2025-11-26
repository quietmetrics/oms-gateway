#include "whitelist.h"
#include <string.h>
#include <strings.h>
#include "nvs_flash.h"
#include "nvs.h"

#define WL_MAX 16
#define WL_NS "oms"
#define WL_KEY "wl"

static whitelist_entry_t entries[WL_MAX];
static size_t entry_count = 0;

static esp_err_t save_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(WL_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, WL_KEY, entries, sizeof(entries));
    if (err == ESP_OK) {
        err = nvs_set_u32(h, "wl_count", entry_count);
    }
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t whitelist_init(void)
{
    entry_count = 0;
    memset(entries, 0, sizeof(entries));

    nvs_handle_t h;
    esp_err_t err = nvs_open(WL_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return ESP_OK;

    size_t sz = sizeof(entries);
    uint32_t cnt = 0;
    esp_err_t e1 = nvs_get_blob(h, WL_KEY, entries, &sz);
    esp_err_t e2 = nvs_get_u32(h, "wl_count", &cnt);
    nvs_close(h);
    if (e1 == ESP_OK && e2 == ESP_OK && cnt <= WL_MAX) {
        entry_count = cnt;
    } else {
        entry_count = 0;
        memset(entries, 0, sizeof(entries));
    }
    return ESP_OK;
}

static bool equal(const char *a, const char *b)
{
    return strcasecmp(a, b) == 0;
}

esp_err_t whitelist_add(const char *addr_hex)
{
    if (addr_hex == NULL) return ESP_ERR_INVALID_ARG;
    if (whitelist_contains(addr_hex)) return ESP_OK;
    if (entry_count >= WL_MAX) return ESP_ERR_NO_MEM;
    strncpy(entries[entry_count].address_hex, addr_hex, sizeof(entries[entry_count].address_hex) - 1);
    entries[entry_count].address_hex[sizeof(entries[entry_count].address_hex) - 1] = '\0';
    entry_count++;
    return save_nvs();
}

esp_err_t whitelist_remove(const char *addr_hex)
{
    if (!addr_hex) return ESP_ERR_INVALID_ARG;
    for (size_t i = 0; i < entry_count; i++) {
        if (equal(entries[i].address_hex, addr_hex)) {
            entries[i] = entries[entry_count - 1];
            entry_count--;
            return save_nvs();
        }
    }
    return ESP_ERR_NOT_FOUND;
}

bool whitelist_contains(const char *addr_hex)
{
    if (!addr_hex) return false;
    for (size_t i = 0; i < entry_count; i++) {
        if (equal(entries[i].address_hex, addr_hex)) return true;
    }
    return false;
}

size_t whitelist_list(whitelist_entry_t *out, size_t max_entries)
{
    size_t n = (entry_count < max_entries) ? entry_count : max_entries;
    if (out && n > 0) {
        memcpy(out, entries, n * sizeof(whitelist_entry_t));
    }
    return entry_count;
}
