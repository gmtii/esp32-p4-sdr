#include <Arduino.h>
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>

#include "nau8822.h"

#include "main.h"

#include "i2s_driver.h"

#include "sdr.h"

#include "ui.h"

#include "agc.h"

#define DEMOD_USB 0
#define DEMOD_LSB 1
#define DEMOD_AM 2
#define DEMOD_SAM 3
#define DEMOD_SAML 4
#define DEMOD_SAMU 5
#define DEMOD_FM 6

const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;

boolean debug = false;
boolean bucle = false;

int volume = 0x3f;
int nr_mode = 0;

int demod_modo = DEMOD_LSB;

String demod_modos_texto[7] = {
    "USB ", // 0
    "LSB ",
    "AM  ",
    "SAM ",
    "S-L ",
    "S-U ",
    " FM "}; // 6

void setup()
{

  Serial.begin(115200);

  pinMode(LCD_LED, OUTPUT);
  ledcAttach(LCD_LED, freq, resolution);
  ledcWrite(LCD_LED, 5);

  lvgl_begin();

  setup_ldo();

  /* Iniciando I2S*/
  i2s_driver_init(48000);

  /* Iniciando CODEC */
  nau8822_init(2); // Modo de inicialización del NAU8822

  nau8822_init(7); // LIN RIN

  nau8822_spk_volume(0x3F);

  /* AGC */
  AGC_init();
  AGC_prep();

  /* Init UI*/

  init_ui();

  xTaskCreatePinnedToCore(sdrTask, "sdrTask", 8192, NULL, 5, NULL, 0);

  Serial.println("Setup terminado...");
}

void loop()
{
  calcula_fft();
  spectrum();

  lv_task_handler(); // let the GUI do its work
  delay(5);          // let this time pass

  if (Serial.available())
  {
    char c = Serial.read(); // lee un carácter

    switch (c)
    {

    case 'd':

      demod_modo--;
      if (demod_modo < 0)
        demod_modo = 6;

      Serial.printf("Demod modo= %s\n", demod_modos_texto[demod_modo]);

      break;

    case 'b':

      if (bucle == false)
        bucle = true;
      else
        bucle = false;

      Serial.printf("DBucle= %d\n", bucle);

      break;

    case '+':

      volume++;

      if (volume > 0x3F)
      {
        volume = 0x3F;
      }

      Serial.printf("Vol= %d\n", volume);

      nau8822_spk_volume(volume);
      break;

    case '-':

      volume--;

      if (volume < 1)
      {
        volume = 0;
      }

      Serial.printf("Vol= %d\n", volume);

      nau8822_spk_volume(volume);
      break;

    case 't':
      break;
    }
  }
}