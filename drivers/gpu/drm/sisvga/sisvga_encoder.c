/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#include "sisvga_device.h"
#include <drm/drmP.h>
#include <drm/drm_modeset_helper_vtables.h>
#include "sisvga_debug.h"
#include "sisvga_reg.h"

static struct sisvga_encoder* sisvga_encoder_of(struct drm_encoder *encoder)
{
	return container_of(encoder, struct sisvga_encoder, base);
}

/*
 * DPMS helpers
 */

static void set_encoder_dpms_mode(struct sisvga_device *sdev, int mode)
{
	u8 sr11, cr17;

	RREG_SR(0x11, sr11);
	RREG_CR(0x17, cr17);

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		cr17 |= 0x80; /* sync pulses enabled */
		sr11 &= 0x3f; /* clear power-management mode */
		break;
	case DRM_MODE_DPMS_STANDBY:
		cr17 |= 0x80; /* sync pulses enabled */
		sr11 &= 0x3f; /* clear power-management mode and...*/
		sr11 |= 0x40; /* ...force suspend mode */
		break;
	case DRM_MODE_DPMS_SUSPEND:
		cr17 |= 0x80; /* sync pulses enabled */
		sr11 &= 0x3f; /* clear power-management mode and...*/
		sr11 |= 0x80; /* ...force stand-by mode */
		break;
	case DRM_MODE_DPMS_OFF:
		cr17 &= 0x7f; /* sync pulses disabled */
		sr11 |= 0xc0; /* force off */
		break;
	default:
		DRM_ERROR("sisvga: invalid DPMS mode %d\n", mode);
		return;
	}

	WREG_CR(0x17, cr17);
	WREG_SR(0x11, sr11);
}

/*
 * Encoder helper funcs
 */

static void sisvga_encoder_helper_dpms(struct drm_encoder *encoder, int mode)
{
	set_encoder_dpms_mode(encoder->dev->dev_private, mode);
}

static bool sisvga_encoder_mode_fixup(struct drm_encoder *encoder,
				      const struct drm_display_mode *mode,
				      struct drm_display_mode *adj_mode)
{
	long ret;
	enum sisvga_vclk vclk;
	enum sisvga_freq freq;
	unsigned long num, denum, div, postscal, f;
	struct sisvga_device *sdev = encoder->dev->dev_private;
	const struct sisvga_device_info *info = sdev->info;

	if (mode->clock > info->max_clock)
		return false; /* not enough bandwidth */

	ret = sisvga_vclk_of_clock(adj_mode->clock, &vclk);
	if (ret < 0) {
		/* BUG: We should have detected this in mode_valid(). */
		DRM_INFO("sisvga: unsupported dot clock of %d KHz, error %ld",
			  mode->clock, -ret);
		return false;
	}
	sisvga_vclk_regs(vclk, &freq, &num, &denum, &div, &postscal, &f);

	if (freq == SISVGA_FREQ_14318) {

		/* For modes that use the internal clock generator, we
		 * fixup the display size to better match the requested dot
		 * clock. */

		long f_diff;
		int dots;

		if ((mode->htotal % 9) == 0)
			dots = 9;
		else
			dots = 8;

		f_diff = KHZ_TO_HZ(adj_mode->crtc_clock) - f;
	}

	return true;
}

static void sisvga_encoder_helper_prepare(struct drm_encoder *encoder)
{
	/* We disable the screen to allow for flicker-free
	 * mode switching. */
	set_encoder_dpms_mode(encoder->dev->dev_private, DRM_MODE_DPMS_OFF);

	sisvga_debug_print_regs(encoder->dev->dev_private);
	sisvga_debug_print_mode(encoder->dev->dev_private);
}

static void sisvga_encoder_helper_commit(struct drm_encoder *encoder)
{
	set_encoder_dpms_mode(encoder->dev->dev_private, DRM_MODE_DPMS_ON);

	sisvga_debug_print_regs(encoder->dev->dev_private);
	sisvga_debug_print_mode(encoder->dev->dev_private);
}

static void sisvga_encoder_helper_mode_set(struct drm_encoder *encoder,
					   struct drm_display_mode *mode,
					   struct drm_display_mode *adj_mode)
{
}

static void sisvga_encoder_helper_disable(struct drm_encoder *encoder)
{
	set_encoder_dpms_mode(encoder->dev->dev_private, DRM_MODE_DPMS_OFF);
}

static void sisvga_encoder_helper_enable(struct drm_encoder *encoder)
{
	set_encoder_dpms_mode(encoder->dev->dev_private, DRM_MODE_DPMS_ON);
}

static const struct drm_encoder_helper_funcs sisvga_encoder_helper_funcs = {
	.dpms = sisvga_encoder_helper_dpms,
	.mode_fixup = sisvga_encoder_mode_fixup,
	.prepare = sisvga_encoder_helper_prepare,
	.commit = sisvga_encoder_helper_commit,
	.mode_set = sisvga_encoder_helper_mode_set,
	.disable = sisvga_encoder_helper_disable,
	.enable = sisvga_encoder_helper_enable,
};

/*
 * Encoder funcs
 */

static void sisvga_encoder_destroy(struct drm_encoder *encoder)
{
	struct sisvga_encoder *sis_encoder = sisvga_encoder_of(encoder);
	struct drm_device *dev = encoder->dev;

	drm_encoder_cleanup(&sis_encoder->base);
	devm_kfree(dev->dev, sis_encoder);
}

static const struct drm_encoder_funcs sisvga_encoder_funcs = {
	.destroy = sisvga_encoder_destroy,
};

/*
 * struct sis_encoder
 */

static int sisvga_encoder_init(struct sisvga_encoder *sis_encoder,
			int encoder_type,
			struct drm_device *dev)
{
	int ret;
	struct drm_encoder *encoder = &sis_encoder->base;

	ret = drm_encoder_init(dev, encoder, &sisvga_encoder_funcs,
			       encoder_type, NULL);
	if (ret < 0)
		return ret;

	drm_encoder_helper_add(encoder, &sisvga_encoder_helper_funcs);

	return 0;
}

struct sisvga_encoder* sisvga_encoder_create(int encoder_type,
					     struct drm_device *dev)
{
	struct sisvga_encoder *sis_encoder;
	int ret;

	sis_encoder = devm_kzalloc(dev->dev, sizeof(*sis_encoder),
				   GFP_KERNEL);
	if (!sis_encoder)
		return ERR_PTR(-ENOMEM);

	ret = sisvga_encoder_init(sis_encoder, DRM_MODE_ENCODER_DAC, dev);
	if (ret)
		goto err_sisvga_encoder_init;

	return sis_encoder;

err_sisvga_encoder_init:
	devm_kfree(dev->dev, sis_encoder);
	return ERR_PTR(ret);
}
