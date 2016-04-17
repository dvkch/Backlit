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

#include <stdio.h>		/* fopen, fread, fwrite, fclose etc */
#include <stdarg.h>		/* va_list for vfprintf */
#include <string.h>		/* memcpy, memset */
#include <unistd.h>		/* unlink */
#include <stdlib.h>		/* malloc, free */
#include <math.h>		/* exp, pow */

#include "niash_xfer.h"
#include "niash_core.h"


#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif


#define XFER_BUF_SIZE  0xF000


/* HP3400 firmware data */
static unsigned char abData0000[] = {
  0xfe, 0x9f, 0x58, 0x1b, 0x00, 0x03, 0xa4, 0x02, 0x63, 0x02, 0x33, 0x02,
  0x0d, 0x02, 0xf0, 0x01,
  0xd8, 0x01, 0xc5, 0x01, 0xb5, 0x01, 0xa8, 0x01, 0x9d, 0x01, 0x93, 0x01,
  0x8b, 0x01, 0x84, 0x01,
  0x7e, 0x01, 0x79, 0x01, 0x74, 0x01, 0x70, 0x01, 0x6d, 0x01, 0x69, 0x01,
  0x67, 0x01, 0x64, 0x01,
  0x62, 0x01, 0x60, 0x01, 0x5f, 0x01, 0x5d, 0x01, 0x5c, 0x01, 0x5b, 0x01,
  0x5a, 0x01, 0x59, 0x01,
  0x58, 0x01, 0x57, 0x01, 0x57, 0x01, 0x56, 0x01, 0x56, 0x01, 0x55, 0x01,
  0x55, 0x01, 0x54, 0x01,
  0x54, 0x01, 0x54, 0x01, 0x54, 0x01, 0x53, 0x01, 0x53, 0x01, 0x53, 0x01,
  0x53, 0x01, 0x52, 0x81
};

/*  1st word : 0x9ffe = 40958, strip 15th bit: 0x1ffe = 8190
    2nd word : 0x1b58 = 7000 -> coincidence ?
    other words: formula: y = 676 / (2 - exp(0.113 * (1-x)) ), where x = 0 for first entry
*/

/* more HP3400 firmware data */
static unsigned char abData0400[] = {
  0xa4, 0x82, 0x00, 0x80, 0xa4, 0x82, 0xaa, 0x02, 0xc0, 0x02, 0xe8, 0x02,
  0x3e, 0x03, 0xc8, 0x03,
  0x58, 0x1b, 0xfe, 0x9f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};



static void
_ConvertMotorTable (unsigned char *pabOld, unsigned char *pabNew, int iSize,
		    int iLpi)
{
  int iData, i, iBit15;

  for (i = 0; i < (iSize / 2); i++)
    {
      iData = pabOld[2 * i + 0] + (pabOld[2 * i + 1] << 8);
      iBit15 = (iData & 0x8000);
      iData = (iData & 0x7FFF);
      if (iData <= 0x400)
	{
	  iData = iData * iLpi / 300;
	}
      if (iBit15 != 0)
	{
	  iData |= 0x8000;
	}
      pabNew[2 * i + 0] = iData & 255;
      pabNew[2 * i + 1] = (iData >> 8) & 255;
    }
}


/*************************************************************************
  _ProbeRegisters
  ===============
    Tries to determine certain hardware properties.

  This is done by checking the writeability of some scanner registers.
  We cannot rely simply on the scanner model to contain a specific
  chip. The HP3300c for example uses one of at least three slightly
  different scanner ASICs (NIASH00012, NIASH00013 and NIASH00014).

  OUT pHWParams     Hardware parameters, updated fields:
        fGamma16    TRUE if 16 bit gamma tables can be used
        fReg07      TRUE if reg07 is writeable
        iBufferSize Size of scanner's internal buffer

  Returns TRUE if a NIASH chipset was found.
*************************************************************************/
static SANE_Bool
_ProbeRegisters (THWParams * pHWParams)
{
  unsigned char bData1, bData2;
  int iHandle;

  iHandle = pHWParams->iXferHandle;

  DBG (DBG_MSG, "Probing scanner...\n");

  /* check register 0x04 */
  NiashWriteReg (iHandle, 0x04, 0x55);
  NiashReadReg (iHandle, 0x04, &bData1);
  NiashWriteReg (iHandle, 0x04, 0xAA);
  NiashReadReg (iHandle, 0x04, &bData2);
  NiashWriteReg (iHandle, 0x04, 0x07);
  if ((bData1 != 0x55) || (bData2 != 0xAA))
    {
      DBG (DBG_ERR, "  No NIASH chipset found!\n");
      return SANE_FALSE;
    }

  /* check writeability of register 3 bit 1 */
  NiashReadReg (iHandle, 0x03, &bData1);
  NiashWriteReg (iHandle, 0x03, bData1 | 0x02);
  NiashReadReg (iHandle, 0x03, &bData2);
  NiashWriteReg (iHandle, 0x03, bData1);
  pHWParams->fGamma16 = ((bData2 & 0x02) != 0);
  DBG (DBG_MSG, "  Gamma table entries are %d bit\n",
       pHWParams->fGamma16 ? 16 : 8);

  /* check register 0x07 */
  NiashReadReg (iHandle, 0x07, &bData1);
  NiashWriteReg (iHandle, 0x07, 0x1C);
  NiashReadReg (iHandle, 0x07, &bData2);
  NiashWriteReg (iHandle, 0x07, bData1);
  pHWParams->fReg07 = (bData2 == 0x1C);

  if (!pHWParams->fGamma16)
    {
      /* internal scan buffer size is an educated guess, but seems to correlate
         well with the size calculated from several windows driver log files
         size = 128kB - 44088 unsigned chars (space required for gamma/calibration table)
       */
      pHWParams->iBufferSize = 86984L;
      DBG (DBG_MSG, "  NIASH version < 00014\n");
    }
  else
    {
      pHWParams->iBufferSize = 0x60000L;
      if (!pHWParams->fReg07)
	{
	  DBG (DBG_MSG, "  NIASH version = 00014\n");
	}
      else
	{
	  DBG (DBG_MSG, "  NIASH version > 00014\n");
	}
    }

  return SANE_TRUE;
}


