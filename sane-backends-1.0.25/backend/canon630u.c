/* sane - Scanner Access Now Easy.

   Copyright (C) 2002, Nathan Rutman <nathan@gordian.com>
   Copyright (C) 2001, Marcio Luis Teixeira

   Parts copyright (C) 1996, 1997 Andreas Beck
   Parts copyright (C) 2000, 2001 Michael Herder <crapsite@gmx.net>
   Parts copyright (C) 2001 Henning Meier-Geinitz <henning@meier-geinitz.de>

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

#define BUILD 1
#define MM_IN_INCH 25.4

#include "../include/sane/config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_usb.h"
#define BACKEND_NAME        canon630u
#define CANONUSB_CONFIG_FILE "canon630u.conf"
#include "../include/sane/sanei_backend.h"

#include "canon630u-common.c"

typedef struct Canon_Device
{
  struct Canon_Device *next;
  SANE_String name;
  SANE_Device sane;
}
Canon_Device;

typedef struct Canon_Scanner
{
  struct Canon_Scanner *next;
  Canon_Device *device;
  CANON_Handle scan;
}
Canon_Scanner;

static int num_devices = 0;
static const SANE_Device **devlist = NULL;
static Canon_Device *first_dev = NULL;
static Canon_Scanner *first_handle = NULL;

static SANE_Parameters parms = {
  SANE_FRAME_RGB,
  0,
  0,				/* Number of bytes returned per scan line: */
  0,				/* Number of pixels per scan line.  */
  0,				/* Number of lines for the current scan.  */
  8				/* Number of bits per sample. */
};


struct _SANE_Option
{
  SANE_Option_Descriptor *descriptor;
    SANE_Status (*callback) (struct _SANE_Option * option, SANE_Handle handle,
			     SANE_Action action, void *value,
			     SANE_Int * info);
};

typedef struct _SANE_Option SANE_Option;


/*-----------------------------------------------------------------*/

static SANE_Word getNumberOfOptions (void);	/* Forward declaration */

/*
This read-only option returns the number of options available for
the device. It should be the first option in the options array
declared below.
*/

static SANE_Option_Descriptor optionNumOptionsDescriptor = {
  SANE_NAME_NUM_OPTIONS,
  SANE_TITLE_NUM_OPTIONS,
  SANE_DESC_NUM_OPTIONS,
  SANE_TYPE_INT,
  SANE_UNIT_NONE,
  sizeof (SANE_Word),
  SANE_CAP_SOFT_DETECT,
  SANE_CONSTRAINT_NONE,
  {NULL}
};

static SANE_Status
optionNumOptionsCallback (SANE_Option * option, SANE_Handle handle,
			  SANE_Action action, void *value, SANE_Int * info)
{
  option = option;
  handle = handle;
  info = info;			/* Eliminate warning about unused parameters */

  if (action != SANE_ACTION_GET_VALUE)
    return SANE_STATUS_INVAL;
  *(SANE_Word *) value = getNumberOfOptions ();
  return SANE_STATUS_GOOD;
}

/*-----------------------------------------------------------------*/

/*
This option lets the user force scanner calibration.  Normally, this is
done only once, at first scan after powerup.
 */

static SANE_Word optionCalibrateValue = SANE_FALSE;

static SANE_Option_Descriptor optionCalibrateDescriptor = {
  "cal",
  SANE_I18N ("Calibrate Scanner"),
  SANE_I18N ("Force scanner calibration before scan"),
  SANE_TYPE_BOOL,
  SANE_UNIT_NONE,
  sizeof (SANE_Word),
  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
  SANE_CONSTRAINT_NONE,
  {NULL}
};

