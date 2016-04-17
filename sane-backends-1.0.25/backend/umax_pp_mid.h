/* sane - Scanner Access Now Easy.
   Copyright (C) 2001-2012 Stéphane Voltz <stef.dev@free.fr>
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

   This file implements a SANE backend for Umax PP flatbed scanners.  */

#ifdef HAVE_LINUX_PPDEV_H
#include <linux/parport.h>
#include <linux/ppdev.h>
#include <sys/ioctl.h>
#endif

#include "umax_pp_low.h"

#ifndef _UMAX1220P_
#define  _UMAX1220P_

#define UMAX1220P_OK               0
#define UMAX1220P_NOSCANNER        1
#define UMAX1220P_TRANSPORT_FAILED 2
#define UMAX1220P_PROBE_FAILED     3
#define UMAX1220P_SCANNER_FAILED   4
#define UMAX1220P_PARK_FAILED      5
#define UMAX1220P_START_FAILED     6
#define UMAX1220P_READ_FAILED      7
#define UMAX1220P_BUSY             8

/*
 probes the port for an umax1220p scanner
 initialize transport layer
 probe scanner

 if name is null, direct I/O is attempted to address given in port
 else ppdev is tried using the given name as device

 on success returns UMAX1220P_OK,
 else one of the error above.

*/

extern int sanei_umax_pp_attach (int port, char *name);

/*
 recognizes 1220P from 2000P

 on success returns UMAX1220P_OK,
 else one of the error above.

*/

extern int sanei_umax_pp_model (int port, int *model);


/* 
if on=1 -> lights scanner lamp
if on=0 -> lights off scanner lamp

 on success returns UMAX1220P_OK,
 else one of the error above.
*/

extern int sanei_umax_pp_lamp (int on);



/*
 probes the port for an umax1220p scanner
 initialize transport layer
 initialize scanner

 on succes returns UMAX1220P_OK,
 else one of the error above.

 port: addr when doing direc I/O
 name: ppdev character device name

*/

extern int sanei_umax_pp_open (int port, char *name);



/* 
	release any ressource acquired during open
	since there may be only one scanner, no port parameter
*/
extern int sanei_umax_pp_close (void);




/*
 stops any pending action, then parks the head
*/
extern int sanei_umax_pp_cancel (void);

/* starts scanning:
	- find scanner origin
	- does channel gain calibration if needed
	- does calibration
	- initialize scan operation

	x, y, width and height are expressed in 600 dpi unit

	dpi must be 75, 150, 300, 600 or 1200

	color is true for color scan, false for gray-levels

	gain value is 256*red_gain+16*green_gain+blue_gain
	if gain is given (ie <> 0), auto gain will not be performed

	

   returns UMAX1220P_OK on success, or one of the error above 
   if successful, rbpp holds bytes/pixel, rth the height and rtw
   the width of scanned area expressed in pixels
*/
extern int sanei_umax_pp_start (int x, int y, int width, int height, int dpi,
				int color, int autoset, int gain,
				int offset, int *rbpp, int *rtw, int *rth);


/* reads one block of data from scanner
	returns UMAX1220P_OK on success, or UMAX1220P_READ_FAILED on error
	it also sets internal cancel flag on error
	
	len if the length of the block needed
	window if the width in pixels of the scanned area
	dpi is the resolution, it is used to choose the best read method
	last is true if it is the last block of the scan
	buffer will hold the data read
*/
extern int sanei_umax_pp_read (long len, int window, int dpi, int last,
			       unsigned char *buffer);



/* get ASIC status from scanner
	returns UMAX1220P_OK if scanner idle, or UMAX1220P_BUSY if
	scanner's motor is on
*/
extern int sanei_umax_pp_status (void);

/* set auto calibration 0: no, else yes */
extern void sanei_umax_pp_setauto (int mode);

/* set umax astra model number */
extern void sanei_umax_pp_setastra (int val);

/* set gamma tables */
extern void sanei_umax_pp_gamma (int *red, int *green, int *blue);

/* sets coordinate of first usable left pixel */
extern int sanei_umax_pp_getLeft (void);
#endif