/* returns 0 on success, < 0 otherwise */
STATIC int
NiashOpen (THWParams * pHWParams, const char *pszName)
{
  int iXferHandle;

  iXferHandle = NiashXferOpen (pszName, &pHWParams->eModel);
  if (iXferHandle < 0)
    {
      DBG (DBG_ERR, "NiashXferOpen failed for '%s'\n", pszName);
      return -1;
    }

  pHWParams->iXferHandle = iXferHandle;

  NiashWakeup (pHWParams->iXferHandle);

  /* default HW params */
  pHWParams->iSensorSkew = 8;
  pHWParams->iTopLeftX = 0;
  pHWParams->iTopLeftY = 3;
  pHWParams->fReg07 = SANE_FALSE;
  pHWParams->iSkipLines = 0;
  pHWParams->iExpTime = 5408;
  pHWParams->iReversedHead = SANE_TRUE;

  switch (pHWParams->eModel)
    {

    case eHp3300c:
      DBG (DBG_MSG, "Setting params for Hp3300\n");
      pHWParams->iTopLeftX = 4;
      pHWParams->iTopLeftY = 11;
      pHWParams->iSkipLines = 14;
      break;

    case eHp3400c:
    case eHp4300c:
      DBG (DBG_MSG, "Setting params for Hp3400c/Hp4300c\n");
      pHWParams->iTopLeftX = 3;
      pHWParams->iTopLeftY = 14;
      pHWParams->fReg07 = SANE_TRUE;
      break;

    case eAgfaTouch:
      DBG (DBG_MSG, "Setting params for AgfaTouch\n");
      pHWParams->iReversedHead = SANE_FALSE;	/* head not reversed on Agfa Touch */
      pHWParams->iTopLeftX = 3;
      pHWParams->iTopLeftY = 10;
      pHWParams->iSkipLines = 7;
      break;

    case eUnknownModel:
      DBG (DBG_MSG, "Setting params for UnknownModel\n");
      break;

    default:
      DBG (DBG_ERR, "ERROR: internal error! (%d)\n", (int) pHWParams->eModel);
      return -1;
    }

  /* autodetect some hardware properties */
  if (!_ProbeRegisters (pHWParams))
    {
      DBG (DBG_ERR, "_ProbeRegisters failed!\n");
      return -1;
    }

  return 0;
}


STATIC void
NiashClose (THWParams * pHWPar)
{
  NiashXferClose (pHWPar->iXferHandle);
  pHWPar->iXferHandle = 0;
}


static void
WriteRegWord (int iHandle, unsigned char bReg, SANE_Word wData)
{
  NiashWriteReg (iHandle, bReg, wData & 0xFF);
  NiashWriteReg (iHandle, bReg + 1, (wData >> 8) & 0xFF);
}


/* calculate a 4096 unsigned char gamma table */
STATIC void
CalcGamma (unsigned char *pabTable, double Gamma)
{
  int i, iData;

  /* fill gamma table */
  for (i = 0; i < 4096; i++)
    {
      iData = floor (256.0 * pow (((double) i / 4096.0), 1.0 / Gamma));
      pabTable[i] = iData;
    }
}


/*
  Hp3400WriteFw
  =============
    Writes data to scanners with a NIASH00019 chipset, e.g.
    gamma, calibration and motor control data.

  IN  pabData   pointer to firmware data
      iLen      Size of firmware date (unsigned chars)
      iAddr     Scanner address to write to
*/
static void
Hp3400cWriteFW (int iXferHandle, unsigned char *pabData, int iLen, int iAddr)
{
  iAddr--;
  NiashWriteReg (iXferHandle, 0x21, iAddr & 0xFF);
  NiashWriteReg (iXferHandle, 0x22, (iAddr >> 8) & 0xFF);
  NiashWriteReg (iXferHandle, 0x23, (iAddr >> 16) & 0xFF);
  NiashWriteBulk (iXferHandle, pabData, iLen);
}


/* Writes the gamma and offset/gain tables to the scanner.
   In case a calibration file exist, it will be used for offset/gain */
STATIC void
WriteGammaCalibTable (unsigned char *pabGammaR, unsigned char *pabGammaG,
		      unsigned char *pabGammaB, unsigned char *pabCalibTable,
		      int iGain, int iOffset, THWParams * pHWPar)
{
  int i, j, k;
  static unsigned char abGamma[60000];
  int iData;
  int iHandle;

  iHandle = pHWPar->iXferHandle;

  j = 0;
  /* fill gamma table for red component */
  /* pad entries with 0 for 16-bit gamma table */
  for (i = 0; i < 4096; i++)
    {
      if (pHWPar->fGamma16)
	{
	  abGamma[j++] = 0;
	}
      abGamma[j++] = pabGammaR[i];
    }
  /* fill gamma table for green component */
  for (i = 0; i < 4096; i++)
    {
      if (pHWPar->fGamma16)
	{
	  abGamma[j++] = 0;
	}
      abGamma[j++] = pabGammaG[i];
    }
  /* fill gamma table for blue component */
  for (i = 0; i < 4096; i++)
    {
      if (pHWPar->fGamma16)
	{
	  abGamma[j++] = 0;
	}
      abGamma[j++] = pabGammaB[i];
    }

  if (pabCalibTable == NULL)
    {
      iData = (iGain << 6) + iOffset;
      for (i = 0; i < HW_PIXELS; i++)
	{
	  for (k = 0; k < 3; k++)
	    {
	      abGamma[j++] = (iData) & 255;
	      abGamma[j++] = (iData >> 8) & 255;
	    }
	}
    }
  else
    {
      memcpy (&abGamma[j], pabCalibTable, HW_PIXELS * 6);
      j += HW_PIXELS * 6;
    }

  NiashWriteReg (iHandle, 0x02, 0x80);
  NiashWriteReg (iHandle, 0x03, 0x01);
  NiashWriteReg (iHandle, 0x03, 0x11);
  NiashWriteReg (iHandle, 0x02, 0x84);

  if (pHWPar->fReg07)
    {
      Hp3400cWriteFW (iHandle, abGamma, j, 0x2000);
    }
  else
    {
      NiashWriteBulk (iHandle, abGamma, j);
    }

  NiashWriteReg (iHandle, 0x02, 0x80);
}


