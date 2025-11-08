#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#include <math.h>

#include "esp_dsp.h"

void IRAM_ATTR NR(int ANR_on, float *demod_out, int BUFFER_SIZE)
{
    // LMS automatic notch filter to eliminate annoying birdies

    // Automatic noise reduction
    // Variable-leak LMS algorithm
    // taken from (c) Warren Pratts wdsp library 2016
    // GPLv3 licensed

    if (ANR_on > 0)
    {
        // variable leak LMS algorithm for automatic notch or noise reduction
        // (c) Warren Pratt wdsp library 2016

        // LMS automatic notch filter
#define ANR_DLINE_SIZE 512               // 256 //512 //2048 funktioniert nicht, 128 & 256 OK                 // dline_size
        static const int ANR_taps = 64;  // 64;                       // taps
        static const int ANR_delay = 16; // 16;                       // delay
        static const int ANR_dline_size = ANR_DLINE_SIZE;
        static const int ANR_buff_size = BUFFER_SIZE;
        // static int ANR_position = 0;
        static const float ANR_two_mu = 0.001;      // 0.0001                  // two_mu --> "gain"
        static const float ANR_gamma = 0.1;         // gamma --> "leakage"
        static float ANR_lidx = 120.0;              // lidx
        static const float ANR_lidx_min = 0.0;      // lidx_min
        static const float ANR_lidx_max = 200.0;    // lidx_max
        static float ANR_ngamma = 0.001;            // ngamma
        static const float ANR_den_mult = 6.25e-10; // den_mult
        static const float ANR_lincr = 1.0;         // lincr
        static const float ANR_ldecr = 3.0;         // ldecr
        static int ANR_mask = ANR_dline_size - 1;
        static int ANR_in_idx = 0;
        static float ANR_d[ANR_DLINE_SIZE];
        static float ANR_w[ANR_DLINE_SIZE];

        int i, j, idx;
        float c0, c1;
        float y, error, sigma, inv_sigp;
        float nel, nev;

        for (i = 0; i < ANR_buff_size; i++)
        {
            ANR_d[ANR_in_idx] = demod_out[i];

            y = 0;
            sigma = 0;

            for (j = 0; j < ANR_taps; j++)
            {
                idx = (ANR_in_idx + j + ANR_delay) & ANR_mask;
                y += ANR_w[j] * ANR_d[idx];
                sigma += ANR_d[idx] * ANR_d[idx];
            }
            inv_sigp = 1.0 / (sigma + 1e-10);
            error = ANR_d[ANR_in_idx] - y;

            // demod_out[i] = error;
            if (ANR_on == 1)
                demod_out[i] = error; // NOTCH FILTER
            else
                demod_out[i] = y; // NOISE REDUCTION

            if ((nel = error * (1.0 - ANR_two_mu * sigma * inv_sigp)) < 0.0)
                nel = -nel;
            if ((nev = ANR_d[ANR_in_idx] - (1.0 - ANR_two_mu * ANR_ngamma) * y - ANR_two_mu * error * sigma * inv_sigp) < 0.0)
                nev = -nev;
            if (nev < nel)
            {
                if ((ANR_lidx += ANR_lincr) > ANR_lidx_max)
                    ANR_lidx = ANR_lidx_max;
                else if ((ANR_lidx -= ANR_ldecr) < ANR_lidx_min)
                    ANR_lidx = ANR_lidx_min;
            }
            ANR_ngamma = ANR_gamma * (ANR_lidx * ANR_lidx) * (ANR_lidx * ANR_lidx) * ANR_den_mult;

            c0 = 1.0 - ANR_two_mu * ANR_ngamma;
            c1 = ANR_two_mu * error * inv_sigp;

            for (j = 0; j < ANR_taps; j++)
            {
                idx = (ANR_in_idx + j + ANR_delay) & ANR_mask;
                ANR_w[j] = c0 * ANR_w[j] + c1 * ANR_d[idx];
            }
            ANR_in_idx = (ANR_in_idx + ANR_mask) & ANR_mask;
        }
    }
}