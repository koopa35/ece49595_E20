#include <stdio.h>
#include "hardware_configs.h"

// Switch configuration
gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_DISABLE,
    .mode = GPIO_MODE_INPUT,
    .pin_bit_mask = ((1ULL<<SOURCE_SEL) | (1ULL<<ON_OFF_SEL)),
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    };

// Rotary encoder configuration
rotary_config_t encoder0 = {
    .pin_a = ROTARY0_A,
    .pin_b = ROTARY0_B,
    .button_pin = ROTARY0_BUTTON,
    .debounce_ms = 500,
    .rotation_debounce_ms = 100,
    };

rotary_config_t encoder1 = {
    .pin_a = ROTARY1_A,
    .pin_b = ROTARY1_B,
    .button_pin = ROTARY1_BUTTON,
    .debounce_ms = 500,
    .rotation_debounce_ms = 100,
    };

spi_bus_config_t buscfg = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64
    };

spi_device_interface_config_t devcfg0 = {
        .mode = 3,
        .clock_speed_hz = 1000000,
        .spics_io_num = CS_OLED0_PIN,
        .queue_size = 16,
        .flags = SPI_DEVICE_HALFDUPLEX
    };

spi_device_interface_config_t devcfg1 = {
        .mode = 3,
        .clock_speed_hz = 1000000,
        .spics_io_num = CS_OLED1_PIN,
        .queue_size = 16,
        .flags = SPI_DEVICE_HALFDUPLEX
    };

// SPI bus/display init
spi_device_handle_t spi_oled0;
spi_device_handle_t spi_oled1;

void spi_init(void)
{
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg0, &spi_oled0));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg1, &spi_oled1));
}

