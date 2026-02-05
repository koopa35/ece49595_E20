#include <stdio.h>
#include "hardware_configs.h"

// Switch configuration
gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_DISABLE,
    .mode = GPIO_MODE_INPUT,
    .pin_bit_mask = ((1ULL<<SOURCE_SEL) | (1ULL<<DISP_POWER)),
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    };

// Rotary encoder configuration
rotary_config_t encoder0 = {
    .pin_a = ROTARY0_A,
    .pin_b = ROTARY0_B,
    .button_pin = ROTARY0_BUTTON,
    .debounce_ms = 500,
    .rotation_debounce_ms = 100,
    };

rotary_config_t encoder1 = {
    .pin_a = ROTARY1_A,
    .pin_b = ROTARY1_B,
    .button_pin = ROTARY1_BUTTON,
    .debounce_ms = 500,
    .rotation_debounce_ms = 100,
    };

spi_bus_config_t buscfg = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64
    };

spi_device_interface_config_t devcfg0 = {
        .mode = 3,
        .clock_speed_hz = 1000000,
        .spics_io_num = CS_OLED0_PIN,
        .queue_size = 16,
        .flags = SPI_DEVICE_HALFDUPLEX
    };

spi_device_interface_config_t devcfg1 = {
        .mode = 3,
        .clock_speed_hz = 1000000,
        .spics_io_num = CS_OLED1_PIN,
        .queue_size = 16,
        .flags = SPI_DEVICE_HALFDUPLEX
    };

// SPI bus/display init
spi_device_handle_t spi_oled0;
spi_device_handle_t spi_oled1;


//----------AUDIO DSP---------------//

// Duplex I2S for ADC/DAC
i2s_std_config_t std_cfg_duplex = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
        .mclk = GPIO_NUM_21,    
        .bclk = GPIO_NUM_38, 
        .ws   = GPIO_NUM_39,
        .dout = GPIO_NUM_40, 
        .din  = GPIO_NUM_37,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv   = false,
        },
    },
};

// Simplex I2S for BT
i2s_std_config_t std_cfg_simplex = {
    .clk_cfg  = {
        .sample_rate_hz = SAMPLE_RATE,
        .clk_src = I2S_CLK_SRC_DEFAULT,
        .mclk_multiple = I2S_MCLK_MULTIPLE_384,
    },
    .slot_cfg = slot_config,
    .gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,  
        .bclk = GPIO_NUM_41,
        .ws   = GPIO_NUM_42,
        .dout = I2S_GPIO_UNUSED,
        .din  = GPIO_NUM_45,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv   = false,
        },
    },
};




void spi_init(void)
{
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg0, &spi_oled0));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg1, &spi_oled1));
}

