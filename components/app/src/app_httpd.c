#include "app/app_httpd.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <esp_check.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_http_server.h>
#include <esp_littlefs.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "app/app_config.h"
#include "app/app_config_json.h"
#include "app/app_diagnostics.h"
#include "app/app_ota.h"
#include "app/app_wifi.h"

static const char *TAG = "deskmon_httpd";
static const size_t MAX_POST_BODY = 2048;
static const size_t QWEATHER_RESPONSE_MAX = 4096;
static const size_t QWEATHER_TASK_STACK = 8192;
static const UBaseType_t QWEATHER_TASK_PRIORITY = 5;
static const TickType_t QWEATHER_FETCH_TIMEOUT_TICKS = pdMS_TO_TICKS(6000);

static httpd_handle_t s_server;
static deskmon_config_t *s_config;

static const char *CONTROL_PAGE =
    "<!doctype html><html lang=\"zh-CN\"><meta charset=\"utf-8\">"
    "<title>Desk Monitor Control</title>"
    "<h1>Desk Monitor</h1>"
    "<form id=\"cfg\">"
    "<fieldset><legend>WiFi</legend>"
    "<label>SSID <input name=\"wifi_ssid\"></label><br>"
    "<label>Password <input name=\"wifi_password\" type=\"password\"></label>"
    "</fieldset>"
    "<fieldset><legend>和风天气</legend>"
    "<label>API Key <input name=\"qweather_api_key\"></label><br>"
    "<label>Location <input name=\"qweather_location\"></label> "
    "<button id=\"qweather_test\" type=\"button\">测试</button><br>"
    "<pre id=\"qweather_result\"></pre>"
    "</fieldset>"
    "<fieldset><legend>页面</legend>"
    "<label>汇总信息 <input name=\"page_summary_note\"></label><br>"
    "<label>轮播间隔 [s] <input name=\"carousel_interval_sec\" type=\"number\" min=\"5\" "
    "max=\"3600\"></label><br>"
    "<label>传感器读取间隔 [s] <input name=\"sensor_read_interval_sec\" type=\"number\" min=\"1\" "
    "max=\"3600\"></label><br>"
    "<label>历史保留时长 [h] <input name=\"sensor_history_retention_hours\" type=\"number\" "
    "min=\"1\" max=\"720\"></label><br>"
    "<label><input name=\"summary\" type=\"checkbox\">汇总</label>"
    "<label><input name=\"weather\" type=\"checkbox\">天气</label>"
    "<label><input name=\"sensor\" type=\"checkbox\">传感器</label>"
    "<label><input name=\"memo\" type=\"checkbox\">备忘录</label>"
    "<label><input name=\"album\" type=\"checkbox\">相册</label>"
    "</fieldset>"
    "<button type=\"submit\">Save / 保存</button> <span id=\"result\"></span>"
    "</form>"
    "<form id=\"ota\"><fieldset><legend>OTA</legend>"
    "<label>固件链接 <input name=\"url\"></label>"
    "<button type=\"submit\">Start OTA</button>"
    "</fieldset></form>"
    "<section><h2>传感器</h2>"
    "<table border=\"1\" cellpadding=\"4\"><thead><tr>"
    "<th>名称</th><th>传感器</th><th>地址</th><th>状态</th><th>读数</th><th>单位</th>"
    "</tr></thead><tbody id=\"sensor_status\"><tr><td colspan=\"6\">加载中</td></tr></tbody></table>"
    "</section>"
    "<section><h2>系统</h2>"
    "<h3>内存</h3><table border=\"1\" cellpadding=\"4\"><tbody id=\"memory_status\"><tr><td>"
    "加载中</td></tr></tbody></table>"
    "<h3>存储</h3><table border=\"1\" cellpadding=\"4\"><tbody id=\"storage_status\"><tr><td>"
    "加载中</td></tr></tbody></table>"
    "<h3>分区</h3><table border=\"1\" "
    "cellpadding=\"4\"><thead><tr><th>标签</th><th>Type</th><th>Subtype</th><th>地址</th><th>大小</th></tr></"
    "thead><tbody "
    "id=\"partition_status\"><tr><td colspan=\"5\">加载中</td></tr></tbody></table>"
    "</section>"
    "<script>"
    "const $=n=>document.querySelector('[name='+n+']');"
    "const bytes=n=>n>=1048576?(n/1048576).toFixed(1)+' MiB':n>=1024?(n/1024).toFixed(1)+' KiB':n+' B';"
    "async function loadDiagnostics(){const r=await fetch('/api/diagnostics');const d=await r.json();const "
    "rows=d.quantities.map(x=>`<tr><td>${x.name}</td><td>${x.sensor}</td><td>${x.address}</td><td>${x.status}</"
    "td><td>${x.reading}</td><td>${x.unit}</td></"
    "tr>`).join('');document.getElementById('sensor_status').innerHTML=rows||'<tr><td "
    "colspan=\"6\">无传感器</td></tr>';const "
    "m=d.system.memory;document.getElementById('memory_status').innerHTML=`<tr><th>可用</"
    "th><td>${bytes(m.free_heap_bytes)}</td></tr><tr><th>最小可用</th><td>${bytes(m.minimum_free_heap_bytes)}</td></"
    "tr><tr><th>最大块</th><td>${bytes(m.largest_free_block_bytes)}</td></tr>`;const "
    "s=d.system.storage;document.getElementById('storage_status').innerHTML=`<tr><th>标签</th><td>${s.label}</td></"
    "tr><tr><th>状态</th><td>${s.status}</td></tr><tr><th>已用</th><td>${bytes(s.used_bytes)} / "
    "${bytes(s.total_bytes)}</td></tr><tr><th>可用</th><td>${bytes(s.free_bytes)}</td></"
    "tr>`;document.getElementById('partition_status').innerHTML=d.system.partitions.map(p=>`<tr><td>${p.label}</"
    "td><td>${p.type}</td><td>${p.subtype}</td><td>0x${p.address.toString(16)}</td><td>${bytes(p.size_bytes)}</td></"
    "tr>`).join('')||'<tr><td colspan=\"5\">无分区</td></tr>';};"
    "fetch('/api/config').then(r=>r.json()).then(c=>{for(const k of "
    "['wifi_ssid','wifi_password','qweather_api_key','qweather_location','page_summary_note','carousel_interval_sec','"
    "sensor_read_interval_sec','sensor_history_retention_hours'])$(k).value=c[k]||'';for(const k in "
    "c.pages)$(k).checked=c.pages[k];});"
    "loadDiagnostics();setInterval(loadDiagnostics,10000);"
    "document.getElementById('cfg').onsubmit=async e=>{e.preventDefault();const "
    "b={wifi_ssid:$('wifi_ssid').value,wifi_password:$('wifi_password').value,qweather_api_key:$('qweather_api_key')."
    "value,qweather_location:$('qweather_location').value,page_summary_note:$('page_summary_note').value,carousel_"
    "interval_sec:Number($('carousel_interval_sec').value),sensor_read_interval_sec:Number($('sensor_read_interval_sec'"
    ").value),sensor_history_retention_hours:Number($('sensor_history_retention_hours').value),pages:{summary:$('"
    "summary').checked,weather:$('weather').checked,sensor:$('sensor').checked,memo:$('memo').checked,album:$('album')."
    "checked}};const r=await "
    "fetch('/api/"
    "config',{method:'POST',body:JSON.stringify(b)});document.getElementById('result').textContent=r.ok?'已保存':'"
    "保存失败';};"
    "document.getElementById('qweather_test').onclick=async()=>{const "
    "out=document.getElementById('qweather_result');out.textContent='测试中...';const r=await "
    "fetch('/api/qweather/"
    "test',{method:'POST',body:JSON.stringify({qweather_api_key:$('qweather_api_key').value,qweather_location:$('"
    "qweather_location').value})});const d=await r.json();out.textContent=JSON.stringify(d,null,2);};"
    "document.getElementById('ota').onsubmit=async e=>{e.preventDefault();await "
    "fetch('/api/ota',{method:'POST',body:JSON.stringify({url:$('url').value})});};"
    "</script>"
    "</html>";

