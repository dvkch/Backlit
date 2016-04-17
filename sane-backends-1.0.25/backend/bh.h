/* sane - Scanner Access Now Easy.
   Copyright (C) 1999,2000 Tom Martone
   This file is part of a SANE backend for Bell and Howell Copiscan II
   Scanners using the Remote SCSI Controller(RSC).

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

#ifndef BH_H
#define BH_H 1

#ifndef PATH_MAX
#define PATH_MAX (1024)
#endif

#define BH_CONFIG_FILE "bh.conf"

/* number of barcode types that can be searched at one time */
#define NUM_SEARCH_BARS 6
/* number of additional scanning/decoding sections supported */
#define NUM_SECTIONS 8
/* number of possible reads per scan plus the extra one for
 * the barcode file
 */
#define NUM_READS 56 + 1

/* specify sanity limits for autoborder detection and barcode decoding */
#define BH_AUTOBORDER_TRIES 100
#define BH_DECODE_TRIES 100
/* specify a fudge factor in mm for border around decoded barcodes */
#define BH_DECODE_FUDGE 1.0

/* section flags - what operation(s) to perform on section */
#define BH_SECTION_FRONT_IMAGE (1 << 0)
#define BH_SECTION_BACK_IMAGE (1 << 1)
#define BH_SECTION_FRONT_BAR (1 << 2)
#define BH_SECTION_BACK_BAR (1 << 3)
#define BH_SECTION_FRONT_PATCH (1 << 4)
#define BH_SECTION_BACK_PATCH (1 << 5)

typedef enum 
{ 
  BH_UNIT_INCH, 
  BH_UNIT_MM, 
  BH_UNIT_POINT
} bh_measureUnit;

typedef enum
{
  BH_COMP_NONE, 
  BH_COMP_G31D,
  BH_COMP_G32D,
  BH_COMP_G42D
} bh_compress;

typedef enum
{
  BH_ROTATION_0, 
  BH_ROTATION_90,
  BH_ROTATION_180,
  BH_ROTATION_270
} bh_rotation;

typedef enum 
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  /* inquiry string */
  OPT_INQUIRY,
  /* preview mode */
  OPT_PREVIEW,
  /* scan mode */
  OPT_SCAN_MODE,
  /* resolution */
  OPT_RESOLUTION,
  /* hardware compression */
  OPT_COMPRESSION,

  OPT_GEOMETRY_GROUP,
  /* automatic border detection */
  OPT_AUTOBORDER,
  /* hardware rotation */
  OPT_ROTATION,
  /* hardware deskew */
  OPT_DESKEW,
  /* paper size */
  OPT_PAPER_SIZE,
  /* top-left x */
  OPT_TL_X,
  /* top-left y */
  OPT_TL_Y,
  /* bottom-right x */
  OPT_BR_X,
  /* bottom-right y */
  OPT_BR_Y,

  OPT_FEEDER_GROUP,
  /* scan source (eg. ADF) */
  OPT_SCAN_SOURCE,
  /* scan in batch mode */
  OPT_BATCH,
  /* scan both sides of the page */
  OPT_DUPLEX,
  /* timeout in seconds with manual feed */
  OPT_TIMEOUT_MANUAL,
  /* timeout in seconds with ADF */
  OPT_TIMEOUT_ADF,
  /* check for page in ADF before scanning */
  OPT_CHECK_ADF,

  OPT_ENHANCEMENT_GROUP,
  /* Enables the scanner's control panel */
  OPT_CONTROL_PANEL,
  /* ACE Function */
  OPT_ACE_FUNCTION,
  /* ACE Sensitivity */
  OPT_ACE_SENSITIVITY,
  /* Brightness */
  OPT_BRIGHTNESS,
  /* Threshold */
  OPT_THRESHOLD,
  /* Contrast */
  OPT_CONTRAST,
  /* Negative (reverse image) */
  OPT_NEGATIVE,

  OPT_ICON_GROUP,
  /* Width of icon (thumbnail) image in pixels */
  OPT_ICON_WIDTH,
  /* Length of icon (thumbnail) image in pixels */
  OPT_ICON_LENGTH,

  OPT_BARCODE_GROUP,
  /* Add <name> to barcode search priority. */
  OPT_BARCODE_SEARCH_BAR,
  /* Barcode search count (1-7, default 3). */
  OPT_BARCODE_SEARCH_COUNT,
  /* Barcode search mode. 
   * (1 = horizontal,2 = vertical, 6 = v then h, 9 = h then v). 
   */
  OPT_BARCODE_SEARCH_MODE,
  /* Patch code min height (def=127 (5mm)) */
  OPT_BARCODE_HMIN,       
  /* Barcode search timeout in ms 
   * (20-65535,default is disabled). 
   */
  OPT_BARCODE_SEARCH_TIMEOUT,
  /* Specify image sections and functions
   */
  OPT_SECTION,
  /* Specifies the maximum relation from the widest to 
   * the smallest bar 
   */
  OPT_BARCODE_RELMAX,
  /* Specifies the minimum number of bars in Bar/Patch code */
  OPT_BARCODE_BARMIN,
  /* Specifies the maximum number of bars in a Bar/Patch code */
  OPT_BARCODE_BARMAX,
  /* Specifies the image contrast used in decoding.  
   * Use higher values when there are more white pixels 
   * in the code 
   */
  OPT_BARCODE_CONTRAST,
  /* Controls Patch Code detection. */
  OPT_BARCODE_PATCHMODE,

  /* must come last: */
  NUM_OPTIONS

} BH_Option;

