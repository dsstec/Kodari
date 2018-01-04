/*
 * s5m87xx-irq.c
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

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mfd/kodari/kodari-micom-core.h>
#include<linux/gpio.h>
#include <linux/slab.h>

struct kodari_micom_irq_data {
	int reg;
	int mask;
};

static struct kodari_micom_irq_data kodari_micom_irqs[] = {		
	[KODARI_MICOM_BMODULE_ENC_PWR_ON] = {
		.reg = 1,
		.mask = KODARI_MICOM_BMODULE_ENC_PWR_ON_MASK,
	},
	[KODARI_MICOM_BMODULE_ENC_ERASE_OK] = {
		.reg = 1,
		.mask = KODARI_MICOM_BMODULE_ENC_ERASE_OK_MASK,
	},
	[KODARI_MICOM_BMODULE_ENC_ERASE_OK] = {
		.reg = 1,
		.mask = KODARI_MICOM_BMODULE_ENC_ERASE_OK_MASK,
	},
	[KODARI_MICOM_BMODULE_ENC_FAIL] = {
		.reg = 1,
		.mask = KODARI_MICOM_BMODULE_ENC_FAIL_MASK,
	},
	[KODARI_MICOM_BMODULE_ENC_INJECT_OK] = {
		.reg = 1,
		.mask = KODARI_MICOM_BMODULE_ENC_INJECT_OK_MASK,
	},
	[KODARI_MICOM_BMODULE_ENC_INJECT_ING] = {
		.reg = 1,
		.mask = KODARI_MICOM_BMODULE_ENC_INJECT_ING_MASK,
	},
	[KODARI_MICOM_BMODULE_ENC_INJECT_RST] = {
		.reg = 1,
		.mask = KODARI_MICOM_BMODULE_ENC_INJECT_RST_MASK,
	},
	[KODARI_MICOM_BMODULE_ENC_INJECT_BOOT_OK] = {
		.reg = 1,
		.mask = KODARI_MICOM_BMODULE_ENC_INJECT_BOOT_OK_MASK,
	},
	[KODARI_MICOM_BMODULE_ENC_INJECT_DETECT] = {
		.reg = 1,
		.mask = KODARI_MICOM_BMODULE_ENC_INJECT_DETECT_MASK,
	},
};

struct my_attr {
    struct attribute attr;
    int value;
};

/* ------------------  Sysfile1: /sys/bmodule_injecting/done  ------------------ */
static struct kobject *mykobj;

static struct my_attr done = {
    .attr.name="done",
    .attr.mode = 0444,
    .value = 0,
};

static struct attribute * myattr[] = {
    &done.attr,
    NULL
};

static int bmodule_injecting_done = 0;

static ssize_t bmodule_injecting_done_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", bmodule_injecting_done);
}

static struct sysfs_ops myops = {
    .show = bmodule_injecting_done_show,
};

static struct kobj_type mytype = {
    .sysfs_ops = &myops,
    .default_attrs = myattr,
};

static inline struct kodari_micom_irq_data *
irq_to_kodari_micom_irq(struct kodari_micom_dev *kodaridev, int irq)
{
//leehc remark too many logging
//	dev_err(kodaridev->dev, "irq_to_kodari_micom_irq: %d, kodari_micom_irqs[irq - kodaridev->irq_base]:%d\n",
//			irq, irq - kodaridev->irq_base);
	

	return &kodari_micom_irqs[irq - kodaridev->irq_base];
}

static void kodari_micom_irq_lock(struct irq_data *data)
{
	struct kodari_micom_dev *kodaridev = irq_data_get_irq_chip_data(data);

//leech 
	//dev_err(kodaridev->dev, "kodari_micom_irq_lock\n");

	mutex_lock(&kodaridev->irqlock);
}

static void kodari_micom_irq_sync_unlock(struct irq_data *data)
{
	struct kodari_micom_dev *kodaridev = irq_data_get_irq_chip_data(data);
	int i;

//	for (i = 0; i < ARRAY_SIZE(kodaridev->irq_masks_cur); i++) {
//		if (kodaridev->irq_masks_cur[i] != kodaridev->irq_masks_cache[i]) {
//			kodaridev->irq_masks_cache[i] = kodaridev->irq_masks_cur[i];
//			kodari_micom_reg_write(kodaridev->i2c, S5M8767_REG_INT1M + i,
//					kodaridev->irq_masks_cur[i]);
//		}
//	}

	//leehc remark too many logging
	//dev_err(kodaridev->dev, "kodari_micom_irq_sync_unlock\n");

	
	if (kodaridev->irq_masks_cur != kodaridev->irq_masks_cache) {
		kodaridev->irq_masks_cache = kodaridev->irq_masks_cur;
		kodari_micom_reg_write(kodaridev->i2c, KODARI_BMODULE_STATES,
				kodaridev->irq_masks_cur);
		//leehc remark too many logging
		//dev_err(kodaridev->dev, "kodari_micom_irq_sync_unlock - kodari_micom_reg_write\n");
		
	}
	mutex_unlock(&kodaridev->irqlock);
}

