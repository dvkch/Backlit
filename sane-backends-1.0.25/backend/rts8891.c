/* sane - Scanner Access Now Easy.

   Copyright (C) 2007-2013 stef.dev@free.fr

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

   --------------------------------------------------------------------------

   SANE FLOW DIAGRAM

   - sane_init() : initialize backend, attach scanners
   . - sane_get_devices() : query list of scanner devices
   . - sane_open() : open a particular scanner device, adding a handle
   		     to the opened device
   . . - sane_set_io_mode : set blocking mode
   . . - sane_get_select_fd : get scanner fd
   . . - sane_get_option_descriptor() : get option information
   . . - sane_control_option() : change option values
   . .
   . . - sane_start() : start image acquisition
   . .   - sane_get_parameters() : returns actual scan parameters
   . .   - sane_read() : read image data
   . .
   . . - sane_cancel() : cancel operation
   . - sane_close() : close opened scanner device, freeing scanner handle
   - sane_exit() : terminate use of backend, freeing all resources for attached
   		   devices

   BACKEND USB locking policy
   - interface is released at the end of sane_open() to allow
   	multiple usage by fronteds
   - if free, interface is claimed at sane_start(), and held until
   	sane_cancel() is called
   - if free interface is claimed then released just during the access
        to buttons registers
*/

/* ------------------------------------------------------------------------- */

#include "../include/sane/config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_LIBC_H
# include <libc.h>
#endif

#include "../include/sane/sane.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_backend.h"

#define DARK_TARGET		3.1	/* 3.5 target average for dark calibration */
#define DARK_MARGIN		0.3	/* acceptable margin for dark average */

#define OFFSET_TARGET		3.5	/* target average for offset calibration */
#define OFFSET_MARGIN		0.3	/* acceptable margin for offset average */

#define RED_GAIN_TARGET	    	170	/* target average for gain calibration for blue color */
#define GREEN_GAIN_TARGET	170	/* target average for gain calibration for blue color */
#define BLUE_GAIN_TARGET	180	/* target average for gain calibration for blue color */
#define GAIN_MARGIN		  2	/* acceptable margin for gain average */

#define MARGIN_LEVEL            128	/* white level for margin detection */

/* width used for calibration */
#define CALIBRATION_WIDTH       637

/* data size for calibration: one RGB line*/
#define CALIBRATION_SIZE        CALIBRATION_WIDTH*3

/* #define FAST_INIT 1 */

#define BUILD 2401

#define MOVE_DPI 100

#include "rts8891.h"
#include "rts88xx_lib.h"
#include "rts8891_low.c"
#include "rts8891_devices.c"


#define DEEP_DEBUG 1

/**
 * Pointer to the first Rts8891_Session in the linked list of
 * opened device. Sessions are inserted here on sane_open() and
 * removed on sane_close().
 */
static Rts8891_Session *first_handle = NULL;


/* pointer to the first device attached to the backend
 * the same device may be opened several time
 * entry are inserted here by attach_rts8891 */
static Rts8891_Device *first_device = NULL;
static SANE_Int num_devices = 0;

/*
 * needed by sane_get_devices
 */
static const SANE_Device **devlist = 0;


static SANE_String_Const mode_list[] = {
  SANE_VALUE_SCAN_MODE_COLOR,
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_LINEART,
  0
};

static SANE_Range x_range = {
  SANE_FIX (0.0),		/* minimum */
  SANE_FIX (216.0),		/* maximum */
  SANE_FIX (0.0)		/* quantization */
};

static SANE_Range y_range = {
  SANE_FIX (0.0),		/* minimum */
  SANE_FIX (299.0),		/* maximum */
  SANE_FIX (0.0)		/* no quantization */
};

/* model number ranges from 0 to 2, must be changed if
 * Rts8891_USB_Device_Entry changes */
static const SANE_Range model_range = {
  0,				/* minimum */
  2,				/* maximum */
  0				/* no quantization */
};

/* sensor number ranges from 0 to SENSOR_TYPE_MAX, must be changed if */
static const SANE_Range sensor_range = {
  0,				/* minimum */
  SENSOR_TYPE_MAX,		/* maximum */
  0				/* no quantization */
};

static const SANE_Range u8_range = {
  0,				/* minimum */
  255,				/* maximum */
  0				/* no quantization */
};

static const SANE_Range threshold_percentage_range = {
  SANE_FIX (0),			/* minimum */
  SANE_FIX (100),		/* maximum */
  SANE_FIX (1)			/* quantization */
};

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  SANE_Int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }
  return max_size;
}

  /**> placeholders for decoded configuration values */
static Rts8891_Config rtscfg;

/* ------------------------------------------------------------------------- */
static SANE_Status probe_rts8891_devices (void);
static SANE_Status config_attach_rts8891 (SANEI_Config * config,
					  const char *devname);
static SANE_Status attach_rts8891 (const char *name);
static SANE_Status set_lamp_brightness (struct Rts8891_Device *dev,
					int level);
static SANE_Status init_options (struct Rts8891_Session *session);
#ifndef FAST_INIT
static SANE_Status init_device (struct Rts8891_Device *dev);
#else
static SANE_Status detect_device (struct Rts8891_Device *dev);
static SANE_Status initialize_device (struct Rts8891_Device *dev);
#endif
static SANE_Status init_lamp (struct Rts8891_Device *dev);
static SANE_Status find_origin (struct Rts8891_Device *dev,
				SANE_Bool * changed);
static SANE_Status find_margin (struct Rts8891_Device *dev);
static SANE_Status dark_calibration (struct Rts8891_Device *dev, int mode,
				     int light);
static SANE_Status gain_calibration (struct Rts8891_Device *dev, int mode,
				     int light);
static SANE_Status offset_calibration (struct Rts8891_Device *dev, int mode,
				       int light);
static SANE_Status shading_calibration (struct Rts8891_Device *dev,
					SANE_Bool color, int mode, int light);
static SANE_Status send_calibration_data (struct Rts8891_Session *session);
static SANE_Status write_scan_registers (struct Rts8891_Session *session);
static SANE_Status read_data (struct Rts8891_Session *session,
			      SANE_Byte * dest, SANE_Int length);
static SANE_Status compute_parameters (struct Rts8891_Session *session);
static SANE_Status move_to_scan_area (struct Rts8891_Session *session);
static SANE_Status park_head (struct Rts8891_Device *dev, SANE_Bool wait);
static SANE_Status update_button_status (struct Rts8891_Session *session);
static SANE_Status set_lamp_state (struct Rts8891_Session *session, int on);


/* ------------------------------------------------------------------------- */
/* writes gray data to a pnm file */
static void
write_gray_data (unsigned char *image, char *name, SANE_Int width,
		 SANE_Int height)
{
  FILE *fdbg = NULL;

  fdbg = fopen (name, "wb");
  if (fdbg == NULL)
    return;
  fprintf (fdbg, "P5\n%d %d\n255\n", width, height);
  fwrite (image, width, height, fdbg);
  fclose (fdbg);
}

/* ------------------------------------------------------------------------- */
/* writes rgb data to a pnm file */
static void
write_rgb_data (char *name, unsigned char *image, SANE_Int width,
		SANE_Int height)
{
  FILE *fdbg = NULL;

  fdbg = fopen (name, "wb");
  if (fdbg == NULL)
    return;
  fprintf (fdbg, "P6\n%d %d\n255\n", width, height);
  fwrite (image, width * 3, height, fdbg);
  fclose (fdbg);
}

/* ------------------------------------------------------------------------- */

/*
 * SANE Interface
 */


/**
 * Called by SANE initially.
 * 
 * From the SANE spec:
 * This function must be called before any other SANE function can be
 * called. The behavior of a SANE backend is undefined if this
 * function is not called first. The version code of the backend is
 * returned in the value pointed to by version_code. If that pointer
 * is NULL, no version code is returned. Argument authorize is either
 * a pointer to a function that is invoked when the backend requires
 * authentication for a specific resource or NULL if the frontend does
 * not support authentication.
 */
SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  SANE_Status status;

  authorize = authorize;	/* get rid of compiler warning */

  /* init ASIC libraries */
  sanei_rts88xx_lib_init ();
  rts8891_low_init ();

  /* init backend debug */
  DBG_INIT ();
  DBG (DBG_info, "SANE Rts8891 backend version %d.%d-%d\n",
       SANE_CURRENT_MAJOR, V_MINOR, BUILD);
  DBG (DBG_proc, "sane_init: start\n");

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  /* cold-plugging case : probe for already plugged devices */
  status = probe_rts8891_devices ();

  DBG (DBG_proc, "sane_init: exit\n");
  return status;
}


/**
 * Called by SANE to find out about supported devices.
 * 
 * From the SANE spec:
 * This function can be used to query the list of devices that are
 * available. If the function executes successfully, it stores a
 * pointer to a NULL terminated array of pointers to SANE_Device
 * structures in *device_list. The returned list is guaranteed to
 * remain unchanged and valid until (a) another call to this function
 * is performed or (b) a call to sane_exit() is performed. This
 * function can be called repeatedly to detect when new devices become
 * available. If argument local_only is true, only local devices are
 * returned (devices directly attached to the machine that SANE is
 * running on). If it is false, the device list includes all remote
 * devices that are accessible to the SANE library.
 * 
 * SANE does not require that this function is called before a
 * sane_open() call is performed. A device name may be specified
 * explicitly by a user which would make it unnecessary and
 * undesirable to call this function first.
 */
SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  SANE_Int dev_num;
  struct Rts8891_Device *device;
  SANE_Device *sane_device;
  int i;

  DBG (DBG_proc, "sane_get_devices: start: local_only = %s\n",
       local_only == SANE_TRUE ? "true" : "false");

  /* hot-plugging case : probe for devices plugged since sane_init called */
  probe_rts8891_devices ();

  /* reset devlist first if needed */
  if (devlist)
    {
      for (i = 0; i < num_devices; i++)
	free ((char *) devlist[i]);
      free (devlist);
      devlist = NULL;
    }
  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  *device_list = devlist;

  dev_num = 0;

  /* we build a list of SANE_Device from the list of attached devices */
  for (device = first_device; dev_num < num_devices; device = device->next)
    {
      sane_device = malloc (sizeof (*sane_device));
      if (!sane_device)
	return SANE_STATUS_NO_MEM;
      sane_device->name = device->file_name;
      sane_device->vendor = device->model->vendor;
      sane_device->model = device->model->product;
      sane_device->type = device->model->type;
      devlist[dev_num++] = sane_device;
    }
  devlist[dev_num++] = 0;

  *device_list = devlist;

  DBG (DBG_proc, "sane_get_devices: exit\n");

  return SANE_STATUS_GOOD;
}


/**
 * Called to establish connection with the session. This function will
 * also establish meaningful defaults and initialize the options.
 *
 * From the SANE spec:
 * This function is used to establish a connection to a particular
 * device. The name of the device to be opened is passed in argument
 * name. If the call completes successfully, a handle for the device
 * is returned in *h. As a special case, specifying a zero-length
 * string as the device requests opening the first available device
 * (if there is such a device).
 * TODO handle SANE_STATUS_BUSY
 */
SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * handle)
{
  struct Rts8891_Session *session = NULL;
  struct Rts8891_Device *device = NULL;
  SANE_Status status;

  DBG (DBG_proc, "sane_open: start (devicename=%s)\n", name);
  if (name[0] == 0 || strncmp (name, "rts8891", 7) == 0)
    {
      DBG (DBG_info, "sane_open: no device requested, using default\n");
      if (first_device)
	{
	  device = first_device;
	  DBG (DBG_info, "sane_open: device %s used as default device\n",
	       device->file_name);
	}
    }
  else
    {
      DBG (DBG_info, "sane_open: device %s requested\n", name);
      /* walk the device list until we find a matching name */
      device = first_device;
      while (device && strcmp (device->file_name, name) != 0)
	{
	  DBG (DBG_info, "sane_open: device %s doesn't match\n",
	       device->file_name);
	  device = device->next;
	}
    }

  /* check wether we have found a match or reach the end of the device list */
  if (!device)
    {
      DBG (DBG_info, "sane_open: no device found\n");
      return SANE_STATUS_INVAL;
    }

  /* now we have a device, duplicate it and return it in handle */
  DBG (DBG_info, "sane_open: device %s found\n", name);

  if (device->model->flags & RTS8891_FLAG_UNTESTED)
    {
      DBG (DBG_error0,
	   "WARNING: Your scanner is not fully supported or at least \n");
      DBG (DBG_error0,
	   "         had only limited testing. Please be careful and \n");
      DBG (DBG_error0, "         report any failure/success to \n");
      DBG (DBG_error0,
	   "         sane-devel@lists.alioth.debian.org. Please provide as many\n");
      DBG (DBG_error0,
	   "         details as possible, e.g. the exact name of your\n");
      DBG (DBG_error0, "         scanner and what does (not) work.\n");

    }

  /* open USB link */
  status = sanei_usb_open (device->file_name, &device->devnum);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_warn, "sane_open: couldn't open device `%s': %s\n",
	   device->file_name, sane_strstatus (status));
      return status;
    }

  /* device initialization */
  if (device->initialized == SANE_FALSE)
    {
#ifdef FAST_INIT
      status = detect_device (device);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "sane_open: detect_device failed\n");
	  DBG (DBG_proc, "sane_open: exit on error\n");
	  return status;
	}
      status = initialize_device (device);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "sane_open: initialize_device failed\n");
	  DBG (DBG_proc, "sane_open: exit on error\n");
	  return status;
	}
#else
      status = init_device (device);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "sane_open: init_device failed\n");
	  DBG (DBG_proc, "sane_open: exit on error\n");
	  return status;
	}
      device->initialized = SANE_TRUE;
#endif
    }

  /* prepare handle to return */
  session = (Rts8891_Session *) malloc (sizeof (Rts8891_Session));

  session->scanning = SANE_FALSE;
  session->dev = device;

  init_options (session);
  session->scanning = SANE_FALSE;
  session->non_blocking = SANE_FALSE;
  *handle = session;

  /* add the handle to the list */
  session->next = first_handle;
  first_handle = session;

  /* release the interface to allow device sharing */
  if (session->dev->conf.allowsharing == SANE_TRUE)
    {
      sanei_usb_release_interface (device->devnum, 0);
    }

  DBG (DBG_proc, "sane_open: exit\n");
  return SANE_STATUS_GOOD;
}


/**
 * Set non blocking mode. In this mode, read return immediatly when
 * no data is available, instead of polling the scanner.
 */
SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Rts8891_Session *session = (Rts8891_Session *) handle;

  DBG (DBG_proc, "sane_set_io_mode: start\n");
  if (session->scanning != SANE_TRUE)
    {
      DBG (DBG_error, "sane_set_io_mode: called out of a scan\n");
      return SANE_STATUS_INVAL;
    }
  session->non_blocking = non_blocking;
  DBG (DBG_warn, "sane_set_io_mode: I/O mode set to %sblocking.\n",
       non_blocking ? "non " : " ");
  DBG (DBG_proc, "sane_set_io_mode: exit\n");
  return SANE_STATUS_GOOD;
}


/**
 * An advanced method we don't support but have to define.
 */
SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fdp)
{

  DBG (DBG_proc, "sane_get_select_fd: start\n");
  if(handle==0 || fdp==NULL)
    {
      return SANE_STATUS_INVAL;
    }
  DBG (DBG_warn, "sane_get_select_fd: unsupported ...\n");
  DBG (DBG_proc, "sane_get_select_fd: exit\n");
  return SANE_STATUS_UNSUPPORTED;
}


/**
 * Returns the options we know.
 *
 * From the SANE spec:
 * This function is used to access option descriptors. The function
 * returns the option descriptor for option number n of the device
 * represented by handle h. Option number 0 is guaranteed to be a
 * valid option. Its value is an integer that specifies the number of
 * options that are available for device handle h (the count includes
 * option 0). If n is not a valid option index, the function returns
 * NULL. The returned option descriptor is guaranteed to remain valid
 * (and at the returned address) until the device is closed.
 */
const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  struct Rts8891_Session *session = handle;

  DBG (DBG_proc, "sane_get_option_descriptor: start\n");

  if ((unsigned) option >= NUM_OPTIONS)
    return NULL;

  DBG (DBG_info, "sane_get_option_descriptor: \"%s\"\n",
       session->opt[option].name);

  DBG (DBG_proc, "sane_get_option_descriptor: exit\n");
  return &(session->opt[option]);
}

/**
 * sets automatic value for an option , called by sane_control_option after
 * all checks have been done */
static SANE_Status
set_automatic_value (Rts8891_Session * s, int option, SANE_Int * myinfo)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int i, min;
  SANE_Word *dpi_list;

  switch (option)
    {
    case OPT_TL_X:
      s->val[OPT_TL_X].w = x_range.min;
      *myinfo |= SANE_INFO_RELOAD_PARAMS;
      break;
    case OPT_TL_Y:
      s->val[OPT_TL_Y].w = y_range.min;
      *myinfo |= SANE_INFO_RELOAD_PARAMS;
      break;
    case OPT_BR_X:
      s->val[OPT_BR_X].w = x_range.max;
      *myinfo |= SANE_INFO_RELOAD_PARAMS;
      break;
    case OPT_BR_Y:
      s->val[OPT_BR_Y].w = y_range.max;
      *myinfo |= SANE_INFO_RELOAD_PARAMS;
      break;
    case OPT_RESOLUTION:
      /* we set up to the lowest available dpi value */
      dpi_list = (SANE_Word *) s->opt[OPT_RESOLUTION].constraint.word_list;
      min = 65536;
      for (i = 1; i < dpi_list[0]; i++)
	{
	  if (dpi_list[i] < min)
	    min = dpi_list[i];
	}
      s->val[OPT_RESOLUTION].w = min;
      *myinfo |= SANE_INFO_RELOAD_PARAMS;
      break;
    case OPT_THRESHOLD:
      s->val[OPT_THRESHOLD].w = SANE_FIX (50);
      break;
    case OPT_PREVIEW:
      s->val[OPT_PREVIEW].w = SANE_FALSE;
      *myinfo |= SANE_INFO_RELOAD_PARAMS;
      break;
    case OPT_MODE:
      if (s->val[OPT_MODE].s)
	free (s->val[OPT_MODE].s);
      s->val[OPT_MODE].s = strdup (mode_list[0]);
      *myinfo |= SANE_INFO_RELOAD_OPTIONS;
      *myinfo |= SANE_INFO_RELOAD_PARAMS;
      break;
    case OPT_CUSTOM_GAMMA:
      s->val[option].b = SANE_FALSE;
      DISABLE (OPT_GAMMA_VECTOR);
      DISABLE (OPT_GAMMA_VECTOR_R);
      DISABLE (OPT_GAMMA_VECTOR_G);
      DISABLE (OPT_GAMMA_VECTOR_B);
      break;
    case OPT_GAMMA_VECTOR:
    case OPT_GAMMA_VECTOR_R:
    case OPT_GAMMA_VECTOR_G:
    case OPT_GAMMA_VECTOR_B:
      if (s->dev->model->gamma != s->val[option].wa)
	free (s->val[option].wa);
      s->val[option].wa = s->dev->model->gamma;
      break;
    default:
      DBG (DBG_warn, "set_automatic_value: can't set unknown option %d\n",
	   option);
    }

  return status;
}

/**
 * sets an option , called by sane_control_option after all
 * checks have been done */
static SANE_Status
set_option_value (Rts8891_Session * s, int option, void *val,
		  SANE_Int * myinfo)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int i;
  SANE_Word tmpw;

  switch (option)
    {
    case OPT_TL_X:
    case OPT_TL_Y:
    case OPT_BR_X:
    case OPT_BR_Y:
      s->val[option].w = *(SANE_Word *) val;
      /* we ensure geometry is coherent */
      /* this happens when user drags TL corner right or below the BR point */
      if (s->val[OPT_BR_Y].w < s->val[OPT_TL_Y].w)
	{
	  tmpw = s->val[OPT_BR_Y].w;
	  s->val[OPT_BR_Y].w = s->val[OPT_TL_Y].w;
	  s->val[OPT_TL_Y].w = tmpw;
	}
      if (s->val[OPT_BR_X].w < s->val[OPT_TL_X].w)
	{
	  tmpw = s->val[OPT_BR_X].w;
	  s->val[OPT_BR_X].w = s->val[OPT_TL_X].w;
	  s->val[OPT_TL_X].w = tmpw;
	}

      *myinfo |= SANE_INFO_RELOAD_PARAMS;
      break;

    case OPT_RESOLUTION:
    case OPT_THRESHOLD:
    case OPT_PREVIEW:
      s->val[option].w = *(SANE_Word *) val;
      *myinfo |= SANE_INFO_RELOAD_PARAMS;
      break;

    case OPT_MODE:
      if (s->val[option].s)
	free (s->val[option].s);
      s->val[option].s = strdup (val);
      if (strcmp (s->val[option].s, LINEART_MODE) == 0)
	{
	  ENABLE (OPT_THRESHOLD);
	}
      else
	{
	  DISABLE (OPT_THRESHOLD);
	}

      /* if custom gamma, toggle gamma table options according to the mode */
      if (s->val[OPT_CUSTOM_GAMMA].b == SANE_TRUE)
	{
	  if (strcmp (s->val[option].s, COLOR_MODE) == 0)
	    {
	      DISABLE (OPT_GAMMA_VECTOR);
	      ENABLE (OPT_GAMMA_VECTOR_R);
	      ENABLE (OPT_GAMMA_VECTOR_G);
	      ENABLE (OPT_GAMMA_VECTOR_B);
	    }
	  else
	    {
	      ENABLE (OPT_GAMMA_VECTOR);
	      DISABLE (OPT_GAMMA_VECTOR_R);
	      DISABLE (OPT_GAMMA_VECTOR_G);
	      DISABLE (OPT_GAMMA_VECTOR_B);
	    }
	}
      *myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      break;

    case OPT_CUSTOM_GAMMA:
      *myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      s->val[OPT_CUSTOM_GAMMA].b = *(SANE_Bool *) val;

      if (s->val[OPT_CUSTOM_GAMMA].b == SANE_TRUE)
	{
	  if (strcmp (s->val[OPT_MODE].s, COLOR_MODE) == 0)
	    {
	      DISABLE (OPT_GAMMA_VECTOR);
	      ENABLE (OPT_GAMMA_VECTOR_R);
	      ENABLE (OPT_GAMMA_VECTOR_G);
	      ENABLE (OPT_GAMMA_VECTOR_B);
	    }
	  else
	    {
	      ENABLE (OPT_GAMMA_VECTOR);
	      DISABLE (OPT_GAMMA_VECTOR_R);
	      DISABLE (OPT_GAMMA_VECTOR_G);
	      DISABLE (OPT_GAMMA_VECTOR_B);
	    }
	}
      else
	{
	  DISABLE (OPT_GAMMA_VECTOR);
	  DISABLE (OPT_GAMMA_VECTOR_R);
	  DISABLE (OPT_GAMMA_VECTOR_G);
	  DISABLE (OPT_GAMMA_VECTOR_B);
	}
      break;

    case OPT_GAMMA_VECTOR:
    case OPT_GAMMA_VECTOR_R:
    case OPT_GAMMA_VECTOR_G:
    case OPT_GAMMA_VECTOR_B:
      /* sanity checks */
      for (i = 0; i < (int) (s->opt[option].size / sizeof (SANE_Word)); i++)
	{
	  /* avoid 0xaa values since they will be problematic */
	  if (((SANE_Int *) val)[i] == 0xaa)
	    ((SANE_Int *) val)[i] = 0xab;
	}

      /* free memory from previous set */
      if (s->dev->model->gamma != s->val[option].wa)
	free (s->val[option].wa);

      /* then alloc memory */
      s->val[option].wa = (SANE_Word *) malloc (256 * sizeof (SANE_Word));
      if (s->val[option].wa == NULL)
	{
	  s->val[option].wa = s->dev->model->gamma;
	  DBG (DBG_error0,
	       "set_option_value: not enough memory for %lu bytes!\n",
	       (u_long) (256 * sizeof (SANE_Word)));
	  return SANE_STATUS_NO_MEM;
	}

      /* data copy */
      memcpy (s->val[option].wa, val, s->opt[option].size);
      break;

    case OPT_LAMP_ON:
      return set_lamp_state (s, 1);
      break;

    case OPT_LAMP_OFF:
      return set_lamp_state (s, 0);
      break;

    default:
      DBG (DBG_warn, "set_option_value: can't set unknown option %d\n",
	   option);
    }
  return status;
}

/**
 * gets an option , called by sane_control_option after all checks
 * have been done */
static SANE_Status
get_option_value (Rts8891_Session * s, int option, void *val)
{
  switch (option)
    {
      /* word or word equivalent options: */
    case OPT_NUM_OPTS:
    case OPT_RESOLUTION:
    case OPT_PREVIEW:
    case OPT_TL_X:
    case OPT_TL_Y:
    case OPT_BR_X:
    case OPT_BR_Y:
    case OPT_THRESHOLD:
    case OPT_CUSTOM_GAMMA:
      *(SANE_Word *) val = s->val[option].w;
      break;
    case OPT_BUTTON_1:
    case OPT_BUTTON_2:
    case OPT_BUTTON_3:
    case OPT_BUTTON_4:
    case OPT_BUTTON_5:
    case OPT_BUTTON_6:
    case OPT_BUTTON_7:
    case OPT_BUTTON_8:
    case OPT_BUTTON_9:
    case OPT_BUTTON_10:
    case OPT_BUTTON_11:
      /* no button pressed by default */
      *(SANE_Word *) val = SANE_FALSE;
      if (option - OPT_BUTTON_1 > s->dev->model->buttons)
	{
	  DBG (DBG_warn,
	       "get_option_value: invalid button option %d\n", option);
	}
      else
	{
	  update_button_status (s);
	  /* copy the button state */
	  *(SANE_Word *) val = s->val[option].w;
	  /* clear the button state */
	  s->val[option].w = SANE_FALSE;
	  DBG (DBG_io,
	       "get_option_value: button option %d=%d\n", option,
	       *(SANE_Word *) val);
	}
      break;
      /* string options: */
    case OPT_MODE:
      strcpy (val, s->val[option].s);
      break;
    case OPT_GAMMA_VECTOR:
    case OPT_GAMMA_VECTOR_R:
    case OPT_GAMMA_VECTOR_G:
    case OPT_GAMMA_VECTOR_B:
      memcpy (val, s->val[option].wa, s->opt[option].size);
      break;
    default:
      DBG (DBG_warn, "get_option_value: can't get unknown option %d\n",
	   option);
    }
  return SANE_STATUS_GOOD;
}

/**
 * Gets or sets an option value.
 * 
 * From the SANE spec:
 * This function is used to set or inquire the current value of option
 * number n of the device represented by handle h. The manner in which
 * the option is controlled is specified by parameter action. The
 * possible values of this parameter are described in more detail
 * below.  The value of the option is passed through argument val. It
 * is a pointer to the memory that holds the option value. The memory
 * area pointed to by v must be big enough to hold the entire option
 * value (determined by member size in the corresponding option
 * descriptor).
 * 
 * The only exception to this rule is that when setting the value of a
 * string option, the string pointed to by argument v may be shorter
 * since the backend will stop reading the option value upon
 * encountering the first NUL terminator in the string. If argument i
 * is not NULL, the value of *i will be set to provide details on how
 * well the request has been met.
 * action is SANE_ACTION_GET_VALUE, SANE_ACTION_SET_VALUE or SANE_ACTION_SET_AUTO
 */
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  Rts8891_Session *s = handle;
  SANE_Status status;
  SANE_Word cap;
  SANE_Int myinfo = 0;

  DBG (DBG_io2,
       "sane_control_option: start: action = %s, option = %s (%d)\n",
       (action == SANE_ACTION_GET_VALUE) ? "get" : (action ==
						    SANE_ACTION_SET_VALUE) ?
       "set" : (action == SANE_ACTION_SET_AUTO) ? "set_auto" : "unknown",
       s->opt[option].name, option);

  if (info)
    *info = 0;

  /* do checks before trying to apply action */

  if (s->scanning)
    {
      DBG (DBG_warn, "sane_control_option: don't call this function while "
	   "scanning (option = %s (%d))\n", s->opt[option].name, option);
      return SANE_STATUS_DEVICE_BUSY;
    }

  /* option must be within existing range */
  if (option >= NUM_OPTIONS || option < 0)
    {
      DBG (DBG_warn,
	   "sane_control_option: option %d >= NUM_OPTIONS || option < 0\n",
	   option);
      return SANE_STATUS_INVAL;
    }

  /* don't access an inactive option */
  cap = s->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      DBG (DBG_warn, "sane_control_option: option %d is inactive\n", option);
      return SANE_STATUS_INVAL;
    }

  /* now checks have been done, apply action */
  switch (action)
    {
    case SANE_ACTION_GET_VALUE:
      status = get_option_value (s, option, val);
      break;

    case SANE_ACTION_SET_VALUE:
      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (DBG_warn, "sane_control_option: option %d is not settable\n",
	       option);
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (s->opt + option, val, &myinfo);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_warn,
	       "sane_control_option: sanei_constrain_value returned %s\n",
	       sane_strstatus (status));
	  return status;
	}

      /* return immediatly if no change */
      if (s->opt[option].type == SANE_TYPE_INT
	  && *(SANE_Word *) val == s->val[option].w)
	{
	  status = SANE_STATUS_GOOD;
	}
      else
	{			/* apply change */
	  status = set_option_value (s, option, val, &myinfo);
	}
      break;

    case SANE_ACTION_SET_AUTO:
      /* sets automatic values */
      if (!(cap & SANE_CAP_AUTOMATIC))
	{
	  DBG (DBG_warn,
	       "sane_control_option: option %d is not autosettable\n",
	       option);
	  return SANE_STATUS_INVAL;
	}

      status = set_automatic_value (s, option, &myinfo);
      break;

    default:
      DBG (DBG_error, "sane_control_option: invalid action %d\n", action);
      status = SANE_STATUS_INVAL;
      break;
    }

  if (info)
    *info = myinfo;

  DBG (DBG_io2, "sane_control_option: exit\n");
  return status;
}

/**
 * returns the name of the sensor
 * @param sensor sensor numnber
 * @return string holding the name of the sensor
 */
static char *sensor_name (int sensor)
{
  switch (sensor)
    {
    case SENSOR_TYPE_BARE:
      return "SENSOR_TYPE_BARE";
    case SENSOR_TYPE_XPA:
      return "SENSOR_TYPE_XPA";
    case SENSOR_TYPE_4400:
      return "SENSOR_TYPE_4400";
    case SENSOR_TYPE_4400_BARE:
      return "SENSOR_TYPE_4400_BARE";
    default:
      return "BOGUS";
    }
}

/**
 * Called by SANE when a page acquisition operation is to be started.
 * @param handle opaque handle to a frontend session
 * @return SANE_STATUS_GOOD on success, SANE_STATUS_BUSY if the device is
 * in use by another session or SANE_STATUS_WARMING_UP if the device is
 * warming up. In this case the fronted as to call sane_start again until
 * warming up is done. Any other values returned are error status.
 */
