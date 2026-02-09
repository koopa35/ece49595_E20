#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rom/ets_sys.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include "sdkconfig.h"

#include "dsp.h"
#include "global_defs.h"
#include "global_vars.h"
#include "hardware_configs.h"
#include "1602A_OLED.h"
#include "user_interface.h"

//----------LOGGING----------
static const char TAG[] = "i2s_out_plasma_spkr";

//----------INITIALIZATION HELPERS----------
static void init_double_buffer(void)
{
    memset(buffer_pool[0].data, 0, BUF_SIZE * sizeof(int16_t));
    memset(buffer_pool[1].data, 0, BUF_SIZE * sizeof(int16_t));
    memset(buffer_pool[2].data, 0, BUF_SIZE * sizeof(int16_t));

    buffer_pool[0].length = BUF_SIZE;
    buffer_pool[1].length = BUF_SIZE;
    buffer_pool[2].length = BUF_SIZE;
}

//----------EQ FILTER GENERATION AND APPLICATION----------
void generate_EQ_filters(float* gains_arr, float* edges, float* q_factors)
{
    for (int i = 0; i < EQ_BANDS; i++)
    {
        float center_f = 0;
        if (i == 0)
        {
            center_f = (edges[i] + edges[i+1]) / 2; //  arithmetic mean 
        }
        else
        {
            center_f = sqrt(edges[i] * edges[i+1]); // geometric mean
        }

        float q_factor = 1 * center_f / (edges[i+1] - edges[i]);

        if (dsps_biquad_gen_bpf0db_f32(iir_coeffs[i], center_f / SAMPLE_RATE, q_factors[i]) == ESP_OK)
        {
            ESP_LOGI("IIR FILTERS", "Successfully Created Filter. Range [%.0f, %.0f] Gain %.2f", edges[i], edges[i+1], gains_arr[i]);
        }
        else
        {
            ESP_LOGI("IIR FILTERS", "Error!! Failed to create filter. Range [%.0f, %.0f] Gain %.2f", edges[i], edges[i+1], gains_arr[i]);
        }

        memset(iir_delay[i], 0, sizeof(iir_delay[i]));
    }
}

void apply_EQ(float* input, float* output, float* eq_gains, int len)
{
    memset(iir_out, 0, BUF_SIZE * sizeof(*iir_out));

    float temp_out[BUF_SIZE];
    for (int i = 0; i < EQ_BANDS; i++) // for each band
    {
        dsps_biquad_f32(input, temp_out, len, iir_coeffs[i], iir_delay[i]);

        for (int j = 0; j < BUF_SIZE; j++) // for each element in the buffer in this band apply gain
        {
            iir_out[j] += temp_out[j] * eq_gains[i];
        }
    }
}

//----------FFT AND SPECTRUM PROCESSING----------
void fft(int N, int16_t* x, float* spectrum, float delta)
{
    static __attribute__((aligned(16))) float fft_buf[2*BUF_SIZE]; 
    // apply blackman window to signal
    for (int i = 0; i < N; i++)
    {
        fft_buf[2*i] = window[i] * x[i];
        fft_buf[2*i + 1] = 0;
    }

    dsps_fft2r_fc32(fft_buf, N); // run fft
    dsps_bit_rev2r_fc32(fft_buf, N); // bit reverse
    dsps_cplx2real_fc32(fft_buf, N); // cuts spectrum in half (since real inputs have symmetric spectra)
    
    for (int i = 0; i < N/2; i++)
    {
        spectrum[2*i]  = delta * fft_buf[2*i]; // real
        spectrum[2*i + 1] = delta * fft_buf[2*i + 1]; // imag
    }
}

void spec2bins(int N, int N_bins, float* spectrum, float* spectrum_binned)
{
    float freq_spacing = SAMPLE_RATE / N;

    for (int i = 0; i < N_bins; i++)
    { 
        float sum = 0;
        int idxs_per_bin_count = 0;
        for (int j = 0; j < N/2; j++)
        {
            if (j*freq_spacing >= freq_bins[i] && j*freq_spacing < freq_bins[i+1])
            {
                sum += (spectrum[2*j] * spectrum[2*j] + spectrum[2*j + 1] * spectrum[2*j + 1]);
                idxs_per_bin_count++;
            } 
        }
        float avg_power = (sum > 0) ? sum / idxs_per_bin_count : 1e-12;
        spectrum_binned[i] = 10*log10(avg_power);
    }
}

