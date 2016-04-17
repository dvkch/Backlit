/* sane - Scanner Access Now Easy.
   Copyright (C) Marian Matthias Eichholz 2001
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

#ifndef _H_SM3600
#define _H_SM3600

/* ======================================================================

sm3600.h

SANE backend master module.

Definitions ported from "scantool.h" 5.4.2001.

(C) Marian Matthias Eichholz 2001

Start: 2.4.2001

====================================================================== */

#define DEBUG_SCAN     0x0001
#define DEBUG_COMM     0x0002
#define DEBUG_ORIG     0x0004
#define DEBUG_BASE     0x0011
#define DEBUG_DEVSCAN  0x0012
#define DEBUG_REPLAY   0x0014
#define DEBUG_BUFFER   0x0018
#define DEBUG_SIGNALS  0x0020
#define DEBUG_CALIB    0x0040

#define DEBUG_CRITICAL 1
#define DEBUG_VERBOSE  2
#define DEBUG_INFO     3
#define DEBUG_JUNK     5

#define USB_TIMEOUT_JIFFIES  2000

#define SCANNER_VENDOR     0x05DA

#define MAX_PIXEL_PER_SCANLINE  5300

/* ====================================================================== */

typedef enum { false, true } TBool;

typedef SANE_Status TState;

typedef enum { unknown, sm3600, sm3700, sm3750 } TModel;

typedef struct {
  TBool         bCalibrated;
  int           xMargin; /* in 1/600 inch */
  int           yMargin; /* in 1/600 inch */
  unsigned char nHoleGray;
  unsigned char nBarGray;
  long          rgbBias;
  unsigned char      *achStripeY;
  unsigned char      *achStripeR;
  unsigned char      *achStripeG;
  unsigned char      *achStripeB;
} TCalibration;

typedef struct {
  int x;
  int y;
  int cx;
  int cy;
  int res; /* like all parameters in 1/1200 inch */
  int nBrightness; /* -255 ... 255 */
  int nContrast;   /* -128 ... 127 */
} TScanParam;

typedef enum { fast, high, best } TQuality;
typedef enum { color, gray, line, halftone } TMode;

#define INST_ASSERT() { if (this->nErrorState) return this->nErrorState; }

#define CHECK_ASSERTION(a) if (!(a)) return SetError(this,SANE_STATUS_INVAL,"assertion failed in %s %d",__FILE__,__LINE__)

#define CHECK_POINTER(p) \
if (!(p)) return SetError(this,SANE_STATUS_NO_MEM,"memory failed in %s %d",__FILE__,__LINE__)

#define dprintf debug_printf

typedef struct TInstance *PTInstance;
typedef TState (*TReadLineCB)(PTInstance);

typedef struct TScanState {
  TBool           bEOF;         /* EOF marker for sane_read */
  TBool           bCanceled;
  TBool           bScanning;    /* block is active? */
  TBool           bLastBulk;    /* EOF announced */
  int             iReadPos;     /* read() interface */
  int             iBulkReadPos; /* bulk read pos */
  int             iLine;        /* log no. line */
  int             cchBulk;      /* available bytes in bulk buffer */
  int             cchLineOut;   /* buffer size */
  int             cxPixel,cyPixel; /* real pixel */
  int             cxMax;        /* uninterpolated in real pixels */
  int             cxWindow;     /* Window with in 600 DPI */
  int             cyWindow;     /* Path length in 600 DPI */
  int             cyTotalPath;  /* from bed start to window end in 600 dpi */
  int             nFixAspect;   /* aspect ratio in percent, 75-100 */
  int             cBacklog;     /* depth of ppchLines */
  int             ySensorSkew;  /* distance in pixel between sensors */
  char           *szOrder;      /* 123 or 231 or whatever */
  unsigned char  *pchBuf;       /* bulk transfer buffer */
  short         **ppchLines;    /* for error diffusion and color corr. */
  unsigned char  *pchLineOut;   /* read() interface */
  TReadLineCB     ReadProc;     /* line getter callback */
} TScanState;


#ifndef INSANE_VERSION

#ifdef SM3600_SUPPORT_EXPOSURE
#define NUM_OPTIONS 18
#else
#define NUM_OPTIONS 16
#endif


typedef struct TDevice {
  struct TDevice        *pNext;
  struct usb_device     *pdev;
  TModel                 model;
  SANE_Device            sane;
  char			*szSaneName;
} TDevice;

#endif

typedef struct TInstance {
#ifndef INSANE_VERSION
  struct TInstance         *pNext;
  SANE_Option_Descriptor    aoptDesc[NUM_OPTIONS];
  Option_Value              aoptVal[NUM_OPTIONS];
#endif
  SANE_Int           agammaY[4096];
  SANE_Int           agammaR[4096];
  SANE_Int           agammaG[4096];
  SANE_Int           agammaB[4096];
  TScanState         state;
  TCalibration       calibration;
  TState             nErrorState;
  char              *szErrorReason;
  TBool              bSANE;
  TScanParam         param;
  TBool              bWriteRaw;
  TBool              bVerbose;
  TBool              bOptSkipOriginate;
  TQuality           quality;
  TMode              mode;
  TModel             model;
  int                hScanner;
  FILE              *fhLog;
  FILE              *fhScan;
  int                ichPageBuffer; /* write position in full page buffer */
  int                cchPageBuffer; /* total size of '' */
  unsigned char     *pchPageBuffer; /* the humble buffer */
} TInstance;

