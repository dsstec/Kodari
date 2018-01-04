/*
 * wm1800.c  --  WM1800 ALSA SoC Audio driver
 *
 * Author: Liam Girdwood
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/wm1800.h>

#include "wm1800.h"

#define AUDIO_NAME "wm1800"

/* R25 - Power 1 */
#define WM1800_VMID_MASK	0x180
#define WM1800_VREF			0x40

/* R26 - Power 2 */
#define WM1800_PWR2_LOUT1	0x40
#define WM1800_PWR2_ROUT1	0x20
#define WM1800_PWR2_OUT3	0x02

/* R28 - Anti-pop 1 */
#define WM1800_POBCTRL		0x80
#define WM1800_BUFDCOPEN	0x10
#define WM1800_BUFIOEN		0x08
#define WM1800_SOFT_ST		0x04
#define WM1800_HPSTBY		0x01

/* R29 - Anti-pop 2 */
#define WM1800_DISOP		0x40
#define WM1800_DRES_MASK	0x30

/*
 * wm1800 register cache
 * We can't read the WM1800 register space when we are
 * using 2 wire for device control, so we cache them instead.
 */
static const u16 wm1800_reg[WM1800_CACHEREGNUM] = {
	0x0097, 0x0097, 0x017f, 0x017f,
	0x0000, 0x0008, 0x0000, 0x000a,
	0x01c0, 0x0000, 0x00ff, 0x00ff,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x007b, 0x0100, 0x0032,
	0x0000, 0x00c3, 0x00c3, 0x01c0,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0100, 0x0100, 0x0050, 0x0050,
	0x0050, 0x0050, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0040, 0x0000,
	0x0000, 0x0050, 0x0050, 0x0000,
	0x0002, 0x0037, 0x004d, 0x0080,
	0x0008, 0x0031, 0x0026, 0x00e9,
};

struct wm1800_priv {
	enum snd_soc_control_type control_type;

	struct snd_soc_codec codec;
	struct snd_soc_dapm_widget *lout1;
	struct snd_soc_dapm_widget *rout1;
	struct snd_soc_dapm_widget *out3;
};

#define wm1800_reset(c)	snd_soc_write(c, WM1800_RESET, 0)

/* enumerated controls */
static const char *wm1800_deemph[] = {"None", "32Khz", "44.1Khz", "48Khz"};
static const char *wm1800_polarity[] = {"No Inversion", "Left Inverted",
	"Right Inverted", "Stereo Inversion"};
static const char *wm1800_3d_upper_cutoff[] = {"High", "Low"};
static const char *wm1800_3d_lower_cutoff[] = {"Low", "High"};
static const char *wm1800_alcfunc[] = {"Off", "Right", "Left", "Stereo"};
static const char *wm1800_alcmode[] = {"ALC", "Limiter"};

static const struct soc_enum wm1800_enum[] = {
	SOC_ENUM_SINGLE(WM1800_DACCTL1, 1, 4, wm1800_deemph),
	SOC_ENUM_SINGLE(WM1800_DACCTL1, 5, 4, wm1800_polarity),
	SOC_ENUM_SINGLE(WM1800_DACCTL2, 5, 4, wm1800_polarity),
	SOC_ENUM_SINGLE(WM1800_3D, 6, 2, wm1800_3d_upper_cutoff),
	SOC_ENUM_SINGLE(WM1800_3D, 5, 2, wm1800_3d_lower_cutoff),
	SOC_ENUM_SINGLE(WM1800_ALC1, 7, 4, wm1800_alcfunc),
	SOC_ENUM_SINGLE(WM1800_ALC3, 8, 2, wm1800_alcmode),
};

static const DECLARE_TLV_DB_SCALE(adc_tlv, -9700, 50, 0);
static const DECLARE_TLV_DB_SCALE(dac_tlv, -12700, 50, 1);
static const DECLARE_TLV_DB_SCALE(bypass_tlv, -2100, 300, 0);
static const DECLARE_TLV_DB_SCALE(out_tlv, -12100, 100, 1);

