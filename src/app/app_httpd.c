#include "app/app_httpd.h"

#include <stdlib.h>
#include <string.h>

#include "app/app_config_json.h"
#include "app/app_diagnostics.h"
#include "app/app_ota.h"
#include "app/app_wifi.h"
#include "cJSON.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "deskmon_httpd";
static const size_t MAX_POST_BODY = 2048;

static httpd_handle_t s_server;
static deskmon_config_t *s_config;

static const char *CONTROL_PAGE =
    "<!doctype html><html lang=\"zh-CN\"><meta charset=\"utf-8\">"
    "<title>Desk Monitor Control</title>"
    "<h1>Desk Monitor / 桌面监视器</h1>"
    "<form id=\"cfg\">"
    "<fieldset><legend>WiFi</legend>"
    "<label>SSID <input name=\"wifi_ssid\"></label><br>"
    "<label>Password <input name=\"wifi_password\" type=\"password\"></label>"
    "</fieldset>"
    "<fieldset><legend>QWeather / 和风天气</legend>"
    "<label>API Key <input name=\"qweather_api_key\"></label><br>"
    "<label>Location <input name=\"qweather_location\"></label>"
    "</fieldset>"
    "<fieldset><legend>Pages / 页面</legend>"
    "<label>Summary note / 汇总信息 <input name=\"page_summary_note\"></label><br>"
    "<label>Carousel seconds / 轮播秒数 <input name=\"carousel_interval_sec\" type=\"number\" min=\"5\" max=\"3600\"></label><br>"
    "<label>Sensor read seconds / 传感器读取秒数 <input name=\"sensor_read_interval_sec\" type=\"number\" min=\"5\" max=\"3600\"></label><br>"
    "<label>History retention hours / 历史保留小时 <input name=\"sensor_history_retention_hours\" type=\"number\" min=\"1\" max=\"720\"></label><br>"
    "<label><input name=\"summary\" type=\"checkbox\"> Summary / 汇总</label>"
    "<label><input name=\"weather\" type=\"checkbox\"> Weather / 天气</label>"
    "<label><input name=\"sensor\" type=\"checkbox\"> Sensors / 传感器</label>"
    "<label><input name=\"memo\" type=\"checkbox\"> Memo / 备忘录</label>"
    "<label><input name=\"album\" type=\"checkbox\"> Album / 相册</label>"
    "</fieldset>"
    "<button type=\"submit\">Save / 保存</button> <span id=\"result\"></span>"
    "</form>"
    "<form id=\"ota\"><fieldset><legend>OTA</legend>"
    "<label>Firmware HTTPS URL <input name=\"url\"></label>"
    "<button type=\"submit\">Start OTA</button>"
    "</fieldset></form>"
    "<section><h2>Sensors / 传感器</h2>"
    "<table border=\"1\" cellpadding=\"4\"><thead><tr>"
    "<th>Name / 名称</th><th>Sensor / 传感器</th><th>Address / 地址</th><th>Status / 状态</th><th>Reading / 读数</th><th>Unit / 单位</th>"
    "</tr></thead><tbody id=\"sensor_status\"><tr><td colspan=\"6\">Loading / 加载中</td></tr></tbody></table>"
    "</section>"
    "<script>"
    "const $=n=>document.querySelector('[name='+n+']');"
    "async function loadDiagnostics(){const r=await fetch('/api/diagnostics');const d=await r.json();const rows=d.quantities.map(x=>`<tr><td>${x.name}</td><td>${x.sensor}</td><td>${x.address}</td><td>${x.status}</td><td>${x.reading}</td><td>${x.unit}</td></tr>`).join('');document.getElementById('sensor_status').innerHTML=rows||'<tr><td colspan=\"6\">No sensors / 无传感器</td></tr>';};"
    "fetch('/api/config').then(r=>r.json()).then(c=>{for(const k of ['wifi_ssid','wifi_password','qweather_api_key','qweather_location','page_summary_note','carousel_interval_sec','sensor_read_interval_sec','sensor_history_retention_hours'])$(k).value=c[k]||'';for(const k in c.pages)$(k).checked=c.pages[k];});"
    "loadDiagnostics();setInterval(loadDiagnostics,10000);"
    "document.getElementById('cfg').onsubmit=async e=>{e.preventDefault();const b={wifi_ssid:$('wifi_ssid').value,wifi_password:$('wifi_password').value,qweather_api_key:$('qweather_api_key').value,qweather_location:$('qweather_location').value,page_summary_note:$('page_summary_note').value,carousel_interval_sec:Number($('carousel_interval_sec').value),sensor_read_interval_sec:Number($('sensor_read_interval_sec').value),sensor_history_retention_hours:Number($('sensor_history_retention_hours').value),pages:{summary:$('summary').checked,weather:$('weather').checked,sensor:$('sensor').checked,memo:$('memo').checked,album:$('album').checked}};const r=await fetch('/api/config',{method:'POST',body:JSON.stringify(b)});document.getElementById('result').textContent=r.ok?'Saved / 已保存':'Save failed / 保存失败';};"
    "document.getElementById('ota').onsubmit=async e=>{e.preventDefault();await fetch('/api/ota',{method:'POST',body:JSON.stringify({url:$('url').value})});};"
    "</script>"
    "</html>";

