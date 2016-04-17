/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek.h
 *  @brief Definitions for the backend.
 *
 * Based on Kazuhiro Sasayama previous
 * work on plustek.[ch] file from the SANE package.<br>
 *
 * original code taken from sane-0.71<br>
 * Copyright (C) 1997 Hypercore Software Design, Ltd.<br>
 * Copyright (C) 2001-2007 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - no changes
 * - 0.32 - no changes
 * - 0.33 - no changes
 * - 0.34 - moved some definitions and typedefs from plustek.c
 * - 0.35 - removed OPT_MODEL from options list
 *        - added max_y to struct Plustek_Scan
 * - 0.36 - added reader_pid, pipe and bytes_read to struct Plustek_Scanner
 *        - removed unused variables from struct Plustek_Scanner
 *        - moved fd from struct Plustek_Scanner to Plustek_Device
 *        - added next members to struct Plustek_Scanner and Plustek_Device
 * - 0.37 - added max_x to struct Plustek_Device
 * - 0.38 - added caps to struct Plustek_Device
 *        - added exit code to struct Plustek_Scanner
 *        - removed dropout stuff
 * - 0.39 - PORTTYPE enum
 *        - added function pointers to control a scanner device
 *          (Parport and USB)
 * - 0.40 - added USB stuff
 * - 0.41 - added configuration stuff
 * - 0.42 - added custom gamma tables
 *        - changed usbId to static array
 *        - added _MAX_ID_LEN
 * - 0.43 - no changes
 * - 0.44 - added flag initialized
 * - 0.45 - added readLine function
 * - 0.46 - flag initialized is now used as device index
 *        - added calFile to Plustek_Device
 *        - removed _OPT_HALFTONE
 * - 0.47 - added mov to adjustment
 *        - changed stopScan function definition
 *        - removed open function
 *        - added OPT_LAMPSWITCH and OPT_WARMUPTIME
 * - 0.48 - added OPT_CACHECAL, OPT_OVR_*, OPT_LAMPOFF_TIMER and
 *          OPT_LAMPOFF_ONEND, also did some cleanup
 *        - moved SCANDEF definitions to plustek-usb.h
 *        - removed function pointer
 *        - added OPT_BIT_DEPTH
 * - 0.49 - added typedef struct DevList
 *        - added button stuff
 *        - added transferRate to struct Plustek_Device
 * - 0.50 - cleanup
 *        - added OPT_SPEEDUP
 * - 0.51 - added OPT_CALIBRATE
 * - 0.52 - added skipDarkStrip and incDarkTgt to struct AdjDef
 *        - added OPT_LOFF4DARK
 * .
 * <hr>
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * As a special exception, the authors of SANE give permission for
 * additional uses of the libraries contained in this release of SANE.
 *
 * The exception is that, if you link a SANE library with other files
 * to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License.  Your use of that executable is in no way restricted on
 * account of linking the SANE library code into it.
 *
 * This exception does not, however, invalidate any other reasons why
 * the executable file might be covered by the GNU General Public
 * License.
 *
 * If you submit changes to SANE to the maintainers to be included in
 * a subsequent release, you agree by submitting the changes that
 * those changes may be distributed with this exception intact.
 *
 * If you write modifications of your own for SANE, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 * <hr>
 */
#ifndef __PLUSTEK_H__
#define __PLUSTEK_H__

/************************ some definitions ***********************************/


#define PLUSTEK_CONFIG_FILE	"plustek.conf"

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

#define _MEASURE_BASE       300UL
#define _DEF_DPI            50
#define DEFAULT_RATE        1000000

/** the default image size
 */
#define _DEFAULT_TLX          0     /* 0..216 mm */
#define _DEFAULT_TLY          0     /* 0..297 mm */
#define _DEFAULT_BRX        103     /* 0..216 mm */
#define _DEFAULT_BRY         76.21  /* 0..297 mm */

#define _DEFAULT_TP_TLX      3.5    /* 0..42.3 mm */
#define _DEFAULT_TP_TLY     10.5    /* 0..43.1 mm */
#define _DEFAULT_TP_BRX     38.5    /* 0..42.3 mm */
#define _DEFAULT_TP_BRY     33.5    /* 0..43.1 mm */

#define _DEFAULT_NEG_TLX     1.5    /* 0..38.9 mm */
#define _DEFAULT_NEG_TLY     1.5    /* 0..29.6 mm */
#define _DEFAULT_NEG_BRX    37.5    /* 0..38.9 mm */
#define _DEFAULT_NEG_BRY    25.5    /* 0..29.6 mm */

/** image sizes for normal, transparent and negative modes
 */
#define _TPAPageWidth        500UL
#define _TPAPageHeight       510UL
#define _TPALargePageWidth  1270UL
#define _TPALargePageHeight 1570UL
#define _TPAMinDpi           150

