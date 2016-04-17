/* sane - Scanner Access Now Easy.

   pieusb_specific.h

   Copyright (C) 2012-2015 Jan Vleeshouwers, Michael Rickmann, Klaus Kaempf

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

#ifndef PIEUSB_SPECIFIC_H
#define	PIEUSB_SPECIFIC_H

#include "../include/sane/sanei_ir.h"
#include "../include/sane/sanei_backend.h"
#include "pieusb_scancmd.h"
#include "pieusb_buffer.h"

/* Settings for scan modes available to SANE */
/* In addition to those defined in sane.h */
#define SANE_VALUE_SCAN_MODE_RGBI    "RGBI"

/* Scanner settings for colors to scan */
#define SCAN_ONE_PASS_RGBI           0x90
#define SCAN_ONE_PASS_COLOR          0x80
#define SCAN_FILTER_INFRARED         0x10
#define SCAN_FILTER_BLUE             0x08
#define SCAN_FILTER_GREEN            0x04
#define SCAN_FILTER_RED              0x02
#define SCAN_FILTER_NEUTRAL          0x01

/* Settings for color depth of scan */
#define SCAN_COLOR_DEPTH_16          0x20
#define SCAN_COLOR_DEPTH_12          0x10
#define SCAN_COLOR_DEPTH_10          0x08
#define SCAN_COLOR_DEPTH_8           0x04
#define SCAN_COLOR_DEPTH_4           0x02
#define SCAN_COLOR_DEPTH_1           0x01

/* Settings for format of the scanned data */
#define SCAN_COLOR_FORMAT_INDEX      0x04
#define SCAN_COLOR_FORMAT_LINE       0x02
#define SCAN_COLOR_FORMAT_PIXEL      0x01

/* Settings for calibration mode */
#define SCAN_CALIBRATION_DEFAULT     "default values"
#define SCAN_CALIBRATION_AUTO        "from internal test"
#define SCAN_CALIBRATION_PREVIEW     "from preview"
#define SCAN_CALIBRATION_OPTIONS     "from options"

/* Settings for additional gain */
#define SCAN_GAIN_ADJUST_03 "* 0.3"
#define SCAN_GAIN_ADJUST_05 "* 0.5"
#define SCAN_GAIN_ADJUST_08 "* 0.8"
#define SCAN_GAIN_ADJUST_10 "* 1.0"
#define SCAN_GAIN_ADJUST_12 "* 1.2"
#define SCAN_GAIN_ADJUST_16 "* 1.6"
#define SCAN_GAIN_ADJUST_19 "* 1.9"
#define SCAN_GAIN_ADJUST_24 "* 2.4"
#define SCAN_GAIN_ADJUST_30 "* 3.0"

/* Post-processing */
#define POST_SW_COLORS          (1 << 0)        /* gain, negatives, ..., can be done at any time */
#define POST_SW_IRED            (1 << 1)        /* remove spectral overlap, needs complete scan */
#define POST_SW_DIRT            (1 << 2)        /* our digital lavabo, needs complete scan */
#define POST_SW_GRAIN           (1 << 3)        /* smoothen a bit */
#define POST_SW_CROP            (1 << 4)        /* trim whole image in sane_start
                                                   before sane_get_parameters() is answered */
#define POST_SW_IRED_MASK       (POST_SW_IRED | POST_SW_DIRT)
#define POST_SW_ACCUM_MASK      (POST_SW_IRED_MASK | POST_SW_GRAIN | POST_SW_CROP)

#define DEFAULT_GAIN                    19   /* 0x13 */
#define DEFAULT_EXPOSURE                2937 /* 0xb79 minimum value, see Pieusb_Settings */
#define DEFAULT_OFFSET                  0
#define DEFAULT_LIGHT                   4
#define DEFAULT_ADDITIONAL_ENTRIES      1
#define DEFAULT_DOUBLE_TIMES            0

/* --------------------------------------------------------------------------
 *
 * DEVICE DEFINITION STRUCTURES
 *
 * --------------------------------------------------------------------------*/

