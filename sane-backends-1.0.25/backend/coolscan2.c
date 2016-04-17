/* ========================================================================= */
/*
   SANE - Scanner Access Now Easy.
   coolscan2.c , version 0.1.8

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

   This file implements a SANE backend for Nikon Coolscan film scanners.

   Written by András Major (andras@users.sourceforge.net), 2001-2002.

   The developers wish to express their thanks to Nikon Corporation
   for providing technical information and thus making this backend
   possible.
*/
/* ========================================================================= */


/* ========================================================================= */
/*
   Revision log:

   0.1.9, 20/10/2005, ariel: added support for the LS-50/5000
   0.1.8, 27/09/2002, andras: added subframe and load options
   0.1.7, 22/08/2002, andras: added exposure correction option
                                and hack for LS-40 IR readout
   0.1.6, 14/06/2002, andras: types etc. fixed, fixes for LS-8000
   0.1.5, 26/04/2002, andras: lots of minor fixes related to saned
   0.1.4, 22/04/2002, andras: first version to be included in SANE CVS

*/
/* ========================================================================= */

#ifdef _AIX
# include "../include/lalloca.h"	/* MUST come first for AIX! */
#endif
#include "../include/sane/config.h"
#include "../include/lalloca.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
/*
#include <limits.h>
#include <sys/types.h>
*/

#include "../include/_stdint.h"

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_config.h"
#define BACKEND_NAME coolscan2
#include "../include/sane/sanei_backend.h"	/* must be last */

#define CS2_VERSION_MAJOR 0
#define CS2_VERSION_MINOR 1
#define CS2_REVISION 8
#define CS2_CONFIG_FILE "coolscan2.conf"

#define WSIZE (sizeof (SANE_Word))

/*
#define CS2_BLEEDING_EDGE
*/


/* ========================================================================= */
/* typedefs */

typedef enum
{
  CS2_TYPE_UNKOWN,
  CS2_TYPE_LS30,
  CS2_TYPE_LS40,
  CS2_TYPE_LS50,
  CS2_TYPE_LS2000,
  CS2_TYPE_LS4000,
  CS2_TYPE_LS5000,
  CS2_TYPE_LS8000
}
cs2_type_t;

typedef enum
{
  CS2_INTERFACE_UNKNOWN,
  CS2_INTERFACE_SCSI,		/* includes IEEE1394 via SBP2 */
  CS2_INTERFACE_USB
}
cs2_interface_t;

typedef enum
{
  CS2_PHASE_NONE = 0x00,
  CS2_PHASE_STATUS = 0x01,
  CS2_PHASE_OUT = 0x02,
  CS2_PHASE_IN = 0x03,
  CS2_PHASE_BUSY = 0x04
}
cs2_phase_t;

typedef enum
{
  CS2_SCAN_NORMAL,
  CS2_SCAN_AE,
  CS2_SCAN_AE_WB
}
cs2_scan_t;

typedef enum
{
  CS2_INFRARED_OFF,
  CS2_INFRARED_IN,
  CS2_INFRARED_OUT
}
cs2_infrared_t;

typedef enum
{
  CS2_STATUS_READY = 0,
  CS2_STATUS_BUSY = 1,
  CS2_STATUS_NO_DOCS = 2,
  CS2_STATUS_PROCESSING = 4,
  CS2_STATUS_ERROR = 8,
  CS2_STATUS_REISSUE = 16,
  CS2_STATUS_ALL = 31		/* sum of all others */
}
cs2_status_t;

typedef enum
{
  CS2_OPTION_NUM = 0,

  CS2_OPTION_PREVIEW,

  CS2_OPTION_NEGATIVE,

  CS2_OPTION_INFRARED,

  CS2_OPTION_SAMPLES_PER_SCAN,

  CS2_OPTION_DEPTH,

  CS2_OPTION_EXPOSURE,
  CS2_OPTION_EXPOSURE_R,
  CS2_OPTION_EXPOSURE_G,
  CS2_OPTION_EXPOSURE_B,
  CS2_OPTION_SCAN_AE,
  CS2_OPTION_SCAN_AE_WB,

  CS2_OPTION_LUT_R,
  CS2_OPTION_LUT_G,
  CS2_OPTION_LUT_B,

  CS2_OPTION_RES,
  CS2_OPTION_RESX,
  CS2_OPTION_RESY,
  CS2_OPTION_RES_INDEPENDENT,

  CS2_OPTION_PREVIEW_RESOLUTION,

  CS2_OPTION_FRAME,
  CS2_OPTION_SUBFRAME,
  CS2_OPTION_XMIN,
  CS2_OPTION_XMAX,
  CS2_OPTION_YMIN,
  CS2_OPTION_YMAX,

  CS2_OPTION_LOAD,
  CS2_OPTION_EJECT,
  CS2_OPTION_RESET,

  CS2_OPTION_FOCUS_ON_CENTRE,
  CS2_OPTION_FOCUS,
  CS2_OPTION_AUTOFOCUS,
  CS2_OPTION_FOCUSX,
  CS2_OPTION_FOCUSY,

  CS2_N_OPTIONS			/* must be last -- counts number of enum items */
}
cs2_option_t;

typedef unsigned int cs2_pixel_t;

typedef struct
{
  /* interface */
  cs2_interface_t interface;
  int fd;
  SANE_Byte *send_buf, *recv_buf;
  size_t send_buf_size, recv_buf_size;
  size_t n_cmd, n_send, n_recv;

  /* device characteristics */
  char vendor_string[9], product_string[17], revision_string[5];
  cs2_type_t type;
  int maxbits;
  unsigned int resx_optical, resx_min, resx_max, *resx_list, resx_n_list;
  unsigned int resy_optical, resy_min, resy_max, *resy_list, resy_n_list;
  unsigned long boundaryx, boundaryy;
  unsigned long frame_offset;
  unsigned int unit_dpi;
  double unit_mm;
  int n_frames;

  int focus_min, focus_max;

  /* settings */
  SANE_Bool preview, negative, infrared;
  int samples_per_scan, depth, real_depth, bytes_per_pixel, shift_bits,
	n_colour_in, n_colour_out;
  cs2_pixel_t n_lut;
  cs2_pixel_t *lut_r, *lut_g, *lut_b, *lut_neutral;
  unsigned long resx, resy, res, res_independent, res_preview;
  unsigned long xmin, xmax, ymin, ymax;
  int i_frame;
  double subframe;

  unsigned int real_resx, real_resy, real_pitchx, real_pitchy;
  unsigned long real_xoffset, real_yoffset, real_width, real_height,
    logical_width, logical_height;
  int odd_padding;
  int block_padding;

  double exposure, exposure_r, exposure_g, exposure_b;
  unsigned long real_exposure[10];

  SANE_Bool focus_on_centre;
  unsigned long focusx, focusy, real_focusx, real_focusy;
  int focus;

  /* status */
  SANE_Bool scanning;
  cs2_infrared_t infrared_stage, infrared_next;
  SANE_Byte *infrared_buf;
  size_t n_infrared_buf, infrared_index;
  SANE_Byte *line_buf;
  ssize_t n_line_buf, i_line_buf;
  unsigned long sense_key, sense_asc, sense_ascq, sense_info;
  unsigned long sense_code;
  cs2_status_t status;
  size_t xfer_position, xfer_bytes_total;

  /* SANE stuff */
  SANE_Option_Descriptor option_list[CS2_N_OPTIONS];
}
cs2_t;


/* ========================================================================= */
/* prototypes */

static SANE_Status cs2_open (const char *device, cs2_interface_t interface,
			     cs2_t ** sp);
static void cs2_close (cs2_t * s);
static SANE_Status cs2_attach (const char *dev);
static SANE_Status cs2_scsi_sense_handler (int fd, u_char * sense_buffer,
					   void *arg);
static SANE_Status cs2_parse_sense_data (cs2_t * s);
static void cs2_init_buffer (cs2_t * s);
static SANE_Status cs2_pack_byte (cs2_t * s, SANE_Byte byte);
static SANE_Status cs2_parse_cmd (cs2_t * s, char *text);
static SANE_Status cs2_grow_send_buffer (cs2_t * s);
static SANE_Status cs2_issue_cmd (cs2_t * s);
static cs2_phase_t cs2_phase_check (cs2_t * s);
static SANE_Status cs2_set_boundary (cs2_t *s);
static SANE_Status cs2_scanner_ready (cs2_t * s, int flags);
static SANE_Status cs2_page_inquiry (cs2_t * s, int page);
static SANE_Status cs2_full_inquiry (cs2_t * s);
static SANE_Status cs2_execute (cs2_t * s);
static SANE_Status cs2_load (cs2_t * s);
static SANE_Status cs2_eject (cs2_t * s);
static SANE_Status cs2_reset (cs2_t * s);
static SANE_Status cs2_focus (cs2_t * s);
static SANE_Status cs2_autofocus (cs2_t * s);
static SANE_Status cs2_get_exposure (cs2_t * s);
static SANE_Status cs2_convert_options (cs2_t * s);
static SANE_Status cs2_scan (cs2_t * s, cs2_scan_t type);
static void *cs2_xmalloc (size_t size);
static void *cs2_xrealloc (void *p, size_t size);
static void cs2_xfree (const void *p);


/* ========================================================================= */
/* global variables */

static int cs2_colour_list[] = { 1, 2, 3, 9 };

static SANE_Device **device_list = NULL;
static int n_device_list = 0;
static cs2_interface_t try_interface = CS2_INTERFACE_UNKNOWN;
static int open_devices = 0;


/* ========================================================================= */
/* SANE entry points */

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  DBG_INIT ();
  DBG (10, "sane_init() called.\n");
  DBG (1, "coolscan2 backend, version %i.%i.%i initializing.\n", CS2_VERSION_MAJOR, CS2_VERSION_MINOR, CS2_REVISION);

  authorize = authorize;	/* to shut up compiler */

  if (version_code)
    *version_code =
      SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

  sanei_usb_init ();

  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  int i;

  DBG (10, "sane_exit() called.\n");

  for (i = 0; i < n_device_list; i++)
    {
      cs2_xfree (device_list[i]->name);
      cs2_xfree (device_list[i]->vendor);
      cs2_xfree (device_list[i]->model);
      cs2_xfree (device_list[i]);
    }
  cs2_xfree (device_list);
}

