/*
 * kodari_micom_bmodule.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/bcd.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/rtc.h>
#include <linux/mfd/kodari/kodari-micom-core.h>
#if 0 //def CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

struct bmodule_info {
	struct device		*dev;
	struct kodari_micom_dev *kodaridev;
#if 0 //def CONFIG_HAS_EARLYSUSPEND
        struct early_suspend early_suspend;
#endif
};

#if 0 //def CONFIG_HAS_EARLYSUSPEND
static void bmodule_early_suspend(struct early_suspend *h);
static void bmodule_late_resume(struct early_suspend *h);
#endif

struct i2c_client *bmodule_i2c_client;

static const struct platform_device_id kodari_bmodule_id[] = {
	{ "kodari-micom-bmodule", 0 },
};

static int kodari_irq_status = -1;

struct my_attr {
    struct attribute attr;
    int value;
};

/* ------------------  Sysfile1: /sys/bmodule_irq/irq  ------------------ */
static struct kobject *mykobj;

static struct my_attr irq = {
    .attr.name="irq",
    .attr.mode = 0444,
    .value = 0,
};

static struct attribute * myattr[] = {
    &irq.attr,
    NULL
};

static ssize_t irq_status_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    //struct my_attr *a = container_of(attr, struct my_attr, attr);
	
    return sprintf(buf, "%d\n", kodari_irq_status);
}

/*
static ssize_t irq_status_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
    struct my_attr *a = container_of(attr, struct my_attr, attr);

    sscanf(buf, "%d", &a->value);
    irq.value = a->value;
    kodari_irq_status = a->value;
    sysfs_notify(mykobj, NULL, "irq"); // sysfile update

    return sizeof(int);
}
*/

static struct sysfs_ops myops = {
    .show = irq_status_show,
    //.store = irq_status_store,
};

static struct kobj_type mytype = {
    .sysfs_ops = &myops,
    .default_attrs = myattr,
};

/* ------------------  Sysfile2: /sys/bmodule_led/led  ------------------ */
static struct kobject *mykobj2;
int kodari_blind_mode;

static struct my_attr BModuleLed = {
    .attr.name="led",
    .attr.mode = 0666,
    .value = 0,
};

static struct attribute * myattr2[] = {
    &BModuleLed.attr,
    NULL
};

static ssize_t bmodule_led_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct my_attr *a = container_of(attr, struct my_attr, attr);
	
    return sprintf(buf, "%d\n", a->value);
}

static ssize_t bmodule_led_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
    struct my_attr *a = container_of(attr, struct my_attr, attr);
    int ret;
    int mode = 0, red = 0, green = 0, blue = 0;
    u8 value = 0;

    sscanf(buf, "%d", &a->value);

    // 0: user-controll off, 1: user-controll on, 2: blind-mode enter, 3: blind-mode exit, 4: led-disable-mode, 5: led-enable-mode
    mode = (a->value) & 0xf000000;

    red = (a->value) & 0xf0000;
    green = (a->value) & 0xf00;
    blue = (a->value) & 0xf;

    if(mode == 0x2000000) {
        BModuleLed.value = 1;
        kodari_blind_mode = 1;
        //sysfs_notify(mykobj2, NULL, "led");
    }

    if(mode == 0x3000000) {
        BModuleLed.value = 0;
        kodari_blind_mode = 0;
        //sysfs_notify(mykobj2, NULL, "led");
    }

    if(mode == 0x1000000)
        value |= 1 << 7;  // controlled by HOST
	
    if(mode == 0x2000000)
        value |= 1 << 6;  // blind-mode on

    if(mode == 0x4000000)
        value |= 1 << 5;  // led-disable-mode

    if(mode == 0x5000000)
        value |= 1 << 4;  // led-enable-mode

    if(blue)
        value |= 1 << 2;  // blue led on

    if(green)
        value |= 1 << 1;  // green led on

    if(red)
        value |= 1;  // red led on

    printk("########## bmodule_led: value = 0x%x\n", value);
    ret = kodari_micom_reg_write(bmodule_i2c_client, KODARI_BMODULE_LED_CONTROL, value);
	
    if (ret < 0)
        printk("kodari_micom_reg_write() -- KODARI_BMODULE_LED_CONTROL fail\n");

    return sizeof(int);
}

