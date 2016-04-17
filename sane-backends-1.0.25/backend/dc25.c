/***************************************************************************
 * SANE - Scanner Access Now Easy.

   dc25.c 

   $Id$

   This file (C) 1998 Peter Fales

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

 ***************************************************************************

   This file implements a SANE backend for the Kodak DC-25 (and 
   probably the DC-20) digital cameras.  THIS IS EXTREMELY ALPHA CODE!
   USE AT YOUR OWN RISK!! 

   (feedback to:  dc25-devel@fales-lorenz.net)

   This backend is based heavily on the dc20ctrl package by Ugo
   Paternostro <paterno@dsi.unifi.it>.  I've attached his header below:

 ***************************************************************************

 *	Copyright (C) 1998 Ugo Paternostro <paterno@dsi.unifi.it>
 *
 *	This file is part of the dc20ctrl package. The complete package can be
 *	downloaded from:
 *	    http://aguirre.dsi.unifi.it/~paterno/binaries/dc20ctrl.tar.gz
 *
 *	This package is derived from the dc20 package, built by Karl Hakimian
 *	<hakimian@aha.com> that you can find it at ftp.eecs.wsu.edu in the
 *	/pub/hakimian directory. The complete URL is:
 *	    ftp://ftp.eecs.wsu.edu/pub/hakimian/dc20.tar.gz
 *
 *	This package also includes a sligthly modified version of the Comet to ppm
 *	conversion routine written by YOSHIDA Hideki <hideki@yk.rim.or.jp>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published 
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *

 ***************************************************************************/

#include "../include/sane/config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#define BACKEND_NAME	dc25
#include "../include/sane/sanei_backend.h"

#include "dc25.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#define MAGIC			(void *)0xab730324
#define DC25_CONFIG_FILE 	"dc25.conf"
#define THUMBSIZE  ( (CameraInfo.model == 0x25 ) ? 14400 : 5120 )

static SANE_Bool is_open = 0;

static SANE_Byte dc25_opt_image_number = 1;	/* Image to load */
static SANE_Bool dc25_opt_thumbnails;	/* Load thumbnails */
static SANE_Bool dc25_opt_snap;	/* Take new picture */
static SANE_Bool dc25_opt_lowres;	/* Use low resoluiton */
#define DC25_OPT_CONTRAST_DEFAULT 1.6
						/* Contrast enhancement */
static SANE_Fixed dc25_opt_contrast = SANE_FIX (DC25_OPT_CONTRAST_DEFAULT);
#define DC25_OPT_GAMMA_DEFAULT 4.5
						/* Gamma correction (10x) */
static SANE_Fixed dc25_opt_gamma = SANE_FIX (DC25_OPT_GAMMA_DEFAULT);
static SANE_Bool dc25_opt_erase;	/* Erase all after download */
static SANE_Bool dc25_opt_erase_one;	/* Erase one after download */
static SANE_Bool dumpinquiry;

static SANE_Int info_flags;

static int tfd;			/* Camera File Descriptor */
static char tty_name[PATH_MAX];
#define DEF_TTY_NAME "/dev/ttyS0"

static speed_t tty_baud = DEFAULT_TTY_BAUD;
static char *tmpname;
static char tmpnamebuf[] = "/tmp/dc25XXXXXX";

static Dc20Info *dc20_info;
static Dc20Info CameraInfo;

static SANE_Byte contrast_table[256];

static struct pixmap *pp;

static const SANE_Range percentage_range = {
  -100 << SANE_FIXED_SCALE_SHIFT,	/* minimum */
  100 << SANE_FIXED_SCALE_SHIFT,	/* maximum */
  0 << SANE_FIXED_SCALE_SHIFT	/* quantization */
};

static const SANE_Range contrast_range = {
  0 << SANE_FIXED_SCALE_SHIFT,	/* minimum */
  3 << SANE_FIXED_SCALE_SHIFT,	/* maximum */
  16384				/* quantization ~ 0.025 */
};

static const SANE_Range gamma_range = {
  0 << SANE_FIXED_SCALE_SHIFT,	/* minimum */
  10 << SANE_FIXED_SCALE_SHIFT,	/* maximum */
  16384				/* quantization ~ 0.025 */
};

static SANE_Range image_range = {
  0,
  14,
  0
};

