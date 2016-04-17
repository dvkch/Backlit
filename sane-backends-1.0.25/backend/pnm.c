/* sane - Scanner Access Now Easy.
   Copyright (C) 1996, 1997 Andreas Beck
   Copyright (C) 2000, 2001 Michael Herder <crapsite@gmx.net>
   Copyright (C) 2001, 2002 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2008 Stéphane Voltz <stef.dev@free.fr>
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

#define BUILD 9

#include "../include/sane/config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#define BACKEND_NAME	pnm
#include "../include/sane/sanei_backend.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#define MAGIC	(void *)0xab730324

static int is_open = 0;
static int rgb_comp = 0;
static int three_pass = 0;
static int hand_scanner = 0;
static int pass = 0;
static char filename[PATH_MAX] = "/tmp/input.ppm";
static SANE_Word status_none = SANE_TRUE;
static SANE_Word status_eof = SANE_FALSE;
static SANE_Word status_jammed = SANE_FALSE;
static SANE_Word status_nodocs = SANE_FALSE;
static SANE_Word status_coveropen = SANE_FALSE;
static SANE_Word status_ioerror = SANE_FALSE;
static SANE_Word status_nomem = SANE_FALSE;
static SANE_Word status_accessdenied = SANE_FALSE;
static SANE_Word test_option = 0;
#ifdef SANE_STATUS_WARMING_UP
static SANE_Word warming_up = SANE_FALSE;
static struct timeval start;
#endif

static SANE_Fixed bright = 0;
static SANE_Word res = 75;
static SANE_Fixed contr = 0;
static SANE_Bool gray = SANE_FALSE;
static SANE_Bool usegamma = SANE_FALSE;
static SANE_Word gamma[4][256];
static enum
{
  ppm_bitmap,
  ppm_greyscale,
  ppm_color
}
ppm_type = ppm_color;
static FILE *infile = NULL;
static const SANE_Word resbit_list[] = {
  17,
  75, 90, 100, 120, 135, 150, 165, 180, 195,
  200, 210, 225, 240, 255, 270, 285, 300
};
static const SANE_Range percentage_range = {
  -100 << SANE_FIXED_SCALE_SHIFT,	/* minimum */
  100 << SANE_FIXED_SCALE_SHIFT,	/* maximum */
  0 << SANE_FIXED_SCALE_SHIFT	/* quantization */
};
static const SANE_Range gamma_range = {
  0,				/* minimum */
  255,				/* maximum */
  0				/* quantization */
};
typedef enum
{
  opt_num_opts = 0,
  opt_source_group,
  opt_filename,
  opt_resolution,
  opt_enhancement_group,
  opt_brightness,
  opt_contrast,
  opt_grayify,
  opt_three_pass,
  opt_hand_scanner,
  opt_default_enhancements,
  opt_read_only,
  opt_gamma_group,
  opt_custom_gamma,
  opt_gamma,
  opt_gamma_r,
  opt_gamma_g,
  opt_gamma_b,
  opt_status_group,
  opt_status,
  opt_status_eof,
  opt_status_jammed,
  opt_status_nodocs,
  opt_status_coveropen,
  opt_status_ioerror,
  opt_status_nomem,
  opt_status_accessdenied,

  /* must come last: */
  num_options
}
pnm_opts;

