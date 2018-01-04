/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 and
  * only version 2 as published by the Free Software Foundation.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  */
 
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/vib_kodari.h>
#include <linux/slab.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/err.h>
#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
 #include "../staging/android/timed_output.h"

#define KODARI_VIB_MIN_ON 10
#define DUG 0;

 
enum vib_cmd_conf {
       VIB_CMD_PROBE,
       VIB_CMD_SUSPEND,
       VIB_CMD_RESUME,
       VIB_CMD_REMOVE,
};
 
struct kodari_vib_data {
	struct kodari_vib_platform_data *pdata;
	struct timed_output_dev timed_dev;
	struct hrtimer vib_timer;
	spinlock_t lock;
	bool suspended;
	bool on;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};
 
 #ifdef CONFIG_HAS_EARLYSUSPEND
static void kodari_vib_early_syuspend(struct early_suspend *h);
static void kodari_vib_late_resume(struct early_suspend *h);
#endif

static void vib_on(struct kodari_vib_data *data)
{

	
	//printk(KERN_ERR "### Vibrator vib_on\n");
	

    gpio_set_value(data->pdata->vibrator_gpio, 1);
    data->on = true;
}

static void vib_off(struct kodari_vib_data *data)
{

//	printk(KERN_ERR "### Vibrator vib_off\n");

	gpio_set_value(data->pdata->vibrator_gpio, 0);
	data->on = false;
}