static SANE_Status
optionCalibrateCallback (SANE_Option * option, SANE_Handle handle,
			 SANE_Action action, void *value, SANE_Int * info)
{
  handle = handle;
  option = option;		/* Eliminate warning about unused parameters */

  switch (action)
    {
    case SANE_ACTION_SET_AUTO:
      return SANE_STATUS_INVAL;
      break;
    case SANE_ACTION_SET_VALUE:
      *info |= SANE_INFO_RELOAD_PARAMS;
      optionCalibrateValue = *(SANE_Bool *) value;
      break;
    case SANE_ACTION_GET_VALUE:
      *(SANE_Word *) value = optionCalibrateValue;
      break;
    }
  return SANE_STATUS_GOOD;
}

/*-----------------------------------------------------------------*/

/*
This option lets the user select the scan resolution. The Canon fb630u
scanner supports the following resolutions: 75, 150, 300, 600, 1200
*/

static const SANE_Word optionResolutionList[] = {
  4,				/* Number of elements */
  75, 150, 300, 600		/* Resolution list */
    /* also 600x1200, but ignore that for now. */
};

static SANE_Option_Descriptor optionResolutionDescriptor = {
  SANE_NAME_SCAN_RESOLUTION,
  SANE_TITLE_SCAN_RESOLUTION,
  SANE_DESC_SCAN_RESOLUTION,
  SANE_TYPE_INT,
  SANE_UNIT_DPI,
  sizeof (SANE_Word),
  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_AUTOMATIC,
  SANE_CONSTRAINT_WORD_LIST,
  {(const SANE_String_Const *) optionResolutionList}
};

static SANE_Word optionResolutionValue = 75;

static SANE_Status
optionResolutionCallback (SANE_Option * option, SANE_Handle handle,
			  SANE_Action action, void *value, SANE_Int * info)
{
  SANE_Status status;
  SANE_Word autoValue = 75;

  handle = handle;		/* Eliminate warning about unused parameters */

  switch (action)
    {
    case SANE_ACTION_SET_AUTO:
      status =
	sanei_constrain_value (option->descriptor, (void *) &autoValue, info);
      if (status != SANE_STATUS_GOOD)
	return status;
      optionResolutionValue = autoValue;
      *info |= SANE_INFO_RELOAD_PARAMS;
      break;
    case SANE_ACTION_SET_VALUE:
      *info |= SANE_INFO_RELOAD_PARAMS;
      optionResolutionValue = *(SANE_Word *) value;
      break;
    case SANE_ACTION_GET_VALUE:
      *(SANE_Word *) value = optionResolutionValue;
      break;
    }
  return SANE_STATUS_GOOD;
}

/*-----------------------------------------------------------------*/

#ifdef GRAY
/*
This option lets the user select a gray scale scan
*/
static SANE_Word optionGrayscaleValue = SANE_FALSE;

static SANE_Option_Descriptor optionGrayscaleDescriptor = {
  "gray",
  SANE_I18N ("Grayscale scan"),
  SANE_I18N ("Do a grayscale rather than color scan"),
  SANE_TYPE_BOOL,
  SANE_UNIT_NONE,
  sizeof (SANE_Word),
  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
  SANE_CONSTRAINT_NONE,
  {NULL}
};

static SANE_Status
optionGrayscaleCallback (SANE_Option * option, SANE_Handle handle,
			 SANE_Action action, void *value, SANE_Int * info)
{
  handle = handle;
  option = option;		/* Eliminate warning about unused parameters */

  switch (action)
    {
    case SANE_ACTION_SET_AUTO:
      return SANE_STATUS_INVAL;
      break;
    case SANE_ACTION_SET_VALUE:
      *info |= SANE_INFO_RELOAD_PARAMS;
      optionGrayscaleValue = *(SANE_Bool *) value;
      break;
    case SANE_ACTION_GET_VALUE:
      *(SANE_Word *) value = optionGrayscaleValue;
      break;
    }
  return SANE_STATUS_GOOD;
}
#endif /* GRAY */

/*-----------------------------------------------------------------*/

/* Analog Gain setting */
static const SANE_Range aGainRange = {
  0,				/* minimum */
  64,				/* maximum */
  1				/* quantization */
};

static SANE_Int optionAGainValue = 1;

