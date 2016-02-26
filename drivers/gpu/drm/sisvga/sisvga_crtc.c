/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#include "sisvga_device.h"
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_plane_helper.h>
#include "sisvga_reg.h"

static struct sisvga_crtc* sisvga_crtc_of(struct drm_crtc* crtc)
{
	return container_of(crtc, struct sisvga_crtc, base);
}

/*
 * LUT helpers
 */

static void load_lut_18(struct sisvga_device *sdev, const u8 (*lut)[3],
			size_t len)
{
	size_t i, j;
	u8 rgb[3];

	for (i = 0; i < len; ++i) {
		RREG_DAC(i, rgb[0], rgb[1], rgb[2]);
		for (j = 0; j < ARRAY_SIZE(rgb); ++j) {
			rgb[j] = (rgb[j] & 0xc0) | (lut[i][0] & 0x3f);
		}
		WREG_DAC(i, rgb[0], rgb[1], rgb[2]);
	}
}

/* SiS graphics cards allow for 8-bit values in the DAC. */
static void load_lut_24(struct sisvga_device *sdev, const u8 (*lut)[3],
			size_t len)
{
	size_t i;

	for (i = 0; i < len; ++i) {
		WREG_DAC(i, lut[i][0], lut[i][1], lut[i][2]);
	}
}

/*
 * DPMS helpers
 */

static void set_crtc_dpms_mode(struct sisvga_device* sdev, int mode)
{
	u8 sr01;

	RREG_SR(0x01, sr01);

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		sr01 &= 0xdf; /* screen enabled */
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		sr01 |= 0x20; /* screen disabled */
		break;
	default:
		DRM_ERROR("sisvga: invalid DPMS mode %d\n", mode);
		return;
	}

	WREG_SR(0x01, sr01);
}

/*
 * Mode-setting helpers
 */

static u32 bytes_per_pixel(const struct drm_format_info* format)
{
	u32 bpp;
	u8 i;

	for (bpp = 0, i = 0; i < format->num_planes; ++i) {
		bpp += format->cpp[i];
	}
	return bpp;
}

static int compute_offset(unsigned int width,
			  const struct drm_format_info* format,
			  u8 addr_incr, bool interlaced)
{
	int offset = width * bytes_per_pixel(format);

	if (interlaced)
		offset *= 2;

	return offset / (addr_incr * 2);

}

static int set_framebuffer(struct sisvga_device *sdev,
			   struct sisvga_framebuffer *new_sis_fb,
			   struct sisvga_framebuffer *old_sis_fb,
			   bool atomic, bool interlaced)
{
	struct sisvga_bo *bo;
	int ret, offset;
	u64 gpu_addr;
	u32 start_address;
	u8 cr13, cr0c, cr0d, sr02, sr03, sr04, sr06, sr0a, sr0c, sr26, sr27, sr3e;

	/* push the previous fb to system ram */
	if (!atomic && old_sis_fb) {
		bo = gem_to_sisvga_bo(old_sis_fb->gem_obj);
		ret = sisvga_bo_reserve(bo, false);
		if (ret)
			return ret;
		sisvga_bo_push_to_system(bo);
		sisvga_bo_unreserve(bo);
	}

	bo = gem_to_sisvga_bo(new_sis_fb->gem_obj);

	ret = sisvga_bo_reserve(bo, false);
	if (ret)
		return ret;

	ret = sisvga_bo_pin(bo, TTM_PL_FLAG_VRAM, &gpu_addr);
	if (ret) {
		sisvga_bo_unreserve(bo);
		return ret;
	}

	sisvga_bo_unreserve(bo);

	ret = compute_offset(new_sis_fb->base.width, new_sis_fb->base.format,
			     4, interlaced);
	if (ret < 0)
		return ret;
	offset = ret;

	start_address = gpu_addr >> 2;

	RREG_SR(0x02, sr02);
	RREG_SR(0x03, sr03);
	RREG_SR(0x04, sr04);
	RREG_SR(0x06, sr06);
	RREG_SR(0x0a, sr0a);
	RREG_SR(0x27, sr27);
	RREG_SR(0x3e, sr3e);

	cr13 = offset & 0xff;
	cr0c = (start_address & 0x0000ff00) >> 8;
	cr0d = start_address & 0x000000ff;
	sr02 &= 0xf0; /* preserve reserved bits */
	sr02 |= 0x0f; /* writes to all planes enabled */
	sr03 &= 0xc0; /* preserve reserved bits */
	sr04 &= 0xf1; /* preserve reserved bits */
	sr04 |= 0x08 | /* Chain-4 mode enabled */
		0x04 | /* Odd/Even disable */
		0x02; /* Extended memory enabled */
	sr06 |= 0x80; /* linear addressing enabled */
	sr0a &= 0x0f; /* preserve bits */
	sr0a |= (offset & 0xf00) >> 4;
	sr0c = 0x80; /* 32-bit graphics-memory access enabled */
	sr26 = 0x10; /* continuous memory access enabled */
	sr27 &= 0xf0; /* preserve reserved bits */
	sr27 |= (start_address & 0x000f0000) >> 16;

	WREG_CR(0x13, cr13);
	WREG_CR(0x0c, cr0c);
	WREG_CR(0x0d, cr0d);

	WREG_SR(0x02, sr02);
	WREG_SR(0x03, sr03);
	WREG_SR(0x04, sr04);
	WREG_SR(0x06, sr06);
	WREG_SR(0x0a, sr0a);
	WREG_SR(0x0c, sr0c);
	WREG_SR(0x26, sr26);
	WREG_SR(0x27, sr27);
	WREG_SR(0x3e, sr3e);

	return 0;
}

