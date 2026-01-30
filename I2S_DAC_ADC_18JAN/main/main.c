#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rom/ets_sys.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include "sdkconfig.h"
//---------------------
#include "1602A_OLED.h"
#include "main.h"
#include "filters.h"
#include "i2s.h"

#if CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ == 160
    static const long cpu_freq = 160000000;
#endif

#define NUM_BANDS 8
#define FIR_COEFFS_LEN 64

float eq_gains[NUM_BANDS] = {1.0, 0.5, 1.0, 0.5, 0, 0, 1.0, 1.0};
float band_edges[NUM_BANDS + 1] = {0, 350.0, 1100.0, 2200.0, 4000.0, 6000.0, 8000.0, 10500.0, SAMPLE_RATE/2};
int freq_bins[] = {0, 100, 350, 700, 1100, 1600, 2200, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10500, 12000, SAMPLE_RATE/2};

fir_f32_t fir_handles[NUM_BANDS];
const int32_t fir_len = FIR_COEFFS_LEN;
const int32_t fir_decim = 1;

static __attribute__((aligned(16))) float fir_coeffs[NUM_BANDS][FIR_COEFFS_LEN];

static __attribute__((aligned(16))) float delay_line[FIR_COEFFS_LEN];
static __attribute__((aligned(16))) float fir_out[BUF_SIZE];
static __attribute__((aligned(16))) float fir_in[BUF_SIZE];

static DataBlock buffer_pool[3];
int32_t running_buf_avg = 0;

static QueueHandle_t process_queue = NULL;
static QueueHandle_t output_queue = NULL;
static QueueHandle_t oled_queue = NULL;

__attribute__((aligned(16))) float window[BUF_SIZE];
__attribute__((aligned(16))) float spectrum[2 * BUF_SIZE];
__attribute__((aligned(16))) float spec_binned[NUM_BINS];

spi_device_handle_t spi_oled;
static i2s_chan_handle_t  tx_chan;        // I2S tx channel handler
static i2s_chan_handle_t  rx_chan;        // I2S rx channel handler

static float latest_spectrum[NUM_BINS];
static SemaphoreHandle_t spectrum_mutex = NULL;

TaskHandle_t audio_decoding_task_handle = NULL;
TaskHandle_t sampling_task_handle = NULL;
TaskHandle_t processing_task_handle = NULL;
TaskHandle_t output_task_handle = NULL;
TaskHandle_t oled_task_handle = NULL;

char* TAG = "i2s_out_plsma_spkr";

static void i2s_example_write_task(void *args)
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
                int32_t sample = (int32_t)data_out[i] * 10;
                out_sum += sample;
                int32_t s32 = sample << 16;
                i2s_buf[2*i]     = s32; // Left
                i2s_buf[2*i + 1] = s32; // Right
            }

                /* Write i2s data */
            if (i2s_channel_write(tx_chan, i2s_buf, BUF_SIZE*2*sizeof(int32_t), &bytes_written, portMAX_DELAY) != ESP_OK) 
            {
                printf("Write Task: i2s write failed\n");
            }
        }
    }
    vTaskDelete(NULL);
}
/* old adc
static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t* edata, void* user_data)
{
    BaseType_t mustYield = pdFALSE;
    vTaskNotifyGiveFromISR(sampling_task_handle, &mustYield);
    return (mustYield == pdTRUE);
}*/

static void init_double_buffer(void)
{
    memset(buffer_pool[0].data, 0, BUF_SIZE * sizeof(int16_t));
    memset(buffer_pool[1].data, 0, BUF_SIZE * sizeof(int16_t));
    memset(buffer_pool[2].data, 0, BUF_SIZE * sizeof(int16_t));

    buffer_pool[0].length = BUF_SIZE;
    buffer_pool[1].length = BUF_SIZE;
    buffer_pool[2].length = BUF_SIZE;
}

void generate_EQ_filters(int N_band, int N_fir, float* gains_arr, float* edges, float* delay)
{
    for (int i = 0; i < N_band; i++)
    {
        float temp1[N_fir];
        generate_FIR_coefficients(temp1, N_fir, edges[i+1]/ SAMPLE_RATE);
        float temp2[N_fir];
        generate_FIR_coefficients(temp2, N_fir, edges[i]/ SAMPLE_RATE);
        for (int j = 0; j < N_fir; j++)
        {
            fir_coeffs[i][j] = temp1[j] - temp2[j];
        }
        if (ESP_OK == dsps_fir_init_f32(&fir_handles[i], fir_coeffs[i], delay, N_fir))
        {
            ESP_LOGI(TAG, "Successfully Created BPF. Range [%.0f, %.0f] Gain %.2f", edges[i], edges[i+1], gains_arr[i]);
        }
        else
        {
            ESP_LOGI(TAG, "Error!! Could not create BPF. Range [%.0f, %.0f] Gain %.2f", edges[i], edges[i+1], gains_arr[i]);
        }
    }
}