static SANE_Option_Descriptor optionAGainDescriptor = {
  "gain",
  SANE_I18N ("Analog Gain"),
  SANE_I18N ("Increase or decrease the analog gain of the CCD array"),
  SANE_TYPE_INT,
  SANE_UNIT_NONE,
  sizeof (SANE_Int),
  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED,
  SANE_CONSTRAINT_RANGE,
  {(const SANE_String_Const *) &aGainRange}
};

static SANE_Status
optionAGainCallback (SANE_Option * option, SANE_Handle handle,
		     SANE_Action action, void *value, SANE_Int * info)
{
  option = option;
  handle = handle;
  info = info;			/* Eliminate warning about unused parameters */

  switch (action)
    {
    case SANE_ACTION_SET_AUTO:
      return SANE_STATUS_INVAL;
      break;
    case SANE_ACTION_SET_VALUE:
      optionAGainValue = *(SANE_Int *) value;
      *info |= SANE_INFO_RELOAD_PARAMS;
      break;
    case SANE_ACTION_GET_VALUE:
      *(SANE_Int *) value = optionAGainValue;
      break;
    }
  return SANE_STATUS_GOOD;
}

/*-----------------------------------------------------------------*/

/* Scanner gamma setting */
static SANE_Fixed optionGammaValue = SANE_FIX (1.6);

static SANE_Option_Descriptor optionGammaDescriptor = {
  "gamma",
  SANE_I18N ("Gamma Correction"),
  SANE_I18N ("Selects the gamma corrected transfer curve"),
  SANE_TYPE_FIXED,
  SANE_UNIT_NONE,
  sizeof (SANE_Int),
  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED,
  SANE_CONSTRAINT_NONE,
  {NULL}
};


static SANE_Status
optionGammaCallback (SANE_Option * option, SANE_Handle handle,
		     SANE_Action action, void *value, SANE_Int * info)
{
  option = option;
  handle = handle;
  info = info;			/* Eliminate warning about unused parameters */

  switch (action)
    {
    case SANE_ACTION_SET_AUTO:
      return SANE_STATUS_INVAL;
      break;
    case SANE_ACTION_SET_VALUE:
      optionGammaValue = *(SANE_Fixed *) value;
      *info |= SANE_INFO_RELOAD_PARAMS;
      break;
    case SANE_ACTION_GET_VALUE:
      *(SANE_Fixed *) value = optionGammaValue;
      break;
    }
  return SANE_STATUS_GOOD;
}

/*-----------------------------------------------------------------*/

/*
Scan range
*/

static const SANE_Range widthRange = {
  0,				/* minimum */
  SANE_FIX (CANON_MAX_WIDTH * MM_IN_INCH / 600),	/* maximum */
  0				/* quantization */
};

static const SANE_Range heightRange = {
  0,				/* minimum */
  SANE_FIX (CANON_MAX_HEIGHT * MM_IN_INCH / 600),	/* maximum */
  0				/* quantization */
};

/*-----------------------------------------------------------------*/
/*
This option controls the top-left-x corner of the scan
*/

static SANE_Fixed optionTopLeftXValue = 0;

static SANE_Option_Descriptor optionTopLeftXDescriptor = {
  SANE_NAME_SCAN_TL_X,
  SANE_TITLE_SCAN_TL_X,
  SANE_DESC_SCAN_TL_X,
  SANE_TYPE_FIXED,
  SANE_UNIT_MM,
  sizeof (SANE_Fixed),
  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
  SANE_CONSTRAINT_RANGE,
  {(const SANE_String_Const *) &widthRange}
};

static SANE_Status
optionTopLeftXCallback (SANE_Option * option, SANE_Handle handle,
			SANE_Action action, void *value, SANE_Int * info)
{
  option = option;
  handle = handle;
  value = value;		/* Eliminate warning about unused parameters */

  switch (action)
    {
    case SANE_ACTION_SET_AUTO:
      return SANE_STATUS_INVAL;
      break;
    case SANE_ACTION_SET_VALUE:
      optionTopLeftXValue = *(SANE_Fixed *) value;
      *info |= SANE_INFO_RELOAD_PARAMS;
      break;
    case SANE_ACTION_GET_VALUE:
      *(SANE_Fixed *) value = optionTopLeftXValue;
      break;
    }
  return SANE_STATUS_GOOD;
}