SANE_Status
sane_start (SANE_Handle handle)
{
  struct Rts8891_Session *session = handle;
  int light, mode;
  struct Rts8891_Device *dev = session->dev;
  SANE_Status status;
  SANE_Bool changed;
#ifdef HAVE_SYS_TIME_H
  struct timeval current;
#endif

  DBG (DBG_proc, "sane_start: start\n");

  /* if already scanning, tell we're busy */
  if (session->scanning == SANE_TRUE)
    {
      DBG (DBG_warn, "sane_start: device is already scanning\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  /* claim the interface reserve device */
  if (dev->conf.allowsharing == SANE_TRUE)
    {
      status = sanei_usb_claim_interface (dev->devnum, 0);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_warn, "sane_start: cannot claim usb interface\n");
	  return SANE_STATUS_DEVICE_BUSY;
	}
    }

  /* check if we need warming-up */
  sanei_rts88xx_get_lamp_status (dev->devnum, dev->regs);
  if ((dev->regs[0x8e] & 0x60) != 0x60)
    {
      DBG (DBG_info, "sane_start: lamp needs warming (0x%02x)\n",
	   dev->regs[0x8e]);
      dev->needs_warming = SANE_TRUE;
#ifdef HAVE_SYS_TIME_H
      dev->start_time.tv_sec = current.tv_sec;
      dev->last_scan.tv_sec = current.tv_sec;
#endif
    }

#ifdef HAVE_SYS_TIME_H
  /* if last scan time is more than 10 minutes ago, scanner also needs
   * warming-up */
  gettimeofday (&current, NULL);
  if ((current.tv_sec - dev->last_scan.tv_sec) / 60 > 10)
    {
      DBG (DBG_info,
	   "sane_start: more than 10 minutes without scanning, lamp needs warming\n");
      dev->needs_warming = SANE_TRUE;
      dev->start_time.tv_sec = current.tv_sec;
      dev->last_scan.tv_sec = current.tv_sec;
    }
#endif

  /* if parking, wait for head to stop */
  if(dev->parking==SANE_TRUE)
    {
      rts8891_wait_for_home (dev, dev->regs);
    }

  /* reinit registers to start values */
  rts8891_set_default_regs (dev->regs);

  /* step 0: light up */
  init_lamp (dev);

  /* do warming up if needed: just detected or at sane_open() */
  if (dev->needs_warming == SANE_TRUE)
    {
      DBG (DBG_info, "sane_start: warming lamp ...\n");
#ifdef HAVE_SYS_TIME_H
      /* check if current time is >15s after warming-up start */
      gettimeofday (&current, NULL);
      if ((current.tv_sec - dev->start_time.tv_sec) < 15)
	{
#ifdef SANE_STATUS_WARMING_UP
	  if (dev->conf.allowsharing == SANE_TRUE)
	    {
	      sanei_usb_release_interface (dev->devnum, 0);
	    }
	  return SANE_STATUS_WARMING_UP;
#else
	  DBG (DBG_info,
	       "sane_start: waiting to let lamp get warm enough ...\n");
	  sleep (15 - (current.tv_sec - dev->start_time.tv_sec));
#endif
	}
#else
      DBG (DBG_info,
	   "sane_start: waiting 15s to let lamp get warm enough ...\n");
      sleep (15);
#endif
    }
  dev->needs_warming = SANE_FALSE;

  /* restore default brightness */
  dev->regs[LAMP_BRIGHT_REG] = 0xA7;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_BRIGHT_REG,
			   dev->regs + LAMP_BRIGHT_REG);
  DBG (DBG_info, "sane_start: sensor initial type is %s (%d)\n",
       sensor_name (dev->sensor), dev->sensor);

  /* step 1: locate scan area by doing a scan, then goto calibration area */
  /* we also detect if the sensor type is inadequate and then change it   */
  do
    {
      changed = SANE_FALSE;
      status = find_origin (dev, &changed);
      if (status != SANE_STATUS_GOOD)
	{
	  if (dev->conf.allowsharing == SANE_TRUE)
	    {
	      sanei_usb_release_interface (dev->devnum, 0);
	    }
	  DBG (DBG_error, "sane_start: failed to find origin!\n");
	  return status;
	}

      /* in case find_origin detected we need to change sensor */
      if (changed)
	{
	  /* for a sensor 'pair', we switch */
	  switch (dev->sensor)
	    {
	    case SENSOR_TYPE_BARE:
	      DBG (DBG_info,
		   "sane_start: sensor changed to type 'SENSOR_TYPE_XPA'!\n");
	      dev->sensor = SENSOR_TYPE_XPA;
	      break;
	    case SENSOR_TYPE_XPA:
	      DBG (DBG_info,
		   "sane_start: sensor changed to type 'SENSOR_TYPE_BARE'!\n");
	      dev->sensor = SENSOR_TYPE_BARE;
	      break;
	    case SENSOR_TYPE_4400:
	      DBG (DBG_info,
		   "sane_start: sensor changed to type '4400 SENSOR_TYPE_4400_BARE'!\n");
	      dev->sensor = SENSOR_TYPE_4400_BARE;
	      break;
	    case SENSOR_TYPE_4400_BARE:
	      DBG (DBG_info,
		   "sane_start: sensor changed to type 'SENSOR_TYPE_4400'!\n");
	      dev->sensor = SENSOR_TYPE_4400;
	      break;
	    }
	}
    }
  while (changed);

  /* light source to use */
  switch (dev->sensor)
    {
    case SENSOR_TYPE_XPA:
      light = 0x3f;
      mode = 0x20;
      break;
    case SENSOR_TYPE_BARE:
      light = 0x3b;
      mode = 0x20;
      break;
    case SENSOR_TYPE_4400:
    case SENSOR_TYPE_4400_BARE:
      light = 0x2a;
      mode = 0x10;
      break;
    default:
      light = 0x3b;
      mode = 0x20;
      break;
    }
  DBG (DBG_info, "sane_start: sensor final type is %s (%d)\n",
       sensor_name (dev->sensor), dev->sensor);
  DBG (DBG_info, "sane_start: mode=0x%02x, light=0x%02x\n", mode, light);

  /* step 2: dark calibration */
  status = dark_calibration (dev, mode, light);
  if (status != SANE_STATUS_GOOD)
    {
      if (dev->conf.allowsharing == SANE_TRUE)
	{
	  sanei_usb_release_interface (dev->devnum, 0);
	}
      DBG (DBG_error, "sane_start: failed to do dark calibration!\n");
      return status;
    }

  /* step 3: find left margin */
  status = find_margin (dev);
  if (status != SANE_STATUS_GOOD)
    {
      if (dev->conf.allowsharing == SANE_TRUE)
	{
	  sanei_usb_release_interface (dev->devnum, 0);
	}
      DBG (DBG_error, "sane_start: failed find left margin!\n");
      return status;
    }

  /* restore brightness */
  dev->regs[LAMP_BRIGHT_REG] = 0xA7;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_BRIGHT_REG,
			   dev->regs + LAMP_BRIGHT_REG);

  /* step 4: gain calibration */
  status = gain_calibration (dev, mode, light);
  if (status != SANE_STATUS_GOOD)
    {
      if (dev->conf.allowsharing == SANE_TRUE)
	{
	  sanei_usb_release_interface (dev->devnum, 0);
	}
      DBG (DBG_error, "sane_start: failed to do gain calibration!\n");
      return status;
    }

  /* step 5: fine offset calibration */
  status = offset_calibration (dev, mode, light);
  if (status != SANE_STATUS_GOOD)
    {
      if (dev->conf.allowsharing == SANE_TRUE)
	{
	  sanei_usb_release_interface (dev->devnum, 0);
	}
      DBG (DBG_error, "sane_start: failed to do offset calibration!\n");
      return status;
    }

  /* we compute all the scan parameters so that */
  /* we will be able to set up the registers correctly */
  compute_parameters (session);

  /* allocate buffer and compute start pointer */
  if (dev->scanned_data != NULL)
    free (dev->scanned_data);
  dev->scanned_data =
    (SANE_Byte *) malloc (dev->data_size + dev->lds_max + dev->ripple);
  dev->start = dev->scanned_data + dev->lds_max + dev->ripple;

  /* computes data end pointer */
  dev->end = dev->start + dev->data_size;

  /* no bytes sent yet to frontend */
  session->sent = 0;

  sanei_rts88xx_set_offset (dev->regs, dev->red_offset, dev->green_offset,
			    dev->blue_offset);
  sanei_rts88xx_set_gain (dev->regs, dev->red_gain, dev->green_gain,
			  dev->blue_gain);

  /* step 6: shading calibration */
  status =
    shading_calibration (dev, session->params.format == SANE_FRAME_RGB
			 || session->emulated_gray == SANE_TRUE, mode, light);
  if (status != SANE_STATUS_GOOD)
    {
      if (dev->conf.allowsharing == SANE_TRUE)
	{
	  sanei_usb_release_interface (dev->devnum, 0);
	}
      DBG (DBG_error, "sane_start: failed to do shading calibration!\n");
      return status;
    }

  /* step 6b: move at dev->model->min_ydpi to target area */
  if (dev->ydpi > dev->model->min_ydpi
      && (dev->ystart * MOVE_DPI) / dev->ydpi > 150)
    {
      status = move_to_scan_area (session);
      if (status != SANE_STATUS_GOOD)
	{
	  if (dev->conf.allowsharing == SANE_TRUE)
	    {
	      sanei_usb_release_interface (dev->devnum, 0);
	    }
	  DBG (DBG_error, "sane_start: failed to move to scan area!\n");
	  return status;
	}
    }

  /* step 7: effective scan start   */
  /* build register set and send it */
  status = write_scan_registers (session);
  if (status != SANE_STATUS_GOOD)
    {
      if (dev->conf.allowsharing == SANE_TRUE)
	{
	  sanei_usb_release_interface (dev->devnum, 0);
	}
      DBG (DBG_error, "sane_start: failed to write scan registers!\n");
      return status;
    }

  /* step 8: send calibration data  */
  status = send_calibration_data (session);
  if (status != SANE_STATUS_GOOD)
    {
      if (dev->conf.allowsharing == SANE_TRUE)
	{
	  sanei_usb_release_interface (dev->devnum, 0);
	}
      DBG (DBG_error, "sane_start: failed to send calibration data!\n");
      return status;
    }

  /* commit the scan */
  /* TODO send_calibration seems to embbed its commit */
  sanei_rts88xx_cancel (dev->devnum);
  sanei_rts88xx_write_control (dev->devnum, 0x08);
  status = sanei_rts88xx_write_control (dev->devnum, 0x08);

  session->scanning = SANE_TRUE;

  DBG (DBG_proc, "sane_start: exit\n");
  return status;
}

/**
 * This function computes two set of parameters. The one for the SANE's standard
 * and the other for the hardware. Among these parameters are the bit depth, total 
 * number of lines, total number of columns, extra line to read for data reordering... 
 */
static SANE_Status
compute_parameters (Rts8891_Session * session)
{
  Rts8891_Device *dev = session->dev;
  SANE_Int dpi;			/* dpi for scan */
  SANE_String mode;
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int yshift;
  SANE_Int xshift;

  int tl_x, tl_y, br_x, br_y;

  mode = session->val[OPT_MODE].s;
  dpi = session->val[OPT_RESOLUTION].w;

  /* scan coordinates */
  tl_x = SANE_UNFIX (session->val[OPT_TL_X].w);
  tl_y = SANE_UNFIX (session->val[OPT_TL_Y].w);
  br_x = SANE_UNFIX (session->val[OPT_BR_X].w);
  br_y = SANE_UNFIX (session->val[OPT_BR_Y].w);

  /* only single pass scanning supported */
  session->params.last_frame = SANE_TRUE;
  session->emulated_gray = SANE_FALSE;

  /* gray modes */
  dev->threshold = (255 * SANE_UNFIX (session->val[OPT_THRESHOLD].w)) / 100;
  if (strcmp (mode, GRAY_MODE) == 0 || strcmp (mode, LINEART_MODE) == 0)
    {
      session->params.format = SANE_FRAME_GRAY;
      if (dev->model->flags & RTS8891_FLAG_EMULATED_GRAY_MODE)
	session->emulated_gray = SANE_TRUE;
    }
  else
    {
      /* Color */
      session->params.format = SANE_FRAME_RGB;
    }

  /* SANE level values */
  session->params.lines = ((br_y - tl_y) * dpi) / MM_PER_INCH;
  if (session->params.lines == 0)
    session->params.lines = 1;
  session->params.pixels_per_line = ((br_x - tl_x) * dpi) / MM_PER_INCH;
  if (session->params.pixels_per_line == 0)
    session->params.pixels_per_line = 1;

  DBG (DBG_data, "compute_parameters: pixels_per_line   =%d\n",
       session->params.pixels_per_line);

  if (strcmp (mode, LINEART_MODE) == 0)
    {
      session->params.depth = 1;
      /* in lineart, having pixels multiple of 8 avoids a costly test */
      /* at each bit to see we must go to the next byte               */
      /* TODO : implement this requirement in sane_control_option */
      session->params.pixels_per_line =
	((session->params.pixels_per_line + 7) / 8) * 8;
    }
  else
    session->params.depth = 8;

  /* width needs to be even */
  if (session->params.pixels_per_line & 1)
    session->params.pixels_per_line++;

  /* Hardware settings : they can differ from the ones at SANE level */
  /* for instance the effective DPI used by a sensor may be higher   */
  /* than the one needed for the SANE scan parameters                */
  dev->lines = session->params.lines;
  dev->pixels = session->params.pixels_per_line;

  /* motor and sensor DPI */
  dev->xdpi = dpi;
  dev->ydpi = dpi;

  /* handle bounds of motor's dpi range */
  if (dev->ydpi > dev->model->max_ydpi)
    {
      dev->ydpi = dev->model->max_ydpi;
      dev->lines = (dev->lines * dev->model->max_ydpi) / dpi;
      if (dev->lines == 0)
	dev->lines = 1;

      /* round number of lines */
      session->params.lines =
	(session->params.lines / dev->lines) * dev->lines;
      if (session->params.lines == 0)
	session->params.lines = 1;
    }
  if (dev->ydpi < dev->model->min_ydpi)
    {
      dev->ydpi = dev->model->min_ydpi;
      dev->lines = (dev->lines * dev->model->min_ydpi) / dpi;
    }

  /* hardware values */
  dev->xstart =
    ((SANE_UNFIX (dev->model->x_offset) + tl_x) * dev->xdpi) / MM_PER_INCH;
  dev->ystart =
    ((SANE_UNFIX (dev->model->y_offset) + tl_y) * dev->ydpi) / MM_PER_INCH;

  /* xstart needs to be even */
  if (dev->xstart & 1)
    dev->xstart++;

  /* computes bytes per line */
  session->params.bytes_per_line = session->params.pixels_per_line;
  dev->bytes_per_line = dev->pixels;
  if (session->params.format == SANE_FRAME_RGB
      || session->emulated_gray == SANE_TRUE)
    {
      if (session->emulated_gray != SANE_TRUE)
	{
	  session->params.bytes_per_line *= 3;
	}
      dev->bytes_per_line *= 3;
    }
  session->to_send = session->params.bytes_per_line * session->params.lines;

  /* in lineart mode we adjust bytes_per_line needed by frontend */
  /* we do that here because we needed sent/to_send to be as if  */
  /* there was no lineart                                        */
  if (session->params.depth == 1)
    {
      session->params.bytes_per_line =
	(session->params.bytes_per_line + 7) / 8;
    }

  /* compute how many extra bytes we need to reorder data */
  dev->ripple = 0;
  if (session->params.format == SANE_FRAME_RGB
      || session->emulated_gray == SANE_TRUE)
    {
      dev->lds_r
	= ((dev->model->ld_shift_r * dev->ydpi) / dev->model->max_ydpi)
	* dev->bytes_per_line;
      dev->lds_g
	= ((dev->model->ld_shift_g * dev->ydpi) / dev->model->max_ydpi)
	* dev->bytes_per_line;
      dev->lds_b
	= ((dev->model->ld_shift_b * dev->ydpi) / dev->model->max_ydpi)
	* dev->bytes_per_line;
      if (dev->xdpi == dev->model->max_xdpi)
	{
	  dev->ripple = 2 * dev->bytes_per_line;
	}
    }
  else
    {
      dev->lds_r = 0;
      dev->lds_g = 0;
      dev->lds_b = 0;
    }

  /* pick max lds value */
  dev->lds_max = dev->lds_r;
  if (dev->lds_g > dev->lds_max)
    dev->lds_max = dev->lds_g;
  if (dev->lds_b > dev->lds_max)
    dev->lds_max = dev->lds_b;

  /* since the extra lines for reordering are before data */
  /* we substract lds_max */
  dev->lds_r -= dev->lds_max;
  dev->lds_g -= dev->lds_max;
  dev->lds_b -= dev->lds_max;

  /* we need to add extra line to handle line distance reordering  */
  /* highest correction value is the blue one for our case         */
  /* decrease y start to take these extra lines into account       */
  dev->lines += (dev->lds_max + dev->ripple) / dev->bytes_per_line;

  /* shading calibration is allways 66 lines regardless of ydpi, so */
  /* we take this into account to compute ystart                    */
  if (dev->ydpi > dev->model->min_ydpi)
    {
      /* no that clean, but works ... */
      switch (dev->ydpi)
	{
	case 300:
	  yshift = 0;
	  break;
	case 600:
	  yshift = 33;
	  break;
	default:
	  yshift = 0;
	}
      dev->ystart += yshift;
    }
  dev->ystart -= (dev->lds_max + dev->ripple) / dev->bytes_per_line;

  /* 1200 and 600 dpi scans seems to have other timings that
   * need their own xstart */
  /* no that clean, but works ... */
  switch (dev->xdpi)
    {
    case 600:
      xshift = -38;
      break;
    case 1200:
      xshift = -76;
      break;
    default:
      xshift = 0;
    }
  dev->xstart += xshift;

  dev->read = 0;
  dev->to_read = dev->lines * dev->bytes_per_line;

  /* compute best size for scanned data buffer  */
  /* we must have a round number of lines, with */
  /* enough space to handle line distance shift */
  if (dev->xdpi < dev->model->max_ydpi)
    {
      dev->data_size =
	(PREFERED_BUFFER_SIZE / dev->bytes_per_line) * (dev->bytes_per_line);
    }
  else
    {
      dev->data_size =
	(((PREFERED_BUFFER_SIZE / 2) - dev->lds_max -
	  dev->ripple) / dev->bytes_per_line) * dev->bytes_per_line;
    }

  /* 32 lines minimum in buffer */
  if (dev->data_size < 32 * dev->bytes_per_line)
    dev->data_size = 32 * dev->bytes_per_line;

  /* buffer must be smaller than total amount to read from session */
  if (dev->data_size > dev->to_read)
    dev->data_size = dev->to_read;

  DBG (DBG_data, "compute_parameters: bytes_per_line    =%d\n",
       session->params.bytes_per_line);
  DBG (DBG_data, "compute_parameters: depth             =%d\n",
       session->params.depth);
  DBG (DBG_data, "compute_parameters: lines             =%d\n",
       session->params.lines);
  DBG (DBG_data, "compute_parameters: pixels_per_line   =%d\n",
       session->params.pixels_per_line);
  DBG (DBG_data, "compute_parameters: image size        =%d\n",
       session->to_send);

  DBG (DBG_data, "compute_parameters: xstart            =%d\n", dev->xstart);
  DBG (DBG_data, "compute_parameters: ystart            =%d\n", dev->ystart);
  DBG (DBG_data, "compute_parameters: dev lines         =%d\n", dev->lines);
  DBG (DBG_data, "compute_parameters: dev extra lines   =%d\n",
       (dev->lds_max + dev->ripple) / dev->bytes_per_line);
  DBG (DBG_data, "compute_parameters: dev bytes per line=%d\n",
       dev->bytes_per_line);
  DBG (DBG_data, "compute_parameters: dev pixels        =%d\n", dev->pixels);
  DBG (DBG_data, "compute_parameters: data size         =%d\n",
       dev->data_size);
  DBG (DBG_data, "compute_parameters: to read           =%d\n", dev->to_read);
  DBG (DBG_data, "compute_parameters: threshold         =%d\n",
       dev->threshold);

  return status;
}


/**
 * Called by SANE to retrieve information about the type of data
 * that the current scan will return.
 *
 * From the SANE spec:
 * This function is used to obtain the current scan parameters. The
 * returned parameters are guaranteed to be accurate between the time
 * a scan has been started (sane_start() has been called) and the
 * completion of that request. Outside of that window, the returned
 * values are best-effort estimates of what the parameters will be
 * when sane_start() gets invoked.
 * 
 * Calling this function before a scan has actually started allows,
 * for example, to get an estimate of how big the scanned image will
 * be. The parameters passed to this function are the handle of the
 * device for which the parameters should be obtained and a pointer
 * to a parameter structure.
 */
SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  SANE_Status status;
  struct Rts8891_Session *session = (struct Rts8891_Session *) handle;

  DBG (DBG_proc, "sane_get_parameters: start\n");

  /* call parameters computing function */
  status = compute_parameters (session);
  if (status == SANE_STATUS_GOOD && params)
    *params = session->params;

  DBG (DBG_proc, "sane_get_parameters: exit\n");
  return status;
}


/**
 * Called by SANE to read data.
 * 
 * From the SANE spec:
 * This function is used to read image data from the device
 * represented by handle h.  Argument buf is a pointer to a memory
 * area that is at least maxlen bytes long.  The number of bytes
 * returned is stored in *len. A backend must set this to zero when
 * the call fails (i.e., when a status other than SANE_STATUS_GOOD is
 * returned).
 * 
 * When the call succeeds, the number of bytes returned can be
 * anywhere in the range from 0 to maxlen bytes.
 */
SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf,
	   SANE_Int max_len, SANE_Int * len)
{
  struct Rts8891_Session *session;
  struct Rts8891_Device *dev;
  SANE_Status status;
  SANE_Int length;
  SANE_Int data_size;
  SANE_Byte val = 0;
  SANE_Int bit;

  DBG (DBG_proc, "sane_read: start\n");
  DBG (DBG_io, "sane_read: up to %d bytes required by frontend\n", max_len);

  /* some sanity checks first to protect from would be buggy frontends */
  if (!handle)
    {
      DBG (DBG_error, "sane_read: handle is null!\n");
      return SANE_STATUS_INVAL;
    }

  session = (struct Rts8891_Session *) handle;
  if (!session)
    {
      DBG (DBG_error, "sane_read: session is null!\n");
      return SANE_STATUS_INVAL;
    }
  dev = session->dev;

  if (!buf)
    {
      DBG (DBG_error, "sane_read: buf is null!\n");
      return SANE_STATUS_INVAL;
    }

  if (!len)
    {
      DBG (DBG_error, "sane_read: len is null!\n");
      return SANE_STATUS_INVAL;
    }

  /* no data read yet */
  *len = 0;
  buf[*len] = 0;

  /* check if session is scanning */
  if (!session->scanning)
    {
      DBG (DBG_warn,
	   "sane_read: scan was cancelled, is over or has not been initiated yet\n");
      return SANE_STATUS_CANCELLED;
    }

  /* check for EOF, must be done before any physical read */
  if (session->sent >= session->to_send)
    {
      DBG (DBG_io, "sane_read: end of scan reached\n");

      /* signal end of scanning */
      session->scanning=SANE_FALSE;

      /* send asynchronous head parking command then return */
      park_head (session->dev, SANE_FALSE);
      return SANE_STATUS_EOF;
    }

  dev->regs[LAMP_REG] = 0xad;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &(dev->regs[LAMP_REG]));

  /* byte length for high dpi mode */
  length = (session->params.bytes_per_line * 8) / session->params.depth;

  /* checks if buffer has been pre filled with the extra data needed for */
  /* line distance reordering                                            */
  if (dev->read == 0
      && (session->params.format == SANE_FRAME_RGB
	  || session->emulated_gray == SANE_TRUE))
    {
      /* the data need if the size of the highest line distance shift */
      /* we must only read what's needed */
      data_size = dev->data_size + dev->lds_max + dev->ripple;
      if (dev->to_read - dev->read < data_size)
	data_size = dev->to_read - dev->read;

      status = read_data (session, dev->scanned_data, data_size);
      if (status == SANE_STATUS_DEVICE_BUSY)
	{
	  DBG (DBG_io, "sane_read: no data currently available\n");
	  return SANE_STATUS_GOOD;
	}
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "sane_read: failed to read data from scanner\n");
	  return status;
	}

      /* now buffer is filled with data and an extra amount for line reordering */
      dev->current = dev->start;
    }

  /* if all buffer has been used, get the next block of data from scanner */
  if (dev->read == 0 || dev->current >= dev->end)
    {
      /* effective buffer filling */
      /* we must only read what's needed */
      data_size = dev->data_size;
      if (dev->to_read - dev->read < data_size)
	data_size = dev->to_read - dev->read;

      status = read_data (session, dev->start, data_size);
      if (status == SANE_STATUS_DEVICE_BUSY)
	{
	  DBG (DBG_io, "sane_read: no data currently available\n");
	  return SANE_STATUS_GOOD;
	}
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "sane_read: failed to read data from scanner\n");
	  return status;
	}

      /* reset current pointer */
      dev->current = dev->start;
    }

  dev->regs[LAMP_REG] = 0x8d;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &(dev->regs[LAMP_REG]));

  /* it seems there is no gray or lineart hardware mode for the rts8891 */
  if (session->params.format == SANE_FRAME_GRAY
      && session->emulated_gray == SANE_FALSE)
    {
      DBG (DBG_error0, "sane_read: unimplemented native gray scanning\n");
      return SANE_STATUS_INVAL;
    }
  else				/* hardware data is 3 bytes/per pixels */
    {
      /* we return as much data as possible from current buffer  */
      /* if buffer is smaller than what is asked, we only return */

      /* here we have made sure there is something to read from data buffers */
      /* data from scanner is in R,G,B format, but we have to cope with line */
      /* distance correction. We also take care that motor resolution may be */
      /* higher than the scan resolution asked for some modes.               */
      /* We also handle emulated color mode                                  */

      /* we loop until we reach max_len or end of data buffer  */
      /* or we sent what we advised to frontend                */
      while (dev->current < dev->end && *len < max_len
	     && session->sent < session->to_send)
	{
	  /* data is received in RGB format */
	  /* these switches are there to handle reads not aligned
	   * on pixels that are allowed by the SANE standard */
	  if (dev->xdpi == dev->model->max_xdpi)
	    {			/* at max xdpi, data received is distorted and ydpi is half of xdpi */
	      if (session->emulated_gray == SANE_TRUE)
		{
		  /* in emulated gray mode we are allways reading 3 bytes of raw data */
		  /* at a time                                                        */
		  switch (((session->sent * 3) % dev->bytes_per_line) % 6)
		    {
		    case 0:
		    case 1:
		    case 2:
		      val = dev->current[dev->lds_g];
		      break;
		    case 3:
		    case 4:
		    case 5:
		      val = dev->current[dev->lds_g - dev->ripple];
		      break;
		    }

		  if (session->params.depth == 1)
		    {
		      bit = 7 - (session->sent) % 8;
		      if (val <= dev->threshold)
			{
			  buf[*len] |= 1 << bit;
			}
		      if (bit == 0)
			{
			  (*len)++;
			  buf[*len] = 0;
			}
		    }
		  else
		    {
		      buf[*len] = val;
		      (*len)++;
		    }
		  session->sent++;
		  dev->current += 3;
		}
	      else
		{
		  switch ((session->sent % dev->bytes_per_line) % 6)
		    {
		    case 0:
		      buf[*len] = dev->current[dev->lds_r];
		      break;
		    case 1:
		      buf[*len] = dev->current[dev->lds_g];
		      break;
		    case 2:
		      buf[*len] = dev->current[dev->lds_b];
		      break;
		    case 3:
		      buf[*len] = dev->current[dev->lds_r - dev->ripple];
		      break;
		    case 4:
		      buf[*len] = dev->current[dev->lds_g - dev->ripple];
		      break;
		    case 5:
		      buf[*len] = dev->current[dev->lds_b - dev->ripple];
		      break;
		    }
		  (*len)++;
		  session->sent++;
		  dev->current++;
		}

	      /* we currently double lines to have match xdpy */
	      /* so we logically rewind for each odd line     */
	      /* we test at start of a scanned picture line   */
	      if (((session->sent / length) % 2 == 1)
		  && (session->sent % length == 0))
		{
		  DBG (DBG_io,
		       "sane_read: rewind by %d bytes after %d bytes sent\n",
		       dev->bytes_per_line, session->sent);
		  dev->current -= dev->bytes_per_line;
		}
	    }
	  else if (dev->ydpi == session->val[OPT_RESOLUTION].w)
	    {
	      if (session->emulated_gray == SANE_TRUE)
		{
		  /* in emulated gray mode we are allways reading 3 bytes of raw data */
		  /* at a time, so we know where we are                               */
		  val = dev->current[dev->lds_g];
		  if (session->params.depth == 1)
		    {
		      bit = 7 - (session->sent) % 8;
		      if (val <= dev->threshold)
			{
			  buf[*len] |= 1 << bit;
			}
		      else
			{
			  buf[*len] &= ~(1 << bit);
			}
		      if (bit == 0)
			{
			  (*len)++;
			}
		    }
		  else
		    {
		      buf[*len] = val;
		      (*len)++;
		    }
		  session->sent++;
		  dev->current += 3;
		}
	      else
		{
		  switch ((dev->current - dev->start) % 3)
		    {
		    case 0:
		      buf[*len] = dev->current[dev->lds_r];
		      break;
		    case 1:
		      buf[*len] = dev->current[dev->lds_g];
		      break;
		    case 2:
		      buf[*len] = dev->current[dev->lds_b];
		      break;
		    }
		  (*len)++;
		  session->sent++;
		  dev->current++;
		}
	    }
	  else
	    {
	      /* we currently handle ydi=2*dpi */
	      if (session->emulated_gray == SANE_TRUE)
		{
		  /* in emulated gray mode we are allways reading 3 bytes of raw data */
		  /* at a time, so we know where we are                               */
		  val = (dev->current[dev->lds_g]
			 + dev->current[dev->lds_g +
					dev->bytes_per_line]) / 2;
		  if (session->params.depth == 1)
		    {
		      bit = 7 - (session->sent) % 8;
		      if (val <= dev->threshold)
			{
			  buf[*len] |= 1 << bit;
			}
		      else
			{
			  buf[*len] &= ~(1 << bit);
			}
		      if (bit == 0)
			{
			  (*len)++;
			}
		    }
		  else
		    {
		      buf[*len] = val;
		      (*len)++;
		    }
		  dev->current += 3;
		}
	      else
		{
		  switch ((dev->current - dev->start) % 3)
		    {
		    case 0:
		      buf[*len] = (dev->current[dev->lds_r]
				   + dev->current[dev->lds_r +
						  dev->bytes_per_line]) / 2;
		      break;
		    case 1:
		      buf[*len] = (dev->current[dev->lds_g]
				   + dev->current[dev->lds_g +
						  dev->bytes_per_line]) / 2;
		      break;
		    case 2:
		      buf[*len] = (dev->current[dev->lds_b]
				   + dev->current[dev->lds_b +
						  dev->bytes_per_line]) / 2;
		      break;
		    }
		  (*len)++;
		  dev->current++;
		}
	      session->sent++;

	      /* at the end of each line, we must count another one because */
	      /* 2 lines are used to produce one                            */
	      if ((dev->current - dev->start) % dev->bytes_per_line == 0)
		dev->current += dev->bytes_per_line;
	    }
	}
    }

  /* if we exhausted data buffer, we prepare for the next read */
  /* in color mode, we need to copy the remainder of the      */
  /* buffer because of line distance handling, when blue data */
  /* is exhausted, read and green haven't been fully read yet */
  if (dev->current >= dev->end
      && (session->params.format == SANE_FRAME_RGB
	  || session->emulated_gray == SANE_TRUE))
    {
      memcpy (dev->scanned_data, dev->end - (dev->lds_max + dev->ripple),
	      dev->lds_max + dev->ripple);
    }

  status = SANE_STATUS_GOOD;
  DBG (DBG_io, "sane_read: sent %d bytes to frontend\n", *len);
  DBG (DBG_proc, "sane_read: exit\n");
  return status;
}


/**
 * Cancels a scan. 
 *
 * From the SANE spec:
 * This function is used to immediately or as quickly as possible
 * cancel the currently pending operation of the device represented by
 * handle h.  This function can be called at any time (as long as
 * handle h is a valid handle) but usually affects long-running
 * operations only (such as image is acquisition). It is safe to call
 * this function asynchronously (e.g., from within a signal handler).
 * It is important to note that completion of this operaton does not
 * imply that the currently pending operation has been cancelled. It
 * only guarantees that cancellation has been initiated. Cancellation
 * completes only when the cancelled call returns (typically with a
 * status value of SANE_STATUS_CANCELLED).  Since the SANE API does
 * not require any other operations to be re-entrant, this implies
 * that a frontend must not call any other operation until the
 * cancelled operation has returned.
 */
void
sane_cancel (SANE_Handle handle)
{
  Rts8891_Session *session = handle;
  struct Rts8891_Device *dev = session->dev;
#ifdef HAVE_SYS_TIME_H
  struct timeval current;
#endif

  DBG (DBG_proc, "sane_cancel: start\n");

#ifdef HAVE_SYS_TIME_H
  /* store last scan time for the device */
  gettimeofday (&current, NULL);
  dev->last_scan.tv_sec = current.tv_sec;
#endif

  /* if scanning, abort and park head */
  if (session->scanning == SANE_TRUE)
    {
      /* canceling while all data hasn't bee read */
      if (dev->read < dev->to_read)
	{
	  sanei_rts88xx_cancel (dev->devnum);
	  usleep (500000);
	  sanei_rts88xx_cancel (dev->devnum);
	}
      session->scanning = SANE_FALSE;

      /* head parking */
      if (park_head (dev, SANE_FALSE) != SANE_STATUS_GOOD)
        {
          DBG (DBG_error, "sane_cancel: failed to park head!\n");
        }
    }

  /* free ressources used by scanning */
  if (dev->scanned_data != NULL)
    {
      free (dev->scanned_data);
      dev->scanned_data = NULL;
    }
  if (dev->shading_data != NULL)
    {
      free (dev->shading_data);
      dev->shading_data = NULL;
    }

  /* release the interface to allow device sharing */
  if (dev->conf.allowsharing == SANE_TRUE)
    {
      sanei_usb_release_interface (dev->devnum, 0);
    }

  DBG (DBG_proc, "sane_cancel: exit\n");
}


/**
 * Ends use of the session.
 * 
 * From the SANE spec:
 * This function terminates the association between the device handle
 * passed in argument h and the device it represents. If the device is
 * presently active, a call to sane_cancel() is performed first. After
 * this function returns, handle h must not be used anymore.
 *
 * Handle resources are free'd before disposing the handle. But devices
 * resources must not be mdofied, since it could be used or reused until
 * sane_exit() is called.
 */