static int set_color_mode(const struct sisvga_device *sdev,
			  const struct drm_framebuffer *fb)
{
	struct drm_format_name_buf buf;
	u8 cr14, cr17, ar10, ar11, ar12, ar13, ar14, gr00, gr01,
	   gr02, gr03, gr04, gr05, gr06, gr07, gr08, sr06, sr0b;

	RREG_CR(0x14, cr14);
	RREG_CR(0x17, cr17);
	RREG_GR(0x00, gr00);
	RREG_GR(0x01, gr01);
	RREG_GR(0x02, gr02);
	RREG_GR(0x03, gr03);
	RREG_GR(0x04, gr04);
	RREG_GR(0x05, gr05);
	RREG_GR(0x06, gr06);
	RREG_GR(0x07, gr07);
	RREG_AR(0x10, ar10);
	RREG_AR(0x12, ar12);
	RREG_AR(0x13, ar13);
	RREG_AR(0x14, ar14);
	RREG_SR(0x06, sr06);
	RREG_SR(0x0b, sr0b);

	cr14 |= 0x40; /* set double-word addressing */
	//cr14 &= 0x9f; /* double-word addressing disabled */
	cr17 &= 0xbf; /* word-mode addressing enabled */
	//if (fb->format->depth < 8)
	//	cr17 |= 0x20;
	cr17 |= /*0x20 |*/ /* set MA 15 bit in word-address mode */
                0x80; /* word-based refresh enabled */

	gr00 &= 0xf0; /* preserve reserved bits */
	gr01 &= 0xf0; /* preserve reserved bits */
	gr02 &= 0xf0; /* preserve reserved bits */
	gr03 &= 0xe0; /* preserve reserved bits */
	gr04 &= 0xfa; /* preserve reserved bits */
	gr05 &= 0x84; /* preserve reserved bits */
	gr06 &= 0xf0; /* preserve reserved bits */
	gr06 |= /*0x04 |*/
		0x01; /* graphics mode */
	gr07 &= 0xf0; /* preserve reserved bits */
	gr07 |= 0x0f; /* color-don't-care for Read Mode 1 */
	gr08 = 0xff; /* bitmask for Write Modes 0,2,3 */

	ar10 &= 0x10; /* preserve reserved bits */
	ar10 |= 0x01; /* graphics mode enabled */
	ar11 = 0x00; /* clear overscan palette index */
	ar12 &= 0xf0; /* preserve reserved bits */
	//ar12 |= 0x0f; /* enable color planes */
	ar13 &= 0xf0; /* preserve reserved bits */
	ar14 &= 0xf0; /* preserve reserved bits */

	/* In TrueColor mode, the hardware expects RGB buffers in
	 * big-endian byte order. If the framebuffer is in little
	 * endian, we invert the RGB/BGR mode. */
	switch (fb->format->format) {
	case DRM_FORMAT_RGB888:
		sr06 &= 0xe0; /* clear graphics/text mode */
		sr06 |= 0x10; /* TrueColor enabled */
		sr06 |= 0x02; /* enhanced graphcs mode enabled */
		if (fb->format->format & DRM_FORMAT_BIG_ENDIAN)
			sr0b &= 0x7f; /* RGB byte order */
		else
			sr0b |= 0x80; /* RGB byte order (little endian) */
		break;
	case DRM_FORMAT_BGR888:
		sr06 &= 0xe0; /* clear graphics/text mode */
		sr06 |= 0x10; /* TrueColor enabled */
		sr06 |= 0x02; /* enhanced graphcs mode enabled */
		if (fb->format->format & DRM_FORMAT_BIG_ENDIAN)
			sr0b |= 0x80; /* BGR byte order */
		else
			sr0b &= 0x7f; /* BGR byte order (little endian) */
		break;
	case DRM_FORMAT_RGB565:
		sr06 &= 0xe0; /* clear graphics/text mode */
		sr06 |= 0x08; /* 64KColor enabled */;
		sr06 |= 0x02; /* enhanced graphcs mode enabled */
		sr0b &= 0x7f; /* clear RGB byte order */
		break;
	case DRM_FORMAT_XRGB1555:
		sr06 &= 0xe0; /* clear graphics/text mode */
		sr06 |= 0x04; /* 32KColor enabled */
		sr06 |= 0x02; /* enhanced graphcs mode enabled */
		sr0b &= 0x7f; /* clear RGB byte order */
		break;
	case DRM_FORMAT_C8:
		gr05 = 0x40; /* 256-color palette enabled */
		ar10 |= 0x40; /* 8-bit palette enabled */
		sr06 &= 0xe3; /* clear enhanced color modes */
		sr0b &= 0x7f; /* clear RGB byte-order bit */
		break;
	default: 	/* unsupported VGA color configuration */
		/* BUG: We should have detected this in mode_fixup(). */
		DRM_ERROR("sisvga: %s color format is not supported\n",
			  drm_get_format_name(fb->format->format, &buf));
		return -EINVAL;
	}

	WREG_CR(0x14, cr14);
	WREG_CR(0x17, cr17);

	WREG_GR(0x00, gr00);
	WREG_GR(0x01, gr01);
	WREG_GR(0x02, gr02);
	WREG_GR(0x03, gr03);
	WREG_GR(0x04, gr04);
	WREG_GR(0x05, gr05);
	WREG_GR(0x06, gr06);
	WREG_GR(0x07, gr07);
	WREG_GR(0x08, gr08);

	WREG_AR(0x10, ar10);
	WREG_AR(0x11, ar11);
	WREG_AR(0x12, ar12);
	WREG_AR(0x13, ar13);
	WREG_AR(0x14, ar14);

	WREG_SR(0x06, sr06);
	WREG_SR(0x0b, sr0b);

	return 0;
}

