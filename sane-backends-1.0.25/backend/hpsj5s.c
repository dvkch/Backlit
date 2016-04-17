/* sane - Scanner Access Now Easy.
   Copyright (C) 2002 Max Vorobiev <pcwizard@telecoms.sins.ru>
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
   If you do not wish that, delete this exception notice.  */

#define BUILD 3

#define BACKEND_NAME	hpsj5s
#define HPSJ5S_CONFIG_FILE "hpsj5s.conf"

#include "../include/sane/config.h"
#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_backend.h"

#include "hpsj5s.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


#define LINES_TO_FEED	480	/*Default feed length */

static int scanner_d = -1;	/*This is handler to the only-supported. Will be fixed. */
static char scanner_path[PATH_MAX] = "";	/*String for device-file */
static SANE_Byte bLastCalibration;	/*Here we store calibration result */
static SANE_Byte bCalibration;	/*Here we store new calibration value */
static SANE_Byte bHardwareState;	/*Here we store copy of hardware flags register */

/*Here we store Parameters:*/
static SANE_Word wWidth = 2570;	/*Scan area width */
static SANE_Word wResolution = 300;	/*Resolution in DPI */
static SANE_Frame wCurrentFormat = SANE_FRAME_GRAY;	/*Type of colors in image */
static SANE_Int wCurrentDepth = 8;	/*Bits per pixel in image */

/*Here we count lines of every new image...*/
static SANE_Word wVerticalResolution;

/*Limits for resolution control*/
static const SANE_Range ImageWidthRange = {
  0,				/*minimal */
  2570,				/*maximum */
  2				/*quant */
};

static const SANE_Word ImageResolutionsList[] = {
  6,				/*Number of resolutions */
  75,
  100,
  150,
  200,
  250,
  300
};

static SANE_Option_Descriptor sod[] = {
  {				/*Number of options */
   SANE_NAME_NUM_OPTIONS,
   SANE_TITLE_NUM_OPTIONS,
   SANE_DESC_NUM_OPTIONS,
   SANE_TYPE_INT,
   SANE_UNIT_NONE,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_NONE,
   {NULL}			/*No constraints required */
   }
  ,
  {				/*Width of scaned area */
   "width",
   "Width",
   "Width of area to scan",
   SANE_TYPE_INT,
   SANE_UNIT_PIXEL,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_RANGE,
   {NULL}			/*Range constrain setted in sane_init */
   }
  ,
  {				/*Resolution for scan */
   "resolution",
   "Resolution",
   "Image resolution",
   SANE_TYPE_INT,
   SANE_UNIT_DPI,
   sizeof (SANE_Word),
   SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
   SANE_CONSTRAINT_WORD_LIST,
   {NULL}			/*Word list constrain setted in sane_init */
   }
};

static SANE_Parameters parms;

/*Recalculate Lenght in dependace of resolution*/
static SANE_Word
LengthForRes (SANE_Word Resolution, SANE_Word Length)
{
  switch (Resolution)
    {
    case 75:
      return Length / 4;
    case 100:
      return Length / 3;
    case 150:
      return Length / 2;
    case 200:
      return Length * 2 / 3;
    case 250:
      return Length * 5 / 6;
    case 300:
    default:
      return Length;
    }
}

static struct parport_list pl;	/*List of detected parallel ports. */

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char line[PATH_MAX];		/*Line from config file */
  int len;			/*Length of string from config file */
  FILE *config_file;		/*Handle to config file of this backend */

  DBG_INIT ();
  DBG (1, ">>sane_init");
  DBG (2, "sane_init: version_code %s 0, authorize %s 0\n",
       version_code == 0 ? "=" : "!=", authorize == 0 ? "=" : "!=");
  DBG (1, "sane_init: SANE hpsj5s backend version %d.%d.%d\n",
       SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  /*Inform about supported version */
  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  /*Open configuration file for this backend */
  config_file = sanei_config_open (HPSJ5S_CONFIG_FILE);

  if (!config_file)		/*Failed to open config file */
    {
      DBG (1, "sane_init: no config file found.");
      return SANE_STATUS_GOOD;
    }

  /*Read line by line */
  while (sanei_config_read (line, PATH_MAX, config_file))
    {
      if ((line[0] == '#') || (line[0] == '\0'))	/*comment line or empty line */
	continue;
      len = strlen (line);	/*sanei_config_read guaranty, it's not more then PATH_MAX-1 */
      strcpy (scanner_path, line);	/*so, we choose last in file (uncommented) */
    }

  fclose (config_file);		/*We don't need config file any more */

  /*sanei_config_attach_matching_devices(devname, attach_one); To do latter */

  scanner_d = -1;		/*scanner device not opened yet. */
  DBG (1, "<<sane_init");

  /*Init params structure with defaults values: */
  wCurrentFormat = SANE_FRAME_GRAY;
  wCurrentDepth = 8;
  wWidth = 2570;
  wResolution = 300;

  /*Setup some option descriptors */
  sod[1].constraint.range = &ImageWidthRange;	/*Width option */
  sod[2].constraint.word_list = &ImageResolutionsList[0];	/*Resolution option */

  /*Search for ports in system: */
  ieee1284_find_ports (&pl, 0);

  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  if (scanner_d != -1)
    {
      CloseScanner (scanner_d);
      scanner_d = -1;
    }

  /*Free alocated ports information: */
  ieee1284_free_ports (&pl);

  DBG (2, "sane_exit\n");
  return;
}

