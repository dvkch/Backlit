/*
 * epson2_net.c - SANE library for Epson scanners.
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

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "sane/sane.h"
#include "sane/saneopts.h"
#include "sane/sanei_tcp.h"
#include "sane/sanei_config.h"
#include "sane/sanei_backend.h"

#include "epson2.h"
#include "epson2_net.h"

#include "byteorder.h"

#include "sane/sanei_debug.h"

static int
sanei_epson_net_read_raw(Epson_Scanner *s, unsigned char *buf, ssize_t wanted,
		       SANE_Status *status)
{
	int ready, read = -1;
	fd_set readable;
	struct timeval tv;

	tv.tv_sec = 10;
	tv.tv_usec = 0;

	FD_ZERO(&readable);
	FD_SET(s->fd, &readable);

	ready = select(s->fd + 1, &readable, NULL, NULL, &tv);
	if (ready > 0) {
		read = sanei_tcp_read(s->fd, buf, wanted);
	} else {
		DBG(15, "%s: select failed: %d\n", __func__, ready);
	}

	*status = SANE_STATUS_GOOD;

	if (read < wanted) {
		*status = SANE_STATUS_IO_ERROR;
	}

	return read;
}

int
sanei_epson_net_read(Epson_Scanner *s, unsigned char *buf, ssize_t wanted,
		       SANE_Status * status)
{
	ssize_t size;
	ssize_t read = 0;
	unsigned char header[12];

	/* read from buffer, if available */
	if (s->netptr != s->netbuf) {
		DBG(23, "reading %lu from buffer at %p, %lu available\n",
			(u_long) wanted, s->netptr, (u_long) s->netlen);

		memcpy(buf, s->netptr, wanted);
		read = wanted;

		s->netlen -= wanted;

		if (s->netlen == 0) {
			DBG(23, "%s: freeing %p\n", __func__, s->netbuf);
			free(s->netbuf);
			s->netbuf = s->netptr = NULL;
			s->netlen = 0;
		}

		return read;
	}

	/* receive net header */
	size = sanei_epson_net_read_raw(s, header, 12, status);
	if (size != 12) {
		return 0;
	}

	if (header[0] != 'I' || header[1] != 'S') {
		DBG(1, "header mismatch: %02X %02x\n", header[0], header[1]);
		*status = SANE_STATUS_IO_ERROR;
		return 0;
	}

	size = be32atoh(&header[6]);

	DBG(23, "%s: wanted = %lu, available = %lu\n", __FUNCTION__,
		(u_long) wanted, (u_long) size);

	*status = SANE_STATUS_GOOD;

	if (size == wanted) {

		DBG(15, "%s: full read\n", __func__);

		read = sanei_epson_net_read_raw(s, buf, size, status);

		if (s->netbuf) {
			free(s->netbuf);
			s->netbuf = NULL;
			s->netlen = 0;
		}

		if (read < 0) {
			return 0;
		}
		
/*	} else if (wanted < size && s->netlen == size) { */
	} else {
		DBG(23, "%s: partial read\n", __func__);

		read = sanei_epson_net_read_raw(s, s->netbuf, size, status);
		if (read != size) {
			return 0;
		}

		s->netlen = size - wanted;
		s->netptr += wanted;
		read = wanted;

		DBG(23, "0,4 %02x %02x\n", s->netbuf[0], s->netbuf[4]);
		DBG(23, "storing %lu to buffer at %p, next read at %p, %lu bytes left\n",
			(u_long) size, s->netbuf, s->netptr, (u_long) s->netlen);

		memcpy(buf, s->netbuf, wanted);
	}

	return read;
}


int
sanei_epson_net_write(Epson_Scanner *s, unsigned int cmd, const unsigned char *buf,
			size_t buf_size, size_t reply_len, SANE_Status *status)
{
	unsigned char *h1, *h2, *payload;
	unsigned char *packet = malloc(12 + 8 + buf_size);

	/* XXX check allocation failure */

	h1 = packet;
	h2 = packet + 12;
	payload = packet + 12 + 8;

	if (reply_len) {
		s->netbuf = s->netptr = malloc(reply_len);
		s->netlen = reply_len;
		DBG(24, "allocated %lu bytes at %p\n",
			(u_long) reply_len, s->netbuf);
	}

	DBG(24, "%s: cmd = %04x, buf = %p, buf_size = %lu, reply_len = %lu\n",
		__FUNCTION__, cmd, buf, (u_long) buf_size, (u_long) reply_len);

	memset(h1, 0x00, 12);
	memset(h2, 0x00, 8);

	h1[0] = 'I';
	h1[1] = 'S';

	h1[2] = cmd >> 8;
	h1[3] = cmd;

	h1[4] = 0x00;
	h1[5] = 0x0C; /* Don't know what's that */

	DBG(24, "H1[0]: %02x %02x %02x %02x\n", h1[0], h1[1], h1[2], h1[3]);

	if((cmd >> 8) == 0x20) {
		htobe32a(&h1[6], buf_size + 8);		

		htobe32a(&h2[0], buf_size);
		htobe32a(&h2[4], reply_len);

		DBG(24, "H1[6]: %02x %02x %02x %02x (%lu)\n", h1[6], h1[7], h1[8], h1[9], (u_long) (buf_size + 8));
		DBG(24, "H2[0]: %02x %02x %02x %02x (%lu)\n", h2[0], h2[1], h2[2], h2[3], (u_long) buf_size);
		DBG(24, "H2[4]: %02x %02x %02x %02x (%lu)\n", h2[4], h2[5], h2[6], h2[7], (u_long) reply_len);
	}

	if ((cmd >> 8) == 0x20 && (buf_size || reply_len)) {
		if (buf_size)
			memcpy(payload, buf, buf_size);

		sanei_tcp_write(s->fd, packet, 12 + 8 + buf_size);
	}
	else
		sanei_tcp_write(s->fd, packet, 12);

	free(packet);

	*status = SANE_STATUS_GOOD;
	return buf_size;
}

SANE_Status
sanei_epson_net_lock(struct Epson_Scanner *s)
{
	SANE_Status status;
	unsigned char buf[1];

	DBG(1, "%s\n", __func__);

	sanei_epson_net_write(s, 0x2100, NULL, 0, 0, &status);
	sanei_epson_net_read(s, buf, 1, &status);
	return status;
}

SANE_Status
sanei_epson_net_unlock(struct Epson_Scanner *s)
{
	SANE_Status status;

	DBG(1, "%s\n", __func__);

	sanei_epson_net_write(s, 0x2101, NULL, 0, 0, &status);
/*	sanei_epson_net_read(s, buf, 1, &status); */
	return status;
}
