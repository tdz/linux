/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#ifndef SISVGA_DEVICE_H
#define SISVGA_DEVICE_H

/* Set SISVGA_REGS to the standard location for
 * memory-mapped I/O ports. */
#define	SISVGA_REGS \
	((void __iomem *)((sdev)->regs.mem))

#include <drm/drm_crtc.h>
#include <drm/drm_encoder.h>
#include <drm/drm_fb_helper.h>
#include <drm/drmP.h> // required by drm/drm_gem.h
#include <drm/drm_gem.h>
#include <drm/ttm/ttm_bo_driver.h>
#include <linux/compiler.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include "sisvga_drv.h"
#include "sisvga_modes.h"
#include "sisvga_vclk.h"

#define DRM_FILE_PAGE_OFFSET (0x100000000ULL >> PAGE_SHIFT)

#define SISVGA_PCI_BAR_VRAM	0	/* BAR 0 contains VRAM */
#define SISVGA_PCI_BAR_MMIO	1	/* BAR 1 contains MMIO region */
#define SISVGA_PCI_BAR_REGS	2	/* BAR 2 contains VGA registers */

#define SISVGA_LUT_SIZE		256

struct drm_device;
struct drm_file;
struct drm_mode_fb_cmd2;
struct resource;

enum sisvga_model {
	SISVGA_MODEL_6202,
	SISVGA_MODEL_6215,
	SISVGA_MODEL_6326
};

struct sisvga_device_info {

	enum sisvga_model model;

	/* Video RAM */
	resource_size_t max_size; /* Byte */

	/* RAMDAC speed */
	int max_clock; /* KHz */

	/* Bitmask of supported VCLK frequencies */
	unsigned long supported_vclks;

	/* CRTC */
	int max_htotal;
	int max_hsync_start;
	int max_hsync_end;
	int max_hdisplay;
	int max_vtotal;
	int max_vsync_start;
	int max_vsync_end;
	int max_vdisplay;
	int max_bpp;
	int preferred_bpp;

	/* List of all supported VGA modes */
	const struct sisvga_mode* vga_modes;
	size_t vga_modes_len;
};

#define SISVGA_DEVICE_INFO(model_, size_, clock_, supported_vclks_, htotal_,\
			   hsync_start_, hsync_end_, hdisplay_, vtotal_,\
			   vsync_start_, vsync_end_, vdisplay_,\
			   bpp_, preferred_bpp_,\
			   vga_modes_, vga_modes_len_)\
	.model = (model_),\
	.max_size = (size_),\
	.max_clock = (clock_),\
	.supported_vclks = (supported_vclks_),\
	.max_htotal = (htotal_),\
	.max_hsync_start = (hsync_start_),\
	.max_hsync_end = (hsync_end_),\
	.max_hdisplay = (hdisplay_),\
	.max_vtotal = (vtotal_),\
	.max_vsync_start = (vsync_start_),\
	.max_vsync_end = (vsync_end_),\
	.max_vdisplay = (vdisplay_),\
	.max_bpp = (bpp_),\
	.preferred_bpp = (preferred_bpp_),\
	.vga_modes = (vga_modes_),\
	.vga_modes_len = (vga_modes_len_)

#define SISVGA_VCLK_BIT(vclk_) \
    (1ul << (unsigned long)(vclk_))

struct sisvga_ddc {
	struct drm_device *dev;
	u8 scl_mask;
	u8 sda_mask;

	struct i2c_adapter adapter;

	int udelay; /* I2C delay, in usec */
	int timeout; /* I2C timeout, in jiffies */

	/* saved register state */
	u8 sr01;
};

struct sisvga_connector {
	struct drm_connector base;
	struct sisvga_ddc ddc;
};

struct sisvga_plane {
	struct drm_plane base;
};

struct sisvga_crtc {
	struct drm_crtc	base;
	u8		lut[SISVGA_LUT_SIZE][3];
	size_t		lut_len;
	bool		lut_24;
};

struct sisvga_encoder {
	struct drm_encoder base;
};

struct sisvga_framebuffer {
	struct drm_framebuffer base;
	struct drm_gem_object *gem_obj;
};

struct sisvga_fbdev {
	struct drm_fb_helper helper;
	struct sisvga_framebuffer *fb;

	int x1, y1, x2, y2; /* dirty rect */
	spinlock_t dirty_lock;
};

struct sisvga_ioports {
	struct resource	*res;
	void __iomem	*mem;
};

