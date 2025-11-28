// High-level RX pipeline for wM-Bus T-mode using CC1101
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "radio/cc1101_hal.h"
#include "wmbus/packet.h"

#define WMBUS_MAX_PACKET_BYTES   291
#define WMBUS_MAX_ENCODED_BYTES  584

typedef struct
{
    uint8_t *rx_packet;     // Decoded packet buffer (size WMBUS_MAX_PACKET_BYTES)
    uint8_t *rx_bytes;      // Encoded byte buffer (size WMBUS_MAX_ENCODED_BYTES)
    uint8_t *rx_logical;    // CRC-free packet buffer (size WMBUS_MAX_PACKET_BYTES)
    uint16_t packet_size;   // Decoded size (incl. all fields)
    uint16_t encoded_len;   // Encoded byte count read
    uint16_t logical_len;   // CRC-free length (L+1) if rx_logical is provided
    WmbusFrameInfo frame_info; // Parsed header + payload len if rx_logical provided
    uint8_t l_field;        // L-field value
    bool complete;
    uint8_t status;         // WMBUS_PKT_xxx
    float rssi_dbm;
    uint8_t lqi;
    uint8_t rssi_raw;
    uint8_t lqi_raw;
    uint8_t marc_state;
    uint8_t pkt_status;
} wmbus_rx_result_t;

esp_err_t wmbus_pipeline_init(cc1101_hal_t *dev);
esp_err_t wmbus_pipeline_receive(cc1101_hal_t *dev, wmbus_rx_result_t *res, uint32_t timeout_ms);
void wmbus_rx_set_low_sensitivity(bool enable);
void wmbus_rx_set_cs_level(cc1101_cs_level_t level);
void wmbus_rx_set_sync_mode(cc1101_sync_mode_t mode);
// Apply current RX knobs (low sensitivity / CS level / sync mode) to the radio.
// Call when radio is idle (e.g., before starting RX) after updating the setters.
esp_err_t wmbus_rx_apply_settings(cc1101_hal_t *dev);
