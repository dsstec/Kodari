/*
 * exynos-usb-switch.c - USB switch driver for Exynos
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 * Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include <plat/devs.h>
#include <plat/ehci.h>
#include <plat/usbgadget.h>

#include <mach/regs-clock.h>

#include "../gadget/s3c_udc.h"
#include "exynos-usb-switch.h"


//leehc
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/kmod.h>
#include <mach/gpio-kodari.h>



#define DRIVER_DESC "Exynos USB Switch Driver"
#define SWITCH_WAIT_TIME	500
#define WAIT_TIMES		10

static const char switch_name[] = "exynos_usb_switch";

static struct exynos_usb_switch *g_usb_switch = NULL;

unsigned char chk_suspend_hub_status=0;

EXPORT_SYMBOL(chk_suspend_hub_status);


#define HUB_UNBIND_DRIVER_PATH "/sys/bus/usb/drivers/hub/unbind"
#define HUB_BIND_DRIVER_PATH "/sys/bus/usb/drivers/hub/bind"

//#define TUSB_RESET_PATH "/sys/class/gpio/gpio128/value"
//#define HUB_RESET_PATH "/sys/class/gpio/gpio195/value"

#ifdef CONFIG_KODARI_USB_HUB_SEPARATE
#define TUSB_POWER_PATH "/sys/class/gpio/gpio196/value"
#define HUB1_POWER_PATH "/sys/class/gpio/gpio203/value"
#define HUB2_POWER_PATH "/sys/class/gpio/gpio195/value"
#else
#define TUSB_POWER_PATH "/sys/class/gpio/gpio132/value"
#define HUB_POWER_PATH "/sys/class/gpio/gpio131/value"
#endif

#define OTG_POWER_PATH "/sys/class/gpio/gpio129/value"

#define BMODULE_POWER_PATH "/sys/bmodule_power/power"

#define HUB1_DEV_ID "1-0:1.0"
#define HUB2_DEV_ID "2-0:1.0"




struct proc_dir_entry *kodari_dev_dir;
#define KODARI_HUB "kodari_hub"
static int _kodari_ssd 		= 0;
static int _kodari_tmft 		= 0;
static int _kodari_bmodule 	= 0;
static int _kodari_keyboard 	= 0;
static int _kodari_otg		= 0;
static int _kodari_states		= 0;



static int _dev_ssd = 0;
static int _dev_tmft = 0;
static int _dev_bmodule = 0;
static int _dev_keyboard = 0;
static int _dev_otg = 0;



enum usb_dev_status {
	USB_HOST_SSD = 1,
	USB_HOST_TMFT = 2,
	USB_HOST_BMODULE = 4,
	USB_HOST_KEYBOARD = 8,
	USB_DEVICE_OTG = 16,
};





#define	PROC_BUF_MAX	80
#define	_str_format_s32	"%d\n"


static void hub_ssd_off(void);
static void hub_ssd_on(void);
static void hub_tmft_on(void);
static void hub_tmft_off(void);
static void hub_bmodule_on(void);
static void hub_bmodule_off(void);
static void hub_keyboard_on(void);
static void hub_keyboard_off(void);
static void hub_otg_on(void);
static void hub_otg_off(void);


static int is_device_detected(void) ;

int get_proc_str(const char *from, char *to, int count, int max)
{
	if (count > max-1) count = max - 1;
	if (copy_from_user(to,from,count)) return -EFAULT;
	to[count] = '\0';
	return count;
}
 
int read_proc_s32(char *buf,char **start,off_t offset,int count,int *eof,void *data)
{
	int ret = -1;
	if(g_usb_switch) {
		mutex_lock(&g_usb_switch->mutex);
		ret = sprintf(buf,_str_format_s32,*(s32*)data); 
		mutex_unlock(&g_usb_switch->mutex);
	}
	return ret;
}


int read_proc_states(char *buf,char **start,off_t offset,int count,int *eof,void *data)
{
	int ret = -1;
	if(g_usb_switch) {
		mutex_lock(&g_usb_switch->mutex);
		*(s32*)data = is_device_detected();
		ret = sprintf(buf,_str_format_s32,*(s32*)data); 
		mutex_unlock(&g_usb_switch->mutex);
	}
	return ret;
}


 
int write_proc_ssd (struct file *file,const char *buf,unsigned long count,void *data)
{
	char procbuf[PROC_BUF_MAX];
	s32 ssd_on;
	int ret;
	
	if(g_usb_switch) {		
		mutex_lock(&g_usb_switch->mutex);
		ssd_on = _kodari_ssd;
		ret = get_proc_str(buf,procbuf,count,sizeof(procbuf));
                if(ssd_on) chk_suspend_hub_status=0;
                else chk_suspend_hub_status=1;

		printk(KERN_ERR "%s - 1 - ssd_on:%i\n", __func__, ssd_on);
		if (ret<0) {
			mutex_unlock(&g_usb_switch->mutex);
			return ret;
		}
		*(unsigned int*)data = (unsigned int) simple_strtoll(procbuf,NULL,10);
		printk(KERN_ERR "%s - 2 - ssd_on:%i, _kodari_ssd:%i\n", __func__, ssd_on, _kodari_ssd);
		if(ssd_on != _kodari_ssd) {
			if(_kodari_ssd == 1 ) {
				hub_ssd_on();
			} else if (_kodari_ssd == 0) {
				hub_ssd_off();
			}	
		}
		_kodari_ssd = _dev_ssd ? 1:0;
		mutex_unlock(&g_usb_switch->mutex);
	}
	
	return count;
}


 
int write_proc_tmft (struct file *file,const char *buf,unsigned long count,void *data)
{
	char procbuf[PROC_BUF_MAX];
	s32 tmft_on = _kodari_tmft;
	int ret;

	if(g_usb_switch) {
		mutex_lock(&g_usb_switch->mutex);
		tmft_on = _kodari_tmft;
		ret = get_proc_str(buf,procbuf,count,sizeof(procbuf));
		printk(KERN_ERR "%s - 1 - tmft_on:%i\n", __func__, tmft_on);
		if (ret<0) {
			mutex_unlock(&g_usb_switch->mutex);
			return ret;
		}
		*(unsigned int*)data = (unsigned int) simple_strtoll(procbuf,NULL,10);

		printk(KERN_ERR "%s - 2 - tmft_on:%i, _kodari_tmft:%i\n", __func__, tmft_on, _kodari_tmft);
		if(tmft_on != _kodari_tmft) {
			if(_kodari_tmft == 1 ) {
				hub_tmft_on();
			} else if (_kodari_tmft == 0) {
				hub_tmft_off();
			}
		}
		_kodari_tmft = _dev_tmft ? 1:0;
		mutex_unlock(&g_usb_switch->mutex);
	}
	
	return count;
}


 
int write_proc_bmodule (struct file *file,const char *buf,unsigned long count,void *data)
{
	char procbuf[PROC_BUF_MAX];
	s32 bmodule_on;
	int ret;
	if(g_usb_switch) {
		mutex_lock(&g_usb_switch->mutex);
		bmodule_on = _kodari_bmodule;
		ret = get_proc_str(buf,procbuf,count,sizeof(procbuf));
	        
		if(bmodule_on) chk_suspend_hub_status=0;
        	else chk_suspend_hub_status=1;

		printk(KERN_ERR "%s - 1 - bmodule_on in suspend:%i\n", __func__, bmodule_on);
		if (ret<0) {
			mutex_unlock(&g_usb_switch->mutex);
			return ret;
		}
		*(unsigned int*)data = (unsigned int) simple_strtoll(procbuf,NULL,10);
		printk(KERN_ERR "%s - 2 - bmodule_on:%i, _kodari_bmodule:%i\n", __func__, bmodule_on, _kodari_bmodule);

		if(bmodule_on != _kodari_bmodule) {
			if(_kodari_bmodule == 1 ) {
				hub_bmodule_on();
			} else if (_kodari_bmodule == 0) {
				hub_bmodule_off();
			}
		}
		_kodari_bmodule = _dev_bmodule ? 1:0;
		mutex_unlock(&g_usb_switch->mutex);
	}
	
	return count;
}




 int write_proc_keyboard (struct file *file,const char *buf,unsigned long count,void *data)
{
	char procbuf[PROC_BUF_MAX];
	s32 keyboard_on;
	int ret;
	if(g_usb_switch) {
		mutex_lock(&g_usb_switch->mutex);
		keyboard_on = _kodari_keyboard;
		ret = get_proc_str(buf,procbuf,count,sizeof(procbuf));
		printk(KERN_ERR "%s - 1 - keyboard_on:%i\n", __func__, keyboard_on);
		if (ret<0) {
			mutex_unlock(&g_usb_switch->mutex);
			return ret;
		}
		*(unsigned int*)data = (unsigned int) simple_strtoll(procbuf,NULL,10);
		printk(KERN_ERR "%s - 2 - keyboard_on:%i, _kodari_keyboard:%i\n", __func__, keyboard_on, _kodari_keyboard);

		if(keyboard_on != _kodari_keyboard) {
			if(_kodari_keyboard == 1 ) {
				hub_keyboard_on();
			} else if (_kodari_keyboard == 0) {
				hub_keyboard_off();
			}
		}
		_kodari_keyboard = _dev_keyboard ? 1:0;
		mutex_unlock(&g_usb_switch->mutex);
	}
	
	return count;
}


 int write_proc_otg (struct file *file,const char *buf,unsigned long count,void *data)
{
	char procbuf[PROC_BUF_MAX];
	s32 otg_on;
	int ret;
	if(g_usb_switch) {
		mutex_lock(&g_usb_switch->mutex);
		otg_on = _kodari_otg;
		ret = get_proc_str(buf,procbuf,count,sizeof(procbuf));
		printk(KERN_ERR "%s - 1 - otg_on:%i\n", __func__, otg_on);
		if (ret<0) {
			mutex_unlock(&g_usb_switch->mutex);
			return ret;
		}
		*(unsigned int*)data = (unsigned int) simple_strtoll(procbuf,NULL,10);
		printk(KERN_ERR "%s - 2 - otg_on:%i, _kodari_otg:%i\n", __func__, otg_on, _kodari_otg);

		if(otg_on != _kodari_otg) {
			if(_kodari_otg == 1 ) {
				hub_otg_on();
			} else if (_kodari_otg == 0) {
				hub_otg_off();
			}
		}
		_kodari_otg = _dev_otg ? 1:0;
		mutex_unlock(&g_usb_switch->mutex);
	}
	
	return count;
}




struct proc_dir_entry *create_rw_proc_entry(const char *name, mode_t mode,
										struct proc_dir_entry *parent,
										read_proc_t *read_proc,
										write_proc_t *write_proc,
										void *data)
{
	struct proc_dir_entry *entry = create_proc_entry(name,mode,parent);
	if (entry) {
		entry->read_proc = read_proc;
		entry->write_proc = write_proc;
		entry->data = data;
	}
	return entry;
}





static void remove_kodari_proc_entry(void)
{
	remove_proc_entry("driver/" KODARI_HUB "/ssd",NULL);
	remove_proc_entry("driver/" KODARI_HUB "/tmft",NULL);
	remove_proc_entry("driver/" KODARI_HUB "/bmodule",NULL);
	remove_proc_entry("driver/" KODARI_HUB "/keyboard",NULL);
	remove_proc_entry("driver/" KODARI_HUB "/otg",NULL);
	remove_proc_entry("driver/" KODARI_HUB "/states",NULL);
}

static int create_kodari_proc_entry(void)
{
	struct proc_dir_entry *entry;	
		/* main directory */

	printk(KERN_ERR "1 %s\n", __func__);
	kodari_dev_dir = proc_mkdir("driver/" KODARI_HUB ,NULL);
	if (!kodari_dev_dir) return -EFAULT;

	entry = create_rw_proc_entry("ssd",0666,kodari_dev_dir,
		read_proc_s32, write_proc_ssd, (void*)&_kodari_ssd);
	if (!entry) { remove_kodari_proc_entry(); return -ENOMEM; }


	entry = create_rw_proc_entry("tmft",0666,kodari_dev_dir,
		read_proc_s32, write_proc_tmft, (void*)&_kodari_tmft);
	if (!entry) { remove_kodari_proc_entry(); return -ENOMEM; }

	entry = create_rw_proc_entry("bmodule",0666,kodari_dev_dir,
		read_proc_s32, write_proc_bmodule, (void*)&_kodari_bmodule);
	if (!entry) { remove_kodari_proc_entry(); return -ENOMEM; }


	entry = create_rw_proc_entry("keyboard",0666,kodari_dev_dir,
		read_proc_s32, write_proc_keyboard, (void*)&_kodari_keyboard);
	if (!entry) { remove_kodari_proc_entry(); return -ENOMEM; }

	
	entry = create_rw_proc_entry("otg",0666,kodari_dev_dir,
		read_proc_s32, write_proc_otg, (void*)&_kodari_otg);
	if (!entry) { remove_kodari_proc_entry(); return -ENOMEM; }


	entry = create_rw_proc_entry("states",0666,kodari_dev_dir,
		read_proc_states, NULL, (void*)&_kodari_states);
	if (!entry) { remove_kodari_proc_entry(); return -ENOMEM; }
	


	printk(KERN_ERR "2 %s\n", __func__);
	return 0;
}



