#include <Arduino.h>
#include <nau8822.h>
#include "driver/i2c_master.h"

#define spi_dat_pin 32
#define spi_clk_pin 33
#define nau8822_cs_pin 20

#define SPI

static const int nau8822_mclk_scaler[] = {10, 15, 20, 30, 40, 60, 80, 120};

nau8822_pll pll_param;

void nau8822_spi_transfer(byte data)
{
	; // function to actually bit shift the data byte out

	for (int i = 1; i <= 8; i++)
	{ // setup a loop of 8 iterations, one for each bit
		if (data > 127)
		{									 // test the most significant bit
			digitalWrite(spi_dat_pin, HIGH); // if it is a 1 (ie. B1XXXXXXX), set the master out pin high
		}
		else
		{
			digitalWrite(spi_dat_pin, LOW); // if it is not 1 (ie. B0XXXXXXX), set the master out pin low
		}
		digitalWrite(spi_clk_pin, HIGH); // set clock high, the pot IC will read the bit into its register
		data = data << 1;
		digitalWrite(spi_clk_pin, LOW); // set clock low, the pot IC will stop reading and prepare for the next iteration (next significant bit
	}
}

void nau8822_register_write_spi(uint8_t addr, uint16_t data)
{

	uint8_t a = ((addr << 1) | (data >> 8));
	uint8_t b = ((int8_t)(data & 0x00ff));

	// use it as you would the regular arduino SPI API

	digitalWrite(nau8822_cs_pin, LOW); // pull SS slow to prep other end for transfer

	nau8822_spi_transfer(a);
	nau8822_spi_transfer(b);

	digitalWrite(nau8822_cs_pin, HIGH); // pull ss high to signify end of data transfer;
}

void nau8822_register_write(uint8_t addr, uint16_t data)
{

#ifdef SPI

	uint8_t a = ((addr << 1) | (data >> 8));
	uint8_t b = ((int8_t)(data & 0x00ff));

	// use it as you would the regular arduino SPI API

	digitalWrite(nau8822_cs_pin, LOW); // pull SS slow to prep other end for transfer

	nau8822_spi_transfer(a);
	nau8822_spi_transfer(b);

	digitalWrite(nau8822_cs_pin, HIGH); // pull ss high to signify end of data transfer;

#else

	int ret;

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (ESP_SLAVE_ADDR) | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, ((addr << 1) | (data >> 8)) | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, ((int8_t)(data & 0x00ff)) | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 10000);
	i2c_cmd_link_delete(cmd);

	if (ret != ESP_OK)
		printf("I2C error at: %i with code: %i \n", addr, ret);

#endif
}

void nau8822_spk_volume(uint8_t volume)
{

	uint8_t Volume1;

	if (volume > 0x3F)
	{
		volume = 0x3F;
	}
	if (volume < 1)
	{
		volume = 0;
	}

#ifdef SPI

	nau8822_register_write_spi(52, volume);			/* HP Volume */
	nau8822_register_write_spi(53, volume | 0x100); /* HP Volume */

	nau8822_register_write_spi(54, volume);			/* SPK Volume */
	nau8822_register_write_spi(55, volume | 0x100); /* SPK Volume */

#else

	i2c_master_init();

	nau8822_register_write_spi(52, Volume1);	   /* HP Volume */
	nau8822_register_write_spi(53, Volume1 + 256); /* HP Volume */

	nau8822_register_write_idf(54, Volume1);
	nau8822_register_write_idf(55, Volume1 + 256);

	i2c_driver_delete(I2C_MASTER_NUM);

#endif
}

