/*
 *  KODARI_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 *  Copyright (C) 2009 Samsung Electronics
 *  Minkyu Kang <mk7.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
//#include <linux/power/kodari_battery.h>
#include <linux/mfd/kodari/kodari-micom-core.h>
#include <linux/slab.h>

//#define KODARI_AC_ONLINE_0 		0x00
//#define KODARI_BATTERY_ONLINE_0 	0x01
//#define KODARI_VCELL_MSB_0		0x02
//#define KODARI_VCELL_LSB_0		0x03
//#define KODARI_SOC_MSB_0			0x04
//#define KODARI_SOC_LSB_0			0x05
//#define KODARI_MICOM_MAJOR_VER	0x06
//#define KODARI_MICOM_MINOR_VER	0x07
//#define KODARI_BOARD_VER			0x08

//#define KODARI_AC_ONLINE_1 		0x10
//#define KODARI_BATTERY_ONLINE_1 	0x11
//#define KODARI_VCELL_MSB_1		0x12
//#define KODARI_VCELL_LSB_1		0x13
//#define KODARI_SOC_MSB_1			0x14
//#define KODARI_SOC_LSB_1			0x15


//#define KODARI_TYPE_MOBILE	0xAA
//#define KODARI_TYPE_CAR		0x55



//#define KODARI_MODE_MSB	0x06
//#define KODARI_MODE_LSB	0x07
//#define KODARI_VER_MSB	0x08
//#define KODARI_VER_LSB	0x09
//#define KODARI_RCOMP_MSB	0x0C
//#define KODARI_RCOMP_LSB	0x0D
//#define KODARI_CMD_MSB	0xFE
//#define KODARI_CMD_LSB	0xFF

#define KODARI_BATTERY_DELAY			msecs_to_jiffies(5000)
#define KODARI_BATTERY_FULL		100
#define	POWER_OFF_VOLTAGE		2900000
//#define	POWER_OFF_VOLTAGE		3300000
//#define	POWER_MAX_VOLTAGE		4500000
#define	POWER_MAX_VOLTAGE		4150000
#define LOG_APPLY	0

struct i2c_client *battery_i2c_client;

static int kodari_battery_write_reg(struct i2c_client *client, int reg, u8 value);

struct my_attr {
    struct attribute attr;
    int value;
};

/* ----------------------------  Sysfile1 status  ---------------------------- */
static struct kobject *mykobj1;
static int kodari_type;

static struct my_attr type = {
    .attr.name="type",
    .attr.mode = 0666,
    .value = 0,
};

static struct attribute * myattr1[] = {
    &type.attr,
    NULL
};

static ssize_t type_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    //struct my_attr *a = container_of(attr, struct my_attr, attr);
	
    return sprintf(buf, "%d\n", kodari_type);
}

static struct sysfs_ops myops1 = {
    .show = type_show,
    //.store = blind_mode_store,
};

static struct kobj_type mytype1 = {
    .sysfs_ops = &myops1,
    .default_attrs = myattr1,
};

/* ----------------------------  Sysfile2 status  ---------------------------- */
static struct kobject *mykobj2;
static int kodari_vesion_up;

static struct my_attr vesion_up = {
    .attr.name="vesion_up",
    .attr.mode = 0666,
    .value = 0,
};

static struct attribute * myattr2[] = {
    &vesion_up.attr,
    NULL
};

static ssize_t kodari_vesion_up_show(struct kobject *kobj, struct attribute *attr, char *buf)
{	
    return sprintf(buf, "%d\n", kodari_vesion_up);
}


static struct sysfs_ops myops2 = {
    .show = kodari_vesion_up_show,
    //.store = blind_mode_store,
};

static struct kobj_type mytype2 = {
    .sysfs_ops = &myops2,
    .default_attrs = myattr2,
};
/* ----------------------------  Sysfile3 status  ---------------------------- */

static struct kobject *mykobj3;
static int kodari_vesion_down;

static struct my_attr vesion_down = {
    .attr.name="vesion_down",
    .attr.mode = 0666,
    .value = 0,
};

static struct attribute * myattr3[] = {
    &vesion_down.attr,
    NULL
};

static ssize_t kodari_vesion_down_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    //struct my_attr *a = container_of(attr, struct my_attr, attr);
	
    return sprintf(buf, "%d\n", kodari_vesion_down);
}

static struct sysfs_ops myops3 = {
    .show = kodari_vesion_down_show,
    //.store = blind_mode_store,
};

static struct kobj_type mytype3 = {
    .sysfs_ops = &myops3,
    .default_attrs = myattr3,
};

/* ------------------  Sysfile4: /sys/battery_led/led  ------------------ */
static struct kobject *mykobj4;

static struct my_attr BatteryLed = {
    .attr.name="led",
    .attr.mode = 0666,
    .value = 0,
};

static struct attribute * myattr4[] = {
    &BatteryLed.attr,
    NULL
};

static ssize_t battery_led_enable_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct my_attr *a = container_of(attr, struct my_attr, attr);

    return sprintf(buf, "%d\n", a->value);
}

