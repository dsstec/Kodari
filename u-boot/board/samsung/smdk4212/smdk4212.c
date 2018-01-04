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
#include <asm/arch/gpio.h>
#include <asm/arch/clock.h>

#ifdef CONFIG_LCD
#include <lcd.h>
#endif

#include <fat.h>



#ifdef CONFIG_EXYNOS_CPUFREQ
#include <asm/arch/exynos4-cpufreq.h>
#endif

#if defined CONFIG_EXYNOS_TMU
#include <asm/arch/exynos4-tmu.h>
#endif


#include <asm/arch/power.h>






void s3cfb_backlight_on(unsigned int onoff);
void lcd_power_on(void);





unsigned int OmPin;

DECLARE_GLOBAL_DATA_PTR;
extern int nr_dram_banks;
unsigned int second_boot_info = 0xffffffff;

/* ------------------------------------------------------------------------- */
#define SMC9115_Tacs	(0x0)	// 0clk		address set-up
#define SMC9115_Tcos	(0x4)	// 4clk		chip selection set-up
#define SMC9115_Tacc	(0xe)	// 14clk	access cycle
#define SMC9115_Tcoh	(0x1)	// 1clk		chip selection hold
#define SMC9115_Tah	(0x4)	// 4clk		address holding time
#define SMC9115_Tacp	(0x6)	// 6clk		page mode access cycle
#define SMC9115_PMC	(0x0)	// normal(1data)page mode configuration

#define SROM_DATA16_WIDTH(x)	(1<<((x*4)+0))
#define SROM_WAIT_ENABLE(x)	(1<<((x*4)+1))
#define SROM_BYTE_ENABLE(x)	(1<<((x*4)+2))


static uint32_t cpufreq_loop_count;


/*
 * Called to do the needful when tstc has a character ready
 * Meant to work in contrast to board_poll_devices
 */
void board_tstc_ready(void)
{
	//printf("%s - %i\n", __func__, cpufreq_loop_count);

//#ifdef CONFIG_EXYNOS_CPUFREQ
//	if (cpufreq_loop_count >= 10000000) {
//		/* Character received, increase ARM frequency */
//		exynos4x12_set_frequency(CPU_FREQ_L1000);
//		printf("%s: set CPU frequency scaling to CPU_FREQ_L1400\n", __func__);
//	}
//	cpufreq_loop_count = 0;
//#endif /* CONFIG_EXYNOS_CPUFREQ */
}


/*
 * Polling various devices on board for details and status monitoring purposes
 */

static int bfreq_1000 = 0;
void board_poll_devices(void)
{


#if defined CONFIG_EXYNOS_TMU
	int temp;

	switch (tmu_monitor(&temp)) {
	case TMU_STATUS_TRIPPED:
		printf("EXYNOS_TMU: TRIPPING! Device power going down ...%i\n", temp);
		power_shutdown();
		break;
	case TMU_STATUS_WARNING:
		printf("EXYNOS_TMU: WARNING! Temperature very high %i\n", temp);
#ifdef CONFIG_EXYNOS_CPUFREQ		
		if(bfreq_1000 == 1) {
			bfreq_1000 = 0;
			exynos4x12_set_frequency(CPU_FREQ_L200);
		}
#endif
		break;
	case TMU_STATUS_INIT:
	case TMU_STATUS_NORMAL:
#ifdef CONFIG_EXYNOS_CPUFREQ
		if(bfreq_1000 == 0) {
			bfreq_1000 = 1;
	//		printf("freq change L1000\n");
			exynos4x12_set_frequency(CPU_FREQ_L1000);
		}
#endif
		break;
	default:
		debug("Unknown TMU state\n");
	}
#endif /* CONFIG_EXYNOS_TMU */
#ifdef CONFIG_EXYNOS_CPUFREQ
//	cpufreq_loop_count++;
//	if (cpufreq_loop_count == 10000000) {
		/* User is idle, decrease ARM frequency*/
//		exynos4x12_set_frequency(CPU_FREQ_L200);
//		printf("%s: set CPU frequency scaling to FREQ_L200\n", __func__);
//	}
#endif /* CONFIG_EXYNOS_CPUFREQ */
}




/*
 * Miscellaneous platform dependent initialisations
 */
