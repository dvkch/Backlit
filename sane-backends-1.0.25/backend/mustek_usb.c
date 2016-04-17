/* sane - Scanner Access Now Easy.

   Copyright (C) 2000 Mustek.
   Originally maintained by Tom Wang <tom.wang@mustek.com.tw>

   Copyright (C) 2001 - 2004 by Henning Meier-Geinitz.

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

   This file implements a SANE backend for Mustek 1200UB and similar 
   USB flatbed scanners.  */

#define BUILD 18

#include "../include/sane/config.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#define BACKEND_NAME mustek_usb

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_usb.h"

#include "mustek_usb.h"
#include "mustek_usb_high.c"

#ifndef SANE_I18N
#define SANE_I18N(text) text
#endif

static SANE_Int num_devices;
static Mustek_Usb_Device *first_dev;
static Mustek_Usb_Scanner *first_handle;
static const SANE_Device **devlist = 0;

/* Maximum amount of data read in one turn from USB. */
static SANE_Word max_block_size = (8 * 1024);

/* Array of newly attached devices */
static Mustek_Usb_Device **new_dev;

/* Length of new_dev array */
static SANE_Int new_dev_len;

/* Number of entries alloced for new_dev */
static SANE_Int new_dev_alloced;

static SANE_String_Const mode_list[6];

static const SANE_Range char_range = {
  -127, 127, 1
};

static const SANE_Range u8_range = {
  0,				/* minimum */
  255,				/* maximum */
  0				/* quantization */
};


static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  SANE_Int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }
  return max_size;
}


static SANE_Status
calc_parameters (Mustek_Usb_Scanner * s)
{
  SANE_String val;
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int max_x, max_y;

  DBG (5, "calc_parameters: start\n");
  val = s->val[OPT_MODE].s;

  s->params.last_frame = SANE_TRUE;

  if (!strcmp (val, SANE_VALUE_SCAN_MODE_LINEART))
    {
      s->params.format = SANE_FRAME_GRAY;
      s->params.depth = 1;
      s->bpp = 1;
      s->channels = 1;
    }
  else if (!strcmp (val, SANE_VALUE_SCAN_MODE_GRAY))
    {
      s->params.format = SANE_FRAME_GRAY;
      s->params.depth = 8;
      s->bpp = 8;
      s->channels = 1;
    }
  else if (!strcmp (val, SANE_VALUE_SCAN_MODE_COLOR))
    {
      s->params.format = SANE_FRAME_RGB;
      s->params.depth = 8;
      s->bpp = 24;
      s->channels = 3;
    }
  else
    {
      DBG (1, "calc_parameters: invalid mode %s\n", (SANE_Char *) val);
      status = SANE_STATUS_INVAL;
    }

  s->tl_x = SANE_UNFIX (s->val[OPT_TL_X].w) / MM_PER_INCH;
  s->tl_y = SANE_UNFIX (s->val[OPT_TL_Y].w) / MM_PER_INCH;
  s->width = SANE_UNFIX (s->val[OPT_BR_X].w) / MM_PER_INCH - s->tl_x;
  s->height = SANE_UNFIX (s->val[OPT_BR_Y].w) / MM_PER_INCH - s->tl_y;

  if (s->width < 0)
    {
      DBG (1, "calc_parameters: warning: tl_x > br_x\n");
    }
  if (s->height < 0)
    {
      DBG (1, "calc_parameters: warning: tl_y > br_y\n");
    }
  max_x = s->hw->max_width * SANE_UNFIX (s->val[OPT_RESOLUTION].w) / 300;
  max_y = s->hw->max_height * SANE_UNFIX (s->val[OPT_RESOLUTION].w) / 300;

  s->tl_x_dots = s->tl_x * SANE_UNFIX (s->val[OPT_RESOLUTION].w);
  s->width_dots = s->width * SANE_UNFIX (s->val[OPT_RESOLUTION].w);
  s->tl_y_dots = s->tl_y * SANE_UNFIX (s->val[OPT_RESOLUTION].w);
  s->height_dots = s->height * SANE_UNFIX (s->val[OPT_RESOLUTION].w);

  if (s->width_dots > max_x)
    s->width_dots = max_x;
  if (s->height_dots > max_y)
    s->height_dots = max_y;
  if (!strcmp (val, SANE_VALUE_SCAN_MODE_LINEART))
    {
      s->width_dots = (s->width_dots / 8) * 8;
      if (s->width_dots == 0)
	s->width_dots = 8;
    }
  if (s->tl_x_dots < 0)
    s->tl_x_dots = 0;
  if (s->tl_y_dots < 0)
    s->tl_y_dots = 0;
  if (s->tl_x_dots + s->width_dots > max_x)
    s->tl_x_dots = max_x - s->width_dots;
  if (s->tl_y_dots + s->height_dots > max_y)
    s->tl_y_dots = max_y - s->height_dots;

  s->val[OPT_TL_X].w = SANE_FIX (s->tl_x * MM_PER_INCH);
  s->val[OPT_TL_Y].w = SANE_FIX (s->tl_y * MM_PER_INCH);
  s->val[OPT_BR_X].w = SANE_FIX ((s->tl_x + s->width) * MM_PER_INCH);
  s->val[OPT_BR_Y].w = SANE_FIX ((s->tl_y + s->height) * MM_PER_INCH);

  s->params.pixels_per_line = s->width_dots;
  if (s->params.pixels_per_line < 0)
    s->params.pixels_per_line = 0;
  s->params.lines = s->height_dots;
  if (s->params.lines < 0)
    s->params.lines = 0;
  s->params.bytes_per_line = s->params.pixels_per_line * s->params.depth / 8
    * s->channels;

  DBG (4, "calc_parameters: format=%d\n", s->params.format);
  DBG (4, "calc_parameters: last frame=%d\n", s->params.last_frame);
  DBG (4, "calc_parameters: lines=%d\n", s->params.lines);
  DBG (4, "calc_parameters: pixels per line=%d\n", s->params.pixels_per_line);
  DBG (4, "calc_parameters: bytes per line=%d\n", s->params.bytes_per_line);
  DBG (4, "calc_parameters: Pixels %dx%dx%d\n",
       s->params.pixels_per_line, s->params.lines, 1 << s->params.depth);

  DBG (5, "calc_parameters: exit\n");
  return status;
}