static ssize_t battery_led_enable_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
    struct my_attr *a = container_of(attr, struct my_attr, attr);
    int ret = 0;

    sscanf(buf, "%d", &a->value);

    if(a->value == 1) // led disable
    {
        printk("########## battery_led: value = 0x20\n");
        ret = kodari_battery_write_reg(battery_i2c_client, KODARI_BATTERY_LED_DISABLE, 0x20);
    }
    else if(a->value == 2) // led enable
    {
        printk("########## battery_led: value = 0x10\n");
        ret = kodari_battery_write_reg(battery_i2c_client, KODARI_BATTERY_LED_DISABLE, 0x10);
    }
    else if(a->value == 3) // notify received-message
    {
        printk("########## message_led: value = 0x1\n");
        ret = kodari_battery_write_reg(battery_i2c_client, KODARI_BATTERY_LED_CONTROL, 1);
    }
    else if(a->value == 4) // notify received-message read done
    {
        printk("########## message_led: value = 0x0\n");
        ret = kodari_battery_write_reg(battery_i2c_client, KODARI_BATTERY_LED_CONTROL, 0);
    }
	
    if (ret < 0)
        printk("kodari_micom_reg_write() -- KODARI_BATTERY_LED_DISABLE fail\n");

    return sizeof(int);
}

static struct sysfs_ops myops4 = {
    .show = battery_led_enable_show,
    .store = battery_led_enable_store,
};

static struct kobj_type mytype4 = {
    .sysfs_ops = &myops4,
    .default_attrs = myattr4,
};


/* ----------------------------  battery status  ---------------------------- */
enum {
	Battery_index_0,
	Battery_index_1
};

int car_mobile_flag = 0;


struct kodari_battery_chip {
	struct device 			*dev;
	struct i2c_client		*client;
	struct delayed_work		work;
	struct power_supply		battery_0;
	struct power_supply		battery_1;
	struct power_supply     ac;
	struct power_supply     dc;
	struct power_supply     usb;
	struct kodari_battery_platform_data	*pdata;

	/* State Of Connect */
	int online_0;
	int online_1;
	/* battery voltage */
	int vcell_0;
	int vcell_1;
	/* battery capacity */
	int soc_0;
	int soc_1;
	/* State Of Charge */
	int status_0;
	int status_1;
	
	int ac_online;

	int battery_online_0;
	int battery_online_1;
	char strVer[20];

	int AAA_battery_online_0;
	int AAA_battery_online_1;

	int dc_online;
};

static int kodari_get_property_0(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct kodari_battery_chip *chip = container_of(psy,
				struct kodari_battery_chip, battery_0);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->status_0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->vcell_0;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->soc_0;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->battery_online_0;
		break;		
	case POWER_SUPPLY_PROP_TECHNOLOGY:		
		if(chip->AAA_battery_online_0)
			val->intval = POWER_SUPPLY_TECHNOLOGY_AAA;
		else if(chip->battery_online_0)
			val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		else
			val->intval = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if(chip->battery_online_0)
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		else
			val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		val->strval = chip->strVer;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}


static int kodari_get_property_1(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct kodari_battery_chip *chip = container_of(psy,
				struct kodari_battery_chip, battery_1);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->status_1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->vcell_1;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->soc_1;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->battery_online_1;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:		
		if(chip->AAA_battery_online_1)
			val->intval = POWER_SUPPLY_TECHNOLOGY_AAA;
		else if(chip->battery_online_1)
			val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		else
			val->intval = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if(chip->battery_online_1)
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		else
			val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		val->strval = chip->strVer;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}


static int kodari_battery_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	//ret = i2c_smbus_write_byte_data(client, reg, value);
	ret = kodari_micom_reg_write(client, reg, value);

//	if (ret < 0)
//		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

//	printk(KERN_ERR "kodari_get_AC_online kodari_battery_write_reg ret = %d\n", ret);

	return ret;
}

static int kodari_battery_read_reg(struct i2c_client *client, int reg)
{
	int ret;
	u8 retreg = 0;
	

	//ret = i2c_smbus_read_byte_data(client, reg);
	ret = kodari_micom_reg_read(client, reg, &retreg);

#if LOG_APPLY
	printk(KERN_ERR "i2c read reg:0x%x - ret value:%i\n", reg, ret);
#endif


//	if (ret < 0)
//		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return retreg;
}

static int kodari_battery_read_i2c_reg(struct i2c_client *client, int reg)
{
	int ret = 0;
	u8 retreg = 0;

	ret = kodari_micom_reg_read(client, reg, &retreg);

#if LOG_APPLY
	printk(KERN_ERR "[%s] i2c read reg:0x%x - ret value:%i\n", __func__, reg, ret);
#endif

	if(ret < 0) // i2c read fail
		return ret;
	else
		return retreg;
}

static int kodari_get_type(struct i2c_client *client)
{
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(client);
	struct kodari_battery_chip *chip = (struct kodari_battery_chip*)kodaridev->kodari_battery_dev;
	int  type;

//	type = kodari_read_reg(client, KODARI_BOARD_VER);
	type = kodari_battery_read_reg(client, KODARI_BOARD_VER);

//	dev_info(&client->dev, "kodari_get_version type:%d \n", type);

	//type = 0xAA;
	//type = 0x55;

	return type;
}



