/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#include "sisvga_device.h"
#include <drm/drm_crtc_helper.h>
#include <drm/drmP.h>

/*
 * FB ops
 */

static void memcpy_to(void* dst, const void* src, size_t size, bool is_iomem)
{
	if (is_iomem)
		memcpy_toio(dst, src, size);
	else
		memcpy(dst, src, size);
}

static void sisvga_fbdev_dirty_update(struct sisvga_fbdev *sis_fbdev,
				      int x, int y, int width, int height)
{
	int i;
	struct sisvga_bo *bo;
	int src_offset, dst_offset;
	int bpp = sis_fbdev->fb->base.format->cpp[0];
	int ret = -EBUSY;
	bool store_for_later = false;
	int x2, y2;
	unsigned long flags;
	const u8 *src;
	bool is_iomem;
	u8 *dst;

	bo = gem_to_sisvga_bo(sis_fbdev->fb->gem_obj);

	/*
	 * try and reserve the BO, if we fail with busy
	 * then the BO is being moved and we should
	 * store up the damage until later.
	 */
	if (drm_can_sleep())
		ret = sisvga_bo_reserve(bo, true);
	if (ret) {
		if (ret != -EBUSY)
			return;

		store_for_later = true;
	}

	x2 = x + width - 1;
	y2 = y + height - 1;
	spin_lock_irqsave(&sis_fbdev->dirty_lock, flags);

	if (sis_fbdev->y1 < y)
		y = sis_fbdev->y1;
	if (sis_fbdev->y2 > y2)
		y2 = sis_fbdev->y2;
	if (sis_fbdev->x1 < x)
		x = sis_fbdev->x1;
	if (sis_fbdev->x2 > x2)
		x2 = sis_fbdev->x2;

	if (store_for_later) {
		sis_fbdev->x1 = x;
		sis_fbdev->x2 = x2;
		sis_fbdev->y1 = y;
		sis_fbdev->y2 = y2;
		spin_unlock_irqrestore(&sis_fbdev->dirty_lock, flags);
		return;
	}

	sis_fbdev->x1 = sis_fbdev->y1 = INT_MAX;
	sis_fbdev->x2 = sis_fbdev->y2 = 0;
	spin_unlock_irqrestore(&sis_fbdev->dirty_lock, flags);

	if (!bo->kmap.virtual) {
		ret = ttm_bo_kmap(&bo->bo, 0, bo->bo.num_pages, &bo->kmap);
		if (ret) {
			DRM_ERROR("failed to kmap fb updates\n");
			sisvga_bo_unreserve(bo);
			return;
		}
	}

	dst = ttm_kmap_obj_virtual(&bo->kmap, &is_iomem);
	src = sis_fbdev->helper.fbdev->screen_base;

	for (i = y; i <= y2; i++) {
		/* assume equal stride for now */
		src_offset = i * sis_fbdev->fb->base.pitches[0] + (x * bpp);
		dst_offset = src_offset;
		memcpy_to(dst + dst_offset, src + src_offset,
			  (x2 - x + 1) * bpp, is_iomem);
	}

	sisvga_bo_unreserve(bo);
}

static void sisvga_fb_ops_fillrect(struct fb_info *info,
				   const struct fb_fillrect *rect)
{
	struct sisvga_fbdev *sis_fbdev = info->par;

	drm_fb_helper_sys_fillrect(info, rect);

	sisvga_fbdev_dirty_update(sis_fbdev, rect->dx, rect->dy, rect->width,
				  rect->height);
}

static void sisvga_fb_ops_copyarea(struct fb_info *info,
				   const struct fb_copyarea *area)
{
	struct sisvga_fbdev *sis_fbdev = info->par;

	drm_fb_helper_sys_copyarea(info, area);

	sisvga_fbdev_dirty_update(sis_fbdev, area->dx, area->dy, area->width,
				  area->height);
}

static void sisvga_fb_ops_imageblit(struct fb_info *info,
				    const struct fb_image *image)
{
	struct sisvga_fbdev *sis_fbdev = info->par;

	drm_fb_helper_sys_imageblit(info, image);

	sisvga_fbdev_dirty_update(sis_fbdev, image->dx, image->dy, image->width,
			          image->height);
}

static struct fb_ops sisvga_fb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_fillrect = sisvga_fb_ops_fillrect,
	.fb_copyarea = sisvga_fb_ops_copyarea,
	.fb_imageblit = sisvga_fb_ops_imageblit,
	.fb_pan_display = drm_fb_helper_pan_display,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcmap = drm_fb_helper_setcmap,
};

/*
 * Fbdev helpers
 */

