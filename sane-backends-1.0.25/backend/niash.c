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
    Concept for a backend for scanners based on the NIASH chipset,
    such as HP3300C, HP3400C, HP4300C, Agfa Touch.
    Parts of this source were inspired by other backends.
*/

#include "../include/sane/config.h"
#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/saneopts.h"

#include <stdlib.h>             /* malloc, free */
#include <string.h>             /* memcpy */
#include <stdio.h>
#include <sys/time.h>
#include <sys/wait.h>

/* definitions for debug */
#define BACKEND_NAME niash
#define BUILD 1

#define DBG_ASSERT  1
#define DBG_ERR     16
#define DBG_MSG     32

/* Just to avoid conflicts between niash backend and testtool */
#define WITH_NIASH 1


/* (source) includes for data transfer methods */
#define STATIC static

#include "niash_core.c"
#include "niash_xfer.c"


#define ASSERT(cond) (!(cond) ? DBG(DBG_ASSERT, "!!! ASSERT(%S) FAILED!!!\n",STRINGIFY(cond));)


#define MM_TO_PIXEL(_mm_, _dpi_)    ((_mm_) * (_dpi_) / 25.4 )
#define PIXEL_TO_MM(_pixel_, _dpi_) ((_pixel_) * 25.4 / (_dpi_) )


/* options enumerator */
typedef enum
{
  optCount = 0,

  optGroupGeometry,
  optTLX, optTLY, optBRX, optBRY,
  optDPI,

  optGroupImage,
  optGammaTable,                /* gamma table */

  optGroupMode,
  optMode,

  optGroupEnhancement,
  optThreshold,

#ifdef EXPERIMENTAL
  optGroupMisc,
  optLamp,

  optCalibrate,
  optGamma,                      /* analog gamma = single number */
#endif
  optLast
} EOptionIndex;


typedef union
{
  SANE_Word w;
  SANE_Word *wa;                /* word array */
  SANE_String s;
} TOptionValue;

#define HW_GAMMA_SIZE 4096
#define SANE_GAMMA_SIZE 4096

typedef struct
{
  SANE_Option_Descriptor aOptions[optLast];
  TOptionValue aValues[optLast];

  TScanParams ScanParams;
  THWParams HWParams;

  TDataPipe DataPipe;
  int iLinesLeft;               /* lines to scan */
  int iBytesLeft;               /* bytes to read */
  int iPixelsPerLine;           /* pixels in one scan line */

  SANE_Int aGammaTable[SANE_GAMMA_SIZE];        /* a 12-to-8 bit color lookup table */

  /* fCancelled needed to let sane issue the cancel message
     instead of an error message */
  SANE_Bool fCancelled;         /* SANE_TRUE if scanning cancelled */

  SANE_Bool fScanning;          /* SANE_TRUE if actively scanning */

  int WarmUpTime;               /* time to wait before a calibration starts */
  unsigned char CalWhite[3];    /* values for the last calibration of white */
  struct timeval WarmUpStarted;
  /* system type to trace the time elapsed */
} TScanner;


/* linked list of SANE_Device structures */
typedef struct TDevListEntry
{
  struct TDevListEntry *pNext;
  SANE_Device dev;
} TDevListEntry;


static TDevListEntry *_pFirstSaneDev = 0;
static int iNumSaneDev = 0;
static const SANE_Device **_pSaneDevList = 0;


/* option constraints */
static const SANE_Range rangeGammaTable = { 0, 255, 1 };

/* available scanner resolutions */
static const SANE_Int setResolutions[] = { 4, 75, 150, 300, 600 };

#ifdef EXPERIMENTAL
/* range of an analog gamma */
static const SANE_Range rangeGamma = { SANE_FIX (0.25), SANE_FIX (4.0),
  SANE_FIX (0.0)
};
#endif

/* interpolate a sane gamma table to a hardware appropriate one
   just in case the sane gamma table would be smaller */
static void
_ConvertGammaTable (SANE_Word * saneGamma, unsigned char *hwGamma)
{
  int i;
  int current = 0;
  for (i = 0; i < SANE_GAMMA_SIZE; ++i)
    {
      int j;
      int next;

      /* highest range of copy indices */
      next = ((i + 1) * HW_GAMMA_SIZE) / SANE_GAMMA_SIZE;

      /* always copy the first */
      hwGamma[current] = saneGamma[i];

      /* the interpolation of the rest depends on the gap */
      for (j = current + 1; j < HW_GAMMA_SIZE && j < next; ++j)
        {
          hwGamma[j] =
            (saneGamma[i] * (next - j) +
             saneGamma[i + 1] * (j - current)) / (next - current);
        }
      current = next;
    }
}

/* create a unity gamma table */
static void
_UnityGammaTable (unsigned char *hwGamma)
{
  int i;
  for (i = 0; i < HW_GAMMA_SIZE; ++i)
    {
      hwGamma[i] = (i * 256) / HW_GAMMA_SIZE;
    }

}

static const SANE_Range rangeXmm = { 0, 220, 1 };
static const SANE_Range rangeYmm = { 0, 296, 1 };
static const SANE_Int startUpGamma = SANE_FIX (1.6);

static const char colorStr[] = { SANE_VALUE_SCAN_MODE_COLOR };
static const char grayStr[] = { SANE_VALUE_SCAN_MODE_GRAY };
static const char lineartStr[] = { SANE_VALUE_SCAN_MODE_LINEART };

#define DEPTH_LINEART  1
#define DEPTH_GRAY     8
#define DEPTH_COLOR    8

#define BYTES_PER_PIXEL_GRAY   1
#define BYTES_PER_PIXEL_COLOR  3

