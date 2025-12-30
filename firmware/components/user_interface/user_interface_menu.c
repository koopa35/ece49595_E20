#include "user_interface.h"
#include "esp_log.h"
#include "rotary_encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

// External variables
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

// Menu state variables
static uint8_t cursor = 0;
static uint8_t prev_cursor = 0;
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

        if (cursor != prev_cursor) {
            prev_cursor = cursor;
            need_refresh = true;
        }

        if (need_refresh) {
            draw_menu(spi_oled, cursor, current_menu);
            need_refresh = false;
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // 500ms is very sluggish for UI
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
