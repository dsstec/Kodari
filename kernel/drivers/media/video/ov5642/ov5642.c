/* linux/drivers/media/video/ov5642.c
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

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
//#include <media/v4l2-i2c-drv.h>
#include <media/ov5642_platform.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include "ov5642.h"
#ifdef CONFIG_VIDEO_OV5640_AUTO_FOCUS
#include "ov5640_af_firmware.h"
#else
#include "ov5642_af_firmware.h"
#endif
#include "ov5642_set_640_480.h"
#include "ov5642_set_1280_720.h"
#include "ov5642_set_1920_1080.h"
#include "ov5642_set_2592_1936.h"

#define OV5642_DRIVER_NAME	"OV5642"

/* Default resolution & pixelformat. plz ref ov5642_platform.h */
#define DEFAULT_RES		WVGA	/* Index of resoultion */
#define DEFAULT_FPS_INDEX	OV5642_15FPS
#define DEFAULT_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */

#define MIN_FPS			15
#define MAX_FPS			30
#define DEFAULT_FPS		30

#define CHIPID_HI               0x300a
#define CHIPID_LO               0x300b

/*
 * Specification
 * Parallel : ITU-R. 656/601 YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Serial : MIPI CSI2 (single lane) YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Resolution : 1280 (H) x 1024 (V)
 * Image control : Brightness, Contrast, Saturation, Sharpness, Glamour
 * Effect : Mono, Negative, Sepia, Aqua, Sketch
 * FPS : 15fps @full resolution, 30fps @VGA, 24fps @720p
 * Max. pixel clock frequency : 48MHz(upto)
 * Internal PLL (6MHz to 27MHz input frequency)
 */

static bool bInitialized = false;

static int ov5642_init(struct v4l2_subdev *sd, u32 val);
static int ov5642_set_mode(struct v4l2_subdev *sd, int mode);
static void ov5642_set_focus(struct v4l2_subdev *sd);
#ifdef CONFIG_VIDEO_OV5640_AUTO_FOCUS
static int ov5640_set_af_firmware(struct v4l2_subdev *sd);
static void ov5640_single_af(struct v4l2_subdev *sd);

#define OV5640_CMD_MAIN 0x3022
#define OV5640_CMD_TAG 0x3023
#define OV5640_STS_FOCUS 0x3029
#endif


#define SIN_STEP 5
static const int ov5642_sin_table[] = {
	   0,	 87,   173,   258,   342,   422,
	 499,	573,   642,   707,   766,   819,
	 866,	906,   939,   965,   984,   996,
	1000
};

static int ov5642_sin(int theta)
{
	int chs = 1;
	int sin;

	if (theta < 0) {
		theta = -theta;
		chs = -1;
	}
	if (theta <= 90)
		sin = ov5642_sin_table[theta / SIN_STEP];
	else {
		theta -= 90;
		sin = 1000 - ov5642_sin_table[theta / SIN_STEP];
	}
	return sin * chs;
}

static int ov5642_cosine(int theta)
{
	theta = 90 - theta;
	if (theta > 180)
		theta -= 360;
	else if (theta < -180)
		theta += 360;
	return ov5642_sin(theta);
}


/* Camera functional setting values configured by user concept */
struct ov5642_userset {
	signed int exposure_bias;	/* V4L2_CID_EXPOSURE */
	unsigned int ae_lock;
	unsigned int awb_lock;
	unsigned int auto_wb;	/* V4L2_CID_CAMERA_WHITE_BALANCE */
	unsigned int manual_wb;	/* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int wb_temp;	/* V4L2_CID_WHITE_BALANCE_TEMPERATURE */
	unsigned int effect;	/* Color FX (AKA Color tone) */
	unsigned int contrast;	/* V4L2_CID_CAMERA_CONTRAST */
	unsigned int saturation;	/* V4L2_CID_CAMERA_SATURATION */
	unsigned int sharpness;	/* V4L2_CID_CAMERA_SHARPNESS */
	unsigned int glamour;
};
#if 0
struct ov5642_state {
	struct ov5642_mbus_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_mbus_framefmt 	fmt;
	struct v4l2_fract 		timeperframe;
	struct ov5642_userset 		userset;
	int 				freq;	/* MCLK in KHz */
	int 				is_mipi;
	int 				isize;
	int 				ver;
	int 				fps;
	int 				fmt_index;
	unsigned short 			devid_mask;	
	int check_previewdata;
};
#endif
struct ov5642_framesize {
		    u32 width;
			    u32 height;
};


