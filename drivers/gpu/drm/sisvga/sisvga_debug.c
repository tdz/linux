/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#include "sisvga_debug.h"
#include "sisvga_device.h"
#include "sisvga_reg.h"

void sisvga_debug_print_mode(struct sisvga_device* sdev)
{
	struct drm_display_mode mode;

	int dots, htotal, hsync_start, hsync_end, hdisplay, hskew, hblank_start,
	    hblank_end, vtotal, vsync_start, vsync_end, vdisplay, vscan,
	    vblank_start, vblank_end;
	int fr, fd, num, denum, div, postscal;

	u8 misc;
	u8 sr01, sr05, sr12, sr13, sr0a, sr2a, sr2b, sr38;
	u8 cr00, cr01, cr02, cr03, cr04, cr05, cr06, cr07, cr09,
	   cr10, cr11, cr12, cr15, cr16;
	u8 saved_sr38, freqi;

	misc = RREG8(REG_MISC_IN);

	RREG_SR(0x01, sr01);
	RREG_SR(0x05, sr05);
	RREG_SR(0x12, sr12);
	RREG_SR(0x0a, sr0a);

	RREG_CR(0x00, cr00);
	RREG_CR(0x01, cr01);
	RREG_CR(0x02, cr02);
	RREG_CR(0x03, cr03);
	RREG_CR(0x04, cr04);
	RREG_CR(0x05, cr05);
	RREG_CR(0x06, cr06);
	RREG_CR(0x07, cr07);
	RREG_CR(0x09, cr09);
	RREG_CR(0x10, cr10);
	RREG_CR(0x11, cr11);
	RREG_CR(0x12, cr12);
	RREG_CR(0x15, cr15);
	RREG_CR(0x16, cr16);

	if (sr01 & 0x01)
		dots = 8;
	else
		dots = 9;

	htotal = (((u32)sr12 & 0x01) << 8) | cr00;
	hblank_start = (((u32)sr12 & 0x04) << 6) | cr02;
	hblank_end = (((u32)sr12 & 0x10) << 4) |
		     (((u32)sr05 & 0x80) >> 2) | (cr03 & 0x1f);
	hdisplay = (((u32)sr12 & 0x02) << 7) | cr01;
	hsync_start = (((u32)sr12 & 0x08) << 5) | cr04;
	hsync_end = (((u32)sr12 & 0x08) << 5) | (cr04 & 0xe0) | (cr05 & 0x1f);
	hskew = ((cr05 & 0x60) >> 5);

	vtotal = (((u32)sr0a & 0x01) << 9) |
		 (((u32)cr07 & 0x20) << 4) |
		 (((u32)cr07 & 0x01) << 8) | cr06;
	vblank_start = (((u32)sr0a & 0x04) << 7) |
		       (((u32)cr09 & 0x20) << 4) |
		       (((u32)cr07 & 0x08) << 5) | cr15;
	vblank_end = vblank_start + (cr16 & 0x1f);
	vdisplay = (((u32)sr0a & 0x02) << 8) |
		   (((u32)cr07 & 0x02) << 7) |
		   (((u32)cr07 & 0x40) << 3) | cr12;
	vsync_start = (((u32)sr0a & 0x08) << 6) |
		      (((u32)cr07 & 0x80) << 2) |
		      (((u32)cr07 & 0x04) << 6) | cr10;
	vsync_end = (((u32)sr0a & 0x08) << 6) |
		    (((u32)cr07 & 0x80) << 2) |
		    (((u32)cr07 & 0x04) << 6) | (cr10 & 0xf0) | (cr11 & 0x0f);
	vscan = cr09 & 0x1f;

	memset(&mode, 0, sizeof(mode));
	mode.type = DRM_MODE_TYPE_DRIVER;
	mode.htotal = (htotal + 5) * dots;
	/*mode.hblank_start = (hblank_start + 1) * dots;
	mode.hblank_end = (hblank_end + 1) * dots;*/
	mode.hdisplay = (hdisplay + 1) * dots;
	mode.hsync_start = (hsync_start + 1) * dots;
	mode.hsync_end = (hsync_end + 1) * dots;
	mode.hskew = hskew * dots;

	mode.vtotal = vtotal + 2; // 2?
	/*mode.vblank_start = vblank_start;
	mode.vblank_end = vblank_end;*/
	mode.vdisplay = vdisplay + 1;
	mode.vsync_start = vsync_start + 1;
	mode.vsync_end = vsync_end + 1;
	if (vscan)
		mode.vscan = vscan + 1;
	else
		mode.vscan = 0;


	if (misc & 0x80)
		mode.flags |= DRM_MODE_FLAG_NVSYNC;
	else
		mode.flags |= DRM_MODE_FLAG_PVSYNC;
	if (misc & 0x40)
		mode.flags |= DRM_MODE_FLAG_NHSYNC;
	else
		mode.flags |= DRM_MODE_FLAG_PHSYNC;
	if (cr09 & 0x80)
		mode.flags |= DRM_MODE_FLAG_DBLSCAN;
	if (hskew)
		mode.flags |= DRM_MODE_FLAG_HSKEW;
	if (sr01 & 0x08)
		mode.flags |= DRM_MODE_FLAG_CLKDIV2;

	RREG_SR(0x38, sr38);

	saved_sr38 = sr38;

	switch (misc & 0x0c) {
	case 0x00:
		fr = 25175;
		freqi = 0x01;
		break;
	case 0x04:
		fr = 28322;
		freqi = 0x02;
		break;
	case 0x0c:
		fr = 14813;
		freqi = 0x00;
		break;
	default:
		BUG();
		break;
	}

	sr38 &= 0xfc;
	sr38 |= freqi;

	WREG_SR(0x38, sr38);
	WAIT_SR_MASKED(0x38, freqi, 0x03);

	RREG_SR(0x13, sr13);
	RREG_SR(0x2a, sr2a);
	RREG_SR(0x2b, sr2b);

	/* Restore previous sr38 */
	WREG_SR(0x38, saved_sr38);
	WAIT_SR(0x38, saved_sr38);

	div = ((sr2a & 0x80) >> 7) + 1;
	num = (sr2a & 0x7f) + 1;
	denum = ((sr2b & 0x1f) + 1);
	postscal = ((sr2b & 0x60) >> 5) + 1;
	if (sr13 & 0x80)
		postscal *= 2;

	fd = (fr * num * div) / (denum * postscal);
	mode.clock = fd;

	DRM_INFO("fd=%d fr=%d num=%d denum=%d div=%d postscal=%d\n", fd, fr, num, denum, div, postscal);

	DRM_INFO("mode: " DRM_MODE_FMT "\n", DRM_MODE_ARG(&mode));
}

