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

#ifndef __KODARI_MICOM_H__
#define __KODARI_MICOM_H__

#define GPD0CON		*(volatile unsigned long *)(0x114000A0)
#define GPD0DAT		*(volatile unsigned long *)(0x114000A4)
#define GPD0PUD		*(volatile unsigned long *)(0x114000A8)


#define IIC7_ESCL_Hi	GPD0DAT |= (0x1<<3)
#define IIC7_ESCL_Lo	GPD0DAT &= ~(0x1<<3)
#define IIC7_ESDA_Hi	GPD0DAT |= (0x1<<2)
#define IIC7_ESDA_Lo	GPD0DAT &= ~(0x1<<2)


#define IIC7_ESCL_INP		GPD0CON &= ~(0xf<<12)
#define IIC7_ESCL_OUTP	GPD0CON = (GPD0CON & ~(0xf<<12))|(0x1<<12)

#define IIC7_ESDA_INP		GPD0CON &= ~(0xf<<8)
#define IIC7_ESDA_OUTP	GPD0CON = (GPD0CON & ~(0xf<<8))|(0x1<<8)



#define MICOM_DELAY		100

#define KODARI_MICOM_ADDR 0x48

enum kodari_micom_regs {
	KODARI_AC_ONLINE_0,
	KODARI_BATTERY_ONLINE_0,
	KODARI_VCELL_MSB_0,
	KODARI_VCELL_LSB_0,
	KODARI_SOC_MSB_0,
	KODARI_SOC_LSB_0,
	KODARI_MICOM_MAJOR_VER,
	KODARI_MICOM_MINOR_VER,
	KODARI_BOARD_VER,
	KODARI_LED_CONTROL,
	KODARI_AC_ONLINE_1 =		0x10,
	KODARI_BATTERY_ONLINE_1,
	KODARI_VCELL_MSB_1,
	KODARI_VCELL_LSB_1,
	KODARI_SOC_MSB_1,
	KODARI_SOC_LSB_1,


	KODARI_BMODULE_STATES = 0x20,	

};


void IIC7_ESetport(void);
void IIC7_ERead (unsigned char ChipId, unsigned char IicAddr, unsigned char *IicData);
void IIC7_ETest ();

void IIC7_EHigh(void);
void IIC7_ELow(void);




#endif /*__KODARI_MICOM_H__*/

