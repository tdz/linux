/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#include "sisvga_device.h"
#include <drm/drmP.h>
#include <drm/drm_pci.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include "sisvga_reg.h"

static int modify_vga_status(struct sisvga_device *sdev, bool enable)
{
	static const u8 mask = 0x01;
	u8 newvga, vga;
	int i = 0;

	/* Tests and modifies the VGA enable bit. The loop waits
	 * several microseconds for success if necessary. */

	vga = RREG8(REG_VGA);

	if (enable)
		newvga = vga | 0x01;
	else
		newvga = vga & 0xfe;

	do {
		if ((vga & mask) == (newvga & mask))
			return 0;
		if (i)
			udelay(10);
		else
			WREG8(REG_VGA, newvga);
		vga = RREG8(REG_VGA);
	} while (i++ < 50);

	return -ETIMEDOUT;
}

static int enable_vga(struct sisvga_device *sdev)
{
	int ret = modify_vga_status(sdev, true);
	if (ret < 0)
		DRM_ERROR("sisvga: could not enable VGA card");
	return ret;
}

static int disable_vga(struct sisvga_device *sdev)
{
	int ret = modify_vga_status(sdev, false);
	if (ret < 0)
		DRM_ERROR("sisvga: could not disable VGA card");
	return ret;
}

static int modify_device_lock(struct sisvga_device *sdev, u8 wr, u8 rd)
{
	u8 sr05;
	int i = 0;

	/* Tests and modifies the device's lock. The loop waits
	 * several microseconds for success if necessary. */

	RREG_SR(0x05, sr05);

	do {
		if (sr05 == rd)
			return 0;
		if (i)
			udelay(10);
		else
			WREG_SR(0x05, wr);
		RREG_SR(0x05, sr05);
	} while (i++ < 50);

	return -ETIMEDOUT;
}

static int unlock_device(struct sisvga_device *sdev)
{
	int ret = modify_device_lock(sdev, 0x86, 0xa1);
	if (ret < 0)
		DRM_ERROR("sisvga: could not unlock SiS extended registers");
	return ret;
}

static int lock_device(struct sisvga_device *sdev)
{
	int ret = modify_device_lock(sdev, 0x00, 0x21);
	if (ret < 0)
		DRM_ERROR("sisvga: could not lock SiS extended registers");
	return ret;
}

/*
 * sisvga_device::regs
 */

static int sisvga_regs_init(struct sisvga_ioports* regs,
			    struct pci_dev *pdev)
{
	resource_size_t base, size;
	int err;

	base = pci_resource_start(pdev, SISVGA_PCI_BAR_REGS);
	size = pci_resource_len(pdev, SISVGA_PCI_BAR_REGS);

	regs->res = request_region(base, size, "sisvga");
	if (!regs->res) {
		DRM_ERROR("sisvga: can't reserve I/O ports\n");
		return -ENXIO;
	}

	regs->mem = pci_iomap(pdev, SISVGA_PCI_BAR_REGS, 0);
	if (!regs->mem) {
		DRM_ERROR("sisvga: can't map I/O ports\n");
		err = -ENOMEM;
		goto err;
	}

	return 0;

err:
	release_resource(regs->res);
	return err;
}

static void sisvga_regs_fini(struct sisvga_ioports *regs,
			     struct pci_dev *pdev)
{
	pci_iounmap(pdev, regs->mem);
	release_resource(regs->res);
}

/*
 * sisvga_device::mmio
 */

static int sisvga_mmio_init(struct sisvga_devmem *mmio,
			    struct sisvga_device *sdev,
			    struct pci_dev *pdev)
{
	u8 sr0b;
	resource_size_t base, size;

	RREG_SR(0x0b, sr0b);

	sr0b |= 0x60; /* map MMIO range */

	WREG_SR(0x0b, sr0b);

	base = pci_resource_start(pdev, SISVGA_PCI_BAR_MMIO);
	size = pci_resource_len(pdev, SISVGA_PCI_BAR_MMIO);

	mmio->base = base;
	mmio->size = size;
	mmio->mtrr = 0;

	mmio->res = request_mem_region(base, size, "sisvga-mmio");
	if (!mmio->res) {
		DRM_ERROR("sisvga: can't reserve MMIO memory\n");
		return -ENXIO;
	}

	mmio->mem = ioremap(base, size);
	if (!mmio->mem) {
		DRM_ERROR("sisvga: can't map MMIO memory\n");
		return -ENOMEM;
	}

	return 0;
}

static void sisvga_mmio_fini(struct sisvga_devmem *mmio,
			     struct pci_dev *pdev)
{
	iounmap(mmio->mem);
	release_region(mmio->base, mmio->size);
}