static esp_err_t send_json(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t read_body(httpd_req_t *req, char **body)
{
    if (req->content_len == 0 || req->content_len > MAX_POST_BODY) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *buffer = calloc(req->content_len + 1, 1);
    if (buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    size_t offset = 0;
    while (offset < req->content_len) {
        int received = httpd_req_recv(req, buffer + offset, req->content_len - offset);
        if (received <= 0) {
            free(buffer);
            return ESP_FAIL;
        }
        offset += (size_t)received;
    }

    *body = buffer;
    return ESP_OK;
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, CONTROL_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t health_get_handler(httpd_req_t *req)
{
    deskmon_wifi_status_t wifi = deskmon_wifi_status();
    char response[128];
    snprintf(response, sizeof(response), "{\"sta_connected\":%s,\"ap_started\":%s,\"sta_ip\":\"%s\"}",
             wifi.sta_connected ? "true" : "false", wifi.ap_started ? "true" : "false", wifi.sta_ip);
    return send_json(req, response);
}

static esp_err_t config_get_handler(httpd_req_t *req)
{
    char *json = deskmon_config_to_json(s_config);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "config encode failed");
        return ESP_FAIL;
    }

    esp_err_t err = send_json(req, json);
    free(json);
    return err;
}

static esp_err_t diagnostics_get_handler(httpd_req_t *req)
{
    char *json = deskmon_diagnostics_json();
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "diagnostics encode failed");
        return ESP_FAIL;
    }

    esp_err_t err = send_json(req, json);
    free(json);
    return err;
}

static esp_err_t config_post_handler(httpd_req_t *req)
{
    char *body = NULL;
    esp_err_t err = read_body(req, &body);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body");
        return ESP_FAIL;
    }

    deskmon_config_t next = *s_config;
    deskmon_config_status_t status = deskmon_config_from_json(body, &next);
    free(body);

    if (status != DESKMON_CONFIG_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, deskmon_config_status_name(status));
        return ESP_FAIL;
    }

    err = deskmon_config_save(&next);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "config save failed");
        return ESP_FAIL;
    }

    *s_config = next;
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t ota_post_handler(httpd_req_t *req)
{
    char *body = NULL;
    esp_err_t err = read_body(req, &body);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    cJSON *url = cJSON_GetObjectItemCaseSensitive(root, "url");
    if (!cJSON_IsString(url) || url->valuestring == NULL) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing url");
        return ESP_FAIL;
    }

    err = deskmon_ota_start_url(url->valuestring);
    cJSON_Delete(root);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, esp_err_to_name(err));
        return ESP_FAIL;
    }

    return send_json(req, "{\"ota_started\":true}");
}

esp_err_t deskmon_httpd_start(deskmon_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_config = config;
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.max_uri_handlers = 8;

    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &server_config), TAG, "http server start failed");

    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler};
    const httpd_uri_t health = {.uri = "/api/health", .method = HTTP_GET, .handler = health_get_handler};
    const httpd_uri_t config_get = {.uri = "/api/config", .method = HTTP_GET, .handler = config_get_handler};
    const httpd_uri_t config_post = {.uri = "/api/config", .method = HTTP_POST, .handler = config_post_handler};
    const httpd_uri_t diagnostics_get = {.uri = "/api/diagnostics", .method = HTTP_GET, .handler = diagnostics_get_handler};
    const httpd_uri_t ota_post = {.uri = "/api/ota", .method = HTTP_POST, .handler = ota_post_handler};

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &root), TAG, "register root failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &health), TAG, "register health failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &config_get), TAG, "register config get failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &config_post), TAG, "register config post failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &diagnostics_get), TAG, "register diagnostics failed");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &ota_post), TAG, "register ota post failed");

    ESP_LOGI(TAG, "web control started");
    return ESP_OK;
}