static int kodari_get_version_msb(struct i2c_client *client)
{
//	struct kodari_battery_chip *chip = i2c_get_clientdata(client);
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(client);
	struct kodari_battery_chip *chip = (struct kodari_battery_chip*)kodaridev->kodari_battery_dev;

	int msb;


	msb = kodari_battery_read_reg(client, KODARI_MICOM_MAJOR_VER);
		dev_info(&client->dev, "kodari_get_version msb:%d \n", msb);
#if LOG_APPLY
	printk(KERN_ERR "kodari_get_AC_online kodari_get_version_msb msb = %d\n", msb);
#endif
	
	return msb;
}

static int kodari_get_version_lsb(struct i2c_client *client)
{
//	struct kodari_battery_chip *chip = i2c_get_clientdata(client);
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(client);
	struct kodari_battery_chip *chip = (struct kodari_battery_chip*)kodaridev->kodari_battery_dev;


	int lsb;	
	lsb = kodari_battery_read_reg(client, KODARI_MICOM_MINOR_VER);
	dev_info(&client->dev, "kodari_get_version lsb:%d\n", lsb);
#if LOG_APPLY
	printk(KERN_ERR "kodari_get_AC_online kodari_get_version_lsb lsb = %d\n", lsb);
#endif

	return lsb;
	
}


static void kodari_get_vcell_0(struct i2c_client *client)
{
	//struct kodari_battery_chip *chip = i2c_get_clientdata(client);
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(client);
	struct kodari_battery_chip *chip = (struct kodari_battery_chip*)kodaridev->kodari_battery_dev;
	int msb;
	int lsb;

#if 0
	msb = kodari_battery_read_reg(client, KODARI_VCELL_MSB_0);
	lsb = kodari_battery_read_reg(client, KODARI_VCELL_LSB_0);
#else
	msb = kodari_battery_read_i2c_reg(client, KODARI_VCELL_MSB_0);
	lsb = kodari_battery_read_i2c_reg(client, KODARI_VCELL_LSB_0);
#endif

//	msb = 0x96;
//	lsb = 0x04;

#if LOG_APPLY
	printk(KERN_ERR "kodari_get_AC_online kodari_get_vcell_0 msb = %d, lsb = %d\n", msb, lsb);
#endif	

         if((msb < 0) && (lsb < 0))
     	{
		chip->vcell_0 = -1;
     	}
	 else
 	{
		chip->vcell_0 = (msb << 4) + (lsb >> 4);
		//	printk(KERN_ERR "kodari_get_AC_online kodari_get_vcell_1  chip->vcell_0= %d\n", chip->vcell_0);

		chip->vcell_0 = (chip->vcell_0 * 125) * 10;
		//	printk(KERN_ERR "kodari_get_AC_online kodari_get_vcell_0  chip->vcell_0= %d\n", chip->vcell_0);
 	}
}

static void kodari_get_vcell_1(struct i2c_client *client)
{
//	struct kodari_battery_chip *chip = i2c_get_clientdata(client);
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(client);
	struct kodari_battery_chip *chip = (struct kodari_battery_chip*)kodaridev->kodari_battery_dev;

	int msb;
	int lsb;

#if 0
	msb = kodari_battery_read_reg(client, KODARI_VCELL_MSB_1);
	lsb = kodari_battery_read_reg(client, KODARI_VCELL_LSB_1);
#else
	msb = kodari_battery_read_i2c_reg(client, KODARI_VCELL_MSB_1);
	lsb = kodari_battery_read_i2c_reg(client, KODARI_VCELL_LSB_1);
#endif


//	msb = 0x96;
//	lsb = 0x04;

#if LOG_APPLY
	printk(KERN_ERR "kodari_get_AC_online kodari_get_vcell_1 msb = %d, lsb = %d\n", msb, lsb);
#endif	

	if((msb < 0) && (lsb < 0))
	{
		chip->vcell_1 = -1;
	}
	else
	{
		chip->vcell_1 = (msb << 4) + (lsb >> 4);
		//	printk(KERN_ERR "kodari_get_AC_online kodari_get_vcell_1  chip->vcell_1= %d\n", chip->vcell_1);

		chip->vcell_1 = (chip->vcell_1 * 125) * 10;
		//	printk(KERN_ERR "kodari_get_AC_online kodari_get_vcell_1  chip->vcell_1= %d\n", chip->vcell_1);
	}	
}


