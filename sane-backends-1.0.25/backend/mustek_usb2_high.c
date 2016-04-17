/* sane - Scanner Access Now Easy.

   Copyright (C) 2005 Mustek.
   Originally maintained by Mustek
   Author:Jack Roy 2005.5.24

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

   This file implements a SANE backend for the Mustek BearPaw 2448 TA Pro 
   and similar USB2 scanners. */

#include <pthread.h>		/* HOLD */
#include <stdlib.h>

/* local header files */
#include "mustek_usb2_asic.c"

#include "mustek_usb2_high.h"

/* ******************++ spuicall_g.h ****************************/

/*global variable HOLD: these should go to scanner structure */

/*base type*/
static SANE_Bool g_bOpened;
static SANE_Bool g_bPrepared;
static SANE_Bool g_isCanceled;
static SANE_Bool g_bSharpen;
static SANE_Bool g_bFirstReadImage;
static SANE_Bool g_isScanning;
static SANE_Bool g_isSelfGamma;

static SANE_Byte g_bScanBits;
static SANE_Byte *g_lpReadImageHead;

static unsigned short s_wOpticalYDpi[] = { 1200, 600, 300, 150, 75, 0 };
static unsigned short s_wOpticalXDpi[] = { 1200, 600, 300, 150, 75, 0 };
static unsigned short g_X;
static unsigned short g_Y;
static unsigned short g_Width;
static unsigned short g_Height;
static unsigned short g_XDpi;
static unsigned short g_YDpi;
static unsigned short g_SWWidth;
static unsigned short g_SWHeight;
static unsigned short g_wPixelDistance;		/*even & odd sensor problem */
static unsigned short g_wLineDistance;
static unsigned short g_wScanLinesPerBlock;
static unsigned short g_wReadedLines;
static unsigned short g_wReadImageLines;
static unsigned short g_wReadyShadingLine;
static unsigned short g_wStartShadingLinePos;
static unsigned short g_wLineartThreshold;

static unsigned int g_wtheReadyLines;
static unsigned int g_wMaxScanLines;
static unsigned int g_dwScannedTotalLines;
static unsigned int g_dwImageBufferSize;
static unsigned int g_BytesPerRow;
static unsigned int g_SWBytesPerRow;
static unsigned int g_dwCalibrationSize;
static unsigned int g_dwBufferSize;

static unsigned int g_dwTotalTotalXferLines;

static unsigned short *g_pGammaTable;
static unsigned char *g_pDeviceFile;

static pthread_t g_threadid_readimage;

/*user define type*/
static COLORMODE g_ScanMode;
static TARGETIMAGE g_tiTarget;
static SCANTYPE g_ScanType = ST_Reflective;
static SCANSOURCE g_ssScanSource;
static PIXELFLAVOR g_PixelFlavor;

static SUGGESTSETTING g_ssSuggest;
static Asic g_chip;

static int g_nSecLength, g_nDarkSecLength;
static int g_nSecNum, g_nDarkSecNum;
static unsigned short g_wCalWidth;
static unsigned short g_wDarkCalWidth;
static int g_nPowerNum;
static unsigned short g_wStartPosition;

static pthread_mutex_t g_scannedLinesMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_readyLinesMutex = PTHREAD_MUTEX_INITIALIZER;

/*for modify the last point*/
static SANE_Byte * g_lpBefLineImageData = NULL;
static SANE_Bool g_bIsFirstReadBefData = TRUE;
static unsigned int g_dwAlreadyGetLines = 0;

/* forward declarations */
static SANE_Bool MustScanner_Init (void);
static SANE_Bool MustScanner_GetScannerState (void);
static SANE_Bool MustScanner_PowerControl (SANE_Bool isLampOn, SANE_Bool isTALampOn);
static SANE_Bool MustScanner_BackHome (void);
static SANE_Bool MustScanner_Prepare (SANE_Byte bScanSource);
#ifdef SANE_UNUSED
static SANE_Bool MustScanner_AdjustOffset (int nTimes, SANE_Bool * bDirection, SANE_Byte * bOffset,
				      SANE_Byte * bLastMin, SANE_Byte * bLastOffset,
				      unsigned short * wMinValue, SANE_Byte * bOffsetUpperBound,
				      SANE_Byte * bOffsetLowerBound, unsigned short wStdMinLevel,
				      unsigned short wStdMaxLevel);
static SANE_Bool MustScanner_SecondAdjustOffset (int nTimes, SANE_Bool * bDirection,
					    SANE_Byte * bOffset, SANE_Byte * bLastMin,
					    SANE_Byte * bLastOffset, unsigned short * wMinValue,
					    SANE_Byte * bOffsetUpperBound,
					    SANE_Byte * bOffsetLowerBound,
					    unsigned short wStdMinLevel, unsigned short wStdMaxLevel);
#endif
static unsigned short MustScanner_FiltLower (unsigned short * pSort, unsigned short TotalCount, unsigned short LowCount,
				   unsigned short HighCount);
