/*
 * kodari-core.c
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
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/kodari/kodari-micom-core.h>h

static struct mfd_cell kodari_micom_devs[] = {
	{
		.name = "kodari-micom-battery",
	},
	{
		.name = "kodari-micom-bmodule",
	},
};



int kodari_micom_reg_read(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&kodaridev->iolock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&kodaridev->iolock);
	if (ret < 0)
		return ret;

	ret &= 0xff;
	*dest = ret;
	return 0;
}


EXPORT_SYMBOL(kodari_micom_reg_read);


int kodari_micom_reg_write(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&kodaridev->iolock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&kodaridev->iolock);
	return ret;
}
EXPORT_SYMBOL(kodari_micom_reg_write);


int kodari_micom_reg_update(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&kodaridev->iolock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&kodaridev->iolock);
	return ret;
}
EXPORT_SYMBOL(kodari_micom_reg_update);

static int kodari_micom_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct kodari_micom_platform_data *pdata = i2c->dev.platform_data;
	struct kodari_micom_dev *kodaridev;
	int ret = 0;
	u8 data;

	kodaridev = kzalloc(sizeof(struct kodari_micom_dev), GFP_KERNEL);
	if (kodaridev == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, kodaridev);
	kodaridev->dev = &i2c->dev;
	kodaridev->i2c = i2c;
	kodaridev->irq = i2c->irq;


	if (pdata) {
		kodaridev->ono = pdata->ono;
		kodaridev->irq_base = pdata->irq_base;
		kodaridev->wakeup = pdata->wakeup;
	}

	mutex_init(&kodaridev->iolock); 

	if (pdata && pdata->cfg_kodari_micom_irq)
		pdata->cfg_kodari_micom_irq();
 
	kodari_micom_irq_init(kodaridev);

	pm_runtime_set_active(kodaridev->dev);

	ret = mfd_add_devices(kodaridev->dev, -1,
				kodari_micom_devs, ARRAY_SIZE(kodari_micom_devs),
				NULL, 0);

	if (ret < 0)
		goto err;

	dev_info(kodaridev->dev ,"kodari micom MFD probe done!!! \n");
	return ret;

err:
	mfd_remove_devices(kodaridev->dev);
	kodari_micom_irq_exit(kodaridev);
err_mfd:
	kfree(kodaridev);
	return ret;
}

static int kodari_micom_i2c_remove(struct i2c_client *i2c)
{
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(i2c);

	mfd_remove_devices(kodaridev->dev);
	if (kodaridev->irq)
		free_irq(kodaridev->irq, kodaridev);
	kfree(kodaridev);

	return 0;
}

static const struct i2c_device_id kodari_micom_i2c_id[] = {
	{ "kodari-micom", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, kodari_micom_i2c_id);

#ifdef CONFIG_PM
static int kodari_micom_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(i2c);
#if 0
	if (kodaridev->wakeup)
		enable_irq_wake(kodaridev->irq);
#endif
	disable_irq(kodaridev->irq);

	return 0;
}

static int kodari_micom_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(i2c);
#if 0
	if (kodaridev->wakeup)
		disable_irq_wake(kodaridev->irq);
#endif
	enable_irq(kodaridev->irq);

	return 0;
}
#else
#define s5m_suspend       NULL
#define s5m_resume                NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops kodari_micom_apm = {
	.suspend = kodari_micom_suspend,
	.resume = kodari_micom_resume,
};

static struct i2c_driver kodari_micom_i2c_driver = {
	.driver = {
		   .name = "kodari-micom",
		   .owner = THIS_MODULE,
		   .pm = &kodari_micom_apm,
	},
	.probe = kodari_micom_i2c_probe,
	.remove = kodari_micom_i2c_remove,
	.id_table = kodari_micom_i2c_id,
};

static int __init kodari_micom_i2c_init(void)
{
	return i2c_add_driver(&kodari_micom_i2c_driver);
}

subsys_initcall(kodari_micom_i2c_init);

static void __exit kodari_micom_i2c_exit(void)
{
	i2c_del_driver(&kodari_micom_i2c_driver);
}
module_exit(kodari_micom_i2c_exit);

MODULE_AUTHOR("lee hyungChul <hclee@kodari.co.kr>");
MODULE_DESCRIPTION("Core support for the KODARI Micom MFD");
MODULE_LICENSE("GPL");
