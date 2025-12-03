#ifndef MAIN_H
#define MAIN_H
#include <rom/ets_sys.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_adc/adc_continuous.h"
#include "esp_dsp.h"
#include "sdkconfig.h"
#include "sdm.h"

#define BUF_SIZE 256
#define NUM_BINS 16 
#define SAMPLE_RATE 44100

extern TaskHandle_t audio_decoding_task_handle;
typedef struct {
    int16_t data[BUF_SIZE];
    size_t length;
} DataBlock;

void fft(int, int16_t*, float*, float);
void spec2bins(int N, int N_bins, float* spectrum, float* spectrum_binned);
void task_dsp(void*);
void task_adc_sample(void*);
void print_to_OLED(int, float*);
void plot_spec_to_lcd(int N_bins, float* spectrum);
void task_output(void* pvParameters);
void task_audio_decode(void *args);
void task_oled(void *args);
bool IRAM_ATTR example_timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);

#endif // end of header file