SANE_Status
sane_get_devices (const SANE_Device *** list, SANE_Bool local_only)
{
  char line[PATH_MAX], *p;
  FILE *config;

  local_only = local_only;	/* to shut up compiler */

  DBG (10, "sane_get_devices() called.\n");

  if (device_list)
    DBG (6,
	 "sane_get_devices(): Device list already populated, not probing again.\n");
  else
    {
      if (open_devices)
	{
	  DBG (4,
	       "sane_get_devices(): Devices open, not scanning for scanners.\n");
	  return SANE_STATUS_IO_ERROR;
	}

      config = sanei_config_open (CS2_CONFIG_FILE);
      if (config)
	{
	  DBG (4, "sane_get_devices(): Reading config file.\n");
	  while (sanei_config_read (line, sizeof (line), config))
	    {
	      p = line;
	      p += strspn (line, " \t");
	      if (strlen (p) && (p[0] != '\n') && (p[0] != '#'))
		cs2_open (line, CS2_INTERFACE_UNKNOWN, NULL);
	    }
	  fclose (config);
	}
      else
	{
	  DBG (4, "sane_get_devices(): No config file found.\n");
	  cs2_open ("auto", CS2_INTERFACE_UNKNOWN, NULL);
	}

      switch (n_device_list)
	{
	case 0:
	  DBG (6, "sane_get_devices(): No devices detected.\n");
	  break;
	case 1:
	  DBG (6, "sane_get_devices(): 1 device detected.\n");
	  break;
	default:
	  DBG (6, "sane_get_devices(): %i devices detected.\n",
	       n_device_list);
	  break;
	}
    }

  *list = (const SANE_Device **) device_list;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * h)
{
  SANE_Status status;
  cs2_t *s;
  int i_option;
  unsigned int i_list;
  SANE_Option_Descriptor o;
  SANE_Word *word_list;
  SANE_Range *range = NULL;
  int alloc_failed = 0;

  DBG (10, "sane_open() called.\n");

  status = cs2_open (name, CS2_INTERFACE_UNKNOWN, &s);
  if (status)
    return status;

  *h = (SANE_Handle) s;

  /* get device properties */

  s->lut_r = s->lut_g = s->lut_b = s->lut_neutral = NULL;
  s->resx_list = s->resy_list = NULL;
  s->resx_n_list = s->resy_n_list = 0;

  status = cs2_full_inquiry (s);
  if (status)
    return status;

  /* option descriptors */

  for (i_option = 0; i_option < CS2_N_OPTIONS; i_option++)
    {
      o.name = o.title = o.desc = NULL;
      o.type = o.unit = o.cap = o.constraint_type = o.size = 0;
      o.constraint.range = NULL;	/* only one union member needs to be NULLed */
      switch (i_option)
	{
	case CS2_OPTION_NUM:
	  o.name = "";
	  o.title = SANE_TITLE_NUM_OPTIONS;
	  o.desc = SANE_DESC_NUM_OPTIONS;
	  o.type = SANE_TYPE_INT;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_DETECT;
	  break;
	case CS2_OPTION_PREVIEW:
	  o.name = "preview";
	  o.title = "Preview mode";
	  o.desc = "Preview mode";
	  o.type = SANE_TYPE_BOOL;
	  o.size = WSIZE;
	  o.cap =
	    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
	  break;
	case CS2_OPTION_NEGATIVE:
	  o.name = "negative";
	  o.title = "Negative";
	  o.desc = "Negative film: make scanner invert colours";
	  o.type = SANE_TYPE_BOOL;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
#ifndef CS2_BLEEDING_EDGE
	  o.cap |= SANE_CAP_INACTIVE;
#endif
	  break;
	case CS2_OPTION_INFRARED:
	  o.name = "infrared";
	  o.title = "Read infrared channel";
	  o.desc = "Read infrared channel in addition to scan colours";
	  o.type = SANE_TYPE_BOOL;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  break;
	case CS2_OPTION_SAMPLES_PER_SCAN:
	  o.name = "samples-per-scan";
	  o.title = "Samples per Scan";
	  o.desc = "Number of samples per scan";
	  o.type = SANE_TYPE_INT;
	  o.unit = SANE_UNIT_NONE;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  if (s->type != CS2_TYPE_LS2000 && s->type != CS2_TYPE_LS4000
		  && s->type != CS2_TYPE_LS5000 && s->type != CS2_TYPE_LS8000)
		o.cap |= SANE_CAP_INACTIVE;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (! range)
		alloc_failed = 1;
	  else
		{
		  range->min = 1;
		  range->max = 16;
		  range->quant = 1;
		  o.constraint.range = range;
		}
	  break;
	case CS2_OPTION_DEPTH:
	  o.name = "depth";
	  o.title = "Bit depth per channel";
	  o.desc = "Number of bits output by scanner for each channel";
	  o.type = SANE_TYPE_INT;
	  o.unit = SANE_UNIT_NONE;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_WORD_LIST;
	  word_list = (SANE_Word *) cs2_xmalloc (2 * sizeof (SANE_Word));
	  if (!word_list)
	    alloc_failed = 1;
	  else
	    {
	      word_list[1] = 8;
	      word_list[2] = s->maxbits;
	      word_list[0] = 2;
	      o.constraint.word_list = word_list;
	    }
	  break;
	case CS2_OPTION_EXPOSURE:
	  o.name = "exposure";
	  o.title = "Exposure multiplier";
	  o.desc = "Exposure multiplier for all channels";
	  o.type = SANE_TYPE_FIXED;
	  o.unit = SANE_UNIT_NONE;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = SANE_FIX (0.);
	      range->max = SANE_FIX (10.);
	      range->quant = SANE_FIX (0.1);
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_EXPOSURE_R:
	  o.name = "red-exposure";
	  o.title = "Red exposure time";
	  o.desc = "Exposure time for red channel";
	  o.type = SANE_TYPE_FIXED;
	  o.unit = SANE_UNIT_MICROSECOND;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = SANE_FIX (50.);
	      range->max = SANE_FIX (20000.);
	      range->quant = SANE_FIX (10.);
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_EXPOSURE_G:
	  o.name = "green-exposure";
	  o.title = "Green exposure time";
	  o.desc = "Exposure time for green channel";
	  o.type = SANE_TYPE_FIXED;
	  o.unit = SANE_UNIT_MICROSECOND;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = SANE_FIX (50.);
	      range->max = SANE_FIX (20000.);
	      range->quant = SANE_FIX (10.);
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_EXPOSURE_B:
	  o.name = "blue-exposure";
	  o.title = "Blue exposure time";
	  o.desc = "Exposure time for blue channel";
	  o.type = SANE_TYPE_FIXED;
	  o.unit = SANE_UNIT_MICROSECOND;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = SANE_FIX (50.);
	      range->max = SANE_FIX (20000.);
	      range->quant = SANE_FIX (10.);
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_LUT_R:
	  o.name = "red-gamma-table";
	  o.title = "LUT for red channel";
	  o.desc = "LUT for red channel";
	  o.type = SANE_TYPE_INT;
	  o.size = s->n_lut * WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = 0;
	      range->max = s->n_lut - 1;
	      range->quant = 1;
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_LUT_G:
	  o.name = "green-gamma-table";
	  o.title = "LUT for green channel";
	  o.desc = "LUT for green channel";
	  o.type = SANE_TYPE_INT;
	  o.size = s->n_lut * WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = 0;
	      range->max = s->n_lut - 1;
	      range->quant = 1;
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_LUT_B:
	  o.name = "blue-gamma-table";
	  o.title = "LUT for blue channel";
	  o.desc = "LUT for blue channel";
	  o.type = SANE_TYPE_INT;
	  o.size = s->n_lut * WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = 0;
	      range->max = s->n_lut - 1;
	      range->quant = 1;
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_LOAD:
	  o.name = "load";
	  o.title = "Load";
	  o.desc = "Load next slide";
	  o.type = SANE_TYPE_BUTTON;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  break;
	case CS2_OPTION_EJECT:
	  o.name = "eject";
	  o.title = "Eject";
	  o.desc = "Eject loaded medium";
	  o.type = SANE_TYPE_BUTTON;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  break;
	case CS2_OPTION_RESET:
	  o.name = "reset";
	  o.title = "Reset scanner";
	  o.desc = "Initialize scanner";
	  o.type = SANE_TYPE_BUTTON;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  break;
	case CS2_OPTION_RESX:
	case CS2_OPTION_RES:
	case CS2_OPTION_PREVIEW_RESOLUTION:
	  if (i_option == CS2_OPTION_PREVIEW_RESOLUTION)
	    {
	      o.name = "preview-resolution";
	      o.title = "Preview resolution";
	      o.desc =
		"Scanning resolution for preview mode in dpi, affecting both x and y directions";
	    }
	  else if (i_option == CS2_OPTION_RES)
	    {
	      o.name = "resolution";
	      o.title = "Resolution";
	      o.desc =
		"Scanning resolution in dpi, affecting both x and y directions";
	    }
	  else
	    {
	      o.name = "x-resolution";
	      o.title = "X resolution";
	      o.desc =
		"Scanning resolution in dpi, affecting x direction only";
	    }
	  o.type = SANE_TYPE_INT;
	  o.unit = SANE_UNIT_DPI;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  if (i_option == CS2_OPTION_RESX)
	    o.cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
	  if (i_option == CS2_OPTION_PREVIEW_RESOLUTION)
	    o.cap |= SANE_CAP_ADVANCED;
	  o.constraint_type = SANE_CONSTRAINT_WORD_LIST;
	  word_list =
	    (SANE_Word *) cs2_xmalloc ((s->resx_n_list + 1) *
				       sizeof (SANE_Word));
	  if (!word_list)
	    alloc_failed = 1;
	  else
	    {
	      for (i_list = 0; i_list < s->resx_n_list; i_list++)
		word_list[i_list + 1] = s->resx_list[i_list];
	      word_list[0] = s->resx_n_list;
	      o.constraint.word_list = word_list;
	    }
	  break;
	case CS2_OPTION_RESY:
	  o.name = "y-resolution";
	  o.title = "Y resolution";
	  o.desc = "Scanning resolution in dpi, affecting y direction only";
	  o.type = SANE_TYPE_INT;
	  o.unit = SANE_UNIT_DPI;
	  o.size = WSIZE;
	  o.cap =
	    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE |
	    SANE_CAP_ADVANCED;
	  o.constraint_type = SANE_CONSTRAINT_WORD_LIST;
	  word_list =
	    (SANE_Word *) cs2_xmalloc ((s->resy_n_list + 1) *
				       sizeof (SANE_Word));
	  if (!word_list)
	    alloc_failed = 1;
	  else
	    {
	      for (i_list = 0; i_list < s->resy_n_list; i_list++)
		word_list[i_list + 1] = s->resy_list[i_list];
	      word_list[0] = s->resy_n_list;
	      o.constraint.word_list = word_list;
	    }
	  break;
	case CS2_OPTION_RES_INDEPENDENT:
	  o.name = "independent-res";
	  o.title = "Independent x/y resolutions";
	  o.desc =
	    "Enable independent controls for scanning resolution in x and y direction";
	  o.type = SANE_TYPE_BOOL;
	  o.size = WSIZE;
	  o.cap =
	    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE |
	    SANE_CAP_ADVANCED;
	  break;
	case CS2_OPTION_FRAME:
	  o.name = "frame";
	  o.title = "Frame number";
	  o.desc = "Number of frame to be scanned, starting with 1";
	  o.type = SANE_TYPE_INT;
	  o.unit = SANE_UNIT_NONE;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  if (s->n_frames <= 1)
	    o.cap |= SANE_CAP_INACTIVE;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = 1;
	      range->max = s->n_frames;
	      range->quant = 1;
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_SUBFRAME:
	  o.name = "subframe";
	  o.title = "Frame shift";
	  o.desc = "Fine position within the selected frame";
	  o.type = SANE_TYPE_FIXED;
	  o.unit = SANE_UNIT_MM;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = SANE_FIX (0.);
	      range->max = SANE_FIX ((s->boundaryy - 1) * s->unit_mm);
	      range->quant = SANE_FIX (0.);
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_XMIN:
	  o.name = "tl-x";
	  o.title = "Left x value of scan area";
	  o.desc = "Left x value of scan area";
	  o.type = SANE_TYPE_INT;
	  o.unit = SANE_UNIT_PIXEL;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	      range->min = 0;
	      range->max = s->boundaryx - 1;
	      range->quant = 1;
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_XMAX:
	  o.name = "br-x";
	  o.title = "Right x value of scan area";
	  o.desc = "Right x value of scan area";
	  o.type = SANE_TYPE_INT;
	  o.unit = SANE_UNIT_PIXEL;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = 0;
	      range->max = s->boundaryx - 1;
	      range->quant = 1;
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_YMIN:
	  o.name = "tl-y";
	  o.title = "Top y value of scan area";
	  o.desc = "Top y value of scan area";
	  o.type = SANE_TYPE_INT;
	  o.unit = SANE_UNIT_PIXEL;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = 0;
	      range->max = s->boundaryy - 1;
	      range->quant = 1;
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_YMAX:
	  o.name = "br-y";
	  o.title = "Bottom y value of scan area";
	  o.desc = "Bottom y value of scan area";
	  o.type = SANE_TYPE_INT;
	  o.unit = SANE_UNIT_PIXEL;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = 0;
	      range->max = s->boundaryy - 1;
	      range->quant = 1;
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_FOCUS_ON_CENTRE:
	  o.name = "focus-on-centre";
	  o.title = "Use centre of scan area as AF point";
	  o.desc =
	    "Use centre of scan area as AF point instead of manual AF point selection";
	  o.type = SANE_TYPE_BOOL;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  break;
	case CS2_OPTION_FOCUS:
	  o.name = "focus";
	  o.title = "Focus position";
	  o.desc = "Focus position for manual focus";
	  o.type = SANE_TYPE_INT;
	  o.unit = SANE_UNIT_NONE;
	  o.size = WSIZE;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = s->focus_min;
	      range->max = s->focus_max;
	      range->quant = 1;
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_AUTOFOCUS:
	  o.name = "autofocus";
	  o.title = "Autofocus now";
	  o.desc = "Autofocus now";
	  o.type = SANE_TYPE_BUTTON;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  break;
	case CS2_OPTION_FOCUSX:
	  o.name = "focusx";
	  o.title = "X coordinate of AF point";
	  o.desc = "X coordinate of AF point";
	  o.type = SANE_TYPE_INT;
	  o.unit = SANE_UNIT_PIXEL;
	  o.size = WSIZE;
	  o.cap =
	    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = 0;
	      range->max = s->boundaryx - 1;
	      range->quant = 1;
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_FOCUSY:
	  o.name = "focusy";
	  o.title = "Y coordinate of AF point";
	  o.desc = "Y coordinate of AF point";
	  o.type = SANE_TYPE_INT;
	  o.unit = SANE_UNIT_PIXEL;
	  o.size = WSIZE;
	  o.cap =
	    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
	  o.constraint_type = SANE_CONSTRAINT_RANGE;
	  range = (SANE_Range *) cs2_xmalloc (sizeof (SANE_Range));
	  if (!range)
	    alloc_failed = 1;
	  else
	    {
	      range->min = 0;
	      range->max = s->boundaryy - 1;
	      range->quant = 1;
	      o.constraint.range = range;
	    }
	  break;
	case CS2_OPTION_SCAN_AE:
	  o.name = "ae";
	  o.title = "Auto-exposure scan now";
	  o.desc = "Perform auto-exposure scan";
	  o.type = SANE_TYPE_BUTTON;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  break;
	case CS2_OPTION_SCAN_AE_WB:
	  o.name = "ae-wb";
	  o.title = "Auto-exposure scan with white balance now";
	  o.desc = "Perform auto-exposure scan with white balance";
	  o.type = SANE_TYPE_BUTTON;
	  o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	  break;
	default:
	  DBG (1, "BUG: sane_open(): Unknown option number.\n");
	  return SANE_STATUS_INVAL;
	  break;
	}
      s->option_list[i_option] = o;
    }

  s->scanning = SANE_FALSE;
  s->preview = SANE_FALSE;
  s->negative = SANE_FALSE;
  s->depth = 8;
  s->infrared = 0;
  s->samples_per_scan = 1;
  s->i_frame = 1;
  s->subframe = 0.;
  s->res = s->resx = s->resx_max;
  s->resy = s->resy_max;
  s->res_independent = SANE_FALSE;
  s->res_preview = s->resx_max / 10;
  if (s->res_preview < s->resx_min)
    s->res_preview = s->resx_min;
  s->xmin = 0;
  s->xmax = s->boundaryx - 1;
  s->ymin = 0;
  s->ymax = s->boundaryy - 1;
  s->focus_on_centre = SANE_TRUE;
  s->focus = 0;
  s->focusx = 0;
  s->focusy = 0;
  s->exposure = 1.;
  s->exposure_r = 1200.;
  s->exposure_g = 1200.;
  s->exposure_b = 1000.;
  s->infrared_stage = CS2_INFRARED_OFF;
  s->infrared_next = CS2_INFRARED_OFF;
  s->infrared_buf = NULL;
  s->n_infrared_buf = 0;
  s->line_buf = NULL;
  s->n_line_buf = 0;

  if (alloc_failed)
    {
      cs2_close (s);
      return SANE_STATUS_NO_MEM;
    }

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle h)
{
  cs2_t *s = (cs2_t *) h;

  DBG (10, "sane_close() called.\n");

  cs2_close (s);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle h, SANE_Int n)
{
  cs2_t *s = (cs2_t *) h;

  DBG (10, "sane_get_option_descriptor() called, option #%i.\n", n);

  if ((n >= 0) && (n < CS2_N_OPTIONS))
    return &s->option_list[n];
  else
    return NULL;
}

