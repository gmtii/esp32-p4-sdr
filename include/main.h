#pragma once

#ifndef _MAIN_H_
#define _MAIN_H_

#include <Arduino.h>

#include "lvgl.h"
#include "pins_config.h"
#include "display_code/lcd/jd9165_lcd.h"
#include "display_code/touch/gt911_touch.h"

#include "esp_lcd_mipi_dsi.h"

#include "esp_ldo_regulator.h"

esp_ldo_channel_handle_t ldo2 = NULL;
esp_ldo_channel_handle_t ldo3 = NULL;

jd9165_lcd lcd = jd9165_lcd(LCD_RST);
gt911_touch touch = gt911_touch(TP_I2C_SDA, TP_I2C_SCL, TP_RST, TP_INT);

lv_display_t *disp_drv;
static lv_color_t *buf0;
static lv_color_t *buf1;

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

static void lvgl_begin(void)
{
    lcd.begin();
    touch.begin();

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
}

static void setup_ldo(void)
{
    esp_ldo_dump(stdout);

    // Create configuration for LDO index 3
    esp_ldo_channel_config_t config3 = {
        .chan_id = 4, // discovered by trial and error
        .voltage_mv = 3300,
        .flags = {
            .adjustable = 1,
            .owned_by_hw = 0,
            .bypass = 0}};

    if (esp_ldo_acquire_channel(&config3, &ldo3) == ESP_OK)
    {
        Serial.println("LDO index 3 acquired");
    }
    else
    {
        Serial.println("Failed to acquire LDO index 3");
    }

    if (ldo3)
    {
        esp_ldo_channel_adjust_voltage(ldo3, 3300);
        Serial.println("LDO index 3 voltage adjusted");
    }

    // Optionally: dump to see results
    esp_ldo_dump(stdout);
}
#endif