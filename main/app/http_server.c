#include "app/http_server.h"

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "app/config.h"
#include <inttypes.h>
#include "app/net/backend.h"
#include "app/net/wifi.h"
#include "app/wmbus/whitelist.h"
#include "app/wmbus/packet_router.h"
#include "freertos/semphr.h"

extern const unsigned char index_html_start[] asm("_binary_index_html_start");
extern const unsigned char index_html_end[] asm("_binary_index_html_end");
extern const unsigned char app_js_start[] asm("_binary_app_js_start");
extern const unsigned char app_js_end[] asm("_binary_app_js_end");
extern const unsigned char style_css_start[] asm("_binary_style_css_start");
extern const unsigned char style_css_end[] asm("_binary_style_css_end");

#define ICON_DECLARE(name)                                              \
    extern const unsigned char _binary_##name##_svg_start[]             \
        asm("_binary_" #name "_svg_start");                             \
    extern const unsigned char _binary_##name##_svg_end[]               \
        asm("_binary_" #name "_svg_end")

ICON_DECLARE(wifi);
ICON_DECLARE(cloud_done);
ICON_DECLARE(cloud_off);
ICON_DECLARE(shield);
ICON_DECLARE(sensors);
ICON_DECLARE(trash);
ICON_DECLARE(podcast);

typedef struct
{
    const char *name;
    const unsigned char *start;
    const unsigned char *end;
} icon_entry_t;

#define ICON_ENTRY(name)             \
    {                                \
        #name ".svg", _binary_##name##_svg_start, _binary_##name##_svg_end \
    }

static const icon_entry_t ICONS[] = {
    ICON_ENTRY(wifi),
    ICON_ENTRY(cloud_done),
    ICON_ENTRY(cloud_off),
    ICON_ENTRY(shield),
    ICON_ENTRY(sensors),
    ICON_ENTRY(trash),
    ICON_ENTRY(podcast),
};


static const char *TAG = "http";
static httpd_handle_t s_server = NULL;
static services_state_t *s_services = NULL;
static SemaphoreHandle_t s_pkt_mutex = NULL;
static bool s_sink_registered = false;
static bool s_backend_reachable = false;
static bool s_backend_has_probe = false;

typedef struct
{
    char gateway[APP_HOSTNAME_MAX];
    uint16_t manuf;
    char id[9];
    uint8_t control;
    uint8_t dev_type;
    uint8_t version;
    uint8_t ci;
    float rssi;
    uint32_t payload_len;
    bool whitelisted;
} pkt_entry_t;

#define PKT_BUF_MAX 40
static pkt_entry_t s_pkt_buf[PKT_BUF_MAX];
static size_t s_pkt_count = 0;

static esp_err_t send_ok(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t send_err(httpd_req_t *req, const char *code, const char *msg)
{
    httpd_resp_set_status(req, code);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, msg);
}

void http_server_record_packet(const WmbusPacketEvent *evt)
{
    if (!evt || !evt->frame_info.parsed)
    {
        return;
    }
    if (!s_pkt_mutex)
    {
        return;
    }
    pkt_entry_t e = {0};
    e.manuf = evt->frame_info.header.manufacturer_le;
    snprintf(e.id, sizeof(e.id), "%02X%02X%02X%02X",
             evt->frame_info.header.id[3], evt->frame_info.header.id[2],
             evt->frame_info.header.id[1], evt->frame_info.header.id[0]);
    e.control = evt->frame_info.header.control;
    e.dev_type = evt->frame_info.header.device_type;
    e.version = evt->frame_info.header.version;
    e.ci = evt->frame_info.header.ci_field;
    e.rssi = evt->rssi_dbm;
    e.payload_len = evt->frame_info.payload_len;
    if (evt->gateway_name)
    {
        strlcpy(e.gateway, evt->gateway_name, sizeof(e.gateway));
    }
    else
    {
        e.gateway[0] = '\0';
    }
    bool allowed = false;
    if (s_services)
    {
        const wmbus_whitelist_t *wl = services_whitelist(s_services);
        allowed = wmbus_whitelist_contains(wl, e.manuf, evt->frame_info.header.id);
    }
    e.whitelisted = allowed;

    if (xSemaphoreTake(s_pkt_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        if (s_pkt_count < PKT_BUF_MAX)
        {
            memmove(&s_pkt_buf[1], &s_pkt_buf[0], s_pkt_count * sizeof(pkt_entry_t));
            s_pkt_buf[0] = e;
            s_pkt_count++;
        }
        else
        {
            memmove(&s_pkt_buf[1], &s_pkt_buf[0], (PKT_BUF_MAX - 1) * sizeof(pkt_entry_t));
            s_pkt_buf[0] = e;
        }
        xSemaphoreGive(s_pkt_mutex);
    }
}

static void http_pkt_sink(const WmbusPacketEvent *evt, void *user)
{
    (void)user;
    http_server_record_packet(evt);
}

static uint16_t parse_hex16(const char *s)
{
    return (uint16_t)strtoul(s, NULL, 16);
}

static void url_decode_inplace(char *s)
{
    if (!s)
    {
        return;
    }
    char *src = s;
    char *dst = s;
    while (*src)
    {
        if (*src == '%' && src[1] && src[2])
        {
            char hex[3] = {src[1], src[2], '\0'};
            *dst = (char)strtol(hex, NULL, 16);
            src += 3;
        }
        else if (*src == '+')
        {
            *dst = ' ';
            src++;
        }
        else
        {
            *dst = *src;
            src++;
        }
        dst++;
    }
    *dst = '\0';
}

static void format_whitelist(char *buf, size_t cap, const wmbus_whitelist_t *wl)
{
    size_t pos = 0;
    pos += snprintf(buf + pos, cap - pos, "{\"entries\":[");
    size_t count = 0;
    if (wl)
    {
        count = wl->count;
        if (count > WMBUS_WHITELIST_MAX)
        {
            count = WMBUS_WHITELIST_MAX;
        }
    }
    for (size_t i = 0; i < count && pos < cap; i++)
    {
        const uint8_t *id = wl->entries[i].id;
        pos += snprintf(buf + pos, cap - pos, "%s{\"manuf\":\"%04X\",\"id\":\"%02X%02X%02X%02X\"}",
                        (i == 0) ? "" : ",",
                        wl->entries[i].manufacturer_le,
                        id[3], id[2], id[1], id[0]);
    }
    if (pos < cap)
    {
        snprintf(buf + pos, cap - pos, "]}");
    }
}

static esp_err_t handle_static(httpd_req_t *req, const unsigned char *start, const unsigned char *end, const char *type)
{
    httpd_resp_set_type(req, type);
    size_t len = (size_t)(end - start);
    return httpd_resp_send(req, (const char *)start, len);
}

static esp_err_t handle_root(httpd_req_t *req)
{
    return handle_static(req, index_html_start, index_html_end, "text/html");
}

static esp_err_t handle_static_js(httpd_req_t *req)
{
    return handle_static(req, app_js_start, app_js_end, "application/javascript");
}

static esp_err_t handle_static_css(httpd_req_t *req)
{
    return handle_static(req, style_css_start, style_css_end, "text/css");
}

static esp_err_t handle_static_icon(httpd_req_t *req)
{
    const char *uri = req->uri;
    if (strncmp(uri, "/static/icons/", 14) != 0)
    {
        return httpd_resp_send_404(req);
    }
    const char *name = uri + 14;
    for (size_t i = 0; i < sizeof(ICONS) / sizeof(ICONS[0]); i++)
    {
        if (strcmp(name, ICONS[i].name) == 0)
        {
            return handle_static(req, ICONS[i].start, ICONS[i].end, "image/svg+xml");
        }
    }
    return httpd_resp_send_404(req);
}

static esp_err_t handle_status(httpd_req_t *req)
{
    char wl_json[1024];
    format_whitelist(wl_json, sizeof(wl_json), services_whitelist(s_services));

    app_wifi_status_t wifi = {0};
    services_get_wifi_status(&wifi);
    app_radio_status_t radio = {0};
    services_get_radio_status(s_services, &radio);
    app_ap_status_t ap = {0};
    services_get_ap_status(&ap);
    char backend_url[192] = {0};
    services_get_backend_url(s_services, backend_url, sizeof(backend_url));
    bool backend_ok = (backend_url[0] != '\0') && s_backend_has_probe && s_backend_reachable;

    char json[2048];
    int n = snprintf(json, sizeof(json),
                     "{\"hostname\":\"%s\",\"wifi\":{\"connected\":%s,\"ssid\":\"%s\",\"ip\":\"%s\",\"has_pass\":%s,"
                     "\"rssi\":%d,\"gateway\":\"%s\",\"dns\":\"%s\"},"
                     "\"ap\":{\"ssid\":\"%s\",\"channel\":%u,\"has_pass\":%s},"
                     "\"backend\":{\"url\":\"%s\",\"reachable\":%s},\"radio\":{\"cs_level\":%u,\"sync_mode\":%u},\"whitelist\":%s}",
                     services_hostname(s_services),
                     wifi.connected ? "true" : "false",
                     wifi.ssid,
                     wifi.ip,
                     wifi.has_pass ? "true" : "false",
                     wifi.rssi,
                     wifi.gateway,
                     wifi.dns,
                     ap.ssid,
                     ap.channel,
                     ap.has_pass ? "true" : "false",
                     backend_url,
                     backend_ok ? "true" : "false",
                     radio.cs_level,
                     radio.sync_mode,
                    wl_json);
    if (n < 0 || n >= (int)sizeof(json))
    {
        return httpd_resp_send_500(req);
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, n);
}

static esp_err_t handle_backend(httpd_req_t *req)
{
    char query[256] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        char url[192] = {0};
        if (httpd_query_key_value(query, "url", url, sizeof(url)) == ESP_OK)
        {
            url_decode_inplace(url);
            services_set_backend_url(s_services, url);
            s_backend_has_probe = false;
            s_backend_reachable = false;
        }
    }
    return send_ok(req);
}

static esp_err_t handle_backend_test(httpd_req_t *req)
{
    char query[256] = {0};
    char url[192] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        httpd_query_key_value(query, "url", url, sizeof(url));
    }
    url_decode_inplace(url);
    const char *target = (url[0] != '\0') ? url : NULL;
    if (!target)
    {
        return send_err(req, "400", "{\"error\":\"missing url\"}");
    }
    esp_err_t err = backend_check_url(target, 1500);
    httpd_resp_set_type(req, "application/json");
    s_backend_has_probe = true;
    s_backend_reachable = (err == ESP_OK);
    if (s_backend_reachable)
    {
        return httpd_resp_sendstr(req, "{\"reachable\":true}");
    }
    httpd_resp_set_status(req, "503");
    return httpd_resp_sendstr(req, "{\"reachable\":false}");
}

static esp_err_t handle_wifi(httpd_req_t *req)
{
    char query[256] = {0};
    char ssid[32] = {0};
    char pass[64] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        httpd_query_key_value(query, "ssid", ssid, sizeof(ssid));
        httpd_query_key_value(query, "pass", pass, sizeof(pass));
    }
    url_decode_inplace(ssid);
    url_decode_inplace(pass);
    if (ssid[0] == '\0')
    {
        return send_err(req, "400", "{\"error\":\"ssid required\"}");
    }
    services_set_wifi_credentials(s_services, ssid, pass);
    return send_ok(req);
}

