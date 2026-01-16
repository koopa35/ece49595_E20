#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "global_defs.h"

// Global Variables
bool power_status = ON;

float voltage = 12.5;
float current = 2.3;
float power = 28.75;
float temperature = 45.2;

uint16_t volume = 67;

uint16_t equalizer_band[EQ_BANDS] = {
    50, 50, 50, 50, 50, 50, 50, 50
};

bool bluetooth_status = ON;
bool aux_status = OFF;

char current_track[STR_LEN] = "Song Title";
char current_artist[STR_LEN] = "Artist Name";
char current_album[STR_LEN] = "Album Name";
uint16_t track_runtime_sec = 125;

uint32_t runtime_total_sec = 3600;
uint32_t next_change_sec = 3600000;