static bool is_9_dot_mode(const struct drm_display_mode* mode)
{
	int hdisplay = mode->hdisplay;
	if (mode->flags & DRM_MODE_FLAG_CLKDIV2)
		hdisplay *= 2;

	/* VGA 9-dot modes have a ratio of 9:5 */
	return !(hdisplay % 9) && (((mode->vdisplay * 9) / 5) == hdisplay);
}

static int set_display_mode(struct sisvga_device *sdev,
			    const struct drm_display_mode *mode)
{
	static const enum sisvga_freq freq_list[] = {
		SISVGA_FREQ_14318,
		SISVGA_FREQ_25175,
		SISVGA_FREQ_28322
	};
	static const u8 freq_bits_list[] = {
		[SISVGA_FREQ_14318] = 0x03,
		[SISVGA_FREQ_25175] = 0x00,
		[SISVGA_FREQ_28322] = 0x01
	};
	static const u8 freqi_bits_list[] = {
		[SISVGA_FREQ_14318] = 0x00,
		[SISVGA_FREQ_25175] = 0x01,
		[SISVGA_FREQ_28322] = 0x02
	};

	size_t i;
	long ret;
	enum sisvga_vclk vclk;
	enum sisvga_freq freq;
	unsigned long num, denum, div, postscal, f;
	u8 freq_bits, freqi_bits, num_bits, denum_bits, div_bits, postscal_bits;

	int dots, htotal, hsync_start, hsync_end, hdisplay, hskew, hblank_start,
	    hblank_end, vtotal, vsync_start, vsync_end, vdisplay, vscan,
	    vblank_start, vblank_end, line_compare;
	u8 misc;
	u8 cr00, cr01, cr02, cr03, cr04, cr05, cr06, cr07, cr08, cr09, cr0a,
	   cr0b, cr0c, cr0d, cr0e, cr0f, cr10, cr11, cr12, cr14, cr15,
	   cr16, cr17, cr18;
	u8 sr01, sr06, sr07, sr0a, sr12, sr13, sr2a, sr2b, sr38;

	/* The CRTC values are the same as for regular VGA adapters.
	 * Some of the bits for higher resolutions will be copied to
	 * SiS' extended registers. */

	if (is_9_dot_mode(mode))
		dots = 9;
	else
		dots = 8;

	htotal = (mode->crtc_htotal / dots) - 5;
	hsync_start = (mode->crtc_hsync_start / dots) - 1;
	hsync_end = (mode->crtc_hsync_end / dots) - 1;
	hdisplay = (mode->crtc_hdisplay / dots) - 1;
	if (mode->flags & DRM_MODE_FLAG_HSKEW)
		hskew = mode->hskew / dots;
	else
		hskew = 0;
	hblank_start = (mode->crtc_hblank_start / dots) - 1;
	hblank_end = (mode->crtc_hblank_end / dots) - 1;

	vtotal = mode->crtc_vtotal - 2;
	vsync_start = mode->crtc_vsync_start - 1;
	vsync_end = mode->crtc_vsync_end - 1;
	vdisplay = mode->crtc_vdisplay - 1;
	if (mode->vscan)
		vscan = mode->vscan - 1;
	else
		vscan = 0;
	vblank_start = mode->crtc_vblank_start - 1;
	vblank_end = mode->crtc_vblank_end - 1;
	line_compare = mode->crtc_vtotal + 1; /* beyond end of display; disabled */

	/* We have to compute the PLL's configuration for the given
	 * dot clock. With the computed parameters, we can also select
	 * the correct registers. sr38 serves as index register for
	 * sr13, sr2a, and sr2b. */

	ret = sisvga_vclk_of_clock(mode->clock, &vclk);
	if (ret < 0) {
		/* BUG: We should have detected this in mode_valid(). */
		DRM_ERROR("sisvga: unsupported dot clock of %d KHz, error %ld",
			  mode->clock, -ret);
		return ret;
	}
	sisvga_vclk_regs(vclk, &freq, &num, &denum, &div, &postscal, &f);

	freq_bits = freq_bits_list[freq];
	freqi_bits = freqi_bits_list[freq];
	num_bits = num - 1;
	denum_bits = denum - 1;
	div_bits = div - 1;
	postscal_bits = postscal - 1;

	misc = RREG8(REG_MISC_IN);

	RREG_CR(0x08, cr08);
	RREG_CR(0x09, cr09);
	RREG_CR(0x11, cr11);
	RREG_CR(0x14, cr14);
	RREG_CR(0x17, cr17);

	/* Below is the register setup code. sr38 contains the index
	 * register for sr13, sr2a and sr2b. We set it up before all
	 * other sequencer registers. With the correct registers
	 * selected we can later configure the dot clock generator. */
	/* TODO: NOT on 6202 ??? */
	RREG_SR(0x38, sr38);
	sr38 &= 0xfc; /* clear clock-register selector */
	sr38 |= freqi_bits;
	WREG_SR(0x38, sr38);
	WAIT_SR_MASKED(0x38, freqi_bits, 0x03);

	RREG_SR(0x01, sr01);
	RREG_SR(0x06, sr06);
	RREG_SR(0x07, sr07);
	if (MODEL_IS_GE(6326))
		RREG_SR(0x13, sr13);
	else
		sr13 = 0;
	RREG_SR(0x0a, sr0a);
	RREG_SR(0x2a, sr2a);
	RREG_SR(0x2b, sr2b);
	if (MODEL_IS_GE(6326))
		RREG_SR(0x38, sr38);
	else
		sr38 = 0;

	cr00 = htotal & 0xff;
	cr01 = hdisplay & 0xff;
	cr02 = hblank_start & 0xff;
	cr03 = 0x80 | /* preserve reserved bit */
	       ((hskew & 0x03) << 5) |
	       (hblank_end & 0x1f);
	cr04 = hsync_start & 0xff;
	cr05 = ((hblank_end & 0x20) << 2) |
	       (hsync_end & 0x1f);
	cr06 = vtotal & 0xff;
	cr07 = ((vsync_start & 0x200) >> 2) |
	       ((vdisplay & 0x200) >> 3) |
	       ((vtotal & 0x200) >> 4) |
	       ((line_compare & 0x100) >> 4) |
	       ((vblank_start & 0x100) >> 5) |
	       ((vsync_start & 0x100) >> 6) |
	       ((vdisplay & 0x100) >> 7) |
	       ((vtotal & 0x100) >> 8);
	cr08 &= 0x80; /* preserve bit */
	cr09 = ((line_compare & 0x200) >> 3) |
	       ((vblank_start & 0x200) >> 4) |
	       (vscan & 0x1f);
	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		cr09 |= 0x80;
	cr0a = 0;
	cr0b = 0;
	cr0c = 0;
	cr0d = 0;
	cr0e = 0;
	cr0f = 0;
	cr10 = vsync_start & 0xff;
	cr11 = vsync_end & 0x0f;
	cr12 = vdisplay & 0xff;
	cr14 &= 0x80; /* preserve reserved bit */
	cr15 = vblank_start & 0xff;
	cr16 = vblank_end & 0xff; /* SiS uses all 8 bits */
	cr17 |= 0x40 | /* byte-address mode enabled */
		0x03;
	cr18 = line_compare & 0xff;

	if (dots == 9)
		sr01 &= 0xfe; /* 9-dot mode */
	else
		sr01 |= 0x01; /* 8-dot mode */
	sr0a &= 0xf0; /* preserve bits */
	sr0a |= ((vsync_start & 0x400) >> 7) |
		((vblank_start & 0x400) >> 8) |
		((vdisplay & 0x400) >> 9) |
		((vtotal & 0x400) >> 10);
	if (MODEL_IS_GE(6326))
		sr12 = ((hblank_end & 0x40) >> 2) |
		       ((hsync_start & 0x100) >> 5) |
		       ((hblank_start & 0x100) >> 6) |
		       ((hdisplay & 0x100) >> 7) |
		       ((htotal & 0x100) >> 8);
	else
		sr12 = 0;

	misc &= 0x3f; /* clear sync bits */
	if (mode->flags & DRM_MODE_FLAG_NVSYNC)
		misc |= 0x80;
	if (mode->flags & DRM_MODE_FLAG_NHSYNC)
		misc |= 0x40;

	misc &= 0xf3; /* clear clock selector */
	misc |= freq_bits << 2;

	if (mode->flags & DRM_MODE_FLAG_CLKDIV2)
		sr01 |= 0x08;
	else
		sr01 &= 0xf7; /* don't divide dot-clock rate by 2 */

	/* TODO: Force 9/8 dot mode to 0 when switching to
	 * 25.175 MHz in rev 0b and earlier. */

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		sr06 |= 0x20;
	else
		sr06 &= 0xdf;

	if (mode->clock > MHZ_TO_KHZ(135))
		sr07 |= 0x02; /* high-frequency DAC enabled */
	else
		sr07 &= 0xfd;

	if (freq == SISVGA_FREQ_14318) {

		/* On all SiS adapters we have to configure the internal dot
		 * clock generator. In theory (aka 'the manual') on SiS 6326
		 * we can configure VGA clock generators in the same way. In
		 * practice hardware doesn't support it. So we only use VGA
		 * registers for VGA dot clocks, and extended registers for
		 * the internal clock generator. */

		if (MODEL_IS_GE(6326)) {
			if (postscal_bits & 0x04)
				sr13 |= 0x40;
			else
				sr13 &= 0xbf; /* clear post-scaler bit */
		}

		sr2a = ((div_bits & 0x01) << 7) |
		       (num_bits & 0x7f);
		sr2b = ((postscal_bits & 0x03) << 5) |
		       (denum_bits & 0x1f);
		if (mode->clock > MHZ_TO_KHZ(135))
			sr2b |= 0x80; /* high-frequency gain enabled */

		sr06 |= 0x03; /* enable enhanced modes */
	} else {
		sr06 &= 0xfc; /* disable enhanced modes */
	}

	if (MODEL_IS_GE(6326))
		sr38 |= 0x04; /* Disable line compare */

	WREG8(REG_MISC_OUT, misc);
	WAIT8_MASKED(REG_MISC_IN, misc, 0xef);

	WREG_SR(0x01, sr01);

	/* VCLK setup */
	if (freq == SISVGA_FREQ_14318) {
		if (MODEL_IS_GE(6326))
			WREG_SR(0x13, sr13);
		WREG_SR(0x2a, sr2a);
		WREG_SR(0x2b, sr2b);
	}

	WREG_SR(0x0a, sr0a);
	WREG_SR(0x06, sr06);
	WREG_SR(0x07, sr07);
	if (MODEL_IS_GE(6326)) {
		WREG_SR(0x12, sr12);
		WREG_SR(0x38, sr38);
	}

	WREG_CR(0x00, cr00);
	WREG_CR(0x01, cr01);
	WREG_CR(0x02, cr02);
	WREG_CR(0x03, cr03);
	WREG_CR(0x04, cr04);
	WREG_CR(0x05, cr05);
	WREG_CR(0x06, cr06);
	WREG_CR(0x07, cr07);
	WREG_CR(0x08, cr08);
	WREG_CR(0x09, cr09);
	WREG_CR(0x0a, cr0a);
	WREG_CR(0x0b, cr0b);
	WREG_CR(0x0c, cr0c);
	WREG_CR(0x0d, cr0d);
	WREG_CR(0x0e, cr0e);
	WREG_CR(0x0f, cr0f);
	WREG_CR(0x10, cr10);
	WREG_CR(0x11, cr11);
	WREG_CR(0x12, cr12);
	WREG_CR(0x14, cr14);
	WREG_CR(0x15, cr15);
	WREG_CR(0x16, cr16);
	WREG_CR(0x17, cr17);
	WREG_CR(0x18, cr18);

	return 0;
}

