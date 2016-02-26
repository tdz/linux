/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#include "sisvga_device.h"

static struct sisvga_plane* sisvga_plane_of(struct drm_plane* plane)
{
	return container_of(plane, struct sisvga_plane, base);
}

/*
 * Plane funcs
 */

static int sisvga_plane_funcs_update_plane(struct drm_plane *plane,
				struct drm_crtc *crtc,
                  		struct drm_framebuffer *fb,
				int crtc_x, int crtc_y,
                  		unsigned int crtc_w, unsigned int crtc_h,
                  		uint32_t src_x, uint32_t src_y,
                  		uint32_t src_w, uint32_t src_h,
                  		struct drm_modeset_acquire_ctx *ctx)
{
	return 0;
}

static int sisvga_plane_funcs_disable_plane(
	struct drm_plane *plane, struct drm_modeset_acquire_ctx *ctx)
{
        return 0;
}

static void sisvga_plane_funcs_destroy(struct drm_plane *plane)
{
	struct sisvga_plane* sis_plane = sisvga_plane_of(plane);
	struct drm_device *dev = plane->dev;

	drm_plane_cleanup(&sis_plane->base);
	devm_kfree(dev->dev, sis_plane);
}

static int sisvga_plane_funcs_set_property(struct drm_plane *plane,
        				   struct drm_property *property,
                			   uint64_t value)
{
	DRM_INFO("property: %s\n", property->name);

        return 0;
}

static const struct drm_plane_funcs sisvga_plane_funcs = {
        .update_plane = sisvga_plane_funcs_update_plane,
        .disable_plane = sisvga_plane_funcs_disable_plane,
        .destroy = sisvga_plane_funcs_destroy,
        .set_property = sisvga_plane_funcs_set_property,
};

/*
 * struct sisvga_plane
 */

static int sisvga_plane_init(struct sisvga_plane* sis_plane,
			     struct drm_device *dev,
		             const uint32_t *formats,
			     unsigned int format_count,
		             const uint64_t *format_modifiers,
		             enum drm_plane_type type)
{
	int ret;

	ret = drm_universal_plane_init(dev, &sis_plane->base, 0,
				       &sisvga_plane_funcs, formats,
				       format_count, format_modifiers, type,
				       NULL);
	if (ret)
		return ret;

	ret = drm_plane_create_zpos_immutable_property(&sis_plane->base, 0);
	if (ret) {
		DRM_ERROR("%s:%d %d\n", __func__, __LINE__, -ret);
		goto err;
	}

	ret = drm_plane_create_rotation_property(&sis_plane->base,
						 DRM_MODE_ROTATE_0,
						 DRM_MODE_ROTATE_0);
	if (ret) {
		DRM_ERROR("%s:%d %d\n", __func__, __LINE__, -ret);
		goto err;
	}

	return 0;

err:
	drm_plane_cleanup(&sis_plane->base);
	return ret;
}

struct sisvga_plane* sisvga_plane_create(struct drm_device *dev,
				         const uint32_t *formats,
					 unsigned int format_count,
				         const uint64_t *format_modifiers,
				         enum drm_plane_type type)
{
	struct sisvga_plane* sis_plane;
	int ret;

	sis_plane = devm_kzalloc(dev->dev, sizeof(*sis_plane), GFP_KERNEL);
	if (!sis_plane)
		return ERR_PTR(-ENOMEM);

	ret = sisvga_plane_init(sis_plane, dev, formats, format_count,
				format_modifiers, type);
	if (ret)
		goto err_sisvga_plane_init;

	return sis_plane;

err_sisvga_plane_init:
	devm_kfree(dev->dev, sis_plane);
	return ERR_PTR(ret);
}
