/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#include "sisvga_device.h"
#include <drm/drmP.h>
#include <drm/ttm/ttm_page_alloc.h>
#include "sisvga_reg.h"

#undef VRAM_TTM_MEM
#define VRAM_TTM_MEM(__ttm)	((__ttm)->mem_global_ref)

#undef VRAM_TTM_BO
#define VRAM_TTM_BO(__ttm)	((__ttm)->bo_global_ref)

#undef VRAM_TTM_BO_DEV
#define VRAM_TTM_BO_DEV(__ttm)	((__ttm)->bdev)

static struct sisvga_device* sisvga_device_of_bo_device(
	struct ttm_bo_device *bdev)
{
	return container_of(bdev, struct sisvga_device, ttm.bdev);
}

static struct sisvga_bo* sisvga_bo_of_ttm_buffer_object(
	struct ttm_buffer_object *bo)
{
	return container_of(bo, struct sisvga_bo, bo);
}

/*
 * TTM global memory
 */

static int sisvga_global_ttm_mem_init(struct drm_global_reference *ref)
{
	return ttm_mem_global_init(ref->object);
}

static void sisvga_global_ttm_mem_release(struct drm_global_reference *ref)
{
	ttm_mem_global_release(ref->object);
}

static int sisvga_init_ttm_mem(struct sisvga_ttm *ttm)
{
	int ret;

	VRAM_TTM_MEM(ttm).global_type = DRM_GLOBAL_TTM_MEM;
	VRAM_TTM_MEM(ttm).size = sizeof(struct ttm_mem_global);
	VRAM_TTM_MEM(ttm).init = &sisvga_global_ttm_mem_init;
	VRAM_TTM_MEM(ttm).release = &sisvga_global_ttm_mem_release;

	ret = drm_global_item_ref(&VRAM_TTM_MEM(ttm));
	if (ret < 0) {
		DRM_ERROR("sisvga: setup of TTM memory accounting failed\n");
		return ret;
	}

	return 0;
}

/*
 * TTM global BO
 */

static int sisvga_init_ttm_bo(struct sisvga_ttm* ttm)
{
	int ret;

	VRAM_TTM_BO(ttm).mem_glob = VRAM_TTM_MEM(ttm).object;
	VRAM_TTM_BO(ttm).ref.global_type = DRM_GLOBAL_TTM_BO;
	VRAM_TTM_BO(ttm).ref.size = sizeof(struct ttm_bo_global);
	VRAM_TTM_BO(ttm).ref.init = &ttm_bo_global_init;
	VRAM_TTM_BO(ttm).ref.release = &ttm_bo_global_release;

	ret = drm_global_item_ref(&VRAM_TTM_BO(ttm).ref);
	if (ret < 0) {
		DRM_ERROR("sisvga: setup of TTM BO failed\n");
		return ret;
	}

	return 0;
}

/*
 * TTM BO device
 */

static void sisvga_ttm_backend_destroy(struct ttm_tt *tt)
{
	ttm_tt_fini(tt);
	kfree(tt);
}

static struct ttm_backend_func sisvga_ttm_backend_func = {
	.destroy = sisvga_ttm_backend_destroy
};

static struct ttm_tt *sisvga_ttm_tt_create(struct ttm_buffer_object *bo,
					   uint32_t page_flags)
{
	struct ttm_tt *tt;
	int ret;

	tt = kzalloc(sizeof(*tt), GFP_KERNEL);
	if (tt == NULL)
		return NULL;

	tt->func = &sisvga_ttm_backend_func;

	ret = ttm_tt_init(tt, bo, page_flags);
	if (ret < 0) {
		kfree(tt);
		return NULL;
	}

	return tt;
}

static int sisvga_bo_init_mem_type(struct ttm_bo_device *bdev, uint32_t type,
		 		   struct ttm_mem_type_manager *man)
{
	switch (type) {
	case TTM_PL_SYSTEM:
		man->flags = TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_MASK_CACHING;
		man->default_caching = TTM_PL_FLAG_CACHED;
		break;
	case TTM_PL_VRAM:
		man->func = &ttm_bo_manager_func;
		man->flags = TTM_MEMTYPE_FLAG_FIXED |
			     TTM_MEMTYPE_FLAG_MAPPABLE;
		man->available_caching = TTM_PL_FLAG_UNCACHED |
					 TTM_PL_FLAG_WC;
		man->default_caching = TTM_PL_FLAG_WC;
		break;
	default:
		DRM_ERROR("sisvga: Unsupported memory type %u\n", (unsigned)type);
		return -EINVAL;
	}
	return 0;
}