/*
 * CRTC helper funcs
 */

static void sisvga_crtc_helper_disable(struct drm_crtc *crtc)
{
	set_crtc_dpms_mode(crtc->dev->dev_private, DRM_MODE_DPMS_OFF);
}

static void sisvga_crtc_helper_dpms(struct drm_crtc *crtc, int mode)
{
	struct sisvga_crtc *sis_crtc = sisvga_crtc_of(crtc);

	set_crtc_dpms_mode(crtc->dev->dev_private, mode);

	if (sis_crtc->lut_24) {
		load_lut_24(crtc->dev->dev_private, sis_crtc->lut,
			    ARRAY_SIZE(sis_crtc->lut));
	} else {
		load_lut_18(crtc->dev->dev_private, sis_crtc->lut,
			    ARRAY_SIZE(sis_crtc->lut));
	}
}

static bool sisvga_crtc_helper_mode_fixup(struct drm_crtc *crtc,
					  const struct drm_display_mode *mode,
					  struct drm_display_mode *adj_mode)
{
	u8 bpp;
	int framebuffer_size;
	struct sisvga_device *sdev = crtc->dev->dev_private;
	const struct sisvga_device_info *info = sdev->info;

	if (adj_mode->hdisplay > info->max_hdisplay)
		return false;

	if (adj_mode->vdisplay > info->max_vdisplay)
		return false;