#define TRUE  1
#define FALSE 0

/* ====================================================================== */

#define ERR_FAILED -1
#define OK         0

#define NUM_SCANREGS      74

/* ====================================================================== */

/* note: The first register has address 0x01 */

#define R_ALL    0x01

/* have to become an enumeration */

typedef enum { none, hpos, hposH, hres } TRegIndex;

/* WORD */
#define R_SPOS   0x01
#define R_XRES   0x03
/* WORD */
#define R_SWID   0x04
/* WORD */
#define R_STPS   0x06
/* WORD */
#define R_YRES   0x08
/* WORD */
#define R_SLEN   0x0A
/* WORD*/
#define R_INIT   0x12
#define RVAL_INIT 0x1540
/* RGB */
#define R_CCAL   0x2F

/* WORD */
#define R_CSTAT  0x42
#define R_CTL    0x46
/* WORD */
#define R_POS    0x52
/* WORD */
#define R_LMP    0x44
#define R_QLTY   0x4A
#define R_STAT   0x54

#define LEN_MAGIC   0x24EA

/* ====================================================================== */
#define USB_CHUNK_SIZE 0x8000

/* sm3600-scanutil.c */
__SM3600EXPORT__ int SetError(TInstance *this, int nError, const char *szFormat, ...);
__SM3600EXPORT__ void debug_printf(unsigned long ulType, const char *szFormat, ...);
__SM3600EXPORT__ TState FreeState(TInstance *this, TState nReturn);
__SM3600EXPORT__ TState EndScan(TInstance *this);
__SM3600EXPORT__ TState ReadChunk(TInstance *this, unsigned char *achOut,
				  int cchMax, int *pcchRead);
#ifdef INSANE_VERSION
__SM3600EXPORT__ void DumpBuffer(FILE *fh, const char *pch, int cch);
__SM3600EXPORT__ TState DoScanFile(TInstance *this);
#endif

__SM3600EXPORT__ void   GetAreaSize(TInstance *this);
__SM3600EXPORT__ void   ResetCalibration(TInstance *this);

__SM3600EXPORT__ TState InitGammaTables(TInstance *this,
					int nBrightness,
					int nContrast);
__SM3600EXPORT__ TState CancelScan(TInstance *this);

/* sm3600-scanmtek.c */
extern unsigned short aidProduct[];
__SM3600EXPORT__ TState DoInit(TInstance *this);
__SM3600EXPORT__ TState DoReset(TInstance *this);
__SM3600EXPORT__ TState WaitWhileBusy(TInstance *this,int cSecs);
__SM3600EXPORT__ TState WaitWhileScanning(TInstance *this,int cSecs);
__SM3600EXPORT__ TModel GetScannerModel(unsigned short idVendor, unsigned short idProduct);

#ifdef INSANE_VERSION
__SM3600EXPORT__ TState DoLampSwitch(TInstance *this,int nPattern);
#endif
__SM3600EXPORT__ TState DoCalibration(TInstance *this);
__SM3600EXPORT__ TState UploadGammaTable(TInstance *this, int iByteAddress, SANE_Int *pnGamma);
__SM3600EXPORT__ TState UploadGainCorrection(TInstance *this, int iTableOffset);

/* sm3600-scanusb.c */
__SM3600EXPORT__ TState RegWrite(TInstance *this,int iRegister, int cb, unsigned long ulValue);
__SM3600EXPORT__ TState RegWriteArray(TInstance *this,int iRegister, int cb, unsigned char *pchBuffer);
#ifdef INSANE_VERSIONx
__SM3600EXPORT__ TState RegCheck(TInstance *this,int iRegister, int cch, unsigned long ulValue);
__SM3600EXPORT__ int BulkRead(TInstance *this,FILE *fhOut, unsigned int cchBulk);
__SM3600EXPORT__ TState MemReadArray(TInstance *this, int iAddress, int cb, unsigned char *pchBuffer);
#endif
__SM3600EXPORT__ int BulkReadBuffer(TInstance *this,unsigned char *puchBufferOut, unsigned int cchBulk); /* gives count */
__SM3600EXPORT__ unsigned int RegRead(TInstance *this,int iRegister, int cch);
__SM3600EXPORT__ TState MemWriteArray(TInstance *this, int iAddress, int cb, unsigned char *pchBuffer);

/* sm3600-gray.c */
__SM3600EXPORT__ TState StartScanGray(TInstance *this);
/* sm3600-color.c */
__SM3600EXPORT__ TState StartScanColor(TInstance *this);

/* sm3600-homerun.c */
#ifdef INSANE_VERSION
__SM3600EXPORT__ TState FakeCalibration(TInstance *this);
#endif

__SM3600EXPORT__ TState DoOriginate(TInstance *this, TBool bStepOut);
__SM3600EXPORT__ TState DoJog(TInstance *this,int nDistance);

/* ====================================================================== */

#endif