/* macros for accessing the value for an option within a scanning context */
#define _OPT_VAL_WORD(s, o) ((s)->val[(o)].w)
#define _OPT_VAL_WORD_THOUSANDTHS(s, o) \
    (SANE_UNFIX(_OPT_VAL_WORD((s), (o))) * 1000.0 / MM_PER_INCH)
#define _OPT_VAL_STRING(s, o) ((s)->val[(o)].s)
#define _OPT_VAL_WORD_ARRAY(s, o) ((s)->val[(o)].wa)


typedef struct _BH_Paper
{
  SANE_String name;

  /* paper dimensions in mm */
  double width, length;
} BH_Paper;

typedef struct _BH_Section
{
  /* section dimensions - in millimeters */
  u_long top, left, width, length;

  /* compression type/arg/frameformat */
  SANE_Byte compressiontype;
  SANE_Byte compressionarg;
  SANE_Frame format;

  /* Flags (see BH_SECTION_...) indicating operation(s) to perform
   * on the section.  If zero, the section is completely disabled
   * and will not even be defined in set_window.
   */
  SANE_Word flags;

} BH_Section;

typedef struct _BH_Info
{
  SANE_Range x_range;
  SANE_Range y_range;

  SANE_Int res_default;
  SANE_Bool autoborder_default;
  SANE_Bool batch_default;
  SANE_Bool deskew_default;
  SANE_Bool check_adf_default;
  SANE_Bool duplex_default;
  SANE_Int timeout_adf_default;
  SANE_Int timeout_manual_default;
  SANE_Bool control_panel_default;

  /* additional discovered/guessed capabilities */
  SANE_Bool canACE;
  SANE_Bool canDuplex;
  SANE_Bool canCheckADF;

  /* standard information */
  SANE_Byte devtype;
  SANE_Char vendor[9];	/* model name */
  SANE_Char product[17];	/* product name */
  SANE_Char revision[5];	/* revision */

  /* VPD information */
  SANE_Bool canADF;		/* is there an ADF available */
  SANE_Bool colorBandW;	/* can scanner do black and white */
  SANE_Bool colorHalftone;	/* can scanner do Halftone */
  SANE_Bool canWhiteFrame;	/* data processing: White Framing */
  SANE_Bool canBlackFrame;	/* data processing: Black Framing */
  SANE_Bool canEdgeExtract;	/* data processing: ACE: Edge Extraction */
  SANE_Bool canNoiseFilter;	/* data processing: ACE: Noise Filtering */
  SANE_Bool canSmooth;	/* data processing: ACE: Smoothing */
  SANE_Bool canLineBold;	/* data processing: ACE: LineBolding */
  SANE_Bool comprG3_1D;	/* compression: Group 3, 1 dimensional */
  SANE_Bool comprG3_2D;	/* compression: Group 3, 2 dimensional */
  SANE_Bool comprG4;		/* compression: Group 4 */
  SANE_Bool canBorderRecog;	/* can do border recognition */
  SANE_Bool canBarCode;	/* bar code support available */
  SANE_Bool canIcon;		/* icon support available */
  SANE_Bool canSection;	/* section support available */
  SANE_Int lineMaxBytes;	/* maximum bytes per scan-line */

  /* jis information */
  SANE_Int resBasicX;		/* basic X resolution */
  SANE_Int resBasicY;		/* basic Y resolution */
  SANE_Int resMaxX;		/* maximum X resolution */
  SANE_Int resMaxY;		/* maximum Y resolution */
  SANE_Int resMinX;		/* minimum X resolution */
  SANE_Int resMinY;		/* minimum Y resolution */
  SANE_Int resStdList[16+1];    /* list of available standard resolutions 
				 * (first slot is the length) 
				 */
  SANE_Int winWidth;		/* length of window (in BasicX res DPI) */
  SANE_Int winHeight;		/* height of window (in BasicY res DPI) */
} BH_Info;

