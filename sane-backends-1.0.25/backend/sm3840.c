/* sane - Scanner Access Now Easy.

   ScanMaker 3840 Backend
   Copyright (C) 2005-7 Earle F. Philhower, III
   earle@ziplabel.com - http://www.ziplabel.com

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

*/




#include "../include/sane/config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>

#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"

#define BACKENDNAME sm3840
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_config.h"

#include "sm3840.h"

#include "sm3840_scan.c"
#include "sm3840_lib.c"

static double sm3840_unit_convert (SANE_Int val);

static int num_devices;
static SM3840_Device *first_dev;
static SM3840_Scan *first_handle;
static const SANE_Device **devlist = 0;

static const SANE_String_Const mode_list[] = {
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_COLOR,
  SANE_VALUE_SCAN_MODE_LINEART,
  SANE_VALUE_SCAN_MODE_HALFTONE,
  0
};

static const SANE_Word resolution_list[] = {
  4, 1200, 600, 300, 150
};

static const SANE_Word bpp_list[] = {
  2, 8, 16
};

static const SANE_Range x_range = {
  SANE_FIX (0),
  SANE_FIX (215.91),		/* 8.5 inches */
  SANE_FIX (0)
};

static const SANE_Range y_range = {
  SANE_FIX (0),
  SANE_FIX (297.19),		/* 11.7 inches */
  SANE_FIX (0)
};

static const SANE_Range brightness_range = {
  1,
  4096,
  1.0
};

static const SANE_Range contrast_range = {
  SANE_FIX (0.1),
  SANE_FIX (9.9),
  SANE_FIX (0.1)
};

static const SANE_Range lamp_range = {
  1,
  15,
  1
};

static const SANE_Range threshold_range = {
  0,
  255,
  1
};

/*--------------------------------------------------------------------------*/
static int
min (int a, int b)
{
  if (a < b)
    return a;
  else
    return b;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  SM3840_Scan *s = handle;
  unsigned char c, d;
  int i;

  DBG (2, "+sane-read:%p %p %d %p\n", (unsigned char *) s, buf, max_len,
       (unsigned char *) len);
  DBG (2,
       "+sane-read:remain:%lu offset:%lu linesleft:%d linebuff:%p linesread:%d\n",
       (u_long)s->remaining, (u_long)s->offset, s->linesleft, s->line_buffer, s->linesread);

  if (!s->scanning)
    return SANE_STATUS_INVAL;

  if (!s->remaining)
    {
      if (!s->linesleft)
	{
	  *len = 0;
	  s->scanning = 0;
	  /* Move to home position */
	  reset_scanner ((p_usb_dev_handle)s->udev);
	  /* Send lamp timeout */
	  set_lamp_timer ((p_usb_dev_handle)s->udev, s->sm3840_params.lamp);

	  /* Free memory */
	  if (s->save_scan_line)
	    free (s->save_scan_line);
	  s->save_scan_line = NULL;
	  if (s->save_dpi1200_remap)
	    free (s->save_dpi1200_remap);
	  s->save_dpi1200_remap = NULL;
	  if (s->save_color_remap)
	    free (s->save_color_remap);
	  s->save_color_remap = NULL;

	  return SANE_STATUS_EOF;
	}

      record_line ((s->linesread == 0) ? 1 : 0,
		   (p_usb_dev_handle) s->udev,
		   s->line_buffer,
		   s->sm3840_params.dpi,
		   s->sm3840_params.scanpix,
		   s->sm3840_params.gray,
		   (s->sm3840_params.bpp == 16) ? 1 : 0,
		   &s->save_i,
		   &s->save_scan_line,
		   &s->save_dpi1200_remap, &s->save_color_remap);
      s->remaining = s->sm3840_params.linelen;
      s->offset = 0;
      s->linesread++;
      s->linesleft--;
    }

  /* Need to software emulate 1-bpp modes, simple threshold and error */
  /* diffusion dither implemented. */
  if (s->sm3840_params.lineart || s->sm3840_params.halftone)
    {
      d = 0;
      for (i = 0; i < min (max_len * 8, s->remaining); i++)
	{
	  d = d << 1;
	  if (s->sm3840_params.halftone)
	    {
	      c = (*(unsigned char *) (s->offset + s->line_buffer + i));
	      if (c + s->save_dither_err < 128)
		{
		  d |= 1;
		  s->save_dither_err += c;
		}
	      else
		{
		  s->save_dither_err += c - 255;
		}
	    }
	  else
	    {
	      if ((*(unsigned char *) (s->offset + s->line_buffer + i)) < s->threshold )
		d |= 1;
	    }
	  if (i % 8 == 7)
	    *(buf++) = d;
	}
      *len = i / 8;
      s->offset += i;
      s->remaining -= i;
    }
  else
    {
      memcpy (buf, s->offset + s->line_buffer, min (max_len, s->remaining));
      *len = min (max_len, s->remaining);
      s->offset += min (max_len, s->remaining);
      s->remaining -= min (max_len, s->remaining);
    }

  DBG (2, "-sane_read\n");

  return SANE_STATUS_GOOD;
}