struct sisvga_devmem {
	resource_size_t	base;
	resource_size_t	size;
	int mtrr;
	struct resource	*res;
	void __iomem	*mem;
};

struct sisvga_ttm {
	struct drm_global_reference mem_global_ref;
	struct ttm_bo_global_ref bo_global_ref;
	struct ttm_bo_device bdev;
};

struct sisvga_bo {
	struct ttm_buffer_object bo;
	struct ttm_placement placement;
	struct ttm_bo_kmap_obj kmap;
	struct drm_gem_object gem;

	/* Supported placements are VRAM and SYSTEM */
	struct ttm_place placements[3];
	int pin_count;
};
#define gem_to_sisvga_bo(gobj) container_of((gobj), struct sisvga_bo, gem)

struct sisvga_device {
	struct drm_device	base;

	const struct sisvga_device_info	*info;

	struct sisvga_ioports		regs;
	struct sisvga_devmem		mmio;
	struct sisvga_devmem		vram;

	struct sisvga_ttm		ttm;

	struct sisvga_fbdev	fbdev;
};

#define MODEL_IS_EQ(num_) \
	((sdev)->info->model == SISVGA_MODEL_ ## num_)

#define MODEL_IS_LE(num_) \
	((sdev)->info->model <= SISVGA_MODEL_ ## num_)

#define MODEL_IS_GE(num_) \
	((sdev)->info->model >= SISVGA_MODEL_ ## num_)

#define MODEL_IS_LT(num_) \
	(!MODEL_IS_GE(num_))

#define MODEL_IS_GT(num_) \
	(!MODEL_IS_LE(num_))

int sisvga_device_init(struct sisvga_device *sdev,
		       struct drm_driver *driver, struct pci_dev *pdev,
		       const struct sisvga_device_info *info);
void sisvga_device_fini(struct sisvga_device *sdev);
int sisvga_device_mmap(struct sisvga_device *sdev, struct file *filp,
		       struct vm_area_struct *vma);
int sisvga_device_create_dumb(struct sisvga_device *sdev,
			      struct drm_file *file,
			      struct drm_mode_create_dumb *args);

struct sisvga_connector* sisvga_connector_create_vga(struct drm_device *dev);

struct sisvga_plane* sisvga_plane_create(struct drm_device *dev,
					 const uint32_t *formats,
					 unsigned int format_count,
		      			 const uint64_t *format_modifiers,
		      			 enum drm_plane_type type);

struct sisvga_crtc* sisvga_crtc_create(struct drm_device *dev,
				       struct drm_plane *primary_plane,
				       struct drm_plane *cursor_plane);

int sisvga_ddc_init(struct sisvga_ddc *sis_ddc, struct drm_device *dev);
void sisvga_ddc_fini(struct sisvga_ddc *sis_ddc);

struct sisvga_encoder* sisvga_encoder_create(int encoder_type,
					     struct drm_device *dev);

int sisvga_fbdev_init(struct sisvga_fbdev *sis_fbdev,
		      struct drm_device *dev, int bpp);
void sisvga_fbdev_fini(struct sisvga_fbdev *sis_fbdev);

struct sisvga_framebuffer* sisvga_framebuffer_create(
	struct drm_device *dev,
	struct drm_gem_object *gem_obj,
	const struct drm_mode_fb_cmd2 *mode_cmd);

struct sisvga_framebuffer* sisvga_framebuffer_of(struct drm_framebuffer *fb);

int sisvga_ttm_init(struct sisvga_ttm *ttm,
		    struct drm_device *dev,
		    unsigned long p_size);
void sisvga_ttm_fini(struct sisvga_ttm *ttm);

int sisvga_bo_create(struct drm_device *dev, int size, int align,
		     uint32_t flags, struct sisvga_bo **sis_bo_out);
void sisvga_bo_unref(struct sisvga_bo **sis_bo);
int sisvga_bo_reserve(struct sisvga_bo *bo, bool no_wait);
void sisvga_bo_unreserve(struct sisvga_bo *bo);
u64 sisvga_bo_mmap_offset(struct sisvga_bo *sis_bo);
int sisvga_bo_pin(struct sisvga_bo *bo, u32 pl_flag, u64 *gpu_addr);
int sisvga_bo_unpin(struct sisvga_bo *bo);
int sisvga_bo_push_to_system(struct sisvga_bo *bo);

int sisvga_gem_create(struct drm_device *dev, u32 size, bool iskernel,
		      struct drm_gem_object **obj);

#endif /* SISVGA_DEVICE_H */
