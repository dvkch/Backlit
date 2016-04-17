/* sane - Scanner Access Now Easy.
   Copyright (C) 2003 Martijn van Oosterhout <kleptog@svana.org>
   Copyright (C) 2003 Thomas Soumarmon <thomas.soumarmon@cogitae.net>

   Originally copied from HP3300 testtools. Original notice follows:

   Copyright (C) 2001 Bertrik Sikken (bertrik@zonnet.nl)

   This file is part of the SANE package.

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


/*
    SANE interface for hp54xx scanners. Prototype.
    Parts of this source were inspired by other backends.
*/

#include "../include/sane/config.h"

/* definitions for debug */
#include "hp5400_debug.h"

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_usb.h"

#include <stdlib.h>         /* malloc, free */
#include <string.h>         /* memcpy */
#include <stdio.h>
#include <errno.h>

#define HP5400_CONFIG_FILE "hp5400.conf"

#include "hp5400.h"

/* includes for data transfer methods */
#include "hp5400.h"

#ifdef STANDALONE
#include "hp5400_scanner.h"
#endif

#if defined(LINUX_USB_SUPPORT)
  #include "hp5400_linux.c"
#endif
#if defined(USCANNER_SUPPORT)
  #include "hp5400_uscanner.c"
#endif
#if defined(LIBUSB_SUPPORT)
  #include "hp5400_libusb.c"
#endif
#if defined(LIBIEEE1284_SUPPORT)
  #include "hp5400_ieee1284.c"
#endif



/* other definitions */
#ifndef min
#define min(A,B) (((A)<(B)) ? (A) : (B))
#endif
#ifndef max
#define max(A,B) (((A)>(B)) ? (A) : (B))
#endif

#define TRUE 1
#define FALSE 0

#define MM_TO_PIXEL(_mm_, _dpi_)    ((_mm_) * (_dpi_) / 25.4)
#define PIXEL_TO_MM(_pixel_, _dpi_) ((_pixel_) * 25.4 / (_dpi_))

#define NUM_GAMMA_ENTRIES  65536


/* options enumerator */
typedef enum
{
  optCount = 0,

  optGroupGeometry,
  optTLX, optTLY, optBRX, optBRY,
  optDPI,

  optGroupImage,

  optGammaTableRed,		/* Gamma Tables */
  optGammaTableGreen,
  optGammaTableBlue,

  optLast,			/* Disable the offset code */

  optGroupMisc,
  optOffsetX, optOffsetY


/* put temporarily disabled options here after optLast */
/*
  optLamp,
*/

}
EOptionIndex;

typedef union
{
  SANE_Word w;
  SANE_Word *wa;		/* word array */
  SANE_String s;
}
TOptionValue;


typedef struct
{
  SANE_Option_Descriptor aOptions[optLast];
  TOptionValue aValues[optLast];

  TScanParams ScanParams;
  THWParams HWParams;

  TDataPipe DataPipe;
  int iLinesLeft;

  SANE_Int *aGammaTableR;	/* a 16-to-16 bit color lookup table */
  SANE_Int *aGammaTableG;	/* a 16-to-16 bit color lookup table */
  SANE_Int *aGammaTableB;	/* a 16-to-16 bit color lookup table */

  int fScanning;		/* TRUE if actively scanning */
  int fCanceled;
}
TScanner;


/* linked list of SANE_Device structures */
typedef struct TDevListEntry
{
  struct TDevListEntry *pNext;
  SANE_Device dev;
  char* devname;
}
TDevListEntry;



/* Device filename for USB access */
char usb_devfile[128];

static TDevListEntry *_pFirstSaneDev = 0;
static int iNumSaneDev = 0;


static const SANE_Device **_pSaneDevList = 0;

