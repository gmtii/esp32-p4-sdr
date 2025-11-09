#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#include "sdr.h"

#include "lvgl.h"

static lv_obj_t *waveform_canvas;
static lv_obj_t *screen;
static lv_style_t border_style;
static lv_style_t popupBox_style;

lv_obj_t *btn1;
lv_obj_t *btn2;
lv_obj_t *btn3;
lv_obj_t *btn4;
lv_obj_t *btn5;
lv_obj_t *btn6;

lv_obj_t *label1;
lv_obj_t *label2;
lv_obj_t *label3;
lv_obj_t *label4;
lv_obj_t *label5;
lv_obj_t *label6;

extern int16_t pixelnew[SAMPLE_BUFFER_SIZE];
extern int16_t pixelold[SAMPLE_BUFFER_SIZE];

// Offset y máxima altura. Para controlar el espectro.
int spectrum_y = 0; // upper edge
int spectrum_x = 0;
int spectrum_height = WAVEFORM_HEIGHT;

#define W WAVEFORM_WIDTH
#define H WAVEFORM_HEIGHT

/* bytes por fila en I4 = ceil(W/2) */
#define ROW_BYTES ((W + 1) / 2)
#define BUF_SIZE (ROW_BYTES * H)

static uint8_t waveformbuffer[BUF_SIZE] __attribute__((aligned(32)));