static SANE_Bool MustScanner_GetRgb48BitLine (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
					 unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetRgb48BitLine1200DPI (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
						unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetRgb24BitLine (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
					 unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetRgb24BitLine1200DPI (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
						unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetMono16BitLine (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
					  unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetMono16BitLine1200DPI (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
						 unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetMono8BitLine (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
					 unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetMono8BitLine1200DPI (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
						unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetMono1BitLine (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
					 unsigned short * wLinesCount);
static SANE_Bool MustScanner_GetMono1BitLine1200DPI (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
						unsigned short * wLinesCount);
static void *MustScanner_ReadDataFromScanner (void * dummy);
static void MustScanner_PrepareCalculateMaxMin (unsigned short wResolution);
static void MustScanner_CalculateMaxMin (SANE_Byte * pBuffer, unsigned short * lpMaxValue,
					 unsigned short * lpMinValue, unsigned short wResolution);

static SANE_Byte QBET4 (SANE_Byte A, SANE_Byte B);
static unsigned int GetScannedLines (void);
static unsigned int GetReadyLines (void);
static void AddScannedLines (unsigned short wAddLines);
static void AddReadyLines (void);
static void ModifyLinePoint (SANE_Byte * lpImageData,
			     SANE_Byte * lpImageDataBefore,
			     unsigned int dwBytesPerLine,
			     unsigned int dwLinesCount,
			     unsigned short wPixDistance, unsigned short wModPtCount);

#include "mustek_usb2_reflective.c"
#include "mustek_usb2_transparent.c"

/**********************************************************************
Author: Jack            Date: 2005/05/13
	Routine Description: 
Parameters:
	none
Return value: 
	if initialize the scanner success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_Init ()
{
  DBG (DBG_FUNC, "MustScanner_Init: Call in\n");

  g_chip.firmwarestate = FS_NULL;
  if (STATUS_GOOD != Asic_Open (&g_chip, g_pDeviceFile))
    {
      DBG (DBG_FUNC, "MustScanner_Init: Asic_Open return error\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_Initialize (&g_chip))
    {
      DBG (DBG_FUNC, "MustScanner_Init: Asic_Initialize return error\n");
      return FALSE;
    }

  g_dwImageBufferSize = 24L * 1024L * 1024L;
  g_dwBufferSize = 64L * 1024L;
  g_dwCalibrationSize = 64L * 1024L;
  g_lpReadImageHead = NULL;

  g_isCanceled = FALSE;
  g_bFirstReadImage = TRUE;
  g_bOpened = FALSE;
  g_bPrepared = FALSE;
  g_bSharpen = FALSE;

  g_isScanning = FALSE;
  g_isSelfGamma = FALSE;
  g_pGammaTable = NULL;

  if (NULL != g_pDeviceFile)
    {
      free (g_pDeviceFile);
      g_pDeviceFile = NULL;
    }

  g_ssScanSource = SS_Reflective;
  g_PixelFlavor = PF_BlackIs0;

  Asic_Close (&g_chip);

  DBG (DBG_FUNC, "MustScanner_Init: leave MustScanner_Init\n");

  return TRUE;
}

/**********************************************************************
Author: Jack            Date: 2005/05/13
Routine Description: 
	check the scanner connect status
Parameters:
	none
Return value: 
	if scanner's status is OK
	return TRUE
	else 
	return FASLE
***********************************************************************/
static SANE_Bool
MustScanner_GetScannerState ()
{

  if (STATUS_GOOD != Asic_Open (&g_chip, g_pDeviceFile))
    {
      DBG (DBG_FUNC, "MustScanner_GetScannerState: Asic_Open return error\n");
      return FALSE;
    }
  else
    {
      Asic_Close (&g_chip);
      return TRUE;
    }
}

/**********************************************************************
Author: Jack            Date: 2005/05/13
Routine Description: 
	Turn the lamp on or off
Parameters:
	isLampOn: turn the lamp on or off
	isTALampOn: turn the TA lamp on or off
Return value: 
	if operation success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_PowerControl (SANE_Bool isLampOn, SANE_Bool isTALampOn)
{
  SANE_Bool hasTA;
  DBG (DBG_FUNC, "MustScanner_PowerControl: Call in\n");
  if (STATUS_GOOD != Asic_Open (&g_chip, g_pDeviceFile))
    {
      DBG (DBG_FUNC, "MustScanner_PowerControl: Asic_Open return error\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_TurnLamp (&g_chip, isLampOn))
    {
      DBG (DBG_FUNC,
	   "MustScanner_PowerControl: Asic_TurnLamp return error\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_IsTAConnected (&g_chip, &hasTA))
    {
      DBG (DBG_FUNC,
	   "MustScanner_PowerControl: Asic_IsTAConnected return error\n");
      return FALSE;
    }

  if (hasTA)
    {
      if (STATUS_GOOD != Asic_TurnTA (&g_chip, isTALampOn))
	{
	  DBG (DBG_FUNC,
	       "MustScanner_PowerControl: Asic_TurnTA return error\n");
	  return FALSE;
	}
    }

  Asic_Close (&g_chip);

  DBG (DBG_FUNC,
       "MustScanner_PowerControl: leave MustScanner_PowerControl\n");
  return TRUE;
}

/**********************************************************************
Author: Jack            Date: 2005/05/13
Routine Description: 
	Turn the carriage home
Parameters:
	none
Return value: 
	if the operation success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_BackHome ()
{
  DBG (DBG_FUNC, "MustScanner_BackHome: call in \n");

  if (STATUS_GOOD != Asic_Open (&g_chip, g_pDeviceFile))
    {
      DBG (DBG_FUNC, "MustScanner_BackHome: Asic_Open return error\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_CarriageHome (&g_chip, FALSE))
    {
      DBG (DBG_FUNC,
	   "MustScanner_BackHome: Asic_CarriageHome return error\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_WaitUnitReady (&g_chip))
    {
      DBG (DBG_FUNC,
	   "MustScanner_BackHome: Asic_WaitUnitReady return error\n");
      return FALSE;
    }

  Asic_Close (&g_chip);

  DBG (DBG_FUNC, "MustScanner_BackHome: leave  MustScanner_BackHome\n");
  return TRUE;
}

/**********************************************************************
Author: Jack            Date: 2005/05/13
Routine Description: 
	prepare the scan image
Parameters:
	bScanSource: the scan source
Return value: 
	if operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_Prepare (SANE_Byte bScanSource)
{
  DBG (DBG_FUNC, "MustScanner_Prepare: call in\n");

  if (STATUS_GOOD != Asic_Open (&g_chip, g_pDeviceFile))


    {
      DBG (DBG_FUNC, "MustScanner_Prepare: Asic_Open return error\n");
      return FALSE;
    }
  if (STATUS_GOOD != Asic_WaitUnitReady (&g_chip))
    {
      DBG (DBG_FUNC,
	   "MustScanner_Prepare: Asic_WaitUnitReady return error\n");
      return FALSE;
    }

  if (SS_Reflective == bScanSource)
    {
      DBG (DBG_FUNC, "MustScanner_Prepare:ScanSource is SS_Reflective\n");
      if (STATUS_GOOD != Asic_TurnLamp (&g_chip, TRUE))
	{
	  DBG (DBG_FUNC, "MustScanner_Prepare: Asic_TurnLamp return error\n");
	  return FALSE;
	}

      if (STATUS_GOOD != Asic_SetSource (&g_chip, LS_REFLECTIVE))
	{
	  DBG (DBG_FUNC,
	       "MustScanner_Prepare: Asic_SetSource return error\n");
	  return FALSE;
	}
    }
  else if (SS_Positive == bScanSource)
    {
      DBG (DBG_FUNC, "MustScanner_Prepare:ScanSource is SS_Positive\n");
      if (STATUS_GOOD != Asic_TurnTA (&g_chip, TRUE))
	{
	  DBG (DBG_FUNC, "MustScanner_Prepare: Asic_TurnTA return error\n");
	  return FALSE;
	}
      if (STATUS_GOOD != Asic_SetSource (&g_chip, LS_POSITIVE))
	{
	  DBG (DBG_FUNC,
	       "MustScanner_Prepare: Asic_SetSource return error\n");
	  return FALSE;
	}
    }
  else if (SS_Negative == bScanSource)
    {
      DBG (DBG_FUNC, "MustScanner_Prepare:ScanSource is SS_Negative\n");

      if (STATUS_GOOD != Asic_TurnTA (&g_chip, TRUE))
	{
	  DBG (DBG_FUNC, "MustScanner_Prepare: Asic_TurnTA return error\n");
	  return FALSE;
	}

      if (STATUS_GOOD != Asic_SetSource (&g_chip, LS_NEGATIVE))
	{
	  DBG (DBG_FUNC,
	       "MustScanner_Prepare: Asic_SetSource return error\n");
	  return FALSE;
	}
      DBG (DBG_FUNC, "MustScanner_Prepare: Asic_SetSource return good\n");
    }

  Asic_Close (&g_chip);
  g_bPrepared = TRUE;

  DBG (DBG_FUNC, "MustScanner_Prepare: leave MustScanner_Prepare\n");
  return TRUE;
}

#ifdef SANE_UNUSED
/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Adjuest the offset
Parameters:
	nTimes: Adjuest offset the times
	bDirection: whether direction
	bOffset: the data of offset
	bLastMin: the last min data
	bLastOffset: the last offset data
	wMinValue: the min value of offset
	bOffsetUpperBound: the upper bound of offset
	bOffsetLowerBound: the lower bound of offset
	wStdMinLevel: the min level of offset
	wStdMaxLevel: the max level of offset
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_AdjustOffset (int nTimes, SANE_Bool * bDirection, SANE_Byte * bOffset,
			  SANE_Byte * bLastMin, SANE_Byte * bLastOffset,
			  unsigned short * wMinValue, SANE_Byte * bOffsetUpperBound,
			  SANE_Byte * bOffsetLowerBound, unsigned short wStdMinLevel,
			  unsigned short wStdMaxLevel)
{
  if ((*wMinValue <= wStdMaxLevel) && (*wMinValue >= wStdMinLevel))
    {
      return TRUE;
    }

  if (nTimes == 0)
    {
      *bLastMin = LOSANE_Byte (*wMinValue);
      *bLastOffset = *bOffset;

      if (*wMinValue == 255)
	{
	  *bOffset = 0;
	}
      else
	{
	  *bOffset = 255;
	}
    }

  if (nTimes == 1)
    {
      if (*wMinValue > *bLastMin)
	{
	  if (*wMinValue > wStdMaxLevel && *bLastMin > wStdMaxLevel)
	    {
	      *bDirection = !(*bDirection);
	      return TRUE;
	    }

	  if (*wMinValue < wStdMinLevel && *bLastMin < wStdMinLevel)
	    return TRUE;
	}

      if (*wMinValue < *bLastMin)
	{
	  if (*wMinValue < wStdMinLevel && *bLastMin < wStdMinLevel)
	    *bDirection = !(*bDirection);

	  if (*wMinValue > wStdMaxLevel && *bLastMin > wStdMaxLevel)
	    return TRUE;
	}
    }

  if (nTimes > 1)
    {
      if (*wMinValue > *bLastMin)
	{
	  SANE_Byte bTemp;

	  bTemp = *bOffset;

	  *bOffset = (*bOffsetUpperBound + *bOffsetLowerBound) / 2;

	  if (nTimes > 2)
	    {
	      if (*wMinValue > wStdMaxLevel)
		if (bDirection)
		  *bOffsetLowerBound = bTemp;
		else
		  *bOffsetUpperBound = bTemp;


	      else if (bDirection)
		*bOffsetUpperBound = bTemp;
	      else
		*bOffsetLowerBound = bTemp;
	    }

	  *bLastOffset = bTemp;
	  *bLastMin = (SANE_Byte) * wMinValue;
	}
      else
	{
	  SANE_Byte bTemp;

	  bTemp = *bOffset;

	  *bOffset = (*bOffsetUpperBound + *bOffsetLowerBound) / 2;

	  if (nTimes > 2)
	    {
	      if (*wMinValue == *bLastMin)
		{
		  if (*wMinValue > wStdMaxLevel)
		    {
		      if (!bDirection)
			*bOffsetUpperBound = bTemp;
		      else
			*bOffsetLowerBound = bTemp;
		    }
		  else
		    {
		      if (!bDirection)
			*bOffsetLowerBound = bTemp;
		      else
			*bOffsetUpperBound = bTemp;
		    }
		}
	      else
		{
		  if (*wMinValue > wStdMaxLevel)
		    {
		      if (bDirection)
			*bOffsetUpperBound = bTemp;
		      else
			*bOffsetLowerBound = bTemp;
		    }
		  else
		    {
		      if (bDirection)
			*bOffsetLowerBound = bTemp;
		      else
			*bOffsetUpperBound = bTemp;
		    }
		}
	    }

	  *bLastOffset = bTemp;
	  *bLastMin = (SANE_Byte) * wMinValue;

	}
    }				/* end of if(nTimes > 1) */

  return TRUE;
}
#endif

#ifdef SANE_UNUSED
/**********************************************************************

Author: Jack             Date: 2005/05/15
Routine Description: 
	Adjuest the offset second times
Parameters:
	nTimes: Adjuest offset the times
	bDirection: whether direction
	bOffset: the data of offset
	bLastMin: the last min data
	bLastOffset: the last offset data
	wMinValue: the min value of offset
	bOffsetUpperBound: the upper bound of offset
	bOffsetLowerBound: the lower bound of offset
	wStdMinLevel: the min level of offset
	wStdMaxLevel: the max level of offset
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_SecondAdjustOffset (int nTimes, SANE_Bool * bDirection, SANE_Byte * bOffset,
				SANE_Byte * bLastMin, SANE_Byte * bLastOffset,
				unsigned short * wMinValue, SANE_Byte * bOffsetUpperBound,
				SANE_Byte * bOffsetLowerBound, unsigned short wStdMinLevel,
				unsigned short wStdMaxLevel)
{
  if ((*wMinValue <= wStdMaxLevel) && (*wMinValue >= wStdMinLevel))
    {
      return TRUE;
    }
  if (nTimes == 0)
    {
      *bLastMin = LOSANE_Byte (*wMinValue);
      *bLastOffset = *bOffset;

      if (*bDirection == 0)
	{
	  *bOffsetUpperBound = *bLastOffset;
	  *bOffsetLowerBound = 0;
	  *bOffset = 0;
	}
      else
	{
	  *bOffsetUpperBound = 255;
	  *bOffsetLowerBound = *bLastOffset;
	  *bOffset = 255;
	}
    }

  if (nTimes >= 1)
    {
      if (*wMinValue > wStdMaxLevel)
	{
	  if (*wMinValue > *bLastMin)
	    {
	      if (*bDirection == 0)
		{
		  *bOffsetUpperBound = *bOffset;
		}
	      else
		{
		  *bOffsetLowerBound = *bOffset;
		}

	      *bOffset = (*bOffsetUpperBound + *bOffsetLowerBound) / 2;
	    }
	  else
	    {
	      if (*bDirection == 1)
		{
		  *bOffsetUpperBound = *bOffset;
		  *bOffset = (*bOffsetUpperBound + *bOffsetLowerBound) / 2;
		}
	      else
		{
		  *bOffsetLowerBound = *bOffset;
		  *bOffset = (*bOffsetUpperBound + *bOffsetLowerBound) / 2;
		}
	    }
	}			/*end of if(*wMinValue > MAX_OFFSET) */


      if (*wMinValue < wStdMinLevel)
	{
	  if (*wMinValue > *bLastMin)
	    {
	      if (*bDirection == 0)
		{
		  *bOffsetLowerBound = *bOffset;
		}
	      else
		{
		  *bOffsetUpperBound = *bOffset;
		}

	      *bOffset = (*bOffsetUpperBound + *bOffsetLowerBound) / 2;
	    }
	  else
	    {
	      if (*bDirection == 1)
		{
		  *bOffsetUpperBound = *bOffset;
		  *bOffset = (*bOffsetUpperBound + *bOffsetLowerBound) / 2;
		}
	      else
		{
		  *bOffsetLowerBound = *bOffset;
		  *bOffset = (*bOffsetUpperBound + *bOffsetLowerBound) / 2;
		}
	    }
	}			/*end of if(*wMinValue > MIN_OFFSET) */
      *bLastMin = (SANE_Byte) * wMinValue;
    }				/*end of if(nTimes >= 1)     */

  /* HOLD: missing return value! Correct? */
  return FALSE;
}
#endif

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Filter the data
Parameters:
	pSort: the sort data
	TotalCount: the total count 
	LowCount: the low count
	HighCount: the upper count
Return value: 
	the data of Filter 	
***********************************************************************/
static unsigned short
MustScanner_FiltLower (unsigned short * pSort, unsigned short TotalCount, unsigned short LowCount,
		       unsigned short HighCount)
{
  unsigned short Bound = TotalCount - 1;
  unsigned short LeftCount = HighCount - LowCount;
  int Temp = 0;
  unsigned int Sum = 0;
  unsigned short i, j;

  for (i = 0; i < Bound; i++)

    {
      for (j = 0; j < Bound - i; j++)
	{
	  if (pSort[j + 1] > pSort[j])
	    {
	      Temp = pSort[j];
	      pSort[j] = pSort[j + 1];
	      pSort[j + 1] = Temp;
	    }
	}
    }

  for (i = 0; i < LeftCount; i++)
    Sum += pSort[i + LowCount];
  return (unsigned short) (Sum / LeftCount);
}

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Repair line when single CCD and color is 48bit
Parameters:
	lpLine: point to image be repaired
	isOrderInvert: RGB or BGR
	wLinesCount: how many line be repaired
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_GetRgb48BitLine (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
			     unsigned short * wLinesCount)
{
  unsigned short wWantedTotalLines;
  unsigned short TotalXferLines;
  unsigned short wRLinePos = 0;
  unsigned short wGLinePos = 0;
  unsigned short wBLinePos = 0;
  unsigned short wRTempData;
  unsigned short wGTempData;
  unsigned short wBTempData;
  unsigned short i;

  DBG (DBG_FUNC, "MustScanner_GetRgb48BitLine: call in \n");

  g_isCanceled = FALSE;
  g_isScanning = TRUE;
  wWantedTotalLines = *wLinesCount;
  TotalXferLines = 0;

  if (g_bFirstReadImage)
    {
      pthread_create (&g_threadid_readimage, NULL,
		      MustScanner_ReadDataFromScanner, NULL);
      DBG (DBG_FUNC, "MustScanner_GetRgb48BitLine: thread create\n");
      g_bFirstReadImage = FALSE;
    }

  if (!isOrderInvert)
    {
      for (; TotalXferLines < wWantedTotalLines;)
	{
	  if (g_dwTotalTotalXferLines >= g_SWHeight)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);
	      DBG (DBG_FUNC, "MustScanner_GetRgb48BitLine: thread exit\n");

	      *wLinesCount = TotalXferLines;
	      g_isScanning = FALSE;
	      return TRUE;
	    }

	  if (GetScannedLines () > g_wtheReadyLines)
	    {
	      wRLinePos = g_wtheReadyLines % g_wMaxScanLines;
	      wGLinePos =
		(g_wtheReadyLines - g_wLineDistance) % g_wMaxScanLines;
	      wBLinePos =
		(g_wtheReadyLines - g_wLineDistance * 2) % g_wMaxScanLines;

	      for (i = 0; i < g_SWWidth; i++)
		{
		  wRTempData =
		    *(g_lpReadImageHead + wRLinePos * g_BytesPerRow + i * 6 +
		      0);
		  wRTempData +=
		    *(g_lpReadImageHead + wRLinePos * g_BytesPerRow + i * 6 +
		      1) << 8;
		  wGTempData =
		    *(g_lpReadImageHead + wGLinePos * g_BytesPerRow + i * 6 +
		      2);
		  wGTempData +=
		    *(g_lpReadImageHead + wGLinePos * g_BytesPerRow + i * 6 +
		      3) << 8;
		  wBTempData =
		    *(g_lpReadImageHead + wBLinePos * g_BytesPerRow + i * 6 +
		      4);
		  wBTempData +=
		    *(g_lpReadImageHead + wBLinePos * g_BytesPerRow + i * 6 +
		      5) << 8;
		  *(lpLine + i * 6 + 0) = LOBYTE (g_pGammaTable[wRTempData]);
		  *(lpLine + i * 6 + 1) = HIBYTE (g_pGammaTable[wRTempData]);
		  *(lpLine + i * 6 + 2) =
		    LOBYTE (g_pGammaTable[wGTempData + 65536]);
		  *(lpLine + i * 6 + 3) =
		    HIBYTE (g_pGammaTable[wGTempData + 65536]);
		  *(lpLine + i * 6 + 4) =
		    LOBYTE (g_pGammaTable[wBTempData + 131072]);
		  *(lpLine + i * 6 + 5) =
		    HIBYTE (g_pGammaTable[wBTempData + 131072]);
		}
	      TotalXferLines++;
	      g_dwTotalTotalXferLines++;
	      lpLine += g_SWBytesPerRow;
	      AddReadyLines ();
	    }

	  if (g_isCanceled)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);

	      DBG (DBG_FUNC, "MustScanner_GetRgb48BitLine: thread exit\n");

	      break;
	    }
	}
    }
  else
    {
      for (; TotalXferLines < wWantedTotalLines;)
	{
	  if (g_dwTotalTotalXferLines >= g_SWHeight)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);

	      DBG (DBG_FUNC, "MustScanner_GetRgb48BitLine: thread exit\n");


	      *wLinesCount = TotalXferLines;
	      g_isScanning = FALSE;
	      return TRUE;
	    }

	  if (GetScannedLines () > g_wtheReadyLines)
	    {
	      wRLinePos = g_wtheReadyLines % g_wMaxScanLines;
	      wGLinePos =
		(g_wtheReadyLines - g_wLineDistance) % g_wMaxScanLines;
	      wBLinePos =
		(g_wtheReadyLines - g_wLineDistance * 2) % g_wMaxScanLines;

	      for (i = 0; i < g_SWWidth; i++)
		{
		  wRTempData =
		    *(g_lpReadImageHead + wRLinePos * g_BytesPerRow + i * 6 +
		      0);
		  wRTempData +=
		    *(g_lpReadImageHead + wRLinePos * g_BytesPerRow + i * 6 +
		      1) << 8;
		  wGTempData =
		    *(g_lpReadImageHead + wGLinePos * g_BytesPerRow + i * 6 +
		      2);
		  wGTempData +=
		    *(g_lpReadImageHead + wGLinePos * g_BytesPerRow + i * 6 +
		      3) << 8;
		  wBTempData =
		    *(g_lpReadImageHead + wBLinePos * g_BytesPerRow + i * 6 +
		      4);
		  wBTempData +=
		    *(g_lpReadImageHead + wBLinePos * g_BytesPerRow + i * 6 +
		      5) << 8;
		  *(lpLine + i * 6 + 4) = LOBYTE (g_pGammaTable[wRTempData]);
		  *(lpLine + i * 6 + 5) = HIBYTE (g_pGammaTable[wRTempData]);
		  *(lpLine + i * 6 + 2) =
		    LOBYTE (g_pGammaTable[wGTempData + 65536]);
		  *(lpLine + i * 6 + 3) =
		    HIBYTE (g_pGammaTable[wGTempData + 65536]);
		  *(lpLine + i * 6 + 0) =
		    LOBYTE (g_pGammaTable[wBTempData + 131072]);
		  *(lpLine + i * 6 + 1) =
		    HIBYTE (g_pGammaTable[wBTempData + 131072]);
		}

	      TotalXferLines++;
	      g_dwTotalTotalXferLines++;
	      lpLine += g_SWBytesPerRow;
	      AddReadyLines ();

	    }
	  if (g_isCanceled)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);

	      DBG (DBG_FUNC, "MustScanner_GetRgb48BitLine: thread exit\n");
	      break;
	    }
	}			/*end for */
    }

  *wLinesCount = TotalXferLines;
  g_isScanning = FALSE;

  DBG (DBG_FUNC,
       "MustScanner_GetRgb48BitLine: leave MustScanner_GetRgb48BitLine\n");
  return TRUE;
}

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Repair line when double CCD and color is 48bit
Parameters:
	lpLine: point to image be repaired
	isOrderInvert: RGB or BGR
	wLinesCount: how many line be repaired
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_GetRgb48BitLine1200DPI (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
				    unsigned short * wLinesCount)
{
  unsigned short wWantedTotalLines;
  unsigned short TotalXferLines;

  unsigned short wRLinePosOdd = 0;
  unsigned short wGLinePosOdd = 0;
  unsigned short wBLinePosOdd = 0;
  unsigned short wRLinePosEven = 0;
  unsigned short wGLinePosEven = 0;
  unsigned short wBLinePosEven = 0;
  unsigned int wRTempData;
  unsigned int wGTempData;
  unsigned int wBTempData;
  unsigned int wNextTempData;
  unsigned short i;

  DBG (DBG_FUNC, "MustScanner_GetRgb48BitLine1200DPI: call in \n");

  TotalXferLines = 0;
  wWantedTotalLines = *wLinesCount;

  g_isCanceled = FALSE;
  g_isScanning = TRUE;

  if (g_bFirstReadImage)
    {
      pthread_create (&g_threadid_readimage, NULL,
		      MustScanner_ReadDataFromScanner, NULL);
      DBG (DBG_FUNC, "MustScanner_GetRgb48BitLine1200DPI: thread create\n");
      g_bFirstReadImage = FALSE;
    }

  if (!isOrderInvert)
    {
      for (; TotalXferLines < wWantedTotalLines;)
	{
	  if (g_dwTotalTotalXferLines >= g_SWHeight)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb48BitLine1200DPI: thread exit\n");

	      *wLinesCount = TotalXferLines;
	      g_isScanning = FALSE;
	      return TRUE;
	    }

	  if (GetScannedLines () > g_wtheReadyLines)
	    {
	      if (ST_Reflective == g_ScanType)
		{
		  wRLinePosOdd =
		    (g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
		  wGLinePosOdd =
		    (g_wtheReadyLines - g_wLineDistance -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wBLinePosOdd =
		    (g_wtheReadyLines - g_wLineDistance * 2 -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wRLinePosEven = (g_wtheReadyLines) % g_wMaxScanLines;
		  wGLinePosEven =
		    (g_wtheReadyLines - g_wLineDistance) % g_wMaxScanLines;
		  wBLinePosEven =
		    (g_wtheReadyLines -
		     g_wLineDistance * 2) % g_wMaxScanLines;
		}
	      else
		{
		  wRLinePosEven =
		    (g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
		  wGLinePosEven =
		    (g_wtheReadyLines - g_wLineDistance -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wBLinePosEven =
		    (g_wtheReadyLines - g_wLineDistance * 2 -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wRLinePosOdd = (g_wtheReadyLines) % g_wMaxScanLines;
		  wGLinePosOdd =
		    (g_wtheReadyLines - g_wLineDistance) % g_wMaxScanLines;
		  wBLinePosOdd =
		    (g_wtheReadyLines -
		     g_wLineDistance * 2) % g_wMaxScanLines;
		}

	      for (i = 0; i < g_SWWidth;)
		{
		  if (i + 1 != g_SWWidth)
		    {
		      wRTempData =
			*(g_lpReadImageHead + wRLinePosOdd * g_BytesPerRow +
			  i * 6 + 0);
		      wRTempData +=
			*(g_lpReadImageHead + wRLinePosOdd * g_BytesPerRow +
			  i * 6 + 1) << 8;
		      wNextTempData =
			*(g_lpReadImageHead + wRLinePosEven * g_BytesPerRow +
			  (i + 1) * 6 + 0);
		      wNextTempData +=
			*(g_lpReadImageHead + wRLinePosEven * g_BytesPerRow +
			  (i + 1) * 6 + 1) << 8;
		      wRTempData = (wRTempData + wNextTempData) >> 1;

		      wGTempData =
			*(g_lpReadImageHead + wGLinePosOdd * g_BytesPerRow +
			  i * 6 + 2);
		      wGTempData +=
			*(g_lpReadImageHead + wGLinePosOdd * g_BytesPerRow +
			  i * 6 + 3) << 8;
		      wNextTempData =
			*(g_lpReadImageHead + wGLinePosEven * g_BytesPerRow +
			  (i + 1) * 6 + 2);
		      wNextTempData +=
			*(g_lpReadImageHead + wGLinePosEven * g_BytesPerRow +
			  (i + 1) * 6 + 3) << 8;
		      wGTempData = (wGTempData + wNextTempData) >> 1;

		      wBTempData =
			*(g_lpReadImageHead + wBLinePosOdd * g_BytesPerRow +
			  i * 6 + 4);
		      wBTempData +=
			*(g_lpReadImageHead + wBLinePosOdd * g_BytesPerRow +
			  i * 6 + 5) << 8;
		      wNextTempData =
			*(g_lpReadImageHead + wBLinePosEven * g_BytesPerRow +
			  (i + 1) * 6 + 4);
		      wNextTempData +=
			*(g_lpReadImageHead + wBLinePosEven * g_BytesPerRow +
			  (i + 1) * 6 + 5) << 8;
		      wBTempData = (wBTempData + wNextTempData) >> 1;

		      *(lpLine + i * 6 + 0) =
			LOBYTE (g_pGammaTable[wRTempData]);
		      *(lpLine + i * 6 + 1) =
			HIBYTE (g_pGammaTable[wRTempData]);
		      *(lpLine + i * 6 + 2) =
			LOBYTE (g_pGammaTable[wGTempData + 65536]);
		      *(lpLine + i * 6 + 3) =
			HIBYTE (g_pGammaTable[wGTempData + 65536]);
		      *(lpLine + i * 6 + 4) =
			LOBYTE (g_pGammaTable[wBTempData + 131072]);
		      *(lpLine + i * 6 + 5) =
			HIBYTE (g_pGammaTable[wBTempData + 131072]);
		      i++;
		      if (i >= g_SWWidth)
			{
			  break;
			}

		      wRTempData =
			*(g_lpReadImageHead + wRLinePosEven * g_BytesPerRow +
			  i * 6 + 0);
		      wRTempData +=
			*(g_lpReadImageHead + wRLinePosEven * g_BytesPerRow +
			  i * 6 + 1) << 8;
		      wNextTempData =
			*(g_lpReadImageHead + wRLinePosOdd * g_BytesPerRow +
			  (i + 1) * 6 + 0);
		      wNextTempData +=
			*(g_lpReadImageHead + wRLinePosOdd * g_BytesPerRow +
			  (i + 1) * 6 + 1) << 8;
		      wRTempData = (wRTempData + wNextTempData) >> 1;

		      wGTempData =
			*(g_lpReadImageHead + wGLinePosEven * g_BytesPerRow +
			  i * 6 + 2);
		      wGTempData +=
			*(g_lpReadImageHead + wGLinePosEven * g_BytesPerRow +
			  i * 6 + 3) << 8;
		      wNextTempData =
			*(g_lpReadImageHead + wGLinePosOdd * g_BytesPerRow +
			  (i + 1) * 6 + 2);
		      wNextTempData +=
			*(g_lpReadImageHead + wGLinePosOdd * g_BytesPerRow +
			  (i + 1) * 6 + 3) << 8;
		      wGTempData = (wGTempData + wNextTempData) >> 1;

		      wBTempData =
			*(g_lpReadImageHead + wBLinePosEven * g_BytesPerRow +
			  i * 6 + 4);
		      wBTempData +=
			*(g_lpReadImageHead + wBLinePosEven * g_BytesPerRow +
			  i * 6 + 5) << 8;
		      wNextTempData =
			*(g_lpReadImageHead + wBLinePosOdd * g_BytesPerRow +
			  (i + 1) * 6 + 4);
		      wNextTempData +=
			*(g_lpReadImageHead + wBLinePosOdd * g_BytesPerRow +
			  (i + 1) * 6 + 5) << 8;
		      wBTempData = (wBTempData + wNextTempData) >> 1;

		      *(lpLine + i * 6 + 0) =
			LOBYTE (g_pGammaTable[wRTempData]);
		      *(lpLine + i * 6 + 1) =
			HIBYTE (g_pGammaTable[wRTempData]);
		      *(lpLine + i * 6 + 2) =
			LOBYTE (g_pGammaTable[wGTempData + 65536]);
		      *(lpLine + i * 6 + 3) =
			HIBYTE (g_pGammaTable[wGTempData + 65536]);
		      *(lpLine + i * 6 + 4) =
			LOBYTE (g_pGammaTable[wBTempData + 131072]);
		      *(lpLine + i * 6 + 5) =
			HIBYTE (g_pGammaTable[wBTempData + 131072]);

		      i++;
		    }
		}

	      TotalXferLines++;
	      g_dwTotalTotalXferLines++;
	      lpLine += g_SWBytesPerRow;
	      AddReadyLines ();
	    }
	  if (g_isCanceled)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb48BitLine1200DPI: thread exit\n");
	      break;

	    }
	}


    }
  else
    {
      for (; TotalXferLines < wWantedTotalLines;)
	{
	  if (g_dwTotalTotalXferLines >= g_SWHeight)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb48BitLine1200DPI: thread exit\n");

	      *wLinesCount = TotalXferLines;
	      g_isScanning = FALSE;
	      return TRUE;
	    }

	  if (GetScannedLines () > g_wtheReadyLines)
	    {
	      if (ST_Reflective == g_ScanType)
		{
		  wRLinePosOdd =
		    (g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
		  wGLinePosOdd =
		    (g_wtheReadyLines - g_wLineDistance -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wBLinePosOdd =
		    (g_wtheReadyLines - g_wLineDistance * 2 -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wRLinePosEven = (g_wtheReadyLines) % g_wMaxScanLines;
		  wGLinePosEven =
		    (g_wtheReadyLines - g_wLineDistance) % g_wMaxScanLines;
		  wBLinePosEven =
		    (g_wtheReadyLines -
		     g_wLineDistance * 2) % g_wMaxScanLines;
		}
	      else
		{
		  wRLinePosEven =
		    (g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
		  wGLinePosEven =
		    (g_wtheReadyLines - g_wLineDistance -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wBLinePosEven =
		    (g_wtheReadyLines - g_wLineDistance * 2 -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wRLinePosOdd = (g_wtheReadyLines) % g_wMaxScanLines;
		  wGLinePosOdd =
		    (g_wtheReadyLines - g_wLineDistance) % g_wMaxScanLines;
		  wBLinePosOdd =
		    (g_wtheReadyLines -
		     g_wLineDistance * 2) % g_wMaxScanLines;
		}

	      for (i = 0; i < g_SWWidth;)
		{
		  if ((i + 1) != g_SWWidth)
		    {
		      wRTempData =
			*(g_lpReadImageHead + wRLinePosOdd * g_BytesPerRow +
			  i * 6 + 0);
		      wRTempData +=
			*(g_lpReadImageHead + wRLinePosOdd * g_BytesPerRow +
			  i * 6 + 1) << 8;
		      wNextTempData =
			*(g_lpReadImageHead + wRLinePosEven * g_BytesPerRow +
			  (i + 1) * 6 + 0);
		      wNextTempData +=
			*(g_lpReadImageHead + wRLinePosEven * g_BytesPerRow +
			  (i + 1) * 6 + 1) << 8;
		      wRTempData = (wRTempData + wNextTempData) >> 1;

		      wGTempData =
			*(g_lpReadImageHead + wGLinePosOdd * g_BytesPerRow +
			  i * 6 + 2);
		      wGTempData +=
			*(g_lpReadImageHead + wGLinePosOdd * g_BytesPerRow +
			  i * 6 + 3) << 8;
		      wNextTempData =
			*(g_lpReadImageHead + wGLinePosEven * g_BytesPerRow +
			  (i + 1) * 6 + 2);
		      wNextTempData +=
			*(g_lpReadImageHead + wGLinePosEven * g_BytesPerRow +
			  (i + 1) * 6 + 3) << 8;
		      wGTempData = (wGTempData + wNextTempData) >> 1;

		      wBTempData =
			*(g_lpReadImageHead + wBLinePosOdd * g_BytesPerRow +
			  i * 6 + 4);
		      wBTempData +=
			*(g_lpReadImageHead + wBLinePosOdd * g_BytesPerRow +
			  i * 6 + 5) << 8;
		      wNextTempData =
			*(g_lpReadImageHead + wBLinePosEven * g_BytesPerRow +
			  (i + 1) * 6 + 4);
		      wNextTempData +=
			*(g_lpReadImageHead + wBLinePosEven * g_BytesPerRow +
			  (i + 1) * 6 + 5) << 8;
		      wBTempData = (wBTempData + wNextTempData) >> 1;

		      *(lpLine + i * 6 + 4) =
			LOBYTE (g_pGammaTable[wRTempData]);
		      *(lpLine + i * 6 + 5) =
			HIBYTE (g_pGammaTable[wRTempData]);
		      *(lpLine + i * 6 + 2) =
			LOBYTE (g_pGammaTable[wGTempData + 65536]);
		      *(lpLine + i * 6 + 3) =
			HIBYTE (g_pGammaTable[wGTempData + 65536]);
		      *(lpLine + i * 6 + 0) =
			LOBYTE (g_pGammaTable[wBTempData + 131072]);
		      *(lpLine + i * 6 + 1) =
			HIBYTE (g_pGammaTable[wBTempData + 131072]);
		      i++;
		      if (i >= g_SWWidth)
			{
			  break;
			}

		      wRTempData =
			*(g_lpReadImageHead + wRLinePosEven * g_BytesPerRow +
			  i * 6 + 0);
		      wRTempData +=
			*(g_lpReadImageHead + wRLinePosEven * g_BytesPerRow +
			  i * 6 + 1) << 8;
		      wNextTempData =
			*(g_lpReadImageHead + wRLinePosOdd * g_BytesPerRow +
			  (i + 1) * 6 + 0);
		      wNextTempData +=
			*(g_lpReadImageHead + wRLinePosOdd * g_BytesPerRow +
			  (i + 1) * 6 + 1) << 8;
		      wRTempData = (wRTempData + wNextTempData) >> 1;

		      wGTempData =
			*(g_lpReadImageHead + wGLinePosEven * g_BytesPerRow +
			  i * 6 + 2);
		      wGTempData +=
			*(g_lpReadImageHead + wGLinePosEven * g_BytesPerRow +
			  i * 6 + 3) << 8;
		      wNextTempData =
			*(g_lpReadImageHead + wGLinePosOdd * g_BytesPerRow +
			  (i + 1) * 6 + 2);
		      wNextTempData +=
			*(g_lpReadImageHead + wGLinePosOdd * g_BytesPerRow +
			  (i + 1) * 6 + 3) << 8;
		      wGTempData = (wGTempData + wNextTempData) >> 1;


		      wBTempData =
			*(g_lpReadImageHead + wBLinePosEven * g_BytesPerRow +
			  i * 6 + 4);
		      wBTempData +=
			*(g_lpReadImageHead + wBLinePosEven * g_BytesPerRow +
			  i * 6 + 5) << 8;
		      wNextTempData =
			*(g_lpReadImageHead + wBLinePosOdd * g_BytesPerRow +
			  (i + 1) * 6 + 4);
		      wNextTempData +=
			*(g_lpReadImageHead + wBLinePosOdd * g_BytesPerRow +
			  (i + 1) * 6 + 5) << 8;
		      wBTempData = (wBTempData + wNextTempData) >> 1;

		      *(lpLine + i * 6 + 4) =
			LOBYTE (g_pGammaTable[wRTempData]);
		      *(lpLine + i * 6 + 5) =
			HIBYTE (g_pGammaTable[wRTempData]);
		      *(lpLine + i * 6 + 2) =
			LOBYTE (g_pGammaTable[wGTempData + 65536]);
		      *(lpLine + i * 6 + 3) =
			HIBYTE (g_pGammaTable[wGTempData + 65536]);
		      *(lpLine + i * 6 + 0) =
			LOBYTE (g_pGammaTable[wBTempData + 131072]);
		      *(lpLine + i * 6 + 1) =
			HIBYTE (g_pGammaTable[wBTempData + 131072]);
		      i++;
		    }
		}

	      TotalXferLines++;
	      g_dwTotalTotalXferLines++;
	      lpLine += g_SWBytesPerRow;
	      AddReadyLines ();
	    }
	  if (g_isCanceled)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);

	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb48BitLine1200DPI: thread exit\n");

	      break;
	    }
	}
    }

  *wLinesCount = TotalXferLines;
  g_isScanning = FALSE;

  DBG (DBG_FUNC,
       "MustScanner_GetRgb48BitLine1200DPI: leave MustScanner_GetRgb48BitLine1200DPI\n");
  return TRUE;
}

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Repair line when single CCD and color is 24bit
Parameters:
	lpLine: point to image be repaired
	isOrderInvert: RGB or BGR
	wLinesCount: how many line be repaired
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_GetRgb24BitLine (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
			     unsigned short * wLinesCount)
{
  unsigned short wWantedTotalLines;
  unsigned short TotalXferLines;
  unsigned short wRLinePos = 0;
  unsigned short wGLinePos = 0;
  unsigned short wBLinePos = 0;
  SANE_Byte byRed;
  SANE_Byte byGreen;
  SANE_Byte byBlue;
  SANE_Byte bNextPixel = 0;
  unsigned short i;

  unsigned short tempR, tempG, tempB;

  DBG (DBG_FUNC, "MustScanner_GetRgb24BitLine: call in\n");

  g_isCanceled = FALSE;
  g_isScanning = TRUE;

  wWantedTotalLines = *wLinesCount;
  DBG (DBG_FUNC,
       "MustScanner_GetRgb24BitLine: get wWantedTotalLines= %d\n",
       wWantedTotalLines);

  TotalXferLines = 0;

  if (g_bFirstReadImage)
    {
      pthread_create (&g_threadid_readimage, NULL,
		      MustScanner_ReadDataFromScanner, NULL);
      DBG (DBG_FUNC, "MustScanner_GetRgb24BitLine: thread create\n");

      g_bFirstReadImage = FALSE;
    }

  if (!isOrderInvert)
    {
      DBG (DBG_FUNC, "MustScanner_GetRgb24BitLine: !isOrderInvert\n");

      for (; TotalXferLines < wWantedTotalLines;)
	{
	  if (g_dwTotalTotalXferLines >= g_SWHeight)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);
	      DBG (DBG_FUNC, "MustScanner_GetRgb24BitLine: thread exit\n");

	      *wLinesCount = TotalXferLines;
	      g_isScanning = FALSE;
	      return TRUE;
	    }

	  if (GetScannedLines () > g_wtheReadyLines)
	    {
	      wRLinePos = g_wtheReadyLines % g_wMaxScanLines;
	      wGLinePos =
		(g_wtheReadyLines - g_wLineDistance) % g_wMaxScanLines;
	      wBLinePos =
		(g_wtheReadyLines - g_wLineDistance * 2) % g_wMaxScanLines;

	      for (i = 0; i < g_SWWidth; i++)
		{
		  byRed =
		    *(g_lpReadImageHead + wRLinePos * g_BytesPerRow + i * 3 +
		      0);
		  bNextPixel =
		    *(g_lpReadImageHead + wRLinePos * g_BytesPerRow +
		      (i + 1) * 3 + 0);
		  byRed = (byRed + bNextPixel) >> 1;

		  byGreen =
		    *(g_lpReadImageHead + wGLinePos * g_BytesPerRow + i * 3 +
		      1);
		  bNextPixel =
		    *(g_lpReadImageHead + wGLinePos * g_BytesPerRow +
		      (i + 1) * 3 + 1);
		  byGreen = (byGreen + bNextPixel) >> 1;

		  byBlue =
		    *(g_lpReadImageHead + wBLinePos * g_BytesPerRow + i * 3 +
		      2);
		  bNextPixel =
		    *(g_lpReadImageHead + wBLinePos * g_BytesPerRow +
		      (i + 1) * 3 + 2);
		  byBlue = (byBlue + bNextPixel) >> 1;

#ifdef ENABLE_GAMMA
		  tempR = (unsigned short) ((byRed << 4) | QBET4 (byBlue, byGreen));
		  tempG = (unsigned short) ((byGreen << 4) | QBET4 (byRed, byBlue));
		  tempB = (unsigned short) ((byBlue << 4) | QBET4 (byGreen, byRed));

		  *(lpLine + i * 3 + 0) =
		    (unsigned char) (*(g_pGammaTable + tempR));
		  *(lpLine + i * 3 + 1) =
		    (unsigned char) (*(g_pGammaTable + 4096 + tempG));
		  *(lpLine + i * 3 + 2) =
		    (unsigned char) (*(g_pGammaTable + 8192 + tempB));
#else
		  *(lpLine + i * 3 + 0) = (unsigned char) byRed;
		  *(lpLine + i * 3 + 1) = (unsigned char) byGreen;
		  *(lpLine + i * 3 + 2) = (unsigned char) byBlue;
#endif
		}

	      TotalXferLines++;
	      g_dwTotalTotalXferLines++;
	      lpLine += g_SWBytesPerRow;
	      AddReadyLines ();

	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine: g_dwTotalTotalXferLines=%d,g_SWHeight=%d\n",
		   g_dwTotalTotalXferLines, g_SWHeight);
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine: g_SWBytesPerRow=%d\n",
		   g_SWBytesPerRow);
	    }
	  if (g_isCanceled)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);
	      DBG (DBG_FUNC, "MustScanner_GetRgb24BitLine: thread exit\n");

	      break;
	    }
	}
    }
  else
    {
      DBG (DBG_FUNC, "MustScanner_GetRgb24BitLine: isOrderInvert is TRUE\n");
      for (; TotalXferLines < wWantedTotalLines;)
	{
	  if (g_dwTotalTotalXferLines >= g_SWHeight)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);
	      DBG (DBG_FUNC, "MustScanner_GetRgb24BitLine: thread exit\n");

	      *wLinesCount = TotalXferLines;
	      g_isScanning = FALSE;
	      return TRUE;
	    }

	  if (GetScannedLines () > g_wtheReadyLines)
	    {
	      wRLinePos = g_wtheReadyLines % g_wMaxScanLines;
	      wGLinePos =
		(g_wtheReadyLines - g_wLineDistance) % g_wMaxScanLines;
	      wBLinePos =
		(g_wtheReadyLines - g_wLineDistance * 2) % g_wMaxScanLines;

	      for (i = 0; i < g_SWWidth; i++)
		{
		  DBG (DBG_FUNC,
		       "MustScanner_GetRgb24BitLine: before byRed\n");
		  byRed =
		    *(g_lpReadImageHead + wRLinePos * g_BytesPerRow + i * 3 +
		      0);
		  bNextPixel = *(g_lpReadImageHead + wRLinePos * g_BytesPerRow + (i + 1) * 3 + 0);	/*R-channel */
		  byRed = (byRed + bNextPixel) >> 1;

		  DBG (DBG_FUNC,
		       "MustScanner_GetRgb24BitLine: before byGreen\n");

		  byGreen =
		    *(g_lpReadImageHead + wGLinePos * g_BytesPerRow + i * 3 +
		      1);
		  bNextPixel = *(g_lpReadImageHead + wGLinePos * g_BytesPerRow + (i + 1) * 3 + 1);	/*G-channel */
		  byGreen = (byGreen + bNextPixel) >> 1;

		  DBG (DBG_FUNC,
		       "MustScanner_GetRgb24BitLine: before byBlue\n");

		  byBlue =
		    *(g_lpReadImageHead + wBLinePos * g_BytesPerRow + i * 3 +
		      2);
		  bNextPixel = *(g_lpReadImageHead + wBLinePos * g_BytesPerRow + (i + 1) * 3 + 2);	/*B-channel */
		  byBlue = (byBlue + bNextPixel) >> 1;


		  DBG (DBG_FUNC,
		       "MustScanner_GetRgb24BitLine: before set lpLine\n");
		  DBG (DBG_FUNC, "MustScanner_GetRgb24BitLine: i=%d\n", i);
#ifdef ENABLE_GAMMA
		  *(lpLine + i * 3 + 2) =
		    (unsigned
		     char) (*(g_pGammaTable +
			      (unsigned short) ((byRed << 4) |
				      QBET4 (byBlue, byGreen))));
		  *(lpLine + i * 3 + 1) =
		    (unsigned
		     char) (*(g_pGammaTable + 4096 +
			      (unsigned short) ((byGreen << 4) |
				      QBET4 (byRed, byBlue))));
		  *(lpLine + i * 3 + 0) =
		    (unsigned
		     char) (*(g_pGammaTable + 8192 +
			      (unsigned short) ((byBlue << 4) |
				      QBET4 (byGreen, byRed))));
#else
		  *(lpLine + i * 3 + 2) = (unsigned char) byRed;
		  *(lpLine + i * 3 + 1) = (unsigned char) byGreen;
		  *(lpLine + i * 3 + 0) = (unsigned char) byBlue;
#endif
		}

	      TotalXferLines++;
	      g_dwTotalTotalXferLines++;
	      lpLine += g_SWBytesPerRow;
	      AddReadyLines ();

	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine: g_dwTotalTotalXferLines=%d,g_SWHeight=%d\n",
		   g_dwTotalTotalXferLines, g_SWHeight);
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine: g_SWBytesPerRow=%d\n",
		   g_SWBytesPerRow);
	    }
	  if (g_isCanceled)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);
	      DBG (DBG_FUNC, "MustScanner_GetRgb24BitLine: thread exit\n");

	      break;
	    }
	}			/*end for */
    }

  *wLinesCount = TotalXferLines;
  g_isScanning = FALSE;

  DBG (DBG_FUNC,
       "MustScanner_GetRgb24BitLine: leave MustScanner_GetRgb24BitLine\n");
  return TRUE;
}

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Repair line when double CCD and color is 24bit
Parameters:
	lpLine: point to image be repaired
	isOrderInvert: RGB or BGR
	wLinesCount: how many line be repaired
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_GetRgb24BitLine1200DPI (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
				    unsigned short * wLinesCount)
{
  SANE_Byte *lpTemp;
  unsigned short wWantedTotalLines;
  unsigned short TotalXferLines;
  unsigned short wRLinePosOdd = 0;
  unsigned short wGLinePosOdd = 0;
  unsigned short wBLinePosOdd = 0;
  unsigned short wRLinePosEven = 0;
  unsigned short wGLinePosEven = 0;
  unsigned short wBLinePosEven = 0;
  SANE_Byte byRed;
  SANE_Byte byGreen;
  SANE_Byte byBlue;
  SANE_Byte bNextPixel = 0;
  unsigned short i;

  DBG (DBG_FUNC, "MustScanner_GetRgb24BitLine1200DPI: call in\n");

  g_isCanceled = FALSE;
  g_isScanning = TRUE;
  TotalXferLines = 0;
  wWantedTotalLines = *wLinesCount;
  lpTemp = lpLine;

  if (g_bFirstReadImage)
    {
      pthread_create (&g_threadid_readimage, NULL,
		      MustScanner_ReadDataFromScanner, NULL);
      DBG (DBG_FUNC, "MustScanner_GetRgb24BitLine1200DPI: thread create\n");

      g_bFirstReadImage = FALSE;
    }

  if (!isOrderInvert)
    {
      for (; TotalXferLines < wWantedTotalLines;)
	{
	  if (g_dwTotalTotalXferLines >= g_SWHeight)
	    {
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine1200DPI: g_dwTotalTotalXferLines=%d\n",
		   g_dwTotalTotalXferLines);
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine1200DPI: g_Height=%d\n",
		   g_Height);

	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine1200DPI: thread exit\n");

	      *wLinesCount = TotalXferLines;
	      g_isScanning = FALSE;
	      return TRUE;
	    }

	  if (GetScannedLines () > g_wtheReadyLines)
	    {
	      if (ST_Reflective == g_ScanType)
		{
		  wRLinePosOdd =
		    (g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
		  wGLinePosOdd =
		    (g_wtheReadyLines - g_wLineDistance -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wBLinePosOdd =
		    (g_wtheReadyLines - g_wLineDistance * 2 -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wRLinePosEven = (g_wtheReadyLines) % g_wMaxScanLines;
		  wGLinePosEven =
		    (g_wtheReadyLines - g_wLineDistance) % g_wMaxScanLines;
		  wBLinePosEven =
		    (g_wtheReadyLines -
		     g_wLineDistance * 2) % g_wMaxScanLines;
		}
	      else
		{
		  wRLinePosEven =
		    (g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
		  wGLinePosEven =
		    (g_wtheReadyLines - g_wLineDistance -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wBLinePosEven =
		    (g_wtheReadyLines - g_wLineDistance * 2 -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wRLinePosOdd = (g_wtheReadyLines) % g_wMaxScanLines;
		  wGLinePosOdd =
		    (g_wtheReadyLines - g_wLineDistance) % g_wMaxScanLines;
		  wBLinePosOdd =
		    (g_wtheReadyLines -
		     g_wLineDistance * 2) % g_wMaxScanLines;
		}



	      for (i = 0; i < g_SWWidth;)
		{
		  if ((i + 1) != g_SWWidth)
		    {
		      byRed =
			*(g_lpReadImageHead + wRLinePosOdd * g_BytesPerRow +
			  i * 3 + 0);
		      bNextPixel = *(g_lpReadImageHead + wRLinePosEven * g_BytesPerRow + (i + 1) * 3 + 0);	/*R-channel */
		      byRed = (byRed + bNextPixel) >> 1;

		      byGreen =
			*(g_lpReadImageHead + wGLinePosOdd * g_BytesPerRow +
			  i * 3 + 1);
		      bNextPixel = *(g_lpReadImageHead + wGLinePosEven * g_BytesPerRow + (i + 1) * 3 + 1);	/*G-channel */
		      byGreen = (byGreen + bNextPixel) >> 1;

		      byBlue =
			*(g_lpReadImageHead + wBLinePosOdd * g_BytesPerRow +
			  i * 3 + 2);
		      bNextPixel = *(g_lpReadImageHead + wBLinePosEven * g_BytesPerRow + (i + 1) * 3 + 2);	/*B-channel */
		      byBlue = (byBlue + bNextPixel) >> 1;
#ifdef ENABLE_GAMMA
		      *(lpLine + i * 3 + 0) =
			(unsigned
			 char) (*(g_pGammaTable +
				  (unsigned short) ((byRed << 4) |
					  QBET4 (byBlue, byGreen))));
		      *(lpLine + i * 3 + 1) =
			(unsigned
			 char) (*(g_pGammaTable + 4096 +
				  (unsigned short) ((byGreen << 4) |
					  QBET4 (byRed, byBlue))));
		      *(lpLine + i * 3 + 2) =
			(unsigned
			 char) (*(g_pGammaTable + 8192 +
				  (unsigned short) ((byBlue << 4) |
					  QBET4 (byGreen, byRed))));
#else
		      *(lpLine + i * 3 + 0) = (unsigned char) byRed;
		      *(lpLine + i * 3 + 1) = (unsigned char) byGreen;
		      *(lpLine + i * 3 + 2) = (unsigned char) byBlue;
#endif

		      i++;
		      if (i >= g_SWWidth)
			{
			  break;
			}

		      byRed =
			*(g_lpReadImageHead + wRLinePosEven * g_BytesPerRow +
			  i * 3 + 0);
		      bNextPixel =
			*(g_lpReadImageHead + wRLinePosOdd * g_BytesPerRow +
			  (i + 1) * 3 + 0);
		      byRed = (byRed + bNextPixel) >> 1;

		      byGreen =
			*(g_lpReadImageHead + wGLinePosEven * g_BytesPerRow +
			  i * 3 + 1);
		      bNextPixel =
			*(g_lpReadImageHead + wGLinePosOdd * g_BytesPerRow +
			  (i + 1) * 3 + 1);
		      byGreen = (byGreen + bNextPixel) >> 1;

		      byBlue =
			*(g_lpReadImageHead + wBLinePosEven * g_BytesPerRow +
			  i * 3 + 2);
		      bNextPixel =
			*(g_lpReadImageHead + wBLinePosOdd * g_BytesPerRow +
			  (i + 1) * 3 + 2);
		      byBlue = (byBlue + bNextPixel) >> 1;
#ifdef ENABLE_GAMMA
		      *(lpLine + i * 3 + 0) =
			(unsigned
			 char) (*(g_pGammaTable +
				  (unsigned short) ((byRed << 4) |
					  QBET4 (byBlue, byGreen))));
		      *(lpLine + i * 3 + 1) =
			(unsigned
			 char) (*(g_pGammaTable + 4096 +
				  (unsigned short) ((byGreen << 4) |
					  QBET4 (byRed, byBlue))));
		      *(lpLine + i * 3 + 2) =
			(unsigned
			 char) (*(g_pGammaTable + 8192 +
				  (unsigned short) ((byBlue << 4) |
					  QBET4 (byGreen, byRed))));
#else
		      *(lpLine + i * 3 + 0) = (unsigned char) byRed;
		      *(lpLine + i * 3 + 1) = (unsigned char) byGreen;
		      *(lpLine + i * 3 + 2) = (unsigned char) byBlue;
#endif
		      i++;
		    }
		}


	      TotalXferLines++;
	      g_dwTotalTotalXferLines++;
	      lpLine += g_SWBytesPerRow;
	      AddReadyLines ();

	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine1200DPI: g_dwTotalTotalXferLines=%d\n",
		   g_dwTotalTotalXferLines);
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine1200DPI: g_Height=%d\n",
		   g_Height);

	    }
	  if (g_isCanceled)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine1200DPI: thread exit\n");

	      break;
	    }

	}
    }
  else
    {

      for (; TotalXferLines < wWantedTotalLines;)
	{
	  if (g_dwTotalTotalXferLines >= g_SWHeight)
	    {
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine1200DPI: g_dwTotalTotalXferLines=%d\n",
		   g_dwTotalTotalXferLines);
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine1200DPI: g_Height=%d\n",
		   g_Height);

	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine1200DPI: thread exit\n");

	      *wLinesCount = TotalXferLines;
	      g_isScanning = FALSE;
	      return TRUE;
	    }

	  if (GetScannedLines () > g_wtheReadyLines)
	    {
	      if (ST_Reflective == g_ScanType)
		{
		  wRLinePosOdd =
		    (g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
		  wGLinePosOdd =
		    (g_wtheReadyLines - g_wLineDistance -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wBLinePosOdd =
		    (g_wtheReadyLines - g_wLineDistance * 2 -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wRLinePosEven = (g_wtheReadyLines) % g_wMaxScanLines;
		  wGLinePosEven =
		    (g_wtheReadyLines - g_wLineDistance) % g_wMaxScanLines;
		  wBLinePosEven =
		    (g_wtheReadyLines -
		     g_wLineDistance * 2) % g_wMaxScanLines;
		}
	      else
		{
		  wRLinePosEven =
		    (g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
		  wGLinePosEven =
		    (g_wtheReadyLines - g_wLineDistance -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wBLinePosEven =
		    (g_wtheReadyLines - g_wLineDistance * 2 -
		     g_wPixelDistance) % g_wMaxScanLines;
		  wRLinePosOdd = (g_wtheReadyLines) % g_wMaxScanLines;
		  wGLinePosOdd =
		    (g_wtheReadyLines - g_wLineDistance) % g_wMaxScanLines;
		  wBLinePosOdd =
		    (g_wtheReadyLines -
		     g_wLineDistance * 2) % g_wMaxScanLines;
		}

	      for (i = 0; i < g_SWWidth;)
		{
		  if ((i + 1) != g_SWWidth)
		    {
		      byRed =
			*(g_lpReadImageHead + wRLinePosOdd * g_BytesPerRow +
			  i * 3 + 0);
		      bNextPixel =
			*(g_lpReadImageHead + wRLinePosEven * g_BytesPerRow +
			  (i + 1) * 3 + 0);
		      byRed = (byRed + bNextPixel) >> 1;

		      byGreen =
			*(g_lpReadImageHead + wGLinePosOdd * g_BytesPerRow +
			  i * 3 + 1);
		      bNextPixel =
			*(g_lpReadImageHead + wGLinePosEven * g_BytesPerRow +
			  (i + 1) * 3 + 1);
		      byGreen = (byGreen + bNextPixel) >> 1;

		      byBlue =
			*(g_lpReadImageHead + wBLinePosOdd * g_BytesPerRow +
			  i * 3 + 2);
		      bNextPixel =
			*(g_lpReadImageHead + wBLinePosEven * g_BytesPerRow +
			  (i + 1) * 3 + 2);
		      byBlue = (byBlue + bNextPixel) >> 1;

#ifdef ENABLE_GAMMA
		      *(lpLine + i * 3 + 2) =
			(unsigned
			 char) (*(g_pGammaTable +
				  (unsigned short) ((byRed << 4) |
					  QBET4 (byBlue, byGreen))));
		      *(lpLine + i * 3 + 1) =
			(unsigned
			 char) (*(g_pGammaTable + 4096 +
				  (unsigned short) ((byGreen << 4) |
					  QBET4 (byRed, byBlue))));
		      *(lpLine + i * 3 + 0) =
			(unsigned
			 char) (*(g_pGammaTable + 8192 +
				  (unsigned short) ((byBlue << 4) |
					  QBET4 (byGreen, byRed))));
#else
		      *(lpLine + i * 3 + 2) = (unsigned char) byRed;
		      *(lpLine + i * 3 + 1) = (unsigned char) byGreen;
		      *(lpLine + i * 3 + 0) = (unsigned char) byBlue;
#endif
		      i++;
		      if (i >= g_SWWidth)
			{
			  break;
			}

		      byRed =
			*(g_lpReadImageHead + wRLinePosEven * g_BytesPerRow +
			  i * 3 + 0);
		      bNextPixel =
			*(g_lpReadImageHead + wRLinePosOdd * g_BytesPerRow +
			  (i + 1) * 3 + 0);
		      byRed = (byRed + bNextPixel) >> 1;

		      byGreen =
			*(g_lpReadImageHead + wGLinePosEven * g_BytesPerRow +
			  i * 3 + 1);
		      bNextPixel =
			*(g_lpReadImageHead + wGLinePosOdd * g_BytesPerRow +
			  (i + 1) * 3 + 1);
		      byGreen = (byGreen + bNextPixel) >> 1;

		      byBlue =
			*(g_lpReadImageHead + wBLinePosEven * g_BytesPerRow +
			  i * 3 + 2);
		      bNextPixel =
			*(g_lpReadImageHead + wBLinePosOdd * g_BytesPerRow +
			  (i + 1) * 3 + 2);
		      byBlue = (byBlue + bNextPixel) >> 1;
#ifdef ENABLE_GAMMA
		      *(lpLine + i * 3 + 2) =
			(unsigned
			 char) (*(g_pGammaTable +
				  (unsigned short) ((byRed << 4) |
					  QBET4 (byBlue, byGreen))));
		      *(lpLine + i * 3 + 1) =
			(unsigned
			 char) (*(g_pGammaTable + 4096 +
				  (unsigned short) ((byGreen << 4) |
					  QBET4 (byRed, byBlue))));
		      *(lpLine + i * 3 + 0) =
			(unsigned
			 char) (*(g_pGammaTable + 8192 +
				  (unsigned short) ((byBlue << 4) |
					  QBET4 (byGreen, byRed))));
#else
		      *(lpLine + i * 3 + 2) = (unsigned char) byRed;
		      *(lpLine + i * 3 + 1) = (unsigned char) byGreen;
		      *(lpLine + i * 3 + 0) = (unsigned char) byBlue;
#endif
		      i++;
		    }
		}

	      TotalXferLines++;
	      g_dwTotalTotalXferLines++;
	      lpLine += g_SWBytesPerRow;
	      AddReadyLines ();

	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine1200DPI: g_dwTotalTotalXferLines=%d\n",
		   g_dwTotalTotalXferLines);
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine1200DPI: g_Height=%d\n",
		   g_Height);

	    }
	  if (g_isCanceled)
	    {
	      pthread_cancel (g_threadid_readimage);
	      pthread_join (g_threadid_readimage, NULL);
	      DBG (DBG_FUNC,
		   "MustScanner_GetRgb24BitLine1200DPI: thread exit\n");


	      break;
	    }
	}
    }

  *wLinesCount = TotalXferLines;
  g_isScanning = FALSE;

  DBG (DBG_FUNC,
       "MustScanner_GetRgb24BitLine1200DPI: leave MustScanner_GetRgb24BitLine1200DPI\n");
  return TRUE;
}

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Repair line when single CCD and color is 16bit
Parameters:
	lpLine: point to image be repaired
	isOrderInvert: RGB or BGR
	wLinesCount: how many line be repaired
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_GetMono16BitLine (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
			      unsigned short * wLinesCount)
{
  unsigned short wWantedTotalLines;
  unsigned short TotalXferLines;
  unsigned int wTempData;

  unsigned short wLinePos = 0;
  unsigned short i;

  isOrderInvert = isOrderInvert;

  DBG (DBG_FUNC, "MustScanner_GetMono16BitLine: call in\n");

  TotalXferLines = 0;
  g_isCanceled = FALSE;
  g_isScanning = TRUE;
  wWantedTotalLines = *wLinesCount;

  if (g_bFirstReadImage)
    {
      pthread_create (&g_threadid_readimage, NULL,
		      MustScanner_ReadDataFromScanner, NULL);
      DBG (DBG_FUNC, "MustScanner_GetMono16BitLine: thread create\n");
      g_bFirstReadImage = FALSE;
    }

  for (; TotalXferLines < wWantedTotalLines;)
    {

      if (g_dwTotalTotalXferLines >= g_SWHeight)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "MustScanner_GetMono16BitLine: thread exit\n");

	  *wLinesCount = TotalXferLines;
	  g_isScanning = FALSE;
	  return TRUE;
	}

      if (GetScannedLines () > g_wtheReadyLines)
	{
	  wLinePos = g_wtheReadyLines % g_wMaxScanLines;

	  for (i = 0; i < g_SWWidth; i++)
	    {
	      wTempData =
		*(g_lpReadImageHead + wLinePos * g_BytesPerRow + i * 2 + 0);
	      wTempData +=
		*(g_lpReadImageHead + wLinePos * g_BytesPerRow + i * 2 +
		  1) << 8;
	      *(lpLine + i * 2 + 0) = LOBYTE (g_pGammaTable[wTempData]);
	      *(lpLine + i * 2 + 1) = HIBYTE (g_pGammaTable[wTempData]);
	    }

	  TotalXferLines++;
	  g_dwTotalTotalXferLines++;

	  lpLine += g_SWBytesPerRow;
	  AddReadyLines ();
	}
      if (g_isCanceled)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "MustScanner_GetMono16BitLine: thread exit\n");

	  break;
	}
    }

  *wLinesCount = TotalXferLines;
  g_isScanning = FALSE;

  DBG (DBG_FUNC,
       "MustScanner_GetMono16BitLine: leave MustScanner_GetMono16BitLine\n");
  return TRUE;
}

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Repair line when double CCD and color is 16bit
Parameters:
	lpLine: point to image be repaired
	isOrderInvert: RGB or BGR
	wLinesCount: how many line be repaired
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_GetMono16BitLine1200DPI (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
				     unsigned short * wLinesCount)
{
  unsigned short wWantedTotalLines;
  unsigned short TotalXferLines;
  unsigned int dwTempData;
  unsigned short wLinePosOdd = 0;
  unsigned short wLinePosEven = 0;
  unsigned short i;
  SANE_Byte * lpTemp = lpLine;

  isOrderInvert = isOrderInvert;
  DBG (DBG_FUNC, "MustScanner_GetMono16BitLine1200DPI: call in\n");

  TotalXferLines = 0;
  g_isCanceled = FALSE;
  g_isScanning = TRUE;
  wWantedTotalLines = *wLinesCount;

  if (g_bFirstReadImage)
    {
      pthread_create (&g_threadid_readimage, NULL,
		      MustScanner_ReadDataFromScanner, NULL);
      DBG (DBG_FUNC, "MustScanner_GetMono16BitLine1200DPI: thread create\n");
      g_bFirstReadImage = FALSE;
    }

  for (; TotalXferLines < wWantedTotalLines;)
    {
      if (g_dwTotalTotalXferLines >= g_SWHeight)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC,
	       "MustScanner_GetMono16BitLine1200DPI: thread exit\n");

	  *wLinesCount = TotalXferLines;
	  g_isScanning = FALSE;
	  return TRUE;
	}

      if (GetScannedLines () > g_wtheReadyLines)
	{
	  if (ST_Reflective == g_ScanType)
	    {
	      wLinePosOdd =
		(g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
	      wLinePosEven = (g_wtheReadyLines) % g_wMaxScanLines;
	    }
	  else
	    {
	      wLinePosEven =
		(g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
	      wLinePosOdd = (g_wtheReadyLines) % g_wMaxScanLines;
	    }


	  for (i = 0; i < g_SWWidth;)
	    {
	      if ((i + 1) != g_SWWidth)
		{
		  dwTempData =
		    (unsigned int) (*
			     (g_lpReadImageHead +
			      wLinePosOdd * g_BytesPerRow + i * 2 + 0));
		  dwTempData +=
		    (unsigned int) (*
			     (g_lpReadImageHead +
			      wLinePosOdd * g_BytesPerRow + i * 2 + 1) << 8);
		  dwTempData +=
		    (unsigned int) (*
			     (g_lpReadImageHead +
			      wLinePosEven * g_BytesPerRow + (i + 1) * 2 +
			      0));
		  dwTempData +=
		    (unsigned int) (*
			     (g_lpReadImageHead +
			      wLinePosEven * g_BytesPerRow + (i + 1) * 2 +
			      1) << 8);
		  dwTempData = g_pGammaTable[dwTempData >> 1];
		  *(lpLine + i * 2 + 0) = LOBYTE ((unsigned short) dwTempData);
		  *(lpLine + i * 2 + 1) = HIBYTE ((unsigned short) dwTempData);
		  i++;
		  if (i >= g_SWWidth)
		    {
		      break;
		    }

		  dwTempData =
		    (unsigned int) (*
			     (g_lpReadImageHead +
			      wLinePosEven * g_BytesPerRow + i * 2 + 0));
		  dwTempData +=
		    (unsigned int) (*
			     (g_lpReadImageHead +
			      wLinePosEven * g_BytesPerRow + i * 2 + 1) << 8);
		  dwTempData +=
		    (unsigned int) (*
			     (g_lpReadImageHead +
			      wLinePosOdd * g_BytesPerRow + (i + 1) * 2 + 0));
		  dwTempData +=
		    (unsigned int) (*
			     (g_lpReadImageHead +
			      wLinePosOdd * g_BytesPerRow + (i + 1) * 2 +
			      1) << 8);
		  dwTempData = g_pGammaTable[dwTempData >> 1];
		  *(lpLine + i * 2 + 0) = LOBYTE ((unsigned short) dwTempData);
		  *(lpLine + i * 2 + 1) = HIBYTE ((unsigned short) dwTempData);
		  i++;
		}
	    }

	  TotalXferLines++;
	  g_dwTotalTotalXferLines++;
	  lpLine += g_SWBytesPerRow;
	  AddReadyLines ();
	}
      if (g_isCanceled)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC,
	       "MustScanner_GetMono16BitLine1200DPI: thread exit\n");

	  break;
	}
    }

  *wLinesCount = TotalXferLines;
  g_isScanning = FALSE;

  /*for modify the last point */
  if (g_bIsFirstReadBefData)
    {
      g_lpBefLineImageData = (SANE_Byte *) malloc (g_SWBytesPerRow);
      if (NULL == g_lpBefLineImageData)
	{
	  return FALSE;
	}
      memset (g_lpBefLineImageData, 0, g_SWBytesPerRow);
      memcpy (g_lpBefLineImageData, lpTemp, g_SWBytesPerRow);
      g_bIsFirstReadBefData = FALSE;
    }

  ModifyLinePoint (lpTemp, g_lpBefLineImageData, g_SWBytesPerRow,
		   wWantedTotalLines, 2, 4);

  memcpy (g_lpBefLineImageData,
	  lpTemp + (wWantedTotalLines - 1) * g_SWBytesPerRow,
	  g_SWBytesPerRow);
  g_dwAlreadyGetLines += wWantedTotalLines;
  if (g_dwAlreadyGetLines >= g_SWHeight)
    {
      DBG (DBG_FUNC,
	   "MustScanner_GetMono16BitLine1200DPI: free before line data!\n");
      free (g_lpBefLineImageData);
      g_lpBefLineImageData = NULL;
      g_dwAlreadyGetLines = 0;
      g_bIsFirstReadBefData = TRUE;
    }

  DBG (DBG_FUNC,
       "MustScanner_GetMono16BitLine1200DPI: leave MustScanner_GetMono16BitLine1200DPI\n");
  return TRUE;
}

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Repair line when single CCD and color is 8bit
Parameters:
	lpLine: point to image be repaired
	isOrderInvert: RGB or BGR
	wLinesCount: how many line be repaired
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_GetMono8BitLine (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
			     unsigned short * wLinesCount)
{
  unsigned short wWantedTotalLines;
  unsigned short TotalXferLines;

  unsigned short i;
  unsigned short wLinePos = 0;

  isOrderInvert = isOrderInvert;
  DBG (DBG_FUNC, "MustScanner_GetMono8BitLine: call in\n");

  TotalXferLines = 0;
  g_isCanceled = FALSE;
  g_isScanning = TRUE;
  wWantedTotalLines = *wLinesCount;

  if (g_bFirstReadImage)
    {
      pthread_create (&g_threadid_readimage, NULL,
		      MustScanner_ReadDataFromScanner, NULL);
      DBG (DBG_FUNC, "MustScanner_GetMono8BitLine: thread create\n");
      g_bFirstReadImage = FALSE;
    }

  for (; TotalXferLines < wWantedTotalLines;)
    {
      if (g_dwTotalTotalXferLines >= g_SWHeight)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "MustScanner_GetMono8BitLine: thread exit\n");

	  *wLinesCount = TotalXferLines;
	  g_isScanning = FALSE;
	  return TRUE;
	}

      if (GetScannedLines () > g_wtheReadyLines)
	{
	  wLinePos = g_wtheReadyLines % g_wMaxScanLines;

	  for (i = 0; i < g_SWWidth; i++)
	    {
	      *(lpLine + i) =
		(SANE_Byte) * (g_pGammaTable +
			  (unsigned short) ((*
				   (g_lpReadImageHead +
				    wLinePos * g_BytesPerRow +
				    i) << 4) | (rand () & 0x0f)));
	    }

	  TotalXferLines++;
	  g_dwTotalTotalXferLines++;
	  lpLine += g_SWBytesPerRow;
	  AddReadyLines ();

	}
      if (g_isCanceled)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "MustScanner_GetMono8BitLine: thread exit\n");

	  break;
	}
    }

  *wLinesCount = TotalXferLines;
  g_isScanning = FALSE;

  DBG (DBG_FUNC,
       "MustScanner_GetMono8BitLine: leave MustScanner_GetMono8BitLine\n");
  return TRUE;
}

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Repair line when double CCD and color is 8bit
Parameters:
	lpLine: point to image be repaired
	isOrderInvert: RGB or BGR
	wLinesCount: how many line be repaired
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_GetMono8BitLine1200DPI (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
				    unsigned short * wLinesCount)
{
  SANE_Byte *lpTemp;
  unsigned short wWantedTotalLines;
  unsigned short TotalXferLines;

  unsigned short wLinePosOdd = 0;
  unsigned short wLinePosEven = 0;
  SANE_Byte byGray;
  unsigned short i;
  SANE_Byte bNextPixel = 0;

  isOrderInvert = isOrderInvert;
  DBG (DBG_FUNC, "MustScanner_GetMono8BitLine1200DPI: call in\n");

  TotalXferLines = 0;
  g_isCanceled = FALSE;
  g_isScanning = TRUE;
  wWantedTotalLines = *wLinesCount;
  lpTemp = lpLine;

  if (g_bFirstReadImage)
    {
      pthread_create (&g_threadid_readimage, NULL,
		      MustScanner_ReadDataFromScanner, NULL);
      DBG (DBG_FUNC, "MustScanner_GetMono8BitLine1200DPI: thread create\n");
      g_bFirstReadImage = FALSE;
    }

  for (; TotalXferLines < wWantedTotalLines;)
    {
      if (g_dwTotalTotalXferLines >= g_SWHeight)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "MustScanner_GetMono8BitLine1200DPI: thread exit\n");

	  *wLinesCount = TotalXferLines;
	  g_isScanning = FALSE;
	  return TRUE;
	}

      if (GetScannedLines () > g_wtheReadyLines)
	{
	  if (ST_Reflective == g_ScanType)

	    {
	      wLinePosOdd =
		(g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
	      wLinePosEven = (g_wtheReadyLines) % g_wMaxScanLines;
	    }
	  else
	    {
	      wLinePosEven =
		(g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
	      wLinePosOdd = (g_wtheReadyLines) % g_wMaxScanLines;
	    }


	  for (i = 0; i < g_SWWidth;)
	    {
	      if ((i + 1) != g_SWWidth)
		{
		  byGray =
		    *(g_lpReadImageHead + wLinePosOdd * g_BytesPerRow + i);
		  bNextPixel =
		    *(g_lpReadImageHead + wLinePosEven * g_BytesPerRow +
		      (i + 1));
		  byGray = (byGray + bNextPixel) >> 1;

		  *(lpLine + i) =
		    (SANE_Byte) * (g_pGammaTable +
			      (byGray << 4 | (rand () & 0x0f)));
		  i++;
		  if (i >= g_SWWidth)
		    {
		      break;
		    }

		  byGray =
		    *(g_lpReadImageHead + wLinePosEven * g_BytesPerRow + i);
		  bNextPixel =
		    *(g_lpReadImageHead + wLinePosOdd * g_BytesPerRow +
		      (i + 1));
		  byGray = (byGray + bNextPixel) >> 1;

		  *(lpLine + i) =
		    (SANE_Byte) * (g_pGammaTable +
			      (byGray << 4 | (rand () & 0x0f)));
		  i++;
		}
	    }

	  TotalXferLines++;
	  g_dwTotalTotalXferLines++;
	  lpLine += g_SWBytesPerRow;
	  AddReadyLines ();
	}
      if (g_isCanceled)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "MustScanner_GetMono8BitLine1200DPI: thread exit\n");

	  break;
	}
    }


  *wLinesCount = TotalXferLines;
  g_isScanning = FALSE;

  /*for modify the last point */
  if (g_bIsFirstReadBefData)
    {
      g_lpBefLineImageData = (SANE_Byte *) malloc (g_SWBytesPerRow);
      if (NULL == g_lpBefLineImageData)
	{
	  return FALSE;
	}
      memset (g_lpBefLineImageData, 0, g_SWBytesPerRow);
      memcpy (g_lpBefLineImageData, lpTemp, g_SWBytesPerRow);
      g_bIsFirstReadBefData = FALSE;
    }

  ModifyLinePoint (lpTemp, g_lpBefLineImageData, g_SWBytesPerRow,
		   wWantedTotalLines, 1, 4);

  memcpy (g_lpBefLineImageData,
	  lpTemp + (wWantedTotalLines - 1) * g_SWBytesPerRow,
	  g_SWBytesPerRow);
  g_dwAlreadyGetLines += wWantedTotalLines;
  if (g_dwAlreadyGetLines >= g_SWHeight)
    {
      DBG (DBG_FUNC,
	   "MustScanner_GetMono8BitLine1200DPI: free the before line data!\n");
      free (g_lpBefLineImageData);
      g_lpBefLineImageData = NULL;
      g_dwAlreadyGetLines = 0;
      g_bIsFirstReadBefData = TRUE;
    }

  DBG (DBG_FUNC,
       "MustScanner_GetMono8BitLine1200DPI: leave MustScanner_GetMono8BitLine1200DPI\n");
  return TRUE;
}

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Repair line when single CCD and color is 1bit
Parameters:
	lpLine: point to image be repaired
	isOrderInvert: RGB or BGR
	wLinesCount: how many line be repaired
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_GetMono1BitLine (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
			     unsigned short * wLinesCount)
{
  unsigned short wWantedTotalLines;
  unsigned short TotalXferLines;
  unsigned short wLinePos;
  unsigned short i;

  isOrderInvert = isOrderInvert;

  DBG (DBG_FUNC, "MustScanner_GetMono1BitLine: call in\n");

  g_isCanceled = FALSE;
  g_isScanning = TRUE;
  wWantedTotalLines = *wLinesCount;

  if (g_bFirstReadImage)
    {
      pthread_create (&g_threadid_readimage, NULL,
		      MustScanner_ReadDataFromScanner, NULL);
      DBG (DBG_FUNC, "MustScanner_GetMono1BitLine: thread create\n");
      g_bFirstReadImage = FALSE;
    }

  memset (lpLine, 0, wWantedTotalLines * g_SWWidth / 8);

  for (TotalXferLines = 0; TotalXferLines < wWantedTotalLines;)

    {
      if (g_dwTotalTotalXferLines >= g_SWHeight)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "MustScanner_GetMono1BitLine: thread exit\n");

	  *wLinesCount = TotalXferLines;
	  g_isScanning = FALSE;
	  return TRUE;
	}

      if (GetScannedLines () > g_wtheReadyLines)
	{
	  wLinePos = g_wtheReadyLines % g_wMaxScanLines;

	  for (i = 0; i < g_SWWidth; i++)
	    {
	      if (*(g_lpReadImageHead + wLinePos * g_BytesPerRow + i) >
		  g_wLineartThreshold)
		{
		  *(lpLine + i / 8) += (0x80 >> (i % 8));
		}
	    }

	  TotalXferLines++;
	  g_dwTotalTotalXferLines++;
	  lpLine += (g_SWBytesPerRow / 8);
	  AddReadyLines ();
	}
      if (g_isCanceled)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "MustScanner_GetMono1BitLine: thread exit\n");

	  break;
	}
    }

  *wLinesCount = TotalXferLines;
  g_isScanning = FALSE;

  DBG (DBG_FUNC,
       "MustScanner_GetMono1BitLine: leave MustScanner_GetMono1BitLine\n");
  return TRUE;
}

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Repair line when double CCD and color is 1bit
Parameters:
	lpLine: point to image be repaired
	isOrderInvert: RGB or BGR
	wLinesCount: how many line be repaired
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
MustScanner_GetMono1BitLine1200DPI (SANE_Byte * lpLine, SANE_Bool isOrderInvert,
				    unsigned short * wLinesCount)
{
  unsigned short wWantedTotalLines;
  unsigned short TotalXferLines;
  unsigned short i;
  unsigned short wLinePosOdd;
  unsigned short wLinePosEven;

  isOrderInvert = isOrderInvert;

  DBG (DBG_FUNC, "MustScanner_GetMono1BitLine1200DPI: call in\n");

  g_isCanceled = FALSE;
  g_isScanning = TRUE;
  wWantedTotalLines = *wLinesCount;

  if (g_bFirstReadImage)
    {
      pthread_create (&g_threadid_readimage, NULL,
		      MustScanner_ReadDataFromScanner, NULL);
      DBG (DBG_FUNC, "MustScanner_GetMono1BitLine1200DPI: thread create\n");
      g_bFirstReadImage = FALSE;
    }

  memset (lpLine, 0, wWantedTotalLines * g_SWWidth / 8);

  for (TotalXferLines = 0; TotalXferLines < wWantedTotalLines;)
    {
      if (g_dwTotalTotalXferLines >= g_SWHeight)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "MustScanner_GetMono1BitLine1200DPI: thread exit\n");

	  *wLinesCount = TotalXferLines;
	  g_isScanning = FALSE;
	  return TRUE;
	}

      if (GetScannedLines () > g_wtheReadyLines)
	{
	  if (ST_Reflective == g_ScanType)
	    {
	      wLinePosEven = (g_wtheReadyLines) % g_wMaxScanLines;
	      wLinePosOdd =
		(g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
	    }
	  else
	    {
	      wLinePosOdd = (g_wtheReadyLines) % g_wMaxScanLines;
	      wLinePosEven =
		(g_wtheReadyLines - g_wPixelDistance) % g_wMaxScanLines;
	    }



	  for (i = 0; i < g_SWWidth;)
	    {
	      if ((i + 1) != g_SWWidth)
		{
		  if (*(g_lpReadImageHead + wLinePosOdd * g_BytesPerRow + i) >
		      g_wLineartThreshold)
		    *(lpLine + i / 8) += (0x80 >> (i % 8));
		  i++;
		  if (i >= g_SWWidth)
		    {
		      break;
		    }

		  if (*(g_lpReadImageHead + wLinePosEven * g_BytesPerRow + i)
		      > g_wLineartThreshold)
		    *(lpLine + i / 8) += (0x80 >> (i % 8));
		  i++;
		}
	    }

	  TotalXferLines++;
	  g_dwTotalTotalXferLines++;
	  lpLine += g_SWBytesPerRow / 8;
	  AddReadyLines ();


	}
      if (g_isCanceled)
	{
	  pthread_cancel (g_threadid_readimage);
	  pthread_join (g_threadid_readimage, NULL);
	  DBG (DBG_FUNC, "MustScanner_GetMono1BitLine1200DPI: thread exit\n");

	  break;
	}
    }				/*end for */

  *wLinesCount = TotalXferLines;
  g_isScanning = FALSE;

  DBG (DBG_FUNC,
       "MustScanner_GetMono1BitLine1200DPI: leave MustScanner_GetMono1BitLine1200DPI\n");
  return TRUE;
}

