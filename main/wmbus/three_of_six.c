/*
 * three_of_six.c - 3-of-6 decoding algorithm implementation
 * 
 * This file implements the 3-of-6 decoding algorithm used in wM-Bus protocol.
 * The 3-of-6 encoding maps 4 bits of data to 6 bits of encoded data.
 * This implementation decodes the 6-bit encoded values back to 4-bit data.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wmbus.h"

// 3-of-6 decoding lookup table
// Maps 6-bit encoded values to 4-bit decoded values
// Invalid combinations are marked with 0xFF
static uint8_t three_of_six_decode_table[64];

// Initialize the 3-of-6 decoding lookup table
// This function populates the decoding table with proper values
static void initialize_three_of_six_decode_table(void) {
    // The 3-of-6 code maps 4 data bits to 6 encoded bits
    // Valid 3-of-6 codes have exactly 3 bits set out of 6
    for (int i = 0; i < 64; i++) {
        // Count number of set bits in the 6-bit value
        int count = 0;
        int temp = i;
        while (temp) {
            count += temp & 1;
            temp >>= 1;
        }
        
        // Only store values that have exactly 3 bits set
        if (count == 3) {
            switch (i) {
                case 0x07:  // 000111
                    three_of_six_decode_table[i] = 0x0;  // 0000
                    break;
                case 0x0B:  // 001011
                    three_of_six_decode_table[i] = 0x1;  // 0001
                    break;
                case 0x0D:  // 001101
                    three_of_six_decode_table[i] = 0x2;  // 0010
                    break;
                case 0x0E:  // 001110
                    three_of_six_decode_table[i] = 0x3;  // 0011
                    break;
                case 0x13:  // 010011
                    three_of_six_decode_table[i] = 0x4;  // 0100
                    break;
                case 0x15:  // 010101
                    three_of_six_decode_table[i] = 0x5;  // 0101
                    break;
                case 0x16:  // 010110
                    three_of_six_decode_table[i] = 0x6;  // 0110
                    break;
                case 0x19:  // 011001
                    three_of_six_decode_table[i] = 0x7;  // 0111
                    break;
                case 0x1A:  // 011010
                    three_of_six_decode_table[i] = 0x8;  // 1000
                    break;
                case 0x1C:  // 011100
                    three_of_six_decode_table[i] = 0x9;  // 1001
                    break;
                case 0x23:  // 100011
                    three_of_six_decode_table[i] = 0xA;  // 1010
                    break;
                case 0x25:  // 100101
                    three_of_six_decode_table[i] = 0xB;  // 1011
                    break;
                case 0x26:  // 100110
                    three_of_six_decode_table[i] = 0xC;  // 1100
                    break;
                case 0x29:  // 101001
                    three_of_six_decode_table[i] = 0xD;  // 1101
                    break;
                case 0x2A:  // 101010
                    three_of_six_decode_table[i] = 0xE;  // 1110
                    break;
                case 0x2C:  // 101100
                    three_of_six_decode_table[i] = 0xF;  // 1111
                    break;
                default:
                    three_of_six_decode_table[i] = 0xFF;  // Invalid code
                    break;
            }
        } else {
            three_of_six_decode_table[i] = 0xFF;  // Invalid code
        }
    }
}

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
                                  uint8_t *decoded_length) {
    // Validate input parameters
    if (encoded_data == NULL || decoded_data == NULL || decoded_length == NULL) {
        return WMBUS_ERROR_INVALID_INPUT;
    }
    
    // Initialize the decoding table if not already done
    // Note: In a real implementation, this should be done once during initialization
    static bool table_initialized = false;
    if (!table_initialized) {
        initialize_three_of_six_decode_table();
        table_initialized = true;
    }
    
    // Calculate the expected maximum decoded length
    // Each 6-bit encoded value produces 4 bits of decoded data
    // So 2 bytes of encoded data (16 bits) can contain 2 or 3 6-bit values
    // Maximum decoded length is approximately (encoded_length * 8) / 6 * 4 / 8 = encoded_length * 2 / 3
    uint8_t max_decoded_length = (encoded_length * 4) / 3 + 1;  // Conservative estimate
    
    // Temporary buffer to store decoded nibbles
    uint8_t decoded_nibbles[256];  // Assuming max frame size
    size_t nibble_count = 0;
    
    // Process the encoded data bit by bit
    uint32_t bit_buffer = 0;
    uint8_t bits_in_buffer = 0;
    
    for (int i = 0; i < encoded_length; i++) {
        // Add the current byte to the bit buffer
        bit_buffer |= ((uint32_t)encoded_data[i]) << bits_in_buffer;
        bits_in_buffer += 8;
        
        // Extract 6-bit chunks while we have enough bits
        while (bits_in_buffer >= 6) {
            uint8_t sixbit_value = bit_buffer & 0x3F;  // Extract 6 bits
            bit_buffer >>= 6;
            bits_in_buffer -= 6;
            
            // Look up the decoded value
            uint8_t decoded_value = three_of_six_decode_table[sixbit_value];
            
            // Check if the 6-bit value is valid in 3-of-6 encoding
            if (decoded_value == 0xFF) {
                // Invalid 3-of-6 code
                return WMBUS_ERROR_DECODE_FAILED;
            }
            
            // Store the decoded nibble
            if (nibble_count < sizeof(decoded_nibbles)) {
                decoded_nibbles[nibble_count++] = decoded_value;
            } else {
                // Buffer overflow
                return WMBUS_ERROR_INVALID_INPUT;
            }
        }
    }
    
    // Now combine the nibbles back into bytes
    *decoded_length = 0;
    for (int i = 0; i < nibble_count; i += 2) {
        if (*decoded_length >= max_decoded_length) {
            break;  // Prevent buffer overflow
        }
        
        uint8_t byte_value = decoded_nibbles[i];  // High nibble
        
        if (i + 1 < nibble_count) {
            // Combine with low nibble
            byte_value |= decoded_nibbles[i + 1] << 4;
        }
        // If odd number of nibbles, the last nibble becomes the high part of the last byte
        
        decoded_data[*decoded_length] = byte_value;
        (*decoded_length)++;
    }
    
    return WMBUS_SUCCESS;
}

// Additional helper function to encode data using 3-of-6 (for testing purposes)
// This is not part of the required interface but useful for testing
static const uint8_t three_of_six_encode_table[16] = {
    0x07, 0x0B, 0x0D, 0x0E, 0x13, 0x15, 0x16, 0x19,
    0x1A, 0x1C, 0x23, 0x25, 0x26, 0x29, 0x2A, 0x2C
};

/**
 * @brief Helper function to encode data using 3-of-6 encoding (for testing)
 * 
 * @param raw_data Input buffer containing raw data to encode
 * @param raw_length Length of the raw data in bytes
 * @param encoded_data Output buffer for encoded data
 * @param encoded_length Pointer to store the length of encoded data
 * @return wmbus_error_t Error code indicating success or failure
 */
