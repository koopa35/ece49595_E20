#include "driver/i2s_pdm.h"
#include "driver/gpio.h"
#include "main.h"

#define PDM_TX_CLK_IO GPIO_NUM_2
#define PDM_TX_DOUT_IO GPIO_NUM_17

i2s_chan_handle_t init_i2s_pdm(void);
