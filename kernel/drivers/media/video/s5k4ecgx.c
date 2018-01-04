/* drivers/media/video/s5k4ecgx.c
 *
 * Driver for s5k4ecgx (5MP Camera) from SEC
 *
 * Copyright (C) 2010, SAMSUNG ELECTRONICS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/s5k4ecgx.h>
#include <linux/videodev2_samsung.h>

#include "s5k4ecgx_regset.h"

/* Should include twice to generate regset and data */
#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_0
#include "s5k4ecgx_regs_1_0.h"
#include "s5k4ecgx_regs_1_0.h"
#endif /* CONFIG_VIDEO_S5K4ECGX_V_1_0 */

#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_1
#ifndef CONFIG_VIDEO_S5K4ECGX_SLSI_4EC
#include "s5k4ecgx_regs_1_1.h"
#include "s5k4ecgx_regs_1_1.h"
#else
#include "s5k4ecgx_regs_1_1_slsi-4ec.h"
#include "s5k4ecgx_regs_1_1_slsi-4ec.h"
#endif
#endif /* CONFIG_VIDEO_S5K4ECGX_V_1_1 */

#define FORMAT_FLAGS_COMPRESSED		0x3
#define SENSOR_JPEG_SNAPSHOT_MEMSIZE	0x410580

#define DEFAULT_PIX_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */
#define DEFAULT_MCLK		24000000
#define POLL_TIME_MS		10
#define CAPTURE_POLL_TIME_MS    1000

/* maximum time for one frame at minimum fps (15fps) in normal mode */
#define NORMAL_MODE_MAX_ONE_FRAME_DELAY_MS     67
/* maximum time for one frame at minimum fps (4fps) in night mode */
#define NIGHT_MODE_MAX_ONE_FRAME_DELAY_MS     250

/* time to move lens to target position before last af mode register write */
#define LENS_MOVE_TIME_MS       100

/* level at or below which we need to enable flash when in auto mode */
#define LOW_LIGHT_LEVEL		0x1D

/* level at or below which we need to use low light capture mode */
#define HIGH_LIGHT_LEVEL	0x80

#define FIRST_AF_SEARCH_COUNT   80
#define SECOND_AF_SEARCH_COUNT  80
#define AE_STABLE_SEARCH_COUNT  4

#ifdef CONFIG_VIDEO_S5K4ECGX_DEBUG
enum {
	S5K4ECGX_DEBUG_I2C		= 1U << 0,
	S5K4ECGX_DEBUG_I2C_BURSTS	= 1U << 1,
};
static uint32_t s5k4ecgx_debug_mask = S5K4ECGX_DEBUG_I2C_BURSTS;
module_param_named(debug_mask, s5k4ecgx_debug_mask, uint, S_IWUSR | S_IRUGO);

#define s5k4ecgx_debug(mask, x...) \
	do { \
		if (s5k4ecgx_debug_mask & mask) \
			pr_info(x);	\
	} while (0)
#else

#define s5k4ecgx_debug(mask, x...)

#endif

#define S5K4ECGX_VERSION_1_0	0x00
#define S5K4ECGX_VERSION_1_1	0x11

/* result values returned to HAL */
enum {
	AUTO_FOCUS_FAILED,
	AUTO_FOCUS_DONE,
	AUTO_FOCUS_CANCELLED,
};

enum af_operation_status {
	AF_NONE = 0,
	AF_START,
	AF_CANCEL,
	AF_INITIAL,
};

enum s5k4ecgx_oprmode {
	S5K4ECGX_OPRMODE_VIDEO = 0,
	S5K4ECGX_OPRMODE_IMAGE = 1,
};

enum s5k4ecgx_preview_frame_size {
	S5K4ECGX_PREVIEW_QCIF = 0,	/* 176x144 */
	S5K4ECGX_PREVIEW_CIF,		/* 352x288 */
	S5K4ECGX_PREVIEW_VGA,		/* 640x480 */
	S5K4ECGX_PREVIEW_D1,		/* 720x480 */
	S5K4ECGX_PREVIEW_WVGA,		/* 800x480 */
	S5K4ECGX_PREVIEW_SVGA,		/* 800x600 */
	S5K4ECGX_PREVIEW_WSVGA,		/* 1024x600*/
	S5K4ECGX_PREVIEW_MAX,
};

enum s5k4ecgx_capture_frame_size {
	S5K4ECGX_CAPTURE_VGA = 0,	/* 640x480 */
	S5K4ECGX_CAPTURE_WVGA,		/* 800x480 */
	S5K4ECGX_CAPTURE_SVGA,		/* 800x600 */
	S5K4ECGX_CAPTURE_WSVGA,		/* 1024x600 */
	S5K4ECGX_CAPTURE_1MP,		/* 1280x960 */
	S5K4ECGX_CAPTURE_W1MP,		/* 1600x960 */
	S5K4ECGX_CAPTURE_2MP,		/* UXGA  - 1600x1200 */
	S5K4ECGX_CAPTURE_W2MP,		/* 35mm Academy Offset Standard 1.66 */
					/* 2048x1232, 2.4MP */
	S5K4ECGX_CAPTURE_3MP,		/* QXGA  - 2048x1536 */
	S5K4ECGX_CAPTURE_W4MP,		/* WQXGA - 2560x1536 */
	S5K4ECGX_CAPTURE_5MP,		/* 2560x1920 */
	S5K4ECGX_CAPTURE_MAX,
};

struct s5k4ecgx_framesize {
	u32 index;
	u32 width;
	u32 height;
};

static const struct s5k4ecgx_framesize s5k4ecgx_preview_framesize_list[] = {
	{ S5K4ECGX_PREVIEW_QCIF,	176,  144 },
	{ S5K4ECGX_PREVIEW_CIF,		352,  288 },
	{ S5K4ECGX_PREVIEW_VGA,		640,  480 },
	{ S5K4ECGX_PREVIEW_D1,		720,  480 },
};

static const struct s5k4ecgx_framesize s5k4ecgx_capture_framesize_list[] = {
	{ S5K4ECGX_CAPTURE_VGA,		640,  480 },
	{ S5K4ECGX_CAPTURE_1MP,		1280, 960 },
	{ S5K4ECGX_CAPTURE_2MP,		1600, 1200 },
	{ S5K4ECGX_CAPTURE_3MP,		2048, 1536 },
	{ S5K4ECGX_CAPTURE_5MP,		2560, 1920 },
};

struct s5k4ecgx_version {
	u32 major;
	u32 minor;
};

struct s5k4ecgx_date_info {
	u32 year;
	u32 month;
	u32 date;
};

enum s5k4ecgx_runmode {
	S5K4ECGX_RUNMODE_NOTREADY,
	S5K4ECGX_RUNMODE_IDLE,
	S5K4ECGX_RUNMODE_RUNNING,
	S5K4ECGX_RUNMODE_CAPTURE,
};

struct s5k4ecgx_firmware {
	u32 addr;
	u32 size;
};

struct s5k4ecgx_jpeg_param {
	u32 enable;
	u32 quality;
	u32 main_size;		/* Main JPEG file size */
	u32 thumb_size;		/* Thumbnail file size */
	u32 main_offset;
	u32 thumb_offset;
	u32 postview_offset;
};

struct s5k4ecgx_position {
	int x;
	int y;
};

struct gps_info_common {
	u32 direction;
	u32 dgree;
	u32 minute;
	u32 second;
};

struct s5k4ecgx_gps_info {
	unsigned char gps_buf[8];
	unsigned char altitude_buf[4];
	int gps_timeStamp;
};

struct s5k4ecgx_regset_table {
#ifdef CONFIG_VIDEO_S5K4ECGX_DEBUG
	const char *name;
#endif
	const struct s5k4ecgx_reg *regset;
	const u8 *data;
};

#ifdef CONFIG_VIDEO_S5K4ECGX_DEBUG
#define S5K4ECGX_REGSET_TABLE(REGSET)	\
	{						\
		.name		= #REGSET,		\
		.regset		= REGSET,		\
		.data		= REGSET##_data,	\
	}
#else
#define S5K4ECGX_REGSET_TABLE(REGSET)	\
	{						\
		.regset		= REGSET,		\
		.data		= REGSET##_data,	\
	}
#endif

#define S5K4ECGX_REGSET(IDX, REGSET)	\
	[(IDX)] = S5K4ECGX_REGSET_TABLE(REGSET)

#define EV_ENUM(i) (EV_##i - EV_MINUS_4)

struct s5k4ecgx_regs {
	struct s5k4ecgx_regset_table ev[EV_ENUM(MAX)];
	struct s5k4ecgx_regset_table metering[METERING_MAX];
	struct s5k4ecgx_regset_table iso[ISO_MAX];
	struct s5k4ecgx_regset_table effect[IMAGE_EFFECT_MAX];
	struct s5k4ecgx_regset_table white_balance[WHITE_BALANCE_MAX];
	struct s5k4ecgx_regset_table preview_size[S5K4ECGX_PREVIEW_MAX];
	struct s5k4ecgx_regset_table capture_size[S5K4ECGX_CAPTURE_MAX];
	struct s5k4ecgx_regset_table scene_mode[SCENE_MODE_MAX];
	struct s5k4ecgx_regset_table saturation[SATURATION_MAX];
	struct s5k4ecgx_regset_table contrast[CONTRAST_MAX];
	struct s5k4ecgx_regset_table sharpness[SHARPNESS_MAX];
	struct s5k4ecgx_regset_table fps[FRAME_RATE_MAX];
	struct s5k4ecgx_regset_table preview_return;
	struct s5k4ecgx_regset_table jpeg_quality_high;
	struct s5k4ecgx_regset_table jpeg_quality_normal;
	struct s5k4ecgx_regset_table jpeg_quality_low;
	struct s5k4ecgx_regset_table flash_start;
	struct s5k4ecgx_regset_table flash_end;
	struct s5k4ecgx_regset_table af_assist_flash_start;
	struct s5k4ecgx_regset_table af_assist_flash_end;
	struct s5k4ecgx_regset_table af_low_light_mode_on;
	struct s5k4ecgx_regset_table af_low_light_mode_off;
	struct s5k4ecgx_regset_table ae_awb_lock_on;
	struct s5k4ecgx_regset_table ae_awb_lock_off;
	struct s5k4ecgx_regset_table low_cap_on;
	struct s5k4ecgx_regset_table low_cap_off;
	struct s5k4ecgx_regset_table wdr_on;
	struct s5k4ecgx_regset_table wdr_off;
	struct s5k4ecgx_regset_table face_detection_on;
	struct s5k4ecgx_regset_table face_detection_off;
	struct s5k4ecgx_regset_table capture_start;
	struct s5k4ecgx_regset_table af_macro_mode;
	struct s5k4ecgx_regset_table af_normal_mode;
	struct s5k4ecgx_regset_table af_return_macro_position;
	struct s5k4ecgx_regset_table single_af_start;
	struct s5k4ecgx_regset_table single_af_off_1;
	struct s5k4ecgx_regset_table single_af_off_2;
	struct s5k4ecgx_regset_table dtp_start;
	struct s5k4ecgx_regset_table dtp_stop;
	struct s5k4ecgx_regset_table init_reg;
	struct s5k4ecgx_regset_table flash_init;
	struct s5k4ecgx_regset_table reset_crop;
	struct s5k4ecgx_regset_table get_ae_stable_status;
	struct s5k4ecgx_regset_table get_light_level;
	struct s5k4ecgx_regset_table get_1st_af_search_status;
	struct s5k4ecgx_regset_table get_2nd_af_search_status;
	struct s5k4ecgx_regset_table get_capture_status;
	struct s5k4ecgx_regset_table get_esd_status;
	struct s5k4ecgx_regset_table get_iso;
	struct s5k4ecgx_regset_table get_shutterspeed;
};

struct sec_cam_parm {
	struct v4l2_captureparm capture;
	int contrast;
	int effects;
	int brightness;
	int flash_mode;
	int focus_mode;
	int iso;
	int metering;
	int saturation;
	int scene_mode;
	int sharpness;
	int white_balance;
	int fps;
};

