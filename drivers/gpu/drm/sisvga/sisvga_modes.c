/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#include "sisvga_modes.h"

static bool
depth_is_compatible(const struct sisvga_mode* smode, int bpp)
{
	return (smode->depth == bpp) || ((smode->depth == 24) && (bpp == 32));
}

bool
sisvga_mode_is_compatible(const struct sisvga_mode* smode,
			  const struct drm_display_mode* mode,
			  int bpp)
{
	return (smode->hdisplay == mode->hdisplay) &&
	       (smode->vdisplay == mode->vdisplay) &&
	       (smode->clock == mode->clock) &&
	       (depth_is_compatible(smode, bpp));

}

const struct sisvga_mode*
sisvga_find_compatible_mode(const struct sisvga_mode* beg,
			    const struct sisvga_mode* end,
			    const struct drm_display_mode* mode,
			    int bpp)
{
	while (beg < end) {
		if (sisvga_mode_is_compatible(beg, mode, bpp)) {
			return beg;
		}
		++beg;
	}
	return NULL;
}