typedef struct _BH_Device BH_Device;

struct _BH_Device
{
  BH_Device *next;
  SANE_Device sane;
  BH_Info info;
};


typedef struct _BH_Scanner BH_Scanner;
struct _BH_Scanner
{
  /* all the state needed to define a scan request: */

  /* linked list for housekeeping */
  BH_Scanner *next;

  /* scanner dependent/low-level state: */
  BH_Device *hw;

  /* SCSI filedescriptor */
  int fd;
  
  /* tempfile which is used to send decoded barcode data */
  FILE *barf;
  char barfname[PATH_MAX+1];

  /* SANE option descriptors and values */
  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];

  /* additional values that don't fit into Option_Value representation */
  SANE_Byte search_bars[NUM_SEARCH_BARS];
  BH_Section sections[NUM_SECTIONS];
  SANE_Int num_sections;

  /* SANE image parameters */
  SANE_Parameters params;
  
  /* state information - not options */

  /* Basic Measurement Unit */
  SANE_Int bmu;
  /* Measurement Unit Divisor */
  SANE_Int mud;

  /* track data to be read.  ReadList contains the codes of the read types
   * (see BH_READ_TYPE...) to perform, readcnt is the total number of reads
   * for this scan and readptr points to the current read operation.
   */
  SANE_Byte readlist[NUM_READS];
  SANE_Int readcnt, readptr;

  u_long InvalidBytes;
  SANE_Bool scanning;
  SANE_Bool cancelled;
  SANE_Bool backpage;
  SANE_Bool barcodes;
  SANE_Bool patchcodes;
  SANE_Bool icons;
  u_long iconwidth, iconlength;
  SANE_Bool barcode_not_found;
};

static const SANE_Range u8_range =
{
    0,		/* minimum */
    255,	/* maximum */
    0		/* quantization */
};

static const SANE_Range u16_range =
{
    0,		/* minimum */
    65535, 	/* maximum */
    0		/* quantization */
};

static const SANE_Range icon_range =
{
    0,		/* minimum */
    3600, 	/* maximum */
    8		/* quantization */
};

static const SANE_Range barcode_search_timeout_range =
{
    20,		/* minimum */
    65535, 	/* maximum */
    0		/* quantization */
};

static const SANE_Range barcode_hmin_range =
{
    1,		/* minimum */
    1660, 	/* maximum (when converted from mm 
		 * to thousandths will still be less 
		 * than 65536) 
		 */
    0		/* quantization */
};

static const SANE_Range barcode_search_count_range =
{
    1,		/* minimum */
    7,  	/* maximum */
    0		/* quantization */
};

static const SANE_Range barcode_relmax_range =
{
    0,		/* minimum */
    6,  	/* maximum */
    0		/* quantization */
};

static const SANE_Range barcode_contrast_range =
{
    0,		/* minimum */
    6,  	/* maximum */
    0		/* quantization */
};

static const SANE_Range barcode_patchmode_range =
{
    0,		/* minimum */
    1,  	/* maximum */
    0		/* quantization */
};

static const SANE_Range ace_function_range =
{
    -4,		/* minimum */
    4,  	/* maximum */
    0		/* quantization */
};

static const SANE_Range ace_sensitivity_range =
{
    0,		/* minimum */
    9,  	/* maximum */
    0		/* quantization */
};

static SANE_String_Const scan_mode_list[] =
{
  "lineart",
  "halftone",
  0
};

static SANE_String_Const scan_mode_min_list[] =
{
  "lineart",
  0
};

static SANE_String_Const barcode_search_mode_list[] =
{
  "horiz-vert", /* 9 */
  "horizontal", /* 1 */
  "vertical",   /* 2 */
  "vert-horiz", /* 6 */
  0
};

static SANE_String_Const scan_source_list[] =
{
  "Automatic Document Feeder",
  "Manual Feed Tray",
  0
};

static SANE_String_Const compression_list[] =
{
  "none",
  "g31d",
  "g32d",
  "g42d",
  0
};

/* list of supported bar/patch codes */
static SANE_String_Const barcode_search_bar_list[] =
{
  "none",
  "ean-8",
  "ean-13",
  "reserved-ean-add",
  "code39",
  "code2-5-interleaved",
  "code2-5-3lines-matrix",
  "code2-5-3lines-datalogic",
  "code2-5-5lines-industrial",
  "patchcode",
  "codabar",
  "codabar-with-start-stop",
  "code39ascii",
  "code128",
  "code2-5-5lines-iata",
  0
};

