/*
 *  drivers/switch/switch_gpio.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>


struct gpio_switch_data {
	struct switch_dev sdev;
	unsigned gpio;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	int irq;
	struct work_struct work;
};


struct gpio_switch_priv {
	int num_switches;
	struct gpio_switch_data switches[];
};
static void gpio_switch_work(struct work_struct *work)
{
	int state;
	struct gpio_switch_data	*data =
		container_of(work, struct gpio_switch_data, work);

	state = gpio_get_value(data->gpio);

	//printk(KERN_ERR "%s -- name:%s, gpio:%i\n", __func__, data->sdev.name, data->gpio);
	
	switch_set_state(&data->sdev, state);
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct gpio_switch_data *switch_data =
	    (struct gpio_switch_data *)dev_id;

	schedule_work(&switch_data->work);
	return IRQ_HANDLED;
}

static ssize_t switch_gpio_print_state(struct switch_dev *sdev, char *buf)
{
	struct gpio_switch_data	*switch_data =
		container_of(sdev, struct gpio_switch_data, sdev);
	const char *state;
	if (switch_get_state(sdev))
		state = switch_data->state_on;
	else
		state = switch_data->state_off;

	if (state)
		return sprintf(buf, "%s\n", state);
	return -1;
}

static int __devinit create_gpio_switch(const struct gpio_switch * template,
	struct gpio_switch_data * switch_data, struct device *parent) 
{
	int ret = 0;
	switch_data = kzalloc(sizeof(struct gpio_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;
	
	switch_data->sdev.name = template->name;
	switch_data->gpio = template->gpio;
	switch_data->name_on = template->name_on;
	switch_data->name_off = template->name_off;
	switch_data->state_on = template->state_on;
	switch_data->state_off = template->state_off;
	switch_data->sdev.print_state = switch_gpio_print_state;
	
	//printk(KERN_ERR "%s sdev.name:%s\n", __func__, switch_data->sdev.name);
	  ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0) {
		printk(KERN_ERR "%s error dev register:%i\n", __func__, ret);
		goto err_switch_dev_register;
	}

	ret = gpio_request(switch_data->gpio, switch_data->sdev.name);
	if (ret < 0) {
		printk(KERN_ERR "%s error gpio(%i) request:%i\n", __func__, switch_data->gpio, ret);
		goto err_request_gpio;
	}

	ret = gpio_direction_input(switch_data->gpio); 
	if (ret < 0) {
		printk(KERN_ERR "%s error gpio(%i) input request:%i\n", __func__, switch_data->gpio, ret);
		goto err_set_gpio_input;
	}

	INIT_WORK(&switch_data->work, gpio_switch_work);

	switch_data->irq = gpio_to_irq(switch_data->gpio);
	if (switch_data->irq < 0) {
		ret = switch_data->irq;
		printk(KERN_ERR "%s - fail to gpio to irq: ret:%i\n", __func__, ret);
		goto err_detect_irq_num_failed;
	}

	ret = request_irq(switch_data->irq, gpio_irq_handler,
		//	  IRQF_TRIGGER_LOW, pdev->name, switch_data);
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                  switch_data->sdev.name, switch_data);
	if (ret < 0) {
		printk(KERN_ERR "%s - fail to request_irq: ret:%i\n", __func__, ret);
		goto err_request_irq;
	}

	/* Perform initial detection */
	gpio_switch_work(&switch_data->work);

	return 0;
err_request_irq:
err_detect_irq_num_failed:
err_set_gpio_input:
	gpio_free(switch_data->gpio);
err_request_gpio:
    switch_dev_unregister(&switch_data->sdev);
err_switch_dev_register:
	kfree(switch_data);


	return ret;

}
	

static int gpio_switch_probe(struct platform_device *pdev)
{
	struct gpio_switch_platform_data *pdata = pdev->dev.platform_data;
//	struct gpio_switch_data *switch_data;
	struct gpio_switch_priv * priv;
	int i;
	int ret = 0;

	//printk(KERN_ERR "%s\n", __func__);

	if (!pdata || (pdata->num_switches == 0))
		return -EBUSY;

	priv = kzalloc(sizeof(struct gpio_switch_priv), GFP_KERNEL);

	if(!priv) return -ENOMEM;

	priv->num_switches = pdata->num_switches;

	//printk(KERN_ERR "%s num:%i\n", __func__, priv->num_switches);

	for(i=0; i < priv->num_switches; i++) {
		//printk(KERN_ERR "%s create num:%i\n", __func__, i);
		ret = create_gpio_switch(&pdata->switches[i], &priv->switches[i], &pdev->dev);
		//printk(KERN_ERR "%s create ret:%i\n", __func__, ret);
		if(ret < 0) {
			kfree(priv);
			return ret;
		}
			
	}
	platform_set_drvdata(pdev, priv);
	
	return 0;
}

static void delete_gpio_switch(struct gpio_switch_data *switch_data)
{
	if (!gpio_is_valid(switch_data->gpio))
		return;
	cancel_work_sync(&switch_data->work);
	gpio_free(switch_data->gpio);
	switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);
}


static int __devexit gpio_switch_remove(struct platform_device *pdev)
{
	struct gpio_switch_priv *priv = platform_get_drvdata(pdev);
	int i;
	for (i = 0; i < priv->num_switches; i++)
		delete_gpio_switch(&priv->switches[i]);

	return 0;
}



static struct platform_driver gpio_switch_driver = {
	.probe		= gpio_switch_probe,
	.remove		= __devexit_p(gpio_switch_remove),
	.driver		= {
		.name	= "switch-gpio",
		.owner	= THIS_MODULE,
	},
};





static int __init gpio_switch_init(void)
{
	printk(KERN_ERR "%s\n", __func__);
	
	return platform_driver_register(&gpio_switch_driver);
}

static void __exit gpio_switch_exit(void)
{
	printk(KERN_ERR "%s\n", __func__);

	platform_driver_unregister(&gpio_switch_driver);
}

module_init(gpio_switch_init);
module_exit(gpio_switch_exit);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("GPIO Switch driver");
MODULE_LICENSE("GPL");