static void kodari_get_soc_0(struct i2c_client *client)
{
//	struct kodari_battery_chip *chip = i2c_get_clientdata(client);
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(client);
	struct kodari_battery_chip *chip = (struct kodari_battery_chip*)kodaridev->kodari_battery_dev;

	int msb;
	//int lsb;

#if 0
	msb = kodari_battery_read_reg(client, KODARI_SOC_MSB_0);
	lsb = kodari_battery_read_reg(client, KODARI_SOC_LSB_0);
#else
	msb = kodari_battery_read_i2c_reg(client, KODARI_SOC_MSB_0);
	//lsb = kodari_battery_read_i2c_reg(client, KODARI_SOC_LSB_0);
#endif

//	msb = 0x63;
//	msb = 0x0A;


//	printk(KERN_ERR "kodari_get_AC_online kodari_get_soc_0 msb = %d, lsb = %d\n", msb, lsb);




	chip->soc_0= msb;

	if(chip->soc_0 > 0) {
		if(chip->vcell_0 < POWER_OFF_VOLTAGE)		chip->soc_0 = 0;
		if(chip->soc_0 > KODARI_BATTERY_FULL)	{
			if(chip->vcell_0 < POWER_MAX_VOLTAGE)	chip->soc_0 = KODARI_BATTERY_FULL;
			else								chip->soc_0 = KODARI_BATTERY_FULL;
		}
	}
	else if(chip->soc_0 == 0) {
		if(chip->vcell_0 >= POWER_OFF_VOLTAGE)	chip->soc_0 = 1;
	}
#if LOG_APPLY
	printk(KERN_ERR "kodari_get_AC_online kodari_get_soc_0:%d\n", chip->soc_0);
#endif	

}

static void kodari_get_soc_1(struct i2c_client *client)
{
//	struct kodari_battery_chip *chip = i2c_get_clientdata(client);
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(client);
	struct kodari_battery_chip *chip = (struct kodari_battery_chip*)kodaridev->kodari_battery_dev;

	int msb;
	//int lsb;

#if 0
	msb = kodari_battery_read_reg(client, KODARI_SOC_MSB_1);
	lsb = kodari_battery_read_reg(client, KODARI_SOC_LSB_1);
#else
	msb = kodari_battery_read_i2c_reg(client, KODARI_SOC_MSB_1);
	//lsb = kodari_battery_read_i2c_reg(client, KODARI_SOC_LSB_1);
#endif

//	msb = 0x0A;
//	msb = 0x63;


//	printk(KERN_ERR "kodari_get_AC_online kodari_get_soc_1 msb = %d, lsb = %d\n", msb, lsb);
	
	chip->soc_1= msb;

	if(chip->soc_1 > 0) {
		if(chip->vcell_1 < POWER_OFF_VOLTAGE)		chip->soc_1 = 0;
		if(chip->soc_1 > KODARI_BATTERY_FULL)	{
			if(chip->vcell_1 < POWER_MAX_VOLTAGE)	chip->soc_1 = KODARI_BATTERY_FULL;
			else								chip->soc_1 = KODARI_BATTERY_FULL;
		}
	}
	else if(chip->soc_1 == 0) {
		if(chip->vcell_1 >= POWER_OFF_VOLTAGE)	chip->soc_1 = 1;
	}
#if LOG_APPLY
	printk(KERN_ERR "kodari_get_AC_online kodari_get_soc_1:%d\n", chip->soc_1);
#endif		
}


//static int max17040_get_version(struct i2c_client *client)
//{
//	int msb;
//	int lsb;

//	msb = max17040_read_reg(client, MAX17040_VER_MSB);
//	if(msb < 0)
//		return msb;
//	lsb = max17040_read_reg(client, MAX17040_VER_LSB);
//
//	dev_info(&client->dev, "MAX17040 Fuel-Gauge Ver %d%d\n", msb, lsb);
//	return lsb;
//}

static void kodari_get_battery_online_0(struct i2c_client *client)
{
//	struct kodari_battery_chip *chip = i2c_get_clientdata(client);
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(client);
	struct kodari_battery_chip *chip = (struct kodari_battery_chip*)kodaridev->kodari_battery_dev;

	u8 msb;
	u8 AAA_msb = 0;
	chip->battery_online_0 = 0;
	msb = kodari_battery_read_reg(client, KODARI_BATTERY_ONLINE_0);
//	msb = 1;

#if LOG_APPLY
	printk(KERN_ERR "kodari_get_AC_online online0 = %d\n", msb);
#endif

	chip->battery_online_0 = msb;

	chip->AAA_battery_online_0 = 0;
	AAA_msb = kodari_battery_read_reg(client, KODARI_AAA_BATTERY_ONLINE_0);

#if LOG_APPLY
	printk(KERN_ERR "kodari_get_AAA_battery_online_0 = %d\n", AAA_msb);
#endif

	chip->AAA_battery_online_0 = AAA_msb;
}


static void kodari_get_battery_online_1(struct i2c_client *client)
{
//	struct kodari_battery_chip *chip = i2c_get_clientdata(client);
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(client);
	struct kodari_battery_chip *chip = (struct kodari_battery_chip*)kodaridev->kodari_battery_dev;
	u8 msb;
	u8 AAA_msb = 0;
	chip->battery_online_1 = 0;
	msb = kodari_battery_read_reg(client, KODARI_BATTERY_ONLINE_1);	
//	msb = 1;

#if LOG_APPLY
	printk(KERN_ERR "kodari_get_AC_online online1 = %d\n", msb);
#endif
	chip->battery_online_1 = msb;

	chip->AAA_battery_online_1 = 0;
	AAA_msb = kodari_battery_read_reg(client, KODARI_AAA_BATTERY_ONLINE_1);

#if LOG_APPLY
	printk(KERN_ERR "kodari_get_AAA_battery_online_1 = %d\n", AAA_msb);
#endif

	chip->AAA_battery_online_1 = AAA_msb;
}

