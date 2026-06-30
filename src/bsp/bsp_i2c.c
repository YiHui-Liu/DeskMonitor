#include "bsp/bsp_i2c.h"

#include <stddef.h>

#include "bsp/bsp_io.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "deskmon_i2c";

static i2c_master_bus_handle_t s_i2c_bus;

static const deskmon_i2c_device_info_t s_known_devices[] = {
    {"TSL2591 light", DESKMON_TSL2591_I2C_ADDR},
    {"ENS160 air", DESKMON_ENS160_I2C_ADDR},
    {"AHT20 temp/humidity", DESKMON_AHT20_I2C_ADDR},
};

esp_err_t deskmon_i2c_init(void)
{
    if (s_i2c_bus != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = DESKMON_I2C_PORT,
        .sda_io_num = DESKMON_I2C_SDA_GPIO,
        .scl_io_num = DESKMON_I2C_SCL_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "I2C bus ready: SDA=%d SCL=%d", DESKMON_I2C_SDA_GPIO, DESKMON_I2C_SCL_GPIO);
    }
    return err;
}

esp_err_t deskmon_i2c_probe(uint16_t address, int timeout_ms)
{
    esp_err_t err = deskmon_i2c_init();
    if (err != ESP_OK) {
        return err;
    }
    return i2c_master_probe(s_i2c_bus, address, timeout_ms);
}

i2c_master_bus_handle_t deskmon_i2c_bus(void)
{
    if (deskmon_i2c_init() != ESP_OK) {
        return NULL;
    }
    return s_i2c_bus;
}

const deskmon_i2c_device_info_t *deskmon_i2c_known_devices(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(s_known_devices) / sizeof(s_known_devices[0]);
    }
    return s_known_devices;
}
