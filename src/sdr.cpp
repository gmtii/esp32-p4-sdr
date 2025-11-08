#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "esp_dsp.h"
#include "driver/i2s_std.h"

#include "sdr_math.h"

#include "esp_mac.h"

#include "ui.h"

#include "sdr.h"
#include "sdr_priv.h"

#include "agc.h"
#include "nr.h"
#include "sam.h"

float IRAM_ATTR alpha_beta_mag(float inphase, float quadrature)
// (c) András Retzler
// taken from libcsdr: https://github.com/simonyiszk/csdr
{
    // Min RMS Err      0.947543636291 0.392485425092
    // Min Peak Err     0.960433870103 0.397824734759
    // Min RMS w/ Avg=0 0.948059448969 0.392699081699
    const float alpha = 0.960433870103; // 1.0; //0.947543636291;
    const float beta = 0.397824734759;
    /* magnitude ~= alpha * max(|I|, |Q|) + beta * min(|I|, |Q|) */
    float abs_inphase = fabs(inphase);
    float abs_quadrature = fabs(quadrature);
    if (abs_inphase > abs_quadrature)
    {
        return alpha * abs_inphase + beta * abs_quadrature;
    }
    else
    {
        return alpha * abs_quadrature + beta * abs_inphase;
    }
}

void IRAM_ATTR sdrTask(void *args)
{

    esp_err_t ret = ESP_OK;
    size_t bytes_read = 0;
    size_t bytes_write = 0;

    // Filtro biquad LPF 48000 x 0.15 para modos AM

    float coeffs_am[5];
    float w_lpf_i[5] = {0, 0};
    float w_lpf_q[5] = {0, 0};

    dsps_biquad_gen_lpf_f32(coeffs_am, 0.05, 1); // Q=3

    // Filtros FIR de I para SSB DSP ESP32 S3 (parecen algo más rápidoss que los CMSIS)

    dsps_fir_init_f32(&fir_i, FIR_HILB_RX_I_coeffs, fir_i_State, IQ_NUM_TAPS);

    // Filtros FIR de Q para SSB

    dsps_fir_init_f32(&fir_q, FIR_HILB_RX_Q_coeffs, fir_q_State, IQ_NUM_TAPS);

    /* FIR MR para decimar/interpolar */

    dsps_firmr_init_f32(&firmr_i, FirRxDecimate, firmr_i_State, RX_DECIMATE_NUM_TAPS, 1, 4, 0);
    dsps_firmr_init_f32(&firmr_q, FirRxDecimate, firmr_q_State, RX_DECIMATE_NUM_TAPS, 1, 4, 0);

    dsps_firmr_init_f32(&firmr_p, FirRxInterpolate, firmr_p_State, RX_INTERPOLATE_NUM_TAPS, 4, 1, 0);

    int i = 0;

    while (1)
    {
        /* Lee i2s ADC */
        ret = i2s_channel_read(rx_handle, (char *)&sampleData_in[0].sample, SAMPLE_BUFFER_SIZE * 4, &bytes_read, 1000);

        /* Vectores para FFT */

        for (i = 0; i < SAMPLE_BUFFER_SIZE; i++)
        {
            i_sample[i] = ((float)sampleData_in[i].ch[0] / (float)(32768));
            i_fft[i] = i_sample[i];
            q_sample[i] = ((float)sampleData_in[i].ch[1] / (float)(32768));
            q_fft[i] = q_sample[i];
        }

        if (demod_modo != DEMOD_FM && !bucle)
        {
            // Ya estamos en CODEC_SAMPLERATE
            // Hago una conversion de frecuencia a SR/4
            // p.e. 192khz serán 48khz, por lo tanto 5.450 pasa a ser 5.402. Sintonizamos por abajo
            // pero presentamos la frecuencia con esa suma de SR/4.
            for (i = 0; i < SAMPLE_BUFFER_SIZE; i += 4)
            { // i_sample_d contains I = real values
                // i_sample_d contains Q = imaginary values
                // xnew(0) =  xreal(0) + jximag(0)
                // leave as it is!
                // xnew(1) =  - ximag(1) + jxreal(1)
                float hh1 = -q_sample[i + 1];
                float hh2 = i_sample[i + 1];
                i_sample[i + 1] = hh1;
                q_sample[i + 1] = hh2;
                // xnew(2) = -xreal(2) - jximag(2)
                hh1 = -i_sample[i + 2];
                hh2 = -q_sample[i + 2];
                i_sample[i + 2] = hh1;
                q_sample[i + 2] = hh2;
                // xnew(3) = + ximag(3) - jxreal(3)
                hh1 = q_sample[i + 3];
                hh2 = -i_sample[i + 3];
                i_sample[i + 3] = hh1;
                q_sample[i + 3] = hh2;
            }

            dsps_firmr_f32(&firmr_i, i_sample, i_sample_d, SAMPLE_BUFFER_SIZE);
            dsps_firmr_f32(&firmr_q, q_sample, q_sample_d, SAMPLE_BUFFER_SIZE);

            if (demod_modo == DEMOD_USB || demod_modo == DEMOD_LSB) // En AM/SAM/FM no aplicamos desfase a Q
            {
                dsps_fir_f32(&fir_i, i_sample_d, i_sample_out, SAMPLE_BUFFER_SIZE / DR);
                dsps_fir_f32(&fir_q, q_sample_d, q_sample_out, SAMPLE_BUFFER_SIZE / DR);
            }
            else
            {
                // Filtros AM
                dsps_biquad_f32_arp4(i_sample_d, i_sample_out, SAMPLE_BUFFER_SIZE / DR, coeffs_am, w_lpf_i);
                dsps_biquad_f32_arp4(q_sample_d, q_sample_out, SAMPLE_BUFFER_SIZE / DR, coeffs_am, w_lpf_q);
            }
        }

        switch (demod_modo)
        {

        case DEMOD_FM:

            float angle, x, y;
            float a, b;

            for (i = 0; i < SAMPLE_BUFFER_SIZE; i++)
            {
                y = (q_sample[i] * fm_variables.i_sample_prev) - (i_sample[i] * fm_variables.q_sample_prev);
                x = (i_sample[i] * fm_variables.i_sample_prev) + (q_sample[i] * fm_variables.q_sample_prev);

                angle = ApproxAtan2(y, x);

                if (isnanf(angle))
                {
                    angle = 0.0f;
                }

                demod_out[i] = (float)(angle / PI) * 0.1f;

                fm_variables.q_sample_prev = q_sample[i]; // save "previous" value of each channel to allow detection of the change of angle in next go-around
                fm_variables.i_sample_prev = i_sample[i];
            }
            break;

        case DEMOD_USB:
            dsps_add_f32(i_sample_out, q_sample_out, demod_out_d, SAMPLE_BUFFER_SIZE / DR, 1, 1, 1); // Demodula USB
            break;

        case DEMOD_LSB:
            dsps_sub_f32(i_sample_out, q_sample_out, demod_out_d, SAMPLE_BUFFER_SIZE / DR, 1, 1, 1); // Demodula LSB
            break;

        case DEMOD_SAM:
        case DEMOD_SAML:
        case DEMOD_SAMU:
            SAM(i_sample_out, q_sample_out, demod_out_d, SAMPLE_BUFFER_SIZE / DR, demod_modo);
            break;

        case DEMOD_AM: // Demodula AM con las IQ resultantes del LPF
            for (i = 0; i < SAMPLE_BUFFER_SIZE / DR; i++)
            {
                audiotmp = alpha_beta_mag(i_sample_out[i], q_sample_out[i]);
                w = audiotmp + wold * 0.9999f; // yes, I want a superb bass response ;-)
                demod_out_d[i] = w - wold;
                wold = w;
            }
            break;
        }

        /* Pongo entrada en salida */

        if (!bucle)
        {

            if (demod_modo != DEMOD_FM)
            {
                NR(2, demod_out_d, SAMPLE_BUFFER_SIZE / DR);
                RxAGC(demod_out_d, SAMPLE_BUFFER_SIZE / DR);
                dsps_firmr_f32(&firmr_p, demod_out_d, demod_out, SAMPLE_BUFFER_SIZE / DR);
            }

            for (i = 0; i < SAMPLE_BUFFER_SIZE; i++) // convierte a int16
            {
                sampleData_out[i].ch[0] = int16_t(demod_out[i] * (float)32768.0f);
                sampleData_out[i].ch[1] = int16_t(demod_out[i] * (float)32768.0f); // segundo canal para el SFM

                if (sampleData_out[i].ch[0] > 32767)
                    sampleData_out[i].ch[0] = 32767;
                if (sampleData_out[i].ch[1] > 32767)
                    sampleData_out[i].ch[1] = 32767;

                if (sampleData_out[i].ch[0] < -32767)
                    sampleData_out[i].ch[0] = -32767;
                if (sampleData_out[i].ch[1] < -32767)
                    sampleData_out[i].ch[1] = -32767;
            }
        }
        else
        {
            for (i = 0; i < SAMPLE_BUFFER_SIZE; i++)
            {
                sampleData_out[i].ch[0] = sampleData_in[i].ch[0];
                sampleData_out[i].ch[1] = sampleData_in[i].ch[1];
            }
        }

        // Envia el DAC SAMPLE_BUFFER_SIZE * 4 ( 2 canales, 16 bit cada uno)
        ret = i2s_channel_write(tx_handle, (char *)&sampleData_out[0].sample, SAMPLE_BUFFER_SIZE * 4, &bytes_write, 1000);
    }

    vTaskDelete(NULL);
}

