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
#include "esp_adc/adc_continuous.h"
#include "esp_dsp.h"
#include "sdkconfig.h"
//---------------------
#include "adc.h"
#include "1602A_OLED.h"
#include "main.h"
#include "sdm.h"

#if CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ == 160
    static const long cpu_freq = 160000000;
#endif

#define MOSI_PIN 5
#define SCLK_PIN 4
#define CS_PIN 6

#define NUM_BANDS 8
#define FIR_COEFFS_LEN 64

float eq_gains[NUM_BANDS] = {1.0, 0.5, 1.0, 0.5, 0, 0, 1.0, 1.0};
float band_edges[NUM_BANDS + 1] = {0, 350.0, 1100.0, 2200.0, 4000.0, 6000.0, 8000.0, 10500.0, SAMPLE_RATE/2};
int freq_bins[] = {0, 100, 350, 700, 1100, 1600, 2200, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10500, 12000, SAMPLE_RATE/2};

fir_f32_t fir_handles[NUM_BANDS];
fir_f32_t fir_low;
fir_f32_t fir_mid;
fir_f32_t fir_high;
const int32_t fir_len = FIR_COEFFS_LEN;
const int32_t fir_decim = 1;

static __attribute__((aligned(16))) float fir_coeffs[NUM_BANDS][FIR_COEFFS_LEN];

static __attribute__((aligned(16))) float delay_line[FIR_COEFFS_LEN];
static __attribute__((aligned(16))) float fir_out[BUF_SIZE];
static __attribute__((aligned(16))) float fir_in[BUF_SIZE];

static int16_t output_buffer_0[BUF_SIZE];
static int16_t output_buffer_1[BUF_SIZE];

static volatile int16_t* playing_buffer = output_buffer_0;
static volatile int16_t* filling_buffer = output_buffer_1;

static volatile uint32_t buffer_read_index = 0;
static volatile uint32_t buffer_length = BUF_SIZE;
static volatile bool buffer_swap_ready = false;

int32_t running_buf_avg = 0;

static QueueHandle_t process_queue = NULL;
static QueueHandle_t oled_queue = NULL;

__attribute__((aligned(16))) float window[BUF_SIZE];
__attribute__((aligned(16))) float spectrum[2 * BUF_SIZE];
__attribute__((aligned(16))) float spec_binned[NUM_BINS];

adc_continuous_handle_t adc_handle = NULL;
adc_channel_t ADC_CHANNEL = ADC_CHANNEL_0;
spi_device_handle_t spi_oled;
sdm_channel_handle_t sdm_chan;
DataBlock out_block;

static float latest_spectrum[NUM_BINS];
static SemaphoreHandle_t spectrum_mutex = NULL;
static SemaphoreHandle_t buffer_swap_sem = NULL;

TaskHandle_t audio_decoding_task_handle = NULL;
TaskHandle_t sampling_task_handle = NULL;
TaskHandle_t processing_task_handle = NULL;
TaskHandle_t output_task_handle = NULL;
TaskHandle_t oled_task_handle = NULL;

char* TAG = "sdm_out_plsma_spkr";

bool IRAM_ATTR example_timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    sdm_channel_handle_t sdm_chan = (sdm_channel_handle_t)user_ctx;

    int16_t sample = playing_buffer[buffer_read_index];

    int8_t pulse_den = (int8_t)(sample / 8); // convert 16bit to 8bit
    //pulse_den = pulse_den > 127 ? 127 : pulse_den;
    //pulse_den = pulse_den < -128 ? -128 : pulse_den;
    sdm_channel_set_pulse_density(sdm_chan, pulse_den); // set the pulse density 
    buffer_read_index++;
        
    if (buffer_read_index >= buffer_length)
    {
        buffer_read_index = 0;
        if (buffer_swap_ready)
        {
            volatile int16_t* temp = playing_buffer;
            playing_buffer = filling_buffer;
            filling_buffer = temp;
            
            buffer_swap_ready = false;
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(buffer_swap_sem, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }

    }
    
    return false;
}

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t* edata, void* user_data)
{
    BaseType_t mustYield = pdFALSE;
    vTaskNotifyGiveFromISR(sampling_task_handle, &mustYield);
    return (mustYield == pdTRUE);
}

