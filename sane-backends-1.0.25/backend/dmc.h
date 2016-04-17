/* sane - Scanner Access Now Easy.
   Copyright (C) 1998 David F. Skoll
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

/* $Id$ */

#ifndef polaroid_dmc_h
#define polaroid_dmc_h

#include <sys/types.h>

#define BYTES_PER_RAW_LINE 1599

typedef enum {
    OPT_NUM_OPTS = 0,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_MODE_GROUP,		/* Image acquisition mode */
    OPT_IMAGE_MODE,		/* Thumbnail, center cut or MFI'd image */
    OPT_ASA,			/* ASA Settings */
    OPT_SHUTTER_SPEED,		/* Shutter speed */
    OPT_WHITE_BALANCE,		/* White balance */

    /* must come last: */
    NUM_OPTIONS
} DMC_Option;

typedef struct DMC_Device {
    struct DMC_Device *next;
    SANE_Device sane;
    SANE_Range shutterSpeedRange;
    unsigned int shutterSpeed;
    int asa;
    int whiteBalance;
} DMC_Device;

typedef struct DMC_Camera {
    /* all the state needed to define a scan request: */
    struct DMC_Camera *next;

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];

    SANE_Parameters params;
    size_t bytes_to_read;

    SANE_Range tl_x_range;
    SANE_Range tl_y_range;
    SANE_Range br_x_range;
    SANE_Range br_y_range;

    int imageMode;

    /* The DMC needs certain reads to be done in one chunk, meaning
       we might have to buffer them. */
    char *readBuffer;
    char *readPtr;
    int inViewfinderMode;
    int fd;			/* SCSI filedescriptor */
    SANE_Byte currentRawLine[BYTES_PER_RAW_LINE];
    SANE_Byte nextRawLine[BYTES_PER_RAW_LINE];
    int nextRawLineValid;

    /* scanner dependent/low-level state: */
    DMC_Device *hw;
} DMC_Camera;

/* We only support the following four imaging modes */
#define IMAGE_MFI        0x0000 /* 801x600 filtered image   */
#define IMAGE_VIEWFINDER 0x0001 /* 270x201 viewfinder image */
#define IMAGE_RAW        0x0002 /* 1599x600 raw image       */
#define IMAGE_THUMB      0x0003 /* 80x60 thumbnail image    */
#define IMAGE_SUPER_RES  0x0004
#define NUM_IMAGE_MODES  5

#define ASA_25  0
#define ASA_50  1
#define ASA_100 2

#define WHITE_BALANCE_DAYLIGHT 0
#define WHITE_BALANCE_INCANDESCENT 1
#define WHITE_BALANCE_FLUORESCENT 2

#endif /* polaroid_dmc_h */