/* Device select/open/close */
static const SANE_Device dev[] = {
  {
   "hpsj5s",
   "Hewlett-Packard",
   "ScanJet 5S",
   "sheetfed scanner"}
};

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  /*One device is supported and currently present */
  static const SANE_Device *devlist[] = {
    dev + 0, 0
  };

  /*No scanners presents */
  static const SANE_Device *void_devlist[] = { 0 };

  DBG (2, "sane_get_devices: local_only = %d\n", local_only);

  if (scanner_d != -1)		/*Device is opened, so it's present. */
    {
      *device_list = devlist;
      return SANE_STATUS_GOOD;
    };

  /*Device was not opened. */
  scanner_d = OpenScanner (scanner_path);

  if (scanner_d == -1)		/*No devices present */
    {
      DBG (1, "failed to open scanner.\n");
      *device_list = void_devlist;
      return SANE_STATUS_GOOD;
    }
  DBG (1, "port opened.\n");

  /*Check device. */
  DBG (1, "sane_get_devices: check scanner started.");
  if (DetectScanner () == 0)
    {				/*Device malfunction! */
      DBG (1, "sane_get_devices: Device malfunction.");
      *device_list = void_devlist;
      return SANE_STATUS_GOOD;
    }
  else
    {
      DBG (1, "sane_get_devices: Device works OK.");
      *device_list = devlist;
    }

  /*We do not need it any more */
  CloseScanner (scanner_d);
  scanner_d = -1;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  int i;

  if (!devicename)
    {
      DBG (1, "sane_open: devicename is NULL!");
      return SANE_STATUS_INVAL;
    }

  DBG (2, "sane_open: devicename = \"%s\"\n", devicename);

  if (!devicename[0])
    i = 0;
  else
    for (i = 0; i < NELEMS (dev); ++i)	/*Search for device in list */
      if (strcmp (devicename, dev[i].name) == 0)
	break;

  if (i >= NELEMS (dev))	/*No such device */
    return SANE_STATUS_INVAL;

  if (scanner_d != -1)		/*scanner opened already! */
    return SANE_STATUS_DEVICE_BUSY;

  DBG (1, "sane_open: scanner device path name is \'%s\'\n", scanner_path);

  scanner_d = OpenScanner (scanner_path);
  if (scanner_d == -1)
    return SANE_STATUS_DEVICE_BUSY;	/*This should be done more carefully */

  /*Check device. */
  DBG (1, "sane_open: check scanner started.");
  if (DetectScanner () == 0)
    {				/*Device malfunction! */
      DBG (1, "sane_open: Device malfunction.");
      CloseScanner (scanner_d);
      scanner_d = -1;
      return SANE_STATUS_IO_ERROR;
    }
  DBG (1, "sane_open: Device found.All are green.");
  *handle = (SANE_Handle) (unsigned long)scanner_d;

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  DBG (2, "sane_close\n");
  /*We support only single device - so ignore handle (FIX IT LATER) */
  if ((handle != (SANE_Handle) (unsigned long)scanner_d) || (scanner_d == -1))
    return;			/* wrong device */
  StandByScanner ();
  CloseScanner (scanner_d);
  scanner_d = -1;
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  DBG (2, "sane_get_option_descriptor: option = %d\n", option);
  if ((handle != (SANE_Handle) (unsigned long)scanner_d) || (scanner_d == -1))
    return NULL;		/* wrong device */

  if (option < 0 || option >= NELEMS (sod))	/*No real options supported */
    return NULL;

  return &sod[option];		/*Return demanded option */
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *value, SANE_Int * info)
{
  if ((handle != (SANE_Handle) (unsigned long)scanner_d) || (scanner_d == -1))
    return SANE_STATUS_INVAL;	/* wrong device */

  if ((option >= NELEMS (sod)) || (option < 0))	/*Supported only this option */
    return SANE_STATUS_INVAL;

  switch (option)
    {
    case 0:			/*Number of options */
      if (action != SANE_ACTION_GET_VALUE)	/*It can be only read */
	return SANE_STATUS_INVAL;

      *((SANE_Int *) value) = NELEMS (sod);
      return SANE_STATUS_GOOD;
    case 1:			/*Scan area width */
      switch (action)
	{
	case SANE_ACTION_GET_VALUE:
	  *((SANE_Word *) value) = wWidth;
	  return SANE_STATUS_GOOD;
	case SANE_ACTION_SET_VALUE:	/*info should be setted */
	  wWidth = *((SANE_Word *) value);
	  if (info != NULL)
	    *info = SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;
	default:
	  return SANE_STATUS_INVAL;
	}
    case 2:			/*Resolution */
      switch (action)
	{
	case SANE_ACTION_GET_VALUE:
	  *((SANE_Word *) value) = wResolution;
	  return SANE_STATUS_GOOD;
	case SANE_ACTION_SET_VALUE:	/*info should be setted */
	  wResolution = *((SANE_Word *) value);
	  if (info != NULL)
	    *info = 0;
	  return SANE_STATUS_GOOD;
	default:
	  return SANE_STATUS_INVAL;
	}
    default:
      return SANE_STATUS_INVAL;
    }
  return SANE_STATUS_GOOD;	/*For now we have no options to control */
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  DBG (2, "sane_get_parameters\n");

  if ((handle != (SANE_Handle) (unsigned long)scanner_d) || (scanner_d == -1))
    return SANE_STATUS_INVAL;	/* wrong device */

  /*Ignore handle parameter for now. FIX it latter. */
  /*These parameters are OK for gray scale mode. */
  parms.depth = /*wCurrentDepth */ 8;
  parms.format = /*wCurrentFormat */ SANE_FRAME_GRAY;
  parms.last_frame = SANE_TRUE;	/*For grayscale... */
  parms.lines = -1;		/*Unknown a priory */
  parms.pixels_per_line = LengthForRes (wResolution, wWidth);	/*For grayscale... */
  parms.bytes_per_line = parms.pixels_per_line;	/*For grayscale... */
  *params = parms;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  int i;
  DBG (2, "sane_start\n");

  if ((handle != (SANE_Handle) (unsigned long)scanner_d) || (scanner_d == -1))
    return SANE_STATUS_IO_ERROR;

  CallFunctionWithParameter (0x93, 2);
  bLastCalibration = CallFunctionWithRetVal (0xA9);
  if (bLastCalibration == 0)
    bLastCalibration = -1;

  /*Turn on the lamp: */
  CallFunctionWithParameter (FUNCTION_SETUP_HARDWARE, FLAGS_HW_LAMP_ON);
  bHardwareState = FLAGS_HW_LAMP_ON;
  /*Get average white point */
  bCalibration = GetCalibration ();

  if (bLastCalibration - bCalibration > 16)
    {				/*Lamp is not warm enouth */
      DBG (1, "sane_start: warming lamp for 30 sec.\n");
      for (i = 0; i < 30; i++)
	sleep (1);
    }

  /*Check paper presents */
  if (CheckPaperPresent () == 0)
    {
      DBG (1, "sane_start: no paper detected.");
      return SANE_STATUS_NO_DOCS;
    }
  CalibrateScanElements ();
  TransferScanParameters (GrayScale, wResolution, wWidth);
  /*Turn on indicator and prepare engine. */
  SwitchHardwareState (FLAGS_HW_INDICATOR_OFF | FLAGS_HW_MOTOR_READY, 1);
  /*Feed paper */
  if (PaperFeed (LINES_TO_FEED) == 0)	/*Feed only for fixel lenght. Change it */
    {
      DBG (1, "sane_start: paper feed failed.");
      SwitchHardwareState (FLAGS_HW_INDICATOR_OFF | FLAGS_HW_MOTOR_READY, 0);
      return SANE_STATUS_JAMMED;
    }
  /*Set paper moving speed */
  TurnOnPaperPulling (GrayScale, wResolution);

  wVerticalResolution = 0;	/*Reset counter */

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * data,
	   SANE_Int max_length, SANE_Int * length)
{
  SANE_Byte bFuncResult, bTest;
  int timeout;

  if (!length)
    {
      DBG (1, "sane_read: length == NULL\n");
      return SANE_STATUS_INVAL;
    }
  *length = 0;
  if (!data)
    {
      DBG (1, "sane_read: data == NULL\n");
      return SANE_STATUS_INVAL;
    }

  if ((handle != (SANE_Handle) (unsigned long)scanner_d) || (scanner_d == -1))
    {
      DBG (1, "sane_read: unknown handle\n");
      return SANE_STATUS_INVAL;
    }

  /*While end of paper sheet was not reached */
  /*Wait for scaned line ready */
  timeout = 0;
  while (((bFuncResult = CallFunctionWithRetVal (0xB2)) & 0x20) == 0)
    {
      bTest = CallFunctionWithRetVal (0xB5);
      usleep (1);
      timeout++;
      if ((timeout < 1000) &&
	  (((bTest & 0x80) && ((bTest & 0x3F) <= 2)) ||
	   (((bTest & 0x80) == 0) && ((bTest & 0x3F) >= 5))))
	continue;

      if (timeout >= 1000)
	continue;		/*do it again! */

      /*Data ready state! */

      if ((bFuncResult & 0x20) != 0)	/*End of paper reached! */
	{
	  *length = 0;
	  return SANE_STATUS_EOF;
	}

      /*Data ready */
      *length = LengthForRes (wResolution, wWidth);
      if (*length >= max_length)
	*length = max_length;

      CallFunctionWithParameter (0xCD, 0);
      CallFunctionWithRetVal (0xC8);
      WriteScannerRegister (REGISTER_FUNCTION_CODE, 0xC8);
      WriteAddress (ADDRESS_RESULT);
      /*Test if we need this line for current resolution
         (scanner doesn't control vertical resolution in hardware) */
      wVerticalResolution -= wResolution;
      if (wVerticalResolution > 0)
	{
	  timeout = 0;
	  continue;
	}
      else
	wVerticalResolution = 300;	/*Reset counter */

      ReadDataBlock (data, *length);

      /*switch indicator */
      bHardwareState ^= FLAGS_HW_INDICATOR_OFF;
      CallFunctionWithParameter (FUNCTION_SETUP_HARDWARE, bHardwareState);
      return SANE_STATUS_GOOD;
    }
  return SANE_STATUS_EOF;
}