void
sane_close (SANE_Handle handle)
{
  Rts8891_Session *prev, *session;
  Rts8891_Device *dev;
  int i;

  DBG (DBG_proc, "sane_close: start\n");

  /* remove handle from list of open handles: */
  prev = NULL;
  for (session = first_handle; session; session = session->next)
    {
      if (session == handle)
	break;
      prev = session;
    }
  if (!session)
    {
      DBG (DBG_error, "close: invalid handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }
  
  dev=session->dev;

  /* cancel any active scan */
  if (session->scanning == SANE_TRUE)
    {
      sane_cancel (handle);
    }
  /* if head is parking, wait for completion */
  if(dev->parking==SANE_TRUE)
    {
      rts8891_wait_for_home (dev, dev->regs);
    }
  set_lamp_brightness (dev, 0);
  

  if (prev)
    prev->next = session->next;
  else
    first_handle = session->next;

  /* switch off lamp and close usb */
  if (dev->conf.allowsharing == SANE_TRUE)
    {
      SANE_Status status = sanei_usb_claim_interface (dev->devnum, 0);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (DBG_warn, "sane_close: cannot claim usb interface: %s\n",
               sane_strstatus(status));
          DBG (DBG_warn, "sane_close: continuing anyway\n");
        }
    }
  set_lamp_state (session, 0);
  sanei_usb_close (dev->devnum);

  /* free per session data */
  if (session->dev->model->gamma != session->val[OPT_GAMMA_VECTOR].wa)
    free (session->val[OPT_GAMMA_VECTOR].wa);
  if (session->dev->model->gamma != session->val[OPT_GAMMA_VECTOR_R].wa)
    free (session->val[OPT_GAMMA_VECTOR_R].wa);
  if (session->dev->model->gamma != session->val[OPT_GAMMA_VECTOR_G].wa)
    free (session->val[OPT_GAMMA_VECTOR_G].wa);
  if (session->dev->model->gamma != session->val[OPT_GAMMA_VECTOR_B].wa)
    free (session->val[OPT_GAMMA_VECTOR_B].wa);
  free (session->val[OPT_MODE].s);
  free (session->opt[OPT_RESOLUTION].constraint.word_list);
  for (i = OPT_BUTTON_1; i <= OPT_BUTTON_11; i++)
    {
      free (session->opt[i].name);
      free (session->opt[i].title);
    }

  free (session);

  DBG (DBG_proc, "sane_close: exit\n");
}


/**
 * Terminates the backend.
 * 
 * From the SANE spec:
 * This function must be called to terminate use of a backend. The
 * function will first close all device handles that still might be
 * open (it is recommended to close device handles explicitly through
 * a call to sane_close(), but backends are required to release all
 * resources upon a call to this function). After this function
 * returns, no function other than sane_init() may be called
 * (regardless of the status value returned by sane_exit(). Neglecting
 * to call this function may result in some resources not being
 * released properly.
 */
void
sane_exit (void)
{
  struct Rts8891_Session *session, *next;
  struct Rts8891_Device *dev, *nextdev;
  int i;

  DBG (DBG_proc, "sane_exit: start\n");

  /* free session structs */
  for (session = first_handle; session; session = next)
    {
      next = session->next;
      sane_close ((SANE_Handle *) session);
      free (session);
    }
  first_handle = NULL;

  /* free devices structs */
  for (dev = first_device; dev; dev = nextdev)
    {
      nextdev = dev->next;
      free (dev->file_name);
      free (dev);
    }
  first_device = NULL;

  /* now list of devices */
  if (devlist)
    {
      for (i = 0; i < num_devices; i++)
	{
	  free ((SANE_Device *) devlist[i]);
	}
      free (devlist);
      devlist = NULL;
    }
  num_devices = 0;

  DBG (DBG_proc, "sane_exit: exit\n");
}


/** This function tries to find plugged relevant devices
 */
static SANE_Status
probe_rts8891_devices (void)
{
  /**> configuration structure used during attach */
  SANEI_Config config;
  /**> list of configuration options */
  SANE_Option_Descriptor *options[NUM_CFG_OPTIONS];
  /**> placeholders pointers for option values */
  void *values[NUM_CFG_OPTIONS];
  int i;
  SANE_Status status;

  DBG (DBG_proc, "probe_rts8891_devices: start\n");

  /* sharing is off by default and no model option */
  rtscfg.allowsharing = SANE_FALSE;
  rtscfg.modelnumber = -1;
  rtscfg.sensornumber = -1;

  /* initialize configuration options */
  options[CFG_MODEL_NUMBER] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[CFG_MODEL_NUMBER]->name = "modelnumber";
  options[CFG_MODEL_NUMBER]->desc =
    "user provided scanner's internal model number";
  options[CFG_MODEL_NUMBER]->type = SANE_TYPE_INT;
  options[CFG_MODEL_NUMBER]->unit = SANE_UNIT_NONE;
  options[CFG_MODEL_NUMBER]->size = sizeof (SANE_Word);
  options[CFG_MODEL_NUMBER]->cap = SANE_CAP_SOFT_SELECT;
  options[CFG_MODEL_NUMBER]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[CFG_MODEL_NUMBER]->constraint.range = &model_range;
  values[CFG_MODEL_NUMBER] = &rtscfg.modelnumber;

  options[CFG_ALLOW_SHARING] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[CFG_ALLOW_SHARING]->name = "allowsharing";
  options[CFG_ALLOW_SHARING]->desc =
    "allow sharing of the scanner by several frontends";
  options[CFG_ALLOW_SHARING]->type = SANE_TYPE_BOOL;
  options[CFG_ALLOW_SHARING]->unit = SANE_UNIT_NONE;
  options[CFG_ALLOW_SHARING]->size = sizeof (SANE_Bool);
  options[CFG_ALLOW_SHARING]->cap = SANE_CAP_SOFT_SELECT;
  options[CFG_ALLOW_SHARING]->constraint_type = SANE_CONSTRAINT_NONE;
  values[CFG_ALLOW_SHARING] = &rtscfg.allowsharing;

  /* sensor type override */
  options[CFG_SENSOR_NUMBER] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[CFG_SENSOR_NUMBER]->name = "sensornumber";
  options[CFG_SENSOR_NUMBER]->desc =
    "user provided scanner's internal sensor number";
  options[CFG_SENSOR_NUMBER]->type = SANE_TYPE_INT;
  options[CFG_SENSOR_NUMBER]->unit = SANE_UNIT_NONE;
  options[CFG_SENSOR_NUMBER]->size = sizeof (SANE_Word);
  options[CFG_SENSOR_NUMBER]->cap = SANE_CAP_SOFT_SELECT;
  options[CFG_SENSOR_NUMBER]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[CFG_SENSOR_NUMBER]->constraint.range = &sensor_range;
  values[CFG_SENSOR_NUMBER] = &rtscfg.sensornumber;

  /* set configuration options structure */
  config.descriptors = options;
  config.values = values;
  config.count = NUM_CFG_OPTIONS;

  /* init usb use */
  sanei_usb_init ();

  /* generic configure and attach function */
  status = sanei_configure_attach (RTS8891_CONFIG_FILE, &config,
				   config_attach_rts8891);
  /* free allocated options */
  for (i = 0; i < NUM_CFG_OPTIONS; i++)
    {
      free (options[i]);
    }

  DBG (DBG_proc, "probe_rts8891_devices: end\n");
  return status;
}

/** This function is called by sanei_configure_attach to try
 * to attach the backend to a device specified by the configuration file.
 *
 * @param config configuration structure filled with values read
 * 	         from configuration file
 * @param devname name of the device to try to attach to, it is
 * 	          the unprocessed line of the configuration file
 *
 * @return status SANE_STATUS_GOOD if no errors (even if no matching
 * 	    devices found)
 * 	   SANE_STATUS_INVAL in case of error
 */
static SANE_Status
config_attach_rts8891 (SANEI_Config * config, const char *devname)
{
  /* currently, the config is a global variable so config is useless here */
  /* the correct thing would be to have a generic sanei_attach_matching_devices
   * using an attach function with a config parameter */
  if(config==NULL)
    {
      return SANE_STATUS_INVAL;
    }

  /* the devname has been processed and is ready to be used 
   * directly. Since the backend is an USB only one, we can 
   * call sanei_usb_attach_matching_devices straight */
  sanei_usb_attach_matching_devices (devname, attach_rts8891);

  return SANE_STATUS_GOOD;
}

/**
 * The attach tries to open the given usb device and match it
 * with devices handled by the backend. The configuration parameter
 * contains the values of the allready parsed configuration options
 * from the conf file.
 * @param config configuration structure filled with values read
 * 	         from configuration file
 * @param devicename name of the device to try to attach to, it is
 * 	          the unprocessed line of the configuration file
 *
 * @return status SANE_STATUS_GOOD if no errors (even if no matching
 * 	    devices found)
 * 	   SANE_STATUS_NOM_MEM if there isn't enough memory to allocate the
 * 	   			device structure
 * 	   SANE_STATUS_UNSUPPORTED if the device if unknown by the backend
 * 	   SANE_STATUS_INVAL in case of other error
 */
static SANE_Status
attach_rts8891 (const char *devicename)
{
  struct Rts8891_Device *device;
  SANE_Int dn, vendor, product;
  SANE_Status status;

  DBG (DBG_proc, "attach_rts8891(%s): start\n", devicename);

  /* search if we already have it attached */
  for (device = first_device; device; device = device->next)
    {
      if (strcmp (device->file_name, devicename) == 0)
	{
	  DBG (DBG_warn,
	       "attach_rts8891: device already attached (is ok)!\n");
	  DBG (DBG_proc, "attach_rts8891: exit\n");
	  return SANE_STATUS_GOOD;
	}
    }

  /* open usb device */
  status = sanei_usb_open (devicename, &dn);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "attach_rts8891: couldn't open device `%s': %s\n",
	   devicename, sane_strstatus (status));
      return status;
    }
  else
    {
      DBG (DBG_info, "attach_rts8891: device `%s' successfully opened\n",
	   devicename);
    }

  /* get vendor and product id */
  status = sanei_usb_get_vendor_product (dn, &vendor, &product);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "attach_rts8891: couldn't get vendor and product ids of device `%s': %s\n",
	   devicename, sane_strstatus (status));
      sanei_usb_close (dn);
      DBG (DBG_proc, "attach_rts8891: exit\n");
      return status;
    }
  sanei_usb_close (dn);

  /* get the index of the device in the device description table */
  /* if the value is provided by configuration option, just use it */
  if (rtscfg.modelnumber < 0)
    {
      /* walk the list of devices to find matching entry */
      dn = 0;
      while ((vendor != rts8891_usb_device_list[dn].vendor_id
	      || product != rts8891_usb_device_list[dn].product_id)
	     && (rts8891_usb_device_list[dn].vendor_id != 0))
	dn++;
      /* if we reach the list termination entry, the device is unknown */
      if (rts8891_usb_device_list[dn].vendor_id == 0)
	{
	  DBG (DBG_info,
	       "attach_rts8891: unknown device `%s': 0x%04x:0x%04x\n",
	       devicename, vendor, product);
	  DBG (DBG_proc, "attach_rts8891: exit\n");
	  return SANE_STATUS_UNSUPPORTED;
	}
    }
  else
    {
      /* user provided value */
      dn = rtscfg.modelnumber;
    }

  /* allocate device struct */
  device = malloc (sizeof (*device));
  if (device == NULL)
    {
      return SANE_STATUS_NO_MEM;
    }
  memset (device, 0, sizeof (*device));

  /* struct describing low lvel device */
  device->model = rts8891_usb_device_list[dn].model;

  /* name of the device */
  device->file_name = strdup (devicename);

  DBG (DBG_info, "attach_rts8891: found %s %s %s at %s\n",
       device->model->vendor,
       device->model->product, device->model->type, device->file_name);

  /* we insert new device at start of the chained list */
  /* head of the list becomes the next, and start is replaced */
  /* with the new session struct */
  num_devices++;
  device->next = first_device;
  first_device = device;

  device->reg_count = 244;
  /* intialization is done at sane_open */
  device->initialized = SANE_FALSE;
  device->needs_warming = SANE_TRUE;
  device->parking = SANE_FALSE;
#ifdef HAVE_SYS_TIME_H
  device->last_scan.tv_sec = 0;
  device->start_time.tv_sec = 0;
#endif

  /* in case autodection au sensor doesn't work, use the given override */
  device->sensor = rtscfg.sensornumber;

  /* copy configuration settings to device */
  device->conf.modelnumber = dn;
  device->conf.allowsharing = rtscfg.allowsharing;

  DBG (DBG_proc, "attach_rts8891: exit\n");
  return SANE_STATUS_GOOD;
}


/* set initial value for the scanning options */
static SANE_Status
init_options (struct Rts8891_Session *session)
{
  SANE_Int option, count, i, min, idx;
  SANE_Word *dpi_list;
  Rts8891_Model *model = session->dev->model;

  DBG (DBG_proc, "init_options: start\n");

  /* we first initialize each options with a default value */
  memset (session->opt, 0, sizeof (session->opt));
  memset (session->val, 0, sizeof (session->val));

  for (option = 0; option < NUM_OPTIONS; option++)
    {
      session->opt[option].size = sizeof (SANE_Word);
      session->opt[option].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  /* we set up all the options listed in the Rts8891_Option enum */

  /* last option / end of list marker */
  session->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
  session->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  session->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  session->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  session->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  session->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Standard" group: */
  session->opt[OPT_STANDARD_GROUP].title = SANE_TITLE_STANDARD;
  session->opt[OPT_STANDARD_GROUP].name = SANE_NAME_STANDARD;
  session->opt[OPT_STANDARD_GROUP].desc = SANE_DESC_STANDARD;
  session->opt[OPT_STANDARD_GROUP].type = SANE_TYPE_GROUP;
  session->opt[OPT_STANDARD_GROUP].size = 0;
  session->opt[OPT_STANDARD_GROUP].cap = 0;
  session->opt[OPT_STANDARD_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  session->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  session->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  session->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  session->opt[OPT_MODE].type = SANE_TYPE_STRING;
  session->opt[OPT_MODE].cap |= SANE_CAP_AUTOMATIC;
  session->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  session->opt[OPT_MODE].size = max_string_size (mode_list);
  session->opt[OPT_MODE].constraint.string_list = mode_list;
  session->val[OPT_MODE].s = strdup (mode_list[0]);

  /* preview */
  session->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  session->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  session->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  session->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  session->opt[OPT_PREVIEW].cap |= SANE_CAP_AUTOMATIC;
  session->opt[OPT_PREVIEW].unit = SANE_UNIT_NONE;
  session->opt[OPT_PREVIEW].constraint_type = SANE_CONSTRAINT_NONE;
  session->val[OPT_PREVIEW].w = SANE_FALSE;

  /* build resolution list */
  /* TODO we build it from xdpi, one could prefer building it from
   * ydpi list, or even allow for independent X and Y dpi (with 2 options then)
   */
  for (count = 0; model->xdpi_values[count] != 0; count++);
  dpi_list = malloc ((count + 1) * sizeof (SANE_Word));
  if (!dpi_list)
    return SANE_STATUS_NO_MEM;
  dpi_list[0] = count;
  for (count = 0; model->xdpi_values[count] != 0; count++)
    dpi_list[count + 1] = model->xdpi_values[count];

  session->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  session->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  session->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  session->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  session->opt[OPT_RESOLUTION].cap |= SANE_CAP_AUTOMATIC;
  session->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  session->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  session->opt[OPT_RESOLUTION].constraint.word_list = dpi_list;

  /* initial value is lowest available dpi */
  min = 65536;
  for (i = 1; i < count; i++)
    {
      if (dpi_list[i] < min)
	min = dpi_list[i];
    }
  session->val[OPT_RESOLUTION].w = min;

  /* "Geometry" group: */
  session->opt[OPT_GEOMETRY_GROUP].title = SANE_TITLE_GEOMETRY;
  session->opt[OPT_GEOMETRY_GROUP].name = SANE_NAME_GEOMETRY;
  session->opt[OPT_GEOMETRY_GROUP].desc = SANE_DESC_GEOMETRY;
  session->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  session->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  session->opt[OPT_GEOMETRY_GROUP].size = 0;
  session->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* adapt the constraint range to the detected model */
  x_range.max = model->x_size;
  y_range.max = model->y_size;

  /* top-left x */
  session->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  session->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  session->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  session->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  session->opt[OPT_TL_X].cap |= SANE_CAP_AUTOMATIC;
  session->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  session->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  session->opt[OPT_TL_X].constraint.range = &x_range;
  session->val[OPT_TL_X].w = 0;

  /* top-left y */
  session->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  session->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  session->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  session->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  session->opt[OPT_TL_Y].cap |= SANE_CAP_AUTOMATIC;
  session->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  session->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  session->opt[OPT_TL_Y].constraint.range = &y_range;
  session->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  session->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  session->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  session->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  session->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  session->opt[OPT_BR_X].cap |= SANE_CAP_AUTOMATIC;
  session->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  session->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  session->opt[OPT_BR_X].constraint.range = &x_range;
  session->val[OPT_BR_X].w = x_range.max;

  /* bottom-right y */
  session->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  session->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  session->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  session->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  session->opt[OPT_BR_Y].cap |= SANE_CAP_AUTOMATIC;
  session->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  session->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  session->opt[OPT_BR_Y].constraint.range = &y_range;
  session->val[OPT_BR_Y].w = y_range.max;

  /* "Enhancement" group: */
  session->opt[OPT_ENHANCEMENT_GROUP].title = SANE_TITLE_ENHANCEMENT;
  session->opt[OPT_ENHANCEMENT_GROUP].name = SANE_NAME_ENHANCEMENT;
  session->opt[OPT_ENHANCEMENT_GROUP].desc = SANE_DESC_ENHANCEMENT;
  session->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  session->opt[OPT_ENHANCEMENT_GROUP].cap = SANE_CAP_ADVANCED;
  session->opt[OPT_ENHANCEMENT_GROUP].size = 0;
  session->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* BW threshold */
  session->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  session->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  session->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  session->opt[OPT_THRESHOLD].type = SANE_TYPE_FIXED;
  session->opt[OPT_THRESHOLD].unit = SANE_UNIT_PERCENT;
  session->opt[OPT_THRESHOLD].cap |=
    SANE_CAP_INACTIVE | SANE_CAP_AUTOMATIC | SANE_CAP_EMULATED;
  session->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  session->opt[OPT_THRESHOLD].constraint.range = &threshold_percentage_range;
  session->val[OPT_THRESHOLD].w = SANE_FIX (50);

  /* custom-gamma table */
  session->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  session->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  session->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  session->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  session->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_ADVANCED;
  session->val[OPT_CUSTOM_GAMMA].b = SANE_FALSE;

  /* grayscale gamma vector */
  session->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  session->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  session->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  session->opt[OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
  session->opt[OPT_GAMMA_VECTOR].cap |=
    SANE_CAP_INACTIVE | SANE_CAP_AUTOMATIC | SANE_CAP_ADVANCED;
  session->opt[OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
  session->opt[OPT_GAMMA_VECTOR].size = 256 * sizeof (SANE_Word);
  session->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  session->opt[OPT_GAMMA_VECTOR].constraint.range = &u8_range;
  session->val[OPT_GAMMA_VECTOR].wa = model->gamma;

  /* red gamma vector */
  session->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  session->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  session->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  session->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  session->opt[OPT_GAMMA_VECTOR_R].cap |=
    SANE_CAP_INACTIVE | SANE_CAP_AUTOMATIC | SANE_CAP_ADVANCED;
  session->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  session->opt[OPT_GAMMA_VECTOR_R].size = 256 * sizeof (SANE_Word);
  session->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  session->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
  session->val[OPT_GAMMA_VECTOR_R].wa = model->gamma;

  /* green gamma vector */
  session->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  session->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  session->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  session->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  session->opt[OPT_GAMMA_VECTOR_G].cap |=
    SANE_CAP_INACTIVE | SANE_CAP_AUTOMATIC | SANE_CAP_ADVANCED;
  session->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  session->opt[OPT_GAMMA_VECTOR_G].size = 256 * sizeof (SANE_Word);
  session->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  session->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
  session->val[OPT_GAMMA_VECTOR_G].wa = model->gamma;

  /* blue gamma vector */
  session->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  session->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  session->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  session->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  session->opt[OPT_GAMMA_VECTOR_B].cap |=
    SANE_CAP_INACTIVE | SANE_CAP_AUTOMATIC | SANE_CAP_ADVANCED;
  session->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  session->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
  session->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  session->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
  session->val[OPT_GAMMA_VECTOR_B].wa = model->gamma;

  /* "Sensors" group */
  session->opt[OPT_SENSOR_GROUP].title = SANE_TITLE_SENSORS;
  session->opt[OPT_SENSOR_GROUP].name = SANE_NAME_SENSORS;
  session->opt[OPT_SENSOR_GROUP].desc = SANE_DESC_SENSORS;
  session->opt[OPT_SENSOR_GROUP].type = SANE_TYPE_GROUP;
  session->opt[OPT_SENSOR_GROUP].cap = SANE_CAP_ADVANCED;
  session->opt[OPT_SENSOR_GROUP].size = 0;
  session->opt[OPT_SENSOR_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scanner buttons */
  for (i = OPT_BUTTON_1; i <= OPT_BUTTON_11; i++)
    {
      char name[39];
      char title[64];

      idx = i - OPT_BUTTON_1;

      if (idx < model->buttons)
	{
	  sprintf (name, "button-%s", model->button_name[idx]);
	  sprintf (title, "%s", model->button_title[idx]);

	  session->opt[i].name = strdup (name);
	  session->opt[i].title = strdup (title);
	  session->opt[i].desc = SANE_I18N ("This option reflects the status "
					    "of a scanner button.");
	  session->opt[i].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
	}
      else
	{
	  session->opt[i].name = strdup ("unused");
	  session->opt[i].title = strdup ("unused button");
	  session->opt[i].cap |= SANE_CAP_INACTIVE;
	}

      session->opt[i].type = SANE_TYPE_BOOL;
      session->opt[i].unit = SANE_UNIT_NONE;
      session->opt[i].size = sizeof (SANE_Bool);
      session->opt[i].constraint_type = SANE_CONSTRAINT_NONE;
      session->opt[i].constraint.range = 0;
      session->val[i].w = SANE_FALSE;
    }

  update_button_status (session);

  /* "Advanced" group: */
  session->opt[OPT_ADVANCED_GROUP].title = SANE_I18N ("Advanced");
  session->opt[OPT_ADVANCED_GROUP].desc = "";
  session->opt[OPT_ADVANCED_GROUP].type = SANE_TYPE_GROUP;
  session->opt[OPT_ADVANCED_GROUP].cap = SANE_CAP_ADVANCED;
  session->opt[OPT_ADVANCED_GROUP].size = 0;
  session->opt[OPT_ADVANCED_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* lamp on */
  session->opt[OPT_LAMP_ON].name = "lamp-on";
  session->opt[OPT_LAMP_ON].title = SANE_I18N ("Lamp on");
  session->opt[OPT_LAMP_ON].desc = SANE_I18N ("Turn on scanner lamp");
  session->opt[OPT_LAMP_ON].type = SANE_TYPE_BUTTON;
  session->opt[OPT_LAMP_ON].cap |= SANE_CAP_ADVANCED;
  session->opt[OPT_LAMP_ON].unit = SANE_UNIT_NONE;
  session->opt[OPT_LAMP_ON].size = 0;
  session->opt[OPT_LAMP_ON].constraint_type = SANE_CONSTRAINT_NONE;
  session->val[OPT_LAMP_ON].w = 0;

  /* lamp off */
  session->opt[OPT_LAMP_OFF].name = "lamp-off";
  session->opt[OPT_LAMP_OFF].title = SANE_I18N ("Lamp off");
  session->opt[OPT_LAMP_OFF].desc = SANE_I18N ("Turn off scanner lamp");
  session->opt[OPT_LAMP_OFF].type = SANE_TYPE_BUTTON;
  session->opt[OPT_LAMP_OFF].cap |= SANE_CAP_ADVANCED;
  session->opt[OPT_LAMP_OFF].unit = SANE_UNIT_NONE;
  session->opt[OPT_LAMP_OFF].size = 0;
  session->opt[OPT_LAMP_OFF].constraint_type = SANE_CONSTRAINT_NONE;
  session->val[OPT_LAMP_OFF].w = 0;

  DBG (DBG_proc, "init_options: exit\n");
  return SANE_STATUS_GOOD;
}

/**
 * average scanned rgb data, returns the global average. Each channel avearge is also
 * returned. 
 */
static float
average_area (int color, SANE_Byte * data, int width, int height,
	      float *ra, float *ga, float *ba)
{
  int x, y;
  float global, pixels;
  float rc, gc, bc;
  pixels = width * height;
  *ra = 0;
  *ga = 0;
  *ba = 0;
  rc = 0;
  gc = 0;
  bc = 0;
  if (color == SANE_TRUE)
    {
      for (y = 0; y < height; y++)
	{
	  for (x = 0; x < width; x++)
	    {
	      rc += data[3 * width * y + x];
	      gc += data[3 * width * y + x + 1];
	      bc += data[3 * width * y + x + 2];
	    }
	}
      global = (rc + gc + bc) / (3 * pixels);
      *ra = rc / pixels;
      *ga = gc / pixels;
      *ba = bc / pixels;
    }
  else
    {
      for (y = 0; y < height; y++)
	{
	  for (x = 0; x < width; x++)
	    {
	      gc += data[width * y + x];
	    }
	}
      global = gc / pixels;
      *ga = gc / pixels;
    }
  DBG (7, "average_area: global=%.2f, red=%.2f, green=%.2f, blue=%.2f\n",
       global, *ra, *ga, *ba);
  return global;
}


/**
 * Sets lamp brightness (hum, maybe some timing before light off)
 */
static SANE_Status
set_lamp_brightness (struct Rts8891_Device *dev, int level)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte reg;

  reg = 0xA0 | (level & 0x0F);
  sanei_rts88xx_write_reg (dev->devnum, LAMP_BRIGHT_REG, &reg);
  switch (level)
    {
    case 0:
      reg = 0x8d;
      break;
    case 7:
      reg = 0x82;
      break;
    default:
      reg = 0x8d;
      break;
    }
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &reg);
  reg = (reg & 0xF0) | 0x20 | ((~reg) & 0x0F);
  dev->regs[LAMP_REG] = reg;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &reg);
  sanei_rts88xx_write_control (dev->devnum, 0x00);
  sanei_rts88xx_write_control (dev->devnum, 0x00);
  sanei_rts88xx_get_status (dev->devnum, dev->regs);
  DBG (DBG_io, "set_lamp_brightness: status=0x%02x 0x%02x\n", dev->regs[0x10],
       dev->regs[0x11]);
  if (dev->sensor != SENSOR_TYPE_4400)
    {
      dev->regs[0x10] = 0x28;
      dev->regs[0x11] = 0x3f;
    }
  else
    {
      dev->regs[0x10] = 0x10;
      dev->regs[0x11] = 0x2f;
    }
  reg = dev->regs[LAMP_REG];
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &reg);
  status = sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &reg);
  if (reg != 0x00)
    {
      DBG (DBG_warn,
	   "set_lamp_brightness: unexpected CONTROL_REG value, 0x%02x instead of 0x00\n",
	   reg);
    }
  return status;
}


/**
 * Sets the lamp to half brightness and standard parameters
 */
static SANE_Status
init_lamp (struct Rts8891_Device *dev)
{
  SANE_Byte reg;

  sanei_rts88xx_write_control (dev->devnum, 0x01);
  sanei_rts88xx_write_control (dev->devnum, 0x01);
  sanei_rts88xx_write_control (dev->devnum, 0x00);
  sanei_rts88xx_write_control (dev->devnum, 0x00);
  sanei_rts88xx_cancel (dev->devnum);
  dev->regs[0x12] = 0xff;
  dev->regs[0x13] = 0x20;
  sanei_rts88xx_write_regs (dev->devnum, 0x12, dev->regs + 0x12, 2);
  /* 0xF8/0x28 expected */
  sanei_rts88xx_write_regs (dev->devnum, 0x14, dev->regs + 0x14, 2);
  sanei_rts88xx_write_control (dev->devnum, 0x00);
  sanei_rts88xx_write_control (dev->devnum, 0x00);
  if (dev->sensor != SENSOR_TYPE_4400 && dev->sensor != SENSOR_TYPE_4400_BARE)
    {
      sanei_rts88xx_set_status (dev->devnum, dev->regs, 0x28, 0x3f);
      dev->regs[0x11] = 0x3f;
    }
  else
    {
      sanei_rts88xx_set_status (dev->devnum, dev->regs, 0x10, 0x22);
      dev->regs[0x11] = 0x22;
    }
  reg = 0x8d;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &reg);
  dev->regs[LAMP_REG] = 0xa2;
  dev->regs[LAMP_BRIGHT_REG] = 0xa0;
  rts8891_write_all (dev->devnum, dev->regs, dev->reg_count);
  return set_lamp_brightness (dev, 7);
}

/**
 * This function detects physical start of scanning area find finding a black area below the "roof"
 * if during scan we detect that sensor is inadequate, changed is set to SANE_TRUE
 */
static SANE_Status
find_origin (struct Rts8891_Device *dev, SANE_Bool * changed)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte control, reg;
  unsigned int max = 0;
  unsigned char *image = NULL;
  unsigned char *data = NULL;
  SANE_Word total;
  int startx = 300;
  int width = 1200;
  int x, y, sum, current;
  int starty = 18;
  int height = 180;
  int timing;

  DBG (DBG_proc, "find_origin: start\n");

  /* check if head is at home 
   * once sensor is correctly set up, we are allways park here,
   * but in case sensor has just changed, we are not so we park head */
  sanei_rts88xx_read_reg (dev->devnum, CONTROLER_REG, &reg);
  if ((reg & 0x02) == 0)
    {
      if (park_head (dev, SANE_TRUE) != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "find_origin: failed to park head!\n");
	  return SANE_STATUS_IO_ERROR;
	}
      DBG (DBG_info, "find_origin: head parked\n");
    }

  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
  if (control != 0)
    {
      DBG (DBG_warn, "find_origin: unexpected control value 0x%02x\n",
	   control);
    }
  sanei_rts88xx_reset_lamp (dev->devnum, dev->regs);
  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
  if (control != 0)
    {
      DBG (DBG_warn, "find_origin: unexpected control value 0x%02x\n",
	   control);
    }
  sanei_rts88xx_reset_lamp (dev->devnum, dev->regs);

  /* set lamp to standard settings */
  init_lamp (dev);

  /* set scan parameters */
  dev->regs[0x00] = 0xe5;
  dev->regs[0x32] = 0x80;
  dev->regs[0x33] = 0x81;
  dev->regs[0x39] = 0x02;
  dev->regs[0x64] = 0x01;
  dev->regs[0x65] = 0x20;
  dev->regs[0x79] = 0x20;
  dev->regs[0x7a] = 0x01;
  dev->regs[0x90] = 0x1c;

  dev->regs[0x35] = 0x1b;
  dev->regs[0x36] = 0x29;
  dev->regs[0x3a] = 0x1b;
  timing=0x32;
  dev->regs[0x85] = 0x00;
  dev->regs[0x86] = 0x06;
  dev->regs[0x87] = 0x00;
  dev->regs[0x88] = 0x06;
  dev->regs[0x8d] = 0x80;
  dev->regs[0x8e] = 0x68;

  dev->regs[0xb2] = 0x02;	/* 0x04 -> no data */

  dev->regs[0xc0] = 0xff;
  dev->regs[0xc1] = 0x0f;
  dev->regs[0xc3] = 0xff;
  dev->regs[0xc4] = 0xff;
  dev->regs[0xc5] = 0xff;
  dev->regs[0xc6] = 0xff;
  dev->regs[0xc7] = 0xff;
  dev->regs[0xc8] = 0xff;
  dev->regs[0xc9] = 0x00;
  dev->regs[0xca] = 0x0e;
  dev->regs[0xcb] = 0x00;
  dev->regs[0xcd] = 0xf0;
  dev->regs[0xce] = 0xff;
  dev->regs[0xcf] = 0xf5;
  dev->regs[0xd0] = 0xf7;
  dev->regs[0xd1] = 0xea;
  dev->regs[0xd2] = 0x0b;
  dev->regs[0xd3] = 0x03;
  dev->regs[0xd4] = 0x05;
  dev->regs[0xd6] = 0xab;
  dev->regs[0xd7] = 0x30;
  dev->regs[0xd8] = 0xf6;
  dev->regs[0xe2] = 0x01;
  dev->regs[0xe3] = 0x00;
  dev->regs[0xe4] = 0x00;
  SET_DOUBLE (dev->regs, EXPOSURE_REG, 4124);
  /* dev->regs[0xe5] = 0x1c;
     dev->regs[0xe6] = 0x10;     101c=4124 */
  dev->regs[0xe7] = 0x00;
  dev->regs[0xe8] = 0x00;
  dev->regs[0xe9] = 0x00;

  if (dev->sensor == SENSOR_TYPE_XPA || dev->sensor == SENSOR_TYPE_4400)
    {
      dev->regs[0xc3] = 0x00;
      dev->regs[0xc4] = 0xf0;
      dev->regs[0xc7] = 0x0f;
      dev->regs[0xc8] = 0x00;
      dev->regs[0xc9] = 0xff;
      dev->regs[0xca] = 0xf1;
      dev->regs[0xcb] = 0xff;
      dev->regs[0xd7] = 0x10;
    }
  if (dev->sensor == SENSOR_TYPE_4400 || dev->sensor == SENSOR_TYPE_4400_BARE)
    {
      /* 4400 values / 'XPA' values */
      dev->regs[0x13] = 0x39;	/* 0x20 */
      dev->regs[0x14] = 0xf0;	/* 0xf8 */
      dev->regs[0x15] = 0x29;	/* 0x28 */
      dev->regs[0x16] = 0x0f;	/* 0x07 */
      dev->regs[0x17] = 0x10;	/* 0x00 */

      dev->regs[0x23] = 0x00;	/* 0xff */

      dev->regs[0x39] = 0x00;	/* 0x02 */
    }
  if (dev->sensor == SENSOR_TYPE_4400_BARE)
    {
      dev->regs[0xc3] = 0xff;	/* 0x00 */
      dev->regs[0xc4] = 0xff;	/* 0xf0 */
      dev->regs[0xc7] = 0xff;	/* 0x0f */
      dev->regs[0xc8] = 0xff;	/* 0x00 */
      dev->regs[0xc9] = 0x00;	/* 0xff */
      dev->regs[0xca] = 0x0e;	/* 0xf1 */
      dev->regs[0xcb] = 0x00;	/* 0xff */
      dev->regs[0xd7] = 0x30;	/* 0x10 */
      dev->regs[0xda] = 0xa7;	/* 0xa0 */
    }
  SET_DOUBLE (dev->regs, TIMING_REG, timing);
  SET_DOUBLE (dev->regs, TIMING1_REG, timing+1);
  SET_DOUBLE (dev->regs, TIMING2_REG, timing+2);


  /* allocate memory for the data */
  total = width * height;
  data = (unsigned char *) malloc (total);
  if (data == NULL)
    {
      DBG (DBG_error, "find_origin: failed to allocate %d bytes\n", total);
      return SANE_STATUS_NO_MEM;
    }
  image = (unsigned char *) malloc (total);
  if (image == NULL)
    {
      free(data);
      DBG (DBG_error, "find_origin: failed to allocate %d bytes\n", total);
      return SANE_STATUS_NO_MEM;
    }

  /* set vertical and horizontal start/end positions */
  sanei_rts88xx_set_scan_area (dev->regs, starty, starty + height, startx,
			       startx + width);
  sanei_rts88xx_set_offset (dev->regs, 0x7f, 0x7f, 0x7f);
  sanei_rts88xx_set_gain (dev->regs, 0x10, 0x10, 0x10);

  /* gray level scan */
  dev->regs[LAMP_REG] = 0xad;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, dev->regs + LAMP_REG);
  if (dev->sensor != SENSOR_TYPE_4400
      && (dev->sensor != SENSOR_TYPE_4400_BARE))
    {
      sanei_rts88xx_set_status (dev->devnum, dev->regs, 0x20, 0x3b);
    }
  else
    {
      sanei_rts88xx_set_status (dev->devnum, dev->regs, 0x10, 0x22);
    }

  status =
    rts8891_simple_scan (dev->devnum, dev->regs, dev->reg_count, 0x03,
			 total, data);
  if (status != SANE_STATUS_GOOD)
    {
      free(image);
      free(data);
      DBG (DBG_error, "find_origin: failed to scan\n");
      return status;
    }

  if (DBG_LEVEL > DBG_io2)
    {
      write_gray_data (data, "find_origin.pnm", width, height);
    }

  /* now we have the data, search for the black area so that we can      */
  /* deduce start of scan area                                           */
  /* we apply an Y direction sobel filter to get reliable edge detection */
  /* Meanwhile we check that picture is correct, if not, sensor needs to */
  /* be changed .                                                        */
  /* TODO possibly check for insufficient warming                        */
  for (y = 1; y < height - 1; y++)
    for (x = 1; x < width - 1; x++)
      {
	/* if sensor is wrong, picture is black, so max will be low */
	if (data[y * width + x] > max)
	  max = data[y * width + x];

	/* sobel y filter */
	current =
	  -data[(y - 1) * width + x + 1] - 2 * data[(y - 1) * width + x] -
	  data[(y - 1) * width + x - 1] + data[(y + 1) * width + x + 1] +
	  2 * data[(y + 1) * width + x] + data[(y + 1) * width + x - 1];
	if (current < 0)
	  current = -current;
	image[y * width + x] = current;
      }
  if (DBG_LEVEL > DBG_io2)
    {
      write_gray_data (image, "find_origin_ysobel.pnm", width, height);
    }

  /* sensor test */
  if (max < 10)
    {
      DBG (DBG_info,
	   "find_origin: sensor type needs to be changed (max=%d)\n", max);
      *changed = SANE_TRUE;
    }
  else
    {
      *changed = SANE_FALSE;
    }

  /* the edge will be where the white area stops                     */
  /* we average y position of white->black transition on each column */
  sum = 0;
  for (x = 1; x < width - 1; x++)
    {
      for (y = 1; y < height - 2; y++)
	{
	  /* egde detection on each line */
	  if (image[x + (y + 1) * width] - image[x + y * width] >= 20)
	    {
	      sum += y;
	      break;
	    }
	}
    }
  sum /= width;
  DBG (DBG_info, "find_origin: scan area offset=%d lines\n", sum);

  /* convert the detected value into max ydpi */
  dev->top_offset = (48 * dev->model->max_ydpi) / 300;

  /* no we're done */
  free (image);
  free (data);

  /* safe guard test against moving to far toward the top */
  if (sum > 11)
    {
      /* now go back to the white area so that calibration can work on it */
      dev->regs[0x35] = 0x0e;
      dev->regs[0x3a] = 0x0e;

      dev->regs[0xb2] = 0x06;	/* no data (0x04) */

      if (dev->sensor == SENSOR_TYPE_4400)
	{
	  dev->regs[0x36] = 0x21;	/* direction reverse (& ~0x08) */

	  dev->regs[0xe2] = 0x03;	/* 0x01 */

	  /* dev->regs[0xe5] = 0x0d;     0x1c 
	     dev->regs[0xe6] = 0x08;     0x10 080d=2061=1030*2+1 */
	  SET_DOUBLE (dev->regs, EXPOSURE_REG, 2061);
	}
      else
	{
	  dev->regs[0x11] = 0x3f;	/* 0x3b */

	  dev->regs[0x36] = 0x24;	/* direction reverse (& ~0x08) */

	  dev->regs[0xe2] = 0x07;

	  /*
	     dev->regs[0xe5] = 0x06;
	     dev->regs[0xe6] = 0x04;     406=1030 */
	  SET_DOUBLE (dev->regs, EXPOSURE_REG, 1030);
	}

      /* move by a fixed amount relative to the 'top' of the scanner */
      sanei_rts88xx_set_scan_area (dev->regs, height - sum + 10,
				   height - sum + 11, 637, 893);
      rts8891_write_all (dev->devnum, dev->regs, dev->reg_count);
      rts8891_commit (dev->devnum, 0x03);

      /* wait for the motor to stop */
      do
	{
	  status = sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &reg);
	}
      while ((reg & 0x08) == 0x08);
    }

  DBG (DBG_proc, "find_origin: exit\n");
  return status;
}

