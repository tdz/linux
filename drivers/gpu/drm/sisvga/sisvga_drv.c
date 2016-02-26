/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#include "sisvga_drv.h"
#include <drm/drm_pciids.h>
#include <linux/console.h>
#include <linux/module.h>
#include "sisvga_device.h"

static const struct sisvga_mode sisvga_6202_mode_list[] = {
	/* VGA modes */
	{ SISVGA_MODE("13",    320,  200, 70,  8,  25175,    0, 0) },
	/* Enhanced video modes */
	{ SISVGA_MODE("2D",    640,  350, 70,  8,  25175,    0, 0) },
	{ SISVGA_MODE("2E",    640,  480, 60,  8,  25175,    0, 0) },
	{ SISVGA_MODE("2E*",   640,  480, 72,  8,  31500,    0, 0) },
	{ SISVGA_MODE("2E+",   640,  480, 75,  8,  31500,    0, 0) },
	{ SISVGA_MODE("2F",    640,  400, 70,  8,  25175,    0, 0) },
	{ SISVGA_MODE("30",    800,  600, 56,  8,  36000,    0, 0) },
	{ SISVGA_MODE("30*",   800,  600, 60,  8,  40000,    0, 0) },
	{ SISVGA_MODE("30+",   800,  600, 72,  8,  50000,    0, 0) },
	{ SISVGA_MODE("30#",   800,  600, 75,  8,  50000,    0, 0) },
	{ SISVGA_MODE("38i",  1024,  768, 87,  8,  44900,    0,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("38n",  1024,  768, 60,  8,  65000,    0, 0) },
	{ SISVGA_MODE("38n+", 1024,  768, 70,  8,  75000,    0, 0) },
	{ SISVGA_MODE("38n#", 1024,  768, 75,  8,  80000,    0, 0) },
	{ SISVGA_MODE("3Ai",  1280, 1024, 89,  8,  80000, 2048,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("3An",  1280, 1024, 60,  8, 110000, 2048, 0) },
	{ SISVGA_MODE("40",    320,  200, 70, 15,  25175,    0, 0) },
	{ SISVGA_MODE("41",    320,  200, 70, 16,  25175,    0, 0) },
	{ SISVGA_MODE("42",    320,  200, 70, 24,  25175,    0, 0) },
	{ SISVGA_MODE("43",    640,  480, 60, 15,  25175,    0, 0) },
	{ SISVGA_MODE("43*",   640,  480, 72, 15,  31500,    0, 0) },
	{ SISVGA_MODE("43+",   640,  480, 75, 15,  31500,    0, 0) },
	{ SISVGA_MODE("44",    640,  480, 60, 16,  25175,    0, 0) },
	{ SISVGA_MODE("44*",   640,  480, 72, 16,  31500,    0, 0) },
	{ SISVGA_MODE("44+",   640,  480, 75, 16,  31500,    0, 0) },
	{ SISVGA_MODE("45",    640,  480, 60, 24,  25175,    0, 0) },
	{ SISVGA_MODE("45*",   640,  480, 72, 24,  31500,    0, 0) },
	{ SISVGA_MODE("45+",   640,  480, 75, 24,  31500,    0, 0) },
	{ SISVGA_MODE("46",    800,  600, 56, 15,  36000,    0, 0) },
	{ SISVGA_MODE("46*",   800,  600, 60, 15,  40000,    0, 0) },
	{ SISVGA_MODE("46+",   800,  600, 72, 15,  50000,    0, 0) },
	{ SISVGA_MODE("46#",   800,  600, 75, 15,  50000,    0, 0) },
	{ SISVGA_MODE("47",    800,  600, 56, 16,  36000,    0, 0) },
	{ SISVGA_MODE("47*",   800,  600, 60, 16,  40000,    0, 0) },
	{ SISVGA_MODE("47+",   800,  600, 72, 16,  50000,    0, 0) },
	{ SISVGA_MODE("47#",   800,  600, 75, 16,  50000,    0, 0) },
	{ SISVGA_MODE("48",    800,  600, 56, 24,  36000, 2048, 0) },
	{ SISVGA_MODE("48*",   800,  600, 60, 24,  40000, 2048, 0) },
	{ SISVGA_MODE("48+",   800,  600, 72, 24,  50000, 2048, 0) },
	{ SISVGA_MODE("48#",   800,  600, 75, 24,  50000, 2048, 0) },
	{ SISVGA_MODE("49i",  1024,  768, 87, 15,  44900, 2048,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("49n",  1024,  768, 60, 15,  65000, 2048, 0) },
	{ SISVGA_MODE("49n+", 1024,  768, 70, 15,  75000, 2048, 0) },
	{ SISVGA_MODE("49n#", 1024,  768, 75, 15,  80000, 2048, 0) },
	{ SISVGA_MODE("4Ai",  1024,  768, 87, 16,  44900, 2048,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("4An",  1024,  768, 60, 16,  65000, 2048, 0) },
	{ SISVGA_MODE("4An+", 1024,  768, 70, 16,  75000, 2048, 0) },
	{ SISVGA_MODE("4An#", 1024,  768, 75, 16,  80000, 2048, 0) },
	{ SISVGA_MODE("4Bi",  1024,  768, 87, 24,  44900, 4096,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("4Ci",  1280, 1024, 89, 15,  80000, 4096,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("4Di",  1280, 1024, 89, 16,  80000, 4096,
			DRM_MODE_FLAG_INTERLACE) },
};

static const struct sisvga_mode sisvga_6215_mode_list[] = {
	/* VGA modes */
	{ SISVGA_MODE("13",     320,  200, 70,  8,  25175,    0, 0) },
	/* Enhanced video modes */
	{ SISVGA_MODE("2D",     640,  350, 70,  8,  25175,    0, 0) },
	{ SISVGA_MODE("2E",     640,  480, 60,  8,  25175,    0, 0) },
	{ SISVGA_MODE("2E*",    640,  480, 72,  8,  31500,    0, 0) },
	{ SISVGA_MODE("2E+",    640,  480, 75,  8,  31500,    0, 0) },
	{ SISVGA_MODE("2E++",   640,  480, 85,  8,  36000,    0, 0) },
	{ SISVGA_MODE("2F",     640,  400, 70,  8,  25175,    0, 0) },
	{ SISVGA_MODE("30",     800,  600, 56,  8,  36000,    0, 0) },
	{ SISVGA_MODE("30*",    800,  600, 60,  8,  40000,    0, 0) },
	{ SISVGA_MODE("30+",    800,  600, 72,  8,  50000,    0, 0) },
	{ SISVGA_MODE("30#",    800,  600, 75,  8,  50000,    0, 0) },
	{ SISVGA_MODE("30##",   800,  600, 85,  8,  56300,    0, 0) },
	{ SISVGA_MODE("38i",   1024,  768, 87,  8,  44900,    0,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("38n",   1024,  768, 60,  8,  65000,    0, 0) },
	{ SISVGA_MODE("38n+",  1024,  768, 70,  8,  75000,    0, 0) },
	{ SISVGA_MODE("38n#",  1024,  768, 75,  8,  80000,    0, 0) },
	{ SISVGA_MODE("38n##", 1024,  768, 85,  8,  94500,    0, 0) },
	{ SISVGA_MODE("3Ai",   1280, 1024, 87,  8,  80000, 2048,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("3An",   1280, 1024, 60,  8, 110000, 2048, 0) },
	{ SISVGA_MODE("3An+",  1280, 1024, 75,  8, 135000, 2048, 0) },
	{ SISVGA_MODE("40",     320,  200, 70, 15,  25175,    0, 0) },
	{ SISVGA_MODE("41",     320,  200, 70, 16,  25175,    0, 0) },
	{ SISVGA_MODE("42",     320,  200, 70, 24,  25175,    0, 0) },
	{ SISVGA_MODE("43",     640,  480, 60, 15,  25175,    0, 0) },
	{ SISVGA_MODE("43*",    640,  480, 72, 15,  31500,    0, 0) },
	{ SISVGA_MODE("43+",    640,  480, 75, 15,  31500,    0, 0) },
	{ SISVGA_MODE("43++",   640,  480, 85, 15,  36000,    0, 0) },
	{ SISVGA_MODE("44",     640,  480, 60, 16,  25175,    0, 0) },
	{ SISVGA_MODE("44*",    640,  480, 72, 16,  31500,    0, 0) },
	{ SISVGA_MODE("44+",    640,  480, 75, 16,  31500,    0, 0) },
	{ SISVGA_MODE("44++",   640,  480, 85, 16,  36000,    0, 0) },
	{ SISVGA_MODE("45",     640,  480, 60, 24,  25175,    0, 0) },
	{ SISVGA_MODE("45*",    640,  480, 72, 24,  31500, 2048, 0) },
	{ SISVGA_MODE("45+",    640,  480, 75, 24,  31500, 2048, 0) },
	{ SISVGA_MODE("45++",   640,  480, 85, 24,  36000,    0, 0) },
	{ SISVGA_MODE("46",     800,  600, 56, 15,  36000,    0, 0) },
	{ SISVGA_MODE("46*",    800,  600, 60, 15,  40000,    0, 0) },
	{ SISVGA_MODE("46+",    800,  600, 72, 15,  50000, 2048, 0) },
	{ SISVGA_MODE("46#",    800,  600, 75, 15,  50000, 2048, 0) },
	{ SISVGA_MODE("46##",   800,  600, 85, 15,  56300,    0, 0) },
	{ SISVGA_MODE("47",     800,  600, 56, 16,  36000,    0, 0) },
	{ SISVGA_MODE("47*",    800,  600, 60, 16,  40000,    0, 0) },
	{ SISVGA_MODE("47+",    800,  600, 72, 16,  50000, 2048, 0) },
	{ SISVGA_MODE("47#",    800,  600, 75, 16,  50000, 2048, 0) },
	{ SISVGA_MODE("47##",   800,  600, 85, 16,  56300,    0, 0) },
	{ SISVGA_MODE("48",     800,  600, 56, 24,  36000, 2048, 0) },
	{ SISVGA_MODE("48*",    800,  600, 60, 24,  40000, 2048, 0) },
	{ SISVGA_MODE("48+",    800,  600, 72, 24,  50000, 2048, 0) },
	{ SISVGA_MODE("48#",    800,  600, 75, 24,  50000, 2048, 0) },
	{ SISVGA_MODE("48##",   800,  600, 85, 24,  56300, 2048, 0) },
	{ SISVGA_MODE("49i",   1024,  768, 87, 15,  44900, 2048,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("49n",   1024,  768, 60, 15,  65000, 2048, 0) },
	{ SISVGA_MODE("49n+",  1024,  768, 70, 15,  75000, 2048, 0) },
	{ SISVGA_MODE("49n#",  1024,  768, 75, 15,  80000, 2048, 0) },
	{ SISVGA_MODE("49n##", 1024,  768, 85, 15,  94500, 2048, 0) },
	{ SISVGA_MODE("4Ai",   1024,  768, 87, 16,  44900, 2048,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("4An",   1024,  768, 60, 16,  65000, 2048, 0) },
	{ SISVGA_MODE("4An+",  1024,  768, 70, 16,  75000, 2048, 0) },
	{ SISVGA_MODE("4An#",  1024,  768, 75, 16,  80000, 2048, 0) },
	{ SISVGA_MODE("4An##", 1024,  768, 85, 16,  94500, 2048, 0) },
};

static const struct sisvga_mode sisvga_6326_mode_list[] = {
	/* VGA modes */
	{ SISVGA_MODE("13",     320,  200, 70,  8,  25175,    0, 0) },
	/* Enhanced video modes */
	{ SISVGA_MODE("2D",     640,  350, 70,  8,  25175,    0, 0) },
	{ SISVGA_MODE("2E",     640,  480, 60,  8,  25175,    0, 0) },
	{ SISVGA_MODE("2E*",    640,  480, 72,  8,  31500,    0, 0) },
	{ SISVGA_MODE("2E+",    640,  480, 75,  8,  31500,    0, 0) },
	{ SISVGA_MODE("2E++",   640,  480, 85,  8,  36000,    0, 0) },
	{ SISVGA_MODE("2F",     640,  400, 70,  8,  25175,    0, 0) },
	{ SISVGA_MODE("30",     800,  600, 56,  8,  36000,    0, 0) },
	{ SISVGA_MODE("30*",    800,  600, 60,  8,  40000,    0, 0) },
	{ SISVGA_MODE("30+",    800,  600, 72,  8,  50000,    0, 0) },
	{ SISVGA_MODE("30#",    800,  600, 75,  8,  50000,    0, 0) },
	{ SISVGA_MODE("30##",   800,  600, 85,  8,  56300,    0, 0) },
	{ SISVGA_MODE("38i",   1024,  768, 87,  8,  44900,    0,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("38n",   1024,  768, 60,  8,  65000,    0, 0) },
	{ SISVGA_MODE("38n+",  1024,  768, 70,  8,  75000,    0, 0) },
	{ SISVGA_MODE("38n#",  1024,  768, 75,  8,  80000,    0, 0) },
	{ SISVGA_MODE("38n##", 1024,  768, 85,  8,  94500,    0, 0) },
	{ SISVGA_MODE("3Ai",   1280, 1024, 87,  8,  80000, 2048,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("3An",   1280, 1024, 60,  8, 110000, 2048, 0) },
	{ SISVGA_MODE("3An+",  1280, 1024, 75,  8, 135000, 2048, 0) },
	{ SISVGA_MODE("3Ci",   1600, 1200, 87,  8, 135000, 2048, 0) },
	{ SISVGA_MODE("3C",    1600, 1200, 60,  8, 162000, 2048, 0) },
	{ SISVGA_MODE("3C*",   1600, 1200, 65,  8, 175500, 2048, 0) },
	{ SISVGA_MODE("40",     320,  200, 70, 15,  25175,    0, 0) },
	{ SISVGA_MODE("41",     320,  200, 70, 16,  25175,    0, 0) },
	{ SISVGA_MODE("42",     320,  200, 70, 24,  25175,    0, 0) },
	{ SISVGA_MODE("43",     640,  480, 60, 15,  25175,    0, 0) },
	{ SISVGA_MODE("43*",    640,  480, 72, 15,  31500,    0, 0) },
	{ SISVGA_MODE("43+",    640,  480, 75, 15,  31500,    0, 0) },
	{ SISVGA_MODE("43++",   640,  480, 85, 15,  36000,    0, 0) },
	{ SISVGA_MODE("44",     640,  480, 60, 16,  25175,    0, 0) },
	{ SISVGA_MODE("44*",    640,  480, 72, 16,  31500,    0, 0) },
	{ SISVGA_MODE("44+",    640,  480, 75, 16,  31500,    0, 0) },
	{ SISVGA_MODE("44++",   640,  480, 85, 16,  36000,    0, 0) },
	{ SISVGA_MODE("45",     640,  480, 60, 24,  25175,    0, 0) },
	{ SISVGA_MODE("45*",    640,  480, 72, 24,  31500, 2048, 0) },
	{ SISVGA_MODE("45+",    640,  480, 75, 24,  31500, 2048, 0) },
	{ SISVGA_MODE("45++",   640,  480, 85, 24,  36000,    0, 0) },
	{ SISVGA_MODE("46",     800,  600, 56, 15,  36000,    0, 0) },
	{ SISVGA_MODE("46*",    800,  600, 60, 15,  40000,    0, 0) },
	{ SISVGA_MODE("46+",    800,  600, 72, 15,  50000, 2048, 0) },
	{ SISVGA_MODE("46#",    800,  600, 75, 15,  50000, 2048, 0) },
	{ SISVGA_MODE("46##",   800,  600, 85, 15,  56300,    0, 0) },
	{ SISVGA_MODE("47",     800,  600, 56, 16,  36000,    0, 0) },
	{ SISVGA_MODE("47*",    800,  600, 60, 16,  40000,    0, 0) },
	{ SISVGA_MODE("47+",    800,  600, 72, 16,  50000, 2048, 0) },
	{ SISVGA_MODE("47#",    800,  600, 75, 16,  50000, 2048, 0) },
	{ SISVGA_MODE("47##",   800,  600, 85, 16,  56300,    0, 0) },
	{ SISVGA_MODE("48",     800,  600, 56, 24,  36000, 2048, 0) },
	{ SISVGA_MODE("48*",    800,  600, 60, 24,  40000, 2048, 0) },
	{ SISVGA_MODE("48+",    800,  600, 72, 24,  50000, 2048, 0) },
	{ SISVGA_MODE("48#",    800,  600, 75, 24,  50000, 2048, 0) },
	{ SISVGA_MODE("48##",   800,  600, 85, 24,  56300, 2048, 0) },
	{ SISVGA_MODE("49i",   1024,  768, 87, 15,  44900, 2048,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("49n",   1024,  768, 60, 15,  65000, 2048, 0) },
	{ SISVGA_MODE("49n+",  1024,  768, 70, 15,  75000, 2048, 0) },
	{ SISVGA_MODE("49n#",  1024,  768, 75, 15,  80000, 2048, 0) },
	{ SISVGA_MODE("49n##", 1024,  768, 85, 15,  94500, 2048, 0) },
	{ SISVGA_MODE("4Ai",   1024,  768, 87, 16,  44900, 2048,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("4An",   1024,  768, 60, 16,  65000, 2048, 0) },
	{ SISVGA_MODE("4An+",  1024,  768, 70, 16,  75000, 2048, 0) },
	{ SISVGA_MODE("4An#",  1024,  768, 75, 16,  80000, 2048, 0) },
	{ SISVGA_MODE("4An##", 1024,  768, 85, 16,  94500, 2048, 0) },
	{ SISVGA_MODE("4Bi",   1024,  768, 87, 24,  44900, 4096,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("4Bn",   1024,  768, 60, 24,  65000, 4096, 0) },
	{ SISVGA_MODE("4Bn+",  1024,  768, 70, 24,  75000, 4096, 0) },
	{ SISVGA_MODE("4Bn#",  1024,  768, 75, 24,  80000, 4096, 0) },
	{ SISVGA_MODE("4Bn##", 1024,  768, 85, 24,  94500, 4096, 0) },
	{ SISVGA_MODE("4Ci",   1280, 1024, 89, 15,  80000, 4096,
			DRM_MODE_FLAG_INTERLACE) },
	{ SISVGA_MODE("4Ci",   1280, 1024, 89, 16,  80000, 4096,
			DRM_MODE_FLAG_INTERLACE) },
};

static const struct sisvga_device_info sisvga_device_info_list[] = {
	[SISVGA_MODEL_6202] = {
		SISVGA_DEVICE_INFO(SISVGA_MODEL_6202, 2 * 1024 * 1024, 130000,
				   SISVGA_VCLK_BIT(SISVGA_VCLK_25175) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_28322) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_31500) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_36000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_40000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_44889) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_50000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_65000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_75000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_77000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_80000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_94500) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_110000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_120000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_130000) |
				   /* Only in modes list; not in VCKL spec */
				   SISVGA_VCLK_BIT(SISVGA_VCLK_30000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_44900),
				   2048, 2048, 2048, 1280,
				   1024, 1024, 1024, 1024,
				   24, 24,
				   sisvga_6202_mode_list,
				   ARRAY_SIZE(sisvga_6202_mode_list)) },
	[SISVGA_MODEL_6215] = {
		SISVGA_DEVICE_INFO(SISVGA_MODEL_6215, 2 * 1024 * 1024, 135000,
				   SISVGA_VCLK_BIT(SISVGA_VCLK_25175) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_28322) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_31500) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_36000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_40000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_44889) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_50000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_65000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_75000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_77000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_80000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_94500) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_110000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_120000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_135000) |
				   /* Only in modes list; not in VCKL spec */
				   SISVGA_VCLK_BIT(SISVGA_VCLK_30000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_44900),
				   2048, 2048, 2048, 1280,
				   1024, 1024, 1024, 1024,
				   24, 24,
				   sisvga_6215_mode_list,
				   ARRAY_SIZE(sisvga_6215_mode_list)) },
	[SISVGA_MODEL_6326] = {
		SISVGA_DEVICE_INFO(SISVGA_MODEL_6326, 8 * 1024 * 1024, 175500,
				   SISVGA_VCLK_BIT(SISVGA_VCLK_25175) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_28322) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_30000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_31500) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_36000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_40000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_44889) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_50000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_65000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_77000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_80000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_94500) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_110000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_110000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_120000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_135000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_162000) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_175500) |
				   /* Only in modes list; not in VCKL spec */
				   SISVGA_VCLK_BIT(SISVGA_VCLK_44900) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_56300) |
				   SISVGA_VCLK_BIT(SISVGA_VCLK_75000),
				   4096, 4096, 4096, 1600,
				   2048, 2048, 2048, 1200,
				   24, 24,
				   sisvga_6326_mode_list,
				   ARRAY_SIZE(sisvga_6326_mode_list)) },
};

