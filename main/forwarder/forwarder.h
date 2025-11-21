/*
 * forwarder.h - Backend communication interface
 * 
 * This module handles forwarding of whitelisted wM-Bus device data to the backend server.
 * It ensures secure communication and proper payload packaging.
 */

#ifndef FORWARDER_H
#define FORWARDER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Structure representing a wM-Bus frame to be forwarded
 */
typedef struct {
    uint8_t device_address[8];      /*!< Device address from the wM-Bus frame */
    uint8_t *payload;               /*!< Raw payload data */
    size_t payload_size;            /*!< Size of the payload */
    uint8_t frame_type;             /*!< Type of wM-Bus frame */
    uint32_t timestamp;             /*!< Timestamp of frame reception */
} wmbus_frame_t;

/**
 * @brief Initialize the forwarder module
 * 
 * This function initializes the forwarder module, setting up network connections
 * and any required resources for backend communication.
 * 
 * @return ESP_OK on success, ESP_ERR_XXX on failure
 */
esp_err_t forwarder_init(void);

/**
 * @brief Forward data to the backend server
 * 
 * This function sends the provided wM-Bus frame data to the backend server
 * if the device is whitelisted. It handles secure communication with the backend.
 * 
 * @param frame Pointer to the wM-Bus frame structure containing data to forward
 * @return ESP_OK on success, ESP_ERR_XXX on failure
 */
esp_err_t forward_to_backend(const wmbus_frame_t *frame);

/**
 * @brief Package payload for forwarding to backend
 * 
 * This function prepares the payload data for secure transmission to the backend,
 * including any necessary encryption or formatting.
 * 
 * @param frame Pointer to the wM-Bus frame to package
 * @param[out] output_buffer Buffer to store the packaged payload
 * @param[out] output_size Size of the packaged payload
 * @return ESP_OK on success, ESP_ERR_XXX on failure
 */
esp_err_t package_payload_for_forwarding(const wmbus_frame_t *frame, 
                                        uint8_t **output_buffer, 
                                        size_t *output_size);

/**
 * @brief Check if a device is whitelisted
 * 
 * This function verifies if the provided device address is in the whitelist
 * before allowing data forwarding to the backend.
 * 
 * @param device_address Pointer to the 8-byte device address
 * @return true if device is whitelisted, false otherwise
 */
bool is_device_whitelisted(const uint8_t *device_address);

/**
 * @brief Set the backend server URL
 * 
 * This function allows configuration of the backend server endpoint
 * 
 * @param url The URL of the backend server
 * @return ESP_OK on success, ESP_ERR_XXX on failure
 */
esp_err_t set_backend_url(const char *url);

/**
 * @brief Deinitialize the forwarder module
 * 
 * This function cleans up resources used by the forwarder module
 * 
 * @return ESP_OK on success, ESP_ERR_XXX on failure
 */
esp_err_t forwarder_deinit(void);

#endif // FORWARDER_H