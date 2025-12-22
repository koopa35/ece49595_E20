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
    uint32_t rotation_debounce_ms;  // Debounce time for rotation
    int64_t last_rotation_time;     // For rotation debounce

} rotary_config_t;

esp_err_t pcnt_init(rotary_config_t *encoder);
esp_err_t rotary_init(rotary_config_t *encoder);
bool check_rotary_button_pressed(rotary_config_t *encoder);
int get_rotary_delta(rotary_config_t *encoder);
int get_rotary_direction(rotary_config_t *encoder);  // Returns -1 (left), 1 (right), or 0 (no rotation)
