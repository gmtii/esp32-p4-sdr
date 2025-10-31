#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#include "sdr.h"

#include "lvgl.h"

extern int16_t pixelnew[SAMPLE_BUFFER_SIZE];
extern int16_t pixelold[SAMPLE_BUFFER_SIZE];

static lv_obj_t *waveform_canvas;
static lv_obj_t *screen;
static lv_style_t border_style;
static lv_style_t popupBox_style;

#define WAVEFORM_WIDTH SAMPLE_BUFFER_SIZE
#define WAVEFORM_HEIGHT 256

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

// Dibuja un píxel en coordenadas (x,y) con color = índice 0..15
void lv_draw_pixel(uint16_t x, uint16_t y, uint8_t color_idx)
{
  if (x >= W || y >= H)
    return;
  size_t index = (size_t)y * ROW_BYTES + (x >> 1);
  uint8_t px = waveformbuffer[index];
  if (x & 1)
    px = (px & 0xF0) | (color_idx & 0x0F);
  else
    px = (px & 0x0F) | ((color_idx & 0x0F) << 4);
  waveformbuffer[index] = px;
}

// Dibuja una línea horizontal desde (x,y) de longitud “len” píxeles
void lv_draw_vline(uint16_t x, uint16_t y, uint16_t longitud, uint8_t color_idx)
{
  if (x >= W || y >= H)
    return;
  if (y + longitud > H)
    longitud = H - y;
  uint8_t *ptr = &waveformbuffer[y * ROW_BYTES + (x >> 1)];
  uint8_t nibble_mask = (x & 1) ? 0xF0 : 0x0F;
  uint8_t color_val = (x & 1) ? (color_idx & 0x0F) : ((color_idx & 0x0F) << 4);
  for (uint16_t i = 0; i < longitud; i++)
  {
    *ptr = (*ptr & nibble_mask) | color_val;
    ptr += ROW_BYTES;
  }
}

/* ---- CONTEXTO DE DRAG ---- */
typedef struct
{
  lv_point_t start_ptr; // punto de toque inicial (coords de pantalla)
  lv_point_t start_pos; // posición inicial del objeto (coords del padre)
} drag_ctx_t;

static void drag_event_cb(lv_event_t *e)
{
  lv_obj_t *obj = lv_event_get_target_obj(e);
  drag_ctx_t *ctx = (drag_ctx_t *)lv_event_get_user_data(e); // tu contexto
  lv_indev_t *indev = lv_indev_active();
  if (!indev || !ctx)
    return;

  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_PRESSED)
  {
    // guarda punto inicial del dedo y posición inicial del objeto
    lv_indev_get_point(indev, &ctx->start_ptr);
    ctx->start_pos.x = lv_obj_get_x(obj);
    ctx->start_pos.y = lv_obj_get_y(obj);
    return;
  }

  if (code == LV_EVENT_PRESSING)
  {
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_coord_t dx = p.x - ctx->start_ptr.x;
    lv_coord_t dy = p.y - ctx->start_ptr.y;

    // nueva pos propuesta
    lv_coord_t nx = ctx->start_pos.x + dx;
    lv_coord_t ny = ctx->start_pos.y + dy;

    // (opcional) confinar dentro del padre
    lv_obj_t *parent = lv_obj_get_parent(obj);
    if (parent)
    {
      lv_coord_t pw = lv_obj_get_content_width(parent);
      lv_coord_t ph = lv_obj_get_content_height(parent);
      lv_coord_t ow = lv_obj_get_width(obj);
      lv_coord_t oh = lv_obj_get_height(obj);

      if (nx < 0)
        nx = 0;
      if (ny < 0)
        ny = 0;
      if (nx > pw - ow)
        nx = pw - ow;
      if (ny > ph - oh)
        ny = ph - oh;
    }

    lv_obj_set_pos(obj, nx, ny);
    return;
  }
}