SANE_Status
sane_control_option (SANE_Handle h, SANE_Int n, SANE_Action a, void *v,
		     SANE_Int * i)
{
  cs2_t *s = (cs2_t *) h;
  SANE_Int flags = 0;
  cs2_pixel_t pixel;
  SANE_Status status;
  SANE_Option_Descriptor o = s->option_list[n];

  DBG (10, "sane_control_option() called, option #%i, action #%i.\n", n, a);

  switch (a)
    {
    case SANE_ACTION_GET_VALUE:

      switch (n)
	{
	case CS2_OPTION_NUM:
	  *(SANE_Word *) v = CS2_N_OPTIONS;
	  break;
	case CS2_OPTION_NEGATIVE:
	  *(SANE_Word *) v = s->negative;
	  break;
	case CS2_OPTION_INFRARED:
	  *(SANE_Word *) v = s->infrared;
	  break;
	case CS2_OPTION_SAMPLES_PER_SCAN:
	  *(SANE_Word *) v = s->samples_per_scan;
	  break;
	case CS2_OPTION_DEPTH:
	  *(SANE_Word *) v = s->depth;
	  break;
	case CS2_OPTION_PREVIEW:
	  *(SANE_Word *) v = s->preview;
	  break;
	case CS2_OPTION_EXPOSURE:
	  *(SANE_Word *) v = SANE_FIX (s->exposure);
	  break;
	case CS2_OPTION_EXPOSURE_R:
	  *(SANE_Word *) v = SANE_FIX (s->exposure_r);
	  break;
	case CS2_OPTION_EXPOSURE_G:
	  *(SANE_Word *) v = SANE_FIX (s->exposure_g);
	  break;
	case CS2_OPTION_EXPOSURE_B:
	  *(SANE_Word *) v = SANE_FIX (s->exposure_b);
	  break;
	case CS2_OPTION_LUT_R:
	  if (!(s->lut_r))
	    return SANE_STATUS_INVAL;
	  for (pixel = 0; pixel < s->n_lut; pixel++)
	    ((SANE_Word *) v)[pixel] = s->lut_r[pixel];
	  break;
	case CS2_OPTION_LUT_G:
	  if (!(s->lut_g))
	    return SANE_STATUS_INVAL;
	  for (pixel = 0; pixel < s->n_lut; pixel++)
	    ((SANE_Word *) v)[pixel] = s->lut_g[pixel];
	  break;
	case CS2_OPTION_LUT_B:
	  if (!(s->lut_b))
	    return SANE_STATUS_INVAL;
	  for (pixel = 0; pixel < s->n_lut; pixel++)
	    ((SANE_Word *) v)[pixel] = s->lut_b[pixel];
	  break;
	case CS2_OPTION_EJECT:
	  break;
	case CS2_OPTION_LOAD:
	  break;
	case CS2_OPTION_RESET:
	  break;
	case CS2_OPTION_FRAME:
	  *(SANE_Word *) v = s->i_frame;
	  break;
	case CS2_OPTION_SUBFRAME:
	  *(SANE_Word *) v = SANE_FIX (s->subframe);
	  break;
	case CS2_OPTION_RES:
	  *(SANE_Word *) v = s->res;
	  break;
	case CS2_OPTION_RESX:
	  *(SANE_Word *) v = s->resx;
	  break;
	case CS2_OPTION_RESY:
	  *(SANE_Word *) v = s->resy;
	  break;
	case CS2_OPTION_RES_INDEPENDENT:
	  *(SANE_Word *) v = s->res_independent;
	  break;
	case CS2_OPTION_PREVIEW_RESOLUTION:
	  *(SANE_Word *) v = s->res_preview;
	  break;
	case CS2_OPTION_XMIN:
	  *(SANE_Word *) v = s->xmin;
	  break;
	case CS2_OPTION_XMAX:
	  *(SANE_Word *) v = s->xmax;
	  break;
	case CS2_OPTION_YMIN:
	  *(SANE_Word *) v = s->ymin;
	  break;
	case CS2_OPTION_YMAX:
	  *(SANE_Word *) v = s->ymax;
	  break;
	case CS2_OPTION_FOCUS_ON_CENTRE:
	  *(SANE_Word *) v = s->focus_on_centre;
	  break;
	case CS2_OPTION_FOCUS:
	  *(SANE_Word *) v = s->focus;
	  break;
	case CS2_OPTION_AUTOFOCUS:
	  break;
	case CS2_OPTION_FOCUSX:
	  *(SANE_Word *) v = s->focusx;
	  break;
	case CS2_OPTION_FOCUSY:
	  *(SANE_Word *) v = s->focusy;
	  break;
	case CS2_OPTION_SCAN_AE:
	  break;
	case CS2_OPTION_SCAN_AE_WB:
	  break;
	default:
	  DBG (4, "Error: sane_control_option(): Unknown option (bug?).\n");
	  return SANE_STATUS_INVAL;
	}
      break;

    case SANE_ACTION_SET_VALUE:
      if (s->scanning)
	return SANE_STATUS_INVAL;
/* XXXXXXXXXXXXXXXXX do this for all elements of arrays */
      switch (o.type)
	{
	case SANE_TYPE_BOOL:
	  if ((*(SANE_Word *) v != SANE_TRUE)
	      && (*(SANE_Word *) v != SANE_FALSE))
	    return SANE_STATUS_INVAL;
	  break;
	case SANE_TYPE_INT:
	case SANE_TYPE_FIXED:
	  switch (o.constraint_type)
	    {
	    case SANE_CONSTRAINT_RANGE:
	      if (*(SANE_Word *) v < o.constraint.range->min)
		{
		  *(SANE_Word *) v = o.constraint.range->min;
		  flags |= SANE_INFO_INEXACT;
		}
	      else if (*(SANE_Word *) v > o.constraint.range->max)
		{
		  *(SANE_Word *) v = o.constraint.range->max;
		  flags |= SANE_INFO_INEXACT;
		}
	      break;
	    case SANE_CONSTRAINT_WORD_LIST:
	      break;
	    default:
	      break;
	    }
	  break;
	case SANE_TYPE_STRING:
	  break;
	case SANE_TYPE_BUTTON:
	  break;
	case SANE_TYPE_GROUP:
	  break;
	}
      switch (n)
	{
	case CS2_OPTION_NUM:
	  return SANE_STATUS_INVAL;
	  break;
	case CS2_OPTION_NEGATIVE:
	  s->negative = *(SANE_Word *) v;
	  break;
	case CS2_OPTION_INFRARED:
	  s->infrared = *(SANE_Word *) v;
	  /*      flags |= SANE_INFO_RELOAD_PARAMS; XXXXXXXXXXXXXXXXX */
	  break;
	case CS2_OPTION_SAMPLES_PER_SCAN:
	  s->samples_per_scan = *(SANE_Word *) v;
	  break;
	case CS2_OPTION_DEPTH:
	  s->depth = *(SANE_Word *) v;
	  flags |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case CS2_OPTION_PREVIEW:
	  s->preview = *(SANE_Word *) v;
	  break;
	case CS2_OPTION_EXPOSURE:
	  s->exposure = SANE_UNFIX (*(SANE_Word *) v);
	  break;
	case CS2_OPTION_EXPOSURE_R:
	  s->exposure_r = SANE_UNFIX (*(SANE_Word *) v);
	  break;
	case CS2_OPTION_EXPOSURE_G:
	  s->exposure_g = SANE_UNFIX (*(SANE_Word *) v);
	  break;
	case CS2_OPTION_EXPOSURE_B:
	  s->exposure_b = SANE_UNFIX (*(SANE_Word *) v);
	  break;
	case CS2_OPTION_LUT_R:
	  if (!(s->lut_r))
	    return SANE_STATUS_INVAL;
	  for (pixel = 0; pixel < s->n_lut; pixel++)
	    s->lut_r[pixel] = ((SANE_Word *) v)[pixel];
	  break;
	case CS2_OPTION_LUT_G:
	  if (!(s->lut_g))
	    return SANE_STATUS_INVAL;
	  for (pixel = 0; pixel < s->n_lut; pixel++)
	    s->lut_g[pixel] = ((SANE_Word *) v)[pixel];
	  break;
	case CS2_OPTION_LUT_B:
	  if (!(s->lut_b))
	    return SANE_STATUS_INVAL;
	  for (pixel = 0; pixel < s->n_lut; pixel++)
	    s->lut_b[pixel] = ((SANE_Word *) v)[pixel];
	  break;
	case CS2_OPTION_LOAD:
	  cs2_load (s);
	  break;
	case CS2_OPTION_EJECT:
	  cs2_eject (s);
	  break;
	case CS2_OPTION_RESET:
	  cs2_reset (s);
	  break;
	case CS2_OPTION_FRAME:
	  s->i_frame = *(SANE_Word *) v;
	  break;
	case CS2_OPTION_SUBFRAME:
	  s->subframe = SANE_UNFIX (*(SANE_Word *) v);
	  break;
	case CS2_OPTION_RES:
	  s->res = *(SANE_Word *) v;
	  flags |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case CS2_OPTION_RESX:
	  s->resx = *(SANE_Word *) v;
	  flags |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case CS2_OPTION_RESY:
	  s->resy = *(SANE_Word *) v;
	  flags |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case CS2_OPTION_RES_INDEPENDENT:
	  s->res_independent = *(SANE_Word *) v;
	  flags |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case CS2_OPTION_PREVIEW_RESOLUTION:
	  s->res_preview = *(SANE_Word *) v;
	  flags |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case CS2_OPTION_XMIN:
	  s->xmin = *(SANE_Word *) v;
	  flags |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case CS2_OPTION_XMAX:
	  s->xmax = *(SANE_Word *) v;
	  flags |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case CS2_OPTION_YMIN:
	  s->ymin = *(SANE_Word *) v;
	  flags |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case CS2_OPTION_YMAX:
	  s->ymax = *(SANE_Word *) v;
	  flags |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case CS2_OPTION_FOCUS_ON_CENTRE:
	  s->focus_on_centre = *(SANE_Word *) v;
	  if (s->focus_on_centre)
	    {
	      s->option_list[CS2_OPTION_FOCUSX].cap |= SANE_CAP_INACTIVE;
	      s->option_list[CS2_OPTION_FOCUSY].cap |= SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->option_list[CS2_OPTION_FOCUSX].cap &= ~SANE_CAP_INACTIVE;
	      s->option_list[CS2_OPTION_FOCUSY].cap &= ~SANE_CAP_INACTIVE;
	    }
	  flags |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case CS2_OPTION_FOCUS:
	  s->focus = *(SANE_Word *) v;
	  break;
	case CS2_OPTION_AUTOFOCUS:
	  cs2_autofocus (s);
	  flags |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case CS2_OPTION_FOCUSX:
	  s->focusx = *(SANE_Word *) v;
	  break;
	case CS2_OPTION_FOCUSY:
	  s->focusy = *(SANE_Word *) v;
	  break;
	case CS2_OPTION_SCAN_AE:
	  cs2_scanner_ready (s, CS2_STATUS_NO_DOCS);
	  status = cs2_scan (s, CS2_SCAN_AE);
	  if (status)
	    return status;
	  status = cs2_get_exposure (s);
	  if (status)
	    return status;
	  s->exposure = 1.;
	  s->exposure_r = s->real_exposure[1] / 100.;
	  s->exposure_g = s->real_exposure[2] / 100.;
	  s->exposure_b = s->real_exposure[3] / 100.;
	  flags |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case CS2_OPTION_SCAN_AE_WB:
	  cs2_scanner_ready (s, CS2_STATUS_NO_DOCS);
	  status = cs2_scan (s, CS2_SCAN_AE_WB);
	  if (status)
	    return status;
	  status = cs2_get_exposure (s);
	  if (status)
	    return status;
	  s->exposure = 1.;
	  s->exposure_r = s->real_exposure[1] / 100.;
	  s->exposure_g = s->real_exposure[2] / 100.;
	  s->exposure_b = s->real_exposure[3] / 100.;
	  flags |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	default:
	  DBG (4,
	       "Error: sane_control_option(): Unknown option number (bug?).\n");
	  return SANE_STATUS_INVAL;
	  break;
	}
      break;

    default:
      DBG (1, "BUG: sane_control_option(): Unknown action number.\n");
      return SANE_STATUS_INVAL;
      break;
    }

  if (i)
    *i = flags;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters (SANE_Handle h, SANE_Parameters * p)
{
  cs2_t *s = (cs2_t *) h;
  SANE_Status status;

  DBG (10, "sane_get_parameters() called.\n");

  if (!s->scanning)		/* only recalculate when not scanning */
    {
      status = cs2_convert_options (s);
      if (status)
	return status;
    }

  if (s->infrared_stage == CS2_INFRARED_OUT)
    {
      p->format = SANE_FRAME_GRAY;
      p->bytes_per_line = s->logical_width * s->bytes_per_pixel;
    }
  else
    {
      p->format = SANE_FRAME_RGB;	/* XXXXXXXX CCCCCCCCCC */
      p->bytes_per_line =
	s->n_colour_out * s->logical_width * s->bytes_per_pixel;
    }
  p->last_frame = SANE_TRUE;
  p->lines = s->logical_height;
  p->depth = 8 * s->bytes_per_pixel;
  p->pixels_per_line = s->logical_width;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle h)
{
  cs2_t *s = (cs2_t *) h;
  SANE_Status status;

  DBG (10, "sane_start() called.\n");

  if (s->scanning)
    return SANE_STATUS_INVAL;

  status = cs2_convert_options (s);
  if (status)
    return status;

  s->infrared_index = 0;
  s->i_line_buf = 0;
  s->xfer_position = 0;

  s->scanning = SANE_TRUE;

  if (s->infrared_stage == CS2_INFRARED_OUT)
    return SANE_STATUS_GOOD;
  else
    return cs2_scan (s, CS2_SCAN_NORMAL);
}

SANE_Status
sane_read (SANE_Handle h, SANE_Byte * buf, SANE_Int maxlen, SANE_Int * len)
{
  cs2_t *s = (cs2_t *) h;
  SANE_Status status;
  ssize_t xfer_len_in, xfer_len_line, xfer_len_out;
  unsigned long index;
  int colour, n_colours, sample_pass;
  uint8_t *s8 = NULL;
  uint16_t *s16 = NULL;
  double m_avg_sum;
  SANE_Byte *line_buf_new;

  DBG (10, "sane_read() called, maxlen = %i.\n", maxlen);

  if (!s->scanning) {
    *len = 0;
    return SANE_STATUS_CANCELLED;
  }

  if (s->infrared_stage == CS2_INFRARED_OUT)
    {
      xfer_len_out = maxlen;

      if (s->xfer_position + xfer_len_out > s->n_infrared_buf)
	xfer_len_out = s->n_infrared_buf - s->xfer_position;

      if (xfer_len_out == 0)	/* no more data */
	{
	  *len = 0;
	s->scanning = SANE_FALSE;
	  return SANE_STATUS_EOF;
	}

      memcpy (buf, &(s->infrared_buf[s->xfer_position]), xfer_len_out);

      s->xfer_position += xfer_len_out;

      if (s->xfer_position >= s->n_infrared_buf)
	s->infrared_next = CS2_INFRARED_OFF;

      *len = xfer_len_out;
      return SANE_STATUS_GOOD;
    }

  if (s->i_line_buf > 0)
    {
      xfer_len_out = s->n_line_buf - s->i_line_buf;
      if (xfer_len_out > maxlen)
	xfer_len_out = maxlen;

      memcpy (buf, &(s->line_buf[s->i_line_buf]), xfer_len_out);

      s->i_line_buf += xfer_len_out;
      if (s->i_line_buf >= s->n_line_buf)
	s->i_line_buf = 0;

      *len = xfer_len_out;
      return SANE_STATUS_GOOD;
    }

  xfer_len_line = s->n_colour_out * s->logical_width * s->bytes_per_pixel;
  xfer_len_in =
    s->n_colour_in * s->logical_width * s->bytes_per_pixel +
    s->n_colour_in * s->odd_padding;
  /* Do not change the behaviour of older models */
  if ((s->type == CS2_TYPE_LS50) || (s->type == CS2_TYPE_LS5000))
    {
      /* Ariel - Check, win driver uses multiple of 64, docu seems to say 512? */
      ssize_t i;
      xfer_len_in += s->block_padding;
      i = (xfer_len_in & 0x3f);
      if (i != 0)
        DBG (1, "BUG: sane_read(): Read size is not a multiple of 64. (0x%06lx)\n", (long) i);
    }

  if (s->xfer_position + xfer_len_line > s->xfer_bytes_total)
    xfer_len_line = s->xfer_bytes_total - s->xfer_position; /* just in case */

  if (xfer_len_line == 0)	/* no more data */
    {
      *len = 0;
	s->scanning = SANE_FALSE;
      return SANE_STATUS_EOF;
    }

  if (xfer_len_line != s->n_line_buf)
    {
      line_buf_new =
	(SANE_Byte *) cs2_xrealloc (s->line_buf,
				    xfer_len_line * sizeof (SANE_Byte));
      if (!line_buf_new)
	{
	  *len = 0;
	  return SANE_STATUS_NO_MEM;
	}
      s->line_buf = line_buf_new;
      s->n_line_buf = xfer_len_line;
    }

  /* adapt for multi-sampling */
  xfer_len_in *= s->samples_per_scan;

  cs2_scanner_ready (s, CS2_STATUS_READY);
  cs2_init_buffer (s);
  cs2_parse_cmd (s, "28 00 00 00 00 00");
  cs2_pack_byte (s, (xfer_len_in >> 16) & 0xff);
  cs2_pack_byte (s, (xfer_len_in >> 8) & 0xff);
  cs2_pack_byte (s, xfer_len_in & 0xff);
  cs2_parse_cmd (s, "00");
  s->n_recv = xfer_len_in;
  status = cs2_issue_cmd (s);

  if (status)
    {
      *len = 0;
      return status;
    }

  n_colours = s->n_colour_out +
	  (s->infrared_stage == CS2_INFRARED_IN ? 1 : 0);

  for (index = 0; index < s->logical_width; index++)
    for (colour = 0; colour < n_colours; colour++) {
	  m_avg_sum = 0.0;
      switch (s->bytes_per_pixel)
	{
	case 1:
	  /* calculate target address */
	  if ((s->infrared_stage == CS2_INFRARED_IN)
	      && (colour == s->n_colour_out))
	    s8 = (uint8_t *) & (s->infrared_buf[s->infrared_index++]);
	  else
	    s8 =
	      (uint8_t *) & (s->line_buf[s->n_colour_out * index + colour]);

	  if (s->samples_per_scan > 1)
		{
		  /* calculate average of multi samples */
		  for (sample_pass = 0;
			   sample_pass < s->samples_per_scan;
			   sample_pass++)
			m_avg_sum += (double)
			  s->recv_buf[s->logical_width *
			  (sample_pass * n_colours + colour) +
			  (colour + 1) * s->odd_padding + index];

		  *s8 = (uint8_t) (m_avg_sum / s->samples_per_scan + 0.5);
		}
	  else
		/* shortcut for single sample */
		*s8 =
		  s->recv_buf[colour * s->logical_width +
					  (colour + 1) * s->odd_padding + index];
	  break;
	case 2:
	  /* calculate target address */
	  if ((s->infrared_stage == CS2_INFRARED_IN)
	      && (colour == s->n_colour_out))
	    s16 =
	      (uint16_t *) & (s->infrared_buf[2 * (s->infrared_index++)]);
	  else
	    s16 =
	      (uint16_t *) & (s->
			       line_buf[2 *
					(s->n_colour_out * index + colour)]);

	  if (s->samples_per_scan > 1)
		{
		  /* calculate average of multi samples */
		  for (sample_pass = 0;
			   s->samples_per_scan > 1 && sample_pass < s->samples_per_scan;
			   sample_pass++)
			m_avg_sum += (double)
			  (s->recv_buf[2 * (s->logical_width * (sample_pass * n_colours + colour) + index)] * 256 +
			   s->recv_buf[2 * (s->logical_width * (sample_pass * n_colours + colour) + index) + 1]);

		  *s16 = (uint16_t) (m_avg_sum / s->samples_per_scan + 0.5);
		}
	  else
		/* shortcut for single sample */
		*s16 =
		  s->recv_buf[2 * (colour * s->logical_width + index)] * 256 +
		  s->recv_buf[2 * (colour * s->logical_width + index) + 1];
	  *s16 <<= s->shift_bits;
	  break;
	default:
	  DBG (1, "BUG: sane_read(): Unknown number of bytes per pixel.\n");
	  *len = 0;
	  return SANE_STATUS_INVAL;
	  break;
	}
	}
  s->xfer_position += xfer_len_line;

  xfer_len_out = xfer_len_line;
  if (xfer_len_out > maxlen)
    xfer_len_out = maxlen;

  memcpy (buf, s->line_buf, xfer_len_out);
  if (xfer_len_out < xfer_len_line)
    s->i_line_buf = xfer_len_out; /* data left in the line buffer, read out next time */

  if ((s->infrared_stage == CS2_INFRARED_IN)
      && (s->xfer_position >= s->n_infrared_buf))
    s->infrared_next = CS2_INFRARED_OUT;

  *len = xfer_len_out;
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle h)
{
  cs2_t *s = (cs2_t *) h;

  if (s->scanning)
    DBG (10, "sane_cancel() called while scanning.\n");
  else
    DBG (10, "sane_cancel() called while not scanning.\n");

  if (s->scanning && (s->infrared_stage != CS2_INFRARED_OUT))
    {
      cs2_init_buffer (s);
      cs2_parse_cmd (s, "c0 00 00 00 00 00");
      cs2_issue_cmd (s);
    }

  s->scanning = SANE_FALSE;
}

SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool m)
{
  cs2_t *s = (cs2_t *) h;

  DBG (10, "sane_set_io_mode() called.\n");

  if (!s->scanning)
    return SANE_STATUS_INVAL;
  if (m == SANE_FALSE)
    return SANE_STATUS_GOOD;
  else
    return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int * fd)
{
  cs2_t *s = (cs2_t *) h;

  DBG (10, "sane_get_select_fd() called.\n");

  fd = fd;			/* to shut up compiler */
  s = s;			/* to shut up compiler */

  return SANE_STATUS_UNSUPPORTED;
}