/**
 * This function detects the left margin (ie x offset of scanning area) by doing
 * a grey scan of the very beginning of the sensor and finding the start of the
 * white area of calibration zone, which is start of usable pixels. 
 */
static SANE_Status
find_margin (struct Rts8891_Device *dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  unsigned char *data = NULL;
  SANE_Word total;
  int startx = 33;
  int width = 250;
  int x;
  int starty = 1;
  int height = 1;
  SANE_Byte reg = 0xa2;
  int timing=0;

  DBG (DBG_proc, "find_margin: start\n");

  /* increase brightness to have better margin detection */
  sanei_rts88xx_write_reg (dev->devnum, LAMP_BRIGHT_REG, &reg);

  /* maximum gain, offsets untouched */
  if (dev->sensor == SENSOR_TYPE_4400 || dev->sensor == SENSOR_TYPE_4400_BARE)
    {
      sanei_rts88xx_set_status (dev->devnum, dev->regs, 0x10, 0x23);
    }
  else
    {
      sanei_rts88xx_set_status (dev->devnum, dev->regs, 0x28, 0x3b);
    }

  sanei_rts88xx_set_gain (dev->regs, 0x3f, 0x3f, 0x3f);

  /* set scan parameters */
  dev->regs[0x33] = 0x01;
  dev->regs[0x34] = 0x10;
  dev->regs[0x35] = 0x1b;
  dev->regs[0x3a] = 0x1b;

  dev->regs[0x72] = 0x3a;
  dev->regs[0x73] = 0x15;
  dev->regs[0x74] = 0x62;

  timing=0;

  dev->regs[0xc0] = 0xff;
  dev->regs[0xc1] = 0xff;
  dev->regs[0xc2] = 0xff;
  dev->regs[0xc3] = 0x00;
  dev->regs[0xc4] = 0xf0;
  dev->regs[0xc7] = 0x0f;
  dev->regs[0xc8] = 0x00;
  dev->regs[0xcb] = 0xe0;
  dev->regs[0xcc] = 0xff;
  dev->regs[0xcd] = 0xff;
  dev->regs[0xce] = 0xff;
  dev->regs[0xcf] = 0xe9;
  dev->regs[0xd0] = 0xeb;
  dev->regs[0xd3] = 0x0c;
  dev->regs[0xd4] = 0x0e;
  dev->regs[0xd6] = 0xab;
  dev->regs[0xd7] = 0x14;
  dev->regs[0xd8] = 0xf6;

  dev->regs[0xda] = 0xa7;	/* XXX STEF XXX à l'origine, pas 'bare' */

  dev->regs[0xe2] = 0x01;

  /* dev->regs[0xe5] = 0x7b;
     dev->regs[0xe6] = 0x15;     157b=5499 */
  SET_DOUBLE (dev->regs, EXPOSURE_REG, 5499);

  dev->regs[0xe7] = 0x00;
  dev->regs[0xe8] = 0x00;
  dev->regs[0xe9] = 0x00;
  dev->regs[0xea] = 0x00;
  dev->regs[0xeb] = 0x00;
  dev->regs[0xec] = 0x00;
  dev->regs[0xed] = 0x00;
  dev->regs[0xef] = 0x00;
  dev->regs[0xf0] = 0x00;
  dev->regs[0xf2] = 0x00;
  if (dev->sensor == SENSOR_TYPE_XPA || dev->sensor == SENSOR_TYPE_4400)
    {
      dev->regs[0xc0] = 0x00;
      dev->regs[0xc1] = 0xf8;
      dev->regs[0xc2] = 0x7f;
      dev->regs[0xc4] = 0xf8;
      dev->regs[0xc5] = 0x7f;
      dev->regs[0xc6] = 0x00;
      dev->regs[0xc7] = 0xf8;
      dev->regs[0xc8] = 0x7f;
      dev->regs[0xc9] = 0xff;
      dev->regs[0xca] = 0xff;
      dev->regs[0xcb] = 0x8f;
      dev->regs[0xcd] = 0x07;
      dev->regs[0xce] = 0x80;
      dev->regs[0xcf] = 0xea;
      dev->regs[0xd0] = 0xec;
      dev->regs[0xd1] = 0xf7;
      dev->regs[0xd2] = 0x00;
      dev->regs[0xd3] = 0x10;
      dev->regs[0xd4] = 0x12;
      dev->regs[0xd7] = 0x31;
    }
  if (dev->sensor == SENSOR_TYPE_4400_BARE)
    {
      dev->regs[0x13] = 0x39;	/* 0x20 */
      dev->regs[0x14] = 0xf0;	/* 0xf8 */
      dev->regs[0x15] = 0x29;	/* 0x28 */
      dev->regs[0x16] = 0x0f;	/* 0x07 */
      dev->regs[0x17] = 0x10;	/* 0x00 */
      dev->regs[0x23] = 0x00;	/* 0xff */
      dev->regs[0x39] = 0x00;	/* 0x02 */
      dev->regs[0x85] = 0x46;	/* 0x00 */
      dev->regs[0x86] = 0x0b;	/* 0x06 */
      dev->regs[0x87] = 0x8c;	/* 0x00 */
      dev->regs[0x88] = 0x10;	/* 0x06 */
      dev->regs[0x8d] = 0x3b;	/* 0x00 */
      timing=0x00b0;
    }
  SET_DOUBLE (dev->regs, TIMING_REG, timing);
  SET_DOUBLE (dev->regs, TIMING1_REG, timing+1);
  SET_DOUBLE (dev->regs, TIMING2_REG, timing+2);

  /* set vertical and horizontal start/end positions */
  sanei_rts88xx_set_scan_area (dev->regs, starty, starty + height, startx,
			       startx + width);

  /* allocate memory for the data */
  total = width * height;
  data = (unsigned char *) malloc (total);
  if (data == NULL)
    {
      DBG (DBG_error, "find_margin: failed to allocate %d bytes\n", total);
      return SANE_STATUS_NO_MEM;
    }

  /* gray level scan */
  status =
    rts8891_simple_scan (dev->devnum, dev->regs, dev->reg_count, 0x0c,
			 total, data);
  if (status != SANE_STATUS_GOOD)
    {
      free(data);
      DBG (DBG_error, "find_margin: failed to scan\n");
      return status;
    }

  if (DBG_LEVEL > DBG_io2)
    {
      write_gray_data (data, "find_margin.pnm", width, height);
    }

  /* we search from left to right the first white pixel */
  x = 0;
  while (x < width && data[x] < MARGIN_LEVEL)
    x++;
  if (x == width)
    {
      DBG (DBG_warn, "find_margin: failed to find left margin!\n");
      DBG (DBG_warn, "find_margin: using default...\n");
      x = 48 + 40;
    }
  DBG (DBG_info, "find_margin: scan area margin=%d pixels\n", x);

  /* convert the detected value into max ydpi */
  dev->left_offset = ((x - 40) * dev->model->max_xdpi) / 150;
  DBG (DBG_info, "find_margin: left_offset=%d pixels\n", x);

  /* no we're done */
  free (data);

  DBG (DBG_proc, "find_margin: exit\n");
  return status;
}

#ifdef FAST_INIT
/*
 * This function intializes the device:
 * 	- initial registers values
 * 	- test if at home
 * 	- head parking if needed
 */
static SANE_Status
initialize_device (struct Rts8891_Device *dev)
{
  SANE_Status status;
  SANE_Byte reg, control;

  DBG (DBG_proc, "initialize_device: start\n");
  if (dev->initialized == SANE_TRUE)
    return SANE_STATUS_GOOD;

  sanei_rts88xx_set_status (dev->devnum, dev->regs, 0x20, 0x28);
  sanei_rts88xx_write_control (dev->devnum, 0x01);
  sanei_rts88xx_write_control (dev->devnum, 0x01);
  sanei_rts88xx_write_control (dev->devnum, 0x00);
  sanei_rts88xx_write_control (dev->devnum, 0x00);

  sanei_rts88xx_read_reg (dev->devnum, 0xb0, &control);
  DBG (DBG_io, "initialize_device: reg[0xb0]=0x%02x\n", control);

  /* we expect to get 0x80 */
  if (control != 0x80)
    {
      DBG (DBG_warn,
	   "initialize_device: expected reg[0xb0]=0x80, got 0x%02x\n",
	   control);
      /* TODO fail here ??? */
    }

  /* get lamp status */
  status = sanei_rts88xx_get_lamp_status (dev->devnum, dev->regs);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "initialize_device: failed to read lamp status!\n");
      return SANE_STATUS_IO_ERROR;
    }
  DBG (DBG_io, "initialize_device: lamp status=0x%02x\n", dev->regs[0x8e]);

  /* sensor type the one for 4470c sold with XPA is slightly different 
   * than those sold bare, for this model we allways start with xpa type sensor,
   * and change it later if we detect black scans in find_origin(). In case the
   * attach function set up the sensor type, we don't modify it */
  if (dev->sensor == -1)
    {
      dev->sensor = device->model->sensor;
    }
  DBG (DBG_info, "initialize_device: initial sensor type is %d\n", dev->sensor);
  DBG (DBG_info, "initialize_device: reg[8e]=0x%02x\n", dev->regs[0x8e]);

  /* detects if warming up is needed */
  if ((dev->regs[0x8e] & 0x60) != 0x60)
    {
      DBG (DBG_info, "initialize_device: lamp needs warming\n");
      dev->needs_warming = SANE_TRUE;
    }
  else
    {
      dev->needs_warming = SANE_FALSE;
    }

  sanei_rts88xx_read_reg (dev->devnum, LAMP_REG, &reg);

  sanei_rts88xx_write_control (dev->devnum, 0x00);
  sanei_rts88xx_write_control (dev->devnum, 0x00);

  /* read scanner present register */
  sanei_rts88xx_read_reg (dev->devnum, LINK_REG, &control);
  if (control != 0x00 && control != 0x01)
    {
      DBG (DBG_warn,
	   "initialize_device: unexpected LINK_REG=0x%02x\n", control);
    }

  /* head parking if needed */
  sanei_rts88xx_read_reg (dev->devnum, CONTROLER_REG, &control);
  if (!(control & 0x02))
    {
      if (park_head (dev, SANE_TRUE) != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "initialize_device: failed to park head!\n");
	  return SANE_STATUS_IO_ERROR;
	}
    }

  /* writes initial register set */
  dev->regs[0x00] = 0xe5;	/* 0xf5 */
  dev->regs[0x02] = 0x1f;	/* 0x00 */
  dev->regs[0x03] = 0x1f;	/* 0x00 */
  dev->regs[0x04] = 0x1f;	/* 0x00 */
  dev->regs[0x05] = 0x1f;	/* 0x00 */
  dev->regs[0x06] = 0x1f;	/* 0x00 */
  dev->regs[0x07] = 0x1f;	/* 0x00 */
  dev->regs[0x08] = 0x0a;	/* 0x00 */
  dev->regs[0x09] = 0x0a;	/* 0x00 */
  dev->regs[0x0a] = 0x0a;	/* 0x00 */
  dev->regs[0x10] = 0x28;	/* 0x60 */
  dev->regs[0x11] = 0x28;	/* 0x1b */
  dev->regs[0x13] = 0x20;	/* 0x3e */
  dev->regs[0x14] = 0xf8;	/* 0x00 */
  dev->regs[0x15] = 0x28;	/* 0x00 */
  dev->regs[0x16] = 0x07;	/* 0xff */
  dev->regs[0x17] = 0x00;	/* 0x3e */
  dev->regs[0x18] = 0xff;	/* 0x00 */
  dev->regs[0x1d] = 0x20;	/* 0x22 */

  /* LCD display */
  dev->regs[0x20] = 0x3a;	/* 0x00 */
  dev->regs[0x21] = 0xf2;	/* 0x00 */
  dev->regs[0x24] = 0xff;	/* 0x00 */
  dev->regs[0x25] = 0x00;	/* 0xff */

  dev->regs[0x30] = 0x00;	/* 0x01 */
  dev->regs[0x31] = 0x00;	/* 0x42 */
  dev->regs[0x36] = 0x07;	/* 0x00 */
  dev->regs[0x39] = 0x00;	/* 0x02 */
  dev->regs[0x40] = 0x20;	/* 0x80 */
  dev->regs[0x44] = 0x8c;	/* 0x18 */
  dev->regs[0x45] = 0x76;	/* 0x00 */
  dev->regs[0x50] = 0x00;	/* 0x20 */
  dev->regs[0x51] = 0x00;	/* 0x24 */
  dev->regs[0x52] = 0x00;	/* 0x04 */
  dev->regs[0x64] = 0x00;	/* 0x10 */
  dev->regs[0x68] = 0x00;	/* 0x10 */
  dev->regs[0x6a] = 0x00;	/* 0x01 */
  dev->regs[0x70] = 0x00;	/* 0x20 */
  dev->regs[0x71] = 0x00;	/* 0x20 */
  dev->regs[0x72] = 0xe1;	/* 0x00 */
  dev->regs[0x73] = 0x14;	/* 0x00 */
  dev->regs[0x74] = 0x18;	/* 0x00 */
  dev->regs[0x75] = 0x15;	/* 0x00 */
  dev->regs[0x76] = 0x00;	/* 0x20 */
  dev->regs[0x77] = 0x00;	/* 0x01 */
  dev->regs[0x79] = 0x00;	/* 0x02 */
  dev->regs[0x81] = 0x00;	/* 0x41 */
  dev->regs[0x83] = 0x00;	/* 0x10 */
  dev->regs[0x84] = 0x00;	/* 0x21 */
  dev->regs[0x85] = 0x00;	/* 0x20 */
  dev->regs[0x87] = 0x00;	/* 0x20 */
  dev->regs[0x88] = 0x00;	/* 0x81 */
  dev->regs[0x89] = 0x00;	/* 0x20 */
  dev->regs[0x8a] = 0x00;	/* 0x01 */
  dev->regs[0x8b] = 0x00;	/* 0x01 */
  dev->regs[0x8d] = 0x80;	/* 0x22 */
  dev->regs[0x8e] = 0x68;	/* 0x00 */
  dev->regs[0x8f] = 0x00;	/* 0x40 */
  dev->regs[0x90] = 0x00;	/* 0x05 */
  dev->regs[0x93] = 0x02;	/* 0x01 */
  dev->regs[0x94] = 0x0e;	/* 0x1e */
  dev->regs[0xb0] = 0x00;	/* 0x80 */
  dev->regs[0xb2] = 0x02;	/* 0x06 */
  dev->regs[0xc0] = 0xff;	/* 0x00 */
  dev->regs[0xc1] = 0x0f;	/* 0x00 */
  dev->regs[0xc3] = 0xff;	/* 0x00 */
  dev->regs[0xc4] = 0xff;	/* 0x00 */
  dev->regs[0xc5] = 0xff;	/* 0x00 */
  dev->regs[0xc6] = 0xff;	/* 0x00 */
  dev->regs[0xc7] = 0xff;	/* 0x00 */
  dev->regs[0xc8] = 0xff;	/* 0x00 */
  dev->regs[0xca] = 0x0e;	/* 0x00 */
  dev->regs[0xcd] = 0xf0;	/* 0x00 */
  dev->regs[0xce] = 0xff;	/* 0x00 */
  dev->regs[0xcf] = 0xf5;	/* 0x00 */
  dev->regs[0xd0] = 0xf7;	/* 0x00 */
  dev->regs[0xd1] = 0xea;	/* 0x00 */
  dev->regs[0xd2] = 0x0b;	/* 0x00 */
  dev->regs[0xd3] = 0x03;	/* 0x00 */
  dev->regs[0xd4] = 0x05;	/* 0x01 */
  dev->regs[0xd5] = 0x86;	/* 0x06 */
  dev->regs[0xd7] = 0x30;	/* 0x10 */
  dev->regs[0xd8] = 0xf6;	/* 0x7a */
  dev->regs[0xd9] = 0x80;	/* 0x00 */
  dev->regs[0xda] = 0x00;	/* 0x15 */
  dev->regs[0xe2] = 0x01;	/* 0x00 */
  /* dev->regs[0xe5] = 0x14;     0x0f */
  SET_DOUBLE (dev->regs, EXPOSURE_REG, 20);

  status = rts8891_write_all (dev->devnum, dev->regs, dev->reg_count);

  DBG (DBG_proc, "initialize_device: exit\n");
  dev->initialized = SANE_TRUE;

  return status;
}
#else /* FAST_INIT */

#ifndef UNIT_TESTING
static
#endif
SANE_Status
init_registers (struct Rts8891_Device *dev)
{
int i;

  /* initial set written to scanner
   * 0xe5 0x41 0x1f 0x1f 0x1f 0x1f 0x1f 0x1f 0x0a 0x0a 0x0a 0x70 0x00 0x00 0x00 0x00 0x60 0x1b 0x08 0x20 0x00 0x20 0x08 0x00 0x00 0x00 0x00 0x00 0x00 0x20 0x00 0x00 0x3a 0xf2 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x10 0x00 0x07 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x20 0x00 0x00 0x00 0x8c 0x76 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x80 0x68 0x00 0x00 0x00 0x00 0x02 0x0e 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xcc 0x27 0x64 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x02 ---- 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xe0 0x00 0x00 0x00 0x00 0x86 0x1b 0x00 0xff 0x00 0x27 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x01 0x00 0x00 0x14 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
   */
  /* lengthy set up to make register content obvious, may be some day I will
   * group corresponding values between models */
  switch (dev->sensor)
    {
    case SENSOR_TYPE_BARE:
    case SENSOR_TYPE_XPA:
      dev->regs[0x01] = 0x41;
      dev->regs[0x0b] = 0x70;
      dev->regs[0x0c] = 0x00;
      dev->regs[0x0d] = 0x00;
      dev->regs[0x0e] = 0x00;
      dev->regs[0x0f] = 0x00;

      dev->regs[0x12] = 0x00;
      dev->regs[0x13] = 0x20;
      dev->regs[0x14] = 0x00;
      dev->regs[0x15] = 0x20;
      dev->regs[0x16] = 0x00;
      dev->regs[0x17] = 0x00;
      dev->regs[0x18] = 0x00;
      dev->regs[0x19] = 0x00;
      dev->regs[0x1a] = 0x00;
      dev->regs[0x1b] = 0x00;
      dev->regs[0x1c] = 0x00;
      dev->regs[0x1d] = 0x20;
      dev->regs[0x1e] = 0x00;
      dev->regs[0x1f] = 0x00;

      /* LCD display */
      dev->regs[0x20] = 0x3a;
      dev->regs[0x21] = 0xf2;
      dev->regs[0x22] = 0x00;

      dev->regs[0x23] = 0x00;
      dev->regs[0x24] = 0x00;
      dev->regs[0x25] = 0x00;
      dev->regs[0x26] = 0x00;
      dev->regs[0x27] = 0x00;
      dev->regs[0x28] = 0x00;
      dev->regs[0x29] = 0x00;
      dev->regs[0x2a] = 0x00;
      dev->regs[0x2b] = 0x00;
      dev->regs[0x2c] = 0x00;
      dev->regs[0x2d] = 0x00;
      dev->regs[0x2e] = 0x00;
      dev->regs[0x2f] = 0x00;
      dev->regs[0x30] = 0x00;
      dev->regs[0x31] = 0x00;
      dev->regs[0x32] = 0x00;
      dev->regs[0x33] = 0x00;
      dev->regs[0x34] = 0x10;
      dev->regs[0x35] = 0x00;
      dev->regs[0x36] = 0x07;
      dev->regs[0x37] = 0x00;
      dev->regs[0x38] = 0x00;
      dev->regs[0x39] = 0x00;
      dev->regs[0x3a] = 0x00;
      dev->regs[0x3b] = 0x00;
      dev->regs[0x3c] = 0x00;
      dev->regs[0x3d] = 0x00;
      dev->regs[0x3e] = 0x00;
      dev->regs[0x3f] = 0x00;
      dev->regs[0x40] = 0x20;
      dev->regs[0x41] = 0x00;
      dev->regs[0x42] = 0x00;
      dev->regs[0x43] = 0x00;
      dev->regs[0x44] = 0x8c;
      dev->regs[0x45] = 0x76;
      dev->regs[0x46] = 0x00;
      dev->regs[0x47] = 0x00;
      dev->regs[0x48] = 0x00;
      dev->regs[0x49] = 0x00;
      dev->regs[0x4a] = 0x00;
      dev->regs[0x4b] = 0x00;
      dev->regs[0x4c] = 0x00;
      dev->regs[0x4d] = 0x00;
      dev->regs[0x4e] = 0x00;
      dev->regs[0x4f] = 0x00;
      dev->regs[0x50] = 0x00;
      dev->regs[0x51] = 0x00;
      dev->regs[0x52] = 0x00;
      dev->regs[0x53] = 0x00;
      dev->regs[0x54] = 0x00;
      dev->regs[0x55] = 0x00;
      dev->regs[0x56] = 0x00;
      dev->regs[0x57] = 0x00;
      dev->regs[0x58] = 0x00;
      dev->regs[0x59] = 0x00;
      dev->regs[0x5a] = 0x00;
      dev->regs[0x5b] = 0x00;
      dev->regs[0x5c] = 0x00;
      dev->regs[0x5d] = 0x00;
      dev->regs[0x5e] = 0x00;
      dev->regs[0x5f] = 0x00;
      dev->regs[0x64] = 0x00;
      dev->regs[0x65] = 0x00;
      dev->regs[0x66] = 0x00;
      dev->regs[0x67] = 0x00;
      dev->regs[0x68] = 0x00;
      dev->regs[0x69] = 0x00;
      dev->regs[0x6a] = 0x00;
      dev->regs[0x6b] = 0x00;
      dev->regs[0x6e] = 0x00;
      dev->regs[0x6f] = 0x00;
      dev->regs[0x70] = 0x00;
      dev->regs[0x71] = 0x00;
      dev->regs[0x72] = 0x00;
      dev->regs[0x73] = 0x00;
      dev->regs[0x74] = 0x00;
      dev->regs[0x75] = 0x00;
      dev->regs[0x76] = 0x00;
      dev->regs[0x77] = 0x00;
      dev->regs[0x78] = 0x00;
      dev->regs[0x79] = 0x00;
      dev->regs[0x7a] = 0x00;
      dev->regs[0x7b] = 0x00;
      dev->regs[0x7c] = 0x00;
      dev->regs[0x7d] = 0x00;
      dev->regs[0x7e] = 0x00;
      dev->regs[0x7f] = 0x00;
      dev->regs[0x80] = 0x00;
      dev->regs[0x81] = 0x00;
      dev->regs[0x82] = 0x00;
      dev->regs[0x83] = 0x00;
      dev->regs[0x84] = 0x00;
      dev->regs[0x85] = 0x00;
      dev->regs[0x86] = 0x00;
      dev->regs[0x87] = 0x00;
      dev->regs[0x88] = 0x00;
      dev->regs[0x89] = 0x00;
      dev->regs[0x8a] = 0x00;
      dev->regs[0x8b] = 0x00;
      dev->regs[0x8c] = 0x00;
      dev->regs[0x8d] = 0x80;
      dev->regs[0x8e] = 0x68;
      dev->regs[0x8f] = 0x00;
      dev->regs[0x90] = 0x00;
      dev->regs[0x91] = 0x00;
      dev->regs[0x92] = 0x00;
      dev->regs[0x93] = 0x02;
      dev->regs[0x94] = 0x0e;
      dev->regs[0x95] = 0x00;
      dev->regs[0x96] = 0x00;
      dev->regs[0x97] = 0x00;
      dev->regs[0x98] = 0x00;
      dev->regs[0x99] = 0x00;
      dev->regs[0x9a] = 0x00;
      dev->regs[0x9b] = 0x00;
      dev->regs[0x9c] = 0x00;
      dev->regs[0x9d] = 0x00;
      dev->regs[0x9e] = 0x00;
      dev->regs[0x9f] = 0x00;
      dev->regs[0xa0] = 0x00;
      dev->regs[0xa1] = 0x00;
      dev->regs[0xa2] = 0x00;
      dev->regs[0xa3] = 0xcc;
      dev->regs[0xa4] = 0x27;
      dev->regs[0xa5] = 0x64;
      dev->regs[0xa6] = 0x00;
      dev->regs[0xa7] = 0x00;
      dev->regs[0xa8] = 0x00;
      dev->regs[0xa9] = 0x00;
      dev->regs[0xaa] = 0x00;
      dev->regs[0xab] = 0x00;
      dev->regs[0xac] = 0x00;
      dev->regs[0xad] = 0x00;
      dev->regs[0xae] = 0x00;
      dev->regs[0xaf] = 0x00;
      dev->regs[0xb0] = 0x00;
      dev->regs[0xb1] = 0x00;
      dev->regs[0xb2] = 0x02;
      dev->regs[0xb4] = 0x00;
      dev->regs[0xb5] = 0x00;
      dev->regs[0xb6] = 0x00;
      dev->regs[0xb7] = 0x00;
      dev->regs[0xb8] = 0x00;
      dev->regs[0xb9] = 0x00;
      dev->regs[0xba] = 0x00;
      dev->regs[0xbb] = 0x00;
      dev->regs[0xbc] = 0x00;
      dev->regs[0xbd] = 0x00;
      dev->regs[0xbe] = 0x00;
      dev->regs[0xbf] = 0x00;
      dev->regs[0xc0] = 0x00;
      dev->regs[0xc1] = 0x00;
      dev->regs[0xc2] = 0x00;
      dev->regs[0xc3] = 0x00;
      dev->regs[0xc4] = 0x00;
      dev->regs[0xc5] = 0x00;
      dev->regs[0xc6] = 0x00;
      dev->regs[0xc7] = 0x00;
      dev->regs[0xc8] = 0x00;
      dev->regs[0xc9] = 0x00;
      dev->regs[0xca] = 0x00;
      dev->regs[0xcb] = 0x00;
      dev->regs[0xcc] = 0x00;
      dev->regs[0xcd] = 0x00;
      dev->regs[0xce] = 0x00;
      dev->regs[0xcf] = 0x00;
      dev->regs[0xd0] = 0x00;
      dev->regs[0xd1] = 0x00;
      dev->regs[0xd2] = 0x00;
      dev->regs[0xd3] = 0x00;
      dev->regs[0xd4] = 0x00;
      dev->regs[0xd5] = 0x86;
      dev->regs[0xd6] = 0x1b;
      dev->regs[0xd7] = 0x00;
      dev->regs[0xd8] = 0xff;
      dev->regs[0xd9] = 0x00;
      dev->regs[0xda] = 0x27;
      dev->regs[0xdb] = 0x00;
      dev->regs[0xdc] = 0x00;
      dev->regs[0xdd] = 0x00;
      dev->regs[0xde] = 0x00;
      dev->regs[0xdf] = 0x00;
      dev->regs[0xe0] = 0x00;
      dev->regs[0xe1] = 0x00;
      dev->regs[0xe2] = 0x01;
      dev->regs[0xe3] = 0x00;
      dev->regs[0xe4] = 0x00;

      /*dev->regs[0xe5] = 0x14;
         dev->regs[0xe6] = 0x00;    14=20 */
      SET_DOUBLE (dev->regs, EXPOSURE_REG, 20);

      dev->regs[0xe7] = 0x00;
      dev->regs[0xe8] = 0x00;
      dev->regs[0xe9] = 0x00;
      dev->regs[0xea] = 0x00;
      dev->regs[0xeb] = 0x00;
      dev->regs[0xec] = 0x00;
      dev->regs[0xed] = 0x00;
      dev->regs[0xee] = 0x00;
      dev->regs[0xef] = 0x00;
      dev->regs[0xf0] = 0x00;
      dev->regs[0xf1] = 0x00;
      dev->regs[0xf2] = 0x00;
      dev->regs[0xf3] = 0x00;
      break;
    case SENSOR_TYPE_4400:
    case SENSOR_TYPE_4400_BARE:
      for (i = 0; i < dev->reg_count; i++)
	dev->regs[i] = 0x00;
      /* 2d may be 0x20 or 0x22: signals something */
      dev->regs[0x00] = 0xf5;
      dev->regs[0x01] = 0x41;
      dev->regs[0x0b] = 0x70;
      dev->regs[0x10] = 0x50;
      dev->regs[0x11] = 0x03;
      dev->regs[0x12] = 0xff;
      dev->regs[0x13] = 0x3e;
      dev->regs[0x16] = 0xff;
      dev->regs[0x17] = 0x3e;
      dev->regs[0x1d] = 0x22;
      dev->regs[0x22] = 0x01;
      dev->regs[0x23] = 0xff;
      dev->regs[0x25] = 0xfe;
      dev->regs[0x34] = 0x10;
      dev->regs[0x39] = 0x02;
      dev->regs[0x40] = 0x80;
      dev->regs[0x44] = 0x8c;
      dev->regs[0x45] = 0x76;
      dev->regs[0x50] = 0x20;
      dev->regs[0x8b] = 0xff;
      dev->regs[0x8c] = 0x3f;
      dev->regs[0x8d] = 0x80;
      dev->regs[0x8e] = 0x68;
      dev->regs[0x93] = 0x01;
      dev->regs[0x94] = 0x1e;
      dev->regs[0xa3] = 0xcc;
      dev->regs[0xa4] = 0x27;
      dev->regs[0xa5] = 0x64;
      dev->regs[0xb0] = 0x80;
      dev->regs[0xb1] = 0x01;
      dev->regs[0xb2] = 0x06;
      dev->regs[0xd4] = 0x01;
      dev->regs[0xd5] = 0x06;
      dev->regs[0xd6] = 0x1b;
      dev->regs[0xd7] = 0x10;
      dev->regs[0xd8] = 0x7a;
      dev->regs[0xda] = 0xa7;
      dev->regs[0xe5] = 0x0f;
      if (dev->sensor == SENSOR_TYPE_4400_BARE)
	{
	  dev->regs[0x30] = 0x40;	/* 0x00 */
	  dev->regs[0x31] = 0x80;	/* 0x00 */
	  dev->regs[0x44] = 0x00;	/* 0x8c */
	  dev->regs[0x45] = 0x00;	/* 0x76 */
	  dev->regs[0x52] = 0x04;	/* 0x00 */
	  dev->regs[0x69] = 0x01;	/* 0x00 */
	  dev->regs[0x6a] = 0x10;	/* 0x00 */
	  dev->regs[0x6e] = 0x08;	/* 0x00 */
	  dev->regs[0x70] = 0x40;	/* 0x00 */
	  dev->regs[0x71] = 0x08;	/* 0x00 */
	  dev->regs[0x78] = 0x08;	/* 0x00 */
	  dev->regs[0x79] = 0x11;	/* 0x00 */
	  dev->regs[0x82] = 0x40;	/* 0x00 */
	  dev->regs[0x83] = 0x30;	/* 0x00 */
	  dev->regs[0x84] = 0x45;	/* 0x00 */
	  dev->regs[0x87] = 0x02;	/* 0x00 */
	  dev->regs[0x88] = 0x05;	/* 0x00 */
	  dev->regs[0x8b] = 0x01;	/* 0xff */
	  dev->regs[0x8c] = 0x80;	/* 0x3f */
	  dev->regs[0x8d] = 0x04;	/* 0x80 */
	  dev->regs[0x8e] = 0x10;	/* 0x68 */
	  dev->regs[0x8f] = 0x01;	/* 0x00 */
	  dev->regs[0x90] = 0x80;	/* 0x00 */
	}
      break;
    }
  return SANE_STATUS_GOOD;
}

/*
 * This function intializes the device:
 * 	- initial registers values
 * 	- test if at home
 * 	- head parking if needed
 */