#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_0
static const struct s5k4ecgx_regs regs_for_fw_version_1_0 = {
	.ev = {
		S5K4ECGX_REGSET(EV_ENUM(MINUS_4), s5k4ecgx_EV_Minus_4_v1),
		S5K4ECGX_REGSET(EV_ENUM(MINUS_3), s5k4ecgx_EV_Minus_3_v1),
		S5K4ECGX_REGSET(EV_ENUM(MINUS_2), s5k4ecgx_EV_Minus_2_v1),
		S5K4ECGX_REGSET(EV_ENUM(MINUS_1), s5k4ecgx_EV_Minus_1_v1),
		S5K4ECGX_REGSET(EV_ENUM(DEFAULT), s5k4ecgx_EV_Default_v1),
		S5K4ECGX_REGSET(EV_ENUM(PLUS_1), s5k4ecgx_EV_Plus_1_v1),
		S5K4ECGX_REGSET(EV_ENUM(PLUS_2), s5k4ecgx_EV_Plus_2_v1),
		S5K4ECGX_REGSET(EV_ENUM(PLUS_3), s5k4ecgx_EV_Plus_3_v1),
		S5K4ECGX_REGSET(EV_ENUM(PLUS_4), s5k4ecgx_EV_Plus_4_v1),
	},
	.metering = {
		S5K4ECGX_REGSET(METERING_MATRIX, s5k4ecgx_Metering_Matrix_v1),
		S5K4ECGX_REGSET(METERING_CENTER, s5k4ecgx_Metering_Center_v1),
		S5K4ECGX_REGSET(METERING_SPOT, s5k4ecgx_Metering_Spot_v1),
	},
	.iso = {
		S5K4ECGX_REGSET(ISO_AUTO, s5k4ecgx_ISO_Auto_v1),
		S5K4ECGX_REGSET(ISO_50, s5k4ecgx_ISO_100_v1),     /* use 100 */
		S5K4ECGX_REGSET(ISO_100, s5k4ecgx_ISO_100_v1),
		S5K4ECGX_REGSET(ISO_200, s5k4ecgx_ISO_200_v1),
		S5K4ECGX_REGSET(ISO_400, s5k4ecgx_ISO_400_v1),
		S5K4ECGX_REGSET(ISO_800, s5k4ecgx_ISO_400_v1),    /* use 400 */
		S5K4ECGX_REGSET(ISO_1600, s5k4ecgx_ISO_400_v1),   /* use 400 */
		S5K4ECGX_REGSET(ISO_SPORTS, s5k4ecgx_ISO_Auto_v1),/* use auto */
		S5K4ECGX_REGSET(ISO_NIGHT, s5k4ecgx_ISO_Auto_v1), /* use auto */
		S5K4ECGX_REGSET(ISO_MOVIE, s5k4ecgx_ISO_Auto_v1), /* use auto */
	},
	.effect = {
		S5K4ECGX_REGSET(IMAGE_EFFECT_NONE, s5k4ecgx_Effect_Normal_v1),
		S5K4ECGX_REGSET(IMAGE_EFFECT_BNW,
				s5k4ecgx_Effect_Black_White_v1),
		S5K4ECGX_REGSET(IMAGE_EFFECT_SEPIA, s5k4ecgx_Effect_Sepia_v1),
		S5K4ECGX_REGSET(IMAGE_EFFECT_NEGATIVE,
				s5k4ecgx_Effect_Negative_v1),
	},
	.white_balance = {
		S5K4ECGX_REGSET(WHITE_BALANCE_AUTO, s5k4ecgx_WB_Auto_v1),
		S5K4ECGX_REGSET(WHITE_BALANCE_SUNNY, s5k4ecgx_WB_Sunny_v1),
		S5K4ECGX_REGSET(WHITE_BALANCE_CLOUDY, s5k4ecgx_WB_Cloudy_v1),
		S5K4ECGX_REGSET(WHITE_BALANCE_TUNGSTEN,
				s5k4ecgx_WB_Tungsten_v1),
		S5K4ECGX_REGSET(WHITE_BALANCE_FLUORESCENT,
				s5k4ecgx_WB_Fluorescent_v1),
	},
	.preview_size = {
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_QCIF, s5k4ecgx_176_Preview_v1),
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_CIF, s5k4ecgx_352_Preview_v1),
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_VGA, s5k4ecgx_640_Preview_v1),
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_D1, s5k4ecgx_720_Preview_v1),
	},
	.capture_size = {
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_VGA, s5k4ecgx_VGA_Capture_v1),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_1MP, s5k4ecgx_1M_Capture_v1),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_2MP, s5k4ecgx_2M_Capture_v1),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_3MP, s5k4ecgx_3M_Capture_v1),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_5MP, s5k4ecgx_5M_Capture_v1),
	},
	.scene_mode = {
		S5K4ECGX_REGSET(SCENE_MODE_NONE, s5k4ecgx_Scene_Default_v1),
		S5K4ECGX_REGSET(SCENE_MODE_PORTRAIT,
				s5k4ecgx_Scene_Portrait_v1),
		S5K4ECGX_REGSET(SCENE_MODE_NIGHTSHOT,
				s5k4ecgx_Scene_Nightshot_v1),
		S5K4ECGX_REGSET(SCENE_MODE_LANDSCAPE,
				s5k4ecgx_Scene_Landscape_v1),
		S5K4ECGX_REGSET(SCENE_MODE_SPORTS, s5k4ecgx_Scene_Sports_v1),
		S5K4ECGX_REGSET(SCENE_MODE_PARTY_INDOOR,
				s5k4ecgx_Scene_Party_Indoor_v1),
		S5K4ECGX_REGSET(SCENE_MODE_BEACH_SNOW,
				s5k4ecgx_Scene_Beach_Snow_v1),
		S5K4ECGX_REGSET(SCENE_MODE_SUNSET, s5k4ecgx_Scene_Sunset_v1),
		S5K4ECGX_REGSET(SCENE_MODE_FIREWORKS,
				s5k4ecgx_Scene_Fireworks_v1),
		S5K4ECGX_REGSET(SCENE_MODE_CANDLE_LIGHT,
				s5k4ecgx_Scene_Candle_Light_v1),
	},
	.saturation = {
		S5K4ECGX_REGSET(SATURATION_MINUS_2,
				s5k4ecgx_Saturation_Minus_2_v1),
		S5K4ECGX_REGSET(SATURATION_MINUS_1,
				s5k4ecgx_Saturation_Minus_1_v1),
		S5K4ECGX_REGSET(SATURATION_DEFAULT,
				s5k4ecgx_Saturation_Default_v1),
		S5K4ECGX_REGSET(SATURATION_PLUS_1,
				s5k4ecgx_Saturation_Plus_1_v1),
		S5K4ECGX_REGSET(SATURATION_PLUS_2,
				s5k4ecgx_Saturation_Plus_2_v1),
	},
	.contrast = {
		S5K4ECGX_REGSET(CONTRAST_MINUS_2, s5k4ecgx_Contrast_Minus_2_v1),
		S5K4ECGX_REGSET(CONTRAST_MINUS_1, s5k4ecgx_Contrast_Minus_1_v1),
		S5K4ECGX_REGSET(CONTRAST_DEFAULT, s5k4ecgx_Contrast_Default_v1),
		S5K4ECGX_REGSET(CONTRAST_PLUS_1, s5k4ecgx_Contrast_Plus_1_v1),
		S5K4ECGX_REGSET(CONTRAST_PLUS_2, s5k4ecgx_Contrast_Plus_2_v1),
	},
	.sharpness = {
		S5K4ECGX_REGSET(SHARPNESS_MINUS_2,
				s5k4ecgx_Sharpness_Minus_2_v1),
		S5K4ECGX_REGSET(SHARPNESS_MINUS_1,
				s5k4ecgx_Sharpness_Minus_1_v1),
		S5K4ECGX_REGSET(SHARPNESS_DEFAULT,
				s5k4ecgx_Sharpness_Default_v1),
		S5K4ECGX_REGSET(SHARPNESS_PLUS_1, s5k4ecgx_Sharpness_Plus_1_v1),
		S5K4ECGX_REGSET(SHARPNESS_PLUS_2, s5k4ecgx_Sharpness_Plus_2_v1),
	},
	.preview_return = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Preview_Return_v1),
	.jpeg_quality_high =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_Jpeg_Quality_High_v1),
	.jpeg_quality_normal =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_Jpeg_Quality_Normal_v1),
	.jpeg_quality_low = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Jpeg_Quality_Low_v1),
	.flash_start = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Flash_Start_v1),
	.flash_end = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Flash_End_v1),
	.af_assist_flash_start =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_Pre_Flash_Start_v1),
	.af_assist_flash_end =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_Pre_Flash_End_v1),
	.af_low_light_mode_on =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Low_Light_Mode_On_v1),
	.af_low_light_mode_off =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Low_Light_Mode_Off_v1),
	.ae_awb_lock_on =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_AE_AWB_Lock_On_v1),
	.ae_awb_lock_off =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_AE_AWB_Lock_Off_v1),
	.low_cap_on = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Low_Cap_On_v1),
	.low_cap_off = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Low_Cap_Off_v1),
	.wdr_on = S5K4ECGX_REGSET_TABLE(s5k4ecgx_WDR_on_v1),
	.wdr_off = S5K4ECGX_REGSET_TABLE(s5k4ecgx_WDR_off_v1),
	.face_detection_on =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_Face_Detection_On_v1),
	.face_detection_off =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_Face_Detection_Off_v1),
	.capture_start = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Capture_Start_v1),
	.af_macro_mode = S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Macro_mode_v1),
	.af_normal_mode = S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Normal_mode_v1),
	.af_return_macro_position =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Return_Macro_pos_v1),
	.single_af_start = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Single_AF_Start_v1),
	.single_af_off_1 = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Single_AF_Off_1_v1),
	.single_af_off_2 = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Single_AF_Off_2_v1),
	.dtp_start = S5K4ECGX_REGSET_TABLE(s5k4ecgx_DTP_init_v1),
	.dtp_stop = S5K4ECGX_REGSET_TABLE(s5k4ecgx_DTP_stop_v1),
	.init_reg = S5K4ECGX_REGSET_TABLE(s5k4ecgx_init_reg_v1),
	.flash_init = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Flash_init_v1),
	.reset_crop = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Reset_Crop_v1),
	.get_ae_stable_status =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_Get_AE_Stable_Status_v1),
	.get_light_level = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Get_Light_Level_v1),
	.get_1st_af_search_status =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_get_1st_af_search_status_v1),
	.get_2nd_af_search_status =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_get_2nd_af_search_status_v1),
	.get_capture_status =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_get_capture_status_v1),
	.get_esd_status = S5K4ECGX_REGSET_TABLE(s5k4ecgx_get_esd_status_v1),
	.get_iso = S5K4ECGX_REGSET_TABLE(s5k4ecgx_get_iso_reg_v1),
	.get_shutterspeed =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_get_shutterspeed_reg_v1),
};
#endif

