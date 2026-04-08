#pragma once

#include "driver/adc.h"
#include "esp_err.h"

// NTC thermistor parameters
#define NTC_R_REF 10000.0f  // 10k reference resistor
#define NTC_R25   10000.0f  // NTC resistance at 25°C
#define NTC_BETA  3950.0f   // Beta value
#define NTC_T0    298.15f   // 25°C in Kelvin

// Function prototypes
esp_err_t ntc_init(void);
void ntc_read_temperatures(void);
