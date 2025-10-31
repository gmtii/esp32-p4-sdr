#pragma once

#define LCD_H_RES 1024
#define LCD_V_RES 600

#define LCD_RST 27
#define LCD_LED 23

#define TP_I2C_SDA 7
#define TP_I2C_SCL 8
#define TP_RST 22
#define TP_INT 21

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