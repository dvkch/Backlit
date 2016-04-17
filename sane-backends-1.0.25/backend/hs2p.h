/* sane - Scanner Access Now Easy.
   Copyright (C) 2007 Jeremy Johnson
   This file is part of a SANE backend for Ricoh IS450
   and IS420 family of HS2P Scanners using the SCSI controller.
   
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
   If you do not wish that, delete this exception notice. */

#ifndef HS2P_H
#define HS2P_H 1

#include <sys/types.h>
#include "../include/sane/config.h"

#include "hs2p-scsi.h"
#include "hs2p-saneopts.h"

#define HS2P_CONFIG_FILE "hs2p.conf"

#define DBG_error0       0
#define DBG_error        1
#define DBG_sense        2
#define DBG_warning      3
#define DBG_inquiry      4
#define DBG_info         5
#define DBG_info2        6
#define DBG_proc         7
#define DBG_read         8
#define DBG_sane_init   10
#define DBG_sane_proc   11
#define DBG_sane_info   12
#define DBG_sane_option 13

typedef struct
{
  const char *mfg;
  const char *model;
} HS2P_HWEntry;

enum CONNECTION_TYPES
{ CONNECTION_SCSI = 0, CONNECTION_USB };

enum media
{ FLATBED = 0x00, SIMPLEX, DUPLEX };


typedef struct data
{
  size_t bufsize;
  /* 00H IMAGE */
  /* 01H RESERVED */
  /* 02H Halftone Mask  */
  SANE_Byte gamma[256];		/* 03H Gamma Function */
  /* 04H - 7FH Reserved */
  SANE_Byte endorser[19];	/* 80H Endorser */
  SANE_Byte size;		/* 81H startpos(4bits) + width(4bits) */
  /* 82H Reserved */
  /* 83H Reserved (Vendor Unique) */
  SANE_Byte nlines[5];		/* 84H Page Length */
  MAINTENANCE_DATA maintenance;	/* 85H */
  SANE_Byte adf_status;		/* 86H */
  /* 87H Reserved (Skew Data) */
  /* 88H-91H Reserved (Vendor Unique) */
  /* 92H Reserved (Scanner Extension I/O Access) */
  /* 93H Reserved (Vendor Unique) */
  /* 94H-FFH Reserved (Vendor Unique) */
} HS2P_DATA;

