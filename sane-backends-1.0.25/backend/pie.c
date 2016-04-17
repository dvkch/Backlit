/* sane - Scanner Access Now Easy.

   pie.c

   Copyright (C) 2000 Simon Munton, based on the umax backend by Oliver Rauch

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
   If you do not wish that, delete this exception notice.  */

/*
 * 22-2-2003 set devlist to NULL in sane_exit()
 *           set first_dev to NULL in sane_exit()
 *           eliminated num_devices
 *
 * 23-7-2002 added TL_X > BR_X, TL_Y > BR_Y check in sane_start
 *
 * 17-9-2001 changed ADLIB to AdLib as the comparison is case sensitive and
 * 	     the scanner returns AdLib
 *
 * 7-5-2001 removed removal of '\n' after sanei_config_read()
 *	    free devlist allocated in sane_get_devices() on sane_exit()
 *
 * 2-3-2001 improved the reordering of RGB data in pie_reader_process()
 *
 * 11-11-2000 eliminated some warnings about signed/unsigned comparisons
 *            removed #undef NDEBUG and C++ style comments
 *
 * 1-10-2000 force gamma table to one to one mappping if lineart or halftone selected
 *
 * 30-9-2000 added ADLIB devices to scanner_str[]
 *
 * 29-9-2000 wasn't setting 'background is halftone bit' (BGHT) in halftone mode
 *
 * 27-9-2000 went public with build 4
 */

#include "../include/sane/config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_debug.h"

#define BACKEND_NAME	pie
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"

# include "../include/sane/sanei_thread.h"

#include "pie-scsidef.h"

#define DBG_error0  0
#define DBG_error   1
#define DBG_sense   2
#define DBG_warning 3
#define DBG_inquiry 4

#define DBG_info    5
#define DBG_info2   6
#define DBG_proc    7
#define DBG_read    8
#define DBG_sane_init   10
#define DBG_sane_proc   11
#define DBG_sane_info   12
#define DBG_sane_option 13
#define DBG_dump	14

#define BUILD 9

#define PIE_CONFIG_FILE "pie.conf"

#define LINEART_STR         SANE_VALUE_SCAN_MODE_LINEART
#define HALFTONE_STR        SANE_VALUE_SCAN_MODE_HALFTONE
#define GRAY_STR            SANE_VALUE_SCAN_MODE_GRAY
#define COLOR_STR           SANE_VALUE_SCAN_MODE_COLOR

#define LINEART             1
#define HALFTONE            2
#define GRAYSCALE           3
#define RGB                 4

#define CAL_MODE_PREVIEW        (INQ_CAP_FAST_PREVIEW)
#define CAL_MODE_FLATBED        0x00
#define CAL_MODE_ADF            (INQ_OPT_DEV_ADF)
#define CAL_MODE_TRANPSARENCY   (INQ_OPT_DEV_TP)
#define CAL_MODE_TRANPSARENCY1  (INQ_OPT_DEV_TP1)

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))


/* names of scanners that are supported because */
/* the inquiry_return_block is ok and driver is tested */

static char *scanner_str[] = {
  "DEVCOM", "9636PRO",
  "DEVCOM", "9636S",
  "DEVCOM", "9630S",
  "PIE", "ScanAce 1236S",
  "PIE", "ScanAce 1230S",
  "PIE", "ScanAce II",
  "PIE", "ScanAce III",
  "PIE", "ScanAce Plus",
  "PIE", "ScanAce II Plus",
  "PIE", "ScanAce III Plus",
  "PIE", "ScanAce V",
  "PIE", "ScanMedia",
  "PIE", "ScanMedia II",
  "PIE", "ScanAce 630S",
  "PIE", "ScanAce 636S",
  "AdLib", "JetScan 630",
  "AdLib", "JetScan 636PRO",
  "END_OF_LIST"
};

/* times (in us) to delay after certain commands. Scanner seems to lock up if it returns busy
 * status and commands are repeatedly reissued (by kernel error handler) */

#define DOWNLOAD_GAMMA_WAIT_TIME	(1000000)
#define SCAN_WAIT_TIME			(1000000)
#define SCAN_WARMUP_WAIT_TIME		(500000)
#define TUR_WAIT_TIME			(500000)


/* options supported by the scanner */

enum Pie_Option
{
  OPT_NUM_OPTS = 0,

  /* ------------------------------------------- */
  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_RESOLUTION,


  /* ------------------------------------------- */

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  /* ------------------------------------------- */

  OPT_ENHANCEMENT_GROUP,

  OPT_HALFTONE_PATTERN,
  OPT_SPEED,
  OPT_THRESHOLD,

  OPT_GAMMA_VECTOR,
  OPT_GAMMA_VECTOR_R,
  OPT_GAMMA_VECTOR_G,
  OPT_GAMMA_VECTOR_B,

  /* ------------------------------------------- */

  OPT_ADVANCED_GROUP,
  OPT_PREVIEW,

  /* must come last: */
  NUM_OPTIONS
};




/* This defines the information needed during calibration */

struct Pie_cal_info
{
  int cal_type;
  int receive_bits;
  int send_bits;
  int num_lines;
  int pixels_per_line;
};


/* This structure holds the information about a physical scanner */

typedef struct Pie_Device
{
  struct Pie_Device *next;

  char *devicename;		/* name of the scanner device */

  char vendor[9];		/* will be xxxxx */
  char product[17];		/* e.g. "SuperVista_S12" or so */
  char version[5];		/* e.g. V1.3 */

  SANE_Device sane;
  SANE_Range dpi_range;
  SANE_Range x_range;
  SANE_Range y_range;

  SANE_Range exposure_range;
  SANE_Range shadow_range;
  SANE_Range highlight_range;

  int inquiry_len;		/* length of inquiry return block */

  int inquiry_x_res;		/* maximum x-resolution */
  int inquiry_y_res;		/* maximum y-resolution */
  int inquiry_pixel_resolution;
  double inquiry_fb_width;	/* flatbed width in inches */
  double inquiry_fb_length;	/* flatbed length in inches */

  int inquiry_trans_top_left_x;
  int inquiry_trans_top_left_y;
  double inquiry_trans_width;	/* transparency width in inches */
  double inquiry_trans_length;	/* transparency length in inches */

  int inquiry_halftones;	/* number of halftones supported */
  int inquiry_filters;		/* available colour filters */
  int inquiry_color_depths;	/* available colour depths */
  int inquiry_color_format;	/* colour format from scanner */
  int inquiry_image_format;	/* image data format */
  int inquiry_scan_capability;	/* additional scanner features, number of speeds */
  int inquiry_optional_devices;	/* optional devices */
  int inquiry_enhancements;	/* enhancements */
  int inquiry_gamma_bits;	/* no of bits used for gamma table */
  int inquiry_fast_preview_res;	/* fast preview resolution */
  int inquiry_min_highlight;	/* min highlight % that can be used */
  int inquiry_max_shadow;	/* max shadow % that can be used */
  int inquiry_cal_eqn;		/* which calibration equation to use */
  int inquiry_min_exp;		/* min exposure % */
  int inquiry_max_exp;		/* max exposure % */

  SANE_String scan_mode_list[7];	/* holds names of types of scan (color, ...) */

  SANE_String halftone_list[17];	/* holds the names of the halftone patterns from the scanner */

  SANE_String speed_list[9];	/* holds the names of available speeds */

  int cal_info_count;		/* number of calibration info sets */
  struct Pie_cal_info *cal_info;	/* points to the actual calibration information */
}
Pie_Device;

/* This structure holds information about an instance of an 'opened' scanner */

typedef struct Pie_Scanner
{
  struct Pie_Scanner *next;
  Pie_Device *device;		/* pointer to physical scanner */

  int sfd;			/* scanner file desc. */
  int bufsize;			/* max scsi buffer size */

  SANE_Option_Descriptor opt[NUM_OPTIONS];	/* option descriptions for this instance */
  Option_Value val[NUM_OPTIONS];	/* option settings for this instance */
  SANE_Int *gamma_table[4];	/* gamma tables for this instance */
  SANE_Range gamma_range;
  int gamma_length;		/* size of gamma table */

  int scanning;			/* true if actually doing a scan */
  SANE_Parameters params;

  SANE_Pid reader_pid;
  int pipe;
  int reader_fds;
  
  int colormode;		/* whether RGB, GRAY, LINEART, HALFTONE */
  int resolution;
  int cal_mode;			/* set to value to compare cal_info mode to */

  int cal_filter;		/* set to indicate which filters will provide data for cal */

  int filter_offset1;		/* offsets between colors in indexed scan mode */
  int filter_offset2;

  int bytes_per_line;		/* number of bytes per line */

}
Pie_Scanner;

static const SANE_Range percentage_range_100 = {
  0 << SANE_FIXED_SCALE_SHIFT,	/* minimum */
  100 << SANE_FIXED_SCALE_SHIFT,	/* maximum */
  0 << SANE_FIXED_SCALE_SHIFT	/* quantization */
};

static Pie_Device *first_dev = NULL;
static Pie_Scanner *first_handle = NULL;
static const SANE_Device **devlist = NULL;



static SANE_Status pie_wait_scanner (Pie_Scanner * scanner);


/* ---------------------------------- PIE DUMP_BUFFER ---------------------------------- */

#define DBG_DUMP(level, buf, n)	{ if (DBG_LEVEL >= (level)) pie_dump_buffer(level,buf,n); }


static void
pie_dump_buffer (int level, unsigned char *buf, int n)
{
  char s[80], *p = s;
  int a = 0;

  while (n--)
    {
      if ((a % 16) == 0)
	p += sprintf (p, "  %04X  ", a);

      p += sprintf (p, "%02X ", *buf++);

      if ((n == 0) || (a % 16) == 15)
	{
	  DBG (level, "%s\n", s);
	  p = s;
	}
      a++;
    }
}

/* ---------------------------------- PIE INIT ---------------------------------- */

static void
pie_init (Pie_Device * dev)	/* pie_init is called once while driver-initialization */
{
  DBG (DBG_proc, "init\n");

  dev->cal_info_count = 0;
  dev->cal_info = NULL;

  dev->devicename = NULL;
  dev->inquiry_len = 0;

#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
  DBG (DBG_info,
       "variable scsi buffer size (usage of sanei_scsi_open_extended)\n");
#else
  DBG (DBG_info, "fixed scsi buffer size = %d bytes\n",
       sanei_scsi_max_request_size);
#endif
}


/* ---------------------------- SENSE_HANDLER ------------------------------ */