	if (((adj_mode->crtc_hdisplay % 8) != 0 ||
	     (adj_mode->crtc_hsync_start % 8) != 0 ||
	     (adj_mode->crtc_hsync_end % 8) != 0 ||
	     (adj_mode->crtc_htotal % 8) != 0) &&
	    ((adj_mode->crtc_hdisplay % 9) != 0 ||
	     (adj_mode->crtc_hsync_start % 9) != 0 ||
	     (adj_mode->crtc_hsync_end % 9) != 0 ||
	     (adj_mode->crtc_htotal % 9) != 0)) {
		return false;
	}

	if (adj_mode->crtc_hdisplay > info->max_hdisplay ||
	    adj_mode->crtc_hsync_start > info->max_hsync_start ||
	    adj_mode->crtc_hsync_end > info->max_hsync_end ||
	    adj_mode->crtc_htotal > info->max_htotal ||
	    adj_mode->crtc_vdisplay > info->max_vdisplay ||
	    adj_mode->crtc_vsync_start > info->max_vsync_start ||
	    adj_mode->crtc_vsync_end > info->max_vsync_end ||
	    adj_mode->crtc_vtotal > info->max_vtotal) {
		return false;
	}

	bpp = bytes_per_pixel(crtc->primary->fb->format);
	framebuffer_size = adj_mode->crtc_hdisplay * adj_mode->crtc_vdisplay *
			   bpp;