/* option constraints */
static const SANE_Range rangeGammaTable = {0, 65535, 1};
#ifdef SUPPORT_2400_DPI
static const SANE_Int   setResolutions[] = {6, 75, 150, 300, 600, 1200, 2400};
#else
static const SANE_Int   setResolutions[] = {5, 75, 150, 300, 600, 1200};
#endif
static const SANE_Range rangeXmm = {0, 220, 1};
static const SANE_Range rangeYmm = {0, 300, 1};
static const SANE_Range rangeXoffset = {0, 20, 1};
static const SANE_Range rangeYoffset = {0, 70, 1};
static const SANE_Int offsetX = 5;
static const SANE_Int offsetY = 52;


static void _InitOptions(TScanner *s)
{
  int i, j;
  SANE_Option_Descriptor *pDesc;
  TOptionValue *pVal;

  /* set a neutral gamma */
  if( s->aGammaTableR == NULL )   /* Not yet allocated */
  {
    s->aGammaTableR = malloc( NUM_GAMMA_ENTRIES * sizeof( SANE_Int ) );
    s->aGammaTableG = malloc( NUM_GAMMA_ENTRIES * sizeof( SANE_Int ) );
    s->aGammaTableB = malloc( NUM_GAMMA_ENTRIES * sizeof( SANE_Int ) );

    for (j = 0; j < NUM_GAMMA_ENTRIES; j++) {
      s->aGammaTableR[j] = j;
      s->aGammaTableG[j] = j;
      s->aGammaTableB[j] = j;
    }
  }

  for (i = optCount; i < optLast; i++) {

    pDesc = &s->aOptions[i];
    pVal = &s->aValues[i];

    /* defaults */
    pDesc->name   = "";
    pDesc->title  = "";
    pDesc->desc   = "";
    pDesc->type   = SANE_TYPE_INT;
    pDesc->unit   = SANE_UNIT_NONE;
    pDesc->size   = sizeof(SANE_Word);
    pDesc->constraint_type = SANE_CONSTRAINT_NONE;
    pDesc->cap    = 0;

    switch (i) {

    case optCount:
      pDesc->title  = SANE_TITLE_NUM_OPTIONS;
      pDesc->desc   = SANE_DESC_NUM_OPTIONS;
      pDesc->cap    = SANE_CAP_SOFT_DETECT;
      pVal->w       = (SANE_Word)optLast;
      break;

    case optGroupGeometry:
      pDesc->title  = "Geometry";
      pDesc->type   = SANE_TYPE_GROUP;
      pDesc->size   = 0;
      break;

    case optTLX:
      pDesc->name   = SANE_NAME_SCAN_TL_X;
      pDesc->title  = SANE_TITLE_SCAN_TL_X;
      pDesc->desc   = SANE_DESC_SCAN_TL_X;
      pDesc->unit   = SANE_UNIT_MM;
      pDesc->constraint_type  = SANE_CONSTRAINT_RANGE;
      pDesc->constraint.range = &rangeXmm;
      pDesc->cap    = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      pVal->w       = rangeXmm.min + offsetX;
      break;

    case optTLY:
      pDesc->name   = SANE_NAME_SCAN_TL_Y;
      pDesc->title  = SANE_TITLE_SCAN_TL_Y;
      pDesc->desc   = SANE_DESC_SCAN_TL_Y;
      pDesc->unit   = SANE_UNIT_MM;
      pDesc->constraint_type  = SANE_CONSTRAINT_RANGE;
      pDesc->constraint.range = &rangeYmm;
      pDesc->cap    = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      pVal->w       = rangeYmm.min + offsetY;
      break;

    case optBRX:
      pDesc->name   = SANE_NAME_SCAN_BR_X;
      pDesc->title  = SANE_TITLE_SCAN_BR_X;
      pDesc->desc   = SANE_DESC_SCAN_BR_X;
      pDesc->unit   = SANE_UNIT_MM;
      pDesc->constraint_type  = SANE_CONSTRAINT_RANGE;
      pDesc->constraint.range = &rangeXmm;
      pDesc->cap    = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      pVal->w       = rangeXmm.max + offsetX;
      break;

    case optBRY:
      pDesc->name   = SANE_NAME_SCAN_BR_Y;
      pDesc->title  = SANE_TITLE_SCAN_BR_Y;
      pDesc->desc   = SANE_DESC_SCAN_BR_Y;
      pDesc->unit   = SANE_UNIT_MM;
      pDesc->constraint_type  = SANE_CONSTRAINT_RANGE;
      pDesc->constraint.range = &rangeYmm;
      pDesc->cap    = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      pVal->w       = rangeYmm.max + offsetY;
      break;

    case optDPI:
      pDesc->name   = SANE_NAME_SCAN_RESOLUTION;
      pDesc->title  = SANE_TITLE_SCAN_RESOLUTION;
      pDesc->desc   = SANE_DESC_SCAN_RESOLUTION;
      pDesc->unit   = SANE_UNIT_DPI;
      pDesc->constraint_type  = SANE_CONSTRAINT_WORD_LIST;
      pDesc->constraint.word_list = setResolutions;
      pDesc->cap    = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      pVal->w       = setResolutions[1];
      break;

    case optGroupImage:
      pDesc->title  = SANE_I18N("Image");
      pDesc->type   = SANE_TYPE_GROUP;
      pDesc->size   = 0;
      break;

    case optGammaTableRed:
      pDesc->name   = SANE_NAME_GAMMA_VECTOR_R;
      pDesc->title  = SANE_TITLE_GAMMA_VECTOR_R;
      pDesc->desc   = SANE_DESC_GAMMA_VECTOR_R;
      pDesc->size   = NUM_GAMMA_ENTRIES * sizeof( SANE_Int );
      pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
      pDesc->constraint.range = &rangeGammaTable;
      pDesc->cap    = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      pVal->wa      = s->aGammaTableR;
      break;

    case optGammaTableGreen:
      pDesc->name   = SANE_NAME_GAMMA_VECTOR_G;
      pDesc->title  = SANE_TITLE_GAMMA_VECTOR_G;
      pDesc->desc   = SANE_DESC_GAMMA_VECTOR_G;
      pDesc->size   = NUM_GAMMA_ENTRIES * sizeof( SANE_Int );
      pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
      pDesc->constraint.range = &rangeGammaTable;
      pDesc->cap    = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      pVal->wa      = s->aGammaTableG;
      break;

    case optGammaTableBlue:
      pDesc->name   = SANE_NAME_GAMMA_VECTOR_B;
      pDesc->title  = SANE_TITLE_GAMMA_VECTOR_B;
      pDesc->desc   = SANE_DESC_GAMMA_VECTOR_B;
      pDesc->size   = NUM_GAMMA_ENTRIES * sizeof( SANE_Int );
      pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
      pDesc->constraint.range = &rangeGammaTable;
      pDesc->cap    = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      pVal->wa      = s->aGammaTableB;
      break;

    case optGroupMisc:
      pDesc->title  = SANE_I18N("Miscellaneous");
      pDesc->type   = SANE_TYPE_GROUP;
      pDesc->size   = 0;
      break;

    case optOffsetX:
      pDesc->title  = SANE_I18N("offset X");
      pDesc->desc   = SANE_I18N("Hardware internal X position of the scanning area.");
      pDesc->unit   = SANE_UNIT_MM;
      pDesc->constraint_type  = SANE_CONSTRAINT_RANGE;
      pDesc->constraint.range = &rangeXoffset;
      pDesc->cap    = SANE_CAP_SOFT_SELECT;
      pVal->w       = offsetX;
      break;

    case optOffsetY:
      pDesc->title  = SANE_I18N("offset Y");
      pDesc->desc   = SANE_I18N("Hardware internal Y position of the scanning area.");
      pDesc->unit   = SANE_UNIT_MM;
      pDesc->constraint_type  = SANE_CONSTRAINT_RANGE;
      pDesc->constraint.range = &rangeYoffset;
      pDesc->cap    = SANE_CAP_SOFT_SELECT;
      pVal->w       = offsetY;
      break;


#if 0
    case optLamp:
      pDesc->name   = "lamp";
      pDesc->title  = SANE_I18N("Lamp status");
      pDesc->desc   = SANE_I18N("Switches the lamp on or off.");
      pDesc->type   = SANE_TYPE_BOOL;
      pDesc->cap    = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
      /* switch the lamp on when starting for first the time */
      pVal->w       = SANE_TRUE;
      break;
#endif
#if 0
    case optCalibrate:
      pDesc->name   = "calibrate";
      pDesc->title  = SANE_I18N("Calibrate");
      pDesc->desc   = SANE_I18N("Calibrates for black and white level.");
      pDesc->type   = SANE_TYPE_BUTTON;
      pDesc->cap    = SANE_CAP_SOFT_SELECT;
      pDesc->size   = 0;
      break;
#endif
    default:
      HP5400_DBG(DBG_ERR, "Uninitialised option %d\n", i);
      break;
    }
  }
}