static struct file* file_open(const char* path, int flags, int rights) {
    struct file* filp = NULL;
    mm_segment_t oldfs;
    int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, flags, rights);
    set_fs(oldfs);
    if(IS_ERR(filp)) {
        err = PTR_ERR(filp);
        return NULL;
    }
    return filp;
}

static void file_close(struct file* file) {
    filp_close(file, NULL);
}

int file_read_hub(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_read(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
}   


int file_write_hub(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_write(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
}


#if defined(CONFIG_BATTERY_SAMSUNG)
void exynos_usb_cable_connect(void)
{
	samsung_cable_check_status(1);
}

void exynos_usb_cable_disconnect(void)
{
	samsung_cable_check_status(0);
}
#endif

static void exynos_host_port_power_off(void)
{
	s5p_ehci_port_power_off(&s5p_device_ehci);
	s5p_ohci_port_power_off(&s5p_device_ohci);
}

static void __maybe_unused exynos_host_port_power_on(void)
{
	s5p_ehci_port_power_on(&s5p_device_ehci);
	s5p_ohci_port_power_on(&s5p_device_ohci);
}

static int is_host_detect(struct exynos_usb_switch *usb_switch)
{
	//return !gpio_get_value(usb_switch->gpio_host_detect);

	//printk(KERN_ERR "%s  :%i\n", __func__, gpio_get_value(usb_switch->gpio_host_detect));
	return gpio_get_value(usb_switch->gpio_host_detect);
}

static int is_device_detect(struct exynos_usb_switch *usb_switch)
{
//	printk(KERN_ERR "%s :%i\n", __func__, gpio_get_value(usb_switch->gpio_device_detect));

	return gpio_get_value(usb_switch->gpio_device_detect);
}

static void set_host_vbus(struct exynos_usb_switch *usb_switch, int value)
{
	gpio_set_value(usb_switch->gpio_host_vbus, value);
}

#ifdef CONFIG_KODARI_USB_HUB_SEPARATE
static int is_host_detect2(struct exynos_usb_switch *usb_switch)
{
	return gpio_get_value(usb_switch->gpio_host_detect2);
}

static void set_host_vbus2(struct exynos_usb_switch *usb_switch, int value)
{
	gpio_set_value(usb_switch->gpio_host_vbus2, value);
}
#endif

static int exynos_change_usb_mode(struct exynos_usb_switch *usb_switch,
				enum usb_cable_status mode)
{
	struct s3c_udc *udc = the_controller;
	enum usb_cable_status cur_mode = atomic_read(&usb_switch->connect);
	int ret = 0;
	printk(KERN_ERR "%s - cur_mode:%i\n", __func__, cur_mode);

	if (cur_mode) {
		if((cur_mode & mode) == mode)
		{
			printk(KERN_DEBUG "Skip requested mode (%d), current mode=%d\n",
				mode, cur_mode);
			return -EPERM;	
		}


		/*
		if (mode == USB_DEVICE_ATTACHED || mode == USB_HOST_ATTACHED) {
			printk(KERN_DEBUG "Skip requested mode (%d), current mode=%d\n",
				mode, cur_mode);
			return -EPERM;
		}
		*/
	} else {
		
	 /*
		if (mode == USB_DEVICE_DETACHED || mode == USB_HOST_DETACHED) {
			printk(KERN_DEBUG "Skip requested mode (%d), current mode=%d\n",
				mode, cur_mode);
			return -EPERM;
		}
		*/
	}

	switch (mode) {
	case USB_DEVICE_DETACHED:
		/*if (cur_mode == USB_HOST_ATTACHED) {
			printk(KERN_ERR "Abnormal request mode (%d), current mode=%d\n",
				mode, cur_mode);
			return -EPERM;
		}*/

		udc->gadget.ops->vbus_session(&udc->gadget, 0);
		atomic_set(&usb_switch->connect, cur_mode & ~USB_DEVICE_ATTACHED);
		atomic_set(&usb_switch->connect, atomic_read(&usb_switch->connect) | USB_DEVICE_DETACHED);
		break;
	case USB_DEVICE_ATTACHED:
		udc->gadget.ops->vbus_session(&udc->gadget, 1);
		atomic_set(&usb_switch->connect, cur_mode | USB_DEVICE_ATTACHED);
		atomic_set(&usb_switch->connect, atomic_read(&usb_switch->connect) & ~USB_DEVICE_DETACHED);
		break;
	case USB_HOST_DETACHED:
		/*if (cur_mode == USB_DEVICE_ATTACHED) {
			printk(KERN_ERR "Abnormal request mode (%d), current mode=%d\n",
				mode, cur_mode);
			return -EPERM;
		}*/

		{
			struct file * usb_unbind = NULL;
			int amt;
			usb_unbind = file_open(HUB_UNBIND_DRIVER_PATH,  O_WRONLY, 0);
			if(usb_unbind != NULL) {
				while(1) {		
					amt = file_write_hub(usb_unbind, 0, HUB1_DEV_ID, sizeof(HUB1_DEV_ID)); 
		                	if(amt < 0) { 
		                        if(amt == EINTR) continue; 
		                        printk(KERN_ERR "fail to write to sys fs %s - %i", HUB1_DEV_ID, amt);      
						break;
		                  }else break; 
				}
				file_close(usb_unbind);
			}

			
		}
		

		pm_runtime_put(&s5p_device_ohci.dev);
		pm_runtime_put(&s5p_device_ehci.dev);
		if (usb_switch->gpio_host_vbus)
			set_host_vbus(usb_switch, 1);
#ifdef CONFIG_KODARI_USB_HUB_SEPARATE
			set_host_vbus2(usb_switch, 1);
#endif
	//		set_host_vbus(usb_switch, 0);
//leehc
//		enable_irq(usb_switch->device_detect_irq);
#if defined(CONFIG_BATTERY_SAMSUNG)
		exynos_usb_cable_disconnect();
#endif
		atomic_set(&usb_switch->connect, cur_mode | USB_HOST_DETACHED);
		atomic_set(&usb_switch->connect, atomic_read(&usb_switch->connect) & ~USB_HOST_ATTACHED);

		break;
	case USB_HOST_ATTACHED:
#if defined(CONFIG_BATTERY_SAMSUNG)
		exynos_usb_cable_connect();
#endif
//leehc
//		disable_irq(usb_switch->device_detect_irq);
		if (usb_switch->gpio_host_vbus) {
			set_host_vbus(usb_switch, 0);
#ifdef CONFIG_KODARI_USB_HUB_SEPARATE
			set_host_vbus2(usb_switch, 0);
#endif
//			set_host_vbus(usb_switch, 1);		
		}

		{
			struct file * usb_bind = NULL;
			int amt;
			usb_bind = file_open(HUB_BIND_DRIVER_PATH,  O_WRONLY, 0);
			if(usb_bind != NULL) {
				while(1) {		
					amt = file_write_hub(usb_bind, 0, HUB1_DEV_ID, sizeof(HUB1_DEV_ID)); 
		                	if(amt < 0) { 
		                        if(amt == EINTR) continue; 
		                        printk(KERN_ERR "fail to write to sys fs %s - %i", HUB1_DEV_ID, amt);       
						break;
		                  }else break; 
				}
				file_close(usb_bind);
			}
		}

			
		pm_runtime_get_sync(&s5p_device_ehci.dev);
		pm_runtime_get_sync(&s5p_device_ohci.dev);
	#if 0	
		{
			struct file * usb_bind = NULL;
			int amt;
			usb_bind = file_open(HUB_BIND_DRIVER_PATH,  O_WRONLY, 0);
			if(usb_bind != NULL) {
				while(1) {		
					amt = file_write_hub(usb_bind, 0, HUB1_DEV_ID, sizeof(HUB1_DEV_ID)); 
		                	if(amt < 0) { 
		                        if(amt == EINTR) continue; 
		                        printk(KERN_ERR "fail to write to sys fs %s - %i", HUB1_DEV_ID, amt);       
						break;
		                  }else break; 
				}
				file_close(usb_bind);
			}
		}
	#endif 
		atomic_set(&usb_switch->connect, cur_mode | USB_HOST_ATTACHED);
		atomic_set(&usb_switch->connect, atomic_read(&usb_switch->connect) & ~USB_HOST_DETACHED);
		break;
	default:
		printk(KERN_ERR "Does not changed\n");
	}
	printk(KERN_INFO "usb cable = %d, cur_mode:%i\n", mode, atomic_read(&usb_switch->connect));

	return ret;
}

static void exnos_usb_switch_worker(struct work_struct *work)
{
	struct exynos_usb_switch *usb_switch =
		container_of(work, struct exynos_usb_switch, switch_work);
	int cnt = 0;

	mutex_lock(&usb_switch->mutex);
	/* If already device detached or host_detected, */
	if (!is_device_detect(usb_switch) || is_host_detect(usb_switch))
		goto done;

	while (!pm_runtime_suspended(&s5p_device_ehci.dev) ||
		!pm_runtime_suspended(&s5p_device_ohci.dev)) {

		mutex_unlock(&usb_switch->mutex);
		msleep(SWITCH_WAIT_TIME);
		mutex_lock(&usb_switch->mutex);

		/* If already device detached or host_detected, */
		if (!is_device_detect(usb_switch) || is_host_detect(usb_switch))
			goto done;

		if (cnt++ > WAIT_TIMES) {
			printk(KERN_ERR "%s:device not attached by host\n",
				__func__);
			goto done;
		}

	}

	if (cnt > 1)
		printk(KERN_INFO "Device wait host power during %d\n", (cnt-1));

	/* Check Device, VBUS PIN high active */
	exynos_change_usb_mode(usb_switch, USB_DEVICE_ATTACHED);
done:
	mutex_unlock(&usb_switch->mutex);
}

//static irqreturn_t exynos_host_detect_thread(int irq, void *data)
#if 0
static void exynos_host_detect_thread(struct work_struct *work)
{
	//struct exynos_usb_switch *usb_switch = data;
//	struct exynos_usb_switch *usb_switch =
//		container_of(work, struct exynos_usb_switch, switch_work);


	
	struct exynos_usb_switch *usb_switch =
			container_of(to_delayed_work(work), struct exynos_usb_switch, work_host);
	unsigned int host_state_new =0;

//	printk(KERN_ERR "%s\n", __func__);

	mutex_lock(&usb_switch->mutex);


	host_state_new = is_host_detect(usb_switch);
	if(usb_switch->host_state != host_state_new) {
		usb_switch->host_state = host_state_new;
		if (usb_switch->host_state)
			exynos_change_usb_mode(usb_switch, USB_HOST_ATTACHED);
		else
			exynos_change_usb_mode(usb_switch, USB_HOST_DETACHED);
	}

	mutex_unlock(&usb_switch->mutex);
	
	schedule_delayed_work(&usb_switch->work_host, msecs_to_jiffies(1200));

	//return IRQ_HANDLED;
}
#endif


static void hub_on(int bEnable) 
{
#ifdef CONFIG_KODARI_USB_HUB_SEPARATE
	struct file * hub_power_fd = NULL;
	int amt = 0;
	struct file * hub_power_fd2 = NULL;
	int amt2 = 0;

        if(bEnable) chk_suspend_hub_status=0;
        else chk_suspend_hub_status=1;

//	printk(KERN_ERR "%s\n", __func__);
	hub_power_fd = file_open(HUB1_POWER_PATH,  O_WRONLY, 0);

	if(hub_power_fd) {
		while(1) {
			if(bEnable) amt = file_write_hub(hub_power_fd, 0, "1", 1);
			else amt = file_write_hub(hub_power_fd, 0, "0", 1);
			if(amt < 0) {
				if(amt == EINTR) continue;
				printk(KERN_ERR "fail to write to sys fs HUB power - %i", amt);
				break;
			} else break;			
		}
		file_close(hub_power_fd);
	}

	hub_power_fd2 = file_open(HUB2_POWER_PATH,  O_WRONLY, 0);

	if(hub_power_fd2) {
		while(1) {
			if(bEnable) amt2 = file_write_hub(hub_power_fd2, 0, "1", 1);
			else amt2 = file_write_hub(hub_power_fd2, 0, "0", 1);
			if(amt2 < 0) {
				if(amt2 == EINTR) continue;
				printk(KERN_ERR "fail to write to sys fs HUB power - %i", amt2);
				break;
			} else break;			
		}
		file_close(hub_power_fd2);
	}

	if(bEnable && ((amt >= 0) || (amt2 >= 0)))
		mdelay(120);
#else
	struct file * hub_power_fd = NULL;
	int amt;

//	printk(KERN_ERR "%s\n", __func__);
	hub_power_fd = file_open(HUB_POWER_PATH,  O_WRONLY, 0);

	if(hub_power_fd) {
		while(1) {
			if(bEnable) amt = file_write_hub(hub_power_fd, 0, "1", 1);
			else amt = file_write_hub(hub_power_fd, 0, "0", 1);
			if(amt < 0) {
				if(amt == EINTR) continue;
				printk(KERN_ERR "fail to write to sys fs HUB power - %i", amt);
				break;
			} else break;			
		}
		file_close(hub_power_fd);
	}
#endif
}

static void exynos_host_enable(int bEnable)
{	
	if(g_usb_switch != NULL) {	
	//	mutex_lock(&g_usb_switch->mutex);

		if(bEnable) hub_on(1);
		else hub_on(0);
		if (is_host_detect(g_usb_switch))
			exynos_change_usb_mode(g_usb_switch, USB_HOST_ATTACHED);
		else
			exynos_change_usb_mode(g_usb_switch, USB_HOST_DETACHED);

	//	mutex_unlock(&g_usb_switch->mutex);
	} else {
		printk(KERN_ERR "%s - usb switch driver does not initiated\n", __func__);
	}
}


//static irqreturn_t exynos_device_detect_thread(int irq, void *data)
#if 0
static void exynos_device_detect_thread(struct work_struct *work)

{
	//struct exynos_usb_switch *usb_switch = data;

//	struct exynos_usb_switch *usb_switch =
//		container_of(work, struct exynos_usb_switch, switch_work);

	struct exynos_usb_switch *usb_switch =
		container_of(to_delayed_work(work), struct exynos_usb_switch, work_device);
	unsigned int device_state_new =0;


//	printk(KERN_ERR "%s\n", __func__);

	mutex_lock(&usb_switch->mutex);



	device_state_new = is_device_detect(usb_switch);
	if(usb_switch->device_state != device_state_new) {
		usb_switch->device_state	= device_state_new;
		// (usb_switch->host_state) {
			//	printk(KERN_ERR "Not expected situation\n");
//} else if (is_device_detect(usb_switch)) {
		if (usb_switch->device_state) {
			if (usb_switch->gpio_host_vbus)
				exynos_change_usb_mode(usb_switch, USB_DEVICE_ATTACHED);
			else
				queue_work(usb_switch->workqueue, &usb_switch->switch_work);
		} else {
			/* VBUS PIN low */
			exynos_change_usb_mode(usb_switch, USB_DEVICE_DETACHED);
		}
	}


	
	mutex_unlock(&usb_switch->mutex);
	
	schedule_delayed_work(&usb_switch->work_device, msecs_to_jiffies(1200));

	//return IRQ_HANDLED;
}
#endif

static void exynos_device_enable(void)
{
	if(is_device_detect(g_usb_switch)) {
		if (g_usb_switch->gpio_host_vbus)
			exynos_change_usb_mode(g_usb_switch, USB_DEVICE_ATTACHED);
		else
			queue_work(g_usb_switch->workqueue, &g_usb_switch->switch_work);
	} else {
		/* VBUS PIN low */
		exynos_change_usb_mode(g_usb_switch, USB_DEVICE_DETACHED);
	}

}


static int exynos_usb_status_init(struct exynos_usb_switch *usb_switch)
{
	if (atomic_read(&usb_switch->connect))
		printk(KERN_ERR "Already setting\n");
	else if (is_host_detect(usb_switch))
		exynos_change_usb_mode(usb_switch, USB_HOST_ATTACHED);
	else if (is_device_detect(usb_switch)) {
		if (usb_switch->gpio_host_vbus)
			exynos_change_usb_mode(usb_switch, USB_DEVICE_ATTACHED);
		else
			queue_work(usb_switch->workqueue, &usb_switch->switch_work);
	} else
		atomic_set(&usb_switch->connect, 0);

	return 0;
}

#ifdef CONFIG_PM
static int exynos_usbswitch_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_usb_switch *usb_switch = platform_get_drvdata(pdev);

	//printk(KERN_INFO "%s\n", __func__);
	atomic_set(&usb_switch->connect, 0);

	return 0;
}

static int exynos_usbswitch_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_usb_switch *usb_switch = platform_get_drvdata(pdev);

	//printk(KERN_INFO "%s\n", __func__);
	exynos_usb_status_init(usb_switch);

	if (!atomic_read(&usb_switch->connect) && !usb_switch->gpio_host_vbus)
		exynos_host_port_power_off();

	return 0;
}
#else
#define exynos_usbswitch_suspend	NULL
#define exynos_usbswitch_resume		NULL
#endif

