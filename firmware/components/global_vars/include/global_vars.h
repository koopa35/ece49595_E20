#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "global_defs.h"
#include "hardware_configs.h"
// FreeRTOS types
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// Global Variables
extern float voltage;
extern float current;
extern float power;
extern float temperature;

extern uint16_t volume;

extern uint16_t equalizer_band[EQ_BANDS];

extern float eq_gains[EQ_BANDS];
extern float band_edges[EQ_BANDS + 1];
extern int freq_bins[];

extern float q_factors[EQ_BANDS];
extern float iir_coeffs[EQ_BANDS][5];
extern float iir_delay[EQ_BANDS][2];

extern float iir_out[BUF_SIZE] __attribute__((aligned(16)));
extern float iir_in[BUF_SIZE] __attribute__((aligned(16)));

extern DataBlock buffer_pool[3];
extern int32_t running_buf_avg;

extern QueueHandle_t process_queue;
extern QueueHandle_t output_queue;
extern QueueHandle_t oled_queue;

extern float window[BUF_SIZE] __attribute__((aligned(16)));
extern float spectrum[2 * BUF_SIZE] __attribute__((aligned(16)));
extern float spec_binned[NUM_BINS] __attribute__((aligned(16)));

extern i2s_chan_handle_t tx_chan;
extern i2s_chan_handle_t rx_chan_adc;
extern i2s_chan_handle_t rx_chan_bt;

extern float latest_spectrum[NUM_BINS];
extern SemaphoreHandle_t spectrum_mutex;

extern TaskHandle_t sampling_task_handle;
extern TaskHandle_t processing_task_handle;
extern TaskHandle_t output_task_handle;
extern TaskHandle_t oled_task_handle;

extern bool input_select;
extern bool display_power;

extern char current_track[STR_LEN];
extern char current_artist[STR_LEN];
extern char current_album[STR_LEN];
extern uint16_t track_runtime_sec;

extern uint32_t runtime_total_sec;
extern uint32_t next_change_sec;

extern uint8_t spectrum_norm[16];