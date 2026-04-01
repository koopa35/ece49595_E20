#include "ina260.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "global_defs.h"
#include "global_vars.h"

#define INA260_ADDR  0x40
#define CURRENT_ADDR 0x01
#define VOLTAGE_ADDR 0x02
#define POWER_ADDR 0x03
#define CURRENT_DIV 0.00125f
#define VOLTAGE_DIV 0.00125f
#define POWER_DIV 0.01f

esp_err_t read_ina260_CVP(int timeout)
{

    uint8_t current_reg = CURRENT_ADDR;
    uint8_t voltage_reg = VOLTAGE_ADDR;
    uint8_t power_reg = POWER_ADDR;

    uint8_t current_buf[2];
    uint8_t voltage_buf[2];
    uint8_t power_buf[2];

    esp_err_t current_error = i2c_master_write_read_device(I2C_PORT, INA260_ADDR, &current_reg, 1, current_buf, 2, pdMS_TO_TICKS(timeout));
    esp_err_t voltage_error = i2c_master_write_read_device(I2C_PORT, INA260_ADDR, &voltage_reg, 1, voltage_buf, 2, pdMS_TO_TICKS(timeout));
    esp_err_t power_error = i2c_master_write_read_device(I2C_PORT, INA260_ADDR, &power_reg, 1, power_buf, 2, pdMS_TO_TICKS(timeout));
    
    int16_t current_raw = (int16_t) (current_buf[0] << 8) | current_buf[1]; // current can be positive or negative
    uint16_t voltage_raw = (voltage_buf[0] << 8) | voltage_buf[1];
    uint16_t power_raw = (power_buf[0] << 8) | power_buf[1];;

    current = (float)CURRENT_DIV * current_raw;
    voltage = (float)VOLTAGE_DIV * voltage_raw;
    power = (float)POWER_DIV * power_raw;

    return (current_error | voltage_error | power_error);

}
