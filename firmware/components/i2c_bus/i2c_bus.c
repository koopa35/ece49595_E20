#include "i2c_bus.h"

esp_err_t i2c_bus_init(const i2c_bus_cfg_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = cfg->sda_io_num,
        .sda_pullup_en = cfg->sda_pullup_en ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .scl_io_num = cfg->scl_io_num,
        .scl_pullup_en = cfg->scl_pullup_en ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .master.clk_speed = cfg->clk_speed_hz,
        .clk_flags = 0,
    };

    esp_err_t err = i2c_param_config(cfg->port, &conf);
    if (err != ESP_OK) return err;

    return i2c_driver_install(cfg->port, conf.mode, 0, 0, 0);
}
