#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#include <math.h>

#include "esp_dsp.h"

#include "agc.h"

#include "sdr_math.h"

// AGC

agc_wdsp_params_t agc_wdsp_conf;

#define MAX_SAMPLE_RATE 48000
#define RB_SIZE ((MAX_SAMPLE_RATE / 1000) * 4)

float out_sample[2];
float abs_out_sample;
float tau_attack;
float tau_decay;
int n_tau;
float max_gain;
float var_gain;
float fixed_gain = 1.0;
float max_input;
float out_targ;
float tau_fast_backaverage;
float tau_fast_decay;
float pop_ratio;
uint8_t hang_enable;
float tau_hang_backmult;
float hangtime;
float hang_thresh;
float tau_hang_decay;
float ring[RB_SIZE * 2];
float abs_ring[RB_SIZE];
// assign constants
unsigned ring_buffsize = RB_SIZE;
// do one-time initialization
int out_index = -1;
float ring_max = 0.0;
float volts = 0.0;
float save_volts = 0.0;
float fast_backaverage = 0.0;
float hang_backaverage = 0.0;
int hang_counter = 0;
uint8_t decay_type = 0;
uint8_t state = 0;
int attack_buffsize;
uint32_t in_index;
float attack_mult;
float decay_mult;
float fast_decay_mult;
float fast_backmult;
float onemfast_backmult;
float out_target;
float min_volts;
float inv_out_target;
float tmp;
float slope_constant;
float inv_max_input;
float hang_level;
float hang_backmult;
float onemhang_backmult;
float hang_decay_mult;

void AGC_init(void)
{
    agc_wdsp_conf.agc_action = 0;
    agc_wdsp_conf.AGC_mode = 1;
    agc_wdsp_conf.agc_switch_mode = 1;
    agc_wdsp_conf.agc_thresh = 30;
    agc_wdsp_conf.agc_slope = 100;
    agc_wdsp_conf.agc_decay = 100;
}
// *********************************************************************************************************