typedef struct
{
  SANE_Range xres_range;
  SANE_Range yres_range;
  SANE_Range x_range;
  SANE_Range y_range;

  SANE_Int window_width;
  SANE_Int window_height;

  SANE_Range brightness_range;
  SANE_Range contrast_range;
  SANE_Range threshold_range;

  char inquiry_data[256];

  SANE_Byte max_win_sections;	/* Number of supported window subsections 
				   IS450 supports max of 4 sections 
				   IS420 supports max of 6 sections
				 */

  /* Defaults */
  SANE_Int default_res;
  SANE_Int default_xres;
  SANE_Int default_yres;
  SANE_Int default_imagecomposition;	/* [lineart], halftone, grayscale, color */
  SANE_Int default_media;	/* [flatbed], simplex, duplex */
  SANE_Int default_paper_size;	/* [letter], legal, ledger, ... */
  SANE_Int default_brightness;
  SANE_Int default_contrast;
  SANE_Int default_gamma;	/* Normal, Soft, Sharp, Linear, User */
  SANE_Bool default_adf;
  SANE_Bool default_duplex;
  /*
     SANE_Bool default_border;
     SANE_Bool default_batch;
     SANE_Bool default_deskew;
     SANE_Bool default_check_adf;
     SANE_Int  default_timeout_adf;
     SANE_Int  default_timeout_manual;
     SANE_Bool default_control_panel;
   */

  /* Mode Page Parameters */
  MP_CXN cxn;			/* hdr + Connection Parameters */

  SANE_Int bmu;
  SANE_Int mud;
  SANE_Int white_balance;	/* 00H Relative, 01H Absolute; power on default is relative */
  /* Lamp Timer not supported */
  SANE_Int adf_control;		/* 00H None, 01H Book, 01H Simplex, 02H Duplex */
  SANE_Int adf_mode_control;	/* bit2: prefeed mode invalid: "0" : valid "1" */
  /* Medium Wait Timer not supported */
  SANE_Int endorser_control;	/* Default Off when power on */
  SANE_Char endorser_string[20];
  SANE_Bool scan_wait_mode;	/* wait for operator panel start button to be pressed */
  SANE_Bool service_mode;	/* power on default self_diagnostics 00H; 01H optical_adjustment */

  /* standard information: EVPD bit is 0 */
  SANE_Byte devtype;		/* devtype[6]="scanner" */
  SANE_Char vendor[9];		/* model name 8+1 */
  SANE_Char product[17];	/* product name 16+1 */
  SANE_Char revision[5];	/* revision 4+1 */

  /* VPD information: EVPD bit is 1, Page Code=C0H */
  /* adf_id: 0: No ADF
   *         1: Single-sided ADF
   *         2: Double-sided ADF
   *         3: ARDF (Reverse double-sided ADF)
   *         4: Reserved
   */

  SANE_Bool hasADF;		/* If YES; can either be one of Simplex,Duplex,ARDF */
  SANE_Bool hasSimplex;
  SANE_Bool hasDuplex;
  SANE_Bool hasARDF;

  SANE_Bool hasEndorser;

  SANE_Bool hasIPU;
  SANE_Bool hasXBD;

  /* VPD Image Composition */
  SANE_Bool supports_lineart;
  SANE_Bool supports_dithering;
  SANE_Bool supports_errordiffusion;
  SANE_Bool supports_color;
  SANE_Bool supports_4bitgray;
  SANE_Bool supports_8bitgray;

  /* VPD Image Data Processing ACE (supported for IS420) */
  SANE_Bool supports_whiteframing;
  SANE_Bool supports_blackframing;
  SANE_Bool supports_edgeextraction;
  SANE_Bool supports_noiseremoval;	/* supported for IS450 if IPU installed */
  SANE_Bool supports_smoothing;	/* supported for IS450 if IPU installed */
  SANE_Bool supports_linebolding;

  /* VPD Compression (not supported for IS450) */
  SANE_Bool supports_MH;
  SANE_Bool supports_MR;
  SANE_Bool supports_MMR;
  SANE_Bool supports_MHB;

  /* VPD Marker Recognition (not supported for IS450) */
  SANE_Bool supports_markerrecognition;

  /* VPD Size Recognition (supported for IS450 if IPU installed) */
  SANE_Bool supports_sizerecognition;

  /* VPD X Maximum Output Pixel: IS450:4960   IS420:4880 */
  SANE_Int xmaxoutputpixels;

  /* jis information VPD IDENTIFIER Page Code F0H */
  SANE_Int resBasicX;		/* basic X resolution */
  SANE_Int resBasicY;		/* basic Y resolution */
  SANE_Int resXstep;		/* resolution step in main scan direction */
  SANE_Int resYstep;		/* resolution step in sub scan direction */
  SANE_Int resMaxX;		/* maximum X resolution */
  SANE_Int resMaxY;		/* maximum Y resolution */
  SANE_Int resMinX;		/* minimum X resolution */
  SANE_Int resMinY;		/* minimum Y resolution */
  SANE_Int resStdList[16 + 1];	/* list of available standard resolutions (first slot is the length) */
  SANE_Int winWidth;		/* length of window (in BasicX res DPI) */
  SANE_Int winHeight;		/* height of window (in BasicY res DPI) */
  /* jis.functions duplicates vpd.imagecomposition lineart/dither/grayscale */
  SANE_Bool overflow_support;
  SANE_Bool lineart_support;
  SANE_Bool dither_support;
  SANE_Bool grayscale_support;

} HS2P_Info;

typedef struct HS2P_Device
{
  struct HS2P_Device *next;
  /*
   * struct with pointers to device/vendor/model names, and a type value
   * used to inform sane frontend about the device 
   */
  SANE_Device sane;
  HS2P_Info info;
  SENSE_DATA sense_data;
} HS2P_Device;