struct ov5642_state {
	struct ov5642_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_mbus_framefmt 	fmt;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct ov5642_userset userset;
	struct ov5642_framesize p_sizes;
	struct ov5642_framesize c_sizes;
	int is_af_fw_loaded;
	int flag_p_load;
	int framesize_index;
	int freq;					/* MCLK in KHz */
	int is_mipi;
	int isize;
	int ver;
	int fps;
	int is_initialized;
};


enum {
	OV5642_PREVIEW_VGA,
	//OV5642_PREVIEW_SVGA,
};

struct ov5642_enum_framesize {
	unsigned int index;
	unsigned int width;
	unsigned int height;
};

struct ov5642_enum_framesize ov5642_framesize_list[] = {
	{ OV5642_PREVIEW_VGA,  640, 480 },
	//{ OV5642_PREVIEW_SVGA, 800, 600 }
};

static inline struct ov5642_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov5642_state, sd);
}


static int ov5642_reset(struct v4l2_subdev *sd)
{
//	ov5642_init(sd, 0);
	mdelay(500);
	return 0;
}

/*
 * OV5642 register structure : 2bytes address, 2bytes value
 * retry on write failure up-to 5 times
 */
static inline int ov5642_write(struct v4l2_subdev *sd, u8 addr, u8 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg[1];
	unsigned char reg[2];
	int err = 0;
	int retry = 0;


	if (!client->adapter)
		return -ENODEV;

again:
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = reg;

	reg[0] = addr & 0xff;
	reg[1] = val & 0xff;

	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return err;	/* Returns here on success */

	/* abnormal case: retry 5 times */
	if (retry < 5) {
		dev_err(&client->dev, "%s: address: 0x%02x%02x, "
			"value: 0x%02x%02x\n", __func__,
			reg[0], reg[1], reg[2], reg[3]);
		retry++;
		goto again;
	}

	return err;
}

/*
 * OV5642 register structure : 2bytes address, 2bytes value
 * retry on write failure up-to 5 times
 */
static s32 ov5642_read_reg(struct i2c_client *i2c, u16 reg, u8 *val)
{
	u8 au8RegBuf[2] = {0};
	u8 u8RdVal = 0;

	au8RegBuf[0] = reg >> 8;
	au8RegBuf[1] = reg & 0xff;

	if(2 != i2c_master_send(i2c, au8RegBuf, 2)) {
		printk("%s: read reg error: reg=%x\n", __func__, reg);
		//return -1;
	}

	if(1 != i2c_master_recv(i2c, &u8RdVal, 1)) {
		printk("%s: read reg error: reg=%x, val = %x\n", __func__, reg, u8RdVal);
		//return -1;
	}
	*val = u8RdVal;
	return u8RdVal;
}

static s32 ov5642_write_reg(struct i2c_client *i2c, u16 reg, u8 val)
{
	u8 readback;
	u8 au8Buf[3] = {0};

	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xff;
	au8Buf[2] = val;

	if(i2c_master_send(i2c, au8Buf, 3) < 0) {
		printk("%s: write reg error: reg=%x, val=%x\n", __func__, reg, val);
		return -1;
	}

	if( ov5642_read_reg(i2c, reg, &readback) < 0) {
		printk("%s: readback err\n", __func__);
	}
	else if(readback != val) {
		printk("%s: readback mismatch: reg %04x %02x != %02x\n", __func__,
		reg, val, readback);
	}
	return 0;
}

static int ov5642_i2c_write(struct v4l2_subdev *sd, unsigned char i2c_data[],
				unsigned char length)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[length], i;
	struct i2c_msg msg = {client->addr, 0, length, buf};

	for (i = 0; i < length; i++)
		buf[i] = i2c_data[i];
	return i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
}

