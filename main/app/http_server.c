#include "app/http_server.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "app/config.h"
#include "app/net/backend.h"
#include "app/net/wifi.h"
#include "app/wmbus/whitelist.h"

static const char *TAG = "http";
static httpd_handle_t s_server = NULL;
static services_state_t *s_services = NULL;

static const char UI_HTML[] =
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"  <meta charset=\"utf-8\"/>\n"
"  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"/>\n"
"  <title>OMS Gateway</title>\n"
"  <style>\n"
"    :root { --bg:#0f172a; --card:#111827; --fg:#e2e8f0; --muted:#94a3b8; --accent:#22d3ee; --accent2:#7c3aed; --danger:#ef4444; font-family:Inter,Segoe UI,system-ui,sans-serif; }\n"
"    *{box-sizing:border-box;} body{margin:0;background:var(--bg);color:var(--fg);}\n"
"    header{position:sticky;top:0;z-index:10;backdrop-filter:blur(8px);background:rgba(15,23,42,0.8);border-bottom:1px solid #1f2937;padding:12px 16px;display:flex;align-items:center;justify-content:space-between;}\n"
"    h1{margin:0;font-size:18px;letter-spacing:0.4px;}\n"
"    .pill{display:inline-flex;align-items:center;gap:8px;padding:6px 10px;border-radius:999px;background:rgba(34,211,238,0.15);color:var(--accent);font-size:12px;}\n"
"    main{padding:16px;max-width:1024px;margin:0 auto;}\n"
"    .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:12px;}\n"
"    .card{background:var(--card);border:1px solid #1f2937;border-radius:14px;padding:16px;box-shadow:0 10px 30px rgba(0,0,0,0.15);}\n"
"    .card h2{margin:0 0 12px;font-size:15px;}\n"
"    label{display:block;font-size:12px;color:var(--muted);margin-bottom:6px;}\n"
"    input, select{width:100%;padding:10px;border-radius:10px;border:1px solid #1f2937;background:#0b1220;color:var(--fg);}\n"
"    button{cursor:pointer;border:none;border-radius:10px;padding:10px 14px;font-weight:600;color:#0b1220;background:var(--accent);transition:transform 120ms ease,opacity 120ms ease;}\n"
"    button:active{transform:translateY(1px);}\n"
"    .danger{background:var(--danger);color:#fff;}\n"
"    .muted{color:var(--muted);font-size:12px;}\n"
"    .row{display:flex;gap:10px;}\n"
"    .row > *{flex:1;}\n"
"    .list{border:1px solid #1f2937;border-radius:10px;overflow:hidden;}\n"
"    .list-item{display:flex;justify-content:space-between;align-items:center;padding:10px 12px;border-bottom:1px solid #1f2937;}\n"
"    .list-item:last-child{border-bottom:none;}\n"
"    .tag{display:inline-block;padding:4px 8px;border-radius:8px;background:#0b1220;border:1px solid #1f2937;font-size:11px;}\n"
"    .chips{display:flex;flex-wrap:wrap;gap:6px;}\n"
"    .section-title{font-size:13px;margin:0 0 6px;color:var(--muted);}\n"
"    .status{display:flex;gap:10px;flex-wrap:wrap;}\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"<header><div style=\"display:flex;align-items:center;gap:8px;\"><span style=\"width:10px;height:10px;border-radius:50%;background:#22c55e;display:inline-block;\"></span><h1>OMS Gateway</h1></div><span class=\"pill\" id=\"hostname-pill\">hostname</span></header>\n"
"<main>\n"
"  <div class=\"grid\">\n"
"    <section class=\"card\">\n"
"      <h2>Wi-Fi & Hostname</h2>\n"
"      <label>Hostname</label><input id=\"hostname\" placeholder=\"oms-gateway\"/>\n"
"      <div class=\"row\" style=\"margin-top:10px;\">\n"
"        <div><label>SSID</label><input id=\"wifi-ssid\" placeholder=\"\"/></div>\n"
"        <div><label>Password</label><input id=\"wifi-pass\" type=\"password\" placeholder=\"\"/></div>\n"
"      </div>\n"
"      <button style=\"margin-top:12px;\" onclick=\"saveWifi()\">Save Wi-Fi</button>\n"
"      <p class=\"muted\" id=\"wifi-status\" style=\"margin-top:8px;\">-</p>\n"
"    </section>\n"
"    <section class=\"card\">\n"
"      <h2>Backend</h2>\n"
"      <label>Endpoint URL</label><input id=\"backend-url\" placeholder=\"http://host:port/path\"/>\n"
"      <div class=\"row\" style=\"margin-top:10px;\">\n"
"        <button onclick=\"saveBackend()\">Save</button>\n"
"        <button onclick=\"testBackend()\" style=\"background:var(--accent2);color:#fff;\">Test</button>\n"
"      </div>\n"
"      <p class=\"muted\" id=\"backend-status\" style=\"margin-top:8px;\">-</p>\n"
"    </section>\n"
"    <section class=\"card\">\n"
"      <h2>Radio</h2>\n"
"      <div class=\"row\">\n"
"        <div><label>CS Threshold</label><select id=\"cs-level\"><option value=\"0\">Default</option><option value=\"1\">Medium</option><option value=\"2\">High</option><option value=\"3\">Low</option></select></div>\n"
"        <div><label>Sync Mode</label><select id=\"sync-mode\"><option value=\"0\">Default 15/16</option><option value=\"1\">Tight 16/16</option><option value=\"2\">Strict 30/32</option></select></div>\n"
"      </div>\n"
"      <button style=\"margin-top:12px;\" onclick=\"saveRadio()\">Save Radio</button>\n"
"      <p class=\"muted\" id=\"radio-status\" style=\"margin-top:8px;\">-</p>\n"
"    </section>\n"
"    <section class=\"card\">\n"
"      <h2>Whitelist</h2>\n"
"      <label>Manufacturer (hex LE)</label><input id=\"wl-manuf\" placeholder=\"36F5\"/>\n"
"      <label style=\"margin-top:8px;\">ID (8 hex, LSB first)</label><input id=\"wl-id\" placeholder=\"60931101\"/>\n"
"      <button style=\"margin-top:12px;\" onclick=\"addWhitelist()\">Add</button>\n"
"      <div style=\"margin-top:12px;\"><p class=\"section-title\">Entries</p><div id=\"wl-list\" class=\"list\"></div></div>\n"
"    </section>\n"
"  </div>\n"
"  <section class=\"card\" style=\"margin-top:12px;\">\n"
"    <h2>Recent Packets</h2>\n"
"    <div id=\"pkt-list\" class=\"list\"></div>\n"
"  </section>\n"
"</main>\n"
"<script>\n"
"const qs=(k)=>encodeURIComponent(k);\n"
"function renderWhitelist(entries){const box=document.getElementById('wl-list');box.innerHTML='';if(!entries||!entries.length){box.innerHTML='<div class="list-item"><span class="muted">No entries</span></div>';return;}entries.forEach(e=>{const div=document.createElement('div');div.className='list-item';div.innerHTML=`<span class=\"tag\">${e.manuf} / ${e.id}</span><button class=\"danger\" style=\"padding:6px 10px\" onclick=\"delWhitelist('${e.manuf}','${e.id}')\">Del</button>`;box.appendChild(div);});}\n"
"function renderPackets(list){const box=document.getElementById('pkt-list');box.innerHTML='';if(!list||!list.length){box.innerHTML='<div class="list-item"><span class="muted">No packets yet</span></div>';return;}list.slice(0,20).forEach(p=>{const div=document.createElement('div');div.className='list-item';div.innerHTML=`<div><div class=\"tag\">${p.id}</div><div class=\"muted\">RSSI ${p.rssi.toFixed(1)} | len ${p.payload_len}</div></div><div class=\"tag\">${p.whitelisted?'whitelisted':'new'}</div>`;box.appendChild(div);});}\n"
"async function fetchJSON(path){const r=await fetch(path);if(!r.ok) throw new Error('req failed');return r.json();}\n"
"function fillStatus(data){document.getElementById('hostname-pill').textContent=data.hostname||'oms-gateway';document.getElementById('hostname').value=data.hostname||'';document.getElementById('backend-url').value=data.backend.url||'';document.getElementById('wifi-ssid').value=data.wifi.ssid||'';document.getElementById('wifi-pass').value='';document.getElementById('wifi-status').textContent=data.wifi.connected?`Connected (${data.wifi.ip})`:'Not connected';document.getElementById('backend-status').textContent=data.backend.url?`Configured${data.backend.reachable?', reachable':''}`:'Not set';document.getElementById('cs-level').value=data.radio.cs_level;document.getElementById('sync-mode').value=data.radio.sync_mode;renderWhitelist(data.whitelist);}\n"
"async function loadStatus(){try{const data=await fetchJSON('/api/status');fillStatus(data);}catch(e){console.error(e);}}\n"
"async function saveWifi(){const ssid=document.getElementById('wifi-ssid').value;const pass=document.getElementById('wifi-pass').value;await fetch(`/api/wifi?ssid=${qs(ssid)}&pass=${qs(pass)}`,{method:'POST'});loadStatus();}\n"
"async function saveBackend(){const url=document.getElementById('backend-url').value;await fetch(`/api/backend?url=${qs(url)}`,{method:'POST'});loadStatus();}\n"
"async function testBackend(){const url=document.getElementById('backend-url').value;const r=await fetch(`/api/backend/test?url=${qs(url)}`);document.getElementById('backend-status').textContent=r.ok?'Reachable':'Unreachable';}\n"
"async function saveRadio(){const cs=document.getElementById('cs-level').value;const sync=document.getElementById('sync-mode').value;await fetch(`/api/radio?cs=${qs(cs)}&sync=${qs(sync)}`,{method:'POST'});loadStatus();}\n"
"async function addWhitelist(){const manuf=document.getElementById('wl-manuf').value;const id=document.getElementById('wl-id').value;await fetch(`/api/whitelist/add?manuf=${qs(manuf)}&id=${qs(id)}`,{method:'POST'});loadStatus();}\n"
"async function delWhitelist(manuf,id){await fetch(`/api/whitelist/del?manuf=${qs(manuf)}&id=${qs(id)}`,{method:'POST'});loadStatus();}\n"
"let pktBuf=[];function addPkt(p){pktBuf.unshift(p);if(pktBuf.length>40)pktBuf.pop();renderPackets(pktBuf);}\n"
"if(!!window.EventSource){const es=new EventSource('/api/packets');es.onmessage=(ev)=>{try{const p=JSON.parse(ev.data);addPkt(p);}catch(e){}};}\n"
"loadStatus();\n"
"</script>\n"
"</body></html>\n";

