#include <stdio.h>
#include "user_interface.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAIN_MENU_ITEMS 6
#define VOLUME_MENU_ITEMS 1
#define METADATA_MENU_ITEMS 3
#define EQ_MENU_ITEMS 7
#define INPUT_SELECT_MENU_ITEMS 2
#define PIVT_MENU_ITEMS 4
#define RUNTIME_MENU_ITEMS 2

extern float voltage;
extern float current;
extern float power;
extern float temperature;
extern uint16_t volume;
extern uint16_t equalizer_band0;
extern uint16_t equalizer_band1;
extern uint16_t equalizer_band2;
extern uint16_t equalizer_band3;
extern uint16_t equalizer_band4;
extern uint16_t equalizer_band5;
extern uint16_t equalizer_band6;
extern uint16_t equalizer_band7;
extern bool bluetooth_status;
extern bool aux_status;
extern char current_track[STR_LEN];
extern char current_artist[STR_LEN];
extern char current_album[STR_LEN];
extern uint16_t track_runtime_sec;
extern uint32_t runtime_total_sec;
extern uint32_t next_change_sec;

const char main_menu[][STR_LEN] = {
    "-Plasma Tweeter-",
    " 1. Volume      ",
    " 2. Metadata    ",
    " 3. EQ Control  ",
    " 4. Input Select",
    " 5. P/I/V/Temp  ",
    " 6. Runtime     ",
    ""
};

// Menu state variables
static int8_t cursor = 0;
static int8_t prev_cursor = 0;
static bool in_sub_menu = false;
static bool need_refresh = true;
static menu_type_t current_menu = MENU_MAIN;

typedef struct {
    spi_device_handle_t spi_oled;
    rotary_config_t *encoder;
} menu_task_args_t;

