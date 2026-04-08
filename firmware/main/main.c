/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "global_defs.h"
#include "global_vars.h"
#include "hardware_configs.h"

#include "1602A_OLED.h"
#include "eeprom_24xx.h"
#include "rotary_encoder.h"
#include "user_interface.h"
#include "dsp.h"
#include "ntc.h"


//----------APPLICATION ENTRY POINT----------
void app_main(void)
{
    //----------HARDWARE INITIALIZATION----------
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_config(&ntc_io_conf));
    ESP_ERROR_CHECK(ntc_init());

    spi_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    //----------DISPLAY INITIALIZATION----------
    ESP_ERROR_CHECK(oled_init(spi_oled1));
    ESP_ERROR_CHECK(oled_init(spi_oled2));
    vTaskDelay(pdMS_TO_TICKS(200));

    set_cursor(spi_oled1, 0, 0);
    print(spi_oled1, "Initializing...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    //----------I2C & EEPROM INITIALIZATION----------
    i2c_master_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(eeprom_24xx_load(&eeprom_dev));
    xTaskCreate(eeprom_24xx_task, "eeprom_task", 4096, NULL, 3, NULL);
    vTaskDelay(pdMS_TO_TICKS(100));
            
    runtime_total_sec = 0;
    runtime_total_sec = ELECTRODE_REPLACE_TIME;
        
    //----------ROTARY ENCODER INITIALIZATION----------
    ESP_ERROR_CHECK(rotary_init(&enc1));
    ESP_ERROR_CHECK(rotary_init(&enc2));
    vTaskDelay(pdMS_TO_TICKS(100));

    //----------USER INTERFACE INITIALIZATION----------
    ESP_ERROR_CHECK(start_menu_task(spi_oled1, spi_oled2, &enc1, &enc2));

    //----------DSP SYSTEM INITIALIZATION----------
    ESP_ERROR_CHECK(dsp_init());

    //----------MAIN CONTROL LOOP----------
    while (1) {
        input_select = (gpio_get_level(SOURCE_SEL)) ? AUX : BLUETOOTH;
        display_power = (gpio_get_level(DISP_POWER)) ? OFF : ON;

        if (runtime_total_sec < ELECTRODE_REPLACE_TIME) {
            runtime_total_sec += 1;
            next_change_sec = ELECTRODE_REPLACE_TIME - runtime_total_sec;
        }
        
        ntc_read_temperatures();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
