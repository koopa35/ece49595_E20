#ifndef SDM_H
#define SDM_H

#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/sdm.h"
#include "driver/gptimer.h"

gptimer_handle_t example_init_gptimer(void* args);
sdm_channel_handle_t example_init_sdm(void);

#endif