static void smc9115_pre_init(void)
{
        unsigned int cs1;
	/* gpio configuration */
	writel(0x00220020, 0x11000000 + 0x120);
	writel(0x00002222, 0x11000000 + 0x140);

	/* 16 Bit bus width */
	writel(0x22222222, 0x11000000 + 0x180);
	writel(0x0000FFFF, 0x11000000 + 0x188);
	writel(0x22222222, 0x11000000 + 0x1C0);
	writel(0x0000FFFF, 0x11000000 + 0x1C8);
	writel(0x22222222, 0x11000000 + 0x1E0);
	writel(0x0000FFFF, 0x11000000 + 0x1E8);

	/* SROM BANK1 */
        cs1 = SROM_BW_REG & ~(0xF<<4);
	cs1 |= ((1 << 0) |
		(0 << 2) |
		(1 << 3)) << 4;                

        SROM_BW_REG = cs1;

	/* set timing for nCS1 suitable for ethernet chip */
	SROM_BC1_REG = ( (0x1 << 0) |
		     (0x9 << 4) |
		     (0xc << 8) |
		     (0x1 << 12) |
		     (0x6 << 16) |
		     (0x1 << 24) |
		     (0x1 << 28) );

	GPIO_Init();

	GPIO_SetFunctionEach(eGPIO_X1, eGPIO_0, eGPO); // GPIO_ETHERNET_nRST_L
	udelay(10);
	GPIO_SetDataEach(eGPIO_X1, eGPIO_0, 1);
	udelay(10000);
	GPIO_SetDataEach(eGPIO_X1, eGPIO_0, 0);
	udelay(50000);
	GPIO_SetDataEach(eGPIO_X1, eGPIO_0, 1);
	udelay(10000);

	GPIO_SetFunctionEach(eGPIO_X2, eGPIO_4, eGINT); // GPIO_ETHERNET_INT
	GPIO_SetPullUpDownEach(eGPIO_X2, eGPIO_4, eGPUen);

	GPIO_SetFunctionEach(eGPIO_X2, eGPIO_5, eGINT); // GPIO_ETHERNET_PME
	GPIO_SetPullUpDownEach(eGPIO_X2, eGPIO_5, eGPUen);
}


static void low_power_mode(void)
{
	unsigned int tmp;
	struct exynos4_clock *clk =
	    (struct exynos4_clock *)samsung_get_base_clock();
//	struct exynos4_power *pwr =
//	    (struct exynos4_power *)samsung_get_base_power();

	/* Power down CORE1 */
	/* LOCAL_PWR_CFG [1:0] 0x3 EN, 0x0 DIS */	
	if(ARM_CORE2_STATUS_REG & 0x3) {
		debug("ARM_CORE2 is online\n");
		ARM_CORE2_OPTION_REG |= (0x1 << 12);
		ARM_CORE2_CONFIGURATION_REG = 0;


		do {
			tmp = ARM_CORE2_STATUS_REG;
		} while (ARM_CORE2_STATUS_REG & 0x3);

		debug("ARM_CORE2 is offline\n");

	} else {
		debug("ARM_CORE2 is offline\n");
	}
	if(ARM_CORE3_STATUS_REG & 0x3) {
		debug("ARM_CORE3 is online\n");
		ARM_CORE3_OPTION_REG |= (0x1 << 12);
		ARM_CORE3_CONFIGURATION_REG = 0;


		do {
			tmp = ARM_CORE3_STATUS_REG;
		} while (ARM_CORE3_STATUS_REG & 0x3);

		debug("ARM_CORE3 is offline\n");

	} else {
		debug("ARM_CORE3 is offline\n");
	}

	

	
	


	/* Change the APLL frequency */
	/* ENABLE (1 enable) | LOCKED (1 locked)  */
	/* [31]              | [29]               */
	/* FSEL      | MDIV          | PDIV            | SDIV */
	/* [27]      | [25:16]       | [13:8]          | [2:0]      */
	writel(0xa0c80604, &clk->apll_con0);

	/* Change CPU0 clock divider */
	/* CORE2_RATIO  | APLL_RATIO   | PCLK_DBG_RATIO | ATB_RATIO  */
	/* [30:28]      | [26:24]      | [22:20]        | [18:16]    */
	/* PERIPH_RATIO | COREM1_RATIO | COREM0_RATIO   | CORE_RATIO */
	/* [14:12]      | [10:8]       | [6:4]          | [2:0]      */
	writel(0x00000100, &clk->div_cpu0);

	/* CLK_DIV_STAT_CPU0 - wait until clock gets stable (0 = stable) */
	while (readl(&clk->div_stat_cpu0) & 0x1111111)
		continue;

	/* Change clock divider ratio for DMC */
	/* DMCP_RATIO                  | DMCD_RATIO  */
	/* [22:20]                     | [18:16]     */
	/* DMC_RATIO | DPHY_RATIO | ACP_PCLK_RATIO   | ACP_RATIO */
	/* [14:12]   | [10:8]     | [6:4]            | [2:0]     */
	writel(0x13113117, &clk->div_dmc0);

	/* CLK_DIV_STAT_DMC0 - wait until clock gets stable (0 = stable) */
	while (readl(&clk->div_stat_dmc0) & 0x11111111)
		continue;

	/* Turn off unnecessary power domains */
	if(XXTI_STATUS_REG & 0x1) {
		debug("XXTI is online\n");
		XXTI_CONFIGURATION_REG = 0;	/* XXTI */
		do {
			tmp = XXTI_STATUS_REG;
		} while(tmp & 0x1);
		debug("XXTI is offline\n");
	}
	
	CAM_CONFIGURATION_REG = 0;	/* CAM */
	TV_CONFIGURATION_REG = 0;    /* TV */

	if(MFC_STATUS_REG & 0x7) {
		debug("MFC is online\n");
		MFC_CONFIGURATION_REG = 0;   /* MFC */

		do {
			tmp = MFC_STATUS_REG;
		} while(tmp & 0x7);

		debug("MFC is offline\n");
	}

	if(G3D_STATUS_REG & 0x7) {
		debug("G3D is online\n");
		G3D_CONFIGURATION_REG = 0;   /* G3D */
			do {
			tmp = G3D_STATUS_REG;
		} while(tmp & 0x7);
		debug("G3D is offline\n");
	}
	GPS_CONFIGURATION_REG = 0;   /* GPS */
	GPS_ALIVE_CONFIGURATION_REG = 0;	/* GPS_ALIVE */
	LCD0_CONFIGURATION_REG = 0; /*LCD0*/

	/* Turn off unnecessary clocks */
	writel(0x0, &clk->gate_ip_cam);	/* CAM */
	writel(0x0, &clk->gate_ip_tv);          /* TV */
	writel(0x0, &clk->gate_ip_mfc);	/* MFC */
	writel(0x0, &clk->gate_ip_g3d);	/* G3D */
	writel(0x0, &clk->gate_ip_image);	/* IMAGE */
	writel(0x0, &clk->gate_ip_gps);	/* GPS */
	writel(0x0, &clk->gate_ip_lcd);	/* LCD */
}


