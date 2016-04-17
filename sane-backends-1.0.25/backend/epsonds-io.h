/*
 * epsonds-io.h - Epson ESC/I-2 driver, low level I/O.
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

#ifndef epsonds_io_h
#define epsonds_io_h

#define USB_TIMEOUT (6 * 1000)
#define USB_SHORT_TIMEOUT (1 * 800)

size_t eds_send(epsonds_scanner *s, void *buf, size_t length, SANE_Status *status);
size_t eds_recv(epsonds_scanner *s, void *buf, size_t length, SANE_Status *status);

SANE_Status eds_txrx(epsonds_scanner *s, char *txbuf, size_t txlen,
	char *rxbuf, size_t rxlen);

SANE_Status eds_control(epsonds_scanner *s, void *buf, size_t buf_size);

SANE_Status eds_fsy(epsonds_scanner *s);
SANE_Status eds_fsx(epsonds_scanner *s);
SANE_Status eds_lock(epsonds_scanner *s);

#endif