static int _ReportDevice(TScannerModel *pModel, const char *pszDeviceName)
{
  TDevListEntry *pNew, *pDev;

  HP5400_DBG(DBG_MSG, "hp5400: _ReportDevice '%s'\n", pszDeviceName);

  pNew = malloc(sizeof(TDevListEntry));
  if (!pNew) {
    HP5400_DBG(DBG_ERR, "no mem\n");
    return -1;
  }

  /* add new element to the end of the list */
  if (_pFirstSaneDev == NULL) {
    _pFirstSaneDev = pNew;
  }
  else {
    for (pDev = _pFirstSaneDev; pDev->pNext; pDev = pDev->pNext) {
      ;
    }
    pDev->pNext = pNew;
  }

  /* fill in new element */
  pNew->pNext = 0;
  /* we use devname to avoid having to free a const
   * pointer */
  pNew->devname = (char*)strdup(pszDeviceName);
  pNew->dev.name = pNew->devname;
  pNew->dev.vendor = pModel->pszVendor;
  pNew->dev.model = pModel->pszName;
  pNew->dev.type = "flatbed scanner";

  iNumSaneDev++;

  return 0;
}

static SANE_Status
attach_one_device (SANE_String_Const devname)
{
  const char * filename = (const char*) devname;
  if (HP5400Detect (filename, _ReportDevice) < 0)
    {
      HP5400_DBG (DBG_MSG, "attach_one_device: couldn't attach %s\n", devname);
      return SANE_STATUS_INVAL;
    }
  HP5400_DBG (DBG_MSG, "attach_one_device: attached %s successfully\n", devname);
  return SANE_STATUS_GOOD;
}