/* list of supported rotation angles */
static SANE_String_Const rotation_list[] =
{
  "0",
  "90",
  "180",
  "270",
  0
};

/* list of support paper sizes */
/* 'custom' MUST be item 0; otherwise a width or length of 0 indicates
 * the maximum value supported by the scanner
 */
static const BH_Paper paper_sizes[] =
{
  {"Custom", 0.0, 0.0},
  {"Letter", 215.9, 279.4},
  {"Legal", 215.9, 355.6},
  {"A3", 297, 420},
  {"A4", 210, 297},
  {"A5", 148.5, 210},
  {"A6", 105, 148.5},
  {"B4", 250, 353},
  {"B5", 182, 257},
  {"Full", 0.0, 0.0},
};

/* MUST be kept in sync with paper_sizes */
static SANE_String_Const paper_list[] =
{
  "Custom",
  "Letter",
  "Legal",
  "A3",
  "A4",
  "A5",
  "A6",
  "B4",
  "B5",
  "Full",
  0
};

static /* inline */ int _is_host_little_endian(void);
static /* inline */ int
_is_host_little_endian()
{
  SANE_Int val = 255;
  unsigned char *firstbyte = (unsigned char *) &val;

  return (*firstbyte == 255) ? SANE_TRUE : SANE_FALSE;
}

static /* inline */ void
_lto2b(u_long val, SANE_Byte *bytes)
{
  bytes[0] = (val >> 8) & 0xff;
  bytes[1] = val & 0xff;
}

static /* inline */ void
_lto3b(u_long val, SANE_Byte *bytes)
{
  bytes[0] = (val >> 16) & 0xff;
  bytes[1] = (val >> 8) & 0xff;
  bytes[2] = val & 0xff;
}

static /* inline */ void
_lto4b(u_long val, SANE_Byte *bytes)
{
  bytes[0] = (val >> 24) & 0xff;
  bytes[1] = (val >> 16) & 0xff;
  bytes[2] = (val >> 8) & 0xff;
  bytes[3] = val & 0xff;
}

static /* inline */ u_long
_2btol(SANE_Byte *bytes)
{
  u_long rv;

  rv = (bytes[0] << 8) | bytes[1];

  return rv;
}

static /* inline */ u_long
_4btol(SANE_Byte *bytes)
{
  u_long rv;

  rv = (bytes[0] << 24) |
    (bytes[1] << 16) |
    (bytes[2] << 8) |
    bytes[3];

  return rv;
}

#define SANE_TITLE_SCAN_MODE_GROUP "Scan Mode"
#define SANE_TITLE_GEOMETRY_GROUP "Geometry"
#define SANE_TITLE_FEEDER_GROUP "Feeder"
#define SANE_TITLE_ENHANCEMENT_GROUP "Enhancement"
#define SANE_TITLE_ICON_GROUP "Icon"
#define SANE_TITLE_BARCODE_GROUP "Barcode"

#define SANE_NAME_AUTOBORDER "autoborder"
#define SANE_TITLE_AUTOBORDER "Autoborder"
#define SANE_DESC_AUTOBORDER "Enable Automatic Border Detection"

#define SANE_NAME_COMPRESSION "compression"
#define SANE_TITLE_COMPRESSION "Data Compression"
#define SANE_DESC_COMPRESSION "Sets the compression mode of the scanner"

#define SANE_NAME_ROTATION "rotation"
#define SANE_TITLE_ROTATION "Page Rotation"
#define SANE_DESC_ROTATION "Sets the page rotation mode of the scanner"

#define SANE_NAME_DESKEW "deskew"
#define SANE_TITLE_DESKEW "Page Deskew"
#define SANE_DESC_DESKEW "Enable Deskew Mode"

#define SANE_NAME_TIMEOUT_ADF "timeout-adf"
#define SANE_TITLE_TIMEOUT_ADF "ADF Timeout"
#define SANE_DESC_TIMEOUT_ADF "Sets the timeout in seconds for the ADF"

#define SANE_NAME_TIMEOUT_MANUAL "timeout-manual"
#define SANE_TITLE_TIMEOUT_MANUAL "Manual Timeout"
#define SANE_DESC_TIMEOUT_MANUAL "Sets the timeout in seconds for manual feeder"

#define SANE_NAME_BATCH "batch"
#define SANE_TITLE_BATCH "Batch"
#define SANE_DESC_BATCH "Enable Batch Mode"

#define SANE_NAME_CHECK_ADF "check-adf"
#define SANE_TITLE_CHECK_ADF "Check ADF"
#define SANE_DESC_CHECK_ADF "Check ADF Status prior to starting scan"