static SANE_Status
init_device (struct Rts8891_Device *dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte control, reg, id;
  SANE_Int i, page;
  SANE_Byte buffer[2072];
  char message[256 * 6];
  SANE_Int val;

  /* these commands are used to acces NVRAM through a serial manner */
  /* we ignore NVRAM settingsd for now                              */
  SANE_Byte nv_cmd1[21] =
    { 0x28, 0x38, 0x28, 0x38, 0x08, 0x18, 0x28, 0x38, 0x28, 0x38, 0x28, 0x38,
    0x08, 0x18, 0x28, 0x38, 0x08, 0x18, 0x28, 0x38, 0x08
  };
  SANE_Byte nv_cmd2[21] =
    { 0x28, 0x38, 0x28, 0x38, 0x08, 0x18, 0x28, 0x38, 0x28, 0x38, 0x28, 0x38,
    0x08, 0x18, 0x28, 0x38, 0x28, 0x38, 0x08, 0x18, 0x08
  };
  SANE_Byte nv_cmd3[21] =
    { 0x28, 0x38, 0x28, 0x38, 0x08, 0x18, 0x28, 0x38, 0x28, 0x38, 0x28, 0x38,
    0x08, 0x18, 0x28, 0x38, 0x28, 0x38, 0x28, 0x38, 0x08
  };
  SANE_Byte nv_cmd4[21] =
    { 0x28, 0x38, 0x28, 0x38, 0x08, 0x18, 0x28, 0x38, 0x28, 0x38, 0x08, 0x18,
    0x08, 0x18, 0x28, 0x38, 0x08, 0x18, 0x28, 0x38, 0x08
  };
  SANE_Byte nv_cmd5[21] =
    { 0x28, 0x38, 0x28, 0x38, 0x08, 0x18, 0x28, 0x38, 0x28, 0x38, 0x08, 0x18,
    0x08, 0x18, 0x28, 0x38, 0x28, 0x38, 0x08, 0x18, 0x08
  };
  SANE_Byte nv_cmd6[21] =
    { 0x28, 0x38, 0x28, 0x38, 0x08, 0x18, 0x28, 0x38, 0x28, 0x38, 0x08, 0x18,
    0x08, 0x18, 0x28, 0x38, 0x28, 0x38, 0x28, 0x38, 0x08
  };
  SANE_Byte nv_cmd7[21] =
    { 0x28, 0x38, 0x28, 0x38, 0x08, 0x18, 0x28, 0x38, 0x28, 0x38, 0x08, 0x18,
    0x28, 0x38, 0x08, 0x18, 0x08, 0x18, 0x08, 0x18, 0x08
  };

  DBG (DBG_proc, "init_device: start\n");
  if (dev->initialized == SANE_TRUE)
    return SANE_STATUS_GOOD;

  /* read control register, if busy, something will have to be done ... */
  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
  DBG (DBG_io, "init_device: control=0x%02x\n", control);

  /* when just plugged, we expect to get 0x04 */
  if (control != 0x04)
    {
      DBG (DBG_warn, "init_device: expected control=0x04, got 0x%02x\n",
	   control);
    }

  /* then read "link" register */
  sanei_rts88xx_read_reg (dev->devnum, 0xb0, &control);
  DBG (DBG_io, "init_device: link=0x%02x\n", control);

  /* we expect to get 0x80 */
  if (control != 0x80)
    {
      DBG (DBG_warn, "init_device: expected link=0x80, got 0x%02x\n",
	   control);
    }

  /* reads scanner status */
  sanei_rts88xx_get_status (dev->devnum, dev->regs);
  DBG (DBG_io, "init_device: status=0x%02x 0x%02x\n", dev->regs[0x10],
       dev->regs[0x11]);

  /* reads lamp status and sensor information */
  sanei_rts88xx_get_lamp_status (dev->devnum, dev->regs);
  DBG (DBG_io, "init_device: lamp status=0x%02x\n", dev->regs[0x8e]);

  /* initalize sensor with default from model */
  dev->sensor = dev->model->sensor;
  DBG (DBG_info, "init_device: reg[8e]=0x%02x\n", dev->regs[0x8e]);

  /* reset lamp */
  sanei_rts88xx_reset_lamp (dev->devnum, dev->regs);
  if ((dev->regs[0x8e] & 0x60) != 0x60)
    {
      DBG (DBG_info, "init_device: lamp needs warming\n");
      dev->needs_warming = SANE_TRUE;
    }
  else
    {
      dev->needs_warming = SANE_FALSE;
    }

  /* reads lcd panel status */
  sanei_rts88xx_get_lcd (dev->devnum, dev->regs);
  DBG (DBG_io, "init_device: lcd panel=0x%02x 0x%02x 0x%02x\n",
       dev->regs[0x20], dev->regs[0x21], dev->regs[0x22]);

  /* read mainboard ID/scanner present register */
  sanei_rts88xx_read_reg (dev->devnum, LINK_REG, &id);
  DBG (DBG_io, "init_device: link=0x%02x\n", id);

  /* only known ID is currently 0x00 or 0x01 */
  if (id != 0x00 && id != 0x01)
    {
      DBG (DBG_warn, "init_device: expected id=0x00 or 0x01, got 0x%02x\n",
	   id);
    }

  /* write 0x00 twice to control */
  control = 0x00;
  sanei_rts88xx_write_control (dev->devnum, control);
  sanei_rts88xx_write_control (dev->devnum, control);

  /* read initial register set */
  sanei_rts88xx_read_regs (dev->devnum, 0, dev->regs, dev->reg_count);
  if (DBG_LEVEL > DBG_io2)
    {
      sprintf (message, "init_device: initial register settings: ");
      for (i = 0; i < dev->reg_count; i++)
	sprintf (message + strlen (message), "0x%02x ", dev->regs[i]);

      DBG (DBG_io2, "%s\n", message);
    }

  /* initial sensor guess */
  val = dev->regs[0x44] + 256 * dev->regs[0x45];
  DBG (DBG_io, "init_device: R44/45=0x%04x\n", val);
  if (dev->sensor == SENSOR_TYPE_4400)
    {
      if(val != 0x00)
        {
          DBG (DBG_info, "init_device: SENSOR_TYPE_4400 detected\n");
        }
      else
        {
          DBG (DBG_info, "init_device: SENSOR_TYPE_4400_BARE detected\n");
          dev->sensor = SENSOR_TYPE_4400_BARE;
        }
    }

  init_registers(dev);

  sanei_rts88xx_set_offset (dev->regs, 31, 31, 31);
  sanei_rts88xx_set_gain (dev->regs, 10, 10, 10);
  dev->regs[0] = dev->regs[0] & 0xef;

  rts8891_write_all (dev->devnum, dev->regs, dev->reg_count);
  /* now read button status read_reg(0x1a,2)=0x00 0x00 */
  sanei_rts88xx_read_regs (dev->devnum, 0x1a, dev->regs + 0x1a, 2);
  /* we expect 0x00 0x00 here */
  DBG (DBG_io, "init_device: 0x%02x 0x%02x\n", dev->regs[0x1a],
       dev->regs[0x1b]);

  dev->regs[0xcf] = 0x00;
  dev->regs[0xd0] = 0xe0;
  rts8891_write_all (dev->devnum, dev->regs, dev->reg_count);
  sanei_rts88xx_read_regs (dev->devnum, 0x1a, dev->regs + 0x1a, 2);
  /* we expect 0x08 0x00 here */
  DBG (DBG_io, "init_device: 0x%02x 0x%02x\n", dev->regs[0x1a],
       dev->regs[0x1b]);

  sanei_rts88xx_set_status (dev->devnum, dev->regs, 0x20, 0x28);
  sanei_rts88xx_set_status (dev->devnum, dev->regs, 0x28, 0x28);
  sanei_rts88xx_cancel (dev->devnum);
  sanei_rts88xx_read_reg (dev->devnum, LAMP_REG, &control);
  sanei_rts88xx_write_control (dev->devnum, 0x00);
  sanei_rts88xx_write_control (dev->devnum, 0x00);
  sanei_rts88xx_read_reg (dev->devnum, LINK_REG, &id);
  if (id != 0x00 && id != 0x01)
    {
      DBG (DBG_warn, "init_device: got unexpected id 0x%02x\n", id);
    }


  dev->regs[0x12] = 0xff;
  dev->regs[0x14] = 0xf8;
  dev->regs[0x15] = 0x28;
  dev->regs[0x16] = 0x07;
  dev->regs[0x18] = 0xff;
  dev->regs[0x23] = 0xff;
  dev->regs[0x24] = 0xff;

  dev->regs[0x72] = 0xe1;
  dev->regs[0x73] = 0x14;
  dev->regs[0x74] = 0x18;
  dev->regs[0x75] = 0x15;
  dev->regs[0xc0] = 0xff;
  dev->regs[0xc1] = 0x0f;
  dev->regs[0xc3] = 0xff;
  dev->regs[0xc4] = 0xff;
  dev->regs[0xc5] = 0xff;
  dev->regs[0xc6] = 0xff;
  dev->regs[0xc7] = 0xff;
  dev->regs[0xc8] = 0xff;
  dev->regs[0xca] = 0x0e;
  dev->regs[0xcd] = 0xf0;
  dev->regs[0xce] = 0xff;
  dev->regs[0xcf] = 0xf5;
  dev->regs[0xd0] = 0xf7;
  dev->regs[0xd1] = 0xea;
  dev->regs[0xd2] = 0x0b;
  dev->regs[0xd3] = 0x03;
  dev->regs[0xd4] = 0x05;
  dev->regs[0xd7] = 0x30;
  dev->regs[0xd8] = 0xf6;
  dev->regs[0xd9] = 0x80;
  rts8891_write_all (dev->devnum, dev->regs, dev->reg_count);

  /* now we are writing and reading back from memory, it is surely a memory test since the written data
   * don't look usefull at first glance 
   */
  reg = 0x06;
  sanei_rts88xx_write_reg (dev->devnum, 0x93, &reg);
  for (i = 0; i < 2072; i++)
    {
      buffer[i] = i % 97;
    }
  sanei_rts88xx_set_mem (dev->devnum, 0x81, 0x00, 2072, buffer);
  sanei_rts88xx_get_mem (dev->devnum, 0x81, 0x00, 2072, buffer);
  /* check the data returned */
  for (i = 0; i < 2072; i++)
    {
      if (buffer[i] != id)
	{
	  DBG (DBG_error,
	       "init_device: memory at %d is not 0x%02d (0x%02x)\n", i, id,
	       buffer[i]);
	  /* XXX STEF XXX return SANE_STATUS_IO_ERROR; */
	}
    }
  DBG (DBG_info, "init_device: memory set #1 passed\n");

  /* now test (or set?) #2 */
  reg = 0x02;
  sanei_rts88xx_write_reg (dev->devnum, 0x93, &reg);
  for (i = 0; i < 2072; i++)
    {
      buffer[i] = i % 97;
    }
  sanei_rts88xx_set_mem (dev->devnum, 0x81, 0x00, 2072, buffer);
  sanei_rts88xx_get_mem (dev->devnum, 0x81, 0x00, 2072, buffer);
  /* check the data returned */
  for (i = 0; i < 970; i++)
    {
      if (buffer[i] != (i + 0x36) % 97)
	{
	  DBG (DBG_error,
	       "init_device: memory at %d is not 0x%02x (0x%02x)\n", i,
	       (i + 0x36) % 97, buffer[i]);
	  /* XXX STEF XXX return SANE_STATUS_IO_ERROR; */
	}
    }
  for (i = 993; i < 2072; i++)
    {
      if (buffer[i] != i % 97)
	{
	  DBG (DBG_error,
	       "init_device: memory at %d is invalid 0x%02x, instead of 0x%02x\n",
	       i, buffer[i], i % 97);
	  /* XXX STEF XXX return SANE_STATUS_IO_ERROR; */
	}
    }
  DBG (DBG_info, "init_device: memory set #2 passed\n");


  /* now test (or set?) #3 */
  reg = 0x01;
  sanei_rts88xx_write_reg (dev->devnum, 0x93, &reg);
  for (i = 0; i < 2072; i++)
    {
      buffer[i] = i % 97;
    }
  sanei_rts88xx_set_mem (dev->devnum, 0x81, 0x00, 2072, buffer);
  sanei_rts88xx_get_mem (dev->devnum, 0x81, 0x00, 2072, buffer);
  /* check the data returned */
  for (i = 0; i < 2072; i++)
    {
      if (buffer[i] != i % 97)
	{
	  DBG (DBG_error,
	       "init_device: memory at %d is invalid 0x%02x, instead of 0x%02x\n",
	       i, buffer[i], i % 97);
	  return SANE_STATUS_IO_ERROR;
	}
    }
  DBG (DBG_info, "init_device: memory set #3 passed\n");

  /* we are writing page after page the same pattern, and reading at page 0 until we find we have 'wrapped' */
  /* this is surely some memory amount/number pages detection */
  dev->regs[0x91] = 0x00;	/* page 0 ? */
  dev->regs[0x92] = 0x00;
  dev->regs[0x93] = 0x01;
  rts8891_write_all (dev->devnum, dev->regs, dev->reg_count);

  /* write pattern at page 0 */
  for (i = 0; i < 32; i += 2)
    {
      buffer[i] = i;
      buffer[i + 1] = 0x00;
    }
  sanei_rts88xx_set_mem (dev->devnum, 0x00, 0x00, 32, buffer);

  page = 0;
  do
    {
      /* next page */
      page++;

      /* fill buffer with pattern for page */
      for (i = 0; i < 32; i += 2)
	{
	  buffer[i] = i;
	  buffer[i + 1] = page;
	}
      /* write it to memory */
      sanei_rts88xx_set_mem (dev->devnum, 0x00, page * 16, 32, buffer);
      /* read memory from page 0 */
      sanei_rts88xx_get_mem (dev->devnum, 0x00, 0x00, 32, buffer);
      for (i = 0; i < 32; i += 2)
	{
	  if (buffer[i] != i)
	    {
	      DBG (DBG_error,
		   "init_device: memory at %d is invalid: 0x%02x instead of 0x%02x\n",
		   i, buffer[i], i);
	      return SANE_STATUS_IO_ERROR;
	    }
	  if (buffer[i + 1] != page && buffer[i + 1] != 0)
	    {
	      DBG (DBG_error,
		   "init_device: page %d, memory at %d is invalid: 0x%02x instead of 0x%02x\n",
		   page, i + 1, buffer[i + 1], 0);
	      return SANE_STATUS_IO_ERROR;
	    }
	}
    }
  while (buffer[1] == 0);
  DBG (DBG_info, "init_device: %d pages detected\n", page - 1);

  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
  sanei_rts88xx_read_reg (dev->devnum, CONTROLER_REG, &control);
  if (!(control & 0x02))
    {
      if (park_head (dev, SANE_TRUE) != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "init_device: failed to park head!\n");
	  return SANE_STATUS_IO_ERROR;
	}
    }
  reg = 0x80;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &reg);
  reg = 0xad;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &reg);
  sanei_rts88xx_read_regs (dev->devnum, 0x14, dev->regs + 0x14, 2);
  /* we expect 0xF8 0x28 here */
  dev->regs[0x14] = dev->regs[0x14] & 0x7F;
  sanei_rts88xx_write_regs (dev->devnum, 0x14, dev->regs + 0x14, 2);

  /* reads scanner status */
  sanei_rts88xx_get_status (dev->devnum, dev->regs);
  DBG (DBG_io, "init_device: status=0x%02x 0x%02x\n", dev->regs[0x10],
       dev->regs[0x11]);
  sanei_rts88xx_read_reg (dev->devnum, LINK_REG, &control);
  DBG (DBG_io, "init_device: link=0x%02x\n", control);
  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
  sanei_rts88xx_reset_lamp (dev->devnum, dev->regs);
  reg = 0x8d;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &reg);
  reg = 0xad;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &reg);

  /* here, we are in iddle state */

  /* check link status (scanner still plugged) */
  sanei_rts88xx_read_reg (dev->devnum, LINK_REG, &control);
  DBG (DBG_io, "init_device: link=0x%02x\n", control);

  /* this block appears twice */
  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
  if (control != 0)
    {
      DBG (DBG_warn, "init_device: unexpected control value 0x%02x\n",
	   control);
    }
  sanei_rts88xx_reset_lamp (dev->devnum, dev->regs);
  sanei_rts88xx_read_reg (dev->devnum, 0x40, &reg);
  reg |= 0x80;
  sanei_rts88xx_read_reg (dev->devnum, 0x40, &reg);
  sanei_rts88xx_read_reg (dev->devnum, 0x10, &reg);
  sanei_rts88xx_read_reg (dev->devnum, 0x14, &reg);
  reg = 0x78;
  sanei_rts88xx_write_reg (dev->devnum, 0x10, &reg);
  reg = 0x38;
  sanei_rts88xx_write_reg (dev->devnum, 0x14, &reg);
  reg = 0x78;
  sanei_rts88xx_write_reg (dev->devnum, 0x10, &reg);
  reg = 0x01;
  sanei_rts88xx_write_reg (dev->devnum, CONTROLER_REG, &reg);

  /* now we init nvram */
  /* this is highly dangerous and thus desactivated 
   * in sanei_rts88xx_setup_nvram (HAZARDOUS_EXPERIMENT #define) */
  sanei_rts88xx_setup_nvram (dev->devnum, 21, nv_cmd1);
  sanei_rts88xx_setup_nvram (dev->devnum, 21, nv_cmd2);
  sanei_rts88xx_setup_nvram (dev->devnum, 21, nv_cmd3);
  sanei_rts88xx_set_status (dev->devnum, dev->regs, 0x28, 0x28);

  /* second occurence of this block */
  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
  if (control != 0)
    {
      DBG (DBG_warn, "init_device: unexpected control value 0x%02x\n",
	   control);
    }
  sanei_rts88xx_reset_lamp (dev->devnum, dev->regs);
  sanei_rts88xx_read_reg (dev->devnum, 0x40, &reg);
  reg |= 0x80;
  sanei_rts88xx_read_reg (dev->devnum, 0x40, &reg);
  sanei_rts88xx_read_reg (dev->devnum, 0x10, &reg);
  sanei_rts88xx_read_reg (dev->devnum, 0x14, &reg);
  reg = 0x78;
  sanei_rts88xx_write_reg (dev->devnum, 0x10, &reg);
  reg = 0x38;
  sanei_rts88xx_write_reg (dev->devnum, 0x14, &reg);
  reg = 0x78;
  sanei_rts88xx_write_reg (dev->devnum, 0x10, &reg);
  reg = 0x01;
  sanei_rts88xx_write_reg (dev->devnum, CONTROLER_REG, &reg);

  /* nvram again */
  /* this is highly dangerous and commented out in rts88xx_lib */
  sanei_rts88xx_setup_nvram (dev->devnum, 21, nv_cmd4);
  sanei_rts88xx_setup_nvram (dev->devnum, 21, nv_cmd5);
  sanei_rts88xx_setup_nvram (dev->devnum, 21, nv_cmd6);
  sanei_rts88xx_setup_nvram (dev->devnum, 21, nv_cmd7);

  sanei_rts88xx_set_status (dev->devnum, dev->regs, 0x28, 0x28);

  /* then we do a simple scan to detect a black zone to locate scan area */
  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
  if (control != 0)
    {
      DBG (DBG_warn, "init_device: unexpected control value 0x%02x\n",
	   control);
    }
  sanei_rts88xx_reset_lamp (dev->devnum, dev->regs);
  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
  if (control != 0)
    {
      DBG (DBG_warn, "init_device: unexpected control value 0x%02x\n",
	   control);
    }
  sanei_rts88xx_write_control (dev->devnum, 0x01);
  sanei_rts88xx_write_control (dev->devnum, 0x01);
  sanei_rts88xx_write_control (dev->devnum, 0x00);
  sanei_rts88xx_write_control (dev->devnum, 0x00);
  dev->regs[0x12] = 0xff;
  dev->regs[0x13] = 0x20;
  sanei_rts88xx_write_regs (dev->devnum, 0x12, dev->regs + 0x12, 2);
  dev->regs[0x14] = 0xf8;
  dev->regs[0x15] = 0x28;
  sanei_rts88xx_write_regs (dev->devnum, 0x14, dev->regs + 0x14, 2);
  reg = 0;
  sanei_rts88xx_write_control (dev->devnum, 0x00);
  sanei_rts88xx_write_control (dev->devnum, 0x00);
  sanei_rts88xx_set_status (dev->devnum, dev->regs, 0x28, 0x28);
  reg = 0x80;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &reg);

  dev->regs[0x8b] = 0xff;
  dev->regs[0x8c] = 0x3f;
  dev->regs[LAMP_REG] = 0xad;
  rts8891_write_all (dev->devnum, dev->regs, dev->reg_count);

  set_lamp_brightness (dev, 0);
  dev->initialized = SANE_TRUE;
  DBG (DBG_proc, "init_device: exit\n");
  return status;
}
#endif /* FAST_INIT */

#ifdef FAST_INIT
/*
 * This function detects the scanner
 */
static SANE_Status
detect_device (struct Rts8891_Device *dev)
{
/*
---     5  74945 bulk_out len     4  wrote 0x80 0xb0 0x00 0x01 
---     6  74946 bulk_in  len     1  read  0x80 
---     7  74947 bulk_out len     4  wrote 0x80 0xb3 0x00 0x01 
---     8  74948 bulk_in  len     1  read  0x04 
---     9  74949 bulk_out len     4  wrote 0x80 0xb1 0x00 0x01 
---    10  74950 bulk_in  len     1  read  0x00 
---    11  74951 bulk_out len     5  wrote 0x88 0xb3 0x00 0x01 0x00 
---    12  74952 bulk_out len     5  wrote 0x88 0xb3 0x00 0x01 0x00 
---    13  74953 bulk_out len     4  wrote 0x80 0x00 0x00 0xf4 
---    14  74954 bulk_in  len   192  read  0xf5 0x41 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x70 0x00 0x00 0x00 0x00 0x60 0x1b 0xff 0x3e 0x00 0x00 0xff 0x3e 0x00 0x00 0x00 0x00 0x00 0x22 0x00 0x00 0x00 0x00 0x00 0xff 0x00 0xff 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x01 0x42 0x00 0x00 0x10 0x00 0x00 0x00 0x00 0x02 0x00 0x00 0x00 0x00 0x00 0x00 0x80 0x00 0x00 0x00 0x18 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x20 0x24 0x04 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x80 0x10 0x00 0x80 0x00 0x10 0x00 0x01 0x00 0x00 0x10 0x00 0x00 0x20 0x20 0x00 0x00 0x00 0x00 0x20 0x01 0x00 0x02 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x41 0x00 0x10 0x21 0x20 0x00 0x20 0x81 0x20 0x01 0x01 0x00 0x22 0x00 0x40 0x05 0x00 0x00 0x01 0x1e 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xcc 0x27 0x64 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x80 0x00 0x06 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 
---    15  74955 bulk_in  len    52  read  0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x01 0x06 0x1b 0x10 0x7a 0x00 0x15 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x0f 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 
*/
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte control;
  char message[256 * 5];
  int i;

  DBG (DBG_proc, "detect_device: start\n");

  /* read "b0" register */
  sanei_rts88xx_read_reg (dev->devnum, 0xb0, &control);
  DBG (DBG_io, "detect_device: reg[b0]=0x%02x\n", control);

  /* we expect to get 0x80 */
  if (control != 0x80)
    {
      DBG (DBG_warn, "detect_device: expected reg[b0]=0x80, got 0x%02x\n",
	   control);
      /* TODO : fail here ? */
    }

  /* read control register, if busy, something will have to be done ... */
  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);

  /* when just plugged, we expect to get 0x04 */
  if (control != 0x04)
    {
      DBG (DBG_warn, "detect_device: expected control=0x04, got 0x%02x\n",
	   control);
      /* TODO ok if re-run without plugging */
    }

  /* read "LINK" register */
  sanei_rts88xx_read_reg (dev->devnum, LINK_REG, &control);
  DBG (DBG_io, "detect_device: LINK_REG=0x%02x\n", control);

  /* we expect to get 0x00 */
  if (control != 0x00)
    {
      DBG (DBG_warn, "detect_device: expected LINK_REG=0x00, got 0x%02x\n",
	   control);
    }

  /* write 0x00 twice to control:  cancel/stop ? */
  control = 0x00;
  sanei_rts88xx_write_control (dev->devnum, control);
  sanei_rts88xx_write_control (dev->devnum, control);

  /* read initial register set */
  sanei_rts88xx_read_regs (dev->devnum, 0, dev->regs, dev->reg_count);
  if (DBG_LEVEL >= DBG_io2)
    {
      sprintf (message, "init_device: initial register settings: ");
      for (i = 0; i < dev->reg_count; i++)
	sprintf (message + strlen (message), "0x%02x ", dev->regs[i]);
      sprintf (message + strlen (message), "\n");
      DBG (DBG_io2, message);
    }

  /* initial sensor guess */
  val = dev->regs[0x44] + 256 * dev->regs[0x45];
  DBG (DBG_io, "detect_device: R44/45=0x%04x\n", val);
  if (dev->sensor == SENSOR_TYPE_4400 && val == 0x00)
    {
      dev->sensor = SENSOR_TYPE_4400_BARE;
      DBG (DBG_info, "detect_device: changing to SENSOR_TYPE_4400_BARE\n");
    }

  DBG (DBG_io, "detect_device: status=0x%02x 0x%02x\n", dev->regs[0x10],
       dev->regs[0x11]);
  DBG (DBG_io, "detect_device: lamp status=0x%02x\n", dev->regs[0x8e]);

  dev->initialized = SANE_FALSE;
  DBG (DBG_proc, "detect_device: exit\n");
  return status;
}
#endif

/**
 * Do dark calibration. We scan a well defined area until average pixel value
 * of the black area is about 0x03 for each color channel. This calibration is
 * currently done at 75 dpi regardless of the final scan dpi.
 */
static SANE_Status
dark_calibration (struct Rts8891_Device *dev, int mode, int light)
{
  SANE_Status status = SANE_STATUS_GOOD;
/* red, green and blue offset, within a 't'op and 'b'ottom value */
  int ro = 250, tro = 250, bro = 0;
  int bo = 250, tbo = 250, bbo = 0;
  int go = 250, tgo = 250, bgo = 0;
  unsigned char image[CALIBRATION_SIZE];
  float global, ra, ga, ba;
  int num = 0;
  char name[32];

  DBG (DBG_proc, "dark_calibration: start\n");

  /* set up sensor specific bottom values */
  if (dev->sensor == SENSOR_TYPE_4400)
    {
      bro = 128;
      bgo = 128;
      bbo = 128;
    }

  /* set up starting values */
  sanei_rts88xx_set_gain (dev->regs, 0, 0, 0);
  sanei_rts88xx_set_scan_area (dev->regs, 1, 2, 4, 4 + CALIBRATION_WIDTH);

  sanei_rts88xx_set_status (dev->devnum, dev->regs, mode, light);

  dev->regs[0x00] = 0xe5;	/* scan */
  dev->regs[0x32] = 0x00;
  dev->regs[0x33] = 0x03;
  dev->regs[0x35] = 0x45;
  dev->regs[0x36] = 0x22;
  dev->regs[0x3a] = 0x43;

  dev->regs[0x8d] = 0x00;
  dev->regs[0x8e] = 0x60;

  dev->regs[0xb2] = 0x02;

  dev->regs[0xc0] = 0x06;
  dev->regs[0xc1] = 0xe6;
  dev->regs[0xc2] = 0x67;
  dev->regs[0xc9] = 0x07;
  dev->regs[0xca] = 0x00;
  dev->regs[0xcb] = 0xfe;
  dev->regs[0xcc] = 0xf9;
  dev->regs[0xcd] = 0x19;
  dev->regs[0xce] = 0x98;
  dev->regs[0xcf] = 0xe8;
  dev->regs[0xd0] = 0xea;
  dev->regs[0xd1] = 0xf3;
  dev->regs[0xd2] = 0x14;
  dev->regs[0xd3] = 0x02;
  dev->regs[0xd4] = 0x04;
  dev->regs[0xd6] = 0x0f;
  dev->regs[0xd8] = 0x52;
  dev->regs[0xe2] = 0x1f;

  /*dev->regs[0xe5] = 0x28;      28=40 
     dev->regs[0xe6] = 0x00; */
  SET_DOUBLE (dev->regs, EXPOSURE_REG, 40);

  dev->regs[0xe7] = 0x75;
  dev->regs[0xe8] = 0x01;
  dev->regs[0xe9] = 0x0b;
  dev->regs[0xea] = 0x54;
  dev->regs[0xeb] = 0x01;
  dev->regs[0xec] = 0x04;
  dev->regs[0xed] = 0xb8;
  dev->regs[0xef] = 0x03;
  dev->regs[0xf0] = 0x70;
  dev->regs[0xf2] = 0x01;

  /* handling of different sensors */
  if (dev->sensor == SENSOR_TYPE_XPA || dev->sensor == SENSOR_TYPE_4400)
    {
      dev->regs[0xc0] = 0x67;
      dev->regs[0xc1] = 0x06;
      dev->regs[0xc2] = 0xe6;
      dev->regs[0xc3] = 0x98;
      dev->regs[0xc4] = 0xf9;
      dev->regs[0xc5] = 0x19;
      dev->regs[0xc6] = 0x67;
      dev->regs[0xc7] = 0x06;
      dev->regs[0xc8] = 0xe6;
      dev->regs[0xc9] = 0x01;
      dev->regs[0xca] = 0xf8;
      dev->regs[0xcb] = 0xff;
      dev->regs[0xcc] = 0x98;
      dev->regs[0xcd] = 0xf9;
      dev->regs[0xce] = 0x19;
      dev->regs[0xcf] = 0xe0;
      dev->regs[0xd0] = 0xe2;
      dev->regs[0xd1] = 0xeb;
      dev->regs[0xd2] = 0x0c;
      dev->regs[0xd7] = 0x10;
      dev->regs[0xda] = 0xa7;
    }
  if (dev->sensor == SENSOR_TYPE_4400)
    {
      dev->regs[0x13] = 0x39;	/* 0x20 */
      dev->regs[0x14] = 0xf0;	/* 0xf8 */
      dev->regs[0x15] = 0x29;	/* 0x28 */
      dev->regs[0x16] = 0x0f;	/* 0x07 */
      dev->regs[0x17] = 0x10;	/* 0x00 */
      dev->regs[0x23] = 0x00;	/* 0xff */
      dev->regs[0x35] = 0x48;	/* 0x45 */
      dev->regs[0x39] = 0x00;	/* 0x02 */
      dev->regs[0xe2] = 0x0f;	/* 0x1f */
      /* dev->regs[0xe5] = 0x52; 0x28 */
      SET_DOUBLE (dev->regs, EXPOSURE_REG, 82);	/* 2*40+2 */
      dev->regs[0xe7] = 0x0e;	/* 0x75 */
      dev->regs[0xe9] = 0x0a;	/* 0x0b */
      dev->regs[0xea] = 0xc2;	/* 0x54 */
      dev->regs[0xed] = 0xf6;	/* 0xb8 */
      dev->regs[0xef] = 0x02;	/* 0x03 */
      dev->regs[0xf0] = 0xa8;	/* 0x70 */
    }
  if (dev->sensor == SENSOR_TYPE_4400_BARE)
    {
      dev->regs[0x13] = 0x39;	/* 0x20 */
      dev->regs[0x14] = 0xf0;	/* 0xf8 */
      dev->regs[0x15] = 0x29;	/* 0x28 */
      dev->regs[0x16] = 0x0f;	/* 0x07 */
      dev->regs[0x17] = 0x10;	/* 0x00 */
      dev->regs[0x23] = 0x00;	/* 0xff */
      dev->regs[0x35] = 0x48;	/* 0x45 */
      dev->regs[0x39] = 0x00;	/* 0x02 */
      dev->regs[0xda] = 0xa7;	/* 0xa0 */
      dev->regs[0xe2] = 0x0f;	/* 0x1f */
      SET_DOUBLE (dev->regs, EXPOSURE_REG, 82);
      dev->regs[0xe7] = 0x0e;	/* 0x75 */
      dev->regs[0xe9] = 0x0a;	/* 0x0b */
      dev->regs[0xea] = 0xc2;	/* 0x54 */
      dev->regs[0xed] = 0xf6;	/* 0xb8 */
      dev->regs[0xef] = 0x02;	/* 0x03 */
      dev->regs[0xf0] = 0xa8;	/* 0x70 */
    }

  /* we loop scanning a 637 (1911 bytes) pixels wide area in color mode 
   * until each black average reaches the desired value */
  sanei_rts88xx_set_status (dev->devnum, dev->regs, mode, light);
  do
    {
      /* set scan with the values to try */
      sanei_rts88xx_set_offset (dev->regs, ro, go, bo);
      DBG (DBG_info, "dark_calibration: trying offsets=(%d,%d,%d)\n", ro, go,
	   bo);

      /* do scan */
      status =
	rts8891_simple_scan (dev->devnum, dev->regs, dev->reg_count, 0x02,
			     CALIBRATION_SIZE, image);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "dark_calibration: failed to scan\n");
	  return status;
	}

      /* save scanned picture for data debugging */
      if (DBG_LEVEL >= DBG_io2)
	{
	  sprintf (name, "dark%03d.pnm", num);
	  write_rgb_data (name, image, CALIBRATION_WIDTH, 1);
	  num++;
	}

      /* we now compute the average of red pixels from the first 15 pixels */
      global = average_area (SANE_TRUE, image, 15, 1, &ra, &ga, &ba);
      DBG (DBG_info,
	   "dark_calibration: global=%.2f, channels=(%.2f,%.2f,%.2f)\n",
	   global, ra, ga, ba);

      /* dichotomie ... */
      if (abs (ra - DARK_TARGET) < DARK_MARGIN)
	{
	  /* offset is OK */
	  tro = ro;
	  bro = ro;
	}
      else
	{			/* NOK */
	  if (ra > DARK_TARGET)
	    {
	      tro = ro;
	      ro = (tro + bro) / 2;
	    }
	  if (ra < DARK_TARGET)
	    {
	      bro = ro;
	      ro = (tro + bro + 1) / 2;
	    }

	}

      /* same for blue channel */
      if (abs (ba - DARK_TARGET) < DARK_MARGIN)
	{
	  bbo = bo;
	  tbo = bo;
	}
      else
	{			/* NOK */
	  if (ba > DARK_TARGET)
	    {
	      tbo = bo;
	      bo = (tbo + bbo) / 2;
	    }
	  if (ba < DARK_TARGET)
	    {
	      bbo = bo;
	      bo = (tbo + bbo + 1) / 2;
	    }

	}

      /* and for green channel */
      if (abs (ga - DARK_TARGET) < DARK_MARGIN)
	{
	  tgo = go;
	  bgo = go;
	}
      else
	{			/* NOK */
	  if (ga > DARK_TARGET)
	    {
	      tgo = go;
	      go = (tgo + bgo) / 2;
	    }
	  if (ga < DARK_TARGET)
	    {
	      bgo = go;
	      go = (tgo + bgo + 1) / 2;
	    }
	}
    }
  while ((tro != bro) || (tgo != bgo) || (tbo != bbo));

  /* store detected values in device struct */
  dev->red_offset = bro;
  dev->green_offset = bgo;
  dev->blue_offset = bbo;

  DBG (DBG_info, "dark_calibration: final offsets=(%d,%d,%d)\n", bro, bgo,
       bbo);
  DBG (DBG_proc, "dark_calibration: exit\n");
  return status;
}

/*
 * do gain calibration. We do scans until averaged values of the area match 
 * the target code. We're doing a dichotomy again.
 */
