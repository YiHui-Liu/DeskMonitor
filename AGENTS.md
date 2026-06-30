# AGENTS.md

## Project Rules

- If an implementation detail is unclear or could materially change the design, ask the user for clarification before modifying code.

## Build & Verify

ESP-IDF v6 via PlatformIO, **not Arduino**. Target: `esp32dev` (ESP32-32E, 4MB Flash, no PSRAM).

```
pio run -e esp32dev                    # firmware build (authoritative compile gate)
pio run -e esp32dev -t buildfs         # LittleFS image from data/
pio run -e esp32dev -t menuconfig      # adjust sdkconfig (regenerates sdkconfig.esp32dev)
bash test/run_config_validation.sh     # host-runnable config validation (cc -DUNIT_TEST)
bash test/verify_constraints.sh        # static check: no LCD/GPIO/SD/LVGL runtime init
```

Run all four before declaring work done. `pio run -e esp32dev` is the authoritative compile gate — `lsp_diagnostics` is unreliable because clangd lacks ESP-IDF include paths and xtensa toolchain flags.

After changing `sdkconfig.defaults`, delete `sdkconfig.esp32dev` (gitignored) to force regeneration on next build.

## Architecture

**Source registration**: `src/CMakeLists.txt` uses an explicit `SRCS` list. Every new `.c` file must be added there manually — there is no glob.

**Module layout**:
- `src/bsp/` — I2C bus owner (`deskmon_i2c_bus()`), pin/address constants
- `src/sensors/` — AHT20, TSL2591, ENS160 drivers; share bus via `deskmon_sensor_i2c_add_device()`
- `src/app/` — config, WiFi APSTA, OTA, HTTP server, diagnostics
- `src/ui/`, `src/storage/` — reserved stubs (display/buttons/TF album not initialized)
- `src/main.c` — entry point; wires NVS, netif, storage, config, I2C, WiFi, HTTPD

**Component dependencies** are declared in `src/idf_component.yml` (LVGL, LittleFS, cJSON, esp_lcd_st7796). PlatformIO auto-resolves them into `managed_components/` (gitignored).

**Partition table**: `partitions.csv` — OTA dual-slot + LittleFS data partition.

## Constraints

- **Display, buttons, SD card, and LVGL runtime are intentionally disabled.** Do not add `esp_lcd_*`, `gpio_config`, `sdmmc_*`, `lv_init()`, or `lv_timer_handler()` calls in `src/`. `verify_constraints.sh` will fail.
- **No `/api/diagnostic` singular alias.** The diagnostics endpoint is `/api/diagnostics` only.
- **Sensor reads are on-demand** (triggered by `/api/diagnostics` requests). `sensor_read_interval_sec` and `sensor_history_retention_hours` are config-only placeholders with no runtime consumer yet.

## API

`/api/diagnostics` returns top-level `quantities[]` with rows `{name, sensor, address, status, reading, unit}`. Status is `ok`, `missing`, or `read-error`; reading is numeric or `"n/a"`.

## WiFi

APSTA mode. Setup AP: SSID `DeskMonitor-Setup`, password `deskmonitor`. Web controller at `http://192.168.4.1/` or the STA IP from serial log.