#define BITS_PER_PIXEL_LINEART 1
#define BITS_PER_PIXEL_GRAY    DEPTH_GRAY
#define BITS_PER_PIXEL_COLOR   (DEPTH_COLOR*3)

#define BITS_PER_BYTE  8
#define BITS_PADDING   (BITS_PER_BYTE-1)

#define MODE_COLOR   0
#define MODE_GRAY    1
#define MODE_LINEART 2

/* lineart treshold range */
static const SANE_Range rangeThreshold = {
  0,
  100,
  1
};

/* scanning modes */
static SANE_String_Const modeList[] = {
  colorStr,
  grayStr,
  lineartStr,
  NULL
};

static int
_bytesPerLineLineart (int pixelsPerLine)
{
  return (pixelsPerLine * BITS_PER_PIXEL_LINEART +
          BITS_PADDING) / BITS_PER_BYTE;
}

static int
_bytesPerLineGray (int pixelsPerLine)
{
  return (pixelsPerLine * BITS_PER_PIXEL_GRAY + BITS_PADDING) / BITS_PER_BYTE;
}

static int
_bytesPerLineColor (int pixelsPerLine)
{
  return (pixelsPerLine * BITS_PER_PIXEL_COLOR +
          BITS_PADDING) / BITS_PER_BYTE;
}


/* dummy*/
static void
_rgb2rgb (unsigned char __sane_unused__ *buffer, int __sane_unused__ pixels, int __sane_unused__ threshold)
{
  /* make the compiler content */
}


/* convert 24bit RGB to 8bit GRAY */
static void
_rgb2gray (unsigned char *buffer, int pixels, int __sane_unused__ threshold)
{
#define WEIGHT_R 27
#define WEIGHT_G 54
#define WEIGHT_B 19
#define WEIGHT_W (WEIGHT_R + WEIGHT_G + WEIGHT_B)
  static int aWeight[BYTES_PER_PIXEL_COLOR] =
    { WEIGHT_R, WEIGHT_G, WEIGHT_B };
  int nbyte = pixels * BYTES_PER_PIXEL_COLOR;
  int acc = 0;
  int x;

  for (x = 0; x < nbyte; ++x)
    {
      acc += aWeight[x % BYTES_PER_PIXEL_COLOR] * buffer[x];
      if ((x + 1) % BYTES_PER_PIXEL_COLOR == 0)
        {
          buffer[x / BYTES_PER_PIXEL_COLOR] =
            (unsigned char) (acc / WEIGHT_W);
          acc = 0;
        }
    }
#undef WEIGHT_R
#undef WEIGHT_G
#undef WEIGHT_B
#undef WEIGHT_W
}

/* convert 24bit RGB to 1bit B/W */
static void
_rgb2lineart (unsigned char *buffer, int pixels, int threshold)
{
  static const int aMask[BITS_PER_BYTE] = { 128, 64, 32, 16, 8, 4, 2, 1 };
  int acc = 0;
  int nx;
  int x;
  int thresh;
  _rgb2gray (buffer, pixels, 0);
  nx = ((pixels + BITS_PADDING) / BITS_PER_BYTE) * BITS_PER_BYTE;
  thresh = 255 * threshold / rangeThreshold.max;
  for (x = 0; x < nx; ++x)
    {
      if (x < pixels && buffer[x] < thresh)
        {
          acc |= aMask[x % BITS_PER_BYTE];
        }
      if ((x + 1) % BITS_PER_BYTE == 0)
        {
          buffer[x / BITS_PER_BYTE] = (unsigned char) (acc);
          acc = 0;
        }
    }
}

typedef struct tgModeParam
{
  SANE_Int depth;
  SANE_Frame format;
  int (*bytesPerLine) (int pixelsPerLine);
  void (*adaptFormat) (unsigned char *rgbBuffer, int pixels, int threshold);

} TModeParam;

static const TModeParam modeParam[] = {
  {DEPTH_COLOR, SANE_FRAME_RGB, _bytesPerLineColor, _rgb2rgb},
  {DEPTH_GRAY, SANE_FRAME_GRAY, _bytesPerLineGray, _rgb2gray},
  {DEPTH_LINEART, SANE_FRAME_GRAY, _bytesPerLineLineart, _rgb2lineart}
};


#define WARMUP_AFTERSTART    1  /* flag for 1st warm up */
#define WARMUP_INSESSION     0
#define WARMUP_TESTINTERVAL 15  /* test every 15sec */
#define WARMUP_TIME         30  /* first wait is 30sec minimum */
#define WARMUP_MAXTIME      90  /* after one and a half minute start latest */

#define CAL_DEV_MAX         15
/* maximum deviation of cal values in percent between 2 tests */

/* different warm up after start and after automatic off */
static const int aiWarmUpTime[] = { WARMUP_TESTINTERVAL, WARMUP_TIME };



/* returns 1, when the warm up time "iTime" has elasped */
static int
_TimeElapsed (struct timeval *start, struct timeval *now, int iTime)
{

  /* this is a bit strange, but can deal with overflows */
  if (start->tv_sec > now->tv_sec)
    return (start->tv_sec / 2 - now->tv_sec / 2 > iTime / 2);
  else
    return (now->tv_sec - start->tv_sec >= iTime);
}

static void
_WarmUpLamp (TScanner * s, int iMode)
{
  SANE_Bool fLampOn;
  /* on startup don't care what was before
     assume lamp was off, and the previous
     cal values can never be reached */
  if (iMode == WARMUP_AFTERSTART)
    {
      fLampOn = SANE_FALSE;
      s->CalWhite[0] = s->CalWhite[1] = s->CalWhite[2] = (unsigned char) (-1);
    }
  else
    GetLamp (&s->HWParams, &fLampOn);

  if (!fLampOn)
    {
      /* get the current system time */
      gettimeofday (&s->WarmUpStarted, 0);
      /* determine the time to wait at least */
      s->WarmUpTime = aiWarmUpTime[iMode];
      /* switch on the lamp */
      SetLamp (&s->HWParams, SANE_TRUE);
    }
}

