#ifndef nau8822_h__
#define nau8822_h__
#include "Arduino.h"


#include <nau8822_code/nau8822_map_reg.h>
#include <nau8822_code/nau8822_types.h>


#define NAU_PLL_FREQ_MAX 100000000
#define NAU_PLL_FREQ_MIN 90000000
#define NAU_PLL_REF_MAX 33000000
#define NAU_PLL_REF_MIN 8000000
#define NAU_PLL_OPTOP_MIN 6

/* NAU8822_REG_PLL_N (0x24) */
#define NAU8822_PLLMCLK_DIV2			(0x1 << 4)
#define NAU8822_PLLN_MASK			0xF

#define NAU8822_PLLK1_SFT			18
#define NAU8822_PLLK1_MASK			0x3F

/* NAU8822_REG_PLL_K2 (0x26) */
#define NAU8822_PLLK2_SFT			9
#define NAU8822_PLLK2_MASK			0x1FF

/* NAU8822_REG_PLL_K3 (0x27) */
#define NAU8822_PLLK3_MASK			0x1FF

#define NAU8822_MCLKSEL_SFT			5

#define NAU8822_CLKM_MASK			(0x1 << 8)
#define NAU8822_CLKM_PLL			(0x1 << 8)

#define ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))

struct nau8822_pll {
	int pre_factor;
	int mclk_scaler;
	int pll_frac;
	int pll_int;
	int freq_in;
	int freq_out;
};

uint8_t nau8822_init(int modo);
void nau8822_spk_volume(uint8_t volume);
void i2c_scan(void);
int nau8822_calc_pll(unsigned int pll_in, unsigned int fs);

#endif // nau8822_h__