void
sane_cancel (SANE_Handle handle)
{
  DBG (2, "sane_cancel: handle = %p\n", handle);
  /*Stop motor */
  TurnOffPaperPulling ();

  /*Indicator turn off */
  bHardwareState |= FLAGS_HW_INDICATOR_OFF;
  CallFunctionWithParameter (FUNCTION_SETUP_HARDWARE, bHardwareState);

  /*Get out of paper */
  ReleasePaper ();

  /*Restore indicator */
  bHardwareState &= ~FLAGS_HW_INDICATOR_OFF;
  CallFunctionWithParameter (FUNCTION_SETUP_HARDWARE, bHardwareState);

  bLastCalibration = CallFunctionWithRetVal (0xA9);
  CallFunctionWithParameter (0xA9, bLastCalibration);
  CallFunctionWithParameter (0x93, 4);

}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  DBG (2, "sane_set_io_mode: handle = %p, non_blocking = %d\n", handle,
       non_blocking);
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  DBG (2, "sane_get_select_fd: handle = %p, fd %s 0\n", handle,
       fd ? "!=" : "=");
  return SANE_STATUS_UNSUPPORTED;
}

/*
        Middle-level API:
*/

/*
        Detect if scanner present and works correctly.
        Ret Val: 0 = detection failed, 1 = detection OK.
*/
static int
DetectScanner (void)
{
  int Result1, Result2;
  int Successful, Total;

  Result1 = OutputCheck ();
  Result2 = InputCheck ();

  if (!(Result1 || Result2))	/*If all are 0 - it's error */
    {
      return 0;
    }

  WriteScannerRegister (0x7C, 0x80);
  WriteScannerRegister (0x7F, 0x1);
  WriteScannerRegister (0x72, 0x10);
  WriteScannerRegister (0x72, 0x90);
  WriteScannerRegister (0x7C, 0x24);
  WriteScannerRegister (0x75, 0x0C);
  WriteScannerRegister (0x78, 0x0);
  WriteScannerRegister (0x79, 0x10);
  WriteScannerRegister (0x71, 0x10);
  WriteScannerRegister (0x71, 0x1);
  WriteScannerRegister (0x72, 0x1);

  for (Successful = 0, Total = 0; Total < 5; Total++)
    {
      if (CallCheck ())
	Successful++;
      if (Successful >= 3)
	return 1;		/*Correct and Stable */
    }
  return 0;
}