static void kodari_get_AC_online(struct i2c_client *client)
{
	//struct kodari_battery_chip *chip = i2c_get_clientdata(client);
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(client);
	struct kodari_battery_chip *chip = (struct kodari_battery_chip*)kodaridev->kodari_battery_dev;
	u8 msb;
	chip->ac_online = 0;
	msb = kodari_battery_read_reg(client, KODARI_AC_ONLINE_0);

#if LOG_APPLY
	printk(KERN_ERR "kodari_get_AC_online battery AC = %d\n", msb);
#endif

//	msb = 1;
	chip->ac_online = msb;
}

static void kodari_get_DC_online(struct i2c_client *client)
{
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(client);
	struct kodari_battery_chip *chip = (struct kodari_battery_chip*)kodaridev->kodari_battery_dev;
	u8 msb = 0;

	chip->dc_online= 0;
	msb = kodari_battery_read_reg(client, KODARI_DC_ADAPTER_ONLINE);

#if LOG_APPLY
	printk(KERN_ERR "@@@@@#####$$$$$ kodari_get_DC_online DC = %d\n", msb);
#endif

	if((msb == 0) || (msb == 1)) // 5v dc adapter
		chip->dc_online = msb;
}

static void kodari_get_status_0(struct i2c_client *client)
{
//	struct kodari_battery_chip *chip = i2c_get_clientdata(client);
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(client);
	struct kodari_battery_chip *chip = (struct kodari_battery_chip*)kodaridev->kodari_battery_dev;

#if 0
	if (!chip->pdata->charger_online || !chip->pdata->charger_enable) {
		chip->status = POWER_SUPPLY_STATUS_UNKNOWN;
		return;
	}

	if (chip->pdata->charger_online()) {
		if (chip->pdata->charger_enable())
			chip->status = POWER_SUPPLY_STATUS_CHARGING;
		else
			chip->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	} else {
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
	}
#else
   if(chip->soc_0 > 101) {
		chip->status_0 = POWER_SUPPLY_STATUS_UNKNOWN;
		return;
	}

	if(chip->ac_online)
		chip->status_0 = POWER_SUPPLY_STATUS_CHARGING;
	else
		chip->status_0 = POWER_SUPPLY_STATUS_DISCHARGING;
#endif
	if (chip->soc_0 >= KODARI_BATTERY_FULL)
		chip->status_0 = POWER_SUPPLY_STATUS_FULL;

#if LOG_APPLY
	printk(KERN_ERR "kodari_get_AC_online chip->status_0= %d\n", chip->status_0);
#endif

	
}


static void kodari_get_status_1(struct i2c_client *client)
{
//	struct kodari_battery_chip *chip = i2c_get_clientdata(client);
	struct kodari_micom_dev *kodaridev = i2c_get_clientdata(client);
	struct kodari_battery_chip *chip = (struct kodari_battery_chip*)kodaridev->kodari_battery_dev;



#if 0
	if (!chip->pdata->charger_online || !chip->pdata->charger_enable) {
		chip->status = POWER_SUPPLY_STATUS_UNKNOWN;
		return;
	}

	if (chip->pdata->charger_online()) {
		if (chip->pdata->charger_enable())
			chip->status = POWER_SUPPLY_STATUS_CHARGING;
		else
			chip->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	} else {
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
	}
#else
   if(chip->soc_1 > 101) {
		chip->status_0 = POWER_SUPPLY_STATUS_UNKNOWN;
		return;
	}

	if(chip->ac_online)
		chip->status_1 = POWER_SUPPLY_STATUS_CHARGING;
	else
		chip->status_1 = POWER_SUPPLY_STATUS_DISCHARGING;
#endif
	if (chip->soc_1 >= KODARI_BATTERY_FULL)
		chip->status_1 = POWER_SUPPLY_STATUS_FULL;
#if LOG_APPLY
	printk(KERN_ERR "kodari_get_AC_online chip->status_1= %d\n", chip->status_1);
#endif

}


