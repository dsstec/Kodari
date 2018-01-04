/*
 * Power setup code for EXYNOS5
 *
 * Copyright (C) 2012 Samsung Electronics
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/arch/cpu.h>
#include <asm/arch/power.h>

static void ps_hold_setup(void)
{
//	struct exynos4_power *power =
//		(struct exynos4_power *)samsung_get_base_power();

	/* Set PS-Hold high */
	//setbits_le32(PS_HOLD_CONTROL_REG, POWER_PS_HOLD_CONTROL_DATA_HIGH);

	PS_HOLD_CONTROL_REG |= POWER_PS_HOLD_CONTROL_DATA_HIGH;

}

void power_reset(void)
{
//	struct exynos4_power *power =
//		(struct exynos4_power *)samsung_get_base_power();

	/* Clear inform1 so there's no change we think we've got a wake reset */
	INF_REG1_REG = 0;

	//setbits_le32(SW_RST_REG, 1);

	SW_RST_REG |= 1;
}

/* This function never returns */
void power_shutdown(void)
{
//	struct exynos4_power *power =
//		(struct exynos4_power *)samsung_get_base_power();

	//clrbits_le32(PS_HOLD_CONTROL_REG, POWER_PS_HOLD_CONTROL_DATA_HIGH);

	PS_HOLD_CONTROL_REG &= ~POWER_PS_HOLD_CONTROL_DATA_HIGH;


	hang();
}

int do_shutdown(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	puts ("shutdown ...\n");

	udelay (50000); //wait 50 ms

	PS_HOLD_CONTROL_REG &= ~POWER_PS_HOLD_CONTROL_DATA_HIGH;

	hang();

	return 0; //NOT REACHED
}

void power_enable_hw_thermal_trip(void)
{
//	struct exynos4_power *power =
//		(struct exynos4_power *)samsung_get_base_power();

	/* Enable HW thermal trip */
	//setbits_le32(PS_HOLD_CONTROL_REG, POWER_ENABLE_HW_TRIP);
	PS_HOLD_CONTROL_REG |= POWER_ENABLE_HW_TRIP;
}

uint32_t power_read_reset_status(void)
{
//	struct exynos4_power *power =
//		(struct exynos4_power *)samsung_get_base_power();

	return INF_REG1_REG;
}

void power_exit_wakeup(void)
{
	//struct exynos4_power *power =
	//	(struct exynos4_power *)samsung_get_base_power();
	typedef void (*resume_func)(void);

	((resume_func)INF_REG0_REG)();
}

/**
 * Initialize the pmic voltages to power up the system
 * This also calls i2c_init so that we can program the pmic
 *
 * REG_ENABLE = 0, needed to set the buck/ldo enable bit ON
 *
 * @return	Return 0 if ok, else -1
 */
int power_init(void)
{
	int error = 0;

	ps_hold_setup();

	/* init the i2c so that we can program pmic chip */
	//i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);

	/*
	 * We're using CR1616 coin cell battery that is non-rechargeable
	 * battery. But, BBCHOSTEN bit of the BBAT Charger Register in
	 * MAX77686 is enabled by default for charging coin cell.
	 *
	 * Also, we cannot meet the coin cell reverse current spec. in UL
	 * standard if BBCHOSTEN bit is enabled.
	 *
	 * Disable Coin BATT Charging
	 */
/*	error = max77686_disable_backup_batt();

	error |= max77686_volsetting(PMIC_BUCK2, CONFIG_VDD_ARM_MV,
						REG_ENABLE, MAX77686_MV);
	error |= max77686_volsetting(PMIC_BUCK3, CONFIG_VDD_INT_UV,
						REG_ENABLE, MAX77686_UV);
	error |= max77686_volsetting(PMIC_BUCK1, CONFIG_VDD_MIF_MV,
						REG_ENABLE, MAX77686_MV);
	error |= max77686_volsetting(PMIC_BUCK4, CONFIG_VDD_G3D_MV,
						REG_ENABLE, MAX77686_MV);
	error |= max77686_volsetting(PMIC_LDO2, CONFIG_VDD_LDO2_MV,
						REG_ENABLE, MAX77686_MV);
	error |= max77686_volsetting(PMIC_LDO3, CONFIG_VDD_LDO3_MV,
						REG_ENABLE, MAX77686_MV);
	error |= max77686_volsetting(PMIC_LDO5, CONFIG_VDD_LDO5_MV,
						REG_ENABLE, MAX77686_MV);
	error |= max77686_volsetting(PMIC_LDO10, CONFIG_VDD_LDO10_MV,
						REG_ENABLE, MAX77686_MV);
	if (error != 0)
		debug("power init failed\n");
*/
	return error;
}

void power_enable_xclkout(void)
{
	//struct exynos4_power *power =
	//	(struct exynos4_power *)samsung_get_base_power();

	/* use xxti for xclk out */
//	clrsetbits_le32(&power->pmu_debug, PMU_DEBUG_CLKOUT_SEL_MASK,
//				PMU_DEBUG_XXTI);
}