/* ========================================================================= */
/* private functions */

static SANE_Status
cs2_open (const char *device, cs2_interface_t interface, cs2_t ** sp)
{
  SANE_Status status;
  cs2_t *s;
  char *prefix = NULL, *line, *device2;
  int i;
  int alloc_failed = 0;
  SANE_Device **device_list_new;

  DBG (6, "cs2_open() called, with device = %s and interface = %i\n", device,
       interface);

  if (!strncmp (device, "auto", 5))
    {
      try_interface = CS2_INTERFACE_SCSI;
      sanei_config_attach_matching_devices ("scsi Nikon *", cs2_attach);
      try_interface = CS2_INTERFACE_USB;
      sanei_usb_attach_matching_devices ("usb 0x04b0 0x4000", cs2_attach);
      sanei_usb_attach_matching_devices ("usb 0x04b0 0x4001", cs2_attach);
      sanei_usb_attach_matching_devices ("usb 0x04b0 0x4002", cs2_attach);
      return SANE_STATUS_GOOD;
    }

  if ((s = (cs2_t *) cs2_xmalloc (sizeof (cs2_t))) == NULL)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (cs2_t));
  s->send_buf = s->recv_buf = NULL;
  s->send_buf_size = s->recv_buf_size = 0;

  switch (interface)
    {
    case CS2_INTERFACE_UNKNOWN:
      for (i = 0; i < 2; i++)
	{
	  switch (i)
	    {
	    case 1:
	      prefix = "usb:";
	      try_interface = CS2_INTERFACE_USB;
	      break;
	    default:
	      prefix = "scsi:";
	      try_interface = CS2_INTERFACE_SCSI;
	      break;
	    }
	  if (!strncmp (device, prefix, strlen (prefix)))
	    {
	      device2 = device + strlen (prefix);
	      cs2_xfree (s);
	      return cs2_open (device2, try_interface, sp);
	    }
	}
      cs2_xfree (s);
      return SANE_STATUS_INVAL;
      break;
    case CS2_INTERFACE_SCSI:
      s->interface = CS2_INTERFACE_SCSI;
      DBG (6,
	   "cs2_open(): Trying to open %s, assuming SCSI or SBP2 interface ...\n",
	   device);
      status = sanei_scsi_open (device, &s->fd, cs2_scsi_sense_handler, s);
      if (status)
	{
	  DBG (6, "cs2_open(): ... failed: %s.\n", sane_strstatus (status));
	  cs2_xfree (s);
	  return status;
	}
      break;
    case CS2_INTERFACE_USB:
      s->interface = CS2_INTERFACE_USB;
      DBG (6, "cs2_open(): Trying to open %s, assuming USB interface ...\n",
	   device);
      status = sanei_usb_open (device, &s->fd);
      if (status)
	{
	  DBG (6, "cs2_open(): ... failed: %s.\n", sane_strstatus (status));
	  cs2_xfree (s);
	  return status;
	}
      break;
    }

  open_devices++;
  DBG (6, "cs2_open(): ... looks OK, trying to identify device.\n");

  /* identify scanner */
  status = cs2_page_inquiry (s, -1);
  if (status)
    {
      DBG (4, "Error: cs2_open(): failed to get page: %s.\n",
	   sane_strstatus (status));
      cs2_close (s);
      return status;
    }

  strncpy (s->vendor_string, (char *)s->recv_buf + 8, 8);
  s->vendor_string[8] = '\0';
  strncpy (s->product_string, (char *)s->recv_buf + 16, 16);
  s->product_string[16] = '\0';
  strncpy (s->revision_string, (char *)s->recv_buf + 32, 4);
  s->revision_string[4] = '\0';

  DBG (10,
       "cs2_open(): Inquiry reveals: vendor = '%s', product = '%s', revision = '%s'.\n",
       s->vendor_string, s->product_string, s->revision_string);

  if (!strncmp (s->product_string, "COOLSCANIII     ", 16))
    s->type = CS2_TYPE_LS30;
  else if (!strncmp (s->product_string, "LS-40 ED        ", 16))
    s->type = CS2_TYPE_LS40;
  else if (!strncmp (s->product_string, "LS-50 ED        ", 16))
    s->type = CS2_TYPE_LS50;
  else if (!strncmp (s->product_string, "LS-2000         ", 16))
    s->type = CS2_TYPE_LS2000;
  else if (!strncmp (s->product_string, "LS-4000 ED      ", 16))
    s->type = CS2_TYPE_LS4000;
  else if (!strncmp (s->product_string, "LS-5000 ED      ", 16))
    s->type = CS2_TYPE_LS5000;
  else if (!strncmp (s->product_string, "LS-8000 ED      ", 16))
    s->type = CS2_TYPE_LS8000;

  if (s->type != CS2_TYPE_UNKOWN)
    DBG (10, "cs2_open(): Device identified as coolscan2 type #%i.\n",
	 s->type);
  else
    {
      DBG (10, "cs2_open(): Device not identified.\n");
      cs2_close (s);
      return SANE_STATUS_UNSUPPORTED;
    }

  if (sp)
    *sp = s;
  else
    {
      device_list_new =
	(SANE_Device **) cs2_xrealloc (device_list,
				       (n_device_list +
					2) * sizeof (SANE_Device *));
      if (!device_list_new)
	return SANE_STATUS_NO_MEM;
      device_list = device_list_new;
      device_list[n_device_list] =
	(SANE_Device *) cs2_xmalloc (sizeof (SANE_Device));
      if (!device_list[n_device_list])
	return SANE_STATUS_NO_MEM;
      switch (interface)
	{
	case CS2_INTERFACE_UNKNOWN:
	  DBG (1, "BUG: cs2_open(): unknown interface.\n");
	  cs2_close (s);
	  return SANE_STATUS_UNSUPPORTED;
	  break;
	case CS2_INTERFACE_SCSI:
	  prefix = "scsi:";
	  break;
	case CS2_INTERFACE_USB:
	  prefix = "usb:";
	  break;
	}

      line = (char *) cs2_xmalloc (strlen (device) + strlen (prefix) + 1);
      if (!line)
	alloc_failed = 1;
      else
	{
	  strcpy (line, prefix);
	  strcat (line, device);
	  device_list[n_device_list]->name = line;
	}

      line = (char *) cs2_xmalloc (strlen (s->vendor_string) + 1);
      if (!line)
	alloc_failed = 1;
      else
	{
	  strcpy (line, s->vendor_string);
	  device_list[n_device_list]->vendor = line;
	}

      line = (char *) cs2_xmalloc (strlen (s->product_string) + 1);
      if (!line)
	alloc_failed = 1;
      else
	{
	  strcpy (line, s->product_string);
	  device_list[n_device_list]->model = line;
	}

      device_list[n_device_list]->type = "film scanner";

      if (alloc_failed)
	{
	  cs2_xfree (device_list[n_device_list]->name);
	  cs2_xfree (device_list[n_device_list]->vendor);
	  cs2_xfree (device_list[n_device_list]->model);
	  cs2_xfree (device_list[n_device_list]);
	}
      else
	n_device_list++;
      device_list[n_device_list] = NULL;

      cs2_close (s);
    }

  return SANE_STATUS_GOOD;
}

