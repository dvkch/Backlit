/* sane - Scanner Access Now Easy.

   Copyright (C) 2007-2012 stef.dev@free.fr
   
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
*/

#ifndef RTS8891_LOW_H
#define RTS8891_LOW_H

#include <stddef.h>
#include "../include/sane/sane.h"

#define DBG_error0      0	/* errors/warnings printed even with devuglevel 0 */
#define DBG_error       1	/* fatal errors */
#define DBG_init        2	/* initialization and scanning time messages */
#define DBG_warn        3	/* warnings and non-fatal errors */
#define DBG_info        4	/* informational messages */
#define DBG_proc        5	/* starting/finishing functions */
#define DBG_io          6	/* io functions */
#define DBG_io2         7	/* io functions that are called very often */
#define DBG_data        8	/* log image data */


/* Flags */
#define RTS8891_FLAG_UNTESTED               (1 << 0)	/* Print a warning for these scanners */
#define RTS8891_FLAG_EMULATED_GRAY_MODE     (2 << 0)	/* gray scans are emulated using comor modes */

#define LOWORD(x)  ((uint16_t)(x & 0xffff))
#define HIWORD(x)  ((uint16_t)(x >> 16))
#define LOBYTE(x)  ((uint8_t)((x) & 0xFF))
#define HIBYTE(x)  ((uint8_t)((x) >> 8))

#define MAX_SCANNERS    32
#define MAX_RESOLUTIONS 16

#define SENSOR_TYPE_BARE	0	/* sensor for hp4470 sold bare     */
#define SENSOR_TYPE_XPA		1	/* sensor for hp4470 sold with XPA */
#define SENSOR_TYPE_4400	2	/* sensor for hp4400               */
#define SENSOR_TYPE_4400_BARE	3	/* sensor for hp4400               */
#define SENSOR_TYPE_MAX         3       /* maximum sensor number value     */

/* Forward typedefs */
typedef struct Rts8891_Device Rts8891_Device;

#define SET_DOUBLE(regs,idx,value) regs[idx]=(SANE_Byte)((value)>>8); regs[idx-1]=(SANE_Byte)((value) & 0xff);
/* 
 * defines for RTS8891 registers name
 */
#define BUTTONS_REG2            0x1a
#define LINK_REG                0xb1
#define LAMP_REG                0xd9
#define LAMP_BRIGHT_REG         0xda

/* double reg (E6,E5) -> timing doubles when y resolution doubles
 * E6 is high byte, possibly exposure */
#define EXPOSURE_REG            0xe6


#define TIMING_REG              0x81
#define TIMING1_REG             0x83     /* holds REG8180+1 */
#define TIMING2_REG             0x8a     /* holds REG8180+2 */


/* this struc describes a particular model which is handled by the backend */
/* available resolutions, physical goemetry, scanning area, ... */
typedef struct Rts8891_Model
{
  SANE_String_Const name;
  SANE_String_Const vendor;
  SANE_String_Const product;
  SANE_String_Const type;

  SANE_Int xdpi_values[MAX_RESOLUTIONS];	/* possible x resolutions */
  SANE_Int ydpi_values[MAX_RESOLUTIONS];	/* possible y resolutions */

  SANE_Int max_xdpi;		/* physical maximum x dpi */
  SANE_Int max_ydpi;		/* physical maximum y dpi */
  SANE_Int min_ydpi;		/* physical minimum y dpi */

  SANE_Fixed x_offset;		/* Start of scan area in mm */
  SANE_Fixed y_offset;		/* Start of scan area in mm */
  SANE_Fixed x_size;		/* Size of scan area in mm */
  SANE_Fixed y_size;		/* Size of scan area in mm */

  SANE_Fixed x_offset_ta;	/* Start of scan area in TA mode in mm */
  SANE_Fixed y_offset_ta;	/* Start of scan area in TA mode in mm */
  SANE_Fixed x_size_ta;		/* Size of scan area in TA mode in mm */
  SANE_Fixed y_size_ta;		/* Size of scan area in TA mode in mm */

  /* Line-distance correction (in pixel at max optical dpi) for CCD scanners */
  SANE_Int ld_shift_r;		/* red */
  SANE_Int ld_shift_g;		/* green */
  SANE_Int ld_shift_b;		/* blue */
  
  /* default sensor type */
  SANE_Int sensor;

  /* default gamma table */
  SANE_Word gamma[256];
  SANE_Int buttons;		/* number of buttons for the scanner */
  char *button_name[11];	/* option names for buttons */
  char *button_title[11];	/* option titles for buttons */
  SANE_Word flags;		/* allow per model behaviour control */
} Rts8891_Model;


