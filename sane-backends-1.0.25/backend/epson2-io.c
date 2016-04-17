/*
 * I/O routines for Epson scanners
 *
 * Based on Kazuhiro Sasayama previous
 * Work on epson.[ch] file from the SANE package.
 * Please see those files for original copyrights.
 *
 * Copyright (C) 2006 Tower Technologies
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

#include "epson2.h"
#include "epson2-io.h"

#include "sane/sanei_scsi.h"
#include "sane/sanei_usb.h"
#include "sane/sanei_pio.h"
#include "sane/sanei_tcp.h"

#include "epson2_scsi.h"
#include "epson_usb.h"
#include "epson2_net.h"

#include "byteorder.h"

/* flaming hack to get USB scanners
 * working without timeouts under linux
 * (cribbed from fujitsu.c)
 */
unsigned int r_cmd_count = 0;
unsigned int w_cmd_count = 0;

int
e2_send(Epson_Scanner * s, void *buf, size_t buf_size, size_t reply_len,
	    SANE_Status * status)
{
	DBG(15, "%s: size = %lu, reply = %lu\n",
	    __func__, (u_long) buf_size, (u_long) reply_len);

	if (buf_size == 2) {
		char *cmd = buf;

		switch (cmd[0]) {
		case ESC:
			DBG(9, "%s: ESC %c\n", __func__, cmd[1]);
			break;

		case FS:
			DBG(9, "%s: FS %c\n", __func__, cmd[1]);
			break;
		}
	}

	if (DBG_LEVEL >= 125) {
		unsigned int k;
		const unsigned char *s = buf;

		for (k = 0; k < buf_size; k++) {
			DBG(125, "buf[%d] %02x %c\n", k, s[k],
			    isprint(s[k]) ? s[k] : '.');
		}
	}

	if (s->hw->connection == SANE_EPSON_NET) {
		if (reply_len == 0) {
			DBG(0,
			    "Cannot send this command to a networked scanner\n");
			return SANE_STATUS_INVAL;
		}
		return sanei_epson_net_write(s, 0x2000, buf, buf_size,
					     reply_len, status);
	} else if (s->hw->connection == SANE_EPSON_SCSI) {
		return sanei_epson2_scsi_write(s->fd, buf, buf_size, status);
	} else if (s->hw->connection == SANE_EPSON_PIO) {
		size_t n;

		if (buf_size == (n = sanei_pio_write(s->fd, buf, buf_size)))
			*status = SANE_STATUS_GOOD;
		else
			*status = SANE_STATUS_INVAL;

		return n;

	} else if (s->hw->connection == SANE_EPSON_USB) {
		size_t n;
		n = buf_size;
		*status = sanei_usb_write_bulk(s->fd, buf, &n);
		w_cmd_count++;
		DBG(20, "%s: cmd count, r = %d, w = %d\n",
		    __func__, r_cmd_count, w_cmd_count);

		return n;
	}

	*status = SANE_STATUS_INVAL;
	return 0;
	/* never reached */
}

ssize_t
e2_recv(Epson_Scanner *s, void *buf, ssize_t buf_size,
	    SANE_Status *status)
{
	ssize_t n = 0;

	DBG(15, "%s: size = %ld, buf = %p\n", __func__, (long) buf_size, buf);

	if (s->hw->connection == SANE_EPSON_NET) {
		n = sanei_epson_net_read(s, buf, buf_size, status);
	} else if (s->hw->connection == SANE_EPSON_SCSI) {
		n = sanei_epson2_scsi_read(s->fd, buf, buf_size, status);
	} else if (s->hw->connection == SANE_EPSON_PIO) {
		if (buf_size ==
		    (n = sanei_pio_read(s->fd, buf, (size_t) buf_size)))
			*status = SANE_STATUS_GOOD;
		else
			*status = SANE_STATUS_INVAL;
	} else if (s->hw->connection == SANE_EPSON_USB) {
		/* !!! only report an error if we don't read anything */
		n = buf_size;	/* buf_size gets overwritten */
		*status =
			sanei_usb_read_bulk(s->fd, (SANE_Byte *) buf,
					    (size_t *) & n);
		r_cmd_count += (n + 63) / 64;	/* add # of packets, rounding up */
		DBG(20, "%s: cmd count, r = %d, w = %d\n",
		    __func__, r_cmd_count, w_cmd_count);

		if (n > 0)
			*status = SANE_STATUS_GOOD;
	}

	if (n < buf_size) {
		DBG(1, "%s: expected = %lu, got = %ld, canceling: %d\n", __func__,
		    (u_long) buf_size, (long) n, s->canceling);

		*status = SANE_STATUS_IO_ERROR;
	}

	/* dump buffer if appropriate */
	if (DBG_LEVEL >= 127 && n > 0) {
		int k;
		const unsigned char *s = buf;

		for (k = 0; k < n; k++)
			DBG(127, "buf[%d] %02x %c\n", k, s[k],
			    isprint(s[k]) ? s[k] : '.');
	}

	return n;
}

/* Simple function to exchange a fixed amount of
 * data with the scanner
 */

SANE_Status
e2_txrx(Epson_Scanner * s, unsigned char *txbuf, size_t txlen,
	    unsigned char *rxbuf, size_t rxlen)
{
	SANE_Status status;

	e2_send(s, txbuf, txlen, rxlen, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: tx err, %s\n", __func__, sane_strstatus(status));
		return status;
	}

	e2_recv(s, rxbuf, rxlen, &status);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: rx err, %s\n", __func__, sane_strstatus(status));
	}

	return status;
}

