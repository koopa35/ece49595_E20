#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_types.h"

typedef struct {
    gpio_num_t pin_a;           // Encoder A pin
    gpio_num_t pin_b;           // Encoder B pin
    gpio_num_t button_pin;      // Encoder push button pin
    uint32_t debounce_ms;       // Debounce time for button
    int64_t last_button_time;   // For debounce
    bool button_down;           // Button Pressed
    pcnt_unit_handle_t pcnt;    // Handle for pulse counter
} rotary_config_t;

esp_err_t pcnt_init(rotary_config_t *encoder);
esp_err_t rotary_init(rotary_config_t *encoder);
bool rotary_button_pressed(rotary_config_t *encoder);