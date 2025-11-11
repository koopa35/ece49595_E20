#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "rotary_encoder.h"

static const char *TAG = "ROTARY_ENCODER";
static bool isr_service_installed = false;

// ISR for rotation
static void IRAM_ATTR rotary_isr_handler(void *arg) {
    rotary_config_t *encoder = (rotary_config_t *)arg;

    int a = gpio_get_level(encoder->pin_a);
    int b = gpio_get_level(encoder->pin_b);

    // Determine direction
    if (a != encoder->last_a) {
        if (a == b)
            encoder->position++;
        else
            encoder->position--;
        encoder->last_a = a;
    }
}

// ISR for button
static void IRAM_ATTR rotary_button_isr(void *arg) {
    rotary_config_t *encoder = (rotary_config_t *)arg;

    int64_t now = esp_timer_get_time() / 1000; // ms
    if (now - encoder->last_button_time > encoder->debounce_ms) {
        encoder->button_down = !gpio_get_level(encoder->button_pin);
        encoder->last_button_time = now;
    }
}

// Encoder init
esp_err_t rotary_init(rotary_config_t *encoder) {
    if (!encoder) return ESP_ERR_INVALID_ARG;

    // Pins A and B
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << encoder->pin_a) | (1ULL << encoder->pin_b),
    };
    gpio_config(&io_conf);

    // Button pin
    gpio_config_t btn_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << encoder->button_pin),
    };
    gpio_config(&btn_conf);

    // State variables
    encoder->last_a = gpio_get_level(encoder->pin_a);
    encoder->last_b = gpio_get_level(encoder->pin_b);
    encoder->position = 0;
    encoder->button_down = false;
    encoder->last_button_time = esp_timer_get_time() / 1000;

    if (!isr_service_installed) {
        gpio_install_isr_service(0);
        isr_service_installed = true;   
    }

    gpio_isr_handler_add(encoder->pin_a, rotary_isr_handler, encoder);
    gpio_isr_handler_add(encoder->pin_b, rotary_isr_handler, encoder);
    gpio_isr_handler_add(encoder->button_pin, rotary_button_isr, encoder);

    ESP_LOGI(TAG, "Initialized rotary encoder on pins A=%d, B=%d, BTN=%d",
             encoder->pin_a, encoder->pin_b, encoder->button_pin);

    return ESP_OK;
}

int rotary_get_delta(rotary_config_t *encoder) {
    if (!encoder) return 0;
    int delta = encoder->position;
    encoder->position = 0; // reset after reading
    return delta;
}

bool rotary_button_pressed(rotary_config_t *encoder) {
    if (!encoder) return false;
    bool pressed = encoder->button_down;
    encoder->button_down = false; // clear after reading
    return pressed;
}