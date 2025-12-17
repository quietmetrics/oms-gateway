#include "app/http_server.h"

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "app/config.h"
#include <inttypes.h>
#include "app/net/backend.h"
#include "app/net/wifi.h"
#include "app/wmbus/frame_parse.h"
#include "app/wmbus/parsed_frame.h"
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
    uint8_t dev_type;
    uint8_t version;
} pkt_addr_t;

typedef struct
{
    uint8_t hdr_type; // TPL header type (wmbus_tpl_header_type_t)
    bool has_tpl;
    uint8_t acc;
    uint8_t status;
    uint16_t cfg;
    uint8_t sec_mode;
    uint16_t sec_len;
    bool encrypted;
} pkt_tpl_t;

typedef struct
{
    bool has_ell;
    uint8_t cc;
    uint8_t acc;
    uint8_t ext[8];
    uint8_t ext_len;
} pkt_ell_t;

typedef struct
{
    bool has_afl;
    uint8_t tag;
    uint8_t afll;
    uint16_t offset;
    uint16_t payload_offset;
    uint16_t payload_len;
} pkt_afl_t;

#define PKT_RAW_MAX (WMBUS_FIXED_HEADER_BYTES + 300)

typedef struct
{
    wmbus_parsed_frame_t frame;
    float rssi;
    uint32_t payload_len;
    bool whitelisted;
    uint8_t raw[PKT_RAW_MAX];
    uint16_t raw_len;
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

static void bytes_to_hex(const uint8_t *in, uint16_t len, char *out, size_t out_len)
{
    if (!out || out_len == 0)
    {
        return;
    }
    if (!in || len == 0)
    {
        out[0] = '\0';
        return;
    }
    static const char *hex = "0123456789ABCDEF";
    size_t idx = 0;
    for (uint16_t i = 0; i < len && (idx + 2) < out_len; ++i)
    {
        uint8_t byte = in[i];
        out[idx++] = hex[(byte >> 4) & 0x0F];
        out[idx++] = hex[byte & 0x0F];
    }
    out[idx] = '\0';
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
    if (!evt->logical_packet || evt->logical_len == 0)
    {
        return;
    }

    wmbus_raw_frame_t raw = {
        .bytes = evt->logical_packet,
        .len = evt->logical_len,
    };
    wmbus_parsed_frame_t pf;
    wmbus_parsed_frame_init(&pf, &raw, &evt->frame_info);
    wmbus_parsed_frame_parse_meta(&pf);

    pkt_entry_t e = {0};
    e.frame = pf;
    e.rssi = evt->rssi_dbm;
    e.payload_len = evt->frame_info.payload_len;
    if (evt->gateway_name)
    {
        strlcpy(e.frame.dll.gateway, evt->gateway_name, sizeof(e.frame.dll.gateway));
    }
    else
    {
        e.frame.dll.gateway[0] = '\0';
    }
    bool allowed = false;
    if (s_services)
    {
        const wmbus_whitelist_t *wl = services_whitelist(s_services);
        allowed = wmbus_whitelist_contains(wl, e.frame.dll.manuf, evt->frame_info.header.id);
    }
    e.whitelisted = allowed;
    e.raw_len = evt->logical_len > PKT_RAW_MAX ? PKT_RAW_MAX : evt->logical_len;
    if (e.raw_len && evt->logical_packet)
    {
        memcpy(e.raw, evt->logical_packet, e.raw_len);
    }
    e.frame.raw.bytes = e.raw;
    e.frame.raw.len = e.raw_len;
    e.frame.info.logical_len = e.raw_len;

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
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    const char *start = "{\"packets\":[";
    if (httpd_resp_send_chunk(req, start, strlen(start)) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (s_pkt_mutex && xSemaphoreTake(s_pkt_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        size_t limit = s_pkt_count;
        if (limit > 25)
        {
            limit = 25;
        }
        for (size_t i = 0; i < limit; i++)
        {
            const pkt_entry_t *p = &s_pkt_buf[i];
            const char *hdr = "none";
            if (p->frame.tpl.tpl.header_type == WMBUS_TPL_HDR_SHORT)
            {
                hdr = "tpl_short";
            }
            else if (p->frame.tpl.tpl.header_type == WMBUS_TPL_HDR_LONG)
            {
                hdr = "tpl_long";
            }
            char acc_buf[12];
            const char *acc = p->frame.tpl.has_tpl ? (snprintf(acc_buf, sizeof(acc_buf), "%u", p->frame.tpl.tpl.acc), acc_buf) : "null";
            char status_buf[12];
            const char *status = p->frame.tpl.has_tpl ? (snprintf(status_buf, sizeof(status_buf), "%u", p->frame.tpl.tpl.status), status_buf) : "null";
            char cfg_buf[16];
            const char *cfg = p->frame.tpl.has_tpl ? (snprintf(cfg_buf, sizeof(cfg_buf), "%u", p->frame.tpl.tpl.cfg), cfg_buf) : "null";
            char tpl_ci_buf[12];
            const char *tpl_ci = p->frame.tpl.has_tpl ? (snprintf(tpl_ci_buf, sizeof(tpl_ci_buf), "%u", p->frame.tpl.tpl.ci), tpl_ci_buf) : "null";
            char tpl_mode_buf[8];
            const char *tpl_mode = (p->frame.tpl.has_tpl && p->frame.tpl.tpl.cfg_info.mode) ? (snprintf(tpl_mode_buf, sizeof(tpl_mode_buf), "%u", p->frame.tpl.tpl.cfg_info.mode), tpl_mode_buf) : "null";
            char tpl_content_buf[8];
            const char *tpl_content = p->frame.tpl.has_tpl ? (snprintf(tpl_content_buf, sizeof(tpl_content_buf), "%u", p->frame.tpl.tpl.cfg_info.content), tpl_content_buf) : "null";
            char tpl_index_buf[8];
            const char *tpl_index = p->frame.tpl.has_tpl ? (snprintf(tpl_index_buf, sizeof(tpl_index_buf), "%u", p->frame.tpl.tpl.cfg_info.content_index), tpl_index_buf) : "null";
            char sec_mode_buf[8];
            const char *sec_mode = (p->frame.tpl.tpl.header_type && p->frame.tpl.sec.security_mode) ? (snprintf(sec_mode_buf, sizeof(sec_mode_buf), "%u", p->frame.tpl.sec.security_mode), sec_mode_buf) : "null";
            char sec_len_buf[12];
            const char *sec_len = (p->frame.tpl.sec.enc_len) ? (snprintf(sec_len_buf, sizeof(sec_len_buf), "%u", p->frame.tpl.sec.enc_len), sec_len_buf) : "null";
            char ell_ext_buf[20] = {0};
            if (p->frame.ell.has_ell && p->frame.ell.ell.ext_len)
            {
                for (uint8_t j = 0; j < p->frame.ell.ell.ext_len && (j * 2 + 2) < sizeof(ell_ext_buf); j++)
                {
                    snprintf(ell_ext_buf + j * 2, sizeof(ell_ext_buf) - j * 2, "%02X", p->frame.ell.ell.ext[j]);
                }
            }
            const char *ell_ext = (p->frame.ell.has_ell && p->frame.ell.ell.ext_len) ? ell_ext_buf : "null";
            char ell_cc_buf[8];
            const char *ell_cc = p->frame.ell.has_ell ? (snprintf(ell_cc_buf, sizeof(ell_cc_buf), "%u", p->frame.ell.ell.cc), ell_cc_buf) : "null";
            char ell_acc_buf[8];
            const char *ell_acc = p->frame.ell.has_ell ? (snprintf(ell_acc_buf, sizeof(ell_acc_buf), "%u", p->frame.ell.ell.acc), ell_acc_buf) : "null";
            char afl_tag_buf[8];
            const char *afl_tag = p->frame.afl.has_afl ? (snprintf(afl_tag_buf, sizeof(afl_tag_buf), "%u", p->frame.afl.afl.tag), afl_tag_buf) : "null";
            char afl_afll_buf[8];
            const char *afl_afll = p->frame.afl.has_afl ? (snprintf(afl_afll_buf, sizeof(afl_afll_buf), "%u", p->frame.afl.afl.afll), afl_afll_buf) : "null";
            char afl_mcl_buf[8];
            const char *afl_mcl = p->frame.afl.has_afl ? (snprintf(afl_mcl_buf, sizeof(afl_mcl_buf), "%u", p->frame.afl.afl.mcl), afl_mcl_buf) : "null";
            char raw_hex_value[PKT_RAW_MAX * 2 + 1];
            char raw_hex_json[PKT_RAW_MAX * 2 + 3];
            const char *raw_hex = "null";
            if (p->raw_len > 0)
            {
                bytes_to_hex(p->raw, p->raw_len, raw_hex_value, sizeof(raw_hex_value));
                snprintf(raw_hex_json, sizeof(raw_hex_json), "\"%s\"", raw_hex_value);
                raw_hex = raw_hex_json;
            }
            char entry[1024];
            int written = snprintf(entry, sizeof(entry),
                                   "%s{\"gateway\":\"%s\",\"manuf\":%u,\"id\":\"%s\",\"control\":%u,\"dev_type\":%u,"
                                   "\"version\":%u,\"ci\":%u,\"ci_class\":%u,\"hdr\":\"%s\",\"acc\":%s,\"status\":%s,\"cfg\":%s,"
                                   "\"tpl_ci\":%s,\"tpl_mode\":%s,\"tpl_content\":%s,\"tpl_index\":%s,"
                                   "\"sec_mode\":%s,\"sec_len\":%s,\"encrypted\":%s,"
                                   "\"ell_cc\":%s,\"ell_acc\":%s,\"ell_ext\":%s,"
                                   "\"afl_tag\":%s,\"afl_afll\":%s,\"afl_mcl\":%s,\"afl_offset\":%" PRIu16 ",\"afl_payload_len\":%" PRIu16 ","
                                   "\"raw_hex\":%s,"
                                   "\"rssi\":%.1f,\"payload_len\":%" PRIu32 ","
                                   "\"whitelisted\":%s}",
                                   (i == 0) ? "" : ",",
                                   p->frame.dll.gateway,
                                   p->frame.dll.manuf,
                                   p->frame.dll.id_str,
                                   p->frame.dll.c,
                                   p->frame.dll.dev_type,
                                   p->frame.dll.version,
                                   p->frame.dll.ci,
                                   p->frame.ci_class,
                                   hdr,
                                   acc,
                                   status,
                                   cfg,
                                   tpl_ci,
                                   tpl_mode,
                                   tpl_content,
                                   tpl_index,
                                   sec_mode,
                                   sec_len,
                                   p->frame.encrypted ? "true" : "false",
                                   ell_cc,
                                   ell_acc,
                                   ell_ext,
                                   afl_tag,
                                   afl_afll,
                                   afl_mcl,
                                   p->frame.afl.afl.offset,
                                   p->frame.afl.afl.payload_len,
                                   raw_hex,
                                   p->rssi,
                                   p->payload_len,
                                   p->whitelisted ? "true" : "false");
            if (written < 0)
            {
                continue;
            }
            // Ensure chunked send does not include truncated garbage
            entry[sizeof(entry) - 1] = '\0';
            if (httpd_resp_send_chunk(req, entry, strlen(entry)) != ESP_OK)
            {
                break;
            }
        }
        xSemaphoreGive(s_pkt_mutex);
    }
    const char *end = "]}";
    httpd_resp_send_chunk(req, end, strlen(end));
    return httpd_resp_send_chunk(req, NULL, 0);
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
