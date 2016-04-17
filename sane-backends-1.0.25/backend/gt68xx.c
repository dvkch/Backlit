/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   Copyright (C) 2002 - 2007 Henning Geinitz <sane@geinitz.org>
   Copyright (C) 2009 Stéphane Voltz <stef.dev@free.fr> for sheetfed
                      calibration code.

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
*/

/*
 * SANE backend for Grandtech GT-6801 and GT-6816 based scanners
 */

#include "../include/sane/config.h"

#define BUILD 84
#define MAX_DEBUG
#define WARMUP_TIME 60
#define CALIBRATION_HEIGHT 2.5
#define SHORT_TIMEOUT (1 * 1000)
#define LONG_TIMEOUT (30 * 1000)

/* Use a reader process if possible (usually faster) */
#if defined (HAVE_SYS_SHM_H) && (!defined (USE_PTHREAD)) && (!defined (HAVE_OS2_H))
#define USE_FORK
#define SHM_BUFFERS 10
#endif

#define TUNE_CALIBRATOR

/* Send coarse white or black calibration to stdout */
#if 0
#define SAVE_WHITE_CALIBRATION
#endif
#if 0
#define SAVE_BLACK_CALIBRATION
#endif

/* Debug calibration, print total brightness of the scanned image */
#if 0
#define DEBUG_BRIGHTNESS
#endif

/* Debug calibration, print black mark values */
#if 0
#define DEBUG_BLACK
#endif

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
#include <time.h>
#include <math.h>
#include <dirent.h>

#include "../include/_stdint.h"

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#define BACKEND_NAME gt68xx

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"

#ifndef SANE_I18N
#define SANE_I18N(text) text
#endif

#include "gt68xx.h"
#include "gt68xx_high.c"
#include "gt68xx_devices.c"

static SANE_Int num_devices = 0;
static GT68xx_Device *first_dev = 0;
static GT68xx_Scanner *first_handle = 0;
static const SANE_Device **devlist = 0;
/* Array of newly attached devices */
static GT68xx_Device **new_dev = 0;
/* Length of new_dev array */
static SANE_Int new_dev_len = 0;
/* Number of entries alloced for new_dev */
static SANE_Int new_dev_alloced = 0;
/* Is this computer little-endian ?*/
SANE_Bool little_endian;
SANE_Bool debug_options = SANE_FALSE;

static SANE_String_Const mode_list[] = {
  SANE_VALUE_SCAN_MODE_COLOR,
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_LINEART,
  0
};

static SANE_String_Const gray_mode_list[] = {
  GT68XX_COLOR_RED,
  GT68XX_COLOR_GREEN,
  GT68XX_COLOR_BLUE,
  0
};

static SANE_String_Const source_list[] = {
  SANE_I18N ("Flatbed"),
  SANE_I18N ("Transparency Adapter"),
  0
};

static SANE_Range x_range = {
  SANE_FIX (0.0),               /* minimum */
  SANE_FIX (216.0),             /* maximum */
  SANE_FIX (0.0)                /* quantization */
};

static SANE_Range y_range = {
  SANE_FIX (0.0),               /* minimum */
  SANE_FIX (299.0),             /* maximum */
  SANE_FIX (0.0)                /* quantization */
};


static const SANE_Range offset_range = {
  -63,                          /* minimum */
  63,                           /* maximum */
  1                             /* quantization */
};

static SANE_Range gamma_range = {
  SANE_FIX (0.01),              /* minimum */
  SANE_FIX (5.0),               /* maximum */
  SANE_FIX (0.01)               /* quantization */
};

static const SANE_Range u8_range = {
  0,                            /* minimum */
  255,                          /* maximum */
  0                             /* quantization */
};

/* Test if this machine is little endian (from coolscan.c) */
static SANE_Bool
calc_little_endian (void)
{
  SANE_Int testvalue = 255;
  uint8_t *firstbyte = (uint8_t *) & testvalue;

  if (*firstbyte == 255)
    return SANE_TRUE;
  return SANE_FALSE;
}

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
get_afe_values (SANE_String_Const cp, GT68xx_AFE_Parameters * afe)
{
  SANE_Char *word, *end;
  int i;

  for (i = 0; i < 6; i++)
    {
      cp = sanei_config_get_string (cp, &word);
      if (word && *word)
        {
          long int long_value;
          errno = 0;
          long_value = strtol (word, &end, 0);

          if (end == word)
            {
              DBG (5, "get_afe_values: can't parse %d. parameter `%s'\n",
                   i + 1, word);
              free (word);
              word = 0;
              return SANE_STATUS_INVAL;
            }
          else if (errno)
            {
              DBG (5, "get_afe_values: can't parse %d. parameter `%s' "
                   "(%s)\n", i + 1, word, strerror (errno));
              free (word);
              word = 0;
              return SANE_STATUS_INVAL;
            }
          else if (long_value < 0)
            {
              DBG (5, "get_afe_values: %d. parameter < 0 (%d)\n", i + 1,
                   (int) long_value);
              free (word);
              word = 0;
              return SANE_STATUS_INVAL;
            }
          else if (long_value > 0x3f)
            {
              DBG (5, "get_afe_values: %d. parameter > 0x3f (%d)\n", i + 1,
                   (int) long_value);
              free (word);
              word = 0;
              return SANE_STATUS_INVAL;
            }
          else
            {
              DBG (5, "get_afe_values: %d. parameter set to 0x%02x\n", i + 1,
                   (int) long_value);
              switch (i)
                {
                case 0:
                  afe->r_offset = (SANE_Byte) long_value;
                  break;
                case 1:
                  afe->r_pga = (SANE_Byte) long_value;
                  break;
                case 2:
                  afe->g_offset = (SANE_Byte) long_value;
                  break;
                case 3:
                  afe->g_pga = (SANE_Byte) long_value;
                  break;
                case 4:
                  afe->b_offset = (SANE_Byte) long_value;
                  break;
                case 5:
                  afe->b_pga = (SANE_Byte) long_value;
                  break;
                }
              free (word);
              word = 0;
            }
        }
      else
        {
          DBG (5, "get_afe_values: option `afe' needs 6  parameters\n");
          return SANE_STATUS_INVAL;
        }
    }
  return SANE_STATUS_GOOD;
}

static SANE_Status
setup_scan_request (GT68xx_Scanner * s, GT68xx_Scan_Request * scan_request)
{

  if (s->dev->model->flags & GT68XX_FLAG_MIRROR_X)
    scan_request->x0 =
      s->opt[OPT_TL_X].constraint.range->max - s->val[OPT_BR_X].w;
  else
    scan_request->x0 = s->val[OPT_TL_X].w;
  scan_request->y0 = s->val[OPT_TL_Y].w;
  scan_request->xs = s->val[OPT_BR_X].w - s->val[OPT_TL_X].w;
  scan_request->ys = s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w;

  if (s->val[OPT_FULL_SCAN].w == SANE_TRUE)
    {
      scan_request->x0 -= s->dev->model->x_offset;
      scan_request->y0 -= (s->dev->model->y_offset);
      scan_request->xs += s->dev->model->x_offset;
      scan_request->ys += s->dev->model->y_offset;
    }

  scan_request->xdpi = s->val[OPT_RESOLUTION].w;
  if (scan_request->xdpi > s->dev->model->optical_xdpi)
    scan_request->xdpi = s->dev->model->optical_xdpi;
  scan_request->ydpi = s->val[OPT_RESOLUTION].w;

  if (IS_ACTIVE (OPT_BIT_DEPTH) && !s->val[OPT_PREVIEW].w)
    scan_request->depth = s->val[OPT_BIT_DEPTH].w;
  else
    scan_request->depth = 8;

  if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR) == 0)
    scan_request->color = SANE_TRUE;
  else
    scan_request->color = SANE_FALSE;

  if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART) == 0)
    {
      SANE_Int xs =
        SANE_UNFIX (scan_request->xs) * scan_request->xdpi / MM_PER_INCH +
        0.5;

      if (xs % 8)
        {
          scan_request->xs =
            SANE_FIX ((xs - (xs % 8)) * MM_PER_INCH / scan_request->xdpi);
          DBG (5, "setup_scan_request: lineart mode, %d pixels %% 8 = %d\n",
               xs, xs % 8);
        }
    }

  scan_request->calculate = SANE_FALSE;
  scan_request->lamp = SANE_TRUE;
  scan_request->mbs = SANE_FALSE;

  if (strcmp (s->val[OPT_SOURCE].s, "Transparency Adapter") == 0)
    scan_request->use_ta = SANE_TRUE;
  else
    scan_request->use_ta = SANE_FALSE;

  return SANE_STATUS_GOOD;
}