static esp_err_t handle_hostname(httpd_req_t *req)
{
    char query[128] = {0};
    char name[APP_HOSTNAME_MAX] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        httpd_query_key_value(query, "name", name, sizeof(name));
    }
    url_decode_inplace(name);
    if (name[0] == '\0')
    {
        return send_err(req, "400", "{\"error\":\"name required\"}");
    }
    services_set_hostname(s_services, name);
    return send_ok(req);
}

static esp_err_t handle_ap(httpd_req_t *req)
{
    char query[128] = {0};
    char ssid[32] = {0};
    char pass[64] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        httpd_query_key_value(query, "ssid", ssid, sizeof(ssid));
        httpd_query_key_value(query, "pass", pass, sizeof(pass));
    }
    url_decode_inplace(ssid);
    url_decode_inplace(pass);
    if (ssid[0] == '\0')
    {
        return send_err(req, "400", "{\"error\":\"ssid required\"}");
    }
    services_set_ap_config(s_services, ssid, pass, 1);
    return send_ok(req);
}

static esp_err_t handle_radio(httpd_req_t *req)
{
    char query[128] = {0};
    char cs[8] = {0};
    char sync[8] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        httpd_query_key_value(query, "cs", cs, sizeof(cs));
        httpd_query_key_value(query, "sync", sync, sizeof(sync));
    }
    if (cs[0])
    {
        services_set_radio_cs_level(s_services, (uint8_t)atoi(cs));
    }
    if (sync[0])
    {
        services_set_radio_sync_mode(s_services, (uint8_t)atoi(sync));
    }
    return send_ok(req);
}