/**********************************************************************
Author: Jack            Date: 2005/05/21
Routine Description: 
	prepare calculate Max and Min value	
Parameters:
	wResolution: the scan resolution
Return value:
	none 
***********************************************************************/
static void
MustScanner_PrepareCalculateMaxMin (unsigned short wResolution)
{
  g_wDarkCalWidth = 52;
  if (wResolution <= 600)
    {
      g_wCalWidth = ((5120 * wResolution / 600 + 511) >> 9) << 9;
      g_wDarkCalWidth = g_wDarkCalWidth / (1200 / wResolution);

      if (wResolution < 200)
	{
	  g_nPowerNum = 3;
	  g_nSecLength = 8;	/* 2^nPowerNum */
	  g_nDarkSecLength = g_wDarkCalWidth / 2;	/* Dark has at least 2 sections */
	}
      else
	{
	  g_nPowerNum = 6;
	  g_nSecLength = 64;	/* 2^nPowerNum */
	  g_nDarkSecLength = g_wDarkCalWidth / 3;
	}
    }
  else
    {
      g_nPowerNum = 6;
      g_nSecLength = 64;	/*2^nPowerNum */
      g_wCalWidth = 10240;
      g_nDarkSecLength = g_wDarkCalWidth / 5;
    }

  if (g_nDarkSecLength <= 0)
    {
      g_nDarkSecLength = 1;
    }

  g_wStartPosition = 13 * wResolution / 1200;
  g_wCalWidth -= g_wStartPosition;


  /* start of find Max value */
  g_nSecNum = (int) (g_wCalWidth / g_nSecLength);

  /* start of fin min value */
  g_nDarkSecNum = (int) (g_wDarkCalWidth / g_nDarkSecLength);
}