static const struct snd_kcontrol_new wm1800_snd_controls[] = {

SOC_DOUBLE_R_TLV("Capture Volume", WM1800_LINVOL, WM1800_RINVOL,
		0, 63, 0, adc_tlv),
SOC_DOUBLE_R("Capture Volume ZC Switch", WM1800_LINVOL, WM1800_RINVOL,
		6, 1, 0),
SOC_DOUBLE_R("Capture Switch", WM1800_LINVOL, WM1800_RINVOL,
		7, 1, 0),

SOC_DOUBLE_R_TLV("Playback Volume", WM1800_LDAC, WM1800_RDAC,
		0, 255, 0, dac_tlv),

SOC_DOUBLE_R_TLV("Headphone Playback Volume", WM1800_LOUT1, WM1800_ROUT1,
		0, 127, 0, out_tlv),
SOC_DOUBLE_R("Headphone Playback ZC Switch", WM1800_LOUT1, WM1800_ROUT1,
		7, 1, 0),

SOC_DOUBLE_R_TLV("Speaker Playback Volume", WM1800_LOUT2, WM1800_ROUT2,
		0, 127, 0, out_tlv),
SOC_DOUBLE_R("Speaker Playback ZC Switch", WM1800_LOUT2, WM1800_ROUT2,
		7, 1, 0),
SOC_SINGLE("Speaker DC Volume", WM1800_CLASSD3, 3, 5, 0),
SOC_SINGLE("Speaker AC Volume", WM1800_CLASSD3, 0, 5, 0),

SOC_SINGLE("PCM Playback -6dB Switch", WM1800_DACCTL1, 7, 1, 0),
SOC_ENUM("ADC Polarity", wm1800_enum[1]),
SOC_ENUM("Playback De-emphasis", wm1800_enum[0]),
SOC_SINGLE("ADC High Pass Filter Switch", WM1800_DACCTL1, 0, 1, 0),

SOC_ENUM("DAC Polarity", wm1800_enum[2]),

SOC_ENUM("3D Filter Upper Cut-Off", wm1800_enum[3]),
SOC_ENUM("3D Filter Lower Cut-Off", wm1800_enum[4]),
SOC_SINGLE("3D Volume", WM1800_3D, 1, 15, 0),
SOC_SINGLE("3D Switch", WM1800_3D, 0, 1, 0),

SOC_ENUM("ALC Function", wm1800_enum[5]),
SOC_SINGLE("ALC Max Gain", WM1800_ALC1, 4, 7, 0),
SOC_SINGLE("ALC Target", WM1800_ALC1, 0, 15, 1),
SOC_SINGLE("ALC Min Gain", WM1800_ALC2, 4, 7, 0),
SOC_SINGLE("ALC Hold Time", WM1800_ALC2, 0, 15, 0),
SOC_ENUM("ALC Mode", wm1800_enum[6]),
SOC_SINGLE("ALC Decay", WM1800_ALC3, 4, 15, 0),
SOC_SINGLE("ALC Attack", WM1800_ALC3, 0, 15, 0),

SOC_SINGLE("Noise Gate Threshold", WM1800_NOISEG, 3, 31, 0),
SOC_SINGLE("Noise Gate Switch", WM1800_NOISEG, 0, 1, 0),

SOC_DOUBLE_R("ADC PCM Capture Volume", WM1800_LINPATH, WM1800_RINPATH,
	0, 127, 0),

SOC_SINGLE_TLV("Left Output Mixer Boost Bypass Volume",
	       WM1800_BYPASS1, 4, 7, 1, bypass_tlv),
SOC_SINGLE_TLV("Left Output Mixer LINPUT3 Volume",
	       WM1800_LOUTMIX, 4, 7, 1, bypass_tlv),
SOC_SINGLE_TLV("Right Output Mixer Boost Bypass Volume",
	       WM1800_BYPASS2, 4, 7, 1, bypass_tlv),
SOC_SINGLE_TLV("Right Output Mixer RINPUT3 Volume",
	       WM1800_ROUTMIX, 4, 7, 1, bypass_tlv),
};

static const struct snd_kcontrol_new wm1800_lin_boost[] = {
SOC_DAPM_SINGLE("LINPUT2 Switch", WM1800_LINPATH, 6, 1, 0),
SOC_DAPM_SINGLE("LINPUT3 Switch", WM1800_LINPATH, 7, 1, 0),
SOC_DAPM_SINGLE("LINPUT1 Switch", WM1800_LINPATH, 8, 1, 0),
};

static const struct snd_kcontrol_new wm1800_lin[] = {
SOC_DAPM_SINGLE("Boost Switch", WM1800_LINPATH, 3, 1, 0),
};

static const struct snd_kcontrol_new wm1800_rin_boost[] = {
SOC_DAPM_SINGLE("RINPUT2 Switch", WM1800_RINPATH, 6, 1, 0),
SOC_DAPM_SINGLE("RINPUT3 Switch", WM1800_RINPATH, 7, 1, 0),
SOC_DAPM_SINGLE("RINPUT1 Switch", WM1800_RINPATH, 8, 1, 0),
};

static const struct snd_kcontrol_new wm1800_rin[] = {
SOC_DAPM_SINGLE("Boost Switch", WM1800_RINPATH, 3, 1, 0),
};

