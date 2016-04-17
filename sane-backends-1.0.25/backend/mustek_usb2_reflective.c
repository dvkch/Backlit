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

/* forward declarations */

static SANE_Bool Reflective_Reset (void);
static SANE_Bool Reflective_ScanSuggest (PTARGETIMAGE pTarget, PSUGGESTSETTING pSuggest);
static SANE_Bool Reflective_SetupScan (COLORMODE ColorMode, unsigned short XDpi, unsigned short YDpi,
				  SANE_Bool isInvert, unsigned short X, unsigned short Y, unsigned short Width,
				  unsigned short Height);
static SANE_Bool Reflective_StopScan (void);
static SANE_Bool Reflective_GetRows (SANE_Byte * lpBlock, unsigned short * Rows, SANE_Bool isOrderInvert);
static SANE_Bool Reflective_AdjustAD (void);
static SANE_Bool Reflective_FindTopLeft (unsigned short * lpwStartX, unsigned short * lpwStartY);
static SANE_Bool Reflective_LineCalibration16Bits (void);
static SANE_Bool Reflective_PrepareScan (void);

/*function description*/

/**********************************************************************
Author: Jack            Date: 2005/05/13
Routine Description: 
	reset the scanner status
Parameters:
	none
Return value: 
	if operation is success
	return TRUE
	els
	return FALSE
***********************************************************************/
static SANE_Bool
Reflective_Reset ()
{
  DBG (DBG_FUNC, "Reflective_Reset: call in\n");

  if (g_bOpened)
    {
      DBG (DBG_FUNC, "Reflective_Reset: scanner has been opened\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_Open (&g_chip, g_pDeviceFile))
    {
      DBG (DBG_FUNC, "Reflective_Reset: Asic_Open return error\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_Reset (&g_chip))
    {
      DBG (DBG_FUNC, "Reflective_Reset: Asic_Reset return error\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_SetSource (&g_chip, LS_REFLECTIVE))
    {
      DBG (DBG_FUNC, "Reflective_Reset: Asic_SetSource return error\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_TurnLamp (&g_chip, TRUE))
    {
      DBG (DBG_FUNC, "Reflective_Reset: Asic_TurnLamp return error\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_Close (&g_chip))
    {
      DBG (DBG_FUNC, "Reflective_Reset: Asic_Close return error\n");
      return FALSE;
    }

  g_Y = 0;
  g_X = 0;
  g_Width = 0;
  g_SWWidth = 0;
  g_Height = 0;
  g_SWHeight = 0;

  g_wLineartThreshold = 128;
  g_dwTotalTotalXferLines = 0;
  g_bFirstReadImage = TRUE;

  g_pGammaTable = NULL;

  if (NULL != g_pDeviceFile)
    {
      free (g_pDeviceFile);
      g_pDeviceFile = NULL;
    }

  DBG (DBG_FUNC, "Reflective_Reset: exit\n");

  return TRUE;
}

/**********************************************************************
Author: Jack            Date: 2005/05/13
Routine Description: 
	get the suggest parameter of scaning
Parameters:
	pTarget: the information of scaning
	pSuggest: suggest parameter of scaning
Return value: 
	if the operation is success
	return TRUE
	els
	return FALSE
***********************************************************************/
static SANE_Bool
Reflective_ScanSuggest (PTARGETIMAGE pTarget, PSUGGESTSETTING pSuggest)
{
  int i;
  unsigned short wMaxWidth, wMaxHeight;

  DBG (DBG_FUNC, "Reflective_ScanSuggest: call in\n");

  if (NULL == pTarget || NULL == pSuggest)
    {
      DBG (DBG_FUNC, "Reflective_ScanSuggest: parameters error\n");
      return FALSE;
    }

  /*1. Looking up Optical Y Resolution */
  for (i = 0; s_wOpticalYDpi[i] != 0; i++)
    {
      if (s_wOpticalYDpi[i] <= pTarget->wDpi)
	{
	  pSuggest->wYDpi = s_wOpticalYDpi[i];
	  break;
	}
    }
  if (s_wOpticalYDpi[i] == 0)
    {
      i--;
      pSuggest->wYDpi = s_wOpticalYDpi[i];
    }

  /*2. Looking up Optical X Resolution */
  for (i = 0; s_wOpticalXDpi[i] != 0; i++)
    {
      if (s_wOpticalXDpi[i] <= pTarget->wDpi)
	{
	  pSuggest->wXDpi = s_wOpticalXDpi[i];
	  break;
	}
    }
  if (s_wOpticalXDpi[i] == 0)
    {
      i--;
      pSuggest->wXDpi = s_wOpticalXDpi[i];
    }

  DBG (DBG_FUNC, "Reflective_ScanSuggest: pTarget->wDpi = %d\n",
       pTarget->wDpi);
  DBG (DBG_FUNC, "Reflective_ScanSuggest: pSuggest->wXDpi = %d\n",
       pSuggest->wXDpi);







  DBG (DBG_FUNC, "Reflective_ScanSuggest: pSuggest->wYDpi = %d\n",
       pSuggest->wYDpi);

  /*3. suggest scan area */
  pSuggest->wX =
    (unsigned short) (((unsigned int) (pTarget->wX) * (unsigned int) (pSuggest->wXDpi)) /
	    (unsigned int) (pTarget->wDpi));
  pSuggest->wY =
    (unsigned short) (((unsigned int) (pTarget->wY) * (unsigned int) (pSuggest->wYDpi)) /
	    (unsigned int) (pTarget->wDpi));
  pSuggest->wWidth =
    (unsigned short) (((unsigned int) (pTarget->wWidth) * (unsigned int) (pSuggest->wXDpi)) /
	    (unsigned int) (pTarget->wDpi));
  pSuggest->wHeight =
    (unsigned short) (((unsigned int) (pTarget->wHeight) * (unsigned int) (pSuggest->wYDpi)) /
	    (unsigned int) (pTarget->wDpi));

  pSuggest->wWidth = (pSuggest->wWidth / 2) * 2;

  DBG (DBG_FUNC, "Reflective_ScanSuggest: pTarget->wX = %d\n", pTarget->wX);
  DBG (DBG_FUNC, "Reflective_ScanSuggest: pTarget->wY = %d\n", pTarget->wY);
  DBG (DBG_FUNC, "Reflective_ScanSuggest: pTarget->wWidth = %d\n",
       pTarget->wWidth);
  DBG (DBG_FUNC, "Reflective_ScanSuggest: pTarget->wHeight = %d\n",
       pTarget->wHeight);

  DBG (DBG_FUNC, "Reflective_ScanSuggest: pSuggest->wX = %d\n", pSuggest->wX);
  DBG (DBG_FUNC, "Reflective_ScanSuggest: pSuggest->wY = %d\n", pSuggest->wY);
  DBG (DBG_FUNC, "Reflective_ScanSuggest: pSuggest->wWidth = %d\n",
       pSuggest->wWidth);
  DBG (DBG_FUNC, "Reflective_ScanSuggest: pSuggest->wHeight = %d\n",
       pSuggest->wHeight);

  if (pTarget->cmColorMode == CM_TEXT)
    {
      pSuggest->wWidth = ((pSuggest->wWidth + 7) >> 3) << 3;
      if (pSuggest->wWidth < 8)
	pSuggest->wWidth = 8;
    }

  /*4. check width and height */
  wMaxWidth = (MAX_SCANNING_WIDTH * pSuggest->wXDpi) / 300;
  wMaxHeight = (3480 * pSuggest->wYDpi) / 300;	/* 3480 for bumping */

  DBG (DBG_FUNC, "Reflective_ScanSuggest: wMaxWidth = %d\n", wMaxWidth);
  DBG (DBG_FUNC, "Reflective_ScanSuggest: wMaxHeight = %d\n", wMaxHeight);

  if (CM_TEXT == pTarget->cmColorMode)
    {
      wMaxWidth = (wMaxWidth >> 3) << 3;
    }
  if (pSuggest->wWidth > wMaxWidth)
    {
      pSuggest->wWidth = wMaxWidth;
    }


  if (pSuggest->wHeight > wMaxHeight)
    {
      pSuggest->wHeight = wMaxHeight;
    }

  DBG (DBG_FUNC, "Reflective_ScanSuggest: g_Width=%d\n", g_Width);

  g_Width = ((pSuggest->wWidth + 15) >> 4) << 4;	/*Real Scan Width */

  DBG (DBG_FUNC, "Reflective_ScanSuggest: again, g_Width=%d\n", g_Width);

  g_Height = pSuggest->wHeight;

  if (pTarget->isOptimalSpeed)
    {
      switch (pTarget->cmColorMode)
	{
	case CM_RGB48:
	  pSuggest->cmScanMode = CM_RGB48;
	  pSuggest->dwBytesPerRow = (unsigned int) ((pSuggest->wWidth) * 6);
	  break;
	case CM_RGB24:
	  pSuggest->cmScanMode = CM_RGB24ext;
	  pSuggest->dwBytesPerRow = (unsigned int) ((pSuggest->wWidth) * 3);
	  break;
	case CM_GRAY16:
	  pSuggest->cmScanMode = CM_GRAY16ext;
	  pSuggest->dwBytesPerRow = (unsigned int) ((pSuggest->wWidth) * 2);
	  break;
	case CM_GRAY8:
	  pSuggest->cmScanMode = CM_GRAY8ext;
	  pSuggest->dwBytesPerRow = (unsigned int) ((pSuggest->wWidth));
	  break;
	case CM_TEXT:
	  pSuggest->cmScanMode = CM_TEXT;
	  pSuggest->dwBytesPerRow = (unsigned int) (pSuggest->wWidth) / 8;
	  break;
	default:
	  break;
	}
    }
  else
    {
      switch (pTarget->cmColorMode)
	{
	case CM_RGB48:
	  pSuggest->cmScanMode = CM_RGB48;
	  pSuggest->dwBytesPerRow = (unsigned int) ((pSuggest->wWidth) * 6);
	  break;
	case CM_RGB24:
	  pSuggest->cmScanMode = CM_RGB24ext;
	  pSuggest->dwBytesPerRow = (unsigned int) ((pSuggest->wWidth) * 3);
	  break;
	case CM_GRAY16:
	  pSuggest->cmScanMode = CM_GRAY16ext;
	  pSuggest->dwBytesPerRow = (unsigned int) ((pSuggest->wWidth) * 2);
	  break;
	case CM_GRAY8:
	  pSuggest->cmScanMode = CM_GRAY8ext;
	  pSuggest->dwBytesPerRow = (unsigned int) ((pSuggest->wWidth));
	  break;
	case CM_TEXT:
	  pSuggest->cmScanMode = CM_TEXT;
	  pSuggest->dwBytesPerRow = (unsigned int) (pSuggest->wWidth) / 8;
	  break;
	default:
	  break;
	}
    }

  DBG (DBG_FUNC, "Reflective_ScanSuggest: pSuggest->dwBytesPerRow = %d\n",
       pSuggest->dwBytesPerRow);
  DBG (DBG_FUNC, "Reflective_ScanSuggest: leave Reflective_ScanSuggest\n");
  return TRUE;
}

/**********************************************************************
Author: Jack            Date: 2005/05/13
Routine Description: 
	setup scanning process
Parameters:
	ColorMode: ScanMode of Scanning, CM_RGB48, CM_GRAY and so on
	XDpi: X Resolution
	YDpi: Y Resolution
	isInvert: the RGB order
	X: X start coordinate
	Y: Y start coordinate
	Width: Width of Scan Image
	Height: Height of Scan Image
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
Reflective_SetupScan (COLORMODE ColorMode,
		      unsigned short XDpi,
		      unsigned short YDpi,
		      SANE_Bool isInvert, unsigned short X, unsigned short Y, unsigned short Width, unsigned short Height)
{
  isInvert = isInvert;
  DBG (DBG_FUNC, "Reflective_SetupScan: Call in\n");
  if (g_bOpened)
    {
      DBG (DBG_FUNC, "Reflective_SetupScan: scanner has been opened\n");
      return FALSE;
    }
  if (!g_bPrepared)
    {
      DBG (DBG_FUNC, "Reflective_SetupScan: scanner not prepared\n");
      return FALSE;
    }

  g_ScanMode = ColorMode;
  g_XDpi = XDpi;
  g_YDpi = YDpi;
  g_SWWidth = Width;
  g_SWHeight = Height;

  switch (g_YDpi)
    {
    case 1200:
      g_wPixelDistance = 4;	/*even & odd sensor problem */
      g_wLineDistance = 24;
      g_Height += g_wPixelDistance;
      break;
    case 600:
      g_wPixelDistance = 0;	/*no even & odd problem */
      g_wLineDistance = 12;
      break;
    case 300:
      g_wPixelDistance = 0;
      g_wLineDistance = 6;
      break;
    case 150:
      g_wPixelDistance = 0;
      g_wLineDistance = 3;
      break;

    case 75:
    case 50:
      g_wPixelDistance = 0;
      g_wLineDistance = 1;
      break;
    default:
      g_wLineDistance = 0;
    }

  switch (g_ScanMode)
    {
    case CM_RGB48:
      g_BytesPerRow = 6 * g_Width;
      g_SWBytesPerRow = 6 * g_SWWidth;
      g_bScanBits = 48;
      g_Height += g_wLineDistance * 2;	/*add height to do line distance */
      break;
    case CM_RGB24ext:
      g_BytesPerRow = 3 * g_Width;
      g_SWBytesPerRow = 3 * g_SWWidth;
      g_bScanBits = 24;
      g_Height += g_wLineDistance * 2;	/*add height to do line distance */
      break;
    case CM_GRAY16ext:
      g_BytesPerRow = 2 * g_Width;
      g_SWBytesPerRow = 2 * g_SWWidth;
      g_bScanBits = 16;
      break;
    case CM_GRAY8ext:
    case CM_TEXT:
      g_BytesPerRow = g_Width;
      g_SWBytesPerRow = g_SWWidth;
      g_bScanBits = 8;
      break;
    default:
      break;
    }

  if (Asic_Open (&g_chip, g_pDeviceFile) != STATUS_GOOD)
    {
      DBG (DBG_FUNC, "Reflective_SetupScan: Asic_Open return error\n");
      return FALSE;
    }

  DBG (DBG_FUNC, "Reflective_SetupScan: Asic_Open successfully\n");

  g_bOpened = TRUE;

  Asic_TurnLamp (&g_chip, FALSE);
  Asic_TurnTA (&g_chip, FALSE);
  Asic_TurnLamp (&g_chip, TRUE);

  if (1200 == g_XDpi)
    {
      g_XDpi = 600;

      if (Reflective_AdjustAD () == FALSE)
	{

	  DBG (DBG_FUNC,
	       "Reflective_SetupScan: Reflective_AdjustAD return error\n");
	  return FALSE;
	}
      DBG (DBG_FUNC,
	   "Reflective_SetupScan: Reflective_AdjustAD successfully\n");

      if (Reflective_FindTopLeft (&g_X, &g_Y) == FALSE)
	{
	  g_X = 187;
	  g_Y = 43;
	}

      g_XDpi = 1200;

      if (Reflective_AdjustAD () == FALSE)
	{
	  DBG (DBG_FUNC,
	       "Reflective_SetupScan: Reflective_AdjustAD return error\n");
	  return FALSE;
	}
      DBG (DBG_FUNC,
	   "Reflective_SetupScan: Reflective_AdjustAD successfully\n");
    }
  else
    {
      if (Reflective_AdjustAD () == FALSE)
	{
	  DBG (DBG_FUNC,
	       "Reflective_SetupScan: Reflective_AdjustAD return error\n");
	  return FALSE;
	}
      DBG (DBG_FUNC,
	   "Reflective_SetupScan: Reflective_AdjustAD successfully\n");
      if (Reflective_FindTopLeft (&g_X, &g_Y) == FALSE)
	{
	  g_X = 187;
	  g_Y = 43;
	}
    }


  DBG (DBG_FUNC, "after find top left,g_X=%d,g_Y=%d\n", g_X, g_Y);

  if (1200 == g_XDpi)
    {
      g_X =
	g_X * 1200 / FIND_LEFT_TOP_CALIBRATE_RESOLUTION + X * 1200 / g_XDpi +
	47;

    }
  else
    {
      if (75 == g_XDpi)
	{
	  g_X = g_X + X * 600 / g_XDpi;
	}
      else
	{
	  g_X = g_X + X * 600 / g_XDpi + 23;
	}
    }

  g_Y =
    g_Y * 1200 / FIND_LEFT_TOP_CALIBRATE_RESOLUTION + Y * 1200 / g_YDpi + 47;


  DBG (DBG_FUNC, "before line calibration,g_X=%d,g_Y=%d\n", g_X, g_Y);

  if (Reflective_LineCalibration16Bits () == FALSE)
    {
      DBG (DBG_FUNC,
	   "Reflective_SetupScan: Reflective_LineCalibration16Bits return error\n");
      return FALSE;
    }

  DBG (DBG_FUNC,
       "Reflective_SetupScan: after Reflective_LineCalibration16Bits,g_X=%d,g_Y=%d\n",
       g_X, g_Y);

  DBG (DBG_FUNC, "Reflective_SetupScan: before Asic_SetWindow\n");

  DBG (DBG_FUNC,
       "Reflective_SetupScan: g_bScanBits=%d, g_XDpi=%d, g_YDpi=%d, g_X=%d, g_Y=%d, g_Width=%d, g_Height=%d\n",
       g_bScanBits, g_XDpi, g_YDpi, g_X, g_Y, g_Width, g_Height);

  if (g_Y > 300)
    {
      Asic_MotorMove (&g_chip, TRUE, g_Y - 300);
    }
  else
    {
      Asic_MotorMove (&g_chip, FALSE, 300 - g_Y);
    }
  g_Y = 300;

  Asic_SetWindow (&g_chip, g_bScanBits, g_XDpi, g_YDpi, g_X, g_Y, g_Width,
		  g_Height);

  DBG (DBG_FUNC, "Reflective_SetupScan: leave Reflective_SetupScan\n");
  return Reflective_PrepareScan ();
}

/**********************************************************************
Author: Jack             Date: 2005/05/13
Routine Description: 
	To adjust the value of offset gain of R/G/B
Parameters:
	none
Return value: 
	if operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
Reflective_AdjustAD ()
{
  SANE_Byte * lpCalData;
  unsigned short wCalWidth;
  int nTimesOfCal;
  unsigned short wMaxValueR, wMinValueR, wMaxValueG, wMinValueG, wMaxValueB, wMinValueB;
#if 0
  SANE_Byte bDarkMaxLevel;
  SANE_Byte bDarkMinLevel;
  SANE_Byte bLastMinR, bLastROffset, bROffsetUpperBound = 255, bROffsetLowerBound =
    0;
  SANE_Byte bLastMinG, bLastGOffset, bGOffsetUpperBound = 255, bGOffsetLowerBound =
    0;
  SANE_Byte bLastMinB, bLastBOffset, bBOffsetUpperBound = 255, bBOffsetLowerBound =
    0;
  float fRFactor = 1.0;
  float fGFactor = 1.0;
  float fBFactor = 1.0;
#endif
  unsigned short wAdjustADResolution;

  DBG (DBG_FUNC, "Reflective_AdjustAD: call in\n");
  if (!g_bOpened)
    {
      DBG (DBG_FUNC, "Reflective_AdjustAD: scanner has been opened\n");
      return FALSE;
    }
  if (!g_bPrepared)
    {
      DBG (DBG_FUNC, "Reflective_AdjustAD: scanner not prepared\n");
      return FALSE;
    }


  g_chip.AD.DirectionR = R_DIRECTION;
  g_chip.AD.DirectionG = G_DIRECTION;
  g_chip.AD.DirectionB = B_DIRECTION;
  g_chip.AD.GainR = R_GAIN;
  g_chip.AD.GainG = G_GAIN;
  g_chip.AD.GainB = B_GAIN;
  g_chip.AD.OffsetR = 152;
  g_chip.AD.OffsetG = 56;
  g_chip.AD.OffsetB = 8;

  if (g_XDpi <= 600)
    {
      wAdjustADResolution = 600;
    }
  else
    {
      wAdjustADResolution = 1200;
    }
  wCalWidth = 10240;

  lpCalData = (SANE_Byte *) malloc (sizeof (SANE_Byte) * wCalWidth * 3);
  if (lpCalData == NULL)
    {
      DBG (DBG_FUNC, "Reflective_AdjustAD: lpCalData malloc error\n");
      return FALSE;
    }

  Asic_SetMotorType (&g_chip, FALSE, TRUE);

  Asic_SetCalibrate (&g_chip, 24, wAdjustADResolution, wAdjustADResolution, 0,
		     0, wCalWidth, 1, FALSE);
  MustScanner_PrepareCalculateMaxMin (wAdjustADResolution);
  nTimesOfCal = 0;

#ifdef DEBUG_SAVE_IMAGE
  Asic_SetAFEGainOffset (&g_chip);
  Asic_ScanStart (&g_chip);
  Asic_ReadCalibrationData (&g_chip, lpCalData, wCalWidth * 3, 24);
  Asic_ScanStop (&g_chip);

  FILE *stream = NULL;
  SANE_Byte * lpBuf = (SANE_Byte *) malloc (50);
  if (NULL == lpBuf)
    {
      return FALSE;
    }
  memset (lpBuf, 0, 50);

  stream = fopen ("/root/AD(Ref).pnm\n", "wb+\n");
  sprintf (lpBuf, "P6\n%d %d\n255\n\n", wCalWidth, 1);
  fwrite (lpBuf, sizeof (SANE_Byte), strlen (lpBuf), stream);
  fwrite (lpCalData, sizeof (SANE_Byte), wCalWidth * 3, stream);
  fclose (stream);
  free (lpBuf);
#endif

  do
    {
      DBG (DBG_FUNC,
	   "Reflective_AdjustAD: run in first adjust offset do-while\n");
      Asic_SetAFEGainOffset (&g_chip);
      Asic_ScanStart (&g_chip);
      Asic_ReadCalibrationData (&g_chip, lpCalData, wCalWidth * 3, 24);
      Asic_ScanStop (&g_chip);

      MustScanner_CalculateMaxMin (lpCalData, &wMaxValueR, &wMinValueR,
				   wAdjustADResolution);
      MustScanner_CalculateMaxMin (lpCalData + wCalWidth, &wMaxValueG,
				   &wMinValueG, wAdjustADResolution);
      MustScanner_CalculateMaxMin (lpCalData + wCalWidth * 2, &wMaxValueB,
				   &wMinValueB, wAdjustADResolution);

      if (g_chip.AD.DirectionR == 0)
	{
	  if (wMinValueR > 15)
	    {
	      if (g_chip.AD.OffsetR < 8)
		g_chip.AD.DirectionR = 1;
	      else
		g_chip.AD.OffsetR -= 8;
	    }
	  else if (wMinValueR < 5)
	    g_chip.AD.OffsetR += 8;
	}
      else
	{
	  if (wMinValueR > 15)
	    g_chip.AD.OffsetR += 8;
	  else if (wMinValueR < 5)
	    g_chip.AD.OffsetR -= 8;
	}

      if (g_chip.AD.DirectionG == 0)
	{
	  if (wMinValueG > 15)
	    {
	      if (g_chip.AD.OffsetG < 8)
		g_chip.AD.DirectionG = 1;
	      else
		g_chip.AD.OffsetG -= 8;
	    }
	  else if (wMinValueG < 5)
	    g_chip.AD.OffsetG += 8;
	}
      else
	{
	  if (wMinValueG > 15)
	    g_chip.AD.OffsetG += 8;
	  else if (wMinValueG < 5)
	    g_chip.AD.OffsetG -= 8;
	}

      if (g_chip.AD.DirectionB == 0)
	{
	  if (wMinValueB > 15)
	    {
	      if (g_chip.AD.OffsetB < 8)
		g_chip.AD.DirectionB = 1;
	      else
		g_chip.AD.OffsetB -= 8;
	    }

	  else if (wMinValueB < 5)
	    g_chip.AD.OffsetB += 8;
	}
      else
	{
	  if (wMinValueB > 15)
	    g_chip.AD.OffsetB += 8;
	  else if (wMinValueB < 5)
	    g_chip.AD.OffsetB -= 8;
	}

      nTimesOfCal++;
      if (nTimesOfCal > 10)
	break;
    }
  while (wMinValueR > 15 || wMinValueR < 5
	 || wMinValueG > 15 || wMinValueG < 5
	 || wMinValueB > 15 || wMinValueB < 5);

  DBG (DBG_FUNC,
       "Reflective_AdjustAD: run out first adjust offset do-while\n");

  DBG (DBG_FUNC, "Reflective_AdjustAD: \
					   g_chip.AD.OffsetR=%d,\
					   g_chip.AD.OffsetG=%d,\
					   g_chip.AD.OffsetB=%d\n", g_chip.AD.OffsetR, g_chip.AD.OffsetG, g_chip.AD.OffsetB);

  g_chip.AD.GainR = 1 - (double) (wMaxValueR - wMinValueR) / 210 > 0 ?
    (SANE_Byte) (((1 -
	      (double) (wMaxValueR - wMinValueR) / 210)) * 63 * 6 / 5) : 0;
  g_chip.AD.GainG =
    1 - (double) (wMaxValueG - wMinValueG) / 210 >
    0 ? (SANE_Byte) (((1 - (double) (wMaxValueG - wMinValueG) / 210)) * 63 * 6 /
		5) : 0;
  g_chip.AD.GainB =
    1 - (double) (wMaxValueB - wMinValueB) / 210 >
    0 ? (SANE_Byte) (((1 - (double) (wMaxValueB - wMinValueB) / 210)) * 63 * 6 /
		5) : 0;

  if (g_chip.AD.GainR > 63)
    g_chip.AD.GainR = 63;
  if (g_chip.AD.GainG > 63)
    g_chip.AD.GainG = 63;
  if (g_chip.AD.GainB > 63)
    g_chip.AD.GainB = 63;

  DBG (DBG_FUNC, "Reflective_AdjustAD: "
       "g_chip.AD.GainR = %d,"
       "g_chip.AD.GainG = %d,"
       "g_chip.AD.GainB = %d\n",
       g_chip.AD.GainR, g_chip.AD.GainG, g_chip.AD.GainB);

  nTimesOfCal = 0;
  do
    {
      Asic_SetAFEGainOffset (&g_chip);
      Asic_ScanStart (&g_chip);
      Asic_ReadCalibrationData (&g_chip, lpCalData, wCalWidth * 3, 24);
      Asic_ScanStop (&g_chip);

      MustScanner_CalculateMaxMin (lpCalData, &wMaxValueR, &wMinValueR,
				   wAdjustADResolution);
      MustScanner_CalculateMaxMin (lpCalData + wCalWidth, &wMaxValueG,
				   &wMinValueG, wAdjustADResolution);
      MustScanner_CalculateMaxMin (lpCalData + wCalWidth * 2, &wMaxValueB,
				   &wMinValueB, wAdjustADResolution);

      DBG (DBG_FUNC, "Reflective_AdjustAD: "
	   "RGain=%d, ROffset=%d, RDir=%d  GGain=%d, GOffset=%d, GDir=%d  BGain=%d, BOffset=%d, BDir=%d\n",
	   g_chip.AD.GainR, g_chip.AD.OffsetR, g_chip.AD.DirectionR,
	   g_chip.AD.GainG, g_chip.AD.OffsetG, g_chip.AD.DirectionG,
	   g_chip.AD.GainB, g_chip.AD.OffsetB, g_chip.AD.DirectionB);

      DBG (DBG_FUNC, "Reflective_AdjustAD: "
	   "MaxR=%d, MinR=%d  MaxG=%d, MinG=%d  MaxB=%d, MinB=%d\n",
	   wMaxValueR, wMinValueR, wMaxValueG, wMinValueG, wMaxValueB,
	   wMinValueB);

      /*R Channel */
      if ((wMaxValueR - wMinValueR) > REFL_MAX_LEVEL_RANGE)
	{
	  if (g_chip.AD.GainR > 0)
	    g_chip.AD.GainR--;
	}
      else
	{
	  if ((wMaxValueR - wMinValueR) < REFL_MIN_LEVEL_RANGE)
	    {
	      if (wMaxValueR < REFL_WHITE_MIN_LEVEL)
		{
		  g_chip.AD.GainR++;
		  if (g_chip.AD.GainR > 63)
		    g_chip.AD.GainR = 63;
		}
	      else
		{
		  if (wMaxValueR > REFL_WHITE_MAX_LEVEL)
		    {
		      if (g_chip.AD.GainR < 1)
			g_chip.AD.GainR = 0;
		      else
			g_chip.AD.GainR--;
		    }
		  else
		    {
		      if (g_chip.AD.GainR > 63)
			g_chip.AD.GainR = 63;
		      else
			g_chip.AD.GainR++;
		    }
		}
	    }
	  else
	    {
	      if (wMaxValueR > REFL_WHITE_MAX_LEVEL)
		{
		  if (g_chip.AD.GainR < 1)
		    g_chip.AD.GainR = 0;
		  else
		    g_chip.AD.GainR--;
		}

	      if (wMaxValueR < REFL_WHITE_MIN_LEVEL)
		{
		  if (g_chip.AD.GainR > 63)
		    g_chip.AD.GainR = 63;
		  else
		    g_chip.AD.GainR++;
		}
	    }
	}

      /*G Channel */
      if ((wMaxValueG - wMinValueG) > REFL_MAX_LEVEL_RANGE)
	{
	  if (g_chip.AD.GainG > 0)
	    g_chip.AD.GainG--;
	}
      else
	{
	  if ((wMaxValueG - wMinValueG) < REFL_MIN_LEVEL_RANGE)
	    {
	      if (wMaxValueG < REFL_WHITE_MIN_LEVEL)
		{
		  g_chip.AD.GainG++;
		  if (g_chip.AD.GainG > 63)
		    g_chip.AD.GainG = 63;
		}
	      else
		{
		  if (wMaxValueG > REFL_WHITE_MAX_LEVEL)
		    {
		      if (g_chip.AD.GainG < 1)
			g_chip.AD.GainG = 0;
		      else
			g_chip.AD.GainG--;
		    }
		  else
		    {
		      if (g_chip.AD.GainG > 63)
			g_chip.AD.GainG = 63;
		      else
			g_chip.AD.GainG++;
		    }
		}
	    }
	  else
	    {
	      if (wMaxValueG > REFL_WHITE_MAX_LEVEL)
		{
		  if (g_chip.AD.GainG < 1)
		    g_chip.AD.GainG = 0;
		  else
		    g_chip.AD.GainG--;
		}

	      if (wMaxValueG < REFL_WHITE_MIN_LEVEL)
		{
		  if (g_chip.AD.GainG > 63)
		    g_chip.AD.GainG = 63;
		  else
		    g_chip.AD.GainG++;
		}
	    }
	}

      /* B Channel */
      if ((wMaxValueB - wMinValueB) > REFL_MAX_LEVEL_RANGE)
	{
	  if (g_chip.AD.GainB > 0)
	    g_chip.AD.GainB--;
	}
      else
	{
	  if ((wMaxValueB - wMinValueB) < REFL_MIN_LEVEL_RANGE)
	    {
	      if (wMaxValueB < REFL_WHITE_MIN_LEVEL)
		{
		  g_chip.AD.GainB++;
		  if (g_chip.AD.GainB > 63)
		    g_chip.AD.GainB = 63;
		}
	      else
		{
		  if (wMaxValueB > REFL_WHITE_MAX_LEVEL)
		    {
		      if (g_chip.AD.GainB < 1)
			g_chip.AD.GainB = 0;
		      else
			g_chip.AD.GainB--;
		    }
		  else
		    {
		      if (g_chip.AD.GainB > 63)
			g_chip.AD.GainB = 63;
		      else
			g_chip.AD.GainB++;
		    }
		}
	    }
	  else
	    {
	      if (wMaxValueB > REFL_WHITE_MAX_LEVEL)

		{
		  if (g_chip.AD.GainB < 1)
		    g_chip.AD.GainB = 0;
		  else
		    g_chip.AD.GainB--;
		}

	      if (wMaxValueB < REFL_WHITE_MIN_LEVEL)
		{
		  if (g_chip.AD.GainB > 63)
		    g_chip.AD.GainB = 63;
		  else
		    g_chip.AD.GainB++;
		}
	    }
	}
      nTimesOfCal++;
      if (nTimesOfCal > 10)
	break;
    }
  while ((wMaxValueR - wMinValueR) > REFL_MAX_LEVEL_RANGE
	 || (wMaxValueR - wMinValueR) < REFL_MIN_LEVEL_RANGE
	 || (wMaxValueG - wMinValueG) > REFL_MAX_LEVEL_RANGE
	 || (wMaxValueG - wMinValueG) < REFL_MIN_LEVEL_RANGE
	 || (wMaxValueB - wMinValueB) > REFL_MAX_LEVEL_RANGE
	 || (wMaxValueB - wMinValueB) < REFL_MIN_LEVEL_RANGE);

  /* Adjust Offset 2nd */
  nTimesOfCal = 0;
  do
    {
      DBG (DBG_FUNC,
	   "Reflective_AdjustAD: run in second adjust offset do-while\n");
      Asic_SetAFEGainOffset (&g_chip);
      Asic_ScanStart (&g_chip);
      Asic_ReadCalibrationData (&g_chip, lpCalData, wCalWidth * 3, 24);
      Asic_ScanStop (&g_chip);

      MustScanner_CalculateMaxMin (lpCalData, &wMaxValueR, &wMinValueR,
				   wAdjustADResolution);
      MustScanner_CalculateMaxMin (lpCalData + wCalWidth, &wMaxValueG,
				   &wMinValueG, wAdjustADResolution);
      MustScanner_CalculateMaxMin (lpCalData + wCalWidth * 2, &wMaxValueB,
				   &wMinValueB, wAdjustADResolution);

      DBG (DBG_FUNC, "Reflective_AdjustAD: "
	   "RGain=%d, ROffset=%d, RDir=%d  GGain=%d, GOffset=%d, GDir=%d  BGain=%d, BOffset=%d, BDir=%d\n",
	   g_chip.AD.GainR, g_chip.AD.OffsetR, g_chip.AD.DirectionR,
	   g_chip.AD.GainG, g_chip.AD.OffsetG, g_chip.AD.DirectionG,
	   g_chip.AD.GainB, g_chip.AD.OffsetB, g_chip.AD.DirectionB);

      DBG (DBG_FUNC, "Reflective_AdjustAD: "
	   "MaxR=%d, MinR=%d  MaxG=%d, MinG=%d  MaxB=%d, MinB=%d\n",
	   wMaxValueR, wMinValueR, wMaxValueG, wMinValueG, wMaxValueB,
	   wMinValueB);

      if (g_chip.AD.DirectionR == 0)
	{
	  if (wMinValueR > 20)
	    {
	      if (g_chip.AD.OffsetR < 8)
		g_chip.AD.DirectionR = 1;
	      else
		g_chip.AD.OffsetR -= 8;
	    }

	  else if (wMinValueR < 10)
	    g_chip.AD.OffsetR += 8;
	}
      else
	{
	  if (wMinValueR > 20)
	    g_chip.AD.OffsetR += 8;
	  else if (wMinValueR < 10)
	    g_chip.AD.OffsetR -= 8;
	}

      if (g_chip.AD.DirectionG == 0)
	{
	  if (wMinValueG > 20)
	    {
	      if (g_chip.AD.OffsetG < 8)
		g_chip.AD.DirectionG = 1;
	      else
		g_chip.AD.OffsetG -= 8;
	    }
	  else if (wMinValueG < 10)
	    g_chip.AD.OffsetG += 8;
	}
      else
	{
	  if (wMinValueG > 20)
	    g_chip.AD.OffsetG += 8;
	  else if (wMinValueG < 10)
	    g_chip.AD.OffsetG -= 8;
	}

      if (g_chip.AD.DirectionB == 0)
	{
	  if (wMinValueB > 20)
	    {
	      if (g_chip.AD.OffsetB < 8)
		g_chip.AD.DirectionB = 1;
	      else
		g_chip.AD.OffsetB -= 8;
	    }
	  else if (wMinValueB < 10)
	    g_chip.AD.OffsetB += 8;
	}
      else
	{
	  if (wMinValueB > 20)
	    g_chip.AD.OffsetB += 8;
	  else if (wMinValueB < 10)
	    g_chip.AD.OffsetB -= 8;
	}

      nTimesOfCal++;
      if (nTimesOfCal > 8)
	break;

    }
  while (wMinValueR > 20 || wMinValueR < 10
	 || wMinValueG > 20 || wMinValueG < 10
	 || wMinValueB > 20 || wMinValueB < 10);

  DBG (DBG_FUNC,
       "Reflective_AdjustAD: run in second adjust offset do-while\n");

  DBG (DBG_FUNC, "Reflective_AdjustAD:after ad gain\n");
  DBG (DBG_FUNC, "Reflective_AdjustAD: "
       "g_chip.AD.GainR = %d,"
       "g_chip.AD.GainG = %d,"
       "g_chip.AD.GainB = %d\n",
       g_chip.AD.GainR, g_chip.AD.GainG, g_chip.AD.GainB);

  free (lpCalData);

  DBG (DBG_FUNC, "Reflective_AdjustAD: leave Reflective_AdjustAD\n");
  return TRUE;
}

/**********************************************************************
Author: Jack             Date: 2005/05/14
Routine Description: 
	Find top and left side
Parameters:
	lpwStartX: the left side
	lpwStartY: the top side
Return value:
	if operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
Reflective_FindTopLeft (unsigned short * lpwStartX, unsigned short * lpwStartY)
{
  unsigned short wCalWidth = FIND_LEFT_TOP_WIDTH_IN_DIP;
  unsigned short wCalHeight = FIND_LEFT_TOP_HEIGHT_IN_DIP;

  int i, j;
  unsigned short wLeftSide;
  unsigned short wTopSide;
  int nScanBlock;
  SANE_Byte * lpCalData;
  unsigned int dwTotalSize;
  unsigned short wXResolution, wYResolution;

  DBG (DBG_FUNC, "Reflective_FindTopLeft: call in\n");
  if (!g_bOpened)
    {
      DBG (DBG_FUNC, "Reflective_FindTopLeft: scanner has been opened\n");
      return FALSE;
    }
  if (!g_bPrepared)
    {
      DBG (DBG_FUNC, "Reflective_FindTopLeft: scanner not prepared\n");
      return FALSE;
    }

  wXResolution = wYResolution = FIND_LEFT_TOP_CALIBRATE_RESOLUTION;

  lpCalData = (SANE_Byte *) malloc (sizeof (SANE_Byte) * wCalWidth * wCalHeight);
  if (lpCalData == NULL)
    {
      DBG (DBG_FUNC, "Reflective_FindTopLeft: lpCalData malloc error\n");
      return FALSE;
    }

  dwTotalSize = wCalWidth * wCalHeight;
  nScanBlock = (int) (dwTotalSize / g_dwCalibrationSize);

  Asic_SetMotorType (&g_chip, TRUE, TRUE);
  Asic_SetCalibrate (&g_chip, 8, wXResolution, wYResolution, 0, 0, wCalWidth,
		     wCalHeight, FALSE);
  Asic_SetAFEGainOffset (&g_chip);
  if (Asic_ScanStart (&g_chip) != STATUS_GOOD)
    {
      DBG (DBG_FUNC, "Reflective_FindTopLeft: Asic_ScanStart return error\n");
      free (lpCalData);
      return FALSE;
    }

  for (i = 0; i < nScanBlock; i++)
    {
      if (STATUS_GOOD !=
	  Asic_ReadCalibrationData (&g_chip,
				    lpCalData + i * g_dwCalibrationSize,
				    g_dwCalibrationSize, 8))
	{
	  DBG (DBG_FUNC,
	       "Reflective_FindTopLeft: Asic_ReadCalibrationData return error\n");
	  free (lpCalData);
	  return FALSE;
	}
    }

  if (STATUS_GOOD !=
      Asic_ReadCalibrationData (&g_chip,
				lpCalData +
				(nScanBlock) * g_dwCalibrationSize,
				(dwTotalSize -
				 g_dwCalibrationSize * nScanBlock), 8))
    {

      DBG (DBG_FUNC,
	   "Reflective_FindTopLeft: Asic_ReadCalibrationData return error\n");
      free (lpCalData);
      return FALSE;
    }

  Asic_ScanStop (&g_chip);

#ifdef DEBUG_SAVE_IMAGE
  FILE *stream = NULL;
  stream = fopen ("/root/bound(Ref).pnm", "wb+\n");
  SANE_Byte * lpBuf = (SANE_Byte *) malloc (50);
  if (NULL == lpBuf)
    {
      return FALSE;
    }
  memset (lpBuf, 0, 50);
  sprintf (lpBuf, "P5\n%d %d\n255\n", wCalWidth, wCalHeight);
  fwrite (lpBuf, sizeof (SANE_Byte), strlen (lpBuf), stream);
  fwrite (lpCalData, sizeof (SANE_Byte), wCalWidth * wCalHeight, stream);

  fclose (stream);
  free (lpBuf);
#endif

  wLeftSide = 0;
  wTopSide = 0;

  /* Find Left Side */
  for (i = wCalWidth - 1; i > 0; i--)
    {
      wLeftSide = *(lpCalData + i);
      wLeftSide += *(lpCalData + wCalWidth * 2 + i);
      wLeftSide += *(lpCalData + wCalWidth * 4 + i);
      wLeftSide += *(lpCalData + wCalWidth * 6 + i);
      wLeftSide += *(lpCalData + wCalWidth * 8 + i);
      wLeftSide /= 5;
      if (wLeftSide < 60)
	{
	  if (i == wCalWidth - 1)
	    {
	      break;
	    }
	  *lpwStartX = i;
	  {
	    break;
	  }
	}
    }

  /*Find Top Side i=left side */
  for (j = 0; j < wCalHeight; j++)
    {
      wTopSide = *(lpCalData + wCalWidth * j + i - 2);
      wTopSide += *(lpCalData + wCalWidth * j + i - 4);
      wTopSide += *(lpCalData + wCalWidth * j + i - 6);
      wTopSide += *(lpCalData + wCalWidth * j + i - 8);
      wTopSide += *(lpCalData + wCalWidth * j + i - 10);

      wTopSide /= 5;
      if (wTopSide > 60)
	{
	  if (j == 0)
	    {
	      break;
	    }
	  *lpwStartY = j;
	  {
	    break;
	  }
	}
    }

  if ((*lpwStartX < 100) || (*lpwStartX > 250))
    {
      *lpwStartX = 187;
    }

  if ((*lpwStartY < 10) || (*lpwStartY > 100))
    {
      *lpwStartY = 43;
    }

  DBG (DBG_FUNC,
       "Reflective_FindTopLeft: *lpwStartY = %d, *lpwStartX = %d\n",
       *lpwStartY, *lpwStartX);
  Asic_MotorMove (&g_chip, FALSE,
		  (wCalHeight - *lpwStartY +
		   BEFORE_SCANNING_MOTOR_FORWARD_PIXEL) * 1200 /
		  wYResolution);

  free (lpCalData);

  DBG (DBG_FUNC, "Reflective_FindTopLeft: leave Reflective_FindTopLeft\n");
  return TRUE;

}

/**********************************************************************
Author: Jack             Date: 2005/05/14
Routine Description: 
	Stop scan
Parameters:
	none
Return value: 
	if operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
Reflective_StopScan ()
{
  DBG (DBG_FUNC, "Reflective_StopScan: call in\n");
  if (!g_bOpened)
    {
      DBG (DBG_FUNC, "Reflective_StopScan: scanner not opened\n");

      return FALSE;
    }
  if (!g_bPrepared)
    {
      DBG (DBG_FUNC, "Reflective_StopScan: scanner not prepared\n");

      return FALSE;
    }

  g_isCanceled = TRUE;		/*tell parent process stop read image */

  pthread_cancel (g_threadid_readimage);
  pthread_join (g_threadid_readimage, NULL);

  DBG (DBG_FUNC, "Reflective_StopScan: thread exit\n");

  Asic_ScanStop (&g_chip);
  Asic_Close (&g_chip);

  g_bOpened = FALSE;

  DBG (DBG_FUNC, "Reflective_StopScan: leave Reflective_StopScan\n");
  return TRUE;
}

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Get the calibration data
Parameters:
	none
Return value: 
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
Reflective_LineCalibration16Bits ()
{
  STATUS status;
  SANE_Byte * lpWhiteData;
  SANE_Byte * lpDarkData;
  unsigned int dwWhiteTotalSize;
  unsigned int dwDarkTotalSize;
  unsigned short wCalHeight = LINE_CALIBRATION__16BITS_HEIGHT;
  unsigned short wCalWidth;

  unsigned short *lpWhiteShading;
  unsigned short *lpDarkShading;
  double wRWhiteLevel = 0;
  double wGWhiteLevel = 0;
  double wBWhiteLevel = 0;
  unsigned int dwRDarkLevel = 0;
  unsigned int dwGDarkLevel = 0;
  unsigned int dwBDarkLevel = 0;
  unsigned int dwREvenDarkLevel = 0;
  unsigned int dwGEvenDarkLevel = 0;
  unsigned int dwBEvenDarkLevel = 0;
  unsigned short * lpRWhiteSort;
  unsigned short * lpGWhiteSort;
  unsigned short * lpBWhiteSort;
  unsigned short * lpRDarkSort;
  unsigned short * lpGDarkSort;
  unsigned short * lpBDarkSort;
  int i, j;

  DBG (DBG_FUNC, "Reflective_LineCalibration16Bits: call in\n");
  if (!g_bOpened)
    {
      DBG (DBG_FUNC,
	   "Reflective_LineCalibration16Bits: scanner not opened\n");

      return FALSE;
    }
  if (!g_bPrepared)
    {
      DBG (DBG_FUNC,
	   "Reflective_LineCalibration16Bits: scanner not prepared\n");

      return FALSE;
    }

  wCalWidth = g_Width;

  dwWhiteTotalSize = wCalWidth * wCalHeight * 3 * 2;
  dwDarkTotalSize = wCalWidth * wCalHeight * 3 * 2;
  lpWhiteData = (SANE_Byte *) malloc (sizeof (SANE_Byte) * dwWhiteTotalSize);
  lpDarkData = (SANE_Byte *) malloc (sizeof (SANE_Byte) * dwDarkTotalSize);

  if (lpWhiteData == NULL || lpDarkData == NULL)
    {
      DBG (DBG_FUNC,
	   "Reflective_LineCalibration16Bits: lpWhiteData or lpDarkData malloc error \n");

      return FALSE;
    }

  Asic_SetMotorType (&g_chip, TRUE, TRUE);
  Asic_SetAFEGainOffset (&g_chip);
  status =
    Asic_SetCalibrate (&g_chip, 48, g_XDpi, g_YDpi, g_X, 0, wCalWidth,
		       wCalHeight, TRUE);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_FUNC,
	   "Reflective_LineCalibration16Bits: Asic_SetCalibrate return error \n");

      free (lpWhiteData);

      free (lpDarkData);
      return FALSE;
    }

  status = Asic_ScanStart (&g_chip);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_FUNC,
	   "Reflective_LineCalibration16Bits: Asic_ScanStart return error \n");

      free (lpWhiteData);
      free (lpDarkData);
      return FALSE;
    }

  status =
    Asic_ReadCalibrationData (&g_chip, lpWhiteData, dwWhiteTotalSize, 8);
  if (status != STATUS_GOOD)
    {
      free (lpWhiteData);
      free (lpDarkData);
      return FALSE;
    }

  Asic_ScanStop (&g_chip);

  /*Read dark level data */
  status = Asic_SetMotorType (&g_chip, FALSE, TRUE);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_FUNC,
	   "Reflective_LineCalibration16Bits: Asic_SetMotorType return error \n");

      free (lpWhiteData);
      free (lpDarkData);
      return FALSE;
    }

  status =
    Asic_SetCalibrate (&g_chip, 48, g_XDpi, g_YDpi, g_X, 0, wCalWidth,
		       wCalHeight, TRUE);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_FUNC,
	   "Reflective_LineCalibration16Bits: Asic_SetCalibrate return error \n");

      free (lpWhiteData);
      free (lpDarkData);
      return FALSE;
    }

  status = Asic_TurnLamp (&g_chip, FALSE);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_FUNC,
	   "Reflective_LineCalibration16Bits: Asic_TurnLamp return error \n");

      free (lpWhiteData);
      free (lpDarkData);
      return FALSE;
    }

  usleep (500000);

  status = Asic_ScanStart (&g_chip);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_FUNC,
	   "Reflective_LineCalibration16Bits: Asic_ScanStart return error \n");

      free (lpWhiteData);
      free (lpDarkData);
      return FALSE;
    }

  status = Asic_ReadCalibrationData (&g_chip, lpDarkData, dwDarkTotalSize, 8);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_FUNC,
	   "Reflective_LineCalibration16Bits: Asic_ReadCalibrationData return error \n");

      free (lpWhiteData);
      free (lpDarkData);
      return FALSE;
    }

  Asic_ScanStop (&g_chip);

  /* Turn on lamp */
  status = Asic_TurnLamp (&g_chip, TRUE);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_FUNC,
	   "Reflective_LineCalibration16Bits: Asic_TurnLamp return error \n");

      free (lpWhiteData);
      free (lpDarkData);
      return FALSE;
    }

