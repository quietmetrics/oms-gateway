#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_check.h"
#include "whitelist/whitelist.h"
#include "config/config_manager.h"
#include <string.h>

static const char *TAG = "web";
static httpd_handle_t s_server = NULL;
static web_server_ctx_t s_ctx;

static esp_err_t send_json(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_get_whitelist(httpd_req_t *req)
{
    whitelist_entry_t list[16];
    size_t total = whitelist_list(list, 16);
    char buf[512];
    int len = snprintf(buf, sizeof(buf), "{\"total\":%d,\"entries\":[", (int)total);
    for (size_t i = 0; i < total && len < (int)(sizeof(buf) - 20); i++) {
        len += snprintf(buf + len, sizeof(buf) - len, "\"%s\"%s", list[i].address_hex, (i + 1 == total) ? "" : ",");
    }
    snprintf(buf + len, sizeof(buf) - len, "]}");
    return send_json(req, buf);
}

static esp_err_t handle_add_whitelist(httpd_req_t *req)
{
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char addr[32];
        if (httpd_query_key_value(query, "addr", addr, sizeof(addr)) == ESP_OK) {
            whitelist_add(addr);
        }
    }
    return httpd_resp_sendstr(req, "OK");
}

static esp_err_t handle_delete_whitelist(httpd_req_t *req)
{
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char addr[32];
        if (httpd_query_key_value(query, "addr", addr, sizeof(addr)) == ESP_OK) {
            whitelist_remove(addr);
        }
    }
    return httpd_resp_sendstr(req, "OK");
}

static esp_err_t handle_get_config(httpd_req_t *req)
{
    const app_config_t *cfg = s_ctx.cfg;
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"ssid\":\"%s\",\"hostname\":\"%s\",\"backend_host\":\"%s\",\"backend_port\":%d,\"backend_https\":%s}",
             cfg->ssid, cfg->hostname, cfg->backend_host, cfg->backend_port, cfg->backend_https ? "true" : "false");
    return send_json(req, buf);
}

static esp_err_t handle_update_config(httpd_req_t *req)
{
    // Minimal form-based update for now
    char query[256];
    app_config_t new_cfg = *s_ctx.cfg;
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val[128];
        if (httpd_query_key_value(query, "ssid", val, sizeof(val)) == ESP_OK) strncpy(new_cfg.ssid, val, sizeof(new_cfg.ssid) - 1);
        if (httpd_query_key_value(query, "password", val, sizeof(val)) == ESP_OK) strncpy(new_cfg.password, val, sizeof(new_cfg.password) - 1);
        if (httpd_query_key_value(query, "hostname", val, sizeof(val)) == ESP_OK) strncpy(new_cfg.hostname, val, sizeof(new_cfg.hostname) - 1);
        if (httpd_query_key_value(query, "backend_host", val, sizeof(val)) == ESP_OK) strncpy(new_cfg.backend_host, val, sizeof(new_cfg.backend_host) - 1);
        if (httpd_query_key_value(query, "backend_port", val, sizeof(val)) == ESP_OK) new_cfg.backend_port = atoi(val);
        if (httpd_query_key_value(query, "backend_https", val, sizeof(val)) == ESP_OK) new_cfg.backend_https = (strcmp(val, "1") == 0 || strcasecmp(val, "true") == 0);
        esp_err_t err = config_manager_save(&new_cfg);
        if (err == ESP_OK) {
            *(app_config_t *)s_ctx.cfg = new_cfg;
            return httpd_resp_sendstr(req, "OK");
        }
    }
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
    return ESP_FAIL;
}

static esp_err_t handle_root(httpd_req_t *req)
{
    const app_config_t *cfg = s_ctx.cfg;
    char html[1024];
    int len = snprintf(html, sizeof(html),
                       "<html><body><h2>OMS Gateway</h2>"
                       "<p>Hostname: %s</p>"
                       "<p>Backend: %s://%s:%d</p>"
                       "<h3>Whitelist</h3>",
                       cfg->hostname,
                       cfg->backend_https ? "https" : "http",
                       cfg->backend_host,
                       cfg->backend_port);
    httpd_resp_send_chunk(req, html, len);

    whitelist_entry_t list[16];
    size_t total = whitelist_list(list, 16);
    for (size_t i = 0; i < total; i++) {
        char row[128];
        len = snprintf(row, sizeof(row), "<div>%s</div>", list[i].address_hex);
        httpd_resp_send_chunk(req, row, len);
    }
    httpd_resp_send_chunk(req, "<form action=\"/api/whitelist\" method=\"get\">Add whitelist addr: <input name=\"addr\"/><input type=\"submit\" value=\"Add\"/></form>", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<p>Minimal UI - configuration endpoints TBD.</p></body></html>", HTTPD_RESP_USE_STRLEN);
    return httpd_resp_send_chunk(req, NULL, 0);
}

esp_err_t web_server_start(const web_server_ctx_t *ctx)
{
    if (s_server) return ESP_OK;
    s_ctx = *ctx;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);
    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &config), TAG, "start");

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_root,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &root);

    httpd_uri_t wl_get = {
        .uri = "/api/whitelist",
        .method = HTTP_GET,
        .handler = handle_get_whitelist,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &wl_get);

    httpd_uri_t wl_post = {
        .uri = "/api/whitelist",
        .method = HTTP_POST,
        .handler = handle_add_whitelist,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &wl_post);

    httpd_uri_t wl_del = {
        .uri = "/api/whitelist",
        .method = HTTP_DELETE,
        .handler = handle_delete_whitelist,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &wl_del);

    httpd_uri_t cfg_get = {
        .uri = "/api/config",
        .method = HTTP_GET,
        .handler = handle_get_config,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &cfg_get);

    httpd_uri_t cfg_post = {
        .uri = "/api/config",
        .method = HTTP_POST,
        .handler = handle_update_config,
        .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &cfg_post);
    return ESP_OK;
}

void web_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}
