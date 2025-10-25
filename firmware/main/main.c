/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */


#include <stdio.h>
#include <unistd.h>
#include <sys/lock.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "1602A_OLED.h"

#define SCLK_PIN 4
#define MOSI_PIN 5
#define CS_PIN 6

spi_device_handle_t spi_oled;

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

    // 2. SPI device
    spi_device_interface_config_t devcfg = {
        .mode = 3,
        .clock_speed_hz = 1000000,
        .spics_io_num = CS_PIN,
        .queue_size = 1,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };

    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_oled));
}

void app_main(void){
    vTaskDelay(pdMS_TO_TICKS(100));

    oled_init(NULL);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    clear(NULL);
    vTaskDelay(pdMS_TO_TICKS(100));
}