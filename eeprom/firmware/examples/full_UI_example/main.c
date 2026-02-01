/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include "global_defs.h"
#include "global_vars.h"
#include "hardware_configs.h"

#include "1602A_OLED.h"
#include "rotary_encoder.h"
#include "user_interface.h"

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    spi_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_ERROR_CHECK(oled_init(spi_oled0));
    ESP_ERROR_CHECK(oled_init(spi_oled1));
    vTaskDelay(pdMS_TO_TICKS(200));
    
    ESP_ERROR_CHECK(rotary_init(&encoder0));
    ESP_ERROR_CHECK(rotary_init(&encoder1));
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_ERROR_CHECK(start_menu_task(spi_oled0, spi_oled1, spectrum, &encoder0, &encoder1));

    while (1) {
        input_select = (gpio_get_level(SOURCE_SEL)) ? AUX : BLUETOOTH;
        display_power = (gpio_get_level(DISP_POWER)) ? OFF : ON;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}