static void
WriteAFEReg (int iHandle, int iReg, int iData)
{
  NiashWriteReg (iHandle, 0x25, iReg);
  NiashWriteReg (iHandle, 0x26, iData);
}


/* setup the analog front-end -> coarse calibration */
static void
WriteAFE (int iHandle)
{
  /* see WM8143 datasheet */

  WriteAFEReg (iHandle, 0x04, 0x00);
  WriteAFEReg (iHandle, 0x03, 0x12);
  WriteAFEReg (iHandle, 0x02, 0x04);
  WriteAFEReg (iHandle, 0x05, 0x10);
  WriteAFEReg (iHandle, 0x01, 0x03);

  WriteAFEReg (iHandle, 0x20, 0xc0);	/*c8 *//* red offset */
  WriteAFEReg (iHandle, 0x21, 0xc0);	/*c8 *//* green offset */
  WriteAFEReg (iHandle, 0x22, 0xc0);	/*d0 *//* blue offset */

  WriteAFEReg (iHandle, 0x28, 0x05);	/*5 *//* red gain */
  WriteAFEReg (iHandle, 0x29, 0x03);	/*3 *//* green gain */
  WriteAFEReg (iHandle, 0x2A, 0x04);	/*4 *//* blue gain */
}


/* wait for the carriage to return */
static void
WaitReadyBit (int iHandle)
{
  unsigned char bData;

  do
    {
      NiashReadReg (iHandle, 0x03, &bData);
    }
  while ((bData & 8) == 0);
}


/*
  Initialisation specific for NIASH00014 and lower chips
*/
static void
InitNiash00014 (TScanParams * pParams, THWParams * pHWParams)
{
  int iHandle, iLpiCode;

  iHandle = pHWParams->iXferHandle;

  /* exposure time (in units 24/Fcrystal)? */
  WriteRegWord (iHandle, 0x08, pHWParams->iExpTime - 1);

  /* width in pixels */
  WriteRegWord (iHandle, 0x12, pParams->iWidth - 1);

  /* top */
  WriteRegWord (iHandle, 0x17, pParams->iTop);
  WriteRegWord (iHandle, 0x19, pParams->iTop);

  /* time between stepper motor steps (in units of 24/Fcrystal)? */
  iLpiCode = pParams->iLpi * pHWParams->iExpTime / 1200L;

  if (!pHWParams->fGamma16)
    {
      /* NIASH 00012 / 00013 init */

      /* LPI specific settings */
      if (pParams->iLpi < 600)
	{
	  /* set halfres bit */
	  NiashWriteReg (iHandle, 0x06, 0x01);
	  /* double lpi code because of halfres bit */
	  iLpiCode *= 2;
	}
      else
	{
	  /* clear halfres bit */
	  NiashWriteReg (iHandle, 0x06, 0x00);
	  /* add exptime to make it scan slower */
	  iLpiCode += pHWParams->iExpTime;
	}

      /* unknown setting */
      WriteRegWord (iHandle, 0x27, 0x7FD2);
      WriteRegWord (iHandle, 0x29, 0x6421);

    }
  else
    {
      /* NIASH 00014 init */

      /* halfres bit always cleared */
      NiashWriteReg (iHandle, 0x06, 0x00);

      /* LPI specific settings */
      if (pParams->iLpi >= 600)
	{
	  /* add exptime to make it scan slower */
	  iLpiCode += pHWParams->iExpTime;
	}

      /* unknown setting */
      WriteRegWord (iHandle, 0x27, 0xc862);	/*c862 */
      WriteRegWord (iHandle, 0x29, 0xb853);	/*b853 */
    }

  /* LPI code */
  WriteRegWord (iHandle, 0x0A, iLpiCode - 1);

  /* backtrack reversing speed */
  NiashWriteReg (iHandle, 0x1E, (iLpiCode - 1) / 32);
}


