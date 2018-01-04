/*
 * kodari_gps.c
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

struct kodari_gps_dev {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex iolock;
};

static const struct i2c_device_id kodari_gps_i2c_id[] = {
	{ "ublox-6", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, kodari_gps_i2c_id);

static int MakeChecksum(u8 * packet, int len)
{
	int i = 0;
	u8  *pck_a = &packet[len -2];
	u8 *pck_b = &packet[len -1];
	
	*pck_a = *pck_b = 0;
	int iLength = len - 4;

	u8 *pChk = &packet[2];
	for(i  =0 ; i < iLength; i++)
	{
		*pck_a += *pChk++;
		*pck_b += *pck_a;
	}

	return 0;
}

#if 0
int gps_i2c_bulk_write(struct i2c_client *i2c, u8 reg, int len, u8 *packet)
{
	struct kodari_gps_dev *gps = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&gps->iolock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, len, packet);
	mutex_unlock(&gps->iolock);
	if (ret < 0)
		return ret;

	return 0;
}
#else
int gps_i2c_bulk_write(struct i2c_client *i2c, int len, u8 *packet)
{
	//struct kodari_gps_dev *gps = i2c_get_clientdata(i2c);
	int ret = 0;
	//u8 reg = 0xff;
	//int i = 0;

	MakeChecksum(packet, len);

	//for(i = 0; i < len; i++)
		//printk("packet[%d] = %02x\n", i, packet[i]);

#if 0
	mutex_lock(&gps->iolock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, len, packet);
	mutex_unlock(&gps->iolock);
#else
        ret = i2c_master_send(i2c, packet, len);
#endif

	if (ret < 0)
		return ret;

	return 0;
}
#endif

static int kodari_gps_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct kodari_gps_dev *gps;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		return -EIO;
	}

	gps = kzalloc(sizeof(struct kodari_gps_dev), GFP_KERNEL);
	if (gps == NULL) {
		return -ENOMEM;
	}

	mutex_init(&gps->iolock);

	gps->i2c = i2c;

	i2c_set_clientdata(i2c, gps);

	return 0;
}

static int kodari_gps_i2c_remove(struct i2c_client *i2c)
{
	struct kodari_gps_dev *gps = i2c_get_clientdata(i2c);
	
	kfree(gps);

	return 0;
}

#ifdef CONFIG_PM
static int kodari_gps_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct kodari_gps_dev *gps = i2c_get_clientdata(i2c);
	int ret = 0;
	u8 RXM_POWER_SAVE_PKT[10] = {0xb5, 0x62, 0x06, 0x11, 0x02, 0x00, 0x08, 0x01, 0x00, 0x00}; // Receiver's power-save mode

         gps->i2c->addr = 0x42;
	ret = gps_i2c_bulk_write(gps->i2c, 10, RXM_POWER_SAVE_PKT);
	gps->i2c->addr = 0x01;

	//if(ret < 0)
		//return -1; // If i2c write is fail, Don't enter suspend
	//else
		return 0;
}

static int kodari_gps_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct kodari_gps_dev *gps = i2c_get_clientdata(i2c);
	int ret = 0;
	u8 RXM_MAX_PERFORM_PKT[10] = {0xb5, 0x62, 0x06, 0x11, 0x02, 0x00, 0x08, 0x00, 0x00, 0x00}; // Receiver's max-performance mode

	gps->i2c->addr = 0x42;
	ret = gps_i2c_bulk_write(gps->i2c, 10, RXM_MAX_PERFORM_PKT);
	gps->i2c->addr = 0x01;

	//if(ret < 0)
		//printk("kodari_gps_resume(): i2c write fail\n");

	return 0;
}
#else
#define kodari_gps_suspend NULL
#define kodari_gps_resume NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops kodari_gps_pm = {
	.suspend = kodari_gps_suspend,
	.resume = kodari_gps_resume,
};

static struct i2c_driver kodari_gps_i2c_driver = {
	.driver = {
		   .name = "ublox-6",
		   .owner = THIS_MODULE,
		   .pm = &kodari_gps_pm,
	},
	.probe = kodari_gps_i2c_probe,
	.remove = kodari_gps_i2c_remove,
	.id_table = kodari_gps_i2c_id,
};

static int __init kodari_gps_i2c_init(void)
{
	return i2c_add_driver(&kodari_gps_i2c_driver);
}

static void __exit kodari_gps_i2c_exit(void)
{
	i2c_del_driver(&kodari_gps_i2c_driver);
}

module_init(kodari_gps_i2c_init);
module_exit(kodari_gps_i2c_exit);

