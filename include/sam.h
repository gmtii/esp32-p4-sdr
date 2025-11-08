#include "Arduino.h"
#include <stdint.h>
#include <stdbool.h>

#include "sdr.h"

#include "sdr_math.h"

// SAM
// new synchronous AM PLL & PHASE detector
// wdsp Warren Pratt, 2016

#define SAM_PLL_HILBERT_STAGES 7
#define OUT_IDX (3 * SAM_PLL_HILBERT_STAGES)

typedef struct
{
    float pll_fmax = +4000.0;
    int zeta_help = 65;
    float zeta = (float)zeta_help / 75.0; // PLL step response: smaller, slower response 1.0 - 0.1
    float omegaN = 200.0;                 // PLL bandwidth 50.0 - 1000.0

    // pll
    float omega_min = TPI * -pll_fmax * DR / SAMPLE_RATE;
    float omega_max = TPI * pll_fmax * DR / SAMPLE_RATE;
    float g1 = 1.0 - exp(-2.0 * omegaN * zeta * DR / SAMPLE_RATE);
    float g2 = -g1 + 2.0 * (1 - exp(-omegaN * zeta * DR / SAMPLE_RATE) * cosf(omegaN * DR / SAMPLE_RATE * sqrtf(1.0 - zeta * zeta)));
    float phzerror = 0.0;
    float det = 0.0;
    float fil_out = 0.0;
    float del_out = 0.0;
    float omega2 = 0.0;

    // fade leveler
    float tauR = 0.02; // original 0.02;
    float tauI = 1.4;  // original 1.4;
    float dc = 0.0;
    float dc_insert = 0.0;
    float dcu = 0.0;
    float dc_insertu = 0.0;
    float mtauR = exp(-DR / (SAMPLE_RATE * tauR));
    float onem_mtauR = 1.0 - mtauR;
    float mtauI = exp(-DR / (SAMPLE_RATE * tauI));
    float onem_mtauI = 1.0 - mtauI;
    uint8_t fade_leveler = 1;
    uint8_t WDSP_SAM = 1;

    float c0[SAM_PLL_HILBERT_STAGES];
    float c1[SAM_PLL_HILBERT_STAGES];
    float ai, bi, aq, bq;
    float ai_ps, bi_ps, aq_ps, bq_ps;
    float a[3 * SAM_PLL_HILBERT_STAGES + 3]; // Filter a variables
    float b[3 * SAM_PLL_HILBERT_STAGES + 3]; // Filter b variables
    float c[3 * SAM_PLL_HILBERT_STAGES + 3]; // Filter c variables
    float d[3 * SAM_PLL_HILBERT_STAGES + 3]; // Filter d variables
    float dsI;                               // delayed sample, I path
    float dsQ;                               // delayed sample, Q path
    float corr[2];
    float audio;
    float audiou;
    float SAM_carrier = 0.0;
    float SAM_lowpass = 0.0;
    float SAM_carrier_freq_offset = 0.0;
} sam_variables_t;

void IRAM_ATTR SAM(float *i_sample_out, float *q_sample_out, float *demod_out, int BUFFER_SIZE, int demod_modo);