#ifdef DEBUG_SAVE_IMAGE
  FILE *stream = NULL;
  SANE_Byte * lpBuf = (SANE_Byte *) malloc (50);
  if (NULL == lpBuf)
    {
      return FALSE;

    }
  memset (lpBuf, 0, 50);
  stream = fopen ("/root/whiteshading(Ref).pnm", "wb+\n");
  sprintf (lpBuf, "P6\n%d %d\n65535\n", wCalWidth, wCalHeight);
  fwrite (lpBuf, sizeof (SANE_Byte), strlen (lpBuf), stream);
  fwrite (lpWhiteData, sizeof (SANE_Byte), wCalWidth * wCalHeight * 3 * 2, stream);
  fclose (stream);

  memset (lpBuf, 0, 50);
  stream = fopen ("/root/darkshading(Ref).pnm", "wb+\n");
  sprintf (lpBuf, "P6\n%d %d\n65535\n", wCalWidth, wCalHeight);
  fwrite (lpBuf, sizeof (SANE_Byte), strlen (lpBuf), stream);
  fwrite (lpDarkData, sizeof (SANE_Byte), wCalWidth * wCalHeight * 3 * 2, stream);
  fclose (stream);
  free (lpBuf);
#endif

  sleep (1);

  lpWhiteShading = (unsigned short *) malloc (sizeof (unsigned short) * wCalWidth * 3);
  lpDarkShading = (unsigned short *) malloc (sizeof (unsigned short) * wCalWidth * 3);

  lpRWhiteSort = (unsigned short *) malloc (sizeof (unsigned short) * wCalHeight);
  lpGWhiteSort = (unsigned short *) malloc (sizeof (unsigned short) * wCalHeight);
  lpBWhiteSort = (unsigned short *) malloc (sizeof (unsigned short) * wCalHeight);
  lpRDarkSort = (unsigned short *) malloc (sizeof (unsigned short) * wCalHeight);
  lpGDarkSort = (unsigned short *) malloc (sizeof (unsigned short) * wCalHeight);
  lpBDarkSort = (unsigned short *) malloc (sizeof (unsigned short) * wCalHeight);

  if (lpWhiteShading == NULL || lpDarkShading == NULL
      || lpRWhiteSort == NULL || lpGWhiteSort == NULL || lpBWhiteSort == NULL
      || lpRDarkSort == NULL || lpGDarkSort == NULL || lpBDarkSort == NULL)
    {
      DBG (DBG_FUNC, "Reflective_LineCalibration16Bits: malloc error \n");

      free (lpWhiteData);
      free (lpDarkData);
      return FALSE;
    }

  /* create dark level shading */
  dwRDarkLevel = 0;
  dwGDarkLevel = 0;
  dwBDarkLevel = 0;
  dwREvenDarkLevel = 0;
  dwGEvenDarkLevel = 0;
  dwBEvenDarkLevel = 0;

  DBG (DBG_FUNC,
       "Reflective_LineCalibration16Bits: wCalWidth = %d, wCalHeight = %d\n",
       wCalWidth, wCalHeight);

  for (i = 0; i < wCalWidth; i++)
    {
      for (j = 0; j < wCalHeight; j++)
	{
	  lpRDarkSort[j] =
	    (unsigned short) (*(lpDarkData + j * wCalWidth * 6 + i * 6 + 0));
	  lpRDarkSort[j] +=
	    (unsigned short) (*(lpDarkData + j * wCalWidth * 6 + i * 6 + 1) << 8);

	  lpGDarkSort[j] =
	    (unsigned short) (*(lpDarkData + j * wCalWidth * 6 + i * 6 + 2));
	  lpGDarkSort[j] +=
	    (unsigned short) (*(lpDarkData + j * wCalWidth * 6 + i * 6 + 3) << 8);

	  lpBDarkSort[j] =
	    (unsigned short) (*(lpDarkData + j * wCalWidth * 6 + i * 6 + 4));
	  lpBDarkSort[j] +=
	    (unsigned short) (*(lpDarkData + j * wCalWidth * 6 + i * 6 + 5) << 8);
	}

      if (g_XDpi == 1200)
	{

	  /*do dark shading table with mean */
	  if (i % 2)
	    {
	      dwRDarkLevel +=
		(unsigned int) MustScanner_FiltLower (lpRDarkSort, wCalHeight, 20,
					       30);
	      dwGDarkLevel +=
		(unsigned int) MustScanner_FiltLower (lpGDarkSort, wCalHeight, 20,
					       30);
	      dwBDarkLevel +=
		(unsigned int) MustScanner_FiltLower (lpBDarkSort, wCalHeight, 20,
					       30);
	    }
	  else
	    {
	      dwREvenDarkLevel +=
		(unsigned int) MustScanner_FiltLower (lpRDarkSort, wCalHeight, 20,
					       30);

	      dwGEvenDarkLevel +=
		(unsigned int) MustScanner_FiltLower (lpGDarkSort, wCalHeight, 20,
					       30);
	      dwBEvenDarkLevel +=
		(unsigned int) MustScanner_FiltLower (lpBDarkSort, wCalHeight, 20,
					       30);
	    }
	}
      else
	{

	  dwRDarkLevel +=
	    (unsigned int) MustScanner_FiltLower (lpRDarkSort, wCalHeight, 20, 30);
	  dwGDarkLevel +=
	    (unsigned int) MustScanner_FiltLower (lpGDarkSort, wCalHeight, 20, 30);
	  dwBDarkLevel +=
	    (unsigned int) MustScanner_FiltLower (lpBDarkSort, wCalHeight, 20, 30);
	}
    }

  if (g_XDpi == 1200)
    {
      dwRDarkLevel = (unsigned int) (dwRDarkLevel / (wCalWidth / 2));
      dwGDarkLevel = (unsigned int) (dwGDarkLevel / (wCalWidth / 2));
      dwBDarkLevel = (unsigned int) (dwBDarkLevel / (wCalWidth / 2));
      dwREvenDarkLevel = (unsigned int) (dwREvenDarkLevel / (wCalWidth / 2));
      dwGEvenDarkLevel = (unsigned int) (dwGEvenDarkLevel / (wCalWidth / 2));
      dwBEvenDarkLevel = (unsigned int) (dwBEvenDarkLevel / (wCalWidth / 2));
    }
  else
    {
      dwRDarkLevel = (unsigned int) (dwRDarkLevel / wCalWidth);
      dwGDarkLevel = (unsigned int) (dwGDarkLevel / wCalWidth);
      dwBDarkLevel = (unsigned int) (dwBDarkLevel / wCalWidth);
    }

  /*Create white shading */
  for (i = 0; i < wCalWidth; i++)
    {
      wRWhiteLevel = 0;
      wGWhiteLevel = 0;
      wBWhiteLevel = 0;

      for (j = 0; j < wCalHeight; j++)
	{
	  lpRWhiteSort[j] =
	    (unsigned short) (*(lpWhiteData + j * wCalWidth * 2 * 3 + i * 6 + 0));
	  lpRWhiteSort[j] +=
	    (unsigned short) (*(lpWhiteData + j * wCalWidth * 2 * 3 + i * 6 + 1) << 8);

	  lpGWhiteSort[j] =
	    (unsigned short) (*(lpWhiteData + j * wCalWidth * 2 * 3 + i * 6 + 2));
	  lpGWhiteSort[j] +=
	    (unsigned short) (*(lpWhiteData + j * wCalWidth * 2 * 3 + i * 6 + 3) << 8);

	  lpBWhiteSort[j] =
	    (unsigned short) (*(lpWhiteData + j * wCalWidth * 2 * 3 + i * 6 + 4));
	  lpBWhiteSort[j] +=
	    (unsigned short) (*(lpWhiteData + j * wCalWidth * 2 * 3 + i * 6 + 5) << 8);
	}

      if (g_XDpi == 1200)
	{
	  if (i % 2)
	    {
	      *(lpDarkShading + i * 3 + 0) = (unsigned short) dwRDarkLevel;
	      *(lpDarkShading + i * 3 + 1) = (unsigned short) dwGDarkLevel;
	      *(lpDarkShading + i * 3 + 2) = (unsigned short) dwBDarkLevel;
	    }
	  else
	    {
	      *(lpDarkShading + i * 3 + 0) = (unsigned short) dwREvenDarkLevel;
	      *(lpDarkShading + i * 3 + 1) = (unsigned short) dwGEvenDarkLevel;
	      *(lpDarkShading + i * 3 + 2) = (unsigned short) dwBEvenDarkLevel;
	    }
	}
      else
	{
	  *(lpDarkShading + i * 3 + 0) = (unsigned short) dwRDarkLevel;
	  *(lpDarkShading + i * 3 + 1) = (unsigned short) dwGDarkLevel;
	  *(lpDarkShading + i * 3 + 2) = (unsigned short) dwBDarkLevel;
	}


      /*Create white shading */
      wRWhiteLevel =
	(double) (MustScanner_FiltLower (lpRWhiteSort, wCalHeight, 20, 30) -
		  *(lpDarkShading + i * 3 + 0));
      wGWhiteLevel =
	(double) (MustScanner_FiltLower (lpGWhiteSort, wCalHeight, 20, 30) -
		  *(lpDarkShading + i * 3 + 1));
      wBWhiteLevel =
	(double) (MustScanner_FiltLower (lpBWhiteSort, wCalHeight, 20, 30) -
		  *(lpDarkShading + i * 3 + 2));

      if (wRWhiteLevel > 0)
	*(lpWhiteShading + i * 3 + 0) =
	  (unsigned short) (((float) 65535 / wRWhiteLevel * 0x2000));
      else
	*(lpWhiteShading + i * 3 + 0) = 0x2000;

      if (wGWhiteLevel > 0)
	*(lpWhiteShading + i * 3 + 1) =
	  (unsigned short) (((float) 65535 / wGWhiteLevel * 0x2000));
      else
	*(lpWhiteShading + i * 3 + 1) = 0x2000;

      if (wBWhiteLevel > 0)
	*(lpWhiteShading + i * 3 + 2) =
	  (unsigned short) (((float) 65535 / wBWhiteLevel * 0x2000));
      else
	*(lpWhiteShading + i * 3 + 2) = 0x2000;
    }

  free (lpWhiteData);
  free (lpDarkData);
  free (lpRWhiteSort);
  free (lpGWhiteSort);
  free (lpBWhiteSort);
  free (lpRDarkSort);
  free (lpGDarkSort);
  free (lpBDarkSort);

  Asic_SetShadingTable (&g_chip, lpWhiteShading, lpDarkShading, g_XDpi,
			wCalWidth, 0);

  free (lpWhiteShading);
  free (lpDarkShading);

  DBG (DBG_FUNC,
       "Reflective_LineCalibration16Bits: leave Reflective_LineCalibration16Bits\n");
  return TRUE;
}