/*
 * sisvga_device::vram
 */

static int sisvga_vram_init(struct sisvga_devmem *vram,
			    struct sisvga_device *sdev,
			    struct pci_dev *pdev)
{
	resource_size_t base, size;
	u8 misc, gr06, sr06, sr0b, sr0c, sr20, sr21, sr2d;
	unsigned long sizeexp;

	base = pci_resource_start(pdev, SISVGA_PCI_BAR_VRAM);
	size = pci_resource_len(pdev, SISVGA_PCI_BAR_VRAM);

	misc = RREG8(REG_MISC_IN);
	RREG_GR(0x06, gr06);
	RREG_SR(0x06, sr06);
	RREG_SR(0x0b, sr0b);
	RREG_SR(0x0c, sr0c);
	RREG_SR(0x20, sr20);
	RREG_SR(0x21, sr21);
	RREG_SR(0x2d, sr2d);

	misc |= 0x02 | /* allow CPU access to vram */
		0x01; /* Color-graphics memory range enabled */
	gr06 &= 0xf3; /* select memory map 0 */
	sr06 |= 0x80; /* Linear addressing enabled */
	sr0b |= 0x20 | /* map video memory */
	        0x01; /* CPU-driven bitblt enabled */
	sr0c |= 0x80; /* Graphics-mode 32-bit memory access enabled */
	sr20 = (base & 0x07f80000) >> 19; /* Linear addressing base */
	/* We're mapping the VRAM size to a 3-bit field in sr21, such
	 * that
	 *
	 *     size = 2 ^ sr21[7:5] * 512 * 1024
	 *
	 * 512 KiB maps to 0x00, 1 MiB maps to 0x20, 2 maps to 0x04,
	 * and so on. The manual only specifies bits [6:5], but bit
	 * 7 works as well. This way, cards with more than 4 MiB of
	 * VRAM, such as the Diamond Speedstar A70, are supported.
	 *
	 * If the device fits the spec, (i.e., has at most 4 MiB of
	 * VRAM) we do the safe thing and preserve the reserved bit
	 * no 7.
	 */
	sizeexp = ((size >> 20) - 1);
	if (size > KiB_TO_BYTE(4096)) {
		sr21 = sizeexp << 5;
	} else {
		sr21 &= 0x80; /* preserve reserved bits */
		sr21 |= (sizeexp & 0x03) << 5;
	}
	sr21 |= (base & 0xf8000000) >> 27; /* Linear addressing base */
	sr2d &= 0xf0; /* preserve reserved bits */
	sr2d |= 0x00; /* TODO: page size depends on bus type! */

	WREG8(REG_MISC_OUT, misc);
	WREG_GR(0x06, gr06);
	WREG_SR(0x06, sr06);

	//WREG_SR(0x0b, sr0b);
	//WREG_SR(0x0c, sr0c);
	WREG_SR(0x20, sr20);
	WREG_SR(0x21, sr21);
	//WREG_SR(0x2d, sr2d);

        arch_io_reserve_memtype_wc(base, size);

	vram->base = base;
	vram->size = size;
	vram->mtrr = arch_phys_wc_add(base, size);

	vram->res = request_mem_region(base, size, "sisvga-vram");
	if (!vram->res) {
		DRM_ERROR("sisvga: can't reserve vram registers\n");
		return -ENXIO;
	}

	vram->mem = ioremap_wc(base, size);
	if (!vram->mem) {
		DRM_ERROR("sisvga: can't map video ram\n");
		return -ENOMEM;
	}

	return 0;
}

static void sisvga_vram_fini(struct sisvga_devmem *vram,
			     struct pci_dev *pdev)
{
        arch_io_free_memtype_wc(vram->base, vram->size);
	arch_phys_wc_del(vram->mtrr);

	iounmap(vram->mem);
	release_region(vram->base, vram->size);
}

/*
 * Mode-config funcs
 */

static struct drm_framebuffer* sisvga_mode_config_funcs_fb_create(
	struct drm_device *dev, struct drm_file *filp,
	const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct drm_gem_object *gem_obj;
	struct sisvga_framebuffer *sis_fb;
	int ret;

	gem_obj = drm_gem_object_lookup(filp, mode_cmd->handles[0]);
	if (!gem_obj)
		return ERR_PTR(-ENOENT);

	sis_fb = sisvga_framebuffer_create(dev, gem_obj, mode_cmd);
	if (IS_ERR(sis_fb)) {
		ret = PTR_ERR(sis_fb);
		goto err_sisvga_framebuffer_create;
	}

	drm_gem_object_put_unlocked(gem_obj);

	return &sis_fb->base;

err_sisvga_framebuffer_create:
	drm_gem_object_put_unlocked(gem_obj);
	return ERR_PTR(ret);
}

