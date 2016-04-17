/************************************************************************
   lexmark.h - SANE library for Lexmark scanners.
   Copyright (C) 2003-2004 Lexmark International, Inc. (original source)
   Copyright (C) 2005 Fred Odendaal
   Copyright (C) 2006-2010 Stéphane Voltz	<stef.dev@free.fr>
   Copyright (C) 2010 "Torsten Houwaart" <ToHo@gmx.de> X74 support

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
   **************************************************************************/
#ifndef LEXMARK_H
#define LEXMARK_H

#undef DEEP_DEBUG

#include "../include/sane/config.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/time.h>

#include "../include/_stdint.h"
#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_backend.h"

typedef enum
{
  OPT_NUM_OPTS = 0,
  OPT_MODE,
  OPT_RESOLUTION,
  OPT_PREVIEW,
  OPT_THRESHOLD,

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  /* manual gain handling */
  OPT_MANUAL_GAIN,		/* 6 */
  OPT_GRAY_GAIN,
  OPT_RED_GAIN,
  OPT_GREEN_GAIN,
  OPT_BLUE_GAIN,

  /* must come last: */
  NUM_OPTIONS
}
Lexmark_Options;

/*
 * this struct is used to described the specific parts of each model
 */
typedef struct Lexmark_Model
{
  SANE_Int vendor_id;
  SANE_Int product_id;
  SANE_Byte mainboard_id;	/* matched against the content of reg B0 */
  SANE_String_Const name;
  SANE_String_Const vendor;
  SANE_String_Const model;
  SANE_Int motor_type;
  SANE_Int sensor_type;
  SANE_Int HomeEdgePoint1;
  SANE_Int HomeEdgePoint2;
} Lexmark_Model;

/*
 * this struct is used to store per sensor model constants
 */
typedef struct Lexmark_Sensor
{
  SANE_Int id;
  SANE_Int offset_startx;	/* starting x for offset calibration */
  SANE_Int offset_endx;		/* end x for offset calibration */
  SANE_Int offset_threshold;	/* target threshold for offset calibration */
  SANE_Int xoffset;		/* number of unusable pixels on the start of the sensor */
  SANE_Int default_gain;	/* value of the default gain for a scan */
  SANE_Int red_gain_target;
  SANE_Int green_gain_target;
  SANE_Int blue_gain_target;
  SANE_Int gray_gain_target;
  SANE_Int red_shading_target;
  SANE_Int green_shading_target;
  SANE_Int blue_shading_target;
  SANE_Int gray_shading_target;
  SANE_Int offset_fallback;	/* offset to use in case offset calibration fails */
  SANE_Int gain_fallback;	/* gain to use in case offset calibration fails */
} Lexmark_Sensor;

typedef enum
{
  RED = 0,
  GREEN,
  BLUE
}
Scan_Regions;

/* struct to hold pre channel settings */
typedef struct Channels
{
  SANE_Word red;
  SANE_Word green;
  SANE_Word blue;
  SANE_Word gray;
}
Channels;

/** @name Option_Value union
 * convenience union to access option values given to the backend
 * @{
 */
#ifndef SANE_OPTION
#define SANE_OPTION 1
typedef union
{
  SANE_Bool b;
  SANE_Word w;
  SANE_Word *wa;		/* word array */
  SANE_String s;
}
Option_Value;
#endif
/* @} */

typedef struct Read_Buffer
{
  SANE_Int gray_offset;
  SANE_Int max_gray_offset;
  SANE_Int region;
  SANE_Int red_offset;
  SANE_Int green_offset;
  SANE_Int blue_offset;
  SANE_Int max_red_offset;
  SANE_Int max_green_offset;
  SANE_Int max_blue_offset;
  SANE_Byte *data;
  SANE_Byte *readptr;
  SANE_Byte *writeptr;
  SANE_Byte *max_writeptr;
  size_t size;
  size_t linesize;
  SANE_Bool empty;
  SANE_Int image_line_no;
  SANE_Int bit_counter;
  SANE_Int max_lineart_offset;
}
Read_Buffer;

typedef struct Lexmark_Device
{
  struct Lexmark_Device *next;
  SANE_Bool missing;	/**< devices has been unplugged or swtiched off */

  SANE_Device sane;
  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];
  SANE_Parameters params;
  SANE_Int devnum;
  long data_size;
  SANE_Bool initialized;
  SANE_Bool eof;
  SANE_Int x_dpi;
  SANE_Int y_dpi;
  long data_ctr;
  SANE_Bool device_cancelled;
  SANE_Int cancel_ctr;
  SANE_Byte *transfer_buffer;
  size_t bytes_read;
  size_t bytes_remaining;
  size_t bytes_in_buffer;
  SANE_Byte *read_pointer;
  Read_Buffer *read_buffer;
  SANE_Byte threshold;

  Lexmark_Model model;		/* per model data */
  Lexmark_Sensor *sensor;
  SANE_Byte shadow_regs[255];	/* shadow registers */
  struct Channels offset;
  struct Channels gain;
  float *shading_coeff;
}
Lexmark_Device;

/* Maximum transfer size */
#define MAX_XFER_SIZE 0xFFC0

/* motors and sensors type defines */
#define X1100_MOTOR	1
#define A920_MOTOR	2
#define X74_MOTOR	3

#define X1100_B2_SENSOR 4
#define A920_SENSOR     5
#define X1100_2C_SENSOR 6
#define X1200_SENSOR    7	/* X1200 on USB 1.0 */
#define X1200_USB2_SENSOR 8	/* X1200 on USB 2.0 */
#define X74_SENSOR	9

/* Non-static Function Proto-types (called by lexmark.c) */
SANE_Status sanei_lexmark_low_init (Lexmark_Device * dev);
void sanei_lexmark_low_destroy (Lexmark_Device * dev);
SANE_Status sanei_lexmark_low_open_device (Lexmark_Device * dev);
void sanei_lexmark_low_close_device (Lexmark_Device * dev);
SANE_Bool sanei_lexmark_low_search_home_fwd (Lexmark_Device * dev);
void sanei_lexmark_low_move_fwd (SANE_Int distance, Lexmark_Device * dev,
				 SANE_Byte * regs);
SANE_Bool sanei_lexmark_low_X74_search_home (Lexmark_Device * dev,
					     SANE_Byte * regs);
SANE_Bool sanei_lexmark_low_search_home_bwd (Lexmark_Device * dev);
SANE_Int sanei_lexmark_low_find_start_line (Lexmark_Device * dev);
SANE_Status sanei_lexmark_low_set_scan_regs (Lexmark_Device * dev,
					     SANE_Int resolution,
					     SANE_Int offset,
					     SANE_Bool calibrated);
SANE_Status sanei_lexmark_low_start_scan (Lexmark_Device * dev);
long sanei_lexmark_low_read_scan_data (SANE_Byte * data, SANE_Int size,
				       Lexmark_Device * dev);
SANE_Status sanei_lexmark_low_assign_model (Lexmark_Device * dev,
					    SANE_String_Const devname,
					    SANE_Int vendor, SANE_Int product,
					    SANE_Byte mainboard);

/*
 * scanner calibration functions
 */
SANE_Status sanei_lexmark_low_offset_calibration (Lexmark_Device * dev);
SANE_Status sanei_lexmark_low_gain_calibration (Lexmark_Device * dev);
SANE_Status sanei_lexmark_low_shading_calibration (Lexmark_Device * dev);
SANE_Status sanei_lexmark_low_calibration (Lexmark_Device * dev);

#endif /* LEXMARK_H */
