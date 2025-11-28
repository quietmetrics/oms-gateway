// Simple observer-style router for decoded wM-Bus packet events.
#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "wmbus/packet.h"

typedef struct
{
    WmbusFrameInfo frame_info; // Parsed header + payload length
    uint8_t status;            // WMBUS_PKT_xxx
    float rssi_dbm;
    uint8_t lqi;
    const uint8_t *raw_packet; // On-air bytes incl. CRC blocks
    uint16_t raw_len;
    const uint8_t *logical_packet; // CRC-free packet (L|C|M|ID|Ver|Dev|CI|payload)
    uint16_t logical_len;
    const uint8_t *encoded;    // Encoded (3-of-6) bytes
    uint16_t encoded_len;
    const char *gateway_name;  // Optional identifier/hostname for backend tagging
} WmbusPacketEvent;

typedef void (*wmbus_packet_sink_fn)(const WmbusPacketEvent *evt, void *user);

// Initialize router storage.
esp_err_t wmbus_packet_router_init(void);
// Register a sink; returns ESP_ERR_NO_MEM if max sinks reached.
esp_err_t wmbus_packet_router_register(wmbus_packet_sink_fn fn, void *user);
// Dispatch event to all registered sinks (runs synchronously).
void wmbus_packet_router_dispatch(const WmbusPacketEvent *evt);
