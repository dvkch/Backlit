/* sane - Scanner Access Now Easy.
   Copyright (C) 1996 David Mosberger-Tang
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

   This file implements a SANE backend for the Artec/Ultima scanners.

   Copyright (C) 1998,1999 Chris Pinkham
   Released under the terms of the GPL.
   *NO WARRANTY*

   *********************************************************************
   For feedback/information:

   cpinkham@corp.infi.net
   http://www4.infi.net/~cpinkham/sane/sane-artec-doc.html
   *********************************************************************
 */

#ifndef artec_h
#define artec_h

#include <sys/types.h>

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

#define ARTEC_MIN_X( hw )	( hw->horz_resolution_list[ 0 ] ? \
							hw->horz_resolution_list[ 1 ] : 0 )
#define ARTEC_MAX_X( hw )	( hw->horz_resolution_list[ 0 ] ? \
							hw->horz_resolution_list[ \
								hw->horz_resolution_list[ 0 ] ] : 0 )
#define ARTEC_MIN_Y( hw )	( hw->vert_resolution_list[ 0 ] ? \
							hw->vert_resolution_list[ 1 ] : 0 )
#define ARTEC_MAX_Y( hw )	( hw->vert_resolution_list[ 0 ] ? \
							hw->vert_resolution_list[ \
								hw->vert_resolution_list[ 0 ] ] : 0 )

typedef enum
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    OPT_MODE,
    OPT_X_RESOLUTION,
    OPT_Y_RESOLUTION,
    OPT_RESOLUTION_BIND,
    OPT_PREVIEW,
    OPT_GRAY_PREVIEW,
    OPT_NEGATIVE,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_ENHANCEMENT_GROUP,
    OPT_CONTRAST,
    OPT_BRIGHTNESS,
    OPT_THRESHOLD,
    OPT_HALFTONE_PATTERN,
    OPT_FILTER_TYPE,
    OPT_PIXEL_AVG,
    OPT_EDGE_ENH,

    OPT_CUSTOM_GAMMA, /* use custom gamma table */
    OPT_GAMMA_VECTOR,
    OPT_GAMMA_VECTOR_R,
    OPT_GAMMA_VECTOR_G,
    OPT_GAMMA_VECTOR_B,

    OPT_TRANSPARENCY,
    OPT_ADF,

    OPT_CALIBRATION_GROUP,
    OPT_QUALITY_CAL,
    OPT_SOFTWARE_CAL,

    /* must come last */
    NUM_OPTIONS
  }
ARTEC_Option;

/* Some FLAGS */
#define ARTEC_FLAG_CALIBRATE			0x00000001 /* supports hardware calib */
#define ARTEC_FLAG_CALIBRATE_RGB		0x00000003 /* yes 3, set CALIB. also */
#define ARTEC_FLAG_CALIBRATE_DARK_WHITE	0x00000005 /* yes 5, set CALIB. also */
#define ARTEC_FLAG_RGB_LINE_OFFSET		0x00000008 /* need line offset buffer */
#define ARTEC_FLAG_RGB_CHAR_SHIFT		0x00000010 /* RRRRGGGGBBBB line fmt */
#define ARTEC_FLAG_OPT_CONTRAST         0x00000020 /* supports set contrast */
#define ARTEC_FLAG_ONE_PASS_SCANNER		0x00000040 /* single pass scanner */
#define ARTEC_FLAG_GAMMA				0x00000080 /* supports set gamma */
#define ARTEC_FLAG_GAMMA_SINGLE			0x00000180 /* yes 180, implies GAMMA */
#define ARTEC_FLAG_SEPARATE_RES			0x00000200 /* separate x & y scan res */
#define ARTEC_FLAG_IMAGE_REV_LR         0x00000400 /* reversed left-right */
#define ARTEC_FLAG_ENHANCE_LINE_EDGE    0x00000800 /* line edge enhancement */
#define ARTEC_FLAG_HALFTONE_PATTERN     0x00001000 /* > 1 halftone  pattern */
#define ARTEC_FLAG_REVERSE_WINDOW       0x00002000 /* reverse selected area */
#define ARTEC_FLAG_SC_BUFFERS_LINES     0x00004000 /* scanner has line buffer */
#define ARTEC_FLAG_SC_HANDLES_OFFSET    0x00008000 /* sc. handles line offset */
#define ARTEC_FLAG_SENSE_HANDLER        0x00010000 /* supports sense handler */
#define ARTEC_FLAG_SENSE_ENH_18         0x00020000 /* supports enh. byte 18 */
#define ARTEC_FLAG_SENSE_BYTE_19        0x00040000 /* supports sense byte 19 */
#define ARTEC_FLAG_SENSE_BYTE_22        0x00080000 /* supports sense byte 22 */
#define ARTEC_FLAG_PIXEL_AVERAGING      0x00100000 /* supports pixel avg-ing */
#define ARTEC_FLAG_ADF                  0x00200000 /* auto document feeder */
#define ARTEC_FLAG_OPT_BRIGHTNESS       0x00400000 /* supports set brightness */
#define ARTEC_FLAG_MBPP_NEGATIVE        0x00800000 /* can negate > 1bpp modes */

