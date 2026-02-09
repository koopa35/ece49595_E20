#pragma once
#include <stddef.h>

// Rotary Encoder Pins
#define ENC1_A 15
#define ENC1_B 16
#define ENC1_BTN 17

#define ENC2_A 18
#define ENC2_B 8
#define ENC2_BTN 3

// Switch pins
#define SOURCE_SEL 13
#define DISP_POWER 14

// SPI Pins
#define SCLK 4
#define MOSI 5
#define CS1 6
#define CS2 7

// I2C PINS
#define I2C_PORT     I2C_NUM_0
#define I2C_SDA_GPIO 2
#define I2C_SCL_GPIO 1
#define I2C_FREQ_HZ  10000

// EEPROM Definitions
#define EEPROM_ADDR  0x57
#define EEPROM_SIZE_BYTES 4096
#define EEPROM_MAX_ADDR   (EEPROM_SIZE_BYTES - 1)
#define EEPROM_PAGE_SIZE  32

// States
#define BLUETOOTH 1
#define AUX 0

#define ON 1
#define OFF 0

#define EQ_BANDS 8

#define STR_LEN 20

// DSP definitions
#define BUF_SIZE    256
#define NUM_BINS    16 
#define SAMPLE_RATE 44100

#define I2S_DUPLEX_MCLK 21
#define I2S_DUPLEX_BCLK 38
#define I2S_DUPLEX_WS   39
#define I2S_DUPLEX_DOUT 40
#define I2S_DUPLEX_DIN  37

#define I2S_SIMPLEX_BCLK 41
#define I2S_SIMPLEX_WS   42
#define I2S_SIMPLEX_DIN  45

typedef struct {
    int16_t data[BUF_SIZE];
    size_t length;
} DataBlock;