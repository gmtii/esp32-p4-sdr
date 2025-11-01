#pragma once

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

#include "pins_config.h"

#define MCLK_MULTIPLE I2S_MCLK_MULTIPLE_256 // If not using 24-bit data width, 256 should be enough


i2s_chan_handle_t tx_handle = NULL;
i2s_chan_handle_t rx_handle = NULL;

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
            .clk_src = I2S_CLK_SRC_APLL,   // APLL en P4
            .mclk_multiple = MCLK_MULTIPLE // define en sdkconfig.h (256 recomendado)
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
