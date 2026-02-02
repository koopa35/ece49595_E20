#ifndef ADC_H
#define ADC_H
#include "esp_adc/adc_continuous.h"
#include "main.h"

void init_cont_adc(adc_channel_t, uint8_t, adc_continuous_handle_t*);

#endif
