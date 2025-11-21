/*
 * wmbus.h - wM-Bus protocol handler header file
 * 
 * This file contains function declarations for the wM-Bus protocol implementation
 * including 3-of-6 decoding, frame parsing, and device address extraction.
 */

#ifndef WMBUS_H
#define WMBUS_H

#include <stdint.h>
#include <stdbool.h>

// wM-Bus frame structure constants
#define WMBUS_HEADER_LENGTH 10
#define WMBUS_MIN_FRAME_LENGTH 12
#define WMBUS_MAX_FRAME_LENGTH 255

// Error codes
typedef enum {
    WMBUS_SUCCESS = 0,
    WMBUS_ERROR_INVALID_INPUT,
    WMBUS_ERROR_INVALID_FRAME,
    WMBUS_ERROR_DECODE_FAILED,
    WMBUS_ERROR_CRC_FAILED,
    WMBUS_ERROR_ENCRYPTED_PACKET  // Special case for encrypted packets where CRC may fail
} wmbus_error_t;

// wM-Bus frame structure
typedef struct {
    uint8_t length;           // Length field
    uint8_t type;            // Frame type (C-field)
    uint8_t address[6];      // Device address (A-field)
    uint8_t control;         // Control field (CI-field)
    uint8_t *data;           // Data field
    uint8_t data_length;     // Length of data field
    uint16_t crc;            // CRC field
    bool is_encrypted;       // Flag indicating if packet is encrypted
} wmbus_frame_t;

/**
 * @brief Decode 3-of-6 encoded data
 * 
 * The 3-of-6 encoding maps 4 bits of data to 6 bits of encoded data.
 * This function decodes the 6-bit encoded values back to 4-bit data.
 * 
 * @param encoded_data Input buffer containing 3-of-6 encoded data
 * @param encoded_length Length of the encoded data in bytes
 * @param decoded_data Output buffer for decoded data (must be at least encoded_length/2 * 3 bytes)
 * @param decoded_length Pointer to store the length of decoded data
 * @return wmbus_error_t Error code indicating success or failure
 */
wmbus_error_t decode_three_of_six(const uint8_t *encoded_data, 
                                  uint8_t encoded_length, 
                                  uint8_t *decoded_data, 
                                  uint8_t *decoded_length);

/**
 * @brief Extract device address from wM-Bus frame
 * 
 * Extracts the 6-byte device address from a wM-Bus frame.
 * The address is located in the A-field of the frame.
 * 
 * @param frame_data Pointer to the frame data
 * @param frame_length Length of the frame data
 * @param device_address Output buffer to store the device address (must be at least 6 bytes)
 * @return wmbus_error_t Error code indicating success or failure
 */
wmbus_error_t extract_device_address(const uint8_t *frame_data, 
                                     uint8_t frame_length, 
                                     uint8_t *device_address);

/**
 * @brief Validate wM-Bus frame structure
 * 
 * Validates the basic structure of a wM-Bus frame including length,
 * header format, and CRC check. Handles special case of encrypted
 * packets where CRC may fail.
 * 
 * @param frame_data Pointer to the frame data
 * @param frame_length Length of the frame data
 * @param is_encrypted Output flag indicating if the packet appears to be encrypted
 * @return wmbus_error_t Error code indicating success or failure
 */
wmbus_error_t validate_frame_structure(const uint8_t *frame_data, 
                                       uint8_t frame_length, 
                                       bool *is_encrypted);

/**
 * @brief Parse complete wM-Bus frame
 * 
 * Parses a complete wM-Bus frame, extracting all relevant fields
 * and performing validation. Handles both encrypted and non-encrypted
 * packets appropriately.
 * 
 * @param frame_data Pointer to the frame data
 * @param frame_length Length of the frame data
 * @param parsed_frame Output structure to store parsed frame data
 * @return wmbus_error_t Error code indicating success or failure
 */
wmbus_error_t parse_wmbus_frame(const uint8_t *frame_data, 
                                uint8_t frame_length, 
                                wmbus_frame_t *parsed_frame);

/**
 * @brief Calculate CRC for wM-Bus frame
 * 
 * Calculates the CRC for a wM-Bus frame according to the protocol specification.
 * 
 * @param data Pointer to the data to calculate CRC for
 * @param length Length of the data
 * @return uint16_t Calculated CRC value
 */
uint16_t calculate_wmbus_crc(const uint8_t *data, uint8_t length);

#endif // WMBUS_H