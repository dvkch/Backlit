/** @file u12-scanner.h
 *  @brief Definitions for the devices.
 *
 * Copyright (c) 2003-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.01 - initial version
 * - 0.02 - removed useless stuff
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
#ifndef __U12_SCANNER_H__
#define __U12_SCANNER_H__

/* definitions for the timer functions
 */
typedef double TimerDef;
#define _MSECOND    1000             /* based on 1 us */
#define _SECOND     (1000*_MSECOND)

#if 1
#define _DO_UDELAY(usecs)   u12io_udelay(usecs)
#define _DODELAY(msecs)     u12io_udelay(1000*msecs)
/*{ int i; for( i = msecs; i--; ) _DO_UDELAY(1000); }*/
#else
#define _DODELAY(msecs) 
#endif

/* ModuleStates */
#define _MotorInNormalState 0
#define _MotorGoBackward    1
#define _MotorInStopState   2
#define _MotorAdvancing     3
#define _MotorAdvanced      4


/** some function types
 */
typedef struct u12d *pU12_Device;
typedef struct svd  *pShadingVarDef;
typedef void      (*pFnVoid)(pU12_Device);
typedef void      (*pFnDACOffs)(pU12_Device, pShadingVarDef, u_long);
typedef void      (*pFnDACDark)(pU12_Device, pShadingVarDef, u_long, u_short);
typedef void      (*pFnDataProcess)(pU12_Device, void*, void*, u_long);
typedef SANE_Bool (*pFnBool)(pU12_Device);


/* useful for RGB-values */
typedef struct {
	SANE_Byte Red;
	SANE_Byte Green;
	SANE_Byte Blue;
} RGBByteDef;

typedef struct {
	u_short Red;
	u_short Green;
	u_short Blue;
} RGBUShortDef;

typedef struct {
	u_long Red;
	u_long Green;
	u_long Blue;
} RGBULongDef;

typedef union {
	RGBByteDef Colors;
	SANE_Byte  bColors[3];
} ColorByte;

typedef union {
	RGBUShortDef Colors;
	u_short      wColors[3];
} ColorWord;

typedef union {
	SANE_Byte     *pb;
	u_short       *pw;
	u_long        *pdw;
	RGBUShortDef  *pusrgb;
	RGBULongDef   *pulrgb;
	RGBByteDef    *pbrgb;
} DataPointer;

typedef struct
{
	union {
		SANE_Byte *bp;
		u_short   *usp;
		u_long    *ulp;
	} red;
	union {
		SANE_Byte *bp;
		u_short   *usp;
		u_long    *ulp;
	} green;
	union {
		SANE_Byte *bp;
		u_short   *usp;
		u_long    *ulp;
	} blue;

} RBGPtrDef;

typedef struct {
	SANE_Byte b1st;
	SANE_Byte b2nd;
} WordVal;

typedef struct {
	WordVal w1st;
	WordVal w2nd;
} DWordVal;


typedef union {
	WordVal   wOverlap;
	DWordVal  dwOverlap;
	u_long    dwValue;
	u_short   wValue;
	SANE_Byte bValue;
} DataType;

typedef struct {
	u_short exposureTime;
	u_short xStepTime;
} ExpXStepDef;

typedef struct {
	SANE_Byte reg;
	SANE_Byte val;
} RegDef;

/** for defining a point
 */
typedef struct {
	u_short x;
	u_short y;
} XY;

/** for defining a crop area, all is 300DPI based
 */
typedef struct {
	u_short x;      /**< x-pos of top-left corner */
	u_short y;      /**< y-pos of top-left corner */
	u_short cx;     /**< width                    */
	u_short cy;     /**< height                   */
} CropRect;

/** for defining an image
 */
typedef struct {
	u_long   dwFlag;      /**< i.e. image source           */
	CropRect crArea;      /**< the image size and position */
	XY       xyDpi;       /**< the resolution              */
	u_short  wDataType;   /**< and the data type           */
} ImgDef;

/**
 */
typedef struct {
	u_long dwPixelsPerLine;
	u_long dwBytesPerLine;
	u_long dwLinesPerArea;
	ImgDef image;
} CropInfo;

/** all we need for a scan
 */
typedef struct
{
	u_long  dwScanFlag;
	double  xyRatio;  /**< for scaling */
	u_short wYSum;

	XY      xyPhyDpi; /**< physical resolution of a scan */
	u_long  dwPhysBytesPerLine;
	u_long  wPhyDataType;         /**< how the scanner should scan */

	u_long  dwAsicPixelsPerPlane;
	u_long  dwAsicBytesPerPlane;
	u_long  dwAsicBytesPerLine;

	XY      xyAppDpi;
	u_long  dwAppLinesPerArea;
	u_long  dwAppPixelsPerLine;
	u_long  dwAppPhyBytesPerLine;
	u_long  dwAppBytesPerLine;
	u_short wAppDataType;

	short   siBrightness;
	short   siContrast;

	CropRect crImage;
} DataInfo;

/** this will be our global "overkill" structure
 */
typedef struct
{
	pFnDataProcess  DataProcess;    /* to convert RGB buffers to RGB pixels */
	pFnBool         DoSample;
	pFnBool         DataRead;       /* function to get data from scanner    */

	long            lBufferAdjust;
	u_long          dwScanOrigin;   /* where to start the scan              */
	u_long          negBegin;       /* used while scanning in TPA modes     */
	u_long          posBegin;
	SANE_Byte       bDiscardAll;

	union {
		u_short     wGreenDiscard;
		u_short     wGreenKeep;
	} gd_gk;
	union {
		u_short     wBlueDiscard;
		u_short     wRedKeep;
	} bd_rk;

	u_long          dpiIdx;         /* index to get/set values in the table */
	ExpXStepDef    *negScan;        /* reference to exposure/xtep table     */

	DataPointer     p48BitBuf;      /* for handling 48-bit data             */
	RBGPtrDef       BufBegin;       /* for reading/writing the scan-data    */
	RBGPtrDef       BufEnd;
	RBGPtrDef       BufGet;
	RBGPtrDef       BufData;
	RBGPtrDef       BufPut;

	/* motor movement stuff */
	u_long          dwInterval;
	SANE_Bool       refreshState;
	SANE_Bool       motorBackward;
	SANE_Byte       oldScanState;
	SANE_Byte       bRefresh;
	SANE_Byte       bModuleState;
	SANE_Byte       bNowScanState;

	/* internal FIFO management */
	u_long          dwMinReadFifo;
	u_long          dwMaxReadFifo;
	SANE_Byte       bFifoSelect;    /* defines which FIFO to use            */

} ScanInfo;

/** structure for accessing one buffer in various ways...
 */
typedef struct
{
	union {
		SANE_Byte   *pReadBuf;
		SANE_Byte   *pShadingMap;
		u_short     *pShadingRam;
		DataPointer  Buf;
	} b1;

	union {
		SANE_Byte    *pSumBuf;
		RGBUShortDef *pSumRGB;
	} b2;

	DataPointer TpaBuf;
} BufferDef;

#endif	/* guard __U12_SCANNER_H__ */

/* END U12-SCANNER.H ........................................................*/
