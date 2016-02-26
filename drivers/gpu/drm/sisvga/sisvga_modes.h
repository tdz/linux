/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#ifndef SISVGA_MODE_H
#define SISVGA_MODE_H

#include "sisvga_vclk.h"
#include <drm/drm_modes.h>

struct sisvga_mode {
	int hdisplay;
	int vdisplay;
	int depth;
	int clock; /* Pixel clock in KHz */
	int min_vram; /* Minimally required VRAM in KiB */
	int flags;
	enum sisvga_vclk pixel_clock;
};

#define SISVGA_MODE(name_, hdisplay_, vdisplay_, frame_rate_, depth_,\
		    clock_, min_vram_, flags_)\
	.hdisplay = (hdisplay_),\
	.vdisplay = (vdisplay_),\
	.depth = (depth_),\
	.clock = (clock_),\
	.min_vram = (min_vram_),\
	.flags = (flags_),\
	.pixel_clock = (SISVGA_VCLK_ ## clock_)

bool
sisvga_mode_is_compatible(const struct sisvga_mode* smode,
			  const struct drm_display_mode* mode,
			  int bpp);

const struct sisvga_mode*
sisvga_find_compatible_mode(const struct sisvga_mode* beg,
			    const struct sisvga_mode* end,
			    const struct drm_display_mode* mode,
			    int bpp);

#endif