void
cs2_close (cs2_t * s)
{
  cs2_xfree (s->lut_r);
  cs2_xfree (s->lut_g);
  cs2_xfree (s->lut_b);
  cs2_xfree (s->lut_neutral);
  cs2_xfree (s->infrared_buf);
  cs2_xfree (s->line_buf);

  switch (s->interface)
    {
    case CS2_INTERFACE_UNKNOWN:
      DBG (1, "BUG: cs2_close(): Unknown interface number.\n");
      break;
    case CS2_INTERFACE_SCSI:
      sanei_scsi_close (s->fd);
      open_devices--;
      break;
    case CS2_INTERFACE_USB:
      sanei_usb_close (s->fd);
      open_devices--;
      break;
    }

  cs2_xfree (s);
}

static SANE_Status
cs2_attach (const char *dev)
{
  SANE_Status status;

  if (try_interface == CS2_INTERFACE_UNKNOWN)
    return SANE_STATUS_UNSUPPORTED;

  status = cs2_open (dev, try_interface, NULL);
  return status;
}

static SANE_Status
cs2_scsi_sense_handler (int fd, u_char * sense_buffer, void *arg)
{
  cs2_t *s = (cs2_t *) arg;

  fd = fd;			/* to shut up compiler */

  /* sort this out ! XXXXXXXXX */

  s->sense_key = sense_buffer[2] & 0x0f;
  s->sense_asc = sense_buffer[12];
  s->sense_ascq = sense_buffer[13];
  s->sense_info = sense_buffer[3];

  return cs2_parse_sense_data (s);
}

static SANE_Status
cs2_parse_sense_data (cs2_t * s)
{
  SANE_Status status = SANE_STATUS_GOOD;

  s->sense_code =
    (s->sense_key << 24) + (s->sense_asc << 16) + (s->sense_ascq << 8) +
    s->sense_info;

  if (s->sense_key)
    DBG (10, "Sense code: %02lx-%02lx-%02lx-%02lx\n", s->sense_key,
	 s->sense_asc, s->sense_ascq, s->sense_info);

  switch (s->sense_key)
    {
    case 0x00:
      s->status = CS2_STATUS_READY;
      break;
    case 0x02:
      switch (s->sense_asc)
	{
	case 0x04:
	  s->status = CS2_STATUS_PROCESSING;
	  break;
	case 0x3a:
	  s->status = CS2_STATUS_NO_DOCS;
	  break;
	default:
	  s->status = CS2_STATUS_ERROR;
	  status = SANE_STATUS_IO_ERROR;
	  break;
	}
      break;
    default:
      s->status = CS2_STATUS_ERROR;
      status = SANE_STATUS_IO_ERROR;
      break;
    }

  if ((s->sense_code == 0x09800600) || (s->sense_code == 0x09800601))
    s->status = CS2_STATUS_REISSUE;

  return status;
}

