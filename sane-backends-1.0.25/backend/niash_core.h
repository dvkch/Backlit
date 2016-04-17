/*
  Copyright (C) 2001 Bertrik Sikken (bertrik@zonnet.nl)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  $Id$
*/

/*
    Core NIASH chip functions.
*/


#ifndef _NIASH_CORE_H_
#define _NIASH_CORE_H_

#include <unistd.h>

#include "niash_xfer.h"		/* for EScannerModel */

#define HP3300C_RIGHT  330
#define HP3300C_TOP    452
#define HP3300C_BOTTOM (HP3300C_TOP + 14200UL)

#define HW_PIXELS   5300	/* number of pixels supported by hardware */
#define HW_DPI      600		/* horizontal resolution of hardware */
#define HW_LPI      1200	/* vertical resolution of hardware */

#define BYTES_PER_PIXEL 3

typedef struct
{
  int iXferHandle;		/* handle used for data transfer to HW */
  int iTopLeftX;		/* in mm */
  int iTopLeftY;		/* in mm */
  int iSensorSkew;		/* in units of 1/1200 inch */
  int iSkipLines;		/* lines of garbage to skip */
  SANE_Bool fReg07;		/* NIASH00019 */
  SANE_Bool fGamma16;		/* if TRUE, gamma entries are 16 bit */
  int iExpTime;
  SANE_Bool iReversedHead;	/* Head is reversed */
  int iBufferSize;		/* Size of internal scan buffer */
  EScannerModel eModel;
} THWParams;


typedef struct
{
  int iDpi;			/* horizontal resolution */
  int iLpi;			/* vertical resolution */
  int iTop;			/* in HW coordinates */
  int iLeft;			/* in HW coordinates */
  int iWidth;			/* pixels */
  int iHeight;			/* lines */
  int iBottom;

  int fCalib;			/* if TRUE, disable backtracking? */
} TScanParams;


typedef struct
{
  unsigned char *pabXferBuf;	/* transfer buffer */
  int iCurLine;			/* current line in the transfer buffer */
  int iBytesPerLine;		/* unsigned chars in one scan line */
  int iLinesPerXferBuf;		/* number of lines held in the transfer buffer */
  int iLinesLeft;		/* transfer (down) counter for pabXFerBuf */
  int iSaneBytesPerLine;	/* how many unsigned chars to be read by SANE per line */
  int iScaleDownDpi;		/* factors used to emulate lower resolutions */
  int iScaleDownLpi;		/* than those offered by hardware */
  int iSkipLines;		/* line to skip at the start of scan */
  int iWidth;			/* number of pixels expected by SANE */
  unsigned char *pabCircBuf;	/* circular buffer */
  int iLinesPerCircBuf;		/* lines held in the circular buffer */
  int iRedLine, iGrnLine,	/* start indices for the color information */
    iBluLine;			/* in the circular buffer */
  unsigned char *pabLineBuf;	/* buffer used to pass data to SANE */
} TDataPipe;


STATIC int NiashOpen (THWParams * pHWParams, const char *pszName);
STATIC void NiashClose (THWParams * pHWParams);

/* more sof. method that also returns the values of the white (RGB) value */
STATIC SANE_Bool SimpleCalibExt (THWParams * pHWPar,
				 unsigned char *pabCalibTable,
				 unsigned char *pabCalWhite);

STATIC SANE_Bool GetLamp (THWParams * pHWParams, SANE_Bool * pfLampIsOn);
STATIC SANE_Bool SetLamp (THWParams * pHWParams, SANE_Bool fLampOn);

STATIC SANE_Bool InitScan (TScanParams * pParams, THWParams * pHWParams);
STATIC void FinishScan (THWParams * pHWParams);

STATIC void CalcGamma (unsigned char *pabTable, double Gamma);
STATIC void WriteGammaCalibTable (unsigned char *pabGammaR,
				  unsigned char *pabGammaG,
				  unsigned char *pabGammaB,
				  unsigned char *pabCalibTable, int iGain,
				  int iOffset, THWParams * pHWPar);

/* set -1 for iHeight to disable all checks on buffer transfers */
/* iWidth is in pixels of SANE */
/* iHeight is lines in scanner resolution */
STATIC void CircBufferInit (int iHandle, TDataPipe * p,
			    int iWidth, int iHeight,
			    int iMisAlignment, SANE_Bool iReversedHead,
			    int iScaleDownDpi, int iScaleDownLpi);

/* returns false, when trying to read after end of buffer */
STATIC SANE_Bool CircBufferGetLine (int iHandle, TDataPipe * p,
				    unsigned char *pabLine,
				    SANE_Bool iReversedHead);

/* returns false, when trying to read after end of buffer 
   if fReturn==SANE_TRUE, the head will return automatically on an end of scan */

STATIC SANE_Bool
CircBufferGetLineEx (int iHandle, TDataPipe * p, unsigned char *pabLine,
		     SANE_Bool iReversedHead, SANE_Bool fReturn);

STATIC void CircBufferExit (TDataPipe * p);

#endif /* _NIASH_CORE_H_ */