static void board_power_init(void)
{
	/* PS HOLD */
//	writel(EXYNOS4_PS_HOLD_CON_VAL, (unsigned int)PS_HOLD_CONTROL_REG);
//	PS_HOLD_CONTROL_REG = EXYNOS4_PS_HOLD_CON_VAL;

	/* Set power down */
	/*writel(0, (unsigned int)CAM_CONFIGURATION_REG);
	writel(0, (unsigned int)TV_CONFIGURATION_REG);
	writel(0, (unsigned int)MFC_CONFIGURATION_REG);
	writel(0, (unsigned int)G3D_CONFIGURATION_REG);
	writel(0, (unsigned int)LCD0_CONFIGURATION_REG);
	writel(0, (unsigned int)GPS_CONFIGURATION_REG);
	writel(0, (unsigned int)GPS_ALIVE_CONFIGURATION_REG);
	*/
}


static void board_clock_init(void)
{
	struct exynos4_clock *clk =
		(struct exynos4_clock *)samsung_get_base_clock();
/*
	writel(CLK_SRC_CPU_VAL, (unsigned int)&clk->src_cpu);
	writel(CLK_SRC_TOP0_VAL, (unsigned int)&clk->src_top0);
//	writel(CLK_SRC_FSYS_VAL, (unsigned int)&clk->src_fsys);
	writel(CLK_SRC_PERIL0_VAL, (unsigned int)&clk->src_peril0);

	writel(CLK_DIV_CPU0_VAL, (unsigned int)&clk->div_cpu0);
	writel(CLK_DIV_CPU1_VAL, (unsigned int)&clk->div_cpu1);
	writel(CLK_DIV_DMC0_VAL, (unsigned int)&clk->div_dmc0);
	writel(CLK_DIV_DMC1_VAL, (unsigned int)&clk->div_dmc1);
	writel(CLK_DIV_LEFTBUS_VAL, (unsigned int)&clk->div_leftbus);
	writel(CLK_DIV_RIGHTBUS_VAL, (unsigned int)&clk->div_rightbus);
	writel(CLK_DIV_TOP_VAL, (unsigned int)&clk->div_top);
	writel(CLK_DIV_FSYS1_VAL, (unsigned int)&clk->div_fsys1);
	writel(CLK_DIV_FSYS2_VAL, (unsigned int)&clk->div_fsys2);
	writel(CLK_DIV_FSYS3_VAL, (unsigned int)&clk->div_fsys3);
	writel(CLK_DIV_PERIL0_VAL, (unsigned int)&clk->div_peril0);
	writel(CLK_DIV_PERIL3_VAL, (unsigned int)&clk->div_peril3);

	writel(PLL_LOCKTIME, (unsigned int)&clk->apll_lock);
	writel(PLL_LOCKTIME, (unsigned int)&clk->mpll_lock);
	writel(PLL_LOCKTIME, (unsigned int)&clk->epll_lock);
	writel(PLL_LOCKTIME, (unsigned int)&clk->vpll_lock);
	writel(APLL_CON1_VAL, (unsigned int)&clk->apll_con1);
	writel(APLL_CON0_VAL, (unsigned int)&clk->apll_con0);
	writel(MPLL_CON1_VAL, (unsigned int)&clk->mpll_con1);
	writel(MPLL_CON0_VAL, (unsigned int)&clk->mpll_con0);
	writel(EPLL_CON1_VAL, (unsigned int)&clk->epll_con1);
	writel(EPLL_CON0_VAL, (unsigned int)&clk->epll_con0);
	writel(VPLL_CON1_VAL, (unsigned int)&clk->vpll_con1);
	writel(VPLL_CON0_VAL, (unsigned int)&clk->vpll_con0);



	writel(CLK_GATE_IP_CAM_VAL, (unsigned int)&clk->gate_ip_cam);
	writel(CLK_GATE_IP_VP_VAL, (unsigned int)&clk->gate_ip_tv);
	writel(CLK_GATE_IP_MFC_VAL, (unsigned int)&clk->gate_ip_mfc);
	writel(CLK_GATE_IP_G3D_VAL, (unsigned int)&clk->gate_ip_g3d);
	writel(CLK_GATE_IP_IMAGE_VAL, (unsigned int)&clk->gate_ip_image);
	writel(CLK_GATE_IP_LCD0_VAL, (unsigned int)&clk->gate_ip_lcd);
	//writel(CLK_GATE_IP_FSYS_VAL, (unsigned int)&clk->gate_ip_fsys);
	writel(CLK_GATE_IP_GPS_VAL, (unsigned int)&clk->gate_ip_gps);
	writel(CLK_GATE_IP_PERIL_VAL, (unsigned int)&clk->gate_ip_peril);
	writel(CLK_GATE_IP_PERIR_VAL, (unsigned int)&clk->gate_ip_perir);
	writel(CLK_GATE_BLOCK_VAL, (unsigned int)&clk->gate_block);
*/


}