#define SANE_NAME_DUPLEX "duplex"
#define SANE_TITLE_DUPLEX "Duplex"
#define SANE_DESC_DUPLEX "Enable Duplex (Dual-Sided) Scanning"

#define SANE_NAME_BARCODE_SEARCH_COUNT "barcode-search-count"
#define SANE_TITLE_BARCODE_SEARCH_COUNT "Barcode Search Count"
#define SANE_DESC_BARCODE_SEARCH_COUNT "Number of barcodes to search for in the scanned image"

#define SANE_NAME_BARCODE_HMIN "barcode-hmin"
#define SANE_TITLE_BARCODE_HMIN "Barcode Minimum Height"
#define SANE_DESC_BARCODE_HMIN "Sets the Barcode Minimun Height (larger values increase recognition speed)"

#define SANE_NAME_BARCODE_SEARCH_MODE "barcode-search-mode"
#define SANE_TITLE_BARCODE_SEARCH_MODE "Barcode Search Mode"
#define SANE_DESC_BARCODE_SEARCH_MODE "Chooses the orientation of barcodes to be searched"

#define SANE_NAME_BARCODE_SEARCH_TIMEOUT "barcode-search-timeout"
#define SANE_TITLE_BARCODE_SEARCH_TIMEOUT "Barcode Search Timeout"
#define SANE_DESC_BARCODE_SEARCH_TIMEOUT "Sets the timeout for barcode searching"

#define SANE_NAME_BARCODE_SEARCH_BAR "barcode-search-bar"
#define SANE_TITLE_BARCODE_SEARCH_BAR "Barcode Search Bar"
#define SANE_DESC_BARCODE_SEARCH_BAR "Specifies the barcode type to search for"

#define SANE_NAME_SECTION "section"
#define SANE_TITLE_SECTION "Image/Barcode Search Sections"
#define SANE_DESC_SECTION "Specifies an image section and/or a barcode search region"

#define SANE_NAME_BARCODE_RELMAX "barcode-relmax"
#define SANE_TITLE_BARCODE_RELMAX "Barcode RelMax"
#define SANE_DESC_BARCODE_RELMAX "Specifies the maximum relation from the widest to the smallest bar"

#define SANE_NAME_BARCODE_BARMIN "barcode-barmin"
#define SANE_TITLE_BARCODE_BARMIN "Barcode Bar Minimum"
#define SANE_DESC_BARCODE_BARMIN "Specifies the minimum number of bars in Bar/Patch code"

#define SANE_NAME_BARCODE_BARMAX "barcode-barmax"
#define SANE_TITLE_BARCODE_BARMAX "Barcode Bar Maximum"
#define SANE_DESC_BARCODE_BARMAX "Specifies the maximum number of bars in a Bar/Patch code"

#define SANE_NAME_BARCODE_CONTRAST "barcode-contrast"
#define SANE_TITLE_BARCODE_CONTRAST "Barcode Contrast"
#define SANE_DESC_BARCODE_CONTRAST "Specifies the image contrast used in decoding.  Use higher values when " \
"there are more white pixels in the code"

#define SANE_NAME_BARCODE_PATCHMODE "barcode-patchmode"
#define SANE_TITLE_BARCODE_PATCHMODE "Barcode Patch Mode"
#define SANE_DESC_BARCODE_PATCHMODE "Controls Patch Code detection."

#define SANE_NAME_CONTROL_PANEL "control-panel"
#define SANE_TITLE_CONTROL_PANEL "Control Panel "
#define SANE_DESC_CONTROL_PANEL "Enables the scanner's control panel"

#define SANE_NAME_ACE_FUNCTION "ace-function"
#define SANE_TITLE_ACE_FUNCTION "ACE Function"
#define SANE_DESC_ACE_FUNCTION "ACE Function"

#define SANE_NAME_ACE_SENSITIVITY "ace-sensitivity"
#define SANE_TITLE_ACE_SENSITIVITY "ACE Sensitivity"
#define SANE_DESC_ACE_SENSITIVITY "ACE Sensitivity"

#define SANE_NAME_ICON_WIDTH "icon-width"
#define SANE_TITLE_ICON_WIDTH "Icon Width"
#define SANE_DESC_ICON_WIDTH "Width of icon (thumbnail) image in pixels"

#define SANE_NAME_ICON_LENGTH "icon-length"
#define SANE_TITLE_ICON_LENGTH "Icon Length"
#define SANE_DESC_ICON_LENGTH "Length of icon (thumbnail) image in pixels"

