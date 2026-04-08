#include "eeprom_24xx.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "global_defs.h"
#include "global_vars.h"
#include "ina260.h"

#define VOLUME_ADDR 0x00
#define RUNTIME_ADDR 0x10
#define EQ_ADDR 0x20

// Previous values for change detection
static uint16_t prev_volume = 67;

float prev_eq_gains[EQ_BANDS] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
static uint32_t prev_runtime_total_sec = 3600;

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
        vTaskDelay(pdMS_TO_TICKS(2));
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

esp_err_t eeprom_24xx_load (const eeprom_24xx_t *dev) {
    eeprom_24xx_read_bytes(dev, VOLUME_ADDR, (uint8_t *)&volume, sizeof(volume));
    eeprom_24xx_read_bytes(dev, RUNTIME_ADDR, (uint8_t *)&runtime_total_sec, sizeof(runtime_total_sec));

    for (int i = 0; i < EQ_BANDS; i++) {
            eeprom_24xx_read_bytes(&eeprom_dev, EQ_ADDR + (i * sizeof(float)), (uint8_t *)&eq_gains[i], sizeof(float));
            prev_eq_gains[i] = eq_gains[i];
    }

    return ESP_OK;
}


void eeprom_24xx_task(void *pvParameters)
{
    while (1) {
        for (int i = 0; i < EQ_BANDS; i++) {
            if (prev_eq_gains[i] != eq_gains[i]) {
                eeprom_24xx_write_bytes(&eeprom_dev, EQ_ADDR + (i * sizeof(float)), (const uint8_t *)&eq_gains[i], sizeof(float));
                prev_eq_gains[i] = eq_gains[i];
            }
        }

        if (prev_volume != volume) {
            eeprom_24xx_write_bytes(&eeprom_dev, VOLUME_ADDR, (const uint8_t *)&volume, sizeof(volume));
            prev_volume = volume;
        }

        if (prev_runtime_total_sec != runtime_total_sec) {
            eeprom_24xx_write_bytes(&eeprom_dev, RUNTIME_ADDR, (const uint8_t *)&runtime_total_sec, sizeof(runtime_total_sec));
            prev_runtime_total_sec = runtime_total_sec;
        }

        //ESP_ERROR_CHECK(read_ina260_CVP(1000));
        read_ina260_CVP(1000);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