static void
_WaitForLamp (TScanner * s, unsigned char *pabCalibTable)
{
  struct timeval now[2];        /* toggling time holder */
  int i;                        /* rgb loop */
  int iCal = 0;                 /* counter */
  int iCurrent = 0;             /* buffer and time-holder swap flag */
  SANE_Bool fHasCal;
  unsigned char CalWhite[2][3]; /* toggling buffer */
  int iDelay = 0;               /* delay loop counter */
  _WarmUpLamp (s, SANE_FALSE);


  /* get the time stamp for the wait loops */
  if (s->WarmUpTime)
    gettimeofday (&now[iCurrent], 0);
  SimpleCalibExt (&s->HWParams, pabCalibTable, CalWhite[iCurrent]);
  fHasCal = SANE_TRUE;

  DBG (DBG_MSG, "_WaitForLamp: first calibration\n");


  /* wait until time has elapsed or for values to stabilze */
  while (s->WarmUpTime)
    {
      /* check if the last scan has lower calibration values than
         the current one would have */
      if (s->WarmUpTime && fHasCal)
        {
          SANE_Bool fOver = SANE_TRUE;
          for (i = 0; fOver && i < 3; ++i)
            {
              if (!s->CalWhite[i])
                fOver = SANE_FALSE;
              else if (CalWhite[iCurrent][i] < s->CalWhite[i])
                fOver = SANE_FALSE;
            }

          /* warm up is not needed, when calibration data is above
             the calibration data of the last scan */
          if (fOver)
            {
              s->WarmUpTime = 0;
              DBG (DBG_MSG,
                   "_WaitForLamp: Values seem stable, skipping next calibration cycle\n");
            }
        }


      /* break the loop, when the longest wait time has expired
         to prevent a hanging application,
         even if the values might not be good, yet */
      if (s->WarmUpTime && fHasCal && iCal)
        {
          /* abort, when we have waited long enough */
          if (_TimeElapsed
              (&s->WarmUpStarted, &now[iCurrent], WARMUP_MAXTIME))
            {
              /* stop idling */
              s->WarmUpTime = 0;
              DBG (DBG_MSG, "_WaitForLamp: WARMUP_MAXTIME=%ds elapsed!\n",
                   WARMUP_MAXTIME);
            }
        }


      /* enter a delay loop, when there is still time to wait */
      if (s->WarmUpTime)
        {
          /* if the (too low) calibration values have just been acquired
             we start waiting */
          if (fHasCal)
            DBG (DBG_MSG, "_WaitForLamp: entering delay loop\r");
          else
            DBG (DBG_MSG, "_WaitForLamp: delay loop %d        \r", ++iDelay);
          sleep (1);
          fHasCal = SANE_FALSE;
          gettimeofday (&now[!iCurrent], 0);
        }


      /* look if we should check again */
      if (s->WarmUpTime         /* did we have to wait at all */
          /* is the minimum time elapsed */
          && _TimeElapsed (&s->WarmUpStarted, &now[!iCurrent], s->WarmUpTime)
          /* has the minimum time elapsed since the last calibration */
          && _TimeElapsed (&now[iCurrent], &now[!iCurrent],
                           WARMUP_TESTINTERVAL))
        {
          int dev = 0;          /* 0 percent deviation in cal value as default */
          iDelay = 0;           /* all delays processed */
          /* new calibration */
          ++iCal;
          iCurrent = !iCurrent; /* swap the test-buffer, and time-holder */
          SimpleCalibExt (&s->HWParams, pabCalibTable, CalWhite[iCurrent]);
          fHasCal = SANE_TRUE;

          for (i = 0; i < 3; ++i)
            {
              /* copy for faster and clearer access */
              int cwa;
              int cwb;
              int ldev;
              cwa = CalWhite[!iCurrent][i];
              cwb = CalWhite[iCurrent][i];
              /* find the biggest deviation of one color */
              if (cwa > cwb)
                ldev = 0;
              else if (cwa && cwb)
                ldev = ((cwb - cwa) * 100) / cwb;
              else
                ldev = 100;
              dev = MAX (dev, ldev);
            }

          /* show the biggest deviation of the calibration values */
          DBG (DBG_MSG, "_WaitForLamp: recalibration #%d, deviation = %d%%\n",
               iCal, dev);

          /* the deviation to the previous calibration is tolerable */
          if (dev <= CAL_DEV_MAX)
            s->WarmUpTime = 0;
        }
    }

  /* remember the values of this calibration
     for the next time */
  for (i = 0; i < 3; ++i)
    {
      s->CalWhite[i] = CalWhite[iCurrent][i];
    }
}


/* used, when setting gamma as 1 value */
static void
_SetScalarGamma (SANE_Int * aiGamma, SANE_Int sfGamma)
{
  int j;
  double fGamma;
  fGamma = SANE_UNFIX (sfGamma);
  for (j = 0; j < SANE_GAMMA_SIZE; j++)
    {
      int iData;
      iData =
        floor (256.0 *
               pow (((double) j / (double) SANE_GAMMA_SIZE), 1.0 / fGamma));
      if (iData > 255)
        iData = 255;
      aiGamma[j] = iData;
    }
}


/* return size of longest string in a string list */
static size_t
_MaxStringSize (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
        max_size = size;
    }
  return max_size;
}