static void kodari_work(struct work_struct *work)
{
	struct kodari_battery_chip *chip = container_of(work, struct kodari_battery_chip, work.work);
	int old_ac_online, old_dc_online, old_vcell_0, old_soc_0;
	int old_vcell_1, old_soc_1;

	chip = container_of(work, struct kodari_battery_chip, work.work);

//	printk(KERN_ERR "kodari mobile battery kodari_work\n");


	if(car_mobile_flag == KODARI_BATTERY_TYPE_MOBILE){
		old_ac_online = chip->ac_online;
		old_dc_online = chip->dc_online;
	//	old_vcell_0 = chip->vcell_0;
		old_soc_0 = chip->soc_0;
		kodari_get_vcell_0(chip->client);
		kodari_get_soc_0(chip->client);
		kodari_get_battery_online_0(chip->client);
		kodari_get_AC_online(chip->client);
		kodari_get_DC_online(chip->client);
		kodari_get_status_0(chip->client);

//		printk(KERN_ERR "kodari mobile battery 2 type = %d\n");
		
		if(((chip->vcell_0 >= 0) && (old_vcell_0 !=  chip->vcell_0)) || ((chip->soc_0 >= 0) && (old_soc_0 !=  chip->soc_0))) {
			power_supply_changed(&chip->battery_0);
			if(old_soc_0 !=  chip->soc_0) {
				printk("batt1: %d,%d\%\n", chip->vcell_0,chip->soc_0); //esshin
			}
		}

		if(old_ac_online != chip->ac_online){
			power_supply_changed(&chip->ac);
		}

		if(old_dc_online != chip->dc_online){
			//printk(KERN_ERR "@@@@@#####$$$$$ power_supply_changed : dc\n");
			power_supply_changed(&chip->dc);
		}

		old_vcell_1 = chip->vcell_1;
		old_soc_1 = chip->soc_1;
		kodari_get_vcell_1(chip->client);
		kodari_get_soc_1(chip->client);
		kodari_get_battery_online_1(chip->client);
		kodari_get_status_1(chip->client);
		if(((chip->vcell_1 >= 0) && (old_vcell_1 !=  chip->vcell_1)) || ((chip->soc_1 >= 0) && (old_soc_1 !=  chip->soc_1))) {
			power_supply_changed(&chip->battery_1);
			if(old_soc_1 !=  chip->soc_1) {
				printk("batt2: %d,%d\%\n", chip->vcell_1,chip->soc_1); //esshin
			}
		}
	}
	else if(car_mobile_flag == KODARI_BATTERY_TYPE_CAR){	

		old_ac_online = chip->ac_online;
		old_vcell_0 = chip->vcell_0;
		old_soc_0 = chip->soc_0;
		kodari_get_vcell_0(chip->client);
		kodari_get_soc_0(chip->client);
		kodari_get_battery_online_0(chip->client);
		kodari_get_AC_online(chip->client);
		kodari_get_status_0(chip->client);

	//	printk(KERN_ERR "kodari car battery 1 \n");
		
		if(((chip->vcell_0 >= 0) && (old_vcell_0 != chip->vcell_0)) || ((chip->soc_0 >= 0) && (old_soc_0 != chip->soc_0))) {
			power_supply_changed(&chip->battery_0);
			if(old_soc_0 !=  chip->soc_0) {
				printk("batt: %d,%d\%\n", chip->vcell_0,chip->soc_0); //esshin
			}
		}
		if(old_ac_online != chip->ac_online){
			power_supply_changed(&chip->ac);
		}
	}else{
		printk(KERN_ERR "two battery not used\n");	
		cancel_delayed_work(&chip->work);
	}
	
	schedule_delayed_work(&chip->work, KODARI_BATTERY_DELAY);
}

static enum power_supply_property kodari_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_SERIAL_NUMBER //version number
};

	

static enum power_supply_property adapter_get_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

//static enum power_supply_property usb_get_props[] = {
//	POWER_SUPPLY_PROP_ONLINE,
//};
static int adapter_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	int ret = 0;	
	struct kodari_battery_chip *chip = container_of(psy,
					struct kodari_battery_chip, ac);
	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			kodari_get_AC_online(chip->client);
			val->intval = chip->ac_online;
			break;
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}   

static enum power_supply_property dc_adapter_get_props[] = {
	POWER_SUPPLY_PROP_DC_ONLINE,
};

static int dc_adapter_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	int ret = 0;	
	struct kodari_battery_chip *chip = container_of(psy,
					struct kodari_battery_chip, dc);
	switch (psp) {
		case POWER_SUPPLY_PROP_DC_ONLINE:
			//printk("@@@@@#####$$$$$ dc_adapter_get_property\n");
			//kodari_get_DC_online(chip->client);
			val->intval = chip->dc_online;
			break;
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}

//static int usb_get_property(struct power_supply *psy,
//			enum power_supply_property psp,
//			union power_supply_propval *val)
//{
//	int ret = 0;
//	struct max17040_chip *chip = container_of(psy,
//				struct max17040_chip, usb);
//
//	switch (psp) {
//		case POWER_SUPPLY_PROP_ONLINE:
//			val->intval =  0;//chip->usb_online;
//			break;
//		default:
//			ret = -EINVAL;
//			break;
//	}
//	return ret;
//}


//static int __devinit kodari_battery_probe(struct i2c_client *client,
//			const struct i2c_device_id *id)

