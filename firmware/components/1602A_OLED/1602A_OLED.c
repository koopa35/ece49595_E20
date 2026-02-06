#include <stdio.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "1602A_OLED.h"

/* =========================================================
 * Internal helpers
 * ========================================================= */

static esp_err_t send_command_or_data(spi_device_handle_t spi_oled,
                                      uint8_t mode,
                                      uint8_t value)
{
    /*
     * 10-bit OLED frame:
     * [RS][RW][D7..D0] << 6
     */
    uint16_t word =
        (((mode & 0x01) << 9) | (0 << 8) | value) << 6;

    spi_transaction_t t = {
        .flags  = SPI_TRANS_USE_TXDATA,
        .length = 16,
    };

    t.tx_data[0] = (word >> 8) & 0xFF;
    t.tx_data[1] = word & 0xFF;

    return spi_device_transmit(spi_oled, &t);
}

/* =========================================================
 * Public API
 * ========================================================= */

esp_err_t oled_init(spi_device_handle_t spi_oled)
{
    vTaskDelay(pdMS_TO_TICKS(300));

    command(spi_oled, OLED_FUNCTIONSET | OLED_8BITMODE | OLED_LANG_EN);
    vTaskDelay(pdMS_TO_TICKS(1));

    command(spi_oled, OLED_DISPLAYCTRL | OLED_DISPLAYOFF);
    vTaskDelay(pdMS_TO_TICKS(1));

    command(spi_oled, OLED_CLEARDISPLAY);
    vTaskDelay(pdMS_TO_TICKS(300));

    command(spi_oled, OLED_ENTRYMODESET | OLED_ENTRYLEFT | OLED_ENTRYSHIFTDEC);
    vTaskDelay(pdMS_TO_TICKS(1));

    command(spi_oled, OLED_RETURNHOME);
    vTaskDelay(pdMS_TO_TICKS(2));

    command(spi_oled,
            OLED_DISPLAYCTRL | OLED_DISPLAYON | OLED_CURSOROFF | OLED_BLINKOFF);

    /* Custom bar characters (1–7) */
    static const uint8_t char_map[7][8] = {
        {0,0,0,0,0,0,0,31},
        {0,0,0,0,0,0,31,31},
        {0,0,0,0,0,31,31,31},
        {0,0,0,0,31,31,31,31},
        {0,0,0,31,31,31,31,31},
        {0,0,31,31,31,31,31,31},
        {0,31,31,31,31,31,31,31},
    };

    for (uint8_t i = 0; i < 7; i++) {
        custom_char(spi_oled, i + 1, (uint8_t *)char_map[i]);
    }

    return ESP_OK;
}

esp_err_t clear(spi_device_handle_t spi_oled)
{
    command(spi_oled, OLED_CLEARDISPLAY);
    vTaskDelay(pdMS_TO_TICKS(2));
    return home(spi_oled);
}

esp_err_t home(spi_device_handle_t spi_oled)
{
    command(spi_oled, OLED_RETURNHOME);
    vTaskDelay(pdMS_TO_TICKS(2));
    return ESP_OK;
}

esp_err_t set_cursor(spi_device_handle_t spi_oled, uint8_t row, uint8_t col)
{
    uint8_t addr = (row ? 0x40 : 0x00) | (col & 0x3F);
    command(spi_oled, OLED_SETDDRAMADDR | addr);
    vTaskDelay(pdMS_TO_TICKS(20));
    return ESP_OK;
}

esp_err_t command(spi_device_handle_t spi_oled, uint8_t cmd)
{
    esp_err_t err = send_command_or_data(spi_oled, OLED_COMMAND, cmd);
    vTaskDelay(pdMS_TO_TICKS(1));
    return err;
}

esp_err_t data(spi_device_handle_t spi_oled, uint8_t data_byte)
{
    esp_err_t err = send_command_or_data(spi_oled, OLED_DATA, data_byte);
    vTaskDelay(pdMS_TO_TICKS(1));
    return err;
}

esp_err_t print(spi_device_handle_t spi_oled, const char *str)
{
    while (*str) {
        data(spi_oled, (uint8_t)*str++);
    }
    return ESP_OK;
}

esp_err_t custom_char(spi_device_handle_t spi_oled,
                      uint8_t location,
                      uint8_t charmap[])
{
    location &= 0x7;

    command(spi_oled, OLED_SETCGRAMADDR | (location << 3));

    for (int i = 0; i < 8; i++) {
        data(spi_oled, charmap[i]);
    }

    vTaskDelay(pdMS_TO_TICKS(2));
    command(spi_oled, OLED_SETDDRAMADDR | 0x00);
    vTaskDelay(pdMS_TO_TICKS(2));

    return ESP_OK;
}

esp_err_t freq(spi_device_handle_t spi_oled,
               uint8_t *spectrum,
               size_t len)
{
    home(spi_oled);
    set_cursor(spi_oled, 1, 0);

    for (size_t i = 0; i < len && i < 16; i++) {
        uint8_t v = spectrum[i];

        if (v == 0) {
            data(spi_oled, ' ');
        } else if (v <= 7) {
            data(spi_oled, v);   // CGRAM chars 1–7
        } else {
            data(spi_oled, 7);   // clamp
        }
    }

    return ESP_OK;
}