/*-----------------------------------------------------------------*/
/*
This option controls the top-left-y corner of the scan
*/

static SANE_Fixed optionTopLeftYValue = 0;

static SANE_Option_Descriptor optionTopLeftYDescriptor = {
  SANE_NAME_SCAN_TL_Y,
  SANE_TITLE_SCAN_TL_Y,
  SANE_DESC_SCAN_TL_Y,
  SANE_TYPE_FIXED,
  SANE_UNIT_MM,
  sizeof (SANE_Fixed),
  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
  SANE_CONSTRAINT_RANGE,
  {(const SANE_String_Const *) &heightRange}
};

static SANE_Status
optionTopLeftYCallback (SANE_Option * option, SANE_Handle handle,
			SANE_Action action, void *value, SANE_Int * info)
{
  /* Eliminate warnings about unused parameters */
  option = option;
  handle = handle;

  switch (action)
    {
    case SANE_ACTION_SET_AUTO:
      return SANE_STATUS_INVAL;
      break;
    case SANE_ACTION_SET_VALUE:
      optionTopLeftYValue = *(SANE_Fixed *) value;
      *info |= SANE_INFO_RELOAD_PARAMS;
      break;
    case SANE_ACTION_GET_VALUE:
      *(SANE_Fixed *) value = optionTopLeftYValue;
      break;
    }
  return SANE_STATUS_GOOD;
}

/*-----------------------------------------------------------------*/
/*
This option controls the bot-right-x corner of the scan
Default to 215.9mm, max.
*/

static SANE_Fixed optionBotRightXValue = SANE_FIX (215.9);

static SANE_Option_Descriptor optionBotRightXDescriptor = {
  SANE_NAME_SCAN_BR_X,
  SANE_TITLE_SCAN_BR_X,
  SANE_DESC_SCAN_BR_X,
  SANE_TYPE_FIXED,
  SANE_UNIT_MM,
  sizeof (SANE_Fixed),
  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
  SANE_CONSTRAINT_RANGE,
  {(const SANE_String_Const *) &widthRange}
};

static SANE_Status
optionBotRightXCallback (SANE_Option * option, SANE_Handle handle,
			 SANE_Action action, void *value, SANE_Int * info)
{
  /* Eliminate warnings about unused parameters */
  option = option;
  handle = handle;

  switch (action)
    {
    case SANE_ACTION_SET_AUTO:
      return SANE_STATUS_INVAL;
      break;
    case SANE_ACTION_SET_VALUE:
      optionBotRightXValue = *(SANE_Fixed *) value;
      *info |= SANE_INFO_RELOAD_PARAMS;
      break;
    case SANE_ACTION_GET_VALUE:
      *(SANE_Fixed *) value = optionBotRightXValue;
      break;
    }
  return SANE_STATUS_GOOD;
}

/*-----------------------------------------------------------------*/
/*
This option controls the bot-right-y corner of the scan
Default to 296.3mm, max
*/

static SANE_Fixed optionBotRightYValue = SANE_FIX (296.3);

static SANE_Option_Descriptor optionBotRightYDescriptor = {
  SANE_NAME_SCAN_BR_Y,
  SANE_TITLE_SCAN_BR_Y,
  SANE_DESC_SCAN_BR_Y,
  SANE_TYPE_FIXED,
  SANE_UNIT_MM,
  sizeof (SANE_Fixed),
  SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT,
  SANE_CONSTRAINT_RANGE,
  {(const SANE_String_Const *) &heightRange}
};