static int __devinit exynos_usbswitch_probe(struct platform_device *pdev)
{
	struct s5p_usbswitch_platdata *pdata = dev_get_platdata(&pdev->dev);
//	struct device *dev = &pdev->dev;
	struct exynos_usb_switch *usb_switch;
//	int irq;
	int ret = 0;

		//printk(KERN_ERR "1 %s\n", __func__);

	usb_switch = kzalloc(sizeof(struct exynos_usb_switch), GFP_KERNEL);
	if (!usb_switch) {
		ret = -ENOMEM;
		goto fail;
	}

	mutex_init(&usb_switch->mutex);
	usb_switch->workqueue = create_singlethread_workqueue("usb_switch");
	INIT_WORK(&usb_switch->switch_work, exnos_usb_switch_worker);


	//INIT_DELAYED_WORK_DEFERRABLE(&usb_switch->work_device, exynos_device_detect_thread);	
	//INIT_DELAYED_WORK_DEFERRABLE(&usb_switch->work_host, exynos_host_detect_thread);

	

	usb_switch->gpio_host_detect = pdata->gpio_host_detect;
	usb_switch->gpio_device_detect = pdata->gpio_device_detect;
	usb_switch->gpio_host_vbus = pdata->gpio_host_vbus;
#ifdef CONFIG_KODARI_USB_HUB_SEPARATE
	usb_switch->gpio_host_detect2 = pdata->gpio_host_detect2;
	usb_switch->gpio_host_vbus2 = pdata->gpio_host_vbus2;
#endif
	usb_switch->device_state = 0;
	usb_switch->host_state = 0;




#if 0
	/* USB Device detect IRQ */
	irq = platform_get_irq(pdev, 1);
	if (!irq) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		ret = -ENODEV;
		goto fail;
	}

	dev_err(dev, "request irq for device detect(%d)\n", irq);

	ret = request_threaded_irq(irq, NULL, exynos_device_detect_thread,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"DEVICE_DETECT", usb_switch);
	if (ret) {
		dev_err(dev, "Failed to allocate an DEVICE interrupt(%d)\n",
			irq);
		goto fail;
	}
	usb_switch->device_detect_irq = irq;

	/* USB Host detect IRQ */
	irq = platform_get_irq(pdev, 0);
	if (!irq) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		ret = -ENODEV;
		goto fail;
	}
	
	dev_err(dev, "request irq for device detect(%d)\n", irq);

	ret = request_threaded_irq(irq, NULL, exynos_host_detect_thread,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"HOST_DETECT", usb_switch);
	if (ret) {
		dev_err(dev, "Failed to allocate an HOST interrupt(%d)\n", irq);
		goto fail;
	}
	usb_switch->host_detect_irq = irq;