void AGC_prep(void)
{
    float tmp;
    float sample_rate = MAX_SAMPLE_RATE;
    // Start variables taken from wdsp
    // RXA.c !!!!
    /*
      0.001,                      // tau_attack
      0.250,                      // tau_decay
      4,                        // n_tau
      10000.0,                    // max_gain
      1.5,                      // var_gain
      1000.0,                     // fixed_gain
      1.0,                      // max_input
      1.0,                      // out_target
      0.250,                      // tau_fast_backaverage
      0.005,                      // tau_fast_decay
      5.0,                      // pop_ratio
      1,                        // hang_enable
      0.500,                      // tau_hang_backmult
      0.250,                      // hangtime
      0.250,                      // hang_thresh
      0.100);                     // tau_hang_decay
  */
    /*  GOOD WORKING VARIABLES
      max_gain = 1.0;                    // max_gain
      var_gain = 0.0015; // 1.5                      // var_gain
      fixed_gain = 1.0;                     // fixed_gain
      max_input = 1.0;                 // max_input
      out_target = 0.00005; //0.0001; // 1.0                // out_target
  */
    tau_attack = 0.001; // tau_attack
    //    tau_decay = 0.250;                // tau_decay
    n_tau = 1; // n_tau

    //    max_gain = 1000.0; // 1000.0; max gain to be applied??? or is this AGC threshold = knee level?
    fixed_gain = 0.7; // if AGC == OFF
    max_input = 2.0;  //
    out_targ = 0.3;   // target value of audio after AGC
    //    var_gain = 32.0;  // slope of the AGC --> this is 10 * 10^(slope / 20) --> for 10dB slope, this is 30
    var_gain = powf(10.0, (float)agc_wdsp_conf.agc_slope / 200.0); // 10 * 10^(slope / 20)

    tau_fast_backaverage = 0.250; // tau_fast_backaverage
    tau_fast_decay = 0.005;       // tau_fast_decay
    pop_ratio = 5.0;              // pop_ratio
    hang_enable = 0;              // hang_enable
    tau_hang_backmult = 0.500;    // tau_hang_backmult
    hangtime = 0.250;             // hangtime
    hang_thresh = 0.250;          // hang_thresh
    tau_hang_decay = 0.100;       // tau_hang_decay

    // calculate internal parameters
    if (agc_wdsp_conf.agc_switch_mode)
    {
        switch (agc_wdsp_conf.AGC_mode)
        {
        case 0: // agcOFF
            break;
        case 2: // agcLONG
            hangtime = 2.000;
            agc_wdsp_conf.agc_decay = 2000;
            break;
        case 3: // agcSLOW
            hangtime = 1.000;
            agc_wdsp_conf.agc_decay = 500;
            break;
        case 4: // agcMED
            hang_thresh = 1.0;
            hangtime = 0.000;
            agc_wdsp_conf.agc_decay = 250;
            break;
        case 5: // agcFAST
            hang_thresh = 1.0;
            hangtime = 0.000;
            agc_wdsp_conf.agc_decay = 50;
            break;
        case 1: // agcFrank
            hang_enable = 0;
            hang_thresh = 0.100;       // from which level on should hang be enabled
            hangtime = 2.000;          // hang time, if enabled
            tau_hang_backmult = 0.500; // time constant exponential averager

            agc_wdsp_conf.agc_decay = 4000; // time constant decay long
            tau_fast_decay = 0.05;          // tau_fast_decay
            tau_fast_backaverage = 0.250;   // time constant exponential averager
            //      max_gain = 1000.0; // max gain to be applied??? or is this AGC threshold = knee level?
            //      fixed_gain = 1.0; // if AGC == OFF
            //      max_input = 1.0; //
            //      out_targ = 0.2; // target value of audio after AGC
            //      var_gain = 30.0;  // slope of the AGC -->

            /*    // sehr gut!
               hang_thresh = 0.100;
              hangtime = 2.000;
              tau_decay = 2.000;
              tau_hang_backmult = 0.500;
              tau_fast_backaverage = 0.250;
              out_targ = 0.0004;
              var_gain = 0.001; */
            break;
        default:
            break;
        }
        agc_wdsp_conf.agc_switch_mode = 0;
    }
    tau_decay = (float)agc_wdsp_conf.agc_decay / 1000.0;
    max_gain = powf(10.0, (float)agc_wdsp_conf.agc_thresh / 20.0);

    attack_buffsize = (int)ceil(sample_rate * n_tau * tau_attack);

    in_index = attack_buffsize + out_index;

    attack_mult = 1.0 - expf(-1.0 / (sample_rate * tau_attack));

    decay_mult = 1.0 - expf(-1.0 / (sample_rate * tau_decay));

    fast_decay_mult = 1.0 - expf(-1.0 / (sample_rate * tau_fast_decay));

    fast_backmult = 1.0 - expf(-1.0 / (sample_rate * tau_fast_backaverage));

    onemfast_backmult = 1.0 - fast_backmult;

    out_target = out_targ * (1.0 - expf(-(float)n_tau)) * 0.9999;

    min_volts = out_target / (var_gain * max_gain);
    inv_out_target = 1.0 / out_target;

    tmp = log10f(out_target / (max_input * var_gain * max_gain));
    if (tmp == 0.0)
        tmp = 1e-16;
    slope_constant = (out_target * (1.0 - 1.0 / var_gain)) / tmp;

    inv_max_input = 1.0 / max_input;

    tmp = powf(10.0, (hang_thresh - 1.0) / 0.125);
    hang_level = (max_input * tmp + (out_target /
                                     (var_gain * max_gain)) *
                                        (1.0 - tmp)) *
                 0.637;

    hang_backmult = 1.0 - expf(-1.0 / (sample_rate * tau_hang_backmult));
    onemhang_backmult = 1.0 - hang_backmult;

    hang_decay_mult = 1.0 - expf(-1.0 / (sample_rate * tau_hang_decay));
}

