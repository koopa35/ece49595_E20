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
#include "driver/gpio.h"
#include "rotary_encoder.h"

static const char *TAG = "RENCODE_TEST";

void encoder_task(void *arg) {
    rotary_config_t **encoders = (rotary_config_t **)arg;
    rotary_config_t *encoder1 = encoders[0];
    rotary_config_t *encoder2 = encoders[1];    

    int count1 = 0;
    int count2 = 0;
    
    while (1) {
        int delta1 = rotary_get_delta(encoder1);
        int delta2 = rotary_get_delta(encoder2);

        count1 += delta1;
        count2 += delta2;

        if (delta1 != 0) ESP_LOGI(TAG, "Encoder 1 count: %d", count1);
        if (delta2 != 0) ESP_LOGI(TAG, "Encoder 2 count: %d", count2);

        if (rotary_button_pressed(encoder1)) ESP_LOGI(TAG, "Encoder 1 button pressed!");
        if (rotary_button_pressed(encoder2)) ESP_LOGI(TAG, "Encoder 2 button pressed!");

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


void app_main(void){
    static rotary_config_t encoder1 = {
        .pin_a = GPIO_NUM_1,
        .pin_b = GPIO_NUM_2,
        .button_pin = GPIO_NUM_42,
        .debounce_ms = 50,
        .hold_time_ms = 1000
    };

    static rotary_config_t encoder2 = {
        .pin_a = GPIO_NUM_41,
        .pin_b = GPIO_NUM_40,
        .button_pin = GPIO_NUM_39,
        .debounce_ms = 50,
        .hold_time_ms = 1000
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
    