typedef enum
  {
    ARTEC_COMP_LINEART = 0,
    ARTEC_COMP_HALFTONE,
    ARTEC_COMP_GRAY,
    ARTEC_COMP_UNSUPP1,
    ARTEC_COMP_UNSUPP2,
    ARTEC_COMP_COLOR
  }
ARTEC_Image_Composition;

typedef enum
  {
    ARTEC_DATA_IMAGE = 0,
    ARTEC_DATA_UNSUPP1,
    ARTEC_DATA_HALFTONE_PATTERN,	/* 2 */
    ARTEC_DATA_UNSUPP3,
    ARTEC_DATA_RED_SHADING,	/* 4 */
    ARTEC_DATA_GREEN_SHADING,	/* 5 */
    ARTEC_DATA_BLUE_SHADING,	/* 6 */
    ARTEC_DATA_WHITE_SHADING_OPT,	/* 7 */
    ARTEC_DATA_WHITE_SHADING_TRANS,	/* 8 */
    ARTEC_DATA_CAPABILITY_DATA,	/* 9 */
    ARTEC_DATA_DARK_SHADING,	/* 10, 0xA */
    ARTEC_DATA_RED_GAMMA_CURVE,	/* 11, 0xB */
    ARTEC_DATA_GREEN_GAMMA_CURVE,	/* 12, 0xC */
    ARTEC_DATA_BLUE_GAMMA_CURVE,	/* 13, 0xD */
    ARTEC_DATA_ALL_GAMMA_CURVE	/* 14, 0xE */
  }
ARTEC_Read_Data_Type;

typedef enum
  {
    ARTEC_CALIB_RGB = 0,
    ARTEC_CALIB_DARK_WHITE
  }
ARTEC_Calibrate_Method;

typedef enum
  {
    ARTEC_FILTER_MONO = 0,
    ARTEC_FILTER_RED,
    ARTEC_FILTER_GREEN,
    ARTEC_FILTER_BLUE
  }
ARTEC_Filter_Type;

typedef enum
  {
    ARTEC_SOFT_CALIB_RED = 0,
	ARTEC_SOFT_CALIB_GREEN,
	ARTEC_SOFT_CALIB_BLUE
  }
ARTEC_Software_Calibrate;


typedef struct ARTEC_Device
  {
    struct ARTEC_Device *next;
    SANE_Device sane;
    double width;
    SANE_Range x_range;
    SANE_Word *horz_resolution_list;
    double height;
    SANE_Range y_range;
    SANE_Word *vert_resolution_list;
    SANE_Range threshold_range;
    SANE_Range contrast_range;
    SANE_Range brightness_range;
    SANE_Word setwindow_cmd_size;
    SANE_Word calibrate_method;
	SANE_Word max_read_size;

    long flags;
    SANE_Bool support_cap_data_retrieve;
    SANE_Bool req_shading_calibrate;
    SANE_Bool req_rgb_line_offset;
    SANE_Bool req_rgb_char_shift;

    /* info for 1-pass vs. 3-pass */
    SANE_Bool onepass;

    SANE_Bool support_gamma;
    SANE_Bool single_gamma;
	SANE_Int gamma_length;
  }
ARTEC_Device;

typedef struct ARTEC_Scanner
  {
    /* all the state needed to define a scan request: */
    struct ARTEC_Scanner *next;

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];

	SANE_Int gamma_table[4][4096];
	double soft_calibrate_data[3][2592];
	SANE_Int halftone_pattern[64];
	SANE_Range gamma_range;
	int gamma_length;

    int scanning;
    SANE_Parameters params;
    size_t bytes_to_read;
    SANE_Int line_offset;

    /* scan parameters */
    char *mode;
    SANE_Int x_resolution;
    SANE_Int y_resolution;
    SANE_Int tl_x;
    SANE_Int tl_y;

    /* info for 1-pass vs. 3-pass */
    int this_pass;
    SANE_Bool onepasscolor;
    SANE_Bool threepasscolor;

    int fd;			/* SCSI filedescriptor */

    /* scanner dependent/low-level state: */
    ARTEC_Device *hw;
  }
ARTEC_Scanner;

#endif /* artec_h */
