#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/spi_master.h"
#include "rotary_encoder.h"

#define MENU_DISPLAY_ROWS 2
#define MENU_DISPLAY_COLS 16

typedef enum {
    MENU_ITEM_TYPE_SUBMENU,
    MENU_ITEM_TYPE_ACTION,
    MENU_ITEM_TYPE_SETTING
} menu_item_type_t;

typedef enum {
    MENU_VALUE_TYPE_NONE,
    MENU_VALUE_TYPE_FLOAT,
    MENU_VALUE_TYPE_INT,
    MENU_VALUE_TYPE_STRING
} menu_value_type_t;

typedef struct menu_s menu_t;

typedef struct {
    const char *label;
    menu_item_type_t type;
    menu_t *submenu;
    void (*action)(void);
    menu_value_type_t value_type;
    uint8_t value_index;
    const char *value_format;
} menu_item_t;

struct menu_s {
    menu_item_t *items;
    uint8_t item_count;
    uint8_t selected_index;
    int8_t scroll_offset;
    menu_t *parent;
};

typedef struct {
    float voltage;
    float current;
    float power;
    float temperature;
    int equalizer_band0;
    int equalizer_band1;
    int equalizer_band2;
    int equalizer_band3;
    int equalizer_band4;
    int equalizer_band5;
    int equalizer_band6;
    int equalizer_band7;
    int bluetooth_status;
    int aux_status;
    const char* current_track;
    const char* current_artist;
    int track_runtime_sec;
    int runtime_total_sec;
    int next_change_sec;
} menu_values_t;

typedef struct {
    menu_t *displayed_menu;
    uint8_t displayed_items[2];
    uint8_t displayed_selected;
    bool row_dirty[2];
} display_state_t;

typedef struct {
    menu_t *current_menu;
    menu_t *root_menu;
    rotary_config_t *encoder;
    spi_device_handle_t spi_oled;
    bool needs_refresh;
    display_state_t display_state;
    int64_t last_nav_time;
    uint32_t nav_throttle_ms;
    int64_t last_value_refresh;
    uint32_t value_refresh_ms;
    menu_values_t values;
} menu_system_t;

esp_err_t menu_system_init(menu_system_t *menu_sys, rotary_config_t *encoder, spi_device_handle_t spi_oled);
void menu_system_update(menu_system_t *menu_sys);
void menu_system_refresh(menu_system_t *menu_sys);
void menu_system_navigate_to(menu_system_t *menu_sys, menu_t *menu);
void menu_system_navigate_back(menu_system_t *menu_sys);
void menu_update_values(menu_system_t *menu_sys, const menu_values_t *values);
