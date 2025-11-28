#include "app/wmbus/whitelist.h"

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "app/storage.h"

static const char *TAG = "wmbus_whitelist";
static const char *NAMESPACE = "wmbus";
static const char *KEY = "whitelist";

static bool addr_equal(const wmbus_address_t *a, uint16_t manufacturer_le, const uint8_t id[4])
{
    return a->manufacturer_le == manufacturer_le && memcmp(a->id, id, sizeof(a->id)) == 0;
}

static esp_err_t whitelist_save(const wmbus_whitelist_t *wl)
{
    esp_err_t err = storage_set_blob(NAMESPACE, KEY, wl, sizeof(wmbus_whitelist_t));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "whitelist save failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t whitelist_load(wmbus_whitelist_t *wl)
{
    size_t required = sizeof(wmbus_whitelist_t);
    return storage_get_blob(NAMESPACE, KEY, wl, &required);
}

esp_err_t wmbus_whitelist_init(wmbus_whitelist_t *wl)
{
    if (!wl)
    {
        return ESP_ERR_INVALID_ARG;
    }

    memset(wl, 0, sizeof(wmbus_whitelist_t));

    esp_err_t err = whitelist_load(wl);
    if (err == ESP_OK)
    {
        return ESP_OK;
    }

    if (err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "whitelist load failed: %s", esp_err_to_name(err));
    }

    // Start empty and persist baseline
    return whitelist_save(wl);
}

bool wmbus_whitelist_contains(const wmbus_whitelist_t *wl, uint16_t manufacturer_le, const uint8_t id[4])
{
    if (!wl || !id)
    {
        return false;
    }

    for (size_t i = 0; i < wl->count; i++)
    {
        if (addr_equal(&wl->entries[i], manufacturer_le, id))
        {
            return true;
        }
    }
    return false;
}

esp_err_t wmbus_whitelist_add(wmbus_whitelist_t *wl, uint16_t manufacturer_le, const uint8_t id[4])
{
    if (!wl || !id)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (wmbus_whitelist_contains(wl, manufacturer_le, id))
    {
        return ESP_OK; // already present
    }

    if (wl->count >= WMBUS_WHITELIST_MAX)
    {
        return ESP_ERR_NO_MEM;
    }

    wl->entries[wl->count].manufacturer_le = manufacturer_le;
    memcpy(wl->entries[wl->count].id, id, sizeof(wl->entries[wl->count].id));
    wl->count++;

    return whitelist_save(wl);
}

esp_err_t wmbus_whitelist_remove(wmbus_whitelist_t *wl, uint16_t manufacturer_le, const uint8_t id[4])
{
    if (!wl || !id)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < wl->count; i++)
    {
        if (addr_equal(&wl->entries[i], manufacturer_le, id))
        {
            // Compact array
            if (i != wl->count - 1)
            {
                wl->entries[i] = wl->entries[wl->count - 1];
            }
            wl->count--;
            return whitelist_save(wl);
        }
    }

    return ESP_ERR_NOT_FOUND;
}

size_t wmbus_whitelist_list(const wmbus_whitelist_t *wl, wmbus_address_t *out, size_t max_entries)
{
    if (!wl || !out || max_entries == 0)
    {
        return 0;
    }
    size_t to_copy = wl->count < max_entries ? wl->count : max_entries;
    memcpy(out, wl->entries, to_copy * sizeof(wmbus_address_t));
    return to_copy;
}