static void sisvga_bo_evict_flags(struct ttm_buffer_object *bo,
				  struct ttm_placement *pl)
{
	/* TODO */
	DRM_INFO("%s:%d TODO\n", __func__, __LINE__);
}

static int sisvga_bo_verify_access(struct ttm_buffer_object *bo,
				   struct file *filp)
{
	struct sisvga_bo *sis_bo = sisvga_bo_of_ttm_buffer_object(bo);

	return drm_vma_node_verify_access(&sis_bo->gem.vma_node,
					  filp->private_data);
}

static int sisvga_ttm_io_mem_reserve(struct ttm_bo_device *bdev,
				     struct ttm_mem_reg *mem)
{
	struct ttm_mem_type_manager *man = bdev->man + mem->mem_type;
	struct sisvga_device *sdev = sisvga_device_of_bo_device(bdev);

	if (!(man->flags & TTM_MEMTYPE_FLAG_MAPPABLE))
		return -EINVAL;

	mem->bus.addr = NULL;
	mem->bus.size = mem->num_pages << PAGE_SHIFT;

	switch (mem->mem_type) {
	case TTM_PL_SYSTEM:	/* nothing to do */
		mem->bus.offset = 0;
		mem->bus.base = 0;
		mem->bus.is_iomem = false;
		break;
	case TTM_PL_VRAM:
		mem->bus.offset = mem->start << PAGE_SHIFT;
		mem->bus.base = sdev->vram.base; // pci_resource_start(mdev->dev->pdev, 0);
		mem->bus.is_iomem = true;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void sisvga_ttm_io_mem_free(struct ttm_bo_device *bdev,
				   struct ttm_mem_reg *mem)
{
	/* TODO */
	DRM_INFO("%s:%d TODO\n", __func__, __LINE__);
}

static struct ttm_bo_driver sisvga_bo_driver = {
	.ttm_tt_create = sisvga_ttm_tt_create,
	.ttm_tt_populate = ttm_pool_populate,
	.ttm_tt_unpopulate = ttm_pool_unpopulate,
	.init_mem_type = sisvga_bo_init_mem_type,
	.evict_flags = sisvga_bo_evict_flags,
	.verify_access = sisvga_bo_verify_access,
	.io_mem_reserve = sisvga_ttm_io_mem_reserve,
	.io_mem_free = sisvga_ttm_io_mem_free,
};

static int sisvga_init_ttm_bo_device(struct sisvga_ttm *ttm,
				     struct drm_device *dev,
				     unsigned long p_size)
{
	int ret;

	ret = ttm_bo_device_init(&VRAM_TTM_BO_DEV(ttm),
				 VRAM_TTM_BO(ttm).ref.object,
				 &sisvga_bo_driver,
				 dev->anon_inode->i_mapping,
				 DRM_FILE_PAGE_OFFSET,
				 true);
	if (ret) {
		DRM_ERROR("sisvga: ttm_bo_device_init failed; %d\n", ret);
		return ret;
	}

	ret = ttm_bo_init_mm(&VRAM_TTM_BO_DEV(ttm), TTM_PL_VRAM, p_size);
	if (ret) {
		DRM_ERROR("sisvga: ttm_bo_init_mm failed: %d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * struct sisvga_ttm
 */

int sisvga_ttm_init(struct sisvga_ttm *ttm,
		    struct drm_device *dev,
		    unsigned long p_size)
{
	int ret;

	ret = sisvga_init_ttm_mem(ttm);
	if (ret < 0)
		return ret;

	ret = sisvga_init_ttm_bo(ttm);
	if (ret < 0)
		goto err_init_ttm_bo;

	ret = sisvga_init_ttm_bo_device(ttm, dev, p_size);
	if (ret < 0)
		goto err_init_ttm_bo_device;

	return 0;

err_init_ttm_bo_device:
	ttm_mem_global_release(VRAM_TTM_BO(ttm).ref.object);
err_init_ttm_bo:
	ttm_mem_global_release(VRAM_TTM_MEM(ttm).object);
	return ret;
}

void sisvga_ttm_fini(struct sisvga_ttm *ttm)
{
	ttm_bo_device_release(&VRAM_TTM_BO_DEV(ttm));
	ttm_mem_global_release(VRAM_TTM_BO(ttm).ref.object);
	ttm_mem_global_release(VRAM_TTM_MEM(ttm).object);
}