static SANE_Option_Descriptor sod[] = {
  {				/* opt_num_opts */
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
  {				/* opt_source_group */
   "",
   SANE_I18N ("Source Selection"),
   "",
   SANE_TYPE_GROUP,
   SANE_UNIT_NONE,
   0,
   0,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_filename */
   SANE_NAME_FILE,
   SANE_TITLE_FILE,
   SANE_DESC_FILE,
   SANE_TYPE_STRING,
   SANE_UNIT_NONE,
   sizeof (filename),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {
   /* opt_resolution */
   SANE_NAME_SCAN_RESOLUTION,
   SANE_TITLE_SCAN_RESOLUTION,
   SANE_DESC_SCAN_RESOLUTION,
   SANE_TYPE_INT,
   SANE_UNIT_DPI,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_AUTOMATIC,
   SANE_CONSTRAINT_WORD_LIST,
   {(SANE_String_Const *) resbit_list}
   }
  ,
  {				/* opt_enhancement_group */
   "",
   SANE_I18N ("Image Enhancement"),
   "",
   SANE_TYPE_GROUP,
   SANE_UNIT_NONE,
   0,
   0,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_brightness */
   SANE_NAME_BRIGHTNESS,
   SANE_TITLE_BRIGHTNESS,
   SANE_DESC_BRIGHTNESS,
   SANE_TYPE_FIXED,
   SANE_UNIT_PERCENT,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_RANGE,
   {(SANE_String_Const *) & percentage_range}	/* this is ANSI conformant! */
   }
  ,
  {				/* opt_contrast */
   SANE_NAME_CONTRAST,
   SANE_TITLE_CONTRAST,
   SANE_DESC_CONTRAST,
   SANE_TYPE_FIXED,
   SANE_UNIT_PERCENT,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_RANGE,
   {(SANE_String_Const *) & percentage_range}	/* this is ANSI conformant! */
   }
  ,
  {				/* opt_grayify */
   "grayify",
   SANE_I18N ("Grayify"),
   SANE_I18N ("Load the image as grayscale."),
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_three_pass */
   "three-pass",
   SANE_I18N ("Three-Pass Simulation"),
   SANE_I18N
   ("Simulate a three-pass scanner by returning 3 separate frames.  "
    "For kicks, it returns green, then blue, then red."),
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_hand_scanner */
   "hand-scanner",
   SANE_I18N ("Hand-Scanner Simulation"),
   SANE_I18N ("Simulate a hand-scanner.  Hand-scanners often do not know the "
	      "image height a priori.  Instead, they return a height of -1.  "
	      "Setting this option allows one to test whether a frontend can "
	      "handle this correctly."),
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_default_enhancements */
   "default-enhancements",
   SANE_I18N ("Defaults"),
   SANE_I18N ("Set default values for enhancement controls (brightness & "
	      "contrast)."),
   SANE_TYPE_BUTTON,
   SANE_UNIT_NONE,
   0,
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_read_only */
   "read-only",
   SANE_I18N ("Read only test-option"),
   SANE_I18N ("Let's see whether frontends can treat this right"),
   SANE_TYPE_INT,
   SANE_UNIT_PERCENT,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_gamma_group */
   "",
   SANE_I18N ("Gamma Tables"),
   "",
   SANE_TYPE_GROUP,
   SANE_UNIT_NONE,
   0,
   0,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_custom_gamma */
   SANE_NAME_CUSTOM_GAMMA,
   SANE_TITLE_CUSTOM_GAMMA,
   SANE_DESC_CUSTOM_GAMMA,
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_gamma */
   SANE_NAME_GAMMA_VECTOR,
   SANE_TITLE_GAMMA_VECTOR,
   SANE_DESC_GAMMA_VECTOR,
   SANE_TYPE_INT,
   SANE_UNIT_NONE,
   sizeof (SANE_Word) * 256,
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE,
   SANE_CONSTRAINT_RANGE,
   {(SANE_String_Const *) & gamma_range}
   }
  ,
  {				/* opt_gamma_r */
   SANE_NAME_GAMMA_VECTOR_R,
   SANE_TITLE_GAMMA_VECTOR_R,
   SANE_DESC_GAMMA_VECTOR_R,
   SANE_TYPE_INT,
   SANE_UNIT_NONE,
   sizeof (SANE_Word) * 256,
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE,
   SANE_CONSTRAINT_RANGE,
   {(SANE_String_Const *) & gamma_range}
   }
  ,
  {				/* opt_gamma_g */
   SANE_NAME_GAMMA_VECTOR_G,
   SANE_TITLE_GAMMA_VECTOR_G,
   SANE_DESC_GAMMA_VECTOR_G,
   SANE_TYPE_INT,
   SANE_UNIT_NONE,
   sizeof (SANE_Word) * 256,
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE,
   SANE_CONSTRAINT_RANGE,
   {(SANE_String_Const *) & gamma_range}
   }
  ,
  {				/* opt_gamma_b */
   SANE_NAME_GAMMA_VECTOR_B,
   SANE_TITLE_GAMMA_VECTOR_B,
   SANE_DESC_GAMMA_VECTOR_B,
   SANE_TYPE_INT,
   SANE_UNIT_NONE,
   sizeof (SANE_Word) * 256,
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE,
   SANE_CONSTRAINT_RANGE,
   {(SANE_String_Const *) & gamma_range}
   }
  ,
  {				/* opt_status_group */
   "",
   SANE_I18N ("Status Code Simulation"),
   "",
   SANE_TYPE_GROUP,
   SANE_UNIT_NONE,
   0,
   SANE_CAP_ADVANCED,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_status */
   "status",
   SANE_I18N ("Do not force status code"),
   SANE_I18N ("Do not force the backend to return a status code."),
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Bool),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_status_eof */
   "status-eof",
   SANE_I18N ("Return SANE_STATUS_EOF"),
   SANE_I18N ("Force the backend to return the status code SANE_STATUS_EOF "
	      "after sane_read() has been called."),
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Bool),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_status_jammed */
   "status-jammed",
   SANE_I18N ("Return SANE_STATUS_JAMMED"),
   SANE_I18N
   ("Force the backend to return the status code SANE_STATUS_JAMMED "
    "after sane_read() has been called."),
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Bool),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_status_nodocs */
   "status-nodocs",
   SANE_I18N ("Return SANE_STATUS_NO_DOCS"),
   SANE_I18N ("Force the backend to return the status code "
	      "SANE_STATUS_NO_DOCS after sane_read() has been called."),
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Bool),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_status_coveropen */
   "status-coveropen",
   SANE_I18N ("Return SANE_STATUS_COVER_OPEN"),
   SANE_I18N ("Force the backend to return the status code "
	      "SANE_STATUS_COVER_OPEN after sane_read() has been called."),
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Bool),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_status_ioerror */
   "status-ioerror",
   SANE_I18N ("Return SANE_STATUS_IO_ERROR"),
   SANE_I18N ("Force the backend to return the status code "
	      "SANE_STATUS_IO_ERROR after sane_read() has been called."),
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Bool),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_status_nomem */
   "status-nomem",
   SANE_I18N ("Return SANE_STATUS_NO_MEM"),
   SANE_I18N
   ("Force the backend to return the status code SANE_STATUS_NO_MEM "
    "after sane_read() has been called."),
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Bool),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
  ,
  {				/* opt_status_accessdenied */
   "status-accessdenied",
   SANE_I18N ("Return SANE_STATUS_ACCESS_DENIED"),
   SANE_I18N ("Force the backend to return the status code "
	      "SANE_STATUS_ACCESS_DENIED after sane_read() has been called."),
   SANE_TYPE_BOOL,
   SANE_UNIT_NONE,
   sizeof (SANE_Bool),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}
   }
};