/*
  Initialisation specific for NIASH00019 chips
*/
static void
InitNiash00019 (TScanParams * pParams, THWParams * pHWParams)
{
  int iHandle, iLpiCode;
  static unsigned char abMotor[512];


  iHandle = pHWParams->iXferHandle;

  /* exposure time (in units 24/Fcrystal)? */
  WriteRegWord (iHandle, 0x08, pHWParams->iExpTime);

  /* width in pixels */
  WriteRegWord (iHandle, 0x12, pParams->iWidth);

  /* ? */
  WriteRegWord (iHandle, 0x27, 0xc862);	/*c862 */
  WriteRegWord (iHandle, 0x29, 0xb853);	/*b853 */

  /* specific handling of 150 dpi resolution */
  if (pParams->iLpi == 150)
    {
      /* use 300 LPI but skip every other line */
      pParams->iLpi = 300;
      NiashWriteReg (iHandle, 0x06, 0x01);
    }
  else
    {
      NiashWriteReg (iHandle, 0x06, 0x00);
    }

  /* DPI and position table */
  NiashWriteReg (iHandle, 0x07, 0x02);
  _ConvertMotorTable (abData0000, abMotor, sizeof (abData0000),
		      pParams->iLpi);
  Hp3400cWriteFW (iHandle, abMotor, sizeof (abData0000), 0x000);
  _ConvertMotorTable (abData0400, abMotor, sizeof (abData0400),
		      pParams->iLpi);
  Hp3400cWriteFW (iHandle, abMotor, sizeof (abData0400), 0x400);

  /* backtrack reversing speed */
  iLpiCode = pParams->iLpi * pHWParams->iExpTime / 1200L;
  NiashWriteReg (iHandle, 0x1E, (iLpiCode - 1) / 32);
}


/*
  Scanner initialisation common to all NIASH chips
*/
static void
InitNiashCommon (TScanParams * pParams, THWParams * pHWParams)
{
  int iWidthHW, iHandle, iMaxLevel;


  iHandle = pHWParams->iXferHandle;

  NiashWriteReg (iHandle, 0x02, 0x80);
  NiashWriteReg (iHandle, 0x03, 0x11);
  NiashWriteReg (iHandle, 0x01, 0x8B);
  NiashWriteReg (iHandle, 0x05, 0x01);

  /* dpi */
  WriteRegWord (iHandle, 0x0C, pParams->iDpi);

  /* calculate width in units of HW resolution */
  iWidthHW = pParams->iWidth * (HW_DPI / pParams->iDpi);

  /* set left and right limits */
  if (pHWParams->iReversedHead)
    {
      /* head is reversed */
      /* right */
      WriteRegWord (iHandle, 0x0E,
		    3 * (HW_PIXELS - (pParams->iLeft + iWidthHW)));

      /* left */
      WriteRegWord (iHandle, 0x10, 3 * (HW_PIXELS - pParams->iLeft) - 1);
    }
  else
    {
      /* head is not reversed */
      /*left  */
      WriteRegWord (iHandle, 0x0E, 3 * pParams->iLeft);

      /* right */
      WriteRegWord (iHandle, 0x10, 3 * (pParams->iLeft + iWidthHW) - 1);
    }

  /* bottom */
  WriteRegWord (iHandle, 0x1B, pParams->iBottom);	/* 0x393C); */

  /* forward jogging speed */
  NiashWriteReg (iHandle, 0x1D, 0x60);

  /* backtrack reversing speed? */
  NiashWriteReg (iHandle, 0x2B, 0x15);

  /* backtrack distance */
  if (pParams->iLpi < 600)
    {
      NiashWriteReg (iHandle, 0x1F, 0x30);
    }
  else
    {
      NiashWriteReg (iHandle, 0x1F, 0x18);
    }

  /* max buffer level before backtrace */
  iMaxLevel = MIN (pHWParams->iBufferSize / pParams->iWidth, 250);
  NiashWriteReg (iHandle, 0x14, iMaxLevel - 1);

  /* lamp PWM, max = 0x1ff? */
  WriteRegWord (iHandle, 0x2C, 0x01FF);

  /* not needed? */
  NiashWriteReg (iHandle, 0x15, 0x90);	/* 90 */
  NiashWriteReg (iHandle, 0x16, 0x70);	/* 70 */

  WriteAFE (iHandle);

  WaitReadyBit (iHandle);

  NiashWriteReg (iHandle, 0x03, 0x05);

  NiashWriteReg (iHandle, 0x02, pParams->fCalib ? 0x88 : 0xA8);
}


/* write registers */
STATIC SANE_Bool
InitScan (TScanParams * pParams, THWParams * pHWParams)
{
  int iHeight;
  int iExpTime;
  TScanParams Params;
  int iHandle;

  iHandle = pHWParams->iXferHandle;

  /* check validity of scanparameters */
  switch (pParams->iDpi)
    {
    case 150:
    case 300:
    case 600:
      break;
    default:
      DBG (DBG_ERR, "Invalid dpi (%d)\n", pParams->iDpi);
      return SANE_FALSE;
    }

  iHeight = (pParams->iBottom - pParams->iTop + 1);
  if (iHeight <= 0)
    {
      DBG (DBG_ERR, "Invalid height (%d)\n", iHeight);
      return SANE_FALSE;
    }

  if (pParams->iWidth <= 0)
    {
      DBG (DBG_ERR, "Invalid width (%d)\n", pParams->iWidth);
      return SANE_FALSE;
    }

  switch (pParams->iLpi)
    {
    case 150:
    case 300:
    case 600:
      break;
    default:
      DBG (DBG_ERR, "Invalid lpi (%d)\n", pParams->iLpi);
      return SANE_FALSE;
    }

  /* exposure time (in units of 24/Fcrystal?), must be divisible by 8 !!! */
  iExpTime = 5408;
  if ((iExpTime % 8) != 0)
    {
      DBG (DBG_ERR, "Invalid exposure time (%d)\n", iExpTime);
      return SANE_FALSE;
    }

  /*
   *** Done checking scan parameters validity ***
   */

  /*
     copy the parameters locally and make pParams point to the local copy
   */
  memcpy (&Params, pParams, sizeof (Params));
  pParams = &Params;

  if (!pHWParams->fReg07)
    {
      /* init NIASH00014 and lower */
      InitNiash00014 (pParams, pHWParams);
    }
  else
    {
      /* init NIASH00019 */
      InitNiash00019 (pParams, pHWParams);
    }

  /* common NIASH init */
  InitNiashCommon (pParams, pHWParams);

  return SANE_TRUE;
}