#endif


	exynos_usb_status_init(usb_switch);

	platform_set_drvdata(pdev, usb_switch);

	//schedule_delayed_work(&usb_switch->work_device, msecs_to_jiffies(1200));
	//schedule_delayed_work(&usb_switch->work_host, msecs_to_jiffies(1200));


	g_usb_switch = usb_switch;

	return ret;
fail:
	cancel_work_sync(&usb_switch->switch_work);
	destroy_workqueue(usb_switch->workqueue);
	mutex_destroy(&usb_switch->mutex);
	return ret;
}

static int __devexit exynos_usbswitch_remove(struct platform_device *pdev)
{
	struct exynos_usb_switch *usb_switch = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	free_irq(usb_switch->device_detect_irq, dev);
	free_irq(usb_switch->device_detect_irq, dev);
	platform_set_drvdata(pdev, 0);

	cancel_work_sync(&usb_switch->switch_work);
	destroy_workqueue(usb_switch->workqueue);
	mutex_destroy(&usb_switch->mutex);
	kfree(usb_switch);

	return 0;
}

static int is_device_detected() 
{
	return _dev_ssd | _dev_tmft | _dev_bmodule | _dev_keyboard;
}

static int bmodule_power_on()
{
	struct file *bmodule_power_fd = NULL;
	int amt = 0;

	bmodule_power_fd = file_open(BMODULE_POWER_PATH, O_WRONLY, 0);

	if(bmodule_power_fd != NULL  ) {
		amt = file_write_hub(bmodule_power_fd, 0, "1", 1);
		file_close(bmodule_power_fd);
	}

	return amt;
}

