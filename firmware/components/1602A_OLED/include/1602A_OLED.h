#pragma once

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>

// main commands
#define OLED_DATA			1
#define OLED_COMMAND		0
#define OLED_READ			1
#define OLED_WRITE			0
#define OLED_CLEARDISPLAY   0x01
#define OLED_RETURNHOME     0x02
#define OLED_ENTRYMODESET   0x04
#define OLED_DISPLAYCTRL    0x08
#define OLED_CURSORSHIFT    0x10
#define OLED_FUNCTIONSET    0x28
#define OLED_SETCGRAMADDR   0x40
#define OLED_SETDDRAMADDR   0x80

// to control entry mode, pass OLED_ENTRYMODESET OR'd with following flags
#define OLED_ENTRYLEFT      0x02
#define OLED_ENTRYRIGHT     0x00
#define OLED_ENTRYSHIFTINC  0x01
#define OLED_ENTRYSHIFTDEC  0x00

// to control the display, pass OLED_DISPLAYCTRL OR'd with other flags
#define OLED_DISPLAYON      0x04
#define OLED_DISPLAYOFF     0x00
#define OLED_CURSORON       0x02
#define OLED_CURSOROFF      0x00
#define OLED_BLINKON		0x01
#define OLED_BLINKOFF		0x00

// to control display/cursor shifting, pass OLED_CURSORSHIFT OR'd with following flags
#define OLED_SHIFTCURSOR    0x00
#define OLED_SHIFTDISPLAY   0x08
#define OLED_MOVELEFT		0x00
#define OLED_MOVERIGHT      0x04

// to set function, pass OLED_FUNCTIONSET OR'd with following flags
#define OLED_8BITMODE       0x10
#define OLED_4BITMODE       0x00
#define OLED_LANG_EN        0x00
#define OLED_LANG_JP        0x00
#define OLED_LANG_EU1       0x01
#define OLED_LANG_RU        0x02
#define OLED_LANG_EU2       0x03

// Driver context
typedef struct {
    spi_device_handle_t spi;
    gpio_num_t dc_pin;
    gpio_num_t reset_pin;
    gpio_num_t cs_pin;

    uint8_t display_function;
    uint8_t display_control;
    uint8_t display_mode;
    uint8_t cols;
    uint8_t rows;
} silvervest_oled_t;

esp_err_t oled_init(spi_device_handle_t spi_oled);
esp_err_t clear(spi_device_handle_t spi_oled);
esp_err_t home(spi_device_handle_t spi_oled);
esp_err_t command(spi_device_handle_t spi_oled, uint8_t cmd);
esp_err_t data(spi_device_handle_t spi_oled, uint8_t data_byte);
esp_err_t send_command_or_data(spi_device_handle_t spi_oled, uint8_t mode, uint8_t data);