/************************************************************************/

static SANE_Bool
XferBufferGetLine (int iHandle, TDataPipe * p, unsigned char *pabLine,
		   SANE_Bool fReturn)
{
  unsigned char bData, bData2;
  SANE_Bool fJustDone = SANE_FALSE;
  /* all calculated transfers done ? */
  if (p->iLinesLeft == 0)
    return SANE_FALSE;

  /* time for a fresh read? */
  if (p->iCurLine == 0)
    {
      int iLines;
      iLines = p->iLinesPerXferBuf;
      /* read only as many lines as needed */
      if (p->iLinesLeft > 0 && p->iLinesLeft <= iLines)
	{
	  iLines = p->iLinesLeft;
	  DBG (DBG_MSG, "\n");
	  DBG (DBG_MSG, "last bulk read\n");
	  if (iLines < p->iLinesPerXferBuf)
	    {
	      DBG (DBG_MSG,
		   "reading reduced number of lines: %d instead of %d\n",
		   iLines, p->iLinesPerXferBuf);
	    }
	  fJustDone = SANE_TRUE;
	}
      /* reading old buffer level */
      NiashReadReg (iHandle, 0x20, &bData);
      NiashReadBulk (iHandle, p->pabXferBuf, iLines * p->iBytesPerLine);
      /* reding new buffer level */
      NiashReadReg (iHandle, 0x20, &bData2);
      if (fJustDone && fReturn)
	{
	  NiashWriteReg (iHandle, 0x02, 0x80);
	  DBG (DBG_MSG, "returning scanner head\n");
	}
      DBG (DBG_MSG,
	   "buffer level = %3d, <reading %5d unsigned chars>, buffer level = %3d\r",
	   (int) bData, iLines * p->iBytesPerLine, (int) bData2);
      fflush (stdout);
    }
  /* copy one line */
  if (pabLine != NULL)
    {
      memcpy (pabLine, &p->pabXferBuf[p->iCurLine * p->iBytesPerLine],
	      p->iBytesPerLine);
    }
  /* advance pointer */
  p->iCurLine = (p->iCurLine + 1) % p->iLinesPerXferBuf;

  /* one transfer line less to the XFerBuffer */
  if (p->iLinesLeft > 0)
    --(p->iLinesLeft);
  return SANE_TRUE;
}


static void
XferBufferInit (int iHandle, TDataPipe * p)
{
  int i;

  p->pabXferBuf = (unsigned char *) malloc (XFER_BUF_SIZE);
  p->iCurLine = 0;

  /* skip garbage lines */
  for (i = 0; i < p->iSkipLines; i++)
    {
      XferBufferGetLine (iHandle, p, NULL, SANE_FALSE);
    }
}

/* static procedure that fills the circular buffer in advance to any
   circular buffer data retrieval */
static void
CircBufferFill (int iHandle, TDataPipe * p, SANE_Bool iReversedHead)
{
  int i;
  for (i = 0; i < p->iLinesPerCircBuf; i++)
    {
      if (iReversedHead)
	{
	  XferBufferGetLine (iHandle, p,
			     &p->pabCircBuf[p->iRedLine * p->iBytesPerLine],
			     SANE_FALSE);
	}
      else
	{
	  XferBufferGetLine (iHandle, p,
			     &p->pabCircBuf[p->iBluLine * p->iBytesPerLine],
			     SANE_FALSE);
	}
      /* advance pointers */
      p->iRedLine = (p->iRedLine + 1) % p->iLinesPerCircBuf;
      p->iGrnLine = (p->iGrnLine + 1) % p->iLinesPerCircBuf;
      p->iBluLine = (p->iBluLine + 1) % p->iLinesPerCircBuf;
    }
}

static void
XferBufferExit (TDataPipe * p)
{
  if (p->pabXferBuf != NULL)
    {
      free (p->pabXferBuf);
      p->pabXferBuf = NULL;
    }
  else
    {
      DBG (DBG_ERR, "XferBufExit: Xfer buffer not initialised!\n");
    }
}


/* unscrambles a line:
   - combining the proper R, G and B lines and converting them to interpixel RGB
   - mirroring left to right
*/
static void
_UnscrambleLine (unsigned char *pabLine,
		 unsigned char *pabRed, unsigned char *pabGrn,
		 unsigned char *pabBlu, int iWidth, SANE_Bool iReversedHead,
		 int iScaleDownDpi, int iBufWeight)
{
  /* never change an approved algorithm ...
     so take Bertriks original source for this special case */
  if (iScaleDownDpi == 1 && iBufWeight == 0)
    {
      int i, j;
      if (iReversedHead)
	{
	  /* reversed */
	  for (i = 0; i < iWidth; i++)
	    {
	      j = (iWidth - i) * 3;
	      pabLine[j - 3] = pabRed[i];
	      pabLine[j - 2] = pabGrn[i + iWidth];
	      pabLine[j - 1] = pabBlu[i + iWidth * 2];
	    }
	}
      else
	{
	  /* not reversed */
	  for (i = 0; i < iWidth; i++)
	    {
	      pabLine[3 * i] = pabRed[i];
	      pabLine[3 * i + 1] = pabGrn[i + iWidth];
	      pabLine[3 * i + 2] = pabBlu[i + iWidth * 2];
	    }
	}
    }
  else
    {
      int i, j;			/* loop variables */
      int c;			/* color buffer accumulator for horizontal avarage */

      /* initialize for incremental color buffer access */
      int iInc = 1;
      int iStart = 0;

      /* set for "from the end to the front" of the circular color buffers */
      if (iReversedHead)
	{
	  iStart = iWidth - iScaleDownDpi;
	  iInc = -1;
	}

      /* each pixel is the mean of iScaleDownDpi
         so set the skip width accordingly */
      iInc *= iScaleDownDpi;

      for (i = iStart; i >= 0 && i < iWidth; i += iInc)
	{
	  /* collect the red pixels */
	  for (c = j = 0; j < iScaleDownDpi; ++j)
	    c += pabRed[i + j];
	  *pabLine =
	    (*pabLine * iBufWeight + c / iScaleDownDpi) / (iBufWeight + 1);
	  pabLine++;

	  /* collect the green pixels */
	  for (c = j = 0; j < iScaleDownDpi; ++j)
	    c += pabGrn[i + iWidth + j];
	  *pabLine =
	    (*pabLine * iBufWeight + c / iScaleDownDpi) / (iBufWeight + 1);
	  pabLine++;

	  /* collect the blue pixels */
	  for (c = j = 0; j < iScaleDownDpi; ++j)
	    c += pabBlu[i + 2 * iWidth + j];
	  *pabLine =
	    (*pabLine * iBufWeight + c / iScaleDownDpi) / (iBufWeight + 1);
	  pabLine++;
	}
    }

}