/*--------------------------------------------------------------------------*/
void
sane_cancel (SANE_Handle h)
{
  SM3840_Scan *s = h;

  DBG (2, "trying to cancel...\n");
  if (s->scanning)
    {
      if (!s->cancelled)
	{
	  /* Move to home position */
	  reset_scanner ((p_usb_dev_handle) s->udev);
	  /* Send lamp timeout */
	  set_lamp_timer ((p_usb_dev_handle) s->udev, s->sm3840_params.lamp);

	  /* Free memory */
	  if (s->save_scan_line)
	    free (s->save_scan_line);
	  s->save_scan_line = NULL;
	  if (s->save_dpi1200_remap)
	    free (s->save_dpi1200_remap);
	  s->save_dpi1200_remap = NULL;
	  if (s->save_color_remap)
	    free (s->save_color_remap);
	  s->save_color_remap = NULL;

	  s->scanning = 0;
	  s->cancelled = SANE_TRUE;
	}
    }
}

/*--------------------------------------------------------------------------*/
SANE_Status
sane_start (SANE_Handle handle)
{
  SM3840_Scan *s = handle;
  SANE_Status status;

  /* First make sure we have a current parameter set.  Some of the
   * parameters will be overwritten below, but that's OK.  */
  DBG (2, "sane_start\n");
  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;
  DBG (1, "Got params again...\n");

  s->scanning = SANE_TRUE;
  s->cancelled = 0;

  s->line_buffer = malloc (s->sm3840_params.linelen);
  s->remaining = 0;
  s->offset = 0;
  s->linesleft = s->sm3840_params.scanlines;
  s->linesread = 0;

  s->save_i = 0;
  s->save_scan_line = NULL;
  s->save_dpi1200_remap = NULL;
  s->save_color_remap = NULL;

  s->save_dither_err = 0;
  s->threshold = s->sm3840_params.threshold;

  setup_scan ((p_usb_dev_handle) s->udev, &(s->sm3840_params));

  return (SANE_STATUS_GOOD);
}

static double
sm3840_unit_convert (SANE_Int val)
{
  double d;
  d = SANE_UNFIX (val);
  d /= MM_PER_INCH;
  return d;
}

/*--------------------------------------------------------------------------*/
SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  SM3840_Scan *s = handle;

  DBG (2, "sane_get_parameters\n");
  if (!s->scanning)
    {
      memset (&s->sane_params, 0, sizeof (s->sane_params));
      /* Copy from options to sm3840_params */
      s->sm3840_params.gray =
	(!strcasecmp (s->value[OPT_MODE].s, SANE_VALUE_SCAN_MODE_GRAY)) ? 1 : 0;
      s->sm3840_params.halftone =
	(!strcasecmp (s->value[OPT_MODE].s, SANE_VALUE_SCAN_MODE_HALFTONE)) ? 1 : 0;
      s->sm3840_params.lineart =
	(!strcasecmp (s->value[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART)) ? 1 : 0;

      s->sm3840_params.dpi = s->value[OPT_RESOLUTION].w;
      s->sm3840_params.bpp = s->value[OPT_BIT_DEPTH].w;
      s->sm3840_params.gain = SANE_UNFIX (s->value[OPT_CONTRAST].w);
      s->sm3840_params.offset = s->value[OPT_BRIGHTNESS].w;
      s->sm3840_params.lamp = s->value[OPT_LAMP_TIMEOUT].w;
      s->sm3840_params.threshold = s->value[OPT_THRESHOLD].w;

      if (s->sm3840_params.lineart || s->sm3840_params.halftone)
	{
	  s->sm3840_params.gray = 1;
	  s->sm3840_params.bpp = 8;
	}

      s->sm3840_params.top = sm3840_unit_convert (s->value[OPT_TL_Y].w);
      s->sm3840_params.left = sm3840_unit_convert (s->value[OPT_TL_X].w);
      s->sm3840_params.width =
	sm3840_unit_convert (s->value[OPT_BR_X].w) - s->sm3840_params.left;
      s->sm3840_params.height =
	sm3840_unit_convert (s->value[OPT_BR_Y].w) - s->sm3840_params.top;

      /* Legalize and calculate pixel sizes */
      prepare_params (&(s->sm3840_params));

      /* Copy into sane_params */
      s->sane_params.pixels_per_line = s->sm3840_params.scanpix;
      s->sane_params.lines = s->sm3840_params.scanlines;
      s->sane_params.format =
	s->sm3840_params.gray ? SANE_FRAME_GRAY : SANE_FRAME_RGB;
      s->sane_params.bytes_per_line = s->sm3840_params.linelen;
      s->sane_params.depth = s->sm3840_params.bpp;

      if (s->sm3840_params.lineart || s->sm3840_params.halftone)
	{
	  s->sane_params.bytes_per_line += 7;
	  s->sane_params.bytes_per_line /= 8;
	  s->sane_params.depth = 1;
	  s->sane_params.pixels_per_line = s->sane_params.bytes_per_line * 8;
	}

      s->sane_params.last_frame = SANE_TRUE;
    }				/*!scanning */

  if (params)
    *params = s->sane_params;

  return (SANE_STATUS_GOOD);
}