/*****************************************************************************/

SANE_Status
sane_init (SANE_Int * piVersion, SANE_Auth_Callback pfnAuth)
{
  FILE *conf_fp;		/* Config file stream  */
  SANE_Char line[PATH_MAX];
  SANE_Char *str = NULL;
  SANE_String_Const proper_str;
  int nline = 0;

  /* prevent compiler from complaing about unused parameters */
  pfnAuth = pfnAuth;

  strcpy(usb_devfile, "/dev/usb/scanner0");
  _pFirstSaneDev = 0;
  iNumSaneDev = 0;

  InitHp5400_internal();


  DBG_INIT ();

  HP5400_DBG (DBG_MSG, "sane_init: SANE hp5400 backend version %d.%d-%d (from %s)\n",
       SANE_CURRENT_MAJOR, V_MINOR, BUILD, PACKAGE_STRING);

  sanei_usb_init ();

  conf_fp = sanei_config_open (HP5400_CONFIG_FILE);

  iNumSaneDev = 0;

  if (conf_fp)
    {
      HP5400_DBG (DBG_MSG, "Reading config file\n");

      while (sanei_config_read (line, sizeof (line), conf_fp))
	{
	  ++nline;

	  if (str)
	    {
	      free (str);
	    }

	  proper_str = sanei_config_get_string (line, &str);

	  /* Discards white lines and comments */
	  if (!str || proper_str == line || str[0] == '#')
	    {
	      HP5400_DBG (DBG_MSG, "Discarding line %d\n", nline);
	    }
	  else
	    {
	      /* If line's not blank or a comment, then it's the device
	       * filename or a usb directive. */
	      HP5400_DBG (DBG_MSG, "Trying to attach %s\n", line);
	      sanei_usb_attach_matching_devices (line, attach_one_device);
	    }
	}			/* while */
      fclose (conf_fp);
    }
  else
    {
      HP5400_DBG (DBG_ERR, "Unable to read config file \"%s\": %s\n",
	   HP5400_CONFIG_FILE, strerror (errno));
      HP5400_DBG (DBG_MSG, "Using default built-in values\n");
      attach_one_device (usb_devfile);
    }

  if (piVersion != NULL)
    {
      *piVersion = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);
    }


  return SANE_STATUS_GOOD;
}


