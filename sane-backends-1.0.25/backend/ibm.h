/* sane - Scanner Access Now Easy.
   
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



#ifndef ibm_h
#define ibm_h 1

#include <sys/types.h>

#include "../include/sane/config.h"

/* defines for scan_image_mode field */
#define IBM_BINARY_MONOCHROME   0
#define IBM_DITHERED_MONOCHROME 1
#define IBM_GRAYSCALE           2

/* defines for paper field */
#define IBM_PAPER_USER_DEFINED	0
#define IBM_PAPER_A3		1
#define IBM_PAPER_A4		2
#define IBM_PAPER_A4L		3
#define IBM_PAPER_A5		4
#define IBM_PAPER_A5L		5
#define IBM_PAPER_A6		6
#define IBM_PAPER_B4		7
#define IBM_PAPER_B5		8
#define IBM_PAPER_LEGAL		9
#define IBM_PAPER_LETTER	10

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

static const SANE_String_Const paper_list[] =
  {
    "User",
    "A3",
    "A4", "A4R",
    "A5", "A5R",
    "A6",
    "B4", "B5",
    "Legal", "Letter",
    0
  };

#define PAPER_A3_W	14032
#define PAPER_A3_H	19842
#define PAPER_A4_W	9921
#define PAPER_A4_H	14032
#define PAPER_A4R_W	14032
#define PAPER_A4R_H	9921
#define PAPER_A5_W	7016
#define PAPER_A5_H	9921
#define PAPER_A5R_W	9921
#define PAPER_A5R_H	7016
#define PAPER_A6_W	4960
#define PAPER_A6_H	7016
#define PAPER_B4_W	11811
#define PAPER_B4_H	16677
#define PAPER_B5_W	8598
#define PAPER_B5_H	12142
#define PAPER_LEGAL_W	10200
#define PAPER_LEGAL_H	16800
#define PAPER_LETTER_W	10200
#define PAPER_LETTER_H	13200

static const SANE_Range u8_range =
  {
      0,				/* minimum */
    255,				/* maximum */
      0				        /* quantization */
  };

static const SANE_Range ibm2456_res_range =
  {
    100,				/* minimum */
    600,				/* maximum */
      0				        /* quantization */
  };

static const SANE_Range default_x_range =
  {
    0,				        /* minimum */
/*    (SANE_Word) ( * DEFAULT_MUD),	 maximum */
    14032,				/* maximum (found empirically for Gray mode) */
    					/* in Lineart mode it works till 14062 */
    2				        /* quantization */
  };

static const SANE_Range default_y_range =
  {
    0,				        /* minimum */
/*    (SANE_Word) (14 * DEFAULT_MUD),	 maximum */
    20410,				/* maximum (found empirically) */
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
    OPT_ADF,

    OPT_GEOMETRY_GROUP,
    OPT_PAPER,			/* predefined formats */
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
Ibm_Option;

typedef struct Ibm_Info
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
    SANE_Int paper_default;
    SANE_Int brightness_default;
    SANE_Int contrast_default;
    SANE_Int adf_default;

    SANE_Int bmu;
    SANE_Int mud;
  }
Ibm_Info;

typedef struct Ibm_Device
  {
    struct Ibm_Device *next;
    SANE_Device sane;
    Ibm_Info info;
  }
Ibm_Device;

typedef struct Ibm_Scanner
  {
    /* all the state needed to define a scan request: */
    struct Ibm_Scanner *next;
    int fd;			/* SCSI filedescriptor */

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];
    SANE_Parameters params;
    /* scanner dependent/low-level state: */
    Ibm_Device *hw;

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
/* next lines by mf */
    SANE_Int adf_state;
#define ADF_UNUSED  0             /* scan from flatbed, not ADF */
#define ADF_ARMED   1             /* scan from ADF, everything's set up */
#define ADF_CLEANUP 2             /* eject paper from ADF on close */
/* end lines by mf */
    size_t bytes_to_read;
    int scanning;
  }
Ibm_Scanner;

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

#define IBM_WINDOW_DATA_SIZE 320
struct ibm_window_data {
        /* header */
        SANE_Byte reserved[6];
        SANE_Byte len[2];
        /* data */
        SANE_Byte window_id;         /* must be zero */
        SANE_Byte reserved0;
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
        SANE_Byte halftone_code;     /* halftone_pattern[0] in ricoh.h */
        SANE_Byte halftone_id;       /* halftone_pattern[1] in ricoh.h */
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
/* next 4 lines by mf */
	SANE_Byte adf_page_code;
	SANE_Byte adf_parameter_length;
	SANE_Byte adf_control;
	SANE_Byte res3[5];
};

struct mode_pages {
        SANE_Byte page_code; 
        SANE_Byte parameter_length;
        SANE_Byte rest[14];  /* modified by mf; it was 6; see above */
#if 0
        SANE_Byte more_pages[243]; /* maximum size 255 bytes (incl header) */
#endif
};


#endif /* ibm_h */
