/* sane - Scanner Access Now Easy.

   Copyright (C) 2000-2005 Mustek.
   Originally maintained by Mustek

   Copyright (C) 2001-2005 by Henning Meier-Geinitz.

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

   This file implements a SANE backend for the Mustek BearPaw 2448 TA Pro 
   and similar USB2 scanners. */

#ifndef MUSTEK_USB2_H
#define MUSTEK_USB2_H

#ifndef SANE_I18N
#define SANE_I18N(text) text
#endif

#define ENABLE(OPTION)  s->opt[OPTION].cap &= ~SANE_CAP_INACTIVE
#define DISABLE(OPTION) s->opt[OPTION].cap |=  SANE_CAP_INACTIVE
#define IS_ACTIVE(OPTION) (((s->opt[OPTION].cap) & SANE_CAP_INACTIVE) == 0)
/* RIE: return if error */
#define RIE(function) do {status = function; if (status != SANE_STATUS_GOOD) \
return status;} while (SANE_FALSE)

#define SCAN_BUFFER_SIZE (64 * 1024)
#define MAX_RESOLUTIONS 12
#define DEF_LINEARTTHRESHOLD 128
#define PER_ADD_START_LINES 0
#define PRE_ADD_START_X 0


enum Mustek_Usb_Option
{
  OPT_NUM_OPTS = 0,
  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_SOURCE,
  OPT_RESOLUTION,
  OPT_PREVIEW,

  OPT_DEBUG_GROUP,
  OPT_AUTO_WARMUP,

  OPT_ENHANCEMENT_GROUP,
  OPT_THRESHOLD,
  OPT_GAMMA_VALUE,

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */
  /* must come last: */
  NUM_OPTIONS
};


typedef struct Scanner_Model
{
  /** @name Identification */
  /*@{ */

  /** A single lowercase word to be used in the configuration file. */
  SANE_String_Const name;

  /** Device vendor string. */
  SANE_String_Const vendor;

  /** Device model name. */
  SANE_String_Const model;

  /** Name of the firmware file. */
  SANE_String_Const firmware_name;

  /** @name Scanner model parameters */
  /*@{ */

  SANE_Int dpi_values[MAX_RESOLUTIONS];	/* possible resolutions */
  SANE_Fixed x_offset;		/* Start of scan area in mm */
  SANE_Fixed y_offset;		/* Start of scan area in mm */
  SANE_Fixed x_size;		/* Size of scan area in mm */
  SANE_Fixed y_size;		/* Size of scan area in mm */

  SANE_Fixed x_offset_ta;	/* Start of scan area in TA mode in mm */
  SANE_Fixed y_offset_ta;	/* Start of scan area in TA mode in mm */
  SANE_Fixed x_size_ta;		/* Size of scan area in TA mode in mm */
  SANE_Fixed y_size_ta;		/* Size of scan area in TA mode in mm */


  RGBORDER line_mode_color_order;	/* Order of the CCD/CIS colors */
  SANE_Fixed default_gamma_value;	/* Default gamma value */

  SANE_Bool is_cis;		/* Is this a CIS or CCD scanner? */

  SANE_Word flags;		/* Which hacks are needed for this scanner? */
  /*@} */
} Scanner_Model;

typedef struct Mustek_Scanner
{
  /* all the state needed to define a scan request: */
  struct Mustek_Scanner *next;

  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];
  unsigned short *gamma_table;
  SANE_Parameters params;   /**< SANE Parameters */
  Scanner_Model model;
  SETPARAMETERS setpara;
  GETPARAMETERS getpara;
  SANE_Bool bIsScanning;
  SANE_Bool bIsReading;
  SANE_Word read_rows;		/* transfer image's lines */
  SANE_Byte *Scan_data_buf;	/*store Scanned data for transfer */
  SANE_Byte *Scan_data_buf_start;	/*point to data need to transfer */
  size_t scan_buffer_len;	/* length of data buf */
}
Mustek_Scanner;

#endif