static SANE_Status
sense_handler (__sane_unused__ int scsi_fd, unsigned char *result, __sane_unused__ void *arg)	/* is called by sanei_scsi */
{
  unsigned char asc, ascq, sensekey;
  int asc_ascq, len;
  /* Pie_Device *dev = arg; */

  DBG (DBG_proc, "check condition sense handler\n");

  sensekey = get_RS_sense_key (result);
  asc = get_RS_ASC (result);
  ascq = get_RS_ASCQ (result);
  asc_ascq = (int) (256 * asc + ascq);
  len = 7 + get_RS_additional_length (result);

  if (get_RS_error_code (result) != 0x70)
    {
      DBG (DBG_proc, "invalid sense key => handled as DEVICE BUSY!\n");
      return SANE_STATUS_DEVICE_BUSY;	/* sense key invalid */
    }

  DBG (DBG_sense, "check condition sense: %s\n", sense_str[sensekey]);

  if (get_RS_ILI (result) != 0)
    {
      DBG (DBG_sense,
	   "-> ILI-ERROR: requested data length is larger than actual length\n");
    }

  switch (sensekey)
    {
    case 0x00:			/* no sense, could have been busy */
      return SANE_STATUS_IO_ERROR;
      break;

    case 0x02:
      if (asc_ascq == 0x0401)
	DBG (DBG_sense, "-> Not Ready - Warming Up\n");
      else if (asc_ascq == 0x0483)
	DBG (DBG_sense, "-> Not Ready - Need manual service\n");
      else if (asc_ascq == 0x0881)
	DBG (DBG_sense, "-> Not Ready - Communication time out\n");
      else
	DBG (DBG_sense, "-> unknown medium error: asc=%d, ascq=%d\n", asc,
	     ascq);
      break;

    case 0x03:			/* medium error */
      if (asc_ascq == 0x5300)
	DBG (DBG_sense, "-> Media load or eject failure\n");
      else if (asc_ascq == 0x3a00)
	DBG (DBG_sense, "-> Media not present\n");
      else if (asc_ascq == 0x3b05)
	DBG (DBG_sense, "-> Paper jam\n");
      else if (asc_ascq == 0x3a80)
	DBG (DBG_sense, "-> ADF paper out\n");
      else
	DBG (DBG_sense, "-> unknown medium error: asc=%d, ascq=%d\n", asc,
	     ascq);
      break;


    case 0x04:			/* hardware error */
      if (asc_ascq == 0x4081)
	DBG (DBG_sense, "-> CPU RAM failure\n");
      else if (asc_ascq == 0x4082)
	DBG (DBG_sense, "-> Scanning system RAM failure\n");
      else if (asc_ascq == 0x4083)
	DBG (DBG_sense, "-> Image buffer failure\n");
      else if (asc_ascq == 0x0403)
	DBG (DBG_sense, "-> Manual intervention required\n");
      else if (asc_ascq == 0x6200)
	DBG (DBG_sense, "-> Scan head position error\n");
      else if (asc_ascq == 0x6000)
	DBG (DBG_sense, "-> Lamp or CCD failure\n");
      else if (asc_ascq == 0x6081)
	DBG (DBG_sense, "-> Transparency lamp failure\n");
      else if (asc_ascq == 0x8180)
	DBG (DBG_sense, "-> DC offset or black level calibration failure\n");
      else if (asc_ascq == 0x8181)
	DBG (DBG_sense,
	     "-> Integration time adjustment failure (too light)\n");
      else if (asc_ascq == 0x8182)
	DBG (DBG_sense,
	     "-> Integration time adjustment failure (too dark)\n");
      else if (asc_ascq == 0x8183)
	DBG (DBG_sense, "-> Shading curve adjustment failure\n");
      else if (asc_ascq == 0x8184)
	DBG (DBG_sense, "-> Gain adjustment failure\n");
      else if (asc_ascq == 0x8185)
	DBG (DBG_sense, "-> Optical alignment failure\n");
      else if (asc_ascq == 0x8186)
	DBG (DBG_sense, "-> Optical locating failure\n");
      else if (asc_ascq == 0x8187)
	DBG (DBG_sense, "-> Scan pixel map less than 5100 pixels!\n");
      else if (asc_ascq == 0x4700)
	DBG (DBG_sense, "-> Parity error on SCSI bus\n");
      else if (asc_ascq == 0x4b00)
	DBG (DBG_sense, "-> Data phase error\n");
      else
	DBG (DBG_sense, "-> unknown hardware error: asc=%d, ascq=%d\n", asc,
	     ascq);
      return SANE_STATUS_IO_ERROR;
      break;


    case 0x05:			/* illegal request */
      if (asc_ascq == 0x1a00)
	DBG (DBG_sense, "-> Parameter list length error\n");
      else if (asc_ascq == 0x2c01)
	DBG (DBG_sense, "-> Too many windows specified\n");
      else if (asc_ascq == 0x2c02)
	DBG (DBG_sense, "-> Invalid combination of windows\n");
      else if (asc_ascq == 0x2c81)
	DBG (DBG_sense, "-> Illegal scanning frame\n");
      else if (asc_ascq == 0x2400)
	DBG (DBG_sense, "-> Invalid field in CDB\n");
      else if (asc_ascq == 0x2481)
	DBG (DBG_sense, "-> Request too many lines of data\n");
      else if (asc_ascq == 0x2000)
	DBG (DBG_sense, "-> Invalid command OP code\n");
      else if (asc_ascq == 0x2501)
	DBG (DBG_sense, "-> LUN not supported\n");
      else if (asc_ascq == 0x2601)
	DBG (DBG_sense, "-> Parameter not supported\n");
      else if (asc_ascq == 0x2602)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Parameter not specified\n");
      else if (asc_ascq == 0x2603)
	DBG (DBG_sense, "-> Parameter value invalid - Invalid threshold\n");
      else if (asc_ascq == 0x2680)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Control command sequence error\n");
      else if (asc_ascq == 0x2681)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Grain setting (halftone pattern\n");
      else if (asc_ascq == 0x2682)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Illegal resolution setting\n");
      else if (asc_ascq == 0x2683)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Invalid filter assignment\n");
      else if (asc_ascq == 0x2684)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Illegal gamma adjustment setting (look-up table)\n");
      else if (asc_ascq == 0x2685)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Illegal offset setting (digital brightness)\n");
      else if (asc_ascq == 0x2686)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Illegal bits per pixel setting\n");
      else if (asc_ascq == 0x2687)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Illegal contrast setting\n");
      else if (asc_ascq == 0x2688)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Illegal paper length setting\n");
      else if (asc_ascq == 0x2689)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Illegal highlight/shadow setting\n");
      else if (asc_ascq == 0x268a)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Illegal exposure time setting (analog brightness)\n");
      else if (asc_ascq == 0x268b)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Invalid device select or device not exist\n");
      else if (asc_ascq == 0x268c)
	DBG (DBG_sense,
	     "-> Parameter value invalid - Illegal color packing\n");
      else if (asc_ascq == 0x3d00)
	DBG (DBG_sense, "-> Invalid bits in identify field\n");



      else if (asc_ascq == 0x4900)
	DBG (DBG_sense, "-> Invalid message\n");
      else if (asc_ascq == 0x8101)
	DBG (DBG_sense, "-> Not enough memory for color packing\n");

      if (len >= 0x11)
	{
	  if (get_RS_SKSV (result) != 0)
	    {
	      if (get_RS_CD (result) == 0)
		{

		  DBG (DBG_sense, "-> illegal parameter in CDB\n");
		}
	      else
		{
		  DBG (DBG_sense,
		       "-> illegal parameter is in the data parameters sent during data out phase\n");
		}

	      DBG (DBG_sense, "-> error detected in byte %d\n",
		   get_RS_field_pointer (result));
	    }
	}
      return SANE_STATUS_IO_ERROR;
      break;


    case 0x06:			/* unit attention */
      if (asc_ascq == 0x2900)
	DBG (DBG_sense, "-> power on, reset or bus device reset\n");
      if (asc_ascq == 0x8200)
	DBG (DBG_sense,
	     "-> unit attention - calibration disable not granted\n");
      if (asc_ascq == 0x8300)
	DBG (DBG_sense, "-> unit attention - calibration will be ignored\n");
      else
	DBG (DBG_sense, "-> unit attention: asc=%d, ascq=%d\n", asc, ascq);
      break;


    case 0x09:			/* vendor specific */
      DBG (DBG_sense, "-> vendor specific sense-code: asc=%d, ascq=%d\n", asc,
	   ascq);
      break;

    case 0x0b:
      if (asc_ascq == 0x0006)
	DBG (DBG_sense, "-> Received ABORT message from initiator\n");
      if (asc_ascq == 0x4800)
	DBG (DBG_sense, "-> Initiator detected error message received\n");
      if (asc_ascq == 0x4300)
	DBG (DBG_sense, "-> Message error\n");
      if (asc_ascq == 0x4500)
	DBG (DBG_sense, "-> Select or re-select error\n");
      else
	DBG (DBG_sense, "-> aborted command: asc=%d, ascq=%d\n", asc, ascq);
      break;

    }

  return SANE_STATUS_IO_ERROR;
}


/* -------------------------------- PIE PRINT INQUIRY ------------------------- */


static void
pie_print_inquiry (Pie_Device * dev)
{
  DBG (DBG_inquiry, "INQUIRY:\n");
  DBG (DBG_inquiry, "========\n");
  DBG (DBG_inquiry, "\n");
  DBG (DBG_inquiry, "vendor........................: '%s'\n", dev->vendor);
  DBG (DBG_inquiry, "product.......................: '%s'\n", dev->product);
  DBG (DBG_inquiry, "version.......................: '%s'\n", dev->version);

  DBG (DBG_inquiry, "X resolution..................: %d dpi\n",
       dev->inquiry_x_res);
  DBG (DBG_inquiry, "Y resolution..................: %d dpi\n",
       dev->inquiry_y_res);
  DBG (DBG_inquiry, "pixel resolution..............: %d dpi\n",
       dev->inquiry_pixel_resolution);
  DBG (DBG_inquiry, "fb width......................: %f in\n",
       dev->inquiry_fb_width);
  DBG (DBG_inquiry, "fb length.....................: %f in\n",
       dev->inquiry_fb_length);

  DBG (DBG_inquiry, "transparency width............: %f in\n",
       dev->inquiry_trans_width);
  DBG (DBG_inquiry, "transparency length...........: %f in\n",
       dev->inquiry_trans_length);
  DBG (DBG_inquiry, "transparency offset...........: %d,%d\n",
       dev->inquiry_trans_top_left_x, dev->inquiry_trans_top_left_y);

  DBG (DBG_inquiry, "# of halftones................: %d\n",
       dev->inquiry_halftones);

  DBG (DBG_inquiry, "One pass color................: %s\n",
       dev->inquiry_filters & INQ_ONE_PASS_COLOR ? "yes" : "no");

  DBG (DBG_inquiry, "Filters.......................: %s%s%s%s (%02x)\n",
       dev->inquiry_filters & INQ_FILTER_RED ? "Red " : "",
       dev->inquiry_filters & INQ_FILTER_GREEN ? "Green " : "",
       dev->inquiry_filters & INQ_FILTER_BLUE ? "Blue " : "",
       dev->inquiry_filters & INQ_FILTER_NEUTRAL ? "Neutral " : "",
       dev->inquiry_filters);

  DBG (DBG_inquiry, "Color depths..................: %s%s%s%s%s%s (%02x)\n",
       dev->inquiry_color_depths & INQ_COLOR_DEPTH_16 ? "16 bit " : "",
       dev->inquiry_color_depths & INQ_COLOR_DEPTH_12 ? "12 bit " : "",
       dev->inquiry_color_depths & INQ_COLOR_DEPTH_10 ? "10 bit " : "",
       dev->inquiry_color_depths & INQ_COLOR_DEPTH_8 ? "8 bit " : "",
       dev->inquiry_color_depths & INQ_COLOR_DEPTH_4 ? "4 bit " : "",
       dev->inquiry_color_depths & INQ_COLOR_DEPTH_1 ? "1 bit " : "",
       dev->inquiry_color_depths);

  DBG (DBG_inquiry, "Color Format..................: %s%s%s (%02x)\n",
       dev->inquiry_color_format & INQ_COLOR_FORMAT_INDEX ? "Indexed " : "",
       dev->inquiry_color_format & INQ_COLOR_FORMAT_LINE ? "Line " : "",
       dev->inquiry_color_format & INQ_COLOR_FORMAT_PIXEL ? "Pixel " : "",
       dev->inquiry_color_format);

  DBG (DBG_inquiry, "Image Format..................: %s%s%s%s (%02x)\n",
       dev->inquiry_image_format & INQ_IMG_FMT_OKLINE ? "OKLine " : "",
       dev->inquiry_image_format & INQ_IMG_FMT_BLK_ONE ? "BlackOne " : "",
       dev->inquiry_image_format & INQ_IMG_FMT_MOTOROLA ? "Motorola " : "",
       dev->inquiry_image_format & INQ_IMG_FMT_INTEL ? "Intel" : "",
       dev->inquiry_image_format);

  DBG (DBG_inquiry,
       "Scan Capability...............: %s%s%s%s%d speeds (%02x)\n",
       dev->inquiry_scan_capability & INQ_CAP_PWRSAV ? "PowerSave " : "",
       dev->inquiry_scan_capability & INQ_CAP_EXT_CAL ? "ExtCal " : "",
       dev->inquiry_scan_capability & INQ_CAP_FAST_PREVIEW ? "FastPreview" :
       "",
       dev->inquiry_scan_capability & INQ_CAP_DISABLE_CAL ? "DisCal " : "",
       dev->inquiry_scan_capability & INQ_CAP_SPEEDS,
       dev->inquiry_scan_capability);

  DBG (DBG_inquiry, "Optional Devices..............: %s%s%s%s (%02x)\n",
       dev->inquiry_optional_devices & INQ_OPT_DEV_MPCL ? "MultiPageLoad " :
       "",
       dev->inquiry_optional_devices & INQ_OPT_DEV_TP1 ? "TransModule1 " : "",
       dev->inquiry_optional_devices & INQ_OPT_DEV_TP ? "TransModule " : "",
       dev->inquiry_optional_devices & INQ_OPT_DEV_ADF ? "ADF " : "",
       dev->inquiry_optional_devices);

  DBG (DBG_inquiry, "Enhancement...................: %02x\n",
       dev->inquiry_enhancements);
  DBG (DBG_inquiry, "Gamma bits....................: %d\n",
       dev->inquiry_gamma_bits);

  DBG (DBG_inquiry, "Fast Preview Resolution.......: %d\n",
       dev->inquiry_fast_preview_res);
  DBG (DBG_inquiry, "Min Highlight.................: %d\n",
       dev->inquiry_min_highlight);
  DBG (DBG_inquiry, "Max Shadow....................: %d\n",
       dev->inquiry_max_shadow);
  DBG (DBG_inquiry, "Cal Eqn.......................: %d\n",
       dev->inquiry_cal_eqn);
  DBG (DBG_inquiry, "Min Exposure..................: %d\n",
       dev->inquiry_min_exp);
  DBG (DBG_inquiry, "Max Exposure..................: %d\n",
       dev->inquiry_max_exp);
}


/* ------------------------------ PIE GET INQUIRY VALUES -------------------- */


static void
pie_get_inquiry_values (Pie_Device * dev, unsigned char *buffer)
{
  DBG (DBG_proc, "get_inquiry_values\n");

  dev->inquiry_len = get_inquiry_additional_length (buffer) + 5;

  get_inquiry_vendor ((char *) buffer, dev->vendor);
  dev->vendor[8] = '\0';
  get_inquiry_product ((char *) buffer, dev->product);
  dev->product[16] = '\0';
  get_inquiry_version ((char *) buffer, dev->version);
  dev->version[4] = '\0';

  dev->inquiry_x_res = get_inquiry_max_x_res (buffer);
  dev->inquiry_y_res = get_inquiry_max_y_res (buffer);

  if (dev->inquiry_y_res < 256)
    {
      /* y res is a multiplier */
      dev->inquiry_pixel_resolution = dev->inquiry_x_res;
      dev->inquiry_x_res *= dev->inquiry_y_res;
      dev->inquiry_y_res = dev->inquiry_x_res;
    }
  else
    {
      /* y res really is resolution */
      dev->inquiry_pixel_resolution =
	min (dev->inquiry_x_res, dev->inquiry_y_res);
    }

  dev->inquiry_fb_width =
    (double) get_inquiry_fb_max_scan_width (buffer) /
    dev->inquiry_pixel_resolution;
  dev->inquiry_fb_length =
    (double) get_inquiry_fb_max_scan_length (buffer) /
    dev->inquiry_pixel_resolution;

  dev->inquiry_trans_top_left_x = get_inquiry_trans_x1 (buffer);
  dev->inquiry_trans_top_left_y = get_inquiry_trans_y1 (buffer);

  dev->inquiry_trans_width =
    (double) (get_inquiry_trans_x2 (buffer) -
	      get_inquiry_trans_x1 (buffer)) / dev->inquiry_pixel_resolution;
  dev->inquiry_trans_length =
    (double) (get_inquiry_trans_y2 (buffer) -
	      get_inquiry_trans_y1 (buffer)) / dev->inquiry_pixel_resolution;

  dev->inquiry_halftones = get_inquiry_halftones (buffer) & 0x0f;

  dev->inquiry_filters = get_inquiry_filters (buffer);
  dev->inquiry_color_depths = get_inquiry_color_depths (buffer);
  dev->inquiry_color_format = get_inquiry_color_format (buffer);
  dev->inquiry_image_format = get_inquiry_image_format (buffer);

  dev->inquiry_scan_capability = get_inquiry_scan_capability (buffer);
  dev->inquiry_optional_devices = get_inquiry_optional_devices (buffer);
  dev->inquiry_enhancements = get_inquiry_enhancements (buffer);
  dev->inquiry_gamma_bits = get_inquiry_gamma_bits (buffer);
  dev->inquiry_fast_preview_res = get_inquiry_fast_preview_res (buffer);
  dev->inquiry_min_highlight = get_inquiry_min_highlight (buffer);
  dev->inquiry_max_shadow = get_inquiry_max_shadow (buffer);
  dev->inquiry_cal_eqn = get_inquiry_cal_eqn (buffer);
  dev->inquiry_min_exp = get_inquiry_min_exp (buffer);
  dev->inquiry_max_exp = get_inquiry_max_exp (buffer);

  pie_print_inquiry (dev);

  return;
}

/* ----------------------------- PIE DO INQUIRY ---------------------------- */


static void
pie_do_inquiry (int sfd, unsigned char *buffer)
{
  size_t size;
  SANE_Status status;

  DBG (DBG_proc, "do_inquiry\n");
  memset (buffer, '\0', 256);	/* clear buffer */

  size = 5;

  set_inquiry_return_size (inquiry.cmd, size);	/* first get only 5 bytes to get size of inquiry_return_block */
  status = sanei_scsi_cmd (sfd, inquiry.cmd, inquiry.size, buffer, &size);
  if (status)
    {
      DBG (DBG_error, "pie_do_inquiry: command returned status %s\n",
	   sane_strstatus (status));
    }

  size = get_inquiry_additional_length (buffer) + 5;

  set_inquiry_return_size (inquiry.cmd, size);	/* then get inquiry with actual size */
  status = sanei_scsi_cmd (sfd, inquiry.cmd, inquiry.size, buffer, &size);
  if (status)
    {
      DBG (DBG_error, "pie_do_inquiry: command returned status %s\n",
	   sane_strstatus (status));
    }
}

/* ---------------------- PIE IDENTIFY SCANNER ---------------------- */


static int
pie_identify_scanner (Pie_Device * dev, int sfd)
{
  char vendor[9];
  char product[0x11];
  char version[5];
  char *pp;
  int i = 0;
  unsigned char inquiry_block[256];

  DBG (DBG_proc, "identify_scanner\n");

  pie_do_inquiry (sfd, inquiry_block);	/* get inquiry */

  if (get_inquiry_periph_devtype (inquiry_block) != IN_periph_devtype_scanner)
    {
      return 1;
    }				/* no scanner */

  get_inquiry_vendor ((char *) inquiry_block, vendor);
  get_inquiry_product ((char *) inquiry_block, product);
  get_inquiry_version ((char *) inquiry_block, version);

  pp = &vendor[8];
  vendor[8] = ' ';
  while (*pp == ' ')
    {
      *pp-- = '\0';
    }

  pp = &product[0x10];
  product[0x10] = ' ';
  while (*pp == ' ')
    {
      *pp-- = '\0';
    }

  pp = &version[4];

  version[4] = ' ';
  while (*pp == ' ')
    {
      *pp-- = '\0';
    }

  DBG (DBG_info, "Found %s scanner %s version %s on device %s\n", vendor,
       product, version, dev->devicename);

  while (strncmp ("END_OF_LIST", scanner_str[2 * i], 11) != 0)	/* Now identify full supported scanners */
    {
      if (!strncmp (vendor, scanner_str[2 * i], strlen (scanner_str[2 * i])))
	{
	  if (!strncmp
	      (product, scanner_str[2 * i + 1],
	       strlen (scanner_str[2 * i + 1])))
	    {
	      DBG (DBG_info, "found supported scanner\n");

	      pie_get_inquiry_values (dev, inquiry_block);
	      return 0;
	    }
	}
      i++;
    }

  return 1;			/* NO SUPPORTED SCANNER: short inquiry-block and unknown scanner */
}


/* ------------------------------- GET SPEEDS ----------------------------- */

static void
pie_get_speeds (Pie_Device * dev)
{
  int speeds = dev->inquiry_scan_capability & INQ_CAP_SPEEDS;

  DBG (DBG_proc, "get_speeds\n");

  if (speeds == 3)
    {
      dev->speed_list[0] = strdup ("Normal");
      dev->speed_list[1] = strdup ("Fine");
      dev->speed_list[2] = strdup ("Pro");
      dev->speed_list[3] = NULL;
    }
  else
    {
      int i;
      char buf[2];

      buf[1] = '\0';

      for (i = 0; i < speeds; i++)
	{
	  buf[0] = '1' + i;
	  dev->speed_list[i] = strdup (buf);
	}

      dev->speed_list[i] = NULL;
    }
}

/* ------------------------------- GET HALFTONES ----------------------------- */

static void
pie_get_halftones (Pie_Device * dev, int sfd)
{
  int i;
  size_t size;
  SANE_Status status;
  unsigned char *data;
  unsigned char buffer[128];

  DBG (DBG_proc, "get_halftones\n");

  for (i = 0; i < dev->inquiry_halftones; i++)
    {
      size = 6;

      set_write_length (swrite.cmd, size);

      memcpy (buffer, swrite.cmd, swrite.size);

      data = buffer + swrite.size;
      memset (data, 0, size);

      set_command (data, READ_HALFTONE);
      set_data_length (data, 2);
      data[4] = i;

      status = sanei_scsi_cmd (sfd, buffer, swrite.size + size, NULL, NULL);
      if (status)
	{
	  DBG (DBG_error,
	       "pie_get_halftones: write command returned status %s\n",
	       sane_strstatus (status));
	}
      else
	{
	  /* now read the halftone data */
	  memset (buffer, '\0', sizeof buffer);	/* clear buffer */

	  size = 128;
	  set_read_length (sread.cmd, size);

	  DBG (DBG_info, "doing read\n");
	  status = sanei_scsi_cmd (sfd, sread.cmd, sread.size, buffer, &size);
	  if (status)
	    {
	      DBG (DBG_error,
		   "pie_get_halftones: read command returned status %s\n",
		   sane_strstatus (status));
	    }
	  else
	    {
	      unsigned char *s;

	      s = buffer + 8 + buffer[6] * buffer[7];

	      DBG (DBG_info, "halftone %d: %s\n", i, s);

	      dev->halftone_list[i] = strdup ((char *)s);
	    }
	}
    }
  dev->halftone_list[i] = NULL;
}

/* ------------------------------- GET CAL DATA ----------------------------- */

static void
pie_get_cal_info (Pie_Device * dev, int sfd)
{
  size_t size;
  SANE_Status status;
  unsigned char *data;
  unsigned char buffer[280];

  DBG (DBG_proc, "get_cal_info\n");

  if (!(dev->inquiry_scan_capability & INQ_CAP_EXT_CAL))
    return;

  size = 6;

  set_write_length (swrite.cmd, size);

  memcpy (buffer, swrite.cmd, swrite.size);

  data = buffer + swrite.size;
  memset (data, 0, size);

  set_command (data, READ_CAL_INFO);

  status = sanei_scsi_cmd (sfd, buffer, swrite.size + size, NULL, NULL);
  if (status)
    {
      DBG (DBG_error, "pie_get_cal_info: write command returned status %s\n",
	   sane_strstatus (status));
    }
  else
    {
      /* now read the cal data */
      memset (buffer, '\0', sizeof buffer);	/* clear buffer */

      size = 128;
      set_read_length (sread.cmd, size);

      DBG (DBG_info, "doing read\n");
      status = sanei_scsi_cmd (sfd, sread.cmd, sread.size, buffer, &size);
      if (status)
	{
	  DBG (DBG_error,
	       "pie_get_cal_info: read command returned status %s\n",
	       sane_strstatus (status));
	}
      else
	{
	  int i;

	  dev->cal_info_count = buffer[4];
	  dev->cal_info =
	    malloc (sizeof (struct Pie_cal_info) * dev->cal_info_count);

	  for (i = 0; i < dev->cal_info_count; i++)
	    {
	      dev->cal_info[i].cal_type = buffer[8 + i * buffer[5]];
	      dev->cal_info[i].send_bits = buffer[9 + i * buffer[5]];
	      dev->cal_info[i].receive_bits = buffer[10 + i * buffer[5]];
	      dev->cal_info[i].num_lines = buffer[11 + i * buffer[5]];
	      dev->cal_info[i].pixels_per_line =
		(buffer[13 + i * buffer[5]] << 8) + buffer[12 +
							   i * buffer[5]];

	      DBG (DBG_info2, "%02x %2d %2d %2d %d\n",
		   dev->cal_info[i].cal_type, dev->cal_info[i].send_bits,
		   dev->cal_info[i].receive_bits, dev->cal_info[i].num_lines,
		   dev->cal_info[i].pixels_per_line);
	    }
	}
    }
}

/* ------------------------------- ATTACH SCANNER ----------------------------- */

static SANE_Status
attach_scanner (const char *devicename, Pie_Device ** devp)
{
  Pie_Device *dev;
  int sfd;
  int bufsize;

  DBG (DBG_sane_proc, "attach_scanner: %s\n", devicename);

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devicename) == 0)
	{
	  if (devp)
	    {
	      *devp = dev;
	    }
	  return SANE_STATUS_GOOD;
	}
    }

  dev = malloc (sizeof (*dev));
  if (!dev)
    {
      return SANE_STATUS_NO_MEM;
    }

  DBG (DBG_info, "attach_scanner: opening %s\n", devicename);