static void init_double_buffer(void)
{
    buffer_swap_sem = xSemaphoreCreateBinary();

    memset(output_buffer_0, 0, sizeof(output_buffer_0));
    memset(output_buffer_1, 0, sizeof(output_buffer_1));

    buffer_read_index = 0;
    buffer_length = BUF_SIZE;
}

static void spi_init()
{
    // 1. SPI Bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 2. SPI device
    spi_device_interface_config_t devcfg = {
        .mode = 3,
        .clock_speed_hz = 1000000,
        .spics_io_num = CS_PIN,
        .queue_size = 16,
        .flags = SPI_DEVICE_HALFDUPLEX
    };

    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_oled));
}

void generate_FIR_coefficients(float *fir_coeffs, const unsigned int fir_len, const float ft)
{

    // Even or odd length of the FIR filter
    const bool is_odd = (fir_len % 2) ? (true) : (false);
    const float fir_order = (float)(fir_len - 1);

    // Window coefficients
    float *fir_window = (float *)malloc(fir_len * sizeof(float));
    dsps_wind_blackman_f32(fir_window, fir_len);

    for (int i = 0; i < fir_len; i++) {
        if ((i == fir_order / 2) && (is_odd)) {
            fir_coeffs[i] = 2 * ft;
        } else {
            fir_coeffs[i] = sinf((2 * M_PI * ft * (i - fir_order / 2))) / (M_PI * (i - fir_order / 2));
        }

        if ((!is_odd) && ((i == fir_len /2)))
        {
            //fir_coeffs[i] += 0.25;
        }

        fir_coeffs[i] *= fir_window[i];
    }

    free(fir_window);
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
    for (int i = 0; i < N_band; i++)
    {
        dsps_fir_f32(&fir_handle_arr[i], fir_in, temp_out, N_buf);

        for (int j = 0; j < N_buf; j++)
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
    spi_init();
    vTaskDelay(pdMS_TO_TICKS(100));
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
    
    //-----------------SDM INITIALIZATION--------------------
    /* Initialize sigma-delta modulation on the specific GPIO */
    sdm_chan = example_init_sdm();
    /* Initialize GPTimer and register the timer alarm callback */
    gptimer_handle_t timer_handle = example_init_gptimer(sdm_chan);


    //----------BUFFER QUEUE INIT FOR DATA TRANSFER----------
    process_queue = xQueueCreate(4, sizeof(DataBlock));
    ESP_LOGI(TAG, "Succesfully Created Process Queue");
    
    //----------TASK CREATION-------------------------------- 
#ifdef CONFIG_INPUT_SOURCE_AUX
    xTaskCreate(task_adc_sample, "ADC Sampling", 4096, NULL, 3, &sampling_task_handle);
#endif
    xTaskCreate(task_dsp, "DSP", 16384, NULL, 6, &processing_task_handle);

#ifdef CONFIG_INPUT_SOURCE_AUX
    //--------- INIT ADC & CALLBACK ------------------------------
    init_cont_adc(ADC_CHANNEL, 1, &adc_handle); // initialize adc
    
    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };

    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL)); // register callback when conv frame is full
                                                                                     
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
    ESP_LOGI(TAG, "Succesfully Started ADC in Continuous Mode");
