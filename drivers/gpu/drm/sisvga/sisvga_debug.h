/*
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 * Authors: Thomas Zimmermann
 */

#ifndef SISVGA_DEBUG_H
#define SISVGA_DEBUG_H

struct sisvga_device;

void sisvga_debug_print_mode(struct sisvga_device* sdev);
void sisvga_debug_print_regs(struct sisvga_device* sdev);

#endif
