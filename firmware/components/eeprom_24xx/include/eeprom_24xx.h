#pragma once

#include <stdint.h>
#include <stddef.h>

#include "driver/i2c.h"
#include "esp_err.h"

typedef struct {
    i2c_port_t i2c_port;
    uint8_t    dev_addr_7bit;   //  0x57
    uint16_t   size_bytes;      //  4096
    uint16_t   page_size;       //  32
    uint32_t   timeout_ms;      // 1000
} eeprom_24xx_t;

esp_err_t eeprom_24xx_wait_ready(const eeprom_24xx_t *dev);

//  a single write call must not cross a page boundary
esp_err_t eeprom_24xx_write_bytes(
    const eeprom_24xx_t *dev,
    uint16_t mem_addr,
    const uint8_t *data,
    size_t len
);

esp_err_t eeprom_24xx_read_bytes(
    const eeprom_24xx_t *dev,
    uint16_t mem_addr,
    uint8_t *out,
    size_t len
);

// Non-blocking async write
const eeprom_24xx_t *eeprom_24xx_write_bytes_async_start(
    const eeprom_24xx_t *dev,
    uint16_t mem_addr,
    const uint8_t *data,
    size_t len
);

bool eeprom_24xx_write_async_is_done(
    const eeprom_24xx_t *dev
);

// Mark global variables as dirty (need to be saved)
void eeprom_mark_dirty(void);

// Start persistence task that monitors and saves changed variables
esp_err_t eeprom_24xx_start_persistence_task(
    const eeprom_24xx_t *dev,
    uint8_t priority
);