/* change a sane cap and return true, when a change took place */
static int
_ChangeCap (SANE_Word * pCap, SANE_Word cap, int isSet)
{
  SANE_Word prevCap = *pCap;
  if (isSet)
    {
      *pCap |= cap;
    }
  else
    {
      *pCap &= ~cap;
    }
  return *pCap != prevCap;
}


static void
_InitOptions (TScanner * s)
{
  int i;
  SANE_Option_Descriptor *pDesc;
  TOptionValue *pVal;
  _SetScalarGamma (s->aGammaTable, startUpGamma);

  for (i = optCount; i < optLast; i++)
    {

      pDesc = &s->aOptions[i];
      pVal = &s->aValues[i];

      /* defaults */
      pDesc->name = "";
      pDesc->title = "";
      pDesc->desc = "";
      pDesc->type = SANE_TYPE_INT;
      pDesc->unit = SANE_UNIT_NONE;
      pDesc->size = sizeof (SANE_Word);
      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
      pDesc->cap = 0;

      switch (i)
        {

        case optCount:
          pDesc->title = SANE_TITLE_NUM_OPTIONS;
          pDesc->desc = SANE_DESC_NUM_OPTIONS;
          pDesc->cap = SANE_CAP_SOFT_DETECT;
          pVal->w = (SANE_Word) optLast;
          break;

        case optGroupGeometry:
          pDesc->title = "Geometry";
          pDesc->type = SANE_TYPE_GROUP;
          pDesc->size = 0;
          break;

        case optTLX:
          pDesc->name = SANE_NAME_SCAN_TL_X;
          pDesc->title = SANE_TITLE_SCAN_TL_X;
          pDesc->desc = SANE_DESC_SCAN_TL_X;
          pDesc->unit = SANE_UNIT_MM;
          pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
          pDesc->constraint.range = &rangeXmm;
          pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
          pVal->w = rangeXmm.min;
          break;

        case optTLY:
          pDesc->name = SANE_NAME_SCAN_TL_Y;
          pDesc->title = SANE_TITLE_SCAN_TL_Y;
          pDesc->desc = SANE_DESC_SCAN_TL_Y;
          pDesc->unit = SANE_UNIT_MM;
          pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
          pDesc->constraint.range = &rangeYmm;
          pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
          pVal->w = rangeYmm.min;
          break;

        case optBRX:
          pDesc->name = SANE_NAME_SCAN_BR_X;
          pDesc->title = SANE_TITLE_SCAN_BR_X;
          pDesc->desc = SANE_DESC_SCAN_BR_X;
          pDesc->unit = SANE_UNIT_MM;
          pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
          pDesc->constraint.range = &rangeXmm;
          pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
          pVal->w = 210 /* A4 width instead of rangeXmm.max */ ;
          break;

        case optBRY:
          pDesc->name = SANE_NAME_SCAN_BR_Y;
          pDesc->title = SANE_TITLE_SCAN_BR_Y;
          pDesc->desc = SANE_DESC_SCAN_BR_Y;
          pDesc->unit = SANE_UNIT_MM;
          pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
          pDesc->constraint.range = &rangeYmm;
          pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
          pVal->w = 290 /* have a bit reserve instaed of rangeYmm.max */ ;
          break;

        case optDPI:
          pDesc->name = SANE_NAME_SCAN_RESOLUTION;
          pDesc->title = SANE_TITLE_SCAN_RESOLUTION;
          pDesc->desc = SANE_DESC_SCAN_RESOLUTION;
          pDesc->unit = SANE_UNIT_DPI;
          pDesc->constraint_type = SANE_CONSTRAINT_WORD_LIST;
          pDesc->constraint.word_list = setResolutions;
          pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
          pVal->w = setResolutions[2];  /* default to 150dpi */
          break;

        case optGroupImage:
          pDesc->title = SANE_I18N ("Image");
          pDesc->type = SANE_TYPE_GROUP;
          pDesc->size = 0;
          break;

#ifdef EXPERIMENTAL
        case optGamma:
          pDesc->name = SANE_NAME_ANALOG_GAMMA;
          pDesc->title = SANE_TITLE_ANALOG_GAMMA;
          pDesc->desc = SANE_DESC_ANALOG_GAMMA;
          pDesc->type = SANE_TYPE_FIXED;
          pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
          pDesc->constraint.range = &rangeGamma;
          pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
          pVal->w = startUpGamma;
          break;
#endif

        case optGammaTable:
          pDesc->name = SANE_NAME_GAMMA_VECTOR;
          pDesc->title = SANE_TITLE_GAMMA_VECTOR;
          pDesc->desc = SANE_DESC_GAMMA_VECTOR;
          pDesc->size = sizeof (s->aGammaTable);
          pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
          pDesc->constraint.range = &rangeGammaTable;
          pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
          pVal->wa = s->aGammaTable;
          break;

#ifdef EXPERIMENTAL
        case optGroupMisc:
          pDesc->title = SANE_I18N ("Miscellaneous");
          pDesc->type = SANE_TYPE_GROUP;
          pDesc->size = 0;
          break;

        case optLamp:
          pDesc->name = "lamp";
          pDesc->title = SANE_I18N ("Lamp status");
          pDesc->desc = SANE_I18N ("Switches the lamp on or off.");
          pDesc->type = SANE_TYPE_BOOL;
          pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
          /* switch the lamp on when starting for first the time */
          pVal->w = SANE_TRUE;
          break;

        case optCalibrate:
          pDesc->name = "calibrate";
          pDesc->title = SANE_I18N ("Calibrate");
          pDesc->desc = SANE_I18N ("Calibrates for black and white level.");
          pDesc->type = SANE_TYPE_BUTTON;
          pDesc->cap = SANE_CAP_SOFT_SELECT;
          pDesc->size = 0;
          break;
#endif
        case optGroupMode:
          pDesc->title = SANE_I18N ("Scan Mode");
          pDesc->desc = "";
          pDesc->type = SANE_TYPE_GROUP;
          break;

        case optMode:
          /* scan mode */
          pDesc->name = SANE_NAME_SCAN_MODE;
          pDesc->title = SANE_TITLE_SCAN_MODE;
          pDesc->desc = SANE_DESC_SCAN_MODE;
          pDesc->type = SANE_TYPE_STRING;
          pDesc->size = _MaxStringSize (modeList);
          pDesc->constraint_type = SANE_CONSTRAINT_STRING_LIST;
          pDesc->constraint.string_list = modeList;
          pDesc->cap =
            SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_EMULATED;
          pVal->w = MODE_COLOR;
          break;

        case optGroupEnhancement:
          pDesc->title = SANE_I18N ("Enhancement");
          pDesc->desc = "";
          pDesc->type = SANE_TYPE_GROUP;
          break;

        case optThreshold:
          pDesc->name = SANE_NAME_THRESHOLD;
          pDesc->title = SANE_TITLE_THRESHOLD;
          pDesc->desc = SANE_DESC_THRESHOLD;
          pDesc->type = SANE_TYPE_INT;
          pDesc->unit = SANE_UNIT_PERCENT;
          pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
          pDesc->constraint.range = &rangeThreshold;
          pDesc->cap =
            SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE |
            SANE_CAP_EMULATED;
          pVal->w = 50;

        default:
          DBG (DBG_ERR, "Uninitialised option %d\n", i);
          break;
        }
    }
}


