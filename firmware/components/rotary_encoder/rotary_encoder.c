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

    int64_t now = esp_timer_get_time() / 1000;
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

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    return ESP_OK;
}

// Rotary init
esp_err_t rotary_init(rotary_config_t *encoder) {
    if (!encoder) return ESP_ERR_INVALID_ARG;

    // PCNT start
    ESP_LOGI(TAG, "starting pcnt unit");
    ESP_ERROR_CHECK(pcnt_init(encoder));

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
    encoder->last_rotation_time = esp_timer_get_time() / 1000;
    if (encoder->rotation_debounce_ms == 0) {
        encoder->rotation_debounce_ms = 50; // Default 50ms
    }

    if (!isr_service_installed) {
        gpio_install_isr_service(0);
        isr_service_installed = true;   
    }

    gpio_isr_handler_add(encoder->button_pin, rotary_button_isr, encoder);

    ESP_LOGI(TAG, "Initialized rotary encoder on pins A=%d, B=%d, BTN=%d",
             encoder->pin_a, encoder->pin_b, encoder->button_pin);

    return ESP_OK;
}

bool check_rotary_button_pressed(rotary_config_t *encoder) {
    if (!encoder) return false;
    
    // Check button ISR
    if (!encoder->button_down) {
        return false;
    }
    
    // Verify button
    int level = gpio_get_level(encoder->button_pin);
    if (level != 0) {
        // False trigger, clear the flag
        encoder->button_down = false;
        return false;
    }
    
    int64_t now = esp_timer_get_time() / 1000;
    int64_t time_since_rotation = now - encoder->last_rotation_time;
    
    if (time_since_rotation < (encoder->rotation_debounce_ms + 20)) {
        encoder->button_down = false; // Clear flag
        return false;
    }
    
    // Valid button press
    encoder->button_down = false; // clear after reading
    return true;
}

int get_rotary_delta(rotary_config_t *encoder) {
    if (!encoder) return 0;
    int delta = 0;
    pcnt_unit_get_count(encoder->pcnt, &delta);
    pcnt_unit_clear_count(encoder->pcnt); // clear after reading
    return delta;
}

// Get normalized rotation direction with debounce
int get_rotary_direction(rotary_config_t *encoder) {
    if (!encoder) return 0;
    
    int64_t now = esp_timer_get_time() / 1000; // ms
    
    // Get accumulated delta first
    int delta = 0;
    pcnt_unit_get_count(encoder->pcnt, &delta);
    
    // Check debounce time
    if (now - encoder->last_rotation_time < encoder->rotation_debounce_ms) {
        if (delta != 0) {
            encoder->last_rotation_time = now;
        }
        pcnt_unit_clear_count(encoder->pcnt);
        return 0;
    }
    
    // Clear count after reading
    pcnt_unit_clear_count(encoder->pcnt);
    
    // Normalize
    if (delta != 0) {
        encoder->last_rotation_time = now;
        return (delta > 0) ? 1 : -1;  // 1 = right, -1 = left
    }
    
    return 0;  // No movement
}