#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "driver/i2s_std.h"

#include "esp_dsp.h"

#include "filtros.h"

#include "sdr.h"


/* --------------------------------------------------------------------------------- */

extern boolean debug;
extern int demod_modo;
extern boolean bucle;


/* --------------------------------------------------------------------------------- */

extern i2s_chan_handle_t tx_handle;
extern i2s_chan_handle_t rx_handle;

/* --------------------------------------------------------------------------------- */

fir_f32_t fir_i;
float fir_i_State[IQ_NUM_TAPS];

fir_f32_t fir_q;
float fir_q_State[IQ_NUM_TAPS];

/* --------------------------------------------------------------------------------- */

fir_f32_t firmr_i;
float firmr_i_State[RX_DECIMATE_NUM_TAPS];

fir_f32_t firmr_q;
float firmr_q_State[RX_DECIMATE_NUM_TAPS];

fir_f32_t firmr_p;
float firmr_p_State[RX_INTERPOLATE_NUM_TAPS];

/* --------------------------------------------------------------------------------- */

float audiotmp = 0.0f, w = 0.0f, wold = 0.0f;

/* --------------------------------------------------------------------------------- */

typedef struct
{
    float lpf_prev, hpf_prev_a, hpf_prev_b;
    float i_sample_prev, q_sample_prev;
    float angle;
    float prev_pilot_sample;
    float demod_out_pilot[SAMPLE_BUFFER_SIZE];
    float demod_out_audio[SAMPLE_BUFFER_SIZE];

} fm_variables_t;

fm_variables_t fm_variables;

/* --------------------------------------------------------------------------------- */

union
{
    uint32_t sample;
    int16_t ch[2];
} sampleData_in[SAMPLE_BUFFER_SIZE];

union
{
    uint32_t sample;
    int16_t ch[2];
} sampleData_out[SAMPLE_BUFFER_SIZE];


/* --------------------------------------------------------------------------------- */

float i_sample_out[SAMPLE_BUFFER_SIZE], q_sample_out[SAMPLE_BUFFER_SIZE];
float i_fft[SAMPLE_BUFFER_SIZE], q_fft[SAMPLE_BUFFER_SIZE];
float i_sample[SAMPLE_BUFFER_SIZE], q_sample[SAMPLE_BUFFER_SIZE];
float i_sample_d[SAMPLE_BUFFER_SIZE/DR], q_sample_d[SAMPLE_BUFFER_SIZE/DR];

float demod_out[SAMPLE_BUFFER_SIZE];
float demod_out_d[SAMPLE_BUFFER_SIZE/DR];

int16_t pixelnew[SAMPLE_BUFFER_SIZE];
int16_t pixelold[SAMPLE_BUFFER_SIZE];

float wind[SAMPLE_BUFFER_SIZE];
float fft_vector[SAMPLE_BUFFER_SIZE * 2];
float fft_mag[SAMPLE_BUFFER_SIZE];
float fft_mag_old[SAMPLE_BUFFER_SIZE];