static void kodari_micom_irq_unmask(struct irq_data *data)
{
	struct kodari_micom_dev *kodaridev = irq_data_get_irq_chip_data(data);
	struct kodari_micom_irq_data *irq_data = irq_to_kodari_micom_irq(kodaridev,
							       data->irq);

	dev_err(kodaridev->dev, "kodaridev->irq_masks_cur: 0x%x, irq_data->mask: 0x%x\n",
			kodaridev->irq_masks_cur, irq_data->mask);
	

	kodaridev->irq_masks_cur &= ~irq_data->mask;

	dev_err(kodaridev->dev, "kodaridev->irq_masks_cur: 0x%x\n",kodaridev->irq_masks_cur);
}

static void kodari_micom_irq_mask(struct irq_data *data)
{
	struct kodari_micom_dev *kodaridev = irq_data_get_irq_chip_data(data);
	struct kodari_micom_irq_data *irq_data = irq_to_kodari_micom_irq(kodaridev,
							       data->irq);

	dev_err(kodaridev->dev, "kodaridev->irq_masks_cur: 0x%x, irq_data->mask: 0x%x\n",
		kodaridev->irq_masks_cur, irq_data->mask);
	
	kodaridev->irq_masks_cur |= irq_data->mask;
	
	dev_err(kodaridev->dev, "kodaridev->irq_masks_cur: 0x%x\n",kodaridev->irq_masks_cur);

}

static struct irq_chip kodari_micom_irq_chip = {
	.name = "kodari-micom-irq-chip",
	.irq_bus_lock = kodari_micom_irq_lock,
	.irq_bus_sync_unlock = kodari_micom_irq_sync_unlock,
	.irq_mask = kodari_micom_irq_mask,
	.irq_unmask = kodari_micom_irq_unmask,
};

int bmodule_detect_status;
EXPORT_SYMBOL(bmodule_detect_status);

static irqreturn_t kodari_micom_irq_thread(int irq, void *data)
{
	struct kodari_micom_dev *kodaridev = data;
	//u8 irq_reg[KODARI_MICOM_NUM_IRQ_REGS-1];
	u8 irq_reg;
	int ret;
	int i;
	static int bmodule_injecting = 0;
	//dev_err(kodaridev->dev, "kodari-micom irq handled\n");

	if(bmodule_probe_done == 0)
		return IRQ_HANDLED;

	ret = kodari_micom_reg_read(kodaridev->i2c, KODARI_BMODULE_STATES, &irq_reg);
	if (ret < 0) {
		dev_err(kodaridev->dev, "kodari-micom irq thread Failed to read interrupt register: %d\n",
				ret);
		return IRQ_NONE;
	}

	//	dev_err(kodaridev->dev, "kodari-micom irq read: 0x%x\n",
	//			irq_reg);

//	for (i = 0; i < KODARI_MICOM_NUM_IRQ_REGS - 1; i++)
//		irq_reg[i] &= ~kodaridev->irq_masks_cur[i];

	dev_err(kodaridev->dev, "~kodaridev->irq_masks_cur: 0x%x\n",
			~kodaridev->irq_masks_cur);

	printk("$$$$$$$ ~kodaridev->irq_masks_cur: 0x%x\n", ~kodaridev->irq_masks_cur);
	printk("@@@@@@@ irq_reg = 0x%x\n", irq_reg);
	
	irq_reg &= ~kodaridev->irq_masks_cur;
	dev_err(kodaridev->dev, "kodari-micom irq bit mask: 0x%x\n",
			irq_reg);
	
	printk("####### irq_reg = 0x%x\n", irq_reg);


//	for (i = 0; i < KODARI_MICOM_IRQ_NR; i++) {
//		if (irq_reg[kodari_micom_irqs[i].reg - 1] & kodari_micom_irqs[i].mask)
//			handle_nested_irq(kodaridev->irq_base + i);
//	}

#if 0
	for (i = 0; i < KODARI_MICOM_IRQ_NR; i++) {
			if (irq_reg & kodari_micom_irqs[i].mask) {
				handle_nested_irq(kodaridev->irq_base + i);
				dev_err(kodaridev->dev, "kodari-micom irq handed mask: 0x%x\n",
				kodari_micom_irqs[i].mask);
			}

			if (i == KODARI_MICOM_BMODULE_ENC_INJECT_DETECT) {
				if (!(irq_reg & KODARI_MICOM_BMODULE_ENC_INJECT_DETECT_MASK)) {
					bmodule_detect_status = 0;
				}
			}
	}
#else
         for (i = KODARI_MICOM_BMODULE_ENC_INJECT_DETECT; i >= 0; i--) {
			if (irq_reg & kodari_micom_irqs[i].mask) {
				handle_nested_irq(kodaridev->irq_base + i);
				dev_err(kodaridev->dev, "kodari-micom irq handed mask: 0x%x\n",
				kodari_micom_irqs[i].mask);
			}

			if (i == KODARI_MICOM_BMODULE_ENC_INJECT_DETECT) {
				if (!(irq_reg & KODARI_MICOM_BMODULE_ENC_INJECT_DETECT_MASK)) {
					bmodule_detect_status = 0;
				}
			}

			if (i == KODARI_MICOM_BMODULE_ENC_INJECT_ING) {
				if (irq_reg & KODARI_MICOM_BMODULE_ENC_INJECT_ING_MASK) {
					bmodule_injecting = 1;
					bmodule_injecting_done = 0;
					printk("[bmodule] injecting start\n");
				} else {
					if(bmodule_injecting == 1) {
						bmodule_injecting_done = 1;
						sysfs_notify(mykobj, NULL, "done"); // sysfile update
						printk("[bmodule] injecting done\n");
					}
					bmodule_injecting = 0;
				}
			}
	}
#endif

	return IRQ_HANDLED;
}