static struct sysfs_ops myops2 = {
    .show = bmodule_led_show,
    .store = bmodule_led_store,
};

static struct kobj_type mytype2 = {
    .sysfs_ops = &myops2,
    .default_attrs = myattr2,
};

/* ------------------  Sysfile3: /sys/bmodule_status/status  ------------------ */
static struct kobject *mykobj3;

static struct my_attr status = {
    .attr.name="status",
    .attr.mode = 0444,
    .value = 0,
};

static struct attribute * myattr3[] = {
    &status.attr,
    NULL
};

static ssize_t bmodule_status_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    //struct my_attr *a = container_of(attr, struct my_attr, attr);
	
    return sprintf(buf, "%d\n", kodari_irq_status);
}

/*
static ssize_t bmodule_status_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
    struct my_attr *a = container_of(attr, struct my_attr, attr);

    sscanf(buf, "%d", &a->value);
    status.value = a->value;
    kodari_irq_status = a->value;
    sysfs_notify(mykobj3, NULL, "status");

    return sizeof(int);
}
*/

static struct sysfs_ops myops3 = {
    .show = bmodule_status_show,
    //.store = bmodule_status_store,
};

static struct kobj_type mytype3 = {
    .sysfs_ops = &myops3,
    .default_attrs = myattr3,
};

/* ------------------  Sysfile4: /sys/bmodule_monitor/monitor  ------------------ */
static struct kobject *mykobj4;

static struct my_attr monitor = {
    .attr.name="monitor",
    .attr.mode = 0666,
    .value = 0,
};

static struct attribute * myattr4[] = {
    &monitor.attr,
    NULL
};

static ssize_t bmodule_monitor_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct my_attr *a = container_of(attr, struct my_attr, attr);
	
    return sprintf(buf, "%d\n", a->value);
}

static ssize_t bmodule_monitor_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
    int ret = 0;
    struct my_attr *a = container_of(attr, struct my_attr, attr);

    sscanf(buf, "%d", &a->value);
    monitor.value = a->value;

    if(a->value == 1)
        ret = kodari_micom_reg_write(bmodule_i2c_client, KODARI_BMODULE_MONITOR_START, 1);

    if (ret < 0)
        printk("bmodule_monitor_store() -- kodari_micom_reg_write fail\n");

    return sizeof(int);
}

static struct sysfs_ops myops4 = {
    .show = bmodule_monitor_show,
    .store = bmodule_monitor_store,
};

static struct kobj_type mytype4 = {
    .sysfs_ops = &myops4,
    .default_attrs = myattr4,
};

/* ------------------  Sysfile5: sys/bmodule_power/power  ------------------ */
static struct kobject *mykobj5;

static struct my_attr power = {
    .attr.name="power",
    .attr.mode = 0666,
    .value = 0,
};

static struct attribute * myattr5[] = {
    &power.attr,
    NULL
};

static ssize_t bmodule_power_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct my_attr *a = container_of(attr, struct my_attr, attr);
	
    return sprintf(buf, "%d\n", a->value);
}

static ssize_t bmodule_power_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
    int ret = 0;
    struct my_attr *a = container_of(attr, struct my_attr, attr);

    sscanf(buf, "%d", &a->value);
    power.value = a->value;

    if(a->value == 1)
        ret = kodari_micom_reg_write(bmodule_i2c_client, KODARI_BMODULE_POWER_CONTROL, 1);
    else
        ret = kodari_micom_reg_write(bmodule_i2c_client, KODARI_BMODULE_POWER_CONTROL, 0);

    if (ret < 0)
        printk("bmodule_power_store() -- kodari_micom_reg_write fail: %d\n", a->value);

    return sizeof(int);
}

static struct sysfs_ops myops5 = {
    .show = bmodule_power_show,
    .store = bmodule_power_store,
};

static struct kobj_type mytype5 = {
    .sysfs_ops = &myops5,
    .default_attrs = myattr5,
};

/* ------------------  Sysfile6: /sys/emergency_erase/reason  ------------------ */
#if 0
static struct kobject *mykobj6;

static struct my_attr reason = {
    .attr.name="reason",
    .attr.mode = 0444,
    .value = 0,
};

static struct attribute * myattr6[] = {
    &reason.attr,
    NULL
};

