/*
 *  smdkv2xx_wm1800.c
 *
 *  Copyright (c) 2009 Samsung Electronics Co. Ltd
 *  Author: Jaswinder Singh <jassi.brar@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <mach/regs-clock.h>
#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-exynos4.h>
#include "../codecs/wm8960.h"
#include "i2s.h"

#include "s3c-i2s-v2.h"

#include <asm/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/kthread.h>

//#define USE_EAR_JACK_DETECT

#ifdef USE_EAR_JACK_DETECT
#define EAR_DELAY			1

#define GPIO_EAR_JACK_DET	EXYNOS4_GPX3(0)
static DECLARE_WAIT_QUEUE_HEAD(idle_wait);
static struct task_struct *EarJackTask;
static int earjack_thread(void *data);

extern int wm8960_set_outpath(struct snd_soc_dai *codec_dai, u8 out_path);
#endif

static const struct snd_soc_dapm_widget wm8960_dapm_widgets_cpt[] = {
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
};

static const struct snd_soc_dapm_widget wm8960_dapm_widgets_cpt2[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
};

static const struct snd_soc_dapm_route audio_map_tx[] = {
	{"Headphone Jack", NULL, "HP_L"},
	{"Headphone Jack", NULL, "HP_R"},
	{"LINPUT3", NULL, "MICB"},
	{"MICB", NULL, "Mic Jack"},
};



static int kodari_wm8960_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	static int kthread_ok = 0;

	snd_soc_dapm_nc_pin(dapm, "RINPUT1");
	snd_soc_dapm_nc_pin(dapm, "LINPUT1");
	snd_soc_dapm_nc_pin(dapm, "LINPUT2");
	snd_soc_dapm_nc_pin(dapm, "RINPUT2");
	snd_soc_dapm_nc_pin(dapm, "OUT3");
	snd_soc_dapm_new_controls(dapm, wm8960_dapm_widgets_cpt,
			ARRAY_SIZE(wm8960_dapm_widgets_cpt));
	snd_soc_dapm_new_controls(dapm, wm8960_dapm_widgets_cpt2,
			ARRAY_SIZE(wm8960_dapm_widgets_cpt2));

	snd_soc_dapm_add_routes(dapm, audio_map_tx, ARRAY_SIZE(audio_map_tx));

	snd_soc_dapm_enable_pin(dapm, "Headphone Jack");
	snd_soc_dapm_enable_pin(dapm, "Mic Jack");
	snd_soc_dapm_sync(dapm);

#ifdef USE_EAR_JACK_DETECT
	if(!kthread_ok){
		EarJackTask = kthread_run(earjack_thread, codec, "ear_detect");
		if(!IS_ERR(EarJackTask)) {
			printk(" Ear Jack Poll Task Started\n");
		}
		kthread_ok = 1;
	}
#endif
	return 0;
}



static int kodari_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	int ret, pre, pre_n, rfs, bfs, codec_pre;

	struct clk *clk_epll;
	unsigned int pll_out;
	int psr;

	
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S20_3LE:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		printk(KERN_INFO "%s:params_format(bfs) INVAL\n", __func__);
		return -EINVAL;
	}


	switch (params_rate(params)) {
	case 12000:
	case 11025:
	case 8000:
		if (bfs == 0x30)
			rfs = 0x300;
		else
			rfs = 0x200;

		break;

	case 22050:
	case 16000:
	case 44100:
	case 32000:
	case 96000:
	case 88200:
	case 48000:
	case 24000:
		if (bfs == 0x30)
			rfs = 0x180;
		else
			rfs = 0x100;

		break;
	case 64000:
		rfs = 0x180;
		break;

	default:
		printk(KERN_INFO "%s:params_rate(rfs) INVAL\n", __func__);
		return -EINVAL; 
	}
	pll_out = params_rate(params) * rfs;	
	switch (pll_out) {		
	case 4096000:
	case 5644800:		
	case 6144000:		
	case 8467200:		
	case 9216000: psr = 8; break;
	case 8192000:		
	case 11289600:		
	case 12288000:		
	case 16934400:		
	case 18432000: psr = 4; break;
	case 22579200:		
	case 24576000:		
	case 33868800:		
	case 36864000: psr = 2; break;
	case 67737600:		
	case 73728000: psr = 1; break;
	default:			
		printk(KERN_ERR "Not yet supported!\n");
	return -EINVAL;
	}


	clk_epll = clk_get(NULL, "fout_epll");	
	if(IS_ERR(clk_epll)) {		
		printk(KERN_ERR "failed to get fount_epll\n");		
		return -EBUSY;	
	}

	clk_set_rate(clk_epll, pll_out*psr);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
                                         | SND_SOC_DAIFMT_NB_NF
                                         | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		printk(KERN_INFO "%s:Codec DAI configuration error, %d\n",
			__func__, ret);
		goto exit;
	}

        ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
                                         | SND_SOC_DAIFMT_NB_NF
                                         | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		printk(KERN_INFO "%s:AP configuration error, %d\n",
			__func__, ret);
		goto exit;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_0,
			params_rate(params), SND_SOC_CLOCK_OUT);

	if (ret < 0) {
		printk(KERN_INFO "%s: AP sysclk CDCLK setting error,%d\n",
			__func__, ret);
		goto exit;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, 0x400, params_rate(params),
				SND_SOC_CLOCK_OUT);

	if (ret < 0) {
		printk(KERN_INFO "%s: AP sysclk CLKAUDIO setting error,%d\n",
			__func__, ret);
		goto exit;
	}

	if (params_rate(params) & 0xF)
		pre_n = 0x4099990;
	else
		pre_n = 0x2ee0000;

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_SYSCLKDIV, 0);

	if (ret < 0) {
		printk(KERN_INFO "%s: Codec SYSCLKDIV setting error, %d\n",
			__func__, ret);
		goto exit;
	}

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_DACDIV, 0);

	if (ret < 0) {
		printk(KERN_INFO "%s: Codec DACDIV setting error, %d\n",
                        __func__, ret);
                goto exit;
	}

	codec_pre = params_rate(params) * rfs;
	snd_soc_dai_set_clkdiv(codec_dai, WM8960_DCLKDIV, codec_pre);

	pre = pre_n / codec_pre - 1;
	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_PRESCALER, pre);

	if (ret < 0) {
		printk(KERN_INFO "%s: AP prescalar setting error,%d\n",
			__func__, ret);
		goto exit;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_RCLK, rfs);
	if (ret < 0) {
		printk(KERN_INFO "%s: AP RFS setting error, %d\n",
			__func__, ret);
		goto exit;
	}

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_BCLK, bfs);
	if (ret < 0) {
		printk(KERN_INFO "%s: AP BFS setting error, %d\n",
			__func__, ret);
		goto exit;
	}
	
	clk_put(clk_epll);

	return 0;

exit:

	clk_put(clk_epll);
	return ret;	

	return 0;
}


static struct snd_soc_ops kodari_ops = {
	.hw_params = kodari_hw_params,
};


static struct snd_soc_dai_link kodari_dai = {
	.name		= "KODARI-AUDIO",
	.stream_name	= "WM8960 HiFi",
	.codec_name	= "wm8960-codec.0-001a",
	.platform_name	= "samsung-audio",
	.cpu_dai_name	= "samsung-i2s.0",
	.codec_dai_name	= "wm8960-hifi",
	.init		= kodari_wm8960_init,
	.ops		= &kodari_ops,
};


static struct snd_soc_card kodari = {
	.name = "KODARI-I2S",
	.dai_link = &kodari_dai,
	.num_links = 1,
};


#ifdef USE_EAR_JACK_DETECT
static int earjack_thread(void *data)
{
	int ret;
	u8 value, pvalue = 0xff;
	struct snd_soc_codec *codec = (struct snd_soc_codec *)data;
	
//    s3c_gpio_cfgpin(EXYNOS4_GPX3(0), S3C_GPIO_SFN(0xF));
 //   s3c_gpio_setpull(EXYNOS4_GPX3(0), S3C_GPIO_PULL_UP);

	printk(KERN_ERR "\t#earjack_thread\n");


	s3c_gpio_cfgpin(GPIO_EAR_JACK_DET, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_EAR_JACK_DET, S3C_GPIO_PULL_DOWN);

	do {
		ret = interruptible_sleep_on_timeout(&idle_wait, HZ*EAR_DELAY);

		if(!ret) {
			value = gpio_get_value(GPIO_EAR_JACK_DET);
			if(value != pvalue) {
				printk(KERN_ERR "\t#Ear Jack Detect : value=%d\n", value);
				if(value)
				{
					wm8960_set_outpath(codec, WM8960_OUT_EAR);
					printk(KERN_ERR "\theadphone value=%d\n", value);				
				}
				else
				{
					wm8960_set_outpath(codec, WM8960_OUT_SPK_MUTE);
					printk(KERN_ERR "\tspeak value=%d\n", value);
				}
				pvalue = value;
			}
		}
	} while(!kthread_should_stop());
	return 0;
}
#endif



static struct platform_device *kodari_snd_device;

static int __init kodari_audio_init(void)
{
	int ret;

	printk(KERN_ERR "kodari_audio_init!!!!\n");

	kodari_snd_device = platform_device_alloc("soc-audio", -1);
	if (!kodari_snd_device) {
		printk(KERN_ERR "%s: failed to audio device alloc\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(kodari_snd_device, &kodari);

	ret = platform_device_add(kodari_snd_device);
	if (ret) {
			printk(KERN_ERR "platform_device_put!!!!\n");
		platform_device_put(kodari_snd_device);
	}

	printk(KERN_ERR "platform_device_add %i !!!!\n", ret);

	return ret;
}



static void __exit kodari_audio_exit(void)
{
	platform_device_unregister(kodari_snd_device);
}

module_init(kodari_audio_init);
module_exit(kodari_audio_exit);

MODULE_AUTHOR("Jaswinder Singh, jassi.brar@samsung.com");
MODULE_DESCRIPTION("ALSA SoC SMDK64XX WM8960");
MODULE_LICENSE("GPL");