static int ov5642_write_regs(struct v4l2_subdev *sd, unsigned char regs[],
				int size)
{

	return 0;
}


static const char *ov5642_querymenu_wb_preset[] = {
	"WB Tungsten", "WB Fluorescent", "WB sunny", "WB cloudy", NULL
};

static const char *ov5642_querymenu_effect_mode[] = {
	"Effect Sepia", "Effect Aqua", "Effect Monochrome",
	"Effect Negative", "Effect Sketch", NULL
};

static const char *ov5642_querymenu_ev_bias_mode[] = {
	"-3EV",	"-2,1/2EV", "-2EV", "-1,1/2EV",
	"-1EV", "-1/2EV", "0", "1/2EV",
	"1EV", "1,1/2EV", "2EV", "2,1/2EV",
	"3EV", NULL
};

static struct v4l2_queryctrl ov5642_controls[] = {
	{
		/*
		 * For now, we just support in preset type
		 * to be close to generic WB system,
		 * we define color temp range for each preset
		 */
		.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "White balance in kelvin",
		.minimum = 0,
		.maximum = 10000,
		.step = 1,
		.default_value = 0,	/* FIXME */
	},
	{
		.id = V4L2_CID_WHITE_BALANCE_PRESET,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "White balance preset",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov5642_querymenu_wb_preset) - 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CAMERA_WHITE_BALANCE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Auto white balance",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_EXPOSURE,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Exposure bias",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov5642_querymenu_ev_bias_mode) - 2,
		.step = 1,
		.default_value =
			(ARRAY_SIZE(ov5642_querymenu_ev_bias_mode) - 2) / 2,
			/* 0 EV */
	},
	{
		.id = V4L2_CID_CAMERA_EFFECT,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Image Effect",
		.minimum = 0,
		.maximum = ARRAY_SIZE(ov5642_querymenu_effect_mode) - 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CAMERA_CONTRAST,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Contrast",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
	{
		.id = V4L2_CID_CAMERA_SATURATION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Saturation",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
	{
		.id = V4L2_CID_CAMERA_SHARPNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Sharpness",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
};

const char * const *ov5642_ctrl_get_menu(u32 id)
{
	switch (id) {
	case V4L2_CID_WHITE_BALANCE_PRESET:
		return ov5642_querymenu_wb_preset;

	case V4L2_CID_CAMERA_EFFECT:
		return ov5642_querymenu_effect_mode;

	case V4L2_CID_EXPOSURE:
		return ov5642_querymenu_ev_bias_mode;

	default:
		return v4l2_ctrl_get_menu(id);
	}
}

static inline struct v4l2_queryctrl const *ov5642_find_qctrl(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ov5642_controls); i++)
		if (ov5642_controls[i].id == id)
			return &ov5642_controls[i];

	return NULL;
}

static int ov5642_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ov5642_controls); i++) {
		if (ov5642_controls[i].id == qc->id) {
			memcpy(qc, &ov5642_controls[i],
				sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}

	return -EINVAL;
}

static int ov5642_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	struct v4l2_queryctrl qctrl;

	qctrl.id = qm->id;
	ov5642_queryctrl(sd, &qctrl);

	return v4l2_ctrl_query_menu(qm, &qctrl, ov5642_ctrl_get_menu(qm->id));
}

static int ov5642_s_crystal_freq(struct v4l2_subdev *sd, u32 freq, u32 flags)
{
	int err = -EINVAL;

	return err;
}

static int ov5642_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
#if 0
	struct ov5642_state *state = to_state(sd);
	struct ov5642_mbus_platform_data *pdata = state->pdata;
	int err = 0;
	*fmt = pdata->fmt;
	return err;
#endif
	return 0;
}

static int ov5642_preview_start(struct v4l2_subdev *sd)
{
	struct ov5642_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	msleep(100);
	return 0;
}

