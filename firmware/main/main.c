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
#include "S1602A_OLED.h"

void app_main(void){
    vTaskDelay(pdMS_TO_TICKS(100));

    oled_init(NULL);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    clear(NULL);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    data(NULL, 'H');
    data(NULL, 'e');
    data(NULL, 'l');
    data(NULL, 'l');
    data(NULL, 'o');
    data(NULL, ' ');
    data(NULL, 'W');
    data(NULL, 'o');
    data(NULL, 'r');
    data(NULL, 'l');
    data(NULL, 'd');
    data(NULL, '!');
    
}