static esp_err_t send_json(httpd_req_t *req, const char *json) {
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} qweather_response_t;

static bool valid_header_value(const char *value) {
  if (value == NULL || value[0] == '\0') {
    return false;
  }

  for (const char *p = value; *p != '\0'; ++p) {
    if (*p == '\r' || *p == '\n') {
      return false;
    }
  }
  return true;
}

static bool valid_location_value(const char *value) {
  if (value == NULL || value[0] == '\0') {
    return false;
  }

  for (const char *p = value; *p != '\0'; ++p) {
    if (!isalnum((unsigned char)*p) && *p != ',' && *p != '.' && *p != '-' && *p != '_') {
      return false;
    }
  }
  return true;
}

static esp_err_t qweather_http_event(esp_http_client_event_t *evt) {
  if (evt->event_id != HTTP_EVENT_ON_DATA) {
    return ESP_OK;
  }

  qweather_response_t *response = evt->user_data;
  if (response == NULL || evt->data == NULL || evt->data_len <= 0) {
    return ESP_OK;
  }

  if (response->length + (size_t)evt->data_len >= response->capacity) {
    return ESP_ERR_NO_MEM;
  }

  memcpy(response->data + response->length, evt->data, evt->data_len);
  response->length += (size_t)evt->data_len;
  response->data[response->length] = '\0';
  return ESP_OK;
}