static ssize_t emergency_erase_reason_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    //struct my_attr *a = container_of(attr, struct my_attr, attr);
    int ret = 0;
    u8 regval = 0;

    ret = kodari_micom_reg_read(bmodule_i2c_client, KODARI_EMERGENCY_ERASE_REASON_REQUEST, &regval);

    if(ret < 0)
        printk("emergency_erase_reason_show() -- kodari_micom_reg_read fail\n");
		
    return sprintf(buf, "%d\n", regval); // 1 : erase by b-module, 2: erase by erase-button
}

static struct sysfs_ops myops6 = {
    .show = emergency_erase_reason_show,
};

static struct kobj_type mytype6 = {
    .sysfs_ops = &myops6,
    .default_attrs = myattr6,
};
#endif

/* ------------------  Sysfile7: sys/bmodule_erase_info/erase  ------------------ */
static struct kobject *mykobj7;

static struct my_attr erase = {
    .attr.name="erase",
    .attr.mode = 0666,
    .value = 0,
};

static struct attribute * myattr7[] = {
    &erase.attr,
    NULL
};

static ssize_t bmodule_erase_info_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct my_attr *a = container_of(attr, struct my_attr, attr);
	
    return sprintf(buf, "%d\n", a->value);
}

static ssize_t bmodule_erase_info_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
    int ret = 0;
    struct my_attr *a = container_of(attr, struct my_attr, attr);

    sscanf(buf, "%d", &a->value);
    erase.value = a->value;

    if(a->value == 1)
        ret = kodari_micom_reg_write(bmodule_i2c_client, KODARI_BMODULE_ERASE_NOTIFY, 1);
    else //enable value 0
        ret = kodari_micom_reg_write(bmodule_i2c_client, KODARI_BMODULE_ERASE_NOTIFY, 0);

    if (ret < 0)
        printk("bmodule_erase_info_store() -- kodari_micom_reg_write fail: %d\n", a->value);

    return sizeof(int);
}

static struct sysfs_ops myops7 = {
    .show = bmodule_erase_info_show,
    .store = bmodule_erase_info_store,
};

static struct kobj_type mytype7 = {
    .sysfs_ops = &myops7,
    .default_attrs = myattr7,
};

static irqreturn_t kodari_bmodule_irq(int irq, void *data)
{
	struct bmodule_info *bmodule = data;

	kodari_irq_status = irq - bmodule->kodaridev->irq_base;

	sysfs_notify(mykobj, NULL, "irq"); // sysfile update
	
	return IRQ_HANDLED;
}

static irqreturn_t kodari_bmodule_detect_irq(int irq, void *data)
{
	struct bmodule_info *bmodule = data;

	if(bmodule_detect_status == 0) {
		bmodule_detect_status = 1;
		kodari_irq_status = irq - bmodule->kodaridev->irq_base;
		sysfs_notify(mykobj, NULL, "irq"); // sysfile update
	}
	
	return IRQ_HANDLED;
}

int bmodule_probe_done;
EXPORT_SYMBOL(bmodule_probe_done);