static int
_ReportDevice (TScannerModel * pModel, const char *pszDeviceName)
{
  TDevListEntry *pNew, *pDev;

  DBG (DBG_MSG, "niash: _ReportDevice '%s'\n", pszDeviceName);

  pNew = malloc (sizeof (TDevListEntry));
  if (!pNew)
    {
      DBG (DBG_ERR, "no mem\n");
      return -1;
    }

  /* add new element to the end of the list */
  if (_pFirstSaneDev == 0)
    {
      _pFirstSaneDev = pNew;
    }
  else
    {
      for (pDev = _pFirstSaneDev; pDev->pNext; pDev = pDev->pNext)
        {
          ;
        }
      pDev->pNext = pNew;
    }

  /* fill in new element */
  pNew->pNext = 0;
  pNew->dev.name = strdup (pszDeviceName);
  pNew->dev.vendor = pModel->pszVendor;
  pNew->dev.model = pModel->pszName;
  pNew->dev.type = "flatbed scanner";

  iNumSaneDev++;

  return 0;
}



/*****************************************************************************/

SANE_Status
sane_init (SANE_Int * piVersion, SANE_Auth_Callback __sane_unused__ pfnAuth)
{
  DBG_INIT ();
  DBG (DBG_MSG, "sane_init\n");

  if (piVersion != NULL)
    {
      *piVersion = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);
    }

  /* initialise transfer methods */
  iNumSaneDev = 0;
  NiashXferInit (_ReportDevice);

  return SANE_STATUS_GOOD;
}


void
sane_exit (void)
{
  TDevListEntry *pDev, *pNext;

  DBG (DBG_MSG, "sane_exit\n");

  /* free device list memory */
  if (_pSaneDevList)
    {
      for (pDev = _pFirstSaneDev; pDev; pDev = pNext)
        {
          pNext = pDev->pNext;
          free ((void *) pDev->dev.name);
          free (pDev);
        }
      _pFirstSaneDev = 0;
      free (_pSaneDevList);
      _pSaneDevList = 0;
    }
}


SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool __sane_unused__ local_only)
{
  TDevListEntry *pDev;
  int i;

  DBG (DBG_MSG, "sane_get_devices\n");

  if (_pSaneDevList)
    {
      free (_pSaneDevList);
    }

  _pSaneDevList = malloc (sizeof (*_pSaneDevList) * (iNumSaneDev + 1));
  if (!_pSaneDevList)
    {
      DBG (DBG_MSG, "no mem\n");
      return SANE_STATUS_NO_MEM;
    }
  i = 0;
  for (pDev = _pFirstSaneDev; pDev; pDev = pDev->pNext)
    {
      _pSaneDevList[i++] = &pDev->dev;
    }
  _pSaneDevList[i++] = 0;       /* last entry is 0 */

  *device_list = _pSaneDevList;

  return SANE_STATUS_GOOD;
}


SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * h)
{
  TScanner *s;

  DBG (DBG_MSG, "sane_open: %s\n", name);

  /* check the name */
  if (strlen (name) == 0)
    {
      /* default to first available device */
      name = _pFirstSaneDev->dev.name;
    }

  s = malloc (sizeof (TScanner));
  if (!s)
    {
      DBG (DBG_MSG, "malloc failed\n");
      return SANE_STATUS_NO_MEM;
    }

  if (NiashOpen (&s->HWParams, name) < 0)
    {
      /* is this OK ? */
      DBG (DBG_ERR, "NiashOpen failed\n");
      free ((void *) s);
      return SANE_STATUS_DEVICE_BUSY;
    }
  _InitOptions (s);
  s->fScanning = SANE_FALSE;
  s->fCancelled = SANE_FALSE;
  *h = s;

  /* Turn on lamp by default at startup */
  _WarmUpLamp (s, WARMUP_AFTERSTART);

  return SANE_STATUS_GOOD;
}


void
sane_close (SANE_Handle h)
{
  TScanner *s;

  DBG (DBG_MSG, "sane_close\n");

  s = (TScanner *) h;

  /* turn off scanner lamp */
  SetLamp (&s->HWParams, SANE_FALSE);

  /* close scanner */
  NiashClose (&s->HWParams);

  /* free scanner object memory */
  free ((void *) s);
}


