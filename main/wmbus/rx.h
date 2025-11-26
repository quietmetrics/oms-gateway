#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "cc1101/cc1101.h"
#include "wmbus/packet.h"

typedef struct {
    uint8_t *rx_packet;
    uint8_t *rx_bytes;
    uint16_t packet_size;
    uint16_t encoded_len;
    bool complete;
    float rssi_dbm;
    uint8_t lqi;
    bool crc_ok;
    uint8_t rssi_raw;
    uint8_t lqi_raw;
    uint8_t marc_state;
    uint8_t pkt_status;
} wmbus_rx_result_t;

esp_err_t wmbus_rx_init(cc1101_handle_t *cc);
esp_err_t wmbus_rx_start(wmbus_rx_result_t *result);
esp_err_t wmbus_rx_stop(void);
