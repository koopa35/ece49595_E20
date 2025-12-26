#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "menu.h"
#include "1602A_OLED.h"

// Forward declarations
static menu_t menu_input_selection;
static menu_t menu_equalizer_settings;
static menu_t menu_playback_metadata;
static menu_t menu_runtime_reporter;
static menu_t menu_vip_temp_readings;
static menu_t menu_main;

// Value indices (must match order in menu_values_t)
#define VAL_IDX_VOLTAGE 0
#define VAL_IDX_CURRENT 1
#define VAL_IDX_POWER 2
#define VAL_IDX_TEMP 3
#define VAL_IDX_EQ_BAND0 4
#define VAL_IDX_EQ_BAND1 5
#define VAL_IDX_EQ_BAND2 6
#define VAL_IDX_EQ_BAND3 7
#define VAL_IDX_EQ_BAND4 8
#define VAL_IDX_EQ_BAND5 9
#define VAL_IDX_EQ_BAND6 10
#define VAL_IDX_EQ_BAND7 11
#define VAL_IDX_BLUETOOTH 12
#define VAL_IDX_AUX 13
#define VAL_IDX_TRACK 14
#define VAL_IDX_ARTIST 15
#define VAL_IDX_TRACK_RUNTIME 16
#define VAL_IDX_RUNTIME_TOTAL 17
#define VAL_IDX_NEXT_CHANGE 18

