/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */


#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/lock.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "1602A_OLED.h"

#define SCLK_PIN 4
#define MOSI_PIN 5
#define CS_OLED1_PIN 6
#define CS_OLED2_PIN 7

spi_device_handle_t spi_oled1;
spi_device_handle_t spi_oled2;


static void spi_init()
{
    // 1. SPI Bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // OLED 1 Config 
    spi_device_interface_config_t devcfg1 = {
        .mode = 3,
        .clock_speed_hz = 1000000,
        .spics_io_num = CS_OLED1_PIN,
        .queue_size = 16,
        .flags = SPI_DEVICE_HALFDUPLEX
    };

    // OLED 2 Config 
    spi_device_interface_config_t devcfg2 = {
        .mode = 3,
        .clock_speed_hz = 1000000,
        .spics_io_num = CS_OLED2_PIN,
        .queue_size = 16,
        .flags = SPI_DEVICE_HALFDUPLEX
    };

    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg1, &spi_oled1));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg2, &spi_oled2));
    
}

void app_main(void){
    vTaskDelay(pdMS_TO_TICKS(100));
    spi_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    oled_init(spi_oled1);
    oled_init(spi_oled2);
    vTaskDelay(pdMS_TO_TICKS(2000));

    const int num_bars = 16;
    uint8_t spectrum[num_bars];

    srand(time(NULL));
    clear(spi_oled1);
    clear(spi_oled2);

    while (1)
    {
        for (int i = 0; i < num_bars; i++)
        {
            spectrum[i] = rand() % 8;
        }

        freq(spi_oled1, spectrum, num_bars);
        freq(spi_oled2, spectrum, num_bars);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    
    
    }
    