const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle h, SANE_Int n)
{
  TScanner *s;

  DBG (DBG_MSG, "sane_get_option_descriptor %d\n", n);

  if ((n < optCount) || (n >= optLast))
    {
      return NULL;
    }

  s = (TScanner *) h;
  return &s->aOptions[n];
}


SANE_Status
sane_control_option (SANE_Handle h, SANE_Int n, SANE_Action Action,
                     void *pVal, SANE_Int * pInfo)
{
  TScanner *s;
  static char szTable[100];
  int *pi;
  int i;
  SANE_Int info;
  SANE_Status status;
#ifdef EXPERIMENTAL
  SANE_Bool fLampIsOn;
  SANE_Bool fVal;
  SANE_Bool fSame;
#endif

  DBG (DBG_MSG, "sane_control_option: option %d, action %d\n", n, Action);

  if ((n < optCount) || (n >= optLast))
    {
      return SANE_STATUS_UNSUPPORTED;
    }

  if (Action == SANE_ACTION_GET_VALUE || Action == SANE_ACTION_SET_VALUE)
    {
      if (pVal == NULL)
        {
          return SANE_STATUS_INVAL;
        }
    }

  s = (TScanner *) h;
  info = 0;

  switch (Action)
    {
    case SANE_ACTION_GET_VALUE:
      switch (n)
        {

          /* Get options of type SANE_Word */
        case optCount:
        case optDPI:
#ifdef EXPERIMENTAL
        case optGamma:
#endif
        case optTLX:
        case optTLY:
        case optBRX:
        case optBRY:
        case optThreshold:
          DBG (DBG_MSG,
               "sane_control_option: SANE_ACTION_GET_VALUE %d = %d\n", n,
               (int) s->aValues[n].w);
          *(SANE_Word *) pVal = s->aValues[n].w;
          break;

          /* Get options of type SANE_Word array */
        case optGammaTable:
          DBG (DBG_MSG, "Reading gamma table\n");
          memcpy (pVal, s->aValues[n].wa, s->aOptions[n].size);
          break;

        case optMode:
          DBG (DBG_MSG, "Reading scan mode %s\n",
               modeList[s->aValues[optMode].w]);
          strcpy ((char *) pVal, modeList[s->aValues[optMode].w]);
          break;

#ifdef EXPERIMENTAL
          /* Get options of type SANE_Bool */
        case optLamp:
          GetLamp (&s->HWParams, &fLampIsOn);
          *(SANE_Bool *) pVal = fLampIsOn;
          break;

        case optCalibrate:
          /*  although this option has nothing to read,
             it's added here to avoid a warning when running scanimage --help */
          break;
#endif

        default:
          DBG (DBG_MSG, "SANE_ACTION_GET_VALUE: Invalid option (%d)\n", n);
        }
      break;


    case SANE_ACTION_SET_VALUE:
      if (s->fScanning)
        {
          DBG (DBG_ERR,
               "sane_control_option: SANE_ACTION_SET_VALUE not allowed during scan\n");
          return SANE_STATUS_INVAL;
        }
      switch (n)
        {

        case optCount:
          return SANE_STATUS_INVAL;

#ifdef EXPERIMENTAL
        case optGamma:
#endif
        case optThreshold:
        case optDPI:

          info |= SANE_INFO_RELOAD_PARAMS;
          /* fall through */

        case optTLX:
        case optTLY:
        case optBRX:
        case optBRY:

          status = sanei_constrain_value (&s->aOptions[n], pVal, &info);
          if (status != SANE_STATUS_GOOD)
            {
              DBG (DBG_ERR, "Failed to constrain option %d (%s)\n", n,
                   s->aOptions[n].title);
              return status;
            }

#ifdef EXPERIMENTAL
          /* check values if they are equal */
          fSame = s->aValues[n].w == *(SANE_Word *) pVal;
#endif

          /* set the values */
          s->aValues[n].w = *(SANE_Word *) pVal;
          DBG (DBG_MSG,
               "sane_control_option: SANE_ACTION_SET_VALUE %d = %d\n", n,
               (int) s->aValues[n].w);
#ifdef EXPERIMENTAL
          if (n == optGamma)
            {
              if (!fSame && optLast > optGammaTable)
                {
                  info |= SANE_INFO_RELOAD_OPTIONS;
                }
              _SetScalarGamma (s->aGammaTable, s->aValues[n].w);
            }
#endif
          break;

        case optGammaTable:
          DBG (DBG_MSG, "Writing gamma table\n");
          pi = (SANE_Int *) pVal;
          memcpy (s->aValues[n].wa, pVal, s->aOptions[n].size);

          /* prepare table for debug */
          strcpy (szTable, "Gamma table summary:");
          for (i = 0; i < SANE_GAMMA_SIZE; i++)
            {
              if ((SANE_GAMMA_SIZE / 16) && (i % (SANE_GAMMA_SIZE / 16)) == 0)
                {
                  DBG (DBG_MSG, "%s\n", szTable);
		  szTable[0] = '\0';
                }
              /* test for number print */
              if ((SANE_GAMMA_SIZE / 64) && (i % (SANE_GAMMA_SIZE / 64)) == 0)
                {
                  sprintf (szTable + strlen(szTable), " %04X", pi[i]);
                }
            }
          if (strlen (szTable))
            {
              DBG (DBG_MSG, "%s\n", szTable);
            }
          break;

        case optMode:
          {
            SANE_Word *pCap;
            int fCapChanged = 0;

            pCap = &s->aOptions[optThreshold].cap;

            if (strcmp ((char const *) pVal, colorStr) == 0)
              {
                s->aValues[optMode].w = MODE_COLOR;
                fCapChanged = _ChangeCap (pCap, SANE_CAP_INACTIVE, 1);
              }
            if (strcmp ((char const *) pVal, grayStr) == 0)
              {
                s->aValues[optMode].w = MODE_GRAY;
                fCapChanged = _ChangeCap (pCap, SANE_CAP_INACTIVE, 1);
              }
            if (strcmp ((char const *) pVal, lineartStr) == 0)
              {
                s->aValues[optMode].w = MODE_LINEART;
                fCapChanged = _ChangeCap (pCap, SANE_CAP_INACTIVE, 0);

              }
            info |= SANE_INFO_RELOAD_PARAMS;
            if (fCapChanged)
              {
                info |= SANE_INFO_RELOAD_OPTIONS;
              }
            DBG (DBG_MSG, "setting scan mode: %s\n", (char const *) pVal);
          }
          break;



#ifdef EXPERIMENTAL
        case optLamp:
          fVal = *(SANE_Bool *) pVal;
          DBG (DBG_MSG, "lamp %s\n", fVal ? "on" : "off");
          if (fVal)
            _WarmUpLamp (s, WARMUP_INSESSION);
          else
            SetLamp (&s->HWParams, SANE_FALSE);
          break;

        case optCalibrate:
/*       SimpleCalib(&s->HWParams); */
          break;
#endif

        default:
          DBG (DBG_ERR, "SANE_ACTION_SET_VALUE: Invalid option (%d)\n", n);
        }
      if (pInfo != NULL)
        {
          *pInfo |= info;
        }
      break;


    case SANE_ACTION_SET_AUTO:
      return SANE_STATUS_UNSUPPORTED;


    default:
      DBG (DBG_ERR, "Invalid action (%d)\n", Action);
      return SANE_STATUS_INVAL;
    }

  return SANE_STATUS_GOOD;
}