static void
StandByScanner ()
{
  WriteScannerRegister (0x74, 0x80);
  WriteScannerRegister (0x75, 0x0C);
  WriteScannerRegister (0x77, 0x0);
  WriteScannerRegister (0x78, 0x0);
  WriteScannerRegister (0x79, 0x0);
  WriteScannerRegister (0x7A, 0x0);
  WriteScannerRegister (0x7B, 0x0);
  WriteScannerRegister (0x7C, 0x4);
  WriteScannerRegister (0x70, 0x0);
  WriteScannerRegister (0x72, 0x90);
  WriteScannerRegister (0x70, 0x0);
}

static void
SwitchHardwareState (SANE_Byte mask, SANE_Byte invert_mask)
{
  if (!invert_mask)
    {
      bHardwareState &= ~mask;
    }
  else
    bHardwareState |= mask;

  CallFunctionWithParameter (FUNCTION_SETUP_HARDWARE, bHardwareState);
}

/*return value: 0 - no paper, 1 - paper loaded.*/
static int
CheckPaperPresent ()
{
  if ((CallFunctionWithRetVal (0xB2) & 0x10) == 0)
    return 1;			/*Ok - paper present. */
  return 0;			/*No paper present */
}

static int
ReleasePaper ()
{
  int i;

  if ((CallFunctionWithRetVal (0xB2) & 0x20) == 0)
    {				/*End of paper was not reached */
      CallFunctionWithParameter (0xA7, 0xF);
      CallFunctionWithParameter (0xA8, 0xFF);
      CallFunctionWithParameter (0xC2, 0);

      for (i = 0; i < 90000; i++)
	{
	  if (CallFunctionWithRetVal (0xB2) & 0x80)
	    break;
	  usleep (1);
	}
      if (i >= 90000)
	return 0;		/*Fail. */

      for (i = 0; i < 90000; i++)
	{
	  if ((CallFunctionWithRetVal (0xB2) & 0x20) == 0)
	    break;
	  else if ((CallFunctionWithRetVal (0xB2) & 0x80) == 0)
	    {
	      i = 90000;
	      break;
	    }
	  usleep (1);
	}

      CallFunctionWithParameter (0xC5, 0);

      if (i >= 90000)
	return 0;		/*Fail. */

      while (CallFunctionWithRetVal (0xB2) & 0x80);	/*Wait bit dismiss */

      CallFunctionWithParameter (0xA7, 1);
      CallFunctionWithParameter (0xA8, 0x25);
      CallFunctionWithParameter (0xC2, 0);

      for (i = 0; i < 90000; i++)
	{
	  if (CallFunctionWithRetVal (0xB2) & 0x80)
	    break;
	  usleep (1);
	}
      if (i >= 90000)
	return 0;		/*Fail. */

      for (i = 0; i < 90000; i++)
	{
	  if ((CallFunctionWithRetVal (0xB2) & 0x80) == 0)
	    break;
	  usleep (1);
	}
      if (i >= 90000)
	return 0;		/*Fail. */
    }

  if (CallFunctionWithRetVal (0xB2) & 0x10)
    {
      CallFunctionWithParameter (0xA7, 1);
      CallFunctionWithParameter (0xA8, 0x40);
    }
  else
    {
      CallFunctionWithParameter (0xA7, 0);
      CallFunctionWithParameter (0xA8, 0xFA);
    }
  CallFunctionWithParameter (0xC2, 0);

  for (i = 0; i < 9000; i++)
    {
      if (CallFunctionWithRetVal (0xB2) & 0x80)
	break;
      usleep (1);
    }
  if (i >= 9000)
    return 0;			/*Fail. */

  while (CallFunctionWithRetVal (0xB2) & 0x80)
    usleep (1);

  return 1;
}

static void
TransferScanParameters (enumColorDepth enColor, SANE_Word wResolution,
			SANE_Word wPixelsLength)
{
  SANE_Word wRightBourder = (2570 + wPixelsLength) / 2 + 65;
  SANE_Word wLeftBourder = (2570 - wPixelsLength) / 2 + 65;

  switch (enColor)
    {
    case Drawing:
      CallFunctionWithParameter (0x90, 2);	/*Not supported correctle. FIX ME!!! */
      break;
    case Halftone:
      CallFunctionWithParameter (0x90, 0xE3);	/*Not supported correctly. FIX ME!!! */
      CallFunctionWithParameter (0x92, 3);
      break;
    case GrayScale:
    case TrueColor:
      CallFunctionWithParameter (0x90, 0);	/*Not suppoted correctly. FIX ME!!! */
      break;
    };
  CallFunctionWithParameter (0xA1, 2);
  CallFunctionWithParameter (0xA2, 1);
  CallFunctionWithParameter (0xA3, 0x98);
  /*Resolution: */
  CallFunctionWithParameter (0x9A, (SANE_Byte) (wResolution >> 8));	/*High byte */
  CallFunctionWithParameter (0x9B, (SANE_Byte) wResolution);	/*Low byte */

  LoadingPaletteToScanner ();

  CallFunctionWithParameter (0xA4, 31);	/*Some sort of constant parameter */
  /*Left bourder */
  CallFunctionWithParameter (0xA5, wLeftBourder / 256);
  CallFunctionWithParameter (0xA6, wLeftBourder % 256);
  /*Right bourder */
  CallFunctionWithParameter (0xAA, wRightBourder / 256);
  CallFunctionWithParameter (0xAB, wRightBourder % 256);

  CallFunctionWithParameter (0xD0, 0);
  CallFunctionWithParameter (0xD1, 0);
  CallFunctionWithParameter (0xD2, 0);
  CallFunctionWithParameter (0xD3, 0);
  CallFunctionWithParameter (0xD4, 0);
  CallFunctionWithParameter (0xD5, 0);

  CallFunctionWithParameter (0x9D, 5);
}

