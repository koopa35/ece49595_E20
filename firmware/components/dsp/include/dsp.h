#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"


esp_err_t dsp_init(void);
void i2s_example_read_task(void *pvParameters);
void i2s_example_write_task(void *args);
void task_dsp(void *pvParameters);
void generate_EQ_filters(float* gains_arr, float* edges, float* q_factors);
void apply_EQ(float* input, float* output, float* eq_gains, int len);
void fft(int N, int16_t* x, float* spectrum, float delta);
void spec2bins(int N, int N_bins, float* spectrum, float* spectrum_binned);
void task_oled(void *pvParameters);
void print_to_OLED(int num_bins, float* spectrum_binned);