static SANE_Parameters parms = {
  SANE_FRAME_RGB,
  0,
  0,				/* Number of bytes returned per scan line: */
  0,				/* Number of pixels per scan line.  */
  0,				/* Number of lines for the current scan.  */
  8,				/* Number of bits per sample. */
};

/* This library is a demo implementation of a SANE backend.  It
   implements a virtual device, a PNM file-filter. */
SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  DBG_INIT ();

  DBG (2, "sane_init: version_code %s 0, authorize %s 0\n",
       version_code == 0 ? "=" : "!=", authorize == 0 ? "=" : "!=");
  DBG (1, "sane_init: SANE pnm backend version %d.%d.%d from %s\n",
       SANE_CURRENT_MAJOR, V_MINOR, BUILD, PACKAGE_STRING);

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  DBG (2, "sane_exit\n");
  return;
}

/* Device select/open/close */

static const SANE_Device dev[] = {
  {
   "0",
   "Noname",
   "PNM file reader",
   "virtual device"},
  {
   "1",
   "Noname",
   "PNM file reader",
   "virtual device"},
#ifdef SANE_STATUS_HW_LOCKED
  {
   "locked",
   "Noname",
   "Hardware locked",
   "virtual device"},
#endif
#ifdef SANE_STATUS_WARMING_UP
  {
   "warmup",
   "Noname",
   "Always warming up",
   "virtual device"},
#endif
};

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device *devlist[] = {
    dev + 0, dev + 1,
#ifdef SANE_STATUS_HW_LOCKED
    dev + 2,
#endif
#ifdef SANE_STATUS_WARMING_UP
    dev + 3,
#endif
    0
  };

  DBG (2, "sane_get_devices: local_only = %d\n", local_only);
  *device_list = devlist;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  int i;

  if (!devicename)
    return SANE_STATUS_INVAL;
  DBG (2, "sane_open: devicename = \"%s\"\n", devicename);

  if (!devicename[0])
    i = 0;
  else
    for (i = 0; i < NELEMS (dev); ++i)
      if (strcmp (devicename, dev[i].name) == 0)
	break;
  if (i >= NELEMS (dev))
    return SANE_STATUS_INVAL;

  if (is_open)
    return SANE_STATUS_DEVICE_BUSY;

  is_open = 1;
  *handle = MAGIC;
  for (i = 0; i < 256; i++)
    {
      gamma[0][i] = i;
      gamma[1][i] = i;
      gamma[2][i] = i;
      gamma[3][i] = i;
    }

#ifdef SANE_STATUS_HW_LOCKED
  if(strncmp(devicename,"locked",6)==0)
    return SANE_STATUS_HW_LOCKED;
#endif

#ifdef SANE_STATUS_WARMING_UP
  if(strncmp(devicename,"warmup",6)==0)
    {
      warming_up = SANE_TRUE;
      start.tv_sec = 0;
    }
