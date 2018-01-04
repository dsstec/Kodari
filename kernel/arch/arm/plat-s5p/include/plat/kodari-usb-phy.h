/*
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __PLAT_S5P_KODARI_USB_PHY_H
#define __PLAT_S5P_KODARI_USB_PHY_H

extern int check_times;
extern int test_count;
extern int check_event_occurs;
extern int spin_inform_to_android;
extern int exynos_inform_to_android_hsic_reset(int value);
extern void exynos_hsic_reset_control(int value);
extern void exynos_ehci_reset_control(struct device *dev);
extern void usb4640_1_2v_on(void);
extern void usb4640_1_2v_off(void);

#endif /* __PLAT_S5P_REGS_USB_PHY_H */
