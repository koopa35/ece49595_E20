#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "1602A_OLED.h"


esp_err_t oled_init(spi_device_handle_t spi_oled)
{
    command(spi_oled, OLED_FUNCTIONSET | OLED_8BITMODE | OLED_LANG_EN);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    command(spi_oled, OLED_DISPLAYCTRL | OLED_DISPLAYOFF);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    command(spi_oled, OLED_CLEARDISPLAY);
    vTaskDelay(pdMS_TO_TICKS(2));
    
    command(spi_oled, OLED_ENTRYMODESET | OLED_ENTRYLEFT | OLED_ENTRYSHIFTDEC);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    command(spi_oled, OLED_RETURNHOME);
    vTaskDelay(pdMS_TO_TICKS(2));
    
    command(spi_oled, OLED_DISPLAYCTRL | OLED_DISPLAYON | OLED_CURSORON | OLED_BLINKON);
    
    return ESP_OK;
}

esp_err_t clear(spi_device_handle_t spi_oled)
{
    command(spi_oled, OLED_CLEARDISPLAY);
    vTaskDelay(pdMS_TO_TICKS(6));
    home(spi_oled);

    return ESP_OK;
}

esp_err_t home(spi_device_handle_t spi_oled)
{
    command(spi_oled, OLED_RETURNHOME);
    
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

    return spi_device_queue_trans(spi_oled, &t, 0);
}

esp_err_t command(spi_device_handle_t spi_oled, uint8_t cmd)
{
    send_command_or_data(spi_oled, OLED_COMMAND, cmd);
    return ESP_OK;
}

esp_err_t data(spi_device_handle_t spi_oled, uint8_t data_byte)
{
    send_command_or_data(spi_oled, OLED_DATA, data_byte);
    return ESP_OK;
}


