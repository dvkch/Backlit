/* sane - Scanner Access Now Easy.
   Copyright (C) 2001-2002 Matthew C. Duggan and Simon Krix
   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.

   -----

   This file is part of the canon_pp backend, supporting Canon CanoScan
   Parallel scanners and also distributed as part of the stand-alone driver.  

   canon_pp-io.c: $Revision$

   Low Level Function library for Canon CanoScan Parallel Scanners by
   Simon Krix <kinsei@users.sourceforge.net>
   */

#ifndef NOSANE
#include "../include/sane/config.h"
#endif

#include <sys/time.h>
#include <unistd.h>
#include <ieee1284.h>
#include "canon_pp-io.h"
#include "canon_pp-dev.h"

#ifdef NOSANE

/* No SANE, Things that only apply to stand-alone */
#include <stdio.h>
#include <stdarg.h>

static void DBG(int level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	if (level < 50) vfprintf(stderr, format, args);
	va_end(args);
}
#else

/* Definitions which only apply to SANE compiles */
#ifndef VERSION
#define VERSION "$Revision$"
#endif

/* Fix problem with DBG macro definition having a - in the name */
#define DEBUG_DECLARE_ONLY
#include "canon_pp.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_config.h"

#endif

/* 0x00 = Nibble Mode (M1284_NIBBLE)
   0x10 = ECP Mode (M1284_ECP)
   The scanner driver seems not to support ECP RLE mode 
   (which is a huge bummer because compression would be 
   ace) nor EPP mode.
   */
static int ieee_mode = M1284_NIBBLE;

/* For super-verbose debugging */
/* #define DUMP_PACKETS 1 */

/* Some sort of initialisation command */
static unsigned char cmd_init[10] = { 0xec, 0x20, 0, 0, 0, 0, 0, 0, 0, 0 };

/************* Local Prototypes ******************/

/* Used by wake_scanner */
static int scanner_reset(struct parport *port);
static void scanner_chessboard_control(struct parport *port);
static void scanner_chessboard_data(struct parport *port, int mode);

/* Used by read_data */
static int ieee_transfer(struct parport *port, int length, 
		unsigned char *data);

/* Low level functions */
static int readstatus(struct parport *port);
static int expect(struct parport *port, const char *step, int s, 
		int mask, unsigned int delay);

/* Port-level functions */
static void outdata(struct parport *port, int d);
static void outcont(struct parport *port, int d, int mask);
static void outboth(struct parport *port, int d, int c);

/************************************/

/* 
 * IEEE 1284 defines many values for m,
 * but these scanners only support 2: nibble and ECP modes.
 * And no data compression either (argh!)
 * 0 = Nibble-mode reverse channel transfer
 * 16 = ECP-mode
 */
void sanei_canon_pp_set_ieee1284_mode(int m)
{
	ieee_mode = m;
}

int sanei_canon_pp_wake_scanner(struct parport *port, int mode)
{
	/* The scanner tristates the printer's control lines
	   (essentially disabling the passthrough port) and exits
	   from Transparent Mode ready for communication. */
	int i = 0;
	int tmp;
	int max_cycles = 3;
	
	tmp = readstatus(port);

	/* Reset only works on 30/40 models */
	if (mode != INITMODE_20P)
	{
		if ((tmp != READY))
		{
			DBG(40, "Scanner not ready (0x%x). Attempting to "
					"reset...\n", tmp);
			scanner_reset(port);
			/* give it more of a chance to reset in this case */
			max_cycles = 5;
		}
	} else {
		DBG(0, "WARNING: Don't know how to reset an FBx20P, you may "
				"have to power cycle\n");
	}

	do
	{
		i++;

		/* Send the wakeup sequence */
		scanner_chessboard_control(port);
		scanner_chessboard_data(port, mode);

		if (expect(port, NULL, 0x03, 0x1f, 800000) && 
				(mode == INITMODE_AUTO))
		{
			/* 630 Style init failed, try 620 style */
			scanner_chessboard_control(port);
			scanner_chessboard_data(port, INITMODE_20P);
		}

		if (expect(port, "Scanner wakeup reply 1", 0x03, 0x1f, 50000))
		{
			outboth(port, 0x04, 0x0d);
			usleep(100000);
			outcont(port, 0x07, 0x0f);
			usleep(100000);
		}

	} while ((i < max_cycles) && (!expect(port,"Scanner wakeup reply 2", 
					0x03, 0x1f, 100000) == 0));

	/* Block just after chessboarding
	   Reply 1 (S3 and S4 on, S5 and S7 off) */
	outcont(port, 0, HOSTBUSY); /* C1 off */
	/* Reply 2 - If it ain't happening by now, it ain't gonna happen. */
	if (expect(port, "Reply 2", 0xc, 0x1f, 800000))
		return -1;
	outcont(port, HOSTBUSY, HOSTBUSY); /* C1 on */
	if (expect(port, "Reply 3", 0x0b, 0x1f, 800000))
		return -1;
	outboth(port, 0, NSELECTIN | NINIT | HOSTCLK); /* Clear D, C3+, C1- */

	/* If we had to try the wakeup cycle more than once, we should wait 
	 * here for 10 seconds to let the scanner pull itself together -
	 * it can actually take longer, but I can't wait that long! */
	if (i > 1)
	{
		DBG(10, "Had to reset scanner, waiting for the "
				"head to get back.\n");
		usleep(10000000);
	}

	return 0;
}


