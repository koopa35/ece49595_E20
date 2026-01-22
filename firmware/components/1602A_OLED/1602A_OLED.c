#include <stdio.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "1602A_OLED.h"

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
    
    command(spi_oled, OLED_DISPLAYCTRL | OLED_DISPLAYON | OLED_CURSOROFF | OLED_BLINKOFF);

    uint8_t char_map1[] = {
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b11111
    };

    uint8_t char_map2[] = {
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b11111,
        0b11111
    };

    uint8_t char_map3[] = {
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b11111,
        0b11111,
        0b11111
    };

    uint8_t char_map4[] = {
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b11111,
        0b11111,
        0b11111,
        0b11111
    };


    uint8_t char_map5[] = {
        0b00000,
        0b00000,
        0b00000,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111
    };

    uint8_t char_map6[] = {
        0b00000,
        0b00000,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111
    };

    uint8_t char_map7[] = {
        0b00000,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111,
        0b11111
    };    

    custom_char(spi_oled, 1, char_map1);
    custom_char(spi_oled, 2, char_map2);
    custom_char(spi_oled, 3, char_map3);
    custom_char(spi_oled, 4, char_map4);
    custom_char(spi_oled, 5, char_map5);
    custom_char(spi_oled, 6, char_map6);
    custom_char(spi_oled, 7, char_map7);

    return ESP_OK;
}

esp_err_t clear(spi_device_handle_t spi_oled)
{
    command(spi_oled, OLED_CLEARDISPLAY);
    vTaskDelay(pdMS_TO_TICKS(2));
    home(spi_oled);

    return ESP_OK;
}

esp_err_t home(spi_device_handle_t spi_oled)
{
    command(spi_oled, OLED_RETURNHOME);
    vTaskDelay(pdMS_TO_TICKS(2));
    
    return ESP_OK;
}

esp_err_t set_cursor(spi_device_handle_t spi_oled, uint8_t row, uint8_t col)
{
    command(spi_oled, OLED_SETDDRAMADDR | (row ? 0x40 : 0x00) | (col & 0x3F));
    vTaskDelay(pdMS_TO_TICKS(20));

    return ESP_OK;
}

esp_err_t send_command_or_data(spi_device_handle_t spi_oled, uint8_t mode, uint8_t data)
{
    uint16_t word = SPI_SWAP_DATA_TX((((mode & 0x01) << 9) | (0 << 8) | data) << 6, 16);

    static uint16_t tx_word; 
    tx_word = word;

    spi_transaction_t t = {
        .length = 10,
        .tx_buffer = &tx_word,
    };

    spi_device_queue_trans(spi_oled, &t, portMAX_DELAY);

    return ESP_OK;
}

esp_err_t command(spi_device_handle_t spi_oled, uint8_t cmd)
{
    send_command_or_data(spi_oled, OLED_COMMAND, cmd);
    vTaskDelay(pdMS_TO_TICKS(1));

    return ESP_OK;
}

esp_err_t data(spi_device_handle_t spi_oled, uint8_t data_byte)
{
    send_command_or_data(spi_oled, OLED_DATA, data_byte);
    vTaskDelay(pdMS_TO_TICKS(1));

    return ESP_OK;
}

esp_err_t print(spi_device_handle_t spi_oled, const char *str)
{
    while (*str) {
        data(spi_oled, (uint8_t)*str++);
    }

    return ESP_OK;
}

esp_err_t custom_char(spi_device_handle_t spi_oled, uint8_t location, uint8_t charmap[])
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

esp_err_t freq(spi_device_handle_t spi_oled, uint8_t *spectrum, size_t len)
{
    home(spi_oled);

    char line[17] = {0};
    for (size_t i = 0; i < len && i < 16; i++) {
        if (spectrum[i] == 0) {
            line[i] = ' ';
        } else {
            line[i] = (spectrum[i] == 8 ? 255 : spectrum[i]);
        }
    }

    set_cursor(spi_oled, 1, 0);
    print(spi_oled, line);

    return ESP_OK;
}