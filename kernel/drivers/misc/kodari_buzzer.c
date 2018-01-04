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
#include <linux/buzzer_kodari.h>
#include <linux/slab.h>
#include <linux/pwm.h>
#include <linux/err.h>
#include <plat/gpio-cfg.h>


 #include "../staging/android/timed_output.h"



#define HZ_TO_NANOSECONDS(x) (1000000000UL/(x))
#define KODARI_BUZZER_MIN_ON 10


struct my_attr {
    struct attribute attr;
    int value;
};

/* ----------------------------  Sysfile1 status  ---------------------------- */
static struct kobject *mykobj1;
static int buzzer_hz;

static struct my_attr hz = {
    .attr.name="hz",
    .attr.mode = 0666,
    .value = 0,
};

static struct attribute * myattr1[] = {
    &hz.attr,
    NULL
};

static ssize_t hz_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct my_attr *a = container_of(attr, struct my_attr, attr);
	
    return sprintf(buf, "%d\n", buzzer_hz);
}


static ssize_t hz_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
     struct my_attr *a = container_of(attr, struct my_attr, attr);
    int ret;
    int value = 0;
    sscanf(buf, "%d", &a->value);
    value = a->value;
    printk("####### hz_store: value = %d\n", value);
	buzzer_hz = value; 
	printk("####### hz_store: buzzer_hz = %d\n", buzzer_hz); //path:/sys/Hz/hz 
    return sizeof(int);
}


static struct sysfs_ops myops1 = {
    .show = hz_show,
    .store = hz_store,
};

static struct kobj_type mytype1 = {
    .sysfs_ops = &myops1,
    .default_attrs = myattr1,
};

/* ----------------------------  Sysfile1 status  ---------------------------- */


 
enum buzzer_cmd_conf {
       BUZZER_CMD_PROBE,
       BUZZER_CMD_SUSPEND,
       BUZZER_CMD_RESUME,
       BUZZER_CMD_REMOVE,
};
 
struct kodari_buzzer_data {
	struct kodari_buzzer_platform_data *pdata;
	struct timed_output_dev timed_dev;
	struct hrtimer buzzer_timer;
	spinlock_t lock;
	bool suspended;
	bool on;
}; 


static int kodari_buzzer_gpio_enable(void){

	 printk(KERN_ERR "kodari_buzzer_gpio_enable in ");

	 s3c_gpio_cfgpin(EXYNOS4_GPD0(0), S3C_GPIO_SFN(2));  // Pwm function enable
	 
	 return 0;
}


static int kodari_buzzer_gpio_disenable(void){

	 printk(KERN_ERR "kodari_buzzer_gpio_disenable in ");

	 s3c_gpio_cfgpin(EXYNOS4_GPD0(0), S3C_GPIO_SFN(0));  // Pwm function disenable
 

	 return 0;
}

static void buzzer_on(struct kodari_buzzer_data *data)
{
	if(data != NULL){
        data->pdata->Hz = buzzer_hz;
		printk(KERN_ERR "### data->pdata->Hz = %d",data->pdata->Hz);		
		int period = HZ_TO_NANOSECONDS(data->pdata->Hz);
		kodari_buzzer_gpio_enable();
		pwm_config(data->pdata->buzzer_PWM, period / 2, period);
		pwm_enable(data->pdata->buzzer_PWM );    
	    data->on = true;
	}		
}

static void buzzer_off(struct kodari_buzzer_data *data)
{

	kodari_buzzer_gpio_disenable();
	pwm_config(data->pdata->buzzer_PWM, 1, 1);
	printk(KERN_ERR "### pwm_config");
	pwm_disable(data->pdata->buzzer_PWM);
		printk(KERN_ERR "### pwm_disable");
						
	data->on = false;
}

static void set_buzzer_Hz(struct kodari_buzzer_data *data, long Hz)
{

	if(data != NULL){
        data->pdata->Hz = Hz;
		printk(KERN_ERR "### set_buzzer_Hz = %d",Hz);
	}		
}

