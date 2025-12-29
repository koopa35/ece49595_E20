#pragma once

#include "esp_err.h"
#include "rotary_encoder.h"
#include "1602A_OLED.h"

#define STR_LEN 20

typedef enum {
    MENU_MAIN,
    MENU_VOLUME,
    MENU_METADATA,
    MENU_EQ,
    MENU_INPUT_SELECT,
    MENU_PIVT,
    MENU_RUNTIME
} menu_type_t;

// extern const char main_menu[][STR_LEN];
// extern const char eq_menu[][STR_LEN];
// extern const char metadata_menu[][STR_LEN];
// extern const char input_select_menu[][STR_LEN];
// extern const char pivt_menu[][STR_LEN];
// extern const char runtime_menu[][STR_LEN];

// extern const size_t main_menu_len;
// extern const size_t eq_menu_len;
// extern const size_t metadata_menu_len;
// extern const size_t input_select_menu_len;
// extern const size_t pivt_menu_len;
// extern const size_t runtime_menu_len;

esp_err_t draw_menu(spi_device_handle_t spi_oled, uint8_t cursor, menu_type_t menu);