static SANE_Status
init_options (Mustek_Usb_Scanner * s)
{
  SANE_Int option;
  SANE_Status status;

  DBG (5, "init_options: start\n");

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (option = 0; option < NUM_OPTIONS; ++option)
    {
      s->opt[option].size = sizeof (SANE_Word);
      s->opt[option].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
  s->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Mode" group: */
  s->opt[OPT_MODE_GROUP].title = SANE_I18N ("Scan Mode");
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].size = 0;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  mode_list[0] = SANE_VALUE_SCAN_MODE_COLOR;
  mode_list[1] = SANE_VALUE_SCAN_MODE_GRAY;
  mode_list[2] = SANE_VALUE_SCAN_MODE_LINEART;
  mode_list[3] = NULL;

  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE].s = strdup (mode_list[1]);

  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_FIXED;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_RESOLUTION].constraint.range = &s->hw->dpi_range;
  s->val[OPT_RESOLUTION].w = s->hw->dpi_range.min;
  if (s->hw->chip->scanner_type == MT_600CU)
    s->hw->dpi_range.max = SANE_FIX (600);
  else
    s->hw->dpi_range.max = SANE_FIX (1200);

  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->val[OPT_PREVIEW].w = SANE_FALSE;

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].title = SANE_I18N ("Geometry");
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].size = 0;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &s->hw->x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;
  s->val[OPT_BR_X].w = s->hw->x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_BR_Y].w = s->hw->y_range.max;

  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = SANE_I18N ("Enhancement");
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].size = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &u8_range;
  s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_THRESHOLD].w = 128;

  /* custom-gamma table */
  s->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  s->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;

  /* gray gamma vector */
  s->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR].wa = &s->gray_gamma_table[0];

  /* red gamma vector */
  s->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_R].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_R].wa = &s->red_gamma_table[0];

  /* green gamma vector */
  s->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_G].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_G].wa = &s->green_gamma_table[0];

  /* blue gamma vector */
  s->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_B].wa = &s->blue_gamma_table[0];

  RIE (calc_parameters (s));

  DBG (5, "init_options: exit\n");
  return SANE_STATUS_GOOD;
}