static int bmodule_power_off()
{
	struct file *bmodule_power_fd = NULL;
	int amt = 0;

	bmodule_power_fd = file_open(BMODULE_POWER_PATH, O_WRONLY, 0);

	if(bmodule_power_fd != NULL  ) {
		amt = file_write_hub(bmodule_power_fd, 0, "0", 1);
		file_close(bmodule_power_fd);
	}

	return amt;
}

static void hub_ssd_on()
{	
	struct file * tusb_power_fd = NULL;
	
	int amt;

	if(g_usb_switch == NULL) {
		printk(KERN_ERR "NO usb_switch obj. check usb switch \n");
		return;
	}
	printk(KERN_ERR "before lock \n");
	//mutex_lock(&g_usb_switch->mutex);
	printk(KERN_ERR "enter lock \n");

	if(!is_host_detect(g_usb_switch)) {
		printk(KERN_DEBUG "host does not enabled\n");
		exynos_host_enable(1);		
	}
	tusb_power_fd = file_open(TUSB_POWER_PATH,  O_WRONLY, 0);

	if( tusb_power_fd != NULL  ) {	
		while(1) {								
			amt = file_write_hub(tusb_power_fd, 0, "1", 1); 		
			if(amt < 0) {
				if(amt == EINTR) continue;
				printk(KERN_ERR "fail to write to sys fs tusb power off - %i", amt);
				break;
			} else {
				mdelay(50);
				gpio_direction_output(GPIO_SATA_nRST_H, 1);
				mdelay(10);
				gpio_direction_output(GPIO_SATA_nRST_H, 0);

				_dev_ssd = USB_HOST_SSD;
				break;
			}
		}			
		file_close(tusb_power_fd);

	} else {
		printk(KERN_ERR "failt to open ssd fs file\n");
		if(!is_device_detected() && is_host_detect(g_usb_switch)) {
			printk(KERN_DEBUG "host enabled but no device detected\n");
			exynos_host_enable(0);		
		}
	}
	//mutex_unlock(&g_usb_switch->mutex);
	
	return;

}