int sanei_canon_pp_write(struct parport *port, int length, unsigned char *data)
{

#ifdef DUMP_PACKETS
	ssize_t count;

	DBG(10,"Sending: ");
	for (count = 0; count < length; count++)
	{
		DBG(10,"%02x ", data[count]);
		if (count % 20 == 19)
			DBG(10,"\n      ");
	}	
	if (count % 20 != 19) DBG(10,"\n");
#endif

	DBG(100, "NEW Send Command (length %i):\n", length);

	switch (ieee_mode)
	{
		case M1284_BECP:
		case M1284_ECPRLE:
		case M1284_ECPSWE:
		case M1284_ECP: 
			ieee1284_negotiate(port, ieee_mode);
			if (ieee1284_ecp_write_data(port, 0, (char *)data, 
						length) != length)
				return -1;
			break;
		case M1284_NIBBLE: 
			if (ieee1284_compat_write(port, 0, (char *)data, 
						length) != length)
				return -1;
			break;
		default:
			DBG(0, "Invalid mode in write!\n");
	}		

	DBG(100, "<< write");

	return 0;
}

int sanei_canon_pp_read(struct parport *port, int length, unsigned char *data)
{
	int count, offset;

	DBG(200, "NEW read_data (%i bytes):\n", length);
	ieee1284_negotiate(port, ieee_mode);

	/* This is special; Nibble mode needs a little 
	   extra help from us. */

	if (ieee_mode == M1284_NIBBLE)
	{
		/* Interrupt phase */
		outcont(port, NSELECTIN, HOSTBUSY | NSELECTIN);
		if (expect(port, "Read Data 1", 0, NDATAAVAIL, 6000000))
		{
			DBG(10,"Error 1\n");
			ieee1284_terminate(port);	
			return 1;
		}
		outcont(port, HOSTBUSY, HOSTBUSY);

		if (expect(port, "Read Data 2",  NACK, NACK, 1000000))
		{
			DBG(1,"Error 2\n");
			ieee1284_terminate(port);			
			return 1;
		}
		if (expect(port, "Read Data 3 (Ready?)",  0, PERROR, 1000000))
		{
			DBG(1,"Error 3\n");
			ieee1284_terminate(port);
			return 1;
		}

		/* Host-Busy Data Available phase */

		if ((readstatus(port) & NDATAAVAIL) == NDATAAVAIL)
		{
			DBG(1,"No data to read.\n");
			ieee1284_terminate(port);
			return 1;
		}
	}

	offset = 0;

	DBG(100, "-> ieee_transfer(%d) *\n", length);
	count = ieee_transfer(port, length, data);
	DBG(100, "<- (%d)\n", count);
	/* Early-out if it was not implemented */
	if (count == E1284_NOTIMPL)
		return 2;

	length -= count;
	offset+= count;
	while (length > 0)
	{
		/* If 0 bytes were transferred, it's a legal
		   "No data" condition (I think). Otherwise,
		   it may have run out of buffer.. keep reading*/
		
		if (count < 0) {
			DBG(10, "Couldn't read enough data (need %d more "
					"of %d)\n", length+count,length+offset);
			ieee1284_terminate(port);
			return 1;
		}

		DBG(100, "-> ieee_transfer(%d)\n", length);
		count = ieee_transfer(port, length, data+offset);
		DBG(100, "<- (%d)\n", count);
		length-=count;
		offset+= count;

	}

#ifdef DUMP_PACKETS
	if (length <= 60)
	{
		DBG(10,"Read: ");
		for (count = 0; count < length; count++)
		{
			DBG(10,"%02x ", data[count]);
			if (count % 20 == 19)
				DBG(10,"\n      ");
		}	

		if (count % 20 != 19) DBG(10,"\n");
		}
	else
	{
		DBG(10,"Read: %i bytes\n", length);
	}			
#endif

	if (ieee_mode == M1284_NIBBLE)
    ieee1284_terminate(port);

	return 0;

}

