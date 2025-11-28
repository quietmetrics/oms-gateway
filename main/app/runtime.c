#include "app/runtime.h"

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"

#include "radio/pins.h"
#include "radio/cc1101_hal.h"
#include "wmbus/pipeline.h"
#include "wmbus/packet.h"
#include "wmbus/device_types.h"
#include "app/wmbus/packet_router.h"
#include "app/wmbus/whitelist.h"
#include "app/net/backend.h"
#include "app/net/wifi.h"
#include "app/radio/radio_config.h"
#include "app/services.h"
#include "app/config_defaults.h"
#include "app/config.h"
#include "app/http_server.h"

static const char *TAG = "app";

typedef struct
{
    services_state_t services;
    cc1101_hal_t cc1101;
    cc1101_pin_config_t pins;
    uint8_t rx_packet[WMBUS_MAX_PACKET_BYTES];
    uint8_t rx_bytes[WMBUS_MAX_ENCODED_BYTES];
    uint8_t rx_logical[WMBUS_MAX_PACKET_BYTES];
} app_ctx_t;

static app_ctx_t s_app;

static void log_packet_summary(const wmbus_rx_result_t *res, const WmbusFrameInfo *info)
{
    if (!res || !info || !info->parsed)
    {
        return;
    }

    char manuf_hex[5] = {0};
    char id_hex[9] = {0};
    snprintf(manuf_hex, sizeof(manuf_hex), "%02X%02X", (uint8_t)(info->header.manufacturer_le & 0xFF), (uint8_t)((info->header.manufacturer_le >> 8) & 0xFF));
    snprintf(id_hex, sizeof(id_hex), "%02X%02X%02X%02X", info->header.id[3], info->header.id[2], info->header.id[1], info->header.id[0]);

    const oms_device_type_info_t *dev = oms_device_type_lookup(info->header.device_type);
    const char *dev_name = dev ? dev->name : "unknown";

    ESP_LOGI(TAG, "RX manuf=%s id=%s dev=0x%02X (%s) ver=0x%02X ci=0x%02X payload_len=%u rssi=%.1f",
             manuf_hex,
             id_hex,
             info->header.device_type,
             dev_name,
             info->header.version,
             info->header.ci_field,
             info->payload_len,
             res->rssi_dbm);
}

static void ui_sink(const WmbusPacketEvent *evt, void *user)
{
    (void)user;
    if (!evt)
    {
        return;
    }

    if (evt->status != WMBUS_PKT_OK || !evt->frame_info.parsed)
    {
        return;
    }

    ESP_LOGI(TAG, "[UI] gw=\"%s\" manuf=0x%04X id=%02X%02X%02X%02X dev=0x%02X rssi=%.1f",
             evt->gateway_name ? evt->gateway_name : "-",
             evt->frame_info.header.manufacturer_le,
             evt->frame_info.header.id[3], evt->frame_info.header.id[2], evt->frame_info.header.id[1], evt->frame_info.header.id[0],
             evt->frame_info.header.device_type,
             evt->rssi_dbm);
}

static void forwarder_sink(const WmbusPacketEvent *evt, void *user)
{
    services_state_t *svc = (services_state_t *)user;
    if (!evt || !evt->frame_info.parsed || !svc)
    {
        return;
    }

    const wmbus_whitelist_t *wl = services_whitelist(svc);
    const bool allowed = wmbus_whitelist_contains(wl, evt->frame_info.header.manufacturer_le, evt->frame_info.header.id);
    if (!allowed)
    {
        ESP_LOGI(TAG, "[FW] drop (not whitelisted) manuf=0x%04X id=%02X%02X%02X%02X",
                 evt->frame_info.header.manufacturer_le,
                 evt->frame_info.header.id[3], evt->frame_info.header.id[2], evt->frame_info.header.id[1], evt->frame_info.header.id[0]);
        return;
    }

    if (!wifi_sta_is_connected())
    {
        ESP_LOGI(TAG, "[FW] skip (no network)");
        return;
    }

    backend_config_t *backend = services_backend(svc);
    if (!backend || backend->url[0] == '\0')
    {
        ESP_LOGI(TAG, "[FW] backend URL not set, skipping forward");
        return;
    }

    esp_err_t err = backend_forward_packet(backend, evt);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "[FW] forward failed: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "[FW] forwarded manuf=0x%04X id=%02X%02X%02X%02X payload_len=%u gw=\"%s\"",
                 evt->frame_info.header.manufacturer_le,
                 evt->frame_info.header.id[3], evt->frame_info.header.id[2], evt->frame_info.header.id[1], evt->frame_info.header.id[0],
                 evt->frame_info.payload_len,
                 evt->gateway_name ? evt->gateway_name : "-");
    }
}

