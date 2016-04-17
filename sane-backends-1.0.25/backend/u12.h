/** @file u12.h
 *  @brief Definitions for the backend.
 *
 * Copyright (c) 2003-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.01 - initial version
 * - 0.02 - added scaling variables to struct u12d
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
#ifndef __U12_H__
#define __U12_H__

#ifndef SANE_OPTION
/* for compatibility with older versions */
typedef union
{
	SANE_Word w;
	SANE_Word *wa;		/* word array */
	SANE_String s;
} Option_Value;
#endif

/************************ some definitions ***********************************/

#define U12_CONFIG_FILE "u12.conf"

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

#define _MM_PER_INCH  25.4
#define _MEASURE_BASE 300UL
#define _DEF_DPI      50

/** the default image 
 */
#define _DEFAULT_TLX        0
#define _DEFAULT_TLY        0
#define _DEFAULT_BRX        126
#define _DEFAULT_BRY        76

#define _DEFAULT_TP_TLX     3.5
#define _DEFAULT_TP_TLY     10.5
#define _DEFAULT_TP_BRX     38.5
#define _DEFAULT_TP_BRY     33.5

#define _DEFAULT_NEG_TLX    1.5
#define _DEFAULT_NEG_TLY    1.5
#define _DEFAULT_NEG_BRX    37.5
#define _DEFAULT_NEG_BRY    25.5

/** image sizes for normal, transparent and negative modes
 */
#define _TPAPageWidth		500U
#define _TPAPageHeight		510U
#define _TPAMinDpi		    150
#define _TPAModeSupportMin	COLOR_TRUE24

#define _NegativePageWidth  460UL
#define _NegativePageHeight 350UL

#define _TP_X			((double)_TPAPageWidth/300.0 * _MM_PER_INCH)
#define _TP_Y			((double)_TPAPageHeight/300.0 * _MM_PER_INCH)
#define _NEG_X			((double)_NegativePageWidth/300.0 * _MM_PER_INCH)
#define _NEG_Y			((double)_NegativePageHeight/300.0 * _MM_PER_INCH)

/** scan modes
 */
#define COLOR_BW      0
#define COLOR_256GRAY 1
#define COLOR_TRUE24  2
#define COLOR_TRUE42  3

#define _VAR_NOT_USED(x)	((x)=(x))


/** usb id buffer
 */
#define _MAX_ID_LEN	20

/** Scanmodes
 */
#define _ScanMode_Color         0
#define _ScanMode_AverageOut	1	/* CCD averaged 2 pixels value for output*/
#define _ScanMode_Mono			2   /* not color mode						 */

/** Scansource + additional flags
 */
#define _SCANDEF_PREVIEW        0x00000001
#define _SCANDEF_Transparency   0x00000100
#define _SCANDEF_Negative       0x00000200
#define _SCANDEF_TPA            (_SCANDEF_Transparency | _SCANDEF_Negative)
#define _SCANDEF_SCANNING       0x8000000

/** for Gamma tables
 */
#define _MAP_RED    0
#define _MAP_GREEN  1
#define _MAP_BLUE   2
#define _MAP_MASTER 3

/** the ASIC modes */
#define _PP_MODE_SPP 0
#define _PP_MODE_EPP 1

/************************ some structures ************************************/

enum {
	OPT_NUM_OPTS = 0,
	OPT_MODE_GROUP,
#ifdef ALL_MODES
	OPT_MODE,
	OPT_EXT_MODE,
#endif
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
	NUM_OPTIONS
};

/** for adjusting the scanner settings
 */
typedef struct {
	int lampOff;
	int lampOffOnEnd;
	int warmup;

	/* for adjusting the default gamma settings */
	double  rgamma;
	double  ggamma;
	double  bgamma;
	double  graygamma;

	/* for adjusting scan-area */
	long    upNormal;      
	long    upPositive;
	long    upNegative;
	long    leftNormal;

} AdjDef, *pAdjDef;

/** for holding basic capabilities
 */
typedef struct {
	unsigned short scanAreaX;
	unsigned short scanAreaY;
	unsigned long  flag;
#if 0
	RANGE	    	rDataType;      /* available scan modes 			*/
	unsigned short	wMaxExtentX;	/* scanarea width					*/
	unsigned short	wMaxExtentY;	/* scanarea height					*/
#endif
} ScannerCaps, *pScannerCaps;


/** for defining the scanmodes
 */
typedef const struct mode_param
{
	int color;
	int depth;
	int scanmode;
} ModeParam, *pModeParam;

/** Here we hold all device specific data
 */