uint8_t nau8822_init(int modo)
{

	pinMode(nau8822_cs_pin, OUTPUT);
	pinMode(spi_dat_pin, OUTPUT);
	pinMode(spi_clk_pin, OUTPUT);

#ifdef SPI

	switch (modo)
	{
		// CODIGO PARA 160KHZ DE BW FW I2S

	case 0: // LMICPN RMICPN DIFERENCIAL 96/192KHZ

		nau8822_register_write_spi(0, 0x000);

		delay(500);

		nau8822_register_write_spi(1, 0x11F);
		nau8822_register_write_spi(2, 0x1BF); /* Enable L/R Headphone, ADC Mix/Boost, ADC */
		nau8822_register_write_spi(3, 0x16F); /* Enable L/R main mixer, DAC */

		delay(250);

		nau8822_register_write_spi(4, 0x010);  /* 16-bit word length, I2S format, Stereo */
		nau8822_register_write_spi(5, 0x000);  /* Companding control and loop back mode (all disable) */
		nau8822_register_write_spi(6, 0x00C);  // 16bit/192k
		nau8822_register_write_spi(7, 0x000);  /* 48K for internal filter coefficients */
											   // nau8822_register_write_spi(13, 0x09f);
											   // nau8822_register_write_spi(10, 0x008); /* DAC soft mute is disabled, DAC oversampling rate is 128x */
		nau8822_register_write_spi(10, 0x008); /* DAC soft mute is disabled, DAC oversampling rate is 64X */

		nau8822_register_write_spi(14, 0x108); /* ADC HP filter is disabled, ADC oversampling rate is 64X */

		nau8822_register_write_spi(15, 0x1FF); /* ADC left digital volume control */
		nau8822_register_write_spi(16, 0x1FF); /* ADC right digital volume control */

		nau8822_register_write_spi(11, 0x0FF); /* DAC left digital volume control */
		nau8822_register_write_spi(12, 0x1FF); /* DAC right digital volume control */

		nau8822_register_write_spi(43, 0x010); /* RSUBBYP right speaker amplifier connected to submixer output (inverts RMIX for BTL) */

		nau8822_register_write_spi(44, 0x033); /* Conecta LMICN-P y RMICN-P al DAC */

		nau8822_register_write_spi(45, 0x01F); // Activa PGA para ADC-L
		nau8822_register_write_spi(46, 0x11F); // Activa PGA para ADC-R

		// nau8822_register_write_spi(47, 0x100); // LADC a LIN 0 dB
		// nau8822_register_write_spi(48, 0x100); // RADC a RIN 0 dB

		nau8822_register_write_spi(49, 0x002); // TSEN | SPKBST

		nau8822_register_write_spi(50, 0x001); /* Left DAC connected to LMIX */
		nau8822_register_write_spi(51, 0x001); /* Right DAC connected to RMIX */

		nau8822_register_write_spi(52, 0x01C); /* HP Volume */
		nau8822_register_write_spi(53, 0x11C); /* HP Volume */

		nau8822_register_write_spi(54, 0x007); /* LSPKOUT Volume */
		nau8822_register_write_spi(55, 0x107); /* LSPKOUT Volume */

		/* The PLL49MOUT bit ,Master clock select bit R6[8] and ADCB_OVER R72[5] need to be set in order to
operate in this mode. The OSR32x bit is defined in bit 1 of 0x48 register for DAC, bit 0 of 0x48 for
ADC and the PLL49MOUT is defined in bit3 of 0x48 register. The suggested OSR for 192kHz sampling
frequency is 32x.*/

		nau8822_register_write_spi(72, 0x017); // 16bit/192k

		break;

	case 1: // LMICPN RMICPN DIFERENCIAL 48KHZ

		nau8822_register_write_spi(0, 0x000);

		delay(500);

		nau8822_register_write_spi(1, 0x11F);
		nau8822_register_write_spi(2, 0x1BF); /* Enable L/R Headphone, ADC Mix/Boost, ADC */
		nau8822_register_write_spi(3, 0x16F); /* Enable L/R main mixer, DAC */

		delay(250);

		nau8822_register_write_spi(4, 0x010);  /* 16-bit word length, I2S format, Stereo */
		nau8822_register_write_spi(5, 0x000);  /* Companding control and loop back mode (all disable) */
		nau8822_register_write_spi(6, 0x00C);  // 16bit/192k
		nau8822_register_write_spi(7, 0x000);  /* 48K for internal filter coefficients */
		nau8822_register_write_spi(10, 0x008); /* DAC soft mute is disabled, DAC oversampling rate is 128x */

		nau8822_register_write_spi(14, 0x188); /* ADC HP filter is disabled, ADC oversampling rate is 128x */

		nau8822_register_write_spi(15, 0x0FF); /* ADC left digital volume control */
		nau8822_register_write_spi(16, 0x1FF); /* ADC right digital volume control */

		// nau8822_register_write_spi(11, 0x1CF); /* DAC left digital volume control */
		// nau8822_register_write_spi(12, 0x1CF); /* DAC right digital volume control */

		nau8822_register_write_spi(43, 0x010); /* RSUBBYP right speaker amplifier connected to submixer output (inverts RMIX for BTL) */

		nau8822_register_write_spi(44, 0x077); /* Conecta LMICN-P y RMICN-P al DAC */

		nau8822_register_write_spi(45, 0x11F); // Activa PGA para ADC-L
		nau8822_register_write_spi(46, 0x11F); // Activa PGA para ADC-R

		nau8822_register_write_spi(47, 0x100); // LADC a LIN 0 dB
		nau8822_register_write_spi(48, 0x100); // RADC a RIN 0 dB

		nau8822_register_write_spi(49, 0x006); // TSEN | SPKBST

		nau8822_register_write_spi(50, 0x001); /* Left DAC connected to LMIX */
		nau8822_register_write_spi(51, 0x001); /* Right DAC connected to RMIX */

		nau8822_register_write_spi(52, 0x01C); /* HP Volume */
		nau8822_register_write_spi(53, 0x11C); /* HP Volume */

		nau8822_register_write_spi(54, 0x007); /* LSPKOUT Volume */
		nau8822_register_write_spi(55, 0x107); /* LSPKOUT Volume */

		break;

	case 2: // LIN RIN 96/192KHZ

		nau8822_register_write_spi(0, 0x000);

		delay(500);

		nau8822_register_write_spi(1, 0x11F);
		nau8822_register_write_spi(2, 0x1BF); /* Enable L/R Headphone, ADC Mix/Boost, ADC */
		nau8822_register_write_spi(3, 0x16F); /* Enable L/R main mixer, DAC */

		delay(250);

		nau8822_register_write_spi(4, 0x010); /* 16-bit word length, I2S format, Stereo */
		nau8822_register_write_spi(5, 0x000); /* Companding control and loop back mode (all disable) */
		// nau8822_register_write_spi(6, 0x000); /* Divide by 2, 48K */
		nau8822_register_write_spi(6, 0x00C);  // 16bit/192k
		nau8822_register_write_spi(7, 0x000);  /* 48K for internal filter coefficients */
											   // nau8822_register_write_spi(13, 0x09f);
											   // nau8822_register_write_spi(10, 0x008); /* DAC soft mute is disabled, DAC oversampling rate is 128x */
		nau8822_register_write_spi(10, 0x000); /* DAC soft mute is disabled, DAC oversampling rate is 64x */

		nau8822_register_write_spi(14, 0x100); /* ADC HP filter is disabled, ADC oversampling rate is 64x */

		nau8822_register_write_spi(15, 0x0FF); /* ADC left digital volume control */
		nau8822_register_write_spi(16, 0x1FF); /* ADC right digital volume control */

		// nau8822_register_write_spi(11, 0x1CF); /* DAC left digital volume control */
		// nau8822_register_write_spi(12, 0x1CF); /* DAC right digital volume control */

		nau8822_register_write_spi(43, 0x010); /* RSUBBYP right speaker amplifier connected to submixer output (inverts RMIX for BTL) */

		nau8822_register_write_spi(44, 0x000);

		nau8822_register_write_spi(45, 0x070); // PGA para ADC
		nau8822_register_write_spi(46, 0x170);

		nau8822_register_write_spi(47, 0x050); // LADC a LIN 0 dB
		nau8822_register_write_spi(48, 0x050); // RADC a RIN 0 dB

		nau8822_register_write_spi(49, 0x006); // TSEN | SPKBST

		nau8822_register_write_spi(50, 0x001); /* Left DAC connected to LMIX */
		nau8822_register_write_spi(51, 0x001); /* Right DAC connected to RMIX */

		nau8822_register_write_spi(52, 0x01C); /* HP Volume */
		nau8822_register_write_spi(53, 0x11C); /* HP Volume */

		nau8822_register_write_spi(54, 0x107); /* LSPKOUT Volume */
		nau8822_register_write_spi(55, 0x107); /* LSPKOUT Volume */

		nau8822_register_write_spi(72, 0x013); // 16bit/192k

		break;

	case 3:

		nau8822_register_write_spi(0, 0x000);

		delay(500);

		nau8822_register_write_spi(1, 0x11F);
		nau8822_register_write_spi(2, 0x1BF); /* Enable L/R Headphone, ADC Mix/Boost, ADC */
		nau8822_register_write_spi(3, 0x16F); /* Enable L/R main mixer, DAC */

		delay(250);

		nau8822_register_write_spi(4, 0x010);  /* 16-bit word length, I2S format, Stereo */
		nau8822_register_write_spi(5, 0x000);  /* Companding control and loop back mode (all disable) */
		nau8822_register_write_spi(6, 0x00C);  // 16bit/192k
		nau8822_register_write_spi(7, 0x000);  /* 48K for internal filter coefficients */
		nau8822_register_write_spi(10, 0x000); /* DAC soft mute is disabled, DAC oversampling rate is 128x */

		nau8822_register_write_spi(14, 0x100); /* ADC HP filter is disabled, ADC oversampling rate is 128x */

		nau8822_register_write_spi(15, 0x0FF); /* ADC left digital volume control */
		nau8822_register_write_spi(16, 0x1FF); /* ADC right digital volume control */

		nau8822_register_write_spi(11, 0x0FF); /* DAC left digital volume control */
		nau8822_register_write_spi(12, 0x1FF); /* DAC right digital volume control */

		nau8822_register_write_spi(43, 0x010); /* RSUBBYP right speaker amplifier connected to submixer output (inverts RMIX for BTL) */

		nau8822_register_write_spi(44, 0x000);

		nau8822_register_write_spi(45, 0x070); // PGA para ADC
		nau8822_register_write_spi(46, 0x170);

		nau8822_register_write_spi(47, 0x050); // LADC a LIN 0 dB
		nau8822_register_write_spi(48, 0x050); // RADC a RIN 0 dB

		nau8822_register_write_spi(49, 0x006); // TSEN | SPKBST

		nau8822_register_write_spi(50, 0x001); /* Left DAC connected to LMIX */
		nau8822_register_write_spi(51, 0x001); /* Right DAC connected to RMIX */

		nau8822_register_write_spi(52, 0x01C); /* HP Volume */
		nau8822_register_write_spi(53, 0x11C); /* HP Volume */

		nau8822_register_write_spi(54, 0x107); /* LSPKOUT Volume */
		nau8822_register_write_spi(55, 0x107); /* LSPKOUT Volume */

		nau8822_register_write_spi(72, 0x010); // 16bit/96k

		break;

	case 4: // 192KHZ PLL

		nau8822_register_write_spi(0, 0x000); /* Reset all registers */

		delay(500);

		nau8822_register_write_spi(36, 0x018); // MCLK SCALER Y PLL INT
		nau8822_register_write_spi(37, 0x03B); // K1
		nau8822_register_write_spi(38, 0x1E6); // K2
		nau8822_register_write_spi(39, 0x15B); // K3

		nau8822_register_write_spi(1, 0x02F);
		nau8822_register_write_spi(2, 0x1B3); /* Enable L/R Headphone, ADC Mix/Boost, ADC */
		nau8822_register_write_spi(3, 0x16F); /* Enable L/R main mixer, DAC */

		// offset: 0x4 => default, 24bit, I2S format, Stereo
		nau8822_register_write_spi(4, 0x010); /* 16-bit word length, I2S format, Stereo */

		nau8822_register_write_spi(5, 0x000); /* Companding control and loop back mode (all disable) */
		nau8822_register_write_spi(6, 0x14D); /* Divide by 6, 16K */

		nau8822_register_write_spi(7, 0x000); /* 16K for internal filter coefficients */

		nau8822_register_write_spi(10, 0x008); /* DAC soft mute is disabled, DAC oversampling rate is 128x */
		nau8822_register_write_spi(14, 0x108); /* ADC HP filter is disabled, ADC oversampling rate is 128x */

		nau8822_register_write_spi(15, 0x0FF); /* ADC left digital volume control */
		nau8822_register_write_spi(16, 0x1FF); /* ADC right digital volume control */

		// nau8822_register_write_spi(11, 0x1CF); /* DAC left digital volume control */
		// nau8822_register_write_spi(12, 0x1CF); /* DAC right digital volume control */

		nau8822_register_write_spi(43, 0x010); /* RSUBBYP right speaker amplifier connected to submixer output (inverts RMIX for BTL) */

		nau8822_register_write_spi(44, 0x077); /* Conecta LMICN-P y RMICN-P al DAC */

		nau8822_register_write_spi(45, 0x11F); // Activa PGA para ADC-L
		nau8822_register_write_spi(46, 0x11F); // Activa PGA para ADC-R

		nau8822_register_write_spi(47, 0x100); // LADC a LIN 0 dB
		nau8822_register_write_spi(48, 0x100); // RADC a RIN 0 dB

		nau8822_register_write_spi(49, 0x006); // TSEN | SPKBST

		nau8822_register_write_spi(50, 0x001); /* Left DAC connected to LMIX */
		nau8822_register_write_spi(51, 0x001); /* Right DAC connected to RMIX */

		nau8822_register_write_spi(52, 0x01C); /* HP Volume */
		nau8822_register_write_spi(53, 0x11C); /* HP Volume */

		nau8822_register_write_spi(54, 0x007); /* LSPKOUT Volume */
		nau8822_register_write_spi(55, 0x107); /* LSPKOUT Volume */

		nau8822_register_write_spi(72, 0x017); // PARA 192K

		break;

	case 5: // 48KHZ PLL

		nau8822_register_write_spi(0, 0x000); /* Reset all registers */

		delay(500);

		nau8822_register_write_spi(36, 0x018); // MCLK SCALER Y PLL INT
		nau8822_register_write_spi(37, 0x03B); // K1
		nau8822_register_write_spi(38, 0x1E6); // K2
		nau8822_register_write_spi(39, 0x15B); // K3

		nau8822_register_write_spi(1, 0x02F);
		nau8822_register_write_spi(2, 0x1B3); /* Enable L/R Headphone, ADC Mix/Boost, ADC */
		nau8822_register_write_spi(3, 0x16F); /* Enable L/R main mixer, DAC */

		// offset: 0x4 => default, 24bit, I2S format, Stereo
		nau8822_register_write_spi(4, 0x010); /* 16-bit word length, I2S format, Stereo */

		nau8822_register_write_spi(5, 0x000); /* Companding control and loop back mode (all disable) */
		nau8822_register_write_spi(6, 0x14D); /* Divide by 6, 16K */

		nau8822_register_write_spi(7, 0x000); /* 16K for internal filter coefficients */

		nau8822_register_write_spi(10, 0x008); /* DAC soft mute is disabled, DAC oversampling rate is 128x */
		nau8822_register_write_spi(14, 0x108); /* ADC HP filter is disabled, ADC oversampling rate is 128x */

		nau8822_register_write_spi(15, 0x0FF); /* ADC left digital volume control */
		nau8822_register_write_spi(16, 0x1FF); /* ADC right digital volume control */

		// nau8822_register_write_spi(11, 0x1CF); /* DAC left digital volume control */
		// nau8822_register_write_spi(12, 0x1CF); /* DAC right digital volume control */

		nau8822_register_write_spi(43, 0x010); /* RSUBBYP right speaker amplifier connected to submixer output (inverts RMIX for BTL) */

		nau8822_register_write_spi(44, 0x077); /* Conecta LMICN-P y RMICN-P al DAC */

		nau8822_register_write_spi(45, 0x11F); // Activa PGA para ADC-L
		nau8822_register_write_spi(46, 0x11F); // Activa PGA para ADC-R

		nau8822_register_write_spi(47, 0x100); // LADC a LIN 0 dB
		nau8822_register_write_spi(48, 0x100); // RADC a RIN 0 dB

		nau8822_register_write_spi(49, 0x006); // TSEN | SPKBST

		nau8822_register_write_spi(50, 0x001); /* Left DAC connected to LMIX */
		nau8822_register_write_spi(51, 0x001); /* Right DAC connected to RMIX */

		nau8822_register_write_spi(52, 0x01C); /* HP Volume */
		nau8822_register_write_spi(53, 0x11C); /* HP Volume */

		nau8822_register_write_spi(54, 0x007); /* LSPKOUT Volume */
		nau8822_register_write_spi(55, 0x107); /* LSPKOUT Volume */

		break;

	case 6: // 96KHZ PLL

		nau8822_register_write_spi(0, 0x000); /* Reset all registers */

		delay(500);

		nau8822_register_write_spi(36, 0x018); // MCLK SCALER Y PLL INT
		nau8822_register_write_spi(37, 0x03B); // K1
		nau8822_register_write_spi(38, 0x1E6); // K2
		nau8822_register_write_spi(39, 0x15B); // K3

		nau8822_register_write_spi(1, 0x02F);
		nau8822_register_write_spi(2, 0x1B3); /* Enable L/R Headphone, ADC Mix/Boost, ADC */
		nau8822_register_write_spi(3, 0x16F); /* Enable L/R main mixer, DAC */

		// offset: 0x4 => default, 24bit, I2S format, Stereo
		nau8822_register_write_spi(4, 0x010); /* 16-bit word length, I2S format, Stereo */

		nau8822_register_write_spi(5, 0x000); /* Companding control and loop back mode (all disable) */
		nau8822_register_write_spi(6, 0x10D); /* Divide by 6, 16K */

		nau8822_register_write_spi(7, 0x000); /* 16K for internal filter coefficients */

		nau8822_register_write_spi(10, 0x000); /* DAC soft mute is disabled, DAC oversampling rate is 128x */
		nau8822_register_write_spi(14, 0x100); /* ADC HP filter is disabled, ADC oversampling rate is 128x */

		nau8822_register_write_spi(15, 0x0FF); /* ADC left digital volume control */
		nau8822_register_write_spi(16, 0x1FF); /* ADC right digital volume control */

		// nau8822_register_write_spi(11, 0x1CF); /* DAC left digital volume control */
		// nau8822_register_write_spi(12, 0x1CF); /* DAC right digital volume control */

		nau8822_register_write_spi(43, 0x010); /* RSUBBYP right speaker amplifier connected to submixer output (inverts RMIX for BTL) */

		nau8822_register_write_spi(44, 0x077); /* Conecta LMICN-P y RMICN-P al DAC */

		nau8822_register_write_spi(45, 0x11F); // Activa PGA para ADC-L
		nau8822_register_write_spi(46, 0x11F); // Activa PGA para ADC-R

		nau8822_register_write_spi(47, 0x100); // LADC a LIN 0 dB
		nau8822_register_write_spi(48, 0x100); // RADC a RIN 0 dB

		nau8822_register_write_spi(49, 0x006); // TSEN | SPKBST

		nau8822_register_write_spi(50, 0x001); /* Left DAC connected to LMIX */
		nau8822_register_write_spi(51, 0x001); /* Right DAC connected to RMIX */

		nau8822_register_write_spi(52, 0x01C); /* HP Volume */
		nau8822_register_write_spi(53, 0x11C); /* HP Volume */

		nau8822_register_write_spi(54, 0x007); /* LSPKOUT Volume */
		nau8822_register_write_spi(55, 0x107); /* LSPKOUT Volume */

		nau8822_register_write_spi(72, 0x010); // PARA 96k

		break;

	case 7: // LIN RIN

		nau8822_register_write_spi(44, 0x000); // CVonecta LIN y RIN sólo al PGA

		nau8822_register_write_spi(45, 0x050); // PGA para ADC
		nau8822_register_write_spi(46, 0x050);

		nau8822_register_write_spi(47, 0x040); // LADC a LIN 0 dB
		nau8822_register_write_spi(48, 0x040); // RADC a RIN 0 dB

		nau8822_register_write_spi(50, 0x001); /* Left DAC connected to LMIX */
		nau8822_register_write_spi(51, 0x001); /* Right DAC connected to RMIX */

		break;

	case 8: // MICR MICL PN

		nau8822_register_write_spi(44, 0x033); /* Conecta LMICN-P y RMICN-P al PGA */

		nau8822_register_write_spi(45, 0x11F); // Activa PGA para ADC-L
		nau8822_register_write_spi(46, 0x11F); // Activa PGA para ADC-R

		nau8822_register_write_spi(47, 0x000); // LADC a LIN 0 dB
		nau8822_register_write_spi(48, 0x000); // RADC a RIN 0 dB

		nau8822_register_write_spi(50, 0x001); /* Left DAC connected to LMIX */
		nau8822_register_write_spi(51, 0x001); /* Right DAC connected to RMIX */

		break;
	}

#endif

	return 0;
}