#define SANE_NAME_PAPER_SIZE "paper-size"
#define SANE_TITLE_PAPER_SIZE "Paper Size"
#define SANE_DESC_PAPER_SIZE "Specify the scan window geometry by specifying the paper size " \
"of the documents to be scanned"

#define SANE_NAME_INQUIRY "inquiry"
#define SANE_TITLE_INQUIRY "Inquiry Data"
#define SANE_DESC_INQUIRY "Displays scanner inquiry data"

/* low level SCSI commands and buffers */

/* SCSI commands */
#define BH_SCSI_TEST_UNIT_READY 0x00
#define BH_SCSI_SET_WINDOW 0x24
#define BH_SCSI_GET_WINDOW 0x25
#define BH_SCSI_READ_SCANNED_DATA 0x28
#define BH_SCSI_INQUIRY	0x12
#define BH_SCSI_MODE_SELECT 0x15
#define BH_SCSI_START_SCAN 0x1b
#define BH_SCSI_MODE_SENSE 0x1a
#define BH_SCSI_GET_BUFFER_STATUS 0x34
#define BH_SCSI_OBJECT_POSITION 0x31

/* page codes used with BH_SCSI_INQUIRY */
#define BH_INQUIRY_STANDARD_PAGE_CODE 0x00
#define BH_INQUIRY_VPD_PAGE_CODE 0xC0
#define BH_INQUIRY_JIS_PAGE_CODE 0xF0

/* page codes used with BH_SCSI_MODE_SELECT and BH_SCSI_MODE_SENSE */
#define BH_MODE_MEASUREMENT_PAGE_CODE 0x03
#define BH_MODE_TIMEOUT_PAGE_CODE 0x20
#define BH_MODE_ICON_PAGE_CODE 0x21
#define BH_MODE_BARCODE_PRIORITY_PAGE_CODE 0x30
#define BH_MODE_BARCODE_PARAM1_PAGE_CODE 0x31
#define BH_MODE_BARCODE_PARAM2_PAGE_CODE 0x32
#define BH_MODE_BARCODE_PARAM3_PAGE_CODE 0x32

/* data type codes used with BH_SCSI_READ_SCANNED_DATA */
#define BH_SCSI_READ_TYPE_FRONT 0x80
/* 0x81 thru 0x88 read front page sections 1 thru 8 respectively */
#define BH_SCSI_READ_TYPE_BACK 0x90
/* 0x91 thru 0x98 read back page sections 1 thru 8 respectively */
#define BH_SCSI_READ_TYPE_FRONT_BARCODE 0xA0
/* 0xA1 thru 0xA8 read front page barcodes in sections 1 thru 8 respectively */
#define BH_SCSI_READ_TYPE_BACK_BARCODE 0xB0
/* 0xB1 thru 0xB8 read back page barcodes in sections 1 thru 8 respectively */
#define BH_SCSI_READ_TYPE_FRONT_PATCHCODE 0xC0
/* 0xC1 thru 0xC8 read front page patchcodes in sections 1 thru 8 respectively */
#define BH_SCSI_READ_TYPE_BACK_PATCHCODE 0xD0
/* 0xD1 thru 0xD8 read back page patchcodes in sections 1 thru 8 respectively */
#define BH_SCSI_READ_TYPE_FRONT_ICON 0x89
#define BH_SCSI_READ_TYPE_BACK_ICON 0x99

/* this one is not a real readtype; it's used to help transfer the barcode file */
#define BH_SCSI_READ_TYPE_SENDBARFILE 0xBB

#define BH_HAS_IMAGE_DATA(i) ((i) >= BH_SCSI_READ_TYPE_FRONT && \
   (i) <= BH_SCSI_READ_TYPE_BACK_ICON)

/* batchmode codes used with BH_SCSI_SET_WINDOW */
#define BH_BATCH_DISABLE 0x00
#define BH_BATCH_ENABLE 0x01
#define BH_BATCH_TERMINATE 0x02
#define BH_BATCH_ABORT 0x03

/* deskew mode codes used with BH_SCSI_SET_WINDOW */
#define BH_DESKEW_DISABLE 0x00       /* border detection is assumed, see page 3-37 of 8000 manual */
#define BH_DESKEW_ENABLE 0x04        /* deskew and border detection */

/* used with BH_SCSI_SET_WINDOW, BH_SCSI_GET_WINDOW */
typedef struct _BH_SectionBlock {
  SANE_Byte ul_x[4];
  SANE_Byte ul_y[4];
  SANE_Byte width[4];
  SANE_Byte length[4];
  SANE_Byte compressiontype;
  SANE_Byte compressionarg;
  SANE_Byte reserved[6];
} BH_SectionBlock;