static SANE_Status
attach (SANE_String_Const devname, Mustek_Usb_Device ** devp,
	SANE_Bool may_wait)
{
  Mustek_Usb_Device *dev;
  SANE_Status status;
  Mustek_Type scanner_type;
  SANE_Int fd;

  DBG (5, "attach: start: devp %s NULL, may_wait = %d\n", devp ? "!=" : "==",
       may_wait);
  if (!devname)
    {
      DBG (1, "attach: devname == NULL\n");
      return SANE_STATUS_INVAL;
    }

  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0)
      {
	if (devp)
	  *devp = dev;
	DBG (4, "attach: device `%s' was already in device list\n", devname);
	return SANE_STATUS_GOOD;
      }

  DBG (4, "attach: trying to open device `%s'\n", devname);
  status = sanei_usb_open (devname, &fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (3, "attach: couldn't open device `%s': %s\n", devname,
	   sane_strstatus (status));
      return status;
    }
  DBG (4, "attach: device `%s' successfully opened\n", devname);

  /* try to identify model */
  DBG (4, "attach: trying to identify device `%s'\n", devname);
  status = usb_low_identify_scanner (fd, &scanner_type);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: device `%s' doesn't look like a supported scanner\n",
	   devname);
      sanei_usb_close (fd);
      return status;
    }
  sanei_usb_close (fd);
  if (scanner_type == MT_UNKNOWN)
    {
      DBG (3, "attach: warning: couldn't identify device `%s', must set "
	   "type manually\n", devname);
    }

  dev = malloc (sizeof (Mustek_Usb_Device));
  if (!dev)
    {
      DBG (1, "attach: couldn't malloc Mustek_Usb_Device\n");
      return SANE_STATUS_NO_MEM;
    }

  memset (dev, 0, sizeof (*dev));
  dev->name = strdup (devname);
  dev->sane.name = (SANE_String_Const) dev->name;
  dev->sane.vendor = "Mustek";
  switch (scanner_type)
    {
    case MT_1200CU:
      dev->sane.model = "1200 CU";
      break;
    case MT_1200CU_PLUS:
      dev->sane.model = "1200 CU Plus";
      break;
    case MT_1200USB:
      dev->sane.model = "1200 USB (unsupported)";
      break;
    case MT_1200UB:
      dev->sane.model = "1200 UB";
      break;
    case MT_600CU:
      dev->sane.model = "600 CU";
      break;
    case MT_600USB:
      dev->sane.model = "600 USB (unsupported)";
      break;
    default:
      dev->sane.model = "(unidentified)";
      break;
    }
  dev->sane.type = "flatbed scanner";

  dev->x_range.min = 0;
  dev->x_range.max = SANE_FIX (8.4 * MM_PER_INCH);
  dev->x_range.quant = 0;

  dev->y_range.min = 0;
  dev->y_range.max = SANE_FIX (11.7 * MM_PER_INCH);
  dev->y_range.quant = 0;

  dev->max_height = 11.7 * 300;
  dev->max_width = 8.4 * 300;
  dev->dpi_range.min = SANE_FIX (50);
  dev->dpi_range.max = SANE_FIX (600);
  dev->dpi_range.quant = SANE_FIX (1);

  status = usb_high_scan_init (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: usb_high_scan_init returned status: %s\n",
	   sane_strstatus (status));
      free (dev);
      return status;
    }
  dev->chip->scanner_type = scanner_type;
  dev->chip->max_block_size = max_block_size;

  DBG (2, "attach: found %s %s %s at %s\n", dev->sane.vendor, dev->sane.type,
       dev->sane.model, dev->sane.name);
  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;

  DBG (5, "attach: exit\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one_device (SANE_String_Const devname)
{
  Mustek_Usb_Device *dev;
  SANE_Status status;

  RIE (attach (devname, &dev, SANE_FALSE));

  if (dev)
    {
      /* Keep track of newly attached devices so we can set options as
         necessary.  */
      if (new_dev_len >= new_dev_alloced)
	{
	  new_dev_alloced += 4;
	  if (new_dev)
	    new_dev =
	      realloc (new_dev, new_dev_alloced * sizeof (new_dev[0]));
	  else
	    new_dev = malloc (new_dev_alloced * sizeof (new_dev[0]));
	  if (!new_dev)
	    {
	      DBG (1, "attach_one_device: out of memory\n");
	      return SANE_STATUS_NO_MEM;
	    }
	}
      new_dev[new_dev_len++] = dev;
    }
  return SANE_STATUS_GOOD;
}

static SANE_Status
fit_lines (Mustek_Usb_Scanner * s, SANE_Byte * src, SANE_Byte * dst,
	   SANE_Word src_lines, SANE_Word * dst_lines)
{
  SANE_Int threshold;
  SANE_Word src_width, dst_width;
  SANE_Word dst_pixel, src_pixel;
  SANE_Word dst_line, src_line;
  SANE_Word pixel_switch;
  SANE_Word src_address, dst_address;
  src_width = s->hw->width;
  dst_width = s->width_dots;

  threshold = s->val[OPT_THRESHOLD].w;

  DBG (5, "fit_lines: dst_width=%d, src_width=%d, src_lines=%d, "
       "offset=%d\n", dst_width, src_width, src_lines, s->hw->line_offset);

  dst_line = 0;
  src_line = s->hw->line_offset;

  while (src_line < src_lines)
    {
      DBG (5, "fit_lines: getting line: dst_line=%d, src_line=%d, "
	   "line_switch=%d\n", dst_line, src_line, s->hw->line_switch);

      src_pixel = 0;
      pixel_switch = src_width;
      for (dst_pixel = 0; dst_pixel < dst_width; dst_pixel++)
	{
	  while (pixel_switch > dst_width)
	    {
	      src_pixel++;
	      pixel_switch -= dst_width;
	    }
	  pixel_switch += src_width;

	  src_address = src_pixel * s->hw->bpp / 8
	    + src_width * src_line * s->hw->bpp / 8;
	  dst_address = dst_pixel * s->bpp / 8
	    + dst_width * dst_line * s->bpp / 8;

	  if (s->bpp == 8)
	    {
	      dst[dst_address] = s->gray_table[src[src_address]];
	    }
	  else if (s->bpp == 24)
	    {
	      dst[dst_address]
		= s->red_table[s->gray_table[src[src_address]]];
	      dst[dst_address + 1]
		= s->green_table[s->gray_table[src[src_address + 1]]];
	      dst[dst_address + 2]
		= s->blue_table[s->gray_table[src[src_address + 2]]];
	    }
	  else			/* lineart */
	    {
	      if ((dst_pixel % 8) == 0)
		dst[dst_address] = 0;
	      dst[dst_address] |=
		(((src[src_address] > threshold) ? 0 : 1)
		 << (7 - (dst_pixel % 8)));
	    }
	}

      dst_line++;
      while (s->hw->line_switch >= s->height_dots)
	{
	  src_line++;
	  s->hw->line_switch -= s->height_dots;
	}
      s->hw->line_switch += s->hw->height;
    }

  *dst_lines = dst_line;
  s->hw->line_offset = (src_line - src_lines);

  DBG (4, "fit_lines: exit, src_line=%d, *dst_lines=%d, offset=%d\n",
       src_line, *dst_lines, s->hw->line_offset);
  return SANE_STATUS_GOOD;
}

static SANE_Status
check_gamma_table (SANE_Word * table)
{
  SANE_Word entry, value;
  SANE_Status status = SANE_STATUS_GOOD;

  for (entry = 0; entry < 256; entry++)
    {
      value = table[entry];
      if (value > 255)
	{
	  DBG (1, "check_gamma_table: warning: entry %d > 255 (%d) - fixed\n",
	       entry, value);
	  table[entry] = 255;
	  status = SANE_STATUS_INVAL;
	}
    }

  return status;
}

/* -------------------------- SANE API functions ------------------------- */

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  SANE_Char line[PATH_MAX];
  SANE_Char *word, *end;
  SANE_String_Const cp;
  SANE_Int linenumber;
  FILE *fp;

  DBG_INIT ();
  DBG (2, "SANE Mustek USB backend version %d.%d build %d from %s\n", SANE_CURRENT_MAJOR,
       V_MINOR, BUILD, PACKAGE_STRING);

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  DBG (5, "sane_init: authorize %s null\n", authorize ? "!=" : "==");


  num_devices = 0;
  first_dev = 0;
  first_handle = 0;
  devlist = 0;
  new_dev = 0;
  new_dev_len = 0;
  new_dev_alloced = 0;

  sanei_usb_init ();

  fp = sanei_config_open (MUSTEK_USB_CONFIG_FILE);
  if (!fp)
    {
      /* default to /dev/usb/scanner instead of insisting on config file */
      DBG (3, "sane_init: couldn't open config file `%s': %s. Using "
	   "/dev/usb/scanner directly\n", MUSTEK_USB_CONFIG_FILE,
	   strerror (errno));
      attach ("/dev/usb/scanner", 0, SANE_FALSE);
      return SANE_STATUS_GOOD;
    }
  linenumber = 0;
  DBG (4, "sane_init: reading config file `%s'\n", MUSTEK_USB_CONFIG_FILE);

  while (sanei_config_read (line, sizeof (line), fp))
    {
      word = 0;
      linenumber++;

      cp = sanei_config_get_string (line, &word);
      if (!word || cp == line)
	{
	  DBG (5, "sane_init: config file line %d: ignoring empty line\n",
	       linenumber);
	  if (word)
	    free (word);
	  continue;
	}
      if (word[0] == '#')
	{
	  DBG (5, "sane_init: config file line %d: ignoring comment line\n",
	       linenumber);
	  free (word);
	  continue;
	}

      if (strcmp (word, "option") == 0)
	{
	  free (word);
	  word = 0;
	  cp = sanei_config_get_string (cp, &word);

	  if (!word)
	    {
	      DBG (1, "sane_init: config file line %d: missing quotation mark?\n",
		   linenumber);
	      continue;
	    }

	  if (strcmp (word, "max_block_size") == 0)
	    {
	      free (word);
	      word = 0;
	      cp = sanei_config_get_string (cp, &word);
	      if (!word)
		{
		  DBG (1, "sane_init: config file line %d: missing quotation mark?\n",
		       linenumber);
		  continue;
		}

	      errno = 0;
	      max_block_size = strtol (word, &end, 0);
	      if (end == word)
		{
		  DBG (3, "sane-init: config file line %d: max_block_size "
		       "must have a parameter; using 8192 bytes\n",
		       linenumber);
		  max_block_size = 8192;
		}
	      if (errno)
		{
		  DBG (3,
		       "sane-init: config file line %d: max_block_size `%s' "
		       "is invalid (%s); using 8192 bytes\n", linenumber,
		       word, strerror (errno));
		  max_block_size = 8192;
		}
	      else
		{
		  DBG (3,
		       "sane_init: config file line %d: max_block_size set "
		       "to %d bytes\n", linenumber, max_block_size);
		}
	      if (word)
		free (word);
	      word = 0;
	    }
	  else if (strcmp (word, "1200ub") == 0)
	    {
	      if (new_dev_len > 0)
		{
		  /* this is a 1200 UB */
		  new_dev[new_dev_len - 1]->chip->scanner_type = MT_1200UB;
		  new_dev[new_dev_len - 1]->sane.model = "1200 UB";
		  DBG (3, "sane_init: config file line %d: `%s' is a Mustek "
		       "1200 UB\n", linenumber,
		       new_dev[new_dev_len - 1]->sane.name);
		}
	      else
		{
		  DBG (3, "sane_init: config file line %d: option "
		       "1200ub ignored, was set before any device "
		       "name\n", linenumber);
		}
	      if (word)
		free (word);
	      word = 0;
	    }
	  else if (strcmp (word, "1200cu") == 0)
	    {
	      if (new_dev_len > 0)
		{
		  /* this is a 1200 CU */
		  new_dev[new_dev_len - 1]->chip->scanner_type = MT_1200CU;
		  new_dev[new_dev_len - 1]->sane.model = "1200 CU";
		  DBG (3, "sane_init: config file line %d: `%s' is a Mustek "
		       "1200 CU\n", linenumber,
		       new_dev[new_dev_len - 1]->sane.name);
		}
	      else
		{
		  DBG (3, "sane_init: config file line %d: option "
		       "1200cu ignored, was set before any device "
		       "name\n", linenumber);
		}
	      if (word)
		free (word);
	      word = 0;
	    }
	  else if (strcmp (word, "1200cu_plus") == 0)
	    {
	      if (new_dev_len > 0)
		{
		  /* this is a 1200 CU Plus */
		  new_dev[new_dev_len - 1]->chip->scanner_type
		    = MT_1200CU_PLUS;
		  new_dev[new_dev_len - 1]->sane.model = "1200 CU Plus";
		  DBG (3, "sane_init: config file line %d: `%s' is a Mustek "
		       "1200 CU Plus\n", linenumber,
		       new_dev[new_dev_len - 1]->sane.name);
		}
	      else
		{
		  DBG (3, "sane_init: config file line %d: option "
		       "1200cu_plus ignored, was set before any device "
		       "name\n", linenumber);
		}
	      if (word)
		free (word);
	      word = 0;
	    }
	  else if (strcmp (word, "600cu") == 0)
	    {
	      if (new_dev_len > 0)
		{
		  /* this is a 600 CU */
		  new_dev[new_dev_len - 1]->chip->scanner_type = MT_600CU;
		  new_dev[new_dev_len - 1]->sane.model = "600 CU";
		  DBG (3, "sane_init: config file line %d: `%s' is a Mustek "
		       "600 CU\n", linenumber,
		       new_dev[new_dev_len - 1]->sane.name);
		}
	      else
		{
		  DBG (3, "sane_init: config file line %d: option "
		       "600cu ignored, was set before any device "
		       "name\n", linenumber);
		}
	      if (word)
		free (word);
	      word = 0;
	    }
	  else
	    {
	      DBG (3, "sane_init: config file line %d: option "
		   "%s is unknown\n", linenumber, word);
	      if (word)
		free (word);
	      word = 0;
	    }
	}
      else
	{
	  new_dev_len = 0;
	  DBG (4, "sane_init: config file line %d: trying to attach `%s'\n",
	       linenumber, line);
	  sanei_usb_attach_matching_devices (line, attach_one_device);
	  if (word)
	    free (word);
	  word = 0;
	}
    }

  if (new_dev_alloced > 0)
    {
      new_dev_len = new_dev_alloced = 0;
      free (new_dev);
    }

  fclose (fp);
  DBG (5, "sane_init: exit\n");

  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Mustek_Usb_Device *dev, *next;
  SANE_Status status;

  DBG (5, "sane_exit: start\n");
  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      if (dev->is_prepared)
	{
	  status = usb_high_scan_clearup (dev);
	  if (status != SANE_STATUS_GOOD)
	    DBG (3, "sane_close: usb_high_scan_clearup returned %s\n",
		 sane_strstatus (status));
	}
      status = usb_high_scan_exit (dev);
      if (status != SANE_STATUS_GOOD)
	DBG (3, "sane_close: usb_high_scan_exit returned %s\n",
	     sane_strstatus (status));
      if (dev->chip)
	{
	  status = usb_high_scan_exit (dev);
	  if (status != SANE_STATUS_GOOD)
	    DBG (3,
		 "sane_exit: while closing %s, usb_high_scan_exit returned: "
		 "%s\n", dev->name, sane_strstatus (status));
	}
      free ((void *) dev->name);
      free (dev);
    }
  first_dev = 0;
  if (devlist)
    free (devlist);
  devlist = 0;

  DBG (5, "sane_exit: exit\n");
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  Mustek_Usb_Device *dev;
  SANE_Int dev_num;

  DBG (5, "sane_get_devices: start: local_only = %s\n",
       local_only == SANE_TRUE ? "true" : "false");

  if (devlist)
    free (devlist);

  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  dev_num = 0;
  for (dev = first_dev; dev_num < num_devices; dev = dev->next)
    devlist[dev_num++] = &dev->sane;
  devlist[dev_num++] = 0;

  *device_list = devlist;

  DBG (5, "sane_get_devices: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Mustek_Usb_Device *dev;
  SANE_Status status;
  Mustek_Usb_Scanner *s;
  SANE_Int value;

  DBG (5, "sane_open: start (devicename = `%s')\n", devicename);

  if (devicename[0])
    {
      for (dev = first_dev; dev; dev = dev->next)
	if (strcmp (dev->sane.name, devicename) == 0)
	  break;

      if (!dev)
	{
	  DBG (5,
	       "sane_open: couldn't find `%s' in devlist, trying attach)\n",
	       devicename);
	  RIE (attach (devicename, &dev, SANE_TRUE));
	}
      else
	DBG (5, "sane_open: found `%s' in devlist\n", dev->name);
    }
  else
    {
      /* empty devicname -> use first device */
      dev = first_dev;
      if (dev)
	DBG (5, "sane_open: empty devicename, trying `%s'\n", dev->name);
    }

  if (!dev)
    return SANE_STATUS_INVAL;

  if (dev->chip->scanner_type == MT_UNKNOWN)
    {
      DBG (0, "sane_open: the type of your scanner is unknown, edit "
	   "mustek_usb.conf before using the scanner\n");
      return SANE_STATUS_INVAL;
    }
  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->hw = dev;

  RIE (init_options (s));

  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;

  *handle = s;
  strcpy (s->hw->device_name, dev->name);

  RIE (usb_high_scan_turn_power (s->hw, SANE_TRUE));
  RIE (usb_high_scan_back_home (s->hw));

  s->hw->scan_buffer = (SANE_Byte *) malloc (SCAN_BUFFER_SIZE * 2);
  if (!s->hw->scan_buffer)
    {
      DBG (5, "sane_open: couldn't malloc s->hw->scan_buffer (%d bytes)\n",
	   SCAN_BUFFER_SIZE * 2);
      return SANE_STATUS_NO_MEM;
    }
  s->hw->scan_buffer_len = 0;
  s->hw->scan_buffer_start = s->hw->scan_buffer;

  s->hw->temp_buffer = (SANE_Byte *) malloc (SCAN_BUFFER_SIZE);
  if (!s->hw->temp_buffer)
    {
      DBG (5, "sane_open: couldn't malloc s->hw->temp_buffer (%d bytes)\n",
	   SCAN_BUFFER_SIZE);
      return SANE_STATUS_NO_MEM;
    }
  s->hw->temp_buffer_len = 0;
  s->hw->temp_buffer_start = s->hw->temp_buffer;

  for (value = 0; value < 256; value++)
    {
      s->linear_gamma_table[value] = value;
      s->red_gamma_table[value] = value;
      s->green_gamma_table[value] = value;
      s->blue_gamma_table[value] = value;
      s->gray_gamma_table[value] = value;
    }

  s->red_table = s->linear_gamma_table;
  s->green_table = s->linear_gamma_table;
  s->blue_table = s->linear_gamma_table;
  s->gray_table = s->linear_gamma_table;

  DBG (5, "sane_open: exit\n");

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Mustek_Usb_Scanner *prev, *s;
  SANE_Status status;

  DBG (5, "sane_close: start\n");

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
      DBG (5, "close: invalid handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }

  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  if (s->hw->is_open)
    {
      status = usb_high_scan_turn_power (s->hw, SANE_FALSE);
      if (status != SANE_STATUS_GOOD)
	DBG (3, "sane_close: usb_high_scan_turn_power returned %s\n",
	     sane_strstatus (status));
    }
#if 0
  if (s->hw->is_prepared)
    {
      status = usb_high_scan_clearup (s->hw);
      if (status != SANE_STATUS_GOOD)
	DBG (3, "sane_close: usb_high_scan_clearup returned %s\n",
	     sane_strstatus (status));
    }
  status = usb_high_scan_exit (s->hw);
  if (status != SANE_STATUS_GOOD)
    DBG (3, "sane_close: usb_high_scan_exit returned %s\n",
	 sane_strstatus (status));
#endif
  if (s->hw->scan_buffer)
    {
      free (s->hw->scan_buffer);
      s->hw->scan_buffer = 0;
    }
  if (s->hw->temp_buffer)
    {
      free (s->hw->temp_buffer);
      s->hw->temp_buffer = 0;
    }

  free (handle);

  DBG (5, "sane_close: exit\n");
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Mustek_Usb_Scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  DBG (5, "sane_get_option_descriptor: option = %s (%d)\n",
       s->opt[option].name, option);
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  Mustek_Usb_Scanner *s = handle;
  SANE_Status status;
  SANE_Word cap;
  SANE_Int myinfo = 0;

  DBG (5, "sane_control_option: start: action = %s, option = %s (%d)\n",
       (action == SANE_ACTION_GET_VALUE) ? "get" :
       (action == SANE_ACTION_SET_VALUE) ? "set" :
       (action == SANE_ACTION_SET_AUTO) ? "set_auto" : "unknown",
       s->opt[option].name, option);

  if (info)
    *info = 0;

  if (s->scanning)
    {
      DBG (1, "sane_control_option: don't call this function while "
	   "scanning\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  if (option >= NUM_OPTIONS || option < 0)
    {
      DBG (1, "sane_control_option: option %d >= NUM_OPTIONS || option < 0\n",
	   option);
      return SANE_STATUS_INVAL;
    }

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      DBG (2, "sane_control_option: option %d is inactive\n", option);
      return SANE_STATUS_INVAL;
    }

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_PREVIEW:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_THRESHOLD:
	case OPT_CUSTOM_GAMMA:
	  *(SANE_Word *) val = s->val[option].w;
	  break;
	  /* word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (val, s->val[option].wa, s->opt[option].size);
	  break;
	  /* string options: */
	case OPT_MODE:
	  strcpy (val, s->val[option].s);
	  break;
	default:
	  DBG (2, "sane_control_option: can't get unknown option %d\n",
	       option);
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (2, "sane_control_option: option %d is not settable\n", option);
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (s->opt + option, val, &myinfo);

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (2, "sane_control_option: sanei_constrain_value returned %s\n",
	       sane_strstatus (status));
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
	  s->val[option].w = *(SANE_Word *) val;
	  RIE (calc_parameters (s));
	  myinfo |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_THRESHOLD:
	  s->val[option].w = *(SANE_Word *) val;
	  break;
	  /* Boolean */
	case OPT_PREVIEW:
	  s->val[option].w = *(SANE_Bool *) val;
	  break;
	  /* side-effect-free word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (s->val[option].wa, val, s->opt[option].size);
	  check_gamma_table (s->val[option].wa);
	  break;
	case OPT_CUSTOM_GAMMA:
	  s->val[OPT_CUSTOM_GAMMA].w = *(SANE_Word *) val;
	  myinfo |= SANE_INFO_RELOAD_OPTIONS;
	  if (s->val[OPT_CUSTOM_GAMMA].w == SANE_TRUE)
	    {
	      s->red_table = s->red_gamma_table;
	      s->green_table = s->green_gamma_table;
	      s->blue_table = s->blue_gamma_table;
	      s->gray_table = s->gray_gamma_table;
	      if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_GRAY) == 0)
		s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
	      else if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR) == 0)
		{
		  s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		}
	    }
	  else
	    {
	      s->red_table = s->linear_gamma_table;
	      s->green_table = s->linear_gamma_table;
	      s->blue_table = s->linear_gamma_table;
	      s->gray_table = s->linear_gamma_table;
	      s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	    }
	  break;
	case OPT_MODE:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);

	  RIE (calc_parameters (s));

	  s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
	  s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
	  s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	  s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	  s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	  s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;

	  if (strcmp (val, SANE_VALUE_SCAN_MODE_LINEART) == 0)
	    {
	      s->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;
	      if (s->val[OPT_CUSTOM_GAMMA].w == SANE_TRUE)
		{
		  s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		}
	    }
	  myinfo |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  break;
	default:
	  DBG (2, "sane_control_option: can't set unknown option %d\n",
	       option);
	}
    }
  else
    {
      DBG (2, "sane_control_option: unknown action %d for option %d\n",
	   action, option);
      return SANE_STATUS_INVAL;
    }
  if (info)
    *info = myinfo;

  DBG (5, "sane_control_option: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Mustek_Usb_Scanner *s = handle;
  SANE_Status status;

  DBG (5, "sane_get_parameters: start\n");

  RIE (calc_parameters (s));
  if (params)
    *params = s->params;

  DBG (5, "sane_get_parameters: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Mustek_Usb_Scanner *s = handle;
  SANE_Status status;
  SANE_String val;
  Colormode color_mode;
  SANE_Word dpi, x, y, width, height;

  DBG (5, "sane_start: start\n");

  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */

  s->total_bytes = 0;
  s->total_lines = 0;
  RIE (calc_parameters (s));

  if (s->width_dots <= 0)
    {
      DBG (0, "sane_start: top left x > bottom right x --- exiting\n");
      return SANE_STATUS_INVAL;
    }
  if (s->height_dots <= 0)
    {
      DBG (0, "sane_start: top left y > bottom right y --- exiting\n");
      return SANE_STATUS_INVAL;
    }


  val = s->val[OPT_MODE].s;
  if (!strcmp (val, SANE_VALUE_SCAN_MODE_LINEART))
    color_mode = GRAY8;
  else if (!strcmp (val, SANE_VALUE_SCAN_MODE_GRAY))
    color_mode = GRAY8;
  else				/* Color */
    color_mode = RGB24;

  dpi = SANE_UNFIX (s->val[OPT_RESOLUTION].w);
  x = s->tl_x_dots;
  y = s->tl_y_dots;
  width = s->width_dots;
  height = s->height_dots;

  if (!s->hw->is_prepared)
    {
      RIE (usb_high_scan_prepare (s->hw));
      RIE (usb_high_scan_reset (s->hw));
    }
  RIE (usb_high_scan_set_threshold (s->hw, 128));
  RIE (usb_high_scan_embed_gamma (s->hw, NULL));
  RIE (usb_high_scan_suggest_parameters (s->hw, dpi, x, y, width, height,
					 color_mode));
  RIE (usb_high_scan_setup_scan (s->hw, s->hw->scan_mode, s->hw->x_dpi,
				 s->hw->y_dpi, 0, s->hw->x, s->hw->y,
				 s->hw->width));

  DBG (3, "sane_start: wanted: dpi=%d, x=%d, y=%d, width=%d, height=%d, "
       "scan_mode=%d\n", dpi, x, y, width, height, color_mode);
  DBG (3, "sane_start: got: x_dpi=%d, y_dpi=%d, x=%d, y=%d, width=%d, "
       "height=%d, scan_mode=%d\n", s->hw->x_dpi, s->hw->y_dpi, s->hw->x,
       s->hw->y, s->hw->width, s->hw->height, s->hw->scan_mode);

  s->scanning = SANE_TRUE;
  s->read_rows = s->hw->height;
  s->hw->line_switch = s->hw->height;
  s->hw->line_offset = 0;
  s->hw->scan_buffer_len = 0;

  DBG (5, "sane_start: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  Mustek_Usb_Scanner *s = handle;
  SANE_Word lines_to_read, lines_read;
  SANE_Status status;

  DBG (5, "sane_read: start\n");

  if (!s)
    {
      DBG (1, "sane_read: handle is null!\n");
      return SANE_STATUS_INVAL;
    }

  if (!buf)
    {
      DBG (1, "sane_read: buf is null!\n");
      return SANE_STATUS_INVAL;
    }

  if (!len)
    {
      DBG (1, "sane_read: len is null!\n");
      return SANE_STATUS_INVAL;
    }

  *len = 0;

  if (!s->scanning)
    {
      DBG (3, "sane_read: scan was cancelled, is over or has not been "
	   "initiated yet\n");
      return SANE_STATUS_CANCELLED;
    }

  if (s->hw->scan_buffer_len == 0)
    {
      if (s->read_rows > 0)
	{
	  lines_to_read = SCAN_BUFFER_SIZE / (s->hw->width * s->hw->bpp / 8);
	  if (lines_to_read > s->read_rows)
	    lines_to_read = s->read_rows;
	  s->hw->temp_buffer_start = s->hw->temp_buffer;
	  s->hw->temp_buffer_len = (s->hw->width * s->hw->bpp / 8)
	    * lines_to_read;
	  DBG (4, "sane_read: reading %d source lines\n", lines_to_read);
	  RIE (usb_high_scan_get_rows (s->hw, s->hw->temp_buffer,
				       lines_to_read, SANE_FALSE));
	  RIE (fit_lines (s, s->hw->temp_buffer, s->hw->scan_buffer,
			  lines_to_read, &lines_read));
	  s->read_rows -= lines_to_read;
	  if ((s->total_lines + lines_read) > s->height_dots)
	    lines_read = s->height_dots - s->total_lines;
	  s->total_lines += lines_read;
	  DBG (4, "sane_read: %d destination lines, %d total\n",
	       lines_read, s->total_lines);
	  s->hw->scan_buffer_start = s->hw->scan_buffer;
	  s->hw->scan_buffer_len = (s->width_dots * s->bpp / 8) * lines_read;
	}
      else
	{
	  DBG (4, "sane_read: scan finished -- exit\n");
	  return SANE_STATUS_EOF;
	}
    }
  if (s->hw->scan_buffer_len == 0)
    {
      DBG (4, "sane_read: scan finished -- exit\n");
      return SANE_STATUS_EOF;
    }

  *len = MIN (max_len, (SANE_Int) s->hw->scan_buffer_len);
  memcpy (buf, s->hw->scan_buffer_start, *len);
  DBG (4, "sane_read: exit, read %d bytes from scan_buffer; "
       "%ld bytes remaining\n", *len, 
       (long int) (s->hw->scan_buffer_len - *len));
  s->hw->scan_buffer_len -= (*len);
  s->hw->scan_buffer_start += (*len);
  s->total_bytes += (*len);
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Mustek_Usb_Scanner *s = handle;
  SANE_Status status;

  DBG (5, "sane_cancel: start\n");

  status = usb_high_scan_stop_scan (s->hw);
  if (status != SANE_STATUS_GOOD)
    DBG (3, "sane_cancel: usb_high_scan_stop_scan returned `%s' for `%s'\n",
	 sane_strstatus (status), s->hw->name);
  usb_high_scan_back_home (s->hw);
  if (status != SANE_STATUS_GOOD)
    DBG (3, "sane_cancel: usb_high_scan_back_home returned `%s' for `%s'\n",
	 sane_strstatus (status), s->hw->name);

  if (s->scanning)
    {
      s->scanning = SANE_FALSE;
      if (s->total_bytes != (s->params.bytes_per_line * s->params.lines))
	DBG (1, "sane_cancel: warning: scanned %d bytes, expected %d "
	     "bytes\n", s->total_bytes,
	     s->params.bytes_per_line * s->params.lines);
      else
	DBG (3, "sane_cancel: scan finished, scanned %d bytes\n",
	     s->total_bytes);
    }
  else
    {
      DBG (4, "sane_cancel: scan has not been initiated yet, "
	   "or it is allready aborted\n");
    }
  DBG (5, "sane_cancel: exit\n");
  return;
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Mustek_Usb_Scanner *s = handle;

  DBG (5, "sane_set_io_mode: handle = %p, non_blocking = %s\n",
       handle, non_blocking == SANE_TRUE ? "true" : "false");
  if (!s->scanning)
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
  Mustek_Usb_Scanner *s = handle;

  DBG (5, "sane_get_select_fd: handle = %p, fd = %p\n", handle, (void *) fd);
  if (!s->scanning)
    {
      DBG (1, "sane_get_select_fd: not scanning\n");
      return SANE_STATUS_INVAL;
    }
  return SANE_STATUS_UNSUPPORTED;
}
