/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 * Akshay Saraswat <Akshay.s@samsung.com>
 *
 * EXYNOS - Thermal Management Unit
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __ASM_ARCH_THERMAL_H
#define __ASM_ARCH_THERMAL_H

struct tmu_reg {
	u32 triminfo;
	u32 rsvd1[4];
	u32 triminfo_control;
	u32 rsvd5[2];
	u32 tmu_control;
	u32 rsvd7;
	u32 tmu_status;
	u32 sampling_internal;
	u32 counter_value0;
	u32 counter_value1;
	u32 rsvd8[2];
	u32 current_temp;
	u32 rsvd10[3];
	u32 threshold_temp_rise;
	u32 threshold_temp_fall;
	u32 rsvd13[2];
	u32 past_temp3_0;
	u32 past_temp7_4;
	u32 past_temp11_8;
	u32 past_temp15_12;
	u32 inten;
	u32 intstat;
	u32 intclear;
	u32 rsvd15;
	u32 emul_con;
};




enum tmu_status_t {
	TMU_STATUS_INIT = 0,
	TMU_STATUS_NORMAL,
	TMU_STATUS_WARNING,
	TMU_STATUS_TRIPPED,
};

/*
 * Monitors status of the TMU device and exynos temperature
 *
 * @param temp	pointer to the current temperature value
 * @return	enum tmu_status_t value, code indicating event to execute
 *		and -1 on error
 */
enum tmu_status_t tmu_monitor(int *temp);

/*
 * Initialize TMU device
 *
 * @param blob  FDT blob
 * @return	int value, 0 for success
 */
int tmu_init(void);
#endif

