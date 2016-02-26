/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#include "sisvga_device.h"

/*
 * Buffer objects
 */

void sisvga_bo_ttm_placement(struct sisvga_bo *bo, int domain)
{
	u32 c = 0;
	unsigned i;

	bo->placement.placement = bo->placements;
	bo->placement.busy_placement = bo->placements;

	if (domain & TTM_PL_FLAG_VRAM)
		bo->placements[c++].flags = TTM_PL_FLAG_WC | TTM_PL_FLAG_UNCACHED | TTM_PL_FLAG_VRAM;

	if (domain & TTM_PL_FLAG_SYSTEM)
		bo->placements[c++].flags = TTM_PL_MASK_CACHING | TTM_PL_FLAG_SYSTEM;

	if (!c)
		bo->placements[c++].flags = TTM_PL_MASK_CACHING | TTM_PL_FLAG_SYSTEM;

	bo->placement.num_placement = c;
	bo->placement.num_busy_placement = c;

	for (i = 0; i < c; ++i) {
		bo->placements[i].fpfn = 0;
		bo->placements[i].lpfn = 0;
	}
}

static void sisvga_bo_ttm_destroy(struct ttm_buffer_object *tbo)
{
	struct sisvga_bo *sis_bo;

	sis_bo = container_of(tbo, struct sisvga_bo, bo);

	drm_gem_object_release(&sis_bo->gem);
	kfree(sis_bo);
}

int sisvga_bo_create(struct drm_device *dev, int size, int align,
		     uint32_t flags, struct sisvga_bo **sis_bo_out)
{
	struct sisvga_device *sis_dev = dev->dev_private;
	struct sisvga_bo *sis_bo;
	size_t acc_size;
	int ret;

	sis_bo = kzalloc(sizeof(struct sisvga_bo), GFP_KERNEL);
	if (!sis_bo)
		return -ENOMEM;

	ret = drm_gem_object_init(dev, &sis_bo->gem, size);
	if (ret) {
		kfree(sis_bo);
		return ret;
	}

	sis_bo->bo.bdev = &sis_dev->ttm.bdev;

	sisvga_bo_ttm_placement(sis_bo, TTM_PL_FLAG_VRAM | TTM_PL_FLAG_SYSTEM);

	acc_size = ttm_bo_dma_acc_size(&sis_dev->ttm.bdev, size,
				       sizeof(struct sisvga_bo));

	ret = ttm_bo_init(&sis_dev->ttm.bdev, &sis_bo->bo, size,
			  ttm_bo_type_device, &sis_bo->placement,
			  align >> PAGE_SHIFT, false, acc_size,
			  NULL, NULL, sisvga_bo_ttm_destroy);
	if (ret)
		return ret;

	*sis_bo_out = sis_bo;
	return 0;
}

void sisvga_bo_unref(struct sisvga_bo **sis_bo)
{
	struct ttm_buffer_object *tbo;

	if ((*sis_bo) == NULL)
		return;

	tbo = &((*sis_bo)->bo);
	ttm_bo_unref(&tbo);
	*sis_bo = NULL;
}

int sisvga_bo_reserve(struct sisvga_bo *bo, bool no_wait)
{
	int ret;

	ret = ttm_bo_reserve(&bo->bo, true, no_wait, NULL);
	if (ret) {
		if (ret != -ERESTARTSYS && ret != -EBUSY)
			DRM_ERROR("sisvga: ttm_bo_reserve(%p) failed,"
				  " error %d\n", bo, -ret);
		return ret;
	}
	return 0;
}

void sisvga_bo_unreserve(struct sisvga_bo *bo)
{
	ttm_bo_unreserve(&bo->bo);
}

u64 sisvga_bo_mmap_offset(struct sisvga_bo *sis_bo)
{
	return drm_vma_node_offset_addr(&sis_bo->bo.vma_node);
}

int sisvga_bo_pin(struct sisvga_bo *bo, u32 pl_flag, u64 *gpu_addr)
{
	int i, ret;
	struct ttm_operation_ctx ctx = { false, false };

	if (bo->pin_count) {
		bo->pin_count++;
		goto out;
	}

	sisvga_bo_ttm_placement(bo, pl_flag);
	for (i = 0; i < bo->placement.num_placement; i++)
		bo->placements[i].flags |= TTM_PL_FLAG_NO_EVICT;

	ret = ttm_bo_validate(&bo->bo, &bo->placement, &ctx);
	if (ret) {
		DRM_ERROR("sisvga: ttm_bo_validate failed, error %d\n", -ret);
		return ret;
	}

	bo->pin_count = 1;

out:
	if (gpu_addr)
		*gpu_addr = bo->bo.offset;

	return 0;
}

int sisvga_bo_unpin(struct sisvga_bo *bo)
{
	int i;
	struct ttm_operation_ctx ctx = { false, false };

	if (!bo->pin_count) {
		DRM_ERROR("sisvga: BO %p is not pinned \n", bo);
		return 0;
	}
	bo->pin_count--;
	if (bo->pin_count)
		return 0;

	for (i = 0; i < bo->placement.num_placement ; i++)
		bo->placements[i].flags &= ~TTM_PL_FLAG_NO_EVICT;

	return ttm_bo_validate(&bo->bo, &bo->placement, &ctx);
}

int sisvga_bo_push_to_system(struct sisvga_bo *bo)
{
	int i, ret;
	struct ttm_operation_ctx ctx = { false, false };

	if (!bo->pin_count) {
		DRM_ERROR("sisvga: BO %p is not pinned \n", bo);
		return 0;
	}
	bo->pin_count--;
	if (bo->pin_count)
		return 0;

	if (bo->kmap.virtual)
		ttm_bo_kunmap(&bo->kmap);

	sisvga_bo_ttm_placement(bo, TTM_PL_FLAG_SYSTEM);
	for (i = 0; i < bo->placement.num_placement ; i++)
		bo->placements[i].flags |= TTM_PL_FLAG_NO_EVICT;

	ret = ttm_bo_validate(&bo->bo, &bo->placement, &ctx);
	if (ret) {
		DRM_ERROR("sisvga: ttm_bo_validate failed, error %d\n", -ret);
		return ret;
	}

	return 0;
}

/*
 * GEM objects
 */

int sisvga_gem_create(struct drm_device *dev, u32 size, bool iskernel,
		      struct drm_gem_object **obj)
{
	struct sisvga_bo *sis_bo;
	int ret;

	*obj = NULL;

	size = roundup(size, PAGE_SIZE);
	if (size == 0)
		return -EINVAL;

	ret = sisvga_bo_create(dev, size, 0, 0, &sis_bo);
	if (ret) {
		if (ret != -ERESTARTSYS)
			DRM_ERROR("sisvga: sisvga_bo_create() failed,"
				  " error %d\n", -ret);
		return ret;
	}
	*obj = &sis_bo->gem;
	return 0;
}
