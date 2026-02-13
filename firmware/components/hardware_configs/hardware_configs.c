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
rotary_config_t enc1 = {
    .pin_a = ENC1_A,
    .pin_b = ENC1_B,
    .button_pin = ENC1_BTN,
    .debounce_ms = 500,
    .rotation_debounce_ms = 100,
    };

rotary_config_t enc2 = {
    .pin_a = ENC2_A,
    .pin_b = ENC2_B,
    .button_pin = ENC2_BTN,
    .debounce_ms = 500,
    .rotation_debounce_ms = 100,
    };

spi_bus_config_t buscfg = {
        .mosi_io_num = MOSI,
        .miso_io_num = -1,
        .sclk_io_num = SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64
    };

spi_device_interface_config_t devcfg1 = {
        .mode = 3,
        .clock_speed_hz = 1000000,
        .spics_io_num = CS1,
        .queue_size = 16,
        .flags = SPI_DEVICE_HALFDUPLEX
    };

spi_device_interface_config_t devcfg2 = {
        .mode = 3,
        .clock_speed_hz = 2000000,
        .spics_io_num = CS2,
        .queue_size = 16,
        .flags = SPI_DEVICE_HALFDUPLEX
    };

// SPI bus/display init
spi_device_handle_t spi_oled1;
spi_device_handle_t spi_oled2;

void spi_init(void)
{
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg1, &spi_oled1));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg2, &spi_oled2));
}

// EEPROM init
void i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_GPIO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
        .clk_flags = 0,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0));
}

// EEPROM device instance
eeprom_24xx_t eeprom_dev = {
    .i2c_port = I2C_PORT,
    .dev_addr_7bit = EEPROM_ADDR,
    .size_bytes = EEPROM_SIZE_BYTES,
    .page_size = EEPROM_PAGE_SIZE,
    .timeout_ms = 1000
};


//----------AUDIO DSP---------------//

// Duplex I2S for ADC/DAC
i2s_std_config_t std_cfg_duplex = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
        .mclk = GPIO_NUM_37,    
        .bclk = GPIO_NUM_40, 
        .ws   = GPIO_NUM_39,
        .dout = GPIO_NUM_41, 
        .din  = GPIO_NUM_38,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv   = false,
        },
    },
};

// Simplex I2S for BT
i2s_std_config_t std_cfg_simplex = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
    // .clk_cfg  = {
    //     .sample_rate_hz = SAMPLE_RATE,
    //     .clk_src = I2S_CLK_SRC_DEFAULT,
    //     // .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    // },
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG
    (I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,  
        .bclk = GPIO_NUM_35,
        .ws   = GPIO_NUM_36,
        .dout = I2S_GPIO_UNUSED,
        .din  = GPIO_NUM_45,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv   = false,
        },
    },
};


void i2s_example_init_std_duplex(i2s_chan_handle_t* a_tx_chan, i2s_chan_handle_t* a_rx_chan)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, a_tx_chan, a_rx_chan));

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*a_tx_chan, &std_cfg_duplex));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*a_rx_chan, &std_cfg_duplex));
}


void i2s_init_bluetooth(i2s_chan_handle_t* a_rx_chan)
{
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_SLAVE);
    rx_chan_cfg.auto_clear = true;  // Add this
    
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, a_rx_chan));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*a_rx_chan, &std_cfg_simplex));
}