	if (framebuffer_size > sdev->vram.size)
		return false;

	return true;
}

static int sisvga_crtc_helper_mode_set(struct drm_crtc *crtc,
				       struct drm_display_mode *mode,
				       struct drm_display_mode *adj_mode,
				       int x, int y,
				       struct drm_framebuffer *old_fb)
{
	int ret;

	ret = set_display_mode(crtc->dev->dev_private, adj_mode);
	if (ret < 0)
		return ret;

	ret = set_color_mode(crtc->dev->dev_private, crtc->primary->fb);
	if (ret < 0)
		return ret;

	ret = set_framebuffer(crtc->dev->dev_private,
			      sisvga_framebuffer_of(crtc->primary->fb),
			      sisvga_framebuffer_of(old_fb),
			      false,
			      !!(mode->flags & DRM_MODE_FLAG_INTERLACE));
	if (ret < 0)
		return ret;

	{
		int len = 3 * crtc->primary->fb->width * crtc->primary->fb->height;
		struct sisvga_device* sdev = crtc->dev->dev_private;
		for (ret = 0; ret < len; ++ret)
			((u8*)sdev->vram.mem)[ret] = 0xff;
	}

	return 0;
}

static int sisvga_crtc_helper_mode_set_base(struct drm_crtc *crtc,
					    int x, int y,
					    struct drm_framebuffer *old_fb)
{
	int ret;

