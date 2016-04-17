/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 BYTEC GmbH Germany
   Written by Helmut Koeberle, Email: helmut.koeberle@bytec.de
   Modified by Manuel Panea <Manuel.Panea@rzg.mpg.de>,
   Markus Mertinat <Markus.Mertinat@Physik.Uni-Augsburg.DE>,
   and ULrich Deiters <ulrich.deiters@uni-koeln.de>

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

#ifndef canon_h
#define canon_h 1

#ifdef __GNUC__
#define UNUSEDARG __attribute__ ((unused))
#else
#define UNUSEDARG
#endif

/* all the different possible model names. */
#define FB1200S "IX-12015E      "
#define FB620S  "IX-06035E      "
#define CS300   "IX-03035B      "
#define CS600   "IX-06015C      "
#define CS2700F "IX-27015C       "
#define IX_4025 "IX-4025        "
#define IX_4015 "IX-4015        "
#define IX_3010 "IX-3010        "

#include <sys/types.h>

#define AUTO_DOC_FEEDER_UNIT            0x01
#define TRANSPARENCY_UNIT               0x02
#define TRANSPARENCY_UNIT_FB1200        0x03
#define SCAN_CONTROL_CONDITIONS         0x20
#define SCAN_CONTROL_CON_FB1200         0x21
#define ALL_SCAN_MODE_PAGES             0x3F

#define RED   0
#define GREEN 1
#define BLUE  2

#define ADF_STAT_NONE		0
#define ADF_STAT_INACTIVE	1
#define ADF_STAT_ACTIVE		2
#define ADF_STAT_DISABLED	3

#define ADF_Status		(4+2)	/* byte positioning */
#define ADF_Settings		(4+3)	/* in data block    */

#define ADF_NOT_PRESENT		0x01	/* bit selection    */
#define ADF_PROBLEM		0x0E	/* from bytes in    */
#define ADF_PRIORITY		0x03	/* data block.      */
#define ADF_FEEDER		0x04	/*                  */

#define TPU_STAT_NONE		0
#define TPU_STAT_INACTIVE	1
#define TPU_STAT_ACTIVE		2

#define CS3_600  0		/* CanoScan 300/600 */
#define CS2700   1		/* CanoScan 2700F */
#define FB620    2		/* CanoScan FB620S */
#define FS2710   3		/* CanoScan FS2710S */
#define FB1200   4		/* CanoScan FB1200S */
#define IX4015   5		/* IX-4015 */

#ifndef MAX
#define MAX(A,B)	(((A) > (B))? (A) : (B))
#endif
#ifndef MIN
#define MIN(A,B)	(((A) < (B))? (A) : (B))
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX LONG_MAX
#endif

typedef struct
{
  SANE_Int Status;		/* Auto Document Feeder Unit Status */
  SANE_Int Problem;		/* ADF Problems list */
  SANE_Int Priority;		/* ADF Priority setting */
  SANE_Int Feeder;		/* ADF Feeder setting */

}
CANON_ADF;


typedef struct
{
  SANE_Int Status;		/* Transparency Unit Status */
  SANE_Bool PosNeg;		/* Negative/Positive Film */
  SANE_Int Transparency;	/* TPU Transparency */
  SANE_Int ControlMode;		/* TPU Density Control Mode */
  SANE_Int FilmType;		/* TPU Film Type */

}
CANON_TPU;


