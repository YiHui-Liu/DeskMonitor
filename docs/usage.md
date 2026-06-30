# 使用说明

## Web 控制台

1. 烧录固件并启动设备。
2. 如果还没有配置家庭 WiFi，连接设备创建的热点：
   - SSID：`DeskMonitor-Setup`
   - 密码：`deskmonitor`
3. 浏览器打开 `http://192.168.4.1/` 进入 Web 控制台。
4. 如果已经配置家庭 WiFi，设备会同时保留配置热点，并尝试作为 STA 连接家庭 WiFi。连接成功后，串口日志会打印 `STA connected: <IP>`，也可以通过这个 IP 访问 Web 控制台。

Web 控制台当前包含：

| 区域 | 用途 |
| --- | --- |
| WiFi | 设置家庭 WiFi 的 SSID 和密码。 |
| QWeather / 和风天气 | 设置和风天气 API Key 与位置 ID，并可用当前填写值测试实时天气接口。 |
| Pages / 页面 | 设置页面启用状态、汇总文字、轮播时间、传感器读取周期和历史保留时间。 |
| OTA | 填写固件 HTTPS URL 并触发 OTA。 |
| Sensors / 传感器 | 查看 `/api/diagnostics` 返回的传感器物理量读数。 |
| Time / 时间 | 查看设备时间，设置 NTP 服务器、时区，或用浏览器当前时间校准设备。 |
| System / 系统 | 查看 `/api/diagnostics` 返回的内存、LittleFS 存储和分区表使用情况。 |

## 传感器诊断接口

Web 控制台和 API 都使用 `/api/diagnostics`。接口返回顶层 `quantities[]` 数组，每一行代表一个物理量，而不是一个传感器模块。

示例：

```json
{
  "quantities": [
    {"name":"Illuminance","sensor":"TSL2591","address":"0x29","status":"ok","reading":123.4,"unit":"lux"},
    {"name":"Temperature","sensor":"AHT20","address":"0x38","status":"ok","reading":24.8,"unit":"°C"},
    {"name":"Humidity","sensor":"AHT20","address":"0x38","status":"ok","reading":45.1,"unit":"%RH"}
  ],
  "system": {
    "memory": {"free_heap_bytes":180000,"minimum_free_heap_bytes":150000,"largest_free_block_bytes":90000},
    "storage": {"label":"littlefs","mount":"/littlefs","status":"ok","total_bytes":1048576,"used_bytes":65536,"free_bytes":983040},
    "partitions": [
      {"label":"nvs","type":1,"subtype":2,"address":36864,"size_bytes":24576},
      {"label":"littlefs","type":1,"subtype":130,"address":3211264,"size_bytes":884736}
    ]
  }
}
```

状态说明：

| 状态 | 含义 |
| --- | --- |
| `ok` | 传感器存在，读取成功。 |
| `missing` | I2C 探测不到该地址，读数显示为 `n/a`。 |
| `read-error` | I2C 地址存在，但驱动读取失败，读数显示为 `n/a`。 |

Web 控制台传感器表格列顺序为：`Name / Sensor / Address / Status / Reading / Unit`。

系统区域会显示空闲堆内存、历史最低空闲堆、最大连续空闲块、LittleFS 已用/总量/可用空间，以及分区标签、类型、子类型、地址和大小。

## 和风天气测试

QWeather / 和风天气区域的 `Test / 测试` 按钮会使用输入框里当前填写的 `qweather_api_key` 和 `qweather_location` 调用：

```http
POST /api/qweather/test
Content-Type: application/json
```

请求示例：

```json
{"qweather_api_key":"your-key","qweather_location":"101010100"}
```

固件会向和风天气实时天气接口 `/v7/weather/now` 发起 HTTP 请求，并把 HTTP 状态码和返回体展示在页面中。测试不会自动保存配置。

## 修改设置

推荐使用 Web 控制台修改设置。填写表单后点击 `Save / 保存`，固件会通过 `POST /api/config` 保存配置到 LittleFS，重启后继续使用保存值。

可配置字段：

| 字段 | 说明 | 范围或限制 | 默认值 |
| --- | --- | --- | --- |
| `wifi_ssid` | 家庭 WiFi 名称 | 最长 32 字符；为空时只使用配置热点 | 空 |
| `wifi_password` | 家庭 WiFi 密码 | 最长 64 字符 | 空 |
| `qweather_api_key` | 和风天气 API Key | 最长 96 字符 | 空 |
| `qweather_location` | 和风天气位置 ID | 最长 32 字符 | `101010100` |
| `page_summary_note` | 汇总页文字 | 最长 128 字符 | `Desk Monitor / 桌面监视器` |
| `ntp_server` | NTP 服务器 | 最长 64 字符；不能为空 | `pool.ntp.org` |
| `timezone` | POSIX 时区字符串 | 最长 64 字符；不能为空 | `CST-8` |
| `carousel_interval_sec` | 页面轮播间隔 | `5 - 3600` 秒 | `15` |
| `sensor_read_interval_sec` | 传感器读取周期配置 | `1 - 3600` 秒 | `30` |
| `sensor_history_retention_hours` | 传感器历史保留配置 | `1 - 720` 小时 | `24` |
| `pages.summary` | 是否启用汇总页 | 至少启用一个页面 | 启用 |
| `pages.weather` | 是否启用天气页 | 至少启用一个页面 | 启用 |
| `pages.sensor` | 是否启用传感器页 | 至少启用一个页面 | 启用 |
| `pages.memo` | 是否启用备忘录页 | 至少启用一个页面 | 启用 |
| `pages.album` | 是否启用相册页 | 至少启用一个页面 | 关闭 |

也可以直接调用接口修改配置：

```http
GET /api/config
POST /api/config
Content-Type: application/json
```

`POST /api/config` 示例：

```json
{
  "wifi_ssid": "your-wifi",
  "wifi_password": "your-password",
  "qweather_api_key": "your-key",
  "qweather_location": "101010100",
  "page_summary_note": "Desk Monitor / 桌面监视器",
  "ntp_server": "pool.ntp.org",
  "timezone": "CST-8",
  "carousel_interval_sec": 15,
  "sensor_read_interval_sec": 30,
  "sensor_history_retention_hours": 24,
  "pages": {
    "summary": true,
    "weather": true,
    "sensor": true,
    "memo": true,
    "album": false
  }
}
```

注意：

- `sensor_read_interval_sec` 和 `sensor_history_retention_hours` 已经进入配置项，但当前传感器读数仍是 `/api/diagnostics` 按需读取，后台定时采样和历史曲线还没有启用。
- 页面开关至少需要保留一个启用项，否则保存会失败。
- `/api/diagnostics` 是唯一诊断路径，不提供单数形式的诊断路径。

## 时间与 NTP

设备启动 WiFi 后会按配置启动 SNTP，并使用 `timezone` 设置本地时间显示。Web 控制台会读取 `/api/time` 显示当前设备时间，也可以点击 `同步浏览器时间` 将浏览器当前 Unix 时间写入设备。

```http
GET /api/time
POST /api/time
Content-Type: application/json
```

手动设置时间请求示例：

```json
{"epoch_sec": 1782816000}
```

`/api/diagnostics` 也会返回顶层 `time` 对象，包含当前 epoch、本地时间、UTC 时间、NTP 服务器、时区和 SNTP 状态。