static const struct drm_mode_config_funcs sisvga_mode_config_funcs = {
	.fb_create = sisvga_mode_config_funcs_fb_create,
};

static int sisvga_mode_config_init(struct sisvga_device *sdev)
{
	static const uint32_t primary_formats[] = {
		DRM_FORMAT_RGB888,
		DRM_FORMAT_BGR888,
		DRM_FORMAT_RGB565,
		DRM_FORMAT_C8
	};
	static const uint32_t cursor_formats[] = {
		DRM_FORMAT_XRGB8888
	};
	static const uint64_t format_modifiers[] = {
		DRM_FORMAT_MOD_INVALID
	};

	int ret;
	struct sisvga_plane *sis_primary;
	struct sisvga_plane *sis_cursor;
	struct sisvga_crtc *sis_crtc;
	struct sisvga_encoder *sis_encoder;
	struct sisvga_connector *sis_connector;

	drm_mode_config_init(&sdev->base);
	sdev->base.mode_config.max_width = sdev->info->max_htotal;
	sdev->base.mode_config.max_height = sdev->info->max_vtotal;
	sdev->base.mode_config.funcs = &sisvga_mode_config_funcs;
	sdev->base.mode_config.fb_base = sdev->vram.base;
	sdev->base.mode_config.preferred_depth = sdev->info->preferred_bpp;

	/* Planes */

	sis_primary = sisvga_plane_create(&sdev->base, primary_formats,
					  ARRAY_SIZE(primary_formats),
					  format_modifiers,
					  DRM_PLANE_TYPE_PRIMARY);
	if (IS_ERR(sis_primary)) {
		ret = PTR_ERR(sis_primary);
		goto err_sisvga_plane_create_primary;
	}

	sis_cursor = sisvga_plane_create(&sdev->base, cursor_formats,
					 ARRAY_SIZE(cursor_formats),
					 format_modifiers,
					 DRM_PLANE_TYPE_CURSOR);
	if (IS_ERR(sis_cursor)) {
		ret = PTR_ERR(sis_cursor);
		goto err_sisvga_plane_create_cursor;
	}

	/* CRTCs */

	sis_crtc = sisvga_crtc_create(&sdev->base, &sis_primary->base,
				      &sis_cursor->base);
	if (IS_ERR(sis_crtc)) {
		ret = PTR_ERR(sis_crtc);
		goto err_sisvga_crtc_create;
	}

	/* Encoders */

	sis_encoder = sisvga_encoder_create(DRM_MODE_ENCODER_DAC,
					    &sdev->base);
	if (IS_ERR(sis_encoder)) {
		ret = PTR_ERR(sis_encoder);
		goto err_sisvga_encoder_create;
	}

	sis_encoder->base.possible_crtcs = (1ul << sis_crtc->base.index);

	/* Connectors */

	sis_connector = sisvga_connector_create_vga(&sdev->base);
	if (IS_ERR(sis_connector)) {
		ret = PTR_ERR(sis_connector);
		goto err_sisvga_connector_create_vga;
	}

	ret = drm_mode_connector_attach_encoder(&sis_connector->base,
						&sis_encoder->base);
	if (ret < 0)
		goto err_drm_mode_connector_attach_encoder;

	return 0;

err_drm_mode_connector_attach_encoder:
	/* fall through */
err_sisvga_connector_create_vga:
	/* fall through */
err_sisvga_encoder_create:
	/* fall through */
err_sisvga_crtc_create:
	/* fall through */
err_sisvga_plane_create_cursor:
	/* fall through */
err_sisvga_plane_create_primary:
	drm_mode_config_cleanup(&sdev->base);
	return ret;
}

/*
 * struct sisvga_device
 */

