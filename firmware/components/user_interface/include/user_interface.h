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

esp_err_t draw_menu(spi_device_handle_t spi_oled, uint8_t cursor, menu_type_t menu);
