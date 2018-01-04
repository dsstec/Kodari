/* linux/arch/arm/mach-exynos/mach-kodari.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/clk.h>
#include <linux/lcd.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/gpio_keys.h>
#include <linux/i2c.h>
#include <linux/pwm_backlight.h>
#include <linux/input.h>
#include <linux/mmc/host.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/v4l2-mediabus.h>
#include <linux/memblock.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <plat/regs-serial.h>
#include <plat/exynos4.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/keypad.h>
#include <plat/devs.h>
#include <plat/fb.h>
#include <plat/fb-s5p.h>
#include <plat/fb-core.h>
#include <plat/regs-fb-v4.h>
#include <plat/backlight.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-adc.h>
#include <plat/adc.h>
#include <plat/iic.h>
#include <plat/pd.h>
#include <plat/sdhci.h>
#include <plat/mshci.h>
#include <plat/ehci.h>
#include <plat/usbgadget.h>
#include <plat/s3c64xx-spi.h>
#if defined(CONFIG_VIDEO_FIMC)
#include <plat/fimc.h>
#elif defined(CONFIG_VIDEO_SAMSUNG_S5P_FIMC)
#include <plat/fimc-core.h>
#include <media/s5p_fimc.h>
#endif
#if defined(CONFIG_VIDEO_FIMC_MIPI)
#include <plat/csis.h>
#elif defined(CONFIG_VIDEO_S5P_MIPI_CSIS)
#include <plat/mipi_csis.h>
#endif
#include <plat/tvout.h>
#include <plat/media.h>
#include <plat/regs-srom.h>
#include <plat/sysmmu.h>
#include <plat/tv-core.h>
#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC) || defined(CONFIG_VIDEO_MFC5X)
#include <plat/s5p-mfc.h>
#endif

#include <media/exynos_flite.h>
#include <media/exynos_fimc_is.h>
#include <video/platform_lcd.h>
#include <mach/board_rev.h>
#include <mach/map.h>
#include <mach/spi-clocks.h>
#include <mach/exynos-ion.h>
#include <mach/regs-pmu.h>
#ifdef CONFIG_EXYNOS4_DEV_DWMCI
#include <linux/scatterlist.h>
#include <mach/dwmci.h>
#endif
#ifdef CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION
#include <mach/secmem.h>
#endif
#include <mach/dev.h>
#include <mach/ppmu.h>
#ifdef CONFIG_EXYNOS_C2C
#include <mach/c2c.h>
#endif
#ifdef CONFIG_FB_S5P_MIPI_DSIM
#include <mach/mipi_ddi.h>
#include <mach/dsim.h>
#include <../../../drivers/video/samsung/s3cfb.h>
#endif
#include <plat/fimg2d.h>
#include <mach/dev-sysmmu.h>

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
#include <plat/fimc-core.h>
#include <media/s5p_fimc.h>
#endif

#ifdef CONFIG_VIDEO_JPEG_V2X
#include <plat/jpeg.h>
#endif
#ifdef CONFIG_REGULATOR_S5M8767
#include <linux/mfd/s5m87xx/s5m-core.h>
#include <linux/mfd/s5m87xx/s5m-pmic.h>
#endif
#if defined(CONFIG_EXYNOS_THERMAL)
#include <mach/tmu.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_TSC2004)
#include <linux/irq.h>
#include <linux/i2c/tsc2004.h>
#endif
//hgs Vibrator
#if defined(CONFIG_VIBRATOR_KODARI)
#include <linux/vib_kodari.h>
#endif

//hgs Buzzer
#if defined(CONFIG_BUZZER_KODARI)
#include <linux/buzzer_kodari.h>
#endif

#if defined(CONFIG_SMSC911X)
#include <linux/smsc911x.h>
#endif
#include <mach/gpio-kodari.h>
#if defined(CONFIG_MFD_KODARI_MICOM_CORE)
#include <linux/mfd/kodari/kodari-micom-core.h>
#endif

#if !defined(CONFIG_KODARI_TYPE_CAR) && !defined(CONFIG_KODARI_TYPE_MOBILE)
#error You moust choice one of type for kodari board... in System Type on kernel compile menu
#endif

//leehc add  wake lock
#include <linux/wakelock.h>
#include <linux/hrtimer.h>
#include<linux/switch.h>

#ifdef USE_EEPROM_MAC_RESTORE
extern int is_valid_eeprom_mac(void);
extern void restore_mac(void);
#endif

#define REG_INFORM4            (S5P_INFORM4)

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define KODARI_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define KODARI_ULCON_DEFAULT	S3C2410_LCON_CS8

#define KODARI_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)



static struct s3c2410_uartcfg kodari_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= KODARI_UCON_DEFAULT,
		.ulcon		= KODARI_ULCON_DEFAULT,
		.ufcon		= KODARI_UFCON_DEFAULT,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= KODARI_UCON_DEFAULT,
		.ulcon		= KODARI_ULCON_DEFAULT,
		.ufcon		= KODARI_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= KODARI_UCON_DEFAULT,
		.ulcon		= KODARI_ULCON_DEFAULT,
		.ufcon		= KODARI_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= KODARI_UCON_DEFAULT,
		.ulcon		= KODARI_ULCON_DEFAULT,
		.ufcon		= KODARI_UFCON_DEFAULT,
	},
};

#if defined(CONFIG_SMSC911X)
static struct resource kodari_smsc911x_resources[] = {
	[0] = {
		.start	= EXYNOS4_PA_SROM_BANK(1),
		.end	= EXYNOS4_PA_SROM_BANK(1) + SZ_64K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= GPIO_ETHERNET_IRQ,
		.end	= GPIO_ETHERNET_IRQ,
		
		//.flags	= IORESOURCE_IRQ | IRQF_TRIGGER_HIGH,
		.flags	= IORESOURCE_IRQ | IRQF_TRIGGER_LOW,
	},
};


static int smsc911x_init_irq(void)
{
//    int ret = 0;   
	s3c_gpio_cfgpin(GPIO_ETH_INT, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(GPIO_ETH_INT, S3C_GPIO_PULL_UP);
    return 0;
}

static void smsc911x_exit_irq(void)
{
	
}

static struct smsc911x_platform_config smsc9215_config = {
//	.irq_polarity	= SMSC911X_IRQ_POLARITY_ACTIVE_HIGH,
	.irq_polarity	= SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
	.irq_type	= SMSC911X_IRQ_TYPE_PUSH_PULL,
	.flags		= SMSC911X_USE_16BIT | SMSC911X_FORCE_INTERNAL_PHY | SMSC911X_SAVE_MAC_ADDRESS,
//	.flags		= SMSC911X_USE_16BIT | SMSC911X_FORCE_INTERNAL_PHY,
	.phy_interface	= PHY_INTERFACE_MODE_MII,
	.mac		= {0x00, 0x90, 0x84, 0x23, 0x45, 0x67},
#ifdef CONFIG_KODARI_ETHERNET_WOL
	.wake_up_irq = GPIO_ETHERNET_PME_IRQ,
#else
	.wake_up_irq = GPIO_ETH_IRQ,
#endif
    .init_platform_hw = smsc911x_init_irq,
    .exit_platform_hw = smsc911x_exit_irq,
};

static struct platform_device kodari_smsc911x = {
	.name		= "smsc911x",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(kodari_smsc911x_resources),
	.resource	= kodari_smsc911x_resources,
	.dev		= {
		.platform_data	= &smsc9215_config,
	},
};


static int __init ethaddr_setup(char *line) 
{ 
	char *ep; int i; 
	/* there should really be routines to do this stuff */ 
	for (i = 0; i < 6; i++) {
		smsc9215_config.mac[i] = line ? simple_strtoul(line, &ep, 16) : 0;
		if (line) 
		line = (*ep) ? ep+1 : ep; 
	} 
	printk(KERN_ERR "KODARI User MAC address: %pM\n", smsc9215_config.mac); 
	return 0;
}



__setup("ethaddr=", ethaddr_setup);
#endif

#ifdef CONFIG_EXYNOS_MEDIA_DEVICE
struct platform_device exynos_device_md0 = {
	.name = "exynos-mdev",
	.id = -1,
};
#endif

#define WRITEBACK_ENABLED



#ifdef CONFIG_VIDEO_FIMC


#define CAM_CHECK_ERR_RET(x, msg)					\
	if (unlikely((x) < 0)) {					\
		printk(KERN_ERR "\nfail to %s: err = %d\n", msg, x);	\
		return x;						\
	}
#define CAM_CHECK_ERR(x, msg)						\
	if (unlikely((x) < 0)) {					\
		printk(KERN_ERR "\nfail to %s: err = %d\n", msg, x);	\
	}
#define CAM_CHECK_ERR_GOTO(x, out, fmt, ...) \
	if (unlikely((x) < 0)) { \
		printk(KERN_ERR fmt, ##__VA_ARGS__); \
		goto out; \
	}

	

#ifdef CONFIG_VIDEO_OV5642
#include <media/ov5642_platform.h>

#define OV5642_LINE_LENGTH	640	//1920	// vxres * bpp / 8
#define OV5642_RESOLUTION_W	640	//640
#define OV5642_RESOLUTION_H	480
#define OV5642_PREVIEW_W	640
#define OV5642_PREVIEW_H	480


static int ov5642_power_on(void)
{
	struct regulator *regulator;
	int ret = 0;
	printk(KERN_ERR "%s: in\n", __func__);
	
	//ov5642_cam0_rstn();
	ret = gpio_request(GPIO_CAM_nRST_L, "GPM4");
	if (unlikely(ret)) {
		printk(KERN_ERR "request GPIO_CAM_nRST_L\n");
		return ret;
	}

//	ret = gpio_request(GPIO_CAM_POWER_DOWN_H, "GPM4");
//	if (unlikely(ret)) {
//		printk(KERN_ERR "request GPIO_CAM_POWER_DOWN_H\n");
//		return ret;
//	}

	/* MCLK */
	ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_NONE);
	CAM_CHECK_ERR_RET(ret, "cfg mclk");
	udelay(1);

	/* CAM_1V5 */
	printk(KERN_ERR "regulator_get 1\n");
	regulator = regulator_get(NULL, "CAM_1V5");
	printk(KERN_ERR "regulator_get 2\n");
	if (IS_ERR(regulator))
		return -ENODEV;
	printk(KERN_ERR "regulator_enable 1\n");
	ret = regulator_enable(regulator);
	udelay(1000);
	printk(KERN_ERR "regulator_enable 2,ret=%i\n",ret);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "CAM_1V5");
	udelay(1000);
	

	/* CAM_2V8 */
	regulator = regulator_get(NULL, "CAM_2V8");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "CAM_2V8");
	udelay(1000);

	/* CAM_IO_1V8 */
	regulator = regulator_get(NULL, "CAM_IO_1V8");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "CAM_IO_1V8");
	udelay(1000);

//	ret = gpio_direction_output(GPIO_CAM_POWER_DOWN_H, 0);
//		CAM_CHECK_ERR_RET(ret, "GPIO_CAM_nRST_L");
//	udelay(600);

	/* GPIO_CAM_nRST_L */
	ret = gpio_direction_output(GPIO_CAM_nRST_L, 1);
	CAM_CHECK_ERR_RET(ret, "GPIO_CAM_nRST_L");
	udelay(600);

	
	ret = gpio_direction_output(GPIO_CAM_nRST_L, 0);
	CAM_CHECK_ERR_RET(ret, "GPIO_CAM_nRST_L");
	udelay(600);

	
	ret = gpio_direction_output(GPIO_CAM_nRST_L, 1);
	CAM_CHECK_ERR_RET(ret, "GPIO_CAM_nRST_L");
	udelay(600);

	gpio_free(GPIO_CAM_nRST_L);
//	gpio_free(GPIO_CAM_POWER_DOWN_H);

	/* AF_3V3 */
	regulator = regulator_get(NULL, "AF_3V3");
	if (IS_ERR(regulator))
		return -ENODEV;
	ret = regulator_enable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "AF_3V3");
	udelay(100);

	printk(KERN_ERR "%s:ret:%i\n",__func__, ret);
	
	return ret;
}

static int ov5642_power_off(void)
{
	struct regulator *regulator;
	int ret = 0;
	printk(KERN_ERR "%s: in\n", __func__);

	ret = gpio_request(GPIO_CAM_nRST_L, "GPM4");
	if (unlikely(ret)) {
		printk(KERN_ERR "request GPIO_CAM_nRST_L\n");
		return ret;
	}

//	ret = gpio_request(GPIO_CAM_POWER_DOWN_H, "GPM4");
//	if (unlikely(ret)) {
//		printk(KERN_ERR "request GPIO_CAM_POWER_DOWN_H\n");
//		return ret;
//	}

	/* GPIO_CAM_nRST_L */
	ret = gpio_direction_output(GPIO_CAM_nRST_L, 0);
	CAM_CHECK_ERR_RET(ret, "GPIO_CAM_nRST_L");


//	ret = gpio_direction_output(GPIO_CAM_POWER_DOWN_H, 1);
//	CAM_CHECK_ERR_RET(ret, "GPIO_CAM_POWER_DOWN_H");


	
	/* CAM_1V5 */
	printk(KERN_ERR "regulator_get 1\n");
	regulator = regulator_get(NULL, "CAM_1V5");
	printk(KERN_ERR "regulator_get 2\n");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator)) {
		printk(KERN_ERR "regulator_is_enabled \n");
		ret = regulator_force_disable(regulator);
	}
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "CAM_1V5");
	udelay(100);


	/* CAM_2V8 */
	regulator = regulator_get(NULL, "CAM_2V8");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "CAM_2V8");
	udelay(100);

	/* CAM_IO_1V8 */
	regulator = regulator_get(NULL, "CAM_IO_1V8");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator))
		ret = regulator_force_disable(regulator);
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "CAM_IO_1V8");
	udelay(100);

	/* AF_3V3 */
	regulator = regulator_get(NULL, "AF_3V3");
	printk(KERN_ERR "regulator_get 1 AF_3V3 \n");
	if (IS_ERR(regulator))
		return -ENODEV;
	if (regulator_is_enabled(regulator)) {
		printk(KERN_ERR "regulator_is_enabled 1 AF_3V3 \n");
		ret = regulator_force_disable(regulator);
	}
	regulator_put(regulator);
	CAM_CHECK_ERR_RET(ret, "AF_3V3");
	udelay(100);


	/* MCLK */
	ret = s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_CAM_MCLK, S3C_GPIO_PULL_DOWN);
	CAM_CHECK_ERR_RET(ret, "cfg mclk");
	udelay(1);

	gpio_free(GPIO_CAM_nRST_L);
//	gpio_free(GPIO_CAM_POWER_DOWN_H);
	return ret;
}

static int ov5642_power(int enable)
{
	int ret = 0;

	if (enable) {
		ret = ov5642_power_on();
	} else
		ret = ov5642_power_off();

	if (unlikely(ret)) {
		pr_err("%s: power-on/down failed\n", __func__);
		return ret;
	}
/*
	ret = s3c_csis_power(enable);
	if (unlikely(ret)) {
		pr_err("%s: csis power-on failed\n", __func__);
		return ret;
	}
*/
	return ret;
}



struct s3c2410_platform_i2c ov5642_i2c1_data __initdata = {
	.flags		= 0,
	.slave_addr	= 0x10,
#ifdef CONFIG_VIDEO_OV5640_AUTO_FOCUS
	.frequency	= 400*1000,
#else
	.frequency	= 100*1000,
#endif
	.sda_delay	= 100,
	.bus_num 	= 1,
};

struct s3c2410_platform_i2c i2c_4_data __initdata = {
	.flags		= 0,
	.slave_addr	= 0x10,
	.frequency	= 400*1000,
	.sda_delay	= 100,
	.bus_num 	= 4,
};

