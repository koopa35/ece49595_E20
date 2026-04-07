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

float PDTable[3] = {1.25,-0.25, 0 };
//----------LOGGING----------
static const char TAG[] = "i2s_out_plasma_spkr";

//----------INITIALIZATION HELPERS----------
static void init_double_buffer(void)
{
    memset(buffer_pool[0].data, 0, BUF_SIZE * sizeof(int32_t));
    memset(buffer_pool[1].data, 0, BUF_SIZE * sizeof(int32_t));
    memset(buffer_pool[2].data, 0, BUF_SIZE * sizeof(int32_t));

    buffer_pool[0].length = BUF_SIZE;
    buffer_pool[1].length = BUF_SIZE;
    buffer_pool[2].length = BUF_SIZE;
}

//----------EQ FILTER GENERATION AND APPLICATION----------
void update_coeffs(int band_num, float gain)
{
    if (gain < 0.001)
    {
        gain = 0.001;
    }
    float center_f = sqrt(band_edges[band_num] * band_edges[band_num+1]);
    float w0 = 2 * M_PI * center_f / SAMPLE_RATE;
    float alpha = sin(w0) / (2 * q_factors[band_num]);
    float A = sqrt(gain);

    float b0 = 1 + alpha * A;
    float b1 = -2 * cos(w0);
    float b2 = 1 - alpha * A;
    float a0 = 1 + alpha / A;
    float a1 = -2 * cos(w0);
    float a2 = 1 - alpha / A;

    iir_coeffs[band_num][0] = b0 / a0;
    iir_coeffs[band_num][1] = b1 / a0;
    iir_coeffs[band_num][2] = b2 / a0;
    iir_coeffs[band_num][3] = a1 / a0;
    iir_coeffs[band_num][4] = a2 / a0;
    ESP_LOGI("update_gain", "band %d gain %f", band_num, gain);
}

