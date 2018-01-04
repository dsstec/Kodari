/*
 * wm1800.h  --  WM1800 Soc Audio driver platform data
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _WM1800_PDATA_H
#define _WM1800_PDATA_H

#define WM1800_DRES_400R 0
#define WM1800_DRES_200R 1
#define WM1800_DRES_600R 2
#define WM1800_DRES_150R 3
#define WM1800_DRES_MAX  3

struct wm1800_data {
	bool capless;  /* Headphone outputs configured in capless mode */

	int dres;  /* Discharge resistance for headphone outputs */
};

#endif