int sisvga_device_init(struct sisvga_device *sdev,
		       struct drm_driver *driver, struct pci_dev *pdev,
		       const struct sisvga_device_info *info)
{
	int ret;

	/* DRM initialization
	 */

	ret = drm_dev_init(&sdev->base, driver, &pdev->dev);
	if (ret)
		return ret;
	sdev->base.dev_private = sdev;
	sdev->base.pdev = pdev;

        if (drm_core_check_feature(&sdev->base, DRIVER_USE_AGP)) {
                if (pci_find_capability(sdev->base.pdev, PCI_CAP_ID_AGP))
                        sdev->base.agp = drm_agp_init(&sdev->base);
                if (sdev->base.agp) {
                        sdev->base.agp->agp_mtrr = arch_phys_wc_add(
                                sdev->base.agp->agp_info.aper_base,
                                sdev->base.agp->agp_info.aper_size *
                                1024 * 1024);
                }
        }

	/* Make VGA and extended registers available. Later initialization
	 * requires registers, so this has to be done first.
	 */

	sdev->info = info;

	ret = sisvga_regs_init(&sdev->regs, sdev->base.pdev);
	if (ret < 0)
		goto err_sisvga_regs_init;

	ret = enable_vga(sdev);
	if (ret < 0)
		goto err_enable_vga;

	ret = unlock_device(sdev);
	if (ret < 0)
		goto err_unlock_device;

	/* The MMIO region can contain VGA memory or command buffer. We
	 * always map the latter.
	 */

	ret = sisvga_mmio_init(&sdev->mmio, sdev, sdev->base.pdev);
	if (ret < 0)
		goto err_sisvga_mmio_init;

	/* Next is memory management. We set-up the framebuffer memory and
	 * memory manager for the card.
	 */

	ret = sisvga_vram_init(&sdev->vram, sdev, sdev->base.pdev);
	if (ret < 0)
		goto err_sisvga_vram_init;

	ret = sisvga_ttm_init(&sdev->ttm, &sdev->base,
			      sdev->vram.size >> PAGE_SHIFT);
	if (ret < 0)
		goto err_sisvga_ttm_init;

	/* One by one, we enable all stages of the display pipeline and
	 * connect them with each other.
	 */

	ret = sisvga_mode_config_init(sdev);
	if (ret < 0)
		goto err_sisvga_mode_config_init;

	/* With the display pipeline running, we can now start fbdev
	 * emulation. This will also enable a framebuffer console, if
	 * configured.
	 */

	ret = sisvga_fbdev_init(&sdev->fbdev, &sdev->base,
				info->preferred_bpp);
	if (ret < 0)
		goto err_sisvga_fbdev_init;

	return 0;

err_sisvga_fbdev_init:
	drm_mode_config_cleanup(&sdev->base);
err_sisvga_mode_config_init:
	sisvga_ttm_fini(&sdev->ttm);
err_sisvga_ttm_init:
	sisvga_vram_fini(&sdev->vram, sdev->base.pdev);
err_sisvga_vram_init:
	sisvga_mmio_fini(&sdev->mmio, sdev->base.pdev);
err_sisvga_mmio_init:
	lock_device(sdev);
err_unlock_device:
	disable_vga(sdev);
err_enable_vga:
	sisvga_regs_fini(&sdev->regs, sdev->base.pdev);
err_sisvga_regs_init:
	drm_dev_fini(&sdev->base);
	return ret;
}

void sisvga_device_fini(struct sisvga_device *sdev)
{
	struct drm_device *dev = &sdev->base;

	sisvga_fbdev_fini(&sdev->fbdev);

	drm_mode_config_cleanup(dev);

	sisvga_ttm_fini(&sdev->ttm);
	sisvga_vram_fini(&sdev->vram, dev->pdev);
	sisvga_mmio_fini(&sdev->mmio, dev->pdev);

	lock_device(sdev);
	disable_vga(sdev);

	sisvga_regs_fini(&sdev->regs, dev->pdev);

	dev->dev_private = NULL;
}

int sisvga_device_mmap(struct sisvga_device* sdev, struct file *filp,
		       struct vm_area_struct *vma)
{
	return ttm_bo_mmap(filp, vma, &sdev->ttm.bdev);
}

int sisvga_device_create_dumb(struct sisvga_device *sdev,
			      struct drm_file *file,
			      struct drm_mode_create_dumb *args)
{
	int ret;
	struct drm_gem_object *gobj;
	u32 handle;

	args->pitch = args->width * ((args->bpp + 7) / 8);
	args->size = args->pitch * args->height;

	ret = sisvga_gem_create(&sdev->base, args->size, false, &gobj);
	if (ret)
		return ret;

	ret = drm_gem_handle_create(file, gobj, &handle);
	drm_gem_object_put_unlocked(gobj);
	if (ret)
		return ret;

	args->handle = handle;
	return 0;
}

int sisvga_device_mmap_dumb_offset(struct sisvga_device *sdev,
				   struct drm_file *file, uint32_t handle,
				   uint64_t *offset)
{
	struct drm_gem_object *obj;
	struct sisvga_bo *sis_bo;

	obj = drm_gem_object_lookup(file, handle);
	if (obj == NULL)
		return -ENOENT;

	sis_bo = gem_to_sisvga_bo(obj);
	*offset = sisvga_bo_mmap_offset(sis_bo);

	drm_gem_object_put_unlocked(obj);
	return 0;
}
