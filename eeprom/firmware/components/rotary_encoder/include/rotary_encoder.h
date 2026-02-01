#pragma once

#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_err.h"
#include "esp_types.h"

typedef struct {
    gpio_num_t pin_a;
    gpio_num_t pin_b;
    gpio_num_t button_pin;
    uint32_t debounce_ms;
    int64_t last_button_time;
    bool button_down;
    pcnt_unit_handle_t pcnt;
    uint32_t rotation_debounce_ms;
    int64_t last_rotation_time;
} rotary_config_t;

esp_err_t rotary_init(rotary_config_t *encoder);
bool check_rotary_button_pressed(rotary_config_t *encoder);
int get_rotary_delta(rotary_config_t *encoder);
int get_rotary_direction(rotary_config_t *encoder);
