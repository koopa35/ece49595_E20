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
#include "menu.h"

#define ROTARY_A 33
#define ROTARY_B 25
#define ROTARY_BUTTON 26

#define SCLK_PIN 18
#define MOSI_PIN 23
#define CS_OLED1_PIN 32

#define INPUT_ON 1
#define INPUT_OFF 0

spi_device_handle_t spi_oled1;

// Rotary encoder configuration
static rotary_config_t encoder1 = {
    .pin_a = ROTARY_A,
    .pin_b = ROTARY_B,
    .button_pin = ROTARY_BUTTON,
    .debounce_ms = 500,
    .rotation_debounce_ms = 100,
};

// Menu system
static menu_system_t menu_sys;

// SPI bus/display init
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

// Task to update menu values
void value_update_task(void *arg)
{
    menu_values_t values;
    
    // Static values
    values.voltage = 12.5f;
    values.current = 2.3f;
    values.power = 28.75f;
    values.temperature = 45.2f;
    
    values.equalizer_band0 = 50;
    values.equalizer_band1 = 50;
    values.equalizer_band2 = 50;
    values.equalizer_band3 = 50;
    values.equalizer_band4 = 50;
    values.equalizer_band5 = 50;
    values.equalizer_band6 = 50;
    values.equalizer_band7 = 50;

    values.bluetooth_status = INPUT_ON;
    values.aux_status = INPUT_OFF;

    values.current_track = "Song Title";
    values.current_artist = "Artist Name";
    values.track_runtime_sec = 125;
    
    values.runtime_total_hr = 36;
    values.next_change_hr = 300;
    
    while (1) {
        // Update menu with values
        menu_update_values(&menu_sys, &values);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Menu update task
void menu_task(void *arg)
{
    menu_system_t *menu_sys = (menu_system_t *)arg;
    
    menu_system_refresh(menu_sys);
    
    while (1) {
        menu_system_update(menu_sys);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));
    spi_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_ERROR_CHECK(oled_init(spi_oled1));
    vTaskDelay(pdMS_TO_TICKS(200));
    
    clear(spi_oled1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_ERROR_CHECK(rotary_init(&encoder1));
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_ERROR_CHECK(menu_system_init(&menu_sys, &encoder1, spi_oled1));
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Create value update task
    xTaskCreate(
        value_update_task,
        "ValueUpdateTask",
        2048,
        NULL,
        3,
        NULL
    );
    
    // Create menu task
    xTaskCreate(
        menu_task,
        "MenuTask",
        4096,
        &menu_sys,
        5,
        NULL
    );
}