static esp_err_t send_ok(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static uint16_t parse_hex16(const char *s)
{
    return (uint16_t)strtoul(s, NULL, 16);
}

static void format_whitelist(char *buf, size_t cap, const wmbus_whitelist_t *wl)
{
    size_t pos = 0;
    pos += snprintf(buf + pos, cap - pos, "{\"entries\":[");
    for (size_t i = 0; i < wl->count && pos < cap; i++)
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

static esp_err_t handle_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, UI_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_status(httpd_req_t *req)
{
    char wl_json[1024];
    format_whitelist(wl_json, sizeof(wl_json), services_whitelist(s_services));

    app_wifi_status_t wifi = {0};
    services_get_wifi_status(&wifi);
    app_radio_status_t radio = {0};
    services_get_radio_status(s_services, &radio);
    char backend_url[192] = {0};
    services_get_backend_url(s_services, backend_url, sizeof(backend_url));

    char json[2048];
    int n = snprintf(json, sizeof(json),
                     "{\"hostname\":\"%s\",\"wifi\":{\"connected\":%s,\"ssid\":\"%s\",\"ip\":\"%s\"},"
                     "\"backend\":{\"url\":\"%s\",\"reachable\":false},\"radio\":{\"cs_level\":%u,\"sync_mode\":%u},\"whitelist\":%s}",
                     services_hostname(s_services),
                     wifi.connected ? "true" : "false",
                     wifi.ssid,
                     wifi.ip,
                     backend_url,
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
            services_set_backend_url(s_services, url);
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
    const char *target = (url[0] != '\0') ? url : NULL;
    if (!target)
    {
        httpd_resp_set_status(req, "400");
        return httpd_resp_sendstr(req, "missing url");
    }
    esp_err_t err = backend_check_url(target, 1500);
    httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK)
    {
        return httpd_resp_sendstr(req, "{\"reachable\":true}");
    }
    else
    {
        httpd_resp_set_status(req, "503");
        return httpd_resp_sendstr(req, "{\"reachable\":false}");
    }
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
    if (ssid[0] == '\0')
    {
        httpd_resp_set_status(req, "400");
        return httpd_resp_sendstr(req, "ssid required");
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
    if (name[0] == '\0')
    {
        httpd_resp_set_status(req, "400");
        return httpd_resp_sendstr(req, "name required");
    }
    services_set_hostname(s_services, name);
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
        httpd_resp_set_status(req, "400");
        return httpd_resp_sendstr(req, "invalid params");
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
        httpd_resp_set_status(req, "400");
        return httpd_resp_sendstr(req, "invalid params");
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
    // SSE placeholder: currently no live feed; keep connection alive with comment.
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_sendstr_chunk(req, ": ok\n\n");
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t URI_ROOT = {.uri = "/", .method = HTTP_GET, .handler = handle_root};
static const httpd_uri_t URI_STATUS = {.uri = "/api/status", .method = HTTP_GET, .handler = handle_status};
static const httpd_uri_t URI_BACKEND = {.uri = "/api/backend", .method = HTTP_POST, .handler = handle_backend};
static const httpd_uri_t URI_BACKEND_TEST = {.uri = "/api/backend/test", .method = HTTP_GET, .handler = handle_backend_test};
static const httpd_uri_t URI_WIFI = {.uri = "/api/wifi", .method = HTTP_POST, .handler = handle_wifi};
static const httpd_uri_t URI_HOST = {.uri = "/api/hostname", .method = HTTP_POST, .handler = handle_hostname};
static const httpd_uri_t URI_RADIO = {.uri = "/api/radio", .method = HTTP_POST, .handler = handle_radio};
static const httpd_uri_t URI_WL_ADD = {.uri = "/api/whitelist/add", .method = HTTP_POST, .handler = handle_wl_add};
static const httpd_uri_t URI_WL_DEL = {.uri = "/api/whitelist/del", .method = HTTP_POST, .handler = handle_wl_del};
static const httpd_uri_t URI_PKTS = {.uri = "/api/packets", .method = HTTP_GET, .handler = handle_packets_stream};

esp_err_t http_server_start(services_state_t *svc)
{
    if (s_server)
    {
        return ESP_OK;
    }
    s_services = svc;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 6144;
    cfg.server_port = 80;

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
    httpd_register_uri_handler(s_server, &URI_RADIO);
    httpd_register_uri_handler(s_server, &URI_WL_ADD);
    httpd_register_uri_handler(s_server, &URI_WL_DEL);
    httpd_register_uri_handler(s_server, &URI_PKTS);

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