static void hub_otg_on()
{
	struct file * otg_power_fd = NULL;
	
	int amt;

	if(g_usb_switch == NULL) {
		printk(KERN_ERR "NO usb_switch obj. check usb switch \n");
		return;
	}
//	mutex_lock(&g_usb_switch->mutex);
	if(!is_device_detect(g_usb_switch)) {	
		otg_power_fd = file_open(OTG_POWER_PATH,  O_WRONLY, 0);

		if(otg_power_fd != NULL ) {	
			while(1) {										
				amt = file_write_hub(otg_power_fd, 0, "1", 1); 		
				if(amt < 0) {
					if(amt == EINTR) continue;
					printk(KERN_ERR "fail to write to sys otg - %i", amt);
					break;
				} else {
					_dev_otg = USB_DEVICE_OTG;
					exynos_device_enable();
					break;
				}
			}		
			file_close(otg_power_fd);
		} else {
			printk(KERN_ERR "failt to open otg fs file\n");		
		}
	}else {
		_dev_otg = 0;
	}
//	mutex_unlock(&g_usb_switch->mutex);
	
	return;

}




static void hub_tmft_on(void)
{
	if(g_usb_switch == NULL) {
		printk(KERN_ERR "NO usb_switch obj. check usb switch \n");
		return;
	}
	//mutex_lock(&g_usb_switch->mutex);
	
	if(!is_host_detect(g_usb_switch)) {
		printk(KERN_DEBUG "host does not enabled\n");
		exynos_host_enable(1);		
		_dev_tmft = USB_HOST_TMFT;
	} else _dev_tmft = USB_HOST_TMFT; 
	
	//mutex_unlock(&g_usb_switch->mutex);	
	return;
}