// Input Selection submenu items
static menu_item_t input_selection_items[] = {
    {.label = "Bluetooth:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_STRING, .value_index = VAL_IDX_BLUETOOTH, .value_format = NULL},
    {.label = "Auxiliary:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_STRING, .value_index = VAL_IDX_AUX, .value_format = NULL},
};

// Equalizer Settings submenu items
static menu_item_t equalizer_items[] = {
    {.label = "Band 0:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_EQ_BAND0, .value_format = "%d%%"},
    {.label = "Band 1:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_EQ_BAND1, .value_format = "%d%%"},
    {.label = "Band 2:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_EQ_BAND2, .value_format = "%d%%"},
    {.label = "Band 3:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_EQ_BAND3, .value_format = "%d%%"},
    {.label = "Band 4:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_EQ_BAND4, .value_format = "%d%%"},
    {.label = "Band 5:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_EQ_BAND5, .value_format = "%d%%"},
    {.label = "Band 6:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_EQ_BAND6, .value_format = "%d%%"},
    {.label = "Band 7:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_EQ_BAND7, .value_format = "%d%%"},
};

// Playback Metadata submenu items
static menu_item_t playback_metadata_items[] = {
    {.label = "", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_STRING, .value_index = VAL_IDX_TRACK, .value_format = NULL},
    {.label = "", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_STRING, .value_index = VAL_IDX_ARTIST, .value_format = NULL},
    {.label = "", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_TRACK_RUNTIME, .value_format = "%d:%02d"},
};

// Run-time Reporter submenu items
static menu_item_t runtime_reporter_items[] = {
    {.label = "Runtime:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_RUNTIME_TOTAL, .value_format = "%d:%02d"},
    {.label = "Change:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_NEXT_CHANGE, .value_format = "%d:%02d"},
};

// V/I/P/Temp Readings submenu items
static menu_item_t vip_temp_items[] = {
    {.label = "Power:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_FLOAT, .value_index = VAL_IDX_POWER, .value_format = "%.2f W"},
    {.label = "Temp:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_FLOAT, .value_index = VAL_IDX_TEMP, .value_format = "%.1f C"},
    {.label = "VDC:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_FLOAT, .value_index = VAL_IDX_VOLTAGE, .value_format = "%.2f V"},
    {.label = "IDC:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_FLOAT, .value_index = VAL_IDX_CURRENT, .value_format = "%.2f A"},
};

// Main menu items
static menu_item_t main_menu_items[] = {
    {.label = "Inputs", .type = MENU_ITEM_TYPE_SUBMENU, .submenu = &menu_input_selection, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
    {.label = "Equalizer", .type = MENU_ITEM_TYPE_SUBMENU, .submenu = &menu_equalizer_settings, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
    {.label = "Metadata", .type = MENU_ITEM_TYPE_SUBMENU, .submenu = &menu_playback_metadata, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
    {.label = "Volume", .type = MENU_ITEM_TYPE_ACTION, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
    {.label = "Runtime", .type = MENU_ITEM_TYPE_SUBMENU, .submenu = &menu_runtime_reporter, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
    {.label = "V/I/P/Temp", .type = MENU_ITEM_TYPE_SUBMENU, .submenu = &menu_vip_temp_readings, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
};

// Menu definitions
static menu_t menu_input_selection = {
    .title = "Input Select",
    .items = input_selection_items,
    .item_count = sizeof(input_selection_items) / sizeof(menu_item_t),
    .selected_index = 0,
    .scroll_offset = 0,
    .parent = &menu_main,
};

static menu_t menu_equalizer_settings = {
    .title = "Equalizer",
    .items = equalizer_items,
    .item_count = sizeof(equalizer_items) / sizeof(menu_item_t),
    .selected_index = 0,
    .scroll_offset = 0,
    .parent = &menu_main,
};

static menu_t menu_playback_metadata = {
    .title = "Metadata",
    .items = playback_metadata_items,
    .item_count = sizeof(playback_metadata_items) / sizeof(menu_item_t),
    .selected_index = 0,
    .scroll_offset = 0,
    .parent = &menu_main,
};

static menu_t menu_runtime_reporter = {
    .title = "Runtime Report",
    .items = runtime_reporter_items,
    .item_count = sizeof(runtime_reporter_items) / sizeof(menu_item_t),
    .selected_index = 0,
    .scroll_offset = 0,
    .parent = &menu_main,
};

static menu_t menu_vip_temp_readings = {
    .title = "V/I/P/Temp",
    .items = vip_temp_items,
    .item_count = sizeof(vip_temp_items) / sizeof(menu_item_t),
    .selected_index = 0,
    .scroll_offset = 0,
    .parent = &menu_main,
};

static menu_t menu_main = {
    .title = "Main Menu",
    .items = main_menu_items,
    .item_count = sizeof(main_menu_items) / sizeof(menu_item_t),
    .selected_index = 0,
    .scroll_offset = 0,
    .parent = NULL,
};

static void update_scroll_offset(menu_t *menu) {
    if (menu->item_count <= MENU_DISPLAY_ROWS) {
        menu->scroll_offset = 0;
        return;
    }
    if (menu->selected_index < menu->scroll_offset) {
        menu->scroll_offset = menu->selected_index;
    } else if (menu->selected_index >= menu->scroll_offset + MENU_DISPLAY_ROWS) {
        menu->scroll_offset = menu->selected_index - MENU_DISPLAY_ROWS + 1;
    }
}

static void get_value_string(menu_system_t *menu_sys, menu_item_t *item, char *buffer, size_t buffer_size) {
    if (item->value_type == MENU_VALUE_TYPE_NONE) {
        snprintf(buffer, buffer_size, "N/A");
        return;
    }
    
    switch (item->value_type) {
        case MENU_VALUE_TYPE_FLOAT: {
            float val = 0.0f;
            switch (item->value_index) {
                case VAL_IDX_VOLTAGE: val = menu_sys->values.voltage; break;
                case VAL_IDX_CURRENT: val = menu_sys->values.current; break;
                case VAL_IDX_POWER: val = menu_sys->values.power; break;
                case VAL_IDX_TEMP: val = menu_sys->values.temperature; break;
            }
            if (item->value_format) {
                snprintf(buffer, buffer_size, item->value_format, val);
            } else {
                snprintf(buffer, buffer_size, "%.2f", val);
            }
            break;
        }
        case MENU_VALUE_TYPE_INT: {
            int val = 0;
            switch (item->value_index) {
                case VAL_IDX_EQ_BAND0: val = menu_sys->values.equalizer_band0; break;
                case VAL_IDX_EQ_BAND1: val = menu_sys->values.equalizer_band1; break;
                case VAL_IDX_EQ_BAND2: val = menu_sys->values.equalizer_band2; break;
                case VAL_IDX_EQ_BAND3: val = menu_sys->values.equalizer_band3; break;
                case VAL_IDX_EQ_BAND4: val = menu_sys->values.equalizer_band4; break;
                case VAL_IDX_EQ_BAND5: val = menu_sys->values.equalizer_band5; break;
                case VAL_IDX_EQ_BAND6: val = menu_sys->values.equalizer_band6; break;
                case VAL_IDX_EQ_BAND7: val = menu_sys->values.equalizer_band7; break;

                case VAL_IDX_TRACK_RUNTIME: val = menu_sys->values.track_runtime_sec; break;
                case VAL_IDX_RUNTIME_TOTAL: val = menu_sys->values.runtime_total_hr; break;
                case VAL_IDX_NEXT_CHANGE: val = menu_sys->values.next_change_hr; break;
            }
            if (item->value_format) {
                if (strcmp(item->value_format, "%f") == 0) {
                    float hours = val / 3600.0f;
                    snprintf(buffer, buffer_size, "%.2f", hours);
                } else {
                    snprintf(buffer, buffer_size, item->value_format, val);
                }
            } else {
                snprintf(buffer, buffer_size, "%d", val);
            }
            break;
        }
        case MENU_VALUE_TYPE_STRING: {
            const char *val = NULL;
            switch (item->value_index) {
                case VAL_IDX_BLUETOOTH: val = (menu_sys->values.bluetooth_status) ? "ON" : "OFF"; break;
                case VAL_IDX_AUX: val = (menu_sys->values.aux_status) ? "ON" : "OFF"; break;

                case VAL_IDX_TRACK: val = menu_sys->values.current_track; break;
                case VAL_IDX_ARTIST: val = menu_sys->values.current_artist; break;
            }
            if (val) {
                snprintf(buffer, buffer_size, "%.*s", (int)(buffer_size - 1), val);
            } else {
                snprintf(buffer, buffer_size, "N/A");
            }
            break;
        }
        default:
            snprintf(buffer, buffer_size, "N/A");
            break;
    }
}

static void display_item_row(menu_system_t *menu_sys, menu_t *menu, uint8_t item_idx, uint8_t display_row) {
    char item_line[MENU_DISPLAY_COLS + 1];
    menu_item_t *item = &menu->items[item_idx];
    const char *indicator = (item_idx == menu->selected_index) ? ">" : " ";
    const char *label = item->label;
    
    // Show value for all SETTING items, not just selected
    if (item->type == MENU_ITEM_TYPE_SETTING && 
        item->value_type != MENU_VALUE_TYPE_NONE) {
        
        char value_str[MENU_DISPLAY_COLS + 1];
        get_value_string(menu_sys, item, value_str, MENU_DISPLAY_COLS + 1);
        
        int label_len = strlen(label);
        int value_len = strlen(value_str);
        int available_space = MENU_DISPLAY_COLS - 1 - label_len - value_len;
        
        if (available_space > 0) {
            snprintf(item_line, MENU_DISPLAY_COLS + 1, "%s%s", indicator, label);
            int len = strlen(item_line);
            while (len < MENU_DISPLAY_COLS - value_len) {
                item_line[len++] = ' ';
            }
            strncpy(item_line + len, value_str, MENU_DISPLAY_COLS - len);
            item_line[MENU_DISPLAY_COLS] = '\0';
        } else {
            int max_label_len = MENU_DISPLAY_COLS - 2;
            snprintf(item_line, MENU_DISPLAY_COLS + 1, "%s%.*s", indicator, max_label_len, label);
        }
    } else {
        int max_label_len = MENU_DISPLAY_COLS - 2;
        snprintf(item_line, MENU_DISPLAY_COLS + 1, "%s%.*s", indicator, max_label_len, label);
    }
    
    int len = strlen(item_line);
    while (len < MENU_DISPLAY_COLS) {
        item_line[len++] = ' ';
    }
    item_line[MENU_DISPLAY_COLS] = '\0';
    
    set_cursor(menu_sys->spi_oled, display_row, 0);
    print(menu_sys->spi_oled, item_line);
}

static void display_menu(menu_system_t *menu_sys) {
    menu_t *menu = menu_sys->current_menu;
    display_state_t *state = &menu_sys->display_state;
    
    bool menu_changed = (state->displayed_menu != menu);
    
    if (menu_changed || menu_sys->needs_refresh) {
        clear(menu_sys->spi_oled);
        vTaskDelay(pdMS_TO_TICKS(10));
        
        state->displayed_menu = menu;
        state->displayed_selected = menu->selected_index;
        state->row_dirty[0] = true;
        state->row_dirty[1] = true;
        
        if (menu->item_count == 0) {
            char empty_line[MENU_DISPLAY_COLS + 1];
            memset(empty_line, ' ', MENU_DISPLAY_COLS);
            empty_line[MENU_DISPLAY_COLS] = '\0';
            set_cursor(menu_sys->spi_oled, 0, 0);
            print(menu_sys->spi_oled, empty_line);
            set_cursor(menu_sys->spi_oled, 1, 0);
            print(menu_sys->spi_oled, empty_line);
            state->displayed_items[0] = 0xFF;
            state->displayed_items[1] = 0xFF;
            return;
        }
    }
    
    if (menu->item_count == 0) {
        return;
    }
    
    uint8_t start_idx = menu->scroll_offset;
    uint8_t end_idx = start_idx + MENU_DISPLAY_ROWS;
    if (end_idx > menu->item_count) {
        end_idx = menu->item_count;
    }
    
    // Check if any visible items are SETTING items (for optimization)
    bool has_visible_settings = false;
    for (uint8_t i = start_idx; i < end_idx; i++) {
        menu_item_t *item = &menu->items[i];
        if (item->type == MENU_ITEM_TYPE_SETTING && 
            item->value_type != MENU_VALUE_TYPE_NONE) {
            has_visible_settings = true;
            break;
        }
    }
    
    for (uint8_t display_row = 0; display_row < MENU_DISPLAY_ROWS; display_row++) {
        uint8_t item_idx = start_idx + display_row;
        bool row_needs_update = false;
        
        if (menu_changed || menu_sys->needs_refresh) {
            row_needs_update = true;
        } else if (has_visible_settings) {
            // Always update when SETTING items are visible (to refresh values)
            row_needs_update = true;
        } else {
            if (item_idx < end_idx) {
                if (state->displayed_items[display_row] != item_idx) {
                    row_needs_update = true;
                } else if (state->displayed_selected != menu->selected_index) {
                    if (item_idx == menu->selected_index || item_idx == state->displayed_selected) {
                        row_needs_update = true;
                    }
                } else if (state->row_dirty[display_row]) {
                    row_needs_update = true;
                    state->row_dirty[display_row] = false;
                }
            } else {
                if (state->displayed_items[display_row] != 0xFF) {
                    row_needs_update = true;
                }
            }
        }
        
        // Always display rows with items
        if (item_idx < end_idx) {
            display_item_row(menu_sys, menu, item_idx, display_row);
            state->displayed_items[display_row] = item_idx;
        } else if (row_needs_update) {
            char empty_line[MENU_DISPLAY_COLS + 1];
            memset(empty_line, ' ', MENU_DISPLAY_COLS);
            empty_line[MENU_DISPLAY_COLS] = '\0';
            set_cursor(menu_sys->spi_oled, display_row, 0);
            print(menu_sys->spi_oled, empty_line);
            state->displayed_items[display_row] = 0xFF;
        }
    }
    
    state->displayed_selected = menu->selected_index;
}

esp_err_t menu_system_init(menu_system_t *menu_sys, rotary_config_t *encoder, spi_device_handle_t spi_oled) {
    if (!menu_sys || !encoder || !spi_oled) {
        return ESP_ERR_INVALID_ARG;
    }
    
    menu_sys->encoder = encoder;
    menu_sys->spi_oled = spi_oled;
    menu_sys->root_menu = &menu_main;
    menu_sys->current_menu = &menu_main;
    menu_sys->needs_refresh = true;
    
    menu_sys->nav_throttle_ms = 200;
    menu_sys->last_nav_time = 0;
    
    menu_sys->value_refresh_ms = 500;
    menu_sys->last_value_refresh = 0;
    
    menu_sys->display_state.displayed_menu = NULL;
    menu_sys->display_state.displayed_items[0] = 0xFF;
    menu_sys->display_state.displayed_items[1] = 0xFF;
    menu_sys->display_state.displayed_selected = 0xFF;
    menu_sys->display_state.row_dirty[0] = false;
    menu_sys->display_state.row_dirty[1] = false;
    
    menu_main.selected_index = 0;
    menu_main.scroll_offset = 0;
    menu_input_selection.selected_index = 0;
    menu_input_selection.scroll_offset = 0;
    menu_equalizer_settings.selected_index = 0;
    menu_equalizer_settings.scroll_offset = 0;
    menu_playback_metadata.selected_index = 0;
    menu_playback_metadata.scroll_offset = 0;
    menu_runtime_reporter.selected_index = 0;
    menu_runtime_reporter.scroll_offset = 0;
    menu_vip_temp_readings.selected_index = 0;
    menu_vip_temp_readings.scroll_offset = 0;
    
    memset(&menu_sys->values, 0, sizeof(menu_values_t));
    
    return ESP_OK;
}

void menu_update_values(menu_system_t *menu_sys, const menu_values_t *values) {
    if (!menu_sys || !values) {
        return;
    }
    menu_sys->values = *values;
    menu_sys->display_state.row_dirty[0] = true;
    menu_sys->display_state.row_dirty[1] = true;
}

void menu_system_update(menu_system_t *menu_sys) {
    if (!menu_sys || !menu_sys->current_menu) {
        return;
    }
    
    menu_t *menu = menu_sys->current_menu;
    bool refresh_needed = false;
    
    int direction = get_rotary_direction(menu_sys->encoder);
    if (direction != 0) {
        int64_t now = esp_timer_get_time() / 1000;
        
        if (now - menu_sys->last_nav_time >= menu_sys->nav_throttle_ms) {
            if (direction > 0) {
                if (menu->selected_index < menu->item_count - 1) {
                    menu->selected_index++;
                    refresh_needed = true;
                    menu_sys->last_nav_time = now;
                }
            } else {
                if (menu->selected_index > 0) {
                    menu->selected_index--;
                    refresh_needed = true;
                    menu_sys->last_nav_time = now;
                }
            }
            
            if (refresh_needed) {
                update_scroll_offset(menu);
            }
        }
    }
    
    if (check_rotary_button_pressed(menu_sys->encoder)) {
        if (menu->item_count > 0 && menu->selected_index < menu->item_count) {
            menu_item_t *item = &menu->items[menu->selected_index];
            
            if (item->type == MENU_ITEM_TYPE_SUBMENU && item->submenu != NULL) {
                menu_sys->current_menu = item->submenu;
                menu_sys->current_menu->selected_index = 0;
                menu_sys->current_menu->scroll_offset = 0;
                refresh_needed = true;
            } else if (item->type == MENU_ITEM_TYPE_ACTION && item->action != NULL) {
                item->action();
            } else if (menu->parent != NULL) {
                menu_system_navigate_back(menu_sys);
                refresh_needed = true;
            }
        } else if (menu->parent != NULL) {
            menu_system_navigate_back(menu_sys);
            refresh_needed = true;
        }
    }
    
    // Check if any visible items are SETTING items that need value refresh
    bool value_refresh_needed = false;
    if (menu->item_count > 0) {
        uint8_t start_idx = menu->scroll_offset;
        uint8_t end_idx = start_idx + MENU_DISPLAY_ROWS;
        if (end_idx > menu->item_count) {
            end_idx = menu->item_count;
        }
        
        bool has_setting_items = false;
        for (uint8_t i = start_idx; i < end_idx; i++) {
            menu_item_t *item = &menu->items[i];
            if (item->type == MENU_ITEM_TYPE_SETTING && 
                item->value_type != MENU_VALUE_TYPE_NONE) {
                has_setting_items = true;
                break;
            }
        }
        
        if (has_setting_items) {
            int64_t now = esp_timer_get_time() / 1000;
            if (now - menu_sys->last_value_refresh >= menu_sys->value_refresh_ms) {
                value_refresh_needed = true;
                menu_sys->last_value_refresh = now;
            }
        }
    }
    
    // Force refresh if values need updating
    if (value_refresh_needed) {
        menu_sys->needs_refresh = true;
    }
    
    if (refresh_needed || menu_sys->needs_refresh) {
        display_menu(menu_sys);
        menu_sys->needs_refresh = false;
    }
}

void menu_system_refresh(menu_system_t *menu_sys) {
    if (menu_sys) {
        menu_sys->needs_refresh = true;
    }
}

void menu_system_navigate_to(menu_system_t *menu_sys, menu_t *menu) {
    if (!menu_sys || !menu) {
        return;
    }
    
    menu_sys->current_menu = menu;
    menu->selected_index = 0;
    menu->scroll_offset = 0;
    menu_sys->needs_refresh = true;
}

void menu_system_navigate_back(menu_system_t *menu_sys) {
    if (!menu_sys || !menu_sys->current_menu) {
        return;
    }
    
    if (menu_sys->current_menu->parent != NULL) {
        menu_sys->current_menu = menu_sys->current_menu->parent;
        menu_sys->needs_refresh = true;
    }
}
