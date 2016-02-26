/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#include "sisvga_device.h"
#include <drm/drmP.h> /* include before drm/drm_gem.h */
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem.h>

/*
 * Framebuffer helpers
 */

static void sisvga_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct sisvga_framebuffer *sis_fb = sisvga_framebuffer_of(fb);
	struct drm_device *dev = fb->dev;
	struct drm_gem_object *gem_obj = sis_fb->gem_obj;

	drm_framebuffer_cleanup(&sis_fb->base);
	devm_kfree(dev->dev, sis_fb);

	if (gem_obj)
		drm_gem_object_put_unlocked(gem_obj);
}

static const struct drm_framebuffer_funcs sisvga_framebuffer_funcs = {
	.destroy = sisvga_framebuffer_destroy,
};

/*
 * struct sisvga_framebuffer
 */

static int sisvga_framebuffer_init(struct sisvga_framebuffer *sis_fb,
				   struct drm_device *dev,
				   struct drm_gem_object *gem_obj,
				   const struct drm_mode_fb_cmd2 *mode_cmd)
{
	int ret;

	if (gem_obj)
		drm_gem_object_get(gem_obj);
	sis_fb->gem_obj = gem_obj;

	drm_helper_mode_fill_fb_struct(dev, &sis_fb->base, mode_cmd);

	ret = drm_framebuffer_init(dev, &sis_fb->base,
				   &sisvga_framebuffer_funcs);
	if (ret < 0) {
		DRM_ERROR("drm_framebuffer_init failed: %d\n", ret);
		goto err_drm_framebuffer_init;
	}

	return 0;

err_drm_framebuffer_init:
	if (gem_obj)
		drm_gem_object_put_unlocked(gem_obj);
	return ret;
}

struct sisvga_framebuffer* sisvga_framebuffer_create(
	struct drm_device *dev,
	struct drm_gem_object *gem_obj,
	const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct sisvga_framebuffer *sis_fb;
	int ret;

	sis_fb = devm_kzalloc(dev->dev, sizeof(*sis_fb), GFP_KERNEL);
	if (!sis_fb)
		return ERR_PTR(-ENOMEM);

	ret = sisvga_framebuffer_init(sis_fb, dev, gem_obj, mode_cmd);
	if (ret)
		goto err_sisvga_framebuffer_init;

	return sis_fb;

err_sisvga_framebuffer_init:
	devm_kfree(dev->dev, sis_fb);
	return ERR_PTR(ret);
}

struct sisvga_framebuffer* sisvga_framebuffer_of(struct drm_framebuffer *fb)
{
	return container_of(fb, struct sisvga_framebuffer, base);
}