#endif

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  DBG (2, "sane_close\n");
  if (handle == MAGIC)
    is_open = 0;
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  DBG (2, "sane_get_option_descriptor: option = %d\n", option);
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
  SANE_Int myinfo = 0;
  SANE_Status status;
  int v;
  v = 75;

  DBG (2, "sane_control_option: handle=%p, opt=%d, act=%d, val=%p, info=%p\n",
       handle, option, action, value, info);

  if (handle != MAGIC || !is_open)
    {
      DBG (1, "sane_control_option: unknown handle or not open\n");
      return SANE_STATUS_INVAL;	/* Unknown handle ... */
    }

  if (option < 0 || option >= NELEMS (sod))
    {
      DBG (1, "sane_control_option: option %d < 0 or >= number of options\n",
	   option);
      return SANE_STATUS_INVAL;	/* Unknown option ... */
    }

  if (!SANE_OPTION_IS_ACTIVE (sod[option].cap))
    {
      DBG (4, "sane_control_option: option is inactive\n");
      return SANE_STATUS_INVAL;
    }

  switch (action)
    {
    case SANE_ACTION_SET_AUTO:
      if (!SANE_OPTION_IS_SETTABLE (sod[option].cap))
	{
	  DBG (4, "sane_control_option: option is not settable\n");
	  return SANE_STATUS_INVAL;
	}
      status = sanei_constrain_value (sod + option, (void *) &v, &myinfo);
      if (status != SANE_STATUS_GOOD)
	return status;
      switch (option)
	{
	case opt_resolution:
	  res = 75;
	  myinfo |= SANE_INFO_RELOAD_PARAMS;
	  break;
	default:
	  return SANE_STATUS_INVAL;
	}
      break;
    case SANE_ACTION_SET_VALUE:
      if (!SANE_OPTION_IS_SETTABLE (sod[option].cap))
	{
	  DBG (4, "sane_control_option: option is not settable\n");
	  return SANE_STATUS_INVAL;
	}
      status = sanei_constrain_value (sod + option, value, &myinfo);
      if (status != SANE_STATUS_GOOD)
	return status;
      switch (option)
	{
	case opt_filename:
	  if ((strlen (value) + 1) > sizeof (filename))
	    return SANE_STATUS_NO_MEM;
	  strcpy (filename, value);
	  myinfo |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case opt_resolution:
	  res = *(SANE_Word *) value;
	  break;
	case opt_brightness:
	  bright = *(SANE_Word *) value;
	  break;
	case opt_contrast:
	  contr = *(SANE_Word *) value;
	  break;
	case opt_grayify:
	  gray = !!*(SANE_Word *) value;
	  if (usegamma)
	    {
	      if (gray)
		{
		  sod[opt_gamma].cap &= ~SANE_CAP_INACTIVE;
		  sod[opt_gamma_r].cap |= SANE_CAP_INACTIVE;
		  sod[opt_gamma_g].cap |= SANE_CAP_INACTIVE;
		  sod[opt_gamma_b].cap |= SANE_CAP_INACTIVE;
		}
	      else
		{
		  sod[opt_gamma].cap |= SANE_CAP_INACTIVE;
		  sod[opt_gamma_r].cap &= ~SANE_CAP_INACTIVE;
		  sod[opt_gamma_g].cap &= ~SANE_CAP_INACTIVE;
		  sod[opt_gamma_b].cap &= ~SANE_CAP_INACTIVE;
		}
	    }
	  else
	    {
	      sod[opt_gamma].cap |= SANE_CAP_INACTIVE;
	      sod[opt_gamma_r].cap |= SANE_CAP_INACTIVE;
	      sod[opt_gamma_g].cap |= SANE_CAP_INACTIVE;
	      sod[opt_gamma_b].cap |= SANE_CAP_INACTIVE;
	    }
	  myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  break;
	case opt_three_pass:
	  three_pass = !!*(SANE_Word *) value;
	  myinfo |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case opt_hand_scanner:
	  hand_scanner = !!*(SANE_Word *) value;
	  myinfo |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case opt_default_enhancements:
	  bright = contr = 0;
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case opt_custom_gamma:
	  usegamma = *(SANE_Word *) value;
	  /* activate/deactivate gamma */
	  if (usegamma)
	    {
	      test_option = 100;
	      if (gray)
		{
		  sod[opt_gamma].cap &= ~SANE_CAP_INACTIVE;
		  sod[opt_gamma_r].cap |= SANE_CAP_INACTIVE;
		  sod[opt_gamma_g].cap |= SANE_CAP_INACTIVE;
		  sod[opt_gamma_b].cap |= SANE_CAP_INACTIVE;
		}
	      else
		{
		  sod[opt_gamma].cap |= SANE_CAP_INACTIVE;
		  sod[opt_gamma_r].cap &= ~SANE_CAP_INACTIVE;
		  sod[opt_gamma_g].cap &= ~SANE_CAP_INACTIVE;
		  sod[opt_gamma_b].cap &= ~SANE_CAP_INACTIVE;
		}
	    }
	  else
	    {
	      test_option = 0;
	      sod[opt_gamma].cap |= SANE_CAP_INACTIVE;
	      sod[opt_gamma_r].cap |= SANE_CAP_INACTIVE;
	      sod[opt_gamma_g].cap |= SANE_CAP_INACTIVE;
	      sod[opt_gamma_b].cap |= SANE_CAP_INACTIVE;
	    }
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case opt_gamma:
	  memcpy (&gamma[0][0], (SANE_Word *) value,
		  256 * sizeof (SANE_Word));
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case opt_gamma_r:
	  memcpy (&gamma[1][0], (SANE_Word *) value,
		  256 * sizeof (SANE_Word));
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case opt_gamma_g:
	  memcpy (&gamma[2][0], (SANE_Word *) value,
		  256 * sizeof (SANE_Word));
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case opt_gamma_b:
	  memcpy (&gamma[3][0], (SANE_Word *) value,
		  256 * sizeof (SANE_Word));
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	  /* status */
	case opt_status:
	  status_none = *(SANE_Word *) value;
	  if (status_none)
	    {
	      status_eof = SANE_FALSE;
	      status_jammed = SANE_FALSE;
	      status_nodocs = SANE_FALSE;
	      status_coveropen = SANE_FALSE;
	      status_ioerror = SANE_FALSE;
	      status_nomem = SANE_FALSE;
	      status_accessdenied = SANE_FALSE;
	    }
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case opt_status_eof:
	  status_eof = *(SANE_Word *) value;
	  if (status_eof)
	    {
	      status_none = SANE_FALSE;
	      status_jammed = SANE_FALSE;
	      status_nodocs = SANE_FALSE;
	      status_coveropen = SANE_FALSE;
	      status_ioerror = SANE_FALSE;
	      status_nomem = SANE_FALSE;
	      status_accessdenied = SANE_FALSE;
	    }
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case opt_status_jammed:
	  status_jammed = *(SANE_Word *) value;
	  if (status_jammed)
	    {
	      status_eof = SANE_FALSE;
	      status_none = SANE_FALSE;
	      status_nodocs = SANE_FALSE;
	      status_coveropen = SANE_FALSE;
	      status_ioerror = SANE_FALSE;
	      status_nomem = SANE_FALSE;
	      status_accessdenied = SANE_FALSE;
	    }
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case opt_status_nodocs:
	  status_nodocs = *(SANE_Word *) value;
	  if (status_nodocs)
	    {
	      status_eof = SANE_FALSE;
	      status_jammed = SANE_FALSE;
	      status_none = SANE_FALSE;
	      status_coveropen = SANE_FALSE;
	      status_ioerror = SANE_FALSE;
	      status_nomem = SANE_FALSE;
	      status_accessdenied = SANE_FALSE;
	    }
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case opt_status_coveropen:
	  status_coveropen = *(SANE_Word *) value;
	  if (status_coveropen)
	    {
	      status_eof = SANE_FALSE;
	      status_jammed = SANE_FALSE;
	      status_nodocs = SANE_FALSE;
	      status_none = SANE_FALSE;
	      status_ioerror = SANE_FALSE;
	      status_nomem = SANE_FALSE;
	      status_accessdenied = SANE_FALSE;
	    }
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case opt_status_ioerror:
	  status_ioerror = *(SANE_Word *) value;
	  if (status_ioerror)
	    {
	      status_eof = SANE_FALSE;
	      status_jammed = SANE_FALSE;
	      status_nodocs = SANE_FALSE;
	      status_coveropen = SANE_FALSE;
	      status_none = SANE_FALSE;
	      status_nomem = SANE_FALSE;
	      status_accessdenied = SANE_FALSE;
	    }
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case opt_status_nomem:
	  status_nomem = *(SANE_Word *) value;
	  if (status_nomem)
	    {
	      status_eof = SANE_FALSE;
	      status_jammed = SANE_FALSE;
	      status_nodocs = SANE_FALSE;
	      status_coveropen = SANE_FALSE;
	      status_ioerror = SANE_FALSE;
	      status_none = SANE_FALSE;
	      status_accessdenied = SANE_FALSE;
	    }
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	case opt_status_accessdenied:
	  status_accessdenied = *(SANE_Word *) value;
	  if (status_accessdenied)
	    {
	      status_eof = SANE_FALSE;
	      status_jammed = SANE_FALSE;
	      status_nodocs = SANE_FALSE;
	      status_coveropen = SANE_FALSE;
	      status_ioerror = SANE_FALSE;
	      status_nomem = SANE_FALSE;
	      status_none = SANE_FALSE;
	    }
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  break;
	default:
	  return SANE_STATUS_INVAL;
	}
      break;
    case SANE_ACTION_GET_VALUE:
      switch (option)
	{
	case opt_num_opts:
	  *(SANE_Word *) value = NELEMS (sod);
	  break;
	case opt_filename:
	  strcpy (value, filename);
	  break;
	case opt_resolution:
	  *(SANE_Word *) value = res;
	  break;
	case opt_brightness:
	  *(SANE_Word *) value = bright;
	  break;
	case opt_contrast:
	  *(SANE_Word *) value = contr;
	  break;
	case opt_grayify:
	  *(SANE_Word *) value = gray;
	  break;
	case opt_three_pass:
	  *(SANE_Word *) value = three_pass;
	  break;
	case opt_hand_scanner:
	  *(SANE_Word *) value = hand_scanner;
	  break;
	case opt_read_only:
	  *(SANE_Word *) value = test_option;
	  break;
	case opt_custom_gamma:
	  *(SANE_Word *) value = usegamma;
	  break;
	case opt_gamma:
	  memcpy ((SANE_Word *) value, &gamma[0][0],
		  256 * sizeof (SANE_Word));
	  break;
	case opt_gamma_r:
	  memcpy ((SANE_Word *) value, &gamma[1][0],
		  256 * sizeof (SANE_Word));
	  break;
	case opt_gamma_g:
	  memcpy ((SANE_Word *) value, &gamma[2][0],
		  256 * sizeof (SANE_Word));
	  break;
	case opt_gamma_b:
	  memcpy ((SANE_Word *) value, &gamma[3][0],
		  256 * sizeof (SANE_Word));
	  break;
	case opt_status:
	  *(SANE_Word *) value = status_none;
	  break;
	case opt_status_eof:
	  *(SANE_Word *) value = status_eof;
	  break;
	case opt_status_jammed:
	  *(SANE_Word *) value = status_jammed;
	  break;
	case opt_status_nodocs:
	  *(SANE_Word *) value = status_nodocs;
	  break;
	case opt_status_coveropen:
	  *(SANE_Word *) value = status_coveropen;
	  break;
	case opt_status_ioerror:
	  *(SANE_Word *) value = status_ioerror;
	  break;
	case opt_status_nomem:
	  *(SANE_Word *) value = status_nomem;
	  break;
	case opt_status_accessdenied:
	  *(SANE_Word *) value = status_accessdenied;
	  break;
	default:
	  return SANE_STATUS_INVAL;
	}
      break;
    }
  if (info)
    *info = myinfo;
  return SANE_STATUS_GOOD;
}

