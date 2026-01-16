#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#include "rotary_encoder.h"
#include "1602A_OLED.h"
#include "global_defs.h"

extern float voltage;
extern float current;
extern float power;
extern float temperature;
extern uint16_t volume;
extern uint16_t equalizer_band[EQ_BANDS];
extern bool bluetooth_status;
extern bool aux_status;
extern char current_track[STR_LEN];
extern char current_artist[STR_LEN];
extern char current_album[STR_LEN];
extern uint16_t track_runtime_sec;
extern uint32_t runtime_total_sec;
extern uint32_t next_change_sec;

typedef enum {
    MENU_MAIN,
    MENU_VOLUME,
    MENU_METADATA,
    MENU_EQ,
    MENU_INPUT_SELECT,
    MENU_PIVT,
    MENU_RUNTIME
} menu_type_t;

esp_err_t draw_menu(spi_device_handle_t spi_oled, uint8_t cursor, menu_type_t menu);
esp_err_t start_menu_task(spi_device_handle_t spi_oled, rotary_config_t *encoder_menu, rotary_config_t *encoder_control);
void menu_task(void *pvParameters);