static esp_err_t handle_wl_add(httpd_req_t *req)
{
    char query[128] = {0};
    char manuf_s[8] = {0};
    char id_s[16] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        httpd_query_key_value(query, "manuf", manuf_s, sizeof(manuf_s));
        httpd_query_key_value(query, "id", id_s, sizeof(id_s));
    }
    if (manuf_s[0] == '\0' || strlen(id_s) != 8)
    {
        return send_err(req, "400", "{\"error\":\"invalid manuf/id\"}");
    }
    uint16_t manuf = parse_hex16(manuf_s);
    uint8_t id[4] = {0};
    for (int i = 0; i < 4; i++)
    {
        char byte_s[3] = {id_s[(3 - i) * 2], id_s[(3 - i) * 2 + 1], '\0'};
        id[i] = (uint8_t)strtoul(byte_s, NULL, 16);
    }
    wmbus_whitelist_add(services_whitelist(s_services), manuf, id);
    return send_ok(req);
}

static esp_err_t handle_wl_del(httpd_req_t *req)
{
    char query[128] = {0};
    char manuf_s[8] = {0};
    char id_s[16] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
    {
        httpd_query_key_value(query, "manuf", manuf_s, sizeof(manuf_s));
        httpd_query_key_value(query, "id", id_s, sizeof(id_s));
    }
    if (manuf_s[0] == '\0' || strlen(id_s) != 8)
    {
        return send_err(req, "400", "{\"error\":\"invalid manuf/id\"}");
    }
    uint16_t manuf = parse_hex16(manuf_s);
    uint8_t id[4] = {0};
    for (int i = 0; i < 4; i++)
    {
        char byte_s[3] = {id_s[(3 - i) * 2], id_s[(3 - i) * 2 + 1], '\0'};
        id[i] = (uint8_t)strtoul(byte_s, NULL, 16);
    }
    wmbus_whitelist_remove(services_whitelist(s_services), manuf, id);
    return send_ok(req);
}