static int __devinit kodari_battery_probe(struct platform_device *pdev)
{
//	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct kodari_battery_chip *chip;
	int ret;
	int nversion_msb,nversion_lsb;
	int type;

	struct kodari_micom_dev *kodaridev = dev_get_drvdata(pdev->dev.parent);
	struct kodari_micom_platform_data *pdata = dev_get_platdata(kodaridev->dev);


	printk(KERN_ERR "kodari_battery_probe\n");

//	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
//		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	//chip->client = client;
	//	chip->pdata = client->dev.platform_data;

	//i2c_set_clientdata(client, chip);	
	
	kodaridev->kodari_battery_dev = chip;
//	printk(KERN_ERR "kodaridev->kodari_battery_dev \n");

	chip->dev = &pdev->dev;
	chip->client = kodaridev->i2c;
	battery_i2c_client = kodaridev->i2c;

//	printk(KERN_ERR "1 kodaridev->kodari_battery_dev \n");

	type = kodari_get_type(chip->client);
	kodari_type = type; //kodari_get_type(chip->client);

	nversion_msb = kodari_get_version_msb(chip->client);
	kodari_vesion_up = nversion_msb; //kodari_get_version_msb(chip->client);

	nversion_lsb = kodari_get_version_lsb(chip->client);
	kodari_vesion_down = nversion_lsb; //kodari_get_version_lsb(chip->client);

//	printk(KERN_ERR " type = 0x%x, version msb:%i lsb:%i\n",type, nversion_msb, nversion_lsb);

	//for test
	
	//type = KODARI_TYPE_MOBILE;


#if defined(CONFIG_KODARI_TYPE_MOBILE)
	//if( type == KODARI_BATTERY_TYPE_MOBILE){	
		car_mobile_flag = KODARI_BATTERY_TYPE_MOBILE;
		chip->battery_0.name					= "battery-0";
		chip->battery_0.type					= POWER_SUPPLY_TYPE_BATTERY;
		chip->battery_0.get_property			= kodari_get_property_0;
		chip->battery_0.properties				= kodari_battery_props;
		chip->battery_0.num_properties			= ARRAY_SIZE(kodari_battery_props);
		chip->battery_0.external_power_changed 	= NULL;
	
		chip->battery_1.name					= "battery-1";
		chip->battery_1.type					= POWER_SUPPLY_TYPE_BATTERY;
		chip->battery_1.get_property			= kodari_get_property_1;
		chip->battery_1.properties				= kodari_battery_props;
		chip->battery_1.num_properties			= ARRAY_SIZE(kodari_battery_props);
		chip->battery_1.external_power_changed 	= NULL;

		printk(KERN_ERR " type = 0x%x, battery-2 kodari_battery_probe\n",type);

		ret = power_supply_register(&pdev->dev, &chip->battery_0);

//		printk(KERN_ERR " power_supply_register ret  = %d, battery-0 kodari_battery_probe\n",ret);
		
		if (ret)
	        goto err_battery_failed_0;        

		ret = power_supply_register(&pdev->dev, &chip->battery_1);

//		printk(KERN_ERR " power_supply_register ret  = %d, battery-1 kodari_battery_probe\n",ret);

		
		if (ret)
			goto err_battery_failed_1;

		chip->dc.name = "dc";
		chip->dc.type = POWER_SUPPLY_TYPE_MAINS;
		chip->dc.get_property = dc_adapter_get_property;
		chip->dc.properties = dc_adapter_get_props;
		chip->dc.num_properties = ARRAY_SIZE(dc_adapter_get_props);
		chip->dc.external_power_changed = NULL;

		ret = power_supply_register(&pdev->dev, &chip->dc);
	
		if(ret)
			printk(KERN_ERR "power_supply_register() fail, chip->dc\n");
#elif defined(CONFIG_KODARI_TYPE_CAR)
	//}else if(type == KODARI_BATTERY_TYPE_CAR){
		car_mobile_flag =KODARI_BATTERY_TYPE_CAR;
		chip->battery_0.name					= "battery";
		chip->battery_0.type					= POWER_SUPPLY_TYPE_BATTERY;
		chip->battery_0.get_property			= kodari_get_property_0;
		chip->battery_0.properties				= kodari_battery_props;
		chip->battery_0.num_properties			= ARRAY_SIZE(kodari_battery_props);
		chip->battery_0.external_power_changed 	= NULL;

//		printk(KERN_ERR " type = %d, battery-1 kodari_battery_probe\n",type);

		ret = power_supply_register(&pdev->dev, &chip->battery_0);
		if (ret)
	        goto err_battery_failed_0; 
	//}else{
		//car_mobile_flag = 0;
		//goto err_battery_failed_0;
	//}
#endif

    chip->ac.name       				= "ac";
	chip->ac.type       				= POWER_SUPPLY_TYPE_MAINS;
	chip->ac.get_property 		 	    = adapter_get_property;
	chip->ac.properties 				= adapter_get_props;
	chip->ac.num_properties 			= ARRAY_SIZE(adapter_get_props);
	chip->ac.external_power_changed 	= NULL;

  

    ret = power_supply_register(&pdev->dev, &chip->ac);

	printk(KERN_ERR " power_supply_register ret  = %d, chip->ac kodari_battery_probe\n",ret);
	
	if(ret)
		goto err_ac_failed;

	sprintf(chip->strVer, "%02i.%02i", nversion_msb, nversion_lsb);	

	if(car_mobile_flag == KODARI_BATTERY_TYPE_MOBILE || car_mobile_flag == KODARI_BATTERY_TYPE_CAR){
		INIT_DELAYED_WORK_DEFERRABLE(&chip->work, kodari_work);
		schedule_delayed_work(&chip->work, KODARI_BATTERY_DELAY);
		printk(KERN_ERR "kodari_battery_probe schedule_delayed_work\n");
		
	}

	return 0;


err_ac_failed:	
	if(car_mobile_flag == KODARI_BATTERY_TYPE_MOBILE)
		power_supply_unregister(&chip->battery_1);
err_battery_failed_1:
	power_supply_unregister(&chip->battery_0);
err_battery_failed_0:
	dev_err(&pdev->dev, "failed: power supply register\n");
	//i2c_set_clientdata(client, NULL);
	kfree(chip);

	return ret;

}