static SANE_Status
calc_parameters (GT68xx_Scanner * s)
{
  SANE_String val;
  SANE_Status status = SANE_STATUS_GOOD;
  GT68xx_Scan_Request scan_request;
  GT68xx_Scan_Parameters scan_params;

  DBG (5, "calc_parameters: start\n");
  val = s->val[OPT_MODE].s;

  s->params.last_frame = SANE_TRUE;
  if (strcmp (val, SANE_VALUE_SCAN_MODE_GRAY) == 0
      || strcmp (val, SANE_VALUE_SCAN_MODE_LINEART) == 0)
    s->params.format = SANE_FRAME_GRAY;
  else                          /* Color */
    s->params.format = SANE_FRAME_RGB;

  setup_scan_request (s, &scan_request);
  scan_request.calculate = SANE_TRUE;

  status = gt68xx_device_setup_scan (s->dev, &scan_request, SA_SCAN,
                                     &scan_params);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "calc_parameters: gt68xx_device_setup_scan returned: %s\n",
           sane_strstatus (status));
      return status;
    }

  if (strcmp (val, SANE_VALUE_SCAN_MODE_LINEART) == 0)
    s->params.depth = 1;
  else
    s->params.depth = scan_params.depth;

  s->params.lines = scan_params.pixel_ys;
  s->params.pixels_per_line = scan_params.pixel_xs;
  /* Inflate X if necessary */
  if (s->val[OPT_RESOLUTION].w > s->dev->model->optical_xdpi)
    s->params.pixels_per_line *=
      (s->val[OPT_RESOLUTION].w / s->dev->model->optical_xdpi);
  s->params.bytes_per_line = s->params.pixels_per_line;
  if (s->params.depth > 8)
    {
      s->params.depth = 16;
      s->params.bytes_per_line *= 2;
    }
  else if (s->params.depth == 1)
    s->params.bytes_per_line /= 8;

  if (s->params.format == SANE_FRAME_RGB)
    s->params.bytes_per_line *= 3;

  DBG (5, "calc_parameters: exit\n");
  return status;
}

static SANE_Status
create_bpp_list (GT68xx_Scanner * s, SANE_Int * bpp)
{
  int count;

  for (count = 0; bpp[count] != 0; count++)
    ;
  s->bpp_list[0] = count;
  for (count = 0; bpp[count] != 0; count++)
    {
      s->bpp_list[s->bpp_list[0] - count] = bpp[count];
    }
  return SANE_STATUS_GOOD;
}