#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_1
static const struct s5k4ecgx_regs regs_for_fw_version_1_1 = {
	.ev = {
		S5K4ECGX_REGSET(EV_ENUM(MINUS_4), s5k4ecgx_EV_Minus_4),
		S5K4ECGX_REGSET(EV_ENUM(MINUS_3), s5k4ecgx_EV_Minus_3),
		S5K4ECGX_REGSET(EV_ENUM(MINUS_2), s5k4ecgx_EV_Minus_2),
		S5K4ECGX_REGSET(EV_ENUM(MINUS_1), s5k4ecgx_EV_Minus_1),
		S5K4ECGX_REGSET(EV_ENUM(DEFAULT), s5k4ecgx_EV_Default),
		S5K4ECGX_REGSET(EV_ENUM(PLUS_1), s5k4ecgx_EV_Plus_1),
		S5K4ECGX_REGSET(EV_ENUM(PLUS_2), s5k4ecgx_EV_Plus_2),
		S5K4ECGX_REGSET(EV_ENUM(PLUS_3), s5k4ecgx_EV_Plus_3),
		S5K4ECGX_REGSET(EV_ENUM(PLUS_4), s5k4ecgx_EV_Plus_4),
	},
	.metering = {
		S5K4ECGX_REGSET(METERING_MATRIX, s5k4ecgx_Metering_Matrix),
		S5K4ECGX_REGSET(METERING_CENTER, s5k4ecgx_Metering_Center),
		S5K4ECGX_REGSET(METERING_SPOT, s5k4ecgx_Metering_Spot),
	},
	.iso = {
		S5K4ECGX_REGSET(ISO_AUTO, s5k4ecgx_ISO_Auto),
		S5K4ECGX_REGSET(ISO_50, s5k4ecgx_ISO_100),     /* map to 100 */
		S5K4ECGX_REGSET(ISO_100, s5k4ecgx_ISO_100),
		S5K4ECGX_REGSET(ISO_200, s5k4ecgx_ISO_200),
		S5K4ECGX_REGSET(ISO_400, s5k4ecgx_ISO_400),
		S5K4ECGX_REGSET(ISO_800, s5k4ecgx_ISO_400),    /* map to 400 */
		S5K4ECGX_REGSET(ISO_1600, s5k4ecgx_ISO_400),   /* map to 400 */
		S5K4ECGX_REGSET(ISO_SPORTS, s5k4ecgx_ISO_Auto),/* map to auto */
		S5K4ECGX_REGSET(ISO_NIGHT, s5k4ecgx_ISO_Auto), /* map to auto */
		S5K4ECGX_REGSET(ISO_MOVIE, s5k4ecgx_ISO_Auto), /* map to auto */
	},
	.effect = {
		S5K4ECGX_REGSET(IMAGE_EFFECT_NONE, s5k4ecgx_Effect_Normal),
		S5K4ECGX_REGSET(IMAGE_EFFECT_BNW, s5k4ecgx_Effect_Black_White),
		S5K4ECGX_REGSET(IMAGE_EFFECT_SEPIA, s5k4ecgx_Effect_Sepia),
		S5K4ECGX_REGSET(IMAGE_EFFECT_NEGATIVE,
				s5k4ecgx_Effect_Negative),
	},
	.white_balance = {
		S5K4ECGX_REGSET(WHITE_BALANCE_AUTO, s5k4ecgx_WB_Auto),
		S5K4ECGX_REGSET(WHITE_BALANCE_SUNNY, s5k4ecgx_WB_Sunny),
		S5K4ECGX_REGSET(WHITE_BALANCE_CLOUDY, s5k4ecgx_WB_Cloudy),
		S5K4ECGX_REGSET(WHITE_BALANCE_TUNGSTEN, s5k4ecgx_WB_Tungsten),
		S5K4ECGX_REGSET(WHITE_BALANCE_FLUORESCENT,
				s5k4ecgx_WB_Fluorescent),
	},
	.preview_size = {
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_QCIF, s5k4ecgx_176_Preview),
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_CIF, s5k4ecgx_352_Preview),
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_VGA, s5k4ecgx_640_Preview),
		S5K4ECGX_REGSET(S5K4ECGX_PREVIEW_D1, s5k4ecgx_720_Preview),
	},
	.capture_size = {
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_VGA, s5k4ecgx_VGA_Capture),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_1MP, s5k4ecgx_1M_Capture),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_2MP, s5k4ecgx_2M_Capture),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_3MP, s5k4ecgx_3M_Capture),
		S5K4ECGX_REGSET(S5K4ECGX_CAPTURE_5MP, s5k4ecgx_5M_Capture),
	},
	.scene_mode = {
		S5K4ECGX_REGSET(SCENE_MODE_NONE, s5k4ecgx_Scene_Default),
		S5K4ECGX_REGSET(SCENE_MODE_PORTRAIT, s5k4ecgx_Scene_Portrait),
		S5K4ECGX_REGSET(SCENE_MODE_NIGHTSHOT, s5k4ecgx_Scene_Nightshot),
		S5K4ECGX_REGSET(SCENE_MODE_LANDSCAPE, s5k4ecgx_Scene_Landscape),
		S5K4ECGX_REGSET(SCENE_MODE_SPORTS, s5k4ecgx_Scene_Sports),
		S5K4ECGX_REGSET(SCENE_MODE_PARTY_INDOOR,
				s5k4ecgx_Scene_Party_Indoor),
		S5K4ECGX_REGSET(SCENE_MODE_BEACH_SNOW,
				s5k4ecgx_Scene_Beach_Snow),
		S5K4ECGX_REGSET(SCENE_MODE_SUNSET, s5k4ecgx_Scene_Sunset),
		S5K4ECGX_REGSET(SCENE_MODE_FIREWORKS, s5k4ecgx_Scene_Fireworks),
		S5K4ECGX_REGSET(SCENE_MODE_CANDLE_LIGHT,
				s5k4ecgx_Scene_Candle_Light),
	},
	.saturation = {
		S5K4ECGX_REGSET(SATURATION_MINUS_2,
				s5k4ecgx_Saturation_Minus_2),
		S5K4ECGX_REGSET(SATURATION_MINUS_1,
				s5k4ecgx_Saturation_Minus_1),
		S5K4ECGX_REGSET(SATURATION_DEFAULT,
				s5k4ecgx_Saturation_Default),
		S5K4ECGX_REGSET(SATURATION_PLUS_1, s5k4ecgx_Saturation_Plus_1),
		S5K4ECGX_REGSET(SATURATION_PLUS_2, s5k4ecgx_Saturation_Plus_2),
	},
	.contrast = {
		S5K4ECGX_REGSET(CONTRAST_MINUS_2, s5k4ecgx_Contrast_Minus_2),
		S5K4ECGX_REGSET(CONTRAST_MINUS_1, s5k4ecgx_Contrast_Minus_1),
		S5K4ECGX_REGSET(CONTRAST_DEFAULT, s5k4ecgx_Contrast_Default),
		S5K4ECGX_REGSET(CONTRAST_PLUS_1, s5k4ecgx_Contrast_Plus_1),
		S5K4ECGX_REGSET(CONTRAST_PLUS_2, s5k4ecgx_Contrast_Plus_2),
	},
	.sharpness = {
		S5K4ECGX_REGSET(SHARPNESS_MINUS_2, s5k4ecgx_Sharpness_Minus_2),
		S5K4ECGX_REGSET(SHARPNESS_MINUS_1, s5k4ecgx_Sharpness_Minus_1),
		S5K4ECGX_REGSET(SHARPNESS_DEFAULT, s5k4ecgx_Sharpness_Default),
		S5K4ECGX_REGSET(SHARPNESS_PLUS_1, s5k4ecgx_Sharpness_Plus_1),
		S5K4ECGX_REGSET(SHARPNESS_PLUS_2, s5k4ecgx_Sharpness_Plus_2),
	},
	.fps = {
		S5K4ECGX_REGSET(FRAME_RATE_AUTO, s5k4ecgx_FPS_Auto),
		S5K4ECGX_REGSET(FRAME_RATE_7, s5k4ecgx_FPS_7),
		S5K4ECGX_REGSET(FRAME_RATE_15, s5k4ecgx_FPS_15),
		S5K4ECGX_REGSET(FRAME_RATE_30, s5k4ecgx_FPS_30),
	},
	.preview_return = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Preview_Return),
	.jpeg_quality_high = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Jpeg_Quality_High),
	.jpeg_quality_normal =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_Jpeg_Quality_Normal),
	.jpeg_quality_low = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Jpeg_Quality_Low),
	.flash_start = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Flash_Start),
	.flash_end = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Flash_End),
	.af_assist_flash_start =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_Pre_Flash_Start),
	.af_assist_flash_end =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_Pre_Flash_End),
	.af_low_light_mode_on =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Low_Light_Mode_On),
	.af_low_light_mode_off =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Low_Light_Mode_Off),
	.ae_awb_lock_on =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_AE_AWB_Lock_On),
	.ae_awb_lock_off =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_AE_AWB_Lock_Off),
	.low_cap_on = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Low_Cap_On),
	.low_cap_off = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Low_Cap_Off),
	.wdr_on = S5K4ECGX_REGSET_TABLE(s5k4ecgx_WDR_on),
	.wdr_off = S5K4ECGX_REGSET_TABLE(s5k4ecgx_WDR_off),
	.face_detection_on = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Face_Detection_On),
	.face_detection_off =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_Face_Detection_Off),
	.capture_start = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Capture_Start),
	.af_macro_mode = S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Macro_mode),
	.af_normal_mode = S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Normal_mode),
	.af_return_macro_position =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_AF_Return_Macro_pos),
	.single_af_start = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Single_AF_Start),
	.single_af_off_1 = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Single_AF_Off_1),
	.single_af_off_2 = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Single_AF_Off_2),
	.dtp_start = S5K4ECGX_REGSET_TABLE(s5k4ecgx_DTP_init),
	.dtp_stop = S5K4ECGX_REGSET_TABLE(s5k4ecgx_DTP_stop),
	.init_reg = S5K4ECGX_REGSET_TABLE(s5k4ecgx_init_reg),
	.flash_init = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Flash_init),
	.reset_crop = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Reset_Crop),
	.get_ae_stable_status =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_Get_AE_Stable_Status),
	.get_light_level = S5K4ECGX_REGSET_TABLE(s5k4ecgx_Get_Light_Level),
	.get_1st_af_search_status =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_get_1st_af_search_status),
	.get_2nd_af_search_status =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_get_2nd_af_search_status),
	.get_capture_status =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_get_capture_status),
	.get_esd_status = S5K4ECGX_REGSET_TABLE(s5k4ecgx_get_esd_status),
	.get_iso = S5K4ECGX_REGSET_TABLE(s5k4ecgx_get_iso_reg),
	.get_shutterspeed =
		S5K4ECGX_REGSET_TABLE(s5k4ecgx_get_shutterspeed_reg),
};
#endif

struct s5k4ecgx_state {
	struct s5k4ecgx_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct s5k4ecgx_jpeg_param jpeg;
	struct s5k4ecgx_version fw;
	struct s5k4ecgx_version prm;
	struct s5k4ecgx_date_info dateinfo;
	struct s5k4ecgx_position position;
	struct v4l2_streamparm strm;
	struct s5k4ecgx_gps_info gps_info;
	struct mutex ctrl_lock;
	enum s5k4ecgx_runmode runmode;
	enum s5k4ecgx_oprmode oprmode;
	enum af_operation_status af_status;
	int preview_framesize_index;
	int capture_framesize_index;
	int sensor_version;
	int freq;		/* MCLK in Hz */
	int check_dataline;
	int check_previewdata;
#ifdef CONFIG_VIDEO_S5K4ECGX_FLASHLIGHT
	bool flash_on;
	bool torch_on;
	bool flash_state_on_previous_capture;
#endif
	bool sensor_af_in_low_light_mode;
	bool initialized;
	bool restore_preview_size_needed;
	int one_frame_delay_ms;
	const struct s5k4ecgx_regs *regs;
	enum s5k4ecgx_reg_type reg_type;
	u16 reg_addr_high;
	u16 reg_addr_low;
};

static const struct v4l2_mbus_framefmt capture_fmts[] = {
	{
		.code		= V4L2_MBUS_FMT_FIXED,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
};

/**
 * s5k4ecgx_i2c_read_twobyte: Read 2 bytes from sensor
 */
static int s5k4ecgx_i2c_read_twobyte(struct i2c_client *client,
				  u16 subaddr, u16 *data)
{
	int err;
	unsigned char buf[2];
	struct i2c_msg msg[2];

	subaddr = swab16(subaddr);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = (u8 *)&subaddr;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = buf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		dev_err(&client->dev,
			"%s: register read fail\n", __func__);
		return -EIO;
	}

	*data = ((buf[0] << 8) | buf[1]);

	return 0;
}

static int s5k4ecgx_i2c_write(struct i2c_client *client,
					 const u8 *data, u16 data_len)
{
	int retry_count = 5;
	struct i2c_msg msg = {client->addr, 0, data_len, (u8 *)data};
	int ret = 0;

	do {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;
		msleep(POLL_TIME_MS);
		dev_err(&client->dev, "%s: I2C err %d, retry %d.\n",
			__func__, ret, retry_count);
	} while (retry_count-- > 0);
	if (ret != 1) {
		dev_err(&client->dev, "%s: I2C is not working.\n", __func__);
		return -EIO;
	}

	return 0;
}


static int s5k4ecgx_i2c_write_block(struct v4l2_subdev *sd, u8 *buf, int size)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

#ifdef CONFIG_VIDEO_S5K4ECGX_DEBUG
	if (s5k4ecgx_debug_mask & S5K4ECGX_DEBUG_I2C_BURSTS) {
		if ((buf[0] == 0x0F) && (buf[1] == 0x12))
			pr_info("%s : data[0,1] = 0x%02X%02X,"
				" total data size = %d\n",
				__func__, buf[2], buf[3], size-2);
		else
			pr_info("%s : 0x%02X%02X%02X%02X\n",
				__func__, buf[0], buf[1], buf[2], buf[3]);
	}
#endif

	err = s5k4ecgx_i2c_write(client, buf, size);
	if (err)
		return err;

#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_0
	{
		struct s5k4ecgx_state *state =
			container_of(sd, struct s5k4ecgx_state, sd);
		if (state->fw.minor == 0) {
			/* v1.0 sensor have problems sometimes if we write
			 * too much data too fast, so add a sleep.  I've
			 * tried various combinations of size/delay.  Checking
			 * for a larger size doesn't seem to work reliably, and
			 * a delay of 1ms sometimes isn't enough either.
			 */
			if (size > 16)
				msleep(2);
		}
	}
#endif
	return 0;
}

static int s5k4ecgx_i2c_write_two_word(struct i2c_client *client,
					 u16 addr, u16 w_data)
{
	u8 buf[4];

	addr = swab16(addr);
	w_data = swab16(w_data);

	memcpy(buf, &addr, 2);
	memcpy(buf + 2, &w_data, 2);

	s5k4ecgx_debug(S5K4ECGX_DEBUG_I2C, "%s : W(0x%02X%02X%02X%02X)\n",
		__func__, buf[0], buf[1], buf[2], buf[3]);

	return s5k4ecgx_i2c_write(client, buf, 4);
}