//----------I2S AUDIO TASKS----------
void i2s_example_read_task(void *pvParameters)
{
    static int32_t raw_rx_buf[2* BUF_SIZE];
    size_t bytes_read = 0;

    // ESP_ERROR_CHECK(i2s_channel_enable(rx_chan_adc)); // enable adc
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan_bt)); // enable bt

    vTaskDelay(pdMS_TO_TICKS(100)); // delay to ensure i2s is fully enabled before reading

    while(true) {
        i2s_chan_handle_t input_i2s_handle = (input_select == AUX) ? rx_chan_adc : rx_chan_bt;

        if (i2s_channel_read(input_i2s_handle, raw_rx_buf, BUF_SIZE * 2 * sizeof(int32_t), &bytes_read, portMAX_DELAY) == ESP_OK)
        {
            int buf_idx;
            xQueueReceive(free_queue, &buf_idx, portMAX_DELAY);
            int16_t* data = buffer_pool[buf_idx].data;

            int samples_read = bytes_read / sizeof(int32_t); // = BUF_SIZE * 2
            int block_idx = 0;
            int sum = 0;

            for (int i = 0; i < samples_read; i+=2) // only taking from left channel with i+= 2
            {
                if (input_select == AUX)
                {
                    data[block_idx]  = (int16_t)(raw_rx_buf[i] >> 16);
                }
                else if (input_select == BLUETOOTH)
                {
                    data[block_idx]  = (int16_t)(raw_rx_buf[i]);
                }

                sum += data[block_idx];
                block_idx++;
            }

            // send buffer to process
            running_buf_avg = sum / BUF_SIZE;

            if(xQueueSend(process_queue, &buf_idx, portMAX_DELAY) != pdPASS)
            {
                ESP_LOGE(TAG, "PROCESS QUEUE FULL -- DSP TOO SLOW");
            }
        }
    }
    // vTaskDelete(NULL);
}

void i2s_example_write_task(void *args)
{
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    static int32_t i2s_buf[BUF_SIZE *2];
    int buf_idx = 0;
    size_t bytes_written = 0;

    while (1) {
        if (xQueueReceive(output_queue, &buf_idx, portMAX_DELAY) == pdPASS)
        {
            int32_t out_sum = 0;
            int16_t* data_out = buffer_pool[buf_idx].data;
            for (int i = 0; i < (BUF_SIZE); i++)
            {
                int32_t sample = (int32_t)data_out[i] * 5;
                out_sum += sample;
                int32_t s32 = sample << 16;
                i2s_buf[2*i]     = s32; // Left
                i2s_buf[2*i + 1] = s32; // Right
            }

            /* Write i2s data */
            if (i2s_channel_write(tx_chan, i2s_buf, BUF_SIZE*2*sizeof(int32_t), &bytes_written, portMAX_DELAY) != ESP_OK) 
            {
                ESP_LOGI("I2S OUTPUT", "Write Task: i2s write failed\n");
            }

            xQueueSend(free_queue, &buf_idx, portMAX_DELAY);
        }
    }
    vTaskDelete(NULL);
}

void task_dsp(void *pvParameters)
{
    int N = BUF_SIZE;
    int buf_idx = 0;
    float sample_spacing = 1.0 / SAMPLE_RATE;
    int count = 0;
    static int16_t spectrum_buffer[BUF_SIZE];

    while (true){
        if (xQueueReceive(process_queue, &buf_idx, portMAX_DELAY))
        {
            int16_t* dsp_current_buffer = buffer_pool[buf_idx].data;

            // convert adc ints to floats for filtering
            for (int i = 0; i < BUF_SIZE; i++)
            {
                iir_in[i] = (float)dsp_current_buffer[i];
            }

            apply_EQ(iir_in, iir_out, eq_gains, BUF_SIZE); // FILTERING 

            //filling filtered signals out to output & spectrum
            for (int i = 0; i < BUF_SIZE; i++)
            {
                dsp_current_buffer[i] = (int16_t) (iir_in[i]); 
                spectrum_buffer[i] = (int16_t) (iir_in[i]);
            }

            fft(BUF_SIZE, spectrum_buffer, spectrum, sample_spacing);
            spec2bins(N, NUM_BINS, spectrum, spec_binned);

            if (xQueueSend(output_queue, &buf_idx, portMAX_DELAY) != pdPASS)
            {
                ESP_LOGI("xQueueSend DSP -> Out", "Failed to Send index to output from dsp"); 
            }
            //count++;

            //if (count == 20)
            if (xSemaphoreTake(spectrum_mutex, 0) == pdPASS)
            {
                memcpy(latest_spectrum, spec_binned, sizeof(latest_spectrum));
                count = 0;
                xSemaphoreGive(spectrum_mutex);
            }
            
        }
    }
}