#define _NegPageWidth        460UL
#define _NegPageHeight       350UL
#define _NegLargePageWidth  1270UL
#define _NegLargePageHeight 1570UL

#define _SCALE(X) ((double)(X)/300.0 * MM_PER_INCH)

/** scan modes
 */
#define COLOR_BW        0
#define COLOR_256GRAY   1
#define COLOR_GRAY16    2
#define COLOR_TRUE24    3
#define COLOR_TRUE48    4

/** usb id buffer
 */
#define _MAX_ID_LEN 20

/**
 */
#define SFLAG_ADF   0x00000010  /* Automatic document feeder    */
#define SFLAG_TPA   0x00000080  /* has transparency adapter     */

/**
 */
#define SOURCE_Reflection   0
#define SOURCE_Transparency 1
#define SOURCE_Negative     2
#define SOURCE_ADF          3

/** for Gamma tables
 */
#define _MAP_RED    0
#define _MAP_GREEN  1
#define _MAP_BLUE   2
#define _MAP_MASTER 3

/** generic error codes...
 */
#define _FIRST_ERR -9000

#define _E_ALLOC            (_FIRST_ERR-1)  /**< error allocating memory    */
#define _E_INVALID          (_FIRST_ERR-2)  /**< invalid parameter detected */
#define _E_INTERNAL         (_FIRST_ERR-3)  /**< internal error             */
#define _E_ABORT            (_FIRST_ERR-4)  /**< operation aborted          */

#define _E_LAMP_NOT_IN_POS  (_FIRST_ERR-10)
#define _E_LAMP_NOT_STABLE  (_FIRST_ERR-11)
#define _E_NODATA           (_FIRST_ERR-12)
#define _E_BUFFER_TOO_SMALL (_FIRST_ERR-13)
#define _E_DATAREAD         (_FIRST_ERR-14)

/************************ some structures ************************************/

#define _ENABLE(option)  s->opt[option].cap &= ~SANE_CAP_INACTIVE
#define _DISABLE(option) s->opt[option].cap |=  SANE_CAP_INACTIVE

enum {
	OPT_NUM_OPTS = 0,
	OPT_MODE_GROUP,
	OPT_MODE,
	OPT_BIT_DEPTH,
	OPT_EXT_MODE,
	OPT_RESOLUTION,
	OPT_PREVIEW,
	OPT_GEOMETRY_GROUP,
	OPT_TL_X,
	OPT_TL_Y,
	OPT_BR_X,
	OPT_BR_Y,
	OPT_ENHANCEMENT_GROUP,
	OPT_BRIGHTNESS,
	OPT_CONTRAST,
	OPT_CUSTOM_GAMMA,
	OPT_GAMMA_VECTOR,
	OPT_GAMMA_VECTOR_R,
	OPT_GAMMA_VECTOR_G,
	OPT_GAMMA_VECTOR_B,
	OPT_DEVICE_GROUP,
	OPT_LAMPSWITCH,
	OPT_LAMPOFF_TIMER,
	OPT_LAMPOFF_ONEND,
	OPT_WARMUPTIME,
	OPT_LOFF4DARK,
	OPT_CACHECAL,
	OPT_SPEEDUP,
	OPT_CALIBRATE,
	OPT_AFE_GROUP,
	OPT_OVR_REDGAIN,
	OPT_OVR_GREENGAIN,
	OPT_OVR_BLUEGAIN,
	OPT_OVR_REDOFS,
	OPT_OVR_GREENOFS,
	OPT_OVR_BLUEOFS,
	OPT_OVR_RED_LOFF,
	OPT_OVR_GREEN_LOFF,
	OPT_OVR_BLUE_LOFF,
	OPT_BUTTON_GROUP,
	OPT_BUTTON_0,
	OPT_BUTTON_1,
	OPT_BUTTON_2,
	OPT_BUTTON_3,
	OPT_BUTTON_4,
	OPT_BUTTON_LAST = OPT_BUTTON_4,
	NUM_OPTIONS
};

/**
 */
typedef struct {
	int x;
	int y;
} OffsDef, *pOffsDef;

/** for adjusting the scanner settings
 */
typedef struct {
	int     mov;                /**< model override */
	int     lampOff;
	int     lampOffOnEnd;
	int     warmup;
	int     enableTpa;
	int     skipCalibration;  /**< skip entire calibration */
	int     skipFine;
	int     skipFineWhite;
	int     skipDarkStrip;
	int     incDarkTgt;
	int     disableSpeedup;
	int     invertNegatives;
	int     cacheCalData;
	int     altCalibrate;  /* force use of the alternate canoscan autocal;
	                          perhaps other Canon scanners require the
	                          alternate autocalibration as well */
	/* AFE adjustemnts, gain and offset */
	int rgain;
	int ggain;
	int bgain;
	int rofs;
	int gofs;
	int bofs;
	
	int rlampoff;   /* for red lamp off setting (CIS-scanner)   */
	int glampoff;   /* for green lamp off setting (CIS-scanner) */
	int blampoff;   /* for blue lamp off setting (CIS-scanner)  */

	OffsDef pos;    /* for adjusting normal scan area       */
	OffsDef tpa;    /* for adjusting transparency scan area */
	OffsDef neg;    /* for adjusting negative scan area     */

	int     posShadingY;
	int     tpaShadingY;
	int     negShadingY;

	/* for adjusting the default gamma settings */
	double  rgamma;
	double  ggamma;
	double  bgamma;

	double  graygamma;

} AdjDef;

