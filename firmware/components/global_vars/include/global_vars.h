#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "global_defs.h"

// Global Variables
extern float voltage;
extern float current;
extern float power;
extern float temperature;

extern uint16_t volume;

extern uint16_t equalizer_band[EQ_BANDS];

extern bool input_select;
extern bool display_power;

extern char current_track[STR_LEN];
extern char current_artist[STR_LEN];
extern char current_album[STR_LEN];
extern uint16_t track_runtime_sec;

extern uint32_t runtime_total_sec;
extern uint32_t next_change_sec;

extern uint8_t spectrum[16];