static const struct snd_kcontrol_new wm1800_loutput_mixer[] = {
SOC_DAPM_SINGLE("PCM Playback Switch", WM1800_LOUTMIX, 8, 1, 0),
SOC_DAPM_SINGLE("LINPUT3 Switch", WM1800_LOUTMIX, 7, 1, 0),
SOC_DAPM_SINGLE("Boost Bypass Switch", WM1800_BYPASS1, 7, 1, 0),
};

static const struct snd_kcontrol_new wm1800_routput_mixer[] = {
SOC_DAPM_SINGLE("PCM Playback Switch", WM1800_ROUTMIX, 8, 1, 0),
SOC_DAPM_SINGLE("RINPUT3 Switch", WM1800_ROUTMIX, 7, 1, 0),
SOC_DAPM_SINGLE("Boost Bypass Switch", WM1800_BYPASS2, 7, 1, 0),
};

static const struct snd_kcontrol_new wm1800_mono_out[] = {
SOC_DAPM_SINGLE("Left Switch", WM1800_MONOMIX1, 7, 1, 0),
SOC_DAPM_SINGLE("Right Switch", WM1800_MONOMIX2, 7, 1, 0),
};

static const struct snd_soc_dapm_widget wm1800_dapm_widgets[] = {
SND_SOC_DAPM_INPUT("LINPUT1"),
SND_SOC_DAPM_INPUT("RINPUT1"),
SND_SOC_DAPM_INPUT("LINPUT2"),
SND_SOC_DAPM_INPUT("RINPUT2"),
SND_SOC_DAPM_INPUT("LINPUT3"),
SND_SOC_DAPM_INPUT("RINPUT3"),

SND_SOC_DAPM_MICBIAS("MICB", WM1800_POWER1, 1, 1),

SND_SOC_DAPM_MIXER("Left Boost Mixer", WM1800_POWER1, 5, 0,
		   wm1800_lin_boost, ARRAY_SIZE(wm1800_lin_boost)),
SND_SOC_DAPM_MIXER("Right Boost Mixer", WM1800_POWER1, 4, 0,
		   wm1800_rin_boost, ARRAY_SIZE(wm1800_rin_boost)),

SND_SOC_DAPM_MIXER("Left Input Mixer", WM1800_POWER3, 5, 0,
		   wm1800_lin, ARRAY_SIZE(wm1800_lin)),
SND_SOC_DAPM_MIXER("Right Input Mixer", WM1800_POWER3, 4, 0,
		   wm1800_rin, ARRAY_SIZE(wm1800_rin)),

SND_SOC_DAPM_ADC("Left ADC", "Capture", WM1800_POWER1, 3, 0),
SND_SOC_DAPM_ADC("Right ADC", "Capture", WM1800_POWER1, 2, 0),

SND_SOC_DAPM_DAC("Left DAC", "Playback", WM1800_POWER2, 8, 0),
SND_SOC_DAPM_DAC("Right DAC", "Playback", WM1800_POWER2, 7, 0),

SND_SOC_DAPM_MIXER("Left Output Mixer", WM1800_POWER3, 3, 0,
	&wm1800_loutput_mixer[0],
	ARRAY_SIZE(wm1800_loutput_mixer)),
SND_SOC_DAPM_MIXER("Right Output Mixer", WM1800_POWER3, 2, 0,
	&wm1800_routput_mixer[0],
	ARRAY_SIZE(wm1800_routput_mixer)),

SND_SOC_DAPM_PGA("LOUT1 PGA", WM1800_POWER2, 6, 0, NULL, 0),
SND_SOC_DAPM_PGA("ROUT1 PGA", WM1800_POWER2, 5, 0, NULL, 0),

SND_SOC_DAPM_PGA("Left Speaker PGA", WM1800_POWER2, 4, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right Speaker PGA", WM1800_POWER2, 3, 0, NULL, 0),

SND_SOC_DAPM_PGA("Right Speaker Output", WM1800_CLASSD1, 7, 0, NULL, 0),
SND_SOC_DAPM_PGA("Left Speaker Output", WM1800_CLASSD1, 6, 0, NULL, 0),

SND_SOC_DAPM_OUTPUT("SPK_LP"),
SND_SOC_DAPM_OUTPUT("SPK_LN"),
SND_SOC_DAPM_OUTPUT("HP_L"),
SND_SOC_DAPM_OUTPUT("HP_R"),
SND_SOC_DAPM_OUTPUT("SPK_RP"),
SND_SOC_DAPM_OUTPUT("SPK_RN"),
SND_SOC_DAPM_OUTPUT("OUT3"),
};

static const struct snd_soc_dapm_widget wm1800_dapm_widgets_out3[] = {
SND_SOC_DAPM_MIXER("Mono Output Mixer", WM1800_POWER2, 1, 0,
	&wm1800_mono_out[0],
	ARRAY_SIZE(wm1800_mono_out)),
};

/* Represent OUT3 as a PGA so that it gets turned on with LOUT1/ROUT1 */
static const struct snd_soc_dapm_widget wm1800_dapm_widgets_capless[] = {
SND_SOC_DAPM_PGA("OUT3 VMID", WM1800_POWER2, 1, 0, NULL, 0),
};

static const struct snd_soc_dapm_route audio_paths[] = {
	{ "Left Boost Mixer", "LINPUT1 Switch", "LINPUT1" },
	{ "Left Boost Mixer", "LINPUT2 Switch", "LINPUT2" },
	{ "Left Boost Mixer", "LINPUT3 Switch", "LINPUT3" },

	{ "Left Input Mixer", "Boost Switch", "Left Boost Mixer", },
	{ "Left Input Mixer", NULL, "LINPUT1", },  /* Really Boost Switch */
	{ "Left Input Mixer", NULL, "LINPUT2" },
	{ "Left Input Mixer", NULL, "LINPUT3" },

	{ "Right Boost Mixer", "RINPUT1 Switch", "RINPUT1" },
	{ "Right Boost Mixer", "RINPUT2 Switch", "RINPUT2" },
	{ "Right Boost Mixer", "RINPUT3 Switch", "RINPUT3" },

	{ "Right Input Mixer", "Boost Switch", "Right Boost Mixer", },
	{ "Right Input Mixer", NULL, "RINPUT1", },  /* Really Boost Switch */
	{ "Right Input Mixer", NULL, "RINPUT2" },
	{ "Right Input Mixer", NULL, "LINPUT3" },

	{ "Left ADC", NULL, "Left Input Mixer" },
	{ "Right ADC", NULL, "Right Input Mixer" },

	{ "Left Output Mixer", "LINPUT3 Switch", "LINPUT3" },
	{ "Left Output Mixer", "Boost Bypass Switch", "Left Boost Mixer"} ,
	{ "Left Output Mixer", "PCM Playback Switch", "Left DAC" },

	{ "Right Output Mixer", "RINPUT3 Switch", "RINPUT3" },
	{ "Right Output Mixer", "Boost Bypass Switch", "Right Boost Mixer" } ,
	{ "Right Output Mixer", "PCM Playback Switch", "Right DAC" },

	{ "LOUT1 PGA", NULL, "Left Output Mixer" },
	{ "ROUT1 PGA", NULL, "Right Output Mixer" },

	{ "HP_L", NULL, "LOUT1 PGA" },
	{ "HP_R", NULL, "ROUT1 PGA" },

	{ "Left Speaker PGA", NULL, "Left Output Mixer" },
	{ "Right Speaker PGA", NULL, "Right Output Mixer" },

	{ "Left Speaker Output", NULL, "Left Speaker PGA" },
	{ "Right Speaker Output", NULL, "Right Speaker PGA" },

	{ "SPK_LN", NULL, "Left Speaker Output" },
	{ "SPK_LP", NULL, "Left Speaker Output" },
	{ "SPK_RN", NULL, "Right Speaker Output" },
	{ "SPK_RP", NULL, "Right Speaker Output" },
};

static const struct snd_soc_dapm_route audio_paths_out3[] = {
	{ "Mono Output Mixer", "Left Switch", "Left Output Mixer" },
	{ "Mono Output Mixer", "Right Switch", "Right Output Mixer" },

	{ "OUT3", NULL, "Mono Output Mixer", }
};

static const struct snd_soc_dapm_route audio_paths_capless[] = {
	{ "HP_L", NULL, "OUT3 VMID" },
	{ "HP_R", NULL, "OUT3 VMID" },

	{ "OUT3 VMID", NULL, "Left Output Mixer" },
	{ "OUT3 VMID", NULL, "Right Output Mixer" },
};

static int wm1800_add_widgets(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	snd_soc_dapm_new_controls(dapm, wm1800_dapm_widgets,
				  ARRAY_SIZE(wm1800_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, audio_paths, ARRAY_SIZE(audio_paths));

	return 0;
}

static int wm1800_set_dai_fmt(struct snd_soc_dai *codec_dai,
							unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = 0;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBM_CFM:
			iface |= 0x0040;
			break;
		case SND_SOC_DAIFMT_CBS_CFS:
			break;
		default:
			return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			iface |= 0x0002;
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			iface |= 0x0001;
			break;
		case SND_SOC_DAIFMT_DSP_A:
			iface |= 0x0003;
			break;
		case SND_SOC_DAIFMT_DSP_B:
			iface |= 0x0013;
			break;
		default:
			return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_IB_IF:
			iface |= 0x0090;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			iface |= 0x0080;
			break;
		case SND_SOC_DAIFMT_NB_IF:
			iface |= 0x0010;
			break;
		default:
			return -EINVAL;
	}

	/* set iface */
	snd_soc_write(codec, WM1800_IFACE1, iface);
	return 0;
}

static int wm1800_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;

	u16 iface = snd_soc_read(codec, WM1800_IFACE1) & 0xfff3;

	/* bit size */
	switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			break;
		case SNDRV_PCM_FORMAT_S20_3LE:
			iface |= 0x0004;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			iface |= 0x0008;
			break;
	}

	/* set iface */
	snd_soc_write(codec, WM1800_IFACE1, iface);
	return 0;
}

