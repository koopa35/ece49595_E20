#ifndef FILTERS_H
#define FILTERS_H

#include "esp_dsp.h"
#include <math.h>
#include "main.h"

void generate_FIR_coefficients(float*, const unsigned int , const float);
#endif // FILTERS_H