static int ov5642_capture_start(struct v4l2_subdev *sd)
{
	struct ov5642_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	msleep(100);
	return 0;
//	if(state->c_sizes.width==640 && state->c_sizes.height==480){
//		;
//	}
//	else if(state->c_sizes.width==2560 && state->c_sizes.height==1920) {
//		ov5642_set_registers(client, ov5642_init_2560_1920);
//		state->flag_p_load = 0;
//		msleep(2300);
//	}
//	else
//		return -1;
//	ov5642_set_command(client, SET_AUTO_FOCUS);
//	return 0;
}

#ifdef CONFIG_VIDEO_OV5640_AUTO_FOCUS
static void ov5640_single_af(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
#if 1
	printk("%s: start camera auto-focus\n", __func__);

	ov5642_write_reg(client,OV5640_CMD_MAIN, 0x03);  //single focus command
#else
	u16 af_done_times = 0;
	u8 af_done = 1;

	ov5642_write_reg(client,OV5640_CMD_MAIN, 0x08); // release focus (release the position of lens)
	mdelay(10);

	ov5642_write_reg(client,OV5640_CMD_MAIN, 0x03);  //single focus command
	
	while((af_done !=0x10) && (af_done_times++ < 100))
	{
		ov5642_read_reg(client, OV5640_STS_FOCUS, &af_done); //wait af is ok
		mdelay(100);
	}
	printk("[%s] af_done = 0x%x, af_done_times = %d\n", __func__, af_done, af_done_times);

	ov5642_write_reg(client,OV5640_CMD_MAIN, 0x06); // pause focus
#endif
}

static int ov5640_get_af_done_result(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 af_done = 1;
	
	ov5642_read_reg(client, OV5640_STS_FOCUS, &af_done);

	if(af_done == 0x10)
	{
		printk("%s: af_done = 0x%x\n", __func__, af_done);
		return 1;
	}
	else
	{
		return 0;
	}
}
#endif

static int ov5642_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	struct ov5642_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u32 *width = NULL, *height = NULL;
	int err = 0;

	memcpy(&state->pix, &fmt->fmt.pix, sizeof(fmt->fmt.pix));
	switch(state->pix.priv) {
		case CAM_MODE_PREVIEW:
			//leehc
//			state->p_sizes.width = 640;
//			state->p_sizes.height = 480;
			ov5642_preview_start(sd);
			break;
		case CAM_MODE_SNAPSHOT:
			//leehc
	//		state->c_sizes.width = 640;//state->pix.width;
	//		state->c_sizes.height =480;// state->pix.height;
			ov5642_capture_start(sd);
			break;
	}
	return err;

}

static int ov5642_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
	int err = 0;

	return err;
}

static int ov5642_enum_frameintervals(struct v4l2_subdev *sd,
					struct v4l2_frmivalenum *fival)
{
	int err = 0;

	return err;
}



/* V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 *
 * Returns the sensor's video CAPTURE parameters
 */
static int ov5642_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	dev_dbg(&client->dev, "%s\n", __func__);


	return err;

}

/* V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 *
 * Configures the sensor to use the input parameters, if possible.
 * If not possible, reverts to the old parameters and returns the
 *  appropriate error code.
 */
static int ov5642_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	dev_dbg(&client->dev, "%s: numerator %d, denominator: %d\n", \
		__func__, param->parm.capture.timeperframe.numerator, \
		param->parm.capture.timeperframe.denominator);

	return err;
}

static int ov5642_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5642_state *state = to_state(sd);
	struct ov5642_userset userset = state->userset;
	int err = -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		ctrl->value = userset.exposure_bias;
		err = 0;
		break;

	case V4L2_CID_AUTO_WHITE_BALANCE:
		ctrl->value = userset.auto_wb;
		err = 0;
		break;

	case V4L2_CID_WHITE_BALANCE_PRESET:
		ctrl->value = userset.manual_wb;
		err = 0;
		break;

	case V4L2_CID_COLORFX:
		ctrl->value = userset.effect;
		err = 0;
		break;

	case V4L2_CID_CONTRAST:
		ctrl->value = userset.contrast;
		err = 0;
		break;

	case V4L2_CID_SATURATION:
		ctrl->value = userset.saturation;
		err = 0;
		break;

	case V4L2_CID_SHARPNESS:
		ctrl->value = userset.saturation;
		err = 0;
		break;