static int s5k4ecgx_burst_write_regs(struct v4l2_subdev *sd,
		struct s5k4ecgx_reg **start_reg,
		const u8 *regset_data, int *regset_data_idx)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct s5k4ecgx_reg *curr_reg = *start_reg;
	u16 addr_high, addr_low;

	u8 *burst_buf;
	int burst_len = 0;

	int ret = 0;

	addr_high = curr_reg->addr >> 16;
	addr_low = curr_reg->addr & 0xffff;

	if (state->reg_type != S5K4ECGX_REGTYPE_WRITE) {
		state->reg_addr_high = 0;
		state->reg_addr_low = 0;
		state->reg_type = S5K4ECGX_REGTYPE_WRITE;
	}

	if (state->reg_addr_high != addr_high) {
		s5k4ecgx_i2c_write_two_word(client, 0x0028, addr_high);
		state->reg_addr_high = addr_high;
		state->reg_addr_low = 0;
	}

	if (state->reg_addr_low != addr_low) {
		s5k4ecgx_i2c_write_two_word(client, 0x002A, addr_low);
		state->reg_addr_low = addr_low;
	}

	while (curr_reg->type != S5K4ECGX_REGTYPE_END) {
		burst_len += curr_reg->data_len;

		if (S5K4ECGX_REGTYPE_WRITE != (curr_reg + 1)->type)
			break;

		if (curr_reg->addr + 2 != (curr_reg + 1)->addr)
			break;

		curr_reg += 1;
	}

	burst_buf = vmalloc(burst_len + 2);
	if (burst_buf == NULL)
		return -ENOMEM;

	burst_buf[0] = 0x0F;
	burst_buf[1] = 0x12;
	memcpy(burst_buf + 2, regset_data + (*regset_data_idx), burst_len);

	ret = s5k4ecgx_i2c_write_block(sd, burst_buf, burst_len + 2);

	vfree(burst_buf);

#ifdef CONFIG_VIDEO_S5K4ECGX_DEBUG
	if (s5k4ecgx_debug_mask & S5K4ECGX_DEBUG_I2C_BURSTS) {
		u32 start_addr = (*start_reg)->addr;
		u32 end_addr = curr_reg->addr;
		int reg_count = (end_addr - start_addr) / 2;
		pr_debug("%s: burst write from 0x%08X to 0x%08X. %d regs.",
				"burst data starts with 0x0F12 and follow "
				"%d bytes.\n", __func__,
				start_addr, end_addr, reg_count, burst_len);
	}
#endif

	*start_reg = curr_reg;
	*regset_data_idx += burst_len;
	state->reg_addr_low = 0;

	return ret;
}

static int s5k4ecgx_request_reg_read(struct v4l2_subdev *sd, u32 addr)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);

	u16 addr_high, addr_low;

	addr_high = (addr >> 16) & 0xffff;
	addr_low = addr & 0xffff;


	if (state->reg_type != S5K4ECGX_REGTYPE_READ) {
		state->reg_addr_high = 0;
		state->reg_type = S5K4ECGX_REGTYPE_READ;
	}

	if (state->reg_addr_high != addr_high) {
		s5k4ecgx_i2c_write_two_word(client, 0x002C, addr_high);
		state->reg_addr_high = addr_high;
	}

	s5k4ecgx_i2c_write_two_word(client, 0x002E, addr_low);

	return 0;
}

static int s5k4ecgx_reg_read_16(struct i2c_client *client, u16 *value)
{
	return s5k4ecgx_i2c_read_twobyte(client, 0x0F12, value);
}

static int s5k4ecgx_reg_read_32(struct i2c_client *client, u32 *value)
{
	int err = 0;
	u16 value_high, value_low;

	err |= s5k4ecgx_i2c_read_twobyte(client, 0x0F12, &value_high);
	err |= s5k4ecgx_i2c_read_twobyte(client, 0x0F12, &value_low);

	*value = (value_high << 16) | (value_low & 0xffff);

	return err;
}

static int s5k4ecgx_write_regset(struct v4l2_subdev *sd,
				const struct s5k4ecgx_regset_table *table)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct s5k4ecgx_reg *curr_reg = (struct s5k4ecgx_reg *)table->regset;
	const u8 *regset_data = table->data;
	int data_idx = 0;
	int err = 0;

#ifdef CONFIG_VIDEO_S5K4ECGX_DEBUG
	dev_dbg(&client->dev, "%s: writing regset, %s...\n", __func__,
			table->name);
#endif

	if (curr_reg == NULL)
		return -EINVAL;

	while(curr_reg->type != S5K4ECGX_REGTYPE_END && err == 0) {
		switch(curr_reg->type) {
		case S5K4ECGX_REGTYPE_WRITE:
			err = s5k4ecgx_burst_write_regs(sd, &curr_reg,
					regset_data, &data_idx);
			break;

		case S5K4ECGX_REGTYPE_CMD:
			err = s5k4ecgx_i2c_write(client,
					regset_data + data_idx,
					curr_reg->data_len);
			data_idx += curr_reg->data_len;
			state->reg_type = S5K4ECGX_REGTYPE_CMD;
			break;

		case S5K4ECGX_REGTYPE_READ:
			err = s5k4ecgx_request_reg_read(sd, curr_reg->addr);
			data_idx += curr_reg->data_len;
			break;

		case S5K4ECGX_REGTYPE_DELAY:
			msleep(curr_reg->msec);
			data_idx += curr_reg->data_len;
			break;

		default:
			dev_err(&client->dev,
					"%s: Got unknown reg_type, %d!\n",
					__func__, curr_reg->type);
			err = -EINVAL;
			break;
		}
		curr_reg += 1;
	}

	if (unlikely(curr_reg->type != S5K4ECGX_REGTYPE_END)) {
		dev_err(&client->dev, "%s: fail to write regset!\n", __func__);
		return -EIO;
	}

	return err;
}

static int s5k4ecgx_set_parameter(struct v4l2_subdev *sd,
				int *current_value_ptr,
				int new_value,
				const char *setting_name,
				const struct s5k4ecgx_regset_table *table,
				int table_size)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	if (*current_value_ptr == new_value)
		return 0;

	if ((new_value < 0) || (new_value >= table_size)) {
		dev_err(&client->dev, "%s: invalid index, %d "
				"for setting table, %s[0:%d]\n", __func__,
				new_value, setting_name, table_size);
		return -EINVAL;
	}

	err = s5k4ecgx_write_regset(sd, table + new_value);
	if (!err)
		*current_value_ptr = new_value;

	return err;
}

static int s5k4ecgx_set_preview_stop(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);

	if (state->runmode == S5K4ECGX_RUNMODE_RUNNING)
		state->runmode = S5K4ECGX_RUNMODE_IDLE;

	dev_dbg(&client->dev, "%s:\n", __func__);

	return 0;
}

static int s5k4ecgx_set_preview_start(struct v4l2_subdev *sd)
{
	int err;
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	bool set_size = true;

	dev_dbg(&client->dev, "%s: runmode = %d\n",
		__func__, state->runmode);

	if (!state->pix.width || !state->pix.height ||
		!state->strm.parm.capture.timeperframe.denominator)
		return -EINVAL;

	if (state->runmode == S5K4ECGX_RUNMODE_CAPTURE) {
		dev_dbg(&client->dev, "%s: sending Preview_Return cmd\n",
			__func__);
		err = s5k4ecgx_write_regset(sd, &state->regs->preview_return);
		if (err < 0) {
			dev_err(&client->dev,
				"%s: failed: s5k4ecgx_Preview_Return\n",
				__func__);
			return -EIO;
		}
		set_size = state->restore_preview_size_needed;
	}

	if (set_size) {
		err = s5k4ecgx_write_regset(sd,
				state->regs->preview_size +
				state->preview_framesize_index);
		if (err < 0) {
			dev_err(&client->dev,
				"%s: failed: Could not set preview size\n",
				__func__);
			return -EIO;
		}
	}

	dev_dbg(&client->dev, "%s: runmode now RUNNING\n", __func__);
	state->runmode = S5K4ECGX_RUNMODE_RUNNING;

	return 0;
}

static int s5k4ecgx_set_capture_size(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	int err;

	dev_dbg(&client->dev, "%s: index:%d\n", __func__,
		state->capture_framesize_index);

	err = s5k4ecgx_write_regset(sd,
			state->regs->capture_size +
			state->capture_framesize_index);
	if (err < 0) {
		dev_err(&client->dev,
			"%s: failed: i2c_write for capture_size index %d\n",
			__func__, state->capture_framesize_index);
	}
	state->runmode = S5K4ECGX_RUNMODE_CAPTURE;

	return err;
}

static int s5k4ecgx_set_jpeg_quality(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	const struct s5k4ecgx_regset_table *regset = NULL;

	dev_dbg(&client->dev,
		"%s: jpeg.quality %d\n", __func__, state->jpeg.quality);
	if (state->jpeg.quality < 0)
		state->jpeg.quality = 0;
	if (state->jpeg.quality > 100)
		state->jpeg.quality = 100;

	switch (state->jpeg.quality) {
	case 90 ... 100:
		dev_dbg(&client->dev,
			"%s: setting to high jpeg quality\n", __func__);
		regset = &state->regs->jpeg_quality_high;
		break;
	case 80 ... 89:
		dev_dbg(&client->dev,
			"%s: setting to normal jpeg quality\n", __func__);
		regset = &state->regs->jpeg_quality_normal;
		break;
	default:
		dev_dbg(&client->dev,
			"%s: setting to low jpeg quality\n", __func__);
		regset = &state->regs->jpeg_quality_low;
		break;
	}

	return s5k4ecgx_write_regset(sd, regset);
}

static u16 s5k4ecgx_get_light_level(struct v4l2_subdev *sd)
{
	int err;
	u16 read_value = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);

	err = s5k4ecgx_write_regset(sd, &state->regs->get_light_level);
	if (err) {
		dev_err(&client->dev,
			"%s: write cmd failed, returning 0\n", __func__);
		goto out;
	}
	err = s5k4ecgx_reg_read_16(client, &read_value);
	if (err) {
		dev_err(&client->dev,
			"%s: read cmd failed, returning 0\n", __func__);
		goto out;
	}

	dev_dbg(&client->dev, "%s: read_value = %d (0x%X)\n",
		__func__, read_value, read_value);

out:
	return read_value;
}

static int s5k4ecgx_start_capture(struct v4l2_subdev *sd)
{
	int err;
	u16 read_value;
	u16 light_level;
	int poll_time_ms;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct sec_cam_parm *parms =
		(struct sec_cam_parm *)&state->strm.parm.raw_data;
#ifdef CONFIG_VIDEO_S5K4ECGX_FLASHLIGHT
	struct s5k4ecgx_platform_data *pdata = client->dev.platform_data;
#endif

	/* reset cropping if our current preview is not 640x480,
	 * otherwise the capture will be wrong because of the cropping
	 */
	if (state->preview_framesize_index != S5K4ECGX_PREVIEW_VGA) {
		int err = s5k4ecgx_write_regset(sd, &state->regs->reset_crop);
		if (err < 0) {
			dev_err(&client->dev,
				"%s: failed: Could not set preview size\n",
				__func__);
			return -EIO;
		}
		state->restore_preview_size_needed = true;
	} else
		state->restore_preview_size_needed = false;

	msleep(50);
	light_level = s5k4ecgx_get_light_level(sd);

	dev_dbg(&client->dev, "%s: light_level = %d\n", __func__,
		light_level);

#ifdef CONFIG_VIDEO_S5K4ECGX_FLASHLIGHT
	state->flash_state_on_previous_capture = false;
	if (parms->scene_mode != SCENE_MODE_NIGHTSHOT) {
		switch (parms->flash_mode) {
		case FLASH_MODE_AUTO:
			if (light_level > LOW_LIGHT_LEVEL) {
				/* light level bright enough
				 * that we don't need flash
				 */
				break;
			}
			/* fall through to flash start */
		case FLASH_MODE_ON:
			if (parms->focus_mode == FOCUS_MODE_INFINITY) {
				s5k4ecgx_write_regset(sd,
					&state->regs->af_assist_flash_start);
				s5k4ecgx_write_regset(sd,
					&state->regs->af_assist_flash_end);
				msleep(10);
			}
			s5k4ecgx_write_regset(sd,
					&state->regs->af_assist_flash_start);
			state->flash_on = true;
			state->flash_state_on_previous_capture = true;
			pdata->flash_onoff(1);
			break;
		default:
			break;
		}
	}
#endif

	/* if light is low, use low light capture settings, EXCEPT
	 * if scene mode set to NIGHTSHOT or SPORTS because they
	 * have their own settings (though a low light sport setting
	 * could be useful)
	 */
	if ((light_level <= HIGH_LIGHT_LEVEL) &&
		(parms->scene_mode != SCENE_MODE_NIGHTSHOT) &&
		(parms->scene_mode != SCENE_MODE_SPORTS)) {
		s5k4ecgx_write_regset(sd, &state->regs->low_cap_on);
	}

	err = s5k4ecgx_set_capture_size(sd);
	if (err < 0) {
		dev_err(&client->dev,
			"%s: failed: i2c_write for capture_resolution\n",
			__func__);
		return -EIO;
	}

	dev_dbg(&client->dev, "%s: send Capture_Start cmd\n", __func__);
	s5k4ecgx_write_regset(sd, &state->regs->capture_start);

	/* a shot takes takes at least 50ms so sleep that amount first
	 * and then start polling for completion.
	 */
	msleep(50);
	poll_time_ms = 50;
	do {
		s5k4ecgx_write_regset(sd, &state->regs->get_capture_status);
		s5k4ecgx_reg_read_16(client, &read_value);
		dev_dbg(&client->dev,
			"%s: s5k4ecgx_Capture_Start check = %#x\n",
			__func__, read_value);
		if (read_value != 0x00)
			break;
		msleep(POLL_TIME_MS);
		poll_time_ms += POLL_TIME_MS;
	} while (poll_time_ms < CAPTURE_POLL_TIME_MS);

	dev_dbg(&client->dev, "%s: capture done check finished after %d ms\n",
		__func__, poll_time_ms);

	s5k4ecgx_write_regset(sd, &state->regs->ae_awb_lock_off);

	if ((light_level <= HIGH_LIGHT_LEVEL) &&
		(parms->scene_mode != SCENE_MODE_NIGHTSHOT) &&
		(parms->scene_mode != SCENE_MODE_SPORTS)) {
		s5k4ecgx_write_regset(sd, &state->regs->low_cap_off);
	}

#ifdef CONFIG_VIDEO_S5K4ECGX_FLASHLIGHT
	if ((parms->scene_mode != SCENE_MODE_NIGHTSHOT) && (state->flash_on)) {
		state->flash_on = false;
		pdata->flash_onoff(0);
		s5k4ecgx_write_regset(sd, &state->regs->flash_end);
	}
#endif

	return 0;
}

