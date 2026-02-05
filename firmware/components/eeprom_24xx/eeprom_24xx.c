#include "eeprom_24xx.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