int nau8822_calc_pll(unsigned int pll_in, unsigned int fs)
{
	uint64_t f2, f2_max, pll_ratio;
	int i, scal_sel;

	if (pll_in > NAU_PLL_REF_MAX || pll_in < NAU_PLL_REF_MIN)
		Serial.println("ERROR REF MAX MIN PLL");
	f2_max = 0;
	scal_sel = ARRAY_SIZE(nau8822_mclk_scaler);

	for (i = 0; i < scal_sel; i++)
	{
		f2 = 256 * fs * 4 * nau8822_mclk_scaler[i] / 10;
		if (f2 > NAU_PLL_FREQ_MIN && f2 < NAU_PLL_FREQ_MAX &&
			f2_max < f2)
		{
			f2_max = f2;
			scal_sel = i;
		}
	}

	if (ARRAY_SIZE(nau8822_mclk_scaler) == scal_sel)
		Serial.println("ERROR Generando PLL NAU8822 scaler");
	pll_param.mclk_scaler = scal_sel;
	f2 = f2_max;

	/* Calculate the PLL 4-bit integer input and the PLL 24-bit fractional
	 * input; round up the 24+4bit.
	 */
	pll_ratio = (f2 << 28) / (pll_in);
	pll_param.pre_factor = 0;
	if (((pll_ratio >> 28) & 0xF) < NAU_PLL_OPTOP_MIN)
	{
		pll_ratio <<= 1;
		pll_param.pre_factor = 1;
	}
	pll_param.pll_int = (pll_ratio >> 28) & 0xF;
	pll_param.pll_frac = ((pll_ratio & 0xFFFFFFF) >> 4);

#define NAU8822_REG_PLL_K1 0x25
#define NAU8822_REG_PLL_K2 0x26
#define NAU8822_REG_PLL_K3 0x27

	int clocking = (NAU8822_CLKM_MASK, NAU8822_CLKM_PLL, pll_param.mclk_scaler << NAU8822_MCLKSEL_SFT);
	int entero = (NAU8822_PLLMCLK_DIV2 | NAU8822_PLLN_MASK, (pll_param.pre_factor ? NAU8822_PLLMCLK_DIV2 : 0) | pll_param.pll_int);
	int k1 = (pll_param.pll_frac >> NAU8822_PLLK1_SFT) & NAU8822_PLLK1_MASK;
	int k2 = (pll_param.pll_frac >> NAU8822_PLLK2_SFT) & NAU8822_PLLK2_MASK;
	int k3 = (pll_param.pll_frac & NAU8822_PLLK3_MASK);

	Serial.printf("REGISTRO 6 PRE PLL: %3x \r\n", clocking);
	Serial.printf("REGISTRO 36: %3x \r\n", entero);
	Serial.printf("MCLK SCALER: %3x PRE: %3x INT: %3x FRAC: %6x \r\n", pll_param.mclk_scaler, pll_param.pre_factor, pll_param.pll_int, pll_param.pll_frac);
	Serial.printf("MCLK SCALER: %3x PRE: %3x INT: %3x K1: %3x K2: %3x K3: %3x \r\n", pll_param.mclk_scaler, pll_param.pre_factor, pll_param.pll_int, k1, k2, k3);

	return 0;
}
