/* linux/drivers/media/video/ov5642.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *	         http://www.samsung.com/
 *
 * Driver for OV5642 (UXGA camera) from Samsung Electronics
 * 1/4" 2.0Mp CMOS Image Sensor SoC with an Embedded Image Processor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __OV5642_H__
#define __OV5642_H__

struct ov5642_reg {
	unsigned char addr;
	unsigned char val;
};

struct ov5642_regset_type {
	unsigned char *regset;
	int len;
};

/*
 * Macro
 */
#define REGSET_LENGTH(x)	(sizeof(x)/sizeof(ov5642_reg))

/*
 * User defined commands
 */
/* S/W defined features for tune */
#define REG_DELAY	0xFF00	/* in ms */
#define REG_CMD		0xFFFF	/* Followed by command */

/* Following order should not be changed */
/*
 *	WVGA	//  800 x  480
 *	WSVGA	//  854 x  480
 *	HD1080	// 1920 x 1080
 *	QXGA	// 2048 x 1536
 */
enum image_size_ov5642 {
	/* This SoC supports upto UXGA (1600*1200) */
	QQVGA,	/* 160*120 */
	QCIF,	/* 176*144 */
	QVGA,	/* 320*240 */
	CIF,	/* 352*288 */
	VGA,	/* 640*480 */
	SVGA,	/* 800*600 */
	HD720P,	/* 1280*720 */
	SXGA,	/* 1280*1024 */
	UXGA,	/* 1600*1200 */
};

/*
 * Following values describe controls of camera
 * in user aspect and must be match with index of ov5642_regset[]
 * These values indicates each controls and should be used
 * to control each control
 */
enum ov5642_control {
	OV5642_INIT,
	OV5642_EV,
	OV5642_AWB,
	OV5642_MWB,
	OV5642_EFFECT,
	OV5642_CONTRAST,
	OV5642_SATURATION,
	OV5642_SHARPNESS,
};

#define OV5642_REGSET(x)	{	\
	.regset = x,			\
	.len = sizeof(x)/sizeof(ov5642_reg),}

enum ov5642_mode {
	CAM_MODE_PREVIEW,
	CAM_MODE_SNAPSHOT
};

/*
 * EV bias
 */

static const struct ov5642_reg ov5642_ev_m6[] = {
};

static const struct ov5642_reg ov5642_ev_m5[] = {
};

static const struct ov5642_reg ov5642_ev_m4[] = {
};

static const struct ov5642_reg ov5642_ev_m3[] = {
};

static const struct ov5642_reg ov5642_ev_m2[] = {
};

static const struct ov5642_reg ov5642_ev_m1[] = {
};

static const struct ov5642_reg ov5642_ev_default[] = {
};

static const struct ov5642_reg ov5642_ev_p1[] = {
};

static const struct ov5642_reg ov5642_ev_p2[] = {
};

static const struct ov5642_reg ov5642_ev_p3[] = {
};

static const struct ov5642_reg ov5642_ev_p4[] = {
};

static const struct ov5642_reg ov5642_ev_p5[] = {
};

static const struct ov5642_reg ov5642_ev_p6[] = {
};

/* Order of this array should be following the querymenu data */
static const unsigned char *ov5642_regs_ev_bias[] = {
	(unsigned char *)ov5642_ev_m6, (unsigned char *)ov5642_ev_m5,
	(unsigned char *)ov5642_ev_m4, (unsigned char *)ov5642_ev_m3,
	(unsigned char *)ov5642_ev_m2, (unsigned char *)ov5642_ev_m1,
	(unsigned char *)ov5642_ev_default, (unsigned char *)ov5642_ev_p1,
	(unsigned char *)ov5642_ev_p2, (unsigned char *)ov5642_ev_p3,
	(unsigned char *)ov5642_ev_p4, (unsigned char *)ov5642_ev_p5,
	(unsigned char *)ov5642_ev_p6,
};

/*
 * Auto White Balance configure
 */
static const struct ov5642_reg ov5642_awb_off[] = {
};

static const struct ov5642_reg ov5642_awb_on[] = {
};

static const unsigned char *ov5642_regs_awb_enable[] = {
	(unsigned char *)ov5642_awb_off,
	(unsigned char *)ov5642_awb_on,
};

/*
 * Manual White Balance (presets)
 */
static const struct ov5642_reg ov5642_wb_tungsten[] = {

};

static const struct ov5642_reg ov5642_wb_fluorescent[] = {

};

static const struct ov5642_reg ov5642_wb_sunny[] = {

};

static const struct ov5642_reg ov5642_wb_cloudy[] = {

};