static void
TurnOnPaperPulling (enumColorDepth enColor, SANE_Word wResolution)
{
  switch (enColor)
    {
    case Drawing:
    case Halftone:
      CallFunctionWithParameter (0x91, 0xF7);
      return;
    case GrayScale:
      switch (wResolution)
	{
	case 50:
	case 75:
	case 100:
	  CallFunctionWithParameter (0x91, 0xB7);
	  return;
	case 150:
	case 200:
	  CallFunctionWithParameter (0x91, 0x77);
	  return;
	case 250:
	case 300:
	  CallFunctionWithParameter (0x91, 0x37);
	  return;
	default:
	  return;
	}
    case TrueColor:
      switch (wResolution)
	{
	case 75:
	case 100:
	  CallFunctionWithParameter (0x91, 0xA3);
	  return;
	case 150:
	case 200:
	  CallFunctionWithParameter (0x91, 0x53);
	  return;
	case 250:
	case 300:
	  CallFunctionWithParameter (0x91, 0x3);
	  return;
	default:
	  return;
	}
    default:
      return;
    }
}

static void
TurnOffPaperPulling ()
{
  CallFunctionWithParameter (0x91, 0);
}

/*
        Returns avarage value of scaned row.
        While paper not loaded this is base "white point".
*/
static SANE_Byte
GetCalibration ()
{
  int i;
  int Result;
  SANE_Byte Buffer[2600];
  SANE_Byte bTest;

  CallFunctionWithParameter (0xA1, 2);
  CallFunctionWithParameter (0xA2, 1);
  CallFunctionWithParameter (0xA3, 0x98);

  /*Resolution to 300 DPI */
  CallFunctionWithParameter (0x9A, 1);
  CallFunctionWithParameter (0x9B, 0x2C);

  CallFunctionWithParameter (0x92, 0);
  CallFunctionWithParameter (0xC6, 0);
  CallFunctionWithParameter (0x92, 0x80);

  for (i = 1; i < 256; i++)
    CallFunctionWithParameter (0xC6, i);

  for (i = 0; i < 256; i++)
    CallFunctionWithParameter (0xC6, i);

  for (i = 0; i < 256; i++)
    CallFunctionWithParameter (0xC6, i);

  CallFunctionWithParameter (0xA4, 31);	/*Some sort of constant */

  /*Left bourder */
  CallFunctionWithParameter (0xA5, 0);
  CallFunctionWithParameter (0xA6, 0x41);

  /*Right bourder */
  CallFunctionWithParameter (0xAA, 0xA);
  CallFunctionWithParameter (0xAB, 0x39);

  CallFunctionWithParameter (0xD0, 0);
  CallFunctionWithParameter (0xD1, 0);
  CallFunctionWithParameter (0xD2, 0);
  CallFunctionWithParameter (0xD3, 0);
  CallFunctionWithParameter (0xD4, 0);
  CallFunctionWithParameter (0xD5, 0);

  CallFunctionWithParameter (0x9C, 0x1B);
  CallFunctionWithParameter (0x9D, 5);

  CallFunctionWithParameter (0x92, 0x10);
  CallFunctionWithParameter (0xC6, 0xFF);
  CallFunctionWithParameter (0x92, 0x90);

  for (i = 0; i < 2999; i++)
    CallFunctionWithParameter (0xC6, 0xFF);

  CallFunctionWithParameter (0x92, 0x50);
  CallFunctionWithParameter (0xC6, 0);
  CallFunctionWithParameter (0x92, 0xD0);

  for (i = 0; i < 2999; i++)
    CallFunctionWithParameter (0xC6, 0);

  CallFunctionWithParameter (0x98, 0xFF);	/*Up limit */
  CallFunctionWithParameter (0x95, 0);	/*Low limit */

  CallFunctionWithParameter (0x90, 0);	/*Gray scale... */

  CallFunctionWithParameter (0x91, 0x3B);	/*Turn motor on. */

  for (i = 0; i < 5; i++)
    {
      do
	{			/*WARNING!!! Deadlock possible! */
	  bTest = CallFunctionWithRetVal (0xB5);
	}
      while ((((bTest & 0x80) == 1) && ((bTest & 0x3F) <= 2)) ||
	     (((bTest & 0x80) == 0) && ((bTest & 0x3F) >= 5)));

      CallFunctionWithParameter (0xCD, 0);
      /*Skip this line for ECP: */
      CallFunctionWithRetVal (0xC8);

      WriteScannerRegister (REGISTER_FUNCTION_CODE, 0xC8);
      WriteAddress (0x20);
      ReadDataBlock (Buffer, 2552);
    };
  CallFunctionWithParameter (0x91, 0);	/*Turn off motor. */
  usleep (10);
  for (Result = 0, i = 0; i < 2552; i++)
    Result += Buffer[i];
  return Result / 2552;
}

static int
PaperFeed (SANE_Word wLinesToFeed)
{
  int i;

  CallFunctionWithParameter (0xA7, 0xF);
  CallFunctionWithParameter (0xA8, 0xFF);
  CallFunctionWithParameter (0xC2, 0);

  for (i = 0; i < 9000; i++)
    {
      if (CallFunctionWithRetVal (0xB2) & 0x80)
	break;
      usleep (1);
    }
  if (i >= 9000)
    return 0;			/*Fail. */

  for (i = 0; i < 9000; i += 5)
    {
      if ((CallFunctionWithRetVal (0xB2) & 0x20) == 0)
	break;
      else if ((CallFunctionWithRetVal (0xB2) & 0x80) == 0)
	{
	  i = 9000;
	  break;
	}
      usleep (5);
    }

  CallFunctionWithParameter (0xC5, 0);

  if (i >= 9000)
    return 0;			/*Fail. */

  /*Potential deadlock */
  while (CallFunctionWithRetVal (0xB2) & 0x80);	/*Wait bit dismiss */

  CallFunctionWithParameter (0xA7, wLinesToFeed / 256);
  CallFunctionWithParameter (0xA8, wLinesToFeed % 256);
  CallFunctionWithParameter (0xC2, 0);

  for (i = 0; i < 9000; i++)
    {
      if (CallFunctionWithRetVal (0xB2) & 0x80)
	break;
      usleep (1);
    }
  if (i >= 9000)
    return 0;			/*Fail. */

  for (i = 0; i < 9000; i++)
    {
      if ((CallFunctionWithRetVal (0xB2) & 0x80) == 0)
	break;
      usleep (1);
    }
  if (i >= 9000)
    return 0;			/*Fail. */

  return 1;
}

