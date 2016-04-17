/* sane - Scanner Access Now Easy.

   Copyright (C) 2003, 2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2005-2013 Stephane Voltz <stef.dev@free.fr>
   Copyright (C) 2006 Laurent Charpentier <laurent_pubs@yahoo.com>
   Copyright (C) 2009 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>

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

#ifndef GENESYS_H
#define GENESYS_H

#include "genesys_low.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#if defined(_WIN32) || defined(HAVE_OS2_H)
# define PATH_SEP	'\\'
#else
# define PATH_SEP	'/'
#endif


#define ENABLE(OPTION)  s->opt[OPTION].cap &= ~SANE_CAP_INACTIVE
#define DISABLE(OPTION) s->opt[OPTION].cap |=  SANE_CAP_INACTIVE
#define IS_ACTIVE(OPTION) (((s->opt[OPTION].cap) & SANE_CAP_INACTIVE) == 0)

#define GENESYS_CONFIG_FILE "genesys.conf"

/* Maximum time for lamp warm-up */
#define WARMUP_TIME 65

#define FLATBED "Flatbed"
#define TRANSPARENCY_ADAPTER "Transparency Adapter"

#ifndef SANE_I18N
#define SANE_I18N(text) text
#endif

/** List of SANE options
 */
enum Genesys_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_SOURCE,
  OPT_PREVIEW,
  OPT_BIT_DEPTH,
  OPT_RESOLUTION,

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  /* advanced image enhancement options */
  OPT_ENHANCEMENT_GROUP,
  OPT_CUSTOM_GAMMA,		/* toggle to enable custom gamma tables */
  OPT_GAMMA_VECTOR,
  OPT_GAMMA_VECTOR_R,
  OPT_GAMMA_VECTOR_G,
  OPT_GAMMA_VECTOR_B,
  OPT_SWDESKEW,
  OPT_SWCROP,
  OPT_SWDESPECK,
  OPT_DESPECK,
  OPT_SWSKIP,
  OPT_SWDEROTATE,
  OPT_BRIGHTNESS,
  OPT_CONTRAST,

  OPT_EXTRAS_GROUP,
  OPT_LAMP_OFF_TIME,
  OPT_LAMP_OFF,
  OPT_THRESHOLD,
  OPT_THRESHOLD_CURVE,
  OPT_DISABLE_DYNAMIC_LINEART,
  OPT_DISABLE_INTERPOLATION,
  OPT_COLOR_FILTER,
  OPT_CALIBRATION_FILE,
  OPT_EXPIRATION_TIME,

  OPT_SENSOR_GROUP,
  OPT_SCAN_SW,
  OPT_FILE_SW,
  OPT_EMAIL_SW,
  OPT_COPY_SW,
  OPT_PAGE_LOADED_SW,
  OPT_OCR_SW,
  OPT_POWER_SW,
  OPT_EXTRA_SW,
  OPT_NEED_CALIBRATION_SW,
  OPT_BUTTON_GROUP,
  OPT_CALIBRATE,
  OPT_CLEAR_CALIBRATION,

  /* must come last: */
  NUM_OPTIONS
};


/** Scanner object. Should have better be called Session than Scanner
 */
typedef struct Genesys_Scanner
{
  struct Genesys_Scanner *next;	    /**< Next scanner in list */
  Genesys_Device *dev;		    /**< Low-level device object */

  /* SANE data */
  SANE_Bool scanning;			   /**< We are currently scanning */
  SANE_Option_Descriptor opt[NUM_OPTIONS]; /**< Option descriptors */
  Option_Value val[NUM_OPTIONS];	   /**< Option values */
  Option_Value last_val[NUM_OPTIONS];	   /**< Option values as read by the frontend. used for sensors. */
  SANE_Parameters params;		   /**< SANE Parameters */
  SANE_Int bpp_list[5];			   /**< */
} Genesys_Scanner;

#ifdef UNIT_TESTING
SANE_Status genesys_dark_white_shading_calibration (Genesys_Device * dev);
char *calibration_filename(Genesys_Device *currdev);
void add_device(Genesys_Device *dev);
#endif
#endif /* not GENESYS_H */
