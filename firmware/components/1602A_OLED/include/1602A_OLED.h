#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/spi_master.h"

/* =========================================================
 * OLED control bits
 * ========================================================= */

#define OLED_DATA            1
#define OLED_COMMAND         0
#define OLED_READ            1
#define OLED_WRITE           0

/* Commands */
#define OLED_CLEARDISPLAY    0x01
#define OLED_RETURNHOME      0x02
#define OLED_ENTRYMODESET    0x04
#define OLED_DISPLAYCTRL     0x08
#define OLED_CURSORSHIFT     0x10
#define OLED_FUNCTIONSET     0x28
#define OLED_SETCGRAMADDR    0x40
#define OLED_SETDDRAMADDR    0x80

/* Entry mode */
#define OLED_ENTRYLEFT       0x02
#define OLED_ENTRYRIGHT      0x00
#define OLED_ENTRYSHIFTINC   0x01
#define OLED_ENTRYSHIFTDEC   0x00

/* Display control */
#define OLED_DISPLAYON       0x04
#define OLED_DISPLAYOFF      0x00
#define OLED_CURSORON        0x02
#define OLED_CURSOROFF       0x00
#define OLED_BLINKON         0x01
#define OLED_BLINKOFF        0x00

/* Cursor / display shift */
#define OLED_SHIFTCURSOR     0x00
#define OLED_SHIFTDISPLAY    0x08
#define OLED_MOVELEFT        0x00
#define OLED_MOVERIGHT       0x04

/* Function set */
#define OLED_8BITMODE        0x10
#define OLED_4BITMODE        0x00

/* Language / ROM select */
#define OLED_LANG_EN         0x00
#define OLED_LANG_JP         0x00
#define OLED_LANG_EU1        0x01
#define OLED_LANG_RU         0x02
#define OLED_LANG_EU2        0x03

/* =========================================================
 * Public API
 * ========================================================= */

esp_err_t oled_init(spi_device_handle_t spi_oled);
esp_err_t clear(spi_device_handle_t spi_oled);
esp_err_t home(spi_device_handle_t spi_oled);
esp_err_t set_cursor(spi_device_handle_t spi_oled, uint8_t row, uint8_t col);

esp_err_t command(spi_device_handle_t spi_oled, uint8_t cmd);
esp_err_t data(spi_device_handle_t spi_oled, uint8_t data_byte);
esp_err_t print(spi_device_handle_t spi_oled, const char *str);

esp_err_t custom_char(spi_device_handle_t spi_oled,
                      uint8_t location,
                      uint8_t charmap[]);

esp_err_t freq(spi_device_handle_t spi_oled,
               uint8_t *spectrum,
               size_t len);