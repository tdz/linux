/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#include "sisvga_device.h"
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_modeset_helper_vtables.h>
#include "sisvga_vclk.h"

static struct sisvga_connector* sisvga_connector_of(
	struct drm_connector* connector)
{
	return container_of(connector, struct sisvga_connector, base);
}

/*
 * Connector helper funcs
 */

static int sisvga_connector_helper_get_modes_vga(
	struct drm_connector *connector)
{
	struct sisvga_connector *sis_connector;
	struct edid *edid;
	int ret;

	sis_connector = sisvga_connector_of(connector);

	edid = drm_get_edid(connector, &sis_connector->ddc.adapter);
	if (!edid)
		return 0;

	ret = drm_mode_connector_update_edid_property(connector, edid);
	if (ret < 0)
		goto err;

	ret = drm_add_edid_modes(connector, edid);
	if (ret < 0)
		goto err;

	kfree(edid);

	return 0;

err:
	kfree(edid);
	return ret;
}

static int sisvga_connector_helper_mode_valid_vga(
	struct drm_connector *connector,
	struct drm_display_mode *mode)
{
	enum sisvga_vclk vclk;
	int ret;
	struct sisvga_device *sdev = connector->dev->dev_private;
	const struct sisvga_device_info *info = sdev->info;
	const struct sisvga_mode* smode;
	int bpp = info->max_bpp;

	/* validate dotclock */

	if (mode->clock > info->max_clock)
		return MODE_CLOCK_HIGH;
	ret = sisvga_vclk_of_clock(mode->clock, &vclk);
	if (ret < 0)
		return MODE_CLOCK_RANGE;
	if (!(info->supported_vclks & SISVGA_VCLK_BIT(vclk)))
		return MODE_CLOCK_RANGE;

	/* validate display size */

	if (mode->hdisplay > info->max_hdisplay)
		return MODE_VIRTUAL_X;
	if (mode->vdisplay > info->max_vdisplay)
		return MODE_VIRTUAL_Y;

	if (((mode->hdisplay % 8) != 0 ||
	     (mode->hsync_start % 8) != 0 ||
	     (mode->hsync_end % 8) != 0 ||
	     (mode->htotal % 8) != 0) &&
	    ((mode->hdisplay % 9) != 0 ||
	     (mode->hsync_start % 9) != 0 ||
	     (mode->hsync_end % 9) != 0 ||
	     (mode->htotal % 9) != 0)) {
		return MODE_H_ILLEGAL;
	}

	if (mode->hdisplay > info->max_hdisplay ||
	    mode->hsync_start > info->max_hsync_start ||
	    mode->hsync_end > info->max_hsync_end ||
	    mode->htotal > info->max_htotal ||
	    mode->vdisplay > info->max_vdisplay ||
	    mode->vsync_start > info->max_vsync_start ||
	    mode->vsync_end > info->max_vsync_end ||
	    mode->vtotal > info->max_vtotal) {
		return MODE_BAD;
	}

	/* validate memory requirements */

	if (connector->cmdline_mode.specified) {
		if (connector->cmdline_mode.bpp_specified)
			bpp = connector->cmdline_mode.bpp;
	}

	if ((mode->hdisplay * mode->vdisplay * (bpp / 8)) > sdev->vram.size) {
		if (connector->cmdline_mode.specified)
			connector->cmdline_mode.specified = false;
		return MODE_BAD;
	}

	/* see if the mode is supported by the device */

	smode = sisvga_find_compatible_mode(info->vga_modes,
					    info->vga_modes +
					    info->vga_modes_len,
					    mode, bpp);
	if (!smode)
		return MODE_BAD;

	return MODE_OK;
}

static struct drm_encoder* sisvga_connector_helper_best_encoder_vga(
	struct drm_connector *connector)
{
	struct drm_encoder *encoder;
	size_t i;
	size_t len = ARRAY_SIZE(connector->encoder_ids);

	for (i = 0; (i < len) && connector->encoder_ids[i]; ++i) {

		encoder = drm_encoder_find(connector->dev, NULL,
					   connector->encoder_ids[i]);
		if (!encoder) {
			DRM_ERROR("encoder %d not found",
				connector->encoder_ids[i]);
			continue;
		}
		if (encoder->encoder_type != DRM_MODE_ENCODER_DAC)
			continue;

		return encoder;
	}

	return NULL;
}

static const struct drm_connector_helper_funcs sisvga_connector_helper_funcs = {
	.get_modes = sisvga_connector_helper_get_modes_vga,
	.mode_valid = sisvga_connector_helper_mode_valid_vga,
	.best_encoder = sisvga_connector_helper_best_encoder_vga,
};

/*
 * Connector funcs
 */

static enum drm_connector_status sisvga_connector_detect_vga(
	struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static void sisvga_connector_destroy(struct drm_connector *connector)
{
	struct sisvga_connector *sis_connector = sisvga_connector_of(connector);
	struct drm_device *dev = connector->dev;

	sisvga_ddc_fini(&sis_connector->ddc);

	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
	devm_kfree(dev->dev, sis_connector);
}

static const struct drm_connector_funcs sisvga_connector_funcs_vga = {
	.dpms = drm_helper_connector_dpms,
	.detect = sisvga_connector_detect_vga,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = sisvga_connector_destroy,
};

/*
 * struct sisvga_connector
 */

static int sisvga_connector_init_vga(struct sisvga_connector *sis_connector,
				     struct drm_device *dev)
{
	int ret;
	struct drm_connector *connector = &sis_connector->base;

	ret = drm_connector_init(dev, connector, &sisvga_connector_funcs_vga,
				 DRM_MODE_CONNECTOR_VGA);
	if (ret < 0)
		return ret;

	drm_connector_helper_add(connector, &sisvga_connector_helper_funcs);

	ret = sisvga_ddc_init(&sis_connector->ddc, dev);
	if (ret < 0)
		goto err_sisvga_ddc_init;

	ret = drm_connector_register(connector);
	if (ret < 0)
		goto err_drm_connector_register;

	return 0;

err_drm_connector_register:
	sisvga_ddc_fini(&sis_connector->ddc);
err_sisvga_ddc_init:
	drm_connector_cleanup(connector);
	return ret;
}

struct sisvga_connector* sisvga_connector_create_vga(struct drm_device *dev)
{
	struct sisvga_connector *sis_connector;
	int ret;

	sis_connector = devm_kzalloc(dev->dev, sizeof(*sis_connector),
				     GFP_KERNEL);
	if (!sis_connector)
		return ERR_PTR(-ENOMEM);

	ret = sisvga_connector_init_vga(sis_connector, dev);
	if (ret < 0)
		goto err_sisvga_connector_init_vga;

	return sis_connector;

err_sisvga_connector_init_vga:
	devm_kfree(dev->dev, sis_connector);
	return ERR_PTR(ret);
}
