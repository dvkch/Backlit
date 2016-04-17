/* sane - Scanner Access Now Easy.

   Copyright (C) 1998 Milon Firikis based on David Mosberger-Tang previous
   Work on mustek.c file from the SANE package.

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
#ifndef apple_h
#define apple_h

#include <sys/types.h>


/*
Warning: if you uncomment the next line you 'll get
zero functionality. All the scanner specific function
such as sane_read, attach and the others will return
without doing  anything. This way you can run the backend
without an attached scanner just to see if it gets
its control variables in a proper way.

TODO: This could be a nice thing to do as a sane config
option at runtime. This way one can debug the gui-ipc
part of the backend without actually has the scanner.

*/

#if 0
#define NEUTRALIZE_BACKEND
#define APPLE_MODEL_SELECT APPLESCANNER
#endif
#undef CALIBRATION_FUNCTIONALITY
#undef RESERVE_RELEASE_HACK

#ifdef RESERVE_RELEASE_HACK
/* Also Try these with zero */
#define CONTROLLER_SCSI_ID 7
#define SETTHIRDPARTY 0x10
#endif


#define ERROR_MESSAGE	1
#define USER_MESSAGE	5
#define FLOW_CONTROL	50
#define VARIABLE_CONTROL 70
#define DEBUG_SPECIAL	100
#define IO_MESSAGE	110
#define INNER_LOOP	120


/* mode values: */
enum Apple_Modes
  {
  APPLE_MODE_LINEART=0,
  APPLE_MODE_HALFTONE,
  APPLE_MODE_GRAY,
  APPLE_MODE_BICOLOR,
  EMPTY_DONT_USE_IT,
  APPLE_MODE_COLOR
  };
  
enum Apple_Option
  {
    OPT_NUM_OPTS = 0,

    OPT_HWDETECT_GROUP,
    OPT_MODEL,

    OPT_MODE_GROUP,
    OPT_MODE,
    OPT_RESOLUTION,
    OPT_PREVIEW,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */


    OPT_ENHANCEMENT_GROUP,
    /* COMMON				*/
    OPT_BRIGHTNESS,
    OPT_CONTRAST,
    OPT_THRESHOLD,
    
    /* AppleScanner only		*/
    OPT_GRAYMAP,
    OPT_AUTOBACKGROUND,
    OPT_AUTOBACKGROUND_THRESHOLD,

    /* AppleScanner & OneScanner	*/
    OPT_HALFTONE_PATTERN,
    OPT_HALFTONE_FILE,

    /* ColorOneScanner Only		*/
    OPT_VOLT_REF,
    OPT_VOLT_REF_TOP,
    OPT_VOLT_REF_BOTTOM,

    /* misc : advanced			*/
    OPT_MISC_GROUP,

    /* all				*/
    OPT_LAMP,

    /* AppleScanner Only		*/
    OPT_WAIT,

    /* OneScanner only			*/
    OPT_CALIBRATE,
    OPT_SPEED,

    /* OneScanner && ColorOneScanner	*/
    OPT_LED,
    OPT_CCD,
    
    /* ColorOneScanner only		*/

    OPT_MTF_CIRCUIT,
    OPT_ICP,
    OPT_POLARITY,

    /* color group : advanced		*/

    OPT_COLOR_GROUP,


#ifdef CALIBRATION_FUNCTIONALITY

    /* OneScanner			*/
    OPT_CALIBRATION_VECTOR,

    /* ColorOneScanner			*/

    OPT_CALIBRATION_VECTOR_RED,
    OPT_CALIBRATION_VECTOR_GREEN,
    OPT_CALIBRATION_VECTOR_BLUE,
#endif


    /* OneScanner && ColorOneScanner	*/
    OPT_DOWNLOAD_CALIBRATION_VECTOR,

    /* ColorOneScanner			*/

    OPT_CUSTOM_CCT,
    OPT_CCT,
    OPT_DOWNLOAD_CCT,

    OPT_CUSTOM_GAMMA,

    OPT_GAMMA_VECTOR_R,
    OPT_GAMMA_VECTOR_G,
    OPT_GAMMA_VECTOR_B,

    OPT_DOWNLOAD_GAMMA,
    OPT_COLOR_SENSOR,

    /* must come last: */
    NUM_OPTIONS
  };


/* This is a hack to get fast the model of the Attached Scanner	*/
/* But it Works well and I am not considering in "fix" it	*/
enum SCANNERMODEL
  {
    OPT_NUM_SCANNERS = 0,

    APPLESCANNER, ONESCANNER, COLORONESCANNER,
    NUM_SCANNERS
  };

typedef struct Apple_Device
  {
    struct Apple_Device *next;
    SANE_Int ScannerModel;
    SANE_Device sane;
    SANE_Range dpi_range;
    SANE_Range x_range;
    SANE_Range y_range;
    SANE_Int MaxWidth;
    SANE_Int MaxHeight;
    unsigned flags;
  }
Apple_Device;

typedef struct Apple_Scanner
  {
    /* all the state needed to define a scan request: */
    struct Apple_Scanner *next;

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];

    /* First we put here all the scan variables */

    /* These are needed for converting back and forth the scan area */

    SANE_Int bpp;	/* The actual bpp, before scaling */

    double ulx;
    double uly;
    double wx;
    double wy;
    SANE_Int ULx;
    SANE_Int ULy;
    SANE_Int Width;
    SANE_Int Height;

/*
TODO: Initialize this beasts with malloc instead of statically allocation.
*/
    SANE_Int calibration_vector[2550];
    SANE_Int calibration_vector_red[2700];
    SANE_Int calibration_vector_green[2700];
    SANE_Int calibration_vector_blue[2700];
    SANE_Fixed cct3x3[9];
    SANE_Int gamma_table[3][256];
    SANE_Int halftone_pattern[64];

    SANE_Bool scanning;
    SANE_Bool AbortedByUser;
 
    int pass;			/* pass number */
    SANE_Parameters params;

    int fd;			/* SCSI filedescriptor */

    /* scanner dependent/low-level state: */
    Apple_Device *hw;

  }
Apple_Scanner;

#endif /* apple_h */