wmbus_error_t encode_three_of_six(const uint8_t *raw_data, 
                                  uint8_t raw_length, 
                                  uint8_t *encoded_data, 
                                  uint8_t *encoded_length) {
    if (raw_data == NULL || encoded_data == NULL || encoded_length == NULL) {
        return WMBUS_ERROR_INVALID_INPUT;
    }
    
    uint8_t nibble_count = raw_length * 2;  // Each byte has 2 nibbles
    uint8_t encoded_bytes = (nibble_count * 6 + 7) / 8;  // Number of bytes needed for encoded data
    
    // Temporary buffer for encoded nibbles
    uint8_t encoded_nibbles[512];  // Assuming max frame size
    size_t nibble_idx = 0;
    
    // Convert bytes to nibbles and encode each nibble
    for (int i = 0; i < raw_length; i++) {
        uint8_t high_nibble = (raw_data[i] >> 4) & 0x0F;
        uint8_t low_nibble = raw_data[i] & 0x0F;
        
        encoded_nibbles[nibble_idx++] = three_of_six_encode_table[high_nibble];
        encoded_nibbles[nibble_idx++] = three_of_six_encode_table[low_nibble];
    }
    
    // Pack the 6-bit values into bytes
    uint32_t bit_buffer = 0;
    uint8_t bits_in_buffer = 0;
    *encoded_length = 0;
    
    for (int i = 0; i < nibble_idx; i++) {
        bit_buffer |= ((uint32_t)encoded_nibbles[i]) << bits_in_buffer;
        bits_in_buffer += 6;
        
        while (bits_in_buffer >= 8) {
            if (*encoded_length >= encoded_bytes) {
                return WMBUS_ERROR_INVALID_INPUT;  // Buffer too small
            }
            
            encoded_data[*encoded_length] = bit_buffer & 0xFF;
            (*encoded_length)++;
            bit_buffer >>= 8;
            bits_in_buffer -= 8;
        }
    }
    
    // Add any remaining bits
    if (bits_in_buffer > 0) {
        if (*encoded_length < encoded_bytes) {
            encoded_data[*encoded_length] = bit_buffer & 0xFF;
            (*encoded_length)++;
        }
    }
    
    return WMBUS_SUCCESS;
}