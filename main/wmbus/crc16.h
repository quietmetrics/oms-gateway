// CRC16 implementation for Wireless M-Bus (polynom 0x3D65)
#pragma once

#include <stdint.h>

#define WMBUS_CRC_POLY 0x3D65

uint16_t wmbus_crc16_step(uint16_t crc, uint8_t data);
