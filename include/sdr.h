#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#define SAMPLE_BUFFER_SIZE (512)

#define SAMPLE_RATE (192000)
#define FREQ_CONV_OFFSET (SAMPLE_RATE / DR)

#define WAVEFORM_WIDTH SAMPLE_BUFFER_SIZE
#define WAVEFORM_HEIGHT 256

#define DEMOD_USB 0
#define DEMOD_LSB 1
#define DEMOD_AM 2
#define DEMOD_SAM 3
#define DEMOD_SAML 4
#define DEMOD_SAMU 5
#define DEMOD_FM 6



#define DR 4 // decimation factor

void sdrTask(void *args);
void calcula_fft(void);