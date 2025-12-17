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
    rotary_config_t *encoder2 = encoders[1];    
    
    while (1) {
        int delta1 = get_rotary_delta(encoder1);
        int delta2 = get_rotary_delta(encoder2);

        if (check_rotary_button_pressed(encoder1)) ESP_LOGI(TAG, "Encoder 1 button pressed!");
        if (delta1) ESP_LOGI(TAG, "Encoder 1 Delta %d", delta1);
        if (check_rotary_button_pressed(encoder2)) ESP_LOGI(TAG, "Encoder 2 button pressed!");
        if (delta2) ESP_LOGI(TAG, "Encoder 2 Delta %d", delta2);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void app_main(void){
    static rotary_config_t encoder1 = {
        .pin_a = GPIO_NUM_15,
        .pin_b = GPIO_NUM_16,
        .button_pin = GPIO_NUM_17,
        .debounce_ms = 500,
    };

    static rotary_config_t encoder2 = {
        .pin_a = GPIO_NUM_18,
        .pin_b = GPIO_NUM_8,
        .button_pin = GPIO_NUM_3,
        .debounce_ms = 500,
    };

    rotary_init(&encoder1);
    rotary_init(&encoder2);

    static rotary_config_t *encoders[2] = { &encoder1, &encoder2 };

    xTaskCreate(
            encoder_task,          // Task function
            "EncoderTask",         // Task name
            2048,                  // Stack size (bytes)
            encoders,              // Task argument
            5,                     // Priority
            NULL                   // Task handle
        );    
}
    