void shift_right_circular(int16_t *v, size_t size, int offset)
{
    if (size == 0 || offset == 0)
        return;
    offset %= size;
    int16_t tmp[offset];
    memcpy(tmp, &v[size - offset], offset * sizeof(int16_t));
    memmove(&v[offset], v, (size - offset) * sizeof(int16_t));
    memcpy(v, tmp, offset * sizeof(int16_t));
}

void IRAM_ATTR calcula_fft(void)
{
    int N = SAMPLE_BUFFER_SIZE;

    dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);

    // save old pixels for lowpass filter
    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++)
    {
        pixelold[i] = pixelnew[i];
    }

    // Generate hann window
    dsps_wind_hann_f32(wind, N);

    // Convert two input vectors to one complex vector i,q
    for (int i = 0; i < N; i++)
    {
        fft_vector[i * 2 + 0] = i_fft[i] * wind[i];
        fft_vector[i * 2 + 1] = q_fft[i] * wind[i];
    }

    // FFT
    dsps_fft2r_fc32_arp4(fft_vector, N);
    //  Bit reverse
    dsps_bit_rev_fc32(fft_vector, N);

    // calculate mag = I*I + Q*Q,
    // and simultaneously put them into the right order
    for (int i = 0; i < N / 2; i++)
    {
        fft_mag[i + N / 2] = (fft_vector[i * 2] * fft_vector[i * 2] + fft_vector[i * 2 + 1] * fft_vector[i * 2 + 1]);
        fft_mag[i + 0] = (fft_vector[(i + N / 2) * 2] * fft_vector[(i + N / 2) * 2] + fft_vector[(i + N / 2) * 2 + 1] * fft_vector[(i + N / 2) * 2 + 1]);
    }

    for (int i = 0; i < N; i++)
    {
        fft_mag[i] = 0.6 * fft_mag[i] + 0.4 * fft_mag_old[i];
        fft_mag_old[i] = fft_mag[i];
        pixelnew[N - 1 - i] = 20 * log10f_fast(fft_mag[i] * (float)(32768.0f));
    }

    // Rota 128 a la derecha para corregir el problema con el CANVAS dichoso de LGVL

    shift_right_circular(pixelnew, N, 128);

    if (debug)
    {
        Serial.printf("MAGNITUDES ********************************************************\n");
        for (int i = 0; i < N; i++)
        {
            Serial.printf("%i\n", pixelnew[i]);
        }

        debug = false;
    }
}