/* gets an unscrambled line from the circular buffer. the first couple of lines contain garbage,
   if fReturn==SANE_TRUE, the head will return automatically on an end of scan */
STATIC SANE_Bool
CircBufferGetLineEx (int iHandle, TDataPipe * p, unsigned char *pabLine,
		     SANE_Bool iReversedHead, SANE_Bool fReturn)
{
  int iLineCount;
  for (iLineCount = 0; iLineCount < p->iScaleDownLpi; ++iLineCount)
    {
      if (iReversedHead)
	{
	  if (!XferBufferGetLine (iHandle, p,
				  &p->pabCircBuf[p->iRedLine *
						 p->iBytesPerLine], fReturn))
	    return SANE_FALSE;
	}
      else
	{
	  if (!XferBufferGetLine (iHandle, p,
				  &p->pabCircBuf[p->iBluLine *
						 p->iBytesPerLine], fReturn))
	    return SANE_FALSE;
	}
      if (pabLine != NULL)
	{
	  _UnscrambleLine (pabLine,
			   &p->pabCircBuf[p->iRedLine * p->iBytesPerLine],
			   &p->pabCircBuf[p->iGrnLine * p->iBytesPerLine],
			   &p->pabCircBuf[p->iBluLine * p->iBytesPerLine],
			   p->iWidth * p->iScaleDownDpi, iReversedHead,
			   p->iScaleDownDpi, iLineCount);
	}

      /* advance pointers */
      p->iRedLine = (p->iRedLine + 1) % p->iLinesPerCircBuf;
      p->iGrnLine = (p->iGrnLine + 1) % p->iLinesPerCircBuf;
      p->iBluLine = (p->iBluLine + 1) % p->iLinesPerCircBuf;
    }
  return SANE_TRUE;
}


/* gets an unscrambled line from the circular buffer. the first couple of lines contain garbage */
STATIC SANE_Bool
CircBufferGetLine (int iHandle, TDataPipe * p, unsigned char *pabLine,
		   SANE_Bool iReversedHead)
{
  return CircBufferGetLineEx (iHandle, p, pabLine, iReversedHead, SANE_FALSE);
}


/* try to keep the number of transfers the same, but make them all 
   as good as possible the same size to avoid cranking in critical
   situations
*/
static int
_OptimizeXferSize (int iLines, int iLinesPerXfer)
{
  int iXfers;
  iXfers = (iLines + iLinesPerXfer - 1) / iLinesPerXfer;
  while (--iLinesPerXfer > 0
	 && (iLines + iLinesPerXfer - 1) / iLinesPerXfer == iXfers);
  return iLinesPerXfer + 1;
}

