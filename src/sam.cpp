#include "sam.h"

sam_variables_t sam_variables;

void IRAM_ATTR SAM(float *i_sample_out, float *q_sample_out, float *demod_out, int BUFFER_SIZE, int demod_modo)
{
    float Sin, Cos;

    /**********************************************************************************
     Demodulation
    **********************************************************************************/

    // our desired output is a combination of the real part (left channel) AND the imaginary part (right channel) of
    // the second half of the FFT_buffer which one and how they are combined is dependent upon the demod_mode . . .
    // taken from Warren Pratt´s WDSP, 2016
    // http://svn.tapr.org/repos_sdr_hpsdr/trunk/W5WC/PowerSDR_HPSDR_mRX_PS/Source/wdsp/

    for (unsigned i = 0; i < BUFFER_SIZE; i++)
    {

        sincosf(sam_variables.phzerror, &Sin, &Cos);

        sam_variables.ai = Cos * i_sample_out[i];
        sam_variables.bi = Sin * i_sample_out[i];
        sam_variables.aq = Cos * q_sample_out[i];
        sam_variables.bq = Sin * q_sample_out[i];

        if (demod_modo > DEMOD_SAM && demod_modo < DEMOD_FM) // Solo se aplica si estamos en SAMLSB o SAMUSB
        {
            sam_variables.a[0] = sam_variables.dsI;
            sam_variables.b[0] = sam_variables.bi;
            sam_variables.c[0] = sam_variables.dsQ;
            sam_variables.d[0] = sam_variables.aq;
            sam_variables.dsI = sam_variables.ai;
            sam_variables.dsQ = sam_variables.bq;

            for (int j = 0; j < SAM_PLL_HILBERT_STAGES; j++)
            {
                int k = 3 * j;
                sam_variables.a[k + 3] = sam_variables.c0[j] * (sam_variables.a[k] - sam_variables.a[k + 5]) + sam_variables.a[k + 2];
                sam_variables.b[k + 3] = sam_variables.c1[j] * (sam_variables.b[k] - sam_variables.b[k + 5]) + sam_variables.b[k + 2];
                sam_variables.c[k + 3] = sam_variables.c0[j] * (sam_variables.c[k] - sam_variables.c[k + 5]) + sam_variables.c[k + 2];
                sam_variables.d[k + 3] = sam_variables.c1[j] * (sam_variables.d[k] - sam_variables.d[k + 5]) + sam_variables.d[k + 2];
            }
            sam_variables.ai_ps = sam_variables.a[OUT_IDX];
            sam_variables.bi_ps = sam_variables.b[OUT_IDX];
            sam_variables.bq_ps = sam_variables.c[OUT_IDX];
            sam_variables.aq_ps = sam_variables.d[OUT_IDX];

            for (int j = OUT_IDX + 2; j > 0; j--)
            {
                sam_variables.a[j] = sam_variables.a[j - 1];
                sam_variables.b[j] = sam_variables.b[j - 1];
                sam_variables.c[j] = sam_variables.c[j - 1];
                sam_variables.d[j] = sam_variables.d[j - 1];
            }
        }

        sam_variables.corr[0] = +sam_variables.ai + sam_variables.bq;
        sam_variables.corr[1] = -sam_variables.bi + sam_variables.aq;

        switch (demod_modo)
        {
        case DEMOD_SAM: // both sidebands
            sam_variables.audio = sam_variables.corr[0];
            break;
        case DEMOD_SAML: // LSB
            sam_variables.audio = (sam_variables.ai_ps - sam_variables.bi_ps) + (sam_variables.aq_ps + sam_variables.bq_ps);
            break;
        case DEMOD_SAMU: // USB
            sam_variables.audio = (sam_variables.ai_ps + sam_variables.bi_ps) - (sam_variables.aq_ps - sam_variables.bq_ps);
            break;
        }

        if (sam_variables.fade_leveler)
        {
            sam_variables.dc = sam_variables.mtauR * sam_variables.dc + sam_variables.onem_mtauR * sam_variables.audio;
            sam_variables.dc_insert = sam_variables.mtauI * sam_variables.dc_insert + sam_variables.onem_mtauI * sam_variables.corr[0];
            sam_variables.audio = sam_variables.audio + sam_variables.dc_insert - sam_variables.dc;
        }
        demod_out[i] = sam_variables.audio;

        sam_variables.det = atan2f(sam_variables.corr[1], sam_variables.corr[0]);

        sam_variables.del_out = sam_variables.fil_out;
        sam_variables.omega2 = sam_variables.omega2 + sam_variables.g2 * sam_variables.det;
        if (sam_variables.omega2 < sam_variables.omega_min)
            sam_variables.omega2 = sam_variables.omega_min;
        else if (sam_variables.omega2 > sam_variables.omega_max)
            sam_variables.omega2 = sam_variables.omega_max;
        sam_variables.fil_out = sam_variables.g1 * sam_variables.det + sam_variables.omega2;
        sam_variables.phzerror = sam_variables.phzerror + sam_variables.del_out;

        // wrap round 2PI, modulus
        while (sam_variables.phzerror >= TPI)
            sam_variables.phzerror -= TPI;
        while (sam_variables.phzerror < 0.0)
            sam_variables.phzerror += TPI;
    }
    // SAM_carrier = 0.08 * (omega2 * SAMPLERATE / (DF * TPI);
    // SAM_carrier = SAM_carrier + 0.92 * SAM_lowpass;
    // SAM_carrier_freq_offset = (int)SAM_carrier;
    // //            SAM_display_count = 0;
    // SAM_lowpass = SAM_carrier;
}