/**********************************************************************
Author: Jack            Date: 2005/05/21
Routine Description: 
	calculate the Max and Min value
Parameters:
	pBuffer: the image data
	lpMaxValue: the max value
	lpMinValue: the min value
	wResolution: the scan resolution
Return value:
	none 
***********************************************************************/
static void
MustScanner_CalculateMaxMin (SANE_Byte * pBuffer, unsigned short * lpMaxValue,
			     unsigned short * lpMinValue, unsigned short wResolution)
{
  unsigned short *wSecData = NULL, *wDarkSecData = NULL;
  int i, j;

  wResolution = wResolution;

  wSecData = (unsigned short *) malloc (sizeof (unsigned short) * g_nSecNum);
  if (wSecData == NULL)
    {
      return;
    }
  else
    {
      memset (wSecData, 0, g_nSecNum * sizeof (unsigned short));
    }

  for (i = 0; i < g_nSecNum; i++)
    {

      for (j = 0; j < g_nSecLength; j++)
	wSecData[i] += *(pBuffer + g_wStartPosition + i * g_nSecLength + j);
      wSecData[i] >>= g_nPowerNum;
    }

  *lpMaxValue = wSecData[0];
  for (i = 0; i < g_nSecNum; i++)
    {
      if (*lpMaxValue < wSecData[i])
	*lpMaxValue = wSecData[i];
    }

  free (wSecData);

  wDarkSecData = (unsigned short *) malloc (sizeof (unsigned short) * g_nDarkSecNum);
  if (wDarkSecData == NULL)
    {
      return;
    }
  else
    {
      memset (wDarkSecData, 0, g_nDarkSecNum * sizeof (unsigned short));
    }

  for (i = 0; i < g_nDarkSecNum; i++)
    {
      for (j = 0; j < g_nDarkSecLength; j++)
	wDarkSecData[i] +=
	  *(pBuffer + g_wStartPosition + i * g_nDarkSecLength + j);

      wDarkSecData[i] /= g_nDarkSecLength;
    }

  *lpMinValue = wDarkSecData[0];
  for (i = 0; i < g_nDarkSecNum; i++)
    {
      if (*lpMinValue > wDarkSecData[i])
	*lpMinValue = wDarkSecData[i];
    }
  free (wDarkSecData);
}


