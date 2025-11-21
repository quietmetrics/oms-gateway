#ifndef WMBUS_RECEIVER_H
#define WMBUS_RECEIVER_H

#include <stddef.h>
#include <esp_err.h>
#include "wmbus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    wmbus_frame_t frame;
    uint8_t raw_bytes[WMBUS_MAX_FRAME_LENGTH];
    uint8_t raw_length;
} wmbus_decoded_frame_t;

typedef void (*wmbus_frame_callback_t)(const wmbus_decoded_frame_t *frame);

typedef struct {
    uint32_t frames_decoded;
    uint32_t decode_errors;
    uint32_t parse_errors;
} wmbus_receiver_stats_t;

esp_err_t wmbus_receiver_init(wmbus_frame_callback_t callback);
wmbus_error_t wmbus_receiver_process_encoded(const uint8_t *encoded, size_t encoded_length);
const wmbus_receiver_stats_t* wmbus_receiver_get_stats(void);

#ifdef __cplusplus
}
#endif

#endif // WMBUS_RECEIVER_H