int board_init(void)
{
	u8 read_id;
	u8 read_vol_arm;
	u8 read_vol_int;
	u8 read_vol_g3d;
	u8 read_vol_mif;
	u8 buck1_ctrl;
	u8 buck2_ctrl;
	u8 buck3_ctrl;
	u8 buck4_ctrl;
	u8 ldo14_ctrl;
	char bl1_version[9] = {0};


	IIC0_ERead(0xcc, 0, &read_id);
	printf("read id %x\n", read_id);
	if (read_id == 0x77) {
		IIC0_ERead(0xcc, 0x19, &read_vol_arm);
		IIC0_ERead(0xcc, 0x22, &read_vol_int);
		IIC0_ERead(0xcc, 0x2B, &read_vol_g3d);
		//IIC0_ERead(0xcc, 0x2D, &read_vol_mif);
		IIC0_ERead(0xcc, 0x18, &buck1_ctrl);
		IIC0_ERead(0xcc, 0x21, &buck2_ctrl);
		IIC0_ERead(0xcc, 0x2A, &buck3_ctrl);
		//IIC0_ERead(0xcc, 0x2C, &buck4_ctrl);
		IIC0_ERead(0xcc, 0x48, &ldo14_ctrl);

		printf("vol_arm: %X\n", read_vol_arm);
		printf("vol_int: %X\n", read_vol_int);
		printf("vol_g3d: %X\n", read_vol_g3d);
		printf("buck1_ctrl: %X\n", buck1_ctrl);
		printf("buck2_ctrl: %X\n", buck2_ctrl);
		printf("buck3_ctrl: %X\n", buck3_ctrl);
		printf("ldo14_ctrl: %X\n", ldo14_ctrl);
	} else if (((0 <= read_id) && (read_id <= 0x5)) || (read_id == 0x15)) {
		IIC0_ERead(0xcc, 0x33, &read_vol_mif);
		IIC0_ERead(0xcc, 0x35, &read_vol_arm);
		IIC0_ERead(0xcc, 0x3E, &read_vol_int);
		IIC0_ERead(0xcc, 0x47, &read_vol_g3d);
		printf("vol_mif: %X\n", read_vol_mif);
		printf("vol_arm: %X\n", read_vol_arm);
		printf("vol_int: %X\n", read_vol_int);
		printf("vol_g3d: %X\n", read_vol_g3d);
	} else {
		IIC0_ERead(0x12, 0x14, &read_vol_arm);
		IIC0_ERead(0x12, 0x1E, &read_vol_int);
		IIC0_ERead(0x12, 0x28, &read_vol_g3d);
		IIC0_ERead(0x12, 0x11, &read_vol_mif);
		IIC0_ERead(0x12, 0x10, &buck1_ctrl);
		IIC0_ERead(0x12, 0x12, &buck2_ctrl);
		IIC0_ERead(0x12, 0x1C, &buck3_ctrl);
		IIC0_ERead(0x12, 0x26, &buck4_ctrl);

		printf("vol_arm: %X\n", read_vol_arm);
		printf("vol_int: %X\n", read_vol_int);
		printf("vol_g3d: %X\n", read_vol_g3d);
		printf("vol_mif: %X\n", read_vol_mif);
		printf("buck1_ctrl: %X\n", buck1_ctrl);
		printf("buck2_ctrl: %X\n", buck2_ctrl);
		printf("buck3_ctrl: %X\n", buck3_ctrl);
		printf("buck4_ctrl: %X\n", buck4_ctrl);
	}

	/* display BL1 version */
#ifdef CONFIG_TRUSTZONE
	printf("TrustZone Enabled BSP\n");
	strncpy(&bl1_version[0], (char *)0x0204f810, 8);
#else
	strncpy(&bl1_version[0], (char *)0x020233c8, 8);
#endif
	printf("BL1 version: %s\n", &bl1_version[0]);

	/* check half synchronizer for asynchronous bridge */
	if(*(unsigned int *)(0x10010350) == 0x1)
		printf("Using half synchronizer for asynchronous bridge\n");


#if defined CONFIG_EXYNOS_CPUFREQ
	if (exynos4x12_cpufreq_init()) {
		printf("%s: Failed to init CPU frequency scaling\n", __func__);
		return -1;
	} else {
		exynos4x12_set_frequency(CPU_FREQ_L1000);
	}
#endif


#if defined CONFIG_EXYNOS_TMU
	if (tmu_init()) {
		printf("%s: Failed to init TMU\n", __func__);
		return -1;
	}
#endif

	
#ifdef CONFIG_SMC911X
	smc9115_pre_init();
#endif

#ifdef CONFIG_SMDKC220
	gd->bd->bi_arch_number = MACH_TYPE_C220;
#else
	if(((PRO_ID & 0x300) >> 8) == 2)
		gd->bd->bi_arch_number = MACH_TYPE_C210;
	else
		gd->bd->bi_arch_number = MACH_TYPE_V310;
#endif




	gd->bd->bi_boot_params = (PHYS_SDRAM_1+0x100);

   	OmPin = INF_REG3_REG;
	printf("\n\nChecking Boot Mode ...");
	if(OmPin == BOOT_ONENAND) {
		printf(" OneNand\n");
	} else if (OmPin == BOOT_NAND) {
		printf(" NAND\n");
	} else if (OmPin == BOOT_MMCSD) {
		printf(" SDMMC\n");
	} else if (OmPin == BOOT_EMMC) {
		printf(" EMMC4.3\n");
	} else if (OmPin == BOOT_EMMC_4_4) {
		printf(" EMMC4.41\n");
	}

	return 0;
}

