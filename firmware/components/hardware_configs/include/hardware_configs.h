#pragma once

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "global_defs.h"
#include "rotary_encoder.h"

// Switch configuration
extern gpio_config_t io_conf;

// Rotary encoder configuration
extern rotary_config_t encoder0;
extern rotary_config_t encoder1;

extern spi_bus_config_t buscfg;
extern spi_device_interface_config_t devcfg0;
extern spi_device_interface_config_t devcfg1;

// SPI bus/display init
extern spi_device_handle_t spi_oled0;
extern spi_device_handle_t spi_oled1;

void spi_init(void);