#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
  bufsize = 16384;		/* 16KB */

  if (sanei_scsi_open_extended
      (devicename, &sfd, sense_handler, dev, &bufsize) != 0)
    {
      DBG (DBG_error, "attach_scanner: open failed\n");
      free (dev);
      return SANE_STATUS_INVAL;
    }

  if (bufsize < 4096)		/* < 4KB */
    {
      DBG (DBG_error,
	   "attach_scanner: sanei_scsi_open_extended returned too small scsi buffer (%d)\n",
	   bufsize);
      sanei_scsi_close (sfd);
      free (dev);
      return SANE_STATUS_NO_MEM;
    }

  DBG (DBG_info,
       "attach_scanner: sanei_scsi_open_extended returned scsi buffer size = %d\n",
       bufsize);
#else
  bufsize = sanei_scsi_max_request_size;

  if (sanei_scsi_open (devicename, &sfd, sense_handler, dev) != 0)
    {
      DBG (DBG_error, "attach_scanner: open failed\n");
      free (dev);

      return SANE_STATUS_INVAL;

    }
#endif

  pie_init (dev);		/* preset values in structure dev */

  dev->devicename = strdup (devicename);

  if (pie_identify_scanner (dev, sfd) != 0)
    {
      DBG (DBG_error, "attach_scanner: scanner-identification failed\n");
      sanei_scsi_close (sfd);
      free (dev);
      return SANE_STATUS_INVAL;
    }

  pie_get_halftones (dev, sfd);
  pie_get_cal_info (dev, sfd);
  pie_get_speeds (dev);

  dev->scan_mode_list[0] = COLOR_STR;
  dev->scan_mode_list[1] = GRAY_STR;
  dev->scan_mode_list[2] = LINEART_STR;
  dev->scan_mode_list[3] = HALFTONE_STR;
  dev->scan_mode_list[4] = 0;

  sanei_scsi_close (sfd);

  dev->sane.name = dev->devicename;
  dev->sane.vendor = dev->vendor;
  dev->sane.model = dev->product;
  dev->sane.type = "flatbed scanner";

  dev->x_range.min = SANE_FIX (0);
  dev->x_range.quant = SANE_FIX (0);
  dev->x_range.max = SANE_FIX (dev->inquiry_fb_width * MM_PER_INCH);

  dev->y_range.min = SANE_FIX (0);
  dev->y_range.quant = SANE_FIX (0);
  dev->y_range.max = SANE_FIX (dev->inquiry_fb_length * MM_PER_INCH);

  dev->dpi_range.min = SANE_FIX (25);
  dev->dpi_range.quant = SANE_FIX (1);
  dev->dpi_range.max =
    SANE_FIX (max (dev->inquiry_x_res, dev->inquiry_y_res));

  dev->shadow_range.min = SANE_FIX (0);
  dev->shadow_range.quant = SANE_FIX (1);
  dev->shadow_range.max = SANE_FIX (dev->inquiry_max_shadow);

  dev->highlight_range.min = SANE_FIX (dev->inquiry_min_highlight);
  dev->highlight_range.quant = SANE_FIX (1);
  dev->highlight_range.max = SANE_FIX (100);

  dev->exposure_range.min = SANE_FIX (dev->inquiry_min_exp);
  dev->exposure_range.quant = SANE_FIX (1);
  dev->exposure_range.max = SANE_FIX (dev->inquiry_max_exp);

#if 0
  dev->analog_gamma_range.min = SANE_FIX (1.0);
  dev->analog_gamma_range.quant = SANE_FIX (0.01);
  dev->analog_gamma_range.max = SANE_FIX (2.0);

#endif

  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    {
      *devp = dev;
    }

  return SANE_STATUS_GOOD;
}

/* --------------------------- MAX STRING SIZE ---------------------------- */


static size_t
max_string_size (SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	{
	  max_size = size;
	}
    }

  return max_size;
}


/* --------------------------- INIT OPTIONS ------------------------------- */