SANE_Status
sane_get_parameters (SANE_Handle h, SANE_Parameters * p)
{
  TScanner *s;
  TModeParam const *pMode;

  DBG (DBG_MSG, "sane_get_parameters\n");

  s = (TScanner *) h;

  /* first do some checks */
  if (s->aValues[optTLX].w >= s->aValues[optBRX].w)
    {
      DBG (DBG_ERR, "TLX should be smaller than BRX\n");
      return SANE_STATUS_INVAL; /* proper error code? */
    }
  if (s->aValues[optTLY].w >= s->aValues[optBRY].w)
    {
      DBG (DBG_ERR, "TLY should be smaller than BRY\n");
      return SANE_STATUS_INVAL; /* proper error code? */
    }


  pMode = &modeParam[s->aValues[optMode].w];

  /* return the data */
  p->format = pMode->format;
  p->last_frame = SANE_TRUE;

  p->lines = MM_TO_PIXEL (s->aValues[optBRY].w - s->aValues[optTLY].w,
                          s->aValues[optDPI].w);
  p->depth = pMode->depth;
  p->pixels_per_line =
    MM_TO_PIXEL (s->aValues[optBRX].w - s->aValues[optTLX].w,
                 s->aValues[optDPI].w);
  p->bytes_per_line = pMode->bytesPerLine (p->pixels_per_line);

  return SANE_STATUS_GOOD;
}


/* get the scale down factor for a resolution that is
  not supported by hardware */
static int
_SaneEmulateScaling (int iDpi)
{
  if (iDpi == 75)
    return 2;
  else
    return 1;
}


SANE_Status
sane_start (SANE_Handle h)
{
  TScanner *s;
  SANE_Parameters par;
  int iLineCorr;
  int iScaleDown;
  static unsigned char abGamma[HW_GAMMA_SIZE];
  static unsigned char abCalibTable[HW_PIXELS * 6];

  DBG (DBG_MSG, "sane_start\n");

  s = (TScanner *) h;

  if (sane_get_parameters (h, &par) != SANE_STATUS_GOOD)
    {
      DBG (DBG_MSG, "Invalid scan parameters\n");
      return SANE_STATUS_INVAL;
    }
  iScaleDown = _SaneEmulateScaling (s->aValues[optDPI].w);
  s->iLinesLeft = par.lines;

  /* fill in the scanparams using the option values */
  s->ScanParams.iDpi = s->aValues[optDPI].w * iScaleDown;
  s->ScanParams.iLpi = s->aValues[optDPI].w * iScaleDown;

  /* calculate correction for filling of circular buffer */
  iLineCorr = 3 * s->HWParams.iSensorSkew;      /* usually 16 motor steps */
  /* calculate correction for garbage lines */
  iLineCorr += s->HWParams.iSkipLines * (HW_LPI / s->ScanParams.iLpi);

  s->ScanParams.iTop =
    MM_TO_PIXEL (s->aValues[optTLY].w + s->HWParams.iTopLeftY,
                 HW_LPI) - iLineCorr;
  s->ScanParams.iLeft =
    MM_TO_PIXEL (s->aValues[optTLX].w + s->HWParams.iTopLeftX, HW_DPI);

  s->ScanParams.iWidth = par.pixels_per_line * iScaleDown;
  s->ScanParams.iHeight = par.lines * iScaleDown;
  s->ScanParams.iBottom = HP3300C_BOTTOM;
  s->ScanParams.fCalib = SANE_FALSE;

  /* perform a simple calibration just before scanning */
  _WaitForLamp (s, abCalibTable);

  if (s->aValues[optMode].w == MODE_LINEART)
    {
      /* use a unity gamma table for lineart to be independent from Gamma settings */
      _UnityGammaTable (abGamma);
    }
  else
    {
      /* copy gamma table */
      _ConvertGammaTable (s->aGammaTable, abGamma);
    }

  WriteGammaCalibTable (abGamma, abGamma, abGamma, abCalibTable, 0, 0,
                        &s->HWParams);

  /* prepare the actual scan */
  if (!InitScan (&s->ScanParams, &s->HWParams))
    {
      DBG (DBG_MSG, "Invalid scan parameters\n");
      return SANE_STATUS_INVAL;
    }

  /* init data pipe */
  s->DataPipe.iSkipLines = s->HWParams.iSkipLines;
  /* on the hp3400 and hp4300 we cannot set the top of the scan area (yet),
     so instead we just scan and throw away the data until the top */
  if (s->HWParams.fReg07)
    {
      s->DataPipe.iSkipLines +=
        MM_TO_PIXEL (s->aValues[optTLY].w + s->HWParams.iTopLeftY,
                     s->aValues[optDPI].w * iScaleDown);
    }
  s->iBytesLeft = 0;
  s->iPixelsPerLine = par.pixels_per_line;

  /* hack */
  s->DataPipe.pabLineBuf = (unsigned char *) malloc (HW_PIXELS * 3);
  CircBufferInit (s->HWParams.iXferHandle, &s->DataPipe,
                  par.pixels_per_line, s->ScanParams.iHeight,
                  s->ScanParams.iLpi * s->HWParams.iSensorSkew / HW_LPI,
                  s->HWParams.iReversedHead, iScaleDown, iScaleDown);

  s->fScanning = SANE_TRUE;
  s->fCancelled = SANE_FALSE;
  return SANE_STATUS_GOOD;
}


