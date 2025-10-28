#include <stdint.h>
#include <vector>
#include "esp_system.h"
#include "esp_check.h"
#include "esp_err.h"

#define JC1060L4700_DEV

#ifdef ESP32P4_M3_DEV
#define SAMPLE_BUFFER_SIZE (512)
#define SAMPLE_RATE (48000)
#define MCLK_MULTIPLE I2S_MCLK_MULTIPLE_256 // If not using 24-bit data width, 256 should be enough
#define MCLK_FREQ_HZ (SAMPLE_RATE * MCLK_MULTIPLE)
#define VOICE_VOLUME CONFIG_VOICE_VOLUME

#define I2S_NUM (I2S_NUM_0)
#define I2S_MCK_IO (GPIO_NUM_1)

#define I2S_WS_IO (GPIO_NUM_5)
#define I2S_BCK_IO (GPIO_NUM_4)
#define I2S_DO_IO (GPIO_NUM_3)
#define I2S_DI_IO (GPIO_NUM_2)

#define I2C_NUM (I2C_NUM_0)
#define I2C_SDA_IO GPIO_NUM_7
#define I2C_SCL_IO GPIO_NUM_8
#endif

#ifdef JC1060L4700_DEV
#define SAMPLE_BUFFER_SIZE (512)
#define SAMPLE_RATE (48000)
#define MCLK_MULTIPLE I2S_MCLK_MULTIPLE_256 // If not using 24-bit data width, 256 should be enough
#define MCLK_FREQ_HZ (SAMPLE_RATE * MCLK_MULTIPLE)
#define VOICE_VOLUME CONFIG_VOICE_VOLUME

#define I2S_NUM (I2S_NUM_0)
#define I2S_MCK_IO (GPIO_NUM_5)

#define I2S_WS_IO (GPIO_NUM_45)
#define I2S_BCK_IO (GPIO_NUM_46)
#define I2S_DO_IO (GPIO_NUM_47)
#define I2S_DI_IO (GPIO_NUM_48)

#define SPI_DAT_PIN (GPIO_NUM_4)
#define SPI_CLK_PIN (GPIO_NUM_3)
#define NAU8822_CS_PIN (GPIO_NUM_2)

#define I2C_NUM (I2C_NUM_0)
#define I2C_SCL_IO (GPIO_NUM_8)
#define I2C_SDA_IO (GPIO_NUM_7)

#endif

union
{
    uint32_t sample;
    int16_t ch[2];
} sampleData_in[SAMPLE_BUFFER_SIZE];

union
{
    uint32_t sample;
    int16_t ch[2];
} sampleData_out[SAMPLE_BUFFER_SIZE];

void i2s_driver_init(uint32_t sample_rate);
void i2s_echo(void *args);