static void hub_bmodule_on(void)
{
	if(g_usb_switch == NULL) {
		printk(KERN_ERR "NO usb_switch obj. check usb switch \n");
		return;
	}

	bmodule_power_on();
	
//	mutex_lock(&g_usb_switch->mutex);
	if(!is_host_detect(g_usb_switch)) {
		printk(KERN_DEBUG "host does not enabled\n");
		exynos_host_enable(1);		
		_dev_bmodule = USB_HOST_BMODULE;
	} else _dev_bmodule = USB_HOST_BMODULE; 
	//mutex_unlock(&g_usb_switch->mutex);	
	return;
}

static void hub_keyboard_on(void)
{
	if(g_usb_switch == NULL) {
		printk(KERN_ERR "NO usb_switch obj. check usb switch \n");
		return;
	}
//	mutex_lock(&g_usb_switch->mutex);
	if(!is_host_detect(g_usb_switch)) {
		printk(KERN_DEBUG "host does not enabled\n");
		exynos_host_enable(1);		
		_dev_keyboard = USB_HOST_KEYBOARD;
	} else _dev_keyboard = USB_HOST_KEYBOARD; 
//	mutex_unlock(&g_usb_switch->mutex);	
	return;
}



static void hub_ssd_off()
{

	struct file * tusb_power_fd = NULL;
	int amt;


	if(g_usb_switch == NULL) {
		printk(KERN_ERR "NO usb_switch obj. check usb switch \n");
		return;
	}

//	mutex_lock(&g_usb_switch->mutex);
	tusb_power_fd = file_open(TUSB_POWER_PATH,  O_WRONLY, 0);

	if(tusb_power_fd != NULL ) {	
		while(1) {			
			amt = file_write_hub(tusb_power_fd, 0, "0", 1); 		
			if(amt < 0) {
				if(amt == EINTR) continue;
				printk(KERN_ERR "fail to write to sys fs tusb power on - %i", amt);
				break;
			} else {
				_dev_ssd = 0;			
				break;
			}
		}		
		file_close(tusb_power_fd);
	} else {
		printk(KERN_ERR "failt to open ssd sys fs file\n");
	}
	if(!is_device_detected() && is_host_detect(g_usb_switch)) {
		printk(KERN_DEBUG "host enabled but no device detected\n");
		exynos_host_enable(0);		
	}
//	mutex_unlock(&g_usb_switch->mutex);
}