/* Options supported by the scanner */

enum Pieusb_Option
{
    OPT_NUM_OPTS = 0,
    /* ------------------------------------------- */
    OPT_MODE_GROUP,
    OPT_MODE,                   /* scan mode */
    OPT_BIT_DEPTH,              /* number of bits to encode a color */
    OPT_RESOLUTION,             /* number of pixels per inch */
    OPT_HALFTONE_PATTERN,       /* halftone pattern to use (see halftone_list) */
    OPT_THRESHOLD,              /* halftone threshold */
    OPT_SHARPEN,                /* create a sharper scan at the cost of scan time */
    OPT_SHADING_ANALYSIS,       /* do shading analysis before the scan */
    OPT_FAST_INFRARED,          /* scan infrared channel faster but less accurate */
    OPT_ADVANCE_SLIDE,          /* auto-advance slide after scan */
    OPT_CALIBRATION_MODE,       /* use auto-calibarion settings for scan */
    /* ------------------------------------------- */
    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */
    /* ------------------------------------------- */
    OPT_ENHANCEMENT_GROUP,
    OPT_CORRECT_SHADING,        /* correct scanned data for lamp variations (shading) */
    OPT_CORRECT_INFRARED,       /* correct infrared for red crosstalk */
    OPT_CLEAN_IMAGE,            /* detect and remove dust and scratch artifacts */
    OPT_GAIN_ADJUST,            /* adjust gain (a simpler option than setting gain, exposure and offset directly) */
    OPT_CROP_IMAGE,             /* automatically crop image */
    OPT_SMOOTH_IMAGE,           /* smoothen image */
    OPT_TRANSFORM_TO_SRGB,      /* transform to approximate sRGB data */
    OPT_INVERT_IMAGE,           /* transform negative to positive */
    /* ------------------------------------------- */
    OPT_ADVANCED_GROUP,
    OPT_PREVIEW,                /* scan a preview before the actual scan */
    OPT_SAVE_SHADINGDATA,       /* output shading data */
    OPT_SAVE_CCDMASK,           /* output CCD mask */
    OPT_LIGHT,
    OPT_DOUBLE_TIMES,
    OPT_SET_EXPOSURE_R,           /* exposure times for R */
    OPT_SET_EXPOSURE_G,           /* exposure times for G */
    OPT_SET_EXPOSURE_B,           /* exposure times for B */
    OPT_SET_EXPOSURE_I,           /* exposure times for I */
    OPT_SET_GAIN_R,               /* gain for R */
    OPT_SET_GAIN_G,               /* gain for G */
    OPT_SET_GAIN_B,               /* gain for B */
    OPT_SET_GAIN_I,               /* gain for I */
    OPT_SET_OFFSET_R,             /* offset for R */
    OPT_SET_OFFSET_G,             /* offset for G */
    OPT_SET_OFFSET_B,             /* offset for B */
    OPT_SET_OFFSET_I,             /* offset for I */
    /* must come last: */
    NUM_OPTIONS
};

/* Forward declaration (see pieusb_scancmd.h) */
struct Pieusb_Shading_Parameters;

/* Device characteristics of a Pieusb USB scanner */
struct Pieusb_Device_Definition
{
    struct Pieusb_Device_Definition *next;

    SANE_Device sane;
      /* name = string like "libusb:001:006" == NO! this should be "CrystalScan 7200" or "ProScan 7200"...
       * vendor = "PIE/Pieusb"
       * model = "CrystalScan 7200" or "ProScan 7200"
       * type = "film scanner" */
    /* char *devicename; => sane->name */
    /* char *vendor; => sane->vendor */
    /* char *product; => sane->model */
    SANE_Word vendorId;
    SANE_Word productId;
      /* USB id's like 0x05e3 0x0145, see pieusb.conf */
    SANE_String version; /* INQUIRY productRevision */
    SANE_Byte model; /* INQUIRY model */