#ifdef CONFIG_VIDEO_OV5640_AUTO_FOCUS
	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
		if(ov5640_get_af_done_result(sd) == 1)
			ctrl->value = 2; // af success
		else
			ctrl->value = 0;
		err = 0;
		break;
#endif

	case V4L2_CID_BRIGHTNESS:
	default:
		dev_err(&client->dev, "%s: no such ctrl 0x%x\n", __func__, ctrl->id);
		err = 0;
		break;
	}

	return err;
}
/*
 * If the requested control is supported, sets the control's current value
 * in HW (and updates the video_controlp[ array). Otherwise,
 * returns -EINVAL if the control is not supported
 */

static int ov5642_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5642_state *state = to_state(sd);
	int err = 0;
	int value = ctrl->value;

	switch (ctrl->id) {

	case V4L2_CID_CAMERA_FLASH_MODE:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_FLASH_MODE\n",
			__func__);
		break;
	case V4L2_CID_CAMERA_BRIGHTNESS:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_BRIGHTNESS\n",
			__func__);
		break;
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_WHITE_BALANCE\n",
			__func__);

		if (value <= WHITE_BALANCE_AUTO) {
			err = ov5642_write_regs(sd,
			(unsigned char *) ov5642_regs_awb_enable[value],
				sizeof(ov5642_regs_awb_enable[value]));
		} else {
			err = ov5642_write_regs(sd,
			(unsigned char *) ov5642_regs_wb_preset[value-2],
				sizeof(ov5642_regs_wb_preset[value-2]));
		}
		break;
	case V4L2_CID_CAMERA_EFFECT:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_EFFECT\n", __func__);
		err = ov5642_write_regs(sd,
		(unsigned char *) ov5642_regs_color_effect[value-1],
			sizeof(ov5642_regs_color_effect[value-1]));
		break;
	case V4L2_CID_CAMERA_ISO:
	case V4L2_CID_CAMERA_METERING:
		break;
	case V4L2_CID_CAMERA_CONTRAST:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_CONTRAST\n", __func__);
		err = ov5642_write_regs(sd,
		(unsigned char *) ov5642_regs_contrast_bias[value],
			sizeof(ov5642_regs_contrast_bias[value]));
		break;
	case V4L2_CID_CAMERA_SATURATION:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_SATURATION\n", __func__);
		err = ov5642_write_regs(sd,
		(unsigned char *) ov5642_regs_saturation_bias[value],
			sizeof(ov5642_regs_saturation_bias[value]));
		break;
	case V4L2_CID_CAMERA_SHARPNESS:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_SHARPNESS\n", __func__);
		err = ov5642_write_regs(sd,
		(unsigned char *) ov5642_regs_sharpness_bias[value],
			sizeof(ov5642_regs_sharpness_bias[value]));
		break;
	case V4L2_CID_CAMERA_WDR:
	case V4L2_CID_CAMERA_FACE_DETECTION:
	case V4L2_CID_CAMERA_FOCUS_MODE:
	case V4L2_CID_CAM_JPEG_QUALITY:
	case V4L2_CID_CAMERA_SCENE_MODE:
	case V4L2_CID_CAMERA_GPS_LATITUDE:
	case V4L2_CID_CAMERA_GPS_LONGITUDE:
	case V4L2_CID_CAMERA_GPS_TIMESTAMP:
	case V4L2_CID_CAMERA_GPS_ALTITUDE:
	case V4L2_CID_CAMERA_OBJECT_POSITION_X:
	case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
		break;
#ifdef CONFIG_VIDEO_OV5640_AUTO_FOCUS
	case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
		ov5640_single_af(sd);
		break;