void
sane_exit (void)
{
  TDevListEntry *pDev, *pNext;
  HP5400_DBG (DBG_MSG, "sane_exit\n");

  /* free device list memory */
  if (_pSaneDevList)
    {
      for (pDev = _pFirstSaneDev; pDev; pDev = pNext)
	{
	  pNext = pDev->pNext;
	  free (pDev->devname);
	  /* pDev->dev.name is the same pointer that pDev->devname */
	  free (pDev);
	}
      _pFirstSaneDev = 0;
      free (_pSaneDevList);
      _pSaneDevList = 0;
    }


	FreeHp5400_internal();
}


SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  TDevListEntry *pDev;
  int i;

  HP5400_DBG (DBG_MSG, "sane_get_devices\n");

  local_only = local_only;

  if (_pSaneDevList)
    {
      free (_pSaneDevList);
    }

  _pSaneDevList = malloc (sizeof (*_pSaneDevList) * (iNumSaneDev + 1));
  if (!_pSaneDevList)
    {
      HP5400_DBG (DBG_MSG, "no mem\n");
      return SANE_STATUS_NO_MEM;
    }
  i = 0;
  for (pDev = _pFirstSaneDev; pDev; pDev = pDev->pNext)
    {
      _pSaneDevList[i++] = &pDev->dev;
    }
  _pSaneDevList[i++] = 0;	/* last entry is 0 */

  *device_list = _pSaneDevList;

  return SANE_STATUS_GOOD;
}


SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * h)
{
  TScanner *s;

  HP5400_DBG (DBG_MSG, "sane_open: %s\n", name);

  /* check the name */
  if (strlen (name) == 0)
    {
      /* default to first available device */
      name = _pFirstSaneDev->dev.name;
    }

  s = malloc (sizeof (TScanner));
  if (!s)
    {
      HP5400_DBG (DBG_MSG, "malloc failed\n");
      return SANE_STATUS_NO_MEM;
    }

  memset (s, 0, sizeof (TScanner));	/* Clear everything to zero */
  if (HP5400Open (&s->HWParams, name) < 0)
    {
      /* is this OK ? */
      HP5400_DBG (DBG_ERR, "HP5400Open failed\n");
      free ((void *) s);
      return SANE_STATUS_INVAL;	/* is this OK? */
    }
  HP5400_DBG (DBG_MSG, "Handle=%d\n", s->HWParams.iXferHandle);
  _InitOptions (s);
  *h = s;

  /* Turn on lamp by default at startup */
/*  SetLamp(&s->HWParams, TRUE);  */

  return SANE_STATUS_GOOD;
}


void
sane_close (SANE_Handle h)
{
  TScanner *s;

  HP5400_DBG (DBG_MSG, "sane_close\n");

  s = (TScanner *) h;

  /* turn of scanner lamp */
  SetLamp (&s->HWParams, FALSE);

  /* close scanner */
  HP5400Close (&s->HWParams);

  /* free scanner object memory */
  free ((void *) s);
}