static SANE_Status
gain_calibration (struct Rts8891_Device *dev, int mode, int light)
{
  SANE_Status status = SANE_STATUS_GOOD;
  float global, ra, ga, ba;
  /* previous values */
  int xrg, xbg, xgg;
/* current gains */
  int rg, bg, gg;
/* intervals for dichotomy */
  int trg, tbg, tgg, bgg, brg, bbg;
  int num = 0;
  char name[32];
  int width = CALIBRATION_WIDTH;
  int length = CALIBRATION_SIZE;
  unsigned char image[CALIBRATION_SIZE];
  int pass = 0;
  int timing=0;

  int xstart = (dev->left_offset * 75) / dev->model->max_xdpi;

  DBG (DBG_proc, "gain_calibration: start\n");

  /* set up starting values */
  sanei_rts88xx_set_scan_area (dev->regs, 1, 2, xstart, xstart + width);
  sanei_rts88xx_set_offset (dev->regs, dev->red_offset, dev->green_offset,
			    dev->blue_offset);

  /* same register set than dark calibration */
  dev->regs[0x32] = 0x00;
  dev->regs[0x33] = 0x03;
  dev->regs[0x35] = 0x45;
  dev->regs[0x36] = 0x22;
  dev->regs[0x3a] = 0x43;

  dev->regs[0x8d] = 0x00;
  dev->regs[0x8e] = 0x60;

  dev->regs[0xb2] = 0x02;

  dev->regs[0xc0] = 0x06;
  dev->regs[0xc1] = 0xe6;
  dev->regs[0xc2] = 0x67;
  dev->regs[0xc9] = 0x07;
  dev->regs[0xca] = 0x00;
  dev->regs[0xcb] = 0xfe;
  dev->regs[0xcc] = 0xf9;
  dev->regs[0xcd] = 0x19;
  dev->regs[0xce] = 0x98;
  dev->regs[0xcf] = 0xe8;
  dev->regs[0xd0] = 0xea;
  dev->regs[0xd1] = 0xf3;
  dev->regs[0xd2] = 0x14;
  dev->regs[0xd3] = 0x02;
  dev->regs[0xd4] = 0x04;
  dev->regs[0xd6] = 0x0f;
  dev->regs[0xd8] = 0x52;
  dev->regs[0xe2] = 0x1f;

  /* dev->regs[0xe5] = 0x28;
     dev->regs[0xe6] = 0x00;    28=40  */
  SET_DOUBLE (dev->regs, EXPOSURE_REG, 40);

  dev->regs[0xe7] = 0x75;
  dev->regs[0xe8] = 0x01;
  dev->regs[0xe9] = 0x0b;
  dev->regs[0xea] = 0x54;
  dev->regs[0xeb] = 0x01;
  dev->regs[0xec] = 0x04;
  dev->regs[0xed] = 0xb8;
  dev->regs[0xef] = 0x03;
  dev->regs[0xf0] = 0x70;
  dev->regs[0xf2] = 0x01;

  dev->regs[0x72] = 0xe1;
  dev->regs[0x73] = 0x14;
  dev->regs[0x74] = 0x18;

  dev->regs[0xc3] = 0xff;
  dev->regs[0xc4] = 0xff;
  dev->regs[0xc7] = 0xff;
  dev->regs[0xc8] = 0xff;

  dev->regs[0xd7] = 0x30;

  if (dev->sensor == SENSOR_TYPE_XPA || dev->sensor == SENSOR_TYPE_4400)
    {
      dev->regs[0xc0] = 0x67;
      dev->regs[0xc1] = 0x06;
      dev->regs[0xc2] = 0xe6;
      dev->regs[0xc3] = 0x98;
      dev->regs[0xc4] = 0xf9;
      dev->regs[0xc5] = 0x19;
      dev->regs[0xc6] = 0x67;
      dev->regs[0xc7] = 0x06;
      dev->regs[0xc8] = 0xe6;
      dev->regs[0xc9] = 0x01;
      dev->regs[0xca] = 0xf8;
      dev->regs[0xcb] = 0xff;
      dev->regs[0xcc] = 0x98;
      dev->regs[0xcd] = 0xf9;
      dev->regs[0xce] = 0x19;
      dev->regs[0xcf] = 0xe0;
      dev->regs[0xd0] = 0xe2;
      dev->regs[0xd1] = 0xeb;
      dev->regs[0xd2] = 0x0c;
      dev->regs[0xd3] = 0x02;
      dev->regs[0xd4] = 0x04;
      dev->regs[0xd6] = 0x0f;
      dev->regs[0xd7] = 0x10;
      dev->regs[0xd8] = 0x52;
      dev->regs[0xe2] = 0x1f;
      /* dev->regs[0xe5] = 0x28;    0028 -> 40 
         dev->regs[0xe6] = 0x00; */
      SET_DOUBLE (dev->regs, EXPOSURE_REG, 40);
      dev->regs[0xe7] = 0x75;
      dev->regs[0xe8] = 0x01;
      dev->regs[0xe9] = 0x0b;
      dev->regs[0xea] = 0x54;
      dev->regs[0xeb] = 0x01;
      dev->regs[0xec] = 0x04;
      dev->regs[0xed] = 0xb8;
      dev->regs[0xef] = 0x03;
      dev->regs[0xf0] = 0x70;
      dev->regs[0xf2] = 0x01;
    }
  if (dev->sensor == SENSOR_TYPE_4400)
    {
      dev->regs[0x35] = 0x48;	/* 0x45 */
      /* c5, c6 ? : untouched from previous scan ... */
      dev->regs[0xe2] = 0x0f;	/* 0x1f */
      /* dev->regs[0xe5] = 0x52;        */
      SET_DOUBLE (dev->regs, EXPOSURE_REG, 82);	/* 2*40+2 */
      dev->regs[0xe7] = 0x0e;	/* 0x75 */
      dev->regs[0xe9] = 0x0a;	/* 0x0b */
      dev->regs[0xea] = 0xc2;	/* 0x54 */
      dev->regs[0xed] = 0xf6;	/* 0xb8 */
      dev->regs[0xef] = 0x02;	/* 0x03 */
      dev->regs[0xf0] = 0xa8;	/* 0x70 */
    }
  if (dev->sensor == SENSOR_TYPE_4400_BARE)
    {
      dev->regs[0x13] = 0x39;
      dev->regs[0x14] = 0xf0;
      dev->regs[0x15] = 0x29;
      dev->regs[0x16] = 0x0f;
      dev->regs[0x17] = 0x10;
      dev->regs[0x23] = 0x00;
      dev->regs[0x35] = 0x48;
      dev->regs[0x39] = 0x00;
      dev->regs[0xe2] = 0x0f;
      SET_DOUBLE (dev->regs, EXPOSURE_REG, 82);
      dev->regs[0xe7] = 0x0e;
      dev->regs[0xe9] = 0x0a;
      dev->regs[0xea] = 0xc2;
      dev->regs[0xed] = 0xf6;
      dev->regs[0xef] = 0x02;
      dev->regs[0xf0] = 0xa8;

      timing=0x32;
      dev->regs[0x85] = 0x00;
      dev->regs[0x86] = 0x06;
      dev->regs[0x87] = 0x00;
      dev->regs[0x88] = 0x06;
    }
  SET_DOUBLE (dev->regs, TIMING_REG, timing);
  SET_DOUBLE (dev->regs, TIMING1_REG, timing+1);
  SET_DOUBLE (dev->regs, TIMING2_REG, timing+2);


  /* we loop scanning a 637 (1911 bytes) pixels wide area in color mode until each white average
   * reaches the desired value, doing a dichotomy */
  rg = 0x1f;
  bg = 0x1f;
  gg = 0x1f;
  /* dichotomy interval */
  /* bottom values */
  brg = 0;
  bgg = 0;
  bbg = 0;
  /* top values */
  trg = 0x1f;
  tgg = 0x1f;
  tbg = 0x1f;

  /* loop on gain calibration until we find until we find stable value
   * or we do more than 20 tries */
  do
    {
      /* store current values as previous */
      xrg = rg;
      xbg = bg;
      xgg = gg;

      /* set up for scan */
      DBG (DBG_info,
	   "gain_calibration: trying gains=(0x%02x,0x%02x,0x%02x)\n", rg, gg,
	   bg);
      sanei_rts88xx_set_gain (dev->regs, rg, gg, bg);
      sanei_rts88xx_set_status (dev->devnum, dev->regs, mode, light);

      /* scan on line in RGB */
      status =
	rts8891_simple_scan (dev->devnum, dev->regs, dev->reg_count, 0x02,
			     length, image);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gain_calibration: failed scan data\n");
	  return status;
	}

      /* save scanned picture for data debugging */
      if (DBG_LEVEL >= DBG_io2)
	{
	  sprintf (name, "gain%03d.pnm", num);
	  write_rgb_data (name, image, width, 1);
	  num++;
	}

      /* we now compute the average pixel value on the scanned line */
      /* TODO use computed left margin */
      global = average_area (SANE_TRUE, image, width, 1, &ra, &ga, &ba);
      DBG (DBG_info,
	   "gain_calibration: global=%.2f, channels=(%.2f,%.2f,%.2f)\n",
	   global, ra, ga, ba);

      /* dichotomy again ... */
      if (abs (ra - RED_GAIN_TARGET) < GAIN_MARGIN)
	{
	  /* gain is OK, it is whitin the tolerance margin */
	  trg = rg;
	  brg = rg;
	}
      else
	{			/* average is higher than the desired value */
	  if (ra > RED_GAIN_TARGET)
	    {
	      /* changing gain for another channel alters other output */
	      /* a stable value is shown by top value == bottom value */
	      if (trg == brg)
		{
		  brg--;
		  rg = brg;
		}
	      else
		{
		  /* since we'ra above target value, current gain becomes top value */
		  trg = rg;
		  rg = (trg + brg) / 2;
		}
	    }
	  else			/* below target value */
	    {
	      if (trg == brg)
		{
		  trg++;
		  rg = trg;
		}
	      else
		{
		  /* since we are below target, current value is assigned to bottom value */
		  brg = rg;
		  rg = (trg + brg + 1) / 2;
		}
	    }
	}

      /* same for blue channel */
      if (abs (ba - BLUE_GAIN_TARGET) < GAIN_MARGIN)
	{
	  bbg = bg;
	  tbg = bg;
	}
      else
	{			/* NOK */
	  if (ba > BLUE_GAIN_TARGET)
	    {
	      if (tbg == bbg)
		{
		  bbg--;
		  bg = bbg;
		}
	      else
		{
		  tbg = bg;
		  bg = (tbg + bbg) / 2;
		}
	    }
	  else
	    {
	      if (tbg == bbg)
		{
		  tbg++;
		  bg = tbg;
		}
	      else
		{
		  bbg = bg;
		  bg = (tbg + bbg + 1) / 2;
		}
	    }
	}

      /* and for green channel */
      if (abs (ga - GREEN_GAIN_TARGET) < GAIN_MARGIN)
	{
	  tgg = gg;
	  bgg = gg;
	}
      else
	{			/* NOK */
	  if (ga > GREEN_GAIN_TARGET)
	    {
	      if (tgg == bgg)
		{
		  bgg--;
		  gg = bgg;
		}
	      else
		{
		  tgg = gg;
		  gg = (tgg + bgg) / 2;
		}
	    }
	  else
	    {
	      if (tgg == bgg)
		{
		  tgg++;
		  gg = tgg;
		}
	      else
		{
		  bgg = gg;
		  gg = (tgg + bgg + 1) / 2;
		}
	    }
	}
      pass++;
    }
  while ((pass < 20)
	 && ((trg != brg && xrg != rg)
	     || (tgg != bgg && xgg != gg) || (tbg != bbg && xbg != bg)));

  /* store detected values in device struct */
  dev->red_gain = brg;
  dev->green_gain = bgg;
  dev->blue_gain = bbg;
  DBG (DBG_info, "gain_calibration: gain=(0x%02x,0x%02x,0x%02x)\n", brg, bgg,
       bbg);

  DBG (DBG_proc, "gain_calibration: exit\n");
  return status;
}

/**
 * Do fine offset calibration. Scans are done with gains from gain calibration
 * at 75 dpi regardless of the dpi of the final scan. We loop scanning a 637 
 * (1911 bytes) pixels wide area in color mode until each black average
 * reaches the desired value (OFFSET_TARGET).
 */
static SANE_Status
offset_calibration (struct Rts8891_Device *dev, int mode, int light)
{

  SANE_Status status = SANE_STATUS_GOOD;
  /* red, green and blue offset, within a 't'op and 'b'ottom value */
  int ro = 250, tro = 250, bro = 123;
  int go = 250, tgo = 250, bgo = 123;
  int bo = 250, tbo = 250, bbo = 123;
  unsigned char image[CALIBRATION_SIZE];
  float global, ra, ga, ba;
  int num = 0;
  char name[32];

  DBG (DBG_proc, "offset_calibration: start\n");

  /* set up starting values */
  /* gains from previous step are a little higher than the one used */
  sanei_rts88xx_set_gain (dev->regs, dev->red_gain, dev->green_gain,
			  dev->blue_gain);
  sanei_rts88xx_set_scan_area (dev->regs, 1, 2, 4, 4 + CALIBRATION_WIDTH);

  dev->regs[0x32] = 0x00;
  dev->regs[0x33] = 0x03;
  dev->regs[0x35] = 0x45;
  dev->regs[0x36] = 0x22;
  dev->regs[0x3a] = 0x43;

  dev->regs[0x8d] = 0x00;
  dev->regs[0x8e] = 0x60;
  dev->regs[0xb2] = 0x02;
  dev->regs[0xc0] = 0x06;
  dev->regs[0xc1] = 0xe6;
  dev->regs[0xc2] = 0x67;
  dev->regs[0xc9] = 0x07;
  dev->regs[0xca] = 0x00;
  dev->regs[0xcb] = 0xfe;
  dev->regs[0xcc] = 0xf9;
  dev->regs[0xcd] = 0x19;
  dev->regs[0xce] = 0x98;
  dev->regs[0xcf] = 0xe8;
  dev->regs[0xd0] = 0xea;
  dev->regs[0xd1] = 0xf3;
  dev->regs[0xd2] = 0x14;
  dev->regs[0xd3] = 0x02;
  dev->regs[0xd4] = 0x04;
  dev->regs[0xd6] = 0x0f;
  dev->regs[0xd8] = 0x52;
  dev->regs[0xe2] = 0x1f;
  /* dev->regs[0xe5] = 0x28;
     dev->regs[0xe6] = 0x00;     0x0028=40 */
  SET_DOUBLE (dev->regs, EXPOSURE_REG, 40);
  dev->regs[0xe7] = 0x75;
  dev->regs[0xe8] = 0x01;
  dev->regs[0xe9] = 0x0b;
  dev->regs[0xea] = 0x54;
  dev->regs[0xeb] = 0x01;
  dev->regs[0xec] = 0x04;
  dev->regs[0xed] = 0xb8;
  dev->regs[0xef] = 0x03;
  dev->regs[0xf0] = 0x70;
  dev->regs[0xf2] = 0x01;
  if (dev->sensor == SENSOR_TYPE_XPA || dev->sensor == SENSOR_TYPE_4400)
    {
      dev->regs[0x72] = 0xe1;
      dev->regs[0x73] = 0x14;
      dev->regs[0x74] = 0x18;

      dev->regs[0xc0] = 0x67;
      dev->regs[0xc1] = 0x06;
      dev->regs[0xc2] = 0xe6;
      dev->regs[0xc3] = 0x98;
      dev->regs[0xc4] = 0xf9;
      dev->regs[0xc5] = 0x19;
      dev->regs[0xc6] = 0x67;
      dev->regs[0xc7] = 0x06;
      dev->regs[0xc8] = 0xe6;
      dev->regs[0xc9] = 0x01;
      dev->regs[0xca] = 0xf8;
      dev->regs[0xcb] = 0xff;
      dev->regs[0xcc] = 0x98;
      dev->regs[0xcd] = 0xf9;
      dev->regs[0xce] = 0x19;
      dev->regs[0xcf] = 0xe0;
      dev->regs[0xd0] = 0xe2;
      dev->regs[0xd1] = 0xeb;
      dev->regs[0xd2] = 0x0c;
      dev->regs[0xd7] = 0x10;
    }
  if (dev->sensor == SENSOR_TYPE_4400)
    {
      dev->regs[0x35] = 0x48;	/* 0x45 */
      /* c5,c6 ?? */
      dev->regs[0xe2] = 0x0f;	/* 0x1f */
      /* dev->regs[0xe5] = 0x52;        0x28 */
      SET_DOUBLE (dev->regs, EXPOSURE_REG, 82);	/* 2*40+2 */
      dev->regs[0xe7] = 0x0e;	/* 0x75 */
      dev->regs[0xe9] = 0x0a;	/* 0x0b */
      dev->regs[0xea] = 0xc2;	/* 0x54 */
      dev->regs[0xed] = 0xf6;	/* 0xb8 */
      dev->regs[0xef] = 0x02;	/* 0x03 */
      dev->regs[0xf0] = 0xa8;	/* 0x70 */
    }
  if (dev->sensor == SENSOR_TYPE_4400_BARE)
    {
      dev->regs[0x13] = 0x39;	/* 0x20 */
      dev->regs[0x14] = 0xf0;	/* 0xf8 */
      dev->regs[0x15] = 0x29;	/* 0x28 */
      dev->regs[0x16] = 0x0f;	/* 0x07 */
      dev->regs[0x17] = 0x10;	/* 0x00 */
      dev->regs[0x23] = 0x00;	/* 0xff */
      dev->regs[0x35] = 0x48;	/* 0x45 */
      dev->regs[0x39] = 0x00;	/* 0x02 */
      dev->regs[0xe2] = 0x0f;	/* 0x1f */
      SET_DOUBLE (dev->regs, EXPOSURE_REG, 82);	/* 2*40+2 */
      dev->regs[0xe7] = 0x0e;	/* 0x75 */
      dev->regs[0xe9] = 0x0a;	/* 0x0b */
      dev->regs[0xea] = 0xc2;	/* 0x54 */
      dev->regs[0xed] = 0xf6;	/* 0xb8 */
      dev->regs[0xef] = 0x02;	/* 0x03 */
      dev->regs[0xf0] = 0xa8;	/* 0x70 */
    }

  /* we loop scanning a 637 (1911 bytes) pixels wide area in color mode until each black average
   * reaches the desired value */
  do
    {
      DBG (DBG_info, "offset_calibration: trying offsets=(%d,%d,%d) ...\n",
	   ro, go, bo);
      sanei_rts88xx_set_offset (dev->regs, ro, go, bo);
      sanei_rts88xx_set_status (dev->devnum, dev->regs, mode, light);
      rts8891_simple_scan (dev->devnum, dev->regs, dev->reg_count, 0x02,
			   CALIBRATION_SIZE, image);

      /* save scanned picture for data debugging */
      if (DBG_LEVEL >= DBG_io2)
	{
	  sprintf (name, "offset%03d.pnm", num);
	  write_rgb_data (name, image, CALIBRATION_WIDTH, 1);
	  num++;
	}

      /* we now compute the average of red pixels from the first 15 pixels */
      global = average_area (SANE_TRUE, image, 15, 1, &ra, &ga, &ba);
      DBG (DBG_info,
	   "offset_calibration: global=%.2f, channels=(%.2f,%.2f,%.2f)\n",
	   global, ra, ga, ba);

      /* dichotomie ... */
      if (abs (ra - OFFSET_TARGET) < OFFSET_MARGIN)
	{
	  /* offset is OK */
	  tro = ro;
	  bro = ro;
	}
      else
	{			/* NOK */
	  if (ra > OFFSET_TARGET)
	    {
	      tro = ro;
	      ro = (tro + bro) / 2;
	    }
	  if (ra < OFFSET_TARGET)
	    {
	      bro = ro;
	      ro = (tro + bro + 1) / 2;
	    }

	}

      /* same for blue channel */
      if (abs (ba - OFFSET_TARGET) < OFFSET_MARGIN)
	{
	  bbo = bo;
	  tbo = bo;
	}
      else
	{
	  if (ba > OFFSET_TARGET)
	    {
	      tbo = bo;
	      bo = (tbo + bbo) / 2;
	    }
	  if (ba < OFFSET_TARGET)
	    {
	      bbo = bo;
	      bo = (tbo + bbo + 1) / 2;
	    }

	}

      /* and for green channel */
      if (abs (ga - OFFSET_TARGET) < OFFSET_MARGIN)
	{
	  tgo = go;
	  bgo = go;
	}
      else
	{
	  if (ga > OFFSET_TARGET)
	    {
	      tgo = go;
	      go = (tgo + bgo) / 2;
	    }
	  if (ga < OFFSET_TARGET)
	    {
	      bgo = go;
	      go = (tgo + bgo + 1) / 2;
	    }
	}
    }
  while ((tro != bro) || (tgo != bgo) || (tbo != bbo));

  /* store detected values in device struct */
  dev->red_offset = bro;
  dev->green_offset = bgo;
  dev->blue_offset = bbo;

  DBG (DBG_proc, "offset_calibration: exit\n");
  return status;
}

/*
 * do shading calibration
 * We scan a 637 pixels by 66 linesxoffset=24 , xend=661, pixels=637
y			 offset=1 , yend=67, lines =66
 */
#ifndef UNIT_TESTING
static 
#endif
SANE_Status
setup_shading_calibration (struct Rts8891_Device *dev, int mode, int *light, int *status1, SANE_Byte * regs)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int lines = 66;
  int exposure=0;
  int timing=0;

  DBG (DBG_proc, "setup_shading_calibration: start\n");
  DBG (DBG_info, "setup_shading_calibration: sensor type is %s (%d)\n", sensor_name (dev->sensor), dev->sensor);

  /* set up registers */
  /* 0x20/0x28 0x3b/0x3f seen in logs */
  *status1 = mode;
  if (dev->xdpi > 300)
    {
      *status1 |= 0x08;
    }

  /* we default to 75 dpi then override needed registers */
  timing=0x00b0;
  regs[0x32] = 0x20;
  regs[0x33] = 0x83;
  regs[0x35] = 0x0e;
  regs[0x36] = 0x2c;
  regs[0x3a] = 0x0e;

  regs[0xe2] = 0x05;
  regs[0xe7] = 0x00;
  regs[0xe8] = 0x00;
  regs[0xe9] = 0x00;
  regs[0xea] = 0x00;
  regs[0xeb] = 0x00;
  regs[0xec] = 0x00;
  regs[0xed] = 0x00;
  regs[0xef] = 0x00;
  regs[0xf0] = 0x00;
  regs[0xf2] = 0x00;
  /* regs[0xe5] = 0xdd; */

  exposure=221;
  if (dev->sensor == SENSOR_TYPE_XPA || dev->sensor == SENSOR_TYPE_4400)
    {
      regs[0xc0] = 0x67;
      regs[0xc1] = 0x06;
      regs[0xc2] = 0xe6;
      regs[0xc3] = 0x98;
      regs[0xc4] = 0xf9;
      regs[0xc5] = 0x19;
      regs[0xc6] = 0x67;
      regs[0xc7] = 0x06;
      regs[0xc8] = 0xe6;
      regs[0xc9] = 0x01;
      regs[0xca] = 0xf8;
      regs[0xcb] = 0xff;
      regs[0xcc] = 0x98;
      regs[0xcd] = 0xf9;
      regs[0xce] = 0x19;
      regs[0xcf] = 0xe0;
      regs[0xd0] = 0xe2;

      regs[0xd1] = 0xeb;
      regs[0xd2] = 0x0c;

      regs[0xd7] = 0x10;
    }
  if (dev->sensor == SENSOR_TYPE_4400)
    {
      *light &= 0xf7;		/* clear bit 3 */
      regs[0x36] = 0x29;	/* 0x2c */
      timing=0x0032;
      regs[0x85] = 0x00;	/* 0x8c */
      regs[0x86] = 0x06;	/* 0x10 */
      regs[0x87] = 0x00;	/* 0x18 */
      regs[0x88] = 0x06;	/* 0x1b */
      regs[0x8d] = 0x00;	/* 0x77 */
      /* c5,c6 ?? */
      /* regs[0xd3] = 0x02;         0x0e */
      regs[0xd4] = 0x04;	/* 0x10 */
      regs[0xe2] = 0x02;	/* 0x05 */
      /* regs[0xe5] = 0xbb;    
         regs[0xe6] = 0x01;    1bb =443 */
      exposure=443;
    }
  if (dev->sensor == SENSOR_TYPE_4400_BARE)
    {
      regs[0x13] = 0x39;	/* 0x20 */
      regs[0x14] = 0xf0;	/* 0xf8 */
      regs[0x15] = 0x29;	/* 0x28 */
      regs[0x16] = 0x0f;	/* 0x07 */
      regs[0x17] = 0x10;	/* 0x00 */
      regs[0x23] = 0x00;	/* 0xff */
      regs[0x36] = 0x29;	/* 0x2c */
      regs[0x39] = 0x00;	/* 0x02 */
      regs[0xe2] = 0x02;	/* 0x05 */
      exposure=443;
    }

  switch (dev->xdpi)
    {
    case 75:
      break;

    case 150:
      /* X resolution related registers */
      switch (dev->sensor)
	{
	case SENSOR_TYPE_XPA:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_XPA for 150 dpi\n");
          timing=0x022b;
	  regs[0x85] = 0x18;
	  regs[0x86] = 0x1b;
	  regs[0x87] = 0x30;
	  regs[0x88] = 0x30;
	  regs[0x8d] = 0xef;
	  regs[0xc0] = 0x00;
	  regs[0xc1] = 0x8e;
	  regs[0xc2] = 0xff;
	  regs[0xc3] = 0xff;
	  regs[0xc4] = 0x71;
	  regs[0xc5] = 0x00;
	  regs[0xc6] = 0x00;
	  regs[0xc7] = 0x8e;
	  regs[0xc8] = 0xff;
	  regs[0xc9] = 0xff;
	  regs[0xca] = 0xff;
	  regs[0xcb] = 0x1f;
	  regs[0xcc] = 0xff;
	  regs[0xcd] = 0x71;
	  regs[0xce] = 0x00;
	  regs[0xcf] = 0xe6;
	  regs[0xd0] = 0xe8;
	  regs[0xd1] = 0xf6;
	  regs[0xd2] = 0x17;
	  /* regs[0xd3] = 0x0b; */
	  regs[0xd4] = 0x0d;
	  /* regs[0xe5] = 0xe4; */
          exposure=228;
	  break;
	case SENSOR_TYPE_BARE:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_BARE for 150 dpi\n");
          timing=0x012e;
	  regs[0x85] = 0x8c;
	  regs[0x86] = 0x10;
	  regs[0x87] = 0x18;
	  regs[0x88] = 0x1b;
	  regs[0x8d] = 0x77;
	  regs[0xc0] = 0x80;
	  regs[0xc1] = 0x87;
	  regs[0xc2] = 0x7f;
	  regs[0xc9] = 0x00;
	  regs[0xcb] = 0x78;
	  regs[0xcc] = 0x7f;
	  regs[0xcd] = 0x78;
	  regs[0xce] = 0x80;
	  regs[0xcf] = 0xe6;
	  regs[0xd0] = 0xe8;

	  regs[0xd1] = 0xf7;
	  regs[0xd2] = 0x00;

	  /* regs[0xd3] = 0x0e; */
	  regs[0xd4] = 0x10;

	  /* regs[0xe5] = 0xe4; */
          exposure=228;
	  break;
	case SENSOR_TYPE_4400:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_4400 for 150 dpi\n");
          timing=0x012e;
	  regs[0x85] = 0x8c;
	  regs[0x86] = 0x10;
	  regs[0x87] = 0x18;
	  regs[0x88] = 0x1b;
	  regs[0x8d] = 0x77;
	  regs[0xc0] = 0x00;
	  regs[0xc1] = 0x8e;
	  regs[0xc2] = 0xff;
	  regs[0xc3] = 0xff;
	  regs[0xc4] = 0x71;
	  regs[0xc5] = 0x00;
	  regs[0xc6] = 0x00;
	  regs[0xc7] = 0x8e;
	  regs[0xc8] = 0xff;
	  regs[0xc9] = 0xff;
	  regs[0xca] = 0xff;
	  regs[0xcb] = 0x1f;
	  regs[0xcc] = 0xff;
	  regs[0xcd] = 0x71;
	  regs[0xce] = 0x00;
	  regs[0xcf] = 0xe6;
	  regs[0xd0] = 0xe8;
	  regs[0xd1] = 0xf6;
	  regs[0xd2] = 0x17;
	  /* regs[0xd3] = 0x0b; */
	  regs[0xd4] = 0x0d;
          exposure=457;
	  break;
	case SENSOR_TYPE_4400_BARE:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_4400_BARE for 150 dpi\n");
	  regs[0x13] = 0x39;	/* 0x20 */
	  regs[0x14] = 0xf0;	/* 0xf8 */
	  regs[0x15] = 0x29;	/* 0x28 */
	  regs[0x16] = 0x0f;	/* 0x07 */
	  regs[0x17] = 0x10;	/* 0x00 */
	  regs[0x23] = 0x00;	/* 0xff */
	  regs[0x36] = 0x29;	/* 0x2c */
	  regs[0x39] = 0x00;	/* 0x02 */
          timing=0x00b0;
	  regs[0x85] = 0x46;	/* 0x00 */
	  regs[0x86] = 0x0b;	/* 0x06 */
	  regs[0x87] = 0x8c;	/* 0x00 */
	  regs[0x88] = 0x10;	/* 0x06 */
	  regs[0x8d] = 0x3b;	/* 0x00 */
	  regs[0xc0] = 0xff;	/* 0x06 */
	  regs[0xc1] = 0x0f;	/* 0xe6 */
	  regs[0xc2] = 0x00;	/* 0x67 */
	  regs[0xc9] = 0x00;	/* 0x07 */
	  regs[0xca] = 0x0e;	/* 0x00 */
	  regs[0xcb] = 0x00;	/* 0xfe */
	  regs[0xcc] = 0x00;	/* 0xf9 */
	  regs[0xcd] = 0xf0;	/* 0x19 */
	  regs[0xce] = 0xff;	/* 0x98 */
	  regs[0xcf] = 0xf5;	/* 0xe8 */
	  regs[0xd0] = 0xf7;	/* 0xea */
	  regs[0xd1] = 0xea;	/* 0xf3 */
	  regs[0xd2] = 0x0b;	/* 0x14 */
	  /* regs[0xd3] = 0x17;     0x02 */
	  regs[0xd4] = 0x01;	/* 0x04 */
	  regs[0xe2] = 0x02;	/* 0x05 */
	  /* regs[0xe5] = 0x93;   regs[0xe6] = 0x03; */
          exposure=915;
	  break;
	}
      break;

    case 300:
      switch (dev->sensor)
	{
	case SENSOR_TYPE_XPA:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_XPA for 300 dpi\n");
          timing=0x00b0;
	  regs[0x85] = 0x46;
	  regs[0x86] = 0x0b;
	  regs[0x87] = 0x8c;
	  regs[0x88] = 0x10;
	  regs[0x8d] = 0x3b;
	  regs[0xc0] = 0x00;
	  regs[0xc1] = 0xff;
	  regs[0xc2] = 0x0f;
	  regs[0xc3] = 0xff;
	  regs[0xc4] = 0x00;
	  regs[0xc5] = 0xf0;
	  regs[0xc6] = 0x00;
	  regs[0xc7] = 0xff;
	  regs[0xc8] = 0x0f;
	  regs[0xc9] = 0xff;
	  regs[0xca] = 0xff;
	  regs[0xcb] = 0xf1;
	  regs[0xcc] = 0xff;
	  regs[0xcd] = 0x00;
	  regs[0xce] = 0xf0;
	  regs[0xcf] = 0xed;
	  regs[0xd0] = 0xef;
	  regs[0xd1] = 0xe2;
	  regs[0xd2] = 0x03;
	  /* regs[0xd3] = 0x17; */
	  regs[0xd4] = 0x01;
	  /* regs[0xe5] = 0xc9;
	     regs[0xe6] = 0x01;     0x01c9=457 */
          exposure=457;
	  break;
	case SENSOR_TYPE_BARE:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_BARE for 300 dpi\n");
          timing=0x012e;
	  regs[0x85] = 0x8c;
	  regs[0x86] = 0x10;
	  regs[0x87] = 0x18;
	  regs[0x88] = 0x1b;

	  regs[0x8d] = 0xde;
	  regs[0x8e] = 0x61;	/* low nibble of 8e and 8d are proportional to 
					   the scanned width 1de => 5100 wide scan */

	  regs[0xc0] = 0xff;
	  regs[0xc1] = 0x0f;
	  regs[0xc2] = 0x00;
	  regs[0xc9] = 0x00;
	  regs[0xca] = 0x0e;
	  regs[0xcb] = 0x00;
	  regs[0xcc] = 0x00;
	  regs[0xcd] = 0xf0;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xf5;
	  regs[0xd0] = 0xf7;

	  regs[0xd1] = 0xea;
	  regs[0xd2] = 0x0b;

	  /* regs[0xd3] = 0x17; */
	  regs[0xd4] = 0x01;
	  /* regs[0xe5] = 0xc9;
	     regs[0xe6] = 0x01;    0x1c9=457 */
          exposure=457;
	  break;
	case SENSOR_TYPE_4400:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_4400 for 300 dpi\n");
          timing=0x022b;
	  regs[0x85] = 0x18;
	  regs[0x86] = 0x1b;
	  regs[0x87] = 0x30;
	  regs[0x88] = 0x30;
	  regs[0x8d] = 0xef;
	  regs[0xc0] = 0x00;
	  regs[0xc1] = 0xff;
	  regs[0xc2] = 0x0f;
	  regs[0xc3] = 0xff;
	  regs[0xc4] = 0x00;
	  regs[0xc5] = 0xf0;
	  regs[0xc6] = 0x00;
	  regs[0xc7] = 0xff;
	  regs[0xc8] = 0x0f;
	  regs[0xc9] = 0xff;
	  regs[0xca] = 0xff;
	  regs[0xcb] = 0xf1;
	  regs[0xcc] = 0xff;
	  regs[0xcd] = 0x00;
	  regs[0xce] = 0xf0;
	  regs[0xcf] = 0xed;
	  regs[0xd0] = 0xef;
	  regs[0xd1] = 0xe2;
	  regs[0xd2] = 0x03;
	  /* regs[0xd3] = 0x17; */
	  regs[0xd4] = 0x01;
	  /* 0x0393 = 915 = 2*457+1 */
          exposure=915;
	  break;

	case SENSOR_TYPE_4400_BARE:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_4400_BARE for 300 dpi\n");
	  return SANE_STATUS_INVAL;
	  break;
	}
      break;

    case 600:
      switch (dev->sensor)
	{
	case SENSOR_TYPE_BARE:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_BARE for 600 dpi\n");
	  regs[0x34] = 0x10;

	  regs[0x72] = 0x3a;
	  regs[0x73] = 0x15;
	  regs[0x74] = 0x62;

          timing=0x022b;

	  regs[0x85] = 0x18;
	  regs[0x86] = 0x1b;

	  regs[0x87] = 0x30;
	  regs[0x88] = 0x30;

	  regs[0x8d] = 0xde;
	  regs[0x8e] = 0x61;	/* low nibble of 8e and 8d are proportional to 
					   the scanned width 1de => 5100 wide scan */
	  regs[0xc0] = 0xff;
	  regs[0xc1] = 0xff;
	  regs[0xc2] = 0xff;
	  regs[0xc3] = 0x00;
	  regs[0xc4] = 0xf0;
	  regs[0xc7] = 0x0f;
	  regs[0xc8] = 0x00;
	  regs[0xcb] = 0xe0;
	  regs[0xcc] = 0xff;
	  regs[0xcd] = 0xff;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xe9;
	  regs[0xd0] = 0xeb;

	  regs[0xd7] = 0x14;

	  /* regs[0xe5] = 0x93;
	     regs[0xe6] = 0x03;         0x393=915 = 457*2+1 */
          exposure=915;
	  break;

	case SENSOR_TYPE_XPA:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_XPA for 600 dpi\n");
          *light &= 0xfb;		/* clear bit 2 */
          regs[0x16] = 0x07;
	  regs[0x33] = 0x83;       /* 0x86 */
	  regs[0x34] = 0x10;
	  regs[0x50] = 0x00;       /* 0x18 */
	  regs[0x64] = 0x01;       /* 0x02 */
	  regs[0x65] = 0x20;       /* 0x10 */

	  regs[0x72] = 0x3a;
	  regs[0x73] = 0x15;
	  regs[0x74] = 0x62;

	  regs[0x85] = 0x00;
	  regs[0x86] = 0x06;

	  regs[0x87] = 0x00;
	  regs[0x88] = 0x06;

	  regs[0x8d] = 0x00; 
	  regs[0x8e] = 0x60; /* XXX STEF XXX */

	  regs[0xc0] = 0xf8;
	  regs[0xc1] = 0x7f;
	  regs[0xc2] = 0x00;
	  regs[0xc3] = 0xf8;
	  regs[0xc4] = 0x7f;
	  regs[0xc5] = 0x00;
	  regs[0xc6] = 0xf8;
	  regs[0xc7] = 0x7f;
	  regs[0xc8] = 0x00;
	  regs[0xc9] = 0xff;
	  regs[0xca] = 0x8f;
	  regs[0xcb] = 0xff;
	  regs[0xcc] = 0x07;
	  regs[0xcd] = 0x80;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xf2;
	  regs[0xd0] = 0xf4;
	  regs[0xd1] = 0xe7;
	  regs[0xd2] = 0x08;
	  regs[0xd3] = 0x0e;
	  regs[0xd4] = 0x10;
	  regs[0xd7] = 0x31;
          
          timing=0x0032;
          exposure=915;
	  break;

	case SENSOR_TYPE_4400:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_4400 for 600 dpi\n");
	  *light = 0x23;

	  regs[0x13] = 0x39;
	  regs[0x14] = 0xf0;
	  regs[0x15] = 0x29;
	  regs[0x16] = 0x0f;
	  regs[0x17] = 0x10;
	  regs[0x23] = 0x00;
	  regs[0x34] = 0xf0;
	  regs[0x36] = 0x29;
	  regs[0x39] = 0x00;
	  regs[0x72] = 0x3a;
	  regs[0x73] = 0x15;
	  regs[0x74] = 0x62;
          timing=0x0426;
	  regs[0x85] = 0x30;
	  regs[0x86] = 0x30;
	  regs[0x87] = 0x60;
	  regs[0x88] = 0x5a;
	  regs[0x8d] = 0xde;
	  regs[0x8e] = 0x61;
	  regs[0xc0] = 0xf8;
	  regs[0xc1] = 0x7f;
	  regs[0xc2] = 0x00;
	  regs[0xc3] = 0xf8;
	  regs[0xc4] = 0x7f;
	  regs[0xc5] = 0x00;
	  regs[0xc6] = 0xf8;
	  regs[0xc7] = 0x7f;
	  regs[0xc8] = 0x00;
	  regs[0xc9] = 0xff;
	  regs[0xca] = 0x8f;
	  regs[0xcb] = 0xff;
	  regs[0xcc] = 0x07;
	  regs[0xcd] = 0x80;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xf2;
	  regs[0xd0] = 0xf4;
	  regs[0xd1] = 0xe7;
	  regs[0xd2] = 0x08;
	  regs[0xd3] = 0x0e;
	  regs[0xd4] = 0x10;
	  regs[0xd7] = 0x31;
	  regs[0xe2] = 0x02;
          exposure=1832;

	  break;

	case SENSOR_TYPE_4400_BARE:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_4400_BARE for 600 dpi\n");
	  return SANE_STATUS_INVAL;
	  break;
	}
      break;

    case 1200:
      switch (dev->sensor)
	{
	case SENSOR_TYPE_BARE:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_BARE for 1200 dpi\n");
	  regs[0x34] = 0xf0;
	  regs[0x40] = 0xa0;

          timing=0x0426;

	  regs[0x85] = 0x30;
	  regs[0x86] = 0x30;

	  regs[0x87] = 0x60;
	  regs[0x88] = 0x5a;

	  regs[0x8d] = 0xbd;
	  regs[0x8e] = 0x63;

	  regs[0xc0] = 0xff;
	  regs[0xc1] = 0xff;
	  regs[0xc2] = 0xff;
	  regs[0xc4] = 0x0f;
	  regs[0xc5] = 0x00;
	  regs[0xc6] = 0x00;
	  regs[0xc7] = 0xf0;
	  regs[0xc9] = 0x00;
	  regs[0xca] = 0x0e;
	  regs[0xcb] = 0x00;
	  regs[0xcc] = 0xff;
	  regs[0xcd] = 0xff;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xf5;
	  regs[0xd0] = 0xf7;
	  regs[0xd1] = 0xea;
	  regs[0xd2] = 0x0b;
	  /* regs[0xd3] = 0x17; */
	  regs[0xd4] = 0xc1;
	  regs[0xd7] = 0x14;
	  regs[0xd8] = 0xa4;
	  /* regs[0xe5] = 0x28;
	     regs[0xe6] = 0x07;         0x728=1832=915*2+2 */
          exposure=1832;
	  break;
	case SENSOR_TYPE_XPA:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_XPA for 1200 dpi\n");
	  regs[0x34] = 0x10;
	  regs[0x40] = 0xa0;

          timing=0x00b0;
          exposure=1832;

          /* XXX STEF XXX */
	  regs[0x85] = 0x46;
	  regs[0x86] = 0x0b;

	  regs[0x87] = 0x8c;
	  regs[0x88] = 0x10;

	  regs[0x8e] = 0x3b;
	  regs[0x8d] = 0x60;

	  regs[0xc1] = 0xff;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xf5;
	  regs[0xd0] = 0xf7;
	  regs[0x33] = 0x83;
	  regs[0x50] = 0x00;
	  regs[0x64] = 0x01;
	  regs[0x65] = 0x20;
	  regs[0x8d] = 0xbc;
	  regs[0xc0] = 0xe0;
	  regs[0xc2] = 0x01;
	  regs[0xc3] = 0x1f;
	  regs[0xc4] = 0x00;
	  regs[0xc5] = 0xfe;
	  regs[0xc6] = 0xff;
	  regs[0xc7] = 0xff;
	  regs[0xc8] = 0x00;
	  regs[0xc9] = 0x3f;
	  regs[0xca] = 0xfe;
	  regs[0xcb] = 0xff;
	  regs[0xcc] = 0x00;
	  regs[0xcd] = 0x00;
	  regs[0xd1] = 0xec;
	  regs[0xd2] = 0x0d;
	  regs[0xd3] = 0x05;
	  regs[0xd4] = 0x67;
	  regs[0xd7] = 0x10;
	  regs[0xd8] = 0x52;
	  break;

	case SENSOR_TYPE_4400:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_4400 for 1200 dpi\n");
	  regs[0x13] = 0x39;
	  regs[0x14] = 0xf0;
	  regs[0x15] = 0x29;
	  regs[0x16] = 0x0f;
	  regs[0x17] = 0x10;
	  regs[0x23] = 0x00;
	  regs[0x34] = 0xf0;
	  regs[0x36] = 0x29;
	  regs[0x39] = 0x00;
	  regs[0x40] = 0xa0;
          timing=0x0426;
	  regs[0x85] = 0x30;
	  regs[0x86] = 0x30;
	  regs[0x87] = 0x60;
	  regs[0x88] = 0x5a;
	  regs[0x8d] = 0xde;
	  regs[0x8e] = 0x61;
	  regs[0xc0] = 0xe0;
	  regs[0xc1] = 0xff;
	  regs[0xc2] = 0x01;
	  regs[0xc3] = 0x1f;
	  regs[0xc4] = 0x00;
	  regs[0xc5] = 0xfe;
	  regs[0xc6] = 0xff;
	  regs[0xc7] = 0xff;
	  regs[0xc8] = 0x00;
	  regs[0xc9] = 0x3f;
	  regs[0xca] = 0xfe;
	  regs[0xcb] = 0xff;
	  regs[0xcc] = 0x00;
	  regs[0xcd] = 0x00;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xf5;
	  regs[0xd0] = 0xf7;
	  regs[0xd1] = 0xec;
	  regs[0xd2] = 0x0d;
	  /* regs[0xd3] = 0x05; */
	  regs[0xd4] = 0x67;
	  regs[0xd7] = 0x10;
	  regs[0xd8] = 0x52;
	  regs[0xe2] = 0x02;
	  *light = 0x23;
          exposure=3665;
	  break;

	case SENSOR_TYPE_4400_BARE:
	  DBG (DBG_io,
	       "setup_shading_calibration: setting up SENSOR_TYPE_4400_BARE for 1200 dpi\n");
	  return SANE_STATUS_INVAL;
	  break;
	}
      break;
    }
 
  /* apply computed settings */ 
  SET_DOUBLE (regs, EXPOSURE_REG, exposure);
  SET_DOUBLE (regs, TIMING_REG, timing);
  SET_DOUBLE (regs, TIMING1_REG, timing+1);
  SET_DOUBLE (regs, TIMING2_REG, timing+2);

  /* in logs, the driver use the computed offset minus 2 */
  sanei_rts88xx_set_offset (regs, dev->red_offset, dev->green_offset, dev->blue_offset);
  sanei_rts88xx_set_gain (regs, dev->red_gain, dev->green_gain, dev->blue_gain);
  sanei_rts88xx_set_scan_area (regs, 1, 1 + lines, dev->xstart, dev->xstart + dev->pixels);

  DBG (DBG_proc, "setup_shading_calibration: exit\n");
  return status;
}