SANE_Status
sane_read (SANE_Handle h, SANE_Byte * buf, SANE_Int maxlen, SANE_Int * len)
{
  TScanner *s;
  TDataPipe *p;
  TModeParam const *pMode;

  DBG (DBG_MSG, "sane_read: buf=%p, maxlen=%d, ", buf, maxlen);

  s = (TScanner *) h;

  pMode = &modeParam[s->aValues[optMode].w];

  /* sane_read only allowed after sane_start */
  if (!s->fScanning)
    {
      if (s->fCancelled)
        {
          DBG (DBG_MSG, "\n");
          DBG (DBG_MSG, "sane_read: sane_read cancelled\n");
          s->fCancelled = SANE_FALSE;
          return SANE_STATUS_CANCELLED;
        }
      else
        {
          DBG (DBG_ERR,
               "sane_read: sane_read only allowed after sane_start\n");
          return SANE_STATUS_INVAL;
        }
    }

  p = &s->DataPipe;

  /* anything left to read? */
  if ((s->iLinesLeft == 0) && (s->iBytesLeft == 0))
    {
      CircBufferExit (p);
      free (p->pabLineBuf);
      p->pabLineBuf = NULL;
      FinishScan (&s->HWParams);
      *len = 0;
      DBG (DBG_MSG, "\n");
      DBG (DBG_MSG, "sane_read: end of scan\n");
      s->fCancelled = SANE_FALSE;
      s->fScanning = SANE_FALSE;
      return SANE_STATUS_EOF;
    }

  /* time to read the next line? */
  if (s->iBytesLeft == 0)
    {
      /* read a line from the transfer buffer */
      if (CircBufferGetLineEx (s->HWParams.iXferHandle, p, p->pabLineBuf,
                               s->HWParams.iReversedHead, SANE_TRUE))
        {
          pMode->adaptFormat (p->pabLineBuf, s->iPixelsPerLine,
                              s->aValues[optThreshold].w);
          s->iBytesLeft = pMode->bytesPerLine (s->iPixelsPerLine);
          s->iLinesLeft--;
        }
      /* stop scanning further, when the read action fails
         because we try read after the end of the buffer */
      else
        {
          FinishScan (&s->HWParams);
          CircBufferExit (p);
          free (p->pabLineBuf);
          p->pabLineBuf = NULL;
          *len = 0;
          DBG (DBG_MSG, "\n");
          DBG (DBG_MSG, "sane_read: read after end of buffer\n");
          s->fCancelled = SANE_FALSE;
          s->fScanning = SANE_FALSE;
          return SANE_STATUS_EOF;
        }

    }

  /* copy (part of) a line */
  *len = MIN (maxlen, s->iBytesLeft);
  memcpy (buf,
          &p->pabLineBuf[pMode->bytesPerLine (s->iPixelsPerLine) -
                         s->iBytesLeft], *len);
  s->iBytesLeft -= *len;

  DBG (DBG_MSG, " read=%d    \n", *len);

  return SANE_STATUS_GOOD;
}


void
sane_cancel (SANE_Handle h)
{
  TScanner *s;

  DBG (DBG_MSG, "sane_cancel\n");

  s = (TScanner *) h;
  /* Make sure the scanner head returns home */
  FinishScan (&s->HWParams);
  /* delete allocated data */
  if (s->fScanning)
    {
      CircBufferExit (&s->DataPipe);
      free (s->DataPipe.pabLineBuf);
      s->DataPipe.pabLineBuf = NULL;
      DBG (DBG_MSG, "sane_cancel: freeing buffers\n");
    }
  s->fCancelled = SANE_TRUE;
  s->fScanning = SANE_FALSE;
}


SANE_Status
sane_set_io_mode (SANE_Handle __sane_unused__ h, SANE_Bool m)
{
  DBG (DBG_MSG, "sane_set_io_mode %s\n", m ? "non-blocking" : "blocking");

  if (m)
    {
      return SANE_STATUS_UNSUPPORTED;
    }
  return SANE_STATUS_GOOD;
}


SANE_Status
sane_get_select_fd (SANE_Handle __sane_unused__ h, SANE_Int * __sane_unused__ fd)
{
  DBG (DBG_MSG, "sane_select_fd\n");
  return SANE_STATUS_UNSUPPORTED;
}