#endif
    /* Start the GPTimer */
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "Output start");
    ESP_ERROR_CHECK(gptimer_start(timer_handle));
    while(true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#ifdef CONFIG_INPUT_SOURCE
    ESP_ERROR_CHECK(adc_continuous_stop(adc_handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(adc_handle));
#endif
}

void task_adc_sample(void *pvParameters)
{
    uint8_t result[BUF_SIZE*sizeof(adc_digi_output_data_t)];
    uint32_t bytes_read = 0;
    DataBlock block;

    while(true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        esp_err_t ret = adc_continuous_read(adc_handle,result, sizeof(result), &bytes_read, portMAX_DELAY);
        if (ret == ESP_OK)
        {
            block.length = bytes_read / sizeof(adc_digi_output_data_t);
            //convert raw values to voltage
            int buf_sum = 0;
            for (int i = 0; i < bytes_read / sizeof(adc_digi_output_data_t); i++)
            {
                adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i*sizeof(adc_digi_output_data_t)]; 
                uint32_t raw = p->type2.data;
                block.data[i]  = (int16_t)raw; // - 1094; 
                buf_sum += block.data[i];
            }

            running_buf_avg = buf_sum / BUF_SIZE;
            // send buffer to process
            if(xQueueSend(process_queue, &block, portMAX_DELAY) != pdPASS)
            {
                ESP_LOGE(TAG, "PROCESS QUEUE FULL -- DSP TOO SLOW");
            }
        }
    }
} 

void task_dsp(void *pvParameters)
{
    int N = BUF_SIZE;
    float sample_spacing = 1.0 / SAMPLE_RATE;
    int count = 0;
    DataBlock adc_block;
    DataBlock dsp_block;

    if (xQueueReceive(process_queue, &adc_block, portMAX_DELAY))
    {
        memcpy((void*) filling_buffer, adc_block.data, BUF_SIZE * sizeof(*filling_buffer));
        buffer_swap_ready = true;
    }

    while (true){
        if (xQueueReceive(process_queue, &adc_block, portMAX_DELAY))
        {
            memcpy(dsp_block.data, adc_block.data, adc_block.length * sizeof(int16_t));
            dsp_block.length = adc_block.length;     

            unsigned int start = dsp_get_cpu_cycle_count(); // start time

            // convert adc ints to floats for filtering
            for (int i = 0; i < BUF_SIZE; i++)
            {
                fir_in[i] = (float) (dsp_block.data[i] - running_buf_avg);
            }

            apply_EQ(BUF_SIZE, NUM_BANDS, fir_out, eq_gains, fir_handles); // FILTERING 

            unsigned int start2 = dsp_get_cpu_cycle_count(); // start time
            //filling filtered signals out to output & spectrum
            for (int i = 0; i < BUF_SIZE; i++)
            {
                filling_buffer[i] = (int16_t) fir_out[i];
                dsp_block.data[i] = (int16_t) fir_out[i];
            }
            buffer_length = adc_block.length; 
            buffer_swap_ready = true;

            fft(dsp_block.length, dsp_block.data, spectrum, sample_spacing);
            spec2bins(N, NUM_BINS, spectrum, spec_binned);

            unsigned int end = dsp_get_cpu_cycle_count(); // end time
                                                          
            //ESP_LOGI(TAG, "Processing Complete (ADC Avg Signal = %d) (Buf Size = %d). EQ Duration: %f ms Total Duration: %f ms", running_buf_avg, BUF_SIZE, 1000 *(end - start2)/ (float)cpu_freq, 1000*(end - start)/ (float)cpu_freq); // UNCOMMENT AT YOUR OWN RISK ESP_LOGI ARE EXPENSIVE
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

            if (xSemaphoreTake(buffer_swap_sem, pdMS_TO_TICKS(1000)) == pdFAIL)
            {
                ESP_LOGW(TAG, "Buffer swap timeout - timer may have stalled");
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
            //ESP_LOGI(TAG, "pushing to oled");
            print_to_OLED(NUM_BINS, spec_binned);
        }
    }
}

void print_to_OLED(int num_bins, float* spectrum_binned)
{
    float max = -10.0;
    float min = -45.0;
    uint8_t spectrum[NUM_BINS];
    
    for (int i = 0; i < NUM_BINS; i++)
    {
        spectrum_binned[i] = spectrum_binned[i] < min ? min : spectrum_binned[i];
        spectrum_binned[i] = spectrum_binned[i] > max ? max : spectrum_binned[i];
        spectrum_binned[i] = 8 * ((spectrum_binned[i] - min) / (max - min));
        spectrum[i] = (uint8_t) spectrum_binned[i];
    }

    freq(spi_oled, spectrum, NUM_BINS);
} 