void apply_EQ(int N_buf, int N_band, float* fir_out, float* eq_gains, fir_f32_t* fir_handle_arr)
{
    memset(fir_out, 0, N_buf * sizeof(*fir_out));

    float temp_out[N_buf];
    for (int i = 0; i < N_band; i++) // for each band
    {
        dsps_fir_f32(&fir_handle_arr[i], fir_in, temp_out, N_buf);

        for (int j = 0; j < N_buf; j++) // for each element in the buffer in this band apply gain
        {
            fir_out[j] += temp_out[j] * eq_gains[i];
        }
    }
}

//--------------------------ENTRANCE GATEWAY----------------------------

void app_main(void)
{
    init_double_buffer();
    //------------FILL SINE TEST------------
    uint8_t freq_test[8] = {0,1,2,3,4,5,6,7};
    //----------OLED INITIALIZATION-------
    vTaskDelay(pdMS_TO_TICKS(100));
    spi_init(&spi_oled);
    vTaskDelay(pdMS_TO_TICKS(1000));
    oled_init(spi_oled);
    vTaskDelay(pdMS_TO_TICKS(1000));
    clear(spi_oled);
    vTaskDelay(pdMS_TO_TICKS(1000));
    freq(spi_oled, freq_test, 8);
    vTaskDelay(pdMS_TO_TICKS(1000));
    oled_queue = xQueueCreate(2, sizeof(float) * NUM_BINS);
    xTaskCreate(task_oled, "OLED Printing Task", 4096, NULL, 4, &oled_task_handle);
    spectrum_mutex = xSemaphoreCreateMutex();

    //----------FFT INITIALIZATIONS-------
    dsps_wind_blackman_f32(window, BUF_SIZE); // generate hann window for fft
    ESP_ERROR_CHECK(dsps_fft2r_init_fc32(NULL, 2*BUF_SIZE)); // Initialize FFT2R
    ESP_ERROR_CHECK(dsps_fft4r_init_fc32(NULL, 2*BUF_SIZE)); // Initialize FFT2R
    ESP_LOGI(TAG, "CPU FREQ %ld", cpu_freq);

    //-----------------FIR INITIALIZATION---------------------
    generate_EQ_filters(NUM_BANDS, FIR_COEFFS_LEN, eq_gains, band_edges, delay_line);
    ESP_LOGI(TAG, "FIR INITIALIZED -- COEFFICIENTS SET");
    
    //----------BUFFER QUEUE INIT FOR DATA TRANSFER----------
    process_queue = xQueueCreate(4, sizeof(DataBlock));
    ESP_LOGI(TAG, "Succesfully Created Process Queue");
    output_queue = xQueueCreate(4, sizeof(int));
    if (output_queue != NULL)
    {
        ESP_LOGI(TAG, "Succesfully Created Output Queue");
    }
       
    //----------TASK CREATION-------------------------------- 
    xTaskCreate(task_dsp, "DSP", 16384, NULL, 6, &processing_task_handle);

    //------------------I2S INITIALIZATION--------------------
    i2s_example_init_std_simplex(&tx_chan, &rx_chan); 
    xTaskCreate(i2s_example_read_task, "i2s_example_read_task", 4096, NULL, 5, NULL); //uncomment for ADC
    xTaskCreate(i2s_example_write_task, "i2s_example_write_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "I2S SUCCESFULLY INITIALIZED");
}

void i2s_example_read_task(void *pvParameters)
{
    static int32_t raw_rx_buf[2* BUF_SIZE];
    size_t bytes_read = 0;
    DataBlock block;

    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

    while(true) {
        if (i2s_channel_read(rx_chan, raw_rx_buf, BUF_SIZE * 2 * sizeof(int32_t), &bytes_read, portMAX_DELAY) == ESP_OK)
        {
            int samples_read = bytes_read / sizeof(int32_t); // = BUF_SIZE * 2
            block.length = samples_read / 2; // only take left channel
            int block_idx = 0;
            int sum = 0;

            for (int i = 0; i < samples_read; i+=2) // only taking from left channel with  i+= 2
            {
                block.data[block_idx]  = (int16_t)(raw_rx_buf[i] >> 16); //(int16_t)(((raw_rx_buf[i]  + raw_rx_buf[i+1])/2)>> 16);
                sum += block.data[block_idx];
                block_idx++;

                if (block_idx >= BUF_SIZE) 
                {
                    break;
                }
            }

            // send buffer to process
            running_buf_avg = sum / BUF_SIZE;
            if(xQueueSend(process_queue, &block, portMAX_DELAY) != pdPASS)
            {
                ESP_LOGE(TAG, "PROCESS QUEUE FULL -- DSP TOO SLOW");
            }
        }
    }

    free(raw_rx_buf);
    vTaskDelete(NULL);
} 

void task_dsp(void *pvParameters)
{
    int N = BUF_SIZE;
    int buf_idx = 0;
    float sample_spacing = 1.0 / SAMPLE_RATE;
    int count = 0;
    int16_t spectrum_buffer[BUF_SIZE];
    DataBlock adc_block;

    if (xQueueReceive(process_queue, &adc_block, portMAX_DELAY))
    {
        memcpy((void*) buffer_pool[buf_idx].data, adc_block.data, BUF_SIZE * sizeof(int16_t));
    }

    while (true){
        if (xQueueReceive(process_queue, &adc_block, portMAX_DELAY))
        {
            memcpy(buffer_pool[buf_idx].data, adc_block.data, adc_block.length * sizeof(int16_t));
            buffer_pool[buf_idx].length = adc_block.length;     

            int16_t* dsp_current_buffer = buffer_pool[buf_idx].data;

            // convert adc ints to floats for filtering
            for (int i = 0; i < BUF_SIZE; i++)
            {
                fir_in[i] = (float)dsp_current_buffer[i];
            }

            apply_EQ(BUF_SIZE, NUM_BANDS, fir_out, eq_gains, fir_handles); // FILTERING 

            //filling filtered signals out to output & spectrum
            for (int i = 0; i < BUF_SIZE; i++)
            {
                dsp_current_buffer[i] = (int16_t) (fir_in[i]); 
                spectrum_buffer[i] = (int16_t) (fir_out[i]);
            }

            if (xQueueSend(output_queue, &buf_idx, portMAX_DELAY) == pdPASS)
            {
                buf_idx = (buf_idx + 1) % 3;
            }

            fft(BUF_SIZE, spectrum_buffer, spectrum, sample_spacing);
            spec2bins(N, NUM_BINS, spectrum, spec_binned);

            count++;

            if (count == 20)
            {
                if (xSemaphoreTake(spectrum_mutex, 0) == pdPASS)
                {
                    memcpy(latest_spectrum, spec_binned, sizeof(latest_spectrum));
                    count = 0;
                    xSemaphoreGive(spectrum_mutex);
                }
            }
        }
    }

}

void fft(int N, int16_t* x, float* spectrum, float delta)
{

    __attribute__((aligned(16))) float fft_buf[2*N]; 
    // apply hann window to signal
    for (int i = 0; i < N; i++)
    {
        fft_buf[2*i] = window[i] * x[i]; // added to normalize
        fft_buf[2*i + 1] = 0;
    }

    dsps_fft2r_fc32(fft_buf, N); // run fft
    dsps_bit_rev2r_fc32(fft_buf, N); // bit reverse
    dsps_cplx2real_fc32(fft_buf, N); // cuts spectrum in half (since real inputs have symmetric spectra) 
                                    // x looks like x[0] -> Re(F[0]), x[1] -> Im(F[0]), x[2] -> Re(F[1]) ....
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

void task_oled(void *pvParameters)
{
    float spec_binned[NUM_BINS];

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
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
    float max = 30.0;
    float min = -30.0;
    uint8_t spectrum[NUM_BINS];
    ESP_LOGI("bin_test", "%f %f %f %f", spectrum_binned[0], spectrum_binned[2], spectrum_binned[4], spectrum_binned[6]);
    
    for (int i = 0; i < NUM_BINS; i++)
    {
        spectrum_binned[i] = spectrum_binned[i] < min ? min : spectrum_binned[i];
        spectrum_binned[i] = spectrum_binned[i] > max ? max : spectrum_binned[i];
        spectrum_binned[i] = 8 * ((spectrum_binned[i] - min) / (max - min));
        spectrum[i] = (uint8_t) spectrum_binned[i];
    }

    freq(spi_oled, spectrum, NUM_BINS);
} 