/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Read the data from scanner
Parameters:
	none
Return value: 
	if operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static void *
MustScanner_ReadDataFromScanner (void * dummy)
{
  unsigned short wTotalReadImageLines = 0;
  unsigned short wWantedLines = g_Height;
  SANE_Byte * lpReadImage = g_lpReadImageHead;
  SANE_Bool isWaitImageLineDiff = FALSE;
  unsigned int wMaxScanLines = g_wMaxScanLines;
  unsigned short wReadImageLines = 0;
  unsigned short wScanLinesThisBlock;
  unsigned short wBufferLines = g_wLineDistance * 2 + g_wPixelDistance;

  dummy = dummy;
  DBG (DBG_FUNC,
       "MustScanner_ReadDataFromScanner: call in, and in new thread\n");

  while (wTotalReadImageLines < wWantedLines && g_lpReadImageHead)
    {
      if (!isWaitImageLineDiff)
	{
	  wScanLinesThisBlock =
	    (wWantedLines - wTotalReadImageLines) <
	    g_wScanLinesPerBlock ? (wWantedLines -
				    wTotalReadImageLines) :
	    g_wScanLinesPerBlock;

	  DBG (DBG_FUNC,
	       "MustScanner_ReadDataFromScanner: wWantedLines=%d\n",
	       wWantedLines);

	  DBG (DBG_FUNC,
	       "MustScanner_ReadDataFromScanner: wScanLinesThisBlock=%d\n",
	       wScanLinesThisBlock);

	  if (STATUS_GOOD !=
	      Asic_ReadImage (&g_chip, lpReadImage, wScanLinesThisBlock))
	    {
	      DBG (DBG_FUNC,
		   "MustScanner_ReadDataFromScanner:Asic_ReadImage return error\n");
	      DBG (DBG_FUNC, "MustScanner_ReadDataFromScanner:thread exit\n");
	      return NULL;
	    }

	  /*has read in memroy Buffer */
	  wReadImageLines += wScanLinesThisBlock;

	  AddScannedLines (wScanLinesThisBlock);

	  wTotalReadImageLines += wScanLinesThisBlock;

	  lpReadImage += wScanLinesThisBlock * g_BytesPerRow;

	  /*Buffer is full */
	  if (wReadImageLines >= wMaxScanLines)
	    {
	      lpReadImage = g_lpReadImageHead;
	      wReadImageLines = 0;
	    }

	  if ((g_dwScannedTotalLines - GetReadyLines ())
	      >= (wMaxScanLines - (wBufferLines + g_wScanLinesPerBlock))
	      && g_dwScannedTotalLines > GetReadyLines ())
	    {
	      isWaitImageLineDiff = TRUE;
	    }
	}
      else if (g_dwScannedTotalLines <=
	       GetReadyLines () + wBufferLines + g_wScanLinesPerBlock)
	{
	  isWaitImageLineDiff = FALSE;
	}

      pthread_testcancel ();
    }

  DBG (DBG_FUNC, "MustScanner_ReadDataFromScanner: Read image ok\n");
  DBG (DBG_FUNC, "MustScanner_ReadDataFromScanner: thread exit\n");
  DBG (DBG_FUNC,
       "MustScanner_ReadDataFromScanner: leave MustScanner_ReadDataFromScanner\n");
  return NULL;
}