/* wide dynamic range support */
static int s5k4ecgx_set_wdr(struct v4l2_subdev *sd, int value)
{
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);

	if (value == WDR_ON)
		return s5k4ecgx_write_regset(sd, &state->regs->wdr_on);

	return s5k4ecgx_write_regset(sd, &state->regs->wdr_off);
}

static int s5k4ecgx_set_face_detection(struct v4l2_subdev *sd, int value)
{
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);

	if (value == FACE_DETECTION_ON)
		return s5k4ecgx_write_regset(sd,
				&state->regs->face_detection_on);

	return s5k4ecgx_write_regset(sd,
			&state->regs->face_detection_off);
}

static int s5k4ecgx_return_focus(struct v4l2_subdev *sd)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
			container_of(sd, struct s5k4ecgx_state, sd);

	err = s5k4ecgx_write_regset(sd, &state->regs->af_normal_mode);
	if (err < 0)
		goto fail;

	return 0;
fail:
	dev_err(&client->dev,
		"%s: i2c_write failed\n", __func__);
	return -EIO;
}

static int s5k4ecgx_set_focus_mode(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct sec_cam_parm *parms =
		(struct sec_cam_parm *)&state->strm.parm.raw_data;
	int err;

	if (parms->focus_mode == value)
		return 0;

	dev_dbg(&client->dev, "%s value(%d)\n", __func__, value);

	switch (value) {
	case FOCUS_MODE_MACRO:
		dev_dbg(&client->dev,
				"%s: FOCUS_MODE_MACRO\n", __func__);
		err = s5k4ecgx_write_regset(sd, &state->regs->af_macro_mode);
		if (err < 0)
			goto fail;
		parms->focus_mode = FOCUS_MODE_MACRO;
		break;

	case FOCUS_MODE_INFINITY:
	case FOCUS_MODE_AUTO:
		err = s5k4ecgx_write_regset(sd, &state->regs->af_normal_mode);
		if (err < 0)
			goto fail;
		parms->focus_mode = value;
		break;
	default:
		return -EINVAL;
		break;
	}

	return 0;
fail:
	dev_err(&client->dev,
		"%s: i2c_write failed\n", __func__);
	return -EIO;
}

#ifdef CONFIG_VIDEO_S5K4ECGX_FLASHLIGHT
static void s5k4ecgx_auto_focus_flash_start(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct s5k4ecgx_platform_data *pdata = client->dev.platform_data;
	int count;
	u16 read_value;

	s5k4ecgx_write_regset(sd, &state->regs->af_assist_flash_start);
	state->flash_on = true;
	pdata->af_assist_onoff(1);

	/* delay 200ms (SLSI value) and then poll to see if AE is stable.
	 * once it is stable, lock it and then return to do AF
	 */
	msleep(200);

	for (count = 0; count < AE_STABLE_SEARCH_COUNT; count++) {
		if (state->af_status == AF_CANCEL)
			break;
		s5k4ecgx_write_regset(sd, &state->regs->get_ae_stable_status);
		s5k4ecgx_reg_read_16(client, &read_value);
		dev_dbg(&client->dev, "%s: ae stable status = %#x\n",
			__func__, read_value);
		if (read_value == 0x1)
			break;
		msleep(state->one_frame_delay_ms);
	}

	/* if we were cancelled, turn off flash */
	if (state->af_status == AF_CANCEL) {
		dev_dbg(&client->dev,
			"%s: AF cancelled\n", __func__);
		s5k4ecgx_write_regset(sd, &state->regs->af_assist_flash_end);
		state->flash_on = false;
		pdata->af_assist_onoff(0);
	}
}
#endif

static int s5k4ecgx_start_auto_focus(struct v4l2_subdev *sd)
{
	int light_level;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct sec_cam_parm *parms =
		(struct sec_cam_parm *)&state->strm.parm.raw_data;

	dev_dbg(&client->dev, "%s: start SINGLE AF operation, flash mode %d\n",
		__func__, parms->flash_mode);

	/* in case user calls auto_focus repeatedly without a cancel
	 * or a capture, we need to cancel here to allow ae_awb
	 * to work again, or else we could be locked forever while
	 * that app is running, which is not the expected behavior.
	 */
	s5k4ecgx_write_regset(sd, &state->regs->ae_awb_lock_off);

	if (parms->scene_mode == SCENE_MODE_NIGHTSHOT) {
		/* user selected night shot mode, assume we need low light
		 * af mode.  flash is always off in night shot mode
		 */
		goto enable_af_low_light_mode;
	}

	light_level = s5k4ecgx_get_light_level(sd);

#ifdef CONFIG_VIDEO_S5K4ECGX_FLASHLIGHT
	switch (parms->flash_mode) {
	case FLASH_MODE_AUTO:
		if (light_level > LOW_LIGHT_LEVEL) {
			/* flash not needed */
			break;
		}
		/* fall through to turn on flash for AF assist */
	case FLASH_MODE_ON:
		s5k4ecgx_auto_focus_flash_start(sd);
		if (state->af_status == AF_CANCEL)
			return 0;
		break;
	case FLASH_MODE_OFF:
		break;
	default:
		dev_err(&client->dev,
			"%s: Unknown Flash mode 0x%x\n",
			__func__, parms->flash_mode);
		break;
	}
#endif

	if (light_level > LOW_LIGHT_LEVEL) {
		if (state->sensor_af_in_low_light_mode) {
			state->sensor_af_in_low_light_mode = false;
			s5k4ecgx_write_regset(sd,
				&state->regs->af_low_light_mode_off);
		}
	} else {
enable_af_low_light_mode:
		if (!state->sensor_af_in_low_light_mode) {
			state->sensor_af_in_low_light_mode = true;
			s5k4ecgx_write_regset(sd,
				&state->regs->af_low_light_mode_on);
		}
	}

	s5k4ecgx_write_regset(sd, &state->regs->single_af_start);
	state->af_status = AF_INITIAL;
	dev_dbg(&client->dev, "%s: af_status set to start\n", __func__);

	return 0;
}

/* called by HAL after auto focus was finished.
 * it might off the assist flash
 */
static int s5k4ecgx_finish_auto_focus(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);

#ifdef CONFIG_VIDEO_S5K4ECGX_FLASHLIGHT
	if (state->flash_on) {
		struct s5k4ecgx_platform_data *pd = client->dev.platform_data;
		s5k4ecgx_write_regset(sd, &state->regs->af_assist_flash_end);
		state->flash_on = false;
		pd->af_assist_onoff(0);
	}
#endif

	dev_dbg(&client->dev, "%s: single AF finished\n", __func__);
	state->af_status = AF_NONE;
	return 0;
}

static int s5k4ecgx_stop_auto_focus(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct sec_cam_parm *parms =
		(struct sec_cam_parm *)&state->strm.parm.raw_data;
	int focus_mode = parms->focus_mode;

	dev_dbg(&client->dev, "%s: single AF Off command Setting\n", __func__);

	/* always cancel ae_awb, in case AF already finished before
	 * we got called.
	 */
	s5k4ecgx_write_regset(sd, &state->regs->ae_awb_lock_off);
#ifdef CONFIG_VIDEO_S5K4ECGX_FLASHLIGHT
	if (state->flash_on)
		s5k4ecgx_finish_auto_focus(sd);
#endif

	if (state->af_status != AF_START) {
		/* we weren't in the middle auto focus operation, we're done */
		dev_dbg(&client->dev,
			"%s: auto focus not in progress, done\n", __func__);

		if (focus_mode == FOCUS_MODE_MACRO) {
			/* for change focus mode forcely */
			parms->focus_mode = -1;
			s5k4ecgx_set_focus_mode(sd, FOCUS_MODE_MACRO);
		} else if (focus_mode == FOCUS_MODE_AUTO) {
			/* for change focus mode forcely */
			parms->focus_mode = -1;
			s5k4ecgx_set_focus_mode(sd, FOCUS_MODE_AUTO);
		}

		return 0;
	}

	/* auto focus was in progress.  the other thread
	 * is either in the middle of s5k4ecgx_get_auto_focus_result_first(),
	 * s5k4ecgx_get_auto_focus_result_second()
	 * or will call it shortly.  set a flag to have
	 * it abort it's polling.  that thread will
	 * also do cleanup like restore focus position.
	 *
	 * it might be enough to just send sensor commands
	 * to abort auto focus and the other thread would get
	 * that state from it's polling calls, but I'm not sure.
	 */
	state->af_status = AF_CANCEL;
	dev_dbg(&client->dev,
		"%s: sending Single_AF_Off commands to sensor\n", __func__);

	s5k4ecgx_write_regset(sd, &state->regs->single_af_off_1);

	msleep(state->one_frame_delay_ms);

	s5k4ecgx_write_regset(sd, &state->regs->single_af_off_2);

	return 0;
}

/* called by HAL after auto focus was started to get the first search result*/
static int s5k4ecgx_get_auto_focus_result_first(struct v4l2_subdev *sd,
					struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	u16 read_value;

	if (state->af_status == AF_INITIAL) {
		dev_dbg(&client->dev, "%s: Check AF Result\n", __func__);
		if (state->af_status == AF_NONE) {
			dev_dbg(&client->dev,
				"%s: auto focus never started, returning 0x2\n",
				__func__);
			ctrl->value = AUTO_FOCUS_CANCELLED;
			return 0;
		}

		/* must delay 2 frame times before checking result of 1st phase */
		mutex_unlock(&state->ctrl_lock);
		msleep(state->one_frame_delay_ms*2);
		mutex_lock(&state->ctrl_lock);

		/* lock AE and AWB after first AF search */
		s5k4ecgx_write_regset(sd, &state->regs->ae_awb_lock_on);

		dev_dbg(&client->dev, "%s: 1st AF search\n", __func__);
		/* enter read mode */
		s5k4ecgx_i2c_write_two_word(client, 0x002C, 0x7000);
		state->af_status = AF_START;
	} else if (state->af_status == AF_CANCEL) {
		dev_dbg(&client->dev,
			"%s: AF is cancelled while doing\n", __func__);
		ctrl->value = AUTO_FOCUS_CANCELLED;
		s5k4ecgx_finish_auto_focus(sd);
		return 0;
	}
	s5k4ecgx_write_regset(sd, &state->regs->get_1st_af_search_status);
	s5k4ecgx_i2c_read_twobyte(client, 0x0F12, &read_value);
	dev_dbg(&client->dev,
		"%s: 1st i2c_read --- read_value == 0x%x\n",
		__func__, read_value);
	ctrl->value = read_value;
	return 0;
}