static SANE_Status
init_options (Pie_Scanner * scanner)
{
  int i;

  DBG (DBG_sane_proc, "init_options\n");

  memset (scanner->opt, 0, sizeof (scanner->opt));
  memset (scanner->val, 0, sizeof (scanner->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      scanner->opt[i].size = sizeof (SANE_Word);
      scanner->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  scanner->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  scanner->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  scanner->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  scanner->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  scanner->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Mode" group: */
  scanner->opt[OPT_MODE_GROUP].title = "Scan Mode";
  scanner->opt[OPT_MODE_GROUP].desc = "";
  scanner->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_MODE_GROUP].cap = 0;
  scanner->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  scanner->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  scanner->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  scanner->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  scanner->opt[OPT_MODE].type = SANE_TYPE_STRING;
  scanner->opt[OPT_MODE].size =
    max_string_size ((SANE_String_Const *) scanner->device->scan_mode_list);
  scanner->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_MODE].constraint.string_list =
    (SANE_String_Const *) scanner->device->scan_mode_list;
  scanner->val[OPT_MODE].s =
    (SANE_Char *) strdup (scanner->device->scan_mode_list[0]);

  /* x-resolution */
  scanner->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  scanner->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  scanner->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  scanner->opt[OPT_RESOLUTION].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  scanner->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_RESOLUTION].constraint.range = &scanner->device->dpi_range;
  scanner->val[OPT_RESOLUTION].w = 100 << SANE_FIXED_SCALE_SHIFT;

  /* "Geometry" group: */

  scanner->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  scanner->opt[OPT_GEOMETRY_GROUP].desc = "";
  scanner->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  scanner->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  scanner->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  scanner->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  scanner->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  scanner->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  scanner->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_TL_X].constraint.range = &(scanner->device->x_range);
  scanner->val[OPT_TL_X].w = 0;

  /* top-left y */
  scanner->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  scanner->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  scanner->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_TL_Y].constraint.range = &(scanner->device->y_range);
  scanner->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  scanner->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  scanner->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  scanner->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  scanner->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  scanner->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BR_X].constraint.range = &(scanner->device->x_range);
  scanner->val[OPT_BR_X].w = scanner->device->x_range.max;

  /* bottom-right y */
  scanner->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  scanner->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  scanner->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_BR_Y].constraint.range = &(scanner->device->y_range);
  scanner->val[OPT_BR_Y].w = scanner->device->y_range.max;

  /* "enhancement" group: */

  scanner->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  scanner->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  scanner->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  scanner->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* grayscale gamma vector */
  scanner->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  scanner->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  scanner->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  scanner->opt[OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
  scanner->opt[OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->val[OPT_GAMMA_VECTOR].wa = scanner->gamma_table[0];
  scanner->opt[OPT_GAMMA_VECTOR].constraint.range = &scanner->gamma_range;
  scanner->opt[OPT_GAMMA_VECTOR].size =
    scanner->gamma_length * sizeof (SANE_Word);
  scanner->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;

  /* red gamma vector */
  scanner->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  scanner->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  scanner->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  scanner->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  scanner->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->val[OPT_GAMMA_VECTOR_R].wa = scanner->gamma_table[1];
  scanner->opt[OPT_GAMMA_VECTOR_R].constraint.range = &(scanner->gamma_range);
  scanner->opt[OPT_GAMMA_VECTOR_R].size =
    scanner->gamma_length * sizeof (SANE_Word);

  /* green gamma vector */
  scanner->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  scanner->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  scanner->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  scanner->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  scanner->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->val[OPT_GAMMA_VECTOR_G].wa = scanner->gamma_table[2];
  scanner->opt[OPT_GAMMA_VECTOR_G].constraint.range = &(scanner->gamma_range);
  scanner->opt[OPT_GAMMA_VECTOR_G].size =
    scanner->gamma_length * sizeof (SANE_Word);


  /* blue gamma vector */
  scanner->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  scanner->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  scanner->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  scanner->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  scanner->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  scanner->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->val[OPT_GAMMA_VECTOR_B].wa = scanner->gamma_table[3];
  scanner->opt[OPT_GAMMA_VECTOR_B].constraint.range = &(scanner->gamma_range);
  scanner->opt[OPT_GAMMA_VECTOR_B].size =
    scanner->gamma_length * sizeof (SANE_Word);

  /* halftone pattern */
  scanner->opt[OPT_HALFTONE_PATTERN].name = SANE_NAME_HALFTONE_PATTERN;
  scanner->opt[OPT_HALFTONE_PATTERN].title = SANE_TITLE_HALFTONE_PATTERN;
  scanner->opt[OPT_HALFTONE_PATTERN].desc = SANE_DESC_HALFTONE_PATTERN;
  scanner->opt[OPT_HALFTONE_PATTERN].type = SANE_TYPE_STRING;
  scanner->opt[OPT_HALFTONE_PATTERN].size =
    max_string_size ((SANE_String_Const *) scanner->device->halftone_list);
  scanner->opt[OPT_HALFTONE_PATTERN].constraint_type =
    SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_HALFTONE_PATTERN].constraint.string_list =
    (SANE_String_Const *) scanner->device->halftone_list;
  scanner->val[OPT_HALFTONE_PATTERN].s =
    (SANE_Char *) strdup (scanner->device->halftone_list[0]);
  scanner->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;

  /* speed */
  scanner->opt[OPT_SPEED].name = SANE_NAME_SCAN_SPEED;
  scanner->opt[OPT_SPEED].title = SANE_TITLE_SCAN_SPEED;
  scanner->opt[OPT_SPEED].desc = SANE_DESC_SCAN_SPEED;
  scanner->opt[OPT_SPEED].type = SANE_TYPE_STRING;
  scanner->opt[OPT_SPEED].size =
    max_string_size ((SANE_String_Const *) scanner->device->speed_list);
  scanner->opt[OPT_SPEED].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  scanner->opt[OPT_SPEED].constraint.string_list =
    (SANE_String_Const *) scanner->device->speed_list;
  scanner->val[OPT_SPEED].s =
    (SANE_Char *) strdup (scanner->device->speed_list[0]);

  /* lineart threshold */
  scanner->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  scanner->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  scanner->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  scanner->opt[OPT_THRESHOLD].type = SANE_TYPE_FIXED;
  scanner->opt[OPT_THRESHOLD].unit = SANE_UNIT_PERCENT;
  scanner->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  scanner->opt[OPT_THRESHOLD].constraint.range = &percentage_range_100;
  scanner->val[OPT_THRESHOLD].w = SANE_FIX (50);
  scanner->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;

  /* "advanced" group: */

  scanner->opt[OPT_ADVANCED_GROUP].title = "Advanced";
  scanner->opt[OPT_ADVANCED_GROUP].desc = "";
  scanner->opt[OPT_ADVANCED_GROUP].type = SANE_TYPE_GROUP;
  scanner->opt[OPT_ADVANCED_GROUP].cap = SANE_CAP_ADVANCED;
  scanner->opt[OPT_ADVANCED_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* preview */
  scanner->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  scanner->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  scanner->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  scanner->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  scanner->val[OPT_PREVIEW].w = SANE_FALSE;



  return SANE_STATUS_GOOD;
}


/*------------------------- PIE POWER SAVE -----------------------------*/

static SANE_Status
pie_power_save (Pie_Scanner * scanner, int time)
{
  unsigned char buffer[128];
  size_t size;
  SANE_Status status;
  unsigned char *data;

  DBG (DBG_proc, "pie_power_save: %d min\n", time);

  size = 6;

  set_write_length (swrite.cmd, size);

  memcpy (buffer, swrite.cmd, swrite.size);

  data = buffer + swrite.size;
  memset (data, 0, size);

  set_command (data, SET_POWER_SAVE_CONTROL);
  set_data_length (data, size - 4);
  data[4] = time & 0x7f;

  status =
    sanei_scsi_cmd (scanner->sfd, buffer, swrite.size + size, NULL, NULL);
  if (status)
    {
      DBG (DBG_error, "pie_power_save: write command returned status %s\n",
	   sane_strstatus (status));
    }

  return status;
}

/*------------------------- PIE SEND EXPOSURE ONE -----------------------------*/


static SANE_Status
pie_send_exposure_one (Pie_Scanner * scanner, int filter, int value)
{
  unsigned char buffer[128];
  size_t size;
  SANE_Status status;
  unsigned char *data;

  DBG (DBG_proc, "pie_send_exposure_one\n");

  size = 8;

  set_write_length (swrite.cmd, size);

  memcpy (buffer, swrite.cmd, swrite.size);

  data = buffer + swrite.size;
  memset (data, 0, size);

  set_command (data, SET_EXP_TIME);
  set_data_length (data, size - 4);

  data[4] = filter;

  set_data (data, 6, (int) value, 2);

  status =
    sanei_scsi_cmd (scanner->sfd, buffer, swrite.size + size, NULL, NULL);
  if (status)
    {
      DBG (DBG_error,
	   "pie_send_exposure_one: write command returned status %s\n",
	   sane_strstatus (status));
    }

  return status;
}

/*------------------------- PIE SEND EXPOSURE -----------------------------*/

static SANE_Status
pie_send_exposure (Pie_Scanner * scanner)
{
  SANE_Status status;

  DBG (DBG_proc, "pie_send_exposure\n");

  status = pie_send_exposure_one (scanner, FILTER_RED, 100);
  if (status)
    return status;

  status = pie_send_exposure_one (scanner, FILTER_GREEN, 100);
  if (status)
    return status;

  status = pie_send_exposure_one (scanner, FILTER_BLUE, 100);
  if (status)
    return status;

  return SANE_STATUS_GOOD;
}


/*------------------------- PIE SEND HIGHLIGHT/SHADOW ONE -----------------------------*/

static SANE_Status
pie_send_highlight_shadow_one (Pie_Scanner * scanner, int filter,
			       int highlight, int shadow)
{
  unsigned char buffer[128];
  size_t size;
  SANE_Status status;
  unsigned char *data;

  DBG (DBG_proc, "pie_send_highlight_shadow_one\n");

  size = 8;

  set_write_length (swrite.cmd, size);

  memcpy (buffer, swrite.cmd, swrite.size);

  data = buffer + swrite.size;
  memset (data, 0, size);

  set_command (data, SET_EXP_TIME);
  set_data_length (data, size - 4);

  data[4] = filter;

  data[6] = highlight;
  data[7] = shadow;

  status =
    sanei_scsi_cmd (scanner->sfd, buffer, swrite.size + size, NULL, NULL);
  if (status)
    {
      DBG (DBG_error,
	   "pie_send_highlight_shadow_one: write command returned status %s\n",
	   sane_strstatus (status));
    }

  return status;
}

/*------------------------- PIE SEND HIGHLIGHT/SHADOW -----------------------------*/

static SANE_Status
pie_send_highlight_shadow (Pie_Scanner * scanner)
{
  SANE_Status status;

  DBG (DBG_proc, "pie_send_highlight_shadow\n");

  status = pie_send_highlight_shadow_one (scanner, FILTER_RED, 100, 0);
  if (status)
    return status;

  status = pie_send_highlight_shadow_one (scanner, FILTER_GREEN, 100, 0);
  if (status)
    return status;

  status = pie_send_highlight_shadow_one (scanner, FILTER_BLUE, 100, 0);
  if (status)
    return status;

  return SANE_STATUS_GOOD;
}

/*------------------------- PIE PERFORM CAL ----------------------------*/

static SANE_Status
pie_perform_cal (Pie_Scanner * scanner, int cal_index)
{
  long *red_result;
  long *green_result;
  long *blue_result;
  long *neutral_result;
  long *result = NULL;
  int rcv_length, send_length;
  int rcv_lines, rcv_bits, send_bits;
  int pixels_per_line;
  int i;
  unsigned char *rcv_buffer, *rcv_ptr;
  unsigned char *send_buffer, *send_ptr;
  size_t size;
  int fullscale;
  int cal_limit;
  int k;
  int filter;
  SANE_Status status;

  DBG (DBG_proc, "pie_perform_cal\n");

  pixels_per_line = scanner->device->cal_info[cal_index].pixels_per_line;
  rcv_length = pixels_per_line;
  send_length = pixels_per_line;

  rcv_bits = scanner->device->cal_info[cal_index].receive_bits;
  if (rcv_bits > 8)
    rcv_length *= 2;		/* 2 bytes / sample */

  send_bits = scanner->device->cal_info[cal_index].send_bits;
  if (send_bits > 8)
    send_length *= 2;		/* 2 bytes / sample */

  rcv_lines = scanner->device->cal_info[cal_index].num_lines;

  send_length += 2;		/* space for filter at start */

  if (scanner->colormode == RGB)
    {
      rcv_lines *= 3;
      send_length *= 3;
      rcv_length += 2;		/* 2 bytes for index at front of data (only in RGB??) */
    }

  send_length += 4;		/* space for header at start of data */

  /* alllocate buffers for the receive data, the result buffers, and for the send data */
  rcv_buffer = (unsigned char *) malloc (rcv_length);

  red_result = (long *) calloc (pixels_per_line, sizeof (long));
  green_result = (long *) calloc (pixels_per_line, sizeof (long));
  blue_result = (long *) calloc (pixels_per_line, sizeof (long));
  neutral_result = (long *) calloc (pixels_per_line, sizeof (long));

  if (!rcv_buffer || !red_result || !green_result || !blue_result
      || !neutral_result)
    {
      /* at least one malloc failed, so free all buffers (free accepts NULL) */
      free (rcv_buffer);
      free (red_result);
      free (green_result);
      free (blue_result);
      free (neutral_result);
      return SANE_STATUS_NO_MEM;
    }

  /* read the cal data a line at a time, and accumulate into the result arrays */
  while (rcv_lines--)
    {
      /* TUR */
      status = pie_wait_scanner (scanner);
      if (status)
	{
	  free (rcv_buffer);
	  free (red_result);
	  free (green_result);
	  free (blue_result);
	  free (neutral_result);
	  return status;
	}

      set_read_length (sread.cmd, 1);
      size = rcv_length;

      DBG (DBG_info, "pie_perform_cal: reading 1 line (%lu bytes)\n", (u_long) size);

      status =
	sanei_scsi_cmd (scanner->sfd, sread.cmd, sread.size, rcv_buffer,
			&size);

      if (status)
	{
	  DBG (DBG_error,
	       "pie_perform_cal: read command returned status %s\n",
	       sane_strstatus (status));
	  free (rcv_buffer);
	  free (red_result);
	  free (green_result);
	  free (blue_result);
	  free (neutral_result);
	  return status;
	}

      DBG_DUMP (DBG_dump, rcv_buffer, 32);

      /* which result buffer does this line belong to? */
      if (scanner->colormode == RGB)
	{
	  if (*rcv_buffer == 'R')
	    result = red_result;
	  else if (*rcv_buffer == 'G')
	    result = green_result;
	  else if (*rcv_buffer == 'B')
	    result = blue_result;
	  else if (*rcv_buffer == 'N')
	    result = neutral_result;
	  else
	    {
	      DBG (DBG_error, "pie_perform_cal: invalid index byte (%02x)\n",
		   *rcv_buffer);
	      DBG_DUMP (DBG_error, rcv_buffer, 32);
	      free (rcv_buffer);
	      free (red_result);
	      free (green_result);
	      free (blue_result);
	      free (neutral_result);
	      return SANE_STATUS_INVAL;
	    }
	  rcv_ptr = rcv_buffer + 2;
	}
      else
	{
	  /* monochrome - no bytes indicating filter here */
	  result = neutral_result;
	  rcv_ptr = rcv_buffer;
	}

      /* now add the values in this line to the result array */
      for (i = 0; i < pixels_per_line; i++)
	{
	  result[i] += *rcv_ptr++;
	  if (rcv_bits > 8)
	    {
	      result[i] += (*rcv_ptr++) << 8;
	    }
	}
    }

  /* got all the cal data, now process it ready to send back */
  free (rcv_buffer);
  send_buffer = (unsigned char *) malloc (send_length + swrite.size);

  if (!send_buffer)
    {
      free (red_result);
      free (green_result);
      free (blue_result);
      free (neutral_result);
      return SANE_STATUS_NO_MEM;
    }

  rcv_lines = scanner->device->cal_info[cal_index].num_lines;
  fullscale = (1 << rcv_bits) - 1;
  cal_limit = fullscale / (1 << scanner->device->inquiry_cal_eqn);
  k = (1 << scanner->device->inquiry_cal_eqn) - 1;

  /* set up scsi command and data */
  size = send_length;

  memcpy (send_buffer, swrite.cmd, swrite.size);
  set_write_length (send_buffer, size);

  set_command (send_buffer + swrite.size, SEND_CAL_DATA);
  set_data_length (send_buffer + swrite.size, size - 4);

  send_ptr = send_buffer + swrite.size + 4;

  for (filter = FILTER_NEUTRAL; filter <= FILTER_BLUE; filter <<= 1)
    {

      /* only send data for filter we expect to send */
      if (!(filter & scanner->cal_filter))
	continue;

      set_data (send_ptr, 0, filter, 2);
      send_ptr += 2;

      if (scanner->colormode == RGB)
	{
	  switch (filter)
	    {
	    case FILTER_RED:
	      result = red_result;
	      break;

	    case FILTER_GREEN:
	      result = green_result;
	      break;

	    case FILTER_BLUE:
	      result = blue_result;
	      break;

	    case FILTER_NEUTRAL:
	      result = neutral_result;
	      break;
	    }
	}
      else
	result = neutral_result;

      /* for each pixel */
      for (i = 0; i < pixels_per_line; i++)
	{
	  long x;

	  /* make average */
	  x = result[i] / rcv_lines;

	  /* ensure not overflowed */
	  if (x > fullscale)
	    x = fullscale;

	  /* process according to required calibration equation */
	  if (scanner->device->inquiry_cal_eqn)
	    {
	      if (x <= cal_limit)
		x = fullscale;
	      else
		x = ((fullscale - x) * fullscale) / (x * k);
	    }

	  if (rcv_bits > send_bits)
	    x >>= (rcv_bits - send_bits);
	  else if (send_bits > rcv_bits)
	    x <<= (send_bits - rcv_bits);

	  /* put result into send buffer */
	  *send_ptr++ = x;
	  if (send_bits > 8)
	    *send_ptr++ = x >> 8;
	}
    }

  /* now send the data back to scanner */

  /* TUR */
  status = pie_wait_scanner (scanner);
  if (status)
    {
      free (red_result);
      free (green_result);
      free (blue_result);
      free (neutral_result);
      free (send_buffer);
      return status;
    }

  DBG (DBG_info, "pie_perform_cal: sending cal data (%lu bytes)\n", (u_long) size);
  DBG_DUMP (DBG_dump, send_buffer, 64);

  status =
    sanei_scsi_cmd (scanner->sfd, send_buffer, swrite.size + size, NULL,
		    NULL);
  if (status)
    {
      DBG (DBG_error, "pie_perform_cal: write command returned status %s\n",
	   sane_strstatus (status));
      free (red_result);
      free (green_result);
      free (blue_result);
      free (neutral_result);
      free (send_buffer);
      return status;
    }

  free (red_result);
  free (green_result);
  free (blue_result);
  free (neutral_result);
  free (send_buffer);

  return SANE_STATUS_GOOD;
}

/*------------------------- PIE DO CAL -----------------------------*/

static SANE_Status
pie_do_cal (Pie_Scanner * scanner)
{
  SANE_Status status;
  int cal_index;

  DBG (DBG_proc, "pie_do_cal\n");

  if (scanner->device->inquiry_scan_capability & INQ_CAP_EXT_CAL)
    {
      for (cal_index = 0; cal_index < scanner->device->cal_info_count;
	   cal_index++)
	if (scanner->device->cal_info[cal_index].cal_type ==
	    scanner->cal_mode)
	  {
	    status = pie_perform_cal (scanner, cal_index);
	    if (status != SANE_STATUS_GOOD)
	      return status;
	  }
    }

  return SANE_STATUS_GOOD;
}

/*------------------------- PIE DWNLD GAMMA ONE -----------------------------*/

static SANE_Status
pie_dwnld_gamma_one (Pie_Scanner * scanner, int filter, SANE_Int * table)
{
  unsigned char *buffer;
  size_t size;
  SANE_Status status;
  unsigned char *data;
  int i;

  DBG (DBG_proc, "pie_dwnld_gamma_one\n");

  /* TUR */
  status = pie_wait_scanner (scanner);
  if (status)
    {
      return status;
    }

  if (scanner->device->inquiry_gamma_bits > 8)
    size = scanner->gamma_length * 2 + 6;
  else
    size = scanner->gamma_length + 6;

  buffer = malloc (size + swrite.size);
  if (!buffer)
    return SANE_STATUS_NO_MEM;

  set_write_length (swrite.cmd, size);

  memcpy (buffer, swrite.cmd, swrite.size);

  data = buffer + swrite.size;
  memset (data, 0, size);

  set_command (data, DWNLD_GAMMA_TABLE);
  set_data_length (data, size - 4);

  data[4] = filter;

  for (i = 0; i < scanner->gamma_length; i++)
    {
      if (scanner->device->inquiry_gamma_bits > 8)
	{
	  set_data (data, 6 + 2 * i, table ? table[i] : i, 2);
	}
      else
	{
	  set_data (data, 6 + i, table ? table[i] : i, 1);
	}
    }

  DBG_DUMP (DBG_dump, data, 128);

  status =
    sanei_scsi_cmd (scanner->sfd, buffer, swrite.size + size, NULL, NULL);
  if (status)
    {
      DBG (DBG_error,
	   "pie_dwnld_gamma_one: write command returned status %s\n",
	   sane_strstatus (status));
    }

  free (buffer);

  return status;
}

/*------------------------- PIE DWNLD GAMMA -----------------------------*/

static SANE_Status
pie_dwnld_gamma (Pie_Scanner * scanner)
{
  SANE_Status status;

  DBG (DBG_proc, "pie_dwnld_gamma\n");

  if (scanner->colormode == RGB)
    {
      status =
	pie_dwnld_gamma_one (scanner, FILTER_RED, scanner->gamma_table[1]);
      if (status)
	return status;


      status =
	pie_dwnld_gamma_one (scanner, FILTER_GREEN, scanner->gamma_table[2]);
      if (status)
	return status;

      status =
	pie_dwnld_gamma_one (scanner, FILTER_BLUE, scanner->gamma_table[3]);
      if (status)
	return status;
    }
  else
    {
      SANE_Int *table;

      /* if lineart or half tone, force gamma to be one to one by passing NULL */
      if (scanner->colormode == GRAYSCALE)
	table = scanner->gamma_table[0];
      else
	table = NULL;

      status = pie_dwnld_gamma_one (scanner, FILTER_GREEN, table);
      if (status)
	return status;
    }

  usleep (DOWNLOAD_GAMMA_WAIT_TIME);

  return SANE_STATUS_GOOD;
}

/*------------------------- PIE SET WINDOW -----------------------------*/

static SANE_Status
pie_set_window (Pie_Scanner * scanner)
{
  unsigned char buffer[128];
  size_t size;
  SANE_Status status;
  unsigned char *data;
  double x, dpmm;

  DBG (DBG_proc, "pie_set_window\n");

  size = 14;

  set_write_length (swrite.cmd, size);

  memcpy (buffer, swrite.cmd, swrite.size);

  data = buffer + swrite.size;
  memset (data, 0, size);

  set_command (data, SET_SCAN_FRAME);
  set_data_length (data, size - 4);

  data[4] = 0x80;
  if (scanner->colormode == HALFTONE)
    data[4] |= 0x40;

  dpmm = (double) scanner->device->inquiry_pixel_resolution / MM_PER_INCH;

  x = SANE_UNFIX (scanner->val[OPT_TL_X].w) * dpmm;
  set_data (data, 6, (int) x, 2);
  DBG (DBG_info, "TL_X: %d\n", (int) x);

  x = SANE_UNFIX (scanner->val[OPT_TL_Y].w) * dpmm;
  set_data (data, 8, (int) x, 2);
  DBG (DBG_info, "TL_Y: %d\n", (int) x);

  x = SANE_UNFIX (scanner->val[OPT_BR_X].w) * dpmm;
  set_data (data, 10, (int) x, 2);
  DBG (DBG_info, "BR_X: %d\n", (int) x);

  x = SANE_UNFIX (scanner->val[OPT_BR_Y].w) * dpmm;
  set_data (data, 12, (int) x, 2);
  DBG (DBG_info, "BR_Y: %d\n", (int) x);

  status =
    sanei_scsi_cmd (scanner->sfd, buffer, swrite.size + size, NULL, NULL);
  if (status)
    {
      DBG (DBG_error, "pie_set_window: write command returned status %s\n",
	   sane_strstatus (status));
    }

  return status;
}


/*------------------------- PIE MODE SELECT -----------------------------*/

static SANE_Status
pie_mode_select (Pie_Scanner * scanner)
{

  SANE_Status status;
  unsigned char buffer[128];
  size_t size;
  unsigned char *data;
  int i;

  DBG (DBG_proc, "pie_mode_select\n");

  size = 14;

  set_mode_length (smode.cmd, size);

  memcpy (buffer, smode.cmd, smode.size);

  data = buffer + smode.size;
  memset (data, 0, size);

  /* size of data */
  data[1] = size - 2;

  /* set resolution required */
  set_data (data, 2, scanner->resolution, 2);

  /* set color filter and color depth */
  switch (scanner->colormode)
    {
    case RGB:
      if (scanner->device->inquiry_filters & INQ_ONE_PASS_COLOR)
	{
	  data[4] = INQ_ONE_PASS_COLOR;
	  scanner->cal_filter = FILTER_RED | FILTER_GREEN | FILTER_BLUE;
	}
      else
	{
	  DBG (DBG_error,
	       "pie_mode_select: support for multipass color not yet implemented\n");
	  return SANE_STATUS_UNSUPPORTED;
	}
      data[5] = INQ_COLOR_DEPTH_8;
      break;

    case GRAYSCALE:
    case LINEART:
    case HALFTONE:
      /* choose which filter to use for monochrome mode */
      if (scanner->device->inquiry_filters & INQ_FILTER_NEUTRAL)
	{
	  data[4] = FILTER_NEUTRAL;
	  scanner->cal_filter = FILTER_NEUTRAL;
	}
      else if (scanner->device->inquiry_filters & INQ_FILTER_GREEN)
	{
	  data[4] = FILTER_GREEN;
	  scanner->cal_filter = FILTER_GREEN;
	}
      else if (scanner->device->inquiry_filters & INQ_FILTER_RED)
	{
	  data[4] = FILTER_RED;
	  scanner->cal_filter = FILTER_RED;
	}
      else if (scanner->device->inquiry_filters & INQ_FILTER_BLUE)
	{
	  data[4] = FILTER_BLUE;
	  scanner->cal_filter = FILTER_BLUE;
	}
      else
	{
	  DBG (DBG_error,
	       "pie_mode_select: scanner doesn't appear to support monochrome\n");
	  return SANE_STATUS_UNSUPPORTED;
	}

      if (scanner->colormode == GRAYSCALE)
	data[5] = INQ_COLOR_DEPTH_8;
      else
	data[5] = INQ_COLOR_DEPTH_1;
      break;
    }

  /* choose color packing method */
  if (scanner->device->inquiry_color_format & INQ_COLOR_FORMAT_LINE)
    data[6] = INQ_COLOR_FORMAT_LINE;
  else if (scanner->device->inquiry_color_format & INQ_COLOR_FORMAT_INDEX)
    data[6] = INQ_COLOR_FORMAT_INDEX;
  else
    {
      DBG (DBG_error,
	   "pie_mode_select: support for pixel packing not yet implemented\n");
      return SANE_STATUS_UNSUPPORTED;
    }

  /* choose data format */
  if (scanner->device->inquiry_image_format & INQ_IMG_FMT_INTEL)
    data[8] = INQ_IMG_FMT_INTEL;
  else
    {
      DBG (DBG_error,
	   "pie_mode_select: support for Motorola format not yet implemented\n");
      return SANE_STATUS_UNSUPPORTED;
    }

  /* set required speed */
  i = 0;
  while (scanner->device->speed_list[i] != NULL)
    {
      if (strcmp (scanner->device->speed_list[i], scanner->val[OPT_SPEED].s)
	  == 0)
	break;
      i++;
    }

  if (scanner->device->speed_list[i] == NULL)
    data[9] = 0;
  else
    data[9] = i;

  scanner->cal_mode = CAL_MODE_FLATBED;

  /* if preview supported, ask for preview, limit resolution to max for fast preview */
  if (scanner->val[OPT_PREVIEW].w
      && (scanner->device->inquiry_scan_capability & INQ_CAP_FAST_PREVIEW))
    {
      DBG (DBG_info, "pie_mode_select: setting preview\n");
      scanner->cal_mode |= CAL_MODE_PREVIEW;
      data[9] |= INQ_CAP_FAST_PREVIEW;
      data[9] &= ~INQ_CAP_SPEEDS;
      if (scanner->resolution > scanner->device->inquiry_fast_preview_res)
	set_data (data, 2, scanner->device->inquiry_fast_preview_res, 2);
    }


  /* set required halftone pattern */
  i = 0;
  while (scanner->device->halftone_list[i] != NULL)
    {
      if (strcmp
	  (scanner->device->halftone_list[i],
	   scanner->val[OPT_HALFTONE_PATTERN].s) == 0)
	break;
      i++;
    }

  if (scanner->device->halftone_list[i] == NULL)
    data[12] = 0;		/* halftone pattern */
  else
    data[12] = i;

  data[13] = SANE_UNFIX (scanner->val[OPT_THRESHOLD].w) * 255 / 100;	/* lineart threshold */

  DBG (DBG_info, "pie_mode_select: speed %02x\n", data[9]);
  DBG (DBG_info, "pie_mode_select: halftone %d\n", data[12]);
  DBG (DBG_info, "pie_mode_select: threshold %02x\n", data[13]);

  status =
    sanei_scsi_cmd (scanner->sfd, buffer, smode.size + size, NULL, NULL);
  if (status)
    {
      DBG (DBG_error, "pie_mode_select: write command returned status %s\n",
	   sane_strstatus (status));
    }

  return status;
}


/*------------------------- PIE SCAN -----------------------------*/

static SANE_Status
pie_scan (Pie_Scanner * scanner, int start)
{
  SANE_Status status;

  DBG (DBG_proc, "pie_scan\n");

  /* TUR */
  status = pie_wait_scanner (scanner);
  if (status)
    {
      return status;
    }

  set_scan_cmd (scan.cmd, start);

  do
    {
      status = sanei_scsi_cmd (scanner->sfd, scan.cmd, scan.size, NULL, NULL);
      if (status)
	{
	  DBG (DBG_error, "pie_scan: write command returned status %s\n",
	       sane_strstatus (status));
	  usleep (SCAN_WARMUP_WAIT_TIME);
	}
    }
  while (start && status);

  usleep (SCAN_WAIT_TIME);

  return status;
}


/* --------------------------------------- PIE WAIT SCANNER -------------------------- */


static SANE_Status
pie_wait_scanner (Pie_Scanner * scanner)
{
  SANE_Status status;
  int cnt = 0;

  DBG (DBG_proc, "wait_scanner\n");

  do
    {
      if (cnt > 100)		/* maximal 100 * 0.5 sec = 50 sec */
	{
	  DBG (DBG_warning, "scanner does not get ready\n");
	  return -1;
	}
      /* test unit ready */
      status =
	sanei_scsi_cmd (scanner->sfd, test_unit_ready.cmd,
			test_unit_ready.size, NULL, NULL);
      cnt++;

      if (status)
	{
	  if (cnt == 1)
	    {
	      DBG (DBG_info2, "scanner reports %s, waiting ...\n",
		   sane_strstatus (status));
	    }

	  usleep (TUR_WAIT_TIME);
	}
    }
  while (status != SANE_STATUS_GOOD);

  DBG (DBG_info, "scanner ready\n");


  return status;
}


/* -------------------------------------- PIE GET PARAMS -------------------------- */


static SANE_Status
pie_get_params (Pie_Scanner * scanner)
{
  SANE_Status status;
  size_t size;
  unsigned char buffer[128];

  DBG (DBG_proc, "pie_get_params\n");

  status = pie_wait_scanner (scanner);
  if (status)
    return status;

  if (scanner->device->inquiry_image_format & INQ_IMG_FMT_OKLINE)
    size = 16;
  else

    size = 14;

  set_param_length (param.cmd, size);

  status =
    sanei_scsi_cmd (scanner->sfd, param.cmd, param.size, buffer, &size);

  if (status)
    {
      DBG (DBG_error, "pie_get_params: command returned status %s\n",
	   sane_strstatus (status));
    }
  else
    {
      DBG (DBG_info, "Scan Width:  %d\n", get_param_scan_width (buffer));
      DBG (DBG_info, "Scan Lines:  %d\n", get_param_scan_lines (buffer));
      DBG (DBG_info, "Scan bytes:  %d\n", get_param_scan_bytes (buffer));

      DBG (DBG_info, "Offset 1:    %d\n",
	   get_param_scan_filter_offset1 (buffer));
      DBG (DBG_info, "Offset 2:    %d\n",
	   get_param_scan_filter_offset2 (buffer));
      DBG (DBG_info, "Scan period: %d\n", get_param_scan_period (buffer));
      DBG (DBG_info, "Xfer rate:   %d\n", get_param_scsi_xfer_rate (buffer));
      if (scanner->device->inquiry_image_format & INQ_IMG_FMT_OKLINE)
	DBG (DBG_info, "Avail lines: %d\n",
	     get_param_scan_available_lines (buffer));

      scanner->filter_offset1 = get_param_scan_filter_offset1 (buffer);
      scanner->filter_offset2 = get_param_scan_filter_offset2 (buffer);
      scanner->bytes_per_line = get_param_scan_bytes (buffer);

      scanner->params.pixels_per_line = get_param_scan_width (buffer);
      scanner->params.lines = get_param_scan_lines (buffer);

      switch (scanner->colormode)
	{
	case RGB:
	  scanner->params.format = SANE_FRAME_RGB;
	  scanner->params.depth = 8;
	  scanner->params.bytes_per_line = 3 * get_param_scan_bytes (buffer);
	  break;

	case GRAYSCALE:
	  scanner->params.format = SANE_FRAME_GRAY;
	  scanner->params.depth = 8;
	  scanner->params.bytes_per_line = get_param_scan_bytes (buffer);
	  break;

	case HALFTONE:
	case LINEART:
	  scanner->params.format = SANE_FRAME_GRAY;
	  scanner->params.depth = 1;
	  scanner->params.bytes_per_line = get_param_scan_bytes (buffer);
	  break;
	}

      scanner->params.last_frame = 0;
    }

  return status;
}


/* -------------------------------------- PIE GRAB SCANNER -------------------------- */


static SANE_Status
pie_grab_scanner (Pie_Scanner * scanner)
{
  SANE_Status status;

  DBG (DBG_proc, "grab_scanner\n");


  status = pie_wait_scanner (scanner);
  if (status)
    return status;

  status =
    sanei_scsi_cmd (scanner->sfd, reserve_unit.cmd, reserve_unit.size, NULL,
		    NULL);


  if (status)
    {
      DBG (DBG_error, "pie_grab_scanner: command returned status %s\n",
	   sane_strstatus (status));
    }
  else
    {
      DBG (DBG_info, "scanner reserved\n");
    }

  return status;
}


/* ------------------------------------ PIE GIVE SCANNER -------------------------- */


static SANE_Status
pie_give_scanner (Pie_Scanner * scanner)
{
  SANE_Status status;

  DBG (DBG_info2, "trying to release scanner ...\n");

  status =
    sanei_scsi_cmd (scanner->sfd, release_unit.cmd, release_unit.size, NULL,
		    NULL);
  if (status)
    {
      DBG (DBG_error, "pie_give_scanner: command returned status %s\n",
	   sane_strstatus (status));
    }
  else
    {
      DBG (DBG_info, "scanner released\n");
    }
  return status;
}


/* ------------------- PIE READER PROCESS INDEXED ------------------- */

static int
pie_reader_process_indexed (Pie_Scanner * scanner, FILE * fp)
{
  int status;
  int lines;
  unsigned char *buffer, *reorder = NULL;
  unsigned char *red_buffer = NULL, *green_buffer = NULL;
  unsigned char *red_in = NULL, *red_out = NULL;
  unsigned char *green_in = NULL, *green_out = NULL;
  int red_size = 0, green_size = 0;
  int bytes_per_line;
  int red_count = 0, green_count = 0;

  size_t size;

  DBG (DBG_read, "reading %d lines of %d bytes/line (indexed)\n",
       scanner->params.lines, scanner->params.bytes_per_line);

  lines = scanner->params.lines;
  bytes_per_line = scanner->bytes_per_line;

  /* allocate receive buffer */
  buffer = malloc (bytes_per_line + 2);
  if (!buffer)
    {
      return SANE_STATUS_NO_MEM;
    }

  /* allocate deskew buffers for RGB mode */
  if (scanner->colormode == RGB)
    {
      lines *= 3;

      red_size = bytes_per_line * (scanner->filter_offset1 +
				   scanner->filter_offset2 + 2);
      green_size = bytes_per_line * (scanner->filter_offset2 + 2);

      DBG (DBG_info2,
	   "pie_reader_process_indexed: alloc %d lines (%d bytes) for red buffer\n",
	   red_size / bytes_per_line, red_size);
      DBG (DBG_info2,
	   "pie_reader_process_indexed: alloc %d lines (%d bytes) for green buffer\n",
	   green_size / bytes_per_line, green_size);

      reorder = malloc (scanner->params.bytes_per_line);
      red_buffer = malloc (red_size);
      green_buffer = malloc (green_size);

      if (!reorder || !red_buffer || !green_buffer)
	{
	  free (buffer);
	  free (reorder);
	  free (red_buffer);
	  free (green_buffer);
	  return SANE_STATUS_NO_MEM;
	}

      red_in = red_out = red_buffer;
      green_in = green_out = green_buffer;
    }

  while (lines--)
    {
      set_read_length (sread.cmd, 1);
      size = bytes_per_line + 2;

      do
	{
	  status =
	    sanei_scsi_cmd (scanner->sfd, sread.cmd, sread.size, buffer,
			    &size);
	}
      while (status);

      DBG_DUMP (DBG_dump, buffer, 64);

      if (scanner->colormode == RGB)
	{
	  /* we're assuming that we get red before green before blue here */
	  switch (*buffer)
	    {
	    case 'R':
	      /* copy to red buffer */
	      memcpy (red_in, buffer + 2, bytes_per_line);

	      /* advance in pointer, and check for wrap */
	      red_in += bytes_per_line;
	      if (red_in >= (red_buffer + red_size))
		red_in = red_buffer;

	      /* increment red line count */
	      red_count++;
	      DBG (DBG_info2,
		   "pie_reader_process_indexed: got a red line (%d)\n",
		   red_count);
	      break;

	    case 'G':
	      /* copy to green buffer */
	      memcpy (green_in, buffer + 2, bytes_per_line);

	      /* advance in pointer, and check for wrap */
	      green_in += bytes_per_line;
	      if (green_in >= (green_buffer + green_size))
		green_in = green_buffer;

	      /* increment green line count */
	      green_count++;
	      DBG (DBG_info2,
		   "pie_reader_process_indexed: got a green line (%d)\n",
		   green_count);
	      break;

	    case 'B':
	      /* check we actually have red and green data available */
	      if (!red_count || !green_count)
		{
		  DBG (DBG_error,
		       "pie_reader_process_indexed: deskew buffer empty (%d %d)\n",
		       red_count, green_count);
		  return SANE_STATUS_INVAL;
		}
	      red_count--;
	      green_count--;

	      DBG (DBG_info2,
		   "pie_reader_process_indexed: got a blue line\n");

	      {
		int i;
		unsigned char *red, *green, *blue, *dest;

		/* now pack the pixels lines into RGB format */
		dest = reorder;
		red = red_out;
		green = green_out;
		blue = buffer + 2;

		for (i = bytes_per_line; i > 0; i--)
		  {
		    *dest++ = *red++;
		    *dest++ = *green++;
		    *dest++ = *blue++;
		  }
		fwrite (reorder, 1, scanner->params.bytes_per_line, fp);

		/* advance out pointers, and check for wrap */
		red_out += bytes_per_line;
		if (red_out >= (red_buffer + red_size))
		  red_out = red_buffer;
		green_out += bytes_per_line;
		if (green_out >= (green_buffer + green_size))
		  green_out = green_buffer;
	      }
	      break;

	    default:
	      DBG (DBG_error,
		   "pie_reader_process_indexed: bad filter index\n");
	    }
	}
      else
	{
	  DBG (DBG_info2,
	       "pie_reader_process_indexed: got a line (%lu bytes)\n", (u_long) size);

	  /* just send the data on, assume filter bytes not present as per calibration case */
	  fwrite (buffer, 1, scanner->params.bytes_per_line, fp);
	}
    }

  free (buffer);
  free (reorder);
  free (red_buffer);
  free (green_buffer);
  return 0;
}

/* --------------------------------- PIE READER PROCESS ------------------------ */

static int
pie_reader_process (Pie_Scanner * scanner, FILE * fp)
{
  int status;
  int lines;
  unsigned char *buffer, *reorder;
  size_t size;

  DBG (DBG_read, "reading %d lines of %d bytes/line\n", scanner->params.lines,
       scanner->params.bytes_per_line);

  buffer = malloc (scanner->params.bytes_per_line);
  reorder = malloc (scanner->params.bytes_per_line);
  if (!buffer || !reorder)
    {
      free (buffer);
      free (reorder);
      return SANE_STATUS_NO_MEM;
    }

  lines = scanner->params.lines;

  while (lines--)
    {
      set_read_length (sread.cmd, 1);
      size = scanner->params.bytes_per_line;

      do
	{
	  status =
	    sanei_scsi_cmd (scanner->sfd, sread.cmd, sread.size, buffer,
			    &size);
	}
      while (status);

      DBG_DUMP (DBG_dump, buffer, 64);

      if (scanner->colormode == RGB)
	{
	  int i;
	  unsigned char *src, *dest;
	  int offset;

	  dest = reorder;
	  src = buffer;
	  offset = scanner->params.pixels_per_line;

	  for (i = scanner->params.pixels_per_line; i > 0; i--)
	    {
	      *dest++ = *src;
	      *dest++ = *(src + offset);
	      *dest++ = *(src + 2 * offset);
	      src++;
	    }
	  fwrite (reorder, 1, scanner->params.bytes_per_line, fp);
	}
      else
	{
	  fwrite (buffer, 1, scanner->params.bytes_per_line, fp);
	}

      fflush (fp);
    }

  free (buffer);
  free (reorder);

  return 0;
}



/* --------------------------------- READER PROCESS SIGTERM HANDLER  ------------ */


static RETSIGTYPE
reader_process_sigterm_handler (int signal)
{
  DBG (DBG_sane_info, "reader_process: terminated by signal %d\n", signal);

#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
  sanei_scsi_req_flush_all ();	/* flush SCSI queue */
#else
  sanei_scsi_req_flush_all ();	/* flush SCSI queue */
#endif

  _exit (SANE_STATUS_GOOD);
}



/* ------------------------------ READER PROCESS ----------------------------- */


static int
reader_process ( void *data )	/* executed as a child process */
{
  int status;
  FILE *fp;
  Pie_Scanner * scanner;
  sigset_t ignore_set;
  struct SIGACTION act;

  scanner = (Pie_Scanner *)data;
  
  if (sanei_thread_is_forked ()) {

      close ( scanner->pipe );

      sigfillset (&ignore_set);
      sigdelset (&ignore_set, SIGTERM);
#if defined (__APPLE__) && defined (__MACH__)
      sigdelset (&ignore_set, SIGUSR2);
#endif
      sigprocmask (SIG_SETMASK, &ignore_set, 0);

      memset (&act, 0, sizeof (act));
      sigaction (SIGTERM, &act, 0);
  }
  
  DBG (DBG_sane_proc, "reader_process started\n");

  memset (&act, 0, sizeof (act));	/* define SIGTERM-handler */
  act.sa_handler = reader_process_sigterm_handler;
  sigaction (SIGTERM, &act, 0);

  fp = fdopen (scanner->reader_fds, "w");
  if (!fp)
    {
      return SANE_STATUS_IO_ERROR;
    }

  DBG (DBG_sane_info, "reader_process: starting to READ data\n");

  if (scanner->device->inquiry_color_format & INQ_COLOR_FORMAT_LINE)
    status = pie_reader_process (scanner, fp);
  else if (scanner->device->inquiry_color_format & INQ_COLOR_FORMAT_INDEX)
    status = pie_reader_process_indexed (scanner, fp);
  else
    status = SANE_STATUS_UNSUPPORTED;

  fclose (fp);

  DBG (DBG_sane_info, "reader_process: finished reading data\n");

  return status;
}


/* -------------------------------- ATTACH_ONE ---------------------------------- */


/* callback function for sanei_config_attach_matching_devices(dev_name, attach_one) */
static SANE_Status
attach_one (const char *name)
{
  attach_scanner (name, 0);
  return SANE_STATUS_GOOD;
}


/* ----------------------------- CLOSE PIPE ---------------------------------- */


static SANE_Status
close_pipe (Pie_Scanner * scanner)
{
  DBG (DBG_sane_proc, "close_pipe\n");

  if (scanner->pipe >= 0)
    {
      close (scanner->pipe);
      scanner->pipe = -1;
    }

  return SANE_STATUS_EOF;
}



/* ---------------------------- DO CANCEL ---------------------------------- */


static SANE_Status
do_cancel (Pie_Scanner * scanner)
{
  DBG (DBG_sane_proc, "do_cancel\n");

  scanner->scanning = SANE_FALSE;

  if (scanner->reader_pid != -1)
    {
      DBG (DBG_sane_info, "killing reader_process\n");
      sanei_thread_kill (scanner->reader_pid);
      sanei_thread_waitpid (scanner->reader_pid, 0);
      scanner->reader_pid = -1;
      DBG (DBG_sane_info, "reader_process killed\n");
    }

  if (scanner->sfd >= 0)
    {
      pie_scan (scanner, 0);

      pie_power_save (scanner, 15);

      pie_give_scanner (scanner);	/* reposition and release scanner */

      DBG (DBG_sane_info, "closing scannerdevice filedescriptor\n");
      sanei_scsi_close (scanner->sfd);
      scanner->sfd = -1;
    }

  return SANE_STATUS_CANCELLED;
}



/* --------------------------------------- SANE INIT ---------------------------------- */


SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback __sane_unused__ authorize)
{
  char dev_name[PATH_MAX];
  size_t len;
  FILE *fp;

  DBG_INIT ();

  DBG (DBG_sane_init, "sane_init() build %d\n", BUILD);

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  fp = sanei_config_open (PIE_CONFIG_FILE);
  if (!fp)
    {
      attach_scanner ("/dev/scanner", 0);	/* no config-file: /dev/scanner */
      return SANE_STATUS_GOOD;
    }

  while (sanei_config_read (dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')
	{
	  continue;
	}			/* ignore line comments */

      len = strlen (dev_name);

      if (!len)			/* ignore empty lines */
	{
	  continue;
	}

      sanei_config_attach_matching_devices (dev_name, attach_one);
    }

  fclose (fp);

  return SANE_STATUS_GOOD;
}


/* ----------------------------------------- SANE EXIT ---------------------------------- */


void
sane_exit (void)
{
  Pie_Device *dev, *next;
  int i;

  DBG (DBG_sane_init, "sane_exit()\n");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free (dev->devicename);
      free (dev->cal_info);
      i = 0;
      while (dev->halftone_list[i] != NULL)
	free (dev->halftone_list[i++]);
      i = 0;
      while (dev->speed_list[i] != NULL)
	free (dev->speed_list[i++]);

      free (dev);
    }

  first_dev = NULL;

  if (devlist)
    {
      free (devlist);
      devlist = NULL;
    }
}