static esp_err_t send_qweather_error(httpd_req_t *req, const char *error) {
  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "qweather encode failed");
    return ESP_FAIL;
  }

  cJSON_AddBoolToObject(root, "ok", false);
  cJSON_AddStringToObject(root, "error", error);
  char *json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (json == NULL) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "qweather encode failed");
    return ESP_FAIL;
  }

  esp_err_t err = send_json(req, json);
  free(json);
  return err;
}

static esp_err_t send_qweather_response(httpd_req_t *req, int status_code, const char *body) {
  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "qweather encode failed");
    return ESP_FAIL;
  }

  cJSON_AddBoolToObject(root, "ok", status_code >= 200 && status_code < 300);
  cJSON_AddNumberToObject(root, "http_status", status_code);
  cJSON *parsed = cJSON_Parse(body);
  if (parsed != NULL) {
    cJSON_AddItemToObject(root, "body", parsed);
  } else {
    cJSON_AddStringToObject(root, "body", body);
  }

  char *json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (json == NULL) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "qweather encode failed");
    return ESP_FAIL;
  }

  esp_err_t err = send_json(req, json);
  free(json);
  return err;
}

static esp_err_t fetch_qweather_now(const char *api_key, const char *location, int *status_code, char **body) {
  char url[160];
  int written = snprintf(url, sizeof(url), "https://devapi.qweather.com/v7/weather/now?location=%s", location);
  if (written < 0 || (size_t)written >= sizeof(url)) {
    return ESP_ERR_INVALID_SIZE;
  }

  qweather_response_t response = {
      .data = calloc(QWEATHER_RESPONSE_MAX, 1),
      .capacity = QWEATHER_RESPONSE_MAX,
  };
  if (response.data == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_http_client_config_t client_config = {
      .url = url,
      .method = HTTP_METHOD_GET,
      .timeout_ms = 4000,
      .event_handler = qweather_http_event,
      .user_data = &response,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };

  esp_http_client_handle_t client = esp_http_client_init(&client_config);
  if (client == NULL) {
    free(response.data);
    return ESP_FAIL;
  }

  esp_http_client_set_header(client, "X-QW-Api-Key", api_key);
  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK) {
    *status_code = esp_http_client_get_status_code(client);
    *body = response.data;
  } else {
    free(response.data);
  }
  esp_http_client_cleanup(client);
  return err;
}

// TLS 握手 + 证书验签非常吃栈（SHA-512 软件计算 + 深 mbedTLS 调用链），
// httpd 工作线程的默认栈扛不住。抓取放到这个独立任务里跑，栈与 OTA 任务对齐 (8KB)。
typedef struct {
  char api_key[DESKMON_CONFIG_QWEATHER_KEY_MAX_LEN + 1];
  char location[DESKMON_CONFIG_QWEATHER_LOCATION_MAX_LEN + 1];
  int http_status;
  char *body;
  esp_err_t err;
  SemaphoreHandle_t done;
} qweather_request_t;

static void qweather_fetch_task(void *arg) {
  qweather_request_t *request = (qweather_request_t *)arg;
  request->err = fetch_qweather_now(request->api_key, request->location, &request->http_status, &request->body);
  xSemaphoreGive(request->done);
  vTaskDelete(NULL);
}

static esp_err_t read_body(httpd_req_t *req, char **body) {
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

static esp_err_t root_get_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, CONTROL_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t health_get_handler(httpd_req_t *req) {
  deskmon_wifi_status_t wifi = deskmon_wifi_status();
  char response[128];
  snprintf(response, sizeof(response), "{\"sta_connected\":%s,\"ap_started\":%s,\"sta_ip\":\"%s\"}",
           wifi.sta_connected ? "true" : "false", wifi.ap_started ? "true" : "false", wifi.sta_ip);
  return send_json(req, response);
}

static esp_err_t config_get_handler(httpd_req_t *req) {
  char *json = deskmon_config_to_json(s_config);
  if (json == NULL) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "config encode failed");
    return ESP_FAIL;
  }

  esp_err_t err = send_json(req, json);
  free(json);
  return err;
}

