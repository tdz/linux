/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#ifndef SISVGA_DRV_H
#define SISVGA_DRV_H

#define DRIVER_AUTHOR		"Thomas Zimmermann"
#define DRIVER_NAME		"sisvga"
#define DRIVER_DESC		"SiS graphics driver"
#define DRIVER_DATE		"20160301"
#define DRIVER_MAJOR		1
#define DRIVER_MINOR		0
#define DRIVER_PATCHLEVEL	0

#define KiB_TO_BYTE(_kib)	((_kib) * 1024)
#define MiB_TO_KiB(_mib)	((_mib) * 1024)
#define MiB_TO_BYTE(_mib)	KiB_TO_MiB((MiB_TO_KiB(_mib)))

#endif