static void hub_tmft_off()
{
	if(g_usb_switch == NULL) {
		printk(KERN_ERR "NO usb_switch obj. check usb switch \n");
		return;
	}
//	mutex_lock(&g_usb_switch->mutex);	
	_dev_tmft = 0;
	if(!is_device_detected() && is_host_detect(g_usb_switch)) {
		printk(KERN_DEBUG "host enabled but no device detected\n");
		exynos_host_enable(0);		
	}
//	mutex_unlock(&g_usb_switch->mutex);
}
static void hub_bmodule_off()
{
	if(g_usb_switch == NULL) {
		printk(KERN_ERR "NO usb_switch obj. check usb switch \n");
		return;
	}
//	mutex_lock(&g_usb_switch->mutex);	
	_dev_bmodule = 0;
	if(!is_device_detected() && is_host_detect(g_usb_switch)) {
		printk(KERN_DEBUG "host enabled but no device detected\n");
		exynos_host_enable(0);		
	}
//	mutex_unlock(&g_usb_switch->mutex);

	bmodule_power_off();
}

static void hub_keyboard_off()
{
	if(g_usb_switch == NULL) {
		printk(KERN_ERR "NO usb_switch obj. check usb switch \n");
		return;
	}

	//mutex_lock(&g_usb_switch->mutex);	
	_dev_keyboard = 0;
	if(!is_device_detected() && is_host_detect(g_usb_switch)) {
		printk(KERN_DEBUG "host enabled but no device detected\n");
		exynos_host_enable(0);		
	}
	//mutex_unlock(&g_usb_switch->mutex);
}

static void hub_otg_off()
{
	struct file * otg_power_fd = NULL;
	
	int amt;

	if(g_usb_switch == NULL) {
		printk(KERN_ERR "NO usb_switch obj. check usb switch \n");
		return;
	}
//	mutex_lock(&g_usb_switch->mutex);
	if(is_device_detect(g_usb_switch)) {	
		otg_power_fd = file_open(OTG_POWER_PATH,  O_WRONLY, 0);

		if(otg_power_fd != NULL ) {	
			while(1) {										
				amt = file_write_hub(otg_power_fd, 0, "0", 1); 		
				if(amt < 0) {
					if(amt == EINTR) continue;
					printk(KERN_ERR "fail to write to sys otg - %i", amt);
					break;
				} else {
					_dev_otg = 0;
					exynos_device_enable();
					break;
				}
			}		
			file_close(otg_power_fd);
		} else {
			printk(KERN_ERR "failt to open otg fs file\n");		
		}
	} else {
		_dev_otg = 0;
	}
	//mutex_unlock(&g_usb_switch->mutex);
	
	return;

}


static const struct dev_pm_ops exynos_usbswitch_pm_ops = {
	.suspend                = exynos_usbswitch_suspend,
	.resume                 = exynos_usbswitch_resume,
};

static struct platform_driver exynos_usbswitch_driver = {
	.probe		= exynos_usbswitch_probe,
	.remove		= __devexit_p(exynos_usbswitch_remove),
	.driver		= {
		.name	= "exynos-usb-switch",
		.owner	= THIS_MODULE,
#if 0 //def CONFIG_PM
		.pm	= &exynos_usbswitch_pm_ops,
#endif
	},
};

static int __init exynos_usbswitch_init(void)
{
	int ret;

	ret = platform_device_register(&s5p_device_usbswitch);
	if (ret < 0)
		return ret;

	ret = platform_driver_register(&exynos_usbswitch_driver);
	if (!ret) {
		printk(KERN_INFO "%s: " DRIVER_DESC "\n", switch_name);
		create_kodari_proc_entry();
	}
	

	return ret;
}
late_initcall(exynos_usbswitch_init);

static void __exit exynos_usbswitch_exit(void)
{
	platform_driver_unregister(&exynos_usbswitch_driver);
	remove_kodari_proc_entry();
}
module_exit(exynos_usbswitch_exit);

MODULE_DESCRIPTION("Exynos USB switch driver");
MODULE_AUTHOR("<yulgon.kim@samsung.com>");
MODULE_LICENSE("GPL");