/* called by HAL after first search was succeed to get the second search result*/
static int s5k4ecgx_get_auto_focus_result_second(struct v4l2_subdev *sd,
					struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	u16 read_value;

	if (state->af_status == AF_CANCEL) {
		dev_dbg(&client->dev,
			"%s: AF is cancelled while doing\n", __func__);
		ctrl->value = AUTO_FOCUS_CANCELLED;
		s5k4ecgx_finish_auto_focus(sd);
		return 0;
	}
	s5k4ecgx_write_regset(sd, &state->regs->get_2nd_af_search_status);
	s5k4ecgx_i2c_read_twobyte(client, 0x0F12, &read_value);
	dev_dbg(&client->dev,
		"%s: 2nd i2c_read --- read_value == 0x%x\n",
		__func__, read_value);
	ctrl->value = read_value;
	return 0;
}

static void s5k4ecgx_init_parameters(struct v4l2_subdev *sd)
{
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct sec_cam_parm *parms =
		(struct sec_cam_parm *)&state->strm.parm.raw_data;

	state->strm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parms->capture.capturemode = 0;
	parms->capture.timeperframe.numerator = 1;
	parms->capture.timeperframe.denominator = 30;
	parms->contrast = CONTRAST_DEFAULT;
	parms->effects = IMAGE_EFFECT_NONE;
	parms->brightness = EV_ENUM(DEFAULT);
	parms->flash_mode = FLASH_MODE_AUTO;
	parms->focus_mode = FOCUS_MODE_AUTO;
	parms->iso = ISO_AUTO;
	parms->metering = METERING_CENTER;
	parms->saturation = SATURATION_DEFAULT;
	parms->scene_mode = SCENE_MODE_NONE;
	parms->sharpness = SHARPNESS_DEFAULT;
	parms->white_balance = WHITE_BALANCE_AUTO;

	state->jpeg.enable = 0;
	state->jpeg.quality = 100;
	state->jpeg.main_offset = 0;
	state->jpeg.main_size = 0;
	state->jpeg.thumb_offset = 0;
	state->jpeg.thumb_size = 0;
	state->jpeg.postview_offset = 0;

	state->fw.major = 1;

	state->one_frame_delay_ms = NORMAL_MODE_MAX_ONE_FRAME_DELAY_MS;
}

static void s5k4ecgx_set_framesize(struct v4l2_subdev *sd,
				const struct s5k4ecgx_framesize *frmsize,
				int frmsize_count, bool exact_match);
static int s5k4ecgx_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s: code = 0x%x, field = 0x%x,"
		" colorspace = 0x%x, width = %d, height = %d\n",
		__func__, fmt->code, fmt->field,
		fmt->colorspace,
		fmt->width, fmt->height);

	if (fmt->code == V4L2_MBUS_FMT_FIXED &&
		fmt->colorspace != V4L2_COLORSPACE_JPEG) {
		dev_err(&client->dev,
			"%s: mismatch in pixelformat and colorspace\n",
			__func__);
		return -EINVAL;
	}

	state->pix.width = fmt->width;
	state->pix.height = fmt->height;
	if (fmt->colorspace == V4L2_COLORSPACE_JPEG)
		state->pix.pixelformat = V4L2_PIX_FMT_JPEG;
	else
		state->pix.pixelformat = 0; /* is this used anywhere? */

	if (fmt->colorspace == V4L2_COLORSPACE_JPEG) {
		state->oprmode = S5K4ECGX_OPRMODE_IMAGE;
		/*
		 * In case of image capture mode,
		 * if the given image resolution is not supported,
		 * use the next higher image resolution. */
		s5k4ecgx_set_framesize(sd, s5k4ecgx_capture_framesize_list,
				ARRAY_SIZE(s5k4ecgx_capture_framesize_list),
				false);

	} else {
		state->oprmode = S5K4ECGX_OPRMODE_VIDEO;
		/*
		 * In case of video mode,
		 * if the given video resolution is not matching, use
		 * the default rate (currently S5K4ECGX_PREVIEW_WVGA).
		 */
		s5k4ecgx_set_framesize(sd, s5k4ecgx_preview_framesize_list,
				ARRAY_SIZE(s5k4ecgx_preview_framesize_list),
				true);
	}

	state->jpeg.enable = state->pix.pixelformat == V4L2_PIX_FMT_JPEG;

	return 0;
}

static int s5k4ecgx_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);

	/* The camera interface should read this value, this is the resolution
	 * at which the sensor would provide framedata to the camera i/f
	 *
	 * In case of image capture,
	 * this returns the default camera resolution (SVGA)
	 */
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = state->pix.width;
	fsize->discrete.height = state->pix.height;
	return 0;
}

static int s5k4ecgx_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
				  enum v4l2_mbus_pixelcode *code)
{
	pr_debug("%s: index = %d\n", __func__, index);

	if (index >= ARRAY_SIZE(capture_fmts))
		return -EINVAL;

	*code = capture_fmts[index].code;

	return 0;
}

static int s5k4ecgx_try_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	int num_entries;
	int i;

	num_entries = ARRAY_SIZE(capture_fmts);

	pr_debug("%s: code = 0x%x, colorspace = 0x%x, num_entries = %d\n",
		__func__, fmt->code, fmt->colorspace, num_entries);

	for (i = 0; i < num_entries; i++) {
		if (capture_fmts[i].code == fmt->code &&
		    capture_fmts[i].colorspace == fmt->colorspace) {
			pr_debug("%s: match found, returning 0\n", __func__);
			return 0;
		}
	}

	pr_debug("%s: no match found, returning -EINVAL\n", __func__);
	return -EINVAL;
}

#ifdef CONFIG_VIDEO_S5K4ECGX_FLASHLIGHT
static void s5k4ecgx_enable_torch(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct s5k4ecgx_platform_data *pdata = client->dev.platform_data;

	s5k4ecgx_write_regset(sd, &state->regs->flash_start);
	state->torch_on = true;
	pdata->torch_onoff(1);
}

static void s5k4ecgx_disable_torch(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct s5k4ecgx_platform_data *pdata = client->dev.platform_data;

	if (state->torch_on) {
		state->torch_on = false;
		pdata->torch_onoff(0);
		s5k4ecgx_write_regset(sd, &state->regs->flash_end);
	}
}
static int s5k4ecgx_set_flash_mode(struct v4l2_subdev *sd, int value)
{
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct sec_cam_parm *parms =
		(struct sec_cam_parm *)&state->strm.parm.raw_data;

	if (parms->flash_mode == value)
		return 0;

	if ((value >= FLASH_MODE_OFF) && (value <= FLASH_MODE_TORCH)) {
		pr_debug("%s: setting flash mode to %d\n",
			__func__, value);
		parms->flash_mode = value;
		if (parms->flash_mode == FLASH_MODE_TORCH)
			s5k4ecgx_enable_torch(sd);
		else
			s5k4ecgx_disable_torch(sd);
		return 0;
	}
	pr_debug("%s: trying to set invalid flash mode %d\n",
		__func__, value);
	return -EINVAL;
}
#endif

static int s5k4ecgx_g_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);

	memcpy(param, &state->strm, sizeof(param));
	return 0;
}

static int s5k4ecgx_s_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct sec_cam_parm *new_parms =
		(struct sec_cam_parm *)&param->parm.raw_data;
	struct sec_cam_parm *parms =
		(struct sec_cam_parm *)&state->strm.parm.raw_data;

	dev_dbg(&client->dev, "%s: start\n", __func__);

	if (param->parm.capture.timeperframe.numerator !=
		parms->capture.timeperframe.numerator ||
		param->parm.capture.timeperframe.denominator !=
		parms->capture.timeperframe.denominator) {

		int fps = 0;
		int fps_max = 30;

		if (param->parm.capture.timeperframe.numerator &&
			param->parm.capture.timeperframe.denominator)
			fps =
			    (int)(param->parm.capture.timeperframe.denominator /
				  param->parm.capture.timeperframe.numerator);
		else
			fps = 0;

		if (fps <= 0 || fps > fps_max) {
			dev_err(&client->dev,
				"%s: Framerate %d not supported,"
				" setting it to %d fps.\n",
				__func__, fps, fps_max);
			fps = fps_max;
		}

		/*
		 * Don't set the fps value, just update it in the state
		 * We will set the resolution and
		 * fps in the start operation (preview/capture) call
		 */
		parms->capture.timeperframe.numerator = 1;
		parms->capture.timeperframe.denominator = fps;
	}

	/* we return an error if one happened but don't stop trying to
	 * set all parameters passed
	 */
	err = s5k4ecgx_set_parameter(sd, &parms->contrast, new_parms->contrast,
				"contrast", state->regs->contrast,
				ARRAY_SIZE(state->regs->contrast));
	err |= s5k4ecgx_set_parameter(sd, &parms->effects, new_parms->effects,
				"effect", state->regs->effect,
				ARRAY_SIZE(state->regs->effect));
	err |= s5k4ecgx_set_parameter(sd, &parms->brightness,
				new_parms->brightness - EV_MINUS_4, "brightness",
				state->regs->ev, ARRAY_SIZE(state->regs->ev));
#ifdef CONFIG_VIDEO_S5K4ECGX_FLASHLIGHT
	err |= s5k4ecgx_set_flash_mode(sd, new_parms->flash_mode);
#endif
	/* Must delay 150ms before setting macro mode due to a camera
	 * sensor requirement */
	if ((new_parms->focus_mode == FOCUS_MODE_MACRO) &&
			(parms->focus_mode != FOCUS_MODE_MACRO))
		msleep(150);
	err |= s5k4ecgx_set_focus_mode(sd, new_parms->focus_mode);
	err |= s5k4ecgx_set_parameter(sd, &parms->iso, new_parms->iso,
				"iso", state->regs->iso,
				ARRAY_SIZE(state->regs->iso));
	err |= s5k4ecgx_set_parameter(sd, &parms->metering, new_parms->metering,
				"metering", state->regs->metering,
				ARRAY_SIZE(state->regs->metering));
	err |= s5k4ecgx_set_parameter(sd, &parms->saturation,
				new_parms->saturation, "saturation",
				state->regs->saturation,
				ARRAY_SIZE(state->regs->saturation));
	err |= s5k4ecgx_set_parameter(sd, &parms->scene_mode,
				new_parms->scene_mode, "scene_mode",
				state->regs->scene_mode,
				ARRAY_SIZE(state->regs->scene_mode));
	err |= s5k4ecgx_set_parameter(sd, &parms->sharpness,
				new_parms->sharpness, "sharpness",
				state->regs->sharpness,
				ARRAY_SIZE(state->regs->sharpness));
	err |= s5k4ecgx_set_parameter(sd, &parms->white_balance,
				new_parms->white_balance, "white balance",
				state->regs->white_balance,
				ARRAY_SIZE(state->regs->white_balance));
	err |= s5k4ecgx_set_parameter(sd, &parms->fps,
				new_parms->fps, "fps",
				state->regs->fps,
				ARRAY_SIZE(state->regs->fps));

	if (parms->scene_mode == SCENE_MODE_NIGHTSHOT)
		state->one_frame_delay_ms = NIGHT_MODE_MAX_ONE_FRAME_DELAY_MS;
	else
		state->one_frame_delay_ms = NORMAL_MODE_MAX_ONE_FRAME_DELAY_MS;

	dev_dbg(&client->dev, "%s: returning %d\n", __func__, err);
	return err;
}

int s5k4ecgx_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	dev_info(&client->dev, "%s: stream %s\n", __func__,
			enable ? "on" : "off");

	/* do nothing here! cause of the sensor streaming on automatically
	 * after finish it's init sequence and can off by power down
	 */

	return 0;
}
/* This function is called from the g_ctrl api
 *
 * This function should be called only after the s_fmt call,
 * which sets the required width/height value.
 *
 * It checks a list of available frame sizes and sets the
 * most appropriate frame size.
 *
 * The list is stored in an increasing order (as far as possible).
 * Hence the first entry (searching from the beginning) where both the
 * width and height is more than the required value is returned.
 * In case of no perfect match, we set the last entry (which is supposed
 * to be the largest resolution supported.)
 */
