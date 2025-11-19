#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/sdm.h"
#include "driver/gptimer.h"
#include "main.h"

#define MHZ                             (1000000)
#define CONST_PI                        (3.1416f)// Constant of PI, used for calculating the sine wave
#define SAMPLE_RATE_HZ                  (44100)
#define EXAMPLE_SIGMA_DELTA_GPIO_NUM    (2)                 // Select GPIO_NUM_2 as the sigma-delta output pin
#define EXAMPLE_OVER_SAMPLE_RATE        (10 * MHZ)          // 10 MHz over sample rate
#define EXAMPLE_TIMER_RESOLUTION        (10  * MHZ)          // 1 MHz timer counting resolution

#define EXAMPLE_ALARM_COUNT             (227)

static const char *TAG_SDM = "sdm_dac";

gptimer_handle_t example_init_gptimer(void* args)
{
    /* Allocate GPTimer handle */
    gptimer_handle_t timer_handle;
    gptimer_config_t timer_cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = EXAMPLE_TIMER_RESOLUTION,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_cfg, &timer_handle));

    /* Set the timer alarm configuration */
    gptimer_alarm_config_t alarm_cfg = {
        .alarm_count = EXAMPLE_ALARM_COUNT,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(timer_handle, &alarm_cfg));

    /* Register the alarm callback */
    gptimer_event_callbacks_t cbs = {
        .on_alarm = example_timer_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer_handle, &cbs, args));

    /* Clear the timer raw count value, make sure it'll count from 0 */
    ESP_ERROR_CHECK(gptimer_set_raw_count(timer_handle, 0));
    /* Enable the timer */
    ESP_ERROR_CHECK(gptimer_enable(timer_handle));

    ESP_LOGI(TAG_SDM, "Timer enabled");

    return timer_handle;
}

sdm_channel_handle_t example_init_sdm(void)
{
    /* Allocate sdm channel handle */
    sdm_channel_handle_t sdm_chan = NULL;
    sdm_config_t config = {
        .clk_src = SDM_CLK_SRC_DEFAULT,
        .gpio_num = EXAMPLE_SIGMA_DELTA_GPIO_NUM,
        .sample_rate_hz = EXAMPLE_OVER_SAMPLE_RATE,
    };
    ESP_ERROR_CHECK(sdm_new_channel(&config, &sdm_chan));
    /* Enable the sdm channel */
    ESP_ERROR_CHECK(sdm_channel_enable(sdm_chan));

    ESP_LOGI(TAG_SDM, "Sigma-delta output is attached to GPIO %d", EXAMPLE_SIGMA_DELTA_GPIO_NUM);

    return sdm_chan;
}