static int wm1800_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;

	u16 mute_reg = snd_soc_read(codec, WM1800_DACCTL1) & 0xfff7;

	if (mute)
		snd_soc_write(codec, WM1800_DACCTL1, mute_reg | 0x8);
	else
		snd_soc_write(codec, WM1800_DACCTL1, mute_reg);

	return 0;
}

static int wm1800_set_bias_level(struct snd_soc_codec *codec,
				      enum snd_soc_bias_level level)
{
	u16 reg;

	switch (level) {
		case SND_SOC_BIAS_ON:
			break;

		case SND_SOC_BIAS_PREPARE:
			/* Set VMID to 2x50k */
			reg = snd_soc_read(codec, WM1800_POWER1);
			reg &= ~0x180;
			reg |= 0x80;
			snd_soc_write(codec, WM1800_POWER1, reg);
			break;

		case SND_SOC_BIAS_STANDBY:
			if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {
				/* Enable anti-pop features */
				snd_soc_write(codec, WM1800_APOP1,
						      WM1800_POBCTRL | WM1800_SOFT_ST |
						      WM1800_BUFDCOPEN | WM1800_BUFIOEN);

				/* Enable & ramp VMID at 2x50k */
				reg = snd_soc_read(codec, WM1800_POWER1);
				reg |= 0x80;
				snd_soc_write(codec, WM1800_POWER1, reg);
				msleep(100);

				/* Enable VREF */
				snd_soc_write(codec, WM1800_POWER1, reg | WM1800_VREF);

				/* Disable anti-pop features */
				snd_soc_write(codec, WM1800_APOP1, WM1800_BUFIOEN);
			}

			/* Set VMID to 2x250k */
			reg = snd_soc_read(codec, WM1800_POWER1);
			reg &= ~0x180;
			reg |= 0x100;
			snd_soc_write(codec, WM1800_POWER1, reg);
			break;

		case SND_SOC_BIAS_OFF:
			/* Enable anti-pop features */
			snd_soc_write(codec, WM1800_APOP1,
					     WM1800_POBCTRL | WM1800_SOFT_ST |
					     WM1800_BUFDCOPEN | WM1800_BUFIOEN);

			/* Disable VMID and VREF, let them discharge */
			snd_soc_write(codec, WM1800_POWER1, 0);
			msleep(600);
			break;
	}