typedef enum
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_NEGATIVE,			/* Reverse image format */
  OPT_NEGATIVE_TYPE,		/* Negative film type */
  OPT_SCANNING_SPEED,

  OPT_RESOLUTION_GROUP,
  OPT_RESOLUTION_BIND,
  OPT_HW_RESOLUTION_ONLY,
  OPT_X_RESOLUTION,
  OPT_Y_RESOLUTION,

  OPT_ENHANCEMENT_GROUP,
  OPT_BRIGHTNESS,
  OPT_CONTRAST,
  OPT_THRESHOLD,

  OPT_MIRROR,

  OPT_CUSTOM_GAMMA,		/* use custom gamma tables? */
  OPT_CUSTOM_GAMMA_BIND,
  /* The gamma vectors MUST appear in the order gray, red, green, blue. */
  OPT_GAMMA_VECTOR,
  OPT_GAMMA_VECTOR_R,
  OPT_GAMMA_VECTOR_G,
  OPT_GAMMA_VECTOR_B,
  OPT_AE,			/* Auto Exposure */

  OPT_CALIBRATION_GROUP,	/* Calibration for FB620S */
  OPT_CALIBRATION_NOW,		/* Execute Calibration now for FB620S */
  OPT_SCANNER_SELF_DIAGNOSTIC,	/* Self diagnostic for FB620S */
  OPT_RESET_SCANNER,		/* Reset scanner for FB620S */

  OPT_EJECT_GROUP,
  OPT_EJECT_AFTERSCAN,
  OPT_EJECT_BEFOREEXIT,
  OPT_EJECT_NOW,

  OPT_FOCUS_GROUP,
  OPT_AF,			/* Auto Focus */
  OPT_AF_ONCE,			/* Auto Focus only once between ejects */
  OPT_FOCUS,			/* Manual focus position */

  OPT_MARGINS_GROUP,		/* scan margins */
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  OPT_COLORS_GROUP,
  OPT_HNEGATIVE,		/* Reverse image format */
  OPT_BIND_HILO,		/* Same values vor highlight and shadow
				   points for red, green, blue */
  OPT_HILITE_R,			/* highlight point for red   */
  OPT_SHADOW_R,			/* shadow    point for red   */
  OPT_HILITE_G,			/* highlight point for green */
  OPT_SHADOW_G,			/* shadow    point for green */
  OPT_HILITE_B,			/* highlight point for blue  */
  OPT_SHADOW_B,			/* shadow    point for blue  */

  OPT_ADF_GROUP,		/* to allow display of options. */
  OPT_FLATBED_ONLY,		/* in case you have a sheetfeeder
				   but don't want to use it. */

  OPT_TPU_GROUP,
  OPT_TPU_ON,
  OPT_TPU_PN,
  OPT_TPU_DCM,
  OPT_TPU_TRANSPARENCY,
  OPT_TPU_FILMTYPE,

  OPT_PREVIEW,

  /* must come last: */
  NUM_OPTIONS
}
CANON_Option;


typedef struct CANON_Info
{
  int model;

  SANE_Range xres_range;
  SANE_Range yres_range;
  SANE_Range x_range;
  SANE_Range y_range;
  SANE_Range brightness_range;
  SANE_Range contrast_range;
  SANE_Range threshold_range;
  SANE_Range HiliteR_range;
  SANE_Range ShadowR_range;
  SANE_Range HiliteG_range;
  SANE_Range ShadowG_range;
  SANE_Range HiliteB_range;
  SANE_Range ShadowB_range;
  SANE_Range focus_range;

  SANE_Range x_adf_range;
  SANE_Range y_adf_range;
  SANE_Int xres_default;
  SANE_Int yres_default;
  SANE_Int bmu;
  SANE_Int mud;
  SANE_Range TPU_Transparency_range;
  SANE_Int TPU_Stat;

  SANE_Bool can_focus;			/* has got focus control */
  SANE_Bool can_autoexpose;		/* can do autoexposure by hardware */
  SANE_Bool can_calibrate;		/* has got calibration control */
  SANE_Bool can_diagnose;		/* has diagnostic command */
  SANE_Bool can_eject;			/* can eject medium */
  SANE_Bool can_mirror;			/* can mirror image by hardware */
  SANE_Bool is_filmscanner;
  SANE_Bool has_fixed_resolutions;	/* only a finite number possible */
}
CANON_Info;

typedef struct CANON_Device
{
  struct CANON_Device *next;
  SANE_Device sane;
  CANON_Info info;
  CANON_ADF adf;
  CANON_TPU tpu;
}
CANON_Device;