/* ------------------------------------------ SANE GET DEVICES --------------------------- */


SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool __sane_unused__ local_only)
{
  Pie_Device *dev;
  int i;

  DBG (DBG_sane_init, "sane_get_devices\n");

  i = 0;
  for (dev = first_dev; dev; dev = dev->next)
    i++;

  if (devlist)
    {
      free (devlist);
    }

  devlist = malloc ((i + 1) * sizeof (devlist[0]));
  if (!devlist)
    {
      return SANE_STATUS_NO_MEM;
    }

  i = 0;

  for (dev = first_dev; dev; dev = dev->next)
    {
      devlist[i++] = &dev->sane;
    }

  devlist[i] = NULL;

  *device_list = devlist;

  return SANE_STATUS_GOOD;
}


/* --------------------------------------- SANE OPEN ---------------------------------- */

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Pie_Device *dev;
  SANE_Status status;
  Pie_Scanner *scanner;
  int i, j;

  DBG (DBG_sane_init, "sane_open(%s)\n", devicename);

  if (devicename[0])		/* search for devicename */
    {
      for (dev = first_dev; dev; dev = dev->next)
	{
	  if (strcmp (dev->sane.name, devicename) == 0)
	    {
	      break;
	    }
	}

      if (!dev)
	{
	  status = attach_scanner (devicename, &dev);
	  if (status != SANE_STATUS_GOOD)
	    {
	      return status;
	    }
	}
    }
  else
    {
      dev = first_dev;		/* empty devicename -> use first device */
    }


  if (!dev)
    {
      return SANE_STATUS_INVAL;
    }

  scanner = malloc (sizeof (*scanner));
  if (!scanner)

    {
      return SANE_STATUS_NO_MEM;
    }

  memset (scanner, 0, sizeof (*scanner));

  scanner->device = dev;
  scanner->sfd = -1;
  scanner->pipe = -1;

  scanner->gamma_length = 1 << (scanner->device->inquiry_gamma_bits);

  DBG (DBG_sane_info, "Using %d bits for gamma input\n",
       scanner->device->inquiry_gamma_bits);

  scanner->gamma_range.min = 0;
  scanner->gamma_range.max = scanner->gamma_length - 1;
  scanner->gamma_range.quant = 0;

  scanner->gamma_table[0] =
    (SANE_Int *) malloc (scanner->gamma_length * sizeof (SANE_Int));
  scanner->gamma_table[1] =
    (SANE_Int *) malloc (scanner->gamma_length * sizeof (SANE_Int));
  scanner->gamma_table[2] =
    (SANE_Int *) malloc (scanner->gamma_length * sizeof (SANE_Int));
  scanner->gamma_table[3] =
    (SANE_Int *) malloc (scanner->gamma_length * sizeof (SANE_Int));

  for (i = 0; i < 4; ++i)	/* gamma_table[0,1,2,3] */
    {
      for (j = 0; j < scanner->gamma_length; ++j)
	{
	  scanner->gamma_table[i][j] = j;
	}
    }

  init_options (scanner);

  scanner->next = first_handle;	/* insert newly opened handle into list of open handles: */
  first_handle = scanner;

  *handle = scanner;

  return SANE_STATUS_GOOD;
}


