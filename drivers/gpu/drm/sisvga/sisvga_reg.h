/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#ifndef SISVGA_REG_H
#define SISVGA_REG_H

#include <linux/delay.h>

/* When mapped, I/O ports have an offset from their address in the port
 * address space.
 *
 * FIXME: Is this value configurable?
 */
#define IOPORTS_OFFSET	0x380

#ifndef SISVGA_REGS
#error Define SISVGA_REGS to point to the memory-mapped I/O ports. Usually \
       you simply have to include "sisvga_device.h" before this header file.
#endif

/* Helper macros */

#define _REG_OFFSET(reg) \
	((reg) - IOPORTS_OFFSET)

#define _REG_ADDR(reg) \
	(((uint8_t __iomem *)(SISVGA_REGS)) + _REG_OFFSET(reg))

#define RREG8(reg) ioread8(_REG_ADDR(reg))

#define WREG8(reg, v) 					\
	iowrite8((v), _REG_ADDR(reg))

#define WAIT8(reg, v)					\
	do {						\
		u16 eval_reg = (reg);			\
		u8 eval_v = (v);			\
		do {					\
			u8 tmp = RREG8(eval_reg);	\
			if (tmp == eval_v) {		\
				break;			\
			}				\
			udelay(10);			\
		} while (1);				\
	} while (0)

#define RREG8_MASKED(reg, mask)				\
	( RREG8(reg) & (mask) );			\

#define WREG8_MASKED(reg, v, mask)			\
	do {						\
		u16 eval_reg = (reg);			\
		u8 eval_v = (v);			\
		u8 eval_mask = (mask);			\
		u8 v = RREG8(reg, ~(eval_mask));	\
		WREG8(eval_reg,				\
		      v | (eval_v & eval_mask));	\
	} while (0)

#define WAIT8_MASKED(reg, v, mask)			\
	do {						\
		u16 eval_reg = (reg);			\
		u8 eval_mask = (mask);			\
		u8 eval_v = (v) & eval_mask;		\
		do {					\
			u8 tmp = RREG8_MASKED(		\
				eval_reg, eval_mask);	\
			if (tmp == eval_v) {		\
				break;			\
			}				\
			udelay(10);			\
		} while (1);				\
	} while (0)

#define RREG_I(index, reg, data, v)			\
	do {						\
		WREG8(index, reg);			\
		v = RREG8(data);			\
	} while (0)

#define WREG_I(index, reg, data, v)			\
	do {						\
		WREG8(index, reg);			\
		WREG8(data, (v));			\
	} while (0)

#define WAIT_I(index, reg, data, v)			\
	do {						\
		WREG8(index, reg);			\
		WAIT8(data, v);				\
	} while (0)

#define WAIT_I_MASKED(index, reg, data, v, mask)	\
	do {						\
		WREG8(index, reg);			\
		WAIT8_MASKED(data, v, mask);		\
	} while (0)

/* General registers  */

#define REG_MISC_IN		0x3cc
#define REG_MISC_OUT		0x3c2

#define REG_FEAT_IN		0x3ca
#define REG_FEAT_OUT		0x3da

#define REG_INPUT0		0x3c0
#define REG_INPUT1		0x3da

#define REG_VGA			0x3c3

#define REG_SEGSEL0		0x3cd
#define REG_SEGSEL1		0x3cb

/* CRT Controller registers */

#define REG_CRI			0x3d4
#define REG_CR			0x3d5

#define RREG_CR(reg, v)		RREG_I(REG_CRI, reg, REG_CR, v)
#define WREG_CR(reg, v)		WREG_I(REG_CRI, reg, REG_CR, v)
#define WAIT_CR(reg, v)		WAIT_I(REG_CRI, reg, REG_CR, v)
#define WAIT_CR_MASKED(reg, v, mask) \
				WAIT_I_MASKED(REG_CRI, reg, REG_CR, v, mask)

/* Sequencer and Extended registers */

#define REG_SRI			0x3c4
#define REG_SR			0x3c5

#define RREG_SR(reg, v)		RREG_I(REG_SRI, reg, REG_SR, v)
#define WREG_SR(reg, v)		WREG_I(REG_SRI, reg, REG_SR, v)
#define WAIT_SR(reg, v)		WAIT_I(REG_SRI, reg, REG_SR, v)
#define WAIT_SR_MASKED(reg, v, mask) \
				WAIT_I_MASKED(REG_SRI, reg, REG_SR, v, mask)

/* Graphics Controller registers */

#define REG_GRI			0x3ce
#define REG_GR			0x3cf

#define RREG_GR(reg, v)		RREG_I(REG_GRI, reg, REG_GR, v)
#define WREG_GR(reg, v)		WREG_I(REG_GRI, reg, REG_GR, v)
#define WAIT_GR(reg, v)		WAIT_I(REG_GRI, reg, REG_GR, v)
#define WAIT_GR_MASKED(reg, v, mask) \
				WAIT_I_MASKED(REG_GRI, reg, REG_GR, v, mask)

/* Attribute Controller registers */

#define REG_ARI			0x3c0
#define REG_AR_IN		0x3c1
#define REG_AR_OUT		0x3c0

#if defined(SISVGA_DEBUG) && SISVGA_DEBUG
/* Interrupts likely check the status of the vertical retrace bit in
 * the input register 1. This will interfere with concurrently running
 * access to the attribute registers. Here's an optional check to make
 * sure interrupts are disabled while accessing attribute registers. */
#define _xREG_AR_CHECK_INTR				\
	if (SISVGA_DEBUG) {				\
		u8 cr11;				\
		RREG_CR(0x11, cr11);			\
		if (cr11 & 0x20)			\
			DRM_ERROR("sisvga: Accessing attribute registers with interupts enabled is unsave\n");	\
	}
#else
#define _xREG_AR_CHECK_INTR
#endif

#define RREG_AR(reg, v)					\
	do {						\
		_xREG_AR_CHECK_INTR			\
		RREG8(REG_INPUT1);			\
		WREG8(REG_ARI, reg);			\
		v = RREG8(REG_AR_IN);			\
	} while (0)

#define WREG_AR(reg, v)					\
	do {						\
		_xREG_AR_CHECK_INTR			\
		RREG8(REG_INPUT1);			\
		WREG8(REG_ARI, reg);			\
		WREG8(REG_AR_OUT, v);			\
	} while (0)

/* Color registers */

#define REG_DACS		0x3c7
#define REG_DACI_IN		0x3c7
#define REG_DACI_OUT		0x3c8
#define REG_DAC			0x3c9

#define REG_PEL			0x3c6

#define RREG_DAC(reg, r, g, b)				\
	do {						\
		WREG8(REG_DACI_IN, reg);		\
		r = RREG8(REG_DAC);			\
		g = RREG8(REG_DAC);			\
		b = RREG8(REG_DAC);			\
	} while (0)

#define WREG_DAC(reg, r, g, b)				\
	do {						\
		WREG8(REG_DACI_OUT, reg);		\
		WREG8(REG_DAC, r);			\
		WREG8(REG_DAC, g);			\
		WREG8(REG_DAC, b);			\
	} while (0)

#endif /* SISVGA_REG_H */
