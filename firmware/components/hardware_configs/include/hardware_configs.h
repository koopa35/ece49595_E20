#pragma once

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"

#include "global_defs.h"
#include "rotary_encoder.h"
#include "eeprom_24xx.h"

// Switch configuration
extern gpio_config_t io_conf;

// Rotary encoder configuration
extern rotary_config_t enc1;
extern rotary_config_t enc2;

extern spi_bus_config_t buscfg;
extern spi_device_interface_config_t devcfg1;
extern spi_device_interface_config_t devcfg2;

// SPI bus/display init
extern spi_device_handle_t spi_oled1;
extern spi_device_handle_t spi_oled2;

// I2S configuration
extern i2s_std_config_t std_cfg_duplex;
extern i2s_std_config_t std_cfg_simplex;

extern eeprom_24xx_t eeprom_dev;



// Init functions
void spi_init(void);
void i2c_master_init(void);
void i2s_example_init_std_duplex(i2s_chan_handle_t* a_tx_chan, i2s_chan_handle_t* a_rx_chan);
void i2s_init_bluetooth(i2s_chan_handle_t* a_rx_chan);