	codec->dapm.bias_level = level;
	return 0;
}

/* PLL divisors */
struct _pll_div {
	u32 pre_div:1;
	u32 n:4;
	u32 k:24;
};

/* The size in bits of the pll divide multiplied by 10
 * to allow rounding later */
#define FIXED_PLL_SIZE ((1 << 24) * 10)

static int pll_factors(unsigned int source, unsigned int target,
		       struct _pll_div *pll_div)
{
	unsigned long long Kpart;
	unsigned int K, Ndiv, Nmod;

	pr_debug("WM1800 PLL: setting %dHz->%dHz\n", source, target);

	/* Scale up target to PLL operating frequency */
	/* GARAMJIN: R = 4 x 2 x target = 4 x 2 x 12.288MHz = 98.304MHz */
	target *= 8;

	Ndiv = target / source;
	if (Ndiv < 6) {
		source >>= 1;
		pll_div->pre_div = 1;
		/* PLL N = int R = 8 */
		Ndiv = target / source;
	}
	else
		pll_div->pre_div = 0;

	if ((Ndiv < 6) || (Ndiv > 12)) {
		pr_err("WM1800 PLL: Unsupported N=%d\n", Ndiv);
		return -EINVAL;
	}

	pll_div->n = Ndiv;
	Nmod = target % source;
	Kpart = FIXED_PLL_SIZE * (long long)Nmod;

	do_div(Kpart, source);

	K = Kpart & 0xFFFFFFFF;

	/* Check if we need to round */
	if ((K % 10) >= 5)
		K += 5;

	/* Move down to proper range now rounding is done */
	K /= 10;

	pll_div->k = K;

