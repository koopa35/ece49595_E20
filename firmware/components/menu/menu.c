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
static menu_t menu_volume;
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
#define VAL_IDX_VOLUME 19

// Input Selection submenu items
static menu_item_t input_selection_items[] = {
    {.label = "Bluetooth:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_STRING, .value_index = VAL_IDX_BLUETOOTH, .value_format = NULL},
    {.label = "Auxiliary:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_STRING, .value_index = VAL_IDX_AUX, .value_format = NULL},
    {.label = "Back", .type = MENU_ITEM_TYPE_ACTION, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
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
    {.label = "Back", .type = MENU_ITEM_TYPE_ACTION, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
};

// Playback Metadata submenu items
static menu_item_t playback_metadata_items[] = {
    {.label = "", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_STRING, .value_index = VAL_IDX_TRACK, .value_format = NULL},
    {.label = "", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_STRING, .value_index = VAL_IDX_ARTIST, .value_format = NULL},
    {.label = "", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_TRACK_RUNTIME, .value_format = "MM:SS"},
    {.label = "Back", .type = MENU_ITEM_TYPE_ACTION, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
};

// Run-time Reporter submenu items
static menu_item_t runtime_reporter_items[] = {
    {.label = "Runtime:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_RUNTIME_TOTAL, .value_format = "HH:MM"},
    {.label = "Change:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_NEXT_CHANGE, .value_format = "HH:MM"},
    {.label = "Reset", .type = MENU_ITEM_TYPE_ACTION, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
    {.label = "Back", .type = MENU_ITEM_TYPE_ACTION, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
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
    {.label = "Back", .type = MENU_ITEM_TYPE_ACTION, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
};

// Volume submenu items
static menu_item_t volume_items[] = {
    {.label = "Volume:", .type = MENU_ITEM_TYPE_SETTING, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_INT, .value_index = VAL_IDX_VOLUME, .value_format = "%d%%"},
    {.label = "Back", .type = MENU_ITEM_TYPE_ACTION, .submenu = NULL, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
};

// Main menu items
static menu_item_t main_menu_items[] = {
    {.label = "Inputs", .type = MENU_ITEM_TYPE_SUBMENU, .submenu = &menu_input_selection, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
    {.label = "Equalizer", .type = MENU_ITEM_TYPE_SUBMENU, .submenu = &menu_equalizer_settings, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
    {.label = "Metadata", .type = MENU_ITEM_TYPE_SUBMENU, .submenu = &menu_playback_metadata, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
    {.label = "Volume", .type = MENU_ITEM_TYPE_SUBMENU, .submenu = &menu_volume, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
    {.label = "Runtime", .type = MENU_ITEM_TYPE_SUBMENU, .submenu = &menu_runtime_reporter, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
    {.label = "V/I/P/Temp", .type = MENU_ITEM_TYPE_SUBMENU, .submenu = &menu_vip_temp_readings, .action = NULL,
     .value_type = MENU_VALUE_TYPE_NONE, .value_index = 0, .value_format = NULL},
};

// Submenu definitions
static menu_t menu_input_selection = {
    .items = input_selection_items,
    .item_count = sizeof(input_selection_items) / sizeof(menu_item_t),
    .selected_index = 0,
    .scroll_offset = 0,
    .parent = &menu_main,
};

static menu_t menu_equalizer_settings = {
    .items = equalizer_items,
    .item_count = sizeof(equalizer_items) / sizeof(menu_item_t),
    .selected_index = 0,
    .scroll_offset = 0,
    .parent = &menu_main,
};

static menu_t menu_playback_metadata = {
    .items = playback_metadata_items,
    .item_count = sizeof(playback_metadata_items) / sizeof(menu_item_t),
    .selected_index = 0,
    .scroll_offset = 0,
    .parent = &menu_main,
};

static menu_t menu_runtime_reporter = {
    .items = runtime_reporter_items,
    .item_count = sizeof(runtime_reporter_items) / sizeof(menu_item_t),
    .selected_index = 0,
    .scroll_offset = 0,
    .parent = &menu_main,
};

static menu_t menu_vip_temp_readings = {
    .items = vip_temp_items,
    .item_count = sizeof(vip_temp_items) / sizeof(menu_item_t),
    .selected_index = 0,
    .scroll_offset = 0,
    .parent = &menu_main,
};

static menu_t menu_volume = {
    .items = volume_items,
    .item_count = sizeof(volume_items) / sizeof(menu_item_t),
    .selected_index = 0,
    .scroll_offset = 0,
    .parent = &menu_main,
};

static menu_t menu_main = {
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

                case VAL_IDX_RUNTIME_TOTAL: val = menu_sys->values.runtime_total_sec; break;
                case VAL_IDX_NEXT_CHANGE: val = menu_sys->values.next_change_sec; break;
                case VAL_IDX_VOLUME: val = menu_sys->values.volume; break;
            }
            if (item->value_format) {
                if (strcmp(item->value_format, "HH:MM") == 0) {
                    int hours = val / 3600;
                    int minutes = (val % 3600) / 60;
                    snprintf(buffer, buffer_size, "%02d:%02d", hours, minutes);
                } else if (strcmp(item->value_format, "MM:SS") == 0) {
                    int minutes = val / 60;
                    int seconds = val % 60;
                    snprintf(buffer, buffer_size, "%02d:%02d", minutes, seconds);
                } else if (strchr(item->value_format, 'f')) {
                    float hours = val / 3600.0f;
                    snprintf(buffer, buffer_size, item->value_format, hours);
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
    display_state_t *state = &menu_sys->display_state;
    
    // Check if this is a SETTING item with a value
    bool has_value = (item->type == MENU_ITEM_TYPE_SETTING && 
                      item->value_type != MENU_VALUE_TYPE_NONE);
    
    if (has_value) {
        char value_str[MENU_DISPLAY_COLS + 1];
        get_value_string(menu_sys, item, value_str, MENU_DISPLAY_COLS + 1);
        
        // Check if value has changed
        bool value_changed = (strcmp(value_str, state->last_displayed_values[display_row]) != 0);
        
        // Check if this is the first time displaying this item (item index changed)
        bool item_changed = (state->displayed_items[display_row] != item_idx);
        
        // Check if selection indicator changed (selection moved to/from this item)
        bool selection_changed = (state->displayed_selected != menu->selected_index);
        bool is_selected = (item_idx == menu->selected_index);
        bool was_selected = (item_idx == state->displayed_selected);
        bool indicator_changed = selection_changed && (is_selected || was_selected);
        
        if (item_changed || indicator_changed) {
            // First time displaying this item, write the whole line
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
            
            int len = strlen(item_line);
            while (len < MENU_DISPLAY_COLS) {
                item_line[len++] = ' ';
            }
            item_line[MENU_DISPLAY_COLS] = '\0';
            
            set_cursor(menu_sys->spi_oled, display_row, 0);
            print(menu_sys->spi_oled, item_line);
            strncpy(state->last_displayed_values[display_row], value_str, MENU_DISPLAY_COLS);
            state->last_displayed_values[display_row][MENU_DISPLAY_COLS] = '\0';
        } else if (value_changed) {
            // Value changed, only update the value portion
            // Calculate where the value starts (after indicator + label + spacing)
            int label_len = strlen(label);
            int value_start_col = 1 + label_len; // 1 for indicator
            
            // Find where value actually starts (after padding)
            // Build the label part to find the actual start position
            char label_part[MENU_DISPLAY_COLS + 1];
            snprintf(label_part, MENU_DISPLAY_COLS + 1, "%s%s", indicator, label);
            int label_part_len = strlen(label_part);
            
            // Calculate where value should start (right-aligned in remaining space)
            int value_len = strlen(value_str);
            int max_value_start = MENU_DISPLAY_COLS - value_len;
            int value_start = (label_part_len < max_value_start) ? max_value_start : label_part_len;
            
            // Set cursor to value start position and update only the value
            set_cursor(menu_sys->spi_oled, display_row, value_start);
            
            // Print the new value, padding with spaces if new value is shorter
            char value_update[MENU_DISPLAY_COLS + 1];
            snprintf(value_update, MENU_DISPLAY_COLS + 1, "%s", value_str);
            int value_update_len = strlen(value_update);
            int remaining_cols = MENU_DISPLAY_COLS - value_start;
            while (value_update_len < remaining_cols) {
                value_update[value_update_len++] = ' ';
            }
            value_update[value_update_len] = '\0';
            print(menu_sys->spi_oled, value_update);
            
            strncpy(state->last_displayed_values[display_row], value_str, MENU_DISPLAY_COLS);
            state->last_displayed_values[display_row][MENU_DISPLAY_COLS] = '\0';
        }
        // If value hasn't changed, do nothing (no refresh needed)
    } else {
        // Not a SETTING item or no value, update whole line as before
        int max_label_len = MENU_DISPLAY_COLS - 2;
        snprintf(item_line, MENU_DISPLAY_COLS + 1, "%s%.*s", indicator, max_label_len, label);
        
        int len = strlen(item_line);
        while (len < MENU_DISPLAY_COLS) {
            item_line[len++] = ' ';
        }
        item_line[MENU_DISPLAY_COLS] = '\0';
        
        set_cursor(menu_sys->spi_oled, display_row, 0);
        print(menu_sys->spi_oled, item_line);
        // Clear stored value since this row doesn't have a value
        state->last_displayed_values[display_row][0] = '\0';
    }
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
        // Reset displayed_items so all items are treated as new
        state->displayed_items[0] = 0xFF;
        state->displayed_items[1] = 0xFF;
        // Clear last displayed values when menu changes
        state->last_displayed_values[0][0] = '\0';
        state->last_displayed_values[1][0] = '\0';
        
        if (menu->item_count == 0) {
            char empty_line[MENU_DISPLAY_COLS + 1];
            memset(empty_line, ' ', MENU_DISPLAY_COLS);
            empty_line[MENU_DISPLAY_COLS] = '\0';
            set_cursor(menu_sys->spi_oled, 0, 0);
            print(menu_sys->spi_oled, empty_line);
            set_cursor(menu_sys->spi_oled, 1, 0);
            print(menu_sys->spi_oled, empty_line);
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
    
    for (uint8_t display_row = 0; display_row < MENU_DISPLAY_ROWS; display_row++) {
        uint8_t item_idx = start_idx + display_row;
        bool row_needs_update = false;
        
        if (menu_changed || menu_sys->needs_refresh) {
            row_needs_update = true;
        } else {
            if (item_idx < end_idx) {
                if (state->displayed_items[display_row] != item_idx) {
                    row_needs_update = true;
                } else if (state->displayed_selected != menu->selected_index) {
                    // Selection changed - update row if it's the selected item or was previously selected
                    if (item_idx == menu->selected_index || item_idx == state->displayed_selected) {
                        row_needs_update = true;
                    }
                } else if (state->row_dirty[display_row]) {
                    row_needs_update = true;
                    state->row_dirty[display_row] = false;
                } else if (state->check_value_changes) {
                    // Check if this row has a SETTING item with a value that might have changed
                    // display_item_row will check if value actually changed and skip update if unchanged
                    menu_item_t *item = &menu->items[item_idx];
                    if (item->type == MENU_ITEM_TYPE_SETTING && 
                        item->value_type != MENU_VALUE_TYPE_NONE) {
                        // Allow display_item_row to check if value changed (it will skip if unchanged)
                        row_needs_update = true;
                    }
                }
            } else {
                if (state->displayed_items[display_row] != 0xFF) {
                    row_needs_update = true;
                }
            }
        }
        
        // Always display rows with items (display_item_row will skip if value unchanged)
        if (item_idx < end_idx) {
            if (row_needs_update) {
                display_item_row(menu_sys, menu, item_idx, display_row);
            }
            state->displayed_items[display_row] = item_idx;
        } else if (row_needs_update) {
            char empty_line[MENU_DISPLAY_COLS + 1];
            memset(empty_line, ' ', MENU_DISPLAY_COLS);
            empty_line[MENU_DISPLAY_COLS] = '\0';
            set_cursor(menu_sys->spi_oled, display_row, 0);
            print(menu_sys->spi_oled, empty_line);
            state->displayed_items[display_row] = 0xFF;
            state->last_displayed_values[display_row][0] = '\0';
        }
    }
    
    state->displayed_selected = menu->selected_index;
    state->check_value_changes = false;  // Reset flag after checking
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
    menu_sys->display_state.last_displayed_values[0][0] = '\0';
    menu_sys->display_state.last_displayed_values[1][0] = '\0';
    menu_sys->display_state.check_value_changes = false;
    
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
    menu_volume.selected_index = 0;
    menu_volume.scroll_offset = 0;
    
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
                menu_sys->needs_refresh = true;
                refresh_needed = true;
                // Update local menu pointer to new menu
                menu = menu_sys->current_menu;
            } else if (item->type == MENU_ITEM_TYPE_ACTION && item->action != NULL) {
                item->action();
            } else if (menu->parent != NULL) {
                menu_system_navigate_back(menu_sys);
                refresh_needed = true;
                // Update local menu pointer after navigation
                menu = menu_sys->current_menu;
            }
        } else if (menu->parent != NULL) {
            menu_system_navigate_back(menu_sys);
            refresh_needed = true;
            // Update local menu pointer after navigation
            menu = menu_sys->current_menu;
        }
    }
    
    // Always call display_menu - it will intelligently skip updates when values haven't changed
    // Check if any visible items are SETTING items that need value refresh check
    // Use current_menu directly to ensure we're checking the right menu
    bool value_refresh_check_needed = false;
    menu_t *current_menu = menu_sys->current_menu;
    if (current_menu && current_menu->item_count > 0) {
        uint8_t start_idx = current_menu->scroll_offset;
        uint8_t end_idx = start_idx + MENU_DISPLAY_ROWS;
        if (end_idx > current_menu->item_count) {
            end_idx = current_menu->item_count;
        }
        
        bool has_setting_items = false;
        for (uint8_t i = start_idx; i < end_idx; i++) {
            menu_item_t *item = &current_menu->items[i];
            if (item->type == MENU_ITEM_TYPE_SETTING && 
                item->value_type != MENU_VALUE_TYPE_NONE) {
                has_setting_items = true;
                break;
            }
        }
        
        if (has_setting_items) {
            int64_t now = esp_timer_get_time() / 1000;
            if (now - menu_sys->last_value_refresh >= menu_sys->value_refresh_ms) {
                value_refresh_check_needed = true;
                menu_sys->last_value_refresh = now;
            }
        }
    }
    
    // Store flag for display_menu to check values
    menu_sys->display_state.check_value_changes = value_refresh_check_needed;
    
    if (refresh_needed || menu_sys->needs_refresh || value_refresh_check_needed) {
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