/* used with BH_SCSI_SET_WINDOW, BH_SCSI_GET_WINDOW */
struct window_data {           /* window descriptor block byte layout */
  SANE_Byte windowid;          /* 0 */
  SANE_Byte autoborder;        /* 1 */
  SANE_Byte xres[2];           /* 2,3 */
  SANE_Byte yres[2];           /* 4,5 */
  SANE_Byte ulx[4];            /* 6-9 */
  SANE_Byte uly[4];            /* 10-13 */
  SANE_Byte windowwidth[4];    /* 14-17 */
  SANE_Byte windowlength[4];   /* 18-21 */
  SANE_Byte brightness;        /* 22 */
  SANE_Byte threshold;         /* 23 */
  SANE_Byte contrast;          /* 24 */
  SANE_Byte imagecomposition;  /* 25 */
  SANE_Byte bitsperpixel;      /* 26 */
  SANE_Byte halftonecode;      /* 27 */
  SANE_Byte halftoneid;        /* 28 */
  SANE_Byte paddingtype;       /* 29 */
  SANE_Byte bitordering[2];    /* 30,31 */
  SANE_Byte compressiontype;   /* 32 */
  SANE_Byte compressionarg;    /* 33 */
  SANE_Byte reserved2[6];      /* 34-39 */
  SANE_Byte remote;            /* 40 */
  SANE_Byte acefunction;       /* 41 */
  SANE_Byte acesensitivity;    /* 42 */
  SANE_Byte batchmode;         /* 43 */
  SANE_Byte reserved3[2];      /* 44,45 */
  SANE_Byte border_rotation;   /* 46     added this for copiscan 8080 */
  SANE_Byte reserved4[17];     /* 47-63  added this for copiscan 8080 */
  BH_SectionBlock sectionblock[NUM_SECTIONS];
};

/* used with BH_SCSI_READ_SCANNED_DATA */
/* structure for returned decoded barcode data */
struct barcode_data {
  SANE_Byte reserved1[2];
  SANE_Byte barcodetype[2];
  SANE_Byte statusflag[2];
  SANE_Byte barcodeorientation[2];
  SANE_Byte posxa[2];
  SANE_Byte posya[2];
  SANE_Byte posxb[2];
  SANE_Byte posyb[2];
  SANE_Byte posxc[2];
  SANE_Byte posyc[2];
  SANE_Byte posxd[2];
  SANE_Byte posyd[2];
  SANE_Byte barcodesearchtime[2];
  SANE_Byte reserved2[13];
  SANE_Byte barcodelen;
  SANE_Byte barcodedata[160];
};

/* structure for returned icon data block */
struct icon_data {
  SANE_Byte windowwidth[4];
  SANE_Byte windowlength[4];
  SANE_Byte iconwidth[4];
  SANE_Byte iconwidthbytes[4];
  SANE_Byte iconlength[4];
  SANE_Byte bitordering;
  SANE_Byte reserved[7];
  SANE_Byte icondatalen[4];
};


/* used with BH_SCSI_INQUIRY */

/* Standard Data [EVPD=0] */
struct inquiry_standard_data {
  SANE_Byte devtype;
  SANE_Byte reserved[7];
  SANE_Byte vendor[8];
  SANE_Byte product[16];
  SANE_Byte revision[4];
};

/* VPD Information [EVPD=1, PageCode=C0H] */
struct inquiry_vpd_data {
  SANE_Byte devtype;
  SANE_Byte pagecode;
  SANE_Byte reserved1;
  SANE_Byte alloclen;
  SANE_Byte adf;
  SANE_Byte reserved2[2];
  SANE_Byte imagecomposition;
  SANE_Byte imagedataprocessing[2];
  SANE_Byte compression;
  SANE_Byte reserved3;
  SANE_Byte sizerecognition;
  SANE_Byte optionalfeatures;
  SANE_Byte xmaxoutputbytes[2];
};


/* JIS Information [EVPD=1, PageCode=F0H] */
struct inquiry_jis_data {
  SANE_Byte devtype;
  SANE_Byte pagecode;
  SANE_Byte jisversion;
  SANE_Byte reserved1;
  SANE_Byte alloclen;
  SANE_Byte basicxres[2];
  SANE_Byte basicyres[2];
  SANE_Byte resolutionstep;
  SANE_Byte maxxres[2];
  SANE_Byte maxyres[2];
  SANE_Byte minxres[2];
  SANE_Byte minyres[2];
  SANE_Byte standardres[2];
  SANE_Byte windowwidth[4];
  SANE_Byte windowlength[4];
  SANE_Byte functions;
  SANE_Byte reserved2;
};