static void kodari_buzzer_enable(struct timed_output_dev *dev, int value)
{
	struct kodari_buzzer_data *data = container_of(dev, struct kodari_buzzer_data,
	                                timed_dev);
	unsigned long flags;

	if(value <= 0)
		return ;

	printk(KERN_ERR "### kodari_buzzer_enable function in");

	spin_lock_irqsave(&data->lock, flags);
	if (data->suspended)
	      goto out;

	if (data->on)
	      buzzer_off(data);

	hrtimer_cancel(&data->buzzer_timer);

	if (value >= 0 && value < KODARI_BUZZER_MIN_ON) {
	       if (value != 0 || data->on) {
			   if (!data->on) buzzer_on(data);
	           hrtimer_start(&data->buzzer_timer,
	           ktime_set(0, KODARI_BUZZER_MIN_ON *
	               				        NSEC_PER_MSEC), HRTIMER_MODE_REL);
	      }
	} else if (value > KODARI_BUZZER_MIN_ON) {
		   if (!data->on) buzzer_on(data);
	       hrtimer_start(&data->buzzer_timer,
	       ktime_set(value / MSEC_PER_SEC, (value % MSEC_PER_SEC)
	                              * NSEC_PER_MSEC), HRTIMER_MODE_REL);

	}	
	
out:
    spin_unlock_irqrestore(&data->lock, flags);

}

 
static int kodari_buzzer_get_time(struct timed_output_dev *dev)
{
	struct kodari_buzzer_data *data = container_of(dev, struct kodari_buzzer_data,timed_dev);
	 int remain;
	 if (hrtimer_active(&data->buzzer_timer)) {
			 ktime_t r = hrtimer_get_remaining(&data->buzzer_timer);
			 remain = (int)ktime_to_ms(r);
			 return remain > 0 ? remain : 0;
		 } else {
			 return 0;
	 }
}
 


 
static enum hrtimer_restart buzzer_timer_func(struct hrtimer *timer)
{
	 struct kodari_buzzer_data *data = container_of(timer, struct kodari_buzzer_data,
										  buzzer_timer);
	 buzzer_off(data);
	 return HRTIMER_NORESTART;
}


 static int kodari_buzzer_configure(struct kodari_buzzer_data *data, int val)
 {
	struct kodari_buzzer_platform_data *pdata = data->pdata;
	// 		 struct lc898300_vib_cmd *vib_cmd_info = pdata->vib_cmd_info;
	int rc = 0;
	unsigned long flags;

	printk(KERN_ERR "### buzzer_configure open");

	if (val == BUZZER_CMD_PROBE || val == BUZZER_CMD_RESUME) {
		 if (val == BUZZER_CMD_PROBE) {
			
			 // configure beep gpio
			//EXYNOS4_GPC0(0)
			//pwm register

			pdata->buzzer_PWM = kzalloc(sizeof(pdata->buzzer_PWM), GFP_KERNEL);

  			printk(KERN_ERR "#################	check buzzer_PWM result" );
  
  			
  			if (!pdata->buzzer_PWM){
  				pr_info("pwm allocation open error");
  			}
  
  			pdata->buzzer_PWM=pwm_request(0 ,"KODARI_MORTOR_pwm");
  			if (IS_ERR(pdata->buzzer_PWM)) {
  				rc = PTR_ERR(pdata->buzzer_PWM);
  				pr_info(" kodari_buzzer.c =>  pwm open error=%d\n",rc);
  				pdata->buzzer_PWM = NULL;
  			}else {
  				printk(KERN_ERR "#################  found buzzer_PWM device \n");
				buzzer_hz = pdata->Hz;
		//		set_buzzer_Hz(data, buzzer_hz);
  			}
			  
  			pwm_disable(pdata->buzzer_PWM);
  			//s3c_gpio_cfgpin(EXYNOS4_GPD0(0), S3C_GPIO_SFN(2));  // Pwm function enable
  			
			data->suspended = false;
		 } 
	} else if (val == BUZZER_CMD_REMOVE || val == BUZZER_CMD_SUSPEND) {
		 spin_lock_irqsave(&data->lock, flags);
		 
		 hrtimer_cancel(&data->buzzer_timer);

		 buzzer_off(data);

		 data->suspended = true;

		 spin_unlock_irqrestore(&data->lock, flags); 

		 if (val == BUZZER_CMD_REMOVE) {
				pwm_disable(pdata->buzzer_PWM);
				pwm_free(pdata->buzzer_PWM);
		 }
	}

	return rc;
error:
	pwm_disable(pdata->buzzer_PWM);
	pwm_free(pdata->buzzer_PWM);
	return rc;
 }


 
 static int kodari_buzzer_probe(struct platform_device *pdev)
 {
	 struct kodari_buzzer_data *data;
     int rc;

	 printk(KERN_ERR "buzzer -  %s ", __FUNCTION__);

	 if(!pdev->dev.platform_data) return -EIO;
	 
	 data = kzalloc(sizeof(struct kodari_buzzer_data), GFP_KERNEL);
	 if (!data)
		 return -ENOMEM;


	 printk(KERN_ERR "buzzer - %s 2 ", __FUNCTION__);


	 data->pdata = pdev->dev.platform_data;

	 
	  rc = kodari_buzzer_configure(data, BUZZER_CMD_PROBE);
	  if (rc) {
	 	 printk(KERN_ERR "buzzer - Configure failed rc = %d\n", rc);
	 	 goto error;
	  }

	  printk(KERN_ERR "buzzer - Configure end ");


//	  if(data->pdata->buzzer_PWM!= NULL) {
//		rc=pwm_config(data->pdata->buzzer_PWM, 100000, 200000);
//		rc=pwm_config(data->pdata->buzzer_PWM, 100000, 200000);
//		printk(KERN_ERR "pwm_config ret %d Buzzer_PWM=0x%x", rc, data->pdata->buzzer_PWM);
//		if(rc){
//			printk(KERN_ERR "pwm config error=%d",rc);
//			return -1;
//		}
//		rc = pwm_enable(data->pdata->buzzer_PWM);
//		printk(KERN_ERR "pwm_enable ret %d\n", rc);
//	}			
    
	hrtimer_init(&data->buzzer_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	
	data->buzzer_timer.function = buzzer_timer_func;
	data->timed_dev.name = data->pdata->android_dev_name;
	data->timed_dev.get_time = kodari_buzzer_get_time;
	data->timed_dev.enable = kodari_buzzer_enable;
	rc = timed_output_dev_register(&data->timed_dev);
	
	if (rc < 0) {
		printk(KERN_ERR "buzzer - %s Unalbe to register timed output dev rc = %d", data->pdata->android_dev_name, rc);
		goto error; 
	}

     return 0;
 
 error:
  
	kfree(data);
  	return rc;    
 }
 
 static int __devexit kodari_buzzer_remove(struct platform_device *pdev)
 {

 	printk(KERN_ERR " kodari_buzzer_remove open");
	 
	struct kodari_buzzer_data *data = pdev->dev.platform_data;
	kodari_buzzer_configure(data, BUZZER_CMD_REMOVE);
	hrtimer_cancel(&data->buzzer_timer);
	timed_output_dev_unregister(&data->timed_dev);
	kfree(data);
	return 0;
 }


 
 static int kodari_buzzer_resume(struct device *dev)
 {
 /*
 	 printk(KERN_ERR " kodari_buzzer_resume open ");
	 
 	 struct kodari_buzzer_data *data = dev_get_drvdata(dev);
 	 int rc;
 	 rc = kodari_buzzer_configure(data, BUZZER_CMD_RESUME);
 	return rc;
 */
 	return 0;
 }
 
 
 static int kodari_buzzer_suspend(struct device *dev)
 { 	
/*
	 printk(KERN_ERR " kodari_buzzer_suspend open ");
	 
	 struct kodari_buzzer_data *data = dev_get_drvdata(dev);
 	 int rc;
 	 rc = kodari_buzzer_configure(data, BUZZER_CMD_SUSPEND);
 	 return rc;
 */
 	return 0;
 }
 
 
 static const struct dev_pm_ops kodari_buzzer_pm_ops = {
 	 .suspend = kodari_buzzer_suspend,
 	 .resume = kodari_buzzer_resume,
 };
 
 static struct platform_driver kodari_buzzer_driver = {
         .probe          = kodari_buzzer_probe,
         .remove         = __devexit_p(kodari_buzzer_remove),
         .driver         = {
                 .name   = "kodari_buzzer",
                 .owner  = THIS_MODULE,
                 .pm	 = &kodari_buzzer_pm_ops,
         },
 };


MODULE_ALIAS("platform:kodari_buzzer");
 
 static int __init kodari_buzzer_init(void)
 {

 	mykobj1 = kzalloc(sizeof(*mykobj1), GFP_KERNEL);
	if (mykobj1) {
		kobject_init(mykobj1, &mytype1);
		if (kobject_add(mykobj1, NULL, "%s", "Hz")) {
			printk("kodari_type: kobject_add() failed\n");
			kobject_put(mykobj1);
			mykobj1 = NULL;
		}
	}
	
	 printk(KERN_ERR "buzzer - %s ", __FUNCTION__);
	 return platform_driver_register(&kodari_buzzer_driver);
 }
 
 static void __exit kodari_buzzer_exit(void)
 {
	 printk(KERN_ERR "buzzer - %s ", __FUNCTION__);
	 platform_driver_unregister(&kodari_buzzer_driver);
 }
 
 module_init(kodari_buzzer_init);
 module_exit(kodari_buzzer_exit);
 
 MODULE_AUTHOR("Raphael Assenat <raph@8d.com>, Trent Piepho <tpiepho@freescale.com>");
 MODULE_DESCRIPTION("buzzer driver");
 MODULE_LICENSE("GPL");

 
 MODULE_ALIAS("platform:kodari_buzzer");
 MODULE_DESCRIPTION("kodari buzzer driver");
 MODULE_LICENSE("GPL v2");
 MODULE_AUTHOR("Lee Hyung Chul <hclee@sstd.co.kr>");