STATIC void
CircBufferInit (int iHandle, TDataPipe * p,
		int iWidth, int iHeight,
		int iMisAlignment, SANE_Bool iReversedHead,
		int iScaleDownDpi, int iScaleDownLpi)
{

  /* relevant for internal read and write functions */
  p->iScaleDownLpi = iScaleDownLpi;
  p->iScaleDownDpi = iScaleDownDpi;
  p->iWidth = iWidth;
  p->iBytesPerLine = iWidth * iScaleDownDpi * BYTES_PER_PIXEL;
  p->iSaneBytesPerLine = iWidth * BYTES_PER_PIXEL;
  if (iMisAlignment == 0)
    {
      p->iLinesPerCircBuf = 1;
    }
  else
    {
      p->iLinesPerCircBuf = 3 * iMisAlignment;
    }

  DBG (DBG_MSG, "_iScaleDown (Dpi,Lpi) = (%d,%d)\n", p->iScaleDownDpi,
       p->iScaleDownLpi);
  DBG (DBG_MSG, "_iBytesPerLine = %d\n", p->iBytesPerLine);
  DBG (DBG_MSG, "_iLinesPerCircBuf = %d\n", p->iLinesPerCircBuf);
  p->pabCircBuf =
    (unsigned char *) malloc (p->iBytesPerLine * p->iLinesPerCircBuf);
  if (p->pabCircBuf == NULL)
    {
      DBG (DBG_ERR,
	   "Unable to allocate %d unsigned chars for circular buffer\n",
	   (int) (p->iBytesPerLine * p->iLinesPerCircBuf));
      return;
    }
  DBG (DBG_MSG, "Allocated %d unsigned chars for circular buffer\n",
       p->iBytesPerLine * p->iLinesPerCircBuf);

  if (iReversedHead)
    {
      p->iBluLine = 0;
      p->iGrnLine = iMisAlignment;
      p->iRedLine = iMisAlignment * 2;
    }
  else
    {
      p->iRedLine = 0;
      p->iGrnLine = iMisAlignment;
      p->iBluLine = iMisAlignment * 2;
    }

  /* negative height is an indication for "no Check" */
  if (iHeight < 0)
    {
      p->iLinesLeft = -1;
      p->iLinesPerXferBuf = XFER_BUF_SIZE / p->iBytesPerLine;
      DBG (DBG_MSG, "using unchecked XFER_BUF_SIZE\n");
      DBG (DBG_MSG, "_iXFerSize = %d\n",
	   p->iBytesPerLine * p->iLinesPerXferBuf);
    }
  else
    {
#define SAFETY_LINES 0
#define MAX_LINES_PER_XFERBUF 800
      /* estimate of number of unsigned chars to transfer at all via the USB */
      /* add some lines for securtiy */

      p->iLinesLeft =
	iHeight + p->iSkipLines + p->iLinesPerCircBuf + SAFETY_LINES;
      p->iLinesPerXferBuf = XFER_BUF_SIZE / p->iBytesPerLine;
      /* with more than 800 lines the timing is spoiled */
      if (p->iLinesPerXferBuf > MAX_LINES_PER_XFERBUF)
	{
	  p->iLinesPerXferBuf = MAX_LINES_PER_XFERBUF;
	}
      /* final optimization to keep critical scans smooth */
      p->iLinesPerXferBuf =
	_OptimizeXferSize (p->iLinesLeft, p->iLinesPerXferBuf);

      DBG (DBG_MSG, "_iXFerSize = %d for %d transfer(s)\n",
	   (int) p->iLinesPerXferBuf * p->iBytesPerLine,
	   (p->iLinesLeft + p->iLinesPerXferBuf - 1) / p->iLinesPerXferBuf);
    }
  DBG (DBG_MSG, "_iLinesPerXferBuf = %d\n", p->iLinesPerXferBuf);

  /* init transfer buffer */
  XferBufferInit (iHandle, p);

  /* fill circular buffer */
  CircBufferFill (iHandle, p, iReversedHead);
}


STATIC void
CircBufferExit (TDataPipe * p)
{
  XferBufferExit (p);
  if (p->pabCircBuf != NULL)
    {
      DBG (DBG_MSG, "\n");
      free (p->pabCircBuf);
      p->pabCircBuf = NULL;
    }
  else
    {
      DBG (DBG_ERR, "CircBufferExit: Circular buffer not initialised!\n");
    }
}


/************************************************************************/



static int
_CalcAvg (unsigned char *pabBuf, int n, int iStep)
{
  int i, j, x;

  for (i = j = x = 0; i < n; i++)
    {
      x += pabBuf[j];
      j += iStep;
    }
  return (x / n);
}


/* converts white line data and black point data into a calibration table */
static void
CreateCalibTable (unsigned char *abWhite, unsigned char bBlackR,
		  unsigned char bBlackG, unsigned char bBlackB,
		  int iReversedHead, unsigned char *pabCalibTable)
{
  int i, j, iGain, iOffset, iData;
  unsigned char *pabPixel;

  j = 0;
  for (i = 0; i < HW_PIXELS; i++)
    {
      if (iReversedHead)
	{
	  pabPixel = &abWhite[(HW_PIXELS - i - 1) * 3];
	}
      else
	{
	  pabPixel = &abWhite[i * 3];
	}
      /* red */
      if (bBlackR > 16)
	bBlackR = 16;
      iGain = 65536 / MAX (1, pabPixel[0] - bBlackR);
      iOffset = bBlackR * 4;
      if (iOffset > 63)
	iOffset = 63;
      iData = (iGain << 6) + iOffset;
      pabCalibTable[j++] = (iData) & 255;
      pabCalibTable[j++] = (iData >> 8) & 255;
      /* green */
      if (bBlackG > 16)
	bBlackG = 16;
      iGain = 65536 / MAX (1, pabPixel[1] - bBlackG);
      iOffset = bBlackG * 4;
      if (iOffset > 63)
	iOffset = 63;
      iData = (iGain << 6) + iOffset;
      pabCalibTable[j++] = (iData) & 255;
      pabCalibTable[j++] = (iData >> 8) & 255;
      /* blue */
      if (bBlackB > 16)
	bBlackB = 16;
      iGain = 65536 / MAX (1, pabPixel[2] - bBlackB);
      iOffset = bBlackB * 4;
      if (iOffset > 63)
	iOffset = 63;
      iData = (iGain << 6) + iOffset;
      pabCalibTable[j++] = (iData) & 255;
      pabCalibTable[j++] = (iData >> 8) & 255;
    }
}


/*************************************************************************
  Lamp control functions
*************************************************************************/
STATIC SANE_Bool
GetLamp (THWParams * pHWParams, SANE_Bool * pfLampIsOn)
{
  unsigned char bData;

  NiashReadReg (pHWParams->iXferHandle, 0x03, &bData);
  *pfLampIsOn = ((bData & 0x01) != 0);
  return SANE_TRUE;
}


STATIC SANE_Bool
SetLamp (THWParams * pHWParams, SANE_Bool fLampOn)
{
  unsigned char bData;
  int iHandle;

  iHandle = pHWParams->iXferHandle;

  NiashReadReg (iHandle, 0x03, &bData);
  if (fLampOn)
    {
      NiashWriteReg (iHandle, 0x03, bData | 0x01);
    }
  else
    {
      NiashWriteReg (iHandle, 0x03, bData & ~0x01);
    }
  return SANE_TRUE;
}