/* ------------------------------------ SANE CLOSE --------------------------------- */


void
sane_close (SANE_Handle handle)
{
  Pie_Scanner *prev, *scanner;

  DBG (DBG_sane_init, "sane_close\n");

  /* remove handle from list of open handles: */
  prev = 0;

  for (scanner = first_handle; scanner; scanner = scanner->next)
    {
      if (scanner == handle)
	{
	  break;
	}

      prev = scanner;
    }

  if (!scanner)
    {
      DBG (DBG_error, "close: invalid handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }

  if (scanner->scanning)	/* stop scan if still scanning */
    {
      do_cancel (handle);
    }

  if (prev)
    {
      prev->next = scanner->next;
    }
  else
    {
      first_handle = scanner->next;
    }

  free (scanner->gamma_table[0]);	/* free custom gamma tables */
  free (scanner->gamma_table[1]);
  free (scanner->gamma_table[2]);
  free (scanner->gamma_table[3]);
  free (scanner->val[OPT_MODE].s);
  free (scanner->val[OPT_SPEED].s);
  free (scanner->val[OPT_HALFTONE_PATTERN].s);

  scanner->bufsize = 0;

  free (scanner);		/* free scanner */
}


/* ---------------------------------- SANE GET OPTION DESCRIPTOR ----------------- */

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Pie_Scanner *scanner = handle;

  DBG (DBG_sane_option, "sane_get_option_descriptor %d\n", option);

  if ((unsigned) option >= NUM_OPTIONS)
    {
      return 0;
    }

  return scanner->opt + option;
}


/* ---------------------------------- SANE CONTROL OPTION ------------------------ */


SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option, SANE_Action action,
		     void *val, SANE_Int * info)
{
  Pie_Scanner *scanner = handle;
  SANE_Status status;
  SANE_Word cap;
  SANE_String_Const name;

  if (info)
    {
      *info = 0;
    }

  if (scanner->scanning)
    {
      return SANE_STATUS_DEVICE_BUSY;
    }

  if ((unsigned) option >= NUM_OPTIONS)
    {
      return SANE_STATUS_INVAL;
    }

  cap = scanner->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      return SANE_STATUS_INVAL;
    }

  name = scanner->opt[option].name;
  if (!name)
    {
      name = "(no name)";
    }

  if (action == SANE_ACTION_GET_VALUE)
    {

      DBG (DBG_sane_option, "get %s [#%d]\n", name, option);

      switch (option)
	{
	  /* word options: */
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_PREVIEW:
	case OPT_THRESHOLD:
	  *(SANE_Word *) val = scanner->val[option].w;
	  return SANE_STATUS_GOOD;

	  /* word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (val, scanner->val[option].wa, scanner->opt[option].size);
	  return SANE_STATUS_GOOD;

#if 0
	  /* string options: */
	case OPT_SOURCE:
#endif
	case OPT_MODE:
	case OPT_HALFTONE_PATTERN:
	case OPT_SPEED:
	  strcpy (val, scanner->val[option].s);
	  return SANE_STATUS_GOOD;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      switch (scanner->opt[option].type)
	{
	case SANE_TYPE_INT:
	  DBG (DBG_sane_option, "set %s [#%d] to %d\n", name, option,
	       *(SANE_Word *) val);
	  break;

	case SANE_TYPE_FIXED:
	  DBG (DBG_sane_option, "set %s [#%d] to %f\n", name, option,
	       SANE_UNFIX (*(SANE_Word *) val));
	  break;

	case SANE_TYPE_STRING:
	  DBG (DBG_sane_option, "set %s [#%d] to %s\n", name, option,
	       (char *) val);
	  break;

	case SANE_TYPE_BOOL:
	  DBG (DBG_sane_option, "set %s [#%d] to %d\n", name, option,
	       *(SANE_Word *) val);
	  break;

	default:
	  DBG (DBG_sane_option, "set %s [#%d]\n", name, option);
	}

      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (scanner->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	{
	  return status;
	}

      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_PARAMS;
	    }
	  /* fall through */
	case OPT_NUM_OPTS:
	case OPT_PREVIEW:
	case OPT_THRESHOLD:
	  scanner->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	  /* side-effect-free word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (scanner->val[option].wa, val, scanner->opt[option].size);
	  return SANE_STATUS_GOOD;

	  /* options with side-effects: */

	case OPT_MODE:
	  {
	    int halftoning;

	    if (scanner->val[option].s)
	      {
		free (scanner->val[option].s);
	      }

	    scanner->val[option].s = (SANE_Char *) strdup (val);

	    if (info)
	      {
		*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	      }

	    scanner->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;


	    scanner->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	    scanner->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	    scanner->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	    scanner->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	    scanner->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;

	    halftoning = (strcmp (val, HALFTONE_STR) == 0);

	    if (halftoning || strcmp (val, LINEART_STR) == 0)
	      {			/* one bit modes */
		if (halftoning)
		  {		/* halftoning modes */
		    scanner->opt[OPT_HALFTONE_PATTERN].cap &=
		      ~SANE_CAP_INACTIVE;
		  }
		else
		  {		/* lineart modes */
		  }
		scanner->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
	      }
	    else
	      {			/* multi-bit modes(gray or color) */
	      }

	    if ((strcmp (val, LINEART_STR) == 0)
		|| (strcmp (val, HALFTONE_STR) == 0)
		|| (strcmp (val, GRAY_STR) == 0))
	      {
		scanner->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
	      }
	    else if (strcmp (val, COLOR_STR) == 0)
	      {
		/* scanner->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE; */
		scanner->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		scanner->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		scanner->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
	      }
	    return SANE_STATUS_GOOD;
	  }

	case OPT_SPEED:
	case OPT_HALFTONE_PATTERN:
	  {
	    if (scanner->val[option].s)
	      {
		free (scanner->val[option].s);
	      }

	    scanner->val[option].s = (SANE_Char *) strdup (val);

	    return SANE_STATUS_GOOD;
	  }
	}
    }				/* else */
  return SANE_STATUS_INVAL;
}