static int sisvga_fb_helper_fb_probe(struct drm_fb_helper *helper,
		    		     struct drm_fb_helper_surface_size *sizes)
{
	struct drm_mode_fb_cmd2 mode_cmd;
	void *sysram;
	u32 size;
	int ret;
	struct drm_gem_object *gem_obj;
	struct sisvga_framebuffer *sis_fb;
	struct fb_info *info;
	struct sisvga_fbdev *sis_fbdev =
		container_of(helper, struct sisvga_fbdev, helper);
	struct drm_device *dev = sis_fbdev->helper.dev;
	struct sisvga_device *sis_dev = dev->dev_private;

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	mode_cmd.pitches[0] = mode_cmd.width * ((sizes->surface_bpp + 7) / 8);
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
							  sizes->surface_depth);

	size = mode_cmd.pitches[0] * mode_cmd.height;

	sysram = vmalloc(size);
	if (!sysram)
		return -ENOMEM;

	ret = sisvga_gem_create(dev, size, true, &gem_obj);
	if (ret)
		goto err_sisvga_gem_create;

	sis_fb = sisvga_framebuffer_create(dev, gem_obj, &mode_cmd);
	if (IS_ERR(sis_fb)) {
		ret = PTR_ERR(sis_fb);
		goto err_sisvga_framebuffer_create;
	}
	sis_fbdev->fb = sis_fb;

	/* setup helper */
	sis_fbdev->helper.fb = &sis_fb->base;

	info = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(info)) {
		ret = PTR_ERR(info);
		goto err_drm_fb_helper_alloc_fbi;
	}

	strcpy(info->fix.id, "sisvga-fb");
	info->par = sis_fbdev;
	info->flags = FBINFO_DEFAULT | FBINFO_CAN_FORCE_OUTPUT;
	info->fbops = &sisvga_fb_ops;
	/* setup aperture base/size for vesafb takeover */
	info->apertures->ranges[0].base = dev->mode_config.fb_base;
	info->apertures->ranges[0].size = sis_dev->vram.size;
	drm_fb_helper_fill_fix(info, sis_fb->base.pitches[0],
			       sis_fb->base.format->depth);
	drm_fb_helper_fill_var(info, &sis_fbdev->helper, sizes->fb_width,
			       sizes->fb_height);
	info->screen_base = sysram;
	info->screen_size = size;
	info->pixmap.flags = FB_PIXMAP_SYSTEM;

	drm_gem_object_put_unlocked(gem_obj);

	return 0;

err_drm_fb_helper_alloc_fbi:
	drm_framebuffer_put(&sis_fb->base);
err_sisvga_framebuffer_create:
	drm_gem_object_put_unlocked(gem_obj);
err_sisvga_gem_create:
	vfree(sysram);
	return ret;
}

static const struct drm_fb_helper_funcs sisvga_fb_helper_funcs = {
	.fb_probe = sisvga_fb_helper_fb_probe,
};

/*
 * struct sisvga_fbdev
 */

int sisvga_fbdev_init(struct sisvga_fbdev *sis_fbdev,
		      struct drm_device *dev, int bpp)
{
	int ret;

	sis_fbdev->x1 = sis_fbdev->y1 = INT_MAX;
	sis_fbdev->x2 = sis_fbdev->y2 = 0;
	spin_lock_init(&sis_fbdev->dirty_lock);

	drm_fb_helper_prepare(dev, &sis_fbdev->helper,
			      &sisvga_fb_helper_funcs);

	ret = drm_fb_helper_init(dev, &sis_fbdev->helper, 1);
	if (ret < 0) {
		DRM_ERROR("sisvga: drm_fb_helper_init() failed,"
			  " error %d\n", -ret);
		return ret;
	}

	ret = drm_fb_helper_single_add_all_connectors(&sis_fbdev->helper);
	if (ret < 0) {
		DRM_ERROR("drm_fb_helper_single_add_all_connectors failed: %d", -ret);
		return ret;
	}

	/* disable all the possible outputs/crtcs before entering KMS mode */
	drm_helper_disable_unused_functions(dev);

	ret = drm_fb_helper_initial_config(&sis_fbdev->helper, bpp);
	if (ret < 3) {
		DRM_ERROR("drm_fb_helper_initial_config failed: %d", -ret);
		return ret;
	}

	return 0;
}

void sisvga_fbdev_fini(struct sisvga_fbdev *sis_fbdev)
{
	void *sysram;
	struct sisvga_framebuffer *sis_fb = sis_fbdev->fb;

	sysram = sis_fbdev->helper.fbdev->screen_base;

        drm_fb_helper_unregister_fbi(&sis_fbdev->helper);
        drm_fb_helper_fini(&sis_fbdev->helper);

	drm_framebuffer_put(&sis_fb->base);

	if (sysram)
		vfree(sysram);
}