void generate_EQ_filters(float* gains_arr, float* edges, float* q_factors)
{
    for (int i = 0; i < EQ_BANDS; i++)
    {
        float center_f = 0;
        if (edges[i] == 0)
        {
            center_f = (edges[i] + edges[i+1]) / 2; //  arithmetic mean 
        }
        else
        {
            center_f = sqrt(edges[i] * edges[i+1]); // geometric mean
        }

        //float q_factor = 0.707 * center_f / (edges[i+1] - edges[i]);
        
        if (1)
        {
            float w0 = 2 * M_PI * center_f / SAMPLE_RATE;
            float alpha = sin(w0) / (2 * q_factors[i]);
            float A = sqrt(gains_arr[i]);

            float b0 = 1 + alpha * A;
            float b1 = -2 * cos(w0);
            float b2 = 1 - alpha * A;
            float a0 = 1 + alpha / A;
            float a1 = -2 * cos(w0);
            float a2 = 1 - alpha / A;

            iir_coeffs[i][0] = b0 / a0;
            iir_coeffs[i][1] = b1 / a0;
            iir_coeffs[i][2] = b2 / a0;
            iir_coeffs[i][3] = a1 / a0;
            iir_coeffs[i][4] = a2 / a0;
            ESP_LOGI("IIR FILTERS", "Created Filter. Range [%.0f, %.0f] Gain %.2f Q %.2f", edges[i], edges[i+1], gains_arr[i], q_factors[i]);
        }

        else if (dsps_biquad_gen_bpf_f32(iir_coeffs[i], center_f / SAMPLE_RATE, q_factors[i]) == ESP_OK)
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

static __attribute__((aligned(16))) float temp_out[BUF_SIZE];
void apply_EQ(float* input, float* output, float* eq_gains, int len)
{
    memcpy(output, input, len * sizeof(*output));

    for (int i = 0; i < EQ_BANDS; i++) // for each band
    {
        dsps_biquad_f32(output, output, len, iir_coeffs[i], iir_delay[i]);
       /* 
        dsps_biquad_f32(input, temp_out, len, iir_coeffs[i], iir_delay[i]);
        for (int j = 0; j < BUF_SIZE; j++) // for each element in the buffer in this band apply gain
        {
            output[j] += temp_out[j] / EQ_BANDS;// eq_gains[i] / EQ_BANDS;
        }*/
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
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan_adc)); // enable adc
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan_bt)); // enable bt

    vTaskDelay(pdMS_TO_TICKS(100)); // delay to ensure i2s is fully enabled before reading

    while(true) {
        i2s_chan_handle_t input_i2s_handle = (input_select == AUX) ? rx_chan_adc : rx_chan_bt;
        uint8_t bytes_multiplier = (input_select == AUX) ? 2 : 1;

        if (i2s_channel_read(input_i2s_handle, raw_rx_buf, BUF_SIZE * bytes_multiplier * sizeof(int32_t), &bytes_read, portMAX_DELAY) == ESP_OK)
        {
            int buf_idx;

            if (xQueueReceive(free_queue, &buf_idx, pdMS_TO_TICKS(20)) != pdPASS)
            {
                ESP_LOGE("i2s_read", "free_queue timeout -- too slow");
            }

            int32_t* data = buffer_pool[buf_idx].data;

            int samples_read = bytes_read / sizeof(int32_t); // = BUF_SIZE * 2
            int block_idx = 0;
            int sum = 0;
            
            if (input_select == AUX)
            {
                for (int i = 0; i < samples_read; i++)     
                {
                    int32_t left = (int32_t) (raw_rx_buf[i] >> 8);
                    i++;
                    int32_t right = (int32_t) (raw_rx_buf[i] >> 8);
                    data[block_idx]  = ((left + right ) / 2);

                    sum += data[block_idx];
                    block_idx++;
                }
            }

            else if (input_select == BLUETOOTH)
            {
                for (int i = 0; i < samples_read; i++)
                {
                    int32_t left =   (int16_t)((raw_rx_buf[i] & 0xFFFF));
                    int32_t right =  (int16_t) (( (raw_rx_buf[i] >> 16 ) & 0xFFFF));
                    data[block_idx]  = (left + right )/2;

                    sum += data[block_idx];
                    block_idx++;
                }
            }

            // send buffer to process
            running_buf_avg = sum / BUF_SIZE;

            //ESP_LOGI("time between i2s buffers INPUT", "%f ms", 1000 * (current_time - prev_time) / (float)160000000);
            if(xQueueSend(process_queue, &buf_idx, portMAX_DELAY) != pdPASS)
            {
                ESP_LOGE(TAG, "PROCESS QUEUE FULL -- DSP TOO SLOW");
            }
        }
    }
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
            float volume_scale = (input_select == AUX) ? 1 : 5;

            int32_t out_sum = 0;
            int32_t* data_out = buffer_pool[buf_idx].data;
            for (int i = 0; i < (BUF_SIZE); i++)
            {
                data_out[i] = (input_select == AUX) ? data_out[i] << 7 : data_out[i] << 16;
                int32_t sample = (int32_t)((volume/(50.0*volume_scale))*data_out[i]);
                out_sum += sample;
                i2s_buf[2*i]     = sample; // Left
                i2s_buf[2*i + 1] = sample; // Right
            }

            /* Write i2s data */
            //ESP_LOGI("output", "%d", out_sum / BUF_SIZE);
            if (i2s_channel_write(tx_chan, i2s_buf, BUF_SIZE*2*sizeof(int32_t), &bytes_written, portMAX_DELAY) != ESP_OK) 
            {
                ESP_LOGI("I2S OUTPUT", "Write Task: i2s write failed\n");
            }

            xQueueSend(free_queue, &buf_idx, portMAX_DELAY);
        }
        else
        {
            ESP_LOGI("output queue", "failed to receive");
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
    int refresh_count = 50;

    while (true){
        if (xQueueReceive(process_queue, &buf_idx, portMAX_DELAY))
        {
            int32_t* dsp_current_buffer = buffer_pool[buf_idx].data;

            // convert adc ints to floats for filtering 
            
            for (int i = 0; i < BUF_SIZE; i++)
            {
                iir_in[i] = (float)dsp_current_buffer[i];
            }

            uint32_t eq_start = esp_cpu_get_cycle_count();
            apply_EQ(iir_in, iir_out, eq_gains, BUF_SIZE); // FILTERING 
            uint32_t eq_end = esp_cpu_get_cycle_count();

            //filling filtered signals out to output & spectrum
            uint32_t pd_start = esp_cpu_get_cycle_count();
            for (int i = 0; i < BUF_SIZE; i++)
            {
                if (predistortion == ON)
                {
                    float norm_const = (input_select == AUX) ? 8388608.0f : 32768.0f;
                    float snorm = iir_out[i] / norm_const;
                    float sample_PD = PDTable[0]*snorm + PDTable[1]*snorm*snorm*snorm + PDTable[2]*snorm*snorm*snorm*snorm*snorm;
                    sample_PD = sample_PD > 1.0f ? 1.0f : sample_PD;
                    sample_PD = sample_PD < -1.0f ? -1.0f : sample_PD;
                    dsp_current_buffer[i] = (int32_t)( sample_PD*norm_const);
                }

                else
                {
                    dsp_current_buffer[i] = (int32_t) (iir_out[i]);
                }

                spectrum_buffer[i] = (input_select == AUX) ? (int16_t) (iir_out[i] / 256) : (int16_t) iir_out[i];

            }
            uint32_t pd_end = esp_cpu_get_cycle_count();

            if (xQueueSend(output_queue, &buf_idx, portMAX_DELAY) != pdPASS)
            {
                ESP_LOGI("xQueueSend DSP -> Out", "Failed to Send index to output from dsp"); 
            }

            uint32_t fft_start = esp_cpu_get_cycle_count();
            fft(BUF_SIZE, spectrum_buffer, spectrum, sample_spacing);
            uint32_t s2b_start = esp_cpu_get_cycle_count();
            spec2bins(N, NUM_BINS, spectrum, spec_binned);
            uint32_t s2b_end = esp_cpu_get_cycle_count();

            count++;
            
            if (count % refresh_count == 0)
            {
                if (count % (5*refresh_count) == 0)
                {
                    count = 0;
                    float eq_duration = 1000.0*(eq_end - eq_start) / CLK_FREQ;
                    float fft_duration = 1000.0*(s2b_start - fft_start) / CLK_FREQ;
                    float pd_duration = 1000.0*(pd_end - pd_start) / CLK_FREQ;
                    float s2b_duration = 1000.0*(s2b_end - s2b_start) / CLK_FREQ;
                    ESP_LOGI("TIDRS", "Times to Compute (ms) : EQ %.2f, PD %.2f, FFT %.2f, Spec2Bins %.2f", eq_duration, pd_duration, fft_duration, s2b_duration);
                    ESP_LOGI("CVP", "Current(A):%f, Voltage(V):%f, Power(W): %f", current, voltage,power);
                }

                float max = 15.0;
                float min = -20.0;

                //ESP_LOGI(TAG, "Spectrum Binned: %.2f %.2f %.2f %.2f %d %d %d %d", spec_binned[0], spec_binned[1], spec_binned[2], spec_binned[3], spectrum_norm[0], spectrum_norm[1], spectrum_norm[2], spectrum_norm[3]);
                for (int i = 0; i < NUM_BINS; i++)
                {
                    float val = (spec_binned[i] + 0.5*spectrum_norm[i]) / 1.5;
                    val = val < min ? min : val;
                    val = val > max ? max : val;

                    float normalized_val = (val - min) / (max - min);
                    spectrum_norm[i] = (uint8_t) (7.0 * normalized_val);
                }
            }   
        }
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
    ESP_LOGI("predistortion coefficients", "[%.2f ,%.2f, %.2f]", PDTable[0], PDTable[1], PDTable[2]);
    
    //----------BUFFER QUEUE INIT FOR DATA TRANSFER----------
    process_queue = xQueueCreate(4, sizeof(int));
    output_queue = xQueueCreate(4, sizeof(int));
    free_queue = xQueueCreate(4, sizeof(int));

    for (int i = 0; i < 3; i++)
    {
        xQueueSend(free_queue, &i, 0);
    }
    //----------TASK CREATION--------------------------------
    xTaskCreate(task_dsp, "DSP", 32768, NULL, 5, &processing_task_handle);

    //------------------I2S INITIALIZATION--------------------
    i2s_example_init_std_duplex(&tx_chan, &rx_chan_adc); 
    i2s_init_bluetooth(&rx_chan_bt); 
    xTaskCreate(i2s_example_read_task, "i2s_example_read_task", 4096, NULL, 6, NULL); 
    xTaskCreate(i2s_example_write_task, "i2s_example_write_task", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "I2S SUCCESSFULLY INITIALIZED");

    return ESP_OK;
}