static void kodari_vib_enable(struct timed_output_dev *dev, int value)
{
	struct kodari_vib_data *data = container_of(dev, struct kodari_vib_data,
	                                timed_dev);
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	if (data->suspended)
	      goto out;
	hrtimer_cancel(&data->vib_timer);
	if (value >= 0 && value < KODARI_VIB_MIN_ON) {
	       if (value != 0 || data->on) {
	               if (!data->on) vib_on(data);
	               hrtimer_start(&data->vib_timer,
	               ktime_set(0, KODARI_VIB_MIN_ON *
	                               NSEC_PER_MSEC), HRTIMER_MODE_REL);
	      }
	} else if (value > KODARI_VIB_MIN_ON) {
	       if (!data->on) vib_on(data);
	       hrtimer_start(&data->vib_timer,
	       ktime_set(value / MSEC_PER_SEC, (value % MSEC_PER_SEC)
	                              * NSEC_PER_MSEC), HRTIMER_MODE_REL);

	}
out:
    spin_unlock_irqrestore(&data->lock, flags);

}

 
static int kodari_vib_get_time(struct timed_output_dev *dev)
{
		 struct kodari_vib_data *data = container_of(dev, struct kodari_vib_data,
														  timed_dev);
	 int remain;
	 if (hrtimer_active(&data->vib_timer)) {
			 ktime_t r = hrtimer_get_remaining(&data->vib_timer);
			 remain = (int)ktime_to_ms(r);
			 return remain > 0 ? remain : 0;
		 } else {
			 return 0;
	 }
}
 


 
static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	 struct kodari_vib_data *data = container_of(timer, struct kodari_vib_data,
										  vib_timer);
	 vib_off(data);
	 return HRTIMER_NORESTART;
}


 static int kodari_vib_configure(struct kodari_vib_data *data, int val)
 {
	struct kodari_vib_platform_data *pdata = data->pdata;
	// 		 struct lc898300_vib_cmd *vib_cmd_info = pdata->vib_cmd_info;
	int rc = 0;
	unsigned long flags;

	if (val == VIB_CMD_PROBE || val == VIB_CMD_RESUME) {
		 if (val == VIB_CMD_PROBE) {
#if 1
			struct regulator *regulator;
			int ret = 0;

			regulator = regulator_get(NULL, "MOTOR_1V3");
			if (IS_ERR(regulator)) {
			printk("[%s] regulator_get fail : MOTOR_1V3\n", __func__);
			return -ENODEV;
			}

			ret = regulator_enable(regulator);
			if(ret < 0)
			printk("[%s] regulator_enable fail : MOTOR_1V3\n", __func__);

			regulator_put(regulator);

			mdelay(10);
#endif
			 // configure vibrator gpio
			//EXYNOS4_GPC0(0)
		    rc = gpio_request_one(pdata->vibrator_gpio, GPIOF_OUT_INIT_LOW, "kodari vibrator");	
			if (rc) {
				printk(KERN_ERR "### Vibrator GPIO open fail~~~~ \n");
				goto error;
			}		 
			rc = gpio_direction_output(pdata->vibrator_gpio, 0);
			if (rc) {
				printk(KERN_ERR "### Vibrator GPIO open fail~~~~ \n");
				goto error;
			} 					
			data->suspended = false;
			printk(KERN_ERR "%s , val=%d\n", __FUNCTION__, val);
		 } 
	} else if (val == VIB_CMD_REMOVE || val == VIB_CMD_SUSPEND) {
		 spin_lock_irqsave(&data->lock, flags);
		 hrtimer_cancel(&data->vib_timer);
		 vib_off(data);
		 data->suspended = true;
		 spin_unlock_irqrestore(&data->lock, flags); 
		 if (val == VIB_CMD_REMOVE) {
				 gpio_free(pdata->vibrator_gpio);
		 }
	}
	return rc;
error:
	gpio_free(pdata->vibrator_gpio);
	return rc;
 }

 
 static int kodari_vib_probe(struct platform_device *pdev)
 {
	 struct kodari_vib_data *data;
     int rc;

	 printk(KERN_ERR "%s \n", __FUNCTION__);

	 if(!pdev->dev.platform_data) return -EIO;
	 
	 data = kzalloc(sizeof(struct kodari_vib_data), GFP_KERNEL);
	 if (!data)
		 return -ENOMEM;


	 data->pdata = pdev->dev.platform_data;

	  rc = kodari_vib_configure(data, VIB_CMD_PROBE);
	  if (rc) {
	 	 goto error;
	  }

	hrtimer_init(&data->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	
	data->vib_timer.function = vibrator_timer_func;
	data->timed_dev.name = data->pdata->android_dev_name;
	data->timed_dev.get_time = kodari_vib_get_time;
	data->timed_dev.enable = kodari_vib_enable;

	rc = timed_output_dev_register(&data->timed_dev);

	if (rc < 0) {
		goto error; 
	}
	
	platform_set_drvdata(pdev, data);

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = kodari_vib_early_syuspend;
	data->early_suspend.resume = kodari_vib_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	 printk(KERN_ERR "%s  kodari_vib_configure end \n", __FUNCTION__);

     return 0;
 
 error:
  
	kfree(data);
  	return rc;    
 }
 
 static int __devexit kodari_vib_remove(struct platform_device *pdev)
 {
	struct kodari_vib_data *data = pdev->dev.platform_data;
	kodari_vib_configure(data, VIB_CMD_REMOVE);
	hrtimer_cancel(&data->vib_timer);
	timed_output_dev_unregister(&data->timed_dev);
	kfree(data);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	return 0;
 }

#ifdef CONFIG_HAS_EARLYSUSPEND
static void kodari_vib_early_syuspend(struct early_suspend *h)
{
        struct regulator *regulator;
	int ret = 0;

	regulator = regulator_get(NULL, "MOTOR_1V3");
	if (IS_ERR(regulator)) {
		printk("[%s] regulator_get fail : MOTOR_1V3\n", __func__);
		return ;
	}

	if (regulator_is_enabled(regulator)) {
	ret = regulator_force_disable(regulator);
	if(ret < 0)
		printk("[%s] regulator_force_disable fail : MOTOR_1V3\n", __func__);
	else
		printk("[%s] regulator_force_disable success : MOTOR_1V3\n", __func__);
	}

	regulator_put(regulator);
}

static void kodari_vib_late_resume(struct early_suspend *h)
{
         struct regulator *regulator;
	int ret = 0;

	regulator = regulator_get(NULL, "MOTOR_1V3");
	if (IS_ERR(regulator)) {
		printk("[%s] regulator_get fail : MOTOR_1V3\n", __func__);
		return ;
	}

	ret = regulator_enable(regulator);
	if(ret < 0)
		printk("[%s] regulator_enable fail : MOTOR_1V3\n", __func__);
	else
		printk("[%s] regulator_enable success : MOTOR_1V3\n", __func__);

	regulator_put(regulator);

	mdelay(10);
}
#endif
 
 static int kodari_vib_resume(struct device *dev)
 {
 	 struct kodari_vib_data *data = dev_get_drvdata(dev);
 	 int rc;
 	 rc = kodari_vib_configure(data, VIB_CMD_RESUME);
 	 return rc;
 }
 
 
 static int kodari_vib_suspend(struct device *dev)
 { 	
	 struct kodari_vib_data *data = dev_get_drvdata(dev);
 	 int rc;
 	 rc = kodari_vib_configure(data, VIB_CMD_SUSPEND);
 	 return rc;
 }
 
 
 static const struct dev_pm_ops kodari_vib_pm_ops = {
 	 .suspend = kodari_vib_suspend,
 	 .resume = kodari_vib_resume,
 };
 
 static struct platform_driver kodari_vib_driver = {
         .probe          = kodari_vib_probe,
         .remove         = __devexit_p(kodari_vib_remove),
         .driver         = {
                 .name   = "kodari_vibrator",
                 .owner  = THIS_MODULE,
                 //.pm	 = &kodari_vib_pm_ops,
         },
 };


MODULE_ALIAS("platform:kodari_vibrator");
 
 static int __init kodari_vib_init(void)
 {
	 printk(KERN_ERR "%s \n", __FUNCTION__);
	 return platform_driver_register(&kodari_vib_driver);
 }
 
 static void __exit kodari_vib_exit(void)
 {
	 printk(KERN_ERR "%s \n", __FUNCTION__);
	 platform_driver_unregister(&kodari_vib_driver);
 }
 
 module_init(kodari_vib_init);
 module_exit(kodari_vib_exit);
 
 MODULE_AUTHOR("Raphael Assenat <raph@8d.com>, Trent Piepho <tpiepho@freescale.com>");
 MODULE_DESCRIPTION("GPIO LED driver");
 MODULE_LICENSE("GPL");

 
 MODULE_ALIAS("platform:kodari_vib");
 MODULE_DESCRIPTION("kodari vibrator driver");
 MODULE_LICENSE("GPL v2");
 MODULE_AUTHOR("Lee Hyung Chul <hclee@sstd.co.kr>");