/* ------------------------------------ SANE GET PARAMETERS ------------------------ */


SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Pie_Scanner *scanner = handle;
  const char *mode;

  DBG (DBG_sane_info, "sane_get_parameters\n");

  if (!scanner->scanning)
    {				/* not scanning, so lets use recent values */
      double width, length, x_dpi, y_dpi;

      memset (&scanner->params, 0, sizeof (scanner->params));

      width =
	SANE_UNFIX (scanner->val[OPT_BR_X].w - scanner->val[OPT_TL_X].w);
      length =
	SANE_UNFIX (scanner->val[OPT_BR_Y].w - scanner->val[OPT_TL_Y].w);
      x_dpi = SANE_UNFIX (scanner->val[OPT_RESOLUTION].w);
      y_dpi = x_dpi;

#if 0
      if ((scanner->val[OPT_RESOLUTION_BIND].w == SANE_TRUE)
	  || (scanner->val[OPT_PREVIEW].w == SANE_TRUE))
	{
	  y_dpi = x_dpi;
	}
#endif
      if (x_dpi > 0.0 && y_dpi > 0.0 && width > 0.0 && length > 0.0)
	{
	  double x_dots_per_mm = x_dpi / MM_PER_INCH;
	  double y_dots_per_mm = y_dpi / MM_PER_INCH;

	  scanner->params.pixels_per_line = width * x_dots_per_mm;
	  scanner->params.lines = length * y_dots_per_mm;
	}
    }

  mode = scanner->val[OPT_MODE].s;

  if (strcmp (mode, LINEART_STR) == 0 || strcmp (mode, HALFTONE_STR) == 0)
    {
      scanner->params.format = SANE_FRAME_GRAY;
      scanner->params.bytes_per_line =
	(scanner->params.pixels_per_line + 7) / 8;
      scanner->params.depth = 1;
    }
  else if (strcmp (mode, GRAY_STR) == 0)
    {
      scanner->params.format = SANE_FRAME_GRAY;
      scanner->params.bytes_per_line = scanner->params.pixels_per_line;
      scanner->params.depth = 8;
    }
  else				/* RGB */
    {
      scanner->params.format = SANE_FRAME_RGB;
      scanner->params.bytes_per_line = 3 * scanner->params.pixels_per_line;
      scanner->params.depth = 8;
    }

  scanner->params.last_frame = (scanner->params.format != SANE_FRAME_RED
				&& scanner->params.format !=
				SANE_FRAME_GREEN);

  if (params)
    {
      *params = scanner->params;
    }

  return SANE_STATUS_GOOD;
}


