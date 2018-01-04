/* linux/drivers/video/samsung/s3cfb_wa101s.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * 101WA01S 10.1" Landscape LCD module driver for the SMDK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "s3cfb.h"

static struct s3cfb_lcd od102na0d = {
	.width	= 1024,
	.height	= 600,
	.bpp	= 24,
	.freq	= 60,
	.x_size = 222,
	.y_size = 130,

	.timing = {
		.h_fp	= 48,  //front proch
		.h_bp	= 80,
		.h_sw	= 32,  // v sync len
		.v_fp	= 3,
		.v_fpe	= 0,
		.v_bp	= 4,
		.v_bpe	= 0,
		.v_sw	= 1, // h sync len
	},
	
	.polarity = {
		.rise_vclk	= 0,
		.inv_hsync	= 1,
		.inv_vsync	= 1,
		.inv_vden	= 0,
	},
};

/* name should be fixed as 's3cfb_set_lcd_info' */
void s3cfb_set_lcd_info(struct s3cfb_global *ctrl)
{
	printk(KERN_ERR "%s \n", __func__);
	od102na0d.init_ldi = NULL;
	ctrl->lcd = &od102na0d;
}
