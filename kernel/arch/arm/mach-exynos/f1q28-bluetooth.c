/*
 * arch/arm/mach-exynos/f1q28-bluetooth.c
 * Copyright (c) Lee Hyung Chul <hclee@kodari.co.kr>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	    kodari bluetooth "driver"
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/rfkill.h>
#include <linux/leds.h>



#include <mach/gpio-kodari.h>

#define DRV_NAME "f1q28-bt"

/* Bluetooth control */
static void f1q28bt_enable(int on)
{
	if (on) {
		/* Power on the chip */
		gpio_direction_output(GPIO_BLUETOOTH_EN_H, 1); //gpio_set_value(GPIO_BLUETOOTH_EN_H, 1);
		printk(KERN_ERR "%s - bt enable\n", __func__);
		/* Reset the chip */
		mdelay(10);	
	}
	else {

		gpio_direction_output(GPIO_BLUETOOTH_EN_H, 1); //gpio_set_value(GPIO_BLUETOOTH_EN_H, 0);
		printk(KERN_ERR "%s - bt disable\n", __func__);
		mdelay(10);
	}
}

static int f1q28bt_set_block(void *data, bool blocked)
{
	f1q28bt_enable(!blocked);
	return 0;
}

static const struct rfkill_ops f1q28bt_rfkill_ops = {
	.set_block = f1q28bt_set_block,
};

static int __devinit f1q28bt_probe(struct platform_device *pdev)
{
	struct rfkill *rfk;
	int ret = 0;


	ret = gpio_request(GPIO_BLUETOOTH_EN_H, dev_name(&pdev->dev));
	if (ret) {
		dev_err(&pdev->dev, "could not get BT_POWER\n");
		return ret;
	}


	rfk = rfkill_alloc(DRV_NAME, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			&f1q28bt_rfkill_ops, NULL);
	if (!rfk) {
		ret = -ENOMEM;
		goto err_rfk_alloc;
	}

	ret = rfkill_register(rfk);
	if (ret)
		goto err_rfkill;

	platform_set_drvdata(pdev, rfk);

	return 0;

err_rfkill:
	rfkill_destroy(rfk);
err_rfk_alloc:
	return ret;
}

static int f1q28bt_remove(struct platform_device *pdev)
{
	struct rfkill *rfk = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	gpio_free(GPIO_BLUETOOTH_EN_H);

	if (rfk) {
		rfkill_unregister(rfk);
		rfkill_destroy(rfk);
	}
	rfk = NULL;

	f1q28bt_enable(0);

	return 0;
}


static struct platform_driver f1q28bt_driver = {
	.driver		= {
		.name	= DRV_NAME,
	},
	.probe		= f1q28bt_probe,
	.remove		= f1q28bt_remove,
};


static int __init f1q28bt_init(void)
{
	return platform_driver_register(&f1q28bt_driver);
}

static void __exit f1q28bt_exit(void)
{
	platform_driver_unregister(&f1q28bt_driver);
}

module_init(f1q28bt_init);
module_exit(f1q28bt_exit);

MODULE_AUTHOR("Lee Hyung Chul <hclee@kodari.co.kr>");
MODULE_DESCRIPTION("Driver for the F1Q28 bluetooth chip");
MODULE_LICENSE("GPL");



