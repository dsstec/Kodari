/*
 * (C) Copyright 2011 Samsung Electronics Co. Ltd
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
 
#include <common.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>


unsigned long (*get_uart_clk)(int dev_index);
unsigned long (*get_pwm_clk)(void);
unsigned long (*get_arm_clk)(void);
unsigned long (*get_pll_clk)(int);

void s5p_clock_init(void)
{
}

#define APLL 0
#define MPLL 1
#define EPLL 2
#define VPLL 3




/* ------------------------------------------------------------------------- */
/* NOTE: This describes the proper use of this file.
 *
 * CONFIG_SYS_CLK_FREQ should be defined as the input frequency of the PLL.
 *
 * get_FCLK(), get_HCLK(), get_PCLK() and get_UCLK() return the clock of
 * the specified bus in HZ.
 */
/* ------------------------------------------------------------------------- */

static ulong get_PLLCLK(int pllreg)
{
	ulong r, m, p, s;

	if (pllreg == APLL) {
		r = APLL_CON0_REG;
		m = (r>>16) & 0x3ff;
	} else if (pllreg == MPLL) {
		r = MPLL_CON0_REG;
		m = (r>>16) & 0x3ff;
	} else
		hang();

	p = (r>>8) & 0x3f;
	s = r & 0x7;
#if !(defined(CONFIG_SMDKC220) || defined(CONFIG_ARCH_EXYNOS5))
	if ((pllreg == APLL) || (pllreg == MPLL)) 
		s= s-1;
#endif

	return (m * (CONFIG_SYS_CLK_FREQ / (p * (1 << s))));
}

ulong get_APLL_CLK(void)
{
	return (get_PLLCLK(APLL));
}

ulong get_MPLL_CLK(void)
{
#if defined(CONFIG_CPU_EXYNOS5250_EVT1)
	u32 clk_mux_stat_cdrex, mpll_fout_sel;

	clk_mux_stat_cdrex = __raw_readl(ELFIN_CLOCK_BASE +
			CLK_MUX_STAT_CDREX_OFFSET);

	mpll_fout_sel = ( clk_mux_stat_cdrex >> 16 ) && 0x1;

	if(mpll_fout_sel) {
		return (get_PLLCLK(MPLL) / 2);
	}
#else
	return (get_PLLCLK(MPLL));
#endif
}



/* get_lcd_clk: return lcd clock frequency */
static unsigned long exynos4_get_lcd_clk(void)
{
	struct exynos4_clock *clk =
		(struct exynos4_clock *)samsung_get_base_clock();
	unsigned long pclk, sclk;
	unsigned int sel;
	unsigned int ratio;

	/*
	 * CLK_SRC_LCD0
	 * FIMD0_SEL [3:0]
	 */
	sel = readl(&clk->src_lcd);
	sel = sel & 0xf;

	debug("%s - sel:%x\n", __func__, sel);

	if (sel == 0x6)
		sclk = get_PLLCLK(MPLL);
	else if (sel == 0x7)
		sclk = get_PLLCLK(EPLL);
	else if (sel == 0x8)
		sclk = get_PLLCLK(VPLL);
	else
		return 0;

	/*
	 * CLK_DIV_LCD0
	 * FIMD0_RATIO [3:0]
	 */
	ratio = readl(&clk->div_lcd);
	ratio = ratio & 0xf;

	pclk = sclk / (ratio + 1);

	return pclk;
}

static unsigned long exynos4_set_lcd_clk(void)
{
	struct exynos4_clock *clk =
	    (struct exynos4_clock *)samsung_get_base_clock();
	unsigned int cfg = 0;

	/* set lcd src clock */
	cfg = readl(&clk->src_lcd);
	cfg &= ~(0xf);
	cfg |= 0x6;
	writel(cfg, &clk->src_lcd);

	cfg = readl(&clk->gate_ip_lcd);
	cfg |= 1 << 0;
	writel(cfg, &clk->gate_ip_lcd);

	/* set fimd ratio */
	cfg &= ~(0xf);
	cfg |= 0x1;
	writel(cfg, &clk->div_lcd);

}


unsigned long get_lcd_clk(void)
{
	return exynos4_get_lcd_clk();
}

unsigned long set_lcd_clk(void)
{
	return exynos4_set_lcd_clk();
}


