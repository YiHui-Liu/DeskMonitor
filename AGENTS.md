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
bash test/verify_constraints.sh        # static check: display enabled; SD/buttons/touch runtime init still off
```

Run all four before declaring work done. `pio run -e esp32dev` is the authoritative compile gate — `lsp_diagnostics` is unreliable because clangd lacks ESP-IDF include paths and xtensa toolchain flags.

After changing `sdkconfig.defaults`, delete `sdkconfig.esp32dev` (gitignored) to force regeneration on next build.

## Architecture

**Source registration**: `src/CMakeLists.txt` registers only the PlatformIO app entry component (`src/main.c`). Module code lives under root `components/*`; every component has its own explicit `SRCS` list in `components/<name>/CMakeLists.txt`. There is no glob.

**Module layout**:
- `components/bsp/` — I2C bus owner (`deskmon_i2c_bus()`), pin/address constants; headers in `include/bsp/`, sources in `src/`
- `components/sensors/` — AHT20, TSL2591, ENS160 drivers; share bus via `deskmon_sensor_i2c_add_device()`; headers in `include/sensors/`, sources in `src/`
- `components/app/` — config, WiFi APSTA, OTA, HTTP server, diagnostics; headers in `include/app/`, sources in `src/`
- `components/ui/` — ST7796S/LVGL display bring-up plus reserved button/page stubs; headers in `include/ui/`, sources in `src/`
- `components/storage/` — reserved TF album stub (SD card not initialized); headers in `include/storage/`, sources in `src/`
- `src/main.c` — entry point; wires NVS, netif, storage, config, I2C, WiFi, HTTPD, display

**Component dependencies** are declared next to the consuming components (`components/ui/idf_component.yml` for LVGL/ST7796, `components/app/idf_component.yml` for LittleFS/cJSON). PlatformIO auto-resolves them into `managed_components/` (gitignored).

**Partition table**: `partitions.csv` — OTA dual-slot + LittleFS data partition.

## Constraints

- **Display runtime is enabled.** The ST7796S panel uses ESP-IDF `esp_lcd` + LVGL through `components/ui/src/ui_display.c`, with backlight GPIO output on IO27.
- **Buttons, touch, SD card, and TF album runtime remain intentionally disabled.** Do not add button/touch interrupt GPIO init, `esp_lcd_touch`, `xpt2046`, `esp_vfs_fat_sd`, `sdspi_*`, or `sdmmc_*` calls in `src/` or `components/`. `verify_constraints.sh` will fail.
- **No `/api/diagnostic` singular alias.** The diagnostics endpoint is `/api/diagnostics` only.
- `/api/diagnostics` sensor reads are on-demand. The display data task also refreshes the display snapshot using `sensor_read_interval_sec`. Sensor history is an in-memory ring of the last 50 reads, downsampled to the UI sparkline; there is no persisted history config.
- **Secrets are write-only over the API.** `GET /api/config` returns `wifi_password` and `qweather_api_key` as empty strings with `has_wifi_password` / `has_qweather_api_key` booleans. POST omits a field to preserve the stored value, or sends a new value to overwrite.

## API

`/api/diagnostics` returns top-level `quantities[]` with rows `{name, sensor, address, status, reading, unit}`. Status is `ok`, `missing`, or `read-error`; reading is numeric or `"n/a"`.

## WiFi

APSTA mode. Setup AP: SSID `DeskMonitor-Setup`, password `deskmonitor`. Web controller at `http://192.168.4.1/` or the STA IP from serial log.
