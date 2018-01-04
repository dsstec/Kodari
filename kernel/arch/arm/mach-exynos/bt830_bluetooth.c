/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
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
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-kodari.h>

#define DRV_NAME "bt830"

#define BT_HOST_WAKEUP

#ifdef BT_HOST_WAKEUP
static struct work_struct bt_wakeup_work_queue;
static struct wake_lock bt_wake_lock;

static void bt_wakeup_work_func(struct work_struct *work)
{
	wake_lock_timeout(&bt_wake_lock, 4 * HZ);
}

static irqreturn_t bt_wakeup_irq(int irq, void *handle)
{
	schedule_work(&bt_wakeup_work_queue);

	return IRQ_HANDLED;
}
#endif

/* Bluetooth control */
static void bt830_enable(int on)
{
	if (on) {
		/* Power on the chip */
		gpio_direction_output(GPIO_BLUETOOTH_EN_H, 1);
		printk("bt enable\n");
		/* Reset the chip */
		mdelay(10);	
	}
	else {
		gpio_direction_output(GPIO_BLUETOOTH_EN_H, 1);
		printk("bt disable\n");
		mdelay(10);
	}
}

static int bt830_set_block(void *data, bool blocked)
{
	bt830_enable(!blocked);
	return 0;
}

static const struct rfkill_ops bt830_rfkill_ops = {
	.set_block = bt830_set_block,
};

static int __devinit bt830_probe(struct platform_device *pdev)
{
	struct rfkill *rfk;
	int ret = 0;
	int err = 0;

	ret = gpio_request(GPIO_BLUETOOTH_EN_H, dev_name(&pdev->dev));
	if (ret) {
		dev_err(&pdev->dev, "could not get BT_POWER\n");
		return ret;
	}

	rfk = rfkill_alloc(DRV_NAME, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			&bt830_rfkill_ops, NULL);
	if (!rfk) {
		ret = -ENOMEM;
		goto err_rfk_alloc;
	}

	ret = rfkill_register(rfk);
	if (ret)
		goto err_rfkill;

	platform_set_drvdata(pdev, rfk);

#ifdef BT_HOST_WAKEUP
	wake_lock_init(&bt_wake_lock, WAKE_LOCK_SUSPEND, "bt_wake_lock");

	INIT_WORK(&bt_wakeup_work_queue, bt_wakeup_work_func);

	s3c_gpio_cfgpin(GPIO_BLUETOOTH_WAKEUP, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(GPIO_BLUETOOTH_WAKEUP, S3C_GPIO_PULL_NONE);

	err = request_threaded_irq(GPIO_BLUETOOTH_WAKEUP_IRQ,
							NULL, bt_wakeup_irq, IRQF_TRIGGER_RISING,
							"bt_wakeup_irq", rfk);
	if(err < 0)
		printk("%s: request_threaded_irq() fail\n", __func__);

	enable_irq_wake(GPIO_BLUETOOTH_WAKEUP_IRQ);
#endif

	return 0;

err_rfkill:
	rfkill_destroy(rfk);
err_rfk_alloc:
	return ret;
}

static int bt830_remove(struct platform_device *pdev)
{
	struct rfkill *rfk = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	
	if (rfk) {
		rfkill_unregister(rfk);
		rfkill_destroy(rfk);
	}
	rfk = NULL;

#ifdef BT_HOST_WAKEUP
	wake_lock_destroy(&bt_wake_lock);
#endif

	bt830_enable(0);

	gpio_free(GPIO_BLUETOOTH_EN_H);

	return 0;
}

static struct platform_driver bt830_driver = {
	.driver		= {
		.name	= DRV_NAME,
	},
	.probe		= bt830_probe,
	.remove		= bt830_remove,
};

static int __init bt830_init(void)
{
	return platform_driver_register(&bt830_driver);
}

static void __exit bt830_exit(void)
{
	platform_driver_unregister(&bt830_driver);
}

module_init(bt830_init);
module_exit(bt830_exit);

MODULE_AUTHOR("KODARI");
MODULE_DESCRIPTION("Driver for the BT830 bluetooth chip");
MODULE_LICENSE("GPL");