/**
 * device specific configuration structure to hold option values */
typedef struct Rts8891_Config
{
  /**< index number in device table to override detection */
  SANE_Word modelnumber;

  /**< id of the snedor type, must match SENSOR_TYPE_* defines */
  SANE_Word sensornumber;

  /**< if true, use release/acquire to allow the same device
   * to be used by several frontends */
  SANE_Bool allowsharing;
} Rts8891_Config;


/**
 * device descriptor
 */
struct Rts8891_Device
{
  /**< Next device in linked list */
  struct Rts8891_Device *next;

  /**< USB device number for libusb */
  SANE_Int devnum;
  SANE_String file_name;
  Rts8891_Model *model;		/* points to a structure that decribes model specifics */

  SANE_Int sensor;		/* sensor id */

  SANE_Bool initialized;	/* true if device has been intialized */
  SANE_Bool needs_warming;	/* true if device needs warming up    */
  SANE_Bool parking;	        /* true if device is parking head     */

  /* values detected during find origin */
  /* TODO these are currently unused after detection */
  SANE_Int left_offset;		/* pixels to skip to be on left start of the scanning area */
  SANE_Int top_offset;		/* lines to skip to be at top of the scanning area */

  /* gains from calibration */
  SANE_Int red_gain;
  SANE_Int green_gain;
  SANE_Int blue_gain;

  /* offsets from calibration */
  SANE_Int red_offset;
  SANE_Int green_offset;
  SANE_Int blue_offset;

  /* actual dpi used at hardware level may differ from the one
   * at SANE level */
  SANE_Int xdpi;
  SANE_Int ydpi;

  /* the effective scan area at hardware level may be different from
   * the one at the SANE level*/
  SANE_Int lines;		/* lines to scan */
  SANE_Int pixels;		/* width of scan area */
  SANE_Int bytes_per_line;	/* number of bytes per line */
  SANE_Int xstart;		/* x start coordinate */
  SANE_Int ystart;		/* y start coordinate */

  /* line distance shift for the active scan */
  SANE_Int lds_r;
  SANE_Int lds_g;
  SANE_Int lds_b;

  /* threshold to give 0/1 nit in lineart */
  SANE_Int threshold;

  /* max value from lds_r, lds_g and lds_b */
  SANE_Int lds_max;

  /* amount of data needed to correct ripple effect at highest dpi */
  SANE_Int ripple;

  /* register set of the scanner */
  SANE_Int reg_count;
  SANE_Byte regs[255];

  /* shading calibration data */
  SANE_Byte *shading_data;

  /* data buffer read from scanner */
  SANE_Byte *scanned_data;

  /* size of the buffer */
  SANE_Int data_size;

  /* start of the data within scanned data */
  SANE_Byte *start;

  /* current pointer within scanned data */
  SANE_Byte *current;

  /* end of the data buffer */
  SANE_Byte *end;

  /**
   * amount of bytes read from scanner
   */
  SANE_Int read;

  /**
   * total amount of bytes to read for the scan
   */
  SANE_Int to_read;

#ifdef HAVE_SYS_TIME_H
  /**
   * last scan time, used to detect if warming-up is needed
   */
  struct timeval last_scan;

  /**
   * warming-up start time
   */
  struct timeval start_time;
#endif

  /**
   * device configuration options
   */
  Rts8891_Config conf;
};

/*
 * This struct is used to build a static list of USB IDs and link them
 * to a struct that describes the corresponding model.
 */
typedef struct Rts8891_USB_Device_Entry
{
  SANE_Word vendor_id;			/**< USB vendor identifier */
  SANE_Word product_id;			/**< USB product identifier */
  Rts8891_Model *model;			/**< Scanner model information */
} Rts8891_USB_Device_Entry;

/* this function init the rts8891 library */
void rts8891_lib_init (void);

 /***********************************/
 /* RTS8891 ASIC specific functions */
 /***********************************/

 /* this functions commits pending scan command */
static SANE_Status rts8891_commit (SANE_Int devnum, SANE_Byte value);

/* wait for head to park to home position */
static SANE_Status rts8891_wait_for_home (struct Rts8891_Device *device, SANE_Byte * regs);

 /**
  * move the head backward by a huge line number then poll home sensor until
  * head has get back home
  */
static SANE_Status rts8891_park (struct Rts8891_Device *device, SANE_Byte * regs, SANE_Bool wait);

#endif /* not RTS8891_LOW_H */
