/*
 * buzzer-kodari.h - beep Driver for kodari
 *
 * Lee HyungChul <hclee@kodari.co.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 */
 
 #include <linux/pwm.h>
 
#ifndef _BUZZER_KODARI_H_
#define _BUZZER_KODARI_H_

struct kodari_buzzer_platform_data{
	const char * android_dev_name;
	int Hz;
	struct pwm_device *buzzer_PWM;
};


#endif /* _BUZZER_KODARI_H_ */