    /* Ranges for various quantities */
    SANE_Range dpi_range;
    SANE_Range x_range;
    SANE_Range y_range;
    SANE_Range exposure_range; /* Unit is a 8051 machine cycle, which is approximately 1 us. (Exactly: 12 cycles at 11.059 Mhz = 1.085 us.) */
    SANE_Range dust_range;

    SANE_Range shadow_range;
    SANE_Range highlight_range;

    /* Enumerated ranges vor various quantities */
    SANE_String scan_mode_list[7]; /* names of scan modes (see saneopts.h) */
    SANE_String calibration_mode_list[6]; /* names of calibration modes */
    SANE_String gain_adjust_list[10]; /* gain adjustment values */
    SANE_Word bpp_list[5];	   /* bit depths  */
    SANE_String halftone_list[17]; /* names of the halftone patterns from the scanner */
    SANE_String speed_list[9];	   /* names of available speeds */
    SANE_String ir_sw_list[4];
    SANE_String crop_sw_list[4];
    SANE_Word grain_sw_list[6];

    /* Maximum resolution values */
    int maximum_resolution_x;	   /* maximum x-resolution */
    int maximum_resolution_y;	   /* maximum y-resolution */
    int maximum_resolution;

    /* Geometry */
    double scan_bed_width;	   /* flatbed width in inches (horizontal) */
    double scan_bed_height;	   /* flatbed height in inches (vertical) */
    int slide_top_left_x;          /* top-left location of slide w.r.t. scan bed */
    int slide_top_left_y;          /* top-left location of slide w.r.t. scan bed */
    double slide_width;	           /* transparency width in inches */
    double slide_height;           /* transparency length in inches */

    /* Integer and bit-encoded properties */
    int halftone_patterns;	   /* number of halftones supported */
    int color_filters;	           /* available colour filters: Infrared-0-0-OnePassColor-B-G-R-N */
    int color_depths;	           /* available colour depths: 0-0-16-12-10-8-4-1 */
    int color_formats;	           /* colour data format: 0-0-0-0-0-Index-Line-Pixel */
    int image_formats;	           /* image data format: 0-0-0-0-OKLine-BlkOne-Motorola-Intel */
    int scan_capabilities;         /* additional scanner features, number of speeds: PowerSave-ExtCal-0-FastPreview-DisableCal-[CalSpeeds=3] */
    int optional_devices;          /* optional devices: MultiPageLoad-?-?-0-0-TransModule1-TransModule-AutoDocFeeder */
    int enhancements;	           /* enhancements: unknown coding */
    int gamma_bits;	           /* no of bits used for gamma table */
    int fast_preview_resolution;   /* fast preview resolution */
    int minimum_highlight;	   /* min highlight % that can be used */
    int maximum_shadow;  	   /* max shadow % that can be used */
    int calibration_equation;      /* which calibration equation to use */
    int minimum_exposure;	   /* min exposure */
    int maximum_exposure;	   /* max exposure */

    struct Pieusb_Shading_Parameters_Info shading_parameters[4]; /* array with shading data parameters */

    int x0, y0, x1, y1;
    SANE_String production;
    SANE_String timestamp;
    SANE_String signature;
};

typedef struct Pieusb_Device_Definition Pieusb_Device_Definition;

/* --------------------------------------------------------------------------
 *
 * CURRENTLY ACTIVE DEVICES
 *
 * --------------------------------------------------------------------------*/

/* This structure holds information about an instance of an active scanner */

struct Pieusb_Scanner
{
    struct Pieusb_Scanner *next;
    struct Pieusb_Device_Definition *device; /* pointer to device definition */

    int device_number; /* scanner device number (as determined by USB) */

    /* SANE option descriptions and settings for this scanner instance */
    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];

    /* Scan state */
    struct Pieusb_Scanner_State state;
    SANE_Int scanning; /* true if busy scanning */
    SANE_Int cancel_request; /* if true, scanner should terminate a scan */

    /* Scan settings */
    struct Pieusb_Mode mode;
    struct Pieusb_Settings settings;
    struct Pieusb_Scan_Frame frame;
    SANE_Parameters scan_parameters;

    /* Shading data and CCD-mask */
