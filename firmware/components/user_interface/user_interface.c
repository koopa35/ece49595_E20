#include "user_interface.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAIN_MENU_ITEMS 8
#define VOLUME_MENU_ITEMS 1
#define METADATA_MENU_ITEMS 3
#define EQ_MENU_ITEMS 8
#define INPUT_SELECT_MENU_ITEMS 1
#define PIVT_MENU_ITEMS 4
#define RUNTIME_MENU_ITEMS 2
#define PREDISTORTION_MENU_ITEMS 1

static const char main_menu[][STR_LEN] = {
    "-Plasma Tweeter-",
    " 1. Volume      ",
    " 2. Metadata    ",
    " 3. EQ Control  ",
    " 4. Input Select",
    " 5. P/I/V/Temp  ",
    " 6. Runtime     ",
    " 7. Predistort",
    ""
};

// Menu state variables
static int8_t cursor = 0;
static int8_t prev_cursor = 0;
static int8_t update = 0;
static bool in_sub_menu = false;
static bool need_refresh = true;
static menu_type_t current_menu = MENU_MAIN;
static bool update_10x = false;
static bool prev_input_select = BLUETOOTH;
static bool prev_display_power = ON;
static uint32_t prev_runtime_total_sec = 0;

typedef struct {
    spi_device_handle_t spi_oled1;
    spi_device_handle_t spi_oled2;
    rotary_config_t *encoder_menu;
    rotary_config_t *encoder_control;
} menu_task_args_t;

esp_err_t start_menu_task(spi_device_handle_t spi_oled1, spi_device_handle_t spi_oled2, rotary_config_t *encoder_menu, rotary_config_t *encoder_control)
{
    menu_task_args_t *args = malloc(sizeof(menu_task_args_t));
    if (args == NULL) {
        return ESP_ERR_NO_MEM;
    }

    args->spi_oled1 = spi_oled1;
    args->spi_oled2 = spi_oled2;
    args->encoder_menu = encoder_menu;
    args->encoder_control = encoder_control;

    xTaskCreate(menu_task, "MenuTask", 4096, args, 5, NULL);

    return ESP_OK;
}