// *********************************************************************************************************
void IRAM_ATTR RxAGC(float *demod_out, int BUFFER_SIZE)
{
    int k;
    float mult, vo;

    if (agc_wdsp_conf.AGC_mode == 0) // AGC OFF
    {
        for (unsigned i = 0; i < BUFFER_SIZE; i++)
        {
            demod_out[i] = fixed_gain * demod_out[i];
        }
        return;
    }

    for (unsigned i = 0; i < BUFFER_SIZE; i++)
    {
        if (++out_index >= (int)ring_buffsize)
            out_index -= ring_buffsize;
        if (++in_index >= ring_buffsize)
            in_index -= ring_buffsize;

        out_sample[0] = ring[2 * out_index];
        abs_out_sample = abs_ring[out_index];

        ring[2 * in_index] = demod_out[i];

        abs_ring[in_index] = fabsf(demod_out[i]);

        fast_backaverage = fast_backmult * abs_out_sample + onemfast_backmult * fast_backaverage;
        hang_backaverage = hang_backmult * abs_out_sample + onemhang_backmult * hang_backaverage;

        if ((abs_out_sample >= ring_max) && (abs_out_sample > 0.0))
        {
            ring_max = 0.0;
            k = out_index;
            for (int j = 0; j < attack_buffsize; j++)
            {
                if (++k == (int)ring_buffsize)
                    k = 0;
                if (abs_ring[k] > ring_max)
                    ring_max = abs_ring[k];
            }
        }
        if (abs_ring[in_index] > ring_max)
            ring_max = abs_ring[in_index];

        if (hang_counter > 0)
            --hang_counter;

        switch (state)
        {
        case 0:
        {
            if (ring_max >= volts)
            {
                volts += (ring_max - volts) * attack_mult;
            }
            else
            {
                if (volts > pop_ratio * fast_backaverage)
                {
                    state = 1;
                    volts += (ring_max - volts) * fast_decay_mult;
                }
                else
                {
                    if (hang_enable && (hang_backaverage > hang_level))
                    {
                        state = 2;
                        hang_counter = (int)(hangtime * MAX_SAMPLE_RATE);
                        decay_type = 1;
                    }
                    else
                    {
                        state = 3;
                        volts += (ring_max - volts) * decay_mult;
                        decay_type = 0;
                    }
                }
            }
            break;
        }
        case 1:
        {
            if (ring_max >= volts)
            {
                state = 0;
                volts += (ring_max - volts) * attack_mult;
            }
            else
            {
                if (volts > save_volts)
                {
                    volts += (ring_max - volts) * fast_decay_mult;
                }
                else
                {
                    if (hang_counter > 0)
                    {
                        state = 2;
                    }
                    else
                    {
                        if (decay_type == 0)
                        {
                            state = 3;
                            volts += (ring_max - volts) * decay_mult;
                        }
                        else
                        {
                            state = 4;
                            volts += (ring_max - volts) * hang_decay_mult;
                        }
                    }
                }
            }
            break;
        }
        case 2:
        {
            if (ring_max >= volts)
            {
                state = 0;
                save_volts = volts;
                volts += (ring_max - volts) * attack_mult;
            }
            else
            {
                if (hang_counter == 0)
                {
                    state = 4;
                    volts += (ring_max - volts) * hang_decay_mult;
                }
            }
            break;
        }
        case 3:
        {
            if (ring_max >= volts)
            {
                state = 0;
                save_volts = volts;
                volts += (ring_max - volts) * attack_mult;
            }
            else
            {
                volts += (ring_max - volts) * decay_mult;
            }
            break;
        }
        case 4:
        {
            if (ring_max >= volts)
            {
                state = 0;
                save_volts = volts;
                volts += (ring_max - volts) * attack_mult;
            }
            else
            {
                volts += (ring_max - volts) * hang_decay_mult;
            }
            break;
        }
        }

        if (volts < min_volts)
        {
            volts = min_volts; // no AGC action is taking place
            agc_wdsp_conf.agc_action = 0;
        }
        else
        {
            // LED indicator for AGC action
            agc_wdsp_conf.agc_action = 1;
        }

        vo = log10f_fast(inv_max_input * volts);

        if (vo > 0.0)
        {
            vo = 0.0;
        }
        mult = (out_target - slope_constant * vo) / volts;
        demod_out[i] = out_sample[0] * mult;
    }
}

// *********************************************************************************************************