#define GAMMA_LENGTH 256
typedef struct HS2P_Scanner
{
  /* all the state needed to define a scan request: */
  struct HS2P_Scanner *next;	/* linked list for housekeeping */
  int fd;			/* SCSI filedescriptor */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during reading of config file.         */
  int buffer_size;		/* for sanei_open */
  int connection;		/* hardware interface type */


  /* SANE option descriptors and values */
  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];
  SANE_Parameters params;	/* SANE image parameters */
  /* additional values that don't fit into Option_Value representation */
  SANE_Word gamma_table[GAMMA_LENGTH];	/* Custom Gray Gamma Table */

  /* state information - not options */

  /* scanner dependent/low-level state: */
  HS2P_Device *hw;

  SANE_Int bmu;			/* Basic Measurement Unit       */
  SANE_Int mud;			/* Measurement Unit Divisor     */
  SANE_Byte image_composition;	/* LINEART, HALFTONE, GRAYSCALE */
  SANE_Byte bpp;		/* 1,4,6,or 8 Bits Per Pixel    */


  u_long InvalidBytes;
  size_t bytes_to_read;
  SANE_Bool cancelled;
  /*SANE_Bool backpage; */
  SANE_Bool scanning;
  SANE_Bool another_side;
  SANE_Bool EOM;

  HS2P_DATA data;
} HS2P_Scanner;

static const SANE_Range u8_range = {
  0,				/* minimum */
  255,				/* maximum */
  0				/* quantization */
};
static const SANE_Range u16_range = {
  0,				/* minimum */
  65535,			/* maximum */
  0				/* quantization */
};

#define SM_LINEART        SANE_VALUE_SCAN_MODE_LINEART
#define SM_HALFTONE       SANE_VALUE_SCAN_MODE_HALFTONE
#define SM_DITHER         "Dither"
#define SM_ERRORDIFFUSION "Error Diffusion"
#define SM_COLOR          SANE_VALUE_SCAN_MODE_COLOR
#define SM_4BITGRAY       "4-Bit Gray"
#define SM_6BITGRAY       "6-Bit Gray"
#define SM_8BITGRAY       "8-Bit Gray"
static SANE_String scan_mode_list[9];
enum
{ FB, ADF };
static SANE_String_Const scan_source_list[] = {
  "FB",				/* Flatbed */
  "ADF",			/* Automatic Document Feeder */
  NULL
};
static SANE_String compression_list[6];	/* "none", "g31d MH", "g32d MR", "g42d MMR", "MH byte boundary", NULL} */

typedef struct
{
  SANE_String name;
  double width, length;		/* paper dimensions in mm */
} HS2P_Paper;
/* list of support paper sizes */
/* 'custom' MUST be item 0; otherwise a width or length of 0 indicates
 * the maximum value supported by the scanner
 */
static const HS2P_Paper paper_sizes[] = {	/* Name, Width, Height in mm */
  {"Custom", 0.0, 0.0},
  {"Letter", 215.9, 279.4},
  {"Legal", 215.9, 355.6},
  {"Ledger", 279.4, 431.8},
  {"A3", 297, 420},
  {"A4", 210, 297},
  {"A4R", 297, 210},
  {"A5", 148.5, 210},
  {"A5R", 210, 148.5},
  {"A6", 105, 148.5},
  {"B4", 250, 353},
  {"B5", 182, 257},
  {"Full", 0.0, 0.0},
};

#define PORTRAIT "Portait"
#define LANDSCAPE "Landscape"
static SANE_String_Const orientation_list[] = {
  PORTRAIT,
  LANDSCAPE,
  NULL				/* sentinel */
};

/* MUST be kept in sync with paper_sizes */
static SANE_String_Const paper_list[] = {
  "Custom",
  "Letter",
  "Legal",
  "Ledger",
  "A3",
  "A4", "A4R",
  "A5", "A5R",
  "A6",
  "B4",
  "B5",
  "Full",
  NULL				/* (not the same as "") sentinel */
};

#if 0
static /* inline */ int _is_host_little_endian (void);
static /* inline */ int
_is_host_little_endian ()
{
  SANE_Int val = 255;
  unsigned char *firstbyte = (unsigned char *) &val;

  return (*firstbyte == 255) ? SANE_TRUE : SANE_FALSE;
}
#endif

static /* inline */ void
_lto2b (u_long val, SANE_Byte * bytes)
{
  bytes[0] = (val >> 8) & 0xff;
  bytes[1] = val & 0xff;
}

static /* inline */ void
_lto3b (u_long val, SANE_Byte * bytes)
{
  bytes[0] = (val >> 16) & 0xff;
  bytes[1] = (val >> 8) & 0xff;
  bytes[2] = val & 0xff;
}

static /* inline */ void
_lto4b (u_long val, SANE_Byte * bytes)
{
  bytes[0] = (val >> 24) & 0xff;
  bytes[1] = (val >> 16) & 0xff;
  bytes[2] = (val >> 8) & 0xff;
  bytes[3] = val & 0xff;
}