int kodari_micom_irq_resume(struct kodari_micom_dev *kodaridev)
{
	if (kodaridev->irq && kodaridev->irq_base){
		kodari_micom_irq_thread(kodaridev->irq_base, kodaridev);

	}	
	return 0;
}


int kodari_micom_irq_init(struct kodari_micom_dev *kodaridev)
{
	int i;
	int cur_irq;
	int ret = 0;
	int irq;


	if (!kodaridev->irq) {
		dev_warn(kodaridev->dev,
			 "No interrupt specified, no interrupts\n");
		kodaridev->irq_base = 0;
		return 0;
	}

	if (!kodaridev->irq_base) {
		dev_err(kodaridev->dev,
			"No interrupt base specified, no interrupts\n");
		return 0;
	}

	mutex_init(&kodaridev->irqlock);




	//for (i = 0; i < KODARI_MICOM_NUM_IRQ_REGS - 1; i++) {
		kodaridev->irq_masks_cur = 0x0;
		kodaridev->irq_masks_cache = 0x0;
	//	kodari_micom_reg_write(kodaridev->i2c, KODARI_BMODULE_STATES,0x0);
//	}
	dev_err(kodaridev->dev, "KODARI_MICOM_IRQ_NR: %d\n",KODARI_MICOM_IRQ_NR);
	for (i = 0; i < KODARI_MICOM_IRQ_NR; i++) {
		
		cur_irq = i + kodaridev->irq_base;
		dev_err(kodaridev->dev, "cur_irq: %d\n",cur_irq);
		irq_set_chip_data(cur_irq, kodaridev);
		if (ret) {
			dev_err(kodaridev->dev,
				"Failed to irq_set_chip_data %d: %d\n",
				kodaridev->irq, ret);
			return ret;
		}

		irq_set_chip_and_handler(cur_irq, &kodari_micom_irq_chip,
					 handle_edge_irq);
	//leehc	
//		dev_err(kodaridev->dev, "irq_set_chip_and_handler IRQ %d: %d\n",cur_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	ret = request_threaded_irq(kodaridev->irq, NULL,
				   kodari_micom_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   "kodari-micom-irq", kodaridev);
	if (ret) {
		dev_err(kodaridev->dev, "Failed to request IRQ %d: %d\n",
			kodaridev->irq, ret);
		return ret;
	}

	mykobj = kzalloc(sizeof(*mykobj), GFP_KERNEL);
	if (mykobj) {
		kobject_init(mykobj, &mytype);
		if (kobject_add(mykobj, NULL, "%s", "bmodule_injecting")) {
			printk("bmodule_injecting: kobject_add() failed\n");
			kobject_put(mykobj);
			mykobj = NULL;
		}
	}

	return 0;
}

void kodari_micom_irq_exit(struct kodari_micom_dev *kodaridev)
{

	if (kodaridev->irq)
		free_irq(kodaridev->irq, kodaridev);

	if (mykobj) {
		kobject_put(mykobj);
		kfree(mykobj);
	}
}