#endif
	case V4L2_CID_CAMERA_FRAME_RATE:
		break;
	case V4L2_CID_CAM_PREVIEW_ONOFF:	//0x800.0040
		break;
	case V4L2_CID_CAMERA_CHECK_DATALINE:
	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
		break;
	case V4L2_CID_CAMERA_RESET:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_RESET\n", __func__);
		err = ov5642_reset(sd);
		break;
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "%s: V4L2_CID_EXPOSURE\n", __func__);
		err = ov5642_write_regs(sd,
		(unsigned char *) ov5642_regs_ev_bias[value],
			sizeof(ov5642_regs_ev_bias[value]));
		break;
	default:
		dev_err(&client->dev, "%s: no such control ctrl->id:%x\n", __func__, ctrl->id);
		/* err = -EINVAL; */
		break;
	}

	if (err < 0)
		dev_dbg(&client->dev, "%s: vidioc_s_ctrl failed\n", __func__);

	return err;
}
static int ov5642_get_chipid(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int chip_id = 0;
	u8 readback;

	ov5642_read_reg(client, CHIPID_HI, &readback);
	chip_id = (readback << 8);
	ov5642_read_reg(client, CHIPID_LO, &readback);
	chip_id |= readback;

	return chip_id;
}

#ifdef CONFIG_VIDEO_OV5640_AUTO_FOCUS
static int ov5640_set_af_firmware(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, c_done_times = 0;
	bool bErr = false;
	u8 readback;

	if(bInitialized == true) return 1;

	//ov5642_read_reg(client, OV5640_STS_FOCUS, &readback);
	//printk("before af firmware downloading : readback=0x%x, c_done_times=%d\n", readback, c_done_times);
	for (i = 0; i < OV5640_AF_INIT_REGS; i++) {
		if(i % 2 == 0) {
			bErr = ov5642_write_reg(client, ov5640_af_firmware[i], (u8)ov5640_af_firmware[i+1]);
			if (bErr)
			{
				v4l_info(client, "%s: firmware set failed %x<-%x\n", __func__, ov5640_af_firmware[i], ov5640_af_firmware[i+1]);
				break;
			}
		}
	}

	if (bErr) {
		v4l_err(client, "Failed to load af firmware\n");
	} else {
		// wait until the status is idle
		for(;;)
		{
			ov5642_read_reg(client, OV5640_STS_FOCUS, &readback);
			if((readback == 0x70) || (c_done_times++ > 1000))
				break;
		}

		//bErr = ov5642_write_reg(client, OV5640_CMD_MAIN, 0x04); // set ov5640 to continuous mode
		//if (bErr) printk("confinuous focus setting error\n");
	}

	//ov5642_read_reg(client, OV5640_STS_FOCUS, &readback);
	printk("after af firmware downloading : readback=0x%x, c_done_times=%d\n", readback, c_done_times);

	bInitialized = true;
}
#else
/* AF Command set for OV5642 (General):
 * Command register 0x3024, the input data is hex
 *	EnOverlay	case 0x01, enable overlay window
 *	DisOverlay	case 0x02, disable overlay window
 *	Single		case 0x05, single focusing
 *	Reset		case 0x08, stop focusing and move the lens to infinity position
 *	SetWS		case 0x10, change the focus window size.
 *				Type WS(0-6) into 0x5083 (the old ver. 0x3029)
 *				register before typing into 0x3024.
 *				Focus window size, WS =
 *				0	1/8
 *				1	1/4
 *				2	3/8
 *				3	1/2
 *				4	5/8
 *				5	3/4
 *				6	7/8
 */
static int ov5642_set_af_firmware(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i;
	bool bErr = false;

	if(bInitialized == true) return 1;

	for (i = 0; i < OV5642_AF_INIT_REGS; i++) {
		bErr = ov5642_write_reg(client, ov5642_af_firmware[i][0],
					(u8)ov5642_af_firmware[i][1]);
		if (bErr)
			v4l_info(client, "%s: firmware set failed %x<-%x\n", __func__, ov5642_af_firmware[i][0], ov5642_af_firmware[i][1]);
	}

	bInitialized = true;
}
#endif