static /* inline */ u_long
_2btol (SANE_Byte * bytes)
{
  u_long rv;

  rv = (bytes[0] << 8) | bytes[1];

  return rv;
}

static /* inline */ u_long
_4btol (SANE_Byte * bytes)
{
  u_long rv;

  rv = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];

  return rv;
}

/*
static inline SANE_Int
_2btol(SANE_Byte *bytes)
{
  SANE_Int rv;

  rv = (bytes[0] << 8) | bytes[1];
  return (rv);
}
*/
static inline SANE_Int
_3btol (SANE_Byte * bytes)
{
  SANE_Int rv;

  rv = (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
  return (rv);
}

/*
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
*/
enum adf_ret_bytes
{ ADF_SELECTION = 2, ADF_MODE_CONTROL, MEDIUM_WAIT_TIMER };

#define get_paddingtype_id(s)  (get_list_index( paddingtype_list, (char *)(s) ))
#define get_paddingtype_val(i) (paddingtype[ get_paddingtype_id( (i) ) ].val)
#define get_paddingtype_strndx(v) (get_val_id_strndx(&paddingtype[0], NELEMS(paddingtype), (v)))

#define get_halftone_code_id(s)  (get_list_index( halftone_code, (char *)(s) ))
#define get_halftone_code_val(i) (halftone[get_halftone_code_id( (i) ) ].val)

#define get_halftone_pattern_id(s)  (get_list_index( halftone_pattern_list, (char *)(s) ))
#define get_halftone_pattern_val(i) (halftone[get_halftone_pattern_id( (i) ) ].val)

#define get_auto_binarization_id(s)  (get_list_index( auto_binarization_list, (char *)(s) ))
#define get_auto_binarization_val(i) (auto_binarization[ get_auto_binarization_id( (i) ) ].val)

#define get_auto_separation_id(s)  (get_list_index( auto_separation_list, (char *)(s) ))
#define get_auto_separation_val(i) (auto_separation[ get_auto_separation_id( (i) ) ].val)

#define get_noisematrix_id(s)    (get_list_index( noisematrix_list, (char *)(s) ))
#define get_noisematrix_val(i)   (noisematrix[ get_noisematrix_id( (i) ) ].val)

#define get_grayfilter_id(s)    (get_list_index( grayfilter_list, (char *)(s) ))
#define get_grayfilter_val(i)   (grayfilter[ get_grayfilter_id( (i) ) ].val)

#define get_paper_id(s)       (get_list_index( paper_list, (char *)(s) ))
#define get_compression_id(s) (get_list_index( (const char **)compression_list, (char *)(s) ))
#define get_scan_source_id(s) (get_list_index( (const char **)scan_source_list, (char *)(s) ))

#define reserve_unit(fd) (unit_cmd((fd),HS2P_SCSI_RESERVE_UNIT))
#define release_unit(fd) (unit_cmd((fd),HS2P_SCSI_RELEASE_UNIT))

#define GET SANE_TRUE
#define SET SANE_FALSE

#define get_endorser_control(fd,val)  (endorser_control( (fd), (val), GET ))
#define set_endorser_control(fd,val)  (endorser_control( (fd), (val), SET ))

#define get_connection_parameters(fd,parm)  (connection_parameters( (fd), (parm), GET ))
#define set_connection_parameters(fd,parm)  (connection_parameters( (fd), (parm), SET ))

#define get_adf_control(fd, a, b, c)      (adf_control( (fd), GET, (a), (b), (c) ))
#define set_adf_control(fd, a, b, c)      (adf_control( (fd), SET, (a), (b), (c) ))

#define RELATIVE_WHITE 0x00
#define ABSOLUTE_WHITE 0x01
#define get_white_balance(fd,val)  (white_balance( (fd), (val), GET ))
#define set_white_balance(fd,val)  (white_balance( (fd), (val), SET ))

#define get_scan_wait_mode(fd)      (scan_wait_mode( (fd),     0, GET ))
#define set_scan_wait_mode(fd,val)  (scan_wait_mode( (fd), (val), SET ))

#define get_service_mode(fd)      (service_mode( (fd),     0, GET ))
#define set_service_mode(fd,val)  (service_mode( (fd), (val), SET ))

#define isset_ILI(sd) ( ((sd).sense_key & 0x20) != 0)
#define isset_EOM(sd) ( ((sd).sense_key & 0x40) != 0)


#endif /* HS2P_H */