static SANE_Status
optionBotRightYCallback (SANE_Option * option, SANE_Handle handle,
			 SANE_Action action, void *value, SANE_Int * info)
{
  /* Eliminate warnings about unused parameters */
  option = option;
  handle = handle;

  switch (action)
    {
    case SANE_ACTION_SET_AUTO:
      return SANE_STATUS_INVAL;
      break;
    case SANE_ACTION_SET_VALUE:
      optionBotRightYValue = *(SANE_Fixed *) value;
      *info |= SANE_INFO_RELOAD_PARAMS;
      break;
    case SANE_ACTION_GET_VALUE:
      *(SANE_Fixed *) value = optionBotRightYValue;
      break;
    }
  return SANE_STATUS_GOOD;
}

/*-----------------------------------------------------------------*/
/*
The following array binds the option descriptors to
their respective callback routines
*/

static SANE_Option so[] = {
  {&optionNumOptionsDescriptor, optionNumOptionsCallback},
  {&optionResolutionDescriptor, optionResolutionCallback},
  {&optionCalibrateDescriptor, optionCalibrateCallback},
#ifdef GRAY
  {&optionGrayscaleDescriptor, optionGrayscaleCallback},
#endif
  {&optionAGainDescriptor, optionAGainCallback},
  {&optionGammaDescriptor, optionGammaCallback},
  {&optionTopLeftXDescriptor, optionTopLeftXCallback},
  {&optionTopLeftYDescriptor, optionTopLeftYCallback},
  {&optionBotRightXDescriptor, optionBotRightXCallback},
  {&optionBotRightYDescriptor, optionBotRightYCallback}
};

static SANE_Word
getNumberOfOptions (void)
{
  return NELEMS (so);
}


/*
This routine dispatches the control message to the appropriate
callback routine, it outght to be called by sane_control_option
after any driver specific validation.
*/
static SANE_Status
dispatch_control_option (SANE_Handle handle, SANE_Int option,
			 SANE_Action action, void *value, SANE_Int * info)
{
  SANE_Option *op = so + option;
  SANE_Int myinfo = 0;
  SANE_Status status = SANE_STATUS_GOOD;

  if (option < 0 || option >= NELEMS (so))
    return SANE_STATUS_INVAL;	/* Unknown option ... */

  if ((action == SANE_ACTION_SET_VALUE) &&
      ((op->descriptor->cap & SANE_CAP_SOFT_SELECT) == 0))
    return SANE_STATUS_INVAL;

  if ((action == SANE_ACTION_GET_VALUE) &&
      ((op->descriptor->cap & SANE_CAP_SOFT_DETECT) == 0))
    return SANE_STATUS_INVAL;

  if ((action == SANE_ACTION_SET_AUTO) &&
      ((op->descriptor->cap & SANE_CAP_AUTOMATIC) == 0))
    return SANE_STATUS_INVAL;

  if (action == SANE_ACTION_SET_VALUE)
    {
      status = sanei_constrain_value (op->descriptor, value, &myinfo);
      if (status != SANE_STATUS_GOOD)
	return status;
    }

  status = (op->callback) (op, handle, action, value, &myinfo);

  if (info)
    *info = myinfo;

  return status;
}


static SANE_Status
attach_scanner (const char *devicename, Canon_Device ** devp)
{
  CANON_Handle scan;
  Canon_Device *dev;
  SANE_Status status;

  DBG (3, "attach_scanner: %s\n", devicename);

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devicename) == 0)
	{
	  if (devp)
	    *devp = dev;
	  return SANE_STATUS_GOOD;
	}
    }

  dev = malloc (sizeof (*dev));
  if (!dev)
    return SANE_STATUS_NO_MEM;
  memset (dev, '\0', sizeof (Canon_Device));	/* clear structure */

  DBG (4, "attach_scanner: opening %s\n", devicename);

  status = CANON_open_device (&scan, devicename);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "ERROR: attach_scanner: opening %s failed\n", devicename);
      free (dev);
      return status;
    }
  dev->name = strdup (devicename);
  dev->sane.name = dev->name;
  dev->sane.vendor = "CANON";
  dev->sane.model = CANON_get_device_name (&scan);
  dev->sane.type = "flatbed scanner";
  CANON_close_device (&scan);

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;
  return SANE_STATUS_GOOD;
}


