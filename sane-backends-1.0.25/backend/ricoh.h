/* sane - Scanner Access Now Easy.
   Copyright (C) 1998, Feico W. Dillema
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

/*
	$Id$
*/

#ifndef ricoh_h
#define ricoh_h 1

#include <sys/types.h>

#include "../include/sane/config.h"

/* defines for scan_image_mode field */
#define RICOH_BINARY_MONOCHROME   0
#define RICOH_DITHERED_MONOCHROME 1
#define RICOH_GRAYSCALE           2

/* sizes for mode parameter's base_measurement_unit */
#define INCHES                    0
#define MILLIMETERS               1
#define POINTS                    2
#define DEFAULT_MUD               1200
#define MEASUREMENTS_PAGE         (SANE_Byte)(0x03)

/* Mode Page Control */
#define PC_CURRENT 0x00
#define PC_CHANGE  0x40
#define PC_DEFAULT 0x80
#define PC_SAVED   0xc0

static const SANE_String_Const mode_list[] =
  {
    SANE_VALUE_SCAN_MODE_LINEART,
    SANE_VALUE_SCAN_MODE_HALFTONE,
    SANE_VALUE_SCAN_MODE_GRAY,
    0
  };

static const SANE_Range u8_range =
  {
      0,				/* minimum */
    255,				/* maximum */
      0				        /* quantization */
  };
static const SANE_Range is50_res_range =
  {
     75,                                /* minimum */
    400,                                /* maximum */
      0                                 /* quantization */
  };

static const SANE_Range is60_res_range =
  {
    100,				/* minimum */
    600,				/* maximum */
      0				        /* quantization */
  };

static const SANE_Range default_x_range =
  {
    0,				        /* minimum */
    (SANE_Word) (8 * DEFAULT_MUD),	/* maximum */
    2				        /* quantization */
  };

static const SANE_Range default_y_range =
  {
    0,				        /* minimum */
    (SANE_Word) (14 * DEFAULT_MUD),	/* maximum */
    2				        /* quantization */
  };



static inline void
_lto2b(SANE_Int val, SANE_Byte *bytes)
{

        bytes[0] = (val >> 8) & 0xff;
        bytes[1] = val & 0xff;
}

static inline void
_lto3b(SANE_Int val, SANE_Byte *bytes)
{

        bytes[0] = (val >> 16) & 0xff;
        bytes[1] = (val >> 8) & 0xff;
        bytes[2] = val & 0xff;
}

static inline void
_lto4b(SANE_Int val, SANE_Byte *bytes)
{

        bytes[0] = (val >> 24) & 0xff;
        bytes[1] = (val >> 16) & 0xff;
        bytes[2] = (val >> 8) & 0xff;
        bytes[3] = val & 0xff;
}

static inline SANE_Int
_2btol(SANE_Byte *bytes)
{
        SANE_Int rv;

        rv = (bytes[0] << 8) |
             bytes[1];
        return (rv);
}

static inline SANE_Int
_3btol(SANE_Byte *bytes)
{
        SANE_Int rv;

        rv = (bytes[0] << 16) |
             (bytes[1] << 8) |
             bytes[2];
        return (rv);
}

static inline SANE_Int
_4btol(SANE_Byte *bytes)
{
        SANE_Int rv;

        rv = (bytes[0] << 24) |
             (bytes[1] << 16) |
             (bytes[2] << 8) |
             bytes[3];
        return (rv);
}

typedef enum 
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    OPT_MODE,
    OPT_X_RESOLUTION,
    OPT_Y_RESOLUTION,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_ENHANCEMENT_GROUP,
    OPT_BRIGHTNESS,
    OPT_CONTRAST,

    /* must come last: */
    NUM_OPTIONS
  }
Ricoh_Option;

typedef struct Ricoh_Info
  {
    SANE_Range xres_range;
    SANE_Range yres_range;
    SANE_Range x_range;
    SANE_Range y_range;
    SANE_Range brightness_range;
    SANE_Range contrast_range;

    SANE_Int xres_default;
    SANE_Int yres_default;
    SANE_Int image_mode_default;
    SANE_Int brightness_default;
    SANE_Int contrast_default;

    SANE_Int bmu;
    SANE_Int mud;
  }
Ricoh_Info;

typedef struct Ricoh_Device
  {
    struct Ricoh_Device *next;
    SANE_Device sane;
    Ricoh_Info info;
  }
Ricoh_Device;

typedef struct Ricoh_Scanner
  {
    /* all the state needed to define a scan request: */
    struct Ricoh_Scanner *next;
    int fd;			/* SCSI filedescriptor */

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];
    SANE_Parameters params;
    /* scanner dependent/low-level state: */
    Ricoh_Device *hw;

    SANE_Int xres;
    SANE_Int yres;
    SANE_Int ulx;
    SANE_Int uly;
    SANE_Int width;
    SANE_Int length;
    SANE_Int brightness;
    SANE_Int contrast;
    SANE_Int image_composition;
    SANE_Int bpp;
    SANE_Bool reverse;

    size_t bytes_to_read;
    int scanning;
  }
Ricoh_Scanner;

struct inquiry_data {
        SANE_Byte devtype;
        SANE_Byte byte2;
        SANE_Byte byte3;
        SANE_Byte byte4;
        SANE_Byte byte5;
        SANE_Byte res1[2];
        SANE_Byte flags;
        SANE_Byte vendor[8];
        SANE_Byte product[8];
        SANE_Byte revision[4];
        SANE_Byte byte[60];
};

#define RICOH_WINDOW_DATA_SIZE 328
struct ricoh_window_data {
        /* header */
        SANE_Byte reserved[6];
        SANE_Byte len[2];
        /* data */
        SANE_Byte window_id;         /* must be zero */
        SANE_Byte auto_bit;
        SANE_Byte x_res[2];
        SANE_Byte y_res[2];
        SANE_Byte x_org[4];
        SANE_Byte y_org[4];
        SANE_Byte width[4];
        SANE_Byte length[4];
        SANE_Byte brightness;
        SANE_Byte threshold;
        SANE_Byte contrast;
        SANE_Byte image_comp;        /* image composition (data type) */
        SANE_Byte bits_per_pixel;
        SANE_Byte halftone_pattern[2];
        SANE_Byte pad_type;
        SANE_Byte bit_ordering[2];
        SANE_Byte compression_type;
        SANE_Byte compression_arg;
        SANE_Byte res3[6];

        /* Vendor Specific parameter byte(s) */
        /* Ricoh specific, follow the scsi2 standard ones */
        SANE_Byte byte1;
        SANE_Byte byte2;
        SANE_Byte mrif_filtering_gamma_id;
        SANE_Byte byte3;
        SANE_Byte byte4;
        SANE_Byte binary_filter;
        SANE_Byte reserved2[18];

        SANE_Byte reserved3[256];
};

struct measurements_units_page {
        SANE_Byte page_code; /* 0x03 */
        SANE_Byte parameter_length; /* 0x06 */
        SANE_Byte bmu;
        SANE_Byte res1;
        SANE_Byte mud[2];
        SANE_Byte res2[2];  /* anybody know what `COH' may mean ??? */
#if 0
        SANE_Byte more_pages[243]; /* maximum size 255 bytes (incl header) */
#endif
};

struct mode_pages {
        SANE_Byte page_code; 
        SANE_Byte parameter_length;
        SANE_Byte rest[6];
#if 0
        SANE_Byte more_pages[243]; /* maximum size 255 bytes (incl header) */
#endif
};


#endif /* ricoh_h */
