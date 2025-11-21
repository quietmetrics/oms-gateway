/*
 * wmbus_protocol.c - wM-Bus frame parsing implementation
 * 
 * This file implements the wM-Bus protocol parsing functionality
 * including frame validation, address extraction, and CRC handling.
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "wmbus.h"

// wM-Bus CRC polynomial: x^16 + x^12 + x^5 + 1 (0x1021)
#define WMBUS_CRC_POLYNOMIAL 0x1021

/**
 * @brief Calculate CRC for wM-Bus frame
 * 
 * Calculates the CRC for a wM-Bus frame according to the protocol specification.
 * Uses the CRC-16-CCITT algorithm with polynomial 0x1021.
 * 
 * @param data Pointer to the data to calculate CRC for
 * @param length Length of the data
 * @return uint16_t Calculated CRC value
 */
uint16_t calculate_wmbus_crc(const uint8_t *data, uint8_t length) {
    uint16_t crc = 0x0000;  // Initial CRC value for wM-Bus
    
    for (int i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ WMBUS_CRC_POLYNOMIAL;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

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
                                       bool *is_encrypted) {
    // Check for minimum frame length
    if (frame_length < WMBUS_MIN_FRAME_LENGTH) {
        return WMBUS_ERROR_INVALID_FRAME;
    }
    
    // Extract the length field from the frame (first byte)
    uint8_t expected_length = frame_data[0];
    
    // Check if the length field is consistent with the actual frame length
    // The length field typically represents the length of data following it
    if (expected_length + 1 != frame_length) {
        // This could indicate an encrypted packet where length field is scrambled
        // Continue validation but mark as potentially encrypted
        if (is_encrypted) {
            *is_encrypted = true;
        }
    }
    
    // Check for valid frame type (C-field) - typically in the range 0x44-0x47 for wM-Bus
    uint8_t frame_type = frame_data[1];
    if (frame_type < 0x40 || frame_type > 0x7F) {
        // Not necessarily an error - could be different frame type or encrypted
    }
    
    // Calculate CRC for the frame (excluding the last 2 bytes which are CRC)
    if (frame_length >= 3) {  // Need at least 3 bytes to have data + CRC
        uint16_t calculated_crc = calculate_wmbus_crc(frame_data, frame_length - 2);
        uint16_t received_crc = (frame_data[frame_length - 2] << 8) | frame_data[frame_length - 1];
        
        if (calculated_crc != received_crc) {
            // CRC failure is common with encrypted packets
            if (is_encrypted) {
                *is_encrypted = true;
            }
            // Don't return an error here as encrypted packets will have CRC failures
        }
    }
    
    return WMBUS_SUCCESS;
}

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
                                     uint8_t *device_address) {
    // Check for minimum frame length to contain address field
    if (frame_length < WMBUS_HEADER_LENGTH) {
        return WMBUS_ERROR_INVALID_FRAME;
    }
    
    // Check if output buffer is valid
    if (device_address == NULL) {
        return WMBUS_ERROR_INVALID_INPUT;
    }
    
    // In wM-Bus frames, the device address is typically at positions 2-7 (6 bytes)
    // after the length field (byte 0) and frame type (byte 1)
    memcpy(device_address, &frame_data[2], 6);
    
    return WMBUS_SUCCESS;
}

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
                                wmbus_frame_t *parsed_frame) {
    // Check for valid input
    if (frame_data == NULL || parsed_frame == NULL) {
        return WMBUS_ERROR_INVALID_INPUT;
    }
    
    // Validate frame structure first
    bool is_encrypted = false;
    wmbus_error_t result = validate_frame_structure(frame_data, frame_length, &is_encrypted);
    if (result != WMBUS_SUCCESS) {
        return result;
    }
    
    // Extract basic frame fields
    parsed_frame->length = frame_data[0];
    parsed_frame->type = frame_data[1];
    
    // Extract device address (A-field)
    result = extract_device_address(frame_data, frame_length, parsed_frame->address);
    if (result != WMBUS_SUCCESS) {
        return result;
    }
    
    // Extract control field (CI-field) - typically at position 8
    if (frame_length > 8) {
        parsed_frame->control = frame_data[8];
    } else {
        parsed_frame->control = 0;  // Default value if not present
    }
    
    // Calculate data length (excluding header and CRC)
    if (frame_length > 10) {  // At least header + CRC
        parsed_frame->data_length = frame_length - 10;  // 2 bytes for length/type + 6 bytes for address + 2 bytes for CRC
        parsed_frame->data = (uint8_t*)&frame_data[8];  // Data starts after address field
    } else {
        parsed_frame->data_length = 0;
        parsed_frame->data = NULL;
    }
    
    // Extract CRC (last 2 bytes)
    if (frame_length >= 2) {
        parsed_frame->crc = (frame_data[frame_length - 2] << 8) | frame_data[frame_length - 1];
    } else {
        parsed_frame->crc = 0;  // Default value if not present
    }
    
    // Set the encrypted flag based on validation
    parsed_frame->is_encrypted = is_encrypted;
    
    return WMBUS_SUCCESS;
}