	/* GARAMJIN: 2011.07.07
	 * MCLK Input : 24.596 MHz
	 *	N=7, K=fe55ae, pre_div=1
	 */
	pr_debug("WM1800 PLL: N=%x K=%x pre_div=%d\n",
		 pll_div->n, pll_div->k, pll_div->pre_div);
	return 0;
}

static int wm1800_set_dai_pll(struct snd_soc_dai *codec_dai, int pll_id,
		int source, unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 reg;
	static struct _pll_div pll_div;
	int ret;

	if (freq_in && freq_out) {
		ret = pll_factors(freq_in, freq_out, &pll_div);
		if (ret != 0)
			return ret;
	}

	/* Disable the PLL: even if we are changing the frequency the
	 * PLL needs to be disabled while we do so. */
	snd_soc_write(codec, WM1800_CLOCK1,
		     snd_soc_read(codec, WM1800_CLOCK1) & ~1);
	snd_soc_write(codec, WM1800_POWER2,
		     snd_soc_read(codec, WM1800_POWER2) & ~1);

	if (!freq_in || !freq_out)
		return 0;

	reg = snd_soc_read(codec, WM1800_PLL1) & ~0x3f;
	reg |= pll_div.pre_div << 4;
	reg |= pll_div.n;

	if (pll_div.k) {
		reg |= 0x20;

		snd_soc_write(codec, WM1800_PLL2, (pll_div.k >> 18) & 0x3f);
		snd_soc_write(codec, WM1800_PLL3, (pll_div.k >> 9) & 0x1ff);
		snd_soc_write(codec, WM1800_PLL4, pll_div.k & 0x1ff);
	}
	snd_soc_write(codec, WM1800_PLL1, reg);

	/* Turn it on */
	snd_soc_write(codec, WM1800_POWER2,
		     snd_soc_read(codec, WM1800_POWER2) | 1);
	msleep(250);
	snd_soc_write(codec, WM1800_CLOCK1,
		     snd_soc_read(codec, WM1800_CLOCK1) | 1);
	return 0;
}

static int wm1800_set_dai_clkdiv(struct snd_soc_dai *codec_dai,
								int div_id, int div)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	unsigned int reg;

	switch (div_id) {
		case WM1800_SYSCLKDIV:
			reg = snd_soc_read(codec, WM1800_CLOCK1) & 0x1f9;
			snd_soc_write(codec, WM1800_CLOCK1, reg | div);
			break;
		case WM1800_DACDIV:
			reg = snd_soc_read(codec, WM1800_CLOCK1) & 0x1c7;
			snd_soc_write(codec, WM1800_CLOCK1, reg | div);
			break;
		case WM1800_OPCLKDIV:
			reg = snd_soc_read(codec, WM1800_PLL1) & 0x03f;
			snd_soc_write(codec, WM1800_PLL1, reg | div);
			break;
		case WM1800_DCLKDIV:
			reg = snd_soc_read(codec, WM1800_CLOCK2) & 0x03f;
			snd_soc_write(codec, WM1800_CLOCK2, reg | div);
			break;
		case WM1800_TOCLKSEL:
			reg = snd_soc_read(codec, WM1800_ADDCTL1) & 0x1fd;
			snd_soc_write(codec, WM1800_ADDCTL1, reg | div);
			break;
		case WM1800_SYSCLKSEL:		/*+GARAMJIN:0705*/
			reg = snd_soc_read(codec, WM1800_CLOCK1) & 0x1fe;
			snd_soc_write(codec, WM1800_CLOCK1, reg | div);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

int wm1800_set_outpath(struct snd_soc_codec *codec, u8 out_path)
{
	if(codec == NULL) return -1;

	/*
	u16 hpswen = snd_soc_read(codec, WM1800_ADDCTL2);
	hpswen &= ~(1<<6);
	snd_soc_write(codec, WM1800_ADDCTL2, hpswen);

	u16 hpsw = snd_soc_read(codec, WM1800_POWER2);
	hpsw &= ~(0xF<<3);
	*/

	switch(out_path) {
		case WM1800_OUT_EAR:
			//hpsw |= (0x3<<5);
			//snd_soc_write(codec, WM1800_POWER2, hpsw);
			//snd_soc_write(codec, WM1800_LOUT1, 0x17f);
			//snd_soc_write(codec, WM1800_ROUT1, 0x17f);

			
			snd_soc_write(codec, WM1800_LOUT1, 0x178);
			snd_soc_write(codec, WM1800_ROUT1, 0x178);
	//		snd_soc_write(codec, WM1800_LDAC,   0x1C8);
	//		snd_soc_write(codec, WM1800_RDAC,   0x1C8);

	
			snd_soc_write(codec, WM1800_LDAC,   0x1FF);
			snd_soc_write(codec, WM1800_RDAC,   0x1FF);
			
//			snd_soc_write(codec, WM1800_LOUT2, 0x100);
//			snd_soc_write(codec, WM1800_ROUT2, 0x100);
			snd_soc_write(codec, WM1800_LOUT2, 0x0);
			snd_soc_write(codec, WM1800_ROUT2, 0x0);

			break;
		case WM1800_OUT_SPK_MUTE:			
			snd_soc_write(codec, WM1800_LOUT1, 0x0);
			snd_soc_write(codec, WM1800_ROUT1, 0x0);
			snd_soc_write(codec, WM1800_LOUT2, 0x0);
			snd_soc_write(codec, WM1800_ROUT2, 0x0);
			break;
		case WM1800_OUT_NONE:
		case WM1800_OUT_SPK:
		default:
			//hpsw |= (0x3<<3);
			//snd_soc_write(codec, WM1800_POWER2, hpsw);
			snd_soc_write(codec, WM1800_LOUT1, 0x100);
			snd_soc_write(codec, WM1800_ROUT1, 0x100);
			snd_soc_write(codec, WM1800_LOUT2, 0x17f);
			snd_soc_write(codec, WM1800_ROUT2, 0x17f);
			break;
	}
	return 0;
}


int wm1800_set_inpath(struct snd_soc_codec *codec, u8 out_path)
{
	if(codec == NULL) return -1;

	switch(out_path) {
		case WM1800_IN_MIC3_L:
			//hpsw |= (0x3<<5);
			//snd_soc_write(codec, WM1800_POWER2, hpsw);
			snd_soc_write(codec, WM1800_LOUT1, 0x17f);
			snd_soc_write(codec, WM1800_ROUT1, 0x17f);
			snd_soc_write(codec, WM1800_LOUT2, 0x100);
			snd_soc_write(codec, WM1800_ROUT2, 0x100);
			break;
	}
	return 0;
}


/*TODO:
static struct snd_soc_codec *wm1800_codec;
*/

#define WM1800_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_ops wm1800_dai_ops_playback = {
	//TODO:.set_sysclk = wm1800_set_sysclk,
	.hw_params	= wm1800_hw_params,
	.set_fmt	= wm1800_set_dai_fmt,
	.set_clkdiv = wm1800_set_dai_clkdiv,
	.set_pll	= wm1800_set_dai_pll,
	.digital_mute = wm1800_digital_mute,
};