static esp_err_t diagnostics_get_handler(httpd_req_t *req) {
  char *json = deskmon_diagnostics_json();
  if (json == NULL) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "diagnostics encode failed");
    return ESP_FAIL;
  }

  esp_err_t err = send_json(req, json);
  free(json);
  return err;
}

static esp_err_t config_post_handler(httpd_req_t *req) {
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

static esp_err_t qweather_test_post_handler(httpd_req_t *req) {
  char *body = NULL;
  esp_err_t err = read_body(req, &body);
  if (err != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid body");
    return ESP_FAIL;
  }

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (root == NULL) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
    return ESP_FAIL;
  }

  cJSON *api_key = cJSON_GetObjectItemCaseSensitive(root, "qweather_api_key");
  cJSON *location = cJSON_GetObjectItemCaseSensitive(root, "qweather_location");
  bool inputs_valid = cJSON_IsString(api_key) && valid_header_value(api_key->valuestring) && cJSON_IsString(location) &&
                      valid_location_value(location->valuestring);

  qweather_request_t *request = NULL;
  if (inputs_valid) {
    request = calloc(1, sizeof(qweather_request_t));
    if (request != NULL) {
      strncpy(request->api_key, api_key->valuestring, sizeof(request->api_key) - 1);
      strncpy(request->location, location->valuestring, sizeof(request->location) - 1);
    }
  }
  cJSON_Delete(root);

  if (!inputs_valid) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid qweather fields");
    return ESP_FAIL;
  }
  if (request == NULL) {
    return send_qweather_error(req, "no memory");
  }

  request->done = xSemaphoreCreateBinary();
  if (request->done == NULL) {
    free(request);
    return send_qweather_error(req, "no memory");
  }

  if (xTaskCreate(qweather_fetch_task, "qwfetch", QWEATHER_TASK_STACK, request, QWEATHER_TASK_PRIORITY, NULL) !=
      pdPASS) {
    vSemaphoreDelete(request->done);
    free(request);
    return send_qweather_error(req, "task create failed");
  }

  // 等到任务取回结果再回复。http client 自带 4s 超时，6s 等待已留足余量。
  // 若真的超时，request 是堆分配，任务仍会安全完成（仅这少量内存暂泄漏）。
  if (xSemaphoreTake(request->done, QWEATHER_FETCH_TIMEOUT_TICKS) != pdTRUE) {
    return send_qweather_error(req, "timeout");
  }

  esp_err_t result;
  if (request->err != ESP_OK) {
    result = send_qweather_error(req, esp_err_to_name(request->err));
  } else {
    result = send_qweather_response(req, request->http_status, request->body ? request->body : "");
  }
  free(request->body);
  vSemaphoreDelete(request->done);
  free(request);
  return result;
}

static esp_err_t ota_post_handler(httpd_req_t *req) {
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

esp_err_t deskmon_httpd_start(deskmon_config_t *config) {
  if (config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  s_config = config;
  httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
  server_config.max_uri_handlers = 9;

  ESP_RETURN_ON_ERROR(httpd_start(&s_server, &server_config), TAG, "http server start failed");

  const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler};
  const httpd_uri_t health = {.uri = "/api/health", .method = HTTP_GET, .handler = health_get_handler};
  const httpd_uri_t config_get = {.uri = "/api/config", .method = HTTP_GET, .handler = config_get_handler};
  const httpd_uri_t config_post = {.uri = "/api/config", .method = HTTP_POST, .handler = config_post_handler};
  const httpd_uri_t diagnostics_get = {
      .uri = "/api/diagnostics", .method = HTTP_GET, .handler = diagnostics_get_handler};
  const httpd_uri_t qweather_test_post = {
      .uri = "/api/qweather/test", .method = HTTP_POST, .handler = qweather_test_post_handler};
  const httpd_uri_t ota_post = {.uri = "/api/ota", .method = HTTP_POST, .handler = ota_post_handler};

  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &root), TAG, "register root failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &health), TAG, "register health failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &config_get), TAG, "register config get failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &config_post), TAG, "register config post failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &diagnostics_get), TAG, "register diagnostics failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &qweather_test_post), TAG, "register qweather test failed");
  ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &ota_post), TAG, "register ota post failed");

  ESP_LOGI(TAG, "web control started");
  return ESP_OK;
}
