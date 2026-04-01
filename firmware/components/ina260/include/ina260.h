#pragma once

#include <stdint.h>
#include <stddef.h>

#include "driver/i2c.h"
#include "esp_err.h"

esp_err_t read_ina260_CVP(int timeout);