int dram_init(void)
{
	//gd->ram_size = get_ram_size((long *)PHYS_SDRAM_1, PHYS_SDRAM_1_SIZE);
	
	return 0;
}

void dram_init_banksize(void)
{
		nr_dram_banks = CONFIG_NR_DRAM_BANKS;
		gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
		gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;
		gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
		gd->bd->bi_dram[1].size = PHYS_SDRAM_2_SIZE;
		gd->bd->bi_dram[2].start = PHYS_SDRAM_3;
		gd->bd->bi_dram[2].size = PHYS_SDRAM_3_SIZE;
		gd->bd->bi_dram[3].start = PHYS_SDRAM_4;
		gd->bd->bi_dram[3].size = PHYS_SDRAM_4_SIZE;
#ifdef USE_2G_DRAM
		gd->bd->bi_dram[4].start = PHYS_SDRAM_5;
		gd->bd->bi_dram[4].size = PHYS_SDRAM_5_SIZE;
		gd->bd->bi_dram[5].start = PHYS_SDRAM_6;
		gd->bd->bi_dram[5].size = PHYS_SDRAM_6_SIZE;
		gd->bd->bi_dram[6].start = PHYS_SDRAM_7;
		gd->bd->bi_dram[6].size = PHYS_SDRAM_7_SIZE;
		gd->bd->bi_dram[7].start = PHYS_SDRAM_8;
		gd->bd->bi_dram[7].size = PHYS_SDRAM_8_SIZE;
#endif

#ifdef CONFIG_TRUSTZONE
	gd->bd->bi_dram[nr_dram_banks - 1].size -= CONFIG_TRUSTZONE_RESERVED_DRAM;
#endif
}

int board_eth_init(bd_t *bis)
{
	int rc = 0;
#ifdef CONFIG_SMC911X
	rc = smc911x_initialize(0, CONFIG_SMC911X_BASE);
#endif
	return rc;
}

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	printf("Board:\tSMDKV310\n");
	
	return 0;

}
#endif

#define EINT0_pressed 1





void LCD_drawImage(int startx, int starty, int width, int height, const unsigned short * pImageData)
{
	u32 line = lcd_line_length;
	uchar *screen_base = (uchar *)lcd_base;
	u32 i, j;
	u32 count = 0;
	uchar R,G,B;
	u32 imagesize = width * height;
	printf("%s - image size:%i, length:%i\n", __func__, imagesize, line);


	for (i = starty; i < (starty + height); i++) {
		for (j = startx; j < (startx + width); j++) {
			
			R = ((pImageData[count] & 0x1F) << 3) + 4;
			G = ((pImageData[count] & 0x7E0) >> 3) + 2;
			B = ((pImageData[count] & 0xF800) >> 8) +4;
			
			memset(screen_base + i * line + j * 4 + 0, B, 1);
			memset(screen_base + i * line + j * 4 + 1, G, 1);
			memset(screen_base + i * line + j * 4 + 2, R, 1);
			memset(screen_base + i * line + j * 4 + 3, 0x00, 1);
			count++;
			if(count > imagesize) goto end;
		}
	}

end:
	return;

}




struct color888
{
	   unsigned int blue:8;
	   unsigned int green:8;
	   unsigned int red:8;
	   unsigned int alpha:8;
} ;

// power X=387 Y=55


void LCD_drawImageBlack(int startx, int starty, int width, int height)
{
	u32 line = lcd_line_length;
	uchar *screen_base = (uchar *)lcd_base;
	u32 i, j;

	for (i = starty; i < (starty + height); i++) {
		for (j = startx; j < (startx + width); j++) {
			memset(screen_base + i * line + j * 4 + 0, 0, 1);
			memset(screen_base + i * line + j * 4 + 1, 0, 1);
			memset(screen_base + i * line + j * 4 + 2, 0, 1);
			memset(screen_base + i * line + j * 4 + 3, 0, 1);
		}
	}


}



void LCD_drawImage2(int startx, int starty, int width, int height, const unsigned char * pImageData)
{
	u32 line = lcd_line_length;
	uchar *screen_base = (uchar *)lcd_base;
	u32 i, j;
	struct color888 * pcolor888 = (struct color888*)pImageData;
	for (i = starty; i < (starty + height); i++) {
		for (j = startx; j < (startx + width); j++) {
			memset(screen_base + i * line + j * 4 + 0, pcolor888->blue, 1);
			memset(screen_base + i * line + j * 4 + 1, pcolor888->green, 1);
			memset(screen_base + i * line + j * 4 + 2, pcolor888->red, 1);
			memset(screen_base + i * line + j * 4 + 3, pcolor888->alpha, 1);
			pcolor888++;
		}
	}


}