void sisvga_debug_print_regs(struct sisvga_device* sdev)
{
	u8 ar10, ar11, ar12, ar13, ar14;
	u8 misc;
	u8 sr00, sr01, sr03, sr04, sr06, sr0a, sr0b, sr0c, sr0d,
	   sr0e, sr0f, sr13, sr21, sr26, sr2a, sr2b, sr2d, sr38;
	u8 gr00, gr01, gr02, gr03, gr04, gr05, gr06;
	u8 cr00, cr01, cr02, cr03, cr04, cr05, cr06, cr07,
	   cr08, cr09, cr0a, cr0b, cr0c, cr0d, cr0e, cr0f,
	   cr10, cr11, cr12, cr13, cr14, cr15, cr16, cr17,
	   cr18;

	RREG_AR(0x10, ar10);
	RREG_AR(0x11, ar11);
	RREG_AR(0x12, ar12);
	RREG_AR(0x13, ar13);
	RREG_AR(0x14, ar14);

	misc = RREG8(REG_MISC_IN);

	RREG_SR(0x00, sr00);
	RREG_SR(0x01, sr01);
	RREG_SR(0x03, sr03);
	RREG_SR(0x04, sr04);
	RREG_SR(0x06, sr06);
	RREG_SR(0x0a, sr0a);
	RREG_SR(0x0b, sr0b);
	RREG_SR(0x0c, sr0c);
	RREG_SR(0x0d, sr0d);
	RREG_SR(0x0e, sr0e);
	RREG_SR(0x0f, sr0f);
	RREG_SR(0x13, sr13);
	RREG_SR(0x21, sr21);
	RREG_SR(0x26, sr26);
	RREG_SR(0x2a, sr2a);
	RREG_SR(0x2b, sr2b);
	RREG_SR(0x2d, sr2d);
	RREG_SR(0x38, sr38);

	RREG_GR(0x00, gr00);
	RREG_GR(0x01, gr01);
	RREG_GR(0x02, gr02);
	RREG_GR(0x03, gr03);
	RREG_GR(0x04, gr04);
	RREG_GR(0x05, gr05);
	RREG_GR(0x06, gr06);

	RREG_CR(0x00, cr00);
	RREG_CR(0x01, cr01);
	RREG_CR(0x02, cr02);
	RREG_CR(0x03, cr03);
	RREG_CR(0x04, cr04);
	RREG_CR(0x05, cr05);
	RREG_CR(0x06, cr06);
	RREG_CR(0x07, cr07);
	RREG_CR(0x08, cr08);
	RREG_CR(0x09, cr09);
	RREG_CR(0x0a, cr0a);
	RREG_CR(0x0b, cr0b);
	RREG_CR(0x0c, cr0c);
	RREG_CR(0x0d, cr0d);
	RREG_CR(0x0e, cr0e);
	RREG_CR(0x0f, cr0f);
	RREG_CR(0x10, cr10);
	RREG_CR(0x11, cr11);
	RREG_CR(0x12, cr12);
	RREG_CR(0x13, cr13);
	RREG_CR(0x14, cr14);
	RREG_CR(0x15, cr15);
	RREG_CR(0x16, cr16);
	RREG_CR(0x17, cr17);
	RREG_CR(0x18, cr18);

	DRM_INFO("ar10=0x%x\n", ar10);
	DRM_INFO("ar11=0x%x\n", ar11);
	DRM_INFO("ar12=0x%x\n", ar12);
	DRM_INFO("ar13=0x%x\n", ar13);
	DRM_INFO("ar14=0x%x\n", ar14);

	DRM_INFO("misc=0x%x\n", misc);

	DRM_INFO("sr00=0x%x\n", sr00);
	DRM_INFO("sr01=0x%x\n", sr01);
	DRM_INFO("sr03=0x%x\n", sr03);
	DRM_INFO("sr04=0x%x\n", sr04);
	DRM_INFO("sr06=0x%x\n", sr06);
	DRM_INFO("sr0a=0x%x\n", sr0a);
	DRM_INFO("sr0b=0x%x\n", sr0b);
	DRM_INFO("sr0c=0x%x\n", sr0c);
	DRM_INFO("sr0d=0x%x\n", sr0d);
	DRM_INFO("sr0e=0x%x\n", sr0e);
	DRM_INFO("sr0f=0x%x\n", sr0f);
	DRM_INFO("sr13=0x%x\n", sr13);
	DRM_INFO("sr21=0x%x\n", sr21);
	DRM_INFO("sr26=0x%x\n", sr26);
	DRM_INFO("sr2a=0x%x\n", sr2a);
	DRM_INFO("sr2b=0x%x\n", sr2b);
	DRM_INFO("sr2d=0x%x\n", sr2d);
	DRM_INFO("sr38=0x%x\n", sr38);

	DRM_INFO("gr00=0x%x\n", gr00);
	DRM_INFO("gr01=0x%x\n", gr01);
	DRM_INFO("gr02=0x%x\n", gr02);
	DRM_INFO("gr03=0x%x\n", gr03);
	DRM_INFO("gr04=0x%x\n", gr04);
	DRM_INFO("gr05=0x%x\n", gr05);
	DRM_INFO("gr06=0x%x\n", gr06);

	DRM_INFO("cr00=0x%x\n", cr00);
	DRM_INFO("cr01=0x%x\n", cr01);
	DRM_INFO("cr02=0x%x\n", cr02);
	DRM_INFO("cr03=0x%x\n", cr03);
	DRM_INFO("cr04=0x%x\n", cr04);
	DRM_INFO("cr05=0x%x\n", cr05);
	DRM_INFO("cr06=0x%x\n", cr06);
	DRM_INFO("cr07=0x%x\n", cr07);
	DRM_INFO("cr08=0x%x\n", cr08);
	DRM_INFO("cr09=0x%x\n", cr09);
	DRM_INFO("cr0a=0x%x\n", cr0a);
	DRM_INFO("cr0b=0x%x\n", cr0b);
	DRM_INFO("cr0c=0x%x\n", cr0c);
	DRM_INFO("cr0d=0x%x\n", cr0d);
	DRM_INFO("cr0e=0x%x\n", cr0e);
	DRM_INFO("cr0f=0x%x\n", cr0f);
	DRM_INFO("cr10=0x%x\n", cr10);
	DRM_INFO("cr11=0x%x\n", cr11);
	DRM_INFO("cr12=0x%x\n", cr12);
	DRM_INFO("cr13=0x%x\n", cr13);
	DRM_INFO("cr14=0x%x\n", cr14);
	DRM_INFO("cr15=0x%x\n", cr15);
	DRM_INFO("cr16=0x%x\n", cr16);
	DRM_INFO("cr17=0x%x\n", cr17);
	DRM_INFO("cr18=0x%x\n", cr18);
}