static void
get_line (char *buf, int len, FILE * f)
{
  do
    fgets (buf, len, f);
  while (*buf == '#');
}

static int
getparmfromfile (void)
{
  FILE *fn;
  int x, y;
  char buf[1024];

  parms.depth = 8;
  parms.bytes_per_line = parms.pixels_per_line = parms.lines = 0;
  if ((fn = fopen (filename, "rb")) == NULL)
    {
      DBG (1, "getparmfromfile: unable to open file \"%s\"\n", filename);
      return -1;
    }

  /* Skip comments. */
  do
    get_line (buf, sizeof (buf), fn);
  while (*buf == '#');
  if (!strncmp (buf, "P4", 2))
    {
      /* Binary monochrome. */
      parms.depth = 1;
      ppm_type = ppm_bitmap;
    }
  else if (!strncmp (buf, "P5", 2))
    {
      /* Grayscale. */
      parms.depth = 8;
      ppm_type = ppm_greyscale;
    }
  else if (!strncmp (buf, "P6", 2))
    {
      /* Color. */
      parms.depth = 8;
      ppm_type = ppm_color;
    }
  else
    {
      DBG (1, "getparmfromfile: %s is not a recognized PPM\n", filename);
      fclose (fn);
      return -1;
    }

  /* Skip comments. */
  do
    get_line (buf, sizeof (buf), fn);
  while (*buf == '#');
  sscanf (buf, "%d %d", &x, &y);

  parms.last_frame = SANE_TRUE;
  parms.bytes_per_line = (ppm_type == ppm_bitmap) ? (x + 7) / 8 : x;
  parms.pixels_per_line = x;
  if (hand_scanner)
    parms.lines = -1;
  else
    parms.lines = y;
  if ((ppm_type == ppm_greyscale) || (ppm_type == ppm_bitmap) || gray)
    parms.format = SANE_FRAME_GRAY;
  else
    {
      if (three_pass)
	{
	  parms.format = SANE_FRAME_RED + (pass + 1) % 3;
	  parms.last_frame = (pass >= 2);
	}
      else
	{
	  parms.format = SANE_FRAME_RGB;
	  parms.bytes_per_line *= 3;
	}
    }
  fclose (fn);
  return 0;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  int rc = SANE_STATUS_GOOD;

  DBG (2, "sane_get_parameters\n");
  if (handle != MAGIC || !is_open)
    rc = SANE_STATUS_INVAL;	/* Unknown handle ... */
  else if (getparmfromfile ())
    rc = SANE_STATUS_INVAL;
  *params = parms;
  return rc;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  char buf[1024];
  int nlines;
#ifdef SANE_STATUS_WARMING_UP
  struct timeval current;
#endif

  DBG (2, "sane_start\n");
  rgb_comp = 0;
  if (handle != MAGIC || !is_open)
    return SANE_STATUS_INVAL;	/* Unknown handle ... */

#ifdef SANE_STATUS_WARMING_UP
  if(warming_up == SANE_TRUE)
   {
      gettimeofday(&current,NULL);
      if(current.tv_sec-start.tv_sec>5)
	{
	   start.tv_sec = current.tv_sec;
	   return SANE_STATUS_WARMING_UP;
	}
      if(current.tv_sec-start.tv_sec<5)
	return SANE_STATUS_WARMING_UP;
   }
#endif

  if (infile != NULL)
    {
      fclose (infile);
      infile = NULL;
      if (!three_pass || ++pass >= 3)
	return SANE_STATUS_EOF;
    }

  if (getparmfromfile ())
    return SANE_STATUS_INVAL;

  if ((infile = fopen (filename, "rb")) == NULL)
    {
      DBG (1, "sane_start: unable to open file \"%s\"\n", filename);
      return SANE_STATUS_INVAL;
    }

  /* Skip the header (only two lines for a bitmap). */
  nlines = (ppm_type == ppm_bitmap) ? 1 : 0;
  while (nlines < 3)
    {
      /* Skip comments. */
      get_line (buf, sizeof (buf), infile);
      if (*buf != '#')
	nlines++;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Int rgblength = 0;
static SANE_Byte *rgbbuf = 0;
static SANE_Byte rgbleftover[3] = { 0, 0, 0 };

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * data,
	   SANE_Int max_length, SANE_Int * length)
{
  int len, x, hlp;

  DBG (2, "sane_read: max_length = %d, rgbleftover = {%d, %d, %d}\n",
       max_length, rgbleftover[0], rgbleftover[1], rgbleftover[2]);
  if (!length)
    {
      DBG (1, "sane_read: length == NULL\n");
      return SANE_STATUS_INVAL;
    }
  *length = 0;
  if (!data)
    {
      DBG (1, "sane_read: data == NULL\n");
      return SANE_STATUS_INVAL;
    }
  if (handle != MAGIC)
    {
      DBG (1, "sane_read: unknown handle\n");
      return SANE_STATUS_INVAL;
    }
  if (!is_open)
    {
      DBG (1, "sane_read: call sane_open first\n");
      return SANE_STATUS_INVAL;
    }
  if (!infile)
    {
      DBG (1, "sane_read: scan was cancelled\n");
      return SANE_STATUS_CANCELLED;
    }
  if (feof (infile))
    {
      DBG (2, "sane_read: EOF reached\n");
      return SANE_STATUS_EOF;
    }

  if (status_jammed == SANE_TRUE)
    return SANE_STATUS_JAMMED;
  if (status_eof == SANE_TRUE)
    return SANE_STATUS_EOF;
  if (status_nodocs == SANE_TRUE)
    return SANE_STATUS_NO_DOCS;
  if (status_coveropen == SANE_TRUE)
    return SANE_STATUS_COVER_OPEN;
  if (status_ioerror == SANE_TRUE)
    return SANE_STATUS_IO_ERROR;
  if (status_nomem == SANE_TRUE)
    return SANE_STATUS_NO_MEM;
  if (status_accessdenied == SANE_TRUE)
    return SANE_STATUS_ACCESS_DENIED;

  /* Allocate a buffer for the RGB values. */
  if (ppm_type == ppm_color && (gray || three_pass))
    {
      SANE_Byte *p, *q, *rgbend;
      if (rgbbuf == 0 || rgblength < 3 * max_length)
	{
	  /* Allocate a new rgbbuf. */
	  free (rgbbuf);
	  rgblength = 3 * max_length;
	  rgbbuf = malloc (rgblength);
	  if (rgbbuf == 0)
	    return SANE_STATUS_NO_MEM;
	}
      else
	rgblength = 3 * max_length;

      /* Copy any leftovers into the buffer. */
      q = rgbbuf;
      p = rgbleftover + 1;
      while (p - rgbleftover <= rgbleftover[0])
	*q++ = *p++;

      /* Slurp in the RGB buffer. */
      len = fread (q, 1, rgblength - rgbleftover[0], infile);
      rgbend = rgbbuf + len;

      q = data;
      if (gray)
	{
	  /* Zip through the buffer, converting color data to grayscale. */
	  for (p = rgbbuf; p < rgbend; p += 3)
	    *q++ = ((long) p[0] + p[1] + p[2]) / 3;
	}
      else
	{
	  /* Zip through the buffer, extracting data for this pass. */
	  for (p = (rgbbuf + (pass + 1) % 3); p < rgbend; p += 3)
	    *q++ = *p;
	}

      /* Save any leftovers in the array. */
      rgbleftover[0] = len % 3;
      p = rgbbuf + (len - rgbleftover[0]);
      q = rgbleftover + 1;
      while (p < rgbend)
	*q++ = *p++;

      len /= 3;
    }
  else
    /* Suck in as much of the file as possible, since it's already in the
       correct format. */
    len = fread (data, 1, max_length, infile);

  if (len == 0)
    {
      if (feof (infile))
	{
	  DBG (2, "sane_read: EOF reached\n");
	  return SANE_STATUS_EOF;
	}
      else
	{
	  DBG (1, "sane_read: error while reading file (%s)\n",
	       strerror (errno));
	  return SANE_STATUS_IO_ERROR;
	}
    }

  if (parms.depth == 8)
    {
      /* Do the transformations ... DEMO ONLY ! THIS MAKES NO SENSE ! */
      for (x = 0; x < len; x++)
	{
	  hlp = *((unsigned char *) data + x) - 128;
	  hlp *= (contr + (100 << SANE_FIXED_SCALE_SHIFT));
	  hlp /= 100 << SANE_FIXED_SCALE_SHIFT;
	  hlp += (bright >> SANE_FIXED_SCALE_SHIFT) + 128;
	  if (hlp < 0)
	    hlp = 0;
	  if (hlp > 255)
	    hlp = 255;
	  *(data + x) = hlp;
	}
      /*gamma */
      if (usegamma)
	{
	  unsigned char uc;
	  if (gray)
	    {
	      for (x = 0; x < len; x++)
		{
		  uc = *((unsigned char *) data + x);
		  uc = gamma[0][uc];
		  *(data + x) = uc;
		}
	    }
	  else
	    {
	      for (x = 0; x < len; x++)
		{
		  if (parms.format == SANE_FRAME_RGB)
		    {
		      uc = *((unsigned char *) (data + x));
		      uc = (unsigned char) gamma[rgb_comp + 1][(int) uc];
		      *((unsigned char *) data + x) = uc;
		      rgb_comp += 1;
		      if (rgb_comp > 2)
			rgb_comp = 0;
		    }
		  else
		    {
		      int f = 0;
		      if (parms.format == SANE_FRAME_RED)
			f = 1;
		      if (parms.format == SANE_FRAME_GREEN)
			f = 2;
		      if (parms.format == SANE_FRAME_BLUE)
			f = 3;
		      if (f)
			{
			  uc = *((unsigned char *) (data + x));
			  uc = (unsigned char) gamma[f][(int) uc];
			  *((unsigned char *) data + x) = uc;
			}
		    }
		}
	    }
	}
    }
  *length = len;
  DBG (2, "sane_read: read %d bytes\n", len);
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  DBG (2, "sane_cancel: handle = %p\n", handle);
  pass = 0;
  if (infile != NULL)
    {
      fclose (infile);
      infile = NULL;
    }
  return;
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  DBG (2, "sane_set_io_mode: handle = %p, non_blocking = %d\n", handle,
       non_blocking);
  if (!infile)
    {
      DBG (1, "sane_set_io_mode: not scanning\n");
      return SANE_STATUS_INVAL;
    }
  if (non_blocking)
    return SANE_STATUS_UNSUPPORTED;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  DBG (2, "sane_get_select_fd: handle = %p, fd %s 0\n", handle,
       fd ? "!=" : "=");
  return SANE_STATUS_UNSUPPORTED;
}