/* callback function for sanei_usb_attach_matching_devices
*/
static SANE_Status
attach_one (const char *name)
{
  attach_scanner (name, 0);
  return SANE_STATUS_GOOD;
}


/* 
   Find our devices
 */
SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char config_line[PATH_MAX];
  size_t len;
  FILE *fp;

  DBG_INIT ();

#if 0
  DBG_LEVEL = 10;
#endif

  DBG (2, "sane_init: version_code %s 0, authorize %s 0\n",
       version_code == 0 ? "=" : "!=", authorize == 0 ? "=" : "!=");
  DBG (1, "sane_init: SANE Canon630u backend version %d.%d.%d from %s\n",
       SANE_CURRENT_MAJOR, V_MINOR, BUILD, PACKAGE_STRING);

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  sanei_usb_init ();

  fp = sanei_config_open (CANONUSB_CONFIG_FILE);
  if (!fp)
    {
      /* no config-file: try these */
      attach_scanner ("/dev/scanner", 0);
      attach_scanner ("/dev/usbscanner", 0);
      attach_scanner ("/dev/usb/scanner", 0);
      return SANE_STATUS_GOOD;
    }

  DBG (3, "reading configure file %s\n", CANONUSB_CONFIG_FILE);

  while (sanei_config_read (config_line, sizeof (config_line), fp))
    {
      if (config_line[0] == '#')
	continue;		/* ignore line comments */

      len = strlen (config_line);

      if (!len)
	continue;		/* ignore empty lines */

      DBG (4, "attach_matching_devices(%s)\n", config_line);
      sanei_usb_attach_matching_devices (config_line, attach_one);
    }

  DBG (4, "finished reading configure file\n");

  fclose (fp);

  return SANE_STATUS_GOOD;
}


void
sane_exit (void)
{
  Canon_Device *dev, *next;

  DBG (3, "sane_exit\n");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free (dev->name);
      free (dev);
    }

  if (devlist)
    free (devlist);
  return;
}


SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  Canon_Device *dev;
  int i;

  DBG (3, "sane_get_devices(local_only = %d)\n", local_only);

  if (devlist)
    free (devlist);

  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  i = 0;

  for (dev = first_dev; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;

  devlist[i++] = 0;

  *device_list = devlist;

  return SANE_STATUS_GOOD;
}


SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Canon_Device *dev;
  SANE_Status status;
  Canon_Scanner *scanner;

  DBG (3, "sane_open\n");

  if (devicename[0])		/* search for devicename */
    {
      DBG (4, "sane_open: devicename=%s\n", devicename);

      for (dev = first_dev; dev; dev = dev->next)
	if (strcmp (dev->sane.name, devicename) == 0)
	  break;

      if (!dev)
	{
	  status = attach_scanner (devicename, &dev);
	  if (status != SANE_STATUS_GOOD)
	    return status;
	}
    }
  else
    {
      DBG (2, "sane_open: no devicename, opening first device\n");
      dev = first_dev;
    }

  if (!dev)
    return SANE_STATUS_INVAL;

  scanner = malloc (sizeof (*scanner));
  if (!scanner)
    return SANE_STATUS_NO_MEM;

  memset (scanner, 0, sizeof (*scanner));
  scanner->device = dev;

  status = CANON_open_device (&scanner->scan, dev->sane.name);
  if (status != SANE_STATUS_GOOD)
    {
      free (scanner);
      return status;
    }

  *handle = scanner;

  /* insert newly opened handle into list of open handles: */
  scanner->next = first_handle;

  first_handle = scanner;

  return SANE_STATUS_GOOD;
}