int board_late_init (void)
{
	char temp[32];
	long size = 0;
	char * argv[6] = { "fatload", "mmc", "0", "0x48000000", "image_update.raw", NULL };
	int loop = 0;
	int bMenuKeyPressed = 0;
	int bHomekeyPressed = 0;
	int bBackKeyPressed = 0;
	int bInitVideo = 0;
	


	cmd_tbl_t *bcmd = NULL;






//	IIC7_ERead(KODARI_MICOM_ADDR, 0, &read_id);
//	printf("micom AC :%x\n", read_id);

		//read micom bat level and AC 

	setenv("bootdelay", "0");
	setenv("bootcmd", CONFIG_BOOTCOMMAND);

         if(INF_REG4_REG == 0xfc) { // emergency erase
		printf("%s - emergency erase !!\n", __func__);
		setenv ("bootcmd", CONFIG_BOOTCOMMAND6);
		INF_REG4_REG = 0x0;

		return 0;
         }

//#ifdef CONFIG_UPDATE_SOLUTION
#ifdef CONFIG_EMERGENCY_PROMPT
	GPIO_Init();
	GPIO_SetFunctionEach(eGPIO_X1, eGPIO_7, 0);
	GPIO_SetPullUpDownEach(eGPIO_X1, eGPIO_7, 1); //menukey

	GPIO_SetFunctionEach(eGPIO_X1, eGPIO_6, 0);
	GPIO_SetPullUpDownEach(eGPIO_X1, eGPIO_6, 1); //homekey

	GPIO_SetFunctionEach(eGPIO_X1, eGPIO_1, 0);
	GPIO_SetPullUpDownEach(eGPIO_X1, eGPIO_1, 1); //backkey


//i2c 7	
	GPIO_SetFunctionEach(eGPIO_D0, eGPIO_3, eGPO);
	GPIO_SetFunctionEach(eGPIO_D0, eGPIO_2, eGPO);
	IIC7_ESetport();



	// b-module reset
	IIC7_EWrite(0x48,0x0D,0xAA);
	printf("Send reboot command to MCU\n");


	
	udelay(10);
	if (GPIO_GetDataEach(eGPIO_X1, eGPIO_7) == EINT0_pressed || second_boot_info == 1){
		//setenv ("bootcmd", CONFIG_BOOTCOMMAND2);
		printf("%s - Emergency key pressed\n", __func__);
#ifdef CONFIG_LCD
			s3cfb_backlight_on(1);
			lcd_power_on();
			bInitVideo = 1;
			bcmd = find_cmd("fatload");
			if (bcmd != NULL) {				
				printf("bcmd is not null\n");
				argv[4] = "image_enteremode.raw";
				if (do_fat_fsload(bcmd, 0, 5, argv) == 0) {
#ifdef CONFIG_KODARI_TYPE_MOBILE
					LCD_drawImage2(739, 10, 260, 360, (unsigned char *)0x48000000);
#else //CONFIG_KODARI_TYPE_CAR
					LCD_drawImage2(100, 10, 360, 250, (unsigned char *)0x48000000);
#endif
				}
			}
#endif
		loop = 0;
		while(loop < 60) {
			udelay(1000 * 1000);
			if(GPIO_GetDataEach(eGPIO_X1, eGPIO_7) == EINT0_pressed)  {
				bMenuKeyPressed = 1;
				break;	
			}

			if(GPIO_GetDataEach(eGPIO_X1, eGPIO_6) == EINT0_pressed)  {
				bHomekeyPressed = 1;
				break;	
			}

			if(GPIO_GetDataEach(eGPIO_X1, eGPIO_1) == EINT0_pressed)  {
				bBackKeyPressed = 1;
				break;	
			}
			
			loop++;
		}

		if(bMenuKeyPressed == 1) {
			printf("%s - bMenuKeyPressed\n", __func__);		
			INF_REG4_REG = 0x0;
			setenv("bootdelay", "10");
		} else if(bHomekeyPressed == 1) {
			printf("%s - HomekeyPressed\n", __func__);		
			INF_REG4_REG = 0xf;		
		} else {
			printf("%s - Normal boot\n", __func__);		
			INF_REG4_REG = 0x0;
		}
#ifdef CONFIG_KODARI_TYPE_MOBILE
		LCD_drawImageBlack(739, 10, 260, 360);
#else //CONFIG_KODARI_TYPE_CAR
		LCD_drawImageBlack(100, 10, 360, 250);
#endif
		
	
	} else {
		printf("%s - GPIOx1:%i\n", __func__, GPIO_GetDataEach(eGPIO_X1, eGPIO_7));
	}
	
#endif




//silent_boot


#ifdef CONFIG_CPU_EXYNOS4X12
	if(INF_REG4_REG == 0xf) {
		printf("%s - recovery\n", __func__);
	#ifdef CONFIG_LCD
		if(bInitVideo == 0) {
			s3cfb_backlight_on(1);
			lcd_power_on();
			bInitVideo = 1;
		}
		if(bcmd == NULL)	bcmd = find_cmd("fatload");
		if (bcmd != NULL) { 			
			printf("bcmd is not null\n");
			argv[4] = "image_recovery.raw";
			if (do_fat_fsload(bcmd, 0, 5, argv) == 0) {
				printf("do_fat_fsload \n");
#ifdef CONFIG_KODARI_TYPE_MOBILE
				LCD_drawImage2(900, 30, 47, 207, (unsigned char *)0x48000000);
#else //CONFIG_KODARI_TYPE_CAR
				LCD_drawImage2(100,10, 207, 47, (unsigned char *)0x48000000);
#endif
				argv[4] = "image_power_recovery.raw";
				if (do_fat_fsload(bcmd, 0, 5, argv) == 0) {
#ifdef CONFIG_KODARI_TYPE_MOBILE
					LCD_drawImage2(850, 30, 55, 320, (unsigned char *)0x48000000);
#else //CONFIG_KODARI_TYPE_CAR
					LCD_drawImage2(100,60, 387, 55, (unsigned char *)0x48000000);
#endif
				}
				setenv ("bootcmd", CONFIG_BOOTCOMMAND3);
			}
		}		
	#endif
		//setenv ("bootcmd", CONFIG_BOOTCOMMAND3);
		INF_REG4_REG = 0x0;
		} else if((bMenuKeyPressed == 1) || (INF_REG4_REG == 0xfa)) {
		printf("%s - fastboot\n", __func__);		
#ifdef CONFIG_LCD
			if(bInitVideo == 0) {
				s3cfb_backlight_on(1);
				lcd_power_on();
			}
			if(bcmd == NULL)	bcmd = find_cmd("fatload");
			if (bcmd != NULL) {				
				printf("bcmd is not null\n");
				argv[4] ="image_update.raw";
				if (do_fat_fsload(bcmd, 0, 5, argv) == 0) {
					printf("do_fat_fsload \n");
#ifdef CONFIG_KODARI_TYPE_MOBILE
					LCD_drawImage2(900, 30, 47, 207, (unsigned char *)0x48000000);
#else //CONFIG_KODARI_TYPE_CAR
					LCD_drawImage2(100,10, 207, 47, (unsigned char *)0x48000000);
#endif

					argv[4] = "image_power.raw";
					if (do_fat_fsload(bcmd, 0, 5, argv) == 0) {	
#ifdef CONFIG_KODARI_TYPE_MOBILE
						LCD_drawImage2(700, 30, 192, 387, (unsigned char *)0x48000000);
#else //CONFIG_KODARI_TYPE_CAR
						LCD_drawImage2(100,60, 387, 192, (unsigned char *)0x48000000);
#endif

					}
				}
			}			
#endif
	//	low_power_mode();	
		setenv ("bootcmd", CONFIG_BOOTCOMMAND4);
		INF_REG4_REG = 0x0; //normal boot
	} else if(INF_REG4_REG == 0xff) { //wipe
		printf("%s - wipe\n", __func__);
		setenv ("bootcmd", CONFIG_BOOTCOMMAND5);
		//go back to default
		INF_REG4_REG = 0x0; //normal boot
	} else if(INF_REG4_REG == 0xfb) {
		setenv("bootdelay", "3");
		printf("%s - bootcmd\n", __func__);
		INF_REG4_REG = 0x0;
	}
#endif

	return 0;
}