static void s5k4ecgx_set_framesize(struct v4l2_subdev *sd,
				const struct s5k4ecgx_framesize *frmsize,
				int frmsize_count, bool preview)
{
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct s5k4ecgx_framesize *last_frmsize =
		&frmsize[frmsize_count - 1];

	dev_dbg(&client->dev, "%s: Requested Res: %dx%d\n", __func__,
		state->pix.width, state->pix.height);

	do {
		/*
		 * In case of image capture mode,
		 * if the given image resolution is not supported,
		 * return the next higher image resolution. */
		if (preview) {
			if (frmsize->width == state->pix.width &&
				frmsize->height == state->pix.height) {
				break;
			}
		} else {
			dev_dbg(&client->dev,
				"%s: compare frmsize %dx%d to %dx%d\n",
				__func__,
				frmsize->width, frmsize->height,
				state->pix.width, state->pix.height);
			if (frmsize->width >= state->pix.width &&
				frmsize->height >= state->pix.height) {
				dev_dbg(&client->dev,
					"%s: select frmsize %dx%d, index=%d\n",
					__func__,
					frmsize->width, frmsize->height,
					frmsize->index);
				break;
			}
		}

		frmsize++;
	} while (frmsize <= last_frmsize);

	if (frmsize > last_frmsize)
		frmsize = last_frmsize;

	state->pix.width = frmsize->width;
	state->pix.height = frmsize->height;
	if (preview) {
		state->preview_framesize_index = frmsize->index;
		dev_dbg(&client->dev, "%s: Preview Res Set: %dx%d, index %d\n",
			__func__, state->pix.width, state->pix.height,
			state->preview_framesize_index);
	} else {
		state->capture_framesize_index = frmsize->index;
		dev_dbg(&client->dev, "%s: Capture Res Set: %dx%d, index %d\n",
			__func__, state->pix.width, state->pix.height,
			state->capture_framesize_index);
	}
}

static int s5k4ecgx_check_dataline_stop(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	int err;

	dev_dbg(&client->dev, "%s\n", __func__);

	err = s5k4ecgx_write_regset(sd, &state->regs->dtp_stop);
	if (err < 0) {
		v4l_info(client, "%s: register set failed\n", __func__);
		return -EIO;
	}

	state->check_dataline = 0;

	return err;
}

static void s5k4ecgx_get_esd_int(struct v4l2_subdev *sd,
				struct v4l2_control *ctrl)
{
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 read_value;
	int err;

	if ((S5K4ECGX_RUNMODE_RUNNING == state->runmode) &&
		(state->af_status != AF_START)) {
		err = s5k4ecgx_write_regset(sd,
				&state->regs->get_esd_status);
		err |= s5k4ecgx_reg_read_16(client, &read_value);
		dev_dbg(&client->dev,
			"%s: read_value == 0x%x\n", __func__, read_value);

		if (err < 0) {
			v4l_info(client,
				"Failed I2C for getting ESD information\n");
			ctrl->value = 0x01;
		} else {
			if (read_value != 0x0000) {
				v4l_info(client, "ESD interrupt happened!!\n");
				ctrl->value = 0x01;
			} else {
				dev_dbg(&client->dev,
					"%s: No ESD interrupt!!\n", __func__);
				ctrl->value = 0x00;
			}
		}
	} else
		ctrl->value = 0x00;
}

/* returns the real iso currently used by sensor due to lighting
 * conditions, not the requested iso we sent using s_ctrl.
 */
static int s5k4ecgx_get_iso(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	u16 read_value1 = 0;
	u16 read_value2 = 0;
	int read_value;

	err = s5k4ecgx_write_regset(sd, &state->regs->get_iso);
	err |= s5k4ecgx_reg_read_16(client, &read_value1);
	err |= s5k4ecgx_reg_read_16(client, &read_value2);

	read_value = read_value1 * read_value2 / 384;

	if (read_value > 0x400)
		ctrl->value = ISO_400;
	else if (read_value > 0x200)
		ctrl->value = ISO_200;
	else if (read_value > 0x100)
		ctrl->value = ISO_100;
	else
		ctrl->value = ISO_50;

	dev_dbg(&client->dev, "%s: get iso == %d (0x%x, 0x%x)\n",
		__func__, ctrl->value, read_value1, read_value2);

	return err;
}

static int s5k4ecgx_get_shutterspeed(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	u32 read_value;

	err = s5k4ecgx_write_regset(sd, &state->regs->get_shutterspeed);
	err |= s5k4ecgx_reg_read_32(client, &read_value);

	ctrl->value = read_value * 1000 / 400;
	dev_dbg(&client->dev,
			"%s: get shutterspeed == %d\n", __func__, ctrl->value);

	return err;
}

static int s5k4ecgx_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct sec_cam_parm *parms =
		(struct sec_cam_parm *)&state->strm.parm.raw_data;
	int err = 0;

	if (!state->initialized) {
		dev_err(&client->dev,
			"%s: return error because uninitialized\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&state->ctrl_lock);

	switch (ctrl->id) {
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		ctrl->value = parms->white_balance;
		break;
	case V4L2_CID_CAMERA_EFFECT:
		ctrl->value = parms->effects;
		break;
	case V4L2_CID_CAMERA_CONTRAST:
		ctrl->value = parms->contrast;
		break;
	case V4L2_CID_CAMERA_SATURATION:
		ctrl->value = parms->saturation;
		break;
	case V4L2_CID_CAMERA_SHARPNESS:
		ctrl->value = parms->sharpness;
		break;
	case V4L2_CID_CAM_JPEG_MAIN_SIZE:
		ctrl->value = state->jpeg.main_size;
		break;
	case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
		ctrl->value = state->jpeg.main_offset;
		break;
	case V4L2_CID_CAM_JPEG_THUMB_SIZE:
		ctrl->value = state->jpeg.thumb_size;
		break;
	case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
		ctrl->value = state->jpeg.thumb_offset;
		break;
	case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
		ctrl->value = state->jpeg.postview_offset;
		break;
	case V4L2_CID_CAM_JPEG_MEMSIZE:
		ctrl->value = SENSOR_JPEG_SNAPSHOT_MEMSIZE;
		break;
	case V4L2_CID_CAM_JPEG_QUALITY:
		ctrl->value = state->jpeg.quality;
		break;
#if 0
	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT_FIRST:
		err = s5k4ecgx_get_auto_focus_result_first(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT_SECOND:
		err = s5k4ecgx_get_auto_focus_result_second(sd, ctrl);
		break;
#endif
	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
		err = s5k4ecgx_get_auto_focus_result_first(sd, ctrl);
		if (err) {
			break;
		}
		err = s5k4ecgx_get_auto_focus_result_second(sd, ctrl);
		break;
	case V4L2_CID_CAM_DATE_INFO_YEAR:
		ctrl->value = 2010;
		break;
	case V4L2_CID_CAM_DATE_INFO_MONTH:
		ctrl->value = 2;
		break;
	case V4L2_CID_CAM_DATE_INFO_DATE:
		ctrl->value = 25;
		break;
	case V4L2_CID_CAM_SENSOR_VER:
		ctrl->value = 1;
		break;
	case V4L2_CID_CAM_FW_MINOR_VER:
		ctrl->value = state->fw.minor;
		break;
	case V4L2_CID_CAM_FW_MAJOR_VER:
		ctrl->value = state->fw.major;
		break;
	case V4L2_CID_CAM_PRM_MINOR_VER:
		ctrl->value = state->prm.minor;
		break;
	case V4L2_CID_CAM_PRM_MAJOR_VER:
		ctrl->value = state->prm.major;
		break;
#if 0
	case V4L2_CID_ESD_INT:
		s5k4ecgx_get_esd_int(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_GET_ISO:
		err = s5k4ecgx_get_iso(sd, ctrl);
		break;
	case V4L2_CID_CAMERA_GET_SHT_TIME:
		err = s5k4ecgx_get_shutterspeed(sd, ctrl);
		break;
#ifdef CONFIG_VIDEO_S5K4ECGX_FLASHLIGHT
	case V4L2_CID_CAMERA_GET_FLASH_ONOFF:
		ctrl->value = state->flash_state_on_previous_capture;
		break;
#endif
	case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
	case V4L2_CID_CAMERA_SMART_AUTO_STATUS:
		break;
#endif
	default:
		err = -ENOIOCTLCMD;
		dev_err(&client->dev, "%s: unknown ctrl id 0x%x\n",
			__func__, ctrl->id);
		break;
	}

	mutex_unlock(&state->ctrl_lock);

	return err;
}

static int s5k4ecgx_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	struct sec_cam_parm *parms =
		(struct sec_cam_parm *)&state->strm.parm.raw_data;
	int err = 0;
	int value = ctrl->value;

	if (!state->initialized &&
		(ctrl->id != V4L2_CID_CAMERA_CHECK_DATALINE)) {
		dev_err(&client->dev,
			"%s: return error because uninitialized\n", __func__);
		return -ENODEV;
	}

	dev_dbg(&client->dev, "%s: V4l2 control ID =%d, val = %d\n",
		__func__, ctrl->id - V4L2_CID_PRIVATE_BASE, value);

	mutex_lock(&state->ctrl_lock);

	switch (ctrl->id) {
#ifdef CONFIG_VIDEO_S5K4ECGX_FLASHLIGHT
	case V4L2_CID_CAMERA_FLASH_MODE:
		err = s5k4ecgx_set_flash_mode(sd, value);
		break;
#endif
	case V4L2_CID_CAMERA_BRIGHTNESS:
		if (state->runmode == S5K4ECGX_RUNMODE_RUNNING) {
			err = s5k4ecgx_set_parameter(sd, &parms->brightness,
						value - EV_MINUS_4, "brightness",
						state->regs->ev,
						ARRAY_SIZE(state->regs->ev));
		} else {
			dev_err(&client->dev,
				"%s: trying to set brightness when not "
				"in preview mode\n",
				__func__);
			err = -EINVAL;
		}
		break;
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		if (state->runmode == S5K4ECGX_RUNMODE_RUNNING) {
			err = s5k4ecgx_set_parameter(sd, &parms->white_balance,
					value, "white balance",
					state->regs->white_balance,
					ARRAY_SIZE(state->regs->white_balance));
		} else {
			dev_err(&client->dev,
				"%s: trying to set white balance when not "
				"in preview mode\n",
				__func__);
			err = -EINVAL;
		}
		break;
	case V4L2_CID_CAMERA_EFFECT:
		if (state->runmode == S5K4ECGX_RUNMODE_RUNNING) {
			err = s5k4ecgx_set_parameter(sd, &parms->effects,
					value, "effects", state->regs->effect,
					ARRAY_SIZE(state->regs->effect));
		} else {
			dev_err(&client->dev,
				"%s: trying to set effect when not "
				"in preview mode\n",
				__func__);
			err = -EINVAL;
		}
		break;
	case V4L2_CID_CAMERA_ISO:
		if (state->runmode == S5K4ECGX_RUNMODE_RUNNING) {
			err = s5k4ecgx_set_parameter(sd, &parms->iso,
						value, "iso",
						state->regs->iso,
						ARRAY_SIZE(state->regs->iso));
		} else {
			dev_err(&client->dev,
				"%s: trying to set iso when not "
				"in preview mode\n",
				__func__);
			err = -EINVAL;
		}
		break;
	case V4L2_CID_CAMERA_METERING:
		if (state->runmode == S5K4ECGX_RUNMODE_RUNNING) {
			err = s5k4ecgx_set_parameter(sd, &parms->metering,
					value, "metering",
					state->regs->metering,
					ARRAY_SIZE(state->regs->metering));
		} else {
			dev_err(&client->dev,
				"%s: trying to set metering when not "
				"in preview mode\n",
				__func__);
			err = -EINVAL;
		}
		break;
	case V4L2_CID_CAMERA_CONTRAST:
		err = s5k4ecgx_set_parameter(sd, &parms->contrast,
					value, "contrast",
					state->regs->contrast,
					ARRAY_SIZE(state->regs->contrast));
		break;
	case V4L2_CID_CAMERA_SATURATION:
		err = s5k4ecgx_set_parameter(sd, &parms->saturation,
					value, "saturation",
					state->regs->saturation,
					ARRAY_SIZE(state->regs->saturation));
		break;
	case V4L2_CID_CAMERA_SHARPNESS:
		err = s5k4ecgx_set_parameter(sd, &parms->sharpness,
					value, "sharpness",
					state->regs->sharpness,
					ARRAY_SIZE(state->regs->sharpness));
		break;
	case V4L2_CID_CAMERA_WDR:
		err = s5k4ecgx_set_wdr(sd, value);
		break;
	case V4L2_CID_CAMERA_FACE_DETECTION:
		err = s5k4ecgx_set_face_detection(sd, value);
		break;
	case V4L2_CID_CAMERA_FOCUS_MODE:
		err = s5k4ecgx_set_focus_mode(sd, value);
		break;
	case V4L2_CID_CAM_JPEG_QUALITY:
		if (state->runmode == S5K4ECGX_RUNMODE_RUNNING) {
			state->jpeg.quality = value;
			err = s5k4ecgx_set_jpeg_quality(sd);
		} else {
			dev_err(&client->dev,
				"%s: trying to set jpeg quality when not "
				"in preview mode\n",
				__func__);
			err = -EINVAL;
		}
		break;
	case V4L2_CID_CAMERA_SCENE_MODE:
		err = s5k4ecgx_set_parameter(sd, &parms->scene_mode,
					SCENE_MODE_NONE, "scene_mode",
					state->regs->scene_mode,
					ARRAY_SIZE(state->regs->scene_mode));
		if (err < 0) {
			dev_err(&client->dev,
				"%s: failed to set scene-mode default value\n",
				__func__);
			break;
		}
		if (value != SCENE_MODE_NONE) {
			err = s5k4ecgx_set_parameter(sd, &parms->scene_mode,
					value, "scene_mode",
					state->regs->scene_mode,
					ARRAY_SIZE(state->regs->scene_mode));
		}
		if (parms->scene_mode == SCENE_MODE_NIGHTSHOT) {
			state->one_frame_delay_ms =
				NIGHT_MODE_MAX_ONE_FRAME_DELAY_MS;
		} else {
			state->one_frame_delay_ms =
				NORMAL_MODE_MAX_ONE_FRAME_DELAY_MS;
		}

		break;
	case V4L2_CID_CAMERA_GPS_LATITUDE:
		dev_err(&client->dev,
			"%s: V4L2_CID_CAMERA_GPS_LATITUDE: not implemented\n",
			__func__);
		break;
	case V4L2_CID_CAMERA_GPS_LONGITUDE:
		dev_err(&client->dev,
			"%s: V4L2_CID_CAMERA_GPS_LONGITUDE: not implemented\n",
			__func__);
		break;
	case V4L2_CID_CAMERA_GPS_TIMESTAMP:
		dev_err(&client->dev,
			"%s: V4L2_CID_CAMERA_GPS_TIMESTAMP: not implemented\n",
			__func__);
		break;
	case V4L2_CID_CAMERA_GPS_ALTITUDE:
		dev_err(&client->dev,
			"%s: V4L2_CID_CAMERA_GPS_ALTITUDE: not implemented\n",
			__func__);
		break;
	case V4L2_CID_CAMERA_OBJECT_POSITION_X:
		state->position.x = value;
		break;
	case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
		state->position.y = value;
		break;
	case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
		if (value == AUTO_FOCUS_ON)
			err = s5k4ecgx_start_auto_focus(sd);
		else if (value == AUTO_FOCUS_OFF)
			err = s5k4ecgx_stop_auto_focus(sd);
		else {
			err = -EINVAL;
			dev_err(&client->dev,
				"%s: bad focus value requestion %d\n",
				__func__, value);
		}
		break;
	case V4L2_CID_CAMERA_FRAME_RATE:
		dev_dbg(&client->dev,
			"%s: camera frame rate request for %d fps\n",
			__func__, value);
		err = s5k4ecgx_set_parameter(sd, &parms->fps,
					value, "fps",
					state->regs->fps,
					ARRAY_SIZE(state->regs->fps));
		break;
	case V4L2_CID_CAM_CAPTURE:
		err = s5k4ecgx_start_capture(sd);
		break;

	/* Used to start / stop preview operation.
	 * This call can be modified to START/STOP operation,
	 * which can be used in image capture also
	 */
	case V4L2_CID_CAM_PREVIEW_ONOFF:
		if (value)
			err = s5k4ecgx_set_preview_start(sd);
		else
			err = s5k4ecgx_set_preview_stop(sd);
		break;
	case V4L2_CID_CAMERA_CHECK_DATALINE:
		dev_dbg(&client->dev, "%s: check_dataline set to %d\n",
			__func__, value);
		state->check_dataline = value;
		break;
	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
		err = s5k4ecgx_check_dataline_stop(sd);
		break;
#if 0
	case V4L2_CID_CAMERA_RETURN_FOCUS:
		if (parms->focus_mode != FOCUS_MODE_MACRO)
			err = s5k4ecgx_return_focus(sd);
		break;
	case V4L2_CID_CAMERA_FINISH_AUTO_FOCUS:
		err = s5k4ecgx_finish_auto_focus(sd);
		break;
#endif
	default:
		dev_err(&client->dev, "%s: unknown set ctrl id 0x%x\n",
			__func__, ctrl->id);
		err = -ENOIOCTLCMD;
		break;
	}

	if (err < 0)
		dev_err(&client->dev, "%s: videoc_s_ctrl failed %d\n", __func__,
			err);

	mutex_unlock(&state->ctrl_lock);

	dev_dbg(&client->dev, "%s: videoc_s_ctrl returning %d\n",
		__func__, err);

	return err;
}

static int s5k4ecgx_s_ext_ctrl(struct v4l2_subdev *sd,
			      struct v4l2_ext_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	int err = 0;
	struct gps_info_common *tempGPSType = NULL;

	switch (ctrl->id) {

	case V4L2_CID_CAMERA_GPS_LATITUDE:
		tempGPSType = (struct gps_info_common *)ctrl->reserved2[1];
		state->gps_info.gps_buf[0] = tempGPSType->direction;
		state->gps_info.gps_buf[1] = tempGPSType->dgree;
		state->gps_info.gps_buf[2] = tempGPSType->minute;
		state->gps_info.gps_buf[3] = tempGPSType->second;
		break;
	case V4L2_CID_CAMERA_GPS_LONGITUDE:
		tempGPSType = (struct gps_info_common *)ctrl->reserved2[1];
		state->gps_info.gps_buf[4] = tempGPSType->direction;
		state->gps_info.gps_buf[5] = tempGPSType->dgree;
		state->gps_info.gps_buf[6] = tempGPSType->minute;
		state->gps_info.gps_buf[7] = tempGPSType->second;
		break;
	case V4L2_CID_CAMERA_GPS_ALTITUDE:
		tempGPSType = (struct gps_info_common *)ctrl->reserved2[1];
		state->gps_info.altitude_buf[0] = tempGPSType->direction;
		state->gps_info.altitude_buf[1] =
					(tempGPSType->dgree) & 0x00ff;
		state->gps_info.altitude_buf[2] =
					((tempGPSType->dgree) & 0xff00) >> 8;
		state->gps_info.altitude_buf[3] = tempGPSType->minute;
		break;
	case V4L2_CID_CAMERA_GPS_TIMESTAMP:
		state->gps_info.gps_timeStamp = *((int *)ctrl->reserved2[1]);
		err = 0;
		break;
	default:
		dev_err(&client->dev, "%s: unknown ctrl->id %d\n",
			__func__, ctrl->id);
		err = -ENOIOCTLCMD;
		break;
	}

	if (err < 0)
		dev_err(&client->dev, "%s: vidioc_s_ext_ctrl failed %d\n",
			__func__, err);

	return err;
}

static int s5k4ecgx_s_ext_ctrls(struct v4l2_subdev *sd,
				struct v4l2_ext_controls *ctrls)
{
	struct v4l2_ext_control *ctrl = ctrls->controls;
	int ret = 0;
	int i;

	for (i = 0; i < ctrls->count; i++, ctrl++) {
		ret = s5k4ecgx_s_ext_ctrl(sd, ctrl);

		if (ret) {
			ctrls->error_idx = i;
			break;
		}
	}

	return ret;
}

static int s5k4ecgx_init_regs(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);
	u16 read_value;

	/* we'd prefer to do this in probe, but the framework hasn't
	 * turned on the camera yet so our i2c operations would fail
	 * if we tried to do it in probe, so we have to do it here
	 * and keep track if we succeeded or not.
	 */
	s5k4ecgx_request_reg_read(sd, 0x700001A6);
	s5k4ecgx_reg_read_16(client, &read_value);

	pr_info("%s : revision %08X\n", __func__, read_value);

#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_0
	if (read_value == S5K4ECGX_VERSION_1_0) {
		state->regs = &regs_for_fw_version_1_0;
		state->initialized = true;
		return 0;
	}
#endif
#ifdef CONFIG_VIDEO_S5K4ECGX_V_1_1
	if (read_value == S5K4ECGX_VERSION_1_1) {
		state->fw.minor = 1;
		state->regs = &regs_for_fw_version_1_1;
		state->initialized = true;
		return 0;
	}
#endif

	dev_err(&client->dev, "%s: unknown fw version 0x%x\n",
		__func__, read_value);
	return -ENODEV;
}

static int s5k4ecgx_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);

	dev_dbg(&client->dev, "%s: start\n", __func__);

	s5k4ecgx_init_parameters(sd);

	if (s5k4ecgx_init_regs(&state->sd) < 0)
		return -ENODEV;

	dev_dbg(&client->dev, "%s: state->check_dataline : %d\n",
		__func__, state->check_dataline);

	if (s5k4ecgx_write_regset(sd, &state->regs->init_reg) < 0)
		return -EIO;

	if (s5k4ecgx_write_regset(sd, &state->regs->flash_init) < 0)
		return -EIO;

	if (state->check_dataline
		&& s5k4ecgx_write_regset(sd, &state->regs->dtp_start) < 0)
		return -EIO;

	dev_dbg(&client->dev, "%s: end\n", __func__);

	return 0;
}

