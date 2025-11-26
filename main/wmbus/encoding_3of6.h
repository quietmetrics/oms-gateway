#pragma once
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    WMBUS_DEC_OK = 0,
    WMBUS_DEC_ERROR = 1,
} wmbus_dec_status_t;

void wmbus_encode_3of6(uint8_t *uncoded, uint8_t *encoded, uint8_t last_byte);
wmbus_dec_status_t wmbus_decode_3of6(uint8_t *encoded, uint8_t *decoded, uint8_t last_byte);