void init_ui()
{

  /* Obtén la pantalla activa */
  screen = lv_scr_act();

  // Padre "limpio"
  lv_obj_remove_style_all(screen);
  lv_obj_set_style_border_width(screen, 0, 0);
  lv_obj_set_style_pad_all(screen, 0, 0);
  lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  memset(waveformbuffer, 0x00, BUF_SIZE); // índice 0 (negro)

  waveform_canvas = lv_canvas_create(screen);
  lv_canvas_set_buffer(waveform_canvas, waveformbuffer, W, H, LV_COLOR_FORMAT_I4);

  // Borde y fondo
  lv_obj_set_style_bg_color(waveform_canvas, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(waveform_canvas, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(waveform_canvas, 2, 0);
  lv_obj_set_style_border_color(waveform_canvas, lv_color_hex(0x999999), 0);
  lv_obj_set_style_radius(waveform_canvas, 6, 0);
  lv_obj_set_style_pad_all(waveform_canvas, 0, 0);

  // Para una “aura” visible opcional:
  lv_obj_set_style_outline_width(waveform_canvas, 2, 0);
  lv_obj_set_style_outline_color(waveform_canvas, lv_color_hex(0x00FFA0), 0);

  // Flags para poder arrastrar
  lv_obj_add_flag(waveform_canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(waveform_canvas, LV_OBJ_FLAG_FLOATING);         // ignora layout del padre
  lv_obj_clear_flag(waveform_canvas, LV_OBJ_FLAG_GESTURE_BUBBLE); // evita que burbujee al padre

  // Desactiva scroll del padre (o usa un contenedor intermedio sin layout)
  lv_obj_t *parent = lv_obj_get_parent(waveform_canvas);
  if (parent)
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

  // Contexto para el drag
  static drag_ctx_t s_drag_ctx; // estático (o alócalo si prefieres)
  lv_obj_add_event_cb(waveform_canvas, drag_event_cb, LV_EVENT_PRESSED, &s_drag_ctx);
  lv_obj_add_event_cb(waveform_canvas, drag_event_cb, LV_EVENT_PRESSING, &s_drag_ctx);

  lv_image_dsc_t *img = lv_canvas_get_image(waveform_canvas);
  uint8_t *buf = (uint8_t *)lv_canvas_get_buf(waveform_canvas);
  Serial.printf("W=%u, H=%u, stride=%u bytes\n", (unsigned)img->header.w,
                (unsigned)img->header.h, (unsigned)img->header.stride);

  // Paleta (colores)
  lv_canvas_set_palette(waveform_canvas, 0, lv_color32_make(0, 0, 0, 0xFF));       // negro
  lv_canvas_set_palette(waveform_canvas, 1, lv_color32_make(255, 255, 255, 0xFF)); // blanco
  lv_canvas_set_palette(waveform_canvas, 2, lv_color32_make(255, 0, 0, 0xFF));     // rojo
  lv_canvas_set_palette(waveform_canvas, 3, lv_color32_make(0, 255, 0, 0xFF));     // verde
  lv_canvas_set_palette(waveform_canvas, 4, lv_color32_make(0, 0, 255, 0xFF));     // azul
  lv_canvas_set_palette(waveform_canvas, 5, lv_color32_make(64, 64, 64, 0xFF));    // gris

  // Dibuja algunas líneas verticales
  lv_draw_vline(W - 1, 0, H, 5);

  // ✅ Forzar actualización
  lv_obj_invalidate(waveform_canvas);
}

void spectrum(void)
{

  lv_obj_invalidate(waveform_canvas);

  int16_t y_old, y_new, y1_new, y1_old;
  int16_t y1_old_minus = 0;
  int16_t y1_new_minus = 0;
  int16_t origen = 0;

#define MARGEN_DERECHO 2
#define MARGEN_IZQUIERDO 2

  for (int16_t x = MARGEN_IZQUIERDO; x < SAMPLE_BUFFER_SIZE - MARGEN_DERECHO; x++)
  {

    // moving window - weighted average of 5 points of the spectrum to smooth spectrum in the frequency domain
    // weights:  x: 50% , x-1/x+1: 36%, x+2/x-2: 14%

    y_new = pixelnew[x] * 0.5 + pixelnew[x - 1] * 0.18 + pixelnew[x + 1] * 0.18 + pixelnew[x - 2] * 0.07 + pixelnew[x + 2] * 0.07;
    y_old = pixelold[x] * 0.5 + pixelold[x - 1] * 0.18 + pixelold[x + 1] * 0.18 + pixelold[x - 2] * 0.07 + pixelold[x + 2] * 0.07;

    if (y_old > (spectrum_height - 1))
    {
      y_old = (spectrum_height - 1);
    }

    if (y_new > (spectrum_height - 1))
    {
      y_new = (spectrum_height - 1);
    }

    if (y_old < 0)
      y_old = 0;
    if (y_new < 0)
      y_new = 0;

    y1_old = (spectrum_y + spectrum_height - 1) - y_old;
    y1_new = (spectrum_y + spectrum_height - 1) - y_new;

    if (x == MARGEN_IZQUIERDO)
    {
      y1_old_minus = y1_old;
      y1_new_minus = y1_new;
    }
    if (x == SAMPLE_BUFFER_SIZE - MARGEN_DERECHO)
    {
      y1_old_minus = y1_old;
      y1_new_minus = y1_new;
    }

    // DELETE OLD LINE/POINT
    if (y1_old - y1_old_minus > 1)
    { // plot line upwards
      lv_draw_vline(x + spectrum_x, y1_old_minus + 1, y1_old - y1_old_minus, 0);
    }
    else if (y1_old - y1_old_minus < -1)
    { // plot line downwards
      lv_draw_vline(x + spectrum_x, y1_old, y1_old_minus - y1_old, 0);
    }
    else
    {
      lv_draw_pixel(x + spectrum_x, y1_old, 0); // delete old pixel
    }

    // DRAW NEW LINE/POINT
    if (y1_new - y1_new_minus > 1)
    { // plot line upwards
      lv_draw_vline(x + spectrum_x, y1_new_minus + 1, y1_new - y1_new_minus, 3);
    }
    else if (y1_new - y1_new_minus < -1)
    { // plot line downwards
      lv_draw_vline(x + spectrum_x, y1_new, y1_new_minus - y1_new, 3);
    }
    else
    {
      lv_draw_pixel(x + spectrum_x, y1_new, 3); // write new pixel
    }

    y1_new_minus = y1_new;
    y1_old_minus = y1_old;
  }

  // Dibuja centro de espectro
  lv_draw_vline(W - 1 ,d 0, H, 5);
}