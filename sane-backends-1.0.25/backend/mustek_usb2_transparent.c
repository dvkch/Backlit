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
static SANE_Bool Transparent_Reset (void);
static SANE_Bool Transparent_ScanSuggest (PTARGETIMAGE pTarget, PSUGGESTSETTING pSuggest);
static SANE_Bool Transparent_SetupScan (COLORMODE ColorMode, unsigned short XDpi, unsigned short YDpi,
				   SANE_Bool isInvert, unsigned short X, unsigned short Y, unsigned short Width,
				   unsigned short Height);
static SANE_Bool Transparent_StopScan (void);
static SANE_Bool Transparent_GetRows (SANE_Byte * lpBlock, unsigned short * Rows, SANE_Bool isOrderInvert);
static SANE_Bool Transparent_AdjustAD (void);
static SANE_Bool Transparent_FindTopLeft (unsigned short * lpwStartX, unsigned short * lpwStartY);
static SANE_Bool Transparent_LineCalibration16Bits (unsigned short wTAShadingMinus);
static SANE_Bool Transparent_PrepareScan (void);


/*function description*/

/**********************************************************************
Author: Jack            Date: 2005/05/13
Routine Description: 
	reset the scanner
Parameters:
	none
Return value: 
	if operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
Transparent_Reset ()
{
  DBG (DBG_FUNC, "Transparent_Reset: call in\n");

  if (g_bOpened)
    {
      DBG (DBG_FUNC, "Transparent_Reset: scanner has been opened\n");
      return FALSE;
    }
  if (STATUS_GOOD != Asic_Open (&g_chip, g_pDeviceFile))
    {
      DBG (DBG_FUNC, "Transparent_Reset: can not open scanner\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_Reset (&g_chip))
    {
      DBG (DBG_FUNC, "Reflective_Reset: Asic_Reset return error\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_SetSource (&g_chip, LS_POSITIVE))
    {
      DBG (DBG_FUNC, "Reflective_Reset: Asic_SetSource return error\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_TurnLamp (&g_chip, FALSE))
    {
      DBG (DBG_FUNC, "Reflective_Reset: Asic_TurnLamp return error\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_TurnTA (&g_chip, TRUE))
    {
      DBG (DBG_FUNC, "Reflective_Reset: Asic_TurnTA return error\n");
      return FALSE;
    }

  if (STATUS_GOOD != Asic_Close (&g_chip))
    {
      DBG (DBG_FUNC, "Reflective_Reset: Asic_Close return error\n");
      return FALSE;
    }

  g_Y = 0;
  g_wLineartThreshold = 128;
  g_dwTotalTotalXferLines = 0;
  g_bFirstReadImage = TRUE;
  g_pGammaTable = NULL;

  DBG (DBG_FUNC, "Transparent_Reset: leave Transparent_Reset\n");
  return TRUE;
}

/**********************************************************************
Author: Jack            Date: 2005/05/13
Routine Description: 
	get suggest parameter of scaning
Parameters:
	pTarget: the information of scaning
	pSuggest: the suggest parameter of scaning
Return value:
	if operation is success
	return TRUE
	else
	return FALSE
***********************************************************************/
static SANE_Bool
Transparent_ScanSuggest (PTARGETIMAGE pTarget, PSUGGESTSETTING pSuggest)
{
  unsigned short wMaxWidth, wMaxHeight;
  int i;

  DBG (DBG_FUNC, "Transparent_ScanSuggest: call in\n");

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

  if (pTarget->cmColorMode == CM_TEXT)
    {
      pSuggest->wWidth = ((pSuggest->wWidth + 7) >> 3) << 3;
      if (pSuggest->wWidth < 8)
	pSuggest->wWidth = 8;
    }

  g_Width = ((pSuggest->wWidth + 15) >> 4) << 4;	/* Real Scan Width */
  g_Height = pSuggest->wHeight;

  wMaxWidth = (MAX_SCANNING_WIDTH * pSuggest->wXDpi) / 300;
  wMaxHeight = (MAX_SCANNING_HEIGHT * pSuggest->wYDpi) / 300;

  if (pTarget->cmColorMode == CM_TEXT)
    wMaxWidth = (wMaxWidth >> 3) << 3;

  if (pSuggest->wWidth > wMaxWidth)
    pSuggest->wWidth = wMaxWidth;
  if (pSuggest->wHeight > wMaxHeight)
    pSuggest->wHeight = wMaxHeight;

  if (pTarget->isOptimalSpeed)
    {
      DBG (DBG_FUNC, "Transparent_ScanSuggest: isOptimalSpeed  is true\n");

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
      DBG (DBG_FUNC, "Transparent_ScanSuggest: isOptimalSpeed  not true\n");

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

  DBG (DBG_FUNC, "Transparent_ScanSuggest: leave Transparent_ScanSuggest\n");
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
Transparent_SetupScan (COLORMODE ColorMode, unsigned short XDpi, unsigned short YDpi,
		       SANE_Bool isInvert, unsigned short X, unsigned short Y, unsigned short Width, unsigned short Height)
{
  SANE_Bool hasTA;
  unsigned short wTAShadingMinus = 0;

  isInvert = isInvert;
  DBG (DBG_FUNC, "Transparent_SetupScan: call in\n");

  if (g_bOpened)
    {
      DBG (DBG_FUNC, "Transparent_SetupScan: scanner has been opened\n");
      return FALSE;
    }

  if (!g_bPrepared)
    {
      DBG (DBG_FUNC, "Transparent_SetupScan: scanner not prepared\n");
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
      g_wPixelDistance = 4;
      g_wLineDistance = 24;
      g_Height += g_wPixelDistance;
      break;
    case 600:
      g_wPixelDistance = 0;
      g_wLineDistance = 12;
      g_Height += g_wPixelDistance;
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

  DBG (DBG_FUNC, "Transparent_SetupScan: g_YDpi=%d\n", g_YDpi);
  DBG (DBG_FUNC, "Transparent_SetupScan: g_wLineDistance=%d\n",
       g_wLineDistance);
  DBG (DBG_FUNC, "Transparent_SetupScan: g_wPixelDistance=%d\n",
       g_wPixelDistance);

  switch (g_ScanMode)
    {
    case CM_RGB48:
      g_BytesPerRow = 6 * g_Width;	/* ASIC limit : width must be 8x */
      g_SWBytesPerRow = 6 * g_SWWidth;	/* ASIC limit : width must be 8x */
      g_bScanBits = 48;
      g_Height += g_wLineDistance * 2;
      break;
    case CM_RGB24ext:
      g_BytesPerRow = 3 * g_Width;	/*ASIC limit : width must be 8x */
      g_SWBytesPerRow = 3 * g_SWWidth;
      g_bScanBits = 24;
      g_Height += g_wLineDistance * 2;
      break;
    case CM_GRAY16ext:
      g_BytesPerRow = 2 * g_Width;	/* ASIC limit : width must be 8x */
      g_SWBytesPerRow = 2 * g_SWWidth;
      g_bScanBits = 16;
      break;
    case CM_GRAY8ext:
    case CM_TEXT:
      g_BytesPerRow = g_Width;	/*ASIC limit : width must be 8x */
      g_SWBytesPerRow = g_SWWidth;
      g_bScanBits = 8;
      break;
    default:
      break;
    }

  if (Asic_Open (&g_chip, g_pDeviceFile) != STATUS_GOOD)
    {
      DBG (DBG_FUNC, "Transparent_SetupScan: Asic_Open return error\n");
      return FALSE;
    }

  g_bOpened = TRUE;

  if (STATUS_GOOD != Asic_TurnLamp (&g_chip, FALSE))
    {
      DBG (DBG_FUNC, "Transparent_SetupScan: Asic_TurnLamp return error\n");
      return FALSE;
    }

  if (Asic_IsTAConnected (&g_chip, &hasTA) != STATUS_GOOD)
    {
      DBG (DBG_FUNC,
	   "Transparent_SetupScan: Asic_IsTAConnected return error\n");
      return FALSE;
    }
  if (!hasTA)
    {
      DBG (DBG_FUNC, "Transparent_SetupScan: no TA device\n");
      return FALSE;
    }

  if (Asic_TurnTA (&g_chip, TRUE) != STATUS_GOOD)
    {
      DBG (DBG_FUNC, "Transparent_SetupScan: Asic_TurnTA return error\n");
      return FALSE;
    }

  /* Begin Find Left&Top Side */
  Asic_MotorMove (&g_chip, TRUE, TRAN_START_POS);

  if (1200 == g_XDpi)
    {
      wTAShadingMinus = 1680;
      g_XDpi = 600;
      Transparent_AdjustAD ();
      Transparent_FindTopLeft (&g_X, &g_Y);

      g_XDpi = 1200;
      Transparent_AdjustAD ();
    }
  else
    {
      wTAShadingMinus = 840;
      Transparent_AdjustAD ();
      Transparent_FindTopLeft (&g_X, &g_Y);
    }

  DBG (DBG_FUNC,
       "Transparent_SetupScan: after find top and left g_X=%d, g_Y=%d\n", g_X,
       g_Y);

  if (1200 == g_XDpi)
    {
      g_X =
	g_X * 1200 / FIND_LEFT_TOP_CALIBRATE_RESOLUTION + X * 1200 / g_XDpi;
    }
  else
    {
      if (75 == g_XDpi)
	{
	  g_X = g_X + X * 600 / g_XDpi - 23;
	}
      else
	{
	  g_X = g_X + X * 600 / g_XDpi;
	}
    }

  DBG (DBG_FUNC,
       "Transparent_SetupScan: before line calibration,g_X=%d,g_Y=%d\n", g_X,
       g_Y);

  Transparent_LineCalibration16Bits (wTAShadingMinus);

  DBG (DBG_FUNC,
       "Transparent_SetupScan: after Reflective_LineCalibration16Bits,g_X=%d,g_Y=%d\n",
       g_X, g_Y);

  DBG (DBG_FUNC,
       "Transparent_SetupScan: g_bScanBits=%d, g_XDpi=%d, g_YDpi=%d, g_X=%d, g_Y=%d, g_Width=%d, g_Height=%d\n",
       g_bScanBits, g_XDpi, g_YDpi, g_X, g_Y, g_Width, g_Height);

  g_Y = Y * 1200 / g_YDpi + (300 - 40) + 189;
  Asic_MotorMove (&g_chip, TRUE, g_Y - 360);
  g_Y = 360;

  Asic_SetWindow (&g_chip, g_bScanBits, g_XDpi, g_YDpi, g_X, g_Y, g_Width,
		  g_Height);

  DBG (DBG_FUNC, "Transparent_SetupScan: leave Transparent_SetupScan\n");
  return Transparent_PrepareScan ();
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
Transparent_StopScan ()
{
  DBG (DBG_FUNC, "Transparent_StopScan: call in\n");

  if (!g_bOpened)
    {
      return FALSE;
    }
  if (!g_bPrepared)
    {
      return FALSE;
    }

  g_isCanceled = TRUE;

  pthread_cancel (g_threadid_readimage);
  pthread_join (g_threadid_readimage, NULL);

  DBG (DBG_FUNC, "Transparent_StopScan: thread exit\n");

  Asic_ScanStop (&g_chip);
  Asic_Close (&g_chip);
  g_bOpened = FALSE;

  DBG (DBG_FUNC, "Transparent_StopScan: leave Transparent_StopScan\n");
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
Transparent_GetRows (SANE_Byte * lpBlock, unsigned short * Rows, SANE_Bool isOrderInvert)
{
  DBG (DBG_FUNC, "Transparent_GetRows: call in\n");

  if (!g_bOpened)
    {
      return FALSE;
    }
  if (!g_bPrepared)
    {
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
  return FALSE;
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
Transparent_AdjustAD ()
{
  SANE_Byte * lpCalData;
  unsigned short wCalWidth;
  int nTimesOfCal;
  unsigned short wMaxValueR, wMinValueR, wMaxValueG, wMinValueG, wMaxValueB, wMinValueB;
#if 0
  float fRFactor = 1.0;
  float fGFactor = 1.0;
  float fBFactor = 1.0;
  SANE_Byte bDarkMaxLevel;
  SANE_Byte bDarkMinLevel;
  SANE_Byte bLastMinR, bLastROffset, bROffsetUpperBound = 255, bROffsetLowerBound =
    0;
  SANE_Byte bLastMinG, bLastGOffset, bGOffsetUpperBound = 255, bGOffsetLowerBound =
    0;
  SANE_Byte bLastMinB, bLastBOffset, bBOffsetUpperBound = 255, bBOffsetLowerBound =
    0;
#endif
  unsigned short wAdjustADResolution;

  DBG (DBG_FUNC, "Transparent_AdjustAD: call in\n");
  if (!g_bOpened)
    {
      return FALSE;
    }
  if (!g_bPrepared)
    {
      return FALSE;
    }


  g_chip.AD.DirectionR = R_DIRECTION;
  g_chip.AD.DirectionG = G_DIRECTION;
  g_chip.AD.DirectionB = B_DIRECTION;
  g_chip.AD.GainR = R_GAIN;
  g_chip.AD.GainG = G_GAIN;
  g_chip.AD.GainB = B_GAIN;
  g_chip.AD.OffsetR = 159;
  g_chip.AD.OffsetG = 50;
  g_chip.AD.OffsetB = 45;

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
      DBG (DBG_FUNC,
	   "Transparent_AdjustAD: Leave Transparent_AdjustAD for malloc fail!\n");
      return FALSE;
    }
  memset (lpBuf, 0, 50);
  stream = fopen ("/root/AD(Tra).pnm", "wb+\n");
  sprintf (lpBuf, "P6\n%d %d\n255\n", wCalWidth, 3);
  fwrite (lpBuf, sizeof (SANE_Byte), strlen (lpBuf), stream);
  fwrite (lpCalData, sizeof (SANE_Byte), wCalWidth * 3, stream);
  fclose (stream);
  free (lpBuf);
#endif

  do
    {
      DBG (DBG_FUNC,
	   "Transparent_AdjustAD: run in first adjust offset do-while\n");
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

      DBG (DBG_FUNC, "Transparent_AdjustAD: "
	   "RGain=%d, ROffset=%d, RDir=%d  GGain=%d, GOffset=%d, GDir=%d  BGain=%d, BOffset=%d, BDir=%d\n",
	   g_chip.AD.GainR, g_chip.AD.OffsetR, g_chip.AD.DirectionR,
	   g_chip.AD.GainG, g_chip.AD.OffsetG, g_chip.AD.DirectionG,
	   g_chip.AD.GainB, g_chip.AD.OffsetB, g_chip.AD.DirectionB);

      DBG (DBG_FUNC, "Transparent_AdjustAD: "
	   "MaxR=%d, MinR=%d  MaxG=%d, MinG=%d  MaxB=%d, MinB=%d\n",
	   wMaxValueR, wMinValueR, wMaxValueG, wMinValueG, wMaxValueB,
	   wMinValueB);

      /*R Channel */
      if ((wMaxValueR - wMinValueR) > TRAN_MAX_LEVEL_RANGE)
	{
	  if (g_chip.AD.GainR > 0)
	    g_chip.AD.GainR--;
	}
      else
	{
	  if ((wMaxValueR - wMinValueR) < TRAN_MIN_LEVEL_RANGE)
	    {
	      if (wMaxValueR < TRAN_WHITE_MIN_LEVEL)
		{
		  g_chip.AD.GainR++;
		  if (g_chip.AD.GainR > 63)
		    g_chip.AD.GainR = 63;
		}
	      else
		{
		  if (wMaxValueR > TRAN_WHITE_MAX_LEVEL)
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
	      if (wMaxValueR > TRAN_WHITE_MAX_LEVEL)
		{
		  if (g_chip.AD.GainR < 1)
		    g_chip.AD.GainR = 0;
		  else
		    g_chip.AD.GainR--;
		}

	      if (wMaxValueR < TRAN_WHITE_MIN_LEVEL)
		{
		  if (g_chip.AD.GainR > 63)
		    g_chip.AD.GainR = 63;
		  else
		    g_chip.AD.GainR++;

		}
	    }
	}

      /*G Channel */
      if ((wMaxValueG - wMinValueG) > TRAN_MAX_LEVEL_RANGE)
	{
	  if (g_chip.AD.GainG > 0)
	    g_chip.AD.GainG--;
	}
      else
	{
	  if ((wMaxValueG - wMinValueG) < TRAN_MIN_LEVEL_RANGE)
	    {
	      if (wMaxValueG < TRAN_WHITE_MIN_LEVEL)
		{
		  g_chip.AD.GainG++;
		  if (g_chip.AD.GainG > 63)
		    g_chip.AD.GainG = 63;
		}
	      else
		{
		  if (wMaxValueG > TRAN_WHITE_MAX_LEVEL)
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
	      if (wMaxValueG > TRAN_WHITE_MAX_LEVEL)
		{
		  if (g_chip.AD.GainG < 1)
		    g_chip.AD.GainG = 0;
		  else
		    g_chip.AD.GainG--;
		}

	      if (wMaxValueG < TRAN_WHITE_MIN_LEVEL)
		{
		  if (g_chip.AD.GainG > 63)
		    g_chip.AD.GainG = 63;
		  else
		    g_chip.AD.GainG++;
		}
	    }
	}

      /* B Channel */
      if ((wMaxValueB - wMinValueB) > TRAN_MAX_LEVEL_RANGE)
	{
	  if (g_chip.AD.GainB > 0)
	    g_chip.AD.GainB--;
	}
      else
	{
	  if ((wMaxValueB - wMinValueB) < TRAN_MIN_LEVEL_RANGE)
	    {
	      if (wMaxValueB < TRAN_WHITE_MIN_LEVEL)
		{
		  g_chip.AD.GainB++;
		  if (g_chip.AD.GainB > 63)
		    g_chip.AD.GainB = 63;
		}
	      else
		{
		  if (wMaxValueB > TRAN_WHITE_MAX_LEVEL)
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
	      if (wMaxValueB > TRAN_WHITE_MAX_LEVEL)
		{
		  if (g_chip.AD.GainB < 1)
		    g_chip.AD.GainB = 0;
		  else
		    g_chip.AD.GainB--;
		}

	      if (wMaxValueB < TRAN_WHITE_MIN_LEVEL)
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
  while ((wMaxValueR - wMinValueR) > TRAN_MAX_LEVEL_RANGE
	 || (wMaxValueR - wMinValueR) < TRAN_MIN_LEVEL_RANGE
	 || (wMaxValueG - wMinValueG) > TRAN_MAX_LEVEL_RANGE
	 || (wMaxValueG - wMinValueG) < TRAN_MIN_LEVEL_RANGE
	 || (wMaxValueB - wMinValueB) > TRAN_MAX_LEVEL_RANGE
	 || (wMaxValueB - wMinValueB) < TRAN_MIN_LEVEL_RANGE);

  /* Adjust Offset 2nd */
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
      DBG (DBG_FUNC,
	   "Transparent_AdjustAD: "
	   "RGain=%d, ROffset=%d, RDir=%d  GGain=%d, GOffset=%d, GDir=%d  BGain=%d, BOffset=%d, BDir=%d\n",
	   g_chip.AD.GainR, g_chip.AD.OffsetR, g_chip.AD.DirectionR,
	   g_chip.AD.GainG, g_chip.AD.OffsetG, g_chip.AD.DirectionG,
	   g_chip.AD.GainB, g_chip.AD.OffsetB, g_chip.AD.DirectionB);

      DBG (DBG_FUNC, "Transparent_AdjustAD: "
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

  DBG (DBG_FUNC, "Transparent_AdjustAD: leave Transparent_AdjustAD\n");
  free (lpCalData);

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
Transparent_FindTopLeft (unsigned short * lpwStartX, unsigned short * lpwStartY)
{
  unsigned short wCalWidth = TA_FIND_LEFT_TOP_WIDTH_IN_DIP;
  unsigned short wCalHeight = TA_FIND_LEFT_TOP_HEIGHT_IN_DIP;

  int i, j;
  unsigned short wLeftSide;
  unsigned short wTopSide;
  int nScanBlock;
  SANE_Byte * lpCalData;
  unsigned int dwTotalSize;
  unsigned short wXResolution, wYResolution;

  DBG (DBG_FUNC, "Transparent_FindTopLeft: call in\n");
  if (!g_bOpened)
    {
      DBG (DBG_FUNC, "Transparent_FindTopLeft: scanner not opened\n");

      return FALSE;
    }
  if (!g_bPrepared)
    {
      DBG (DBG_FUNC, "Transparent_FindTopLeft: scanner not prepared\n");
      return FALSE;
    }

  wXResolution = wYResolution = FIND_LEFT_TOP_CALIBRATE_RESOLUTION;


  lpCalData = (SANE_Byte *) malloc (sizeof (SANE_Byte) * wCalWidth * wCalHeight);
  if (lpCalData == NULL)
    {
      DBG (DBG_FUNC, "Transparent_FindTopLeft: lpCalData malloc fail\n");
      return FALSE;
    }

  dwTotalSize = wCalWidth * wCalHeight;
  nScanBlock = (int) (dwTotalSize / g_dwCalibrationSize);

  Asic_SetMotorType (&g_chip, TRUE, TRUE);
  Asic_SetCalibrate (&g_chip, 8, wXResolution, wYResolution, 0, 0, wCalWidth,
		     wCalHeight, FALSE);
  Asic_SetAFEGainOffset (&g_chip);
  Asic_ScanStart (&g_chip);

  for (i = 0; i < nScanBlock; i++)
    Asic_ReadCalibrationData (&g_chip, lpCalData + i * g_dwCalibrationSize,
			      g_dwCalibrationSize, 8);

  Asic_ReadCalibrationData (&g_chip,
			    lpCalData + (nScanBlock) * g_dwCalibrationSize,
			    (dwTotalSize - g_dwCalibrationSize * nScanBlock),
			    8);
  Asic_ScanStop (&g_chip);


#ifdef DEBUG_SAVE_IMAGE
  FILE *stream = NULL;
  SANE_Byte * lpBuf = (SANE_Byte *) malloc (50);
  if (NULL == lpBuf)
    {
      return FALSE;
    }
  memset (lpBuf, 0, 50);
  stream = fopen ("/root/bound(Tra).pnm", "wb+\n");
  sprintf (lpBuf, "P5\n%d %d\n255\n", wCalWidth, wCalHeight);
  fwrite (lpBuf, sizeof (SANE_Byte), strlen (lpBuf), stream);
  fwrite (lpCalData, sizeof (SANE_Byte), wCalWidth * wCalHeight, stream);
  fclose (stream);
  free (lpBuf);
#endif

  wLeftSide = 0;
  wTopSide = 0;

  /* Find Left Side */
  for (i = (wCalWidth - 1); i > 0; i--)
    {
      wLeftSide = *(lpCalData + i);
      wLeftSide += *(lpCalData + wCalWidth * 2 + i);
      wLeftSide += *(lpCalData + wCalWidth * 4 + i);
      wLeftSide += *(lpCalData + wCalWidth * 6 + i);
      wLeftSide += *(lpCalData + wCalWidth * 8 + i);
      wLeftSide /= 5;
      if (wLeftSide < 60)
	{
	  if (i == (wCalWidth - 1))
	    {
	      break;
	    }
	  *lpwStartX = i;
	  break;
	}
    }

  /* Find Top Side i=left side */
  for (j = 0; j < wCalHeight; j++)
    {
      wTopSide = *(lpCalData + wCalWidth * j + i + 2);
      wTopSide += *(lpCalData + wCalWidth * j + i + 4);
      wTopSide += *(lpCalData + wCalWidth * j + i + 6);
      wTopSide += *(lpCalData + wCalWidth * j + i + 8);
      wTopSide += *(lpCalData + wCalWidth * j + i + 10);
      wTopSide /= 5;
      if (wTopSide < 60)
	{
	  if (j == 0)
	    {
	      break;
	    }
	  *lpwStartY = j;
	  break;
	}
    }

  if ((*lpwStartX < 2200) || (*lpwStartX > 2300))
    {
      *lpwStartX = 2260;
    }

  if ((*lpwStartY < 100) || (*lpwStartY > 200))
    {
      *lpwStartY = 124;
    }


  Asic_MotorMove (&g_chip, FALSE,
		  (wCalHeight - *lpwStartY) * 1200 / wYResolution + 300);

  free (lpCalData);

  DBG (DBG_FUNC,
       "Transparent_FindTopLeft: *lpwStartY = %d, *lpwStartX = %d\n",
       *lpwStartY, *lpwStartX);
  DBG (DBG_FUNC, "Transparent_FindTopLeft: leave Transparent_FindTopLeft\n");
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
Transparent_LineCalibration16Bits (unsigned short wTAShadingMinus)
{

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

  SANE_Byte * lpWhiteData;
  SANE_Byte * lpDarkData;
  unsigned int dwWhiteTotalSize;
  unsigned int dwDarkTotalSize;
  unsigned short wCalHeight = LINE_CALIBRATION__16BITS_HEIGHT;
  unsigned short wCalWidth = g_Width;

  DBG (DBG_FUNC, "Transparent_LineCalibration16Bits: call in\n");
  if (!g_bOpened)
    {
      DBG (DBG_FUNC,
	   "Transparent_LineCalibration16Bits: scanner not opened\n");
      return FALSE;
    }
  if (!g_bPrepared)
    {
      DBG (DBG_FUNC,
	   "Transparent_LineCalibration16Bits: scanner not prepared\n");
      return FALSE;
    }

  if (g_XDpi < 600)
    {
      wTAShadingMinus = wTAShadingMinus * g_XDpi / 600;
    }

  dwWhiteTotalSize = wCalWidth * wCalHeight * 3 * 2;
  dwDarkTotalSize = wCalWidth * wCalHeight * 3 * 2;
  lpWhiteData = (SANE_Byte *) malloc (sizeof (SANE_Byte) * dwWhiteTotalSize);
  lpDarkData = (SANE_Byte *) malloc (sizeof (SANE_Byte) * dwDarkTotalSize);
  if (lpWhiteData == NULL || lpDarkData == NULL)
    {
      DBG (DBG_FUNC,
	   "Transparent_LineCalibration16Bits: lpWhiteData or lpDarkData malloc fail\n");
      return FALSE;
    }

  /*Read white level data */
  Asic_SetMotorType (&g_chip, TRUE, TRUE);
  Asic_SetAFEGainOffset (&g_chip);
  Asic_SetCalibrate (&g_chip, 48, g_XDpi, g_YDpi, g_X, 0, wCalWidth,
		     wCalHeight, TRUE);
  Asic_ScanStart (&g_chip);

  /* Read Data */
  Asic_ReadCalibrationData (&g_chip, lpWhiteData, dwWhiteTotalSize, 8);
  Asic_ScanStop (&g_chip);


  /* Read dark level data */
  Asic_SetMotorType (&g_chip, FALSE, TRUE);
  Asic_SetAFEGainOffset (&g_chip);
  Asic_SetCalibrate (&g_chip, 48, g_XDpi, g_YDpi, g_X, 0, wCalWidth,
		     wCalHeight, TRUE);

  Asic_TurnLamp (&g_chip, FALSE);
  Asic_TurnTA (&g_chip, FALSE);

  usleep (500000);

  Asic_ScanStart (&g_chip);
  Asic_ReadCalibrationData (&g_chip, lpDarkData, dwDarkTotalSize, 8);

  Asic_ScanStop (&g_chip);

  Asic_TurnTA (&g_chip, TRUE);

#ifdef DEBUG_SAVE_IMAGE
  FILE *stream = NULL;
  SANE_Byte * lpBuf = (SANE_Byte *) malloc (50);
  if (NULL == lpBuf)
    {
      return FALSE;
    }
  memset (lpBuf, 0, 50);
  stream = fopen ("/root/whiteshading(Tra).pnm", "wb+\n");
  sprintf (lpBuf, "P6\n%d %d\n65535\n", wCalWidth, wCalHeight);
  fwrite (lpBuf, sizeof (SANE_Byte), strlen (lpBuf), stream);
  fwrite (lpWhiteData, sizeof (SANE_Byte), wCalWidth * wCalHeight * 3 * 2, stream);
  fclose (stream);

  memset (lpBuf, 0, 50);
  stream = fopen ("/root/darkshading(Tra).pnm", "wb+\n");
  sprintf (lpBuf, "P6\n%d %d\n65535\n", wCalWidth * wCalHeight);
  fwrite (lpBuf, sizeof (SANE_Byte), strlen (lpBuf), stream);
  fwrite (lpDarkData, sizeof (SANE_Byte), wCalWidth * wCalHeight * 3 * 2, stream);
  fclose (stream);
  free (lpBuf);
#endif

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
      DBG (DBG_FUNC, "Transparent_LineCalibration16Bits: malloc fail\n");

      free (lpWhiteData);
      free (lpDarkData);
      return FALSE;
    }

  DBG (DBG_FUNC,
       "Transparent_LineCalibration16Bits: wCalWidth = %d, wCalHeight = %d\n",
       wCalWidth, wCalHeight);

  /* create dark level shading */
  dwRDarkLevel = 0;
  dwGDarkLevel = 0;
  dwBDarkLevel = 0;
  dwREvenDarkLevel = 0;
  dwGEvenDarkLevel = 0;
  dwBEvenDarkLevel = 0;

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

      /* sum of dark level for all pixels */
      if (g_XDpi == 1200)
	{
	  /* do dark shading table with mean */
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
      dwRDarkLevel = (unsigned int) (dwRDarkLevel / (wCalWidth / 2)) - 512;
      dwGDarkLevel = (unsigned int) (dwGDarkLevel / (wCalWidth / 2)) - 512;
      dwBDarkLevel = (unsigned int) (dwBDarkLevel / (wCalWidth / 2)) - 512;

      dwREvenDarkLevel = (unsigned int) (dwREvenDarkLevel / (wCalWidth / 2)) - 512;
      dwGEvenDarkLevel = (unsigned int) (dwGEvenDarkLevel / (wCalWidth / 2)) - 512;
      dwBEvenDarkLevel = (unsigned int) (dwBEvenDarkLevel / (wCalWidth / 2)) - 512;
    }
  else
    {
      dwRDarkLevel = (unsigned int) (dwRDarkLevel / wCalWidth) - 512;
      dwGDarkLevel = (unsigned int) (dwGDarkLevel / wCalWidth) - 512;
      dwBDarkLevel = (unsigned int) (dwBDarkLevel / wCalWidth) - 512;
    }

  /* Create white shading */
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

      if (1200 == g_XDpi)
	{
	  if (i % 2)
	    {
	      if (SS_Negative == g_ssScanSource)
		{
		  *(lpDarkShading + i * 3 + 0) = (unsigned short) dwRDarkLevel;
		  *(lpDarkShading + i * 3 + 1) = (unsigned short) dwGDarkLevel;
		  *(lpDarkShading + i * 3 + 2) = (unsigned short) dwBDarkLevel;
		}
	      else
		{
		  *(lpDarkShading + i * 3 + 0) = (unsigned short) dwRDarkLevel;
		  *(lpDarkShading + i * 3 + 1) = (unsigned short) (dwGDarkLevel * 0.78);
		  *(lpDarkShading + i * 3 + 2) = (unsigned short) dwBDarkLevel;
		}
	    }
	  else
	    {
	      if (SS_Negative == g_ssScanSource)
		{
		  *(lpDarkShading + i * 3 + 0) = (unsigned short) dwREvenDarkLevel;
		  *(lpDarkShading + i * 3 + 1) = (unsigned short) dwGEvenDarkLevel;
		  *(lpDarkShading + i * 3 + 2) = (unsigned short) dwBEvenDarkLevel;
		}
	      else
		{
		  *(lpDarkShading + i * 3 + 0) = (unsigned short) dwREvenDarkLevel;
		  *(lpDarkShading + i * 3 + 1) =
		    (unsigned short) (dwGEvenDarkLevel * 0.78);
		  *(lpDarkShading + i * 3 + 2) = (unsigned short) dwBEvenDarkLevel;
		}
	    }
	}
      else
	{
	  if (SS_Negative == g_ssScanSource)
	    {
	      *(lpDarkShading + i * 3 + 0) = (unsigned short) dwRDarkLevel;
	      *(lpDarkShading + i * 3 + 1) = (unsigned short) dwRDarkLevel;
	      *(lpDarkShading + i * 3 + 2) = (unsigned short) dwRDarkLevel;
	    }
	  else
	    {
	      *(lpDarkShading + i * 3 + 0) = (unsigned short) dwRDarkLevel;
	      *(lpDarkShading + i * 3 + 1) = (unsigned short) (dwRDarkLevel * 0.78);
	      *(lpDarkShading + i * 3 + 2) = (unsigned short) dwRDarkLevel;
	    }
	}

      /* Create white shading */
      wRWhiteLevel =
	(double) (MustScanner_FiltLower (lpRWhiteSort, wCalHeight, 20, 30) -
		  *(lpDarkShading + i * 3 + 0));
      wGWhiteLevel =
	(double) (MustScanner_FiltLower (lpGWhiteSort, wCalHeight, 20, 30) -
		  *(lpDarkShading + i * 3 + 1));
      wBWhiteLevel =
	(double) (MustScanner_FiltLower (lpBWhiteSort, wCalHeight, 20, 30) -
		  *(lpDarkShading + i * 3 + 2));

      if (g_ssScanSource == SS_Negative)
	{
	  if (wRWhiteLevel > 0)
	    *(lpWhiteShading + i * 3 + 0) =
	      (unsigned short) ((float) 65536 / wRWhiteLevel * 0x1000);
	  else
	    *(lpWhiteShading + i * 3 + 0) = 0x1000;

	  if (wGWhiteLevel > 0)
	    *(lpWhiteShading + i * 3 + 1) =
	      (unsigned short) ((float) (65536 * 1.5) / wGWhiteLevel * 0x1000);
	  else
	    *(lpWhiteShading + i * 3 + 1) = 0x1000;

	  if (wBWhiteLevel > 0)
	    *(lpWhiteShading + i * 3 + 2) =
	      (unsigned short) ((float) (65536 * 2.0) / wBWhiteLevel * 0x1000);
	  else
	    *(lpWhiteShading + i * 3 + 2) = 0x1000;
	}
      else
	{
	  if (wRWhiteLevel > 0)
	    *(lpWhiteShading + i * 3 + 0) =
	      (unsigned short) ((float) 65536 / wRWhiteLevel * 0x1000);
	  else
	    *(lpWhiteShading + i * 3 + 0) = 0x1000;

	  if (wGWhiteLevel > 0)
	    *(lpWhiteShading + i * 3 + 1) =
	      (unsigned short) ((float) (65536 * 1.04) / wGWhiteLevel * 0x1000);
	  else
	    *(lpWhiteShading + i * 3 + 1) = 0x1000;

	  if (wBWhiteLevel > 0)
	    *(lpWhiteShading + i * 3 + 2) =
	      (unsigned short) ((float) 65536 / wBWhiteLevel * 0x1000);
	  else
	    *(lpWhiteShading + i * 3 + 2) = 0x1000;
	}
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
       "Transparent_LineCalibration16Bits: leave Transparent_LineCalibration16Bits\n");
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
Transparent_PrepareScan ()
{
  DBG (DBG_FUNC, "Transparent_PrepareScan: call in\n");

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

      g_lpReadImageHead = (SANE_Byte *) malloc (g_dwImageBufferSize);
      if (g_lpReadImageHead == NULL)
	{
	  DBG (DBG_FUNC, "Transparent_PrepareScan:malloc fail\n");
	  return FALSE;
	}
      break;

    case CM_RGB24ext:
      g_wtheReadyLines = g_wLineDistance * 2 + g_wPixelDistance;
      g_lpReadImageHead = (SANE_Byte *) malloc (g_dwImageBufferSize);

      if (g_lpReadImageHead == NULL)
	{
	  DBG (DBG_FUNC, "Transparent_PrepareScan:malloc fail\n");
	  return FALSE;
	}
      break;

    case CM_GRAY16ext:
      g_wtheReadyLines = g_wPixelDistance;
      g_lpReadImageHead = (SANE_Byte *) malloc (g_dwImageBufferSize);
      if (g_lpReadImageHead == NULL)
	{
	  DBG (DBG_FUNC, "Transparent_PrepareScan:malloc fail\n");
	  return FALSE;
	}
      break;

    case CM_GRAY8ext:
      g_wtheReadyLines = g_wPixelDistance;
      g_lpReadImageHead = (SANE_Byte *) malloc (g_dwImageBufferSize);
      if (g_lpReadImageHead == NULL)
	{
	  DBG (DBG_FUNC, "Transparent_PrepareScan:malloc fail\n");
	  return FALSE;
	}
      break;

    case CM_TEXT:
      g_wtheReadyLines = g_wPixelDistance;
      g_lpReadImageHead = (SANE_Byte *) malloc (g_dwImageBufferSize);
      if (g_lpReadImageHead == NULL)
	{
	  DBG (DBG_FUNC, "Transparent_PrepareScan:malloc fail\n");
	  return FALSE;
	}
      break;
    default:
      return FALSE;
    }

  Asic_ScanStart (&g_chip);

  DBG (DBG_FUNC, "Transparent_PrepareScan: leave Transparent_PrepareScan\n");
  return TRUE;
}				/* end of the file ScannerTransparent.c */