static esp_err_t handle_packets_stream(httpd_req_t *req)
{
    const size_t json_cap = 4096;
    char *json = calloc(1, json_cap);
    if (!json)
    {
        return httpd_resp_send_500(req);
    }
    size_t pos = 0;
    pos += snprintf(json + pos, json_cap - pos, "{\"packets\":[");
    if (s_pkt_mutex && xSemaphoreTake(s_pkt_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        size_t limit = s_pkt_count;
        if (limit > 25)
        {
            limit = 25;
        }
        for (size_t i = 0; i < limit && pos < json_cap; i++)
        {
            const pkt_entry_t *p = &s_pkt_buf[i];
            pos += snprintf(json + pos, json_cap - pos,
                            "%s{\"gateway\":\"%s\",\"manuf\":%u,\"id\":\"%s\",\"control\":%u,\"dev_type\":%u,"
                            "\"version\":%u,\"ci\":%u,\"rssi\":%.1f,\"payload_len\":%" PRIu32 ","
                            "\"whitelisted\":%s}",
                            (i == 0) ? "" : ",",
                            p->gateway,
                            p->manuf,
                            p->id,
                            p->control,
                            p->dev_type,
                            p->version,
                            p->ci,
                            p->rssi,
                            p->payload_len,
                            p->whitelisted ? "true" : "false");
        }
        xSemaphoreGive(s_pkt_mutex);
    }
    if (pos < json_cap)
    {
        pos += snprintf(json + pos, json_cap - pos, "]}");
    }
    httpd_resp_set_type(req, "application/json");
    esp_err_t r = httpd_resp_send(req, json, pos);
    free(json);
    return r;
}

static const httpd_uri_t URI_ROOT = {.uri = "/", .method = HTTP_GET, .handler = handle_root};
static const httpd_uri_t URI_STATUS = {.uri = "/api/status", .method = HTTP_GET, .handler = handle_status};
static const httpd_uri_t URI_BACKEND = {.uri = "/api/backend", .method = HTTP_POST, .handler = handle_backend};
static const httpd_uri_t URI_BACKEND_TEST = {.uri = "/api/backend/test", .method = HTTP_GET, .handler = handle_backend_test};
static const httpd_uri_t URI_WIFI = {.uri = "/api/wifi", .method = HTTP_POST, .handler = handle_wifi};
static const httpd_uri_t URI_HOST = {.uri = "/api/hostname", .method = HTTP_POST, .handler = handle_hostname};
static const httpd_uri_t URI_AP = {.uri = "/api/ap", .method = HTTP_POST, .handler = handle_ap};
static const httpd_uri_t URI_RADIO = {.uri = "/api/radio", .method = HTTP_POST, .handler = handle_radio};
static const httpd_uri_t URI_WL_ADD = {.uri = "/api/whitelist/add", .method = HTTP_POST, .handler = handle_wl_add};
static const httpd_uri_t URI_WL_DEL = {.uri = "/api/whitelist/del", .method = HTTP_POST, .handler = handle_wl_del};
static const httpd_uri_t URI_PKTS = {.uri = "/api/packets", .method = HTTP_GET, .handler = handle_packets_stream};
static const httpd_uri_t URI_STATIC_ICON = {.uri = "/static/icons/*", .method = HTTP_GET, .handler = handle_static_icon};
static const httpd_uri_t URI_STATIC_JS = {.uri = "/static/app.js", .method = HTTP_GET, .handler = handle_static_js};
static const httpd_uri_t URI_STATIC_CSS = {.uri = "/static/style.css", .method = HTTP_GET, .handler = handle_static_css};
// Wildcard handler for any /static/* path (allows cache-busting query etc.)
static esp_err_t handle_static_any(httpd_req_t *req)
{
    const char *uri = req->uri;
    const char *js = "/static/app.js";
    const char *css = "/static/style.css";
    if (strncmp(uri, js, strlen(js)) == 0)
    {
        return handle_static_js(req);
    }
    if (strncmp(uri, css, strlen(css)) == 0)
    {
        return handle_static_css(req);
    }
    httpd_resp_set_status(req, "404");
    return httpd_resp_sendstr(req, "not found");
}
static const httpd_uri_t URI_STATIC_ANY = {.uri = "/static/*", .method = HTTP_GET, .handler = handle_static_any};

esp_err_t http_server_start(services_state_t *svc)
{
    if (s_server)
    {
        return ESP_OK;
    }
    s_services = svc;
    s_pkt_mutex = xSemaphoreCreateMutex();

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 6144;
    cfg.server_port = 80;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.max_uri_handlers = 16;

    esp_err_t err = httpd_start(&s_server, &cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    httpd_register_uri_handler(s_server, &URI_ROOT);
    httpd_register_uri_handler(s_server, &URI_STATUS);
    httpd_register_uri_handler(s_server, &URI_BACKEND);
    httpd_register_uri_handler(s_server, &URI_BACKEND_TEST);
    httpd_register_uri_handler(s_server, &URI_WIFI);
    httpd_register_uri_handler(s_server, &URI_HOST);
    httpd_register_uri_handler(s_server, &URI_AP);
    httpd_register_uri_handler(s_server, &URI_RADIO);
    httpd_register_uri_handler(s_server, &URI_WL_ADD);
    httpd_register_uri_handler(s_server, &URI_WL_DEL);
    httpd_register_uri_handler(s_server, &URI_PKTS);
    httpd_register_uri_handler(s_server, &URI_STATIC_JS);
    httpd_register_uri_handler(s_server, &URI_STATIC_CSS);
    httpd_register_uri_handler(s_server, &URI_STATIC_ICON);
    httpd_register_uri_handler(s_server, &URI_STATIC_ANY);

    ESP_LOGI(TAG, "HTTP server started on port %d", cfg.server_port);
    return ESP_OK;
}

void http_server_stop(void)
{
    if (s_server)
    {
        httpd_stop(s_server);
        s_server = NULL;
    }
}

esp_err_t http_server_register_packet_sink(void)
{
    if (s_sink_registered)
    {
        return ESP_OK;
    }
    esp_err_t err = wmbus_packet_router_register(http_pkt_sink, NULL);
    if (err == ESP_OK)
    {
        s_sink_registered = true;
    }
    return err;
}
