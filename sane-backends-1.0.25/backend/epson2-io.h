/*
 * Prototypes for epson2 I/O functions
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

#ifndef epson2_io_h
#define epson2_io_h

extern unsigned int r_cmd_count;
extern unsigned int w_cmd_count;


SANE_Status e2_cmd_simple(Epson_Scanner * s, void *buf, size_t buf_size);
int e2_send(Epson_Scanner * s, void *buf, size_t buf_size,
		size_t reply_len, SANE_Status * status);
ssize_t e2_recv(Epson_Scanner * s, void *buf, ssize_t buf_size,
		    SANE_Status * status);

SANE_Status
e2_txrx(Epson_Scanner * s, unsigned char *txbuf, size_t txlen,
	    unsigned char *rxbuf, size_t rxlen);

SANE_Status
e2_recv_info_block(Epson_Scanner * s, unsigned char *scanner_status,
		       size_t info_size, size_t * payload_size);

SANE_Status
e2_cmd_info_block(SANE_Handle handle, unsigned char *params,
		      unsigned char params_len, size_t reply_len,
		      unsigned char **buf, size_t * buf_len);

SANE_Status e2_ack(Epson_Scanner * s);
SANE_Status e2_ack_next(Epson_Scanner * s, size_t reply_len);
SANE_Status e2_cancel(Epson_Scanner * s);

SANE_Status
e2_esc_cmd(Epson_Scanner * s, unsigned char cmd, unsigned char val);
#endif /* epson2_io_h */
