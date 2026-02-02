#include "filters.h"

void generate_FIR_coefficients(float *fir_coeffs, const unsigned int fir_len, const float ft)
{
    // Even or odd length of the FIR filter
    const bool is_odd = (fir_len % 2) ? (true) : (false);
    const float fir_order = (float)(fir_len - 1);

    // Window coefficients
    float *fir_window = (float *)malloc(fir_len * sizeof(float));
    dsps_wind_blackman_f32(fir_window, fir_len);

    for (int i = 0; i < fir_len; i++) {
        if ((i == fir_order / 2) && (is_odd)) {
            fir_coeffs[i] = 2 * ft;
        } else {
            fir_coeffs[i] = sinf((2 * M_PI * ft * (i - fir_order / 2))) / (M_PI * (i - fir_order / 2));
        }

        if ((!is_odd) && ((i == fir_len /2)))
        {
            //fir_coeffs[i] += 0.25;
        }

        fir_coeffs[i] *= fir_window[i];
    }

    free(fir_window);
}