void menu_task(void *pvParameters)
{
    menu_task_args_t *args = (menu_task_args_t *)pvParameters;
    if (args == NULL) {
        vTaskDelete(NULL);
        return;
    }

    spi_device_handle_t spi_oled1 = args->spi_oled1;
    spi_device_handle_t spi_oled2 = args->spi_oled2;
    rotary_config_t *encoder_menu = args->encoder_menu;
    rotary_config_t *encoder_control = args->encoder_control;

    free(args);   // free early; task owns the data now

    while (1) {
        // Handle display power state
        if (display_power == OFF) {
            if (prev_display_power == ON){
                clear(spi_oled1);
                clear(spi_oled2);
                prev_display_power = OFF;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        } else {
            if (prev_display_power == OFF) {
                need_refresh = true;
                prev_display_power = ON;
            }
        }

        // draw frequency spectrum on second display
        if (current_menu == MENU_EQ) {

            uint8_t temp[16];

            for (int i = 0; i < 2*EQ_BANDS; i++) {
                if (i % 2 != 0) {
                    temp[i] = 0;
                } else {
                    temp[i] = eq_gains[i/2] < 0.25 ? 0 :
                              eq_gains[i/2] < 0.35 ? 1 :
                              eq_gains[i/2] < 0.50 ? 2 :
                              eq_gains[i/2] < 0.71 ? 3 :
                              eq_gains[i/2] < 1.41 ? 4 :
                              eq_gains[i/2] < 2.00 ? 5 :
                              eq_gains[i/2] < 2.82 ? 6 : 7;
                }
            }

                
            freq(spi_oled2, temp, 16);
            set_cursor(spi_oled2, 0, 0);
            print(spi_oled2, "--- EQ Bands ---");
        } else {
            set_cursor(spi_oled2, 0, 0);
            print(spi_oled2, "--- Spectrum ---");
            freq(spi_oled2, spectrum_norm, 16); 
        }

        // check for source select change
        if (input_select != prev_input_select) {
            prev_input_select = input_select;
            if (in_sub_menu && current_menu == MENU_INPUT_SELECT) {
                need_refresh = true;
            }
        }

        // check for button press to enable 10x adjustments
        if (check_rotary_button_pressed(encoder_control)) {
            if (check_rotary_button_pressed(encoder_menu)) {
                // both buttons pressed: reset EQ to default values
                for (int i = 0; i < EQ_BANDS; i++) {
                    eq_gains[i] = 1.0;
                    update_coeffs(i, eq_gains[i]);
                }
                need_refresh = true;         
            } else {
                update_10x = !update_10x;
            }
        }

        // check for button press to enter/exit sub-menus
        if (check_rotary_button_pressed(encoder_menu) && !check_rotary_button_pressed(encoder_menu)) {
            if (!in_sub_menu) {
                switch (cursor) {
                    case 0:
                        current_menu = MENU_VOLUME;
                        in_sub_menu = true;
                        break;
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
                    case 7:
                        current_menu = MENU_PD;
                        in_sub_menu = true;
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

        // update menu item based on rotary encoder
        update += get_rotary_direction(encoder_control) * (update_10x ? 10 : 1);
        if (update != 0) {
            switch (current_menu) {
                case MENU_MAIN :
                    volume = (volume + update > 100) ? 100 : (volume + update < 0) ? 0 : volume + update;
                    break;
                case MENU_VOLUME :
                    volume = (volume + update > 100) ? 100 : (volume + update < 0) ? 0 : volume + update;
                    need_refresh = true;
                    break;
                case MENU_EQ :
                    eq_gains[cursor] = (eq_gains[cursor] + (update * 0.1) > 4.0 ? 4.0 : 
                                       (eq_gains[cursor] + (update * 0.1) < 0.0 ? 0.0 :
                                        eq_gains[cursor] + (update * 0.1)));
                    need_refresh = true;
                    update_coeffs(cursor, eq_gains[cursor]);
                    break;
                case MENU_PD :
                    predistortion = (predistortion == ON) ? OFF : ON;
                    ESP_LOGI("PD", "PD Status: %d", predistortion);
                    need_refresh = true;

                default :
                    break;
            
            }
            update = 0;
        }

        // clamp cursor within menu bounds
        cursor += get_rotary_direction(encoder_menu);
        cursor = (cursor < 0) ? 0 : cursor;
        cursor = cursor % (
            (current_menu == MENU_MAIN) ? MAIN_MENU_ITEMS :
            (current_menu == MENU_VOLUME) ? VOLUME_MENU_ITEMS :
            (current_menu == MENU_METADATA) ? METADATA_MENU_ITEMS :
            (current_menu == MENU_EQ) ? EQ_MENU_ITEMS :
            (current_menu == MENU_INPUT_SELECT) ? INPUT_SELECT_MENU_ITEMS :
            (current_menu == MENU_PIVT) ? PIVT_MENU_ITEMS :
            (current_menu == MENU_RUNTIME) ? RUNTIME_MENU_ITEMS : 
            (current_menu == MENU_PD) ? PREDISTORTION_MENU_ITEMS : 1
        );

        // update display if cursor changed
        if (cursor != prev_cursor) {
            prev_cursor = cursor;
            need_refresh = true;
        }

        if (need_refresh) {
            draw_menu(spi_oled1, cursor, current_menu);
            need_refresh = false;
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

esp_err_t draw_menu(spi_device_handle_t spi_oled1, uint8_t cursor, menu_type_t menu) {

    clear(spi_oled1);

    switch (menu) {
        case MENU_MAIN :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;

                set_cursor(spi_oled1, row, 0);
                print(spi_oled1, main_menu[idx]);
            }

            // selection ">" indicator
            if (cursor == 0) {
                set_cursor(spi_oled1, 1, 0);
                print(spi_oled1, ">");
            } else {
                set_cursor(spi_oled1, 0, 0);
                print(spi_oled1, ">");
            }

            break;
        case MENU_VOLUME :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;
                set_cursor(spi_oled1, row, 0);
                if (idx == 0) {
                    char volume_str[STR_LEN];
                    snprintf(volume_str, STR_LEN, "Volume: %*d%%", 7, volume);
                    print(spi_oled1, volume_str);
                }
            }
            break;
        case MENU_METADATA :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;
                set_cursor(spi_oled1, row, 0);

                if (idx == 0) {
                    set_cursor(spi_oled1, row, 0);
                    print(spi_oled1, current_artist);
                } else if (idx == 1) {
                    set_cursor(spi_oled1, row, 0);
                    print(spi_oled1, current_album);
                } else if (idx == 2) {
                    set_cursor(spi_oled1, row, 0);
                    print(spi_oled1, current_track);
                } else if (idx == 3) {
                    set_cursor(spi_oled1, row, 0);
                    char runtime_str[STR_LEN];
                    snprintf(runtime_str, STR_LEN, "%02d:%02d", track_runtime_sec / 60, track_runtime_sec % 60);
                    print(spi_oled1, runtime_str);
                }
            }
            break;
        case MENU_EQ :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;
                set_cursor(spi_oled1, row, 0);
                
                char eq_str[STR_LEN];

                if (idx < EQ_BANDS) {
                    snprintf(eq_str, STR_LEN, "Band %d: %7.1f", idx, eq_gains[idx]);
                    print(spi_oled1, eq_str);
                }
                }
            break;
        case MENU_INPUT_SELECT :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;
                set_cursor(spi_oled1, row, 0);

                if (idx == 0) {
                    char bt_str[STR_LEN];
                    snprintf(bt_str, STR_LEN, "Bluetooth: %*s", 5, input_select ? "ON " : "OFF");
                    print(spi_oled1, bt_str);
                } else if (idx == 1) {
                    char aux_str[STR_LEN];
                    snprintf(aux_str, STR_LEN, "AUX: %*s", 11, !input_select ? "ON " : "OFF");
                    print(spi_oled1, aux_str);
                }
            }
            break;
        case MENU_PIVT :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;
                set_cursor(spi_oled1, row, 0);

                if (idx == 0) {
                    char pwr_str[STR_LEN];
                    snprintf(pwr_str, STR_LEN, "PWR: %.1fW", power);
                    print(spi_oled1, pwr_str);
                } else if (idx == 1) {
                    char tmp_str[STR_LEN];
                    snprintf(tmp_str, STR_LEN, "TMP: %.1fC", temperature);
                    print(spi_oled1, tmp_str);
                } else if (idx == 2) {
                    char vdc_str[STR_LEN];
                    snprintf(vdc_str, STR_LEN, "VDC: %.1fV", voltage);
                    print(spi_oled1, vdc_str);
                } else if (idx == 3) {
                    char idc_str[STR_LEN];
                    snprintf(idc_str, STR_LEN, "IDC: %.1fA", current);
                    print(spi_oled1, idc_str);
                }
            }
            break;
        case MENU_RUNTIME :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;
                set_cursor(spi_oled1, row, 0);

                if (idx == 0) {
                    char runtime_str[STR_LEN];
                    int hours = runtime_total_sec / 3600;
                    int minutes = (runtime_total_sec % 3600) / 60;
                    snprintf(runtime_str, STR_LEN, "UP: %*d:%02d", 9, hours, minutes);
                    print(spi_oled1, runtime_str);
                } else if (idx == 1) {
                    char change_str[STR_LEN];
                    int nchours = next_change_sec / 3600;
                    int nminutes = (next_change_sec % 3600) / 60;
                    snprintf(change_str, STR_LEN, "CHNG: %*d:%02d", 7, nchours, nminutes);
                    print(spi_oled1, change_str);
                }
            }
            break;
        case MENU_PD :
            for (int row = 0; row < 2; row++) {
                uint8_t idx = cursor + row;
                set_cursor(spi_oled1, row, 0);

                if (idx == 0) {
                    char pd_str[STR_LEN];
                    snprintf(pd_str, STR_LEN, "Predistort.: %*s", 3, predistortion ? "ON " : "OFF");
                    print(spi_oled1, pd_str);
                }
            } 
            break;
        default :
            break;
    }

    return ESP_OK;
}
