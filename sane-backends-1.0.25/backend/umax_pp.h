/* sane - Scanner Access Now Easy.
   Copyright (C) 2001-2012 Stéphane Voltz <stef.dev@free.fr>
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

   This file implements a SANE backend for Umax PP flatbed scanners.  */

#ifndef umax_pp_h
#define umax_pp_h

#include <sys/types.h>
#include <sys/time.h>
#include <../include/sane/sanei_debug.h>


enum Umax_PP_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_RESOLUTION,
  OPT_PREVIEW,
  OPT_GRAY_PREVIEW,

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  OPT_ENHANCEMENT_GROUP,

  OPT_LAMP_CONTROL,
  OPT_UTA_CONTROL,

  OPT_CUSTOM_GAMMA,		/* use custom gamma tables? */
  /* The gamma vectors MUST appear in the order gray, red, green,
     blue.  */
  OPT_GAMMA_VECTOR,
  OPT_GAMMA_VECTOR_R,
  OPT_GAMMA_VECTOR_G,
  OPT_GAMMA_VECTOR_B,

  OPT_MANUAL_GAIN,
  OPT_GRAY_GAIN,
  OPT_RED_GAIN,
  OPT_GREEN_GAIN,
  OPT_BLUE_GAIN,

  OPT_MANUAL_OFFSET,
  OPT_GRAY_OFFSET,
  OPT_RED_OFFSET,
  OPT_GREEN_OFFSET,
  OPT_BLUE_OFFSET,

  /* must come last: */
  NUM_OPTIONS
};


typedef struct Umax_PP_Descriptor
{
  SANE_Device sane;

  SANE_String port;
  SANE_String ppdevice;

  SANE_Int max_res;
  SANE_Int ccd_res;
  SANE_Int max_h_size;
  SANE_Int max_v_size;
  long int buf_size;
  u_char revision;

  /* default values */
  SANE_Int gray_gain;
  SANE_Int red_gain;
  SANE_Int blue_gain;
  SANE_Int green_gain;
  SANE_Int gray_offset;
  SANE_Int red_offset;
  SANE_Int blue_offset;
  SANE_Int green_offset;
}
Umax_PP_Descriptor;

typedef struct Umax_PP_Device
{
  struct Umax_PP_Device *next;
  Umax_PP_Descriptor *desc;


  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];

  SANE_Int gamma_table[4][256];

  int state;
  int mode;

  int TopX;
  int TopY;
  int BottomX;
  int BottomY;

  int dpi;
  int gain;
  int color;
  int bpp;			/* bytes per pixel */
  int tw;			/* target width in pixels */
  int th;			/* target height in pixels */



  SANE_Byte *calibration;

  SANE_Byte *buf;
  long int bufsize;		/* size of read buffer                 */
  long int buflen;		/* size of data length in buffer       */
  long int bufread;		/* number of bytes read in the buffer  */
  long int read;		/* bytes read from previous start scan */

  SANE_Parameters params;
  SANE_Range dpi_range;
  SANE_Range x_range;
  SANE_Range y_range;

  SANE_Int gray_gain;
  SANE_Int red_gain;
  SANE_Int blue_gain;
  SANE_Int green_gain;

  SANE_Int gray_offset;
  SANE_Int red_offset;
  SANE_Int blue_offset;
  SANE_Int green_offset;
}
Umax_PP_Device;


/**
 * enumeration of configuration options
 */
enum Umax_PP_Configure_Option
{
  CFG_BUFFER = 0,
  CFG_RED_GAIN,
  CFG_GREEN_GAIN,
  CFG_BLUE_GAIN,
  CFG_RED_OFFSET,
  CFG_GREEN_OFFSET,
  CFG_BLUE_OFFSET,
  CFG_VENDOR,
  CFG_NAME,
  CFG_MODEL,
  CFG_ASTRA,
  NUM_CFG_OPTIONS
};

#if (!defined __GNUC__ || __GNUC__ < 2 || \
     __GNUC_MINOR__ < (defined __cplusplus ? 6 : 4))

#define __PRETTY_FUNCTION__	"umax_pp"

#endif

#define DEBUG()		DBG(4, "%s(v%d.%d.%d-%s): line %d: debug exception\n", \
			  __PRETTY_FUNCTION__, SANE_CURRENT_MAJOR, V_MINOR,	\
			  UMAX_PP_BUILD, UMAX_PP_STATE, __LINE__)

#endif /* umax_pp_h */
