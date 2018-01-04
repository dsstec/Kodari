/*
 * drivers/video/exynos_fb.h
 *
 * Copyright (C) 2012 Donghwa Lee <dh09.lee at samsung.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	EXYNOS Frame Buffer Driver
 *	based on skeletonfb.c, sa1100fb.h, s3c2410fb.c
 */

#ifndef _EXYNOS_FB_H_
#define _EXYNOS_FB_H_

#define MAX_CLOCK	(100 * 1000000)
#define PICOS2KHZ(a) (1000000000UL/(a))
#define KHZ2PICOS(a) (1000000000UL/(a))


enum exynos_fb_rgb_mode_t {
	MODE_RGB_P = 0,
	MODE_BGR_P = 1,
	MODE_RGB_S = 2,
	MODE_BGR_S = 3,
};

enum exynos_cpu_auto_cmd_rate {
	DISABLE_AUTO_FRM,
	PER_TWO_FRM,
	PER_FOUR_FRM,
	PER_SIX_FRM,
	PER_EIGHT_FRM,
	PER_TEN_FRM,
	PER_TWELVE_FRM,
	PER_FOURTEEN_FRM,
	PER_SIXTEEN_FRM,
	PER_EIGHTEEN_FRM,
	PER_TWENTY_FRM,
	PER_TWENTY_TWO_FRM,
	PER_TWENTY_FOUR_FRM,
	PER_TWENTY_SIX_FRM,
	PER_TWENTY_EIGHT_FRM,
	PER_THIRTY_FRM,
};

void exynos_fimd_lcd_init_mem(unsigned long screen_base, unsigned long fb_size,
	unsigned long palette_size);
void exynos_fimd_lcd_init(vidinfo_t *vid);
unsigned long exynos_fimd_calc_fbsize(void);

#endif
