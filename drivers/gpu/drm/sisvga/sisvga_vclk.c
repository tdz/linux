/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 */

#include "sisvga_vclk.h"
#include <linux/bug.h>
#include <linux/errno.h>
#include <linux/kernel.h>

struct sisvga_clk_config {
	enum sisvga_freq freq;
	unsigned char num;
	unsigned char denum;
	unsigned char div;
	unsigned char postscal;
	int clock_hz;
};

#define SISVGA_CLK_CONFIG(description, f, n, dn, d, p, c) \
	.freq = (SISVGA_FREQ_ ## f), \
	.num = (n), \
	.denum = (dn), \
	.div = (d), \
	.postscal = (p), \
	.clock_hz = (c)

/* On all SiS adapters we have to configure the internal dot-clock
 * generator. According to its manual on the SiS 6326 we can configure
 * VGA clock generators in the same way. In practice hardware doesn't
 * support this. So we only use VGA registers for VGA dot clocks, and
 * extended registers for the internal clock generator.
 *
 * The config table for the SiS 6326 replaces VGA clock generators,
 * with the internal one, even if the VGA clock generator would produce
 * better results.
 */
static const struct sisvga_clk_config sisvga_vclk_configs[] = {
	/* Any VGA */
	{ SISVGA_CLK_CONFIG( "25.175 MHz", 25175,   1,  1, 1, 1,  25175000) },
	{ SISVGA_CLK_CONFIG( "28.322 MHz", 28322,   1,  1, 1, 1,  28322000) },
	/* SiS 6202 and later */
	{ SISVGA_CLK_CONFIG( "30.000 MHz", 14318,  22, 21, 2, 1,  29999996) },
	{ SISVGA_CLK_CONFIG( "31.500 MHz", 14318,  11,  5, 1, 1,  31499996) },
	//{ SISVGA_CLK_CONFIG( "36.000 MHz", 25175,   5,  7, 2, 1,  35964285) },
	{ SISVGA_CLK_CONFIG( "36.000 MHz", 14318,  83, 11, 1, 3,  36012392) },
	{ SISVGA_CLK_CONFIG( "40.000 MHz", 14318,  88, 21, 2, 3,  39999994) },
	//{ SISVGA_CLK_CONFIG( "44.889 MHz", 25175,  41, 23, 1, 1,  44877173) },
	{ SISVGA_CLK_CONFIG( "44.889 MHz", 14318,  47, 15, 1, 1,  44863630) },
	{ SISVGA_CLK_CONFIG( "44.900 MHz", 14318, 127, 27, 2, 3,  44898984) },
	{ SISVGA_CLK_CONFIG( "50.000 MHz", 14318, 110, 21, 2, 3,  49999993) },
	{ SISVGA_CLK_CONFIG( "56.300 MHz", 14318,  59, 15, 1, 1,  56318174) },
	{ SISVGA_CLK_CONFIG( "65.000 MHz", 14318,  59, 13, 1, 1,  64982509) },
	{ SISVGA_CLK_CONFIG( "75.000 MHz", 14318,  55, 21, 2, 1,  74999990) },
	{ SISVGA_CLK_CONFIG( "77.000 MHz", 14318, 121, 15, 2, 3,  76999990) },
	{ SISVGA_CLK_CONFIG( "80.000 MHz", 14318,  95, 17, 1, 1,  80013358) },
	{ SISVGA_CLK_CONFIG( "94.500 MHz", 14318,  33,  5, 1, 1,  94499988) },
	{ SISVGA_CLK_CONFIG("110.000 MHz", 14318,  73, 19, 2, 1, 110023909) },
	{ SISVGA_CLK_CONFIG("120.000 MHz", 14318,  88, 21, 2, 1, 119999984) },
	{ SISVGA_CLK_CONFIG("130.000 MHz", 14318,  59, 13, 2, 1, 129965018) },
	/* SiS 6215 and later */
	{ SISVGA_CLK_CONFIG("135.000 MHz", 14318,  33,  7, 2, 1, 134999982) },
	/* SiS 6326 and later */
	//{ SISVGA_CLK_CONFIG("162.000 MHz", 25175,  74, 23, 2, 1, 161995652) },
	{ SISVGA_CLK_CONFIG("162.000 MHz", 14318,  17,  3, 2, 1, 162272706) },
	{ SISVGA_CLK_CONFIG("175.500 MHz", 14318,  49,  4, 1, 1, 175397705) },
};

int sisvga_vclk_of_clock(unsigned int clock_khz, enum sisvga_vclk *vclk_out)
{
	switch (clock_khz) {
	case 25000:	/* fall through */
	case 25175:
		*vclk_out = SISVGA_VCLK_25175;
		return 0;
	case 28000:	/* fall through */
	case 28322:
		*vclk_out = SISVGA_VCLK_28322;
		return 0;
	case 30000:
		*vclk_out = SISVGA_VCLK_30000;
		return 0;
	case 31500:
		*vclk_out = SISVGA_VCLK_31500;
		return 0;
	case 36000:
		*vclk_out = SISVGA_VCLK_36000;
		return 0;
	case 40000:
		*vclk_out = SISVGA_VCLK_40000;
		return 0;
	case 44889:
		*vclk_out = SISVGA_VCLK_44889;
		return 0;
	case 44900:
		*vclk_out = SISVGA_VCLK_44900;
		return 0;
	case 50000:
		*vclk_out = SISVGA_VCLK_50000;
		return 0;
	case 56300:
		*vclk_out = SISVGA_VCLK_56300;
		return 0;
	case 65000:
		*vclk_out = SISVGA_VCLK_65000;
		return 0;
	case 75000:
		*vclk_out = SISVGA_VCLK_75000;
		return 0;
	case 77000:
		*vclk_out = SISVGA_VCLK_77000;
		return 0;
	case 80000:
		*vclk_out = SISVGA_VCLK_80000;
		return 0;
	case 94500:
		*vclk_out = SISVGA_VCLK_94500;
		return 0;
	case 110000:
		*vclk_out = SISVGA_VCLK_110000;
		return 0;
	case 120000:
		*vclk_out = SISVGA_VCLK_120000;
		return 0;
	case 130000:
		*vclk_out = SISVGA_VCLK_130000;
		return 0;
	case 135000:
		*vclk_out = SISVGA_VCLK_135000;
		return 0;
	case 162000:
		*vclk_out = SISVGA_VCLK_162000;
		return 0;
	case 175500:
		*vclk_out = SISVGA_VCLK_175500;
		return 0;
	default:
		return -EINVAL;
	}
}

void sisvga_vclk_regs(enum sisvga_vclk vclk, enum sisvga_freq *freq_out,
		      unsigned long *num_out,
		      unsigned long *denum_out,
		      unsigned long *div_out,
		      unsigned long *postscal_out,
	              unsigned long *f_out)
{
	BUG_ON(!(vclk < ARRAY_SIZE(sisvga_vclk_configs)));
	BUG_ON(sisvga_vclk_configs[vclk].freq > 2);
	BUG_ON(sisvga_vclk_configs[vclk].num < 1);
	BUG_ON(sisvga_vclk_configs[vclk].num > 128);
	BUG_ON(sisvga_vclk_configs[vclk].denum < 1);
	BUG_ON(sisvga_vclk_configs[vclk].denum > 32);
	BUG_ON(sisvga_vclk_configs[vclk].div < 1);
	BUG_ON(sisvga_vclk_configs[vclk].div > 2);
	BUG_ON(sisvga_vclk_configs[vclk].postscal < 1);
	BUG_ON(sisvga_vclk_configs[vclk].postscal > 8);

	*freq_out = sisvga_vclk_configs[vclk].freq;
	*num_out = sisvga_vclk_configs[vclk].num;
	*denum_out = sisvga_vclk_configs[vclk].denum;
	*div_out = sisvga_vclk_configs[vclk].div;
	*postscal_out = sisvga_vclk_configs[vclk].postscal;
	*f_out = sisvga_vclk_configs[vclk].clock_hz;
}