void menu_task(void *pvParameters)
{
    menu_task_args_t *args = (menu_task_args_t *)pvParameters;
    if (args == NULL) {
        vTaskDelete(NULL);
        return;
    }

    spi_device_handle_t spi_oled = args->spi_oled;
    rotary_config_t *encoder = args->encoder;

    free(args);   // free early; task owns the data now

    while (1) {
        if (check_rotary_button_pressed(encoder)) {
            if (!in_sub_menu) {
                switch (cursor) {
                    case 1:
                        current_menu = MENU_VOLUME;
                        in_sub_menu = true;
                        break;
                    case 2:
                        current_menu = MENU_METADATA;
                        in_sub_menu = true;
                        break;
                    case 3:
                        current_menu = MENU_EQ;
                        in_sub_menu = true;
                        break;
                    case 4:
                        current_menu = MENU_INPUT_SELECT;
                        in_sub_menu = true;
                        break;
                    case 5:
                        current_menu = MENU_PIVT;
                        in_sub_menu = true;
                        break;
                    case 6:
                        current_menu = MENU_RUNTIME;
                        in_sub_menu = true;
                        break;
                    default:
                        break;
                }
                cursor = 0;
                need_refresh = true;
            } else {
                current_menu = MENU_MAIN;
                cursor = 0;
                in_sub_menu = false;
                need_refresh = true;
            }
        }

        cursor += get_rotary_direction(encoder);
        cursor = (cursor < 0) ? 0 : cursor;
        cursor = cursor % (
            (current_menu == MENU_MAIN) ? MAIN_MENU_ITEMS :
            (current_menu == MENU_VOLUME) ? VOLUME_MENU_ITEMS :
            (current_menu == MENU_METADATA) ? METADATA_MENU_ITEMS :
            (current_menu == MENU_EQ) ? EQ_MENU_ITEMS :
            (current_menu == MENU_INPUT_SELECT) ? INPUT_SELECT_MENU_ITEMS :
            (current_menu == MENU_PIVT) ? PIVT_MENU_ITEMS :
            (current_menu == MENU_RUNTIME) ? RUNTIME_MENU_ITEMS : 1
        );

        if (cursor != prev_cursor) {
            prev_cursor = cursor;
            need_refresh = true;
        }

        if (need_refresh) {
            draw_menu(spi_oled, cursor, current_menu);
            need_refresh = false;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

esp_err_t start_menu_task(spi_device_handle_t spi_oled, rotary_config_t *encoder)
{
    menu_task_args_t *args = malloc(sizeof(menu_task_args_t));
    if (args == NULL) {
        return ESP_ERR_NO_MEM;
    }

    args->spi_oled = spi_oled;
    args->encoder = encoder;

    xTaskCreate(menu_task, "MenuTask", 4096, args, 5, NULL);

    return ESP_OK;
}

esp_err_t draw_menu(spi_device_handle_t spi_oled, uint8_t cursor, menu_type_t menu) {

    clear(spi_oled);

    switch (menu) {
        case MENU_MAIN :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;

                set_cursor(spi_oled, row, 0);
                print(spi_oled, main_menu[idx]);
            }

            // selection ">" indicator
            if (cursor == 0) {
                set_cursor(spi_oled, 1, 0);
                print(spi_oled, ">");
            } else {
                set_cursor(spi_oled, 0, 0);
                print(spi_oled, ">");
            }

            break;
        case MENU_VOLUME :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;
                set_cursor(spi_oled, row, 0);
                if (idx == 0) {
                    char volume_str[STR_LEN];
                    snprintf(volume_str, STR_LEN, "Volume: %*d%%", 7, volume);
                    print(spi_oled, volume_str);
                }
            }
            break;
        case MENU_METADATA :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;
                set_cursor(spi_oled, row, 0);

                if (idx == 0) {
                    set_cursor(spi_oled, row, 0);
                    print(spi_oled, current_artist);
                } else if (idx == 1) {
                    set_cursor(spi_oled, row, 0);
                    print(spi_oled, current_album);
                } else if (idx == 2) {
                    set_cursor(spi_oled, row, 0);
                    print(spi_oled, current_track);
                } else if (idx == 3) {
                    set_cursor(spi_oled, row, 0);
                    char runtime_str[STR_LEN];
                    snprintf(runtime_str, STR_LEN, "%02d:%02d", track_runtime_sec / 60, track_runtime_sec % 60);
                    print(spi_oled, runtime_str);
                }
            }
            break;
        case MENU_EQ :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;
                set_cursor(spi_oled, row, 0);
                
                char eq_str[STR_LEN];
                uint16_t band_value = 0;
                switch (idx) {
                    case 0: band_value = equalizer_band0; break;
                    case 1: band_value = equalizer_band1; break;
                    case 2: band_value = equalizer_band2; break;
                    case 3: band_value = equalizer_band3; break;
                    case 4: band_value = equalizer_band4; break;
                    case 5: band_value = equalizer_band5; break;
                    case 6: band_value = equalizer_band6; break;
                    case 7: band_value = equalizer_band7; break;
                    default: break;
                }
                    snprintf(eq_str, STR_LEN, "Band %d: %*d%%", idx, 7, band_value);
                    print(spi_oled, eq_str);
                }
            break;
        case MENU_INPUT_SELECT :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;
                set_cursor(spi_oled, row, 0);

                if (idx == 0) {
                    char bt_str[STR_LEN];
                    snprintf(bt_str, STR_LEN, "Bluetooth: %*s", 5, bluetooth_status ? "ON " : "OFF");
                    print(spi_oled, bt_str);
                } else if (idx == 1) {
                    char aux_str[STR_LEN];
                    snprintf(aux_str, STR_LEN, "AUX: %*s", 11, aux_status ? "ON " : "OFF");
                    print(spi_oled, aux_str);
                }
            }
            break;
        case MENU_PIVT :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;
                set_cursor(spi_oled, row, 0);

                if (idx == 0) {
                    char pwr_str[STR_LEN];
                    snprintf(pwr_str, STR_LEN, "PWR: %.1fW", power);
                    print(spi_oled, pwr_str);
                } else if (idx == 1) {
                    char tmp_str[STR_LEN];
                    snprintf(tmp_str, STR_LEN, "TMP: %.1fC", temperature);
                    print(spi_oled, tmp_str);
                } else if (idx == 2) {
                    char vdc_str[STR_LEN];
                    snprintf(vdc_str, STR_LEN, "VDC: %.1fV", voltage);
                    print(spi_oled, vdc_str);
                } else if (idx == 3) {
                    char idc_str[STR_LEN];
                    snprintf(idc_str, STR_LEN, "IDC: %.1fA", current);
                    print(spi_oled, idc_str);
                }
            }
            break;
        case MENU_RUNTIME :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;
                set_cursor(spi_oled, row, 0);

                if (idx == 0) {
                    char runtime_str[STR_LEN];
                    int hours = runtime_total_sec / 3600;
                    int minutes = (runtime_total_sec % 3600) / 60;
                    snprintf(runtime_str, STR_LEN, "UP: %*d:%02d", 9, hours, minutes);
                    print(spi_oled, runtime_str);
                } else if (idx == 1) {
                    char change_str[STR_LEN];
                    int nchours = next_change_sec / 3600;
                    int nminutes = (next_change_sec % 3600) / 60;
                    snprintf(change_str, STR_LEN, "CHNG: %*d:%02d", 7, nchours, nminutes);
                    print(spi_oled, change_str);
                }
            }
            break;
        default :
            break;
    }

    return ESP_OK;
}