static int ov5642_set_mode(struct v4l2_subdev *sd, int mode)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i;
	bool bErr = false;
	unsigned short *pRegs;
	int reg_length = 0;
	u16 reg;
	u8 regval;
	

	printk(KERN_ERR"\t%s mode:%d\n", __func__, mode);
	if(mode == CAM_MODE_PREVIEW) {
		pRegs = (unsigned short *) ov5642_init_640_480;
		reg_length = OV5642_VGA_INIT_REGS;
	printk(KERN_ERR"\t%s preview mode:%d\n", __func__, mode);
	}
	else if(mode == CAM_MODE_SNAPSHOT) {
		pRegs = (unsigned short *) ov5642_init_640_480;
		reg_length = OV5642_VGA_INIT_REGS;
	printk(KERN_ERR"\t%s snapshot mode:%d\n", __func__, mode);
	}
	else
		return -1;

	for (i = 0; i < reg_length; i++) {
		bErr = ov5642_write_reg(client, pRegs[0], pRegs[1]);
		if (bErr)
			v4l_info(client, "%s: register set failed\n", __func__);
		pRegs += 2;
	}


	//mirror & flip
	//leehc add
	//mirror
	reg = 0x3820;	
	ov5642_read_reg(client, reg, &regval);
	printk(KERN_ERR "read form reg num:0x%x. - 0x%x\n", reg, regval);
	regval = regval & 0xf9;
	regval = regval | 0x06;
	printk(KERN_ERR "write to reg num:0x%x. - 0x%x\n", reg, regval);
	bErr = ov5642_write_reg(client, reg, regval);
	if (bErr)
		v4l_info(client, "%s: register set failed\n", __func__);
	//flip
/*
	reg = 0x3821;	
	ov5642_read_reg(client, reg, &regval);
	printk(KERN_ERR "read form reg num:0x%x. - 0x%x\n", reg, regval);
	regval = regval & 0xf9;
	regval = regval | 0x00;
	printk(KERN_ERR "write to reg num:0x%x. - 0x%x\n", reg, regval);
	bErr = ov5642_write_reg(client, reg, regval);
	if (bErr)
		v4l_info(client, "%s: register set failed\n", __func__);
		
*/
#ifdef CONFIG_VIDEO_OV5640_AUTO_FOCUS
	ov5640_set_af_firmware(sd);
#endif
	return 0;
}

#define OV5642_CMD_MAIN		0x3024
#define OV5642_CMD_TAG		0x3025
#define OV5642_STS_FOCUS	0x302B

static void ov5642_set_focus(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 readback;

	ov5642_read_reg(client, OV5642_STS_FOCUS, &readback);

#if 0
	ov5642_write_reg(client, OV5642_CMD_MAIN, 0x02);
	ov5642_write_reg(client, OV5642_CMD_TAG,  0x01);

	ov5642_write_reg(client, OV5642_CMD_MAIN, 0x20);
	ov5642_write_reg(client, OV5642_CMD_TAG,  0x01);

	ov5642_write_reg(client, OV5642_CMD_MAIN, 0x05);
	ov5642_write_reg(client, OV5642_CMD_TAG,  0x03);

	ov5642_write_reg(client, OV5642_CMD_MAIN, 0x10);
	ov5642_write_reg(client, OV5642_CMD_MAIN, 0x01);
	ov5642_write_reg(client, OV5642_CMD_TAG,  0x0B);
	ov5642_write_reg(client, OV5642_CMD_MAIN, 0x10);
#endif
	ov5642_write_reg(client, OV5642_CMD_TAG,  0x03);
	ov5642_write_reg(client, OV5642_CMD_MAIN, 0x05);

	ov5642_read_reg(client, OV5642_STS_FOCUS, &readback);
}


static int ov5642_s_stream(struct v4l2_subdev *sd, int enable)
{
	/* No need */
	return 0;
}

static int ov5642_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5642_state *state = to_state(sd);
	struct ov5642_mbus_platform_data *pdata = state->pdata;
	int ret;

	/* bug report */
	BUG_ON(!pdata);

	if(pdata->set_clock) {
		ret = pdata->set_clock(&client->dev, on);
		if(ret)
			return -EIO;
	}
	/* setting power */
	if(pdata->set_power) {
		ret = pdata->set_power(on);
		if(ret)
			return -EIO;
		if(on)
			return ov5642_init(sd, 0);
	}

	return 0;
}


