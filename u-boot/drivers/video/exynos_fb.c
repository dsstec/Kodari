/*
 * EXYNOS LCD Controller driver.
 *
 * Author: InKi Dae <inki.dae at samsung.com>
 * Author: Donghwa Lee <dh09.lee at samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <config.h>
#include <common.h>
#include <lcd.h>
#include <version.h>
#include <stdarg.h>
#include <linux/types.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>
#include <asm/arch/clk.h>
#include <asm/arch/system.h>
#include <malloc.h>
#include "exynos_fb.h"
//#include "logo.h"


int lcd_line_length;
int lcd_color_fg;
int lcd_color_bg;

void *lcd_base;
void *lcd_console_address;

short console_col;
short console_row;

static unsigned int panel_width, panel_height;

/* LCD Panel data */
vidinfo_t panel_info = {
        .vl_freq        = 75,
        .vl_col         = 1024,
        .vl_row         = 600,
        .vl_width       = 1024,
        .vl_height      = 600,
        .vl_clkp        = CONFIG_SYS_LOW,
        .vl_hsp         = CONFIG_SYS_HIGH,
        .vl_vsp         = CONFIG_SYS_HIGH,
        .vl_dp          = CONFIG_SYS_LOW,
        .vl_bpix        = 5,    /* Bits per pixel, 2^5 = 32 */

        /* s6e8ax0 Panel infomation */
        .vl_hspw        = 32,
        .vl_hbpd        = 80,
        .vl_hfpd        = 48,
        .vl_vfpe		 = 0,
	  .vl_vbpe       = 0,

        .vl_vspw        = 1,
        .vl_vbpd        = 4,
        .vl_vfpd        = 3,
        .vl_cmd_allow_len = 0xf,

        .dual_lcd_enabled = 0,

        .init_delay     = 0,
        .power_on_delay = 0,
        .reset_delay    = 0,
        .interface_mode = FIMD_RGB_INTERFACE,                
};

static void exynos_lcd_init_mem(void *lcdbase, vidinfo_t *vid)
{
	unsigned long palette_size, palette_mem_size;
	unsigned int fb_size;

	fb_size = vid->vl_row * vid->vl_col * (NBITS(vid->vl_bpix) >> 3);

	lcd_base = lcdbase;

	palette_size = NBITS(vid->vl_bpix) == 8 ? 256 : 16;
	palette_mem_size = palette_size * sizeof(u32);

	exynos_fimd_lcd_init_mem((unsigned long)lcd_base,
			(unsigned long)fb_size, palette_size);
}

int conv_rgb565_to_rgb888(unsigned short rgb565, unsigned int sw)
{
	char red, green, blue;
	unsigned int threshold = 150;

	red = (rgb565 & 0xF800) >> 11;
	green = (rgb565 & 0x7E0) >> 5;
	blue = (rgb565 & 0x1F);

	red = red << 3;
	green = green << 2;
	blue = blue << 3;

	/* correct error pixels of samsung logo. */
	if (sw) {
		if (red > threshold)
			red = 255;
		if (green > threshold)
			green = 255;
		if (blue > threshold)
			blue = 255;
	}

	return (red << 16 | green << 8 | blue);
}

void _draw_samsung_logo(void *lcdbase, int x, int y, int w, int h, unsigned short *bmp)
{
	int i, j, error_range = 40;
	short k = 0;
	unsigned int pixel;
	unsigned long *fb = (unsigned  long*)lcdbase;

	for (j = y; j < (y + h); j++) {
		for (i = x; i < (x + w); i++) {
			pixel = (*(bmp + k++));

			/* 40 lines under samsung logo image are error range. */
			if (j > h + y - error_range)
				*(fb + (j * panel_width) + i) =
					conv_rgb565_to_rgb888(pixel, 1);
			else
				*(fb + (j * panel_width) + i) =
					conv_rgb565_to_rgb888(pixel, 0);
		}
	}
}



static void draw_samsung_logo(void* lcdbase)
{
#if 0
#else
	int x, y;
	unsigned int in_len, width, height;
	//unsigned long out_len = ARRAY_SIZE(tizen_hd_logo) * sizeof(*tizen_hd_logo);
	void *dst = NULL;

	width = 520;
	height = 120;
	x = ((panel_width - width) >> 1);
	y = ((panel_height - height) >> 1) - 5;

	in_len = width * height * 4;
//	dst = malloc(in_len);
//	if (dst == NULL) {
//		debug("Error: malloc in gunzip failed!\n");
//		return;
//	}
//	memcpy(dst, tizen_hd_logo, out_len);
	//if (gunzip(dst, in_len, (uchar *)logo, &out_len) != 0) {
	//	free(dst);
	//	return;
	//}
	//if (out_len == CONFIG_SYS_VIDEO_LOGO_MAX_SIZE)
		debug("Image could be truncated"
				" (increase CONFIG_SYS_VIDEO_LOGO_MAX_SIZE)!\n");

	//_draw_samsung_logo(lcdbase, x, y, width, height, (unsigned short *) tizen_hd_logo);

	//free(dst);
#endif
}

static void exynos_lcd_init(vidinfo_t *vid)
{
	exynos_fimd_lcd_init(vid);
}

static void lcd_panel_on(vidinfo_t *vid)
{
	udelay(vid->init_delay);

	if (vid->backlight_reset)
		vid->backlight_reset();

	if (vid->cfg_gpio)
		vid->cfg_gpio();

	if (vid->lcd_power_on)
		vid->lcd_power_on();

	udelay(vid->power_on_delay);

	if (vid->reset_lcd) {
		vid->reset_lcd();
		udelay(vid->reset_delay);
	}

	if (vid->backlight_on)
		vid->backlight_on(1);

	if (vid->cfg_ldo)
		vid->cfg_ldo();

	if (vid->enable_ldo)
		vid->enable_ldo(1);
}

void lcd_ctrl_init(void *lcdbase)
{
	char * p;

	set_system_display_ctrl();
	set_lcd_clk();

	/* initialize parameters which is specific to panel. */
	init_panel_info(&panel_info);

	panel_width = panel_info.vl_width;
	panel_height = panel_info.vl_height;

	exynos_lcd_init_mem(lcdbase, &panel_info);
	//memset(lcdbase, 0, panel_width * panel_height * (NBITS(panel_info.vl_bpix) >> 3));	


	exynos_lcd_init(&panel_info);
	
}

void lcd_enable(void)
{
	lcd_panel_on(&panel_info);
	//draw_samsung_logo(lcd_base);
}

void lcd_disable(void)
{
	if (panel_info.backlight_on)
		panel_info.backlight_on(0);
	//draw_samsung_logo(lcd_base);
}


ulong calc_fbsize(void)
{
	return exynos_fimd_calc_fbsize();
}

/* dummy function */
void lcd_setcolreg(ushort regno, ushort red, ushort green, ushort blue)
{
	return;
}


