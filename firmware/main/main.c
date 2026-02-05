
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "i2c_bus.h"
#include "eeprom_24xx.h"
#include "runtime_log.h"

// ================== CONFIG ==================
#define I2C_PORT      I2C_NUM_0
#define I2C_SDA_GPIO  21
#define I2C_SCL_GPIO  22
#define I2C_FREQ_HZ   100000   // sped back up to 100k I2c standard 

#define EEPROM_ADDR_7BIT      0x57
#define EEPROM_SIZE_BYTES     4096
#define EEPROM_PAGE_SIZE      32

#define SAVE_INTERVAL_MINUTES 1

#define RUNTIME_TASK_STACK_WORDS 1024 //4KB at 1024, but can be 4096 max
#define RUNTIME_TASK_PRIORITY 5

static const char *TAG = "RUNTIME_LOG";

// ================== HANDLES ==================
static eeprom_24xx_t eeprom0;
static runtime_log_t runtime0;

// ================== INIT FUNCTIONS ==================
static void i2c_init(void)
{
    i2c_bus_cfg_t cfg = {
        .port = I2C_PORT,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .sda_pullup_en = true,
        .scl_pullup_en = true,
        .clk_speed_hz = I2C_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_bus_init(&cfg));
}

static void eeprom_init(void)
{
    eeprom0 = (eeprom_24xx_t){
        .i2c_port = I2C_PORT,
        .dev_addr_7bit = EEPROM_ADDR_7BIT,
        .size_bytes = EEPROM_SIZE_BYTES,
        .page_size = EEPROM_PAGE_SIZE,
        .timeout_ms = 1000,
    };
}

static void runtime_init(void)
{
    runtime_log_cfg_t cfg = {
        .save_interval_minutes = SAVE_INTERVAL_MINUTES,
    };

    ESP_ERROR_CHECK(runtime_log_init(&runtime0, &eeprom0, &cfg));
    ESP_ERROR_CHECK(runtime_log_load_latest(&runtime0));
    ESP_ERROR_CHECK(runtime_log_set_boot_time_now(&runtime0));

    ESP_LOGI(TAG, "Runtime logger ready (slots=%u, save interval=%u min)",
             (unsigned)runtime0.slot_count, (unsigned)SAVE_INTERVAL_MINUTES);
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));

    i2c_init();
    eeprom_init();
    runtime_init();

    ESP_ERROR_CHECK(
    runtime_log_start_task(&runtime0, RUNTIME_TASK_STACK_WORDS, RUNTIME_TASK_PRIORITY)
);
}
