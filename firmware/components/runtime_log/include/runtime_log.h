
#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"  
#include "eeprom_24xx.h"

typedef struct {
    uint16_t save_interval_minutes;
} runtime_log_cfg_t;

typedef struct {
    const eeprom_24xx_t *eeprom;

    uint16_t slot_count;

    // State 
    uint32_t base_minutes;
    uint32_t last_saved_minutes;
    uint32_t seq;
    uint16_t slot;
    int64_t  boot_us;

    runtime_log_cfg_t cfg;
} runtime_log_t;

esp_err_t runtime_log_init(runtime_log_t *h, const eeprom_24xx_t *eeprom, const runtime_log_cfg_t *cfg);
esp_err_t runtime_log_load_latest(runtime_log_t *h);
esp_err_t runtime_log_set_boot_time_now(runtime_log_t *h);

uint32_t runtime_log_current_total_minutes(const runtime_log_t *h);
esp_err_t runtime_log_maybe_save(runtime_log_t *h);

// Starts a FreeRTOS task that prints runtime  and saves periodically.

esp_err_t runtime_log_start_task(runtime_log_t *h, uint32_t stack_bytes, UBaseType_t priority);
