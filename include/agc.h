#ifndef __AGC_H__
#define __AGC_H__

#include "Arduino.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
uint8_t agc_action;
int8_t AGC_mode;
uint8_t agc_switch_mode;
int agc_thresh;
int agc_slope;
int agc_decay;
} agc_wdsp_params_t;

extern agc_wdsp_params_t agc_wdsp_conf;

void AGC_init(void);
void AGC_prep(void);
void RxAGC(float *demod_out, int BUFFER_SIZE);

#endif