/*
 * do shading calibration
 * We scan a 637 pixels by 66 linesxoffset=24 , xend=661, pixels=637
y			 offset=1 , yend=67, lines =66
 */
static SANE_Status
shading_calibration (struct Rts8891_Device *dev, SANE_Bool color, int mode, int light)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int width;
  int lines = 66;
  SANE_Byte format;
  int size;
  int status1;
  int x, y, sum;
  SANE_Byte *image = NULL;
  FILE *dbg = NULL;

  DBG (DBG_proc, "shading_calibration: start\n");
  DBG (DBG_info, "shading_calibration: sensor type is %s (%d)\n",
       sensor_name (dev->sensor), dev->sensor);

  width = dev->pixels;
  size = lines * dev->bytes_per_line;

  image = (SANE_Byte *) malloc (size);
  if (image == NULL)
    {
      DBG (DBG_error,
	   "shading_calibration: failed to allocate memory for image\n");
      return SANE_STATUS_NO_MEM;
    }
  if (dev->shading_data != NULL)
    free (dev->shading_data);
  dev->shading_data = (unsigned char *) malloc (dev->bytes_per_line);
  if (dev->shading_data == NULL)
    {
      free (image);
      DBG (DBG_error,
	   "shading_calibration: failed to allocate memory for data\n");
      return SANE_STATUS_NO_MEM;
    }
 
  /* set up registers */ 
  status=setup_shading_calibration(dev, mode, &light, &status1, dev->regs);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "shading_calibration: failed to set up registers\n");
      free (dev->shading_data);
      dev->shading_data = NULL;
      free (image);
      return status;
    }

  /* scan shading area */
  sanei_rts88xx_set_status (dev->devnum, dev->regs, status1, light);
  format = rts8891_data_format (dev->xdpi, dev->sensor);
  status = rts8891_simple_scan (dev->devnum, dev->regs, dev->reg_count, format, size, image);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "shading_calibration: failed scan shading area\n");
      free (dev->shading_data);
      dev->shading_data = NULL;
      free (image);
      return status;
    }

  if (DBG_LEVEL >= DBG_io2)
    {
      dbg = fopen ("shading.pnm", "wb");
      if (color)
	{
	  fprintf (dbg, "P6\n%d %d\n255\n", width, lines);
	  fwrite (image, width * 3, lines, dbg);
	}
      else
	{
	  fprintf (dbg, "P5\n%d %d\n255\n", width, lines);
	  fwrite (image, width, lines, dbg);
	}
      fclose (dbg);
    }

  /* now we can compute shading calibration data */
  /* we average each row */
  /* we take the maximum of each row */
  /* we skip first 3 lines and last 10 lines due to some bugs */
  if (color)
    {
      width = width * 3;
    }
  for (x = 0; x < width; x++)
    {
      sum = 0;
      for (y = 3; y < lines - 10; y++)
	{
	  sum += image[x + y * width];
	}
      dev->shading_data[x] = sum / (lines - 13);
    }
  if (DBG_LEVEL >= DBG_io2)
    {
      dbg = fopen ("shading_data.pnm", "wb");
      if (color)
	{
	  fprintf (dbg, "P6\n%d %d\n255\n", width / 3, 1);
	  fwrite (dev->shading_data, width, 1, dbg);
	}
      else
	{
	  fprintf (dbg, "P5\n%d %d\n255\n", width, 1);
	  fwrite (dev->shading_data, width, 1, dbg);
	}
      fclose (dbg);
    }

  free (image);
  DBG (DBG_proc, "shading_calibration: exit\n");
  return status;
}

static void
fill_gamma (SANE_Byte * calibration, int *idx, SANE_Word * gamma)
{
  int i;

  calibration[*idx] = 0;
  (*idx)++;
  for (i = 0; i < 255; i++)
    {
      calibration[*idx] = gamma[i];
      /* escape 0xaa with 0 */
      if (calibration[*idx] == 0xaa)
	{
	  (*idx)++;
	  calibration[*idx] = 0;
	}
      (*idx)++;
      calibration[*idx] = gamma[i];
      /* escape 0xaa with 0 */
      if (calibration[*idx] == 0xaa)
	{
	  (*idx)++;
	  calibration[*idx] = 0;
	}
      (*idx)++;
    }
  calibration[*idx] = 0xff;
  (*idx)++;
}

/*
 * build and send calibration data which contains gamma table and
 * shading correction coefficient
 * 	
 */
static SANE_Status
send_calibration_data (struct Rts8891_Session *session)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int size, width, data_size;
  SANE_Byte *calibration = NULL, format, val;
  struct Rts8891_Device *dev = session->dev;
  int i, idx;
  unsigned int value, red_code, blue_code, green_code;
  FILE *calib = NULL;
  SANE_Word *gamma_r, *gamma_g, *gamma_b;

  DBG (DBG_proc, "send_calibration_data: start\n");

  /* 675 pixels at 75 DPI, 16 bits values, 3 color channels */
  /* 5400 pixels at max sensor 600 dpi                      */
  /* 3 16bits 256 value gamma tables plus start/end markers */
  /* must multple of 32 */
  data_size = (675 * dev->xdpi) / 75;

  width = dev->pixels;

  /* effective data calibration size */
  size = data_size * 2 * 3 + 3 * (512 + 2);
  size = ((size + 31) / 32) * 32;

  DBG (DBG_io, "send_calibration_data: size=%d\n", size);

  /* 
   * FORMAT:
   * 00 
   * 512 bytes gamma table (256 16 bit entry)
   * FF 
   * 00 
   * 512 bytes gamma table (256 16 bit entry)
   * FF 
   * 00 
   * 512 bytes gamma table (256 16 bit entry)
   * FF 
   * 5400 max shading coefficients at 600 dpi repeated 3 times
   * overall size rounded at 32 bytes multiple
   * 675 CCD elements at 75 DPI. 16 bit per element. 1 or 3 channels.
   * there is a 0xFF marker at end of each coefficients row.
   * a gamma table comes first 
   * 75  DPI:  5600=1542+(675)*2*3+8                ->size rounded to 'upper 32'
   * 150 DPI: 9664=675*2*2*3=1542+(675*2)*2*3+22
   * 17760                                        4         +18
   * 33952                                        8     +10
   * 65472+896=66368=                16     +26
   * 
   * COEFFICIENT 16 bit value
   * first is 00, 0x40, 0x80 or 0xC0 => 10 significant bit
   * coeff*average=K  => coeff=K/average
   */

  calibration = (SANE_Byte *) malloc (size);
  if (calibration == NULL)
    {
      DBG (DBG_error,
	   "send_calibration_data: failed to allocate memory for calibration data\n");
      return SANE_STATUS_NO_MEM;
    }
  memset (calibration, 0x00, size);

  /* fill gamma tables first, markers and static gamma table */
  idx = 0;

  /* select red gamma table */
  if (session->params.format == SANE_FRAME_RGB)
    {
      /* 3 different gamma table */
      gamma_r = session->val[OPT_GAMMA_VECTOR_R].wa;
      gamma_g = session->val[OPT_GAMMA_VECTOR_G].wa;
      gamma_b = session->val[OPT_GAMMA_VECTOR_B].wa;
    }
  else
    {
      /* 3 time the same gamma table */
      gamma_r = session->val[OPT_GAMMA_VECTOR].wa;
      gamma_g = session->val[OPT_GAMMA_VECTOR].wa;
      gamma_b = session->val[OPT_GAMMA_VECTOR].wa;
    }

  fill_gamma (calibration, &idx, gamma_r);
  fill_gamma (calibration, &idx, gamma_g);
  fill_gamma (calibration, &idx, gamma_b);

  /* compute calibration coefficients */
  /* real witdh != 675 --> 637 
   * shading data calibration starts at 1542. There are 3 rows of 16 bits values
   * first row is green calibration
   */

  /* to avoid problems with 0xaa values which must be escaped, we change them
   * into 0xab values, which unnoticeable on scans */
  for (i = 0; i < width; i++)
    {
      /* average TARGET CODE 3431046 */
/* #define RED_SHADING_TARGET_CODE   3000000
   #define GREEN_SHADING_TARGET_CODE 300000
   #define BLUE_SHADING_TARGET_CODE  2800000*/
#define RED_SHADING_TARGET_CODE   2800000
#define GREEN_SHADING_TARGET_CODE 2800000
#define BLUE_SHADING_TARGET_CODE  2700000

      red_code = RED_SHADING_TARGET_CODE;
      green_code = GREEN_SHADING_TARGET_CODE;
      blue_code = BLUE_SHADING_TARGET_CODE;

      /* target code debug, will be removed for the release */
      if (getenv ("RED_CODE") != NULL)
	{
	  red_code = atoi (getenv ("RED_CODE"));
	}
      if (getenv ("GREEN_CODE") != NULL)
	{
	  blue_code = atoi (getenv ("GREEN_CODE"));
	}
      if (getenv ("BLUE_CODE") != NULL)
	{
	  green_code = atoi (getenv ("BLUE_CODE"));
	}

      /* correction coefficient is target code divided by average scanned value 
       * but it is put in a 16 bits number. Only 10 first bits are significants.
       */
      /* first color component red data  */
      if (gamma_r[dev->shading_data[i * 3]] < 5)
	value = 0x8000;
      else
	value = red_code / gamma_r[dev->shading_data[i * 3]];
      val = (SANE_Byte) (value / 256);
      if (val == 0xaa)
	val++;
      calibration[idx + i * 2 + 1] = val;
      calibration[idx + i * 2] = (SANE_Byte) (value % 256) & 0xC0;

      /* second color component: green data */
      if (gamma_r[dev->shading_data[i * 3 + 1]] < 5)
	value = 0x8000;
      else
	value = green_code / gamma_g[dev->shading_data[i * 3 + 1]];
      val = (SANE_Byte) (value / 256);
      if (val == 0xaa)
	val++;
      calibration[idx + data_size * 2 + i * 2 + 1] = val;
      calibration[idx + data_size * 2 + i * 2] =
	(SANE_Byte) (value % 256) & 0xC0;

      /* third color component: blue data */
      if (gamma_r[dev->shading_data[i * 3 + 2]] < 5)
	value = 0x8000;
      else
	value = blue_code / gamma_b[dev->shading_data[i * 3 + 2]];
      val = (SANE_Byte) (value / 256);
      if (val == 0xaa)
	val++;
      calibration[idx + data_size * 4 + i * 2 + 1] = val;
      calibration[idx + data_size * 4 + i * 2] =
	(SANE_Byte) (value % 256) & 0xC0;
    }

  /* DEEP TRACING */
  if (DBG_LEVEL >= DBG_io2)
    {
      calib = fopen ("calibration.hex", "wb");
      fprintf (calib, "shading_data(%d)=", width);
      for (i = 0; i < width * 3; i++)
	{
	  fprintf (calib, "%02x ", dev->shading_data[i]);
	}
      fprintf (calib, "\n");
      fprintf (calib, "write_mem(0x00,%d)=", size);
      for (i = 0; i < size; i++)
	{
	  fprintf (calib, "%02x ", calibration[i]);
	}
      fclose (calib);
    }

  /* signals color format/divisor from hardware */
  format = rts8891_data_format (dev->xdpi, dev->sensor);
  status = sanei_rts88xx_write_reg (dev->devnum, 0xd3, &format);

  /* for some reason, we have to add 6 to the size for the first write */
  /* related to the 6 0xaa in gamma table ? */
  if (size > RTS88XX_MAX_XFER_SIZE)
    {
      status =
	sanei_rts88xx_write_mem (dev->devnum, RTS88XX_MAX_XFER_SIZE, 6,
				 calibration);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "send_calibration_data: failed to write calibration data (part 1)\n");
	  return status;
	}
      size -= RTS88XX_MAX_XFER_SIZE;
      status =
	sanei_rts88xx_write_mem (dev->devnum, size, 0,
				 calibration + RTS88XX_MAX_XFER_SIZE);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "send_calibration_data: failed to write calibration data (part 2)\n");
	  return status;
	}
    }
  else
    {
      status = sanei_rts88xx_write_mem (dev->devnum, size, 6, calibration);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "send_calibration_data: failed to write calibration data\n");
	  return status;
	}
    }

  /* set mem start */
  dev->regs[0x91] = 0x00;
  dev->regs[0x92] = 0x00;
  sanei_rts88xx_write_regs (dev->devnum, 0x91, dev->regs + 0x91, 2);

  free (calibration);
  DBG (DBG_proc, "send_calibration_data: exit\n");
  return status;
}

/* move at dev->model->min_ydpi dpi up to the scanning area. Which speeds
 * up scanning 
 */
static SANE_Status
move_to_scan_area (struct Rts8891_Session *session)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte control;
  SANE_Byte regs[RTS8891_MAX_REGISTERS];
  struct Rts8891_Device *dev = session->dev;
  SANE_Int distance;

  DBG (DBG_proc, "move_to_scan_area: start\n");

  /* compute line number to move and fix scan values */
  distance = ((dev->ystart - 1) * MOVE_DPI) / dev->ydpi;
  dev->ystart = dev->ystart - (distance * dev->ydpi) / MOVE_DPI;
  /* extra lines to let head stop */
  distance -= 30;

  DBG (DBG_proc, "move_to_scan_area: distance=%d, ystart=%d\n", distance,
       dev->ystart);

  /* then send move command */
  rts8891_move (dev, regs, distance, SANE_TRUE);

  /* wait for completion */
  do
    {
      sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
    }
  while ((control & 0x08) == 0x08);

  DBG (DBG_proc, "move_to_scan_area: exit\n");
  return status;
}