#define PIEUSB_CCD_MASK_SIZE 0x1a1d  /* pieusb 5340; */ /* cyberview: 6685 0x1a1d */
    SANE_Byte *ccd_mask; /* malloc'ed in sane_open */
    SANE_Int ccd_mask_size;
    SANE_Bool shading_data_present; /* don't correct shading if not present */
    SANE_Int shading_mean[SHADING_PARAMETERS_INFO_COUNT]; /* mean shading value for each color (average all 45 lines)  */
    SANE_Int shading_max[SHADING_PARAMETERS_INFO_COUNT]; /* maximum shading value for each color (for all 45 lines)  */
    SANE_Int* shading_ref[SHADING_PARAMETERS_INFO_COUNT]; /* 4 arrays of shading references for each pixel on a line and for each color */

    /* Calibration using preview */

    SANE_Bool preview_done;
    SANE_Int preview_exposure[4];    /* exposure values used in preview */
    SANE_Int preview_gain[4];        /* gain values used in preview */
    SANE_Int preview_offset[4];      /* offset values used in preview */
    SANE_Int preview_lower_bound[4]; /* lowest RGBI values in preview */
    SANE_Int preview_upper_bound[4]; /* highest RGBI values in preview */

    /* Post processing options */
/*
    SANE_Int processing;
*/
    double *ln_lut; /* logarithmic lookup table */

    /* Reading buffer */
    struct Pieusb_Read_Buffer buffer;
};

typedef struct Pieusb_Scanner Pieusb_Scanner;

SANE_Status sanei_pieusb_parse_config_line(const char* config_line, SANE_Word* vendor_id, SANE_Word* product_id, SANE_Word* model_number);
/* sub to sane_start() */
SANE_Status sanei_pieusb_post (Pieusb_Scanner *scanner,  uint16_t **in_img, int planes);
void sanei_pieusb_correct_shading(struct Pieusb_Scanner *scanner, struct Pieusb_Read_Buffer *buffer);
SANE_Status sanei_pieusb_get_scan_data(Pieusb_Scanner * scanner, SANE_Int parameter_bytes);
SANE_Status sanei_pieusb_get_parameters(Pieusb_Scanner * scanner, SANE_Int *parameter_bytes);
SANE_Status sanei_pieusb_get_ccd_mask(Pieusb_Scanner * scanner);
SANE_Status sanei_pieusb_get_shading_data(Pieusb_Scanner * scanner);
SANE_Status sanei_pieusb_set_mode_from_options(Pieusb_Scanner * scanner);
SANE_Status sanei_pieusb_set_gain_offset(Pieusb_Scanner * scanner, const char* calibration_mode);
SANE_Status sanei_pieusb_set_frame_from_options(Pieusb_Scanner * scanner);
void sanei_pieusb_print_options(struct Pieusb_Scanner *scanner);
/* sub to sane_control_option() and sane_start() */
int sanei_pieusb_analyse_options(struct Pieusb_Scanner *scanner);
SANE_Bool sanei_pieusb_supported_device_list_contains(SANE_Word vendor_id, SANE_Word product_id, SANE_Word model_number);
SANE_Status sanei_pieusb_supported_device_list_add(SANE_Word vendor_id, SANE_Word product_id, SANE_Word model_number);
/* sub to sane_init() and sane_open() */
SANE_Status sanei_pieusb_find_device_callback (const char *devicename);
/* sub to sane_open() */
SANE_Status sanei_pieusb_init_options (Pieusb_Scanner * scanner);
/* sub to sane_start(), sane_read() and sane_close() */
SANE_Status sanei_pieusb_on_cancel (Pieusb_Scanner * scanner);

SANE_Status sanei_pieusb_wait_ready(Pieusb_Scanner *scanner, SANE_Int device_number);
SANE_Status sanei_pieusb_analyze_preview(Pieusb_Scanner * scanner);



#endif	/* PIEUSB_SPECIFIC_H */