typedef struct {
	unsigned short x;
	unsigned short y;
	unsigned short cx;
	unsigned short cy;
} CropRect;

typedef struct image {
	unsigned long  dwFlag;
	CropRect       crArea;
	XY             xyDpi;
	unsigned short wDataType;
} ImgDef;

typedef struct {
	unsigned long dwPixelsPerLine;
	unsigned long dwBytesPerLine;
	unsigned long dwLinesPerArea;
	ImgDef        ImgDef;
} CropInfo;

typedef struct {
	ImgDef ImgDef;
	short  siBrightness;
	short  siContrast;
} ScanInfo;

typedef struct {
	unsigned long  dwFlag;
	unsigned short wMaxExtentX; /**< scanarea width  */
	unsigned short wMaxExtentY; /**< scanarea height */
} ScannerCaps;

typedef struct Plustek_Device
{
	SANE_Int               initialized;      /* device already initialized?  */
	struct Plustek_Device *next;             /* pointer to next dev in list  */
	int                    fd;               /* device handle                */
	char                  *name;             /* (to avoid compiler warnings!)*/
	char                  *calFile;          /* for saving calibration data  */
	unsigned long          transferRate;     /* detected USB-Speed in Bytes/s*/
	SANE_Device            sane;             /* info struct                  */
	SANE_Int               max_x;            /* max XY-extension of the scan-*/
	SANE_Int               max_y;            /* area                         */
	SANE_Range             dpi_range;        /* resolution range             */
	SANE_Range             x_range;          /* x-range of the scan-area     */
	SANE_Range             y_range;          /* y-range of the scan-area     */
	SANE_Int              *res_list;         /* to hold the available phys.  */
	SANE_Int               res_list_size;    /* resolution values            */
	ScannerCaps            caps;             /* caps reported by the driver  */
	AdjDef                 adj;              /* for driver adjustment        */

	/**************************** USB-stuff **********************************/
	char                   usbId[_MAX_ID_LEN];/* to keep Vendor and product  */
	                                         /* ID string (from conf) file   */
	struct ScanDef         scanning;         /* here we hold all stuff for   */
	                                         /* the USB-scanner              */
	struct DeviceDef       usbDev;
#ifdef HAVE_SETITIMER
	struct itimerval       saveSettings;     /* for lamp timer               */
#endif

} Plustek_Device;

#ifndef SANE_OPTION
/* for compatibility with older versions */
typedef union
{
	SANE_Word   w;
	SANE_Word  *wa;
	SANE_String s;
} Option_Value;
#endif

typedef struct Plustek_Scanner
{
	struct Plustek_Scanner *next;
	SANE_Pid                reader_pid;     /* process id of reader          */
	SANE_Status             exit_code;      /* status of the reader process  */
	int                     r_pipe;         /* pipe to reader process        */
	int                     w_pipe;         /* pipe from reader process      */
	unsigned long           bytes_read;     /* number of bytes currently read*/
	Plustek_Device         *hw;             /* pointer to current device     */
	Option_Value            val[NUM_OPTIONS];
	SANE_Byte              *buf;            /* the image buffer              */
	SANE_Bool               scanning;       /* TRUE during scan-process      */
	SANE_Bool               calibrating;    /* TRUE during calibration       */
	SANE_Bool               ipc_read_done;  /* TRUE after ipc has been red   */
	SANE_Parameters         params;         /* for keeping the parameter     */

	/************************** gamma tables *********************************/

	SANE_Word   gamma_table[4][4096];
	SANE_Range  gamma_range;
	int         gamma_length;

	SANE_Option_Descriptor opt[NUM_OPTIONS];

} Plustek_Scanner;

/** for collecting configuration info...
 */
typedef struct {
	
	char     devName[PATH_MAX];
	char     usbId[_MAX_ID_LEN];

	/* contains the stuff to adjust... */
	AdjDef   adj;

} CnfDef;

/** for supported device list
 */
typedef struct DevList {
	SANE_Word       vendor_id;
	SANE_Word       device_id;
	SANE_Bool       attached;
	SANE_Char      *dev_name;
	struct DevList *next;
} DevList;
#endif	/* guard __PLUSTEK_H__ */

/* END PLUSTEK.H.............................................................*/