/*--------------------------------------------------------------------------*/
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  SM3840_Scan *s = handle;
  SANE_Status status = 0;
  SANE_Word cap;
  DBG (2, "sane_control_option\n");
  if (info)
    *info = 0;
  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;
  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;
  cap = s->options_list[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    return SANE_STATUS_INVAL;
  if (action == SANE_ACTION_GET_VALUE)
    {
      DBG (1, "sane_control_option %d, get value\n", option);
      switch (option)
	{
	  /* word options: */
	case OPT_RESOLUTION:
	case OPT_BIT_DEPTH:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	case OPT_CONTRAST:
	case OPT_BRIGHTNESS:
	case OPT_LAMP_TIMEOUT:
	case OPT_THRESHOLD:
	  *(SANE_Word *) val = s->value[option].w;
	  return (SANE_STATUS_GOOD);
	  /* string options: */
	case OPT_MODE:
	  strcpy (val, s->value[option].s);
	  return (SANE_STATUS_GOOD);
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      DBG (1, "sane_control_option %d, set value\n", option);
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return (SANE_STATUS_INVAL);
      if (status != SANE_STATUS_GOOD)
	return (status);
      status = sanei_constrain_value (s->options_list + option, val, info);
      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_RESOLUTION:
	case OPT_BIT_DEPTH:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_TL_X:
	case OPT_TL_Y:
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  /* fall through */
	case OPT_NUM_OPTS:
	case OPT_CONTRAST:
	case OPT_BRIGHTNESS:
	case OPT_LAMP_TIMEOUT:
	case OPT_THRESHOLD:
	  s->value[option].w = *(SANE_Word *) val;
	  return (SANE_STATUS_GOOD);
	case OPT_MODE:
	  if (s->value[option].s)
	    free (s->value[option].s);
	  s->value[option].s = strdup (val);

	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return (SANE_STATUS_GOOD);
	}
    }
  return (SANE_STATUS_INVAL);
}

/*--------------------------------------------------------------------------*/
const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  SM3840_Scan *s = handle;
  DBG (2, "sane_get_option_descriptor\n");
  if ((unsigned) option >= NUM_OPTIONS)
    return (0);
  return (&s->options_list[option]);
}

/*--------------------------------------------------------------------------*/

void
sane_close (SANE_Handle handle)
{
  SM3840_Scan *prev, *s;

  DBG (2, "sane_close\n");
  /* remove handle from list of open handles: */
  prev = 0;
  for (s = first_handle; s; s = s->next)
    {
      if (s == handle)
	break;
      prev = s;
    }
  if (!s)
    {
      DBG (1, "close: invalid handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }

  if (s->scanning)
    {
      sane_cancel (handle);
    }

  sanei_usb_close (s->udev);

  if (s->line_buffer)
    free (s->line_buffer);
  if (s->save_scan_line)
    free (s->save_scan_line);
  if (s->save_dpi1200_remap)
    free (s->save_dpi1200_remap);
  if (s->save_color_remap)
    free (s->save_color_remap);

  if (prev)
    prev->next = s->next;
  else
    first_handle = s;
  free (handle);
}

/*--------------------------------------------------------------------------*/
void
sane_exit (void)
{
  SM3840_Device *next;
  DBG (2, "sane_exit\n");
  while (first_dev != NULL)
    {
      next = first_dev->next;
      free (first_dev);
      first_dev = next;
    }
  if (devlist)
    free (devlist);
}



/*--------------------------------------------------------------------------*/
SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  DBG_INIT ();
  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);
  if (authorize)
    DBG (2, "Unused authorize\n");

  sanei_usb_init ();

  return (SANE_STATUS_GOOD);
}

