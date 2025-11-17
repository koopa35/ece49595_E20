#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/pulse_cnt.h"
#include "rotary_encoder.h"

static const char *TAG = "RENCODER";
static bool isr_service_installed = false;

// ISR for button
static void IRAM_ATTR rotary_button_isr(void *arg) {
    rotary_config_t *encoder = (rotary_config_t *)arg;

    int64_t now = esp_timer_get_time() / 1000; // ms
    if (now - encoder->last_button_time > encoder->debounce_ms) {
        encoder->button_down = true;
        encoder->last_button_time = now;
    }
}

// PCNT init
esp_err_t pcnt_init(rotary_config_t *encoder) {
    ESP_LOGI(TAG, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .high_limit = 100,
        .low_limit = -100,
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    encoder->pcnt = pcnt_unit;

    ESP_LOGI(TAG, "set glitch filter");
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    ESP_LOGI(TAG, "install pcnt channel A");
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = encoder->pin_a,
        .level_gpio_num = encoder->pin_b,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

    ESP_LOGI(TAG, "install pcnt channel B");
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = encoder->pin_b,
        .level_gpio_num = encoder->pin_a,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    ESP_LOGI(TAG, "set edge and level actions for pcnt channels");
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    return ESP_OK;
}

// Encoder init
esp_err_t rotary_init(rotary_config_t *encoder) {
    if (!encoder) return ESP_ERR_INVALID_ARG;

    // Add PIN A and PIN B

    // Button pin
    gpio_config_t btn_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << encoder->button_pin),
    };
    gpio_config(&btn_conf);

    // State variables
    encoder->button_down = false;
    encoder->last_button_time = esp_timer_get_time() / 1000;

    if (!isr_service_installed) {
        gpio_install_isr_service(0);
        isr_service_installed = true;   
    }

    gpio_isr_handler_add(encoder->button_pin, rotary_button_isr, encoder);

    ESP_LOGI(TAG, "Initialized rotary encoder on pins A=%d, B=%d, BTN=%d",
             encoder->pin_a, encoder->pin_b, encoder->button_pin);

    return ESP_OK;
}

bool rotary_button_pressed(rotary_config_t *encoder) {
    if (!encoder) return false;
    bool pressed = encoder->button_down;
    encoder->button_down = false; // clear after reading
    return pressed;
}