/* set up the shadow registers for scan, depending on scan parameters    */
/* the ultimate goal is to have no direct access to registers, but to    */
/* set them through helper functions                                     */
/* NOTE : I couldn't manage to get scans that really uses gray settings. */
/* The windows driver is allways scanning in color, so we do the same.   */
/* For now, the only mode that could be done would be 300 dpi gray scan, */
/* based on the register settings of find_origin()                       */
#ifndef UNIT_TESTING
static
#endif
SANE_Status
setup_scan_registers (struct Rts8891_Session *session, SANE_Byte *status1, SANE_Byte *status2, SANE_Byte *regs)
{
  SANE_Status status = SANE_STATUS_GOOD;
  struct Rts8891_Device *dev = session->dev;
  int exposure=0;
  int timing=0;

  /* only software gray modes for now */
  if (session->params.format == SANE_FRAME_GRAY
      && (session->dev->model->flags & RTS8891_FLAG_EMULATED_GRAY_MODE) == 0)
    {
      DBG (DBG_warn,
	   "setup_scan_registers: native gray modes not implemented for this model, failure expected\n");
    }
  sanei_rts88xx_set_scan_area (regs, dev->ystart, dev->ystart + dev->lines, dev->xstart, dev->xstart + dev->pixels);
  DBG (DBG_info, "setup_scan_registers: xstart=%d, pixels=%d\n", dev->xstart, dev->pixels);
  DBG (DBG_info, "setup_scan_registers: ystart=%d, lines =%d\n", dev->ystart, dev->lines);

  /* this is full register set from a color preview */
  regs[0x00] = regs[0x00] & 0xef;
  regs[0x01] = 0x41;

  sanei_rts88xx_set_offset (regs, dev->red_offset, dev->green_offset, dev->blue_offset);
  sanei_rts88xx_set_gain (regs, dev->red_gain, dev->green_gain, dev->blue_gain);

  switch (dev->sensor)
    {
    case SENSOR_TYPE_4400:
    case SENSOR_TYPE_4400_BARE:
      *status1 = 0x10;
      *status2 = 0x2a;
      break;
    case SENSOR_TYPE_XPA:
      *status1 = 0x20;
      *status2 = 0x3f;
      break;
    default:
      *status1 = 0x20;
      *status2 = 0x3b;
    }

  /* default to 75 dpi color scan */
  regs[0x0b] = 0x70;
  regs[0x0c] = 0x00;
  regs[0x0d] = 0x00;
  regs[0x0e] = 0x00;
  regs[0x0f] = 0x00;

  regs[0x12] = 0xff;
  regs[0x13] = 0x20;
  regs[0x14] = 0xf8;
  regs[0x15] = 0x28;
  regs[0x16] = 0x01;
  regs[0x17] = 0x00;
  regs[0x18] = 0xff;
  regs[0x19] = 0x00;
  regs[0x1a] = 0x00;
  regs[0x1b] = 0x00;
  regs[0x1c] = 0x00;
  regs[0x1d] = 0x20;
  regs[0x1e] = 0x00;
  regs[0x1f] = 0x00;

  /* LCD display */
  regs[0x20] = 0x3a;
  regs[0x21] = 0xf2;
  regs[0x22] = 0x00;

  regs[0x23] = 0x80;
  regs[0x24] = 0xff;
  regs[0x25] = 0x00;
  regs[0x26] = 0x00;
  regs[0x27] = 0x00;
  regs[0x28] = 0x00;
  regs[0x29] = 0x00;
  regs[0x2a] = 0x00;
  regs[0x2b] = 0x00;
  regs[0x2c] = 0x00;
  regs[0x2d] = 0x00;
  regs[0x2e] = 0x00;
  regs[0x2f] = 0x00;
  regs[0x30] = 0x00;
  regs[0x31] = 0x00;
  regs[0x32] = 0x20;
  regs[0x33] = 0x83;
  regs[0x34] = 0x10;
  regs[0x35] = 0x47;
  regs[0x36] = 0x2c;
  regs[0x37] = 0x00;
  regs[0x38] = 0x00;
  regs[0x39] = 0x02;
  regs[0x3a] = 0x43;
  regs[0x3b] = 0x00;
  regs[0x3c] = 0x00;
  regs[0x3d] = 0x00;
  regs[0x3e] = 0x00;
  regs[0x3f] = 0x00;
  regs[0x40] = 0x2c;	/* 0x0c -> use shading data */
  regs[0x41] = 0x00;
  regs[0x42] = 0x00;
  regs[0x43] = 0x00;
  regs[0x44] = 0x8c;
  regs[0x45] = 0x76;
  regs[0x46] = 0x00;
  regs[0x47] = 0x00;
  regs[0x48] = 0x00;
  regs[0x49] = 0x00;
  regs[0x4a] = 0x00;
  regs[0x4b] = 0x00;
  regs[0x4c] = 0x00;
  regs[0x4d] = 0x00;
  regs[0x4e] = 0x00;
  regs[0x4f] = 0x00;
  regs[0x50] = 0x00;
  regs[0x51] = 0x00;
  regs[0x52] = 0x00;
  regs[0x53] = 0x00;
  regs[0x54] = 0x00;
  regs[0x55] = 0x00;
  regs[0x56] = 0x00;
  regs[0x57] = 0x00;
  regs[0x58] = 0x00;
  regs[0x59] = 0x00;
  regs[0x5a] = 0x00;
  regs[0x5b] = 0x00;
  regs[0x5c] = 0x00;
  regs[0x5d] = 0x00;
  regs[0x5e] = 0x00;
  regs[0x5f] = 0x00;

  regs[0x64] = 0x01;
  regs[0x65] = 0x20;

  regs[0x68] = 0x00;
  regs[0x69] = 0x00;
  regs[0x6a] = 0x00;
  regs[0x6b] = 0x00;

  regs[0x6e] = 0x00;
  regs[0x6f] = 0x00;
  regs[0x70] = 0x00;
  regs[0x71] = 0x00;

  regs[0x72] = 0xe1;
  regs[0x73] = 0x14;
  regs[0x74] = 0x18;
  regs[0x75] = 0x15;

  regs[0x76] = 0x00;
  regs[0x77] = 0x00;
  regs[0x78] = 0x00;
  regs[0x79] = 0x20;
  regs[0x7a] = 0x01;
  regs[0x7b] = 0x00;
  regs[0x7c] = 0x00;
  regs[0x7d] = 0x00;
  regs[0x7e] = 0x00;
  regs[0x7f] = 0x00;
  timing=0x00af;
  regs[0x84] = 0x00;
  regs[0x85] = 0x46;
  regs[0x86] = 0x0b;
  regs[0x87] = 0x8c;
  regs[0x88] = 0x10;
  regs[0x8b] = 0xff;
  regs[0x8c] = 0x3f;
  regs[0x8d] = 0x3b;

  regs[0x8e] = 0x60;
  regs[0x8f] = 0x00;
  regs[0x90] = 0x18;	/* 0x1c when shading calibration */

  /* overwritten when calibration data is sent */
  regs[0x91] = 0x00;
  regs[0x92] = 0x00;

  regs[0x93] = 0x01;

  regs[0x94] = 0x0e;
  regs[0x95] = 0x00;
  regs[0x96] = 0x00;
  regs[0x97] = 0x00;
  regs[0x98] = 0x00;
  regs[0x99] = 0x00;
  regs[0x9a] = 0x00;
  regs[0x9b] = 0x00;
  regs[0x9c] = 0x00;
  regs[0x9d] = 0x00;
  regs[0x9e] = 0x00;
  regs[0x9f] = 0x00;
  regs[0xa0] = 0x00;
  regs[0xa1] = 0x00;
  regs[0xa2] = 0x00;
  regs[0xa3] = 0xcc;
  regs[0xa4] = 0x27;
  regs[0xa5] = 0x64;
  regs[0xa6] = 0x00;
  regs[0xa7] = 0x00;
  regs[0xa8] = 0x00;
  regs[0xa9] = 0x00;
  regs[0xaa] = 0x00;
  regs[0xab] = 0x00;
  regs[0xac] = 0x00;
  regs[0xad] = 0x00;
  regs[0xae] = 0x00;
  regs[0xaf] = 0x00;
  regs[0xb0] = 0x00;
  regs[0xb1] = 0x00;
  regs[0xb2] = 0x02;

  regs[0xb4] = 0x00;
  regs[0xb5] = 0x00;
  regs[0xb6] = 0x00;
  regs[0xb7] = 0x00;
  regs[0xb8] = 0x00;
  regs[0xb9] = 0x00;
  regs[0xba] = 0x00;
  regs[0xbb] = 0x00;
  regs[0xbc] = 0x00;
  regs[0xbd] = 0x00;
  regs[0xbe] = 0x00;
  regs[0xbf] = 0x00;
  regs[0xc0] = 0x06;
  regs[0xc1] = 0xe6;
  regs[0xc2] = 0x67;
  regs[0xc3] = 0xff;
  regs[0xc4] = 0xff;
  regs[0xc5] = 0xff;
  regs[0xc6] = 0xff;
  regs[0xc7] = 0xff;
  regs[0xc8] = 0xff;
  regs[0xc9] = 0x07;
  regs[0xca] = 0x00;
  regs[0xcb] = 0xfe;
  regs[0xcc] = 0xf9;
  regs[0xcd] = 0x19;
  regs[0xce] = 0x98;
  regs[0xcf] = 0xe8;
  regs[0xd0] = 0xea;

  regs[0xd1] = 0xf3;
  regs[0xd2] = 0x14;

  regs[0xd3] = 0x02;
  regs[0xd4] = 0x04;
  regs[0xd5] = 0x86;

  regs[0xd6] = 0x0f;
  regs[0xd7] = 0x30;	/* 12303 */

  regs[0xd8] = 0x52;	/* 0x5230=21040 */

  regs[0xd9] = 0xad;
  regs[0xda] = 0xa7;
  regs[0xdb] = 0x00;
  regs[0xdc] = 0x00;
  regs[0xdd] = 0x00;
  regs[0xde] = 0x00;
  regs[0xdf] = 0x00;
  regs[0xe0] = 0x00;
  regs[0xe1] = 0x00;
  regs[0xe2] = 0x0f;
  regs[0xe3] = 0x85;
  regs[0xe4] = 0x03;

  /* regs[0xe5] = 0x52;
     regs[0xe6] = 0x00;     exposure time 0x0052=82 */
  exposure=82;

  regs[0xe7] = 0x75;
  regs[0xe8] = 0x01;
  regs[0xe9] = 0x0b;
  regs[0xea] = 0x54;
  regs[0xeb] = 0x01;
  regs[0xec] = 0x04;
  regs[0xed] = 0xb8;
  regs[0xee] = 0x00;
  regs[0xef] = 0x03;
  regs[0xf0] = 0x70;
  regs[0xf1] = 0x00;
  regs[0xf2] = 0x01;
  regs[0xf3] = 0x00;
  if (dev->sensor == SENSOR_TYPE_XPA || dev->sensor == SENSOR_TYPE_4400)
    {
      regs[0xc0] = 0x67;
      regs[0xc1] = 0x06;
      regs[0xc2] = 0xe6;
      regs[0xc3] = 0x98;
      regs[0xc4] = 0xf9;
      regs[0xc5] = 0x19;
      regs[0xc6] = 0x67;
      regs[0xc7] = 0x06;
      regs[0xc8] = 0xe6;
      regs[0xc9] = 0x01;
      regs[0xca] = 0xf8;
      regs[0xcb] = 0xff;
      regs[0xcc] = 0x98;
      regs[0xcd] = 0xf9;
      regs[0xce] = 0x19;
      regs[0xcf] = 0xe0;
      regs[0xd0] = 0xe2;

      regs[0xd1] = 0xeb;
      regs[0xd2] = 0x0c;

      regs[0xd7] = 0x10;
    }
  if (dev->sensor == SENSOR_TYPE_4400)
    {
      regs[0x13] = 0x39;	/* 0x20 */
      regs[0x14] = 0xf0;	/* 0xf8 */
      regs[0x15] = 0x29;	/* 0x28 */
      regs[0x16] = 0x00;	/* 0x01 */
      regs[0x17] = 0x10;	/* 0x00 */
      regs[0x23] = 0x00;	/* 0x80 */
      regs[0x35] = 0x47;	/* 0x45 */
      regs[0x36] = 0x29;	/* 0x2c */
      regs[0x39] = 0x00;	/* 0x02 */
      timing=0x00af;
      regs[0x85] = 0x46;	/* 0x8c */
      regs[0x86] = 0x0b;	/* 0x10 */
      regs[0x87] = 0x8c;	/* 0x18 */
      regs[0x88] = 0x10;	/* 0x1b */
      regs[0x8d] = 0x3b;	/* 0x77 */

      regs[0xd3] = 0x02;	/* 0x0e */
      regs[0xd4] = 0x04;	/* 0x10 */
      regs[0xe2] = 0x07;	/* 0x0f */
      regs[0xe3] = 0x84;	/* 0x87 */
      /*regs[0xe5] = 0xa5; 0x54 */
      exposure=165;
      regs[0xe7] = 0x0e;	/* 0xa8 */
      regs[0xe8] = 0x01;	/* 0x00 */
      regs[0xe9] = 0x0a;	/* 0x0b */
      regs[0xea] = 0xc2;	/* 0x56 */
      regs[0xed] = 0xf6;	/* 0xba */
      regs[0xef] = 0x02;	/* 0x03 */
      regs[0xf0] = 0xa8;	/* 0x72 */
    }
  if (dev->sensor == SENSOR_TYPE_4400_BARE)
    {
      regs[0x13] = 0x39;	/* 0x20 */
      regs[0x14] = 0xf0;	/* 0xf8 */
      regs[0x15] = 0x29;	/* 0x28 */
      regs[0x16] = 0x00;	/* 0x07 */
      regs[0x17] = 0x10;	/* 0x00 */
      regs[0x23] = 0x00;	/* 0xff */
      regs[0x35] = 0x47;	/* 0x0e */
      regs[0x36] = 0x29;	/* 0x2c */
      regs[0x39] = 0x00;	/* 0x02 */
      regs[0x3a] = 0x43;	/* 0x0e */
      regs[0x40] = 0x2c;	/* 0x20 */
      regs[0x85] = 0x46;	/* 0x00 */
      regs[0x86] = 0x0b;	/* 0x06 */
      regs[0x87] = 0x8c;	/* 0x00 */
      regs[0x88] = 0x10;	/* 0x06 */
      regs[0x8d] = 0x3b;	/* 0x00 */
      regs[0x90] = 0x18;	/* 0x1c */
      regs[0xe2] = 0x07;	/* 0x05 */
      regs[0xe3] = 0x84;	/* 0x00 */
      regs[0xe4] = 0x03;	/* 0x00 */
      timing=0x00af;
      exposure=165;
      regs[0xe7] = 0x0e;	/* 0x00 */
      regs[0xe8] = 0x01;	/* 0x00 */
      regs[0xe9] = 0x0a;	/* 0x00 */
      regs[0xea] = 0xc2;	/* 0x00 */
      regs[0xeb] = 0x01;	/* 0x00 */
      regs[0xec] = 0x04;	/* 0x00 */
      regs[0xed] = 0xf6;	/* 0x00 */
      regs[0xef] = 0x02;	/* 0x00 */
      regs[0xf0] = 0xa8;	/* 0x00 */
      regs[0xf2] = 0x01;	/* 0x00 */
    }
  switch (dev->xdpi)
    {
    case 75:
      break;
    case 150:
      regs[0x35] = 0x45;


      regs[0x85] = 0x8c;
      regs[0x86] = 0x10;

      regs[0x87] = 0x18;
      regs[0x88] = 0x1b;

      regs[0x8d] = 0x77;

      regs[0xe3] = 0x87;

      /* regs[0xe5] = 0x54;
         regs[0xe6] = 0x00;        exposure time 0x0054=84 */
      exposure=84;
      timing=0x012e;

      regs[0xe7] = 0xa8;
      regs[0xe8] = 0x00;
      regs[0xea] = 0x56;
      regs[0xed] = 0xba;

      regs[0xf0] = 0x72;
      switch (dev->sensor)
	{
	case SENSOR_TYPE_XPA:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_XPA for 150 dpi\n");
	  regs[0xc0] = 0x00;
	  regs[0xc1] = 0x8e;
	  regs[0xc2] = 0xff;
	  regs[0xc3] = 0xff;
	  regs[0xc4] = 0x71;
	  regs[0xc5] = 0x00;
	  regs[0xc6] = 0x00;
	  regs[0xc7] = 0x8e;
	  regs[0xc8] = 0xff;
	  regs[0xc9] = 0xff;
	  regs[0xca] = 0xff;
	  regs[0xcb] = 0x1f;
	  regs[0xcc] = 0xff;
	  regs[0xcd] = 0x71;
	  regs[0xce] = 0x00;
	  regs[0xcf] = 0xe6;
	  regs[0xd0] = 0xe8;
	  regs[0xd1] = 0xf6;
	  regs[0xd2] = 0x17;
	  regs[0xd3] = 0x0b;
	  regs[0xd4] = 0x0d;
	  break;
	case SENSOR_TYPE_BARE:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_BARE for 150 dpi\n");
	  regs[0xc0] = 0x80;
	  regs[0xc1] = 0x87;
	  regs[0xc2] = 0x7f;
	  regs[0xc9] = 0x00;
	  regs[0xcb] = 0x78;
	  regs[0xcc] = 0x7f;
	  regs[0xcd] = 0x78;
	  regs[0xce] = 0x80;
	  regs[0xcf] = 0xe6;
	  regs[0xd0] = 0xe8;

	  regs[0xd1] = 0xf7;
	  regs[0xd2] = 0x00;

	  regs[0xd3] = 0x0e;
	  regs[0xd4] = 0x10;
	  break;
	case SENSOR_TYPE_4400:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_4400 for 150 dpi\n");
	  regs[0x35] = 0x47;

          exposure=170;
          timing=0x012e;
	  regs[0x85] = 0x8c;
	  regs[0x86] = 0x10;
	  regs[0x87] = 0x18;
	  regs[0x88] = 0x1b;
	  regs[0x8d] = 0x77;
	  regs[0xc0] = 0x00;
	  regs[0xc1] = 0x8e;
	  regs[0xc2] = 0xff;
	  regs[0xc3] = 0xff;
	  regs[0xc4] = 0x71;
	  regs[0xc5] = 0x00;
	  regs[0xc6] = 0x00;
	  regs[0xc7] = 0x8e;
	  regs[0xc8] = 0xff;
	  regs[0xc9] = 0xff;
	  regs[0xca] = 0xff;
	  regs[0xcb] = 0x1f;
	  regs[0xcc] = 0xff;
	  regs[0xcd] = 0x71;
	  regs[0xce] = 0x00;
	  regs[0xcf] = 0xe6;
	  regs[0xd0] = 0xe8;
	  regs[0xd1] = 0xf6;
	  regs[0xd2] = 0x17;
	  regs[0xd3] = 0x0b;
	  regs[0xd4] = 0x0d;
	  regs[0xe3] = 0x86;
	  regs[0xe7] = 0x00;
	  regs[0xe8] = 0x1c;
	  regs[0xe9] = 0x01;
	  regs[0xea] = 0x0a;
	  regs[0xeb] = 0xc4;
	  regs[0xec] = 0x01;
	  regs[0xed] = 0x04;
	  regs[0xee] = 0xf8;
	  regs[0xef] = 0x00;
	  regs[0xf0] = 0x02;
	  regs[0xf2] = 0x00;
	  break;
	}
      break;

    case 300:
      switch (dev->sensor)
	{
	case SENSOR_TYPE_XPA:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_XPA for 300 dpi\n");
	  regs[0x35] = 0x0e;	/* fast ? */
	  regs[0x3a] = 0x0e;


	  regs[0x85] = 0x18;
	  regs[0x86] = 0x1b;

	  regs[0x87] = 0x30;
	  regs[0x88] = 0x30;

	  regs[0xe7] = 0x00;
	  regs[0xe8] = 0x00;
	  regs[0xe9] = 0x00;
	  regs[0xea] = 0x00;
	  regs[0xeb] = 0x00;
	  regs[0xec] = 0x00;
	  regs[0xed] = 0x00;
	  regs[0xef] = 0x00;
	  regs[0xf0] = 0x00;
	  regs[0x8d] = 0xef;
	  regs[0xc0] = 0x00;
	  regs[0xc1] = 0xff;
	  regs[0xc2] = 0x0f;
	  regs[0xc3] = 0xff;
	  regs[0xc4] = 0x00;
	  regs[0xc5] = 0xf0;
	  regs[0xc6] = 0x00;
	  regs[0xc7] = 0xff;
	  regs[0xc8] = 0x0f;
	  regs[0xc9] = 0xff;
	  regs[0xca] = 0xff;
	  regs[0xcb] = 0xf1;
	  regs[0xcc] = 0xff;
	  regs[0xcd] = 0x00;
	  regs[0xce] = 0xf0;
	  regs[0xcf] = 0xed;
	  regs[0xd0] = 0xef;
	  regs[0xd1] = 0xe2;
	  regs[0xd2] = 0x03;
	  regs[0xd3] = 0x17;
	  regs[0xd4] = 0x01;
	  regs[0xe2] = 0x07;
	  regs[0xe3] = 0x00;
	  regs[0xe4] = 0x00;
          timing=0x022b;
          exposure=342;
	  regs[0xf2] = 0x00;
	  break;
	case SENSOR_TYPE_BARE:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_BARE for 300 dpi\n");
	  regs[0x35] = 0x0e;	/* fast ? */
	  regs[0x3a] = 0x0e;

          timing=0x022b;

	  regs[0x85] = 0x18;
	  regs[0x86] = 0x1b;

	  regs[0x87] = 0x30;
	  regs[0x88] = 0x30;

	  regs[0xe7] = 0x00;
	  regs[0xe8] = 0x00;
	  regs[0xe9] = 0x00;
	  regs[0xea] = 0x00;
	  regs[0xeb] = 0x00;
	  regs[0xec] = 0x00;
	  regs[0xed] = 0x00;
	  regs[0xef] = 0x00;
	  regs[0xf0] = 0x00;
	  regs[0x8d] = 0xf0;
	  regs[0x8e] = 0x60;	/* low nibble of 8e and 8d are proportional to 
					   the scanned width 1de => 5100 wide scan */

	  regs[0xc0] = 0xff;
	  regs[0xc1] = 0x0f;
	  regs[0xc2] = 0x00;
	  regs[0xc9] = 0x00;
	  regs[0xca] = 0x0e;
	  regs[0xcb] = 0x00;
	  regs[0xcc] = 0x00;
	  regs[0xcd] = 0xf0;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xf5;
	  regs[0xd0] = 0xf7;
	  regs[0xd1] = 0xea;
	  regs[0xd2] = 0x0b;

	  regs[0xd3] = 0x17;
	  regs[0xd4] = 0x01;

	  regs[0xe2] = 0x07;
	  regs[0xe3] = 0x00;
	  regs[0xe4] = 0x00;

          exposure=342;
	  break;
	case SENSOR_TYPE_4400:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_4400 for 300 dpi\n");
	  regs[0x11] = 0x22;
	  regs[0x35] = 0x0e;
	  regs[0x3a] = 0x0e;
	  regs[0x85] = 0x18;
	  regs[0x86] = 0x1b;
	  regs[0x87] = 0x30;
	  regs[0x88] = 0x30;
	  regs[0x8d] = 0xef;
	  regs[0xc0] = 0x00;
	  regs[0xc1] = 0xff;
	  regs[0xc2] = 0x0f;
	  regs[0xc3] = 0xff;
	  regs[0xc4] = 0x00;
	  regs[0xc5] = 0xf0;
	  regs[0xc6] = 0x00;
	  regs[0xc7] = 0xff;
	  regs[0xc8] = 0x0f;
	  regs[0xc9] = 0xff;
	  regs[0xca] = 0xff;
	  regs[0xcb] = 0xf1;
	  regs[0xcc] = 0xff;
	  regs[0xcd] = 0x00;
	  regs[0xce] = 0xf0;
	  regs[0xcf] = 0xed;
	  regs[0xd0] = 0xef;
	  regs[0xd1] = 0xe2;
	  regs[0xd2] = 0x03;
	  regs[0xd3] = 0x17;
	  regs[0xd4] = 0x01;
	  regs[0xe2] = 0x03;
	  regs[0xe3] = 0x00;
	  regs[0xe4] = 0x00;
          timing=0x022b;
          exposure=686;
	  regs[0xe7] = 0x00;
	  regs[0xe8] = 0x00;
	  regs[0xe9] = 0x00;
	  regs[0xea] = 0x00;
	  regs[0xeb] = 0x00;
	  regs[0xec] = 0x00;
	  regs[0xed] = 0x00;
	  regs[0xef] = 0x00;
	  regs[0xf0] = 0x00;
	  regs[0xf2] = 0x00;
	  break;
	}
      break;
    case 600:
      *status1 = 0x28;
      switch (dev->sensor)
	{
	case SENSOR_TYPE_BARE:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_BARE for 600 dpi\n");

	  regs[0x34] = 0xf0;
	  regs[0x35] = 0x1b;
	  regs[0x36] = 0x29;
	  regs[0x3a] = 0x1b;

	  regs[0x72] = 0x3a;
	  regs[0x73] = 0x15;
	  regs[0x74] = 0x62;

          timing=0x0425;
	  regs[0x85] = 0x30;
	  regs[0x86] = 0x30;
	  regs[0x87] = 0x60;
	  regs[0x88] = 0x5a;

	  regs[0x8d] = 0xde;
	  regs[0x8e] = 0x61;	/* low nibble of 8e and 8d are proportional to 
					   the scanned width 1de => 5100 wide scan */

	  regs[0xc0] = 0xff;
	  regs[0xc1] = 0xff;
	  regs[0xc2] = 0xff;
	  regs[0xc3] = 0x00;
	  regs[0xc4] = 0xf0;
	  regs[0xc7] = 0x0f;
	  regs[0xc8] = 0x00;
	  regs[0xcb] = 0xe0;
	  regs[0xcc] = 0xff;
	  regs[0xcd] = 0xff;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xe9;
	  regs[0xd0] = 0xeb;

	  regs[0xd7] = 0x14;

	  regs[0xe2] = 0x01;
	  regs[0xe3] = 0x00;
	  regs[0xe4] = 0x00;
	  /* regs[0xe5] = 0xbd;
	     regs[0xe6] = 0x0a;         exposure time = 0x0abd=2749 (5500/2-1) */
          exposure=2749;
	  regs[0xe7] = 0x00;
	  regs[0xe8] = 0x00;
	  regs[0xe9] = 0x00;
	  regs[0xea] = 0x00;
	  regs[0xeb] = 0x00;
	  regs[0xec] = 0x00;
	  regs[0xed] = 0x00;
	  regs[0xef] = 0x00;
	  regs[0xf0] = 0x00;
	  regs[0xf2] = 0x00;
	  break;
	case SENSOR_TYPE_XPA:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_XPA for 600 dpi\n");

          *status2 = 0x3b;
	  regs[0x33] = 0x83; /* 0x86 */
	  regs[0x34] = 0xf0;
	  regs[0x35] = 0x1b;
	  regs[0x36] = 0x29;
	  regs[0x3a] = 0x1b;
	  regs[0x40] = 0x2c; /* 0x24 */
	  regs[0x50] = 0x00; /* 0x18 */
	  
          regs[0x64] = 0x01; /* 0x02 */
	  regs[0x65] = 0x20; /* 0x10 */

	  regs[0x72] = 0x3a;
	  regs[0x73] = 0x15;
	  regs[0x74] = 0x62;


	  regs[0x85] = 0x30;
	  regs[0x86] = 0x30;

	  regs[0x87] = 0x60;
	  regs[0x88] = 0x5a;

	  regs[0x8d] = 0xde;
	  regs[0x8e] = 0x61;	/* 25054 */

	  regs[0xc0] = 0xf8;
	  regs[0xc1] = 0x7f;
	  regs[0xc2] = 0x00;
	  regs[0xc3] = 0xf8;
	  regs[0xc4] = 0x7f;
	  regs[0xc5] = 0x00;
	  regs[0xc6] = 0xf8;
	  regs[0xc7] = 0x7f;
	  regs[0xc8] = 0x00;
	  regs[0xc9] = 0xff;
	  regs[0xca] = 0x8f;
	  regs[0xcb] = 0xff;
	  regs[0xcc] = 0x07;
	  regs[0xcd] = 0x80;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xf2;
	  regs[0xd0] = 0xf4;
	  regs[0xd1] = 0xe7;
	  regs[0xd2] = 0x08;
	  regs[0xd3] = 0x02;
	  regs[0xd4] = 0x10;
	  regs[0xd7] = 0x31;
	  regs[0xe2] = 0x01;
	  regs[0xe3] = 0x00;
	  regs[0xe4] = 0x00;
	  regs[0xe7] = 0x00;
	  regs[0xe8] = 0x00;
	  regs[0xe9] = 0x00;
	  regs[0xea] = 0x00;
	  regs[0xeb] = 0x00;
	  regs[0xec] = 0x00;
	  regs[0xed] = 0x00;
	  regs[0xef] = 0x00;
	  regs[0xf0] = 0x00;
	  regs[0xf2] = 0x00;
          exposure=2749;
          timing=0x0425;
	  break;

	case SENSOR_TYPE_4400:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_4400 for 600 dpi\n");
	  *status1 = 0x10;
	  *status2 = 0x23;

	  regs[0x13] = 0x39;
	  regs[0x14] = 0xf0;
	  regs[0x15] = 0x29;
	  regs[0x16] = 0x00;
	  regs[0x17] = 0x10;
	  regs[0x23] = 0x00;
	  regs[0x34] = 0xf0;
	  regs[0x35] = 0x1b;
	  regs[0x36] = 0x29;
	  regs[0x39] = 0x00;
	  regs[0x3a] = 0x1b;
	  regs[0x72] = 0x3a;
	  regs[0x73] = 0x15;
	  regs[0x74] = 0x62;
	  regs[0x85] = 0x30;
	  regs[0x86] = 0x30;
	  regs[0x87] = 0x60;
	  regs[0x88] = 0x5a;
	  regs[0x8d] = 0xde;
	  regs[0x8e] = 0x61;
	  regs[0xc0] = 0xf8;
	  regs[0xc1] = 0x7f;
	  regs[0xc2] = 0x00;
	  regs[0xc3] = 0xf8;
	  regs[0xc4] = 0x7f;
	  regs[0xc5] = 0x00;
	  regs[0xc6] = 0xf8;
	  regs[0xc7] = 0x7f;
	  regs[0xc8] = 0x00;
	  regs[0xc9] = 0xff;
	  regs[0xca] = 0x8f;
	  regs[0xcb] = 0xff;
	  regs[0xcc] = 0x07;
	  regs[0xcd] = 0x80;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xf2;
	  regs[0xd0] = 0xf4;
	  regs[0xd1] = 0xe7;
	  regs[0xd2] = 0x08;
	  regs[0xd3] = 0x0e;
	  regs[0xd4] = 0x10;
	  regs[0xd7] = 0x31;
	  regs[0xe2] = 0x01;
	  regs[0xe3] = 0x00;
	  regs[0xe4] = 0x00;
	  regs[0xe7] = 0x00;
	  regs[0xe8] = 0x00;
	  regs[0xe9] = 0x00;
	  regs[0xea] = 0x00;
	  regs[0xeb] = 0x00;
	  regs[0xec] = 0x00;
	  regs[0xed] = 0x00;
	  regs[0xef] = 0x00;
	  regs[0xf0] = 0x00;
	  regs[0xf2] = 0x00;
          timing=0x0425;
          exposure=2749;
	  break;

	case SENSOR_TYPE_4400_BARE:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_4400_BARE for 600 dpi\n");
	  return SANE_STATUS_INVAL;
	  break;
	}
      break;
    case 1200:
      *status1 = 0x28;
      switch (dev->sensor)
	{
	case SENSOR_TYPE_BARE:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_BARE for 1200 dpi\n");

	  regs[0x34] = 0xf0;
	  regs[0x35] = 0x1b;
	  regs[0x36] = 0x29;
	  regs[0x3a] = 0x1b;
	  regs[0x40] = 0xac;
          timing=0x081a;
	  regs[0x85] = 0x60;
	  regs[0x86] = 0x5a;
	  regs[0x87] = 0xc0;
	  regs[0x88] = 0xae;

	  regs[0x8d] = 0xbd;	/* about twice the 600 dpi values */
	  regs[0x8e] = 0x63;	/* low nibble of 8e and 8d are proportional to 
					   the scanned width 3b5 => 10124 wide scan */
	  regs[0xc0] = 0xff;
	  regs[0xc1] = 0xff;
	  regs[0xc2] = 0xff;
	  regs[0xc4] = 0x0f;
	  regs[0xc5] = 0x00;
	  regs[0xc6] = 0x00;
	  regs[0xc7] = 0xf0;
	  regs[0xc9] = 0x00;
	  regs[0xca] = 0x0e;
	  regs[0xcb] = 0x00;
	  regs[0xcc] = 0xff;
	  regs[0xcd] = 0xff;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xf5;
	  regs[0xd0] = 0xf7;
	  regs[0xd1] = 0xea;
	  regs[0xd2] = 0x0b;
	  regs[0xd3] = 0x17;
	  regs[0xd4] = 0xc1;

	  regs[0xd7] = 0x14;
	  regs[0xd8] = 0xa4;

	  regs[0xe2] = 0x01;
	  regs[0xe3] = 0x00;
	  regs[0xe4] = 0x00;
	  /* regs[0xe5] = 0x7b;
	     regs[0xe6] = 0x15;         exposure time = 0x157b=5499 */
          exposure=5499;
	  regs[0xe7] = 0x00;
	  regs[0xe8] = 0x00;
	  regs[0xe9] = 0x00;
	  regs[0xea] = 0x00;
	  regs[0xeb] = 0x00;
	  regs[0xec] = 0x00;
	  regs[0xed] = 0x00;
	  regs[0xef] = 0x00;
	  regs[0xf0] = 0x00;
	  regs[0xf2] = 0x00;
	  break;
	case SENSOR_TYPE_XPA:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_XPA for 1200 dpi\n");
	  *status2 = 0x3f;

	  regs[0x34] = 0xf0;
	  regs[0x35] = 0x1b;
	  regs[0x36] = 0x29;
	  regs[0x3a] = 0x1b;
	  regs[0x85] = 0x60;
	  regs[0x86] = 0x5a;
	  regs[0x87] = 0xc0;
	  regs[0x8a] = 0x08;	/* 81c=2076 */

	  regs[0xc1] = 0xff;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xf5;
	  regs[0xd0] = 0xf7;

	  regs[0xe2] = 0x01;
	  regs[0xe3] = 0x00;
	  regs[0xe4] = 0x00;
          timing=0x081a;
          exposure=5499;
	  regs[0xe7] = 0x00;
	  regs[0xe8] = 0x00;
	  regs[0xe9] = 0x00;
	  regs[0xea] = 0x00;
	  regs[0xeb] = 0x00;
	  regs[0xec] = 0x00;
	  regs[0xed] = 0x00;
	  regs[0xef] = 0x00;
	  regs[0xf0] = 0x00;
	  regs[0xf2] = 0x00;
	  regs[0x33] = 0x83;
	  regs[0x40] = 0xac;
	  regs[0x50] = 0x00;
	  regs[0x64] = 0x01;
	  regs[0x65] = 0x20;
	  regs[0x88] = 0xae;
	  regs[0x8d] = 0xbc;
	  regs[0x8e] = 0x63;
	  regs[0xc0] = 0xe0;
	  regs[0xc2] = 0x01;
	  regs[0xc3] = 0x1f;
	  regs[0xc4] = 0x00;
	  regs[0xc5] = 0xfe;
	  regs[0xc6] = 0xff;
	  regs[0xc7] = 0xff;
	  regs[0xc8] = 0x00;
	  regs[0xc9] = 0x3f;
	  regs[0xca] = 0xfe;
	  regs[0xcb] = 0xff;
	  regs[0xcc] = 0x00;
	  regs[0xcd] = 0x00;
	  regs[0xd1] = 0xec;
	  regs[0xd2] = 0x0d;
	  regs[0xd3] = 0x05;
	  regs[0xd4] = 0x67;
	  regs[0xd7] = 0x10;
	  regs[0xd8] = 0x52;
	  break;

	case SENSOR_TYPE_4400:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_4400 for 1200 dpi\n");
	  regs[0x13] = 0x39;
	  regs[0x14] = 0xf0;
	  regs[0x15] = 0x29;
	  regs[0x16] = 0x00;
	  regs[0x17] = 0x10;
	  regs[0x23] = 0x00;
	  regs[0x33] = 0x86;
	  regs[0x34] = 0xf0;
	  regs[0x35] = 0x0e;
	  regs[0x39] = 0x00;
	  regs[0x3a] = 0x0e;
	  regs[0x40] = 0xac;
	  regs[0x64] = 0x02;
	  regs[0x65] = 0x10;
          timing=0x081a;
	  regs[0x85] = 0x60;
	  regs[0x86] = 0x5a;
	  regs[0x87] = 0xc0;
	  regs[0x88] = 0xae;
	  regs[0x8d] = 0xbc;
	  regs[0x8e] = 0x63;
	  regs[0xc0] = 0xe0;
	  regs[0xc1] = 0xff;
	  regs[0xc2] = 0x01;
	  regs[0xc3] = 0x1f;
	  regs[0xc4] = 0x00;
	  regs[0xc5] = 0xfe;
	  regs[0xc6] = 0xff;
	  regs[0xc7] = 0xff;
	  regs[0xc8] = 0x00;
	  regs[0xc9] = 0x3f;
	  regs[0xca] = 0xfe;
	  regs[0xcb] = 0xff;
	  regs[0xcc] = 0x00;
	  regs[0xcd] = 0x00;
	  regs[0xce] = 0xff;
	  regs[0xcf] = 0xf5;
	  regs[0xd0] = 0xf7;
	  regs[0xd1] = 0xec;
	  regs[0xd2] = 0x0d;
	  regs[0xd3] = 0x05;
	  regs[0xd4] = 0x67;
	  regs[0xd7] = 0x10;
	  regs[0xd8] = 0x52;
	  regs[0xe2] = 0x00;
	  regs[0xe3] = 0x00;
	  regs[0xe4] = 0x00;
	  regs[0xe7] = 0x00;
	  regs[0xe8] = 0x00;
	  regs[0xe9] = 0x00;
	  regs[0xea] = 0x00;
	  regs[0xeb] = 0x00;
	  regs[0xec] = 0x00;
	  regs[0xed] = 0x00;
	  regs[0xef] = 0x00;
	  regs[0xf0] = 0x00;
	  regs[0xf2] = 0x00;
          exposure=10999;
	  *status1 = 0x10;
	  *status2 = 0x23;
	  break;

	case SENSOR_TYPE_4400_BARE:
          DBG (DBG_io, "setup_scan_registers: setting up SENSOR_TYPE_4400_BARE for 1200 dpi\n");
	  return SANE_STATUS_INVAL;
	  break;
	}
      break;
    }

  /* apply computed settings */
  SET_DOUBLE (regs, EXPOSURE_REG, exposure);
  SET_DOUBLE (regs, TIMING_REG, timing);
  SET_DOUBLE (regs, TIMING1_REG, timing+1);
  SET_DOUBLE (regs, TIMING2_REG, timing+2);

  /* sets divisor */
  regs[0xd3] = rts8891_data_format (dev->xdpi, dev->sensor);

  /* toggle front panel light to signal gray scan */
  if (session->params.format == SANE_FRAME_GRAY)
    {
      *status1 = (*status1 & 0x0F) | 0x10;
    }

  return status;
}

/* set up the shadow registers for scan, depending on scan parameters    */
/* the ultimate goal is to have no direct access to registers, but to    */
/* set them through helper functions                                     */
/* NOTE : I couldn't manage to get scans that really uses gray settings. */
/* The windows driver is allways scanning in color, so we do the same.   */
/* For now, the only mode that could be done would be 300 dpi gray scan, */
/* based on the register settings of find_origin()                       */
static SANE_Status
write_scan_registers (struct Rts8891_Session *session)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte control;
  SANE_Byte status1, status2;
  struct Rts8891_Device *dev = session->dev;

  /* setup registers for scan */
  status=setup_scan_registers (session, &status1, &status2, dev->regs);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error0, "write_scan_registers: failed to setup registers\n");
      return status;
    }

  /* check if session is idle */
  control = 0x00;
  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
  if (control != 0)
    {
      DBG (DBG_error0, "write_scan_registers: scanner is not idle!\n");
      return SANE_STATUS_IO_ERROR;
    }

  /* effective write of register set for scan */
  status = rts8891_write_all (dev->devnum, dev->regs, dev->reg_count);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error0, "write_scan_registers: failed to write registers\n");
    }
  return status;
}

/**
 * This function parks head relying by moving head backward by a
 * very large amount without scanning
 */
static SANE_Status
park_head (struct Rts8891_Device *dev, SANE_Bool wait)
{
  SANE_Status status;
  SANE_Byte reg, control;
  /* the hammer way : set all regs */
  SANE_Byte regs[244];

  DBG (DBG_proc, "park_head: start\n");

  reg = 0x8d;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &reg);
  reg = 0xad;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &reg);

  status = sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);

  reg = 0xff;
  sanei_rts88xx_write_reg (dev->devnum, 0x23, &reg);

  /* TODO create write_double_reg */
  if (dev->sensor != SENSOR_TYPE_4400)
    {
      dev->regs[0x16] = 0x07;
      dev->regs[0x17] = 0x00;
    }
  else
    {
      dev->regs[0x16] = 0x0f;
      dev->regs[0x17] = 0x10;
    }
  sanei_rts88xx_write_regs (dev->devnum, 0x16, dev->regs + 0x16, 2);

  reg = 0x8d;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &reg);
  reg = 0xad;
  sanei_rts88xx_write_reg (dev->devnum, LAMP_REG, &reg);

  /* 0x20 expected */
  sanei_rts88xx_read_reg (dev->devnum, CONTROLER_REG, &reg);
  if (reg != 0x20)
    {
      DBG (DBG_warn, "park_head: unexpected controler value 0x%02x\n", reg);
    }

  /* head parking */
  status = rts8891_park (dev, regs, wait);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "park_head: failed to park head!\n");
    }

  DBG (DBG_proc, "park_head: end\n");
  return status;
}

/* update button status 
 * button access is allowed during scan, which is usefull for 'cancel' button
 */
static SANE_Status
update_button_status (struct Rts8891_Session *session)
{
  SANE_Int mask = 0, i;
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Bool lock = SANE_FALSE;

  /* while scanning, interface is reserved, so don't claim/release it */
  if (session->scanning != SANE_TRUE)
    {
      lock = SANE_TRUE;

      /* claim the interface to reserve device */
      if (session->dev->conf.allowsharing == SANE_TRUE)
	{
	  status = sanei_usb_claim_interface (session->dev->devnum, 0);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_warn,
		   "update_button_status: cannot claim usb interface\n");
	      return SANE_STATUS_DEVICE_BUSY;
	    }
	}
    }

  /* effective button reading */
  status = rts8891_read_buttons (session->dev->devnum, &mask);

  /* release interface if needed */
  if (lock == SANE_TRUE)
    {
      if (session->dev->conf.allowsharing == SANE_TRUE)
	{
	  sanei_usb_release_interface (session->dev->devnum, 0);
	}
    }

  for (i = 0; i < session->dev->model->buttons; i++)
    {
      if (mask & (1 << i))
	{
	  session->val[OPT_BUTTON_1 + i].w = SANE_TRUE;
	  DBG (DBG_io2, "update_button_status: setting button %d to TRUE\n",
	       i + 1);
	}
    }
  return status;
}

/* set lamp status, 0 for lamp off
 * other values set lamp on */
static SANE_Status
set_lamp_state (struct Rts8891_Session *session, int on)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte reg;

  /* claim the interface reserve device */
  if (session->dev->conf.allowsharing == SANE_TRUE)
    {
      status = sanei_usb_claim_interface (session->dev->devnum, 0);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_warn, "set_lamp_state: cannot claim usb interface\n");
	  return SANE_STATUS_DEVICE_BUSY;
	}
    }

  status = sanei_rts88xx_read_reg (session->dev->devnum, LAMP_REG, &reg);
  if (on)
    {
      DBG (DBG_info, "set_lamp_state: lamp on\n");
      reg = session->dev->regs[LAMP_REG] | 0x80;
    }
  else
    {
      DBG (DBG_info, "set_lamp_state: lamp off\n");
      reg = session->dev->regs[LAMP_REG] & 0x7F;
#ifdef HAVE_SYS_TIME_H
      /* if lamp is switched off, warming up will be needed */
      session->dev->last_scan.tv_sec = 0;
#endif
    }
  status = sanei_rts88xx_write_reg (session->dev->devnum, LAMP_REG, &reg);

  /* release interface and return status from lamp setting */
  if (session->dev->conf.allowsharing == SANE_TRUE)
    {
      sanei_usb_release_interface (session->dev->devnum, 0);
    }
  return status;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