void
sane_close (SANE_Handle handle)
{
  Canon_Scanner *prev, *scanner;
  SANE_Status res;

  DBG (3, "sane_close\n");

  if (!first_handle)
    {
      DBG (1, "ERROR: sane_close: no handles opened\n");
      return;
    }

  /* remove handle from list of open handles: */

  prev = NULL;

  for (scanner = first_handle; scanner; scanner = scanner->next)
    {
      if (scanner == handle)
	break;

      prev = scanner;
    }

  if (!scanner)
    {
      DBG (1, "ERROR: sane_close: invalid handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }

  if (prev)
    prev->next = scanner->next;
  else
    first_handle = scanner->next;

  res = CANON_close_device (&scanner->scan);

  free (scanner);
}


const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  handle = handle;		/* Eliminate compiler warning */

  DBG (3, "sane_get_option_descriptor: option = %d\n", option);
  if (option < 0 || option >= NELEMS (so))
    return NULL;
  return so[option].descriptor;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *value, SANE_Int * info)
{
  handle = handle;		/* Eliminate compiler warning */

  DBG (3,
       "sane_control_option: handle=%p, opt=%d, act=%d, val=%p, info=%p\n",
       handle, option, action, value, (void *)info);

  return dispatch_control_option (handle, option, action, value, info);
}


SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  int rc = SANE_STATUS_GOOD;
  int w =
    SANE_UNFIX (optionBotRightXValue -
		optionTopLeftXValue) / MM_IN_INCH * optionResolutionValue;
  int h =
    SANE_UNFIX (optionBotRightYValue -
		optionTopLeftYValue) / MM_IN_INCH * optionResolutionValue;

  handle = handle;		/* Eliminate compiler warning */

  DBG (3, "sane_get_parameters\n");
  parms.depth = 8;
  parms.last_frame = SANE_TRUE;
  parms.pixels_per_line = w;
  parms.lines = h;

#ifdef GRAY
  if (optionGrayscaleValue == SANE_TRUE)
    {
      parms.format = SANE_FRAME_GRAY;
      parms.bytes_per_line = w;
    }
  else
#endif
    {
      parms.format = SANE_FRAME_RGB;
      parms.bytes_per_line = w * 3;
    }
  *params = parms;
  return rc;
}


SANE_Status
sane_start (SANE_Handle handle)
{
  Canon_Scanner *scanner = handle;
  SANE_Status res;

  DBG (3, "sane_start\n");

  res = CANON_set_scan_parameters (&scanner->scan,
				   optionCalibrateValue,
#ifdef GRAY
				   optionGrayscaleValue,
#else
				   SANE_FALSE,
#endif
				   SANE_UNFIX (optionTopLeftXValue) /
				   MM_IN_INCH * 600,
				   SANE_UNFIX (optionTopLeftYValue) /
				   MM_IN_INCH * 600,
				   SANE_UNFIX (optionBotRightXValue) /
				   MM_IN_INCH * 600,
				   SANE_UNFIX (optionBotRightYValue) /
				   MM_IN_INCH * 600,
				   optionResolutionValue, 
				   optionAGainValue,
				   SANE_UNFIX (optionGammaValue));

  if (res != SANE_STATUS_GOOD)
    return res;

  return CANON_start_scan (&scanner->scan);
}


SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * data,
	   SANE_Int max_length, SANE_Int * length)
{
  Canon_Scanner *scanner = handle;
  return CANON_read (&scanner->scan, data, max_length, length);
}


void
sane_cancel (SANE_Handle handle)
{
  DBG (3, "sane_cancel: handle = %p\n", handle);
  DBG (3, "sane_cancel: cancelling is unsupported in this backend\n");
}


SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  DBG (3, "sane_set_io_mode: handle = %p, non_blocking = %d\n", handle,
       non_blocking);
  if (non_blocking != SANE_FALSE)
    return SANE_STATUS_UNSUPPORTED;
  return SANE_STATUS_GOOD;
}


SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  handle = handle;                   /* silence gcc */
  fd = fd;                           /* silence gcc */
  return SANE_STATUS_UNSUPPORTED;
}
