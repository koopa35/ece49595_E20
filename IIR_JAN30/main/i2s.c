/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "sdkconfig.h"
#include "main.h"

void i2s_example_init_std_simplex(i2s_chan_handle_t* a_tx_chan, i2s_chan_handle_t* a_rx_chan)
{

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, a_tx_chan, a_rx_chan));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = GPIO_NUM_21,  
            .bclk = GPIO_NUM_38, // ON PCB GPIO_NUM_38
            .ws   = GPIO_NUM_39, // ON PCB GPIO_NUM_39
            .dout = GPIO_NUM_40, // ON PCB GPIO_NUM_40
            .din  = GPIO_NUM_37,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*a_tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*a_rx_chan, &std_cfg));
}


void i2s_init_bluetooth(i2s_chan_handle_t* a_rx_chan)
{

    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_SLAVE);
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, a_rx_chan));

    i2s_std_slot_config_t slot_config =  I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);

    i2s_std_config_t rx_std_cfg = {
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
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*a_rx_chan, &rx_std_cfg));
}

