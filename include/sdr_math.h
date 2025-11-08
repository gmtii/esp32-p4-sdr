

#ifndef __SDR_MATH_H__
#define __SDR_MATH_H__

#include "Arduino.h"
#include <stdint.h>
#include <stdbool.h>

#define TPI TWO_PI
#define PIH HALF_PI
#define FOURPI (2.0f * TPI)
#define SIXPI (3.0f * TPI)

float ApproxAtan(float z);
float ApproxAtan2(float y, float x);
float log10f_fast(float X);
float convertToF32(int16_t sample);
int16_t convertToInt16(float sample);
float sign(float x);


#endif