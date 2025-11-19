#include "main.h"
#include "esp_adc/adc_continuous.h"
char* TAG_ADC = "ADC";

void init_cont_adc(adc_channel_t channel, uint8_t channel_num, adc_continuous_handle_t* out_handle)
{
    adc_continuous_handle_t in_handle = NULL;
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 4*BUF_SIZE,
        .conv_frame_size = BUF_SIZE * sizeof(adc_digi_output_data_t),
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &in_handle));

    static adc_digi_pattern_config_t pattern[] = {
        {.atten = ADC_ATTEN_DB_12, .channel = ADC_CHANNEL_0, .unit = ADC_UNIT_1, .bit_width = ADC_BITWIDTH_12}
    }; 

    adc_continuous_config_t dig_cfg = {
        .pattern_num = channel_num,
        .adc_pattern = pattern,
        .sample_freq_hz = SAMPLE_RATE,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };

    ESP_ERROR_CHECK(adc_continuous_config(in_handle, &dig_cfg));
    *out_handle = in_handle;
    ESP_LOGI(TAG_ADC, "Succesfully Inititialized ADC in Continuous Mode");
}