static int __devexit kodari_battery_remove(struct i2c_client *client)
{
	struct kodari_battery_chip *chip = i2c_get_clientdata(client);

	power_supply_unregister(&chip->battery_0);
	power_supply_unregister(&chip->battery_1);
	cancel_delayed_work(&chip->work);
	kfree(chip);
	return 0;
}

#ifdef CONFIG_PM

static int kodari_battery_suspend(struct i2c_client *client,
		pm_message_t state)
{
	struct kodari_battery_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work(&chip->work);
	return 0;
}

static int kodari_battery_resume(struct i2c_client *client)
{
	struct kodari_battery_chip *chip = i2c_get_clientdata(client);

	schedule_delayed_work(&chip->work, KODARI_BATTERY_DELAY);
	return 0;
}

#else

#define kodari_battery_suspend NULL
#define kodari_battery_resume NULL

#endif /* CONFIG_PM */

//static const struct i2c_device_id kodari_id[] = {
//	{ "kodari-battery", 0 },
//	{ }
//};

static const struct platform_device_id kodari_battery_id[] = {
	{ "kodari-micom-battery", 0 },	
};

static void kodari_battery_shutdown(struct platform_device *pdev)
{
	
}




static struct platform_driver kodari_battery_driver = {
	.driver		= {
		.name	= "kodari-micom-battery",
		.owner	= THIS_MODULE,
	},
	.probe		= kodari_battery_probe,
	.remove		= __devexit_p(kodari_battery_remove),
	.shutdown	= kodari_battery_shutdown,
	.id_table	= kodari_battery_id,
};


static int __init kodari_battery_init(void)
{
	mykobj1 = kzalloc(sizeof(*mykobj1), GFP_KERNEL);
	if (mykobj1) {
		kobject_init(mykobj1, &mytype1);
		if (kobject_add(mykobj1, NULL, "%s", "type")) {
			printk("kodari_type: kobject_add() failed\n");
			kobject_put(mykobj1);
			mykobj1 = NULL;
		}
	}

	mykobj2 = kzalloc(sizeof(*mykobj2), GFP_KERNEL);
	if (mykobj2) {
		kobject_init(mykobj2, &mytype2);
		if (kobject_add(mykobj2, NULL, "%s", "version_up")) {
			printk("version_up: kobject_add() failed\n");
			kobject_put(mykobj2);
			mykobj2 = NULL;
		}
	}

	mykobj3 = kzalloc(sizeof(*mykobj3), GFP_KERNEL);
	if (mykobj3) {
		kobject_init(mykobj3, &mytype3);
		if (kobject_add(mykobj3, NULL, "%s", "version_down")) {
			printk("version_down: kobject_add() failed\n");
			kobject_put(mykobj3);
			mykobj3 = NULL;
		}
	}

	mykobj4 = kzalloc(sizeof(*mykobj4), GFP_KERNEL);
	if (mykobj4) {
		kobject_init(mykobj4, &mytype4);
		if (kobject_add(mykobj4, NULL, "%s", "battery_led")) {
			printk("version_down: kobject_add() failed\n");
			kobject_put(mykobj4);
			mykobj4 = NULL;
		}
	}

	return platform_driver_register(&kodari_battery_driver);
}
module_init(kodari_battery_init);

static void __exit kodari_battery_exit(void)
{
    if (mykobj1) {
        kobject_put(mykobj1);
        kfree(mykobj1);
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

    platform_driver_unregister(&kodari_battery_driver);
}
module_exit(kodari_battery_exit);

/*
//MODULE_DEVICE_TABLE(i2c, kodari_id);

static struct i2c_driver kodari_battery_i2c_driver = {
	.driver	= {
		.name	= "kodari-battery",
	},
	.probe		= kodari_battery_probe,
	.remove		= __devexit_p(kodari_remove),
	.suspend	= kodari_battery_suspend,
	.resume		= kodari_battery_resume,
	.id_table	= kodari_id,
};

static int __init kodari_battery_init(void)
{
	return i2c_add_driver(&kodari_battery_i2c_driver);
}
module_init(kodari_battery_init);

static void __exit kodari_battery_exit(void)
{
	i2c_del_driver(&kodari_battery_i2c_driver);
}
module_exit(kodari_battery_exit);
*/
MODULE_AUTHOR("hclee@kodari.co.kr");
MODULE_DESCRIPTION("KODARI battery Fuel Gauge");
MODULE_LICENSE("GPL");