static struct ov5642_platform_data ov5642_plat = {
	.default_width = OV5642_RESOLUTION_W,
	.default_height = OV5642_RESOLUTION_H,
	//.pixelformat = V4L2_PIX_FMT_UYVY,
	.pixelformat = V4L2_PIX_FMT_YUYV,
	.freq = 24000000,
	.is_mipi = 0,
	
};

static struct i2c_board_info  ov5642_i2c_info = {
	I2C_BOARD_INFO("OV5642", 0x78>>1),		// 0x78>>1
	.platform_data = &ov5642_plat,
};


static struct s3c_platform_camera ov5642 = {
	.id		= CAMERA_PAR_A,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
	.info		= &ov5642_i2c_info,
	.i2c_busnum	= 1,
	.pixelformat	= V4L2_PIX_FMT_YUYV,
	.srclk_name	= "xusbxti",
	.clk_name	= "sclk_cam0",
	.clk_rate	= 24000000,			//44000000,
	.line_length	= OV5642_LINE_LENGTH,		//1920,
	.width		= OV5642_RESOLUTION_W,
	.height		= OV5642_RESOLUTION_H,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= OV5642_PREVIEW_W,
		.height	= OV5642_PREVIEW_H,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 0,
	.inv_href	= 0,
	.inv_hsync	= 1,
	.reset_camera	= 1,
	.initialized	= 0,
	.cam_power	= ov5642_power,
};
#endif


#ifdef WRITEBACK_ENABLED
static struct i2c_board_info writeback_i2c_info = {
	I2C_BOARD_INFO("WriteBack", 0x0),
};

static struct s3c_platform_camera writeback = {
	.id		= CAMERA_WB,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &writeback_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_YUV444,
	.line_length	= 800,
	.width		= 480,
	.height		= 800,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 480,
		.height	= 800,
	},

	.initialized	= 0,
};
#endif

/* Interface setting */
static struct s3c_platform_fimc fimc_plat = {
#ifdef CONFIG_ITU_A
		.default_cam	= CAMERA_PAR_A,
#endif
#ifdef CONFIG_ITU_B
		.default_cam	= CAMERA_PAR_B,
#endif
#ifdef CONFIG_CSI_C
		.default_cam	= CAMERA_CSI_C,
#endif
#ifdef CONFIG_CSI_D
		.default_cam	= CAMERA_CSI_D,
#endif
#ifdef WRITEBACK_ENABLED
	.default_cam	= CAMERA_WB,
#endif
	.camera		= {
#ifdef CONFIG_VIDEO_OV5642
		&ov5642,
#endif
#ifdef WRITEBACK_ENABLED
		&writeback,
#endif
	},
	.hw_ver		= 0x51,
};
#endif /* CONFIG_VIDEO_FIMC */

/* for mainline fimc interface */
#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
#ifdef WRITEBACK_ENABLED
struct writeback_mbus_platform_data {
	int id;
	struct v4l2_mbus_framefmt fmt;
};

static struct i2c_board_info __initdata writeback_info = {
	I2C_BOARD_INFO("writeback", 0x0),
};
#endif


#endif /* CONFIG_VIDEO_SAMSUNG_S5P_FIMC */

#ifdef CONFIG_S3C64XX_DEV_SPI
static struct s3c64xx_spi_csinfo spi0_csi[] = {
	[0] = {
		.line = EXYNOS4_GPB(1),
		.set_level = gpio_set_value,
		.fb_delay = 0x2,
	},
};

static struct spi_board_info spi0_board_info[] __initdata = {
	{
		.modalias = "spidev",
		.platform_data = NULL,
		.max_speed_hz = 10*1000*1000,
		.bus_num = 0,
		.chip_select = 0,
		.mode = SPI_MODE_0,
		.controller_data = &spi0_csi[0],
	}
};

#ifndef CONFIG_FB_S5P_LMS501KF03
static struct s3c64xx_spi_csinfo spi1_csi[] = {
	[0] = {
		.line = EXYNOS4_GPB(5),
		.set_level = gpio_set_value,
		.fb_delay = 0x2,
	},
};

static struct spi_board_info spi1_board_info[] __initdata = {
	{
		.modalias = "spidev",
		.platform_data = NULL,
		.max_speed_hz = 10*1000*1000,
		.bus_num = 1,
		.chip_select = 0,
		.mode = SPI_MODE_3,
		.controller_data = &spi1_csi[0],
	}
};
#endif

static struct s3c64xx_spi_csinfo spi2_csi[] = {
	[0] = {
		.line = EXYNOS4_GPC1(2),
		.set_level = gpio_set_value,
		.fb_delay = 0x2,
	},
};

static struct spi_board_info spi2_board_info[] __initdata = {
	{
		.modalias = "spidev",
		.platform_data = NULL,
		.max_speed_hz = 10*1000*1000,
		.bus_num = 2,
		.chip_select = 0,
		.mode = SPI_MODE_0,
		.controller_data = &spi2_csi[0],
	}
};
#endif




static int exynos4_notifier_call(struct notifier_block *this,
					unsigned long code, void *_cmd)
{
	int mode = 0;

	if ((code == SYS_RESTART) && _cmd) {
		if (!strcmp((char *)_cmd, "recovery"))
			mode = 0xf;
		else if(!strcmp((char *)_cmd, "fastboot")) //leehc add reboot fastboot mode
			mode = 0xfa;
		else if(!strcmp((char *)_cmd, "uboot"))
			mode = 0xfb;
		else if(!strcmp((char *)_cmd, "erase"))
			mode = 0xfc;
		else if(!strcmp((char *)_cmd, "wipe"))
			mode = 0xff;
		
		
	}
	printk(KERN_ERR "%s - reboot mode %x\n", __func__, mode);

	__raw_writel(mode, REG_INFORM4);

#ifdef USE_EEPROM_MAC_RESTORE
	if(mode == 0xfc) {
		if(is_valid_eeprom_mac() == 0) {
			restore_mac();
			printk("restore MAC address success!!\n");
		}
		else {
			printk("eeprom MAC address available!!\n");
		}
	}
#endif

	return NOTIFY_DONE;
}

static struct notifier_block exynos4_reboot_notifier = {
	.notifier_call = exynos4_notifier_call,
};

#ifdef CONFIG_EXYNOS4_DEV_DWMCI
static void exynos_dwmci_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS4_GPK0(0); gpio < EXYNOS4_GPK0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	}

	switch (width) {
	case 8:
		for (gpio = EXYNOS4_GPK1(3); gpio <= EXYNOS4_GPK1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(4));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
	case 4:
		for (gpio = EXYNOS4_GPK0(3); gpio <= EXYNOS4_GPK0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
		break;
	case 1:
		gpio = EXYNOS4_GPK0(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	default:
		break;
	}
}

static struct dw_mci_board exynos_dwmci_pdata __initdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION | DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 100 * 1000 * 1000,
	.caps			= MMC_CAP_UHS_DDR50 | MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23,
	.fifo_depth		= 0x80,
	.detect_delay_ms	= 200,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci_cfg_gpio,
};
#endif

#ifdef CONFIG_S3C_DEV_HSMMC
static struct s3c_sdhci_platdata kodari_hsmmc0_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_INTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
#ifdef CONFIG_EXYNOS4_SDHCI_CH0_8BIT
	.max_width		= 8,
	.host_caps		= MMC_CAP_8_BIT_DATA,
#endif
};
#endif

#ifdef CONFIG_S3C_DEV_HSMMC1
static struct s3c_sdhci_platdata kodari_hsmmc1_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_INTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
};
#endif

#ifdef CONFIG_S3C_DEV_HSMMC2
static struct s3c_sdhci_platdata kodari_hsmmc2_pdata __initdata = {
//	.cd_type		= S3C_SDHCI_CD_INTERNAL,//S3C_SDHCI_CD_PERMANENT, //S3C_SDHCI_CD_INTERNAL,// S3C_SDHCI_CD_INTERNAL, //S3C_SDHCI_CD_GPIO, // leehc change S3C_SDHCI_CD_INTERNAL
	.cd_type		= S3C_SDHCI_CD_PERMANENT, //S3C_SDHCI_CD_INTERNAL,// S3C_SDHCI_CD_INTERNAL, //S3C_SDHCI_CD_GPIO, // leehc change S3C_SDHCI_CD_INTERNAL
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
#ifdef CONFIG_EXYNOS4_SDHCI_CH2_8BIT
	.max_width		= 8,
	.host_caps		= MMC_CAP_8_BIT_DATA,
#endif
};
#endif

#ifdef CONFIG_S3C_DEV_HSMMC3
static struct s3c_sdhci_platdata kodari_hsmmc3_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_INTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
};
#endif

#ifdef CONFIG_S5P_DEV_MSHC
static struct s3c_mshci_platdata exynos4_mshc_pdata __initdata = {
	.cd_type		= S3C_MSHCI_CD_PERMANENT,
	.has_wp_gpio		= true,
	.wp_gpio		= 0xffffffff,
#if defined(CONFIG_EXYNOS4_MSHC_8BIT) && \
	defined(CONFIG_EXYNOS4_MSHC_DDR)
	.max_width		= 8,
	.host_caps		= MMC_CAP_8_BIT_DATA | MMC_CAP_1_8V_DDR |
				  MMC_CAP_UHS_DDR50,
#elif defined(CONFIG_EXYNOS4_MSHC_8BIT)
	.max_width		= 8,
	.host_caps		= MMC_CAP_8_BIT_DATA,
#elif defined(CONFIG_EXYNOS4_MSHC_DDR)
	.host_caps		= MMC_CAP_1_8V_DDR | MMC_CAP_UHS_DDR50,
#endif
};
#endif

#ifdef CONFIG_USB_EHCI_S5P
static struct s5p_ehci_platdata kodari_ehci_pdata;

static void __init kodari_ehci_init(void)
{
	struct s5p_ehci_platdata *pdata = &kodari_ehci_pdata;

	s5p_ehci_set_platdata(pdata);
}
#endif

#ifdef CONFIG_USB_OHCI_S5P
static struct s5p_ohci_platdata kodari_ohci_pdata;

static void __init kodari_ohci_init(void)
{
	struct s5p_ohci_platdata *pdata = &kodari_ohci_pdata;

	s5p_ohci_set_platdata(pdata);
}
#endif

/* USB GADGET */
#ifdef CONFIG_USB_GADGET
static struct s5p_usbgadget_platdata kodari_usbgadget_pdata;

static void __init kodari_usbgadget_init(void)
{
	struct s5p_usbgadget_platdata *pdata = &kodari_usbgadget_pdata;

	s5p_usbgadget_set_platdata(pdata);
}
#endif

