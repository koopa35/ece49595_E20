/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */


#include <stdio.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "rotary_encoder.h"

static const char *TAG = "RENCODE_TEST";

void encoder_task(void *arg) {
    rotary_config_t **encoders = (rotary_config_t **)arg;
    rotary_config_t *encoder1 = encoders[0];
    
    while (1) {
        // Check for button press
        if (check_rotary_button_pressed(encoder1)) {
            ESP_LOGI(TAG, "click");
        }
        
        // Check for normalized rotation
        int direction = get_rotary_direction(encoder1);
        if (direction > 0) {
            ESP_LOGI(TAG, "right");
        } else if (direction < 0) {
            ESP_LOGI(TAG, "left");
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


void app_main(void){
    static rotary_config_t encoder1 = {
        .pin_a = GPIO_NUM_33,
        .pin_b = GPIO_NUM_25,
        .button_pin = GPIO_NUM_26,
        .debounce_ms = 500,
        .rotation_debounce_ms = 50,
    };

    rotary_init(&encoder1);
    static rotary_config_t *encoders[1] = {&encoder1};

    xTaskCreate(
            encoder_task,          // Task function
            "EncoderTask",         // Task name
            2048,                  // Stack size (bytes)
            encoders,              // Task argument
            5,                     // Priority
            NULL                   // Task handle
        );    
}
    