/*
 * DRM entry points
 */

static void sisvga_driver_unload(struct drm_device *dev)
{
	struct sisvga_device *sdev = dev->dev_private;

	if (!sdev)
		return;

	sisvga_device_fini(sdev);
}

int sisvga_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file *file_priv = filp->private_data;
	struct sisvga_device *sdev = file_priv->minor->dev->dev_private;

	if (unlikely(vma->vm_pgoff < DRM_FILE_PAGE_OFFSET))
		return -EINVAL;

	return sisvga_device_mmap(sdev, filp, vma);
}

static const struct file_operations sisvga_driver_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = sisvga_mmap,
	.poll = drm_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl = drm_compat_ioctl,
#endif
	.read = drm_read,
};

static void sisvga_driver_gem_free_object(struct drm_gem_object *obj)
{
	struct sisvga_bo *sis_bo = gem_to_sisvga_bo(obj);
	sisvga_bo_unref(&sis_bo);
}

static int sisvga_driver_dumb_create(struct drm_file *file_priv,
                		     struct drm_device *dev,
        			     struct drm_mode_create_dumb *args)
{
	struct sisvga_device *sdev = dev->dev_private;
	return sisvga_device_create_dumb(sdev, file_priv, args);
}

static int sisvga_driver_dumb_mmap_offset(struct drm_file *file_priv,
                        		  struct drm_device *dev,
					  uint32_t handle, uint64_t *offset)
{
	struct drm_gem_object *obj;
	struct sisvga_bo *sis_bo;

	obj = drm_gem_object_lookup(file_priv, handle);
	if (obj == NULL)
		return -ENOENT;

	sis_bo = gem_to_sisvga_bo(obj);
	*offset = sisvga_bo_mmap_offset(sis_bo);

	drm_gem_object_put_unlocked(obj);
	return 0;
}