static struct snd_soc_dai_ops wm1800_dai_ops_capture = {
	//TODO:.set_sysclk	= wm1800_set_sysclk,
	.hw_params	= wm1800_hw_params,
	.set_fmt	= wm1800_set_dai_fmt,
	.set_clkdiv	= wm1800_set_dai_clkdiv,
	.set_pll	= wm1800_set_dai_pll,
};

static struct snd_soc_dai_driver wm1800_dai[] = {
	{
		.name = "wm1800-capture",
		.id = WM1800_DAI_PAIFTX,
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = WM1800_FORMATS,
		},
		.ops = &wm1800_dai_ops_capture,
	},
	{
		.name = "wm1800-playback",
		.id = WM1800_DAI_PAIFRX,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = WM1800_FORMATS,
		},
		.ops = &wm1800_dai_ops_playback,
	},

};


static void foo(struct snd_soc_codec *codec)
{
	u16 reg;
	//reg = snd_soc_read(codec, WM1800_LOUTMIX);
	//snd_soc_write(codec, WM1800_LOUTMIX, reg | 0x100);
	//reg = snd_soc_read(codec, WM1800_ROUTMIX);
	//snd_soc_write(codec, WM1800_ROUTMIX, reg | 0x100);

	/* Latch the update bits */
	reg = snd_soc_read(codec, WM1800_LINVOL);
	snd_soc_write(codec, WM1800_LINVOL, reg | 0x117);
	reg = snd_soc_read(codec, WM1800_RINVOL);
	snd_soc_write(codec, WM1800_RINVOL, reg | 0x117);
	reg = snd_soc_read(codec, WM1800_LADC);
	snd_soc_write(codec, WM1800_LADC, reg | 0x100);
	reg = snd_soc_read(codec, WM1800_RADC);
	snd_soc_write(codec, WM1800_RADC, reg | 0x100);
	reg = snd_soc_read(codec, WM1800_LDAC);
	snd_soc_write(codec, WM1800_LDAC, reg | 0x100);
	reg = snd_soc_read(codec, WM1800_RDAC);
	snd_soc_write(codec, WM1800_RDAC, reg | 0x100);
	reg = snd_soc_read(codec, WM1800_LOUT1);
	snd_soc_write(codec, WM1800_LOUT1, reg | 0x100);
	reg = snd_soc_read(codec, WM1800_ROUT1);
	snd_soc_write(codec, WM1800_ROUT1, reg | 0x100);
	reg = snd_soc_read(codec, WM1800_LOUT2);
	snd_soc_write(codec, WM1800_LOUT2, reg | 0x100);
	reg = snd_soc_read(codec, WM1800_ROUT2);
	snd_soc_write(codec, WM1800_ROUT2, reg | 0x100);

	reg = snd_soc_read(codec, WM1800_CLASSD1);
	snd_soc_write(codec, WM1800_CLASSD1, reg | 0x0C0);

	reg = snd_soc_read(codec, WM1800_LINVOL);
	snd_soc_write(codec, WM1800_LINVOL, reg & ~(0x080));
	reg = snd_soc_read(codec, WM1800_INBMIX1);
	snd_soc_write(codec, WM1800_INBMIX1, reg & ~(0x07e));

	
//	reg = snd_soc_read(codec, WM1800_LOUTMIX);
//	snd_soc_write(codec, WM1800_LOUTMIX, reg | ~(0x080));

    // line in
    // input3 gain
	snd_soc_write(codec, WM1800_INBMIX1, 0x070);                                            
	snd_soc_write(codec, WM1800_INBMIX2, 0x070);                                            

	//input boost gain
	//snd_soc_write(codec, WM1800_LINPATH, 0x128);                                            
	snd_soc_write(codec, WM1800_LINPATH, 0x20);                                            
	snd_soc_write(codec, WM1800_RINPATH, 0x20); 

	//noise gate control enable	
	//reg = snd_soc_read(codec, WM1800_NOISEG);
	//snd_soc_write(codec, WM1800_NOISEG, reg | 0x1); 


	//reg = snd_soc_read(codec, WM1800_POWER1);
	//snd_soc_write(codec, WM1800_POWER1, reg | 0x02a);
	//reg = snd_soc_read(codec, WM1800_LINPATH);
	//snd_soc_write(codec, WM1800_LINPATH, reg | 0x128);
	//reg = snd_soc_read(codec, WM1800_POWER3);
	//snd_soc_write(codec, WM1800_POWER3, reg | 0x020);
	

}