const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle h, SANE_Int n)
{
  TScanner *s;

  HP5400_DBG (DBG_MSG, "sane_get_option_descriptor %d\n", n);

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
  SANE_Int info;

  HP5400_DBG (DBG_MSG, "sane_control_option: option %d, action %d\n", n, Action);

  s = (TScanner *) h;
  info = 0;

  switch (Action)
    {
    case SANE_ACTION_GET_VALUE:
      switch (n)
	{

	  /* Get options of type SANE_Word */
	case optBRX:
	case optTLX:
	  *(SANE_Word *) pVal = s->aValues[n].w;	/* Not needed anymore  - s->aValues[optOffsetX].w; */
	  HP5400_DBG (DBG_MSG,
	       "sane_control_option: SANE_ACTION_GET_VALUE %d = %d\n", n,
	       *(SANE_Word *) pVal);
	  break;

	case optBRY:
	case optTLY:
	  *(SANE_Word *) pVal = s->aValues[n].w;	/* Not needed anymore - - s->aValues[optOffsetY].w; */
	  HP5400_DBG (DBG_MSG,
	       "sane_control_option: SANE_ACTION_GET_VALUE %d = %d\n", n,
	       *(SANE_Word *) pVal);
	  break;

	case optOffsetX:
	case optOffsetY:
	case optCount:
	case optDPI:
	  HP5400_DBG (DBG_MSG,
	       "sane_control_option: SANE_ACTION_GET_VALUE %d = %d\n", n,
	       (int) s->aValues[n].w);
	  *(SANE_Word *) pVal = s->aValues[n].w;
	  break;

	  /* Get options of type SANE_Word array */
	case optGammaTableRed:
	case optGammaTableGreen:
	case optGammaTableBlue:
	  HP5400_DBG (DBG_MSG, "Reading gamma table\n");
	  memcpy (pVal, s->aValues[n].wa, s->aOptions[n].size);
	  break;

#if 0
	  /* Get options of type SANE_Bool */
	case optLamp:
	  GetLamp (&s->HWParams, &fLampIsOn);
	  *(SANE_Bool *) pVal = fLampIsOn;
	  break;
#endif
#if 0
	case optCalibrate:
	  /*  although this option has nothing to read,
	     it's added here to avoid a warning when running scanimage --help */
	  break;
#endif
	default:
	  HP5400_DBG (DBG_MSG, "SANE_ACTION_GET_VALUE: Invalid option (%d)\n", n);
	}
      break;


    case SANE_ACTION_SET_VALUE:
      if (s->fScanning)
	{
	  HP5400_DBG (DBG_ERR,
	       "sane_control_option: SANE_ACTION_SET_VALUE not allowed during scan\n");
	  return SANE_STATUS_INVAL;
	}
      switch (n)
	{

	case optCount:
	  return SANE_STATUS_INVAL;
	  break;

	case optBRX:
	case optTLX:
	  info |= SANE_INFO_RELOAD_PARAMS;
	  s->ScanParams.iLines = 0;	/* Forget actual image settings */
	  s->aValues[n].w = *(SANE_Word *) pVal;	/* Not needed anymore - + s->aValues[optOffsetX].w; */
	  break;

	case optBRY:
	case optTLY:
	  info |= SANE_INFO_RELOAD_PARAMS;
	  s->ScanParams.iLines = 0;	/* Forget actual image settings */
	  s->aValues[n].w = *(SANE_Word *) pVal;	/* Not needed anymore - + s->aValues[optOffsetY].w; */
	  break;
	case optDPI:
	  info |= SANE_INFO_RELOAD_PARAMS;
	  s->ScanParams.iLines = 0;	/* Forget actual image settings */
#ifdef SUPPORT_2400_DPI
	  (s->aValues[n].w) = *(SANE_Word *) pVal;
#else
	  (s->aValues[n].w) = min (1200, *(SANE_Word *) pVal);
#endif
	  break;

	case optGammaTableRed:
	case optGammaTableGreen:
	case optGammaTableBlue:
	  HP5400_DBG (DBG_MSG, "Writing gamma table\n");
	  memcpy (s->aValues[n].wa, pVal, s->aOptions[n].size);
	  break;
/*
    case optLamp:
      fVal = *(SANE_Bool *)pVal;
      HP5400_DBG(DBG_MSG, "lamp %s\n", fVal ? "on" : "off");
      SetLamp(&s->HWParams, fVal);
      break;
*/
#if 0
	case optCalibrate:
/*       SimpleCalib(&s->HWParams); */
	  break;
#endif
	default:
	  HP5400_DBG (DBG_ERR, "SANE_ACTION_SET_VALUE: Invalid option (%d)\n", n);
	}
      if (pInfo != NULL)
	{
	  *pInfo = info;
	}
      break;

    case SANE_ACTION_SET_AUTO:
      return SANE_STATUS_UNSUPPORTED;


    default:
      HP5400_DBG (DBG_ERR, "Invalid action (%d)\n", Action);
      return SANE_STATUS_INVAL;
    }

  return SANE_STATUS_GOOD;
}



