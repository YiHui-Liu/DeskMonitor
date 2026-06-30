#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"

#define DESKMON_I2C_SDA_GPIO GPIO_NUM_32
#define DESKMON_I2C_SCL_GPIO GPIO_NUM_25
#define DESKMON_I2C_PORT I2C_NUM_0
#define DESKMON_I2C_FREQ_HZ 100000

#define DESKMON_TSL2591_I2C_ADDR 0x29
#define DESKMON_ENS160_I2C_ADDR 0x53
#define DESKMON_AHT20_I2C_ADDR 0x38

#define DESKMON_BUTTON_1_GPIO GPIO_NUM_35
#define DESKMON_BUTTON_2_GPIO GPIO_NUM_39
#define DESKMON_BUTTONS_ENABLED 0

#define DESKMON_DISPLAY_ENABLED 1
#define DESKMON_DISPLAY_SPI_HOST SPI2_HOST
/* Physical panel GRAM is 320 (source) x 480 (gate). */
#define DESKMON_DISPLAY_WIDTH 320
#define DESKMON_DISPLAY_HEIGHT 480
/* Orientation: swap_xy rotates to landscape (logical 480x320); mirror bits
 * compensate for module mounting. Defaults match the 4" ST7796S module. */
#define DESKMON_DISPLAY_SWAP_XY 1
#define DESKMON_DISPLAY_MIRROR_X 1
#define DESKMON_DISPLAY_MIRROR_Y 1
#define DESKMON_DISPLAY_SPI_MODE 0
#define DESKMON_DISPLAY_PCLK_HZ (40 * 1000 * 1000)
#define DESKMON_DISPLAY_CMD_BITS 8
#define DESKMON_DISPLAY_PARAM_BITS 8
#define DESKMON_DISPLAY_TRANS_QUEUE_DEPTH 10

/* ST7796S 4-line SPI pins (see docs/peripheral.md and parsed schematic).
 * MISO is write-only: the LCD driver path never reads back, so leave it NC.
 * RST shares the ESP32 EN line (board-level reset), so it is not GPIO-controlled. */
#define DESKMON_DISPLAY_MOSI_GPIO GPIO_NUM_13
#define DESKMON_DISPLAY_SCLK_GPIO GPIO_NUM_14
#define DESKMON_DISPLAY_CS_GPIO GPIO_NUM_15
#define DESKMON_DISPLAY_DC_GPIO GPIO_NUM_2
#define DESKMON_DISPLAY_MISO_GPIO GPIO_NUM_NC
#define DESKMON_DISPLAY_RST_GPIO GPIO_NUM_NC
#define DESKMON_DISPLAY_BL_GPIO GPIO_NUM_27