/*For now we do no calibrate elements - just set maximum limits. FIX ME?*/
static void
CalibrateScanElements ()
{
  /*Those arrays will be used in future for correct calibration. */
  /*Then we need to transfer UP brightness border, we use these registers */
  SANE_Byte arUpTransferBorders[] = { 0x10, 0x20, 0x30 };
  /*Then we need to transfer LOW brightness border, we use these registers */
  SANE_Byte arLowTransferBorders[] = { 0x50, 0x60, 0x70 };
  /*Then we need to save UP brightness border, we use these registers */
  SANE_Byte arUpSaveBorders[] = { 0x98, 0x97, 0x99 };
  /*Then we need to save LOW brightness border, we use these registers */
  SANE_Byte arLowSaveBorders[] = { 0x95, 0x94, 0x96 };
  /*Speeds, used for calibration */
  SANE_Byte arSpeeds[] = { 0x3B, 0x37, 0x3F };
  int j, Average, Temp, Index, /* Line, */ timeout,Calibration;
  SANE_Byte bTest /*, Min, Max, Result */ ;
  /*For current color component: (values from arrays). Next two lines - starting and terminating. */

  SANE_Byte CurrentUpTransferBorder;
  SANE_Byte CurrentLowTransferBorder;
  SANE_Byte CurrentUpSaveBorder;
  SANE_Byte CurrentLowSaveBorder;
  SANE_Byte CurrentSpeed1, CurrentSpeed2; 
  SANE_Byte CorrectionValue;
  SANE_Byte FilteredBuffer[2570];

  CallFunctionWithParameter (0xA1, 2);
  CallFunctionWithParameter (0xA2, 0);
  CallFunctionWithParameter (0xA3, 0x98);

  /*DPI = 300 */
  CallFunctionWithParameter (0x9A, 1);	/*High byte */
  CallFunctionWithParameter (0x9B, 0x2C);	/*Low byte */

  /*Paletter settings. */
  CallFunctionWithParameter (0x92, 0);
  CallFunctionWithParameter (0xC6, 0);
  CallFunctionWithParameter (0x92, 0x80);

  /*First color component */
  for (j = 1; j < 256; j++)
    CallFunctionWithParameter (0xC6, j);

  /*Second color component */
  for (j = 0; j < 256; j++)
    CallFunctionWithParameter (0xC6, j);

  /*Third color component */
  for (j = 0; j < 256; j++)
    CallFunctionWithParameter (0xC6, j);

  CallFunctionWithParameter (0xA4, 31);

  /*Left border */
  CallFunctionWithParameter (0xA5, 0);	/*High byte */
  CallFunctionWithParameter (0xA6, 0x41);	/*Low byte */

  /*Right border */
  CallFunctionWithParameter (0xAA, 0xA);	/*High byte */
  CallFunctionWithParameter (0xAB, 0x4B);	/*Low byte */

  /*Zero these registers... */
  CallFunctionWithParameter (0xD0, 0);
  CallFunctionWithParameter (0xD1, 0);
  CallFunctionWithParameter (0xD2, 0);
  CallFunctionWithParameter (0xD3, 0);
  CallFunctionWithParameter (0xD4, 0);
  CallFunctionWithParameter (0xD5, 0);

  CallFunctionWithParameter (0x9C, 0x1B);
  CallFunctionWithParameter (0x9D, 0x5);

  Average = 0;
  for (Index = 0; Index < 3; Index++)	/*For theree color components */
    {
      /*Up border = 0xFF */
      CallFunctionWithParameter (0x92, arUpTransferBorders[Index]);
      CallFunctionWithParameter (0xC6, 0xFF);
      CallFunctionWithParameter (0x92, arUpTransferBorders[Index] | 0x80);

      for (j = 2999; j > 0; j--)
	CallFunctionWithParameter (0xC6, 0xFF);

      /*Low border = 0x0 */
      CallFunctionWithParameter (0x92, arLowTransferBorders[Index]);
      CallFunctionWithParameter (0xC6, 0x0);
      CallFunctionWithParameter (0x92, arLowTransferBorders[Index] | 0x80);

      for (j = 2999; j > 0; j--)
	CallFunctionWithParameter (0xC6, 0x0);

      /*Save borders */
      CallFunctionWithParameter (arUpSaveBorders[Index], 0xFF);
      CallFunctionWithParameter (arLowSaveBorders[Index], 0x0);
      CallFunctionWithParameter (0x90, 0);	/*Gray Scale or True color sign :) */

      CallFunctionWithParameter (0x91, arSpeeds[Index]);

      /*waiting for scaned line... */
      timeout = 0;
      do
	{
	  bTest = CallFunctionWithRetVal (0xB5);
	  timeout++;
	  usleep (1);
	}
      while ((timeout < 1000) &&
	     ((((bTest & 0x80) == 1) && ((bTest & 0x3F) <= 2)) ||
	      (((bTest & 0x80) == 0) && ((bTest & 0x3F) >= 5))));

      /*Let's read it... */
      if(timeout < 1000)
      {
        CallFunctionWithParameter (0xCD, 0);
        CallFunctionWithRetVal (0xC8);
        WriteScannerRegister (0x70, 0xC8);
        WriteAddress (0x20);

        ReadDataBlock (FilteredBuffer, 2570);
      }

      CallFunctionWithParameter (0x91, 0);	/*Stop engine. */

     /*Note: if first read failed, junk would be calculated, but if previous
	read was succeded, but last one failed, previous data'ld be used.
     */
     for(Temp = 0, j = 0; j < 2570; j++)
     Temp += FilteredBuffer[j];
     Temp /= 2570;

     if((Average == 0)||(Average > Temp))
     Average = Temp; 
    }

    for(Index = 0; Index < 3; Index++) /*Three color components*/
    {
	CurrentUpTransferBorder = arUpTransferBorders[Index];
	CallFunctionWithParameter (0xC6, 0xFF);
	CallFunctionWithParameter (0x92, CurrentUpTransferBorder|0x80);
	for(j=2999; j>0; j--)
	    CallFunctionWithParameter (0xC6, 0xFF);

	CurrentLowTransferBorder = arLowTransferBorders[Index];
	CallFunctionWithParameter (0xC6, 0x0);
	CallFunctionWithParameter (0x92, CurrentLowTransferBorder|0x80);
	for(j=2999; j>0; j--)
	    CallFunctionWithParameter (0xC6, 0);
	
	CurrentUpSaveBorder = arUpSaveBorders[Index];
	CallFunctionWithParameter (CurrentUpSaveBorder, 0xFF);

	CurrentLowSaveBorder = arLowSaveBorders[Index];
	CallFunctionWithParameter (CurrentLowSaveBorder, 0x0);
	CallFunctionWithParameter (0x90,0);
	Calibration = 0x80;
	CallFunctionWithParameter (CurrentUpSaveBorder, 0x80);
	
	CurrentSpeed1 = CurrentSpeed2 = arSpeeds[Index];
	
	for(CorrectionValue = 0x40; CorrectionValue != 0;CorrectionValue >>= 2)
	{
	    CallFunctionWithParameter (0x91, CurrentSpeed2);
	    usleep(10);

    	    /*waiting for scaned line... */
	    for(j = 0; j < 5; j++)
	    {
    		timeout = 0;
    		do
		{
		    bTest = CallFunctionWithRetVal (0xB5);
		    timeout++;
		    usleep (1);
		}
    		while ((timeout < 1000) &&
	    	((((bTest & 0x80) == 1) && ((bTest & 0x3F) <= 2)) ||
	        (((bTest & 0x80) == 0) && ((bTest & 0x3F) >= 5))));

    		/*Let's read it... */
    		if(timeout < 1000)
    		{
    		    CallFunctionWithParameter (0xCD, 0);
        	    CallFunctionWithRetVal (0xC8);
		    WriteScannerRegister (0x70, 0xC8);
    	    	    WriteAddress (0x20);

    		    ReadDataBlock (FilteredBuffer, 2570);
    		}
	    }/*5 times we read. I don't understand what for, but so does HP's driver.
		Perhaps, we can optimize it in future.*/
	    WriteScannerRegister (0x91, 0);
	    usleep(10);
	    
	    for(Temp = 0,j = 0; j < 16;j++)
		Temp += FilteredBuffer[509+j]; /*At this offset calcalates HP's driver.*/
	    Temp /= 16;
	    
	    if(Average > Temp)
	    {
		Calibration += CorrectionValue;
		Calibration = 0xFF < Calibration ? 0xFF : Calibration; /*min*/
	    }
	    else
		Calibration -= CorrectionValue;
	    
	    WriteScannerRegister (CurrentUpSaveBorder, Calibration);
	}/*By CorrectionValue we tune UpSaveBorder*/

	WriteScannerRegister (0x90, 8);
	WriteScannerRegister (0x91, CurrentSpeed1);
	usleep(10);
    }/*By color components*/

  return;
}

