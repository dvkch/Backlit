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

   canon_pp-dev.h: $Revision$

   This file is part of the canon_pp backend, supporting Canon FBX30P 
   and NX40P scanners and also part of the stand-alone driver.

   Simon Krix <kinsei@users.sourceforge.net>
   */

#ifndef CANON_PP_DEV_H

#define CANON_PP_DEV_H

/* Signal names */
/* C port */
#define READY 0x1f
#define NSELECTIN 0x08
#define NINIT 0x04
#define HOSTBUSY 0x02
#define HOSTCLK 0x01

/* S port */
#define BUSY 0x10
#define NACK 0x08
#define PTRCLK 0x08
#define PERROR 0x04
#define ACKDATAREQ 0x04
#define XFLAG 0x02
#define SELECT 0x02
#define NERROR 0x01
#define NFAULT 0x01
#define NDATAAVAIL 0x01

/* Scanner things */
#define CALSIZE 18      /* Lines per calibration */

/* Init modes */
#define INITMODE_20P 1
#define INITMODE_30P 2
#define INITMODE_AUTO 3

/* Misc things */
#define T_SCAN 1
#define T_CALIBRATE 2

/* Macros */
#define MAKE_SHORT(a,b) (((short)a)*0x100+(short)b)
#define LOW_BYTE(a) (a%0x100)
#define HIGH_BYTE(a) (a/0x100)

typedef struct scanner_parameter_struct
{
	/* This is the port the scanner is on, in libieee1284-readable form */
	struct parport *port;

	/* Width of the scanning head in pixels */
	int scanheadwidth;
	int scanbedlength;

	/* Resolution of the scan head where dpi = 75 << natural_resolution */
	int natural_xresolution;
	int natural_yresolution;

	int max_xresolution;
	int max_yresolution;

	/* ID String. Should only be 38(?) bytes long, so we can 
	   reduce the size later. */
	char id_string[80]; 

	/* Short, readable scanner name, such as "FB330P" */
	char name[40];

	/* Pixel weight values from calibration, one per pixel on the scan head.
	   These must be allocated before any scanning can be done. */
	unsigned long *blackweight;
	unsigned long *redweight;
	unsigned long *greenweight;
	unsigned long *blueweight;

	/* Not understood white-balance/gain values */
	unsigned char gamma[32]; 

	/* Type of scanner ( 0 = *20P, 1 = [*30P|*40P] ) */
	unsigned char type;

	/* Are we aborting this scanner now */
	unsigned char abort_now;

} scanner_parameters;

typedef struct scan_parameter_struct
{
	/* Size of image */
	unsigned int width, height;
	/* Position of image on the scanner bed */
	unsigned int xoffset, yoffset;
	/* Resolution at which to scan (remember it's 75 << resolution) */
	int xresolution, yresolution;
	/* Mode of image. 0 = greyscale, 1 = truecolour */
	int mode;
} scan_parameters;

typedef struct image_segment_struct
{
	/* Size of image segment */
	unsigned int width, height;
	/* Which part of the image this is */
	unsigned int start_scanline;
	/* Pointer to image data */
	unsigned char *image_data;
} image_segment;

/* Scan-related functions ========================= */

/* Brings the scanner in and out of transparent mode 
   and detects model information */
int sanei_canon_pp_initialise(scanner_parameters *sp, int mode);
int sanei_canon_pp_close_scanner(scanner_parameters *sp);

/* Image scanning functions */
int sanei_canon_pp_init_scan(scanner_parameters *sp, scan_parameters *scanp);

int sanei_canon_pp_read_segment(image_segment **dest, scanner_parameters *sp, 
		scan_parameters *scanp, int scanline_count, int do_adjust,
		int scanlines_left);

int sanei_canon_pp_abort_scan(scanner_parameters *sp);

/* Loads the gain offset values. Needs a new name. */
int sanei_canon_pp_load_weights(const char *filename, scanner_parameters *sp);


int sanei_canon_pp_calibrate(scanner_parameters *sp, char *cal_file);

int sanei_canon_pp_adjust_gamma(scanner_parameters *sp);

/* Detect if a scanner is present on a given port */
int sanei_canon_pp_detect(struct parport *port, int mode);

/* Put a scanner to sleep */
int sanei_canon_pp_sleep_scanner(struct parport *port);

#endif