static int kodari_bmodule_probe(struct platform_device *pdev)
{
	struct bmodule_info *bmodule;
	struct kodari_micom_dev *kodaridev = dev_get_drvdata(pdev->dev.parent);
	struct kodari_micom_platform_data *pdata = dev_get_platdata(kodaridev->dev);
	int ret;

	bmodule_probe_done = 0;

	printk("@@@@@#####$$$$$ kodari_bmodule_probe -- start\n");
	
	bmodule = kzalloc(sizeof(struct bmodule_info), GFP_KERNEL);
	if (!bmodule)
		return -ENOMEM;

	bmodule->kodaridev = kodaridev;
	bmodule->dev = &pdev->dev;

	bmodule_i2c_client = kodaridev->i2c;

	ret = request_threaded_irq(pdata->irq_base + KODARI_MICOM_BMODULE_ENC_PWR_ON, NULL,
							kodari_bmodule_irq, IRQF_TRIGGER_FALLING,
							"power-on IRQ", bmodule);

	ret = request_threaded_irq(pdata->irq_base + KODARI_MICOM_BMODULE_ENC_ERASE_OK, NULL,
							kodari_bmodule_irq, IRQF_TRIGGER_FALLING,
							"erase IRQ", bmodule);

	ret = request_threaded_irq(pdata->irq_base + KODARI_MICOM_BMODULE_ENC_FAIL, NULL,
							kodari_bmodule_irq, IRQF_TRIGGER_FALLING,
							"fail IRQ", bmodule);

	ret = request_threaded_irq(pdata->irq_base + KODARI_MICOM_BMODULE_ENC_INJECT_OK, NULL,
							kodari_bmodule_irq, IRQF_TRIGGER_FALLING,
							"inject-ok IRQ", bmodule);

	ret = request_threaded_irq(pdata->irq_base + KODARI_MICOM_BMODULE_ENC_INJECT_ING, NULL,
							kodari_bmodule_irq, IRQF_TRIGGER_FALLING,
							"inject-ing IRQ", bmodule);

	ret = request_threaded_irq(pdata->irq_base + KODARI_MICOM_BMODULE_ENC_INJECT_RST, NULL,
							kodari_bmodule_irq, IRQF_TRIGGER_FALLING,
							"inject-rst IRQ", bmodule);

	ret = request_threaded_irq(pdata->irq_base + KODARI_MICOM_BMODULE_ENC_INJECT_BOOT_OK, NULL,
							kodari_bmodule_irq, IRQF_TRIGGER_FALLING,
							"inject-boot-ok IRQ", bmodule);

	ret = request_threaded_irq(pdata->irq_base + KODARI_MICOM_BMODULE_ENC_INJECT_DETECT, NULL,
							kodari_bmodule_detect_irq, IRQF_TRIGGER_FALLING,
							"inject-detect IRQ", bmodule);

#if 0 //def CONFIG_HAS_EARLYSUSPEND
	bmodule->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	bmodule->early_suspend.suspend = bmodule_early_suspend;
	bmodule->early_suspend.resume = bmodule_late_resume;
	register_early_suspend(&bmodule->early_suspend);
#endif

	if (ret < 0)
		goto kodari_bmodule_probe_failed;

	bmodule_probe_done = 1;

	printk("@@@@@#####$$$$$ kodari_bmodule_probe( ) -- success\n");

	return 0;

kodari_bmodule_probe_failed:
	return ret;
}

#if 0 //def CONFIG_HAS_EARLYSUSPEND
static void bmodule_early_suspend(struct early_suspend *h)
{
	int ret = 0;
	//struct bmodule_info *bmodule = container_of(h, struct bmodule_info, early_suspend);

	ret = kodari_micom_reg_write(bmodule_i2c_client, KODARI_SLEEP_MODE_ENTER_NOTIFY, 1);

	if (ret < 0)
		printk("bmodule_early_suspend() -- kodari_micom_reg_write fail\n");
}

static void bmodule_late_resume(struct early_suspend *h)
{
	int ret = 0;
	//struct bmodule_info *bmodule = container_of(h, struct bmodule_info, early_suspend);

	ret = kodari_micom_reg_write(bmodule_i2c_client, KODARI_SLEEP_MODE_ENTER_NOTIFY, 0);

	if (ret < 0)
		printk("bmodule_late_resume() -- kodari_micom_reg_write fail\n");
}
#endif

static int kodari_bmodule_remove(struct platform_device *pdev)
{
	struct kodari_micom_dev *kodaridev = dev_get_drvdata(pdev->dev.parent);
	struct bmodule_info*bmodule = i2c_get_clientdata(kodaridev->i2c);
	
#if 0 //def CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&bmodule->early_suspend);
#endif
	kfree(bmodule);

	return 0;
}

#ifdef CONFIG_PM
static int bmodule_suspend(struct device *dev)
{
	int ret = 0;

	ret = kodari_micom_reg_write(bmodule_i2c_client, KODARI_SLEEP_MODE_ENTER_NOTIFY, 1);

	//if (ret < 0)
		//printk("bmodule_suspend() -- kodari_micom_reg_write fail\n");

	return 0;
}

static int bmodule_resume(struct device *dev)
{
	int ret = 0;

	ret = kodari_micom_reg_write(bmodule_i2c_client, KODARI_SLEEP_MODE_ENTER_NOTIFY, 0);

	//if (ret < 0)
		//printk("bmodule_resume() -- kodari_micom_reg_write fail\n");

	return 0;
}

