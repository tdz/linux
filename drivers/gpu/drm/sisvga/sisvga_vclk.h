/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 */

#ifndef SISVGA_VCLK_H
#define SISVGA_VCLK_H

enum sisvga_freq {
	SISVGA_FREQ_14318 = 0, /* from SiS internal clock generator */
        SISVGA_FREQ_25175,
        SISVGA_FREQ_28322
};

enum sisvga_vclk {
	/* Any VGA */
	SISVGA_VCLK_25175,
	SISVGA_VCLK_28322,
	/* SiS 6202 and later */
	SISVGA_VCLK_30000,
	SISVGA_VCLK_31500,
	SISVGA_VCLK_36000,
	SISVGA_VCLK_40000,
	SISVGA_VCLK_44889,
	SISVGA_VCLK_44900,
	SISVGA_VCLK_50000,
	SISVGA_VCLK_56300,
	SISVGA_VCLK_65000,
	SISVGA_VCLK_75000,
	SISVGA_VCLK_77000,
	SISVGA_VCLK_80000,
	SISVGA_VCLK_94500,
	SISVGA_VCLK_110000,
	SISVGA_VCLK_120000,
	SISVGA_VCLK_130000,
	/* SiS 6215 and later */
	SISVGA_VCLK_135000,
	/* SiS 6326 and later */
	SISVGA_VCLK_162000,
	SISVGA_VCLK_175500
};

#define KHZ_TO_HZ(_khz)		((_khz) * 1000)
#define MHZ_TO_KHZ(_mhz)	((_mhz) * 1000)
#define MHZ_TO_HZ(_mhz)		KHZ_TO_HZ(MHZ_TO_KHZ(_mhz))

int sisvga_vclk_of_clock(unsigned int clock_khz, enum sisvga_vclk *vclk_out);

void sisvga_vclk_regs(enum sisvga_vclk vclk, enum sisvga_freq *freq_out,
		      unsigned long *num_out,
		      unsigned long *denum_out,
		      unsigned long *div_out,
		      unsigned long *postscal_out,
	              unsigned long *f_out);

#endif
