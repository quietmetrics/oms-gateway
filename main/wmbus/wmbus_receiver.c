#include <string.h>
#include "esp_log.h"
#include "wmbus_receiver.h"

static const char *TAG = "WMBUS_RX";

static wmbus_frame_callback_t frame_callback = NULL;
static wmbus_receiver_stats_t stats = {0};

esp_err_t wmbus_receiver_init(wmbus_frame_callback_t callback)
{
    frame_callback = callback;
    memset(&stats, 0, sizeof(stats));
    return ESP_OK;
}

const wmbus_receiver_stats_t* wmbus_receiver_get_stats(void)
{
    return &stats;
}

static void deliver_frame(const uint8_t *decoded, uint8_t decoded_length, const wmbus_frame_t *parsed_frame)
{
    if (!frame_callback) {
        return;
    }

    wmbus_decoded_frame_t event = {0};
    size_t copy_len = decoded_length;
    if (copy_len > sizeof(event.raw_bytes)) {
        copy_len = sizeof(event.raw_bytes);
    }
    memcpy(event.raw_bytes, decoded, copy_len);
    event.raw_length = (uint8_t)copy_len;

    event.frame = *parsed_frame;
    if (parsed_frame->data != NULL) {
        size_t offset = parsed_frame->data - decoded;
        if (offset < decoded_length) {
            event.frame.data = event.raw_bytes + offset;
        } else {
            event.frame.data = NULL;
            event.frame.data_length = 0;
        }
    } else {
        event.frame.data = NULL;
        event.frame.data_length = 0;
    }

    frame_callback(&event);
}

wmbus_error_t wmbus_receiver_process_encoded(const uint8_t *encoded, size_t encoded_length)
{
    if (!encoded || encoded_length == 0 || encoded_length > 255) {
        return WMBUS_ERROR_INVALID_INPUT;
    }

    uint8_t decoded[WMBUS_MAX_FRAME_LENGTH] = {0};
    uint8_t decoded_length = 0;
    wmbus_error_t err = decode_three_of_six(encoded, (uint8_t)encoded_length, decoded, &decoded_length);
    if (err != WMBUS_SUCCESS) {
        stats.decode_errors++;
        ESP_LOGD(TAG, "3-of-6 decode failed: %d", err);
        return err;
    }

    wmbus_frame_t frame = {0};
    err = parse_wmbus_frame(decoded, decoded_length, &frame);
    if (err != WMBUS_SUCCESS) {
        stats.parse_errors++;
        ESP_LOGD(TAG, "Frame parsing failed: %d", err);
        return err;
    }

    stats.frames_decoded++;
    deliver_frame(decoded, decoded_length, &frame);
    return WMBUS_SUCCESS;
}