/**********************************************************************
Author: Jack            Date: 2005/05/26
Routine Description: 
	get the lines of scanned
Parameters:
	none
Return value: 
	the lines of scanned
***********************************************************************/
static unsigned int
GetScannedLines ()
{
  unsigned int dwScannedLines = 0;

  pthread_mutex_lock (&g_scannedLinesMutex);
  dwScannedLines = g_dwScannedTotalLines;
  pthread_mutex_unlock (&g_scannedLinesMutex);

  return dwScannedLines;
}

/**********************************************************************
Author: Jack            Date: 2005/05/26

Routine Description: 
	get lines which pass to superstratum
Parameters:
	none
Return value:
	the lines which pass to superstratum
***********************************************************************/
static unsigned int
GetReadyLines ()
{
  unsigned int dwReadyLines = 0;

  pthread_mutex_lock (&g_readyLinesMutex);
  dwReadyLines = g_wtheReadyLines;
  pthread_mutex_unlock (&g_readyLinesMutex);

  return dwReadyLines;
}

/**********************************************************************
Author: Jack            Date: 2005/05/26
Routine Description: 
	add the scanned total lines
Parameters:
	wAddLines: add the lines
Return value: 
	none
***********************************************************************/
static void
AddScannedLines (unsigned short wAddLines)
{
  pthread_mutex_lock (&g_scannedLinesMutex);

  g_dwScannedTotalLines += wAddLines;

  pthread_mutex_unlock (&g_scannedLinesMutex);
}

