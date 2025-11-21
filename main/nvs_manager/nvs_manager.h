/*
 * NVS Manager Header
 *
 * Centralized Non-Volatile Storage management for the wM-Bus Gateway project.
 * This module handles all NVS initialization and provides utilities for
 * other modules to safely access NVS storage.
 */

#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include <esp_err.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize NVS storage system
 *
 * This function should be called once during application startup
 * to initialize the NVS subsystem. It handles all initialization
 * and potential recovery scenarios.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t nvs_manager_init(void);

/**
 * @brief Deinitialize NVS storage system
 *
 * Frees resources used by the NVS manager.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t nvs_manager_deinit(void);

/**
 * @brief Check if NVS is properly initialized
 *
 * @return true if initialized, false otherwise
 */
bool nvs_manager_is_initialized(void);

/**
 * @brief Safely open an NVS handle with error checking
 *
 * This function checks if NVS is initialized and safely opens an NVS handle
 * with proper error handling.
 *
 * @param[in] name Namespace name for the NVS handle
 * @param[in] open_mode NVS open mode (NVS_READONLY, NVS_READWRITE)
 * @param[out] out_handle Pointer to the NVS handle to be opened
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t nvs_manager_safe_open(const char* name, nvs_open_mode open_mode, nvs_handle_t* out_handle);

/**
 * @brief Safely close an NVS handle
 *
 * Properly closes the NVS handle and handles any potential errors.
 *
 * @param[in] handle NVS handle to close
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t nvs_manager_safe_close(nvs_handle_t handle);

/**
 * @brief Get string value from NVS with safety checks
 *
 * @param[in] handle Opened NVS handle
 * @param[in] key Key to retrieve the string for
 * @param[out] out_value Output buffer for the string value
 * @param[inout] length Length of the output buffer (updated with actual string length)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t nvs_manager_get_string(nvs_handle_t handle, const char* key, char* out_value, size_t* length);

/**
 * @brief Set string value in NVS with safety checks
 *
 * @param[in] handle Opened NVS handle
 * @param[in] key Key to store the string under
 * @param[in] value String value to store
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t nvs_manager_set_string(nvs_handle_t handle, const char* key, const char* value);

/**
 * @brief Acquire NVS mutex for thread-safe operations
 *
 * Before performing any NVS operation, acquire this mutex to ensure thread safety.
 * Call nvs_manager_release_mutex() when done.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t nvs_manager_acquire_mutex(void);

/**
 * @brief Release NVS mutex
 *
 * Release the mutex after completing NVS operations.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t nvs_manager_release_mutex(void);

/**
 * @brief Get NVS statistics
 *
 * Provides statistics about NVS usage including total space, used space, and free space.
 *
 * @param[out] total_entries Total number of available entries
 * @param[out] used_entries Number of used entries
 * @param[out] free_entries Number of free entries
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t nvs_manager_get_stats(size_t* total_entries, size_t* used_entries, size_t* free_entries);

/**
 * @brief Get NVS namespace statistics
 *
 * Provides statistics about a specific NVS namespace.
 *
 * @param[in] namespace_name Name of the namespace to query
 * @param[out] used_entries Number of used entries in this namespace
 * @param[out] free_entries Number of free entries in this namespace
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t nvs_manager_get_namespace_stats(const char* namespace_name, size_t* used_entries, size_t* free_entries);

/**
 * @brief Perform NVS health check
 *
 * Performs a comprehensive health check on the NVS system and reports any issues.
 *
 * @return ESP_OK if healthy, error code if issues found
 */
esp_err_t nvs_manager_health_check(void);

#ifdef __cplusplus
}
#endif

#endif // NVS_MANAGER_H