/*************************************************************************
  Experimental simple calibration, but also returning the white levels
*************************************************************************/
STATIC SANE_Bool
SimpleCalibExt (THWParams * pHWPar, unsigned char *pabCalibTable,
		unsigned char *pabCalWhite)
{
  unsigned char bMinR, bMinG, bMinB;
  TDataPipe DataPipe;
  TScanParams Params;
  unsigned char abGamma[4096];
  int i, j;
  static unsigned char abBuf[HW_PIXELS * 3 * 71];	/* Carefull : see startWhite and endWhite below */
  static unsigned char abLine[HW_PIXELS * 3];
  static unsigned char abWhite[HW_PIXELS * 3];
  unsigned char *pabWhite;
  int iWhiteR, iWhiteG, iWhiteB;
  int iHandle;
  SANE_Bool iReversedHead;
  int startWhiteY, endWhiteY;
  int startBlackY, endBlackY;
  int startBlackX, endBlackX;

  iHandle = pHWPar->iXferHandle;
  iReversedHead = pHWPar->iReversedHead;

  DataPipe.iSkipLines = pHWPar->iSkipLines;

  Params.iDpi = HW_DPI;
  Params.iLpi = HW_DPI;
  if (iReversedHead)		/* hp scanners */
    Params.iTop = 60;
  else				/* agfa scanners */
    Params.iTop = 30;
  Params.iBottom = HP3300C_BOTTOM;
  Params.iLeft = 0;
  Params.iWidth = HW_PIXELS;
  Params.iHeight = 54;
  Params.fCalib = SANE_TRUE;

  /* write gamma table with neutral gain / offset */
  CalcGamma (abGamma, 1.0);
  WriteGammaCalibTable (abGamma, abGamma, abGamma, NULL, 256, 0, pHWPar);

  if (!InitScan (&Params, pHWPar))
    {
      if (pabCalWhite)
	pabCalWhite[0] = pabCalWhite[1] = pabCalWhite[2] = 0;
      return SANE_FALSE;
    }

  /* Definition of white and black areas */
  if (iReversedHead)
    {				/* hp scanners */
      startWhiteY = 0;
      endWhiteY = 15;
      startBlackY = 16;
      endBlackY = 135;
      startBlackX = 0;
      endBlackX = HW_PIXELS;
    }
  else
    {				/* agfa scanners */
      startWhiteY = 0;
      endWhiteY = 70;
      startBlackY = 86;
      endBlackY = 135;
      startBlackX = 1666;
      endBlackX = 3374;
    }

  CircBufferInit (iHandle, &DataPipe, HW_PIXELS, -1, Params.iLpi / 150,
		  iReversedHead, 1, 1);
  /* white level */
  /* skip some lines */
  for (i = 0; i < startWhiteY; i++)
    {
      CircBufferGetLine (iHandle, &DataPipe, abLine, iReversedHead);
    }
  /* Get white lines */
  for (i = 0; i < endWhiteY - startWhiteY + 1; i++)
    {
      CircBufferGetLine (iHandle, &DataPipe, &abBuf[i * HW_PIXELS * 3],
			 iReversedHead);
    }
  /* black level */
  bMinR = 255;
  bMinG = 255;
  bMinB = 255;
  /* Skip some lines */
  for (i = 0; i < startBlackY; i++)
    {
      CircBufferGetLine (iHandle, &DataPipe, abLine, iReversedHead);
    }
  for (i = 0; i < endBlackY - startBlackY + 1; i++)
    {
      CircBufferGetLine (iHandle, &DataPipe, abLine, iReversedHead);
      for (j = 0; j < endBlackX; j++)
	{
	  bMinR = MIN (abLine[j * 3 + 0], bMinR);
	  bMinG = MIN (abLine[j * 3 + 1], bMinG);
	  bMinB = MIN (abLine[j * 3 + 2], bMinB);
	}
    }
  CircBufferExit (&DataPipe);
  FinishScan (pHWPar);

  /* calc average white level */
  pabWhite = abBuf;
  for (i = 0; i < HW_PIXELS; i++)
    {
      abWhite[i * 3 + 0] =
	_CalcAvg (&pabWhite[i * 3 + 0], endWhiteY - startWhiteY + 1,
		  HW_PIXELS * 3);
      abWhite[i * 3 + 1] =
	_CalcAvg (&pabWhite[i * 3 + 1], endWhiteY - startWhiteY + 1,
		  HW_PIXELS * 3);
      abWhite[i * 3 + 2] =
	_CalcAvg (&pabWhite[i * 3 + 2], endWhiteY - startWhiteY + 1,
		  HW_PIXELS * 3);
    }
  iWhiteR = _CalcAvg (&abWhite[0], HW_PIXELS, 3);
  iWhiteG = _CalcAvg (&abWhite[1], HW_PIXELS, 3);
  iWhiteB = _CalcAvg (&abWhite[2], HW_PIXELS, 3);

  DBG (DBG_MSG, "Black level (%d,%d,%d), White level (%d,%d,%d)\n",
       (int) bMinR, (int) bMinG, (int) bMinB, iWhiteR, iWhiteG, iWhiteB);

  /* convert the white line and black point into a calibration table */
  CreateCalibTable (abWhite, bMinR, bMinG, bMinB, iReversedHead,
		    pabCalibTable);
  /* assign the White Levels */
  if (pabCalWhite)
    {
      pabCalWhite[0] = iWhiteR;
      pabCalWhite[1] = iWhiteG;
      pabCalWhite[2] = iWhiteB;
    }
  return SANE_TRUE;
}


/*************************************************************************
  FinishScan
  ==========
    Finishes the scan. Makes the scanner head move back to the home position.

*************************************************************************/
STATIC void
FinishScan (THWParams * pHWParams)
{
  NiashWriteReg (pHWParams->iXferHandle, 0x02, 0x80);
}