static const struct v4l2_subdev_core_ops s5k4ecgx_core_ops = {
	.init = s5k4ecgx_init,	/* initializing API */
	.g_ctrl = s5k4ecgx_g_ctrl,
	.s_ctrl = s5k4ecgx_s_ctrl,
	.s_ext_ctrls = s5k4ecgx_s_ext_ctrls,
};

static const struct v4l2_subdev_video_ops s5k4ecgx_video_ops = {
	.s_mbus_fmt = s5k4ecgx_s_mbus_fmt,
	.enum_framesizes = s5k4ecgx_enum_framesizes,
	.enum_mbus_fmt = s5k4ecgx_enum_mbus_fmt,
	.try_mbus_fmt = s5k4ecgx_try_mbus_fmt,
	.g_parm = s5k4ecgx_g_parm,
	.s_parm = s5k4ecgx_s_parm,
	.s_stream = s5k4ecgx_s_stream,
};

static const struct v4l2_subdev_ops s5k4ecgx_ops = {
	.core = &s5k4ecgx_core_ops,
	.video = &s5k4ecgx_video_ops,
};


/*
 * s5k4ecgx_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int s5k4ecgx_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct s5k4ecgx_state *state;
	struct s5k4ecgx_platform_data *pdata = client->dev.platform_data;

#ifdef CONFIG_VIDEO_S5K4ECGX_FLASHLIGHT
	if ((pdata == NULL) || (pdata->flash_onoff == NULL)) {
		dev_err(&client->dev, "%s: bad platform data\n", __func__);
		return -ENODEV;
	}
#endif

	state = kzalloc(sizeof(struct s5k4ecgx_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	mutex_init(&state->ctrl_lock);

	state->runmode = S5K4ECGX_RUNMODE_NOTREADY;
	sd = &state->sd;
	strcpy(sd->name, S5K4ECGX_DRIVER_NAME);

	/*
	 * Assign default format and resolution
	 * Use configured default information in platform data
	 * or without them, use default information in driver
	 */
	if (pdata) {
		state->pix.width = pdata->default_width;
		state->pix.height = pdata->default_height;

		if (!pdata->pixelformat)
			state->pix.pixelformat = DEFAULT_PIX_FMT;
		else
			state->pix.pixelformat = pdata->pixelformat;

		if (!pdata->freq)
			state->freq = DEFAULT_MCLK;	/* 24MHz default */
		else
			state->freq = pdata->freq;
	} else {
		state->pix.width = 640;
		state->pix.height = 480;
		state->pix.pixelformat = DEFAULT_PIX_FMT;
		state->freq = DEFAULT_MCLK;	/* 24MHz default */
	}

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &s5k4ecgx_ops);

	dev_dbg(&client->dev, "5MP camera S5K4ECGX loaded.\n");

	return 0;
}

static int s5k4ecgx_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k4ecgx_state *state =
		container_of(sd, struct s5k4ecgx_state, sd);

	v4l2_device_unregister_subdev(sd);
	mutex_destroy(&state->ctrl_lock);
	kfree(state);

	dev_dbg(&client->dev, "Unloaded camera sensor S5K4ECGX.\n");

	return 0;
}

static const struct i2c_device_id s5k4ecgx_id[] = {
	{ S5K4ECGX_DRIVER_NAME, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, s5k4ecgx_id);

static struct i2c_driver v4l2_i2c_driver = {
	.driver.name = S5K4ECGX_DRIVER_NAME,
	.probe = s5k4ecgx_probe,
	.remove = s5k4ecgx_remove,
	.id_table = s5k4ecgx_id,
};

static int __init v4l2_i2c_drv_init(void)
{
	return i2c_add_driver(&v4l2_i2c_driver);
}

static void __exit v4l2_i2c_drv_cleanup(void)
{
	i2c_del_driver(&v4l2_i2c_driver);
}

module_init(v4l2_i2c_drv_init);
module_exit(v4l2_i2c_drv_cleanup);

MODULE_DESCRIPTION("LSI S5K4ECGX 5MP SOC camera driver");
MODULE_AUTHOR("Seok-Young Jang <quartz.jang@samsung.com>");
MODULE_LICENSE("GPL");