/* used with BH_SCSI_MODE_SELECT and BH_SCSI_MODE_SENSE */

/* Scanning Measurement Parameters PageCode=03H */
struct mode_page_03 {
  SANE_Byte modedatalen;
  SANE_Byte mediumtype;
  SANE_Byte devicespecificparam;
  SANE_Byte blockdescriptorlen;
  
  SANE_Byte pagecode;
  SANE_Byte paramlen;
  SANE_Byte bmu;
  SANE_Byte reserved1;
  SANE_Byte mud[2];
  SANE_Byte reserved2[2];
};

/* Scan Command Timeout PageCode=20H */
struct mode_page_20 {
  SANE_Byte modedatalen;
  SANE_Byte mediumtype;
  SANE_Byte devicespecificparam;
  SANE_Byte blockdescriptorlen;

  SANE_Byte pagecode;
  SANE_Byte paramlen;
  SANE_Byte timeoutmanual;
  SANE_Byte timeoutadf;
  SANE_Byte reserved[4];
};

/* Icon Definition PageCode=21H */
struct mode_page_21 {
  SANE_Byte modedatalen;
  SANE_Byte mediumtype;
  SANE_Byte devicespecificparam;
  SANE_Byte blockdescriptorlen;

  SANE_Byte pagecode;
  SANE_Byte paramlen;
  SANE_Byte iconwidth[2];
  SANE_Byte iconlength[2];
  SANE_Byte reserved[2];
};

/* Bar/Patch Code search priority order PageCode=30H */
struct mode_page_30 {
  SANE_Byte modedatalen;
  SANE_Byte mediumtype;
  SANE_Byte devicespecificparam;
  SANE_Byte blockdescriptorlen;

  SANE_Byte pagecode;
  SANE_Byte paramlen;
  SANE_Byte priority[6];
};

/* Bar/Patch Code search parameters 1 of 3 PageCode=31H */
struct mode_page_31 {
  SANE_Byte modedatalen;
  SANE_Byte mediumtype;
  SANE_Byte devicespecificparam;
  SANE_Byte blockdescriptorlen;

  SANE_Byte pagecode;
  SANE_Byte paramlen;
  SANE_Byte minbarheight[2];
  SANE_Byte searchcount;
  SANE_Byte searchmode;
  SANE_Byte searchtimeout[2];
};

/* Bar/Patch Code search parameters 2 of 3 PageCode=32H */
struct mode_page_32 {
  SANE_Byte modedatalen;
  SANE_Byte mediumtype;
  SANE_Byte devicespecificparam;
  SANE_Byte blockdescriptorlen;

  SANE_Byte pagecode;
  SANE_Byte paramlen;
  SANE_Byte relmax[2];
  SANE_Byte barmin[2];
  SANE_Byte barmax[2];
};

/* Bar/Patch Code search parameters 3 of 3 PageCode=33H */
struct mode_page_33 {
  SANE_Byte modedatalen;
  SANE_Byte mediumtype;
  SANE_Byte devicespecificparam;
  SANE_Byte blockdescriptorlen;

  SANE_Byte pagecode;
  SANE_Byte paramlen;
  SANE_Byte barcodecontrast[2];
  SANE_Byte patchmode[2];
  SANE_Byte reserved[2];
};

#ifndef sane_isbasicframe
#define SANE_FRAME_TEXT 10
#define SANE_FRAME_JPEG 11
#define SANE_FRAME_G31D 12
#define SANE_FRAME_G32D 13
#define SANE_FRAME_G42D 14
#define sane_strframe(f) ( (f) == SANE_FRAME_GRAY ? "gray" : \
  (f) == SANE_FRAME_RGB ? "RGB" : \
  (f) == SANE_FRAME_RED ? "red" : \
  (f) == SANE_FRAME_GREEN ? "green" : \
  (f) == SANE_FRAME_BLUE ? "blue" : \
  (f) == SANE_FRAME_TEXT ? "text" : \
  (f) == SANE_FRAME_JPEG ? "jpeg" : \
  (f) == SANE_FRAME_G31D ? "g31d" : \
  (f) == SANE_FRAME_G32D ? "g32d" : \
  (f) == SANE_FRAME_G42D ? "g42d" : \
  "unknown" )

#define sane_isbasicframe(f) ( (f) == SANE_FRAME_GRAY || \
  (f) == SANE_FRAME_RGB || \
  (f) == SANE_FRAME_RED || \
  (f) == SANE_FRAME_GREEN || \
  (f) == SANE_FRAME_BLUE )

#endif

#endif /* BH_H */
