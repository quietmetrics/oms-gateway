#pragma once
#include <stdint.h>
#include "encoding_3of6.h"

typedef enum {
    WMBUS_PKT_OK = 0,
    WMBUS_PKT_CODING_ERROR = 1,
    WMBUS_PKT_CRC_ERROR = 2,
} wmbus_packet_status_t;

uint16_t wmbus_packet_size_from_l(uint8_t l_field);
uint16_t wmbus_encoded_size_from_packet(uint16_t packet_size_bytes);
wmbus_packet_status_t wmbus_decode_tmode(uint8_t *encoded, uint8_t *packet, uint16_t packet_size);
uint16_t wmbus_crc_step(uint16_t crcReg, uint8_t crcData);