static int wm1800_probe(struct snd_soc_codec *codec)
{
	struct wm1800_priv *wm1800 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	ret = snd_soc_codec_set_cache_io(codec, 7, 9, wm1800->control_type);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	/* Get the codec into a known state */
	wm1800_reset(codec);

	codec->dapm.bias_level = SND_SOC_BIAS_OFF;

	wm1800_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	foo(codec);

	snd_soc_add_controls(codec, wm1800_snd_controls,
			     ARRAY_SIZE(wm1800_snd_controls));
	wm1800_add_widgets(codec);

	return 0;
}

/* power down chip */
static int wm1800_remove(struct snd_soc_codec *codec)
{
	wm1800_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}


/* TODO:
	wm1800_suspend
	wm1800_resume
*/

static struct snd_soc_codec_driver soc_codec_dev_wm1800 = {
	.probe		= wm1800_probe,
	.remove		= wm1800_remove,
	/*TODO:
	.suspend	= wm1800_suspend,
	.resume		= wm1800_resume,
	*/
	.set_bias_level = wm1800_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(wm1800_reg),
	.reg_word_size = sizeof(u16),
	.reg_cache_default = wm1800_reg,
};

static int wm1800_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct wm1800_priv *wm1800;
	int ret;

	wm1800 = kzalloc(sizeof(struct wm1800_priv), GFP_KERNEL);
	if (wm1800 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, wm1800);
	wm1800->control_type = SND_SOC_I2C;

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_wm1800, wm1800_dai, ARRAY_SIZE(wm1800_dai));
	if (ret < 0)
		kfree(wm1800);

	return ret;
}

static int wm1800_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id wm1800_i2c_id[] = {
	{ "wm1800", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm1800_i2c_id);

static struct i2c_driver wm1800_i2c_driver = {
	.driver = {
		.name = "wm1800-codec",
		.owner = THIS_MODULE,
	},
	.probe		= wm1800_i2c_probe,
	.remove		= wm1800_i2c_remove,
	.id_table	= wm1800_i2c_id,
};

static int __init wm1800_modinit(void)
{
	int ret;
	ret = i2c_add_driver(&wm1800_i2c_driver);
	if (ret != 0) {
		pr_err("Failed to register WM1800 I2C driver: %d\n", ret);
	}

	return ret;
}
module_init(wm1800_modinit);

static void __exit wm1800_exit(void)
{
	i2c_del_driver(&wm1800_i2c_driver);
}
module_exit(wm1800_exit);

MODULE_DESCRIPTION("ASoC WM1800 driver");
MODULE_AUTHOR("garamjin");
MODULE_LICENSE("GPL");