/* Order of this array should be following the querymenu data */
static const unsigned char *ov5642_regs_wb_preset[] = {
	(unsigned char *)ov5642_wb_sunny,
	(unsigned char *)ov5642_wb_cloudy,
	(unsigned char *)ov5642_wb_tungsten,
	(unsigned char *)ov5642_wb_fluorescent,
};

/*
 * Color Effect (COLORFX)
 */
static const struct ov5642_reg ov5642_color_normal[] = {
};

static const struct ov5642_reg ov5642_color_monochrome[] = {
};

static const struct ov5642_reg ov5642_color_sepia[] = {
};

static const struct ov5642_reg ov5642_color_aqua[] = {
};

static const struct ov5642_reg ov5642_color_negative[] = {
};

static const struct ov5642_reg ov5642_color_sketch[] = {
};

/* Order of this array should be following the querymenu data */
static const unsigned char *ov5642_regs_color_effect[] = {
	(unsigned char *)ov5642_color_normal,
	(unsigned char *)ov5642_color_monochrome,
	(unsigned char *)ov5642_color_sepia,
	(unsigned char *)ov5642_color_aqua,
	(unsigned char *)ov5642_color_sketch,
	(unsigned char *)ov5642_color_negative,
};

/*
 * OV5642 Brightness bias
 * 	+4   +3   +2   +1    0   -1   -2   -3   -4
 * 5001 ff   ff   ff   ff   ff   ff   ff   ff   ff
 * 5589	40   30   20   10   00   10   20   30   40
 * 5580 04   04   04   04   04   04   04   04   04
 * 558a 00   00   00   00   00   08   08   08   08
 */
/*
 * OV5642 Contrast bias
 * 	+4   +3   +2   +1    0   -1   -2   -3   -4
 * 5001 ff   ff   ff   ff   ff   ff   ff   ff   ff
 * 5580 04   04   04   04   04   04   04   04   04
 * 5587	30   2c   28   24   20   1c   18   14   10
 * 5588 30   2c   28   24   20   1c   18   14   10
 * 558a 00   00   00   00   00   00   00   00   00
 */
static const struct ov5642_reg ov5642_contrast_m2[] = {
};

static const struct ov5642_reg ov5642_contrast_m1[] = {
};

static const struct ov5642_reg ov5642_contrast_default[] = {
};

static const struct ov5642_reg ov5642_contrast_p1[] = {
};

static const struct ov5642_reg ov5642_contrast_p2[] = {
};

static const unsigned char *ov5642_regs_contrast_bias[] = {
	(unsigned char *)ov5642_contrast_m2,
	(unsigned char *)ov5642_contrast_m1,
	(unsigned char *)ov5642_contrast_default,
	(unsigned char *)ov5642_contrast_p1,
	(unsigned char *)ov5642_contrast_p2,
};

/*
 * OV5642 Saturation bias
 * 	+4   +3   +2   +1    0   -1   -2   -3   -4
 * 5001 ff   ff   ff   ff   ff   ff   ff   ff   ff
 * 5583	80   70   60   50   40   30   20   10   00
 * 5584 80   70   60   50   40   30   20   10   00
 * 5580 02   02   02   02   02   02   02   02   02
 */
static const struct ov5642_reg ov5642_saturation_m2[] = {
};

static const struct ov5642_reg ov5642_saturation_m1[] = {
};

static const struct ov5642_reg ov5642_saturation_default[] = {
};

static const struct ov5642_reg ov5642_saturation_p1[] = {
};

static const struct ov5642_reg ov5642_saturation_p2[] = {
};

static const unsigned char *ov5642_regs_saturation_bias[] = {
	(unsigned char *)ov5642_saturation_m2,
	(unsigned char *)ov5642_saturation_m1,
	(unsigned char *)ov5642_saturation_default,
	(unsigned char *)ov5642_saturation_p1,
	(unsigned char *)ov5642_saturation_p2,
};

/*
 * Sharpness bias
 */
static const struct ov5642_reg ov5642_sharpness_m2[] = {
};

static const struct ov5642_reg ov5642_sharpness_m1[] = {
};

static const struct ov5642_reg ov5642_sharpness_default[] = {
};

static const struct ov5642_reg ov5642_sharpness_p1[] = {
};

static const struct ov5642_reg ov5642_sharpness_p2[] = {
};

static const unsigned char *ov5642_regs_sharpness_bias[] = {
	(unsigned char *)ov5642_sharpness_m2,
	(unsigned char *)ov5642_sharpness_m1,
	(unsigned char *)ov5642_sharpness_default,
	(unsigned char *)ov5642_sharpness_p1,
	(unsigned char *)ov5642_sharpness_p2,
};

#endif