	ret = set_framebuffer(crtc->dev->dev_private,
			      sisvga_framebuffer_of(crtc->primary->fb),
			      sisvga_framebuffer_of(old_fb),
			      false, false);
	if (ret < 0)
		return ret;

	return 0;
}

static void sisvga_crtc_helper_prepare(struct drm_crtc *crtc)
{
	struct sisvga_device* sdev = crtc->dev->dev_private;
	u8 sr00, sr01;

	/* We disable the screen to allow for flicker-free
	 * mode switching. */
	set_crtc_dpms_mode(crtc->dev->dev_private, DRM_MODE_DPMS_OFF);

	RREG_SR(0x00, sr00);
	RREG_SR(0x01, sr01);

	sr00 &= 0xfc; /* sequencer reset */
	sr01 |= 0x20; /* screen disabled */

	WREG_SR(0x01, sr01);
	WREG_SR(0x00, sr00);
}

static void sisvga_crtc_helper_commit(struct drm_crtc *crtc)
{
	struct sisvga_device* sdev = crtc->dev->dev_private;
	u8 sr00, sr01;

	RREG_SR(0x00, sr00);
	RREG_SR(0x01, sr01);

	sr00 |= 0x03; /* no reset; start sequencer */
	sr01 &= 0xdf; /* screen enabled */

	WREG_SR(0x00, sr00);
	WREG_SR(0x01, sr01);

	set_crtc_dpms_mode(crtc->dev->dev_private, DRM_MODE_DPMS_ON);

}

static const struct drm_crtc_helper_funcs sisvga_crtc_helper_funcs = {
	.disable = sisvga_crtc_helper_disable,
	.dpms = sisvga_crtc_helper_dpms,
	.mode_fixup = sisvga_crtc_helper_mode_fixup,
	.mode_set = sisvga_crtc_helper_mode_set,
	.mode_set_base = sisvga_crtc_helper_mode_set_base,
	.prepare = sisvga_crtc_helper_prepare,
	.commit = sisvga_crtc_helper_commit,
};

/*
 * CRTC funcs
 */

static int sisvga_crtc_cursor_set(struct drm_crtc *crtc,
				  struct drm_file *file_priv,
				  uint32_t handle,
				  uint32_t width,
				  uint32_t height)
{
	return 0;
}

static int sisvga_crtc_cursor_move(struct drm_crtc *crtc, int x, int y)
{
	return 0;
}

static int sisvga_crtc_gamma_set(struct drm_crtc *crtc, u16 *red, u16 *green,
				 u16 *blue, uint32_t size,
				 struct drm_modeset_acquire_ctx *ctx)
{
	uint32_t i;
	struct sisvga_crtc *sis_crtc = sisvga_crtc_of(crtc);

	for (i = 0; i < size; i++) {
		sis_crtc->lut[i][0] = red[i] >> 8;
		sis_crtc->lut[i][1] = green[i] >> 8;
		sis_crtc->lut[i][2] = blue[i] >> 8;
	}
	sis_crtc->lut_len = size;
	sis_crtc->lut_24 = true;
	load_lut_24(crtc->dev->dev_private, sis_crtc->lut, sis_crtc->lut_len);

	return 0;
}

static void sisvga_crtc_destroy(struct drm_crtc *crtc)
{
	struct sisvga_crtc *sis_crtc = sisvga_crtc_of(crtc);
	struct drm_device *dev = crtc->dev;

	drm_crtc_cleanup(&sis_crtc->base);
	devm_kfree(dev->dev, sis_crtc);
}

static int sisvga_crtc_page_flip(struct drm_crtc *crtc,
				 struct drm_framebuffer *fb,
				 struct drm_pending_vblank_event *event,
				 uint32_t flags,
				 struct drm_modeset_acquire_ctx *ctx)
{
	struct sisvga_device *sdev = crtc->dev->dev_private;
	struct drm_framebuffer *old_fb = crtc->primary->fb;
	unsigned long irqflags;
	struct drm_format_name_buf buf;