static void
cs2_init_buffer (cs2_t * s)
{
  s->n_cmd = 0;
  s->n_send = 0;
  s->n_recv = 0;
}

static SANE_Status
cs2_pack_byte (cs2_t * s, SANE_Byte byte)
{
  while (s->send_buf_size <= s->n_send)
    {
      s->send_buf_size += 16;
      s->send_buf =
	(SANE_Byte *) cs2_xrealloc (s->send_buf, s->send_buf_size);
      if (!s->send_buf)
	return SANE_STATUS_NO_MEM;
    }

  s->send_buf[s->n_send++] = byte;

  return SANE_STATUS_GOOD;
}

static SANE_Status
cs2_parse_cmd (cs2_t * s, char *text)
{
  size_t i, j;
  char c, h;
  SANE_Status status;

  for (i = 0; i < strlen (text); i += 2)
    if (text[i] == ' ')
      i--;			/* a bit dirty... advance by -1+2=1 */
    else
      {
	if ((!isxdigit (text[i])) || (!isxdigit (text[i + 1])))
	  DBG (1, "BUG: cs2_parse_cmd(): Parser got invalid character.\n");
	c = 0;
	for (j = 0; j < 2; j++)
	  {
	    h = tolower (text[i + j]);
	    if ((h >= 'a') && (h <= 'f'))
	      c += 10 + h - 'a';
	    else
	      c += h - '0';
	    if (j == 0)
	      c <<= 4;
	  }
	status = cs2_pack_byte (s, c);
	if (status)
	  return status;
      }

  return SANE_STATUS_GOOD;
}

static SANE_Status
cs2_grow_send_buffer (cs2_t * s)
{
  if (s->n_send > s->send_buf_size)
    {
      s->send_buf_size = s->n_send;
      s->send_buf =
	(SANE_Byte *) cs2_xrealloc (s->send_buf, s->send_buf_size);
      if (!s->send_buf)
	return SANE_STATUS_NO_MEM;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
cs2_issue_cmd (cs2_t * s)
{
  SANE_Status status = SANE_STATUS_INVAL;
  size_t n_data, n_status;
  static SANE_Byte status_buf[8];
  int status_only = 0;

  DBG (20, "cs2_issue_cmd(): opcode = 0x%02x, n_send = %lu, n_recv = %lu.\n",
       s->send_buf[0], (unsigned long) s->n_send, (unsigned long) s->n_recv);

  s->status = CS2_STATUS_READY;

  if (!s->n_cmd)
    switch (s->send_buf[0])
      {
      case 0x00:
      case 0x12:
      case 0x15:
      case 0x16:
      case 0x17:
      case 0x1a:
      case 0x1b:
      case 0x1c:
      case 0x1d:
      case 0xc0:
      case 0xc1:
	s->n_cmd = 6;
	break;
      case 0x24:
      case 0x25:
      case 0x28:
      case 0x2a:
      case 0xe0:
      case 0xe1:
	s->n_cmd = 10;
	break;
      default:
	DBG (1, "BUG: cs2_issue_cmd(): Unknown command opcode 0x%02x.\n",
	     s->send_buf[0]);
	break;
      }

  if (s->n_send < s->n_cmd)
    {
      DBG (1,
	   "BUG: cs2_issue_cmd(): Negative number of data out bytes requested.\n");
      return SANE_STATUS_INVAL;
    }

  n_data = s->n_send - s->n_cmd;
  if (s->n_recv > 0)
    {
      if (n_data > 0)
	{
	  DBG (1,
	       "BUG: cs2_issue_cmd(): Both data in and data out requested.\n");
	  return SANE_STATUS_INVAL;
	}
      else
	{
	  n_data = s->n_recv;
	}
    }

  s->recv_buf = (SANE_Byte *) cs2_xrealloc (s->recv_buf, s->n_recv);
  if (!s->recv_buf)
    return SANE_STATUS_NO_MEM;

  switch (s->interface)
    {
    case CS2_INTERFACE_UNKNOWN:
      DBG (1,
	   "BUG: cs2_issue_cmd(): Unknown or uninitialized interface number.\n");
      break;
    case CS2_INTERFACE_SCSI:
      sanei_scsi_cmd2 (s->fd, s->send_buf, s->n_cmd, s->send_buf + s->n_cmd,
		       s->n_send - s->n_cmd, s->recv_buf, &s->n_recv);
      status = SANE_STATUS_GOOD;
      break;
    case CS2_INTERFACE_USB:
      status = sanei_usb_write_bulk (s->fd, s->send_buf, &s->n_cmd);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (1, "Error: cs2_issue_cmd(): Could not write command.\n");
          return SANE_STATUS_IO_ERROR;
        }
      switch (cs2_phase_check (s))
	{
	case CS2_PHASE_OUT:
	  if (s->n_send - s->n_cmd < n_data || !n_data)
	    {
	      DBG (4, "Error: cs2_issue_cmd(): Unexpected data out phase.\n");
	      return SANE_STATUS_IO_ERROR;
	    }
	  status =
	    sanei_usb_write_bulk (s->fd, s->send_buf + s->n_cmd, &n_data);
	  break;
	case CS2_PHASE_IN:
	  if (s->n_recv < n_data || !n_data)
	    {
	      DBG (4, "Error: cs2_issue_cmd(): Unexpected data in phase.\n");
	      return SANE_STATUS_IO_ERROR;
	    }
	  status = sanei_usb_read_bulk (s->fd, s->recv_buf, &n_data);
	  s->n_recv = n_data;
	  break;
	case CS2_PHASE_NONE:
	  DBG (4, "Error: cs2_issue_cmd(): No command received!\n");
	  return SANE_STATUS_IO_ERROR;
	default:
	  if (n_data)
	    {
	      DBG (4,
		   "Error: cs2_issue_cmd(): Unexpected non-data phase, but n_data != 0.\n");
	      status_only = 1;
	    }
	  break;
	}
      n_status = 8;
      status = sanei_usb_read_bulk (s->fd, status_buf, &n_status);
      if (n_status != 8)
	{
	  DBG (4,
	       "Error: cs2_issue_cmd(): Failed to read 8 status bytes from USB.\n");
	  return SANE_STATUS_IO_ERROR;
	}
      s->sense_key = status_buf[1] & 0x0f;
      s->sense_asc = status_buf[2] & 0xff;
      s->sense_ascq = status_buf[3] & 0xff;
      s->sense_info = status_buf[4] & 0xff;
      cs2_parse_sense_data (s);
      break;
    }

  if (status_only)
    return SANE_STATUS_IO_ERROR;
  else
    return status;
}

static cs2_phase_t
cs2_phase_check (cs2_t * s)
{
  static SANE_Byte phase_send_buf[1] = { 0xd0 }, phase_recv_buf[1];
  SANE_Status status = 0;
  size_t n = 1;

  status = sanei_usb_write_bulk (s->fd, phase_send_buf, &n);
  status |= sanei_usb_read_bulk (s->fd, phase_recv_buf, &n);

  DBG (6, "cs2_phase_check(): Phase check returned phase = 0x%02x.\n",
       phase_recv_buf[0]);

  if (status)
    return -1;
  else
    return phase_recv_buf[0];
}

static SANE_Status
cs2_scanner_ready (cs2_t * s, int flags)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int i = -1;
  unsigned long count = 0;
  int retry = 3;

  do
    {
      if (i >= 0)		/* dirty !!! */
	usleep (500000);
      cs2_init_buffer (s);
      for (i = 0; i < 6; i++)
	cs2_pack_byte (s, 0x00);
      status = cs2_issue_cmd (s);
      if (status)
	if (--retry < 0)
	  return status;
      if (++count > 240)
	{			/* 120s timeout */
	  DBG (4, "Error: cs2_scanner_ready(): Timeout expired.\n");
	  status = SANE_STATUS_IO_ERROR;
	  break;
	}
    }
  while (s->status & ~flags);	/* until all relevant bits are 0 */

  return status;
}