/*
      Internal use functions:
*/

/*Returns 0 in case of fail and 1 in success.*/
static int
OutputCheck ()
{
  int i;

  WriteScannerRegister (0x7F, 0x1);
  WriteAddress (0x7E);
  for (i = 0; i < 256; i++)
    WriteData ((SANE_Byte) i);

  WriteAddress (0x3F);
  if (ReadDataByte () & 0x80)
    return 0;

  return 1;
}

static int
InputCheck ()
{
  int i;
  SANE_Byte Buffer[256];

  WriteAddress (0x3E);
  for (i = 0; i < 256; i++)
    {
      Buffer[i] = ReadDataByte ();
    }

  for (i = 0; i < 256; i++)
    {
      if (Buffer[i] != i)
	return 0;
    }

  return 1;
}

static int
CallCheck ()
{
  int i;
  SANE_Byte Buffer[256];

  CallFunctionWithParameter (0x92, 0x10);
  CallFunctionWithParameter (0xC6, 0x0);
  CallFunctionWithParameter (0x92, 0x90);
  WriteScannerRegister (REGISTER_FUNCTION_CODE, 0xC6);

  WriteAddress (0x60);

  for (i = 1; i < 256; i++)
    WriteData ((SANE_Byte) i);

  CallFunctionWithParameter (0x92, 0x10);
  CallFunctionWithRetVal (0xC6);
  CallFunctionWithParameter (0x92, 0x90);
  WriteScannerRegister (REGISTER_FUNCTION_CODE, 0xC6);

  WriteAddress (ADDRESS_RESULT);

  ReadDataBlock (Buffer, 256);

  for (i = 0; i < 255; i++)
    {
      if (Buffer[i + 1] != (SANE_Byte) i)
	return 0;
    }
  return 1;
}

static void
LoadingPaletteToScanner ()
{
  /*For now we have statical gamma. */
  SANE_Byte Gamma[256];
  int i;
  for (i = 0; i < 256; i++)
    Gamma[i] = i;

  CallFunctionWithParameter (0x92, 0);
  CallFunctionWithParameter (0xC6, Gamma[0]);
  CallFunctionWithParameter (0x92, 0x80);
  for (i = 1; i < 256; i++)
    CallFunctionWithParameter (0xC6, Gamma[i]);

  for (i = 0; i < 256; i++)
    CallFunctionWithParameter (0xC6, Gamma[i]);

  for (i = 0; i < 256; i++)
    CallFunctionWithParameter (0xC6, Gamma[i]);
}

/*
        Low level warappers:
*/
static void
WriteAddress (SANE_Byte Address)
{
  ieee1284_data_dir (pl.portv[scanner_d], 0);	/*Forward mode */
  ieee1284_frob_control (pl.portv[scanner_d], C1284_NINIT, C1284_NINIT);
  ieee1284_epp_write_addr (pl.portv[scanner_d], 0, (char *) &Address, 1);
}

static void
WriteData (SANE_Byte Data)
{
  ieee1284_data_dir (pl.portv[scanner_d], 0);	/*Forward mode */
  ieee1284_frob_control (pl.portv[scanner_d], C1284_NINIT, C1284_NINIT);
  ieee1284_epp_write_data (pl.portv[scanner_d], 0, (char *) &Data, 1);
}

