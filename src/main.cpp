#include <Arduino.h>
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#include "lvgl.h"
#include "pins_config.h"
#include "src/lcd/jd9165_lcd.h"
#include "src/touch/gt911_touch.h"

#include "esp_lcd_mipi_dsi.h"

#include <stdio.h>
#include <string.h>

#include "nau8822.h"

#include "sdr.h"

jd9165_lcd lcd = jd9165_lcd(LCD_RST);
gt911_touch touch = gt911_touch(TP_I2C_SDA, TP_I2C_SCL, TP_RST, TP_INT);

lv_display_t *disp_drv;
static lv_color_t *buf0;
static lv_color_t *buf1;

const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;

/* The my_disp_flush function is a display flushing callback for the LVGL graphics library, responsible for rendering
a specified area of the display (area) using the provided color map (color_map). It draws the bitmap to the screen using
lcd.lcd_draw_bitmap and signals LVGL that the flushing operation is complete by calling lv_display_flush_ready.
 */

static SemaphoreHandle_t s_flush_sem; // Semáforo para sincronizar el flush con el ISR

/* Se llama en contexto de ISR. Mantenerlo ultracorto. */

static bool IRAM_ATTR panel_eof_isr(esp_lcd_panel_handle_t panel_handle,
                                    esp_lcd_dpi_panel_event_data_t *edata,
                                    void *user_ctx)
{
  BaseType_t need_yield = pdFALSE;
  SemaphoreHandle_t sem = (SemaphoreHandle_t)user_ctx;
  if (sem)
    xSemaphoreGiveFromISR(sem, &need_yield);
  return (need_yield == pdTRUE); // permite yield desde ISR si hace falta
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *color_map)
{
  const int offsetx1 = area->x1;
  const int offsetx2 = area->x2;
  const int offsety1 = area->y1;
  const int offsety2 = area->y2;
  lcd.lcd_draw_bitmap(offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
  // lv_display_flush_ready(disp);  // Usa el callback de espera para esto
}

static void my_flush_wait_cb(lv_display_t *d)
{
  // Espera a que el ISR libere el semáforo
  if (s_flush_sem)
    xSemaphoreTake(s_flush_sem, portMAX_DELAY);
}

/* The my_touchpad_read function reads touch input data from a touchpad device and updates the provided lv_indev_data_t
structure with the touch state and coordinates. If the touchpad is not touched, the state is set to LV_INDEV_STATE_REL;
otherwise, it is set to LV_INDEV_STATE_PR, and the touch coordinates are logged via Serial.printf.  */

void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data)
{
  bool touched;
  uint16_t touchX, touchY;

  touched = touch.getTouch(&touchX, &touchY);

  if (!touched)
  {
    data->state = LV_INDEV_STATE_REL;
  }
  else
  {
    data->state = LV_INDEV_STATE_PR;

    data->point.x = touchX;
    data->point.y = touchY;
    // Serial.printf("x=%d,y=%d \r\n", touchX, touchY);
  }
}

void my_print(lv_log_level_t level, const char *buf)
{
  Serial.printf(buf);
  Serial.flush();
}

/*use Arduinos millis() as tick source*/
static uint32_t my_tick(void)
{
  return millis();
}

void setup()
{
  Serial.begin(115200);

  lcd.begin();
  touch.begin();

  pinMode(LCD_LED, OUTPUT);
  ledcAttach(LCD_LED, freq, resolution);
  ledcWrite(LCD_LED, 25);

  i2s_driver_init(192200);

  lv_init();

  // === Buffer parcial: N líneas ===
  size_t px_cnt = (size_t)LCD_H_RES * LCD_V_RES;  // nº de píxeles del buffer
  size_t buf_bytes = px_cnt * sizeof(lv_color_t); // bytes reales

  // Reserva en PSRAM DMA-capable
  buf0 = (lv_color_t *)heap_caps_aligned_alloc(
      4, buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  buf1 = (lv_color_t *)heap_caps_aligned_alloc(
      4, buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

  assert(buf0);
  assert(buf1);

  disp_drv = lv_display_create(LCD_H_RES, LCD_V_RES);

  s_flush_sem = xSemaphoreCreateBinary();
  esp_lcd_dpi_panel_event_callbacks_t cbs = {
      .on_color_trans_done = panel_eof_isr, // “EOF” de frame/transfer finalizada
                                            // (si en tu IDF aparecen más eventos, puedes añadirlos aquí)
  };
  esp_lcd_dpi_panel_register_event_callbacks(panel_handle /* handle */,
                                             &cbs,
                                             s_flush_sem /* user_ctx */);

  lv_display_set_flush_cb(disp_drv, my_disp_flush);
  lv_display_set_flush_wait_cb(disp_drv, my_flush_wait_cb);

  lv_display_set_buffers(disp_drv, buf0, buf1, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

  /*Initialize the display*/

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

#if LV_USE_LOG
  lv_log_register_print_cb(my_print);
#endif

  lv_tick_set_cb(my_tick);

  /* Iniciando CODEC */
  nau8822_init(2); // Modo de inicialización del NAU8822

  xTaskCreate(i2s_echo, "i2s_echo", 8192, NULL, 5, NULL);
}

void test_pin(void)

{

  pinMode(GPIO_NUM_45, OUTPUT);
  pinMode(GPIO_NUM_46, OUTPUT);
  pinMode(GPIO_NUM_47, OUTPUT);
  pinMode(GPIO_NUM_48, OUTPUT);
  pinMode(GPIO_NUM_5, OUTPUT);
  pinMode(GPIO_NUM_4, OUTPUT);
  pinMode(GPIO_NUM_3, OUTPUT);
  pinMode(GPIO_NUM_2, OUTPUT);

  while (1)
  {
    digitalWrite(GPIO_NUM_45, HIGH);
    digitalWrite(GPIO_NUM_46, HIGH);
    digitalWrite(GPIO_NUM_47, HIGH);
    digitalWrite(GPIO_NUM_48, HIGH);
    digitalWrite(GPIO_NUM_5, HIGH);
    digitalWrite(GPIO_NUM_4, HIGH);
    digitalWrite(GPIO_NUM_3, HIGH);
    digitalWrite(GPIO_NUM_2, HIGH);

    delay(100);

    // digitalWrite(GPIO_NUM_45, LOW);
    digitalWrite(GPIO_NUM_46, LOW);
    digitalWrite(GPIO_NUM_47, LOW);
    digitalWrite(GPIO_NUM_48, LOW);
    digitalWrite(GPIO_NUM_5, LOW);
    digitalWrite(GPIO_NUM_4, LOW);
    digitalWrite(GPIO_NUM_3, LOW);
    digitalWrite(GPIO_NUM_2, LOW);

    delay(100);
  }
}

void loop()
{

  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
  }
}