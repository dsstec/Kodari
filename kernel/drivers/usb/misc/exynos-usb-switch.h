/*
 * exynos-usb-switch.h - USB switch driver for Exynos
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 * Yulgon Kim <yulgon.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef __EXYNOS_USB_SWITCH
#define __EXYNOS_USB_SWITCH

#define SWITCH_WAIT_TIME	500
#define WAIT_TIMES		10

enum usb_cable_status {
	USB_DEVICE_DETACHED = 1,
	USB_DEVICE_ATTACHED = 2,
	USB_HOST_DETACHED = 4,
	USB_HOST_ATTACHED = 8,
};

struct exynos_usb_switch {
	atomic_t connect;
	unsigned int host_detect_irq;
	unsigned int device_detect_irq;
	unsigned int gpio_host_detect;
	unsigned int gpio_device_detect;
	unsigned int gpio_host_vbus;
#ifdef CONFIG_KODARI_USB_HUB_SEPARATE
	unsigned int gpio_host_detect2;
	unsigned int gpio_host_vbus2;
#endif
	unsigned int device_state;
	unsigned int host_state;

	struct workqueue_struct	*workqueue;
	struct delayed_work		work_device;
	struct delayed_work		work_host;
	struct work_struct switch_work;
	struct mutex mutex;
	atomic_t usb_status;
	int (*get_usb_mode)(void);
	int (*change_usb_mode)(int mode);
};

extern int s5p_ehci_port_power_off(struct platform_device *pdev);
extern int s5p_ohci_port_power_off(struct platform_device *pdev);

extern int s5p_ehci_port_power_on(struct platform_device *pdev);
extern int s5p_ohci_port_power_on(struct platform_device *pdev);

#endif