//----------OLED DISPLAY TASK----------
void task_oled(void *pvParameters)
{
    float spec_binned[NUM_BINS];


    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        // wait for new spectrum data from DSP task
        if (xSemaphoreTake(spectrum_mutex, pdMS_TO_TICKS(10)) == pdPASS)
        {
            memcpy(spec_binned, latest_spectrum, sizeof(spec_binned));
            xSemaphoreGive(spectrum_mutex);
            print_to_OLED(NUM_BINS, spec_binned);
        }
    }
}

void print_to_OLED(int num_bins, float* spectrum_binned)
{
    float max = 0.0;
    float min = -30.0;
    float local_norm[NUM_BINS];

ESP_LOGE(TAG, "Spectrum Binned: %.2f, %.2f, %.2f, %.2f",
         spectrum_binned[0], spectrum_binned[1],
         spectrum_binned[2], spectrum_binned[3]);    
    
    for (int i = 0; i < NUM_BINS; i++)
    {
        local_norm[i] = spectrum_binned[i] < min ? min : spectrum_binned[i];
        local_norm[i] = spectrum_binned[i] > max ? max : spectrum_binned[i];
        local_norm[i] = 8 * ((local_norm[i] - min) / (max - min));
        spectrum_norm[i] = (uint8_t) local_norm[i];
    }
}

//----------PUBLIC DSP INITIALIZATION----------
esp_err_t dsp_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    init_double_buffer();
    spectrum_mutex = xSemaphoreCreateMutex();

    //----------FFT INITIALIZATIONS-------
    dsps_wind_blackman_f32(window, BUF_SIZE); // generate blackman window for fft
    ESP_ERROR_CHECK(dsps_fft2r_init_fc32(NULL, 2*BUF_SIZE)); // Initialize FFT2R
    ESP_ERROR_CHECK(dsps_fft4r_init_fc32(NULL, 2*BUF_SIZE)); // Initialize FFT4R

    //-----------------IIR FILTER INITIALIZATION---------------------
    generate_EQ_filters(eq_gains, band_edges, q_factors);
    ESP_LOGI(TAG, "IIR INITIALIZED -- COEFFICIENTS SET");
    
    //----------BUFFER QUEUE INIT FOR DATA TRANSFER----------
    process_queue = xQueueCreate(4, sizeof(int));
    output_queue = xQueueCreate(4, sizeof(int));
    free_queue = xQueueCreate(4, sizeof(int));
    for (int i = 0; i < 3; i++)
    {
        xQueueSend(free_queue, &i, 0);
    }
    //----------TASK CREATION-------------------------------- 
    xTaskCreate(task_dsp, "DSP", 24576, NULL, 5, &processing_task_handle);

    //----------OLED SPECTRUM UPDATE TASK--------------------
    xTaskCreate(task_oled, "task_oled", 4096, NULL, 4, NULL);

    //------------------I2S INITIALIZATION--------------------
    i2s_example_init_std_duplex(&tx_chan, &rx_chan_adc); 
    i2s_init_bluetooth(&rx_chan_bt); 
    xTaskCreate(i2s_example_read_task, "i2s_example_read_task", 4096, NULL, 6, NULL); 
    xTaskCreate(i2s_example_write_task, "i2s_example_write_task", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "I2S SUCCESSFULLY INITIALIZED");

    return ESP_OK;
}