static struct drm_driver sisvga_drm_driver = {
	.driver_features = DRIVER_GEM | DRIVER_MODESET,
	.unload = sisvga_driver_unload,
	.fops = &sisvga_driver_fops,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
	/* GEM interfaces */
	.gem_free_object = sisvga_driver_gem_free_object,
	/* Dumb interfaces */
	.dumb_create = sisvga_driver_dumb_create,
	.dumb_map_offset = sisvga_driver_dumb_mmap_offset,
};

/*
 * PCI driver entry points
 */

static const struct pci_device_id pciidlist[] = {
	{ PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_6202, PCI_ANY_ID, PCI_ANY_ID, 0, 0, SISVGA_MODEL_6202 },
	{ PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_6215, PCI_ANY_ID, PCI_ANY_ID, 0, 0, SISVGA_MODEL_6215 },
	{ PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_6326, PCI_ANY_ID, PCI_ANY_ID, 0, 0, SISVGA_MODEL_6326 },
	{0,}
};

MODULE_DEVICE_TABLE(pci, pciidlist);

static void sisvga_kick_out_firmware_fb(struct pci_dev *pdev)
{
	struct apertures_struct *ap;
	bool primary = false;

	ap = alloc_apertures(1);
	if (!ap)
		return;

	ap->ranges[0].base = pci_resource_start(pdev, SISVGA_PCI_BAR_VRAM);
	ap->ranges[0].size = pci_resource_len(pdev, SISVGA_PCI_BAR_VRAM);

#ifdef CONFIG_X86
	primary = pdev->resource[PCI_ROM_RESOURCE].flags & IORESOURCE_ROM_SHADOW;
#endif
	drm_fb_helper_remove_conflicting_framebuffers(ap, "sisfb", primary);
	kfree(ap);
}