int board_mmc_init(bd_t *bis)
{
	

#ifdef CONFIG_S3C_HSMMC
	setup_hsmmc_clock();
	setup_hsmmc_cfg_gpio();
	if (OmPin == BOOT_EMMC_4_4 || OmPin == BOOT_EMMC) {
#ifdef CONFIG_S5PC210
		smdk_s5p_mshc_init();
#endif
		smdk_s3c_hsmmc_init();
	} else {
		smdk_s3c_hsmmc_init();
#ifdef CONFIG_S5PC210
		smdk_s5p_mshc_init();
#endif
	}
#endif
	return 0;
}

#ifdef CONFIG_ENABLE_MMU
ulong virt_to_phy_s5pv310(ulong addr)
{
	if ((0xc0000000 <= addr) && (addr < 0xe0000000))
		return (addr - 0xc0000000 + 0x40000000);

	return addr;
}
#endif



void led_on()
{
	GPIO_Init();
	GPIO_SetFunctionEach(eGPIO_L2, eGPIO_0, 1); 
	udelay(10); 
	GPIO_SetDataEach(eGPIO_L2,eGPIO_0,1);	
	udelay(10); 	
	debug("%s - R onoff:%i\n", __func__, GPIO_GetDataEach(eGPIO_L2, eGPIO_0));
	GPIO_SetFunctionEach(eGPIO_L2, eGPIO_1, 1); 
	udelay(10); 
	GPIO_SetDataEach(eGPIO_L2,eGPIO_1,1);
	udelay(10); 
	debug("%s - G onoff:%i\n", __func__, GPIO_GetDataEach(eGPIO_L2, eGPIO_1));
	GPIO_SetFunctionEach(eGPIO_L2, eGPIO_4, 1); 
	udelay(10); 
	GPIO_SetDataEach(eGPIO_L2,eGPIO_4,1);
	udelay(10);
	debug("%s - B onoff:%i\n", __func__, GPIO_GetDataEach(eGPIO_L2, eGPIO_4));

}

