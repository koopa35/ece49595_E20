/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "driver/spi_master.h"
#include "1602A_OLED.h"
#include "rotary_encoder.h"
#include "user_interface.h"

#define ROTARY_A 33
#define ROTARY_B 25
#define ROTARY_BUTTON 26

#define SCLK_PIN 18
#define MOSI_PIN 23
#define CS_OLED1_PIN 32

#define INPUT_ON 1
#define INPUT_OFF 0

// Global Variables
float voltage = 12.5;
float current = 2.3;
float power = 28.75;
float temperature = 45.2;

uint16_t volume = 67;

uint16_t equalizer_band0 = 50;
uint16_t equalizer_band1 = 50;
uint16_t equalizer_band2 = 50;
uint16_t equalizer_band3 = 50;
uint16_t equalizer_band4 = 50;
uint16_t equalizer_band5 = 50;
uint16_t equalizer_band6 = 50;
uint16_t equalizer_band7 = 50;

bool bluetooth_status = INPUT_ON;
bool aux_status = INPUT_OFF;

char current_track[STR_LEN] = "Song Title";
char current_artist[STR_LEN] = "Artist Name";
char current_album[STR_LEN] = "Album Name";
uint16_t track_runtime_sec = 125;

uint32_t runtime_total_sec = 3600;
uint32_t next_change_sec = 3600000;

// Menu handled by user_interface component (state moved to user_interface/menu module)

// Rotary encoder configuration
static rotary_config_t encoder1 = {
    .pin_a = ROTARY_A,
    .pin_b = ROTARY_B,
    .button_pin = ROTARY_BUTTON,
    .debounce_ms = 500,
    .rotation_debounce_ms = 100,
};

// SPI bus/display init
spi_device_handle_t spi_oled1;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg1 = {
        .mode = 3,
        .clock_speed_hz = 1000000,
        .spics_io_num = CS_OLED1_PIN,
        .queue_size = 16,
        .flags = SPI_DEVICE_HALFDUPLEX
    };

    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg1, &spi_oled1));
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));
    spi_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_ERROR_CHECK(oled_init(spi_oled1));
    vTaskDelay(pdMS_TO_TICKS(200));
    
    ESP_ERROR_CHECK(rotary_init(&encoder1));
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_ERROR_CHECK(start_menu_task(spi_oled1, &encoder1));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}