static esp_err_t system_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    return ESP_OK;
}

static void apply_test_defaults(services_state_t *svc)
{
    static const uint8_t test_id[4] = APP_TEST_ID;
    backend_set_url(services_backend(svc), APP_TEST_BACKEND_URL);
    wmbus_whitelist_add(services_whitelist(svc), APP_TEST_MANUF, test_id);
    wifi_sta_set_credentials(APP_TEST_WIFI_SSID, APP_TEST_WIFI_PASS);
}

static esp_err_t wifi_start_with_fallback(services_state_t *svc)
{
    if (wifi_sta_connect() != ESP_OK)
    {
        return wifi_ap_start_default(services_hostname(svc));
    }
    return ESP_OK;
}

static esp_err_t app_setup(app_ctx_t *ctx)
{
    ESP_ERROR_CHECK(system_init());
    ESP_ERROR_CHECK(services_init(&ctx->services));
    if (APP_USE_TEST_DEFAULTS)
    {
        apply_test_defaults(&ctx->services);
    }

    ESP_ERROR_CHECK(wmbus_packet_router_init());
    wmbus_packet_router_register(ui_sink, NULL);
    wmbus_packet_router_register(forwarder_sink, &ctx->services);

    ctx->pins = cc1101_default_pins();
    ESP_ERROR_CHECK(cc1101_hal_init(&ctx->pins, &ctx->cc1101));
    ESP_ERROR_CHECK(wmbus_pipeline_init(&ctx->cc1101));
    ESP_ERROR_CHECK(radio_config_apply(services_radio(&ctx->services), &ctx->cc1101));

    ESP_ERROR_CHECK(wifi_start_with_fallback(&ctx->services));
    ESP_ERROR_CHECK(http_server_start(&ctx->services));
    return ESP_OK;
}

static void app_loop(app_ctx_t *ctx)
{
    wmbus_rx_result_t res = {
        .rx_packet = ctx->rx_packet,
        .rx_bytes = ctx->rx_bytes,
        .rx_logical = ctx->rx_logical,
    };

    while (true)
    {
        ESP_ERROR_CHECK(wmbus_pipeline_receive(&ctx->cc1101, &res, APP_RX_TIMEOUT_MS));

        if (!res.complete || res.packet_size == 0 || res.encoded_len == 0)
        {
            ESP_LOGD(TAG, "RX incomplete: complete=%d packet_size=%u encoded_len=%u status=%u", res.complete, res.packet_size, res.encoded_len, res.status);
            vTaskDelay(pdMS_TO_TICKS(APP_RX_INCOMPLETE_DELAY_MS));
            continue;
        }

        if (res.status == WMBUS_PKT_OK)
        {
            if (res.frame_info.parsed)
            {
                log_packet_summary(&res, &res.frame_info);
            }
            else
            {
                ESP_LOGW(TAG, "Header parse failed");
            }
        }

        WmbusPacketEvent evt = {
            .frame_info = res.frame_info,
            .status = res.status,
            .rssi_dbm = res.rssi_dbm,
            .lqi = res.lqi,
            .raw_packet = res.rx_packet,
            .raw_len = res.packet_size,
            .encoded = res.rx_bytes,
            .encoded_len = res.encoded_len,
            .gateway_name = services_hostname(&ctx->services),
            .logical_packet = res.rx_logical,
            .logical_len = res.logical_len,
        };
        wmbus_packet_router_dispatch(&evt);

        vTaskDelay(pdMS_TO_TICKS(APP_RX_LOOP_DELAY_MS));
    }
}

void app_run(void)
{
    app_ctx_t *ctx = &s_app;
    memset(ctx, 0, sizeof(*ctx));
    ESP_ERROR_CHECK(app_setup(ctx));
    app_loop(ctx);
}
