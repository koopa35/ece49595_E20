#include <stdio.h>
#include <math.h>
#include "ntc.h"
#include "global_vars.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ADC handle
static adc_oneshot_unit_handle_t ntc_adc_handle;

// Function to convert ADC reading to temperature
static float adc_to_temp(int adc) {
    if (adc <= 0 || adc >= 4095) return -273.15f;  // Invalid reading
    float r_ntc = NTC_R_REF * ((float)adc / (4095.0f - (float)adc));
    float ln_ratio = logf(r_ntc / NTC_R25);
    float temp_k = 1.0f / (1.0f / NTC_T0 + ln_ratio / NTC_BETA);
    return temp_k - 273.15f;  // Convert to Celsius
}

esp_err_t ntc_init(void) {
    // Setup ADC handle and channels
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_2
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &ntc_adc_handle);
    if (ret != ESP_OK) return ret;

    // Configure channels
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten    = ADC_ATTEN_DB_12
    };
    ret = adc_oneshot_config_channel(ntc_adc_handle, ADC2_CHANNEL_0, &chan_cfg);
    if (ret != ESP_OK) return ret;
    ret = adc_oneshot_config_channel(ntc_adc_handle, ADC2_CHANNEL_1, &chan_cfg);
    return ret;
}

void ntc_read_temperatures(void) {
    // Read and average 100 samples
    int ntc1_adc = 0;
    int ntc2_adc = 0;
    int raw;

    for (int i = 0; i < 100; i++) {
        adc_oneshot_read(ntc_adc_handle, ADC2_CHANNEL_0, &raw);
        ntc1_adc += raw;
        adc_oneshot_read(ntc_adc_handle, ADC2_CHANNEL_1, &raw);
        ntc2_adc += raw;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ntc1_adc /= 100;
    ntc2_adc /= 100;

    // Convert to temperatures
    temperature1 = adc_to_temp(ntc1_adc);
    temperature2 = adc_to_temp(ntc2_adc);
}
