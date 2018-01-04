/*
 * wm1800.h  --  WM1800 Soc Audio driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __WM1800_H__
#define __WM1800_H__

/* WM1800 register space */

#define WM1800_CACHEREGNUM 	56

#define WM1800_LINVOL		0x0
#define WM1800_RINVOL		0x1
#define WM1800_LOUT1		0x2
#define WM1800_ROUT1		0x3
#define WM1800_CLOCK1		0x4
#define WM1800_DACCTL1		0x5
#define WM1800_DACCTL2		0x6
#define WM1800_IFACE1		0x7
#define WM1800_CLOCK2		0x8
#define WM1800_IFACE2		0x9
#define WM1800_LDAC			0xa
#define WM1800_RDAC			0xb

#define WM1800_RESET		0xf
#define WM1800_3D			0x10
#define WM1800_ALC1			0x11
#define WM1800_ALC2			0x12
#define WM1800_ALC3			0x13
#define WM1800_NOISEG		0x14
#define WM1800_LADC			0x15
#define WM1800_RADC			0x16
#define WM1800_ADDCTL1		0x17
#define WM1800_ADDCTL2		0x18
#define WM1800_POWER1		0x19
#define WM1800_POWER2		0x1a
#define WM1800_ADDCTL3		0x1b
#define WM1800_APOP1		0x1c
#define WM1800_APOP2		0x1d

#define WM1800_LINPATH		0x20
#define WM1800_RINPATH		0x21
#define WM1800_LOUTMIX		0x22

#define WM1800_ROUTMIX		0x25
#define WM1800_MONOMIX1		0x26
#define WM1800_MONOMIX2		0x27
#define WM1800_LOUT2		0x28
#define WM1800_ROUT2		0x29
#define WM1800_MONO			0x2a
#define WM1800_INBMIX1		0x2b
#define WM1800_INBMIX2		0x2c
#define WM1800_BYPASS1		0x2d
#define WM1800_BYPASS2		0x2e
#define WM1800_POWER3		0x2f
#define WM1800_ADDCTL4		0x30
#define WM1800_CLASSD1		0x31

#define WM1800_CLASSD3		0x33
#define WM1800_PLL1			0x34
#define WM1800_PLL2			0x35
#define WM1800_PLL3			0x36
#define WM1800_PLL4			0x37

/*
 * WM1800 Clock dividers
 */
#define WM1800_SYSCLKDIV 		0
#define WM1800_DACDIV			1
#define WM1800_OPCLKDIV			2
#define WM1800_DCLKDIV			3
#define WM1800_TOCLKSEL			4
#define WM1800_SYSCLKSEL       	5

#define WM1800_SYSCLK_DIV_1		(0 << 1)
#define WM1800_SYSCLK_DIV_2		(2 << 1)

#define WM1800_SYSCLK_MCLK		(0 << 0)
#define WM1800_SYSCLK_PLL		(1 << 0)

#define WM1800_DAC_DIV_1		(0 << 3)
#define WM1800_DAC_DIV_1_5		(1 << 3)
#define WM1800_DAC_DIV_2		(2 << 3)
#define WM1800_DAC_DIV_3		(3 << 3)
#define WM1800_DAC_DIV_4		(4 << 3)
#define WM1800_DAC_DIV_5_5		(5 << 3)
#define WM1800_DAC_DIV_6		(6 << 3)

#define WM1800_DCLK_DIV_1_5		(0 << 6)
#define WM1800_DCLK_DIV_2		(1 << 6)
#define WM1800_DCLK_DIV_3		(2 << 6)
#define WM1800_DCLK_DIV_4		(3 << 6)
#define WM1800_DCLK_DIV_6		(4 << 6)
#define WM1800_DCLK_DIV_8		(5 << 6)
#define WM1800_DCLK_DIV_12		(6 << 6)
#define WM1800_DCLK_DIV_16		(7 << 6)

#define WM1800_TOCLK_F19		(0 << 1)
#define WM1800_TOCLK_F21		(1 << 1)

#define WM1800_OPCLK_DIV_1		(0 << 0)
#define WM1800_OPCLK_DIV_2		(1 << 0)
#define WM1800_OPCLK_DIV_3		(2 << 0)
#define WM1800_OPCLK_DIV_4		(3 << 0)
#define WM1800_OPCLK_DIV_5_5	(4 << 0)
#define WM1800_OPCLK_DIV_6		(5 << 0)

#define WM1800_DAI_PAIFRX	0
#define WM1800_DAI_PAIFTX	1

/*+GARAMJIN*/
#define WM1800_OUT_NONE		0
#define WM1800_OUT_SPK		1
#define WM1800_OUT_EAR		2
#define WM1800_IN_MIC3_L	3
#define WM1800_OUT_SPK_MUTE	4


/*-GARAMJIN*/

#endif	/*__WM1800_H__*/
