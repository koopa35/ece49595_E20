#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "S1602A_OLED.h"

// GPIO pins for bit-banging SPI (proven to work)
#define SPI_SCK_PIN 14
#define SPI_MOSI_PIN 13
#define SPI_CS_PIN 27

// Bit-banging functions (proven to work)
static void send_bit(int bit);
static void send_command_or_data(uint8_t mode, uint8_t data);

esp_err_t oled_init(spi_device_handle_t spi_oled)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SPI_SCK_PIN) | (1ULL << SPI_MOSI_PIN) | (1ULL << SPI_CS_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    gpio_set_level(SPI_CS_PIN, 1);    // CS high (inactive)
    gpio_set_level(SPI_SCK_PIN, 0);   // SCK low
    gpio_set_level(SPI_MOSI_PIN, 0);  // MOSI low
    
    vTaskDelay(pdMS_TO_TICKS(50));

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

static void send_bit(int bit)
{
    gpio_set_level(SPI_SCK_PIN, 0);
    gpio_set_level(SPI_MOSI_PIN, bit);
    gpio_set_level(SPI_SCK_PIN, 1);
}

static void send_command_or_data(uint8_t mode, uint8_t data)
{
    gpio_set_level(SPI_CS_PIN, 0);
    
    // Send 2-bit header
    if (mode == OLED_DATA) {
        send_bit(1);  // Data = HIGH
    } else {
        send_bit(0);  // Command = LOW
    }
    send_bit(0);      // Write = LOW (we're always writing)
    
    // Send 8-bit data
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
        send_bit(mask & data);
    }
    
    // CS high to end transmission
    gpio_set_level(SPI_CS_PIN, 1);
}

esp_err_t command(spi_device_handle_t spi_oled, uint8_t cmd)
{
    send_command_or_data(OLED_COMMAND, cmd);
    return ESP_OK;
}

esp_err_t data(spi_device_handle_t spi_oled, uint8_t data_byte)
{
    send_command_or_data(OLED_DATA, data_byte);
    return ESP_OK;
}