#if defined(CONFIG_MFD_KODARI_MICOM_CORE)
static int kodari_micom_cfg_irq(void)
{
	/* AP_PMIC_IRQ: EINT17 */
	s3c_gpio_cfgpin(GPIO_MICOM_INT, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(GPIO_MICOM_INT, S3C_GPIO_PULL_UP);
	return 0;
}
#endif

#ifdef CONFIG_REGULATOR_S5M8767
/* S5M8767 Regulator */
static int s5m_cfg_irq(void)
{	
	s3c_gpio_cfgpin(GPIO_PMIC_INT, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(GPIO_PMIC_INT, S3C_GPIO_PULL_UP);
	return 0;
}
static struct regulator_consumer_supply s5m8767_ldo1_supply[] = {
	REGULATOR_SUPPLY("vdd_alive", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo2_supply[] = {
	REGULATOR_SUPPLY("vddq_m12", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo3_supply[] = {
	REGULATOR_SUPPLY("vddioap_18", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo4_supply[] = {
	REGULATOR_SUPPLY("vddq_pre", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo5_supply[] = {
	REGULATOR_SUPPLY("VDD_IO_18", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo6_supply[] = {
	REGULATOR_SUPPLY("vdd10_mpll", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo7_supply[] = {
	REGULATOR_SUPPLY("vdd10_xpll", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo8_supply[] = {
	REGULATOR_SUPPLY("vdd10_mipi", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo9_supply[] = {
	REGULATOR_SUPPLY("vdd33_lcd", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo10_supply[] = {
	REGULATOR_SUPPLY("vdd18_mipi", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo11_supply[] = {
	REGULATOR_SUPPLY("vdd18_abb1", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo12_supply[] = {
	REGULATOR_SUPPLY("vdd33_uotg", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo13_supply[] = {
	REGULATOR_SUPPLY("CAM_IO_1V8", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo14_supply[] = {
	REGULATOR_SUPPLY("vdd18_abb02", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo15_supply[] = {
	REGULATOR_SUPPLY("vdd10_ush", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo16_supply[] = {
	REGULATOR_SUPPLY("vdd18_hsic", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo17_supply[] = {
	REGULATOR_SUPPLY("vmmc", "dw_mmc"),
};
//static struct regulator_consumer_supply s5m8767_ldo18_supply[] = {
//	REGULATOR_SUPPLY("vmmc", "s3c-sdhci.2"),
//};
static struct regulator_consumer_supply s5m8767_ldo18_supply[] = {
	REGULATOR_SUPPLY("bt_3v3", NULL),
};

static struct regulator_consumer_supply s5m8767_ldo19_supply[] = {
	REGULATOR_SUPPLY("CAM_2V8", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo20_supply[] = {
	REGULATOR_SUPPLY("aud_3v0", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo21_supply[] = {
	REGULATOR_SUPPLY("vdd28_af", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo22_supply[] = {
	REGULATOR_SUPPLY("gps_3v3", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo23_supply[] = {
	REGULATOR_SUPPLY("AF_3V3", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo24_supply[] = {
	REGULATOR_SUPPLY("vdd33_a31", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo25_supply[] = {
	REGULATOR_SUPPLY("HUB0_1V2", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo26_supply[] = {
	REGULATOR_SUPPLY("MAC_2V0", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo27_supply[] = {
	REGULATOR_SUPPLY("aud_1v8", NULL),
};
static struct regulator_consumer_supply s5m8767_ldo28_supply[] = {
	REGULATOR_SUPPLY("MOTOR_1V3", NULL),
};

static struct regulator_consumer_supply s5m8767_buck1_consumer =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply s5m8767_buck2_consumer =
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply s5m8767_buck3_consumer =
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply s5m8767_buck4_consumer =
	REGULATOR_SUPPLY("vdd_g3d", NULL);
static struct regulator_consumer_supply s5m8767_buck5_consumer =
	REGULATOR_SUPPLY("vdd_m12", NULL);
static struct regulator_consumer_supply s5m8767_buck6_consumer =
	REGULATOR_SUPPLY("CAM_1V5", NULL);
static struct regulator_consumer_supply s5m8767_buck9_consumer =
	REGULATOR_SUPPLY("vddf28_emmc", NULL);

#define REGULATOR_INIT(_ldo, _name, _min_uV, _max_uV, _always_on, _ops_mask,\
		_disabled) \
	static struct regulator_init_data s5m8767_##_ldo##_init_data = {		\
		.constraints = {					\
			.name	= _name,				\
			.min_uV = _min_uV,				\
			.max_uV = _max_uV,				\
			.always_on	= _always_on,			\
			.boot_on	= _always_on,			\
			.apply_uV	= 1,				\
			.valid_ops_mask = _ops_mask,			\
			.state_mem	= {				\
				.disabled	= _disabled,		\
				.enabled	= !(_disabled),		\
			}						\
		},							\
		.num_consumer_supplies = ARRAY_SIZE(s5m8767_##_ldo##_supply),	\
		.consumer_supplies = &s5m8767_##_ldo##_supply[0],			\
	};

REGULATOR_INIT(ldo1, "VDD_ALIVE", 1100000, 1100000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo2, "VDDQ_M12", 1200000, 1200000, 1,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo3, "VDDIOAP_18", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo4, "VDDQ_PRE", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo5, "VDD_IO_18", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo6, "VDD10_MPLL", 1000000, 1000000, 1,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo7, "VDD10_XPLL", 1000000, 1000000, 1,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo8, "VDD10_MIPI", 1000000, 1000000, 1,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo9, "LAN_3V3", 3300000, 3300000, 1,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo10, "VDD18_MIPI", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo11, "VDD18_ABB1", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo12, "VDD33_UOTG", 3000000, 3000000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo13, "CAM_IO_1V8", 1800000, 1800000, 0,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo14, "VDD18_ABB02", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo15, "VDD10_USH", 1000000, 1000000, 1,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo16, "VDD18_HSIC", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 1);
//REGULATOR_INIT(ldo17, "dw-mmc", 2800000, 2800000, 1,
//		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo17, "dw-mmc", 3300000, 3300000, 1,
		REGULATOR_CHANGE_STATUS, 0);

//REGULATOR_INIT(ldo18, "s3c-sdhci.2", 3300000, 3300000, 0,
//		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo18, "BT_3V3", 3300000, 3300000, 0,
		REGULATOR_CHANGE_STATUS, 0);

REGULATOR_INIT(ldo19, "CAM_2V8", 2800000, 2800000, 0,
		REGULATOR_CHANGE_STATUS, 1);
//REGULATOR_INIT(ldo20, "VDD28_CAM", 3000000, 3000000, 1,
//		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo20, "AUD_3V0", 3000000, 3000000, 0,
		REGULATOR_CHANGE_STATUS, 1);

REGULATOR_INIT(ldo21, "VDD28_AF", 3000000, 3000000, 0,
		REGULATOR_CHANGE_STATUS, 1);
//REGULATOR_INIT(ldo22, "VDDA28_2M", 3300000, 3300000, 1,
//		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo22, "GPS_3V3", 3300000, 3300000, 1,
		REGULATOR_CHANGE_STATUS, 1);

REGULATOR_INIT(ldo23, "AF_3V3", 3300000, 3300000, 0,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo24, "VDD33_A31", 3000000, 3000000, 0,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo25, "HUB0_1V2", 1200000, 1200000, 1,
		REGULATOR_CHANGE_STATUS, 0);
REGULATOR_INIT(ldo26, "MAC_2V0", 2000000, 2000000, 0,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo27, "AUD_1V8", 1800000, 1800000, 0,
		REGULATOR_CHANGE_STATUS, 1);
REGULATOR_INIT(ldo28, "MOTOR_1V3", 1300000, 1300000, 0,
		REGULATOR_CHANGE_STATUS, 1);

static struct regulator_init_data s5m8767_buck1_data = {
	.constraints	= {
		.name		= "vdd_mif range",
		.min_uV		= 800000,
		.max_uV		= 1100000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s5m8767_buck1_consumer,
};

static struct regulator_init_data s5m8767_buck2_data = {
	.constraints	= {
		.name		= "vdd_arm range",
		.min_uV		=  800000,
		.max_uV		= 1350000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s5m8767_buck2_consumer,
};

static struct regulator_init_data s5m8767_buck3_data = {
	.constraints	= {
		.name		= "vdd_int range",
		.min_uV		=  800000,
		.max_uV		= 1150000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.uV		= 1100000,
			.mode		= REGULATOR_MODE_NORMAL,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s5m8767_buck3_consumer,
};

static struct regulator_init_data s5m8767_buck4_data = {
	.constraints	= {
		.name		= "vdd_g3d range",
		.min_uV		= 850000,
		.max_uV		= 1200000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.always_on = 1,
		.boot_on = 1,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_buck4_consumer,
};

static struct regulator_init_data s5m8767_buck5_data = {
	.constraints	= {
		.name		= "vdd_m12 range",
		.min_uV		= 750000,
		.max_uV		= 1500000,
		.apply_uV	= 1,
	//	.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_buck5_consumer,
};

static struct regulator_init_data s5m8767_buck6_data = {
	.constraints	= {
		.name		= "CAM_1V5",
		.min_uV		= 1500000,
		.max_uV		= 1500000,
//		.boot_on	= 1,
//		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
			.uV	= 1500000,
			.mode	= REGULATOR_MODE_NORMAL,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_buck6_consumer,
};

static struct regulator_init_data s5m8767_buck9_data = {
	.constraints	= {
		.name		= "vddf28_emmc range",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.boot_on	= 1,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled	= 1,				
			.uV	= 2800000,
			.mode	= REGULATOR_MODE_NORMAL,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_buck9_consumer,
};

static struct s5m_regulator_data pegasus_regulators[] = {
	{ S5M8767_BUCK1, &s5m8767_buck1_data },
	{ S5M8767_BUCK2, &s5m8767_buck2_data },
	{ S5M8767_BUCK3, &s5m8767_buck3_data },
	{ S5M8767_BUCK4, &s5m8767_buck4_data },
	{ S5M8767_BUCK5, &s5m8767_buck5_data },
	{ S5M8767_BUCK6, &s5m8767_buck6_data },
	{ S5M8767_BUCK9, &s5m8767_buck9_data },

	{ S5M8767_LDO1, &s5m8767_ldo1_init_data },
	{ S5M8767_LDO2, &s5m8767_ldo2_init_data },
	{ S5M8767_LDO3, &s5m8767_ldo3_init_data },
	{ S5M8767_LDO4, &s5m8767_ldo4_init_data },
	{ S5M8767_LDO5, &s5m8767_ldo5_init_data },
	{ S5M8767_LDO6, &s5m8767_ldo6_init_data },
	{ S5M8767_LDO7, &s5m8767_ldo7_init_data },
	{ S5M8767_LDO8, &s5m8767_ldo8_init_data },
	{ S5M8767_LDO9, &s5m8767_ldo9_init_data },
	{ S5M8767_LDO10, &s5m8767_ldo10_init_data },

	{ S5M8767_LDO11, &s5m8767_ldo11_init_data },
	{ S5M8767_LDO12, &s5m8767_ldo12_init_data },
	{ S5M8767_LDO13, &s5m8767_ldo13_init_data },
	{ S5M8767_LDO14, &s5m8767_ldo14_init_data },
	{ S5M8767_LDO15, &s5m8767_ldo15_init_data },
	{ S5M8767_LDO16, &s5m8767_ldo16_init_data },
	{ S5M8767_LDO17, &s5m8767_ldo17_init_data },
	{ S5M8767_LDO18, &s5m8767_ldo18_init_data },
	{ S5M8767_LDO19, &s5m8767_ldo19_init_data },
	{ S5M8767_LDO20, &s5m8767_ldo20_init_data },

	{ S5M8767_LDO21, &s5m8767_ldo21_init_data },
	{ S5M8767_LDO22, &s5m8767_ldo22_init_data },
	{ S5M8767_LDO23, &s5m8767_ldo23_init_data },
	{ S5M8767_LDO24, &s5m8767_ldo24_init_data },
	{ S5M8767_LDO25, &s5m8767_ldo25_init_data },
	{ S5M8767_LDO26, &s5m8767_ldo26_init_data },
	{ S5M8767_LDO27, &s5m8767_ldo27_init_data },
	{ S5M8767_LDO28, &s5m8767_ldo28_init_data },
};

struct s5m_opmode_data s5m8767_opmode_data[S5M8767_REG_MAX] = {
	[S5M8767_BUCK1] = {S5M8767_BUCK1, S5M_OPMODE_STANDBY},
	[S5M8767_BUCK2] = {S5M8767_BUCK2, S5M_OPMODE_STANDBY},
	[S5M8767_BUCK3] = {S5M8767_BUCK3, S5M_OPMODE_STANDBY},
	[S5M8767_BUCK4] = {S5M8767_BUCK4, S5M_OPMODE_STANDBY},
	[S5M8767_BUCK5] = {S5M8767_BUCK5, S5M_OPMODE_STANDBY},

	[S5M8767_BUCK9] = {S5M8767_BUCK9, S5M_OPMODE_STANDBY},
	//CAM
	[S5M8767_BUCK6] = {S5M8767_BUCK6, S5M_OPMODE_NORMAL},
	[S5M8767_LDO19] = {S5M8767_LDO19, S5M_OPMODE_NORMAL},
	[S5M8767_LDO23] = {S5M8767_LDO23, S5M_OPMODE_NORMAL},
	[S5M8767_LDO13] = {S5M8767_LDO13, S5M_OPMODE_NORMAL},
	//LAN
	[S5M8767_LDO9] = {S5M8767_LDO9, S5M_OPMODE_NORMAL},
};

static struct s5m_platform_data exynos4_s5m8767_pdata = {
	.device_type		= S5M8767X,
	.irq_base		= IRQ_BOARD_START,
	.num_regulators		= ARRAY_SIZE(pegasus_regulators),
	.regulators		= pegasus_regulators,
	.cfg_pmic_irq		= s5m_cfg_irq,
	.wakeup			= 1,
	.opmode_data		= s5m8767_opmode_data,
	.wtsr_smpl		= 1,

	.buck2_voltage[2]	= 1150000,
	.buck2_voltage[3]	= 1100000,
	.buck2_voltage[4]	= 1050000,
	.buck2_voltage[5]	= 1000000,
	.buck2_voltage[6]	=  950000,
	.buck2_voltage[7]	=  900000,

	.buck3_voltage[1]	= 1000000,
	.buck3_voltage[2]	= 950000,
	.buck3_voltage[3]	= 900000,
	.buck3_voltage[5]	= 1000000,
	.buck3_voltage[6]	= 950000,
	.buck3_voltage[7]	= 900000,

	.buck4_voltage[1]	= 1150000,
	.buck4_voltage[3]	= 1100000,
	.buck4_voltage[4]	= 1100000,
	.buck4_voltage[5]	= 1100000,
	.buck4_voltage[6]	= 1100000,
	.buck4_voltage[7]	= 1100000,

	.buck_default_idx	= 1,

	.buck_gpios[0]		= EXYNOS4_GPL0(3),
	.buck_gpios[1]		= EXYNOS4_GPL0(4),
	.buck_gpios[2]		= EXYNOS4_GPL0(6),

	.buck_ds[0]		= EXYNOS4_GPL0(0),
	.buck_ds[1]		= EXYNOS4_GPL0(1),
	.buck_ds[2]		= EXYNOS4_GPL0(2),	
	
	.buck_ramp_delay        = 25,
	.buck2_ramp_enable      = true,
	.buck3_ramp_enable      = true,
	.buck4_ramp_enable      = true,

	.buck2_init		= 1100000,
	.buck3_init		= 1000000,
	.buck4_init		= 1000000,
};
/* End of S5M8767 */
#endif


#if defined(CONFIG_MFD_KODARI_MICOM_CORE)
static struct kodari_micom_platform_data exynos4_kodari_micom_pdata = {
	.irq_base		= IRQ_BOARD_KODARI_MICOM_START,
	.cfg_kodari_micom_irq		= kodari_micom_cfg_irq,		
	.wakeup			= 1,
};

#endif

#ifdef CONFIG_VIDEO_S5P_MIPI_CSIS
static struct regulator_consumer_supply mipi_csi_fixed_voltage_supplies[] = {
	REGULATOR_SUPPLY("mipi_csi", "s5p-mipi-csis.0"),
	REGULATOR_SUPPLY("mipi_csi", "s5p-mipi-csis.1"),
};

static struct regulator_init_data mipi_csi_fixed_voltage_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(mipi_csi_fixed_voltage_supplies),
	.consumer_supplies	= mipi_csi_fixed_voltage_supplies,
};

static struct fixed_voltage_config mipi_csi_fixed_voltage_config = {
	.supply_name	= "DC_5V",
	.microvolts	= 5000000,
	.gpio		= -EINVAL,
	.init_data	= &mipi_csi_fixed_voltage_init_data,
};

static struct platform_device mipi_csi_fixed_voltage = {
	.name		= "reg-fixed-voltage",
	.id		= 3,
	.dev		= {
		.platform_data	= &mipi_csi_fixed_voltage_config,
	},
};
#endif

static struct i2c_board_info i2c_devs0[] __initdata = {
#if 0 //def CONFIG_SND_SOC_WM8960
	{ I2C_BOARD_INFO("wm8960", 0x1a), },
#endif
#if 0 //def CONFIG_SND_SOC_WM1800
	{ I2C_BOARD_INFO("wm1800", 0x1a), },
#endif
#ifdef CONFIG_REGULATOR_S5M8767
	{
		I2C_BOARD_INFO("s5m87xx", 0xCC >> 1),
		.platform_data = &exynos4_s5m8767_pdata,
		.irq		= GPIO_PMIC_IRQ,
	},
#endif
};

static struct i2c_board_info i2c_devs1[] __initdata = {
};
#if defined(CONFIG_KODARI_TYPE_CAR)
static struct i2c_board_info i2c_devs2[] __initdata = {
#ifdef CONFIG_VIDEO_TVOUT
	{
		I2C_BOARD_INFO("s5p_ddc", (0x74 >> 1)),
	},
#endif
};
#endif

static struct i2c_board_info i2c_devs3[] __initdata = {
#ifdef CONFIG_KODARI_GPS_POWER_SAVE
	{
		I2C_BOARD_INFO("ublox-6", 0x01),
	},
#endif
};

struct s3c2410_platform_i2c i2c_3_data __initdata = {
	.flags		= 0,
	.slave_addr	= 0x10,
	.frequency	= 100*1000,
	.sda_delay	= 100,
	.bus_num 	= 3,
};

#if defined(CONFIG_SENSORS_BMM050) || defined(CONFIG_SENSORS_BMA250)
static struct i2c_board_info i2c_devs4[] __initdata = {
	#ifdef CONFIG_SENSORS_BMM050
	{
		I2C_BOARD_INFO("bmm050", 0x10),
	},
#endif
#ifdef CONFIG_SENSORS_BMA250
	{
		I2C_BOARD_INFO("bma250", 0x18),
	},
#endif
};
#endif



#if defined(CONFIG_MFD_KODARI_MICOM_CORE)
struct s3c2410_platform_i2c kodari_micom_i2c7_data __initdata = {
	.flags		= 0,
	.slave_addr	= 0x10,
	.frequency	= 100*1000,
	.sda_delay	= 100,
	.bus_num 	= 7,
};


static struct i2c_board_info i2c_devs7[] __initdata = {
	{
		I2C_BOARD_INFO("kodari-micom", 0x48),
		.platform_data = &exynos4_kodari_micom_pdata,
		.irq = GPIO_MICOM_IRQ,
	},
};
#endif

#if defined(CONFIG_TOUCHSCREEN_TSC2004)
/*
 * TSC 2004 Support
 */

static int tsc2004_init_irq(void)
{
       int ret = 0;


       ret = gpio_request(GPIO_TOUCH_nRST_L, "tsc2004-reset");
       if (ret < 0) {
               printk(KERN_WARNING "failed to request GPIO# GPM4_6: %d\n", ret);
               return ret;
       }

	gpio_direction_output(GPIO_TOUCH_nRST_L, 1);
	mdelay(10);
	gpio_direction_output(GPIO_TOUCH_nRST_L, 0);
	mdelay(10);
	gpio_direction_output(GPIO_TOUCH_nRST_L, 1);
	mdelay(100);
	gpio_free(GPIO_TOUCH_nRST_L);
	
	s3c_gpio_cfgpin(GPIO_TOUCH_INT, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(GPIO_TOUCH_INT, S3C_GPIO_PULL_UP);
    return 0;
}

static void tsc2004_exit_irq(void)
{
	//gpio_free(GPIO_TSC2004_GPIO);

	
}

static int tsc2004_get_irq_level(void)
{
	return gpio_get_value(GPIO_TOUCH_INT) ? 0 : 1;
}

struct tsc2004_platform_data kodari_tsc2004data = {
       .model = 2004,
       .x_plate_ohms = 180,
       .get_pendown_state = tsc2004_get_irq_level,
       .init_platform_hw = tsc2004_init_irq,
       .exit_platform_hw = tsc2004_exit_irq,
};
static struct i2c_board_info i2c_devs5[] __initdata = {
	{
		I2C_BOARD_INFO("tsc2004", 0x48),
        .type           = "tsc2004",
        .platform_data  = &kodari_tsc2004data,
        .irq = GPIO_TOUCH_IRQ,
	},
};
#else
static struct i2c_board_info i2c_devs5[] __initdata = {
};
#endif


//hgs vibrator driver
#if defined(CONFIG_VIBRATOR_KODARI)
static struct kodari_vib_platform_data kodari_vibrator_pdata = {
	.android_dev_name = "vibrator",
	.vibrator_gpio = EXYNOS4_GPC0(0),
};

static struct platform_device kodari_vibrator_device = {
	.name	= "kodari_vibrator",
	.id 	= -1,
	.dev	= {
		.platform_data = &kodari_vibrator_pdata,
	},
};

#endif

//hgs LED driver
// LED GPIO 

//gpio_set_value(EXYNOS4_GPL2(1), Led_R);	// red LED
//gpio_set_value(EXYNOS4_GPL2(4), Led_G);	// green LED
//gpio_set_value(EXYNOS4_GPL2(0), Led_B); // blue LED


static struct gpio_led kodari_gpio_leds[] = {
	{
		.name = "red",
		.default_trigger  = "timer",
		.gpio = EXYNOS4_GPL2(1),
	},
	{
		.name = "green",
		.default_trigger  = "timer",
		.gpio = EXYNOS4_GPL2(4),
	},
	{
		.name = "blue",
		.default_trigger  = "timer",
		.gpio = EXYNOS4_GPL2(0),
	},
};


static struct gpio_led_platform_data kodari_gpio_led_info = {
	.leds = kodari_gpio_leds,
	.num_leds = ARRAY_SIZE(kodari_gpio_leds),
//	.gpio_blink_set = smdk4x12_blink_set,
};

static struct platform_device kodari_leds_gpio = {
	.name = "leds-gpio",
	.id   = -1,
	.dev  = {
		.platform_data = &kodari_gpio_led_info,
	},
};


//nskang
#include <plat/kodari-usb-phy.h>
char std_patch[1024];


int test_count = 0;
int check_times = 0;
int spin_inform_to_android;
//end_nskang

static ssize_t show_value(struct device *dev, struct device_attribute *attr, char *buf) {
	int ivalue;

	printk(KERN_ERR "===== S/W reset host_link_%d_rst ===== \n", ivalue);
	exynos_hsic_reset_control(ivalue);
	return;
}

static DEVICE_ATTR(std_patch, 0777, show_value, NULL);

static int sysfs_patch_create_file(struct device *dev) {

int result = 0;

result = device_create_file(dev, &dev_attr_std_patch);
if (result) return result;

return 0;

}

static void sysfs_std_patch_dev_release(struct device *dev) {}
static struct platform_device the_patch = {
.name = "patch_driver",
.id = -1,
.dev = {
.release = sysfs_std_patch_dev_release,
}
};

//static int __init sysfs_std_patch_init(void) {
static int sysfs_std_patch_init(void) {	
int err = 0;

memset(std_patch, 0, sizeof(std_patch));

err = platform_device_register(&the_patch);
if (err) {
printk("platform_device_register error\n");
return err;
}

//dev_set_drvdata(&the_patch.dev, (void*)data);

err = sysfs_patch_create_file(&the_patch.dev);
if (err) {
printk("sysfs_create_file error\n");
goto sysfs_err;
}

return 0;

sysfs_err:
kfree(std_patch);

alloc_err:
platform_device_unregister(&the_patch);
return err;
}


//end_nskang


//hgs buzzer driver
#if defined(CONFIG_BUZZER_KODARI)
static struct kodari_buzzer_platform_data kodari_buzzer_pdata = {
	.android_dev_name = "buzzer",
	.buzzer_PWM = NULL,
	.Hz = 3100, // SMT-Module(2400) -> CSS-Module(3100)
};

static struct platform_device kodari_buzzer_device = {
	.name	= "kodari_buzzer",
	.id 	= -1,
	.dev	= {
		.platform_data = &kodari_buzzer_pdata,
	},
};

#endif

//hgs key driver

#ifdef CONFIG_BATTERY_SAMSUNG
static struct platform_device samsung_device_battery = {
	.name	= "samsung-fake-battery",
	.id	= -1,
};
#endif


#define GPIO_KEYS(_code, _gpio, _active_low, _iswake, _hook)	\
	{\
		.code = _code,\
		.gpio = _gpio,\
		.active_low = _active_low,\
		.type = EV_KEY,\
		.wakeup = _iswake,\
		.debounce_interval = 10,\
		.value = 0,\
	}


/*		.isr_hook = _hook,\ */
static struct gpio_keys_button kodari_buttons[] = {
	GPIO_KEYS(KEY_POWER, GPIO_POWER_KEY,
			1, 1, 0),
	GPIO_KEYS(KEY_MENU, GPIO_MENU_KEY,
			0, 0, 0),
};

struct gpio_keys_platform_data kodari_gpiokeys_platform_data = {
	.buttons = kodari_buttons,
	.nbuttons = ARRAY_SIZE(kodari_buttons),
};

static struct platform_device kodari_keypad = {
	.name	= "gpio-keys",
	.id	= -1,
	.num_resources	= 0,
	.dev	= {
		.platform_data = &kodari_gpiokeys_platform_data,
	},
};


//leehc add headset detect

static struct gpio_switch gpio_switches[] = {
	{
       	.name = "h2w",
      	.gpio = GPIO_EAR_JACT_DET,        
	},
    	{
		.name = "uart2",
	      .gpio = GPIO_UART2_DETECT,        
       },
	{
		.name = "ssd_io",
       	.gpio = GPIO_SATA_IO_DETECT,     
	},
};

static struct gpio_switch_platform_data gpio_switch_data = {
       .num_switches = ARRAY_SIZE(gpio_switches),
	 .switches = gpio_switches,
};

static struct platform_device switches_gpio = {
	.name	= "switch-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_switch_data,
	},
};



#ifdef CONFIG_F1Q28BT
static struct platform_device f1q28_device_bluetooth = {
	.name             = "f1q28-bt",
	.id               = -1,
};
#endif

#ifdef CONFIG_BT830
static struct platform_device bt830_device_bluetooth = {
	.name             = "bt830",
	.id               = -1,
};
#endif

/*
static struct gpio_switch headset_switch_data = {
       .name = "h2w",
       .gpio = GPIO_EAR_JACT_DET,        
};


static struct gpio_switch_platform_data headset_switch_data = {
       .name = "h2w",
       .gpio = GPIO_EAR_JACT_DET,        
};

static struct platform_device headset_switch_device = {
       .name             = "switch-gpio",
       .dev = {
               .platform_data    = &headset_switch_data,
       }
};


static void __init kodari_headset_init(void)
{
	s3c_gpio_cfgpin(GPIO_EAR_JACT_DET, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_EAR_JACT_DET, S3C_GPIO_PULL_DOWN);
}
*/

static void __init kodari_switch_init(void)
{
	s3c_gpio_cfgpin(GPIO_EAR_JACT_DET, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_EAR_JACT_DET, S3C_GPIO_PULL_DOWN);

	s3c_gpio_cfgpin(GPIO_UART2_DETECT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_UART2_DETECT, S3C_GPIO_PULL_NONE);

	s3c_gpio_setpull(GPIO_SATA_IO_DETECT, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_SATA_IO_DETECT, S3C_GPIO_INPUT);	
}

/*
// add uart0 detect
static struct gpio_switch_platform_data uart0_switch_data = {
       .name = "uart0",
       .gpio = GPIO_UART0_DETECT,        
};

static struct platform_device uart0_switch_device = {
       .name             = "switch-gpio",
       .dev = {
               .platform_data    = &uart0_switch_data,
       }
};

static void __init kodari_uart0_detect_gpio_init(void)
{
	s3c_gpio_cfgpin(GPIO_UART0_DETECT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_UART0_DETECT, S3C_GPIO_PULL_UP);
}

//ssd io detect pin
static struct gpio_switch_platform_data ssd_io_switch_data = {
       .name = "ssd_io",
       .gpio = GPIO_SATA_IO_DETECT,        
};

static struct platform_device ssd_io_switch_device = {
       .name             = "switch-gpio",
       .dev = {
               .platform_data    = &ssd_io_switch_data,
       }
};

static void __init kodari_ssd_io_detect_gpio_init(void)
{
	s3c_gpio_setpull(GPIO_SATA_IO_DETECT, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_SATA_IO_DETECT, S3C_GPIO_INPUT);	
}

*/









static void __init kodari_gpio_power_init(void)
{
//	int err = 0;

	s3c_gpio_setpull(GPIO_POWER_KEY, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_POWER_KEY, S3C_GPIO_SFN(0xF));
	

//	err = gpio_request_one(GPIO_POWER_KEY, GPIOF_IN, "GPX1");
//	if (err) {
//		printk(KERN_ERR "failed to request GPX1 for "
//				"suspend/resume control\n");
//		return;
//	}
//	gpio_free(GPIO_POWER_KEY);

	//key menu	
	//err = gpio_request_one(GPIO_MENU_MEY, 0, "GPX1");
	//if (err) {
	//	printk(KERN_ERR "failed to request GPX1 for "
	//			"suspend/resume control\n");
	//	return;
	//}
	s3c_gpio_setpull(GPIO_MENU_KEY, S3C_GPIO_PULL_DOWN);
	//gpio_free(GPIO_MENU_MEY);
}

/* key pad map
 *
 * s1 0, 1        key_1    2      back
 * s2 0, 4        key_2    3      up
 * s3 0, 5        key_3    4      down
 * s4 0, 6        key_4    5      home
 * s5 0, 7        key_5    6      menu
 * jog_down  1, 1   key_d   32    voldown
 * jog_left  1, 4  key_a   30     left
 * jog_right  1. 5  key_B   48    right
 * jog_enter 1. 6  key_e   18     center
 * jog_up 1. 7  key_c   46        vol up
 *
 */  

static uint32_t kodari_keymap[] __initdata = {
	/* KEY(row, col, keycode) */
	KEY(0, 1, KEY_1), KEY(0, 6, KEY_4)
//	KEY(0, 6, KEY_4), KEY(0, 7, KEY_5)
//	KEY(1, 1, KEY_D), KEY(1, 4, KEY_A), KEY(1, 5, KEY_B),
//	KEY(1, 6, KEY_E), KEY(1, 7, KEY_C)
};


static struct matrix_keymap_data kodari_keymap_data __initdata = {
	.keymap		= kodari_keymap,
	.keymap_size	= ARRAY_SIZE(kodari_keymap),
};

static struct samsung_keypad_platdata kodari_keypad_data __initdata = {
	.keymap_data	= &kodari_keymap_data,
	.rows		= 1,
	.cols		= 7,
};

#ifdef CONFIG_VIDEO_FIMG2D
static struct fimg2d_platdata fimg2d_data __initdata = {
	.hw_ver = 0x41,
	.parent_clkname = "mout_g2d0",
	.clkname = "sclk_fimg2d",
	.gate_clkname = "fimg2d",
	.clkrate = 201 * 1000000,	/* 200 Mhz */
};
#endif

#ifdef CONFIG_EXYNOS_C2C
struct exynos_c2c_platdata kodari_c2c_pdata = {
	.setup_gpio	= NULL,
	.shdmem_addr	= C2C_SHAREDMEM_BASE,
	.shdmem_size	= C2C_MEMSIZE_64,
	.ap_sscm_addr	= NULL,
	.cp_sscm_addr	= NULL,
	.rx_width	= C2C_BUSWIDTH_16,
	.tx_width	= C2C_BUSWIDTH_16,
	.clk_opp100	= 400,
	.clk_opp50	= 266,
	.clk_opp25	= 0,
	.default_opp_mode	= C2C_OPP50,
	.get_c2c_state	= NULL,
	.c2c_sysreg	= S5P_VA_CMU + 0x12000,
};
#endif

#ifdef CONFIG_USB_EXYNOS_SWITCH
static struct s5p_usbswitch_platdata kodari_usbswitch_pdata;

static void __init kodari_usbswitch_init(void)
{
	struct s5p_usbswitch_platdata *pdata = &kodari_usbswitch_pdata;
	int err;

	//pdata->gpio_host_detect = EXYNOS4_GPX3(5); /* low active */
	pdata->gpio_host_detect = GPIO_HUB1_3V3_EN_H; /* high active */
	//err = gpio_request_one(pdata->gpio_host_detect, GPIOF_IN, "HOST_DETECT");
	err = gpio_request_one(pdata->gpio_host_detect, GPIOF_OUT_INIT_LOW, "HOST_DETECT");
	if (err) {
		printk(KERN_ERR "failed to request gpio_host_detect\n");
		return;
	}

	s3c_gpio_cfgpin(pdata->gpio_host_detect, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(pdata->gpio_host_detect, S3C_GPIO_PULL_NONE);
	gpio_free(pdata->gpio_host_detect);

#ifdef CONFIG_KODARI_USB_HUB_SEPARATE
	pdata->gpio_host_detect2 = GPIO_HUB2_3V3_EN_H; /* high active */
	err = gpio_request_one(pdata->gpio_host_detect2, GPIOF_OUT_INIT_LOW, "HOST_DETECT2");
	if (err) {
		printk(KERN_ERR "failed to request gpio_host_detect2\n");
		return;
	}

	s3c_gpio_cfgpin(pdata->gpio_host_detect2, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(pdata->gpio_host_detect2, S3C_GPIO_PULL_NONE);
	gpio_free(pdata->gpio_host_detect2);
#endif

	//pdata->gpio_device_detect = EXYNOS4_GPX3(4); /* high active */
	//leehc
	//pdata->gpio_device_detect = EXYNOS4_GPX3(4); /* high active */
	pdata->gpio_device_detect = GPIO_OTG_EN_H; /* high active */
	//err = gpio_request_one(pdata->gpio_device_detect, GPIOF_IN, "DEVICE_DETECT");
	err = gpio_request_one(pdata->gpio_device_detect, GPIOF_OUT_INIT_LOW, "DEVICE_DETECT");
	if (err) {
		printk(KERN_ERR "failed to request gpio_host_detect for\n");
		return;
	}

	s3c_gpio_cfgpin(pdata->gpio_device_detect, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(pdata->gpio_device_detect, S3C_GPIO_PULL_NONE);
	gpio_free(pdata->gpio_device_detect);

	//if (samsung_board_rev_is_0_0())
//		pdata->gpio_host_vbus = 0;
//	else {
		//pdata->gpio_host_vbus = EXYNOS4_GPL2(0);
#ifdef CONFIG_KODARI_USB_HUB_SEPARATE
		pdata->gpio_host_vbus = GPIO_USB_HUB_1_nRST_H;
#else
		pdata->gpio_host_vbus = GPIO_USB_HUB_1_2_nRST_H;
#endif
		//err = gpio_request_one(pdata->gpio_host_vbus, GPIOF_OUT_INIT_LOW, "HOST_VBUS_CONTROL");
		err = gpio_request_one(pdata->gpio_host_vbus, GPIOF_OUT_INIT_HIGH, "HOST_VBUS_CONTROL");
		if (err) {
			printk(KERN_ERR "failed to request gpio_host_vbus\n");
			return;
		}
		s3c_gpio_cfgpin(pdata->gpio_host_vbus, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(pdata->gpio_host_vbus, S3C_GPIO_PULL_NONE);
		gpio_export(GPIO_USB_HUB_1_nRST_H,1);
		printk(KERN_ERR "!!!!!!! HUB Reset GPIO #1: %d\n", GPIO_USB_HUB_1_nRST_H);		
//nskang		gpio_free(pdata->gpio_host_vbus);
//	}

#ifdef CONFIG_KODARI_USB_HUB_SEPARATE
	pdata->gpio_host_vbus2 = GPIO_USB_HUB_2_nRST_H;
	err = gpio_request_one(pdata->gpio_host_vbus2, GPIOF_OUT_INIT_HIGH, "HOST_VBUS_CONTROL2");
	if (err) {
		printk(KERN_ERR "failed to request gpio_host_vbus2\n");
		return;
	}
	s3c_gpio_cfgpin(pdata->gpio_host_vbus2, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(pdata->gpio_host_vbus2, S3C_GPIO_PULL_NONE);
		gpio_export(GPIO_USB_HUB_2_nRST_H,1);
		printk(KERN_ERR "!!!!!!! HUB Reset GPIO #2: %d\n", GPIO_USB_HUB_2_nRST_H);			
//nskang	gpio_free(pdata->gpio_host_vbus2);
#endif

	s5p_usbswitch_set_platdata(pdata);
}
#endif

#ifdef CONFIG_BUSFREQ_OPP
/* BUSFREQ to control memory/bus*/
static struct device_domain busfreq;
#endif

static struct platform_device exynos4_busfreq = {
	.id = -1,
	.name = "exynos-busfreq",
};

static struct platform_device *smdk4412_devices[] __initdata = {
	&s3c_device_adc,
};

static struct platform_device *kodari_devices[] __initdata = {
//hgs Vibrator
#if defined(CONFIG_VIBRATOR_KODARI)
		&kodari_vibrator_device,
#endif

//hgs Buzzer
#if defined(CONFIG_BUZZER_KODARI)
		&kodari_buzzer_device,
#endif


	
	/* Samsung Power Domain */
	&exynos4_device_pd[PD_MFC],
	&exynos4_device_pd[PD_G3D],
	&exynos4_device_pd[PD_LCD0],
	&exynos4_device_pd[PD_CAM],
	&exynos4_device_pd[PD_TV],
	&exynos4_device_pd[PD_GPS],
	&exynos4_device_pd[PD_GPS_ALIVE],
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	&exynos4_device_pd[PD_ISP],
#endif
#ifdef CONFIG_FB_MIPI_DSIM
	&s5p_device_mipi_dsim,
#endif
	/* legacy fimd */
#ifdef CONFIG_FB_S5P
	&s3c_device_fb,
#endif
	&s3c_device_wdt,
	&s3c_device_rtc,
	&s3c_device_i2c0, //pmic
	&s3c_device_i2c1, //ov5642
#if defined(CONFIG_KODARI_TYPE_CAR) //HDMI
	&s3c_device_i2c2,
#endif
        &s3c_device_i2c3,
#if defined(CONFIG_SENSORS_BMM050) || defined(CONFIG_SENSORS_BMA250)
	&s3c_device_i2c4, //sensor
#endif
#if defined(CONFIG_TOUCHSCREEN_TSC2004)	
	&s3c_device_i2c5,
#endif
	&s3c_device_i2c7,
#ifdef CONFIG_USB_EHCI_S5P
	&s5p_device_ehci,
#endif
#ifdef CONFIG_USB_OHCI_S5P
	&s5p_device_ohci,
#endif
#ifdef CONFIG_USB_GADGET
	&s3c_device_usbgadget,
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	&s3c_device_rndis,
#endif
#ifdef CONFIG_USB_ANDROID
	&s3c_device_android_usb,
	&s3c_device_usb_mass_storage,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC
	&s3c_device_hsmmc0,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	&s3c_device_hsmmc1,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	&s3c_device_hsmmc2,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	&s3c_device_hsmmc3,
#endif
#ifdef CONFIG_S5P_DEV_MSHC
	&s3c_device_mshci,
#endif
#ifdef CONFIG_EXYNOS4_DEV_DWMCI
	&exynos_device_dwmci,
#endif
#ifdef CONFIG_SND_SAMSUNG_AC97
	&exynos_device_ac97,
#endif
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos_device_i2s0,
#endif
#ifdef CONFIG_SND_SAMSUNG_PCM
#error CONFIG_SND_SAMSUNG_PCM
	&exynos_device_pcm0,
#endif
#ifdef CONFIG_SND_SAMSUNG_SPDIF
	&exynos_device_spdif,
#endif
#if defined(CONFIG_SND_SAMSUNG_RP) || defined(CONFIG_SND_SAMSUNG_ALP)
	&exynos_device_srp,
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	&exynos4_device_fimc_is,
#endif
#if defined(CONFIG_KODARI_TYPE_CAR)
#ifdef CONFIG_VIDEO_TVOUT
	&s5p_device_tvout,
	&s5p_device_cec,
	&s5p_device_hpd,
#endif
#endif
#ifdef CONFIG_VIDEO_EXYNOS_TV
	&s5p_device_i2c_hdmiphy,
	&s5p_device_hdmi,
	&s5p_device_sdo,
	&s5p_device_mixer,
	&s5p_device_cec,
#endif
#if defined(CONFIG_VIDEO_FIMC)
	&s3c_device_fimc0,
	&s3c_device_fimc1,
	&s3c_device_fimc2,
	&s3c_device_fimc3,
/* CONFIG_VIDEO_SAMSUNG_S5P_FIMC is the feature for mainline */
#elif defined(CONFIG_VIDEO_SAMSUNG_S5P_FIMC)
	&s5p_device_fimc0,
	&s5p_device_fimc1,
	&s5p_device_fimc2,
	&s5p_device_fimc3,
#endif
#if defined(CONFIG_VIDEO_FIMC_MIPI)
	&s3c_device_csis0,
	&s3c_device_csis1,
#elif defined(CONFIG_VIDEO_S5P_MIPI_CSIS)
	&s5p_device_mipi_csis0,
	&s5p_device_mipi_csis1,
#endif
#ifdef CONFIG_VIDEO_S5P_MIPI_CSIS
	&mipi_csi_fixed_voltage,
#endif
#if defined(CONFIG_VIDEO_MFC5X) || defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
	&s5p_device_mfc,
#endif
#ifdef CONFIG_S5P_SYSTEM_MMU
	&SYSMMU_PLATDEV(g2d_acp),
	&SYSMMU_PLATDEV(fimc0),
	&SYSMMU_PLATDEV(fimc1),
	&SYSMMU_PLATDEV(fimc2),
	&SYSMMU_PLATDEV(fimc3),
	&SYSMMU_PLATDEV(jpeg),
	&SYSMMU_PLATDEV(mfc_l),
	&SYSMMU_PLATDEV(mfc_r),
	&SYSMMU_PLATDEV(tv),
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	&SYSMMU_PLATDEV(is_isp),
	&SYSMMU_PLATDEV(is_drc),
	&SYSMMU_PLATDEV(is_fd),
	&SYSMMU_PLATDEV(is_cpu),
#endif
#endif /* CONFIG_S5P_SYSTEM_MMU */
#ifdef CONFIG_ION_EXYNOS
	&exynos_device_ion,
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	&exynos_device_flite0,
	&exynos_device_flite1,
#endif
#ifdef CONFIG_VIDEO_FIMG2D
	&s5p_device_fimg2d,
#endif
#ifdef CONFIG_EXYNOS_MEDIA_DEVICE
	&exynos_device_md0,
#endif
#ifdef CONFIG_VIDEO_JPEG_V2X
	&s5p_device_jpeg,
#endif	
	&samsung_asoc_dma,
	&samsung_asoc_idma,
#ifdef CONFIG_BATTERY_SAMSUNG
	&samsung_device_battery,
#endif
	&samsung_device_keypad,
#ifdef CONFIG_EXYNOS_C2C
	&exynos_device_c2c,
#endif
#ifdef CONFIG_SMSC911X
	&kodari_smsc911x,
#endif
#ifdef CONFIG_S3C64XX_DEV_SPI
	&exynos_device_spi0,
#ifndef CONFIG_FB_S5P_LMS501KF03
	&exynos_device_spi1,
#endif
	&exynos_device_spi2,
#endif
#ifdef CONFIG_EXYNOS_THERMAL
	&exynos_device_tmu,
#endif
#ifdef CONFIG_S5P_DEV_ACE
	&s5p_device_ace,
#endif
	&exynos4_busfreq,
	&switches_gpio,
#ifdef CONFIG_F1Q28BT
	&f1q28_device_bluetooth,
#endif
#ifdef CONFIG_BT830
	&bt830_device_bluetooth,
#endif
//	&ssd_io_switch_device,
	//add headset detected for android
//	&headset_switch_device,
	//add uart0 detected for android
//	&uart0_switch_device,

};

#ifdef CONFIG_EXYNOS_THERMAL
/* below temperature base on the celcius degree */
struct tmu_data exynos_tmu_data __initdata = {
	.ts = {
#if 1 //orignal value
		.stop_throttle  = 82,
		.start_throttle = 85,
		.stop_warning  = 97, // 102 -> 97
		.start_warning = 100, // 105 -> 100
		.start_tripping = 110,		/* temp to do tripping */
		.start_hw_tripping = 113,	/* temp to do hw_trpping*/
		.stop_mem_throttle = 80,
		.start_mem_throttle = 85,
#else //tune value
		.stop_throttle	= 72,
		.start_throttle = 75,
		.stop_warning  = 92,
		.start_warning = 95,
		.start_tripping = 100,		/* temp to do tripping */
		.start_hw_tripping = 103, /* temp to do hw_trpping*/
		.stop_mem_throttle = 70,
		.start_mem_throttle = 75,

//		.stop_throttle	= 65,
//		.start_throttle = 70,
//		.stop_warning  = 75,
//		.start_warning = 80,
//		.start_tripping = 85,		/* temp to do tripping */
//		.start_hw_tripping = 90, /* temp to do hw_trpping*/
//		.stop_mem_throttle = 70,
//		.start_mem_throttle = 75,
#endif
		.stop_tc = 13,
		.start_tc = 10,
	},
	.cpulimit = {
		.throttle_freq = 400000, // 800000 -> 400000
		.warning_freq = 200000,
	},
	.temp_compensate = {
		.arm_volt = 925000, /* vdd_arm in uV for temperature compensation */
		.bus_volt = 900000, /* vdd_bus in uV for temperature compensation */
		.g3d_volt = 900000, /* vdd_g3d in uV for temperature compensation */
	},
	.efuse_value = 55,
	.slope = 0x10008802,
	.mode = 0,
};
#endif

#if defined(CONFIG_KODARI_TYPE_CAR)
#if defined(CONFIG_VIDEO_TVOUT)
static struct s5p_platform_hpd hdmi_hpd_data __initdata = {

};
static struct s5p_platform_cec hdmi_cec_data __initdata = {

};
#endif
#endif

#ifdef CONFIG_VIDEO_EXYNOS_HDMI_CEC
static struct s5p_platform_cec hdmi_cec_data __initdata = {

};
#endif

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
static struct s5p_fimc_isp_info isp_info[] = {
#if defined(WRITEBACK_ENABLED)
	{
		.board_info	= &writeback_info,
		.bus_type	= FIMC_LCD_WB,
		.i2c_bus_num	= 0,
		.mux_id		= 0, /* A-Port : 0, B-Port : 1 */
		.flags		= FIMC_CLK_INV_VSYNC,
	},
#endif
};

static void __init kodari_subdev_config(void)
{
	s3c_fimc0_default_data.isp_info[0] = &isp_info[0];
	s3c_fimc0_default_data.isp_info[0]->use_cam = true;
	s3c_fimc0_default_data.isp_info[1] = &isp_info[1];
	s3c_fimc0_default_data.isp_info[1]->use_cam = true;
	/* support using two fimc as one sensore */
	{
		static struct s5p_fimc_isp_info camcording1;
		static struct s5p_fimc_isp_info camcording2;
		memcpy(&camcording1, &isp_info[0], sizeof(struct s5p_fimc_isp_info));
		memcpy(&camcording2, &isp_info[1], sizeof(struct s5p_fimc_isp_info));
		s3c_fimc1_default_data.isp_info[0] = &camcording1;
		s3c_fimc1_default_data.isp_info[0]->use_cam = false;
		s3c_fimc1_default_data.isp_info[1] = &camcording2;
		s3c_fimc1_default_data.isp_info[1]->use_cam = false;
	}
}
static void __init kodari_camera_config(void)
{
	/* CAM A port(b0010) : PCLK, VSYNC, HREF, DATA[0-4] */
	s3c_gpio_cfgrange_nopull(EXYNOS4212_GPJ0(0), 8, S3C_GPIO_SFN(2));
	/* CAM A port(b0010) : DATA[5-7], CLKOUT(MIPI CAM also), FIELD */
	s3c_gpio_cfgrange_nopull(EXYNOS4212_GPJ1(0), 5, S3C_GPIO_SFN(2));
#if 0
	/* CAM B port(b0011) : PCLK, DATA[0-6] */
	s3c_gpio_cfgrange_nopull(EXYNOS4212_GPM0(0), 8, S3C_GPIO_SFN(3));
	/* CAM B port(b0011) : FIELD, DATA[7]*/
	s3c_gpio_cfgrange_nopull(EXYNOS4212_GPM1(0), 2, S3C_GPIO_SFN(3));
	/* CAM B port(b0011) : VSYNC, HREF, CLKOUT*/
	s3c_gpio_cfgrange_nopull(EXYNOS4212_GPM2(0), 3, S3C_GPIO_SFN(3));
#endif
	/* note : driver strength to max is unnecessary */
}
#endif /* CONFIG_VIDEO_SAMSUNG_S5P_FIMC */

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
static void __set_flite_camera_config(struct exynos_platform_flite *data,
					u32 active_index, u32 max_cam)
{
	data->active_cam_index = active_index;
	data->num_clients = max_cam;
}

static void __init kodari_set_camera_flite_platdata(void)
{
	int flite0_cam_index = 0;
	int flite1_cam_index = 0;
	__set_flite_camera_config(&exynos_flite0_default_data, 0, flite0_cam_index);
	__set_flite_camera_config(&exynos_flite1_default_data, 0, flite1_cam_index);
}
#endif

#if defined(CONFIG_CMA)
static void __init exynos4_reserve_mem(void)
{
	static struct cma_region regions[] = {
#ifndef CONFIG_VIDEOBUF2_ION
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_TV
		{
			.name = "tv",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_TV * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_JPEG
		{
			.name = "jpeg",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_JPEG * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_AUDIO_SAMSUNG_MEMSIZE_SRP
		{
			.name = "srp",
			.size = CONFIG_AUDIO_SAMSUNG_MEMSIZE_SRP * SZ_1K,
			.start = 0,
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMG2D
		{
			.name = "fimg2d",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMG2D * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD
		{
			.name = "fimd",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC0
		{
			.name = "fimc0",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC0 * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC2
		{
			.name = "fimc2",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC2 * SZ_1K,
			.start = 0
		},
#endif
#if !defined(CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION) && \
	defined(CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC3)
		{
			.name = "fimc3",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC3 * SZ_1K,
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC1
		{
			.name = "fimc1",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC1 * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_NORMAL
		{
			.name = "mfc-normal",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_NORMAL * SZ_1K,
			{ .alignment = 1 << 17 },
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC1
		{
			.name = "mfc1",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC1 * SZ_1K,
			{ .alignment = 1 << 17 },
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC0
		{
			.name = "mfc0",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC0 * SZ_1K,
			{ .alignment = 1 << 17 },
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC
		{
			.name = "mfc",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC * SZ_1K,
			{ .alignment = 1 << 17 },
		},
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
		{
			.name = "fimc_is",
			.size = CONFIG_VIDEO_EXYNOS_MEMSIZE_FIMC_IS * SZ_1K,
			{
				.alignment = 1 << 26,
			},
			.start = 0
		},
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS_BAYER
		{
			.name = "fimc_is_isp",
			.size = CONFIG_VIDEO_EXYNOS_MEMSIZE_FIMC_IS_ISP * SZ_1K,
			.start = 0
		},
#endif
#endif
#if !defined(CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION) && \
	defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
		{
			.name		= "b2",
			.size		= 32 << 20,
			{ .alignment	= 128 << 10 },
		},
		{
			.name		= "b1",
			.size		= 32 << 20,
			{ .alignment	= 128 << 10 },
		},
		{
			.name		= "fw",
			.size		= 1 << 20,
			{ .alignment	= 128 << 10 },
		},
#endif
#else /* !CONFIG_VIDEOBUF2_ION */
#ifdef CONFIG_FB_S5P
#error CONFIG_FB_S5P is defined. Select CONFIG_FB_S3C, instead
#endif
		{
			.name	= "ion",
			.size	= CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE * SZ_1K,
		},
#endif /* !CONFIG_VIDEOBUF2_ION */
		{
			.size = 0
		},
	};
#ifdef CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION
	static struct cma_region regions_secure[] = {
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC3
		{
			.name = "fimc3",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC3 * SZ_1K,
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD_VIDEO
		{
			.name = "video",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD_VIDEO * SZ_1K,
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_SECURE
		{
			.name = "mfc-secure",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC_SECURE * SZ_1K,
		},
#endif
		{
			.name = "sectbl",
			.size = SZ_1M,
		},
		{
			.size = 0
		},
	};
#else /* !CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION */
	struct cma_region *regions_secure = NULL;
#endif
	static const char map[] __initconst =
#ifdef CONFIG_EXYNOS_C2C
		"samsung-c2c=c2c_shdmem;"
#endif
		"s3cfb.0/fimd=fimd;exynos4-fb.0/fimd=fimd;"
#ifdef CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION
		"s3cfb.0/video=video;exynos4-fb.0/video=video;"
#endif
		"s3c-fimc.0=fimc0;s3c-fimc.1=fimc1;s3c-fimc.2=fimc2;s3c-fimc.3=fimc3;"
		"exynos4210-fimc.0=fimc0;exynos4210-fimc.1=fimc1;exynos4210-fimc.2=fimc2;exynos4210-fimc.3=fimc3;"
#ifdef CONFIG_VIDEO_MFC5X
		"s3c-mfc/A=mfc0,mfc-secure;"
		"s3c-mfc/B=mfc1,mfc-normal;"
		"s3c-mfc/AB=mfc;"
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_S5P_MFC
		"s5p-mfc/f=fw;"
		"s5p-mfc/a=b1;"
		"s5p-mfc/b=b2;"
#endif
		"samsung-rp=srp;"
		"s5p-jpeg=jpeg;"
		"exynos4-fimc-is/f=fimc_is;"
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS_BAYER
		"exynos4-fimc-is/i=fimc_is_isp;"
#endif
		"s5p-mixer=tv;"
		"s5p-fimg2d=fimg2d;"
		"ion-exynos=ion,fimd,fimc0,fimc1,fimc2,fimc3,fw,b1,b2;"
#ifdef CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION
		"s5p-smem/video=video;"
		"s5p-smem/sectbl=sectbl;"
#endif
		"s5p-smem/mfc=mfc0,mfc-secure;"
		"s5p-smem/fimc=fimc3;"
		"s5p-smem/mfc-shm=mfc1,mfc-normal;"
		"s5p-smem/fimd=fimd;";

	s5p_cma_region_reserve(regions, regions_secure, 0, map);
}
#else
static inline void exynos4_reserve_mem(void)
{
}
#endif /* CONFIG_CMA */

#if defined(CONFIG_KODARI_TYPE_CAR)
/* LCD Backlight data */
static struct samsung_bl_gpio_info kodari_bl_gpio_info = {
	.no = GPIO_LCD_PWM,
	.func = S3C_GPIO_SFN(2),
};

static struct platform_pwm_backlight_data kodari_bl_data = {
	.pwm_id = 1,
};
#endif

static void __init kodari_map_io(void)
{
	clk_xusbxti.rate = 24000000;
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(kodari_uartcfgs, ARRAY_SIZE(kodari_uartcfgs));

	exynos4_reserve_mem();
}
#if defined(CONFIG_SMSC911X)

void kodari_smsc911x_resume(void)
{
	u32 cs1;
//	unsigned int gpio;
	int err;
	
	/* configure nCS1 width to 16 bits */
	cs1 = __raw_readl(S5P_SROM_BW) &
		~(S5P_SROM_BW__CS_MASK << S5P_SROM_BW__NCS1__SHIFT);
	cs1 |= ((1 << S5P_SROM_BW__DATAWIDTH__SHIFT) |
		(1 << S5P_SROM_BW__WAITENABLE__SHIFT) |
		(1 << S5P_SROM_BW__BYTEENABLE__SHIFT)) <<
		S5P_SROM_BW__NCS1__SHIFT;
	__raw_writel(cs1, S5P_SROM_BW);

	/* set timing for nCS1 suitable for ethernet chip */
	__raw_writel((0x1 << S5P_SROM_BCX__PMC__SHIFT) |
			 (0x9 << S5P_SROM_BCX__TACP__SHIFT) |
			 (0xc << S5P_SROM_BCX__TCAH__SHIFT) |
			 (0x1 << S5P_SROM_BCX__TCOH__SHIFT) |
			 (0x6 << S5P_SROM_BCX__TACC__SHIFT) |
			 (0x1 << S5P_SROM_BCX__TCOS__SHIFT) |
			 (0x1 << S5P_SROM_BCX__TACS__SHIFT), S5P_SROM_BC1);	

#if !defined(CONFIG_KODARI_ETHERNET_WOL)
	err = gpio_request_one(GPIO_ETHERNET_nRST_L,
				GPIOF_OUT_INIT_HIGH, "GPX1");
	if (err) {
		printk(KERN_ERR "failed to request GPM3 for "
				"smsc911x reset control\n");
	}
	gpio_set_value(GPIO_ETHERNET_nRST_L, 1);
	mdelay(10);
	gpio_set_value(GPIO_ETHERNET_nRST_L, 0);
	mdelay(50);
	gpio_set_value(GPIO_ETHERNET_nRST_L, 1);
	mdelay(500);
	gpio_free(GPIO_ETHERNET_nRST_L);
#endif
}

static void __init kodari_smsc911x_init(void)
{
	u32 cs1;
	unsigned int gpio;
	int err;
	

  /* GPY0[1] LAN nRST*/
	s3c_gpio_cfgpin(EXYNOS4_GPY0(1), S3C_GPIO_SFN(2));
	s3c_gpio_setpull(EXYNOS4_GPY0(1), S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(EXYNOS4_GPY0(1), S5P_GPIO_DRVSTR_LV4);

  /* GPY0[4:5] nRD nWR*/
	for (gpio = EXYNOS4_GPY0(4); gpio <= EXYNOS4_GPY0(5); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}
   /*  GPY1[0:3]*/
//	for (gpio = EXYNOS4_GPY1(0); gpio <= EXYNOS4_GPY1(3); gpio++) {
//		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
//		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
//		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
//	}
   /*  GPY3[0:6] addr smsc9221*/
	for (gpio = EXYNOS4_GPY3(0); gpio <= EXYNOS4_GPY3(7); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}

#if defined(CONFIG_KODARI_TYPE_CAR)
	/* GPY3[7], GPY4[0] addr  smsc9311*/
	s3c_gpio_cfgpin(EXYNOS4_GPY3(7), S3C_GPIO_SFN(2));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	
	s3c_gpio_cfgpin(EXYNOS4_GPY4(0), S3C_GPIO_SFN(2));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
#endif

   /*  GPY5[0:7] data 0:7*/
	for (gpio = EXYNOS4_GPY5(0); gpio <= EXYNOS4_GPY5(6); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}


	//leehc add setup data lines
	/*	GPY6[0:7] data 8:15*/
	 for (gpio = EXYNOS4_GPY6(0); gpio <= EXYNOS4_GPY6(7); gpio++) {
		 s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		 s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		 s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	 }


	/* configure nCS1 width to 16 bits */
	cs1 = __raw_readl(S5P_SROM_BW) &
		~(S5P_SROM_BW__CS_MASK << S5P_SROM_BW__NCS1__SHIFT);
	cs1 |= ((1 << S5P_SROM_BW__DATAWIDTH__SHIFT) |
		(1 << S5P_SROM_BW__WAITENABLE__SHIFT) |
		(1 << S5P_SROM_BW__BYTEENABLE__SHIFT)) <<
		S5P_SROM_BW__NCS1__SHIFT;
	__raw_writel(cs1, S5P_SROM_BW);

	/* set timing for nCS1 suitable for ethernet chip */
	__raw_writel((0x1 << S5P_SROM_BCX__PMC__SHIFT) |
		     (0x9 << S5P_SROM_BCX__TACP__SHIFT) |
		     (0xc << S5P_SROM_BCX__TCAH__SHIFT) |
		     (0x1 << S5P_SROM_BCX__TCOH__SHIFT) |
		     (0x6 << S5P_SROM_BCX__TACC__SHIFT) |
		     (0x1 << S5P_SROM_BCX__TCOS__SHIFT) |
		     (0x1 << S5P_SROM_BCX__TACS__SHIFT), S5P_SROM_BC1);


		printk(KERN_ERR "GPIO_ETHERNET_nRST_L %i\n", GPIO_ETHERNET_nRST_L);

		err = gpio_request_one(GPIO_ETHERNET_nRST_L,
				GPIOF_OUT_INIT_HIGH, "GPX1");
		if (err) {
			printk(KERN_ERR "failed to request GPM3 for "
					"smsc911x reset control\n");
		}
		gpio_set_value(GPIO_ETHERNET_nRST_L, 1);
		mdelay(10);
		gpio_set_value(GPIO_ETHERNET_nRST_L, 0);
		mdelay(50);
		gpio_set_value(GPIO_ETHERNET_nRST_L, 1);
		mdelay(10);
		gpio_free(GPIO_ETHERNET_nRST_L);

	//	LAN IRQ20
	s3c_gpio_cfgpin(GPIO_ETHERNET_INT, S3C_GPIO_SFN(0xF));
#if defined(CONFIG_KODARI_TYPE_CAR)
        s3c_gpio_setpull(GPIO_ETHERNET_INT, S3C_GPIO_PULL_UP);
#else
	s3c_gpio_setpull(GPIO_ETHERNET_INT, S3C_GPIO_PULL_NONE);
#endif

#ifdef CONFIG_KODARI_ETHERNET_WOL
	s3c_gpio_cfgpin(GPIO_ETHERNET_PME, S3C_GPIO_SFN(0xF));
#if defined(CONFIG_KODARI_TYPE_CAR)
        s3c_gpio_setpull(GPIO_ETHERNET_PME, S3C_GPIO_PULL_UP);
#else
	s3c_gpio_setpull(GPIO_ETHERNET_PME, S3C_GPIO_PULL_NONE);
#endif
#endif
}
#endif

#if 1 //defined(CONFIG_KODARI_TYPE_MOBILE)
static void kodari_5v_power_on()
{
        gpio_request_one(GPIO_5V_EN_H, GPIOF_OUT_INIT_HIGH, "GPX0_3");
	mdelay(50);
	gpio_free(GPIO_5V_EN_H);
}
#endif
#if 0
static void kodari_bluetooth_power_on(void)
{
	gpio_request_one(GPIO_BLUETOOTH_EN_H, GPIOF_OUT_INIT_HIGH, "GPX0_2");
	mdelay(10);
	gpio_free(GPIO_BLUETOOTH_EN_H);
}


static void kodari_usb_hub_reset(bool on)
{
	if(on) {
		s3c_gpio_setpull(GPIO_SATA_nRST_H, S3C_GPIO_PULL_NONE);		
		s3c_gpio_cfgpin(GPIO_SATA_nRST_H, S3C_GPIO_SFN(0x0));
		gpio_request_one(GPIO_SATA_nRST_H, GPIOF_OUT_INIT_HIGH, "GPL2");
		mdelay(50);
		gpio_free(GPIO_SATA_nRST_H);
		
		gpio_request_one(GPIO_USB_HUB_1_2_nRST_H, GPIOF_OUT_INIT_HIGH, "GPX0");
		s3c_gpio_setpull(GPIO_USB_HUB_1_2_nRST_H, S3C_GPIO_PULL_DOWN);
		mdelay(10);
		gpio_free(GPIO_USB_HUB_1_2_nRST_H);		
	} else {
		s3c_gpio_setpull(GPIO_SATA_nRST_H, S3C_GPIO_PULL_NONE);		
		s3c_gpio_cfgpin(GPIO_SATA_nRST_H, S3C_GPIO_SFN(0x0));
		gpio_request_one(GPIO_USB_HUB_1_2_nRST_H, GPIOF_OUT_INIT_LOW, "GPX0");
		s3c_gpio_setpull(GPIO_USB_HUB_1_2_nRST_H, S3C_GPIO_PULL_DOWN);
		mdelay(10);
		gpio_free(GPIO_USB_HUB_1_2_nRST_H);
	}
		
}

static void kodari_usb_sata_power_on(void)
{
    gpio_request_one(EXYNOS4_GPL2(6), GPIOF_OUT_INIT_HIGH, "GPL2");
    mdelay(10);
    gpio_free(EXYNOS4_GPL2(6));
}
#endif



static void __init exynos_sysmmu_init(void)
{
	ASSIGN_SYSMMU_POWERDOMAIN(fimc0, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(fimc1, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(fimc2, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(fimc3, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(jpeg, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(mfc_l, &exynos4_device_pd[PD_MFC].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(mfc_r, &exynos4_device_pd[PD_MFC].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(tv, &exynos4_device_pd[PD_TV].dev);
#ifdef CONFIG_VIDEO_FIMG2D
	sysmmu_set_owner(&SYSMMU_PLATDEV(g2d_acp).dev, &s5p_device_fimg2d.dev);
#endif
#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC) || defined(CONFIG_VIDEO_MFC5X)
	sysmmu_set_owner(&SYSMMU_PLATDEV(mfc_l).dev, &s5p_device_mfc.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(mfc_r).dev, &s5p_device_mfc.dev);
#endif
#if defined(CONFIG_VIDEO_FIMC)
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc0).dev, &s3c_device_fimc0.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc1).dev, &s3c_device_fimc1.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc2).dev, &s3c_device_fimc2.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc3).dev, &s3c_device_fimc3.dev);
#elif defined(CONFIG_VIDEO_SAMSUNG_S5P_FIMC)
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc0).dev, &s5p_device_fimc0.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc1).dev, &s5p_device_fimc1.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc2).dev, &s5p_device_fimc2.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc3).dev, &s5p_device_fimc3.dev);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_TV
	sysmmu_set_owner(&SYSMMU_PLATDEV(tv).dev, &s5p_device_mixer.dev);
#endif
#if defined(CONFIG_KODARI_TYPE_CAR)
#ifdef CONFIG_VIDEO_TVOUT
	sysmmu_set_owner(&SYSMMU_PLATDEV(tv).dev, &s5p_device_tvout.dev);
#endif
#endif
#ifdef CONFIG_VIDEO_JPEG_V2X
	sysmmu_set_owner(&SYSMMU_PLATDEV(jpeg).dev, &s5p_device_jpeg.dev);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	ASSIGN_SYSMMU_POWERDOMAIN(is_isp, &exynos4_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_drc, &exynos4_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_fd, &exynos4_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_cpu, &exynos4_device_pd[PD_ISP].dev);

	sysmmu_set_owner(&SYSMMU_PLATDEV(is_isp).dev,
						&exynos4_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_drc).dev,
						&exynos4_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_fd).dev,
						&exynos4_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_cpu).dev,
						&exynos4_device_fimc_is.dev);
#endif
}

#define SMDK4412_REV_0_0_ADC_VALUE 0
#define SMDK4412_REV_0_1_ADC_VALUE 443
int samsung_board_rev;

static int get_samsung_board_rev(void)
{
	int				adc_val = 0;
	struct clk		*adc_clk;
	struct resource	*res;
	void __iomem	*adc_regs;
	unsigned int	con;
	int		ret;

	if ((soc_is_exynos4412() && samsung_rev() < EXYNOS4412_REV_1_0) ||
		(soc_is_exynos4212() && samsung_rev() < EXYNOS4212_REV_1_0))
		return SAMSUNG_BOARD_REV_0_0;

	adc_clk = clk_get(NULL, "adc");
	if (unlikely(IS_ERR(adc_clk)))
		return SAMSUNG_BOARD_REV_0_0;

	clk_enable(adc_clk);

	res = platform_get_resource(&s3c_device_adc, IORESOURCE_MEM, 0);
	if (unlikely(!res))
		goto err_clk;

	adc_regs = ioremap(res->start, resource_size(res));
	if (unlikely(!adc_regs))
		goto err_clk;

	writel(S5PV210_ADCCON_SELMUX(3), adc_regs + S5PV210_ADCMUX);

	con = readl(adc_regs + S3C2410_ADCCON);
	con &= ~S3C2410_ADCCON_MUXMASK;
	con &= ~S3C2410_ADCCON_STDBM;
	con &= ~S3C2410_ADCCON_STARTMASK;
	con |=  S3C2410_ADCCON_PRSCEN;

	con |= S3C2410_ADCCON_ENABLE_START;
	writel(con, adc_regs + S3C2410_ADCCON);

	udelay(50);

	adc_val = readl(adc_regs + S3C2410_ADCDAT0) & 0xFFF;
	writel(0, adc_regs + S3C64XX_ADCCLRINT);

	iounmap(adc_regs);
err_clk:
	clk_disable(adc_clk);
	clk_put(adc_clk);

	ret = (adc_val < SMDK4412_REV_0_1_ADC_VALUE/2) ?
			SAMSUNG_BOARD_REV_0_0 : SAMSUNG_BOARD_REV_0_1;

	pr_info("SMDK MAIN Board Rev 0.%d (ADC value:%d)\n", ret, adc_val);
	return ret;
}

#if 0
static int kodari_uhostphy_reset(void)
{
/*
	int err;
	err = gpio_request(EXYNOS4_GPX3(5), "GPX3");
	if (!err) {
		s3c_gpio_setpull(EXYNOS4_GPX3(5), S3C_GPIO_PULL_UP);
		gpio_direction_output(EXYNOS4_GPX3(5), 1);
		gpio_set_value(EXYNOS4_GPX3(5), 1);
		gpio_free(EXYNOS4_GPX3(5));
	}
	*/
	return 0;
}
#endif



int kodari_pshold_activate(int on){
    uint val = readl(S5P_PMU_PS_HOLD_CONTROL);
    if(on){
        val |= (3 << 8);
    }else{/* Sleep forever */
        val &= ~(1 << 8);
        mdelay(100);
    }
    writel(val, S5P_PMU_PS_HOLD_CONTROL);
    if(!on)
        while(1);
    return 0;
}
EXPORT_SYMBOL(kodari_pshold_activate);

#if 0
static int kodari_sound_init(void)
{
  // Ear detect 
	s3c_gpio_cfgpin(GPIO_EAR_JACT_DET, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(GPIO_EAR_JACT_DET, S3C_GPIO_PULL_UP);

  // Mic detect hclee kodari doesnot use this pin
//	s3c_gpio_cfgpin(EXYNOS4_GPX2(2), S3C_GPIO_SFN(0xF));
//	s3c_gpio_setpull(EXYNOS4_GPX2(2), S3C_GPIO_PULL_UP);

	return 0;
}


static void TUSB9261_reset(bool on) 
{
	if(on == true) {
		//tusb reset
		gpio_request_one(GPIO_SATA_nRST_H, GPIOF_OUT_INIT_HIGH, "GPL2");
		mdelay(10);
		gpio_set_value(GPIO_SATA_nRST_H, 1);
		mdelay(10);
		gpio_free(GPIO_SATA_nRST_H);
	} else {
		gpio_request_one(GPIO_SATA_nRST_H, GPIOF_OUT_INIT_LOW, "GPL2");
		mdelay(10);
		gpio_set_value(GPIO_SATA_nRST_H, 0);
		mdelay(10);
		gpio_free(GPIO_SATA_nRST_H);
	}
	
}


static void WiFi_BT_reset(bool on) 
{
	if(on == false) {
		//tusb reset
		gpio_request_one(GPIO_WIFI_EN_L, GPIOF_OUT_INIT_HIGH, "GPIO_WIFI_EN_L");
		mdelay(10);
		gpio_set_value(GPIO_WIFI_EN_L, 1);
		mdelay(10);
		gpio_free(GPIO_WIFI_EN_L);
	} else {
		gpio_request_one(GPIO_WIFI_EN_L, GPIOF_OUT_INIT_LOW, "GPIO_WIFI_EN_L");
		mdelay(10);
		gpio_set_value(GPIO_WIFI_EN_L, 0);
		mdelay(10);
		gpio_free(GPIO_WIFI_EN_L);
	}
	
}

#endif



#if 0
struct kodari_lpm {
	int wake;
	int host_wake;

	struct hrtimer enter_lpm_timer;
	ktime_t enter_lpm_delay;

	struct hci_dev *hdev;

	struct wake_lock wake_lock;
	char wake_lock_name[100];
} kodari_lpm;



struct kodari_eth {
	int wake;
	struct wake_lock wake_lock;
	char wake_lock_name[100];
} kodari_eth_wake;
#endif

static void kodari_export_gpios(void)
{
	//leehc
	//gpio export 
	int iRet;

/*	iRet =gpio_request(GPIO_SATA_nRST_H, "GPL2");
	//gpio export
	if(iRet == 0) {
		iRet = gpio_direction_output(GPIO_SATA_nRST_H,1);
		
		printk(KERN_ERR " ssd reset gpio = %d, ret:%i\n",
			GPIO_SATA_nRST_H, iRet);
		iRet = gpio_export(GPIO_SATA_nRST_H, 0);

		printk(KERN_ERR " ssd reset gpio = %d, ret:%i\n",
			GPIO_SATA_nRST_H, iRet);
	} else {
		printk(KERN_ERR " fail to request gpio = %d, ret:%i\n",
			GPIO_SATA_nRST_H, iRet);
		
	}
*/

	gpio_request(GPIO_SATA_nRST_H, "GPL2_2");
	gpio_direction_output(GPIO_SATA_nRST_H,0);

#ifdef CONFIG_KODARI_USB_HUB_SEPARATE
	// hub1 power 3v3
	iRet =gpio_request(GPIO_HUB1_3V3_EN_H, "GPX1_4");
#else
	// hub power 3v3
	iRet =gpio_request(GPIO_HUB1_3V3_EN_H, "GPL2_5");
#endif
	if(iRet == 0) {
		iRet = gpio_direction_output(GPIO_HUB1_3V3_EN_H,0);
		
		printk(KERN_ERR " hub 3v3 gpio = %d, ret:%i\n",
			GPIO_HUB1_3V3_EN_H, iRet);
		iRet = gpio_export(GPIO_HUB1_3V3_EN_H, 0);
		printk(KERN_ERR " hub1 3v3 gpio = %d, ret:%i\n",
			GPIO_HUB1_3V3_EN_H, iRet);
	} else {
		printk(KERN_ERR " fail to request hub 3v3 gpio = %d, ret:%i\n",
			GPIO_HUB1_3V3_EN_H, iRet);
		
	}
	
#ifdef CONFIG_KODARI_USB_HUB_SEPARATE
	// hub2 power 3v3
	iRet =gpio_request(GPIO_HUB2_3V3_EN_H, "GPX0_5");
	
	if(iRet == 0) {
		iRet = gpio_direction_output(GPIO_HUB2_3V3_EN_H,0);
		
		printk(KERN_ERR " hub 3v3 gpio = %d, ret:%i\n",
			GPIO_HUB2_3V3_EN_H, iRet);
		iRet = gpio_export(GPIO_HUB2_3V3_EN_H, 0);
		printk(KERN_ERR " hub2 3v3 gpio = %d, ret:%i\n",
			GPIO_HUB2_3V3_EN_H, iRet);
	} else {
		printk(KERN_ERR " fail to request hub 3v3 gpio = %d, ret:%i\n",
			GPIO_HUB2_3V3_EN_H, iRet);
		
	}
#endif

	//sata power
#ifdef CONFIG_KODARI_USB_HUB_SEPARATE
	iRet =gpio_request(GPIO_SATA_PWR_EN_H, "GPX0_6");	
#else
	iRet =gpio_request(GPIO_SATA_PWR_EN_H, "GPL2_6");	
#endif
	if(iRet == 0) {
		iRet = gpio_direction_output(GPIO_SATA_PWR_EN_H,0);
		
		printk(KERN_ERR " sata pwr gpio = %d, ret:%i\n",
			GPIO_SATA_PWR_EN_H, iRet);
		iRet = gpio_export(GPIO_SATA_PWR_EN_H, 0);
		printk(KERN_ERR " sata pwr gpio = %d, ret:%i\n",
			GPIO_SATA_PWR_EN_H, iRet);
	} else {
		printk(KERN_ERR " fail to request sata pwr gpio = %d, ret:%i\n",
			GPIO_SATA_PWR_EN_H, iRet);
		
	}

	//mmc wipe pin export

	iRet =gpio_request(GPIO_WIPE_EN_H, "GPX0");
	iRet = gpio_direction_input(GPIO_WIPE_EN_H);
	printk(KERN_ERR " wipe mmc pin gpio = %d, ret:%i\n",
			GPIO_WIPE_EN_H, iRet);
	s3c_gpio_setpull(GPIO_WIPE_EN_H, S3C_GPIO_PULL_UP);
	iRet = gpio_export(GPIO_WIPE_EN_H, 0);
	printk(KERN_ERR "wipe export gpio = %d, ret:%i\n",
			GPIO_WIPE_EN_H, iRet);


	//otg detect pin export

	iRet =gpio_request(GPIO_OTG_EN_H, "GPL2-3");
	iRet = gpio_direction_output(GPIO_OTG_EN_H, 0);
	printk(KERN_ERR "otg detect gpio = %d, ret:%i\n",
			GPIO_OTG_EN_H, iRet);
	iRet = gpio_export(GPIO_OTG_EN_H, 0);
	printk(KERN_ERR "otg gpio_export gpio = %d, ret:%i\n",
			GPIO_OTG_EN_H, iRet);

}

static void __init kodari_machine_init(void)
{
//	int iRet =0;

	//TUSB9261_reset(true);

//	WiFi_BT_reset(false);
//	WiFi_BT_reset(true);

#ifdef CONFIG_S3C64XX_DEV_SPI
	struct clk *sclk = NULL;
	struct clk *prnt = NULL;
	struct device *spi0_dev = &exynos_device_spi0.dev;
#ifndef CONFIG_FB_S5P_LMS501KF03
	struct device *spi1_dev = &exynos_device_spi1.dev;
#endif
	struct device *spi2_dev = &exynos_device_spi2.dev;
#endif
	samsung_board_rev = get_samsung_board_rev();
#if defined(CONFIG_EXYNOS_DEV_PD) && defined(CONFIG_PM_RUNTIME)
	exynos_pd_disable(&exynos4_device_pd[PD_MFC].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_G3D].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_LCD0].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_CAM].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_TV].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_GPS].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_GPS_ALIVE].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_ISP].dev);
#elif defined(CONFIG_EXYNOS_DEV_PD)
	/*
	 * These power domains should be always on
	 * without runtime pm support.
	 */
	exynos_pd_enable(&exynos4_device_pd[PD_MFC].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_G3D].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_LCD0].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_CAM].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_TV].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_GPS].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_GPS_ALIVE].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_ISP].dev);
#endif
	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

#if defined(CONFIG_VIDEO_OV5642)
	s3c_i2c1_set_platdata(&ov5642_i2c1_data);
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));
#endif
#if defined(CONFIG_KODARI_TYPE_CAR)
	s3c_i2c2_set_platdata(NULL);
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));
#endif

	s3c_i2c3_set_platdata(&i2c_3_data);
	i2c_register_board_info(3, i2c_devs3, ARRAY_SIZE(i2c_devs3));

#if defined(CONFIG_SENSORS_BMM050) || defined(CONFIG_SENSORS_BMA250)
	s3c_i2c4_set_platdata(&i2c_4_data); // iskim add
	i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
#endif
#if defined(CONFIG_TOUCHSCREEN_TSC2004)
	s3c_i2c5_set_platdata(NULL);
	i2c_register_board_info(5, i2c_devs5, ARRAY_SIZE(i2c_devs5));		
#endif


	s3c_i2c7_set_platdata(NULL);
#if defined(CONFIG_MFD_KODARI_MICOM_CORE)
	i2c_register_board_info(7, i2c_devs7, ARRAY_SIZE(i2c_devs7));
#endif

#if 1 //defined(CONFIG_KODARI_TYPE_MOBILE)
        kodari_5v_power_on();
#endif
        //kodari_bluetooth_power_on();
//leehc
//	kodari_usb_hub_reset(true);
//	kodari_usb_hub_reset(false);

//      kodari_usb_sata_power_on();

#if defined(CONFIG_FB_S5P_MIPI_DSIM)
	mipi_fb_init();
#endif
#ifdef CONFIG_FB_S3C
	dev_set_name(&s5p_device_fimd0.dev, "s3cfb.0");
	clk_add_alias("lcd", "exynos4-fb.0", "lcd", &s5p_device_fimd0.dev);
	clk_add_alias("sclk_fimd", "exynos4-fb.0", "sclk_fimd", &s5p_device_fimd0.dev);
	s5p_fb_setname(0, "exynos4-fb");
#if defined(CONFIG_LCD_AMS369FG06) || defined(CONFIG_LCD_LMS501KF03)
	spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));
#endif
	s5p_fimd0_set_platdata(&kodari_lcd0_pdata);
#ifdef CONFIG_FB_MIPI_DSIM
	s5p_device_mipi_dsim.dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#endif
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_fimd0.dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#endif
#endif
#ifdef CONFIG_FB_S5P
	s3cfb_set_platdata(NULL);
#endif
#ifdef CONFIG_FB_S5P_MIPI_DSIM
	s5p_device_dsim.dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#endif
#ifdef CONFIG_EXYNOS_DEV_PD
	s3c_device_fb.dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#endif
#ifdef CONFIG_USB_EHCI_S5P
	kodari_ehci_init();
#endif
#ifdef CONFIG_USB_OHCI_S5P
	kodari_ohci_init();
#endif
#ifdef CONFIG_USB_GADGET
	kodari_usbgadget_init();
#endif
#ifdef CONFIG_USB_EXYNOS_SWITCH
	kodari_usbswitch_init();
#endif
#if defined(CONFIG_KODARI_TYPE_CAR)
	samsung_bl_set(&kodari_bl_gpio_info, &kodari_bl_data);
#endif
#ifdef CONFIG_EXYNOS4_DEV_DWMCI
	exynos_dwmci_set_platdata(&exynos_dwmci_pdata);
#endif

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	exynos4_fimc_is_set_platdata(NULL);
#ifdef CONFIG_EXYNOS_DEV_PD
	exynos4_device_fimc_is.dev.parent = &exynos4_device_pd[PD_ISP].dev;
#endif
#endif
#ifdef CONFIG_S3C_DEV_HSMMC
	s3c_sdhci0_set_platdata(&kodari_hsmmc0_pdata);
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	s3c_sdhci1_set_platdata(&kodari_hsmmc1_pdata);
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	s3c_sdhci2_set_platdata(&kodari_hsmmc2_pdata);
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	s3c_sdhci3_set_platdata(&kodari_hsmmc3_pdata);
#endif
#ifdef CONFIG_S5P_DEV_MSHC
	s3c_mshci_set_platdata(&exynos4_mshc_pdata);
#endif
#if defined(CONFIG_VIDEO_EXYNOS_TV) && defined(CONFIG_VIDEO_EXYNOS_HDMI)
	dev_set_name(&s5p_device_hdmi.dev, "exynos4-hdmi");
	clk_add_alias("hdmi", "s5p-hdmi", "hdmi", &s5p_device_hdmi.dev);
	clk_add_alias("hdmiphy", "s5p-hdmi", "hdmiphy", &s5p_device_hdmi.dev);

	s5p_tv_setup();

	/* setup dependencies between TV devices */
	s5p_device_hdmi.dev.parent = &exynos4_device_pd[PD_TV].dev;
	s5p_device_mixer.dev.parent = &exynos4_device_pd[PD_TV].dev;

	s5p_i2c_hdmiphy_set_platdata(NULL);
#ifdef CONFIG_VIDEO_EXYNOS_HDMI_CEC
	s5p_hdmi_cec_set_platdata(&hdmi_cec_data);
#endif
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	kodari_set_camera_flite_platdata();
	s3c_set_platdata(&exynos_flite0_default_data,
			sizeof(exynos_flite0_default_data), &exynos_device_flite0);
	s3c_set_platdata(&exynos_flite1_default_data,
			sizeof(exynos_flite1_default_data), &exynos_device_flite1);
#ifdef CONFIG_EXYNOS_DEV_PD
	exynos_device_flite0.dev.parent = &exynos4_device_pd[PD_ISP].dev;
	exynos_device_flite1.dev.parent = &exynos4_device_pd[PD_ISP].dev;
#endif
#endif
#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_set_platdata(&exynos_tmu_data);
#endif
#ifdef CONFIG_VIDEO_FIMC
	s3c_fimc0_set_platdata(&fimc_plat);
	s3c_fimc1_set_platdata(&fimc_plat);
	s3c_fimc2_set_platdata(NULL);
	s3c_fimc3_set_platdata(NULL);
#ifdef CONFIG_EXYNOS_DEV_PD
	s3c_device_fimc0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc2.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc3.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#ifdef CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION
	secmem.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif
#ifdef CONFIG_VIDEO_FIMC_MIPI
	s3c_csis0_set_platdata(NULL);
	s3c_csis1_set_platdata(NULL);
#ifdef CONFIG_EXYNOS_DEV_PD
	s3c_device_csis0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_csis1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif

#endif /* CONFIG_VIDEO_FIMC */

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
	kodari_camera_config();
	kodari_subdev_config();

	dev_set_name(&s5p_device_fimc0.dev, "s3c-fimc.0");
	dev_set_name(&s5p_device_fimc1.dev, "s3c-fimc.1");
	dev_set_name(&s5p_device_fimc2.dev, "s3c-fimc.2");
	dev_set_name(&s5p_device_fimc3.dev, "s3c-fimc.3");

	clk_add_alias("fimc", "exynos4210-fimc.0", "fimc", &s5p_device_fimc0.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.0", "sclk_fimc",
			&s5p_device_fimc0.dev);
	clk_add_alias("fimc", "exynos4210-fimc.1", "fimc", &s5p_device_fimc1.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.1", "sclk_fimc",
			&s5p_device_fimc1.dev);
	clk_add_alias("fimc", "exynos4210-fimc.2", "fimc", &s5p_device_fimc2.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.2", "sclk_fimc",
			&s5p_device_fimc2.dev);
	clk_add_alias("fimc", "exynos4210-fimc.3", "fimc", &s5p_device_fimc3.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.3", "sclk_fimc",
			&s5p_device_fimc3.dev);

	s3c_fimc_setname(0, "exynos4210-fimc");
	s3c_fimc_setname(1, "exynos4210-fimc");
	s3c_fimc_setname(2, "exynos4210-fimc");
	s3c_fimc_setname(3, "exynos4210-fimc");
	/* FIMC */
	s3c_set_platdata(&s3c_fimc0_default_data,
			 sizeof(s3c_fimc0_default_data), &s5p_device_fimc0);
	s3c_set_platdata(&s3c_fimc1_default_data,
			 sizeof(s3c_fimc1_default_data), &s5p_device_fimc1);
	s3c_set_platdata(&s3c_fimc2_default_data,
			 sizeof(s3c_fimc2_default_data), &s5p_device_fimc2);
	s3c_set_platdata(&s3c_fimc3_default_data,
			 sizeof(s3c_fimc3_default_data), &s5p_device_fimc3);
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_fimc0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc2.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc3.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#ifdef CONFIG_VIDEO_S5P_MIPI_CSIS
	dev_set_name(&s5p_device_mipi_csis0.dev, "s3c-csis.0");
	dev_set_name(&s5p_device_mipi_csis1.dev, "s3c-csis.1");
	clk_add_alias("csis", "s5p-mipi-csis.0", "csis",
			&s5p_device_mipi_csis0.dev);
	clk_add_alias("sclk_csis", "s5p-mipi-csis.0", "sclk_csis",
			&s5p_device_mipi_csis0.dev);
	clk_add_alias("csis", "s5p-mipi-csis.1", "csis",
			&s5p_device_mipi_csis1.dev);
	clk_add_alias("sclk_csis", "s5p-mipi-csis.1", "sclk_csis",
			&s5p_device_mipi_csis1.dev);
	dev_set_name(&s5p_device_mipi_csis0.dev, "s5p-mipi-csis.0");
	dev_set_name(&s5p_device_mipi_csis1.dev, "s5p-mipi-csis.1");

	s3c_set_platdata(&s5p_mipi_csis0_default_data,
			sizeof(s5p_mipi_csis0_default_data), &s5p_device_mipi_csis0);
	s3c_set_platdata(&s5p_mipi_csis1_default_data,
			sizeof(s5p_mipi_csis1_default_data), &s5p_device_mipi_csis1);
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_mipi_csis0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_mipi_csis1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif
#endif
	
#if defined(CONFIG_KODARI_TYPE_CAR)
#if defined(CONFIG_VIDEO_TVOUT)
	s5p_hdmi_hpd_set_platdata(&hdmi_hpd_data);
	s5p_hdmi_cec_set_platdata(&hdmi_cec_data);
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_tvout.dev.parent = &exynos4_device_pd[PD_TV].dev;
	exynos4_device_pd[PD_TV].dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#endif
#endif
#endif

#ifdef CONFIG_VIDEO_JPEG_V2X
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_jpeg.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	if (samsung_rev() == EXYNOS4412_REV_2_0)
		exynos4_jpeg_setup_clock(&s5p_device_jpeg.dev, 176000000);
	else
		exynos4_jpeg_setup_clock(&s5p_device_jpeg.dev, 160000000);
#endif
#endif

#ifdef CONFIG_ION_EXYNOS
	exynos_ion_set_platdata();
#endif

#if defined(CONFIG_VIDEO_MFC5X) || defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_mfc.dev.parent = &exynos4_device_pd[PD_MFC].dev;
#endif
	if (soc_is_exynos4412() && samsung_rev() >= EXYNOS4412_REV_2_0)
		exynos4_mfc_setup_clock(&s5p_device_mfc.dev, 220 * MHZ);
	else if ((soc_is_exynos4412() && samsung_rev() >= EXYNOS4412_REV_1_0) ||
		(soc_is_exynos4212() && samsung_rev() >= EXYNOS4212_REV_1_0))
		exynos4_mfc_setup_clock(&s5p_device_mfc.dev, 200 * MHZ);
	else
		exynos4_mfc_setup_clock(&s5p_device_mfc.dev, 267 * MHZ);
#endif

#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
	dev_set_name(&s5p_device_mfc.dev, "s3c-mfc");
	clk_add_alias("mfc", "s5p-mfc", "mfc", &s5p_device_mfc.dev);
	s5p_mfc_setname(&s5p_device_mfc, "s5p-mfc");
#endif

#ifdef CONFIG_VIDEO_FIMG2D
	s5p_fimg2d_set_platdata(&fimg2d_data);
#endif
	samsung_keypad_set_platdata(&kodari_keypad_data);
//	kodari_sound_init();
	kodari_smsc911x_init();

#ifdef CONFIG_EXYNOS_C2C
	exynos_c2c_set_platdata(&kodari_c2c_pdata);
#endif

	exynos_sysmmu_init();

	kodari_switch_init();

	
	//kodari_headset_init();

	//kodari_uart0_detect_gpio_init();

	//leehc
	//kodari_ssd_io_detect_gpio_init();


	platform_add_devices(kodari_devices, ARRAY_SIZE(kodari_devices));
	if (soc_is_exynos4412())
		platform_add_devices(smdk4412_devices, ARRAY_SIZE(smdk4412_devices));


	platform_device_register(&kodari_keypad);
	kodari_gpio_power_init();

#ifdef CONFIG_S3C64XX_DEV_SPI
	sclk = clk_get(spi0_dev, "dout_spi0");
	if (IS_ERR(sclk))
		dev_err(spi0_dev, "failed to get sclk for SPI-0\n");
	prnt = clk_get(spi0_dev, "mout_mpll_user");
	if (IS_ERR(prnt))
		dev_err(spi0_dev, "failed to get prnt\n");
	if (clk_set_parent(sclk, prnt))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				prnt->name, sclk->name);

	clk_set_rate(sclk, 100 * 1000 * 1000);
	clk_put(sclk);
	clk_put(prnt);

	if (!gpio_request(EXYNOS4_GPB(1), "SPI_CS0")) {
		gpio_direction_output(EXYNOS4_GPB(1), 1);
		s3c_gpio_cfgpin(EXYNOS4_GPB(1), S3C_GPIO_SFN(1));
		s3c_gpio_setpull(EXYNOS4_GPB(1), S3C_GPIO_PULL_UP);
		exynos_spi_set_info(0, EXYNOS_SPI_SRCCLK_SCLK,
			ARRAY_SIZE(spi0_csi));
	}

	spi_register_board_info(spi0_board_info, ARRAY_SIZE(spi0_board_info));

#ifndef CONFIG_FB_S5P_LMS501KF03
	sclk = clk_get(spi1_dev, "dout_spi1");
	if (IS_ERR(sclk))
		dev_err(spi1_dev, "failed to get sclk for SPI-1\n");
	prnt = clk_get(spi1_dev, "mout_mpll_user");
	if (IS_ERR(prnt))
		dev_err(spi1_dev, "failed to get prnt\n");
	if (clk_set_parent(sclk, prnt))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				prnt->name, sclk->name);

	clk_set_rate(sclk, 100 * 1000 * 1000);
	clk_put(sclk);
	clk_put(prnt);

	if (!gpio_request(EXYNOS4_GPB(5), "SPI_CS1")) {
		gpio_direction_output(EXYNOS4_GPB(5), 1);
		s3c_gpio_cfgpin(EXYNOS4_GPB(5), S3C_GPIO_SFN(1));
		s3c_gpio_setpull(EXYNOS4_GPB(5), S3C_GPIO_PULL_UP);
		exynos_spi_set_info(1, EXYNOS_SPI_SRCCLK_SCLK,
			ARRAY_SIZE(spi1_csi));
	}

	spi_register_board_info(spi1_board_info, ARRAY_SIZE(spi1_board_info));
#endif

	sclk = clk_get(spi2_dev, "dout_spi2");
	if (IS_ERR(sclk))
		dev_err(spi2_dev, "failed to get sclk for SPI-2\n");
	prnt = clk_get(spi2_dev, "mout_mpll_user");
	if (IS_ERR(prnt))
		dev_err(spi2_dev, "failed to get prnt\n");
	if (clk_set_parent(sclk, prnt))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				prnt->name, sclk->name);

	clk_set_rate(sclk, 100 * 1000 * 1000);
	clk_put(sclk);
	clk_put(prnt);

	if (!gpio_request(EXYNOS4_GPC1(2), "SPI_CS2")) {
		gpio_direction_output(EXYNOS4_GPC1(2), 1);
		s3c_gpio_cfgpin(EXYNOS4_GPC1(2), S3C_GPIO_SFN(1));
		s3c_gpio_setpull(EXYNOS4_GPC1(2), S3C_GPIO_PULL_UP);
		exynos_spi_set_info(2, EXYNOS_SPI_SRCCLK_SCLK,
			ARRAY_SIZE(spi2_csi));
	}

	spi_register_board_info(spi2_board_info, ARRAY_SIZE(spi2_board_info));
#endif
#ifdef CONFIG_BUSFREQ_OPP
	dev_add(&busfreq, &exynos4_busfreq.dev);
	ppmu_init(&exynos_ppmu[PPMU_DMC0], &exynos4_busfreq.dev);
	ppmu_init(&exynos_ppmu[PPMU_DMC1], &exynos4_busfreq.dev);
	ppmu_init(&exynos_ppmu[PPMU_CPU], &exynos4_busfreq.dev);
#endif
	register_reboot_notifier(&exynos4_reboot_notifier);

//hgs pwm register
	platform_device_register(&s3c_device_timer[0]);

//hgs LED Driver register 
	
	platform_device_register(&kodari_leds_gpio);
//nskang patch_driver
    sysfs_std_patch_init();


	kodari_export_gpios();
#if 0
	//leehc this is just for test
	//wake_lock this is prevent enter sleep mode

	snprintf(kodari_lpm.wake_lock_name, sizeof(kodari_lpm.wake_lock_name),
			"KODARILowPower");
	wake_lock_init(&kodari_lpm.wake_lock, WAKE_LOCK_SUSPEND,
			 kodari_lpm.wake_lock_name);

	wake_lock(&kodari_lpm.wake_lock);


	
	snprintf(kodari_eth_wake.wake_lock_name, sizeof(kodari_eth_wake.wake_lock_name),
			"KODARIethLowPower");
	wake_lock_init(&kodari_eth_wake.wake_lock, WAKE_LOCK_IDLE,
			 kodari_eth_wake.wake_lock_name);
	kodari_eth_wake.wake = 0;
#endif



}

#ifdef CONFIG_EXYNOS_C2C
static void __init exynos_c2c_reserve(void)
{
	static struct cma_region region[] = {
		{
			.name = "c2c_shdmem",
			.size = 64 * SZ_1M,
			{ .alignment    = 64 * SZ_1M },
			.start = C2C_SHAREDMEM_BASE
		}, {
		.size = 0,
		}
	};

	s5p_cma_region_reserve(region, NULL, 0, NULL);
}
#endif

MACHINE_START(SMDK4212, "SMDK4X12")
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.init_irq	= exynos4_init_irq,
	.map_io		= kodari_map_io,
	.init_machine	= kodari_machine_init,
	.timer		= &exynos4_timer,
#ifdef CONFIG_EXYNOS_C2C
	.reserve	= &exynos_c2c_reserve,
#endif
MACHINE_END

	//MACHINE_START(SMDK4412, "SMDK4X12")
	MACHINE_START(SMDK4412, "KODARI")
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.init_irq	= exynos4_init_irq,
	.map_io		= kodari_map_io,
	.init_machine	= kodari_machine_init,
	.timer		= &exynos4_timer,
#ifdef CONFIG_EXYNOS_C2C
	.reserve	= &exynos_c2c_reserve,
#endif
MACHINE_END

MACHINE_START(ORIGEN, "ORIGEN")
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.init_irq	= exynos4_init_irq,
	.map_io		= kodari_map_io,
	.init_machine	= kodari_machine_init,
	.timer		= &exynos4_timer,
#ifdef CONFIG_EXYNOS_C2C
	.reserve	= &exynos_c2c_reserve,
#endif
MACHINE_END
