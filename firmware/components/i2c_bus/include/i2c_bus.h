#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"

typedef struct {
    i2c_port_t port;
    gpio_num_t sda_io_num;
    gpio_num_t scl_io_num;
    bool sda_pullup_en;
    bool scl_pullup_en;
    uint32_t clk_speed_hz;
} i2c_bus_cfg_t;

esp_err_t i2c_bus_init(const i2c_bus_cfg_t *cfg);