/* ----------------------------------------- SANE START --------------------------------- */


SANE_Status
sane_start (SANE_Handle handle)
{
  Pie_Scanner *scanner = handle;
  int fds[2];
  const char *mode;
  int status;

  DBG (DBG_sane_init, "sane_start\n");

  /* Check for inconsistencies */

  if (scanner->val[OPT_TL_X].w > scanner->val[OPT_BR_X].w)
    {
      DBG (0, "sane_start: %s (%.1f mm) is bigger than %s (%.1f mm) "
              "-- aborting\n",
              scanner->opt[OPT_TL_X].title, SANE_UNFIX (scanner->val[OPT_TL_X].w),
              scanner->opt[OPT_BR_X].title, SANE_UNFIX (scanner->val[OPT_BR_X].w));
      return SANE_STATUS_INVAL;
    }
  if (scanner->val[OPT_TL_Y].w > scanner->val[OPT_BR_Y].w)
    {
      DBG (0, "sane_start: %s (%.1f mm) is bigger than %s (%.1f mm) "
	      "-- aborting\n",
	      scanner->opt[OPT_TL_Y].title, SANE_UNFIX (scanner->val[OPT_TL_Y].w),
	      scanner->opt[OPT_BR_Y].title, SANE_UNFIX (scanner->val[OPT_BR_Y].w));
      return SANE_STATUS_INVAL;
    }

  mode = scanner->val[OPT_MODE].s;

  if (scanner->sfd < 0)		/* first call, don`t run this routine again on multi frame or multi image scan */
    {
#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
      int scsi_bufsize = 131072;	/* 128KB */

      if (sanei_scsi_open_extended
	  (scanner->device->sane.name, &(scanner->sfd), sense_handler,
	   scanner->device, &scsi_bufsize) != 0)

	{
	  DBG (DBG_error, "sane_start: open failed\n");
	  return SANE_STATUS_INVAL;
	}

      if (scsi_bufsize < 32768)	/* < 32KB */
	{
	  DBG (DBG_error,
	       "sane_start: sanei_scsi_open_extended returned too small scsi buffer (%d)\n",
	       scsi_bufsize);
	  sanei_scsi_close ((scanner->sfd));
	  return SANE_STATUS_NO_MEM;
	}
      DBG (DBG_info,
	   "sane_start: sanei_scsi_open_extended returned scsi buffer size = %d\n",
	   scsi_bufsize);


      scanner->bufsize = scsi_bufsize;
#else
      if (sanei_scsi_open
	  (scanner->device->sane.name, &(scanner->sfd), sense_handler,
	   scanner->device) != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "sane_start: open of %s failed:\n",
	       scanner->device->sane.name);
	  return SANE_STATUS_INVAL;
	}

      /* there is no need to reallocate the buffer because the size is fixed */
#endif

#if 0
      if (pie_check_values (scanner->device) != 0)
	{
	  DBG (DBG_error, "ERROR: invalid scan-values\n");
	  scanner->scanning = SANE_FALSE;
	  pie_give_scanner (scanner);	/* reposition and release scanner */
	  sanei_scsi_close (scanner->sfd);
	  scanner->sfd = -1;
	  return SANE_STATUS_INVAL;
	}
#endif
#if 0
      scanner->params.bytes_per_line = scanner->device->row_len;
      scanner->params.pixels_per_line = scanner->device->width_in_pixels;
      scanner->params.lines = scanner->device->length_in_pixels;

      sane_get_parameters (scanner, 0);

      DBG (DBG_sane_info, "x_resolution (dpi)      = %u\n",
	   scanner->device->x_resolution);
      DBG (DBG_sane_info, "y_resolution (dpi)      = %u\n",
	   scanner->device->y_resolution);
      DBG (DBG_sane_info, "x_coordinate_base (dpi) = %u\n",
	   scanner->device->x_coordinate_base);
      DBG (DBG_sane_info, "y_coordinate_base (dpi) = %u\n",
	   scanner->device->y_coordinate_base);
      DBG (DBG_sane_info, "upper_left_x (xbase)    = %d\n",
	   scanner->device->upper_left_x);
      DBG (DBG_sane_info, "upper_left_y (ybase)    = %d\n",
	   scanner->device->upper_left_y);
      DBG (DBG_sane_info, "scanwidth    (xbase)    = %u\n",
	   scanner->device->scanwidth);
      DBG (DBG_sane_info, "scanlength   (ybase)    = %u\n",
	   scanner->device->scanlength);
      DBG (DBG_sane_info, "width in pixels         = %u\n",
	   scanner->device->width_in_pixels);
      DBG (DBG_sane_info, "length in pixels        = %u\n",
	   scanner->device->length_in_pixels);
      DBG (DBG_sane_info, "bits per pixel/color    = %u\n",
	   scanner->device->bits_per_pixel);
      DBG (DBG_sane_info, "bytes per line          = %d\n",
	   scanner->params.bytes_per_line);
      DBG (DBG_sane_info, "pixels_per_line         = %d\n",
	   scanner->params.pixels_per_line);
      DBG (DBG_sane_info, "lines                   = %d\n",
	   scanner->params.lines);
#endif

      /* grab scanner */
      if (pie_grab_scanner (scanner))
	{
	  sanei_scsi_close (scanner->sfd);
	  scanner->sfd = -1;
	  DBG (DBG_warning,
	       "WARNING: unable to reserve scanner: device busy\n");
	  return SANE_STATUS_DEVICE_BUSY;
	}

      scanner->scanning = SANE_TRUE;

      pie_power_save (scanner, 0);
    }				/* ------------ end of first call -------------- */


  if (strcmp (mode, LINEART_STR) == 0)
    {
      scanner->colormode = LINEART;
    }
  else if (strcmp (mode, HALFTONE_STR) == 0)
    {
      scanner->colormode = HALFTONE;
    }
  else if (strcmp (mode, GRAY_STR) == 0)
    {
      scanner->colormode = GRAYSCALE;
    }
  else if (strcmp (mode, COLOR_STR) == 0)
    {
      scanner->colormode = RGB;
    }

  /* get and set geometric values for scanning */
  scanner->resolution = SANE_UNFIX (scanner->val[OPT_RESOLUTION].w);

  pie_set_window (scanner);
  pie_send_exposure (scanner);
  pie_mode_select (scanner);
  pie_send_highlight_shadow (scanner);

  pie_scan (scanner, 1);

  status = pie_do_cal (scanner);
  if (status)
    return status;

  /* send gammacurves */

  pie_dwnld_gamma (scanner);

  pie_get_params (scanner);

  if (pipe (fds) < 0)		/* create a pipe, fds[0]=read-fd, fds[1]=write-fd */
    {
      DBG (DBG_error, "ERROR: could not create pipe\n");
      scanner->scanning = SANE_FALSE;
      pie_scan (scanner, 0);
      pie_give_scanner (scanner);	/* reposition and release scanner */
      sanei_scsi_close (scanner->sfd);
      scanner->sfd = -1;
      return SANE_STATUS_IO_ERROR;
    }

  scanner->pipe       = fds[0];
  scanner->reader_fds = fds[1];
  scanner->reader_pid = sanei_thread_begin( reader_process, (void*)scanner );

  if (scanner->reader_pid == -1)
    {
      DBG (1, "sane_start: sanei_thread_begin failed (%s)\n",
             strerror (errno));
      return SANE_STATUS_NO_MEM;
    }

  if (sanei_thread_is_forked ())
    {
      close (scanner->reader_fds);
      scanner->reader_fds = -1;
    }

  return SANE_STATUS_GOOD;
}


/* -------------------------------------- SANE READ ---------------------------------- */


SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  Pie_Scanner *scanner = handle;
  ssize_t nread;

  *len = 0;

  nread = read (scanner->pipe, buf, max_len);
  DBG (DBG_sane_info, "sane_read: read %ld bytes\n", (long) nread);

  if (!(scanner->scanning))	/* OOPS, not scanning */
    {
      return do_cancel (scanner);
    }

  if (nread < 0)
    {
      if (errno == EAGAIN)
	{
	  DBG (DBG_sane_info, "sane_read: EAGAIN\n");
	  return SANE_STATUS_GOOD;
	}
      else
	{
	  do_cancel (scanner);	/* we had an error, stop scanner */
	  return SANE_STATUS_IO_ERROR;
	}
    }

  *len = nread;

  if (nread == 0)		/* EOF */
    {
      do_cancel (scanner);

      return close_pipe (scanner);	/* close pipe */
    }

  return SANE_STATUS_GOOD;
}


/* ------------------------------------- SANE CANCEL -------------------------------- */


void
sane_cancel (SANE_Handle handle)
{
  Pie_Scanner *scanner = handle;

  DBG (DBG_sane_init, "sane_cancel\n");

  if (scanner->scanning)
    {
      do_cancel (scanner);
    }
}


/* -------------------------------------- SANE SET IO MODE --------------------------- */


SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Pie_Scanner *scanner = handle;

  DBG (DBG_sane_init, "sane_set_io_mode: non_blocking=%d\n", non_blocking);

  if (!scanner->scanning)
    {
      return SANE_STATUS_INVAL;
    }

  if (fcntl (scanner->pipe, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    {
      return SANE_STATUS_IO_ERROR;
    }

  return SANE_STATUS_GOOD;
}


/* --------------------------------------- SANE GET SELECT FD ------------------------- */


SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  Pie_Scanner *scanner = handle;

  DBG (DBG_sane_init, "sane_get_select_fd\n");

  if (!scanner->scanning)
    {
      return SANE_STATUS_INVAL;
    }
  *fd = scanner->pipe;

  return SANE_STATUS_GOOD;
}
