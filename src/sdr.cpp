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

#include "sdr.h"

static i2s_chan_handle_t tx_handle = NULL;
static i2s_chan_handle_t rx_handle = NULL;

i2c_master_bus_handle_t bus;
i2c_master_dev_handle_t dev;

static esp_err_t i2c_driver_init(void)
{
    /* Initialize I2C peripheral */

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM,
        .sda_io_num = I2C_SDA_IO,
        .scl_io_num = I2C_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus)); // crea el bus

    // --- 3️⃣ Añade un dispositivo esclavo al bus ---
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x1A, // Dirección del dispositivo I2C
        .scl_speed_hz = 100000, // Frecuencia 100 kHz
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &dev));
}

void i2s_driver_init(uint32_t sample_rate)
{

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Limpia DMA legacy
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = sample_rate, // 192 kHz
            .clk_src = I2S_CLK_SRC_APLL,      // APLL en P4
            .mclk_multiple = MCLK_MULTIPLE    // define en sdkconfig.h (256 recomendado)
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCK_IO,
            .bclk = I2S_BCK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_DO_IO,
            .din = I2S_DI_IO,
            .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

}

static void play_tone(float freq_hz, uint32_t ms)
{
    const float amplitude = 0.5f; // 0.0–1.0
    const int16_t max_amp = (int16_t)(32767 * amplitude);
    const int samples_per_period = (int)((float)48000 / freq_hz);
    const size_t frames_total = (48000 * ms) / 1000;
    const size_t chunk_frames = 256; // tamaño de bloque DMA
    int16_t buffer[chunk_frames * 2];

    ESP_LOGI(TAG, "Generando %.1f Hz durante %u ms", freq_hz, ms);

    for (size_t pos = 0; pos < frames_total;)
    {
        size_t frames_now = (frames_total - pos > chunk_frames)
                                ? chunk_frames
                                : (frames_total - pos);

        for (size_t i = 0; i < frames_now; i++)
        {
            float theta = 2.0f * M_PI * (float)((pos + i) % samples_per_period) / samples_per_period;
            int16_t s = (int16_t)(sinf(theta) * max_amp);
            buffer[i * 2 + 0] = s; // canal L
            buffer[i * 2 + 1] = s; // canal R
        }

        size_t bytes_to_write = frames_now * 2 * sizeof(int16_t);
        size_t written = 0;
        ESP_ERROR_CHECK(i2s_channel_write(tx_handle, buffer, bytes_to_write, &written, portMAX_DELAY));
        pos += frames_now;
    }
}

void i2s_echo(void *args)
{

    esp_err_t ret = ESP_OK;
    size_t bytes_read = 0;
    size_t bytes_write = 0;
    Serial.printf("[echo] Echo start");

    while (1)
    {
        /* Lee i2s ADC */
        ret = i2s_channel_read(rx_handle, (char *)&sampleData_in[0].sample, SAMPLE_BUFFER_SIZE * 4, &bytes_read, 1000);

        for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++)
        {
            sampleData_out[i].ch[0] = sampleData_in[i].ch[0];
            sampleData_out[i].ch[1] = sampleData_in[i].ch[1];
        }

        // Envia el DAC SAMPLE_BUFFER_SIZE * 4 ( 2 canales, 16 bit cada uno)
        ret = i2s_channel_write(tx_handle, (char *)&sampleData_out[0].sample, SAMPLE_BUFFER_SIZE * 4, &bytes_write, 1000);
    }
    vTaskDelete(NULL);
}