SANE_Status
sane_get_parameters (SANE_Handle h, SANE_Parameters * p)
{
  TScanner *s;
  HP5400_DBG (DBG_MSG, "sane_get_parameters\n");

  s = (TScanner *) h;

  /* first do some checks */
  if (s->aValues[optTLX].w >= s->aValues[optBRX].w)
    {
      HP5400_DBG (DBG_ERR, "TLX should be smaller than BRX\n");
      return SANE_STATUS_INVAL;	/* proper error code? */
    }
  if (s->aValues[optTLY].w >= s->aValues[optBRY].w)
    {
      HP5400_DBG (DBG_ERR, "TLY should be smaller than BRY\n");
      return SANE_STATUS_INVAL;	/* proper error code? */
    }

  /* return the data */
  p->format = SANE_FRAME_RGB;
  p->last_frame = SANE_TRUE;

  p->depth = 8;
  if (s->ScanParams.iLines)	/* Initialised by doing a scan */
    {
      p->pixels_per_line = s->ScanParams.iBytesPerLine / 3;
      p->lines = s->ScanParams.iLines;
      p->bytes_per_line = s->ScanParams.iBytesPerLine;
    }
  else
    {
      p->lines = MM_TO_PIXEL (s->aValues[optBRY].w - s->aValues[optTLY].w,
			      s->aValues[optDPI].w);
      p->pixels_per_line =
	MM_TO_PIXEL (s->aValues[optBRX].w - s->aValues[optTLX].w,
		     s->aValues[optDPI].w);
      p->bytes_per_line = p->pixels_per_line * 3;
    }

  return SANE_STATUS_GOOD;
}

#define BUFFER_READ_HEADER_SIZE 32