static int ieee_transfer(struct parport *port, int length, unsigned char *data)
{
	int result = 0;

	DBG(100, "IEEE transfer (%i bytes)\n", length);

	switch (ieee_mode) 
	{
		case M1284_BECP:
		case M1284_ECP:
		case M1284_ECPRLE:
		case M1284_ECPSWE:
			result = ieee1284_ecp_read_data(port, 0, (char *)data, 
					length);
			break;
		case M1284_NIBBLE:
			result = ieee1284_nibble_read(port, 0, (char *)data, 
					length);
			break;
		default:
			DBG(1, "Internal error: Wrong mode for transfer.\n"
				"Please email stauff1@users.sourceforge.net\n"
				"or kinsei@users.sourceforge.net\n");
	}

	return result;
}

int sanei_canon_pp_check_status(struct parport *port)
{
	int status;
	unsigned char data[2];

	DBG(200, "* Check Status:\n");

	if (sanei_canon_pp_read(port, 2, data))
		return -1;

	status = data[0] | (data[1] << 8);

	switch(status)
	{
		case 0x0606:
			DBG(200, "Ready - 0x0606\n");
			return 0; 
			break;
		case 0x1414:
			DBG(200, "Busy - 0x1414\n"); 
			return 1;
			break;
		case 0x0805:
			DBG(200, "Resetting - 0x0805\n"); 
			return 3;
			break;
		case 0x1515:
			DBG(1, "!! Invalid Command - 0x1515\n");
			return 2; 
			break;
		case 0x0000:
			DBG(200, "Nothing - 0x0000"); 
			return 4;
			break;

		default:
			DBG(1, "!! Unknown status - %04x\n", status);
			return 100;
	}
}


/* Send a raw byte to the printer port */
static void outdata(struct parport *port, int d)
{
	ieee1284_write_data(port, d & 0xff);
}

/* Send the low nibble of d to the control port.
   The mask affects which bits are changed. */
static void outcont(struct parport *port, int d, int mask)
{
	static int control_port_status = 0;
	control_port_status = (control_port_status & ~mask) | (d & mask);
	ieee1284_write_control(port, (control_port_status & 0x0f));
}

/* Send a byte to both ports */
static void outboth(struct parport *port, int d, int c)
{
	ieee1284_write_data(port, d & 0xff);
	outcont(port, c, 0x0f);
}	

/* readstatus(): 
   Returns the LOGIC value of the S register (ie: all input lines)
   shifted right to to make it easier to read. Note: S5 is inverted
   by ieee1284_read_status so we don't need to */
static int readstatus(struct parport *port)
{
	return (ieee1284_read_status(port) & 0xf8) >> 3;
}

static void scanner_chessboard_control(struct parport *port)
{
	/* Wiggle C1 and C3 (twice) */
	outboth(port, 0x0, 13);
	usleep(10);
	outcont(port, 7, 0xf);
	usleep(10);
	outcont(port, 13, 0xf);
	usleep(10);
	outcont(port, 7, 0xf);
	usleep(10);	
}

