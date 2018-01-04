/*
 * vib-kodari.h - vibrator Driver for kodari
 *
 * Lee HyungChul <hclee@kodari.co.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 */
#ifndef _VIB_KODARI_H_
#define _VIB_KODARI_H_

struct kodari_vib_platform_data{
	const char * android_dev_name;
	int	vibrator_gpio;
};

#endif /* _VIB_KODARI_H_ */