static int sisvga_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int ret;
	enum sisvga_model model;
	struct sisvga_device* sdev;

	sisvga_kick_out_firmware_fb(pdev);

	ret = pcim_enable_device(pdev);
	if (ret)
		return ret;

	model = ent->driver_data & 0xf;
	if (model >= ARRAY_SIZE(sisvga_device_info_list)) {
		/* BUG: There should be a device info for every model. */
		DRM_ERROR("sisvga: unknown device model %d\n", model);
		return -EINVAL;
	}

	sdev = devm_kzalloc(&pdev->dev, sizeof(*sdev), GFP_KERNEL);
	if (!sdev)
		return -ENOMEM;

	ret = sisvga_device_init(sdev, &sisvga_drm_driver, pdev,
				 sisvga_device_info_list + model);
	if (ret)
		return ret;

	ret = drm_dev_register(&sdev->base, 0);
	if (ret)
		goto err_drm_dev_register;

	pci_set_drvdata(pdev, &sdev->base);

	return 0;

err_drm_dev_register:
	sisvga_device_fini(sdev);
	return ret;
}

static void sisvga_remove(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);

	drm_put_dev(dev);
}

static struct pci_driver sisvga_pci_driver = {
	.name = DRIVER_NAME,
	.id_table = pciidlist,
	.probe = sisvga_probe,
	.remove = sisvga_remove,
};

static int __init sisvga_init(void)
{
#ifdef CONFIG_VGA_CONSOLE
	if (vgacon_text_force())
		return -EINVAL;
#endif

	return pci_register_driver(&sisvga_pci_driver);
}

static void __exit sisvga_exit(void)
{
	pci_unregister_driver(&sisvga_pci_driver);
}

module_init(sisvga_init);
module_exit(sisvga_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