/**********************************************************************
Author: Jack             Date: 2005/05/14
Routine Description: 
	Prepare scan image
Parameters:
	none
Return value: 
	if operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
Reflective_PrepareScan ()
{
  g_wScanLinesPerBlock = g_dwBufferSize / g_BytesPerRow;
  g_wMaxScanLines = g_dwImageBufferSize / g_BytesPerRow;
  g_wMaxScanLines =
    (g_wMaxScanLines / g_wScanLinesPerBlock) * g_wScanLinesPerBlock;

  g_isCanceled = FALSE;

  g_dwScannedTotalLines = 0;
  g_wReadedLines = 0;
  g_wtheReadyLines = 0;
  g_wReadImageLines = 0;

  g_wReadyShadingLine = 0;
  g_wStartShadingLinePos = 0;

  switch (g_ScanMode)
    {
    case CM_RGB48:
      g_wtheReadyLines = g_wLineDistance * 2 + g_wPixelDistance;
      DBG (DBG_FUNC, "Reflective_PrepareScan:g_wtheReadyLines=%d\n",
	   g_wtheReadyLines);

      DBG (DBG_FUNC,
	   "Reflective_PrepareScan:g_lpReadImageHead malloc %d Bytes\n",
	   g_dwImageBufferSize);
      g_lpReadImageHead = (SANE_Byte *) malloc (g_dwImageBufferSize);
      if (g_lpReadImageHead == NULL)
	{
	  DBG (DBG_FUNC,
	       "Reflective_PrepareScan: g_lpReadImageHead malloc error \n");
	  return FALSE;
	}
      break;

    case CM_RGB24ext:
      g_wtheReadyLines = g_wLineDistance * 2 + g_wPixelDistance;
      DBG (DBG_FUNC, "Reflective_PrepareScan:g_wtheReadyLines=%d\n",
	   g_wtheReadyLines);

      DBG (DBG_FUNC,
	   "Reflective_PrepareScan:g_lpReadImageHead malloc %d Bytes\n",
	   g_dwImageBufferSize);
      g_lpReadImageHead = (SANE_Byte *) malloc (g_dwImageBufferSize);
      if (g_lpReadImageHead == NULL)
	{
	  DBG (DBG_FUNC,
	       "Reflective_PrepareScan: g_lpReadImageHead malloc error \n");
	  return FALSE;
	}
      break;
    case CM_GRAY16ext:
      g_wtheReadyLines = g_wPixelDistance;
      DBG (DBG_FUNC, "Reflective_PrepareScan:g_wtheReadyLines=%d\n",
	   g_wtheReadyLines);

      DBG (DBG_FUNC,
	   "Reflective_PrepareScan:g_lpReadImageHead malloc %d Bytes\n",
	   g_dwImageBufferSize);
      g_lpReadImageHead = (SANE_Byte *) malloc (g_dwImageBufferSize);
      if (g_lpReadImageHead == NULL)
	{
	  DBG (DBG_FUNC,
	       "Reflective_PrepareScan: g_lpReadImageHead malloc error \n");
	  return FALSE;
	}
      break;
    case CM_GRAY8ext:
      g_wtheReadyLines = g_wPixelDistance;
      DBG (DBG_FUNC, "Reflective_PrepareScan:g_wtheReadyLines=%d\n",
	   g_wtheReadyLines);

      DBG (DBG_FUNC,
	   "Reflective_PrepareScan:g_lpReadImageHead malloc %d Bytes\n",
	   g_dwImageBufferSize);
      g_lpReadImageHead = (SANE_Byte *) malloc (g_dwImageBufferSize);
      if (g_lpReadImageHead == NULL)
	{
	  DBG (DBG_FUNC,
	       "Reflective_PrepareScan: g_lpReadImageHead malloc error \n");
	  return FALSE;
	}
      break;
    case CM_TEXT:
      g_wtheReadyLines = g_wPixelDistance;
      DBG (DBG_FUNC, "Reflective_PrepareScan:g_wtheReadyLines=%d\n",
	   g_wtheReadyLines);

      DBG (DBG_FUNC,
	   "Reflective_PrepareScan:g_lpReadImageHead malloc %d Bytes\n",
	   g_dwImageBufferSize);
      g_lpReadImageHead = (SANE_Byte *) malloc (g_dwImageBufferSize);
      if (g_lpReadImageHead == NULL)
	{
	  DBG (DBG_FUNC,
	       "Reflective_PrepareScan: g_lpReadImageHead malloc error \n");
	  return FALSE;
	}
      break;
    default:
      break;
    }

  Asic_ScanStart (&g_chip);
  return TRUE;
}

/**********************************************************************
Author: Jack             Date: 2005/05/15
Routine Description: 
	Get the data of image
Parameters:
	lpBlock: the data of image
	Rows: the rows of image

	isOrderInvert: the RGB order
Return value:
	if the operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
Reflective_GetRows (SANE_Byte * lpBlock, unsigned short * Rows, SANE_Bool isOrderInvert)
{
  DBG (DBG_FUNC, "Reflective_GetRows: call in \n");
  if (!g_bOpened)
    {
      DBG (DBG_FUNC, "Reflective_GetRows: scanner not opened \n");
      return FALSE;
    }
  if (!g_bPrepared)
    {
      DBG (DBG_FUNC, "Reflective_GetRows: scanner not prepared \n");
      return FALSE;
    }

  switch (g_ScanMode)
    {
    case CM_RGB48:
      if (g_XDpi == 1200)
	return MustScanner_GetRgb48BitLine1200DPI (lpBlock, isOrderInvert,
						   Rows);
      else
	return MustScanner_GetRgb48BitLine (lpBlock, isOrderInvert, Rows);

    case CM_RGB24ext:
      if (g_XDpi == 1200)
	return MustScanner_GetRgb24BitLine1200DPI (lpBlock, isOrderInvert,
						   Rows);
      else
	return MustScanner_GetRgb24BitLine (lpBlock, isOrderInvert, Rows);

    case CM_GRAY16ext:
      if (g_XDpi == 1200)
	return MustScanner_GetMono16BitLine1200DPI (lpBlock, isOrderInvert,
						    Rows);
      else
	return MustScanner_GetMono16BitLine (lpBlock, isOrderInvert, Rows);

    case CM_GRAY8ext:
      if (g_XDpi == 1200)
	return MustScanner_GetMono8BitLine1200DPI (lpBlock, isOrderInvert,
						   Rows);
      else
	return MustScanner_GetMono8BitLine (lpBlock, isOrderInvert, Rows);

    case CM_TEXT:
      if (g_XDpi == 1200)
	return MustScanner_GetMono1BitLine1200DPI (lpBlock, isOrderInvert,
						   Rows);
      else
	return MustScanner_GetMono1BitLine (lpBlock, isOrderInvert, Rows);
    default:
      return FALSE;
    }

  DBG (DBG_FUNC, "Reflective_GetRows: leave Reflective_GetRows \n");
  return FALSE;
}				/* end of the file ScannerReflective.c */