static void
WriteScannerRegister (SANE_Byte Address, SANE_Byte Data)
{
  WriteAddress (Address);
  WriteData (Data);
}

static void
CallFunctionWithParameter (SANE_Byte Function, SANE_Byte Parameter)
{
  WriteScannerRegister (REGISTER_FUNCTION_CODE, Function);
  WriteScannerRegister (REGISTER_FUNCTION_PARAMETER, Parameter);
}

static SANE_Byte
CallFunctionWithRetVal (SANE_Byte Function)
{
  WriteScannerRegister (REGISTER_FUNCTION_CODE, Function);
  WriteAddress (ADDRESS_RESULT);
  return ReadDataByte ();
}

static SANE_Byte
ReadDataByte ()
{
  SANE_Byte Result;

  ieee1284_data_dir (pl.portv[scanner_d], 1);	/*Reverse mode */
  ieee1284_frob_control (pl.portv[scanner_d], C1284_NINIT, C1284_NINIT);
  ieee1284_epp_read_data (pl.portv[scanner_d], 0, (char *) &Result, 1);
  return Result;
}

static void
ReadDataBlock (SANE_Byte * Buffer, int length)
{

  ieee1284_data_dir (pl.portv[scanner_d], 1);	/*Reverse mode */
  ieee1284_frob_control (pl.portv[scanner_d], C1284_NINIT, C1284_NINIT);
  ieee1284_epp_read_data (pl.portv[scanner_d], 0, (char *) Buffer, length);
}

/* Send a daisy-chain-style CPP command packet. */
int
cpp_daisy (struct parport *port, int cmd)
{
  unsigned char s;

  ieee1284_data_dir (port, 0);	/*forward direction */
  ieee1284_write_control (port, C1284_NINIT);
  ieee1284_write_data (port, 0xaa);
  usleep (2);
  ieee1284_write_data (port, 0x55);
  usleep (2);
  ieee1284_write_data (port, 0x00);
  usleep (2);
  ieee1284_write_data (port, 0xff);
  usleep (2);
  s = ieee1284_read_status (port) ^ S1284_INVERTED;	/*Converted for PC-style */

  s &= (S1284_BUSY | S1284_PERROR | S1284_SELECT | S1284_NFAULT);

  if (s != (S1284_BUSY | S1284_PERROR | S1284_SELECT | S1284_NFAULT))
    {
      DBG (1, "%s: cpp_daisy: aa5500ff(%02x)\n", port->name, s);
      return -1;
    }

  ieee1284_write_data (port, 0x87);
  usleep (2);
  s = ieee1284_read_status (port) ^ S1284_INVERTED;	/*Convert to PC-style */

  s &= (S1284_BUSY | S1284_PERROR | S1284_SELECT | S1284_NFAULT);

  if (s != (S1284_SELECT | S1284_NFAULT))
    {
      DBG (1, "%s: cpp_daisy: aa5500ff87(%02x)\n", port->name, s);
      return -1;
    }

  ieee1284_write_data (port, 0x78);
  usleep (2);
  ieee1284_write_control (port, C1284_NINIT);
  ieee1284_write_data (port, cmd);
  usleep (2);
  ieee1284_frob_control (port, C1284_NSTROBE, C1284_NSTROBE);
  usleep (1);
  ieee1284_frob_control (port, C1284_NSTROBE, 0);
  usleep (1);
  s = ieee1284_read_status (port);
  ieee1284_write_data (port, 0xff);
  usleep (2);

  return s;
}

/*Daisy chain deselect operation.*/
void
daisy_deselect_all (struct parport *port)
{
  cpp_daisy (port, 0x30);
}

/*Daisy chain select operation*/
int
daisy_select (struct parport *port, int daisy, int mode)
{
  switch (mode)
    {
      /*For these modes we should switch to EPP mode: */
    case M1284_EPP:
    case M1284_EPPSL:
    case M1284_EPPSWE:
      return cpp_daisy (port, 0x20 + daisy) & S1284_NFAULT;
      /*For these modes we should switch to ECP mode: */
    case M1284_ECP:
    case M1284_ECPRLE:
    case M1284_ECPSWE:
      return cpp_daisy (port, 0xd0 + daisy) & S1284_NFAULT;
      /*Nothing was told for BECP in Daisy chain specification.
         May be it's wise to use ECP? */
    case M1284_BECP:
      /*Others use compat mode */
    case M1284_NIBBLE:
    case M1284_BYTE:
    case M1284_COMPAT:
    default:
      return cpp_daisy (port, 0xe0 + daisy) & S1284_NFAULT;
    }
}

/*Daisy chain assign address operation.*/
int
assign_addr (struct parport *port, int daisy)
{
  return cpp_daisy (port, daisy);
}

static int
OpenScanner (const char *scanner_path)
{
  int handle;
  int caps;

  /*Scaner name was specified in config file?*/
  if (strlen(scanner_path) == 0)
    return -1;

  for (handle = 0; handle < pl.portc; handle++)
    {
      if (strcmp (scanner_path, pl.portv[handle]->name) == 0)
	break;
    }
  if (handle == pl.portc)	/*No match found */
    return -1;

  /*Open port */
  if (ieee1284_open (pl.portv[handle], 0, &caps) != E1284_OK)
    return -1;

  /*Claim port */
  if (ieee1284_claim (pl.portv[handle]) != E1284_OK)
    return -1;

  /*Total chain reset. */
  daisy_deselect_all (pl.portv[handle]);

  /*Assign addresses. */
  assign_addr (pl.portv[handle], 0);	/*Assume we have device first in chain. */

  /*Select required device. For now - first in chain. */
  daisy_select (pl.portv[handle], 0, M1284_EPP);

  return handle;
}

static void
CloseScanner (int handle)
{
  if (handle == -1)
    return;

  daisy_deselect_all (pl.portv[handle]);

  ieee1284_release (pl.portv[handle]);

  ieee1284_close (pl.portv[handle]);
}