static SANE_Status
init_options (GT68xx_Scanner * s)
{
  SANE_Int option, count;
  SANE_Status status;
  SANE_Word *dpi_list;
  GT68xx_Model *model = s->dev->model;
  SANE_Bool has_ta = SANE_FALSE;

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
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE].s = strdup (SANE_VALUE_SCAN_MODE_GRAY);

  /* scan mode */
  s->opt[OPT_GRAY_MODE_COLOR].name = "gray-mode-color";
  s->opt[OPT_GRAY_MODE_COLOR].title = SANE_I18N ("Gray mode color");
  s->opt[OPT_GRAY_MODE_COLOR].desc =
    SANE_I18N ("Selects which scan color is used "
               "gray mode (default: green).");
  s->opt[OPT_GRAY_MODE_COLOR].type = SANE_TYPE_STRING;
  s->opt[OPT_GRAY_MODE_COLOR].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_GRAY_MODE_COLOR].size = max_string_size (gray_mode_list);
  s->opt[OPT_GRAY_MODE_COLOR].constraint.string_list = gray_mode_list;
  s->val[OPT_GRAY_MODE_COLOR].s = strdup (GT68XX_COLOR_GREEN);

  /* scan source */
  s->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  s->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  s->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  s->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
  s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SOURCE].size = max_string_size (source_list);
  s->opt[OPT_SOURCE].constraint.string_list = source_list;
  s->val[OPT_SOURCE].s = strdup ("Flatbed");
  status = gt68xx_device_get_ta_status (s->dev, &has_ta);
  if (status != SANE_STATUS_GOOD || !has_ta)
    DISABLE (OPT_SOURCE);

  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_PREVIEW].unit = SANE_UNIT_NONE;
  s->opt[OPT_PREVIEW].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_PREVIEW].w = SANE_FALSE;

  /* lamp on */
  s->opt[OPT_LAMP_OFF_AT_EXIT].name = SANE_NAME_LAMP_OFF_AT_EXIT;
  s->opt[OPT_LAMP_OFF_AT_EXIT].title = SANE_TITLE_LAMP_OFF_AT_EXIT;
  s->opt[OPT_LAMP_OFF_AT_EXIT].desc = SANE_DESC_LAMP_OFF_AT_EXIT;
  s->opt[OPT_LAMP_OFF_AT_EXIT].type = SANE_TYPE_BOOL;
  s->opt[OPT_LAMP_OFF_AT_EXIT].unit = SANE_UNIT_NONE;
  s->opt[OPT_LAMP_OFF_AT_EXIT].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_LAMP_OFF_AT_EXIT].w = SANE_TRUE;
  if (s->dev->model->is_cis && !(s->dev->model->flags & GT68XX_FLAG_CIS_LAMP))
    DISABLE (OPT_LAMP_OFF_AT_EXIT);

  /* bit depth */
  s->opt[OPT_BIT_DEPTH].name = SANE_NAME_BIT_DEPTH;
  s->opt[OPT_BIT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
  s->opt[OPT_BIT_DEPTH].desc = SANE_DESC_BIT_DEPTH;
  s->opt[OPT_BIT_DEPTH].type = SANE_TYPE_INT;
  s->opt[OPT_BIT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_BIT_DEPTH].size = sizeof (SANE_Word);
  s->opt[OPT_BIT_DEPTH].constraint.word_list = 0;
  s->opt[OPT_BIT_DEPTH].constraint.word_list = s->bpp_list;
  RIE (create_bpp_list (s, s->dev->model->bpp_gray_values));
  s->val[OPT_BIT_DEPTH].w = 8;
  if (s->opt[OPT_BIT_DEPTH].constraint.word_list[0] < 2)
    DISABLE (OPT_BIT_DEPTH);

  /* resolution */
  for (count = 0; model->ydpi_values[count] != 0; count++)
    ;
  dpi_list = malloc ((count + 1) * sizeof (SANE_Word));
  if (!dpi_list)
    return SANE_STATUS_NO_MEM;
  dpi_list[0] = count;
  for (count = 0; model->ydpi_values[count] != 0; count++)
    dpi_list[dpi_list[0] - count] = model->ydpi_values[count];
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_RESOLUTION].constraint.word_list = dpi_list;
  s->val[OPT_RESOLUTION].w = 300;

  /* backtrack */
  s->opt[OPT_BACKTRACK].name = SANE_NAME_BACKTRACK;
  s->opt[OPT_BACKTRACK].title = SANE_TITLE_BACKTRACK;
  s->opt[OPT_BACKTRACK].desc = SANE_DESC_BACKTRACK;
  s->opt[OPT_BACKTRACK].type = SANE_TYPE_BOOL;
  s->val[OPT_BACKTRACK].w = SANE_FALSE;

  /* "Debug" group: */
  s->opt[OPT_DEBUG_GROUP].title = SANE_I18N ("Debugging Options");
  s->opt[OPT_DEBUG_GROUP].desc = "";
  s->opt[OPT_DEBUG_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_DEBUG_GROUP].size = 0;
  s->opt[OPT_DEBUG_GROUP].cap = 0;
  s->opt[OPT_DEBUG_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  if (!debug_options)
    DISABLE (OPT_DEBUG_GROUP);

  /* auto warmup */
  s->opt[OPT_AUTO_WARMUP].name = "auto-warmup";
  s->opt[OPT_AUTO_WARMUP].title = SANE_I18N ("Automatic warmup");
  s->opt[OPT_AUTO_WARMUP].desc =
    SANE_I18N ("Warm-up until the lamp's brightness is constant "
               "instead of insisting on 60 seconds warm-up time.");
  s->opt[OPT_AUTO_WARMUP].type = SANE_TYPE_BOOL;
  s->opt[OPT_AUTO_WARMUP].unit = SANE_UNIT_NONE;
  s->opt[OPT_AUTO_WARMUP].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_AUTO_WARMUP].w = SANE_TRUE;
  if ((s->dev->model->is_cis
       && !(s->dev->model->flags & GT68XX_FLAG_CIS_LAMP)) || !debug_options)
    DISABLE (OPT_AUTO_WARMUP);

  /* full scan */
  s->opt[OPT_FULL_SCAN].name = "full-scan";
  s->opt[OPT_FULL_SCAN].title = SANE_I18N ("Full scan");
  s->opt[OPT_FULL_SCAN].desc =
    SANE_I18N ("Scan the complete scanning area including calibration strip. "
               "Be careful. Don't select the full height. For testing only.");
  s->opt[OPT_FULL_SCAN].type = SANE_TYPE_BOOL;
  s->opt[OPT_FULL_SCAN].unit = SANE_UNIT_NONE;
  s->opt[OPT_FULL_SCAN].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_FULL_SCAN].w = SANE_FALSE;
  if (!debug_options)
    DISABLE (OPT_FULL_SCAN);

  /* coarse calibration */
  s->opt[OPT_COARSE_CAL].name = "coarse-calibration";
  s->opt[OPT_COARSE_CAL].title = SANE_I18N ("Coarse calibration");
  s->opt[OPT_COARSE_CAL].desc =
    SANE_I18N ("Setup gain and offset for scanning automatically. If this "
               "option is disabled, options for setting the analog frontend "
               "parameters manually are provided. This option is enabled "
               "by default. For testing only.");
  s->opt[OPT_COARSE_CAL].type = SANE_TYPE_BOOL;
  s->opt[OPT_COARSE_CAL].unit = SANE_UNIT_NONE;
  s->opt[OPT_COARSE_CAL].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_COARSE_CAL].w = SANE_TRUE;
  if (!debug_options)
    DISABLE (OPT_COARSE_CAL);
  if (s->dev->model->flags & GT68XX_FLAG_SHEET_FED)
    {
      s->val[OPT_COARSE_CAL].w = SANE_FALSE;
      DISABLE (OPT_COARSE_CAL);
    }

  /* coarse calibration only once */
  s->opt[OPT_COARSE_CAL_ONCE].name = "coarse-calibration-once";
  s->opt[OPT_COARSE_CAL_ONCE].title =
    SANE_I18N ("Coarse calibration for first scan only");
  s->opt[OPT_COARSE_CAL_ONCE].desc =
    SANE_I18N ("Coarse calibration is only done for the first scan. Works "
               "with most scanners and can save scanning time. If the image "
               "brightness is different with each scan, disable this option. "
               "For testing only.");
  s->opt[OPT_COARSE_CAL_ONCE].type = SANE_TYPE_BOOL;
  s->opt[OPT_COARSE_CAL_ONCE].unit = SANE_UNIT_NONE;
  s->opt[OPT_COARSE_CAL_ONCE].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_COARSE_CAL_ONCE].w = SANE_FALSE;
  if (!debug_options)
    DISABLE (OPT_COARSE_CAL_ONCE);
  if (s->dev->model->flags & GT68XX_FLAG_SHEET_FED)
    DISABLE (OPT_COARSE_CAL_ONCE);

  /* calibration */
  s->opt[OPT_QUALITY_CAL].name = SANE_NAME_QUALITY_CAL;
  s->opt[OPT_QUALITY_CAL].title = SANE_TITLE_QUALITY_CAL;
  s->opt[OPT_QUALITY_CAL].desc = SANE_TITLE_QUALITY_CAL;
  s->opt[OPT_QUALITY_CAL].type = SANE_TYPE_BOOL;
  s->opt[OPT_QUALITY_CAL].unit = SANE_UNIT_NONE;
  s->opt[OPT_QUALITY_CAL].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_QUALITY_CAL].w = SANE_TRUE;
  if (!debug_options)
    DISABLE (OPT_QUALITY_CAL);
  /* we disable image correction for scanners that can't calibrate */
  if ((s->dev->model->flags & GT68XX_FLAG_SHEET_FED)
    &&(!(s->dev->model->flags & GT68XX_FLAG_HAS_CALIBRATE)))
    {
      s->val[OPT_QUALITY_CAL].w = SANE_FALSE;
      DISABLE (OPT_QUALITY_CAL);
    }

  /* backtrack lines */
  s->opt[OPT_BACKTRACK_LINES].name = "backtrack-lines";
  s->opt[OPT_BACKTRACK_LINES].title = SANE_I18N ("Backtrack lines");
  s->opt[OPT_BACKTRACK_LINES].desc =
    SANE_I18N ("Number of lines the scan slider moves back when backtracking "
               "occurs. That happens when the scanner scans faster than the "
               "computer can receive the data. Low values cause faster scans "
               "but increase the risk of omitting lines.");
  s->opt[OPT_BACKTRACK_LINES].type = SANE_TYPE_INT;
  s->opt[OPT_BACKTRACK_LINES].unit = SANE_UNIT_NONE;
  s->opt[OPT_BACKTRACK_LINES].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BACKTRACK_LINES].constraint.range = &u8_range;
  if (s->dev->model->is_cis && !(s->dev->model->flags & GT68XX_FLAG_SHEET_FED))
    s->val[OPT_BACKTRACK_LINES].w = 0x10;
  else
    s->val[OPT_BACKTRACK_LINES].w = 0x3f;
  if (!debug_options)
    DISABLE (OPT_BACKTRACK_LINES);

  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = SANE_I18N ("Enhancement");
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].size = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* internal gamma value */
  s->opt[OPT_GAMMA_VALUE].name = "gamma-value";
  s->opt[OPT_GAMMA_VALUE].title = SANE_I18N ("Gamma value");
  s->opt[OPT_GAMMA_VALUE].desc =
    SANE_I18N ("Sets the gamma value of all channels.");
  s->opt[OPT_GAMMA_VALUE].type = SANE_TYPE_FIXED;
  s->opt[OPT_GAMMA_VALUE].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VALUE].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VALUE].constraint.range = &gamma_range;
  s->opt[OPT_GAMMA_VALUE].cap |= SANE_CAP_EMULATED;
  s->val[OPT_GAMMA_VALUE].w = s->dev->gamma_value;

  /* threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &u8_range;
  s->val[OPT_THRESHOLD].w = 128;
  DISABLE (OPT_THRESHOLD);

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].title = SANE_I18N ("Geometry");
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].size = 0;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  x_range.max = model->x_size;
  y_range.max = model->y_size;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &x_range;
  s->val[OPT_BR_X].w = x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &y_range;
  s->val[OPT_BR_Y].w = y_range.max;

  /* sensor group */
  s->opt[OPT_SENSOR_GROUP].name = SANE_NAME_SENSORS;
  s->opt[OPT_SENSOR_GROUP].title = SANE_TITLE_SENSORS;
  s->opt[OPT_SENSOR_GROUP].desc = SANE_DESC_SENSORS;
  s->opt[OPT_SENSOR_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_SENSOR_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  
  /* calibration needed */
  s->opt[OPT_NEED_CALIBRATION_SW].name = "need-calibration";
  s->opt[OPT_NEED_CALIBRATION_SW].title = SANE_I18N ("Need calibration");
  s->opt[OPT_NEED_CALIBRATION_SW].desc = SANE_I18N ("The scanner needs calibration for the current settings");
  s->opt[OPT_NEED_CALIBRATION_SW].type = SANE_TYPE_BOOL;
  s->opt[OPT_NEED_CALIBRATION_SW].unit = SANE_UNIT_NONE;
  if (s->dev->model->flags & GT68XX_FLAG_HAS_CALIBRATE)
    s->opt[OPT_NEED_CALIBRATION_SW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  else
    s->opt[OPT_NEED_CALIBRATION_SW].cap = SANE_CAP_INACTIVE;
  s->val[OPT_NEED_CALIBRATION_SW].b = 0;

  /* document present sensor */
  s->opt[OPT_PAGE_LOADED_SW].name = SANE_NAME_PAGE_LOADED;
  s->opt[OPT_PAGE_LOADED_SW].title = SANE_TITLE_PAGE_LOADED;
  s->opt[OPT_PAGE_LOADED_SW].desc = SANE_DESC_PAGE_LOADED;
  s->opt[OPT_PAGE_LOADED_SW].type = SANE_TYPE_BOOL;
  s->opt[OPT_PAGE_LOADED_SW].unit = SANE_UNIT_NONE;
  if (s->dev->model->command_set->document_present)
    s->opt[OPT_PAGE_LOADED_SW].cap =
      SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  else
    s->opt[OPT_PAGE_LOADED_SW].cap = SANE_CAP_INACTIVE;
  s->val[OPT_PAGE_LOADED_SW].b = 0;

  /* button group */
  s->opt[OPT_BUTTON_GROUP].name = "Buttons";
  s->opt[OPT_BUTTON_GROUP].title = SANE_I18N ("Buttons");
  s->opt[OPT_BUTTON_GROUP].desc = SANE_I18N ("Buttons");
  s->opt[OPT_BUTTON_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_BUTTON_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* calibrate button */
  s->opt[OPT_CALIBRATE].name = "calibrate";
  s->opt[OPT_CALIBRATE].title = SANE_I18N ("Calibrate");
  s->opt[OPT_CALIBRATE].desc =
    SANE_I18N ("Start calibration using special sheet");
  s->opt[OPT_CALIBRATE].type = SANE_TYPE_BUTTON;
  s->opt[OPT_CALIBRATE].unit = SANE_UNIT_NONE;
  if (s->dev->model->flags & GT68XX_FLAG_HAS_CALIBRATE)
  s->opt[OPT_CALIBRATE].cap =
      SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT | SANE_CAP_ADVANCED |
      SANE_CAP_AUTOMATIC;
  else
    s->opt[OPT_CALIBRATE].cap = SANE_CAP_INACTIVE;
  s->val[OPT_CALIBRATE].b = 0;

  /* clear calibration cache button */
  s->opt[OPT_CLEAR_CALIBRATION].name = "clear";
  s->opt[OPT_CLEAR_CALIBRATION].title = SANE_I18N ("Clear calibration");
  s->opt[OPT_CLEAR_CALIBRATION].desc = SANE_I18N ("Clear calibration cache");
  s->opt[OPT_CLEAR_CALIBRATION].type = SANE_TYPE_BUTTON;
  s->opt[OPT_CLEAR_CALIBRATION].unit = SANE_UNIT_NONE;
  if (s->dev->model->flags & GT68XX_FLAG_HAS_CALIBRATE)
  s->opt[OPT_CLEAR_CALIBRATION].cap =
    SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT | SANE_CAP_ADVANCED |
    SANE_CAP_AUTOMATIC;
  else
    s->opt[OPT_CLEAR_CALIBRATION].cap = SANE_CAP_INACTIVE;
  s->val[OPT_CLEAR_CALIBRATION].b = 0;


  RIE (calc_parameters (s));

  DBG (5, "init_options: exit\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
attach (SANE_String_Const devname, GT68xx_Device ** devp, SANE_Bool may_wait)
{
  GT68xx_Device *dev;
  SANE_Status status;

  DBG (5, "attach: start: devp %s NULL, may_wait = %d\n", devp ? "!=" : "==",
       may_wait);
  if (!devname)
    {
      DBG (1, "attach: devname == NULL\n");
      return SANE_STATUS_INVAL;
    }

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->file_name, devname) == 0)
        {
          if (devp)
            *devp = dev;
          dev->missing = SANE_FALSE;
          DBG (4, "attach: device `%s' was already in device list\n",
               devname);
          return SANE_STATUS_GOOD;
        }
    }

  DBG (4, "attach: trying to open device `%s'\n", devname);
  RIE (gt68xx_device_new (&dev));
  status = gt68xx_device_open (dev, devname);
  if (status == SANE_STATUS_GOOD)
    DBG (4, "attach: device `%s' successfully opened\n", devname);
  else
    {
      DBG (4, "attach: couldn't open device `%s': %s\n", devname,
           sane_strstatus (status));
      gt68xx_device_free (dev);
      if (devp)
        *devp = 0;
      return status;
    }

  if (!gt68xx_device_is_configured (dev))
    {
      GT68xx_Model *model = NULL;
      DBG (2, "attach: Warning: device `%s' is not listed in device table\n",
           devname);
      DBG (2,
           "attach: If you have manually added it, use override in gt68xx.conf\n");
      gt68xx_device_get_model ("unknown-scanner", &model);
      status = gt68xx_device_set_model (dev, model);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (4, "attach: couldn't set model: %s\n",
               sane_strstatus (status));
          gt68xx_device_free (dev);
          if (devp)
            *devp = 0;
          return status;
        }
      dev->manual_selection = SANE_TRUE;
    }

  dev->file_name = strdup (devname);
  dev->missing = SANE_FALSE;
  if (!dev->file_name)
    return SANE_STATUS_NO_MEM;
  DBG (2, "attach: found %s flatbed scanner %s at %s\n", dev->model->vendor,
       dev->model->model, dev->file_name);
  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;
  gt68xx_device_close (dev);
  DBG (5, "attach: exit\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one_device (SANE_String_Const devname)
{
  GT68xx_Device *dev;
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

#if defined(_WIN32) || defined(HAVE_OS2_H)
# define PATH_SEP       "\\"
#else
# define PATH_SEP       "/"
#endif

static SANE_Status
download_firmware_file (GT68xx_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte *buf = NULL;
  int size = -1;
  SANE_Char filename[PATH_MAX], dirname[PATH_MAX], basename[PATH_MAX];
  FILE *f;

  if (strncmp (dev->model->firmware_name, PATH_SEP, 1) != 0)
    {
      /* probably filename only */
      snprintf (filename, PATH_MAX, "%s%s%s%s%s%s%s",
                STRINGIFY (PATH_SANE_DATA_DIR),
                PATH_SEP, "sane", PATH_SEP, "gt68xx", PATH_SEP,
                dev->model->firmware_name);
      snprintf (dirname, PATH_MAX, "%s%s%s%s%s",
                STRINGIFY (PATH_SANE_DATA_DIR),
                PATH_SEP, "sane", PATH_SEP, "gt68xx");
      strncpy (basename, dev->model->firmware_name, PATH_MAX);
    }
  else
    {
      /* absolute path */
      char *pos;
      strncpy (filename, dev->model->firmware_name, PATH_MAX);
      strncpy (dirname, dev->model->firmware_name, PATH_MAX);
      pos = strrchr (dirname, PATH_SEP[0]);
      if (pos)
        pos[0] = '\0';
      strncpy (basename, pos + 1, PATH_MAX);
    }

  /* first, try to open with exact case */
  DBG (5, "download_firmware: trying %s\n", filename);
  f = fopen (filename, "rb");
  if (!f)
    {
      /* and now any case */
      DIR *dir;
      struct dirent *direntry;

      DBG (5,
           "download_firmware_file: Couldn't open firmware file `%s': %s\n",
           filename, strerror (errno));

      dir = opendir (dirname);
      if (!dir)
        {
          DBG (5, "download_firmware: couldn't open directory `%s': %s\n",
               dirname, strerror (errno));
          status = SANE_STATUS_INVAL;
        }
      if (status == SANE_STATUS_GOOD)
        {
          do
            {
              direntry = readdir (dir);
              if (direntry
                  && (strncasecmp (direntry->d_name, basename, PATH_MAX) ==
                      0))
                {
                  snprintf (filename, PATH_MAX, "%s%s%s",
                            dirname, PATH_SEP, direntry->d_name);
                  break;
                }
            }
          while (direntry != 0);
          if (direntry == 0)
            {
              DBG (5, "download_firmware: file `%s' not found\n", filename);
              status = SANE_STATUS_INVAL;
            }
          closedir (dir);
        }
      if (status == SANE_STATUS_GOOD)
        {
          DBG (5, "download_firmware: trying %s\n", filename);
          f = fopen (filename, "rb");
          if (!f)
            {
              DBG (5,
                   "download_firmware_file: Couldn't open firmware file `%s': %s\n",
                   filename, strerror (errno));
              status = SANE_STATUS_INVAL;
            }
        }

      if (status != SANE_STATUS_GOOD)
        {
          DBG (0, "Couldn't open firmware file (`%s'): %s\n",
               filename, strerror (errno));
        }
    }

  if (status == SANE_STATUS_GOOD)
    {
      fseek (f, 0, SEEK_END);
      size = ftell (f);
      fseek (f, 0, SEEK_SET);
      if (size == -1)
        {
          DBG (1, "download_firmware_file: error getting size of "
               "firmware file \"%s\": %s\n", filename, strerror (errno));
          status = SANE_STATUS_INVAL;
        }
    }

  if (status == SANE_STATUS_GOOD)
    {
      DBG (5, "firmware size: %d\n", size);
      buf = (SANE_Byte *) malloc (size);
      if (!buf)
        {
          DBG (1, "download_firmware_file: cannot allocate %d bytes "
               "for firmware\n", size);
          status = SANE_STATUS_NO_MEM;
        }
    }

  if (status == SANE_STATUS_GOOD)
    {
      int bytes_read = fread (buf, 1, size, f);
      if (bytes_read != size)
        {
          DBG (1, "download_firmware_file: problem reading firmware "
               "file \"%s\": %s\n", filename, strerror (errno));
          status = SANE_STATUS_INVAL;
        }
    }

  if (f)
    fclose (f);

  if (status == SANE_STATUS_GOOD)
    {
      status = gt68xx_device_download_firmware (dev, buf, size);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (1, "download_firmware_file: firmware download failed: %s\n",
               sane_strstatus (status));
        }
    }

  if (buf)
    free (buf);

  return status;
}

/** probe for gt68xx devices
 * This function scan usb and try to attached to scanner
 * configured in gt68xx.conf .
 */
static SANE_Status probe_gt68xx_devices(void)
{
  SANE_Char line[PATH_MAX];
  SANE_Char *word;
  SANE_String_Const cp;
  SANE_Int linenumber;
  GT68xx_Device *dev;
  FILE *fp;

  /* set up for no new devices detected at first */
  new_dev = 0;
  new_dev_len = 0;
  new_dev_alloced = 0;

  /* mark already detected devices as missing, during device probe
   * detected devices will clear this flag */
  dev = first_dev;
  while(dev!=NULL)
    {
      dev->missing = SANE_TRUE;
      dev = dev->next;
    }

  fp = sanei_config_open (GT68XX_CONFIG_FILE);
  if (!fp)
    {
      /* default to /dev/usb/scanner instead of insisting on config file */
      DBG (3, "sane_init: couldn't open config file `%s': %s. Using "
           "/dev/usb/scanner directly\n", GT68XX_CONFIG_FILE,
           strerror (errno));
      attach ("/dev/usb/scanner", 0, SANE_FALSE);
      return SANE_STATUS_GOOD;
    }

  little_endian = calc_little_endian ();
  DBG (5, "sane_init: %s endian machine\n", little_endian ? "little" : "big");

  linenumber = 0;
  DBG (4, "sane_init: reading config file `%s'\n", GT68XX_CONFIG_FILE);
  while (sanei_config_read (line, sizeof (line), fp))
    {
      word = 0;
      linenumber++;

      cp = sanei_config_get_string (line, &word);
      if (!word || cp == line)
        {
          DBG (6, "sane_init: config file line %d: ignoring empty line\n",
               linenumber);
          if (word)
            free (word);
          continue;
        }
      if (word[0] == '#')
        {
          DBG (6, "sane_init: config file line %d: ignoring comment line\n",
               linenumber);
          free (word);
          continue;
        }

      if (strcmp (word, "firmware") == 0)
        {
          free (word);
          word = 0;
          cp = sanei_config_get_string (cp, &word);
          if (word)
            {
              int i;
              for (i = 0; i < new_dev_len; i++)
                {
                  new_dev[i]->model->firmware_name = word;
                  DBG (5, "sane_init: device %s: firmware will be loaded "
                       "from %s\n", new_dev[i]->model->name,
                       new_dev[i]->model->firmware_name);
                }
              if (i == 0)
                DBG (5, "sane_init: firmware %s can't be loaded, set device "
                     "first\n", word);
            }
          else
            {
              DBG (3, "sane_init: option `firmware' needs a parameter\n");
            }
        }
      else if (strcmp (word, "vendor") == 0)
        {
          free (word);
          word = 0;
          cp = sanei_config_get_string (cp, &word);
          if (word)
            {
              int i;

              for (i = 0; i < new_dev_len; i++)
                {
                  new_dev[i]->model->vendor = word;
                  DBG (5, "sane_init: device %s: vendor name set to %s\n",
                       new_dev[i]->model->name, new_dev[i]->model->vendor);
                }
              if (i == 0)
                DBG (5, "sane_init: can't set vendor name %s, set device "
                     "first\n", word);
            }
          else
            {
              DBG (3, "sane_init: option `vendor' needs a parameter\n");
            }
        }
      else if (strcmp (word, "model") == 0)
        {
          free (word);
          word = 0;
          cp = sanei_config_get_string (cp, &word);
          if (word)
            {
              int i;
              for (i = 0; i < new_dev_len; i++)
                {
                  new_dev[i]->model->model = word;
                  DBG (5, "sane_init: device %s: model name set to %s\n",
                       new_dev[i]->model->name, new_dev[i]->model->model);
                }
              if (i == 0)
                DBG (5, "sane_init: can't set model name %s, set device "
                     "first\n", word);
              free (word);
            }
          else
            {
              DBG (3, "sane_init: option `model' needs a parameter\n");
            }
        }
      else if (strcmp (word, "override") == 0)
        {
          free (word);
          word = 0;
          cp = sanei_config_get_string (cp, &word);
          if (word)
            {
              int i;
              for (i = 0; i < new_dev_len; i++)
                {
                  SANE_Status status;
                  GT68xx_Device *dev = new_dev[i];
                  GT68xx_Model *model;
                  if (gt68xx_device_get_model (word, &model) == SANE_TRUE)
                    {
                      status = gt68xx_device_set_model (dev, model);
                      if (status != SANE_STATUS_GOOD)
                        DBG (1, "sane_init: couldn't override model: %s\n",
                             sane_strstatus (status));
                      else
                        DBG (5, "sane_init: new model set to %s\n",
                             dev->model->name);
                    }
                  else
                    {
                      DBG (1, "sane_init: override: model %s not found\n",
                           word);
                    }
                }
              if (i == 0)
                DBG (5, "sane_init: can't override model to %s, set device "
                     "first\n", word);
              free (word);
            }
          else
            {
              DBG (3, "sane_init: option `override' needs a parameter\n");
            }
        }
      else if (strcmp (word, "afe") == 0)
        {
          GT68xx_AFE_Parameters afe = {0, 0, 0, 0, 0, 0};
          SANE_Status status;

          free (word);
          word = 0;

          status = get_afe_values (cp, &afe);
          if (status == SANE_STATUS_GOOD)
            {
              int i;
              for (i = 0; i < new_dev_len; i++)
                {
                  new_dev[i]->model->afe_params = afe;
                  DBG (5, "sane_init: device %s: setting new afe values\n",
                       new_dev[i]->model->name);
                }
              if (i == 0)
                DBG (5,
                     "sane_init: can't set afe values, set device first\n");
            }
          else
            DBG (3, "sane_init: can't set afe values\n");
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
  return SANE_STATUS_GOOD;
}

/* -------------------------- SANE API functions ------------------------- */

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  SANE_Status status;

  DBG_INIT ();
#ifdef DBG_LEVEL
  if (DBG_LEVEL > 0)
    {
      DBG (5, "sane_init: debug options are enabled, handle with care\n");
      debug_options = SANE_TRUE;
    }
#endif
  DBG (2, "SANE GT68xx backend version %d.%d build %d from %s\n", SANE_CURRENT_MAJOR,
       V_MINOR, BUILD, PACKAGE_STRING);

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  DBG (5, "sane_init: authorize %s null\n", authorize ? "!=" : "==");

  sanei_usb_init ();

  num_devices = 0;
  first_dev = 0;
  first_handle = 0;
  devlist = 0;
  new_dev = 0;
  new_dev_len = 0;
  new_dev_alloced = 0;

  status = probe_gt68xx_devices ();
  DBG (5, "sane_init: exit\n");

  return status;
}

void
sane_exit (void)
{
  GT68xx_Device *dev, *next;

  DBG (5, "sane_exit: start\n");
  sanei_usb_exit();
  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      gt68xx_device_free (dev);
    }
  first_dev = 0;
  first_handle = 0;
  if (devlist)
    free (devlist);
  devlist = 0;

  DBG (5, "sane_exit: exit\n");
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  GT68xx_Device *dev;
  SANE_Int dev_num;

  DBG (5, "sane_get_devices: start: local_only = %s\n",
       local_only == SANE_TRUE ? "true" : "false");

  /* hot-plug case : detection of newly connected scanners */
  sanei_usb_scan_devices ();
  probe_gt68xx_devices ();

  if (devlist)
    free (devlist);

  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  dev_num = 0;
  dev = first_dev;
  while(dev!=NULL)
    {
      SANE_Device *sane_device;

      /* don't return devices that have been unplugged */
      if(dev->missing==SANE_FALSE)
        {
          sane_device = malloc (sizeof (*sane_device));
          if (!sane_device)
            return SANE_STATUS_NO_MEM;
          sane_device->name = dev->file_name;
          sane_device->vendor = dev->model->vendor;
          sane_device->model = dev->model->model;
          sane_device->type = strdup ("flatbed scanner");
          devlist[dev_num] = sane_device;
          dev_num++;
        }

      /* next device */
      dev = dev->next;
    }
  devlist[dev_num] = 0;

  *device_list = devlist;

  DBG (5, "sane_get_devices: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  GT68xx_Device *dev;
  SANE_Status status;
  GT68xx_Scanner *s;
  SANE_Bool power_ok;

  DBG (5, "sane_open: start (devicename = `%s')\n", devicename);

  if (devicename[0])
    {
      /* test for gt68xx short hand name */
      if(strcmp(devicename,"gt68xx")!=0)
        {
          for (dev = first_dev; dev; dev = dev->next)
            if (strcmp (dev->file_name, devicename) == 0)
              break;

          if (!dev)
            {
              DBG (5, "sane_open: couldn't find `%s' in devlist, trying attach\n",
                   devicename);
              RIE (attach (devicename, &dev, SANE_TRUE));
            }
          else
            DBG (5, "sane_open: found `%s' in devlist\n", dev->model->name);
        }
      else
        {
          dev = first_dev;
          if (dev)
            {
              devicename = dev->file_name;
              DBG (5, "sane_open: default empty devicename, using first device `%s'\n", devicename);
            }
        }
    }
  else
    {
      /* empty devicname -> use first device */
      dev = first_dev;
      if (dev)
        {
          devicename = dev->file_name;
          DBG (5, "sane_open: empty devicename, trying `%s'\n", devicename);
        }
    }

  if (!dev)
    return SANE_STATUS_INVAL;

  RIE (gt68xx_device_open (dev, devicename));
  RIE (gt68xx_device_activate (dev));

  if (dev->model->flags & GT68XX_FLAG_UNTESTED)
    {
      DBG (0, "WARNING: Your scanner is not fully supported or at least \n");
      DBG (0, "         had only limited testing. Please be careful and \n");
      DBG (0, "         report any failure/success to \n");
      DBG (0, "         sane-devel@lists.alioth.debian.org. Please provide as many\n");
      DBG (0, "         details as possible, e.g. the exact name of your\n");
      DBG (0, "         scanner and what does (not) work.\n");
    }

  if (dev->manual_selection)
    {
      DBG (0, "WARNING: You have manually added the ids of your scanner \n");
      DBG (0,
           "         to gt68xx.conf. Please use an appropriate override \n");
      DBG (0,
           "         for your scanner. Use extreme care and switch off \n");
      DBG (0,
           "         the scanner immediately if you hear unusual noise. \n");
      DBG (0, "         Please report any success to \n");
      DBG (0, "         sane-devel@lists.alioth.debian.org. Please provide as many\n");
      DBG (0, "         details as possible, e.g. the exact name of your\n");
      DBG (0, "         scanner, ids, settings etc.\n");

      if (strcmp (dev->model->name, "unknown-scanner") == 0)
        {
          GT68xx_USB_Device_Entry *entry;

          DBG (0,
               "ERROR: You haven't chosen an override in gt68xx.conf. Please use \n");
          DBG (0, "       one of the following: \n");

          for (entry = gt68xx_usb_device_list; entry->model; ++entry)
            {
              if (strcmp (entry->model->name, "unknown-scanner") != 0)
                DBG (0, "       %s\n", entry->model->name);
            }
          return SANE_STATUS_UNSUPPORTED;
        }
    }

  /* The firmware check is disabled by default because it may confuse
     some scanners: So the firmware is loaded everytime. */
#if 0
  RIE (gt68xx_device_check_firmware (dev, &firmware_loaded));
  firmware_loaded = SANE_FALSE;
  if (firmware_loaded)
    DBG (3, "sane_open: firmware already loaded, skipping load\n");
  else
    RIE (download_firmware_file (dev));
  /*  RIE (gt68xx_device_check_firmware (dev, &firmware_loaded)); */
  if (!firmware_loaded)
    {
      DBG (1, "sane_open: firmware still not loaded? Proceeding anyway\n");
      /* return SANE_STATUS_IO_ERROR; */
    }
#else
  RIE (download_firmware_file (dev));
#endif

  RIE (gt68xx_device_get_id (dev));

  if (!(dev->model->flags & GT68XX_FLAG_NO_STOP))
    RIE (gt68xx_device_stop_scan (dev));

  RIE (gt68xx_device_get_power_status (dev, &power_ok));
  if (power_ok)
    {
      DBG (5, "sane_open: power ok\n");
    }
  else
    {
      DBG (0, "sane_open: power control failure: check power plug!\n");
      return SANE_STATUS_IO_ERROR;
    }

  RIE (gt68xx_scanner_new (dev, &s));
  RIE (gt68xx_device_lamp_control (s->dev, SANE_TRUE, SANE_FALSE));
  gettimeofday (&s->lamp_on_time, 0);

  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;
  *handle = s;
  s->scanning = SANE_FALSE;
  s->first_scan = SANE_TRUE;
  s->gamma_table = 0;
  s->calibrated = SANE_FALSE;
  RIE (init_options (s));
  dev->gray_mode_color = 0x02;

  /* try to restore calibration from file */
  if((s->dev->model->flags & GT68XX_FLAG_HAS_CALIBRATE))
    {
      /* error restoring calibration is non blocking */
      gt68xx_read_calibration(s);
    }

  DBG (5, "sane_open: exit\n");

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  GT68xx_Scanner *prev, *s;
  GT68xx_Device *dev;

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
      return;                   /* oops, not a handle we know about */
    }

  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  if (s->val[OPT_LAMP_OFF_AT_EXIT].w == SANE_TRUE)
    gt68xx_device_lamp_control (s->dev, SANE_FALSE, SANE_FALSE);

  dev = s->dev;
  
  free (s->val[OPT_MODE].s);
  free (s->val[OPT_GRAY_MODE_COLOR].s);
  free (s->val[OPT_SOURCE].s);
  free (dev->file_name);
  free ((void *)(size_t)s->opt[OPT_RESOLUTION].constraint.word_list);

  gt68xx_scanner_free (s);
  
  gt68xx_device_fix_descriptor (dev);

  gt68xx_device_deactivate (dev);
  gt68xx_device_close (dev);

  DBG (5, "sane_close: exit\n");
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  GT68xx_Scanner *s = handle;

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
  GT68xx_Scanner *s = handle;
  SANE_Status status = SANE_STATUS_GOOD;
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
           "scanning (option = %s (%d))\n", s->opt[option].name, option);

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
        case OPT_BIT_DEPTH:
        case OPT_FULL_SCAN:
        case OPT_COARSE_CAL:
        case OPT_COARSE_CAL_ONCE:
        case OPT_QUALITY_CAL:
        case OPT_BACKTRACK:
        case OPT_BACKTRACK_LINES:
        case OPT_PREVIEW:
        case OPT_LAMP_OFF_AT_EXIT:
        case OPT_AUTO_WARMUP:
        case OPT_GAMMA_VALUE:
        case OPT_THRESHOLD:
        case OPT_TL_X:
        case OPT_TL_Y:
        case OPT_BR_X:
        case OPT_BR_Y:
          *(SANE_Word *) val = s->val[option].w;
          break;
          /* string options: */
        case OPT_MODE:
        case OPT_GRAY_MODE_COLOR:
        case OPT_SOURCE:
          strcpy (val, s->val[option].s);
          break;
        case OPT_NEED_CALIBRATION_SW:
          *(SANE_Bool *) val = !s->calibrated;
          break;
        case OPT_PAGE_LOADED_SW:
          s->dev->model->command_set->document_present (s->dev, val);
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
        case OPT_RESOLUTION:
        case OPT_BIT_DEPTH:
        case OPT_FULL_SCAN:
        case OPT_PREVIEW:
        case OPT_TL_X:
        case OPT_TL_Y:
        case OPT_BR_X:
        case OPT_BR_Y:
          s->val[option].w = *(SANE_Word *) val;
          RIE (calc_parameters (s));
          myinfo |= SANE_INFO_RELOAD_PARAMS;
          break;
        case OPT_LAMP_OFF_AT_EXIT:
        case OPT_AUTO_WARMUP:
        case OPT_COARSE_CAL_ONCE:
        case OPT_BACKTRACK_LINES:
        case OPT_QUALITY_CAL:
        case OPT_GAMMA_VALUE:
        case OPT_THRESHOLD:
          s->val[option].w = *(SANE_Word *) val;
          break;
        case OPT_GRAY_MODE_COLOR:
          if (strcmp (s->val[option].s, val) != 0)
            {                   /* something changed */
              if (s->val[option].s)
                free (s->val[option].s);
              s->val[option].s = strdup (val);
            }
          break;
        case OPT_SOURCE:
          if (strcmp (s->val[option].s, val) != 0)
            {                   /* something changed */
              if (s->val[option].s)
                free (s->val[option].s);
              s->val[option].s = strdup (val);
              if (strcmp (s->val[option].s, "Transparency Adapter") == 0)
                {
                  RIE (gt68xx_device_lamp_control
                       (s->dev, SANE_FALSE, SANE_TRUE));
                  x_range.max = s->dev->model->x_size_ta;
                  y_range.max = s->dev->model->y_size_ta;
                }
              else
                {
                  RIE (gt68xx_device_lamp_control
                       (s->dev, SANE_TRUE, SANE_FALSE));
                  x_range.max = s->dev->model->x_size;
                  y_range.max = s->dev->model->y_size;
                }
              s->first_scan = SANE_TRUE;
              myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
              gettimeofday (&s->lamp_on_time, 0);
            }
          break;
        case OPT_MODE:
          if (s->val[option].s)
            free (s->val[option].s);
          s->val[option].s = strdup (val);
          if (strcmp (s->val[option].s, SANE_VALUE_SCAN_MODE_LINEART) == 0)
            {
              ENABLE (OPT_THRESHOLD);
              DISABLE (OPT_BIT_DEPTH);
              ENABLE (OPT_GRAY_MODE_COLOR);
            }
          else
            {
              DISABLE (OPT_THRESHOLD);
              if (strcmp (s->val[option].s, SANE_VALUE_SCAN_MODE_GRAY) == 0)
                {
                  RIE (create_bpp_list (s, s->dev->model->bpp_gray_values));
                  ENABLE (OPT_GRAY_MODE_COLOR);
                }
              else
                {
                  RIE (create_bpp_list (s, s->dev->model->bpp_color_values));
                  DISABLE (OPT_GRAY_MODE_COLOR);
                }
              if (s->bpp_list[0] < 2)
                DISABLE (OPT_BIT_DEPTH);
              else
                ENABLE (OPT_BIT_DEPTH);
            }
          RIE (calc_parameters (s));
          myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          break;

        case OPT_COARSE_CAL:
          s->val[option].w = *(SANE_Word *) val;
          if (s->val[option].w == SANE_TRUE)
            {
              ENABLE (OPT_COARSE_CAL_ONCE);
              s->first_scan = SANE_TRUE;
            }
          else
            {
              DISABLE (OPT_COARSE_CAL_ONCE);
            }
          myinfo |= SANE_INFO_RELOAD_OPTIONS;
          break;

        case OPT_BACKTRACK:
          s->val[option].w = *(SANE_Word *) val;
          if (s->val[option].w == SANE_TRUE)
            ENABLE (OPT_BACKTRACK_LINES);
          else
            DISABLE (OPT_BACKTRACK_LINES);
          myinfo |= SANE_INFO_RELOAD_OPTIONS;
          break;

        case OPT_CALIBRATE:
          status = gt68xx_sheetfed_scanner_calibrate (s);
          myinfo |= SANE_INFO_RELOAD_OPTIONS;
          break;
    
        case OPT_CLEAR_CALIBRATION:
          gt68xx_clear_calibration (s);
          myinfo |= SANE_INFO_RELOAD_OPTIONS;
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
  return status;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  GT68xx_Scanner *s = handle;
  SANE_Status status;

  DBG (5, "sane_get_parameters: start\n");

  RIE (calc_parameters (s));
  if (params)
    *params = s->params;

  DBG (4, "sane_get_parameters: format=%d, last_frame=%d, lines=%d\n",
       s->params.format, s->params.last_frame, s->params.lines);
  DBG (4, "sane_get_parameters: pixels_per_line=%d, bytes per line=%d\n",
       s->params.pixels_per_line, s->params.bytes_per_line);
  DBG (3, "sane_get_parameters: pixels %dx%dx%d\n",
       s->params.pixels_per_line, s->params.lines, 1 << s->params.depth);

  DBG (5, "sane_get_parameters: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  GT68xx_Scanner *s = handle;
  GT68xx_Scan_Request scan_request;
  GT68xx_Scan_Parameters scan_params;
  SANE_Status status;
  SANE_Int i, gamma_size;
  unsigned int *buffer_pointers[3];
  SANE_Bool document;

  DBG (5, "sane_start: start\n");

  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */
  RIE (calc_parameters (s));

  if (s->val[OPT_TL_X].w >= s->val[OPT_BR_X].w)
    {
      DBG (0, "sane_start: top left x >= bottom right x --- exiting\n");
      return SANE_STATUS_INVAL;
    }
  if (s->val[OPT_TL_Y].w >= s->val[OPT_BR_Y].w)
    {
      DBG (0, "sane_start: top left y >= bottom right y --- exiting\n");
      return SANE_STATUS_INVAL;
    }

  if (strcmp (s->val[OPT_GRAY_MODE_COLOR].s, GT68XX_COLOR_BLUE) == 0)
    s->dev->gray_mode_color = 0x01;
  else if (strcmp (s->val[OPT_GRAY_MODE_COLOR].s, GT68XX_COLOR_GREEN) == 0)
    s->dev->gray_mode_color = 0x02;
  else
    s->dev->gray_mode_color = 0x03;

  setup_scan_request (s, &scan_request);
  if (!s->first_scan && s->val[OPT_COARSE_CAL_ONCE].w == SANE_TRUE)
    s->auto_afe = SANE_FALSE;
  else
    s->auto_afe = s->val[OPT_COARSE_CAL].w;

  s->dev->gamma_value = s->val[OPT_GAMMA_VALUE].w;
  gamma_size = s->params.depth == 16 ? 65536 : 256;
  s->gamma_table = malloc (sizeof (SANE_Int) * gamma_size);
  if (!s->gamma_table)
    {
      DBG (1, "sane_start: couldn't malloc %d bytes for gamma table\n",
           gamma_size);
      return SANE_STATUS_NO_MEM;
    }
  for (i = 0; i < gamma_size; i++)
    {
      s->gamma_table[i] =
        (gamma_size - 1) * pow (((double) i + 1) / (gamma_size),
                                1.0 / SANE_UNFIX (s->dev->gamma_value)) + 0.5;
      if (s->gamma_table[i] > (gamma_size - 1))
        s->gamma_table[i] = (gamma_size - 1);
      if (s->gamma_table[i] < 0)
        s->gamma_table[i] = 0;
#if 0
      printf ("%d %d\n", i, s->gamma_table[i]);
#endif
    }

  if(!(s->dev->model->flags & GT68XX_FLAG_HAS_CALIBRATE))
    {
      s->calib = s->val[OPT_QUALITY_CAL].w;
    }

  if (!(s->dev->model->flags & GT68XX_FLAG_NO_STOP))
    RIE (gt68xx_device_stop_scan (s->dev));

  if (!(s->dev->model->flags & GT68XX_FLAG_SHEET_FED))
    RIE (gt68xx_device_carriage_home (s->dev));

  gt68xx_scanner_wait_for_positioning (s);
  gettimeofday (&s->start_time, 0);

  if (s->val[OPT_BACKTRACK].w == SANE_TRUE)
    scan_request.backtrack = SANE_TRUE;
  else
    {
      if (s->val[OPT_RESOLUTION].w >= s->dev->model->ydpi_no_backtrack)
        scan_request.backtrack = SANE_FALSE;
      else
        scan_request.backtrack = SANE_TRUE;
    }

  if (scan_request.backtrack)
    scan_request.backtrack_lines = s->val[OPT_BACKTRACK_LINES].w;
  else
    scan_request.backtrack_lines = 0;

  /* don't call calibration for scanners that use sheetfed_calibrate */
  if(!(s->dev->model->flags & GT68XX_FLAG_HAS_CALIBRATE))
    {
      RIE (gt68xx_scanner_calibrate (s, &scan_request));
    }
  else
    {
      s->calib = s->calibrated;
    }

  /* is possible, wait for document to be inserted before scanning */
  /* wait for 5 secondes max */
  if (s->dev->model->flags & GT68XX_FLAG_SHEET_FED
   && s->dev->model->command_set->document_present)
    {
      i=0;
      do
        {
          RIE(s->dev->model->command_set->document_present(s->dev,&document));
          if(document==SANE_FALSE)
            {
              i++;
              sleep(1);
            }
        } while ((i<5) && (document==SANE_FALSE));
      if(document==SANE_FALSE)
        {
          DBG (4, "sane_start: no doucment detected after %d s\n",i);
          return SANE_STATUS_NO_DOCS;
        }
    }

  /* some sheetfed scanners need a special operation to move
   * paper before starting real scan */
  if (s->dev->model->flags & GT68XX_FLAG_SHEET_FED)
    {
      RIE (gt68xx_sheetfed_move_to_scan_area (s, &scan_request));
    }

  /* restore calibration */
  if(  (s->dev->model->flags & GT68XX_FLAG_HAS_CALIBRATE)
     &&(s->calibrated == SANE_TRUE))
    {
      /* compute scan parameters */
      scan_request.calculate = SANE_TRUE;
      gt68xx_device_setup_scan (s->dev, &scan_request, SA_SCAN, &scan_params); 

      /* restore settings from calibration stored */
      memcpy(s->dev->afe,&(s->afe_params), sizeof(GT68xx_AFE_Parameters));
      RIE (gt68xx_assign_calibration (s, scan_params));
      scan_request.calculate = SANE_FALSE;
    }
 
  /* send scan request to the scanner */
  RIE (gt68xx_scanner_start_scan (s, &scan_request, &scan_params));

  for (i = 0; i < scan_params.overscan_lines; ++i)
    RIE (gt68xx_scanner_read_line (s, buffer_pointers));
  DBG (4, "sane_start: wanted: dpi=%d, x=%.1f, y=%.1f, width=%.1f, "
       "height=%.1f, color=%s\n", scan_request.xdpi,
       SANE_UNFIX (scan_request.x0),
       SANE_UNFIX (scan_request.y0), SANE_UNFIX (scan_request.xs),
       SANE_UNFIX (scan_request.ys), scan_request.color ? "color" : "gray");

  s->line = 0;
  s->byte_count = s->reader->params.pixel_xs;
  s->total_bytes = 0;
  s->first_scan = SANE_FALSE;

#ifdef DEBUG_BRIGHTNESS
  s->average_white = 0;
  s->max_white = 0;
  s->min_black = 255;
#endif

  s->scanning = SANE_TRUE;

  DBG (5, "sane_start: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
           SANE_Int * len)
{
  GT68xx_Scanner *s = handle;
  SANE_Status status;
  static unsigned int *buffer_pointers[3];
  SANE_Int inflate_x;
  SANE_Bool lineart;
  SANE_Int i, color, colors;

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

  DBG (5, "sane_read: start (line %d of %d, byte_count %d of %d)\n",
       s->line, s->reader->params.pixel_ys, s->byte_count,
       s->reader->params.pixel_xs);

  if (s->line >= s->reader->params.pixel_ys
      && s->byte_count >= s->reader->params.pixel_xs)
    {
      DBG (4, "sane_read: nothing more to scan: EOF\n");
      return SANE_STATUS_EOF;
    }

  inflate_x = s->val[OPT_RESOLUTION].w / s->dev->model->optical_xdpi;
  if (inflate_x > 1)
    DBG (5, "sane_read: inflating x by factor %d\n", inflate_x);
  else
    inflate_x = 1;

  lineart = (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART) == 0)
    ? SANE_TRUE : SANE_FALSE;

  if (s->reader->params.color)
    colors = 3;
  else
    colors = 1;

  while ((*len) < max_len)
    {
      if (s->byte_count >= s->reader->params.pixel_xs)
        {
          if (s->line >= s->reader->params.pixel_ys)
            {
              DBG (4, "sane_read: scan complete: %d bytes, %d total\n",
                   *len, s->total_bytes);
              return SANE_STATUS_GOOD;
            }
          DBG (5, "sane_read: getting line %d of %d\n", s->line,
               s->reader->params.pixel_ys);
          RIE (gt68xx_scanner_read_line (s, buffer_pointers));
          s->line++;
          s->byte_count = 0;

          /* Apply gamma */
          for (color = 0; color < colors; color++)
            for (i = 0; i < s->reader->pixels_per_line; i++)
              {
                if (s->reader->params.depth > 8)
                  buffer_pointers[color][i] =
                    s->gamma_table[buffer_pointers[color][i]];
                else
                  buffer_pointers[color][i] =
                    (s->gamma_table[buffer_pointers[color][i] >> 8] << 8) +
                    (s->gamma_table[buffer_pointers[color][i] >> 8]);
              }
          /* mirror lines */
          if (s->dev->model->flags & GT68XX_FLAG_MIRROR_X)
            {
              unsigned int swap;

              for (color = 0; color < colors; color++)
                {
                  for (i = 0; i < s->reader->pixels_per_line / 2; i++)
                    {
                      swap = buffer_pointers[color][i];
                      buffer_pointers[color][i] =
                        buffer_pointers[color][s->reader->pixels_per_line -
                                               1 - i];
                      buffer_pointers[color][s->reader->pixels_per_line - 1 -
                                             i] = swap;
                    }
                }
            }
        }
      if (lineart)
        {
          SANE_Int bit;
          SANE_Byte threshold = s->val[OPT_THRESHOLD].w;

          buf[*len] = 0;
          for (bit = 7; bit >= 0; bit--)
            {
              SANE_Byte is_black =
                (((buffer_pointers[0][s->byte_count] >> 8) & 0xff) >
                 threshold) ? 0 : 1;
              buf[*len] |= (is_black << bit);
              if ((7 - bit) % inflate_x == (inflate_x - 1))
                s->byte_count++;
            }
        }
      else if (s->reader->params.color)
        {
          /* color */
          if (s->reader->params.depth > 8)
            {
              SANE_Int color = (s->total_bytes / 2) % 3;
              if ((s->total_bytes % 2) == 0)
                {
                  if (little_endian)
                    buf[*len] = buffer_pointers[color][s->byte_count] & 0xff;
                  else
                    buf[*len] =
                      (buffer_pointers[color][s->byte_count] >> 8) & 0xff;
                }
              else
                {
                  if (little_endian)
                    buf[*len] =
                      (buffer_pointers[color][s->byte_count] >> 8) & 0xff;
                  else
                    buf[*len] = buffer_pointers[color][s->byte_count] & 0xff;

                  if (s->total_bytes % (inflate_x * 6) == (inflate_x * 6 - 1))
                    s->byte_count++;
                }
            }
          else
            {
              SANE_Int color = s->total_bytes % 3;
              buf[*len] = (buffer_pointers[color][s->byte_count] >> 8) & 0xff;
              if (s->total_bytes % (inflate_x * 3) == (inflate_x * 3 - 1))
                s->byte_count++;
#ifdef DEBUG_BRIGHTNESS
              s->average_white += buf[*len];
              s->max_white =
                (buf[*len] > s->max_white) ? buf[*len] : s->max_white;
              s->min_black =
                (buf[*len] < s->min_black) ? buf[*len] : s->min_black;
#endif
            }
        }
      else
        {
          /* gray */
          if (s->reader->params.depth > 8)
            {
              if ((s->total_bytes % 2) == 0)
                {
                  if (little_endian)
                    buf[*len] = buffer_pointers[0][s->byte_count] & 0xff;
                  else
                    buf[*len] =
                      (buffer_pointers[0][s->byte_count] >> 8) & 0xff;
                }
              else
                {
                  if (little_endian)
                    buf[*len] =
                      (buffer_pointers[0][s->byte_count] >> 8) & 0xff;
                  else
                    buf[*len] = buffer_pointers[0][s->byte_count] & 0xff;
                  if (s->total_bytes % (2 * inflate_x) == (2 * inflate_x - 1))
                    s->byte_count++;
                }
            }
          else
            {
              buf[*len] = (buffer_pointers[0][s->byte_count] >> 8) & 0xff;
              if (s->total_bytes % inflate_x == (inflate_x - 1))
                s->byte_count++;
            }
        }
      (*len)++;
      s->total_bytes++;
    }

  DBG (4, "sane_read: exit (line %d of %d, byte_count %d of %d, %d bytes, "
       "%d total)\n",
       s->line, s->reader->params.pixel_ys, s->byte_count,
       s->reader->params.pixel_xs, *len, s->total_bytes);
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  GT68xx_Scanner *s = handle;

  DBG (5, "sane_cancel: start\n");

  if (s->scanning)
    {
      s->scanning = SANE_FALSE;
      if (s->total_bytes != (s->params.bytes_per_line * s->params.lines))
        DBG (1, "sane_cancel: warning: scanned %d bytes, expected %d "
             "bytes\n", s->total_bytes,
             s->params.bytes_per_line * s->params.lines);
      else
        {
          struct timeval now;
          int secs;

          gettimeofday (&now, 0);
          secs = now.tv_sec - s->start_time.tv_sec;

          DBG (3,
               "sane_cancel: scan finished, scanned %d bytes in %d seconds\n",
               s->total_bytes, secs);
#ifdef DEBUG_BRIGHTNESS
          DBG (1,
               "sane_cancel: average white: %d, max_white=%d, min_black=%d\n",
               s->average_white / s->total_bytes, s->max_white, s->min_black);
#endif

        }
      /* some scanners don't like this command when cancelling a scan */
      sanei_usb_set_timeout (SHORT_TIMEOUT);
      gt68xx_device_fix_descriptor (s->dev);
      gt68xx_scanner_stop_scan (s);
      sanei_usb_set_timeout (LONG_TIMEOUT);

      if (s->dev->model->flags & GT68XX_FLAG_SHEET_FED)
        {
          gt68xx_device_paperfeed (s->dev);
        } 
      else
        {
          sanei_usb_set_timeout (SHORT_TIMEOUT);
          gt68xx_scanner_wait_for_positioning (s);
          sanei_usb_set_timeout (LONG_TIMEOUT);
          gt68xx_device_carriage_home (s->dev);
        }
      if (s->gamma_table)
        free (s->gamma_table);
      s->gamma_table = 0;
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
  GT68xx_Scanner *s = handle;

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
  GT68xx_Scanner *s = handle;

  DBG (5, "sane_get_select_fd: handle = %p, fd = %p\n", handle, (void *) fd);

  if (!s->scanning)
    {
      DBG (1, "sane_get_select_fd: not scanning\n");
      return SANE_STATUS_INVAL;
    }
  return SANE_STATUS_UNSUPPORTED;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