/* This function should be used to send codes that only requires the scanner
 * to give back an ACK or a NAK.
 */
SANE_Status
e2_cmd_simple(Epson_Scanner * s, void *buf, size_t buf_size)
{
	unsigned char result;
	SANE_Status status;

	DBG(12, "%s: size = %lu\n", __func__, (u_long) buf_size);

	status = e2_txrx(s, buf, buf_size, &result, 1);
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

	DBG(1, "%s: result is neither ACK nor NAK but 0x%02x\n", __func__,
	    result);

	return SANE_STATUS_GOOD;
}

/* receives a 4 or 6 bytes information block from the scanner*/
SANE_Status
e2_recv_info_block(Epson_Scanner * s, unsigned char *scanner_status,
		       size_t info_size, size_t * payload_size)
{
	SANE_Status status;
	unsigned char info[6];

	if (s->hw->connection == SANE_EPSON_PIO)
		e2_recv(s, info, 1, &status);
	else
		e2_recv(s, info, info_size, &status);

	if (status != SANE_STATUS_GOOD)
		return status;

	/* check for explicit NAK */
	if (info[0] == NAK) {
		DBG(1, "%s: command not supported\n", __func__);
		return SANE_STATUS_UNSUPPORTED;
	}

	/* check the first byte: if it's not STX, bail out */
	if (info[0] != STX) {
		DBG(1, "%s: expecting STX, got %02X\n", __func__, info[0]);
		return SANE_STATUS_INVAL;
	}

	/* if connection is PIO read the remaining bytes. */
	if (s->hw->connection == SANE_EPSON_PIO) {
		e2_recv(s, &info[1], info_size - 1, &status);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (scanner_status)
		*scanner_status = info[1];

	if (payload_size) {
		*payload_size = le16atoh(&info[2]);

		if (info_size == 6)
			*payload_size *= le16atoh(&info[4]);

		DBG(14, "%s: payload length: %lu\n", __func__,
			(u_long) *payload_size);
	}

	return SANE_STATUS_GOOD;
}

/* This function can be called for commands that
 * will be answered by the scanner with an info block of 4 bytes
 * and a variable payload. The payload is passed back to the caller
 * in **buf. The caller must free it if != NULL,
 * even if the status != SANE_STATUS_GOOD.
 */

SANE_Status
e2_cmd_info_block(SANE_Handle handle, unsigned char *params,
		      unsigned char params_len, size_t reply_len,
		      unsigned char **buf, size_t * buf_len)
{
	SANE_Status status;
	Epson_Scanner *s = (Epson_Scanner *) handle;
	size_t len;

	DBG(13, "%s, params len = %d, reply len = %lu, buf = %p\n",
	    __func__, params_len, (u_long) reply_len, (void *) buf);

	if (buf == NULL)
		return SANE_STATUS_INVAL;

	/* initialize */
	*buf = NULL;

	/* send command, we expect the info block + reply_len back */
	e2_send(s, params, params_len,
		    reply_len ? reply_len + 4 : 0, &status);

	if (status != SANE_STATUS_GOOD)
		goto end;

	status = e2_recv_info_block(s, NULL, 4, &len);
	if (status != SANE_STATUS_GOOD)
		goto end;

	/* do we need to provide the length of the payload? */
	if (buf_len)
		*buf_len = len;

	/* no payload, stop here */
	if (len == 0)
		goto end;

	/* if a reply_len has been specified and the actual
	 * length differs, throw a warning
	 */
	if (reply_len && (len != reply_len)) {
		DBG(1, "%s: mismatched len - expected %lu, got %lu\n",
		    __func__, (u_long) reply_len, (u_long) len);
	}

	/* allocate and receive the payload */
	*buf = malloc(len);

	if (*buf) {
		memset(*buf, 0x00, len);
		e2_recv(s, *buf, len, &status);	/* receive actual data */
	} else
		status = SANE_STATUS_NO_MEM;
      end:

	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: failed, %s\n", __func__, sane_strstatus(status));

		if (*buf) {
			free(*buf);
			*buf = NULL;
		}
	}

	return status;
}


/* This is used for ESC commands with a single byte parameter. Scanner
 * will answer with ACK/NAK.
 */
SANE_Status
e2_esc_cmd(Epson_Scanner * s, unsigned char cmd, unsigned char val)
{
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s: cmd = 0x%02x, val = %d\n", __func__, cmd, val);
	if (!cmd)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = cmd;

	status = e2_cmd_simple(s, params, 2);
	if (status != SANE_STATUS_GOOD)
		return status;

	params[0] = val;

	return e2_cmd_simple(s, params, 1);
}

/* Send an ACK to the scanner */

SANE_Status
e2_ack(Epson_Scanner * s)
{
	SANE_Status status;
	e2_send(s, S_ACK, 1, 0, &status);
	return status;
}

SANE_Status
e2_ack_next(Epson_Scanner * s, size_t reply_len)
{
	SANE_Status status;
	e2_send(s, S_ACK, 1, reply_len, &status);
	return status;
}

SANE_Status
e2_cancel(Epson_Scanner * s)
{
	DBG(1, "%s\n", __func__);
	return e2_cmd_simple(s, S_CAN, 1);
}