SANE_Status
sane_start (SANE_Handle h)
{
  TScanner *s;
  SANE_Parameters par;

  HP5400_DBG (DBG_MSG, "sane_start\n");

  s = (TScanner *) h;

  if (sane_get_parameters (h, &par) != SANE_STATUS_GOOD)
    {
      HP5400_DBG (DBG_MSG, "Invalid scan parameters (sane_get_parameters)\n");
      return SANE_STATUS_INVAL;
    }
  s->iLinesLeft = par.lines;

  /* fill in the scanparams using the option values */
  s->ScanParams.iDpi = s->aValues[optDPI].w;
  s->ScanParams.iLpi = s->aValues[optDPI].w;

  /* Guessing here. 75dpi => 1, 2400dpi => 32 */
  /*  s->ScanParams.iColourOffset = s->aValues[optDPI].w / 75; */
  /* now we don't need correction => corrected by scan request type ? */
  s->ScanParams.iColourOffset = 0;

  s->ScanParams.iTop =
    MM_TO_PIXEL (s->aValues[optTLY].w + s->HWParams.iTopLeftY, HW_LPI);
  s->ScanParams.iLeft =
    MM_TO_PIXEL (s->aValues[optTLX].w + s->HWParams.iTopLeftX, HW_DPI);

  /* Note: All measurements passed to the scanning routines must be in HW_LPI */
  s->ScanParams.iWidth =
    MM_TO_PIXEL (s->aValues[optBRX].w - s->aValues[optTLX].w, HW_LPI);
  s->ScanParams.iHeight =
    MM_TO_PIXEL (s->aValues[optBRY].w - s->aValues[optTLY].w, HW_LPI);

  /* After the scanning, the iLines and iBytesPerLine will be filled in */

  /* copy gamma table */
  WriteGammaCalibTable (s->HWParams.iXferHandle, s->aGammaTableR,
			s->aGammaTableG, s->aGammaTableB);

  /* prepare the actual scan */
  /* We say normal here. In future we should have a preview flag to set preview mode */
  if (InitScan (SCAN_TYPE_NORMAL, &s->ScanParams, &s->HWParams) != 0)
    {
      HP5400_DBG (DBG_MSG, "Invalid scan parameters (InitScan)\n");
      return SANE_STATUS_INVAL;
    }

  /* for the moment no lines has been read */
  s->ScanParams.iLinesRead = 0;

  s->fScanning = TRUE;
  return SANE_STATUS_GOOD;
}


SANE_Status
sane_read (SANE_Handle h, SANE_Byte * buf, SANE_Int maxlen, SANE_Int * len)
{

  /* Read actual scan from the circular buffer */
  /* Note: this is already color corrected, though some work still needs to be done
     to deal with the colour offsetting */
  TScanner *s;
  char *buffer = (char*)buf;

  HP5400_DBG (DBG_MSG, "sane_read: request %d bytes \n", maxlen);

  s = (TScanner *) h;

  /* nothing has been read for the moment */
  *len = 0;


  /* if we read all the lines return EOF */
  if (s->ScanParams.iLinesRead == s->ScanParams.iLines)
    {
/*    FinishScan( &s->HWParams );        *** FinishScan called in sane_cancel */
      HP5400_DBG (DBG_MSG, "sane_read: EOF\n");
      return SANE_STATUS_EOF;
    }

  /* read as many lines the buffer may contain and while there are lines to be read */
  while ((*len + s->ScanParams.iBytesPerLine <= maxlen)
	 && (s->ScanParams.iLinesRead < s->ScanParams.iLines))
    {

      /* get one more line from the circular buffer */
      CircBufferGetLine (s->HWParams.iXferHandle, &s->HWParams.pipe, buffer);

      /* increment pointer, size and line number */
      buffer += s->ScanParams.iBytesPerLine;
      *len += s->ScanParams.iBytesPerLine;
      s->ScanParams.iLinesRead++;
    }

  HP5400_DBG (DBG_MSG, "sane_read: %d bytes read\n", *len);

  return SANE_STATUS_GOOD;
}


void
sane_cancel (SANE_Handle h)
{
  TScanner *s;

  HP5400_DBG (DBG_MSG, "sane_cancel\n");

  s = (TScanner *) h;

  /* to be implemented more thoroughly */

  /* Make sure the scanner head returns home */
  FinishScan (&s->HWParams);

  s->fCanceled = TRUE;
  s->fScanning = FALSE;
}


SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool m)
{
  HP5400_DBG (DBG_MSG, "sane_set_io_mode %s\n", m ? "non-blocking" : "blocking");

  /* prevent compiler from complaining about unused parameters */
  h = h;

  if (m)
    {
      return SANE_STATUS_UNSUPPORTED;
    }
  return SANE_STATUS_GOOD;
}


SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int * fd)
{
  HP5400_DBG (DBG_MSG, "sane_select_fd\n");

  /* prevent compiler from complaining about unused parameters */
  h = h;
  fd = fd;

  return SANE_STATUS_UNSUPPORTED;
}