/*--------------------------------------------------------------------------*/


static SANE_Status
add_sm_device (SANE_String_Const devname, SANE_String_Const modname)
{
  SM3840_Device *dev;

  dev = calloc (sizeof (*dev), 1);
  if (!dev)
    return (SANE_STATUS_NO_MEM);

  memset (dev, 0, sizeof (*dev));
  dev->sane.name = strdup (devname);
  dev->sane.model = modname;
  dev->sane.vendor = "Microtek";
  dev->sane.type = "flatbed scanner";
  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  return (SANE_STATUS_GOOD);
}

static SANE_Status
add_sm3840_device (SANE_String_Const devname)
{
  return add_sm_device (devname, "ScanMaker 3840");
}

static SANE_Status
add_sm4800_device (SANE_String_Const devname)
{
  return add_sm_device (devname, "ScanMaker 4800");
}


/*--------------------------------------------------------------------------*/
SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  SM3840_Device *dev;
  int i;

  DBG (3, "sane_get_devices (local_only = %d)\n", local_only);

  while (first_dev)
    {
      dev = first_dev->next;
      free (first_dev);
      first_dev = dev;
    }
  first_dev = NULL;
  num_devices = 0;

  /* If we get enough scanners should use an array, but for now */
  /* do it one-by-one... */
  sanei_usb_find_devices (0x05da, 0x30d4, add_sm3840_device);
  sanei_usb_find_devices (0x05da, 0x30cf, add_sm4800_device);

  if (devlist)
    free (devlist);
  devlist = calloc ((num_devices + 1) * sizeof (devlist[0]), 1);
  if (!devlist)
    return SANE_STATUS_NO_MEM;
  i = 0;
  for (dev = first_dev; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;
  if (device_list)
    *device_list = devlist;
  return (SANE_STATUS_GOOD);
}


/*--------------------------------------------------------------------------*/

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;
  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }

  return (max_size);
}

/*--------------------------------------------------------------------------*/

