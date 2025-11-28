// App-wide constants and DTOs for status/config reporting.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define APP_HOSTNAME_MAX 32
#define APP_LOG_RX_MODULE "app"
#define APP_RX_TIMEOUT_MS 1500
#define APP_RX_INCOMPLETE_DELAY_MS 20
#define APP_RX_LOOP_DELAY_MS 10

typedef struct
{
    bool connected;
    char ssid[32];
    char ip[16];
} app_wifi_status_t;

typedef struct
{
    char url[192];
    bool reachable;
} app_backend_status_t;

typedef struct
{
    uint8_t cs_level;
    uint8_t sync_mode;
} app_radio_status_t;
