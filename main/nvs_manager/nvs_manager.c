/*
 * NVS Manager Implementation
 *
 * Centralized Non-Volatile Storage management for the wM-Bus Gateway project.
 * This module handles all NVS initialization and provides utilities for
 * other modules to safely access NVS storage.
 */

#include <stdbool.h>
#include <string.h>
#include <esp_log.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include "nvs_manager.h"

static const char* TAG = "NVS_MANAGER";

// Flag to track initialization status
static bool nvs_initialized = false;

// Mutex for thread-safe NVS operations
static SemaphoreHandle_t nvs_mutex = NULL;

esp_err_t nvs_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing NVS storage system");

    if (nvs_initialized) {
        ESP_LOGW(TAG, "NVS already initialized");
        return ESP_OK;
    }

    // Create mutex for thread safety
    nvs_mutex = xSemaphoreCreateMutex();
    if (nvs_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create NVS mutex");
        return ESP_ERR_NO_MEM;
    }

    // Initialize NVS flash for storing configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated or version changed, reinitializing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        vSemaphoreDelete(nvs_mutex);
        nvs_mutex = NULL;
        return ret;
    }

    nvs_initialized = true;
    ESP_LOGI(TAG, "NVS storage system initialized successfully");
    return ESP_OK;
}

esp_err_t nvs_manager_deinit(void)
{
    if (!nvs_initialized) {
        ESP_LOGW(TAG, "NVS not initialized, nothing to deinitialize");
        return ESP_OK;
    }

    // Delete the mutex
    if (nvs_mutex) {
        vSemaphoreDelete(nvs_mutex);
        nvs_mutex = NULL;
    }

    // Erase all NVS partitions (optional, for complete cleanup)
    // esp_err_t ret = nvs_flash_deinit();
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to deinitialize NVS: %s", esp_err_to_name(ret));
    //     return ret;
    // }

    nvs_initialized = false;
    ESP_LOGI(TAG, "NVS storage system deinitialized");
    return ESP_OK;
}

bool nvs_manager_is_initialized(void)
{
    return nvs_initialized;
}

esp_err_t nvs_manager_acquire_mutex(void)
{
    if (!nvs_mutex) {
        ESP_LOGE(TAG, "NVS mutex not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(nvs_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t nvs_manager_release_mutex(void)
{
    if (!nvs_mutex) {
        ESP_LOGE(TAG, "NVS mutex not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreGive(nvs_mutex) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to release NVS mutex");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t nvs_manager_safe_open(const char* name, nvs_open_mode open_mode, nvs_handle_t* out_handle)
{
    if (!nvs_manager_is_initialized()) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!name || !out_handle) {
        ESP_LOGE(TAG, "Invalid parameters for nvs_manager_safe_open");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_manager_acquire_mutex();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ret;
    }

    ret = nvs_open(name, open_mode, out_handle);
    if (ret != ESP_OK) {
        // For read-only operations, ESP_ERR_NVS_NOT_FOUND just means the namespace doesn't exist yet
        // which is not necessarily an error condition - it means no data has been stored
        if (ret == ESP_ERR_NVS_NOT_FOUND && open_mode == NVS_READONLY) {
            ESP_LOGD(TAG, "NVS namespace '%s' does not exist (expected for read-only when no data stored)", name);
            nvs_manager_release_mutex();
            return ret; // Return the error so the calling function can handle appropriately
        } else {
            ESP_LOGE(TAG, "Failed to open NVS handle for namespace '%s': %s", name, esp_err_to_name(ret));
            nvs_manager_release_mutex();
            return ret;
        }
    }

    ESP_LOGD(TAG, "Successfully opened NVS handle for namespace '%s'", name);
    nvs_manager_release_mutex();
    return ESP_OK;
}

esp_err_t nvs_manager_safe_close(nvs_handle_t handle)
{
    if (!nvs_manager_is_initialized()) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Note: We don't acquire mutex for close operation since the handle should
    // already be in use by the current thread and mutex would have been acquired during open
    nvs_close(handle);
    ESP_LOGD(TAG, "Successfully closed NVS handle");
    return ESP_OK;
}

esp_err_t nvs_manager_get_string(nvs_handle_t handle, const char* key, char* out_value, size_t* length)
{
    if (!nvs_manager_is_initialized()) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!key || !out_value || !length) {
        ESP_LOGE(TAG, "Invalid parameters for nvs_manager_get_string");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_manager_acquire_mutex();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ret;
    }

    ret = nvs_get_str(handle, key, out_value, length);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get string for key '%s': %s", key, esp_err_to_name(ret));
        nvs_manager_release_mutex();
        return ret;
    }

    ESP_LOGD(TAG, "Successfully got string for key '%s' (length: %zu)", key, *length);
    nvs_manager_release_mutex();
    return ESP_OK;
}

esp_err_t nvs_manager_set_string(nvs_handle_t handle, const char* key, const char* value)
{
    if (!nvs_manager_is_initialized()) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!key || !value) {
        ESP_LOGE(TAG, "Invalid parameters for nvs_manager_set_string");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_manager_acquire_mutex();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ret;
    }

    ret = nvs_set_str(handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set string for key '%s': %s", key, esp_err_to_name(ret));
        nvs_manager_release_mutex();
        return ret;
    }

    ret = nvs_commit(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(ret));
        nvs_manager_release_mutex();
        return ret;
    }

    ESP_LOGD(TAG, "Successfully set string for key '%s'", key);
    nvs_manager_release_mutex();
    return ESP_OK;
}