typedef struct u12d
{
	SANE_Bool    initialized;       /* device already initialized?  */
	struct u12d *next;              /* pointer to next dev in list  */
	int          fd;                /* device handle                */
	int          mode;
	char        *name;              /* (to avoid compiler warnings!)*/
	SANE_Device  sane;              /* info struct                  */

	/* scan-area settings */
	SANE_Int     max_x;             /* max XY-extension of the scan-*/
	SANE_Int     max_y;             /* area                         */
	SANE_Range   x_range;           /* x-range of the scan-area     */
	SANE_Range   y_range;           /* y-range of the scan-area     */

	/* resolution settings */
	SANE_Int     dpi_max_x;         /* */
	SANE_Int     dpi_max_y;         /* */
	SANE_Range   dpi_range;         /* resolution range             */

	SANE_Int    *res_list;          /* to hold the available phys.  */
	SANE_Int     res_list_size;     /* resolution values            */
	ScannerCaps  caps;              /* caps reported by the driver  */
	AdjDef       adj;               /* for driver adjustment        */
	
	char         usbId[_MAX_ID_LEN];/* to keep Vendor and product   */
                                    /* ID string (from conf) file   */
	/* our gamma tables */
	SANE_Word    gamma_table[4][4096];
	SANE_Range   gamma_range;
	int          gamma_length;

	/* the shading section */
	pFnDACOffs   fnDarkOffset;   /**< ...                 */
	ShadingDef   shade;          /**< shading parameters */

	/* */
	SANE_Byte    PCBID;         /**< which version of the PCB         */

	/* motor control section */
	SANE_Byte    MotorID;       /**< the type of the motor drivers    */
	SANE_Byte    MotorPower;    /**< how to drive the motor           */
	SANE_Bool    f2003;
	SANE_Byte    XStepMono;
	SANE_Byte    XStepColor;
	SANE_Byte    XStepBack;
	SANE_Bool    f0_8_16;
	SANE_Byte    scanStates[_SCANSTATE_BYTES];

	/* CCD section */
	SANE_Byte    CCDID;         /**< what CCD do we have              */
	RegDef      *CCDRegs;       /**< pointer to the register descr    */
	u_short      numCCDRegs;    /**< number of values to write        */

	/* DAC section */
	SANE_Byte    DACType;       /**< what DAC do we have              */
	RegDef      *DACRegs;       /**< pointer to DAC reg descr.        */
	u_short      numDACRegs;    /**< number of values to write        */
	pFnDACDark   fnDACDark;     /**<                                  */
	RGBByteDef   RegDACOffset;
	RGBByteDef   RegDACGain;

	ShadowRegs   regs;       /**< for holding ASIC register values        */
	DataInfo     DataInf;    /**< all static info about the current scan  */
	ScanInfo     scan;       /**< buffer and motor management during scan */
	BufferDef    bufs;
	void        *scaleBuf;   /**< buffer for line scaling    */
	int          scaleStep;  /**< step size for line scaling */
	int          scaleIzoom; /**< factor for line scaling    */

	u_long       ModelOriginY;
	SANE_Byte    ModelCtrl;

	SANE_Bool    Tpa;           /**< do we have a TPA                 */
	SANE_Byte    Buttons;       /**< number of buttons                */

	/* lamp control section */
	SANE_Bool    warmupNeeded;
	SANE_Byte    lastLampStatus;  /**< for keeping the lamp status   */

#ifdef HAVE_SETITIMER
	struct itimerval   saveSettings;    /**< for lamp timer           */
#endif
} U12_Device;

typedef struct u12s
{
	struct u12s     *next;
	SANE_Pid         reader_pid;     /* process id of reader          */
	SANE_Status      exit_code;      /* status of the reader process  */
	int              r_pipe;         /* pipe to reader process        */
	int              w_pipe;         /* pipe from reader process      */
	unsigned long    bytes_read;     /* number of bytes currently read*/
	U12_Device      *hw;             /* pointer to current device     */
	Option_Value     val[NUM_OPTIONS];
	SANE_Byte       *buf;            /* the image buffer              */
	SANE_Bool        scanning;       /* TRUE during scan-process      */
	SANE_Parameters  params;         /* for keeping the parameter     */

	SANE_Option_Descriptor opt[NUM_OPTIONS];

} U12_Scanner;

/** for collecting configuration info...
 */
typedef struct {
	
	char devName[PATH_MAX];
	char usbId[_MAX_ID_LEN];

	/* contains the stuff to adjust... */
	AdjDef   adj;

} CnfDef, *pCnfDef;

#endif	/* guard __U12_H__ */

/* END U12.H ................................................................*/
