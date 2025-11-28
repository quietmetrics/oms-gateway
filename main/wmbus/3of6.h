// 3-out-of-6 encoding/decoding used for wM-Bus T-mode
#pragma once

#include <stdint.h>

#define WMBUS_3OF6_OK    0
#define WMBUS_3OF6_ERROR 1

void wmbus_encode_3of6(const uint8_t *uncoded, uint8_t *encoded, uint8_t last_byte);
uint8_t wmbus_decode_3of6(const uint8_t *encoded, uint8_t *decoded, uint8_t last_byte);