esp_err_t nvs_manager_get_stats(size_t* total_entries, size_t* used_entries, size_t* free_entries)
{
    if (!nvs_manager_is_initialized()) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!total_entries || !used_entries || !free_entries) {
        ESP_LOGE(TAG, "Invalid parameters for nvs_manager_get_stats");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_manager_acquire_mutex();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ret;
    }

    // Get NVS statistics
    nvs_stats_t nvs_stats;
    ret = nvs_get_stats(NULL, &nvs_stats);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get NVS stats: %s", esp_err_to_name(ret));
        nvs_manager_release_mutex();
        return ret;
    }

    *total_entries = nvs_stats.total_entries;
    *used_entries = nvs_stats.used_entries;
    *free_entries = nvs_stats.free_entries;

    ESP_LOGD(TAG, "NVS Stats - Total: %zu, Used: %zu, Free: %zu",
             *total_entries, *used_entries, *free_entries);

    nvs_manager_release_mutex();
    return ESP_OK;
}

esp_err_t nvs_manager_get_namespace_stats(const char* namespace_name, size_t* used_entries, size_t* free_entries)
{
    if (!nvs_manager_is_initialized()) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!namespace_name || !used_entries || !free_entries) {
        ESP_LOGE(TAG, "Invalid parameters for nvs_manager_get_namespace_stats");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_manager_acquire_mutex();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ret;
    }

    // Create a dummy handle to test if namespace exists
    nvs_handle_t handle;
    ret = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open namespace '%s': %s", namespace_name, esp_err_to_name(ret));
        nvs_manager_release_mutex();
        return ret;
    }

    // Get namespace-specific statistics by examining the handle
    // Unfortunately, ESP-IDF doesn't provide direct API for namespace stats
    // So we'll use the global stats and provide a best estimate
    nvs_stats_t nvs_stats;
    esp_err_t stats_ret = nvs_get_stats(namespace_name, &nvs_stats);
    nvs_close(handle);

    if (stats_ret == ESP_OK) {
        *used_entries = nvs_stats.used_entries;
        *free_entries = nvs_stats.total_entries - nvs_stats.used_entries;
    } else {
        // If specific namespace stats fail, provide zero values
        *used_entries = 0;
        *free_entries = 0;
        // This is not an error, just means namespace might be empty
        ret = ESP_OK;
    }

    ESP_LOGD(TAG, "Namespace '%s' Stats - Used: %zu, Free: %zu",
             namespace_name, *used_entries, *free_entries);

    nvs_manager_release_mutex();
    return ret;
}

esp_err_t nvs_manager_health_check(void)
{
    if (!nvs_manager_is_initialized()) {
        ESP_LOGE(TAG, "NVS manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_manager_acquire_mutex();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire NVS mutex");
        return ret;
    }

    ESP_LOGI(TAG, "Performing NVS health check...");

    // Check basic NVS functionality by attempting to open a test namespace
    nvs_handle_t test_handle;
    ret = nvs_open("health_check", NVS_READWRITE, &test_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS health check failed - cannot open namespace: %s", esp_err_to_name(ret));
        nvs_manager_release_mutex();
        return ret;
    }

    // Try to set and get a test value
    const char* test_key = "health_test";
    const char* test_value = "test";
    ret = nvs_set_str(test_handle, test_key, test_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS health check failed - cannot set value: %s", esp_err_to_name(ret));
        nvs_close(test_handle);
        nvs_manager_release_mutex();
        return ret;
    }

    ret = nvs_commit(test_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS health check failed - cannot commit: %s", esp_err_to_name(ret));
        nvs_close(test_handle);
        nvs_manager_release_mutex();
        return ret;
    }

    // Read the test value back
    char read_value[10];
    size_t len = sizeof(read_value);
    ret = nvs_get_str(test_handle, test_key, read_value, &len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS health check failed - cannot get value: %s", esp_err_to_name(ret));
        nvs_close(test_handle);
        nvs_manager_release_mutex();
        return ret;
    }

    nvs_close(test_handle);

    // Also check general NVS statistics
    size_t total_entries, used_entries, free_entries;
    esp_err_t stats_ret = nvs_manager_get_stats(&total_entries, &used_entries, &free_entries);
    if (stats_ret != ESP_OK) {
        ESP_LOGW(TAG, "Could not retrieve NVS statistics: %s", esp_err_to_name(stats_ret));
    } else {
        ESP_LOGI(TAG, "NVS Stats - Total: %zu, Used: %zu, Free: %zu",
                 total_entries, used_entries, free_entries);

        // Log a warning if usage is getting high
        if (total_entries > 0 && used_entries > total_entries * 0.8) {
            ESP_LOGW(TAG, "High NVS usage detected: %.2f%%",
                     (float)used_entries * 100.0f / total_entries);
        }
    }

    ESP_LOGI(TAG, "NVS health check completed successfully");
    nvs_manager_release_mutex();
    return ESP_OK;
}