/**********************************************************************
Author: Jack            Date: 2005/05/26
Routine Description: 
	add the ready lines
Parameters:
	none
Return value: 
	none
***********************************************************************/
static void
AddReadyLines ()
{
  pthread_mutex_lock (&g_readyLinesMutex);
  g_wtheReadyLines++;
  pthread_mutex_unlock (&g_readyLinesMutex);
}

/**********************************************************************
Author: Jack            Date: 2005/05/26
Routine Description:
	modify the point
Parameters:
	lpImageData: the data of image
  lpImageDataBefore: the data of before line image
  dwBytesPerLine: the bytes of per line
  dwLinesCount: the line count
  wPixDistance: the pixel distance
  wModPtCount: the modify point count
Return value:
	none
***********************************************************************/
static void
ModifyLinePoint (SANE_Byte * lpImageData,
		 SANE_Byte * lpImageDataBefore,
		 unsigned int dwBytesPerLine,
		 unsigned int dwLinesCount, unsigned short wPixDistance, unsigned short wModPtCount)
{
  unsigned short i = 0;
  unsigned short j = 0;
  unsigned short wLines = 0;
  unsigned int dwWidth = dwBytesPerLine / wPixDistance;
  for (i = wModPtCount; i > 0; i--)
    {
      for (j = 0; j < wPixDistance; j++)
	{
	  /*modify the first line */
	  *(lpImageData + (dwWidth - i) * wPixDistance + j) =
	    (*(lpImageData + (dwWidth - i - 1) * wPixDistance + j) +
	     *(lpImageDataBefore + (dwWidth - i) * wPixDistance + j)) / 2;
	  /*modify other lines */
	  for (wLines = 1; wLines < dwLinesCount; wLines++)
	    {
	      unsigned int dwBytesBefor = (wLines - 1) * dwBytesPerLine;
	      unsigned int dwBytes = wLines * dwBytesPerLine;
	      *(lpImageData + dwBytes + (dwWidth - i) * wPixDistance + j) =
		(*
		 (lpImageData + dwBytes + (dwWidth - i - 1) * wPixDistance +
		  j) + *(lpImageData + dwBytesBefor + (dwWidth -
						       i) * wPixDistance +
			 j)) / 2;
	    }
	}
    }
}

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Modifiy the image data	
Parameters:
	A: the input the image data
	B: the input the iamge data