static int ov5642_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5642_state *state = to_state(sd);
	int i;
	bool bErr = false;
	int cid;


	cid = ov5642_get_chipid(sd);
	printk(KERN_ERR"%s: CHIPD ID=%X\n", __func__, cid);
	if (bErr || cid != 0x5640) {
		v4l_err(client, "%s: camera initialization failed\n", __func__);
                return -EIO;    /* FIXME */
        }

#ifdef CONFIG_VIDEO_OV5640_AUTO_FOCUS
	bInitialized = false;
#endif

	ov5642_set_mode(sd, CAM_MODE_PREVIEW);
	return 0;
}


static int ov5642_g_chip_ident(struct v4l2_subdev *sd, int *id)
{

    ((struct v4l2_dbg_chip_ident *)id)->match.type = V4L2_CHIP_MATCH_I2C_DRIVER;
    strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name, "ov5642_camera");
    return 0;
}


static const struct v4l2_subdev_core_ops ov5642_core_ops = {
	.init = ov5642_init,	/* initializing API */
	.s_power = ov5642_s_power,	
	.queryctrl = ov5642_queryctrl,
	.querymenu = ov5642_querymenu,
	.g_ctrl = ov5642_g_ctrl,
	.s_ctrl = ov5642_s_ctrl,
	.g_chip_ident = ov5642_g_chip_ident,
};

static const struct v4l2_subdev_video_ops ov5642_video_ops = {
	.s_crystal_freq = ov5642_s_crystal_freq,
	.g_mbus_fmt = ov5642_g_fmt,
	.s_mbus_fmt = ov5642_s_fmt,
	.enum_framesizes = ov5642_enum_framesizes,
	.enum_frameintervals = ov5642_enum_frameintervals,
	.g_parm = ov5642_g_parm,
	.s_parm = ov5642_s_parm,
	.s_stream = ov5642_s_stream,	
};

static const struct v4l2_subdev_ops ov5642_ops = {
	.core = &ov5642_core_ops,
	.video = &ov5642_video_ops,
};

/*
 * ov5642_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int ov5642_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct ov5642_state *state;
	struct v4l2_subdev *sd;
	struct ov5642_mbus_platform_data *pdata = client->dev.platform_data;

	state = kzalloc(sizeof(struct ov5642_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	sd = &state->sd;
	strcpy(sd->name, OV5642_DRIVER_NAME);
	state->pdata = client->dev.platform_data;
	
	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &ov5642_ops);

	/* needed for acquiring subdevice by this module name */
	snprintf(sd->name, sizeof(sd->name), OV5642_DRIVER_NAME);
#if 0
	dev_info(&client->dev, "id: %d, fmt.code: %d, res: res: %d x %d",
	    pdata->id, pdata->fmt.code,
	    pdata->fmt.width, pdata->fmt.height);
#endif
	dev_info(&client->dev, "ov5642 has been probed\n");
	return 0;
}


static int ov5642_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id ov5642_id[] = {
	{ OV5642_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ov5642_id);

static struct i2c_driver ov5642_i2c_driver = {
	.driver = {
		.name	= OV5642_DRIVER_NAME,
	},
	.probe		= ov5642_probe,
	.remove		= ov5642_remove,
	.id_table	= ov5642_id,
};
static int __init ov5642_mod_init(void)
{
	printk(KERN_INFO "%s \n", __func__);	
	return i2c_add_driver(&ov5642_i2c_driver);
}

static void __exit ov5642_mod_exit(void)
{
	i2c_del_driver(&ov5642_i2c_driver);
}
module_init(ov5642_mod_init);
module_exit(ov5642_mod_exit);


MODULE_DESCRIPTION("Samsung Electronics OV5642 UXGA camera driver");
MODULE_AUTHOR("Jinsung Yang <jsgood.yang@samsung.com>");
MODULE_LICENSE("GPL");