static void scanner_chessboard_data(struct parport *port, int mode)
{
	int count;

	/* initial weirdness here for 620P - seems to go quite fast,
	 * just ignore it! */

	for (count = 0; count < 2; count++)
	{
		/* Wiggle data lines (4 times) while strobing C1 */
		/* 33 here for *30P, 55 for *20P */
		if (mode == INITMODE_20P)
			outdata(port, 0x55);
		else
			outdata(port, 0x33);
		outcont(port, HOSTBUSY, HOSTBUSY);	
		usleep(10);
		outcont(port, 0, HOSTBUSY);	
		usleep(10);
		outcont(port, HOSTBUSY, HOSTBUSY);	
		usleep(10);

		if (mode == INITMODE_20P)
			outdata(port, 0xaa);
		else
			outdata(port, 0xcc);
		outcont(port, HOSTBUSY, HOSTBUSY);	
		usleep(10);
		outcont(port, 0, HOSTBUSY);	
		usleep(10);
		outcont(port, HOSTBUSY, HOSTBUSY);	
		usleep(10);
	}
}

/* Reset the scanner. At least, it works 50% of the time. */
static int scanner_reset(struct parport *port) 
{

	/* Resetting only works for the *30Ps, sorry */
	if (readstatus(port) == 0x0b)
	{
		/* Init Block 1 - composed of a 0-byte IEEE read */
		ieee1284_negotiate(port, 0x0);
		ieee1284_terminate(port);
		ieee1284_negotiate(port, 0x0);
		ieee1284_terminate(port);
		scanner_chessboard_data(port, 1);
		scanner_chessboard_data(port, 1);
		scanner_chessboard_data(port, 1);
		scanner_chessboard_data(port, 1);

		scanner_chessboard_data(port, 0);
		scanner_chessboard_data(port, 0);
		scanner_chessboard_data(port, 0);
		scanner_chessboard_data(port, 0);
	}

	/* Reset Block 2 =============== */
	outboth(port, 0x04, 0x0d);

	/* Specifically, we want this: 00111 on S */
	if (expect(port, "Reset 2 response 1", 0x7, 0x1f, 500000))
		return 1;

	outcont(port, 0, HOSTCLK);
	usleep(5);
	outcont(port, 0x0f, 0xf); /* All lines must be 1. */

	/* All lines 1 */
	if (expect(port, "Reset 2 response 2 (READY)", 
				0x1f, 0x1f, 500000))
		return 1;

	outcont(port, 0, HOSTBUSY);
	usleep(100000); /* a short pause */
	outcont(port, HOSTBUSY, HOSTBUSY | NSELECTIN);

	return 0;
}

/* A timed version of expect, which will wait for delay before erroring 
   This is the one and only one we should be using */
static int expect(struct parport *port, const char *msg, int s, 
		int mask, unsigned int delay)
{
	struct timeval tv;

	tv.tv_sec = delay / 1000000;
	tv.tv_usec = delay % 1000000;

	if (ieee1284_wait_status(port, mask << 3, s << 3, &tv))
	{
		if (msg) DBG(10, "Timeout: %s (0x%02x in 0x%02x) - Status "
				"= 0x%02x\n", msg, s, mask, readstatus(port));
		return 1;
	}

	return 0;
}

int sanei_canon_pp_scanner_init(struct parport *port)
{

	int tries = 0;
	int tmp = 0;

	/* Put the scanner in nibble mode */
	ieee1284_negotiate(port, 0x0);

	/* No data to read yet - return to idle mode */
	ieee1284_terminate(port);

	/* In Windows, this is always ECP (or an attempt at it) */
	if (sanei_canon_pp_write(port, 10, cmd_init))
		return -1;
	/* Note that we don't really mind what the status was as long as it 
	 * wasn't a read error (returns -1) */
	/* In fact, the 620P gives an error on that last command, but they
	 * keep going anyway */
	if (sanei_canon_pp_check_status(port) < 0)
		return -1;

	/* Try until it's ready */
	sanei_canon_pp_write(port, 10, cmd_init);
	while ((tries < 3) && (tmp = sanei_canon_pp_check_status(port)))
	{
		if (tmp < 0)
			return -1;
		DBG(10, "scanner_init: Giving the scanner a snooze...\n");
		usleep(500000); 

		tries++;

		sanei_canon_pp_write(port, 10, cmd_init);
	}

	if (tries == 3) return 1;

	return 0;
}