static SANE_Option_Descriptor sod[] = {
  {
   SANE_NAME_NUM_OPTIONS,
   SANE_TITLE_NUM_OPTIONS,
   SANE_DESC_NUM_OPTIONS,
   SANE_TYPE_INT,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define D25_OPT_IMAGE_SELECTION 1
  {
   "",
   "Image Selection",
   "Selection of the image to load.",
   SANE_TYPE_GROUP,
   SANE_UNIT_NONE,
   0,
   0,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define DC25_OPT_IMAGE_NUMBER 2
  {
   "image",
   "Image Number",
   "Select Image Number to load from camera",
   SANE_TYPE_INT,
   SANE_UNIT_NONE,
   4,
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_RANGE,
   {(SANE_String_Const *) & image_range}	/* this is ANSI conformant! */
   }
  ,

#define DC25_OPT_THUMBS 3
  {
   "thumbs",
   "Load Thumbnail",
   "Load the image as thumbnail.",
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
#define DC25_OPT_SNAP 4
  {
   "snap",
   "Snap new picture",
   "Take new picture and download it",
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
#define DC25_OPT_LOWRES 5
  {
   "lowres",
   "Low Resolution",
   "New pictures taken in low resolution",
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE |
   SANE_CAP_ADVANCED,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define DC25_OPT_ERASE 6
  {
   "erase",
   "Erase",
   "Erase all pictures after downloading",
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define DC25_OPT_ERASE_ONE 7
  {
   "erase-one",
   "Erase One",
   "Erase downloaded picture after downloading",
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define DC25_OPT_ENHANCE 8
  {
   "",
   "Image Parameters",
   "Modifications to image parameters",
   SANE_TYPE_GROUP,
   SANE_UNIT_NONE,
   0,
   0,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,

#define DC25_OPT_CONTRAST 9
  {
   "contrast",
   "Contrast Adjustment",
   "Values > 1 enhance contrast",
   SANE_TYPE_FIXED,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_RANGE,
   {(const SANE_String_Const *) &contrast_range}	/* this is ANSI conformant! */
   },

#define DC25_OPT_GAMMA 10
  {
   "gamma",
   "Gamma Adjustment",
   "Larger values make image darker",
   SANE_TYPE_FIXED,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_RANGE,
   {(const SANE_String_Const *) &gamma_range}	/* this is ANSI conformant! */
   },

#define DC25_OPT_DEFAULT 11
  {
   "default-enhancements",
   "Defaults",
   "Set default values for enhancement controls (i.e. contrast).",
   SANE_TYPE_BUTTON,
   SANE_UNIT_NONE,
   0,
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
};

static SANE_Parameters parms = {
  SANE_FRAME_RGB,
  1,
  500,				/* Number of bytes returned per scan line: */
  500,				/* Number of pixels per scan line.  */
  373,				/* Number of lines for the current scan.  */
  8,				/* Number of bits per sample. */
};




static unsigned char init_pck[] = INIT_PCK;

/*
 * List of speeds to try to establish connection with the camera.  
 * Check 9600 first, as it's the speed the camera comes up in, then
 * 115200, as that is the one most likely to be configured from a 
 * previous run
 */
static struct pkt_speed speeds[] = { {B9600, {0x96, 0x00}},
#ifdef B115200
{B115200, {0x11, 0x52}},
#endif
#ifdef B57600
{B57600, {0x57, 0x60}},
#endif
{B38400, {0x38, 0x40}},
{B19200, {0x19, 0x20}},
};
#define NUM_OF_SPEEDS	((int)(sizeof(speeds) / sizeof(struct pkt_speed)))

static struct termios tty_orig;

static int
send_pck (int fd, unsigned char *pck)
{
  int n;
  unsigned char r;

  /*
   * Not quite sure why we need this, but the program works a whole
   * lot better (at least on the DC25)  with this short delay.
   */
#ifdef HAVE_USLEEP
  usleep (10);
#else
  sleep (1);
#endif
  if (write (fd, (char *) pck, 8) != 8)
    {
      DBG (2, "send_pck: error: write returned -1\n");
      return -1;
    }

  if ((n = read (fd, (char *) &r, 1)) != 1)
    {
      DBG (2, "send_pck: error: read returned -1\n");
      return -1;
    }

  return (r == 0xd1) ? 0 : -1;
}

static int
init_dc20 (char *device, speed_t speed)
{
  struct termios tty_new;
  int speed_index;

  DBG (1, "DC-20/25 Backend 05/07/01\n");

  for (speed_index = 0; speed_index < NUM_OF_SPEEDS; speed_index++)
    {
      if (speeds[speed_index].baud == speed)
	{
	  init_pck[2] = speeds[speed_index].pkt_code[0];
	  init_pck[3] = speeds[speed_index].pkt_code[1];
	  break;
	}
    }

  if (init_pck[2] == 0)
    {
      DBG (2, "unsupported baud rate.\n");
      return -1;
    }

  /*
     Open device file.
   */
  if ((tfd = open (device, O_RDWR)) == -1)
    {
      DBG (2, "init_dc20: error: could not open %s for read/write\n", device);
      return -1;
    }
  /*
     Save old device information to restore when we are done.
   */
  if (tcgetattr (tfd, &tty_orig) == -1)
    {
      DBG (2, "init_dc20: error: could not get attributes\n");
      return -1;
    }

  memcpy ((char *) &tty_new, (char *) &tty_orig, sizeof (struct termios));
  /*
     We need the device to be raw. 8 bits even parity on 9600 baud to start.
   */
#ifdef HAVE_CFMAKERAW
  cfmakeraw (&tty_new);
#else
  tty_new.c_lflag &= ~(ICANON | ECHO | ISIG);
#endif
  tty_new.c_oflag &= ~CSTOPB;
  tty_new.c_cflag |= PARENB;
  tty_new.c_cflag &= ~PARODD;
  tty_new.c_cc[VMIN] = 0;
  tty_new.c_cc[VTIME] = 50;
  cfsetospeed (&tty_new, B9600);
  cfsetispeed (&tty_new, B9600);

  if (tcsetattr (tfd, TCSANOW, &tty_new) == -1)
    {
      DBG (2, "init_dc20: error: could not set attributes\n");
      return -1;
    }

  if (send_pck (tfd, init_pck) == -1)
    {
      /*
       *      The camera always powers up at 9600, so we try 
       *      that first.  However, it may be already set to 
       *      a different speed.  Try the entries in the table:
       */

      for (speed_index = NUM_OF_SPEEDS - 1; speed_index > 0; speed_index--)
	{
	  DBG (3, "init_dc20: changing speed to %d\n",
	       (int) speeds[speed_index].baud);

	  cfsetospeed (&tty_new, speeds[speed_index].baud);
	  cfsetispeed (&tty_new, speeds[speed_index].baud);

	  if (tcsetattr (tfd, TCSANOW, &tty_new) == -1)
	    {
	      DBG (2, "init_dc20: error: could not set attributes\n");
	      return -1;
	    }
	  if (send_pck (tfd, init_pck) != -1)
	    break;
	}

      if (speed_index == 0)
	{
	  tcsetattr (tfd, TCSANOW, &tty_orig);
	  DBG (2, "init_dc20: error: no suitable baud rate\n");
	  return -1;
	}
    }
  /*
     Set speed to requested speed. Also, make a long timeout (we need this for
     erase and shoot operations)
   */
  tty_new.c_cc[VTIME] = 150;
  cfsetospeed (&tty_new, speed);
  cfsetispeed (&tty_new, speed);

  if (tcsetattr (tfd, TCSANOW, &tty_new) == -1)
    {
      DBG (2, "init_dc20: error: could not set attributes\n");
      return -1;
    }

  return tfd;
}

static void
close_dc20 (int fd)
{
  DBG (127, "close_dc20() called\n");
  /*
   *      Put the camera back to 9600 baud
   */

  init_pck[2] = speeds[0].pkt_code[0];
  init_pck[3] = speeds[0].pkt_code[1];
  if (send_pck (fd, init_pck) == -1)
    {
      DBG (4, "close_dc20: error: could not set attributes\n");
    }

  /*
     Restore original device settings.
   */
  if (tcsetattr (fd, TCSANOW, &tty_orig) == -1)
    {
      DBG (4, "close_dc20: error: could not set attributes\n");
    }

  if (close (fd) == -1)
    {
      DBG (4, "close_dc20: error: could not close device\n");
    }
}

static unsigned char info_pck[] = INFO_PCK;

static Dc20Info *
get_info (int fd)
{
  unsigned char buf[256];

  if (send_pck (fd, info_pck) == -1)
    {
      DBG (2, "get_info: error: send_pck returned -1\n");
      return NULL;
    }

  DBG (9, "get_info: read info packet\n");

  if (read_data (fd, buf, 256) == -1)
    {
      DBG (2, "get_info: error: read_data returned -1\n");
      return NULL;
    }

  if (end_of_data (fd) == -1)
    {
      DBG (2, "get_info: error: end_of_data returned -1\n");
      return NULL;
    }

  CameraInfo.model = buf[1];
  CameraInfo.ver_major = buf[2];
  CameraInfo.ver_minor = buf[3];
  CameraInfo.pic_taken = buf[8] << 8 | buf[9];
  if (CameraInfo.model == 0x25)
    {

      /* Not sure where the previous line came from.  All the
       * information I have says that even on the DC20 the number of
       * standard res pics is in byte 17 and the number of high res pics
       * is in byte 19.  This is definitely true on my DC25.
       */
      CameraInfo.pic_taken = buf[17] + buf[19];
    }

  image_range.max = CameraInfo.pic_taken;
  image_range.min = CameraInfo.pic_taken ? 1 : 0;

  CameraInfo.pic_left = buf[10] << 8 | buf[11];

  if (CameraInfo.model == 0x25)
    {
      /* Not sure where the previous line came from.  All the
       * information I have says that even on the DC20 the number of
       * standard res pics left is in byte 23 and the number of high res 
       * pics left is in byte 21.  It seems to me that the conservative
       * approach is to report the number of high res pics left.
       */
      CameraInfo.pic_left = buf[21];
    }
  CameraInfo.flags.low_res = buf[23];

  if (CameraInfo.model == 0x25)
    {
      /* Not sure where the previous line came from.  All the
       * information I have says that even on the DC20 the low_res
       * byte is 11.
       */
      CameraInfo.flags.low_res = buf[11];
    }
  CameraInfo.flags.low_batt = buf[29];

  return &CameraInfo;
}

static int
read_data (int fd, unsigned char *buf, int sz)
{
  unsigned char ccsum;
  unsigned char rcsum;
  unsigned char c;
  int retries = 0;
  int n;
  int r = 0;
  int i;

  while (retries++ < 5)
    {

      /*
       * If this is not the first time through, then it must be
       * a retry - signal the camera that we didn't like what
       * we got.  In either case, start filling the packet
       */
      if (retries != 1)
	{

	  DBG (2, "Attempt retry %d\n", retries);
	  c = 0xe3;
	  if (write (fd, (char *) &c, 1) != 1)
	    {
	      DBG (2, "read_data: error: write ack\n");
	      return -1;
	    }

	}

      for (n = 0; n < sz && (r = read (fd, (char *) &buf[n], sz - n)) > 0;
	   n += r)
	;

      if (r <= 0)
	{
	  DBG (2, "read_data: error: read returned -1\n");
	  continue;
	}

      if (n < sz || read (fd, &rcsum, 1) != 1)
	{
	  DBG (2, "read_data: error: buffer underrun or no checksum\n");
	  continue;
	}

      for (i = 0, ccsum = 0; i < n; i++)
	ccsum ^= buf[i];

      if (ccsum != rcsum)
	{
	  DBG (2, "read_data: error: bad checksum (%02x != %02x)\n", rcsum,
	       ccsum);
	  continue;
	}

      /* If we got this far, then the packet is OK */
      break;
    }

  c = 0xd2;

  if (write (fd, (char *) &c, 1) != 1)
    {
      DBG (2, "read_data: error: write ack\n");
      return -1;
    }

  return 0;
}

static int
end_of_data (int fd)
{
  char c;

  if (read (fd, &c, 1) != 1)
    {
      DBG (2, "end_of_data: error: read returned -1\n");
      return -1;
    }

  if (c != 0)
    {
      DBG (2, "end_of_data: error: bad EOD from camera (%02x)\n",
	   (unsigned) c);
      return -1;
    }

  return 0;
}

#include <math.h>

#define BIDIM_ARRAY(name, x, y, width)	(name[((x) + ((y) * (width)))])

/*
 *	These definitions depend on the resolution of the image
 */

#define MY_LOW_RIGHT_MARGIN 6

/*
 *	These definitions are constant with resolution
 */

#define MY_LEFT_MARGIN 2

#define NET_COLUMNS (columns - MY_LEFT_MARGIN - right_margin)
#define NET_LINES   (HEIGHT - TOP_MARGIN - BOTTOM_MARGIN)
#define NET_PIXELS  (NET_COLUMNS * NET_LINES)


#define SCALE 64
#define SMAX (256 * SCALE - 1)
#define HORIZONTAL_INTERPOLATIONS 3
#define HISTOGRAM_STEPS 4096

#define RFACTOR 0.64
#define GFACTOR 0.58
#define BFACTOR 1.00
#define RINTENSITY 0.476
#define GINTENSITY 0.299
#define BINTENSITY 0.175

#define SATURATION 1.0
#define NORM_PERCENTAGE 3

static int columns = HIGH_WIDTH,
  right_margin = HIGH_RIGHT_MARGIN, camera_header_size = HIGH_CAMERA_HEADER;
static int low_i = -1, high_i = -1, norm_percentage = NORM_PERCENTAGE;
static float saturation = SATURATION,
  rfactor = RFACTOR, gfactor = GFACTOR, bfactor = BFACTOR;

static void
set_initial_interpolation (const unsigned char ccd[],
			   short horizontal_interpolation[])
{
  int column, line;
  for (line = 0; line < HEIGHT; line++)
    {
      BIDIM_ARRAY (horizontal_interpolation, MY_LEFT_MARGIN, line, columns) =
	BIDIM_ARRAY (ccd, MY_LEFT_MARGIN + 1, line, columns) * SCALE;
      BIDIM_ARRAY (horizontal_interpolation, columns - right_margin - 1, line,
		   columns) =
	BIDIM_ARRAY (ccd, columns - right_margin - 2, line, columns) * SCALE;
      for (column = MY_LEFT_MARGIN + 1; column < columns - right_margin - 1;
	   column++)
	{
	  BIDIM_ARRAY (horizontal_interpolation, column, line, columns) =
	    (BIDIM_ARRAY (ccd, column - 1, line, columns) +
	     BIDIM_ARRAY (ccd, column + 1, line, columns)) * (SCALE / 2);
	}
    }
}

static void
interpolate_horizontally (const unsigned char ccd[],
			  short horizontal_interpolation[])
{
  int column, line, i, initial_column;
  for (line = TOP_MARGIN - 1; line < HEIGHT - BOTTOM_MARGIN + 1; line++)
    {
      for (i = 0; i < HORIZONTAL_INTERPOLATIONS; i++)
	{
	  for (initial_column = MY_LEFT_MARGIN + 1;
	       initial_column <= MY_LEFT_MARGIN + 2; initial_column++)
	    {
	      for (column = initial_column;
		   column < columns - right_margin - 1; column += 2)
		{
		  BIDIM_ARRAY (horizontal_interpolation, column, line,
			       columns) =
		    ((float) BIDIM_ARRAY (ccd, column - 1, line, columns) /
		     BIDIM_ARRAY (horizontal_interpolation, column - 1, line,
				  columns) + (float) BIDIM_ARRAY (ccd,
								  column + 1,
								  line,
								  columns) /
		     BIDIM_ARRAY (horizontal_interpolation, column + 1, line,
				  columns)) * BIDIM_ARRAY (ccd, column, line,
							   columns) * (SCALE *
								       SCALE /
								       2) +
		    0.5;
		}
	    }
	}
    }
}

static void
interpolate_vertically (const unsigned char ccd[],
			const short horizontal_interpolation[],
			short red[], short green[], short blue[])
{
  int column, line;
  for (line = TOP_MARGIN; line < HEIGHT - BOTTOM_MARGIN; line++)
    {
      for (column = MY_LEFT_MARGIN; column < columns - right_margin; column++)
	{
	  int r2gb, g2b, rg2, rgb2, r, g, b;
	  int this_ccd = BIDIM_ARRAY (ccd, column, line, columns) * SCALE;
	  int up_ccd = BIDIM_ARRAY (ccd, column, line - 1, columns) * SCALE;
	  int down_ccd = BIDIM_ARRAY (ccd, column, line + 1, columns) * SCALE;
	  int this_horizontal_interpolation =
	    BIDIM_ARRAY (horizontal_interpolation, column, line, columns);
	  int this_intensity = this_ccd + this_horizontal_interpolation;
	  int up_intensity =
	    BIDIM_ARRAY (horizontal_interpolation, column, line - 1,
			 columns) + up_ccd;
	  int down_intensity =
	    BIDIM_ARRAY (horizontal_interpolation, column, line + 1,
			 columns) + down_ccd;
	  int this_vertical_interpolation;

	  /*
	   * PSF: I don't understand all this code, but I've found pictures
	   * where up_intensity or down_intensity are zero, resulting in a
	   * divide by zero error.  It looks like this only happens when
	   * up_ccd or down_ccd are also zero, so we just set the intensity
	   * value to non-zero to prevent the error.
	   */
	  if (down_ccd == 0)
	    DBG (10, "down_ccd==0 at %d,%d\n", line, column);
	  if (up_ccd == 0)
	    DBG (10, "up_ccd==0 at %d,%d\n", line, column);
	  if (down_intensity == 0)
	    {
	      DBG (9, "Found down_intensity==0 at %d,%d down_ccd=%d\n", line,
		   column, down_ccd);
	      down_intensity = 1;
	    }
	  if (up_intensity == 0)
	    {
	      DBG (9, "Found up_intensity==0 at %d,%d up_ccd=%d\n", line,
		   column, up_ccd);
	      up_intensity = 1;
	    }

	  if (line == TOP_MARGIN)
	    {
	      this_vertical_interpolation =
		(float) down_ccd / down_intensity * this_intensity + 0.5;
	    }
	  else if (line == HEIGHT - BOTTOM_MARGIN - 1)
	    {
	      this_vertical_interpolation =
		(float) up_ccd / up_intensity * this_intensity + 0.5;
	    }
	  else
	    {
	      this_vertical_interpolation =
		((float) up_ccd / up_intensity +
		 (float) down_ccd / down_intensity) * this_intensity / 2.0 +
		0.5;
	    }
	  if (line & 1)
	    {
	      if (column & 1)
		{
		  r2gb = this_ccd;
		  g2b = this_horizontal_interpolation;
		  rg2 = this_vertical_interpolation;
		  r = (2 * (r2gb - g2b) + rg2) / 5;
		  g = (rg2 - r) / 2;
		  b = g2b - 2 * g;
		}
	      else
		{
		  g2b = this_ccd;
		  r2gb = this_horizontal_interpolation;
		  rgb2 = this_vertical_interpolation;
		  r = (3 * r2gb - g2b - rgb2) / 5;
		  g = 2 * r - r2gb + g2b;
		  b = g2b - 2 * g;
		}
	    }
	  else
	    {
	      if (column & 1)
		{
		  rg2 = this_ccd;
		  rgb2 = this_horizontal_interpolation;
		  r2gb = this_vertical_interpolation;
		  b = (3 * rgb2 - r2gb - rg2) / 5;
		  g = (rgb2 - r2gb + rg2 - b) / 2;
		  r = rg2 - 2 * g;
		}
	      else
		{
		  rgb2 = this_ccd;
		  rg2 = this_horizontal_interpolation;
		  g2b = this_vertical_interpolation;
		  b = (g2b - 2 * (rg2 - rgb2)) / 5;
		  g = (g2b - b) / 2;
		  r = rg2 - 2 * g;
		}
	    }
	  if (r < 0)
	    r = 0;
	  if (g < 0)
	    g = 0;
	  if (b < 0)
	    b = 0;
	  BIDIM_ARRAY (red, column, line, columns) = r;
	  BIDIM_ARRAY (green, column, line, columns) = g;
	  BIDIM_ARRAY (blue, column, line, columns) = b;
	}
    }
}

static void
adjust_color_and_saturation (short red[], short green[], short blue[])
{
  int line, column;
  int r_min = SMAX, g_min = SMAX, b_min = SMAX;
  int r_max = 0, g_max = 0, b_max = 0;
  int r_sum = 0, g_sum = 0, b_sum = 0;
  float sqr_saturation = sqrt (saturation);
  for (line = TOP_MARGIN; line < HEIGHT - BOTTOM_MARGIN; line++)
    {
      for (column = MY_LEFT_MARGIN; column < columns - right_margin; column++)
	{
	  float r = BIDIM_ARRAY (red, column, line, columns) * rfactor;
	  float g = BIDIM_ARRAY (green, column, line, columns) * gfactor;
	  float b = BIDIM_ARRAY (blue, column, line, columns) * bfactor;
	  if (saturation != 1.0)
	    {
	      float *min, *mid, *max, new_intensity;
	      float intensity =
		r * RINTENSITY + g * GINTENSITY + b * BINTENSITY;
	      if (r > g)
		{
		  if (r > b)
		    {
		      max = &r;
		      if (g > b)
			{
			  min = &b;
			  mid = &g;
			}
		      else
			{
			  min = &g;
			  mid = &b;
			}
		    }
		  else
		    {
		      min = &g;
		      mid = &r;
		      max = &b;
		    }
		}
	      else
		{
		  if (g > b)
		    {
		      max = &g;
		      if (r > b)
			{
			  min = &b;
			  mid = &r;
			}
		      else
			{
			  min = &r;
			  mid = &b;
			}
		    }
		  else
		    {
		      min = &r;
		      mid = &g;
		      max = &b;
		    }
		}
	      *mid = *min + sqr_saturation * (*mid - *min);
	      *max = *min + saturation * (*max - *min);
	      new_intensity =
		r * RINTENSITY + g * GINTENSITY + b * BINTENSITY;
	      r *= intensity / new_intensity;
	      g *= intensity / new_intensity;
	      b *= intensity / new_intensity;
	    }
	  r += 0.5;
	  g += 0.5;
	  b += 0.5;
	  if (r_min > r)
	    r_min = r;
	  if (g_min > g)
	    g_min = g;
	  if (b_min > b)
	    b_min = b;
	  if (r_max < r)
	    r_max = r;
	  if (g_max < g)
	    g_max = g;
	  if (b_max < b)
	    b_max = b;
	  r_sum += r;
	  g_sum += g;
	  b_sum += b;
	  BIDIM_ARRAY (red, column, line, columns) = r;
	  BIDIM_ARRAY (green, column, line, columns) = g;
	  BIDIM_ARRAY (blue, column, line, columns) = b;
	}
    }
}

static int
min3 (int x, int y, int z)
{
  return (x < y ? (x < z ? x : z) : (y < z ? y : z));
}

static int
max3 (int x, int y, int z)
{
  return (x > y ? (x > z ? x : z) : (y > z ? y : z));
}

static void
determine_limits (const short red[],
		  const short green[],
		  const short blue[], int *low_i_ptr, int *high_i_ptr)
{
  unsigned int histogram[HISTOGRAM_STEPS + 1];
  int column, line, i, s;
  int low_i = *low_i_ptr, high_i = *high_i_ptr;
  int max_i = 0;
  for (line = TOP_MARGIN; line < HEIGHT - BOTTOM_MARGIN; line++)
    {
      for (column = MY_LEFT_MARGIN; column < columns - right_margin; column++)
	{
	  i = max3 (BIDIM_ARRAY (red, column, line, columns),
		    BIDIM_ARRAY (green, column, line, columns),
		    BIDIM_ARRAY (blue, column, line, columns));
	  if (i > max_i)
	    max_i = i;
	}
    }
  if (low_i == -1)
    {
      for (i = 0; i <= HISTOGRAM_STEPS; i++)
	histogram[i] = 0;
      for (line = TOP_MARGIN; line < HEIGHT - BOTTOM_MARGIN; line++)
	{
	  for (column = MY_LEFT_MARGIN; column < columns - right_margin;
	       column++)
	    {
	      i = min3 (BIDIM_ARRAY (red, column, line, columns),
			BIDIM_ARRAY (green, column, line, columns),
			BIDIM_ARRAY (blue, column, line, columns));
	      histogram[i * HISTOGRAM_STEPS / max_i]++;
	    }
	}
      for (low_i = 0, s = 0;
	   low_i <= HISTOGRAM_STEPS && s < NET_PIXELS * norm_percentage / 100;
	   low_i++)
	{
	  s += histogram[low_i];
	}
      low_i = (low_i * max_i + HISTOGRAM_STEPS / 2) / HISTOGRAM_STEPS;
      *low_i_ptr = low_i;
    }
  if (high_i == -1)
    {
      for (i = 0; i <= HISTOGRAM_STEPS; i++)
	histogram[i] = 0;
      for (line = TOP_MARGIN; line < HEIGHT - BOTTOM_MARGIN; line++)
	{
	  for (column = MY_LEFT_MARGIN; column < columns - right_margin;
	       column++)
	    {
	      i = max3 (BIDIM_ARRAY (red, column, line, columns),
			BIDIM_ARRAY (green, column, line, columns),
			BIDIM_ARRAY (blue, column, line, columns));
	      histogram[i * HISTOGRAM_STEPS / max_i]++;
	    }
	}
      for (high_i = HISTOGRAM_STEPS, s = 0;
	   high_i >= 0 && s < NET_PIXELS * norm_percentage / 100; high_i--)
	{
	  s += histogram[high_i];
	}
      high_i = (high_i * max_i + HISTOGRAM_STEPS / 2) / HISTOGRAM_STEPS;
      *high_i_ptr = high_i;
    }
/*
if (verbose) printf ("%s: determine_limits: low_i = %d, high_i = %d\n", __progname, low_i, high_i);
*/
}

/*
 * The original dc20ctrl program used a default gamma of 0.35, but I thougt
 * 0.45 looks better.  In addition, since xscanimage seems to always force 
 * a resolution of 0.1, I multiply everything by 10 and make the default
 * 4.5.
 */

static unsigned char *
make_gamma_table (int range)
{
  int i;
  double factor =
    pow (256.0, 1.0 / (SANE_UNFIX (dc25_opt_gamma) / 10.0)) / range;
  unsigned char *gamma_table;
  if ((gamma_table = malloc (range * sizeof (unsigned char))) == NULL)
    {
      DBG (1, "make_gamma_table: can't allocate memory for gamma table\n");
      return NULL;
    }
  for (i = 0; i < range; i++)
    {
      int g =
	pow ((double) i * factor, (SANE_UNFIX (dc25_opt_gamma) / 10.0)) + 0.5;
/*
		if (verbose) fprintf (stderr, "%s: make_gamma_table: gamma[%4d] = %3d\n", __progname, i, g);
*/
      if (g > 255)
	g = 255;
      gamma_table[i] = g;
    }
  return gamma_table;
}

static int
lookup_gamma_table (int i, int low_i, int high_i,
		    const unsigned char gamma_table[])
{
  if (i <= low_i)
    return 0;
  if (i >= high_i)
    return 255;
  return gamma_table[i - low_i];
}

static int
output_rgb (const short red[],
	    const short green[],
	    const short blue[], int low_i, int high_i, struct pixmap *pp)
{
  int r_min = 255, g_min = 255, b_min = 255;
  int r_max = 0, g_max = 0, b_max = 0;
  int r_sum = 0, g_sum = 0, b_sum = 0;
  int column, line;
  unsigned char *gamma_table = make_gamma_table (high_i - low_i);

  if (gamma_table == NULL)
    {
      DBG (10, "output_rgb: error: cannot make gamma table\n");
      return -1;
    }

  for (line = TOP_MARGIN; line < HEIGHT - BOTTOM_MARGIN; line++)
    {
      for (column = MY_LEFT_MARGIN; column < columns - right_margin; column++)
	{
	  int r =
	    lookup_gamma_table (BIDIM_ARRAY (red, column, line, columns),
				low_i, high_i, gamma_table);
	  int g =
	    lookup_gamma_table (BIDIM_ARRAY (green, column, line, columns),
				low_i, high_i, gamma_table);
	  int b =
	    lookup_gamma_table (BIDIM_ARRAY (blue, column, line, columns),
				low_i, high_i, gamma_table);
	  if (r > 255)
	    r = 255;
	  else if (r < 0)
	    r = 0;
	  if (g > 255)
	    g = 255;
	  else if (g < 0)
	    g = 0;
	  if (b > 255)
	    b = 255;
	  else if (b < 0)
	    b = 0;
	  set_pixel_rgb (pp, column - MY_LEFT_MARGIN, line - TOP_MARGIN, r, g,
			 b);
	  if (r_min > r)
	    r_min = r;
	  if (g_min > g)
	    g_min = g;
	  if (b_min > b)
	    b_min = b;
	  if (r_max < r)
	    r_max = r;
	  if (g_max < g)
	    g_max = g;
	  if (b_max < b)
	    b_max = b;
	  r_sum += r;
	  g_sum += g;
	  b_sum += b;
	}
    }
  free (gamma_table);
/*
	{
		fprintf (stderr, "%s: output_rgb: r: min = %d, max = %d, ave = %d\n", __progname, r_min, r_max, r_sum / NET_PIXELS);
		fprintf (stderr, "%s: output_rgb: g: min = %d, max = %d, ave = %d\n", __progname, g_min, g_max, g_sum / NET_PIXELS);
		fprintf (stderr, "%s: output_rgb: b: min = %d, max = %d, ave = %d\n", __progname, b_min, b_max, b_sum / NET_PIXELS);
	}
*/
  return 0;
}

static int
comet_to_pixmap (unsigned char *pic, struct pixmap *pp)
{
  unsigned char *ccd;
  short *horizontal_interpolation, *red, *green, *blue;
  int retval = 0;

  if (pic == NULL)
    {
      DBG (1, "cmttoppm: error: no input image\n");
      return -1;
    }

  if (pic[4] == 0x01)
    {
      /* Low resolution mode */
      columns = LOW_WIDTH;
      right_margin = MY_LOW_RIGHT_MARGIN;
      camera_header_size = LOW_CAMERA_HEADER;
    }
  else
    {
      /* High resolution mode */
      columns = HIGH_WIDTH;
      right_margin = HIGH_RIGHT_MARGIN;
      camera_header_size = HIGH_CAMERA_HEADER;
    }
  ccd = pic + camera_header_size;

  if ((horizontal_interpolation =
       malloc (sizeof (short) * HEIGHT * columns)) == NULL)
    {
      DBG (1,
	   "cmttoppm: error: not enough memory for horizontal_interpolation\n");
      return -1;
    }


  if ((red = malloc (sizeof (short) * HEIGHT * columns)) == NULL)
    {
      DBG (1, "error: not enough memory for red\n");
      return -1;
    }

  if ((green = malloc (sizeof (short) * HEIGHT * columns)) == NULL)
    {
      DBG (1, "error: not enough memory for green\n");
      return -1;
    }

  if ((blue = malloc (sizeof (short) * HEIGHT * columns)) == NULL)
    {
      DBG (1, "error: not enough memory for blue\n");
      return -1;
    }

  /* Decode raw CCD data to RGB */
  set_initial_interpolation (ccd, horizontal_interpolation);
  interpolate_horizontally (ccd, horizontal_interpolation);
  interpolate_vertically (ccd, horizontal_interpolation, red, green, blue);

  adjust_color_and_saturation (red, green, blue);

  /* Determine lower and upper limit using histogram */
  if (low_i == -1 || high_i == -1)
    {
      determine_limits (red, green, blue, &low_i, &high_i);
    }

  /* Output pixmap structure */
  retval = output_rgb (red, green, blue, low_i, high_i, pp);

  return retval;
}

static int
convert_pic (char *base_name, int format)
{
  FILE *ifp;
  unsigned char pic[MAX_IMAGE_SIZE];
  int res, image_size, image_width, net_width, camera_header, components;
  struct pixmap *pp2;

  DBG (127, "convert_pic() called\n");

  /*
   *      Read the image in memory
   */

  if ((ifp = fopen (base_name, "rb")) == NULL)
    {
      DBG (10, "convert_pic: error: cannot open %s for reading\n", base_name);
      return -1;
    }

  if (fread (pic, COMET_HEADER_SIZE, 1, ifp) != 1)
    {
      DBG (10, "convert_pic: error: cannot read COMET header\n");
      fclose (ifp);
      return -1;
    }

  if (strncmp ((char *) pic, COMET_MAGIC, sizeof (COMET_MAGIC)) != 0)
    {
      DBG (10, "convert_pic: error: file %s is not in COMET format\n",
	   base_name);
      fclose (ifp);
      return -1;
    }

  if (fread (pic, LOW_CAMERA_HEADER, 1, ifp) != 1)
    {
      DBG (10, "convert_pic: error: cannot read camera header\n");
      fclose (ifp);
      return -1;
    }

  res = pic[4];
  if (res == 0)
    {
      /*
       *      We just read a LOW_CAMERA_HEADER block, so resync with the
       *      HIGH_CAMERA_HEADER length by reading once more one of this.
       */
      if (fread (pic + LOW_CAMERA_HEADER, LOW_CAMERA_HEADER, 1, ifp) != 1)
	{
	  DBG (10,
	       "convert_pic: error: cannot resync with high resolution header\n");
	  fclose (ifp);
	  return -1;
	}
    }

  if (fread (pic + CAMERA_HEADER (res), WIDTH (res), HEIGHT, ifp) != HEIGHT)
    {
      DBG (9, "convert_pic: error: cannot read picture\n");
      fclose (ifp);
      return -1;
    }

  fclose (ifp);

  /*
   *      Setup image size with resolution
   */

  image_size = IMAGE_SIZE (res);
  image_width = WIDTH (res);
  net_width = image_width - LEFT_MARGIN - RIGHT_MARGIN (res);
  camera_header = CAMERA_HEADER (res);
  components = (format & SAVE_24BITS) ? 3 : 1;

  /*
   *      Convert the image to 24 bits
   */

  if ((pp =
       alloc_pixmap (net_width - 1, HEIGHT - BOTTOM_MARGIN - 1,
		     components)) == NULL)
    {
      DBG (1, "convert_pic: error: alloc_pixmap\n");
      return -1;
    }

  comet_to_pixmap (pic, pp);

  if (format & SAVE_ADJASPECT)
    {
      /*
       *      Strech image
       */

      if (res)
	pp2 = alloc_pixmap (320, HEIGHT - BOTTOM_MARGIN - 1, components);
      else
	pp2 = alloc_pixmap (net_width - 1, 373, components);

      if (pp2 == NULL)
	{
	  DBG (2, "convert_pic: error: alloc_pixmap\n");
	  free_pixmap (pp);
	  return -1;
	}

      if (res)
	zoom_x (pp, pp2);
      else
	zoom_y (pp, pp2);

      free_pixmap (pp);
      pp = pp2;
      pp2 = NULL;

    }

  return 0;
}

#define PGM_EXT		"pgm"
#define PPM_EXT		"ppm"

#define RED		0.30
#define GREEN		0.59
#define BLUE		0.11

#define RED_OFFSET	0
#define GREEN_OFFSET	1
#define BLUE_OFFSET	2

#define GET_COMP(pp, x, y, c)	(pp->planes[((x) + (y)*pp->width)*pp->components + (c)])

#define GET_R(pp, x, y)	(GET_COMP(pp, x, y, RED_OFFSET))
#define GET_G(pp, x, y)	(GET_COMP(pp, x, y, GREEN_OFFSET))
#define GET_B(pp, x, y)	(GET_COMP(pp, x, y, BLUE_OFFSET))

static struct pixmap *
alloc_pixmap (int x, int y, int d)
{
  struct pixmap *result = NULL;

  if (d == 1 || d == 3)
    {
      if (x > 0)
	{
	  if (y > 0)
	    {
	      if ((result = malloc (sizeof (struct pixmap))) != NULL)
		{
		  result->width = x;
		  result->height = y;
		  result->components = d;
		  if (!(result->planes = malloc (x * y * d)))
		    {
		      DBG (10,
			   "alloc_pixmap: error: not enough memory for bitplanes\n");
		      free (result);
		      result = NULL;
		    }
		}
	      else
		DBG (10,
		     "alloc_pixmap: error: not enough memory for pixmap\n");
	    }
	  else
	    DBG (10, "alloc_pixmap: error: y is out of range\n");
	}
      else
	DBG (10, "alloc_pixmap: error: x is out of range\n");
    }
  else
    DBG (10, "alloc_pixmap: error: cannot handle %d components\n", d);

  return result;
}

static void
free_pixmap (struct pixmap *p)
{
  if (p)
    {
      free (p->planes);
      free (p);
    }
}

static int
set_pixel_rgb (struct pixmap *p, int x, int y, unsigned char r,
	       unsigned char g, unsigned char b)
{
  int result = 0;

  if (p)
    {
      if (x >= 0 && x < p->width)
	{
	  if (y >= 0 && y < p->height)
	    {
	      if (p->components == 1)
		{
		  GET_R (p, x, y) = RED * r + GREEN * g + BLUE * b;
		}
	      else
		{
		  GET_R (p, x, y) = r;
		  GET_G (p, x, y) = g;
		  GET_B (p, x, y) = b;
		}
	    }
	  else
	    {
	      DBG (10, "set_pixel_rgb: error: y out of range\n");
	      result = -1;
	    }
	}
      else
	{
	  DBG (10, "set_pixel_rgb: error: x out of range\n");
	  result = -1;
	}
    }

  return result;
}

static int
zoom_x (struct pixmap *source, struct pixmap *dest)
{
  int result = 0, dest_col, row, component, src_index;
  float ratio, src_ptr, delta;
  unsigned char src_component;

  if (source && dest)
    {
      /*
       *      We could think of resizing a pixmap and changing the number of
       *      components at the same time. Maybe this will be implemented later.
       */
      if (source->height == dest->height
	  && source->components == dest->components)
	{
	  if (source->width < dest->width)
	    {
	      ratio = ((float) source->width / (float) dest->width);

	      for (src_ptr = 0, dest_col = 0; dest_col < dest->width;
		   src_ptr += ratio, dest_col++)
		{
		  /*
		   *      dest[dest_col] = source[(int)src_ptr] +
		   *        (source[((int)src_ptr) + 1] - source[(int)src_ptr])
		   *        * (src_ptr - (int)src_ptr);
		   */
		  src_index = (int) src_ptr;
		  delta = src_ptr - src_index;

		  for (row = 0; row < source->height; row++)
		    {
		      for (component = 0; component < source->components;
			   component++)
			{
			  src_component =
			    GET_COMP (source, src_index, row, component);

			  GET_COMP (dest, dest_col, row, component) =
			    src_component +
			    (GET_COMP (source, src_index + 1, row,
				       component) - src_component) * delta;
			}
		    }
		}
	    }
	  else
	    {
	      DBG (10, "zoom_x: error: can only zoom out\n");
	      result = -1;
	    }
	}
      else
	{
	  DBG (10, "zoom_x: error: incompatible pixmaps\n");
	  result = -1;
	}
    }

  return result;
}

static int
zoom_y (struct pixmap *source, struct pixmap *dest)
{
  int result = 0, dest_row, column, component, src_index;
  float ratio, src_ptr, delta;
  unsigned char src_component;

  if (source && dest)
    {
      /*
       *      We could think of resizing a pixmap and changing the number of
       *      components at the same time. Maybe this will be implemented later.
       */
      if (source->width == dest->width
	  && source->components == dest->components)
	{
	  if (source->height < dest->height)
	    {
	      ratio = ((float) source->height / (float) dest->height);

	      for (src_ptr = 0, dest_row = 0; dest_row < dest->height;
		   src_ptr += ratio, dest_row++)
		{
		  /*
		   *      dest[dest_row] = source[(int)src_ptr] +
		   *        (source[((int)src_ptr) + 1] - source[(int)src_ptr])
		   *        * (src_ptr - (int)src_ptr);
		   */
		  src_index = (int) src_ptr;
		  delta = src_ptr - src_index;

		  for (column = 0; column < source->width; column++)
		    {
		      for (component = 0; component < source->components;
			   component++)
			{
			  src_component =
			    GET_COMP (source, column, src_index, component);

			  GET_COMP (dest, column, dest_row, component) =
			    src_component +
			    (GET_COMP (source, column, src_index + 1,
				       component) - src_component) * delta;
			}
		    }
		}
	    }
	  else
	    {
	      DBG (10, "zoom_y: error: can only zoom out\n");
	      result = -1;
	    }
	}
      else
	{
	  DBG (10, "zoom_y: error: incompatible pixmaps\n");
	  result = -1;
	}
    }

  return result;
}

static unsigned char shoot_pck[] = SHOOT_PCK;

static int
shoot (int fd)
{
  struct termios tty_temp, tty_old;
  int result = 0;

  DBG (127, "shoot() called\n");

  if (write (fd, (char *) shoot_pck, 8) != 8)
    {
      DBG (3, "shoot: error: write error\n");
      return -1;
    }

  if (CameraInfo.model != 0x25)
    {
      /*
       *      WARNING: now we set the serial port to 9600 baud!
       */

      if (tcgetattr (fd, &tty_old) == -1)
	{
	  DBG (3, "shoot: error: could not get attributes\n");
	  return -1;
	}

      memcpy ((char *) &tty_temp, (char *) &tty_old, sizeof (struct termios));

      cfsetispeed (&tty_temp, B9600);
      cfsetospeed (&tty_temp, B9600);

      /* 
       * Apparently there is a bug in the DC20 where the response to 
       * the shoot request is always at 9600.  The DC25 does not have
       * this bug, so we skip this block.
       */
      if (tcsetattr (fd, TCSANOW, &tty_temp) == -1)
	{
	  DBG (3, "shoot: error: could not set attributes\n");
	  return -1;
	}
    }

  if (read (fd, (char *) &result, 1) != 1)
    {
      DBG (3, "shoot: error: read returned -1\n");
      result = -1;
    }
  else
    {
      result = (result == 0xD1) ? 0 : -1;
    }

  if (CameraInfo.model != 0x25)
    {
      /*
       * We reset the serial to its original speed.
       * We can skip this on the DC25 also.
       */
      if (tcsetattr (fd, TCSANOW, &tty_old) == -1)
	{
	  DBG (3, "shoot: error: could not reset attributes\n");
	  result = -1;
	}
    }

  if (result == 0)
    {
      if (CameraInfo.model == 0x25)
	{
	  /* 
	   * If we don't put this in, the next read will time out
	   * and return failure.  Does the DC-20 need it too?
	   */
	  sleep (3);
	}
      if (end_of_data (fd) == -1)
	{
	  DBG (3, "shoot: error: end_of_data returned -1\n");
	  result = -1;
	}
    }

  return result;
}


static unsigned char erase_pck[] = ERASE_PCK;

static int
erase (int fd)
{
  int count = 0;

  DBG (127, "erase() called for image %d\n", dc25_opt_image_number);
  erase_pck[3] = dc25_opt_image_number;
  if (dc25_opt_erase)
    {
      erase_pck[3] = 0;
    }

  if (send_pck (fd, erase_pck) == -1)
    {
      DBG (3, "erase: error: send_pck returned -1\n");
      return -1;
    }

  if (CameraInfo.model == 0x25)
    {
      /*
       * This block may really apply to the DC20 also, but since I
       * don't have one, it's hard to say for sure.  On the DC25, erase
       * takes long enought that the read may timeout without returning
       * any data before the erase is complete.   We let this happen 
       * up to 4 times, then give up.
       */
      while (count < 4)
	{
	  if (end_of_data (fd) == -1)
	    {
	      count++;
	    }
	  else
	    {
	      break;
	    }
	}
      if (count == 4)
	{
	  DBG (3, "erase: error: end_of_data returned -1\n");
	  return -1;
	}
    }
  else
    {				/* Assume DC-20 */

      if (end_of_data (fd) == -1)
	{
	  DBG (3, "erase: error: end_of_data returned -1\n");
	  return -1;
	}
    }

  return 0;
}

static unsigned char res_pck[] = RES_PCK;

static int
change_res (int fd, unsigned char res)
{
  DBG (127, "change_res called\n");
  if (res != 0 && res != 1)
    {
      DBG (3, "change_res: error: unsupported resolution\n");
      return -1;
    }

  res_pck[2] = res;

  if (send_pck (fd, res_pck) == -1)
    {
      DBG (4, "change_res: error: send_pck returned -1\n");
    }

  if (end_of_data (fd) == -1)
    {
      DBG (4, "change_res: error: end_of_data returned -1\n");
    }
  return 0;
}

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback UNUSEDARG authorize)
{
  char dev_name[PATH_MAX], *p;
  size_t len;
  FILE *fp;
  int baud;

  strcpy (tty_name, DEF_TTY_NAME);

  DBG_INIT ();

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (DC25_CONFIG_FILE);

  DBG (127,
       "sane_init() $Id$\n");

  if (!fp)
    {
      /* default to /dev/ttyS0 instead of insisting on config file */
      DBG (1, "sane_init:  missing config file '%s'\n", DC25_CONFIG_FILE);
    }
  else
    {
      while (sanei_config_read (dev_name, sizeof (dev_name), fp))
	{
	  dev_name[sizeof (dev_name) - 1] = '\0';
	  DBG (20, "sane_init:  config- %s", dev_name);

	  if (dev_name[0] == '#')
	    continue;		/* ignore line comments */
	  len = strlen (dev_name);
	  if (!len)
	    continue;		/* ignore empty lines */
	  if (strncmp (dev_name, "port=", 5) == 0)
	    {
	      p = strchr (dev_name, '/');
	      if (p)
		{
		  strcpy (tty_name, p);
		}
	      DBG (20, "Config file port=%s\n", tty_name);
	    }
	  else if (strncmp (dev_name, "baud=", 5) == 0)
	    {
	      baud = atoi (&dev_name[5]);
	      switch (baud)
		{
		case 9600:
		  tty_baud = B9600;
		  break;
		case 19200:
		  tty_baud = B19200;
		  break;
		case 38400:
		  tty_baud = B38400;
		  break;
#ifdef B57600
		case 57600:
		  tty_baud = B57600;
		  break;
#endif
#ifdef B115200
		case 115200:
		  tty_baud = B115200;
		  break;
#endif
		default:
		  DBG (20, "Unknown baud=%d\n", baud);
		  tty_baud = DEFAULT_TTY_BAUD;
		  break;
		}
	      DBG (20, "Config file baud=%lu\n", (u_long) tty_baud);
	    }
	  else if (strcmp (dev_name, "dumpinquiry") == 0)
	    {
	      dumpinquiry = SANE_TRUE;
	    }
	}
      fclose (fp);
    }

  if ((tfd = init_dc20 (tty_name, tty_baud)) == -1)
    {
      return SANE_STATUS_INVAL;
    }

  if ((dc20_info = get_info (tfd)) == NULL)
    {
      DBG (2, "error: could not get info\n");
      close_dc20 (tfd);
      return SANE_STATUS_INVAL;
    }

  if (dumpinquiry)
    {
      DBG (0, "\nCamera information:\n~~~~~~~~~~~~~~~~~\n\n");
      DBG (0, "Model...........: DC%x\n", dc20_info->model);
      DBG (0, "Firmware version: %d.%d\n", dc20_info->ver_major,
	   dc20_info->ver_minor);
      DBG (0, "Pictures........: %d/%d\n", dc20_info->pic_taken,
	   dc20_info->pic_taken + dc20_info->pic_left);
      DBG (0, "Resolution......: %s\n",
	   dc20_info->flags.low_res ? "low" : "high");
      DBG (0, "Battery state...: %s\n",
	   dc20_info->flags.low_batt ? "low" : "good");
    }

  if (CameraInfo.pic_taken == 0)
    {
/*
		sod[DC25_OPT_IMAGE_NUMBER].cap |= SANE_CAP_INACTIVE;
*/
      image_range.min = 0;
      dc25_opt_image_number = 0;

    }
  else
    {
/*
		sod[DC25_OPT_IMAGE_NUMBER].cap &= ~SANE_CAP_INACTIVE;
*/
      image_range.min = 1;
    }

  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
}

/* Device select/open/close */

static const SANE_Device dev[] = {
  {
   "0",
   "Kodak",
   "DC-25",
   "still camera"},
};

SANE_Status
sane_get_devices (const SANE_Device *** device_list,
		  SANE_Bool UNUSEDARG local_only)
{
  static const SANE_Device *devlist[] = {
    dev + 0, 0
  };

  DBG (127, "sane_get_devices called\n");

  if (dc20_info == NULL)
    {
      return SANE_STATUS_INVAL;
    }
  *device_list = devlist;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  int i;

  DBG (127, "sane_open for device %s\n", devicename);
  if (!devicename[0])
    {
      i = 0;
    }
  else
    {
      for (i = 0; i < NELEMS (dev); ++i)
	{
	  if (strcmp (devicename, dev[i].name) == 0)
	    {
	      break;
	    }
	}
    }

  if (i >= NELEMS (dev))
    {
      return SANE_STATUS_INVAL;
    }

  if (is_open)
    {
      return SANE_STATUS_DEVICE_BUSY;
    }

  is_open = 1;
  *handle = MAGIC;

  if (dc20_info == NULL)
    {
      DBG (1, "No device info\n");
    }

  if (tmpname == NULL)
    {
      tmpname = tmpnamebuf;
      if (mktemp (tmpname) == NULL)
	{
	  DBG (1, "Unable to make temp file %s\n", tmpname);
	  return SANE_STATUS_INVAL;
	}
    }

  DBG (3, "sane_open: pictures taken=%d\n", dc20_info->pic_taken);

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  DBG (127, "sane_close called\n");
  if (handle == MAGIC)
    is_open = 0;

  if (pp)
    {
      free_pixmap (pp);
      pp = NULL;
    }

  close_dc20 (tfd);

  DBG (127, "sane_close returning\n");
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  if (handle != MAGIC || !is_open)
    return NULL;		/* wrong device */
  if (option < 0 || option >= NELEMS (sod))
    return NULL;
  return &sod[option];
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *value, SANE_Int * info)
{
  SANE_Int myinfo = info_flags;
  SANE_Status status;

  info_flags = 0;

  DBG (127, "control_option(handle=%p,opt=%s,act=%s,val=%p,info=%p)\n",
       handle, sod[option].title,
       (action ==
	SANE_ACTION_SET_VALUE ? "SET" : (action ==
					 SANE_ACTION_GET_VALUE ? "GET" :
					 "SETAUTO")), value, (void *)info);

  if (handle != MAGIC || !is_open)
    return SANE_STATUS_INVAL;	/* Unknown handle ... */

  if (option < 0 || option >= NELEMS (sod))
    return SANE_STATUS_INVAL;	/* Unknown option ... */

  switch (action)
    {
    case SANE_ACTION_SET_VALUE:
      status = sanei_constrain_value (sod + option, value, &myinfo);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "Constraint error in control_option\n");
	  return status;
	}

      switch (option)
	{
	case DC25_OPT_IMAGE_NUMBER:
	  dc25_opt_image_number = *(SANE_Word *) value;
/*			myinfo |= SANE_INFO_RELOAD_OPTIONS; */
	  break;

	case DC25_OPT_THUMBS:
	  dc25_opt_thumbnails = !!*(SANE_Word *) value;
	  myinfo |= SANE_INFO_RELOAD_PARAMS;

	  if (dc25_opt_thumbnails)
	    {
	      /* 
	       * DC20 thumbnail are 80x60 grayscale, DC25
	       * thumbnails are color.
	       */
	      parms.format =
		(CameraInfo.model == 0x25) ? SANE_FRAME_RGB : SANE_FRAME_GRAY;
	      parms.bytes_per_line = 80 * 3;
	      parms.pixels_per_line = 80;
	      parms.lines = 60;
	    }
	  else
	    {
	      parms.format = SANE_FRAME_RGB;
	      if (dc20_info->flags.low_res)
		{
		  parms.bytes_per_line = 320 * 3;
		  parms.pixels_per_line = 320;
		  parms.lines = 243;
		}
	      else
		{
		  parms.bytes_per_line = 500 * 3;
		  parms.pixels_per_line = 500;
		  parms.lines = 373;
		}
	    }
	  break;

	case DC25_OPT_SNAP:
	  dc25_opt_snap = !!*(SANE_Word *) value;
	  myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  if (dc25_opt_snap)
	    {
	      sod[DC25_OPT_LOWRES].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      sod[DC25_OPT_LOWRES].cap |= SANE_CAP_INACTIVE;
	    }
	  break;

	case DC25_OPT_LOWRES:
	  dc25_opt_lowres = !!*(SANE_Word *) value;
	  myinfo |= SANE_INFO_RELOAD_PARAMS;

	  if (!dc25_opt_thumbnails)
	    {

	      parms.format = SANE_FRAME_RGB;

	      if (dc20_info->flags.low_res)
		{
		  parms.bytes_per_line = 320 * 3;
		  parms.pixels_per_line = 320;
		  parms.lines = 243;
		}
	      else
		{
		  parms.bytes_per_line = 500 * 3;
		  parms.pixels_per_line = 500;
		  parms.lines = 373;
		}

	    }
	  break;

	case DC25_OPT_CONTRAST:
	  dc25_opt_contrast = *(SANE_Word *) value;
	  break;

	case DC25_OPT_GAMMA:
	  dc25_opt_gamma = *(SANE_Word *) value;
	  break;

	case DC25_OPT_ERASE:
	  dc25_opt_erase = !!*(SANE_Word *) value;

	  /*
	   * erase and erase_one are mutually exclusive.  If
	   * this one is turned on, the other must be off 
	   */
	  if (dc25_opt_erase && dc25_opt_erase_one)
	    {
	      dc25_opt_erase_one = SANE_FALSE;
	      myinfo |= SANE_INFO_RELOAD_OPTIONS;
	    }
	  break;

	case DC25_OPT_ERASE_ONE:
	  dc25_opt_erase_one = !!*(SANE_Word *) value;

	  /*
	   * erase and erase_one are mutually exclusive.  If
	   * this one is turned on, the other must be off 
	   */
	  if (dc25_opt_erase_one && dc25_opt_erase)
	    {
	      dc25_opt_erase = SANE_FALSE;
	      myinfo |= SANE_INFO_RELOAD_OPTIONS;
	    }
	  break;

	case DC25_OPT_DEFAULT:

	  dc25_opt_contrast = SANE_FIX (DC25_OPT_CONTRAST_DEFAULT);
	  dc25_opt_gamma = SANE_FIX (DC25_OPT_GAMMA_DEFAULT);
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;

	default:
	  return SANE_STATUS_INVAL;
	}
      break;

    case SANE_ACTION_GET_VALUE:
      switch (option)
	{
	case 0:
	  *(SANE_Word *) value = NELEMS (sod);
	  break;

	case DC25_OPT_IMAGE_NUMBER:
	  *(SANE_Word *) value = dc25_opt_image_number;
	  break;

	case DC25_OPT_THUMBS:
	  *(SANE_Word *) value = dc25_opt_thumbnails;
	  break;

	case DC25_OPT_SNAP:
	  *(SANE_Word *) value = dc25_opt_snap;
	  break;

	case DC25_OPT_LOWRES:
	  *(SANE_Word *) value = dc25_opt_lowres;
	  break;

	case DC25_OPT_CONTRAST:
	  *(SANE_Word *) value = dc25_opt_contrast;
	  break;

	case DC25_OPT_GAMMA:
	  *(SANE_Word *) value = dc25_opt_gamma;
	  break;

	case DC25_OPT_ERASE:
	  *(SANE_Word *) value = dc25_opt_erase;
	  break;

	case DC25_OPT_ERASE_ONE:
	  *(SANE_Word *) value = dc25_opt_erase_one;
	  break;

	default:
	  return SANE_STATUS_INVAL;
	}
      break;

    case SANE_ACTION_SET_AUTO:
      switch (option)
	{
#if 0
	case DC25_OPT_CONTRAST:
	  dc25_opt_contrast = SANE_FIX (DC25_OPT_CONTRAST_DEFAULT);
	  break;

	case DC25_OPT_GAMMA:
	  dc25_opt_gamma = SANE_FIX (DC25_OPT_GAMMA_DEFAULT);
	  break;
#endif

	default:
	  return SANE_STATUS_UNSUPPORTED;	/* We are DUMB */
	}
    }

  if (info)
    *info = myinfo;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  int rc = SANE_STATUS_GOOD;

  DBG (127, "sane_get_params called\n");

  if (handle != MAGIC || !is_open)
    rc = SANE_STATUS_INVAL;	/* Unknown handle ... */


  *params = parms;
  return rc;
}

static unsigned char thumb_pck[] = THUMBS_PCK;

static unsigned char pic_pck[] = PICS_PCK;

static int bytes_in_buffer;
static int bytes_read_from_buffer;
static SANE_Byte buffer[1024];
static int total_bytes_read;
static SANE_Bool started = SANE_FALSE;
static int outbytes;

SANE_Status
sane_start (SANE_Handle handle)
{
  int n, i;
  FILE *f;

  DBG (127, "sane_start called, handle=%lx\n", (u_long) handle);

  if (handle != MAGIC || !is_open ||
      (dc25_opt_image_number == 0 && dc25_opt_snap == SANE_FALSE))
    return SANE_STATUS_INVAL;	/* Unknown handle ... */

  if (started)
    {
      return SANE_STATUS_EOF;
    }

  if (dc25_opt_snap)
    {

      /*
       * Don't allow picture unless there is room in the 
       * camera.
       */
      if (CameraInfo.pic_left == 0)
	{
	  DBG (3, "No room to store new picture\n");
	  return SANE_STATUS_INVAL;
	}

      /* 
       * DC-20 can only change resolution when camer is empty.
       * DC-25 can do it any time.
       */
      if (CameraInfo.model != 0x20 || CameraInfo.pic_taken == 0)
	{
	  if (change_res (tfd, dc25_opt_lowres) == -1)
	    {
	      DBG (1, "Failed to set resolution\n");
	      return SANE_STATUS_INVAL;
	    }
	}

      /*
       * Not sure why this delay is needed, but it seems to help:
       */
#ifdef HAVE_USLEEP
      usleep (10);
#else
      sleep (1);
#endif
      if (shoot (tfd) == -1)
	{
	  DBG (1, "Failed to snap new picture\n");
	  return SANE_STATUS_INVAL;
	}
      else
	{
	  info_flags |= SANE_INFO_RELOAD_OPTIONS;
	  CameraInfo.pic_taken++;
	  CameraInfo.pic_left--;
	  dc25_opt_image_number = CameraInfo.pic_taken;
	  if (image_range.min == 0)
	    image_range.min = 1;
	  image_range.max++;
	  sod[DC25_OPT_IMAGE_NUMBER].cap &= ~SANE_CAP_INACTIVE;
	}
    }

  if (dc25_opt_thumbnails)
    {

      /*
       * For thumbnails, we can do things right where we
       * start the download, and grab the first block 
       * from the camera.  The reamining blocks will be
       * fetched as necessary by sane_read().
       */
      thumb_pck[3] = (unsigned char) dc25_opt_image_number;

      if (send_pck (tfd, thumb_pck) == -1)
	{
	  DBG (4, "sane_start: error: send_pck returned -1\n");
	  return SANE_STATUS_INVAL;
	}

      if (read_data (tfd, buffer, 1024) == -1)
	{
	  DBG (4, "sane_start: read_data failed\n");
	  return SANE_STATUS_INVAL;
	}

      /* 
       * DC20 thumbnail are 80x60 grayscale, DC25
       * thumbnails are color.
       */
      parms.format =
	(CameraInfo.model == 0x25) ? SANE_FRAME_RGB : SANE_FRAME_GRAY;
      parms.bytes_per_line = 80 * 3;	/* 80 pixels, 3 colors */
      parms.pixels_per_line = 80;
      parms.lines = 60;

      bytes_in_buffer = 1024;
      bytes_read_from_buffer = 0;

    }
  else
    {
      /*
       * We do something a little messy, and violates the SANE
       * philosophy.  However, since it is fairly tricky to 
       * convert the DC2x "comet" files on the fly, we read in
       * the entire data stream in sane_open(), and use convert_pic
       * to convert it to an in-memory pixpmap.  Then when 
       * sane_read() is called, we fill the requests from
       * memory.  A good project for me (or some kind volunteer)
       * would be to rewrite this and move the actual download
       * to sane_read().  However, one argument for keeping it
       * this way is that the data comes down pretty fast, and
       * it helps to dedicate the processor to this task.  We
       * might get serial port overruns if we try to do other
       * things at the same time.  
       *
       * Also, as a side note, I was constantly getting serial
       * port overruns on a 90MHz pentium until I used hdparm
       * to set the "-u1" flag on the system drives.
       */
      int fd;

      fd = open (tmpname, O_CREAT | O_EXCL | O_WRONLY, 0600);
      if (fd == -1)
	{
	  DBG (0, "Unable to open tmp file\n");
	  return SANE_STATUS_INVAL;
	}
      f = fdopen (fd, "wb");
      if (f == NULL)
	{
	  DBG (0, "Unable to fdopen tmp file\n");
	  return SANE_STATUS_INVAL;
	}

      strcpy ((char *) buffer, COMET_MAGIC);
      fwrite (buffer, 1, COMET_HEADER_SIZE, f);

      pic_pck[3] = (unsigned char) dc25_opt_image_number;

      if (send_pck (tfd, pic_pck) == -1)
	{
	  DBG (4, "sane_start: error: send_pck returned -1\n");
	  return SANE_STATUS_INVAL;
	}

      if (read_data (tfd, buffer, 1024) == -1)
	{
	  DBG (5, "sane_start: read_data failed\n");
	  return SANE_STATUS_INVAL;
	}

      if (buffer[4] == 0)
	{			/* hi-res image */
	  DBG (5, "sane_start: hi-res image\n");
	  n = 122;

	  parms.bytes_per_line = 500 * 3;	/* 3 colors */
	  parms.pixels_per_line = 500;
	  parms.lines = 373;

	  bytes_in_buffer = 1024;
	  bytes_read_from_buffer = 0;
	}
      else
	{
	  n = 61;
	  DBG (5, "sane_start: low-res image\n");

	  parms.bytes_per_line = 320 * 3;	/* 3 Colors */
	  parms.pixels_per_line = 320;
	  parms.lines = 243;

	  bytes_in_buffer = 1024;
	  bytes_read_from_buffer = 0;
	}


      fwrite (buffer, 1, 1024, f);

      for (i = 1; i < n; i++)
	{
	  if (read_data (tfd, buffer, 1024) == -1)
	    {
	      DBG (5, "sane_start: read_data failed\n");
	      return SANE_STATUS_INVAL;
	    }
	  fwrite (buffer, 1, 1024, f);
	}

      if (end_of_data (tfd) == -1)
	{
	  fclose (f);
	  DBG (4, "sane_open: end_of_data error\n");
	  return SANE_STATUS_INVAL;
	}
      else
	{
	  fclose (f);
	  if (convert_pic (tmpname, SAVE_ADJASPECT | SAVE_24BITS) == -1)
	    {
	      DBG (3, "sane_open: unable to convert\n");
	      return SANE_STATUS_INVAL;
	    }
	  unlink (tmpname);
	  outbytes = 0;
	}
    }

  started = SANE_TRUE;
  total_bytes_read = 0;

  return SANE_STATUS_GOOD;
}


SANE_Status
sane_read (SANE_Handle UNUSEDARG handle, SANE_Byte * data,
	   SANE_Int max_length, SANE_Int * length)
{
  DBG (127, "sane_read called, maxlen=%d\n", max_length);

  if ( ! started ) {
	return SANE_STATUS_INVAL;
  }

  if (dc25_opt_thumbnails)
    {
      if (total_bytes_read == THUMBSIZE)
	{
	  if (dc25_opt_erase || dc25_opt_erase_one)
	    {

	      if (erase (tfd) == -1)
		{
		  DBG (1, "Failed to erase memory\n");
		  return SANE_STATUS_INVAL;
		}

	      dc25_opt_erase = SANE_FALSE;
	      dc25_opt_erase_one = SANE_FALSE;
	      info_flags |= SANE_INFO_RELOAD_OPTIONS;

	      if (get_info (tfd) == NULL)
		{
		  DBG (2, "error: could not get info\n");
		  close_dc20 (tfd);
		  return SANE_STATUS_INVAL;
		}
	      DBG (10, "Call get_info!, image range=%d,%d\n", image_range.min,
		   image_range.max);
	    }
	  return SANE_STATUS_EOF;
	}

      *length = 0;
      if (!(bytes_in_buffer - bytes_read_from_buffer))
	{
	  if (read_data (tfd, buffer, 1024) == -1)
	    {
	      DBG (5, "sane_read: read_data failed\n");
	      return SANE_STATUS_INVAL;
	    }
	  bytes_in_buffer = 1024;
	  bytes_read_from_buffer = 0;
	}

      while (bytes_read_from_buffer < bytes_in_buffer &&
	     max_length && total_bytes_read < THUMBSIZE)
	{
	  *data++ = buffer[bytes_read_from_buffer++];
	  (*length)++;
	  max_length--;
	  total_bytes_read++;
	}

      if (total_bytes_read == THUMBSIZE)
	{
	  if (end_of_data (tfd) == -1)
	    {
	      DBG (4, "sane_read: end_of_data error\n");
	      return SANE_STATUS_INVAL;
	    }
	  else
	    {
	      return SANE_STATUS_GOOD;
	    }
	}
      else
	{
	  return SANE_STATUS_GOOD;
	}
    }
  else
    {
      int i;
      int filesize = parms.bytes_per_line * parms.lines;

      /* 
       * If outbytes is zero, then this is the first time
       * we've been called, so update the contrast table.
       * The formula is something I came up with that has the
       * following prooperties:
       * 1) It's a smooth curve that provides the effect I wanted
       *    (bright pixels are made brighter, dim pixels are made
       *    dimmer)
       * 2) The contrast parameter can be adjusted to provide 
       *    different amounts of contrast.
       * 3) A parameter of 1.0 can be used to pass the data
       *    through unchanged (but values around 1.75 look 
       *    a lot better
       */
      if (outbytes == 0)
	{
	  double d;
	  double cont = SANE_UNFIX (dc25_opt_contrast);

	  for (i = 0; i < 256; i++)
	    {
	      d = (i * 2.0) / 255 - 1.0;
	      d =
		((-pow (1 - d, cont)) + 1) * (d >=
					      0) + (((pow (d + 1, cont)) -
						     1)) * (d < 0);
	      contrast_table[i] = (d * 127.5) + 127.5;
/*
				fprintf (stderr,"%03d %03d\n",i,contrast_table[i]);
*/
	    }
	}

      /* We're done, so return EOF */
      if (outbytes >= filesize)
	{
	  free_pixmap (pp);
	  pp = NULL;

	  if (dc25_opt_erase || dc25_opt_erase_one)
	    {
	      if (erase (tfd) == -1)
		{
		  DBG (1, "Failed to erase memory\n");
		  return SANE_STATUS_INVAL;
		}
	    }

	  if (get_info (tfd) == NULL)
	    {
	      DBG (2, "error: could not get info\n");
	      close_dc20 (tfd);
	      return SANE_STATUS_INVAL;
	    }
	  DBG (10, "Call get_info!, image range=%d,%d\n", image_range.min,
	       image_range.max);

	  get_info (tfd);

          *length=0;

	  return SANE_STATUS_EOF;
	}

      if (max_length > filesize - outbytes)
	{
	  *length = filesize - outbytes;
	}
      else
	{
	  *length = max_length;
	}

      memcpy (data, pp->planes + outbytes, *length);
      outbytes += *length;


      for (i = 0; i < *length; i++)
	{
	  data[i] = contrast_table[data[i]];
	}

      return SANE_STATUS_GOOD;

    }
}

void
sane_cancel (SANE_Handle UNUSEDARG handle)
{
  DBG (127, "sane_cancel() called\n");
  started = SANE_FALSE;
}

SANE_Status
sane_set_io_mode (SANE_Handle UNUSEDARG handle,
		  SANE_Bool UNUSEDARG non_blocking)
{
  /* sane_set_io_mode() is only valid during a scan */
  if (started)
    {
      if (non_blocking == SANE_FALSE)
	{
	  return SANE_STATUS_GOOD;
	}
      else
	{
	  return SANE_STATUS_UNSUPPORTED;
	}
    }
  else
    {
      /* We aren't currently scanning */
      return SANE_STATUS_INVAL;
    }
}

SANE_Status
sane_get_select_fd (SANE_Handle UNUSEDARG handle, SANE_Int UNUSEDARG * fd)
{
  return SANE_STATUS_UNSUPPORTED;
}
