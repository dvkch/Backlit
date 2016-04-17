/*
 * epsonds-io.c - Epson ESC/I-2 driver, low level I/O.
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

#define DEBUG_DECLARE_ONLY

#include "sane/config.h"
#include <ctype.h>
#include <unistd.h>     /* sleep */

#include "epsonds.h"
#include "epsonds-io.h"

size_t eds_send(epsonds_scanner *s, void *buf, size_t length, SANE_Status *status)
{
	DBG(32, "%s: size = %lu\n", __func__, (u_long) length);

	if (length == 2) {

		char *cmd = buf;

		switch (cmd[0]) {
		case FS:
			DBG(9, "%s: FS %c\n", __func__, cmd[1]);
			break;
		}
	}

	if (s->hw->connection == SANE_EPSONDS_NET) {
		/* XXX */
	} else if (s->hw->connection == SANE_EPSONDS_USB) {

		size_t n = length;

		*status = sanei_usb_write_bulk(s->fd, buf, &n);

		return n;
	}

	/* never reached */

	*status = SANE_STATUS_INVAL;

	return 0;
}

size_t eds_recv(epsonds_scanner *s, void *buf, size_t length, SANE_Status *status)
{
	size_t n = 0;

	DBG(30, "%s: size = %ld, buf = %p\n", __func__, (long) length, buf);

	if (s->hw->connection == SANE_EPSONDS_NET) {
		/* XXX */
	} else if (s->hw->connection == SANE_EPSONDS_USB) {

		/* !!! only report an error if we don't read anything */

		n = length;
		*status = sanei_usb_read_bulk(s->fd, (SANE_Byte *)buf,
					    (size_t *) &n);
		if (n > 0)
			*status = SANE_STATUS_GOOD;
	}

	if (n < length) {
		DBG(1, "%s: expected = %lu, got = %ld, canceling: %d\n", __func__,
		    (u_long)length, (long)n, s->canceling);

		*status = SANE_STATUS_IO_ERROR;
	}

	return n;
}

/* Simple function to exchange a fixed amount of data with the scanner */

SANE_Status eds_txrx(epsonds_scanner* s, char *txbuf, size_t txlen,
	    char *rxbuf, size_t rxlen)
{
	SANE_Status status;
	size_t done;

	done = eds_send(s, txbuf, txlen, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: tx err, %s\n", __func__, sane_strstatus(status));
		return status;
	}

	if (done != txlen) {
		DBG(1, "%s: tx err, short write\n", __func__);
		return SANE_STATUS_IO_ERROR;
	}

	done = eds_recv(s, rxbuf, rxlen, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: rx err, %s\n", __func__, sane_strstatus(status));
	}

	return status;
}

/* This function should be used to send codes that only requires the scanner
 * to give back an ACK or a NAK, namely FS X or FS Y
 */

SANE_Status eds_control(epsonds_scanner *s, void *buf, size_t buf_size)
{
	char result;
	SANE_Status status;

	DBG(12, "%s: size = %lu\n", __func__, (u_long) buf_size);

	status = eds_txrx(s, buf, buf_size, &result, 1);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: failed, %s\n", __func__, sane_strstatus(status));
		return status;
	}

	if (result == ACK)
		return SANE_STATUS_GOOD;

	if (result == NAK) {
		DBG(3, "%s: NAK\n", __func__);
		return SANE_STATUS_INVAL;
	}

	DBG(1, "%s: result is neither ACK nor NAK but 0x%02x\n",
		__func__, result);

	return SANE_STATUS_INVAL;
}

SANE_Status eds_fsy(epsonds_scanner *s)
{
	return eds_control(s, "\x1CY", 2);
}

SANE_Status eds_fsx(epsonds_scanner *s)
{
	SANE_Status status = eds_control(s, "\x1CX", 2);
	if (status == SANE_STATUS_GOOD) {
		s->locked = 1;
	}

	return status;
}

SANE_Status eds_lock(epsonds_scanner *s)
{
	SANE_Status status;

	DBG(5, "%s\n", __func__);

	if (s->hw->connection == SANE_EPSONDS_USB) {
		sanei_usb_set_timeout(USB_SHORT_TIMEOUT);
	}

	status = eds_fsx(s);

	if (s->hw->connection == SANE_EPSONDS_USB) {
		sanei_usb_set_timeout(USB_TIMEOUT);
	}

	return status;
}


