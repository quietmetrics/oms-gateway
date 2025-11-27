// Wireless M-Bus packet utilities (T-mode only)
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define WMBUS_PACKET_C_FIELD  0x44
#define WMBUS_MAN_CODE        0x0CAE
#define WMBUS_MAN_NUMBER      0x12345678
#define WMBUS_MAN_ID          0x01
#define WMBUS_MAN_VER         0x07
#define WMBUS_PACKET_CI_FIELD 0x78

#define WMBUS_PKT_OK            0
#define WMBUS_PKT_CODING_ERROR  1
#define WMBUS_PKT_CRC_ERROR     2

uint16_t wmbus_packet_size(uint8_t l_field);
uint16_t wmbus_byte_size_tmode(bool transmit, uint16_t packet_size);

void wmbus_encode_tx_packet(uint8_t *packet, const uint8_t *data, uint8_t data_size);
void wmbus_encode_tx_bytes_tmode(uint8_t *encoded, const uint8_t *packet, uint16_t packet_size);
uint16_t wmbus_decode_rx_bytes_tmode(const uint8_t *encoded, uint8_t *packet, uint16_t packet_size);