	crtc->primary->fb = fb;
	sisvga_crtc_helper_mode_set_base(crtc, 0, 0, old_fb);

	if (event) {
		spin_lock_irqsave(&sdev->base.event_lock, irqflags);
		drm_crtc_send_vblank_event(crtc, event);
		spin_unlock_irqrestore(&sdev->base.event_lock, irqflags);
	}

	return 0;
}

static const struct drm_crtc_funcs sisvga_crtc_funcs = {
	.cursor_set = sisvga_crtc_cursor_set,
	.cursor_move = sisvga_crtc_cursor_move,
	.gamma_set = sisvga_crtc_gamma_set,
	.set_config = drm_crtc_helper_set_config,
	.destroy = sisvga_crtc_destroy,
	.page_flip = sisvga_crtc_page_flip,
};

/*
 * struct sisvga_crtc
 */

static int map_crtc_registers(struct sisvga_device *sdev)
{
	static const u8 mask = 0x01;
	u8 newmisc, misc;
	int i = 0;

	/* CRTC registers can be mapped in two separate locations: 0x3b? for
	 * compatibility with monochrome adapters and 0x3d? for compatibility
	 * with color adapters. We always use the latter. The loop waits
	 * several microseconds for success if neccessary. */

	misc = RREG8(REG_MISC_IN);

	newmisc = misc | 0x01;

	do {
		if ((misc & mask) == (newmisc & mask))
			return 0;
		if (i)
			udelay(10);
		else
			WREG8(REG_MISC_OUT, newmisc);
		misc = RREG8(REG_MISC_IN);
	} while (i++ < 5);

	DRM_ERROR("sisvga: failed to map CRTC registers\n");

	return -ETIMEDOUT;
}

static int unlock_crtc_registers(struct sisvga_device *sdev)
{
	u8 cr11;
	int i = 0;

	/* Some of the CRTC registers might be write protected. We unprotect
	 * them here, or assume the device is not compatible. The loop waits
	 * several microseconds for success if necessary.*/

	RREG_CR(0x11, cr11);

	do {
		if (!(cr11 & 0x80))
			return 0;
		if (i)
			udelay(10);
		else
			WREG_CR(0x11, cr11 & 0x7f);
		RREG_CR(0x11, cr11);
	} while (i++ < 5);

	DRM_ERROR("sisvga: failed to disable CRTC write protection\n");

	return -ETIMEDOUT;
}

static int sisvga_crtc_init(struct sisvga_crtc *sis_crtc,
			    struct drm_device *dev,
			    struct drm_plane *primary_plane,
			    struct drm_plane *cursor_plane)
{
	struct sisvga_device *sdev = dev->dev_private;
	int ret;
	size_t i, j;

	ret = map_crtc_registers(sdev);
	if (ret < 0)
		return ret;
	ret = unlock_crtc_registers(sdev);
	if (ret < 0)
		return ret;

	ret = drm_crtc_init_with_planes(dev, &sis_crtc->base,
					primary_plane, cursor_plane,
					&sisvga_crtc_funcs, NULL);
	if (ret < 0) {
		DRM_ERROR("sisvga: drmcrtc_init_with_planes() failed,"
			  " error %d\n", -ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(sis_crtc->lut); ++i) {
		for (j = 0; j < ARRAY_SIZE(sis_crtc->lut[0]); ++j) {
			sis_crtc->lut[i][j] = i;
		}
	}
	sis_crtc->lut_len = ARRAY_SIZE(sis_crtc->lut);
	sis_crtc->lut_24 = true;
	drm_mode_crtc_set_gamma_size(&sis_crtc->base, sis_crtc->lut_len);

	drm_crtc_helper_add(&sis_crtc->base, &sisvga_crtc_helper_funcs);

	return 0;
}

struct sisvga_crtc* sisvga_crtc_create(struct drm_device *dev,
				       struct drm_plane *primary_plane,
				       struct drm_plane *cursor_plane)
{
	struct sisvga_crtc* sis_crtc;
	int ret;

	sis_crtc = devm_kzalloc(dev->dev, sizeof(*sis_crtc), GFP_KERNEL);
	if (!sis_crtc)
		return ERR_PTR(-ENOMEM);

	ret = sisvga_crtc_init(sis_crtc, dev, primary_plane, cursor_plane);
	if (ret)
		goto err_sisvga_crtc_init;

	return sis_crtc;

err_sisvga_crtc_init:
	devm_kfree(dev->dev, sis_crtc);
	return ERR_PTR(ret);
}
