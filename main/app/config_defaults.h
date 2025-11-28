// Central place for boot-time defaults (can be disabled when UI covers config).
#pragma once

#ifndef APP_USE_TEST_DEFAULTS
#define APP_USE_TEST_DEFAULTS 0
#endif

#define APP_TEST_BACKEND_URL ""
#define APP_TEST_MANUF 0x0000
#define APP_TEST_ID      \
    {                    \
        0, 0, 0, 0       \
    }
#define APP_TEST_WIFI_SSID ""
#define APP_TEST_WIFI_PASS ""
