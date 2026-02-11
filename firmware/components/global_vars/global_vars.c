#include "global_vars.h"

// Global Variable Definitions
float voltage = 12.5;
float current = 2.3;
float power = 28.75;
float temperature = 45.2;

uint16_t volume = 67;

// uint16_t equalizer_band[EQ_BANDS] = {
//     50, 50, 50, 50, 50, 50, 50, 50
// };

float eq_gains[EQ_BANDS] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
float band_edges[EQ_BANDS + 1] = {0.0, 350.0, 1100.0, 2200.0, 4000.0, 6000.0, 8000.0, 10500.0, SAMPLE_RATE/2};
int freq_bins[] = {0, 100, 350, 700, 1100, 1600, 2200, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10500, 12000, SAMPLE_RATE/2};

float q_factors[EQ_BANDS] = {0.7, 0.85, 1.41, 1.6, 2.4, 3.4, 3.6, 0.7};
float iir_coeffs[EQ_BANDS][5];
float iir_delay[EQ_BANDS][2];

__attribute__((aligned(16))) float iir_out[BUF_SIZE];
__attribute__((aligned(16))) float iir_in[BUF_SIZE];

DataBlock buffer_pool[3];
int32_t running_buf_avg = 0;

QueueHandle_t process_queue = NULL;
QueueHandle_t output_queue = NULL;
QueueHandle_t free_queue = NULL;

__attribute__((aligned(16))) float window[BUF_SIZE];
__attribute__((aligned(16))) float spectrum[2 * BUF_SIZE];
__attribute__((aligned(16))) float spec_binned[NUM_BINS];

i2s_chan_handle_t  tx_chan;        // I2S tx channel handler
i2s_chan_handle_t  rx_chan_adc;        // I2S rx channel handler for adc
i2s_chan_handle_t  rx_chan_bt;        // I2S rx channel handler for bluetooth

float latest_spectrum[NUM_BINS];
SemaphoreHandle_t spectrum_mutex = NULL;

TaskHandle_t sampling_task_handle = NULL;
TaskHandle_t processing_task_handle = NULL;
TaskHandle_t output_task_handle = NULL;
TaskHandle_t oled_task_handle = NULL;

bool input_select = BLUETOOTH;
bool display_power = ON;

char current_track[STR_LEN] = "Song Title";
char current_artist[STR_LEN] = "Artist Name";
char current_album[STR_LEN] = "Album Name";
uint16_t track_runtime_sec = 125;

uint32_t runtime_total_sec = 3600;
uint32_t next_change_sec = ELECTRODE_REPLACE_TIME;

uint8_t spectrum_norm[16] = {0,1,2,3,4,5,6,7,8,7,6,5,4,3,2,1};