Return value:
	the modified data
***********************************************************************/
static SANE_Byte
QBET4 (SANE_Byte A, SANE_Byte B)
{
  SANE_Byte bQBET[16][16] = {
    {0, 0, 0, 0, 1, 1, 2, 2, 4, 4, 5, 5, 8, 8, 9, 9},
    {0, 0, 0, 0, 1, 1, 2, 2, 4, 4, 5, 5, 8, 8, 9, 9},
    {0, 0, 0, 0, 1, 1, 2, 2, 4, 4, 5, 5, 8, 8, 9, 9},
    {0, 0, 0, 0, 1, 1, 2, 2, 4, 4, 5, 5, 8, 8, 9, 9},
    {1, 1, 1, 1, 3, 3, 3, 3, 6, 6, 6, 6, 10, 10, 11, 11},
    {1, 1, 1, 1, 3, 3, 3, 3, 6, 6, 6, 6, 10, 10, 11, 11},
    {2, 2, 2, 2, 3, 3, 3, 3, 7, 7, 7, 7, 10, 10, 11, 11},
    {2, 2, 2, 2, 3, 3, 3, 3, 7, 7, 7, 7, 10, 10, 11, 11},
    {4, 4, 4, 4, 6, 6, 7, 7, 12, 12, 12, 12, 13, 13, 14, 14},
    {4, 4, 4, 4, 6, 6, 7, 7, 12, 12, 12, 12, 13, 13, 14, 14},
    {5, 5, 5, 5, 6, 6, 7, 7, 12, 12, 12, 12, 13, 13, 14, 14},
    {5, 5, 5, 5, 6, 6, 7, 7, 12, 12, 12, 12, 13, 13, 14, 14},
    {8, 8, 8, 8, 10, 10, 10, 10, 13, 13, 13, 13, 15, 15, 15, 15},
    {8, 8, 8, 8, 10, 10, 10, 10, 13, 13, 13, 13, 15, 15, 15, 15},
    {9, 9, 9, 9, 11, 11, 11, 11, 14, 14, 14, 14, 15, 15, 15, 15},
    {9, 9, 9, 9, 11, 11, 11, 11, 14, 14, 14, 14, 15, 15, 15, 15}
  };

  A = A & 0x0f;
  B = B & 0x0f;
  return bQBET[A][B];
}				/* end of the file MustScanner.c */