static const struct dev_pm_ops bmodule_pm_ops = {
	.suspend = bmodule_suspend,
	.resume = bmodule_resume,
};
#else
#define bmodule_suspend	NULL
#define bmodule_resume NULL
#endif

static struct platform_driver kodari_bmodule_driver = {
	.probe		= kodari_bmodule_probe,
	.remove		= kodari_bmodule_remove,
	.id_table	= kodari_bmodule_id,
	.driver		= {
		.name	= "kodari-micom-bmodule",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &bmodule_pm_ops,
#endif
	},
};

static int __init kodari_bmodule_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&kodari_bmodule_driver);

	mykobj = kzalloc(sizeof(*mykobj), GFP_KERNEL);
	if (mykobj) {
		kobject_init(mykobj, &mytype);
		if (kobject_add(mykobj, NULL, "%s", "bmodule_irq")) {
			printk("bmodule_irq: kobject_add() failed\n");
			kobject_put(mykobj);
			mykobj = NULL;
		}
	}

	mykobj2 = kzalloc(sizeof(*mykobj2), GFP_KERNEL);
	if (mykobj2) {
		kobject_init(mykobj2, &mytype2);
		if (kobject_add(mykobj2, NULL, "%s", "bmodule_led")) {
			printk("bmodule_led: kobject_add() failed\n");
			kobject_put(mykobj2);
			mykobj2 = NULL;
		}
	}

	mykobj3 = kzalloc(sizeof(*mykobj3), GFP_KERNEL);
	if (mykobj3) {
		kobject_init(mykobj3, &mytype3);
		if (kobject_add(mykobj3, NULL, "%s", "bmodule_status")) {
			printk("bmodule_status: kobject_add() failed\n");
			kobject_put(mykobj3);
			mykobj3 = NULL;
		}
	}

	mykobj4 = kzalloc(sizeof(*mykobj4), GFP_KERNEL);
	if (mykobj4) {
		kobject_init(mykobj4, &mytype4);
		if (kobject_add(mykobj4, NULL, "%s", "bmodule_monitor")) {
			printk("bmodule_monitor: kobject_add() failed\n");
			kobject_put(mykobj4);
			mykobj4 = NULL;
		}
	}

	mykobj5 = kzalloc(sizeof(*mykobj5), GFP_KERNEL);
	if (mykobj5) {
		kobject_init(mykobj5, &mytype5);
		if (kobject_add(mykobj5, NULL, "%s", "bmodule_power")) {
			printk("bmodule_power: kobject_add() failed\n");
			kobject_put(mykobj5);
			mykobj5 = NULL;
		}
	}
#if 0
	mykobj6 = kzalloc(sizeof(*mykobj6), GFP_KERNEL);
	if (mykobj6) {
		kobject_init(mykobj6, &mytype6);
		if (kobject_add(mykobj6, NULL, "%s", "emergency_erase")) {
			printk("emergency_erase: kobject_add() failed\n");
			kobject_put(mykobj6);
			mykobj6 = NULL;
		}
	}
#endif
	mykobj7 = kzalloc(sizeof(*mykobj7), GFP_KERNEL);
	if (mykobj7) {
		kobject_init(mykobj7, &mytype7);
		if (kobject_add(mykobj7, NULL, "%s", "bmodule_erase_info")) {
			printk("bmodule_erase_info: kobject_add() failed\n");
			kobject_put(mykobj7);
			mykobj7 = NULL;
		}
	}

	return ret;
}
module_init(kodari_bmodule_init);

static void __exit kodari_bmodule_exit(void)
{
	platform_driver_unregister(&kodari_bmodule_driver);

	if (mykobj) {
        kobject_put(mykobj);
        kfree(mykobj);
      }

	if (mykobj2) {
        kobject_put(mykobj2);
        kfree(mykobj2);
	}

	if (mykobj3) {
        kobject_put(mykobj3);
        kfree(mykobj3);
	}

	if (mykobj4) {
        kobject_put(mykobj4);
        kfree(mykobj4);
	}

	if (mykobj5) {
        kobject_put(mykobj5);
        kfree(mykobj5);
	}
#if 0
	if (mykobj6) {
        kobject_put(mykobj6);
        kfree(mykobj6);
	}
#endif
	if (mykobj7) {
        kobject_put(mykobj7);
        kfree(mykobj7);
	}
}
module_exit(kodari_bmodule_exit);

