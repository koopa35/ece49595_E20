#include "eeprom_24xx.h"

#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "global_vars.h"

// Global state for async write
static struct {
    bool in_progress;
    const eeprom_24xx_t *dev;
    esp_err_t last_result;
} async_write_state = {.in_progress = false};

// Dirty flag for persistence task
static volatile bool variables_dirty = false;
static SemaphoreHandle_t dirty_flag_mutex = NULL;

static inline uint16_t eeprom_max_addr(const eeprom_24xx_t *dev)
{
    return (uint16_t)(dev->size_bytes - 1);
}

esp_err_t eeprom_24xx_wait_ready(const eeprom_24xx_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;

    uint8_t dummy = 0x00;

    for (int i = 0; i < 60; i++) {
        esp_err_t err = i2c_master_write_to_device(
            dev->i2c_port,
            dev->dev_addr_7bit,
            &dummy,
            1,
            pdMS_TO_TICKS(10)
        );
        if (err == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t eeprom_24xx_write_bytes(
    const eeprom_24xx_t *dev,
    uint16_t mem_addr,
    const uint8_t *data,
    size_t len
){
    if (!dev || !data || len == 0) return ESP_ERR_INVALID_ARG;
    if ((uint32_t)mem_addr + len - 1 > eeprom_max_addr(dev)) return ESP_ERR_INVALID_ARG;

    // Enforce not crossing a page boundary in a single write 
    uint16_t page_off = mem_addr % dev->page_size;
    if (page_off + len > dev->page_size) return ESP_ERR_INVALID_ARG;

    // Buffer: 2-byte address + data
    // Keep a safe upper bound; up to 256-byte page parts
    uint8_t buf[2 + 256];
    if (dev->page_size > 256) return ESP_ERR_INVALID_STATE;
    if (len > dev->page_size) return ESP_ERR_INVALID_ARG;

    buf[0] = (uint8_t)(mem_addr >> 8);
    buf[1] = (uint8_t)(mem_addr & 0xFF);
    memcpy(&buf[2], data, len);

    esp_err_t err = i2c_master_write_to_device(
        dev->i2c_port,
        dev->dev_addr_7bit,
        buf,
        (size_t)(2 + len),
        pdMS_TO_TICKS(dev->timeout_ms)
    );
    if (err != ESP_OK) return err;

    return eeprom_24xx_wait_ready(dev);
}

esp_err_t eeprom_24xx_read_bytes(
    const eeprom_24xx_t *dev,
    uint16_t mem_addr,
    uint8_t *out,
    size_t len
){
    if (!dev || !out || len == 0) return ESP_ERR_INVALID_ARG;
    if ((uint32_t)mem_addr + len - 1 > eeprom_max_addr(dev)) return ESP_ERR_INVALID_ARG;

    uint8_t addr[2] = {
        (uint8_t)(mem_addr >> 8),
        (uint8_t)(mem_addr & 0xFF)
    };

    return i2c_master_write_read_device(
        dev->i2c_port,
        dev->dev_addr_7bit,
        addr,
        sizeof(addr),
        out,
        len,
        pdMS_TO_TICKS(dev->timeout_ms)
    );
}

// Non-blocking async write - initiates write and returns immediately
const eeprom_24xx_t *eeprom_24xx_write_bytes_async_start(
    const eeprom_24xx_t *dev,
    uint16_t mem_addr,
    const uint8_t *data,
    size_t len
){
    if (!dev || !data || len == 0) return NULL;
    if ((uint32_t)mem_addr + len - 1 > eeprom_max_addr(dev)) return NULL;

    uint16_t page_off = mem_addr % dev->page_size;
    if (page_off + len > dev->page_size) return NULL;

    uint8_t buf[2 + 256];
    if (dev->page_size > 256) return NULL;
    if (len > dev->page_size) return NULL;

    buf[0] = (uint8_t)(mem_addr >> 8);
    buf[1] = (uint8_t)(mem_addr & 0xFF);
    memcpy(&buf[2], data, len);

    // Send write without waiting for completion
    esp_err_t err = i2c_master_write_to_device(
        dev->i2c_port,
        dev->dev_addr_7bit,
        buf,
        (size_t)(2 + len),
        pdMS_TO_TICKS(10)  // Short timeout for just sending data
    );
    
    if (err == ESP_OK) {
        async_write_state.in_progress = true;
        async_write_state.dev = dev;
        async_write_state.last_result = ESP_OK;
        return dev;
    }
    
    return NULL;
}

// Non-blocking check if async write is complete
bool eeprom_24xx_write_async_is_done(
    const eeprom_24xx_t *dev
){
    if (!async_write_state.in_progress || async_write_state.dev != dev) {
        return true;  // Not in progress, so "done"
    }

    // Attempt ready check without blocking
    uint8_t dummy = 0x00;
    esp_err_t err = i2c_master_write_to_device(
        dev->i2c_port,
        dev->dev_addr_7bit,
        &dummy,
        1,
        pdMS_TO_TICKS(1)
    );

    if (err == ESP_OK) {
        async_write_state.in_progress = false;
        return true;  // EEPROM is ready, write is complete
    }

    return false;  // Still writing
}

// Mark global variables as dirty - signals that they need to be saved
void eeprom_mark_dirty(void)
{
    if (dirty_flag_mutex != NULL) {
        xSemaphoreTake(dirty_flag_mutex, portMAX_DELAY);
        variables_dirty = true;
        xSemaphoreGive(dirty_flag_mutex);
    }
}

// Persistence task - monitors dirty flag and saves variables when needed
static void eeprom_persistence_task(void *arg)
{
    const eeprom_24xx_t *dev = (const eeprom_24xx_t *)arg;
    if (!dev) vTaskDelete(NULL);

    // Initialize dirty flag mutex if not already done
    if (dirty_flag_mutex == NULL) {
        dirty_flag_mutex = xSemaphoreCreateMutex();
    }

    while (1) {
        // Check if variables are marked dirty
        bool should_save = false;
        if (dirty_flag_mutex != NULL && xSemaphoreTake(dirty_flag_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (variables_dirty && eeprom_24xx_write_async_is_done(dev)) {
                should_save = true;
                variables_dirty = false;  // Clear flag
            }
            xSemaphoreGive(dirty_flag_mutex);
        }

        // Perform non-blocking save if needed
        if (should_save) {
            eeprom_24xx_write_bytes_async_start(dev, 0x0000, (const uint8_t*)eq_gains, sizeof(eq_gains));
        }

        // Yield to other tasks frequently to keep it non-blocking
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Start the persistence task
esp_err_t eeprom_24xx_start_persistence_task(
    const eeprom_24xx_t *dev,
    uint8_t priority
){
    if (!dev) return ESP_ERR_INVALID_ARG;

    // Initialize mutex if needed
    if (dirty_flag_mutex == NULL) {
        dirty_flag_mutex = xSemaphoreCreateMutex();
        if (dirty_flag_mutex == NULL) return ESP_ERR_NO_MEM;
    }

    // Create the persistence task
    BaseType_t ret = xTaskCreate(
        eeprom_persistence_task,
        "eeprom_persist",
        2048,
        (void *)dev,
        priority,
        NULL
    );

    if (ret == pdPASS) {
        return ESP_OK;
    }
    return ESP_FAIL;
}