typedef struct CANON_Scanner
{
  struct CANON_Scanner *next;
  int fd;
  CANON_Device *hw;
  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];
  char *sense_str;		/* sense string */
  SANE_Int gamma_table[4][256];
  SANE_Parameters params;
  SANE_Bool AF_NOW;		/* To keep track of when to do AF */

  SANE_Int xres;
  SANE_Int yres;
  SANE_Int ulx;
  SANE_Int uly;
  SANE_Int width;
  SANE_Int length;
  SANE_Int brightness;
  SANE_Int contrast;
  SANE_Int threshold;
  SANE_Int image_composition;
  SANE_Int bpp;
  SANE_Bool RIF;		/* Reverse Image Format */
  SANE_Int negative_filmtype;
  SANE_Int scanning_speed;
  SANE_Bool GRC;		/* Gray Response Curve  */
  SANE_Bool Mirror;
  SANE_Bool AE;			/* Auto Exposure */
  SANE_Int HiliteR;
  SANE_Int ShadowR;
  SANE_Int HiliteG;
  SANE_Int ShadowG;
  SANE_Int HiliteB;
  SANE_Int ShadowB;

  /* 990320, ss: array for fixed resolutions */
  SANE_Word xres_word_list[16];
  SANE_Word yres_word_list[16];

  SANE_Byte *inbuffer;		/* modification for FB620S */
  SANE_Byte *outbuffer;		/* modification for FB620S */
  SANE_Int buf_used;		/* modification for FB620S */
  SANE_Int buf_pos;		/* modification for FB620S */
  time_t time0;			/* modification for FB620S */
  time_t time1;			/* modification for FB620S */
  int switch_preview;		/* modification for FB620S */
  int reset_flag;		/* modification for FB620S */

  int tmpfile;		        /* modification for FB1200S */

  size_t bytes_to_read;
  int scanning;

  u_char gamma_map[4][4096];	/* for FS2710S: */
  int colour;			/* index to gamma_map */
  int auxbuf_len;		/* size of auxiliary buffer */
  u_char *auxbuf;
}
CANON_Scanner;

static char *option_name[] = {
  "OPT_NUM_OPTS",

  "OPT_MODE_GROUP",
  "OPT_MODE",
  "OPT_NEGATIVE",
  "OPT_NEGATIVE_TYPE",
  "OPT_SCANNING_SPEED",

  "OPT_RESOLUTION_GROUP",
  "OPT_RESOLUTION_BIND",
  "OPT_HW_RESOLUTION_ONLY",
  "OPT_X_RESOLUTION",
  "OPT_Y_RESOLUTION",

  "OPT_ENHANCEMENT_GROUP",
  "OPT_BRIGHTNESS",
  "OPT_CONTRAST",
  "OPT_THRESHOLD",

  "OPT_MIRROR",

  "OPT_CUSTOM_GAMMA",
  "OPT_CUSTOM_GAMMA_BIND",
  "OPT_GAMMA_VECTOR",
  "OPT_GAMMA_VECTOR_R",
  "OPT_GAMMA_VECTOR_G",
  "OPT_GAMMA_VECTOR_B",
  "OPT_AE",

  "OPT_CALIBRATION_GROUP",
  "OPT_CALIBRATION_NOW",
  "OPT_SCANNER_SELF_DIAGNOSTIC",
  "OPT_RESET_SCANNER",

  "OPT_EJECT_GROUP",
  "OPT_EJECT_AFTERSCAN",
  "OPT_EJECT_BEFOREEXIT",
  "OPT_EJECT_NOW",

  "OPT_FOCUS_GROUP",
  "OPT_AF",
  "OPT_AF_ONCE",
  "OPT_FOCUS",

  "OPT_MARGINS_GROUP",
  "OPT_TL_X",
  "OPT_TL_Y",
  "OPT_BR_X",
  "OPT_BR_Y",

  "OPT_COLORS_GROUP",
  "OPT_HNEGATIVE",
  "OPT_BIND_HILO",

  "OPT_HILITE_R",
  "OPT_SHADOW_R",
  "OPT_HILITE_G",
  "OPT_SHADOW_G",
  "OPT_HILITE_B",
  "OPT_SHADOW_B",

  "OPT_ADF_GROUP",
  "OPT_FLATBED_ONLY",

  "OPT_TPU_GROUP",
  "OPT_TPU_ON",
  "OPT_TPU_PN",
  "OPT_TPU_DCM",
  "OPT_TPU_TRANSPARENCY",
  "OPT_TPU_FILMTYPE",

  "OPT_PREVIEW",

  "NUM_OPTIONS"
};




#endif /* not canon_h */