static void
initialize_options_list (SM3840_Scan * s)
{

  SANE_Int option;
  DBG (2, "initialize_options_list\n");
  for (option = 0; option < NUM_OPTIONS; ++option)
    {
      s->options_list[option].size = sizeof (SANE_Word);
      s->options_list[option].cap =
	SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  s->options_list[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
  s->options_list[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->options_list[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->options_list[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->options_list[OPT_NUM_OPTS].unit = SANE_UNIT_NONE;
  s->options_list[OPT_NUM_OPTS].size = sizeof (SANE_Word);
  s->options_list[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->options_list[OPT_NUM_OPTS].constraint_type = SANE_CONSTRAINT_NONE;
  s->value[OPT_NUM_OPTS].w = NUM_OPTIONS;

  s->options_list[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->options_list[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->options_list[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->options_list[OPT_MODE].type = SANE_TYPE_STRING;
  s->options_list[OPT_MODE].size = max_string_size (mode_list);
  s->options_list[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->options_list[OPT_MODE].constraint.string_list = mode_list;
  s->value[OPT_MODE].s = strdup (mode_list[1]);

  s->options_list[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->options_list[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->options_list[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->options_list[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->options_list[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->options_list[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->options_list[OPT_RESOLUTION].constraint.word_list = resolution_list;
  s->value[OPT_RESOLUTION].w = 300;

  s->options_list[OPT_BIT_DEPTH].name = SANE_NAME_BIT_DEPTH;
  s->options_list[OPT_BIT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
  s->options_list[OPT_BIT_DEPTH].desc = SANE_DESC_BIT_DEPTH;
  s->options_list[OPT_BIT_DEPTH].type = SANE_TYPE_INT;
  s->options_list[OPT_BIT_DEPTH].unit = SANE_UNIT_NONE;
  s->options_list[OPT_BIT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->options_list[OPT_BIT_DEPTH].constraint.word_list = bpp_list;
  s->value[OPT_BIT_DEPTH].w = 8;

  s->options_list[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->options_list[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->options_list[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->options_list[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->options_list[OPT_TL_X].unit = SANE_UNIT_MM;
  s->options_list[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_TL_X].constraint.range = &x_range;
  s->value[OPT_TL_X].w = s->options_list[OPT_TL_X].constraint.range->min;
  s->options_list[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->options_list[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->options_list[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->options_list[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->options_list[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->options_list[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_TL_Y].constraint.range = &y_range;
  s->value[OPT_TL_Y].w = s->options_list[OPT_TL_Y].constraint.range->min;
  s->options_list[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->options_list[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->options_list[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->options_list[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->options_list[OPT_BR_X].unit = SANE_UNIT_MM;
  s->options_list[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_BR_X].constraint.range = &x_range;
  s->value[OPT_BR_X].w = s->options_list[OPT_BR_X].constraint.range->max;
  s->options_list[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->options_list[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->options_list[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->options_list[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->options_list[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->options_list[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_BR_Y].constraint.range = &y_range;
  s->value[OPT_BR_Y].w = s->options_list[OPT_BR_Y].constraint.range->max;

  s->options_list[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->options_list[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->options_list[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->options_list[OPT_CONTRAST].type = SANE_TYPE_FIXED;
  s->options_list[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  s->options_list[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_CONTRAST].constraint.range = &contrast_range;
  s->value[OPT_CONTRAST].w = SANE_FIX (3.5);

  s->options_list[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->options_list[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->options_list[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  s->options_list[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  s->options_list[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  s->options_list[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_BRIGHTNESS].constraint.range = &brightness_range;
  s->value[OPT_BRIGHTNESS].w = 1800;

  s->options_list[OPT_LAMP_TIMEOUT].name = "lamp-timeout";
  s->options_list[OPT_LAMP_TIMEOUT].title = SANE_I18N ("Lamp timeout");
  s->options_list[OPT_LAMP_TIMEOUT].desc =
    SANE_I18N ("Minutes until lamp is turned off after scan");
  s->options_list[OPT_LAMP_TIMEOUT].type = SANE_TYPE_INT;
  s->options_list[OPT_LAMP_TIMEOUT].unit = SANE_UNIT_NONE;
  s->options_list[OPT_LAMP_TIMEOUT].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_LAMP_TIMEOUT].constraint.range = &lamp_range;
  s->value[OPT_LAMP_TIMEOUT].w = 15;

  s->options_list[OPT_THRESHOLD].name = "threshold";
  s->options_list[OPT_THRESHOLD].title = SANE_I18N ("Threshold");
  s->options_list[OPT_THRESHOLD].desc =
    SANE_I18N ("Threshold value for lineart mode");
  s->options_list[OPT_THRESHOLD].type = SANE_TYPE_INT;
  s->options_list[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->options_list[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_THRESHOLD].constraint.range = &threshold_range;
  s->value[OPT_THRESHOLD].w = 128;

}

/*--------------------------------------------------------------------------*/
SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  SANE_Status status;
  SM3840_Device *dev;
  SM3840_Scan *s;
  DBG (2, "sane_open\n");

  /* Make sure we have first_dev */
  sane_get_devices (NULL, 0);
  if (devicename[0])
    {
      for (dev = first_dev; dev; dev = dev->next)
	if (strcmp (dev->sane.name, devicename) == 0)
	  break;
    }
  else
    {
      /* empty devicename -> use first device */
      dev = first_dev;
    }
  DBG (2, "using device: %s %p\n", dev->sane.name, (unsigned char *) dev);
  if (!dev)
    return SANE_STATUS_INVAL;
  s = calloc (sizeof (*s), 1);
  if (!s)
    return SANE_STATUS_NO_MEM;

  status = sanei_usb_open (dev->sane.name, &(s->udev));
  if (status != SANE_STATUS_GOOD)
    return status;

  initialize_options_list (s);
  s->scanning = 0;
  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;
  *handle = s;
  return (SANE_STATUS_GOOD);
}

/*--------------------------------------------------------------------------*/
SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  SM3840_Scan *s = handle;
  DBG (2, "sane_set_io_mode( %p, %d )\n", handle, non_blocking);
  if (s->scanning)
    {
      if (non_blocking == SANE_FALSE)
	return SANE_STATUS_GOOD;
      else
	return (SANE_STATUS_UNSUPPORTED);
    }
  else
    return SANE_STATUS_INVAL;
}

/*---------------------------------------------------------------------------*/
SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  DBG (2, "sane_get_select_fd( %p, %p )\n", (void *) handle, (void *) fd);
  return SANE_STATUS_UNSUPPORTED;
}