static SANE_Status
cs2_page_inquiry (cs2_t * s, int page)
{
  SANE_Status status;

  size_t n;

  if (page >= 0)
    {

      cs2_scanner_ready (s, CS2_STATUS_NO_DOCS);
      cs2_init_buffer (s);
      cs2_parse_cmd (s, "12 01");
      cs2_pack_byte (s, page);
      cs2_parse_cmd (s, "00 04 00");
      s->n_recv = 4;
      status = cs2_issue_cmd (s);
      if (status)
	{
	  DBG (4,
	       "Error: cs2_page_inquiry(): Inquiry of page size failed: %s.\n",
	       sane_strstatus (status));
	  return status;
	}

      n = s->recv_buf[3] + 4;

    }
  else
    n = 36;

  cs2_scanner_ready (s, CS2_STATUS_NO_DOCS);
  cs2_init_buffer (s);
  if (page >= 0)
    {
      cs2_parse_cmd (s, "12 01");
      cs2_pack_byte (s, page);
      cs2_parse_cmd (s, "00");
    }
  else
    cs2_parse_cmd (s, "12 00 00 00");
  cs2_pack_byte (s, n);
  cs2_parse_cmd (s, "00");
  s->n_recv = n;
  status = cs2_issue_cmd (s);
  if (status)
    {
      DBG (4, "Error: cs2_page_inquiry(): Inquiry of page failed: %s.\n",
	   sane_strstatus (status));
      return status;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
cs2_full_inquiry (cs2_t * s)
{
  SANE_Status status;
  int pitch, pitch_max;
  cs2_pixel_t pixel;

  status = cs2_page_inquiry (s, 0xc1);
  if (status)
    {
      DBG (4, "Error: cs2_full_inquiry(): Failed to get page: %s\n",
	   sane_strstatus (status));
      return status;
    }

  s->maxbits = s->recv_buf[82];
  if (s->type == CS2_TYPE_LS30)	/* must be overridden, LS-30 claims to have 12 bits */
    s->maxbits = 10;

  s->n_lut = 1;
  s->n_lut <<= s->maxbits;
  s->lut_r =
    (cs2_pixel_t *) cs2_xrealloc (s->lut_r, s->n_lut * sizeof (cs2_pixel_t));
  s->lut_g =
    (cs2_pixel_t *) cs2_xrealloc (s->lut_g, s->n_lut * sizeof (cs2_pixel_t));
  s->lut_b =
    (cs2_pixel_t *) cs2_xrealloc (s->lut_b, s->n_lut * sizeof (cs2_pixel_t));
  s->lut_neutral =
    (cs2_pixel_t *) cs2_xrealloc (s->lut_neutral,
				  s->n_lut * sizeof (cs2_pixel_t));

  if (!s->lut_r || !s->lut_g || !s->lut_b || !s->lut_neutral)
    {
      cs2_xfree (s->lut_r);
      cs2_xfree (s->lut_g);
      cs2_xfree (s->lut_b);
      cs2_xfree (s->lut_neutral);
      return SANE_STATUS_NO_MEM;
    }

  for (pixel = 0; pixel < s->n_lut; pixel++)
    s->lut_r[pixel] = s->lut_g[pixel] = s->lut_b[pixel] =
      s->lut_neutral[pixel] = pixel;

  s->resx_optical = 256 * s->recv_buf[18] + s->recv_buf[19];
  s->resx_max = 256 * s->recv_buf[20] + s->recv_buf[21];
  s->resx_min = 256 * s->recv_buf[22] + s->recv_buf[23];
  s->boundaryx =
    65536 * (256 * s->recv_buf[36] + s->recv_buf[37]) +
    256 * s->recv_buf[38] + s->recv_buf[39];

  s->resy_optical = 256 * s->recv_buf[40] + s->recv_buf[41];
  s->resy_max = 256 * s->recv_buf[42] + s->recv_buf[43];
  s->resy_min = 256 * s->recv_buf[44] + s->recv_buf[45];
  s->boundaryy =
    65536 * (256 * s->recv_buf[58] + s->recv_buf[59]) +
    256 * s->recv_buf[60] + s->recv_buf[61];

  s->focus_min = 256 * s->recv_buf[76] + s->recv_buf[77];
  s->focus_max = 256 * s->recv_buf[78] + s->recv_buf[79];

  s->n_frames = s->recv_buf[75];

  s->frame_offset = s->resy_max * 1.5 + 1;	/* works for LS-30, maybe not for others */

  /* generate resolution list for x */
  s->resx_n_list = pitch_max = floor (s->resx_max / (double) s->resx_min);
  s->resx_list =
    (unsigned int *) cs2_xrealloc (s->resx_list,
				   pitch_max * sizeof (unsigned int));
  for (pitch = 1; pitch <= pitch_max; pitch++)
    s->resx_list[pitch - 1] = s->resx_max / pitch;

  /* generate resolution list for y */
  s->resy_n_list = pitch_max = floor (s->resy_max / (double) s->resy_min);
  s->resy_list =
    (unsigned int *) cs2_xrealloc (s->resy_list,
				   pitch_max * sizeof (unsigned int));
  for (pitch = 1; pitch <= pitch_max; pitch++)
    s->resy_list[pitch - 1] = s->resy_max / pitch;

  s->unit_dpi = s->resx_max;
  s->unit_mm = 25.4 / s->unit_dpi;

  return SANE_STATUS_GOOD;
}

static SANE_Status
cs2_execute (cs2_t * s)
{
  cs2_scanner_ready (s, CS2_STATUS_NO_DOCS);
  cs2_init_buffer (s);
  cs2_parse_cmd (s, "c1 00 00 00 00 00");
  return cs2_issue_cmd (s);
}

static SANE_Status
cs2_load (cs2_t * s)
{
  SANE_Status status;

  cs2_scanner_ready (s, CS2_STATUS_NO_DOCS);
  cs2_init_buffer (s);
  cs2_parse_cmd (s, "e0 00 d1 00 00 00 00 00 0d 00");
  s->n_send += 13;
  status = cs2_grow_send_buffer (s);
  if (status)
    return status;
  status = cs2_issue_cmd (s);
  if (status)
    return status;

  return cs2_execute (s);
}

static SANE_Status
cs2_eject (cs2_t * s)
{
  SANE_Status status;

  cs2_scanner_ready (s, CS2_STATUS_NO_DOCS);
  cs2_init_buffer (s);
  cs2_parse_cmd (s, "e0 00 d0 00 00 00 00 00 0d 00");
  s->n_send += 13;
  status = cs2_grow_send_buffer (s);
  if (status)
    return status;
  status = cs2_issue_cmd (s);
  if (status)
    return status;

  return cs2_execute (s);
}

static SANE_Status
cs2_reset (cs2_t * s)
{
  SANE_Status status;

  cs2_scanner_ready (s, CS2_STATUS_NO_DOCS);
  cs2_init_buffer (s);
  cs2_parse_cmd (s, "e0 00 80 00 00 00 00 00 0d 00");
  s->n_send += 13;
  status = cs2_grow_send_buffer (s);
  if (status)
    return status;
  status = cs2_issue_cmd (s);
  if (status)
    return status;

  return cs2_execute (s);
}

static SANE_Status
cs2_focus (cs2_t * s)
{
  SANE_Status status;

  cs2_scanner_ready (s, CS2_STATUS_READY);
  cs2_init_buffer (s);
  cs2_parse_cmd (s, "e0 00 c1 00 00 00 00 00 0d 00 00");
  cs2_pack_byte (s, (s->focus >> 24) & 0xff);
  cs2_pack_byte (s, (s->focus >> 16) & 0xff);
  cs2_pack_byte (s, (s->focus >> 8) & 0xff);
  cs2_pack_byte (s, s->focus & 0xff);
  cs2_parse_cmd (s, "00 00 00 00 00 00 00 00");
  status = cs2_issue_cmd (s);
  if (status)
    return status;

  return cs2_execute (s);
}

static SANE_Status
cs2_autofocus (cs2_t * s)
{
  SANE_Status status;

  cs2_convert_options (s);

  cs2_scanner_ready (s, CS2_STATUS_READY);
  cs2_init_buffer (s);
  cs2_parse_cmd (s, "e0 00 a0 00 00 00 00 00 0d 00 00");
  cs2_pack_byte (s, (s->real_focusx >> 24) & 0xff);
  cs2_pack_byte (s, (s->real_focusx >> 16) & 0xff);
  cs2_pack_byte (s, (s->real_focusx >> 8) & 0xff);
  cs2_pack_byte (s, s->real_focusx & 0xff);
  cs2_pack_byte (s, (s->real_focusy >> 24) & 0xff);
  cs2_pack_byte (s, (s->real_focusy >> 16) & 0xff);
  cs2_pack_byte (s, (s->real_focusy >> 8) & 0xff);
  cs2_pack_byte (s, s->real_focusy & 0xff);
  cs2_parse_cmd (s, "00 00 00 00");
  status = cs2_issue_cmd (s);
  if (status)
    return status;

  status = cs2_execute (s);
  if (status)
    return status;

  cs2_scanner_ready (s, CS2_STATUS_READY);
  cs2_init_buffer (s);
  cs2_parse_cmd (s, "e1 00 c1 00 00 00 00 00 0d 00");
  s->n_recv = 13;
  status = cs2_issue_cmd (s);
  if (status)
    return status;

  s->focus =
    65536 * (256 * s->recv_buf[1] + s->recv_buf[2]) + 256 * s->recv_buf[3] +
    s->recv_buf[4];

  return status;
}

static SANE_Status
cs2_get_exposure (cs2_t * s)
{
  SANE_Status status;
  int i_colour;

  for (i_colour = 0; i_colour < 3; i_colour++)
    {				/* XXXXXXXXXXXXX CCCCCCCCCCCCC */
      cs2_scanner_ready (s, CS2_STATUS_NO_DOCS);

      cs2_init_buffer (s);
      cs2_parse_cmd (s, "25 01 00 00 00");
      cs2_pack_byte (s, cs2_colour_list[i_colour]);
      cs2_parse_cmd (s, "00 00 3a 00");
      s->n_recv = 58;
      status = cs2_issue_cmd (s);
      if (status)
	return status;

      s->real_exposure[cs2_colour_list[i_colour]] =
	65536 * (256 * s->recv_buf[54] + s->recv_buf[55]) +
	256 * s->recv_buf[56] + s->recv_buf[57];

      DBG (6, "cs2_get_exposure(): exposure for colour %i: %li * 10ns\n", cs2_colour_list[i_colour], s->real_exposure[cs2_colour_list[i_colour]]);
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
cs2_convert_options (cs2_t * s)
{
  int i_colour;
  unsigned long xmin, xmax, ymin, ymax;
  SANE_Byte *infrared_buf_new;

  s->real_depth = (s->preview ? 8 : s->depth);
  s->bytes_per_pixel = (s->real_depth > 8 ? 2 : 1);
  s->shift_bits = 8 * s->bytes_per_pixel - s->real_depth;

  if (s->preview)
    {
      s->real_resx = s->res_preview;
      s->real_resy = s->res_preview;
    }
  else if (s->res_independent)
    {
      s->real_resx = s->resx;
      s->real_resy = s->resy;
    }
  else
    {
      s->real_resx = s->res;
      s->real_resy = s->res;
    }
  s->real_pitchx = s->resx_max / s->real_resx;
  s->real_pitchy = s->resy_max / s->real_resy;

  s->real_resx = s->resx_max / s->real_pitchx;
  s->real_resy = s->resy_max / s->real_pitchy;

  /* The prefix "real_" refers to data in device units (1/maxdpi), "logical_" refers to resolution-dependent data. */

  if (s->xmin < s->xmax)
    {
      xmin = s->xmin;
      xmax = s->xmax;
    }
  else
    {
      xmin = s->xmax;
      xmax = s->xmin;
    }

  if (s->ymin < s->ymax)
    {
      ymin = s->ymin;
      ymax = s->ymax;
    }
  else
    {
      ymin = s->ymax;
      ymax = s->ymin;
    }

  s->real_xoffset = xmin;
  s->real_yoffset =
    ymin + (s->i_frame - 1) * s->frame_offset + s->subframe / s->unit_mm;
  s->logical_width = (xmax - xmin + 1) / s->real_pitchx;	/* XXXXXXXXX use mm units */
  s->logical_height = (ymax - ymin + 1) / s->real_pitchy;
  s->real_width = s->logical_width * s->real_pitchx;
  s->real_height = s->logical_height * s->real_pitchy;

  s->odd_padding = 0;
  if ((s->bytes_per_pixel == 1) && (s->logical_width & 0x01)
      && (s->type != CS2_TYPE_LS30) && (s->type != CS2_TYPE_LS2000))
    s->odd_padding = 1;

  if (s->focus_on_centre)
    {
      s->real_focusx = s->real_xoffset + s->real_width / 2;
      s->real_focusy = s->real_yoffset + s->real_height / 2;
    }
  else
    {
      s->real_focusx = s->focusx;
      s->real_focusy =
	s->focusy + (s->i_frame - 1) * s->frame_offset +
	s->subframe / s->unit_mm;
    }

  s->real_exposure[1] = s->exposure * s->exposure_r * 100.;
  s->real_exposure[2] = s->exposure * s->exposure_g * 100.;
  s->real_exposure[3] = s->exposure * s->exposure_b * 100.;

  for (i_colour = 0; i_colour < 3; i_colour++)
    if (s->real_exposure[cs2_colour_list[i_colour]] < 1)
      s->real_exposure[cs2_colour_list[i_colour]] = 1;

  s->n_colour_out = s->n_colour_in = 3;	/* XXXXXXXXXXXXXX CCCCCCCCCCCCCC */

  s->xfer_bytes_total =
    s->bytes_per_pixel * s->n_colour_out * s->logical_width *
    s->logical_height;

  if (s->preview)
    s->infrared_stage = s->infrared_next = CS2_INFRARED_OFF;
  else
    {
      if ((s->infrared) && (s->infrared_stage == CS2_INFRARED_OFF))
	s->infrared_next = CS2_INFRARED_IN;

      s->infrared_stage = s->infrared_next;

      if (s->infrared)
	{
	  s->n_colour_in ++;
	  s->n_infrared_buf =
	    s->bytes_per_pixel * s->logical_width * s->logical_height;
	  infrared_buf_new =
	    (SANE_Byte *) cs2_xrealloc (s->infrared_buf, s->n_infrared_buf);
	  if (infrared_buf_new)
	    s->infrared_buf = infrared_buf_new;
	  else
	    return SANE_STATUS_NO_MEM;
	}
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
cs2_set_boundary (cs2_t *s)
{
  SANE_Status status;
  int i_boundary;
  unsigned long lvalue;

/* Ariel - Check this function */

  cs2_scanner_ready (s, CS2_STATUS_READY);
  cs2_init_buffer (s);
  cs2_parse_cmd (s, "2a 00 88 00 00 03");
  cs2_pack_byte (s, ((4 + s->n_frames * 16) >> 16) & 0xff);
  cs2_pack_byte (s, ((4 + s->n_frames * 16) >> 8) & 0xff);
  cs2_pack_byte (s, (4 + s->n_frames * 16) & 0xff);
  cs2_parse_cmd (s, "00");

  cs2_pack_byte (s, ((4 + s->n_frames * 16) >> 8) & 0xff);
  cs2_pack_byte (s, (4 + s->n_frames * 16) & 0xff);
  cs2_pack_byte (s, s->n_frames);
  cs2_pack_byte (s, s->n_frames);
  for (i_boundary = 0; i_boundary < s->n_frames; i_boundary++)
    {
      lvalue = s->frame_offset * i_boundary + s->subframe / s->unit_mm;
      cs2_pack_byte (s, (lvalue >> 24) & 0xff);
      cs2_pack_byte (s, (lvalue >> 16) & 0xff);
      cs2_pack_byte (s, (lvalue >> 8) & 0xff);
      cs2_pack_byte (s, lvalue & 0xff);

      lvalue = 0;
      cs2_pack_byte (s, (lvalue >> 24) & 0xff);
      cs2_pack_byte (s, (lvalue >> 16) & 0xff);
      cs2_pack_byte (s, (lvalue >> 8) & 0xff);
      cs2_pack_byte (s, lvalue & 0xff);

      lvalue = s->frame_offset * i_boundary + s->subframe / s->unit_mm + s->frame_offset - 1;
      cs2_pack_byte (s, (lvalue >> 24) & 0xff);
      cs2_pack_byte (s, (lvalue >> 16) & 0xff);
      cs2_pack_byte (s, (lvalue >> 8) & 0xff);
      cs2_pack_byte (s, lvalue & 0xff);

      lvalue = s->boundaryx - 1;
      cs2_pack_byte (s, (lvalue >> 24) & 0xff);
      cs2_pack_byte (s, (lvalue >> 16) & 0xff);
      cs2_pack_byte (s, (lvalue >> 8) & 0xff);
      cs2_pack_byte (s, lvalue & 0xff);
    }
  status = cs2_issue_cmd (s);
  if (status)
    return status;

  return SANE_STATUS_GOOD;
}

static SANE_Status
cs2_scan (cs2_t * s, cs2_scan_t type)
{
  SANE_Status status;
  int i_colour;
  cs2_pixel_t pixel;
  cs2_pixel_t *lut;

  /* wait for device to be ready with document, and set device unit */

  status = cs2_scanner_ready (s, CS2_STATUS_NO_DOCS);
  if (status)
    return status;
  if (s->status & CS2_STATUS_NO_DOCS)
    return SANE_STATUS_NO_DOCS;

  cs2_scanner_ready (s, CS2_STATUS_READY);
  cs2_init_buffer (s);
  /* Ariel - the '0b' byte in the 'else' part seems to be wrong, should be 0 */
  if ((s->type == CS2_TYPE_LS50) || (s->type == CS2_TYPE_LS5000))
    cs2_parse_cmd (s, "15 10 00 00 14 00 00 00 00 08 00 00 00 00 00 00 00 01 03 06 00 00");
  else
    cs2_parse_cmd (s, "15 10 00 00 0c 00 0b 00 00 00 03 06 00 00");
  cs2_pack_byte (s, (s->unit_dpi >> 8) & 0xff);
  cs2_pack_byte (s, s->unit_dpi & 0xff);
  cs2_parse_cmd (s, "00 00");
  status = cs2_issue_cmd (s);
  if (status)
    return status;

  status = cs2_convert_options (s);
  if (status)
    return status;

  /* Ariel - Is this the best place to initialize it? */
  s->block_padding = 0;

  status = cs2_set_boundary (s);
  if (status)
    return status;

  switch (type)
    {
    case CS2_SCAN_NORMAL:

      for (i_colour = 0; i_colour < s->n_colour_in; i_colour++)
	{
	  cs2_scanner_ready (s, CS2_STATUS_READY);

	  switch (i_colour)
	    {
	    case 0:
	      lut = s->lut_r;
	      break;
	    case 1:
	      lut = s->lut_g;
	      break;
	    case 2:
	      lut = s->lut_b;
	      break;
	    case 3:
	      lut = s->lut_neutral;
	      break;
	    default:
	      DBG (1,
		   "BUG: cs2_scan(): Unknown colour number for LUT download.\n");
	      return SANE_STATUS_INVAL;
	      break;
	    }

	  cs2_init_buffer (s);
	  cs2_parse_cmd (s, "2a 00 03 00");
	  cs2_pack_byte (s, cs2_colour_list[i_colour]);
	  cs2_pack_byte (s, 2 - 1);	/* XXXXXXXXXX number of bytes per data point - 1 */
	  cs2_pack_byte (s, ((2 * s->n_lut) >> 16) & 0xff);	/* XXXXXXXXXX 2 bytes per point */
	  cs2_pack_byte (s, ((2 * s->n_lut) >> 8) & 0xff);	/* XXXXXXXXXX 2 bytes per point */
	  cs2_pack_byte (s, (2 * s->n_lut) & 0xff);	/* XXXXXXXXXX 2 bytes per point */
	  cs2_pack_byte (s, 0x00);

	  for (pixel = 0; pixel < s->n_lut; pixel++)
	    {			/* XXXXXXXXXXXXXXX 2 bytes per point */
	      cs2_pack_byte (s, (lut[pixel] >> 8) & 0xff);
	      cs2_pack_byte (s, lut[pixel] & 0xff);
	    }

	  status = cs2_issue_cmd (s);
	  if (status)
	    return status;
	}

      break;

    default:
      break;
    }

  for (i_colour = 0; i_colour < s->n_colour_in; i_colour++)
    {
      cs2_scanner_ready (s, CS2_STATUS_READY);

      cs2_init_buffer (s);
      if ((s->type == CS2_TYPE_LS40) || (s->type == CS2_TYPE_LS4000))
	cs2_parse_cmd (s, "24 00 00 00 00 00 00 00 3a 80");
      else
	cs2_parse_cmd (s, "24 00 00 00 00 00 00 00 3a 00");
      cs2_parse_cmd (s, "00 00 00 00 00 00 00 32");

      cs2_pack_byte (s, cs2_colour_list[i_colour]);

      cs2_pack_byte (s, 0x00);

      cs2_pack_byte (s, s->real_resx >> 8);
      cs2_pack_byte (s, s->real_resx & 0xff);
      cs2_pack_byte (s, s->real_resy >> 8);
      cs2_pack_byte (s, s->real_resy & 0xff);

      cs2_pack_byte (s, (s->real_xoffset >> 24) & 0xff);
      cs2_pack_byte (s, (s->real_xoffset >> 16) & 0xff);
      cs2_pack_byte (s, (s->real_xoffset >> 8) & 0xff);
      cs2_pack_byte (s, s->real_xoffset & 0xff);

      cs2_pack_byte (s, (s->real_yoffset >> 24) & 0xff);
      cs2_pack_byte (s, (s->real_yoffset >> 16) & 0xff);
      cs2_pack_byte (s, (s->real_yoffset >> 8) & 0xff);
      cs2_pack_byte (s, s->real_yoffset & 0xff);

      cs2_pack_byte (s, (s->real_width >> 24) & 0xff);
      cs2_pack_byte (s, (s->real_width >> 16) & 0xff);
      cs2_pack_byte (s, (s->real_width >> 8) & 0xff);
      cs2_pack_byte (s, s->real_width & 0xff);

      cs2_pack_byte (s, (s->real_height >> 24) & 0xff);
      cs2_pack_byte (s, (s->real_height >> 16) & 0xff);
      cs2_pack_byte (s, (s->real_height >> 8) & 0xff);
      cs2_pack_byte (s, s->real_height & 0xff);

      cs2_pack_byte (s, 0x00);	/* brightness, etc. */
      cs2_pack_byte (s, 0x00);
      cs2_pack_byte (s, 0x00);
      cs2_pack_byte (s, 0x05);	/* image composition CCCCCCC */
      cs2_pack_byte (s, s->real_depth);	/* pixel composition */
      cs2_parse_cmd (s, "00 00 00 00 00 00 00 00 00 00 00 00 00");
      cs2_pack_byte (s, ((s->samples_per_scan - 1) << 4) + 0x00);	/* multiread, ordering */
      /* No need to use an undocumented bit in LS50 */
      if ((s->type == CS2_TYPE_LS50) || (s->type == CS2_TYPE_LS5000))
        cs2_pack_byte (s, 0x00 + (s->negative ? 0 : 1));	/* averaging, pos/neg */
      else
        cs2_pack_byte (s, 0x80 + (s->negative ? 0 : 1));	/* averaging, pos/neg */

      switch (type)
	{			/* scanning kind */
	case CS2_SCAN_NORMAL:
	  cs2_pack_byte (s, 0x01);
	  break;
	case CS2_SCAN_AE:
	  cs2_pack_byte (s, 0x20);
	  break;
	case CS2_SCAN_AE_WB:
	  cs2_pack_byte (s, 0x40);
	  break;
	default:
	  DBG (1, "BUG: cs2_scan(): Unknown scanning type.\n");
	  return SANE_STATUS_INVAL;
	}
      if (s->samples_per_scan == 1)
        cs2_pack_byte (s, 0x02);	/* scanning mode single */
      else
        cs2_pack_byte (s, 0x10);	/* scanning mode multi */
      cs2_pack_byte (s, 0x02);	/* colour interleaving */
      cs2_pack_byte (s, 0xff);	/* (ae) */
      if (i_colour == 3)	/* infrared */
	cs2_parse_cmd (s, "00 00 00 00");	/* automatic */
      else
	{
	  cs2_pack_byte (s,
			 (s->
			  real_exposure[cs2_colour_list[i_colour]] >> 24) &
			 0xff);
	  cs2_pack_byte (s,
			 (s->
			  real_exposure[cs2_colour_list[i_colour]] >> 16) &
			 0xff);
	  cs2_pack_byte (s,
			 (s->
			  real_exposure[cs2_colour_list[i_colour]] >> 8) &
			 0xff);
	  cs2_pack_byte (s,
			 s->real_exposure[cs2_colour_list[i_colour]] & 0xff);
	}
      status = cs2_issue_cmd (s);
      if (status)
	return status;
    }

  cs2_scanner_ready (s, CS2_STATUS_READY);
  cs2_focus (s);

  cs2_scanner_ready (s, CS2_STATUS_READY);
  cs2_init_buffer (s);
  switch (s->n_colour_in)
    {
    case 3:
      cs2_parse_cmd (s, "1b 00 00 00 03 00 01 02 03");
      break;
    case 4:
      cs2_parse_cmd (s, "1b 00 00 00 04 00 01 02 03 09");
      break;
    default:
      DBG (1, "BUG: cs2_scan(): Unknown number of input colours.\n");
      break;
    }
  status = cs2_issue_cmd (s);
  if (status)
    return status;
  if (s->status == CS2_STATUS_REISSUE)
    {
      /* Make sure we don't affect the behaviour for other scanners */
      if ((s->type == CS2_TYPE_LS50) || (s->type == CS2_TYPE_LS5000))
        {
          cs2_init_buffer (s);
          cs2_parse_cmd (s, "28 00 87 00 00 00 00 00 06 00");
          s->n_recv = 6;
          status = cs2_issue_cmd (s);
          if (status)
            return status;
          cs2_init_buffer (s);
          cs2_parse_cmd (s, "28 00 87 00 00 00 00 00");
          cs2_pack_byte (s, s->recv_buf[5] + 6);
          cs2_parse_cmd (s, "00");
          s->n_recv = s->recv_buf[5] + 6;
          status = cs2_issue_cmd (s);
          if (status)
            return status;
          if ((s->recv_buf[11] != 0x08) || (s->recv_buf[12] != 0x00))
            DBG (1, "BUG: cs2_scan(): Unexpected block_padding position.\n");
          s->block_padding = 256 * s->recv_buf[19] + s->recv_buf[20];
          cs2_init_buffer (s);
          switch (s->n_colour_in)
            {
            case 3:
              cs2_parse_cmd (s, "1b 00 00 00 03 00 01 02 03");
              break;
            case 4:
              cs2_parse_cmd (s, "1b 00 00 00 04 00 01 02 03 09");
              break;
            }
        }
      status = cs2_issue_cmd (s);
      if (status)
	return status;
    }

  return SANE_STATUS_GOOD;
}

static void *
cs2_xmalloc (size_t size)
{
  register void *value = malloc (size);

  if (!value)
    DBG (0, "Error: cs2_xmalloc(): Failed to malloc() %lu bytes.\n",
	 (unsigned long) size);

  return value;
}

static void *
cs2_xrealloc (void *p, size_t size)
{
  register void *value;

  if (!size)
    return p;

  value = realloc (p, size);

  if (!value)
    DBG (0, "Error: cs2_xrealloc(): Failed to realloc() %lu bytes.\n",
	 (unsigned long) size);

  return value;
}

static void
cs2_xfree (const void *p)
{
  if (p)
    free ((void *) p);
}
