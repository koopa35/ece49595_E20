#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_types.h"

typedef struct {
    // Pins + Timing
    gpio_num_t pin_a;           // Encoder A pin
    gpio_num_t pin_b;           // Encoder B pin
    gpio_num_t button_pin;      // Encoder push button pin
    uint32_t debounce_ms;       // Debounce time for button
    uint32_t hold_time_ms;      // Time to qualify a "held" event
    int64_t last_button_time;   // For debounce

    // State
    volatile int position;      // Accumulated delta
    volatile bool button_down;  // Button state
    int last_a;                 // Last A pin value
    int last_b;                 // Last B pin value
} rotary_config_t;


esp_err_t rotary_init(rotary_config_t *encoder);
int rotary_get_delta(rotary_config_t *encoder);
bool rotary_button_pressed(rotary_config_t *encoder);