int board_early_init_f(void)
{
	printf("\n%s\n", __func__);

	//board_clock_init();
	//board_power_init();

        led_on();

        // cpu_alive
        GPIO_Init();
#if 0
	GPIO_SetFunctionEach(eGPIO_X0, eGPIO_6, eGPO);
	GPIO_SetPullUpDownEach(eGPIO_X0, eGPIO_6, eGPUen);
	udelay(10); 
	GPIO_SetDataEach(eGPIO_X0,eGPIO_6,1);
	udelay(10);
#endif

	// EXT_POWER_ON (5v_enable)
	GPIO_SetFunctionEach(eGPIO_X0, eGPIO_3, eGPO);
	udelay(10);
	GPIO_SetDataEach(eGPIO_X0,eGPIO_3, 1);
	udelay(10);
	
	return 0;
}




#ifdef CONFIG_LCD
int s3c_gpio_cfgall_range(unsigned int start, unsigned int nr,
			  unsigned int cfg, int pull)
{
	int ret;
	for (; nr > 0; nr--) {	
		GPIO_SetPullUpDownEach(start, nr -1, pull);
		GPIO_SetFunctionEach(start, nr -1, cfg);		
	}


	return 0;
}

static inline int s3c_gpio_cfgrange_nopull(unsigned int pin, unsigned int size,
					   unsigned int cfg)
{
	return s3c_gpio_cfgall_range(pin, size, cfg, 0);
}



static void s3cfb_gpio_setup_24bpp(unsigned int start, unsigned int size,
		unsigned int cfg, int drvstr)
{
	u32 reg;


	s3c_gpio_cfgrange_nopull(start, size, cfg);

	for (; size > 0; size--) {
		GPIO_SetDSEach(start, size -1, drvstr);
	}



	/* Set FIMD0 bypass */
	reg = LCDBLK_CFG_REG;
	reg |= (1<<1);
	LCDBLK_CFG_REG = reg;

}


void s3cfb_backlight_on(unsigned int onoff)
{
	GPIO_Init();

	//backlight
	GPIO_SetFunctionEach(eGPIO_D0, eGPIO_1, 1);
#if !defined(CONFIG_KODARI_TYPE_MOBILE) // CONFIG_KODARI_TYPE_CAR
	GPIO_SetFunctionEach(eGPIO_M4, eGPIO_6, 1);
#endif
	udelay(10);	
	if(onoff) {
#if !defined(CONFIG_KODARI_TYPE_MOBILE) // CONFIG_KODARI_TYPE_CAR
		GPIO_SetDataEach(eGPIO_M4, eGPIO_6, 1);
#endif
		GPIO_SetDataEach(eGPIO_D0, eGPIO_1, 1);
		//GPIO_SetPullUpDownEach(eGPIO_D0, eGPIO_0, eGPUen);
		debug("%s - on\n",__func__);		
	} else {
		GPIO_SetDataEach(eGPIO_D0, eGPIO_1, 0);
#if !defined(CONFIG_KODARI_TYPE_MOBILE) // CONFIG_KODARI_TYPE_CAR
		GPIO_SetDataEach(eGPIO_M4, eGPIO_6, 0);
#endif
		debug("%s - off\n",__func__);
	}

	udelay(10);
	debug("%s - onoff:%i\n", __func__, GPIO_GetDataEach(eGPIO_D0, eGPIO_1));
}

void s3cfb_cfg_gpio()
{

	GPIO_Init();
	//LCD data line
	s3cfb_gpio_setup_24bpp(eGPIO_F0, 8, eGFunc_0, 0x3);
	s3cfb_gpio_setup_24bpp(eGPIO_F1, 8, eGFunc_0, 0x3);
	s3cfb_gpio_setup_24bpp(eGPIO_F2, 8, eGFunc_0, 0x3);
	s3cfb_gpio_setup_24bpp(eGPIO_F3, 4, eGFunc_0, 0x3);


	//GPIO_SetFunctionEach(eGPIO_F0, eGPIO_3, 2); 
	//GPIO_SetPullUpDownEach(eGPIO_F0, eGPIO_3, 0);
	//GPIO_SetDSEach(eGPIO_F0, eGPIO_3, 0x3);

	
}

void lcd_power_on(void)
{
	GPIO_Init();
	//LVDS on
	debug("%s -\n", __func__);
	GPIO_SetFunctionEach(eGPIO_C0, eGPIO_1, 1);
	udelay(10);
	//GPIO_SetPullUpDownEach(eGPIO_C0, eGPIO_0, eGPUen);
	GPIO_SetDataEach(eGPIO_C0, eGPIO_1, 1);
	udelay(10);

	debug("%s - onoff:%i\n", __func__, GPIO_GetDataEach(eGPIO_C0, eGPIO_1));

}





void init_panel_info(vidinfo_t *vid)
{

	//void (*cfg_gpio)(void);
	//void (*backlight_on)(unsigned int onoff);      
	vid->cfg_gpio = s3cfb_cfg_gpio;
//	vid->backlight_on = s3cfb_backlight_on;
//	vid->lcd_power_on = lcd_power_on;
		
		/* for LD9040. */
	//vid->pclk_name = 1; 	/* MPLL */
	//vid->sclk_div = 1;

    
       setenv("lcdinfo", "lcd=s6e8ax0");
   	printf("%s - 2\n", __func__);

	//s3cfb_backlight_on(1);
}
#endif

