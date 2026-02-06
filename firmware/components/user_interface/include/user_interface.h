#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#include "rotary_encoder.h"
#include "1602A_OLED.h"
#include "global_defs.h"
#include "global_vars.h"

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
esp_err_t start_menu_task(spi_device_handle_t spi_oled1, spi_device_handle_t spi_oled2, rotary_config_t *encoder_menu, rotary_config_t *encoder_control);
void menu_task(void *pvParameters);