/*
 * epsonds-usb.h - Epson ESC/I-2 driver, USB device list.
 *
 * Copyright (C) 2015 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#include "sane/sane.h"

#ifndef _EPSONDS_USB_H_
#define _EPSONDS_USB_H_

#define SANE_EPSONDS_VENDOR_ID	(0x4b8)

extern SANE_Word epsonds_usb_product_ids[];
extern int epsonds_get_number_of_ids(void);

#endif
