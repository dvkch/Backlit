/* sane - Scanner Access Now Easy.

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

   This file implements a SANE backend for HP ScanJet 3500 series scanners.
   Currently supported:
    - HP ScanJet 3500C
    - HP ScanJet 3530C
    - HP ScanJet 3570C

   SANE FLOW DIAGRAM

   - sane_init() : initialize backend, attach scanners
   . - sane_get_devices() : query list of scanner devices
   . - sane_open() : open a particular scanner device
   . . - sane_set_io_mode : set blocking mode
   . . - sane_get_select_fd : get scanner fd
   . . - sane_get_option_descriptor() : get option information
   . . - sane_control_option() : change option values
   . .
   . . - sane_start() : start image acquisition
   . .   - sane_get_parameters() : returns actual scan parameters
   . .   - sane_read() : read image data (from pipe)
   . .
   . . - sane_cancel() : cancel operation
   . - sane_close() : close opened scanner device
   - sane_exit() : terminate use of backend


   There are some device specific routines in this file that are in "#if 0"
   sections - these are left in place for documentation purposes in case
   somebody wants to implement features that use those routines.

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
# include <libc.h>		/* NeXTStep/OpenStep */
#endif

#include "../include/sane/sane.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_thread.h"
#include "../include/sane/sanei_backend.h"

#define RTCMD_GETREG		0x80
#define	RTCMD_READSRAM		0x81

#define	RTCMD_SETREG		0x88
#define	RTCMD_WRITESRAM		0x89

#define	RTCMD_NVRAMCONTROL	0x8a

#define	RTCMD_BYTESAVAIL	0x90
#define	RTCMD_READBYTES		0x91

#define	RT_CHANNEL_ALL		0
#define	RT_CHANNEL_RED		1
#define	RT_CHANNEL_GREEN	2
#define	RT_CHANNEL_BLUE		3

typedef int (*rts8801_callback) (void *param, unsigned bytes, void *data);

#define DEBUG 1
#define SCANNER_UNIT_TO_FIXED_MM(number) SANE_FIX(number * MM_PER_INCH / 1200)
#define FIXED_MM_TO_SCANNER_UNIT(number) SANE_UNFIX(number) * 1200 / MM_PER_INCH

#define MSG_ERR         1
#define MSG_USER        5
#define MSG_INFO        6
#define FLOW_CONTROL    10
#define MSG_IO          15
#define MSG_IO_READ     17
#define IO_CMD          20
#define IO_CMD_RES      20
#define MSG_GET         25
/* ------------------------------------------------------------------------- */

enum hp3500_option
{
  OPT_NUM_OPTS = 0,

  OPT_RESOLUTION,
  OPT_GEOMETRY_GROUP,
  OPT_TL_X,
  OPT_TL_Y,
  OPT_BR_X,
  OPT_BR_Y,
  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_BRIGHTNESS,
  OPT_CONTRAST,

  NUM_OPTIONS
};

typedef struct
{
  int left;
  int top;
  int right;
  int bottom;
} hp3500_rect;

struct hp3500_data
{
  struct hp3500_data *next;
  char *devicename;

  int sfd;
  int pipe_r;
  int pipe_w;
  SANE_Pid reader_pid;

  int resolution;
  int mode;

  time_t last_scan;

  hp3500_rect request_mm;
  hp3500_rect actual_mm;
  hp3500_rect fullres_pixels;
  hp3500_rect actres_pixels;

  int rounded_left;
  int rounded_top;
  int rounded_right;
  int rounded_bottom;

  int bytes_per_scan_line;
  int scan_width_pixels;
  int scan_height_pixels;

  int brightness;
  int contrast;

  SANE_Option_Descriptor opt[NUM_OPTIONS];
  SANE_Device sane;
};

struct hp3500_write_info
{
  struct hp3500_data *scanner;
  int bytesleft;
};

typedef struct detailed_calibration_data
{
  unsigned char const *channeldata[3];
  unsigned resolution_divisor;
} detailed_calibration_data;

static struct hp3500_data *first_dev = 0;
static struct hp3500_data **new_dev = &first_dev;
static int num_devices = 0;
static SANE_Int res_list[] =
  { 9, 50, 75, 100, 150, 200, 300, 400, 600, 1200 };
static const SANE_Range range_x =
  { 0, SANE_FIX (215.9), SANE_FIX (MM_PER_INCH / 1200) };
static const SANE_Range range_y =
  { 0, SANE_FIX (298.7), SANE_FIX (MM_PER_INCH / 1200) };
static const SANE_Range range_brightness =
  { 0, 255, 0 };
static const SANE_Range range_contrast =
  { 0, 255, 0 };


#define HP3500_COLOR_SCAN 0
#define HP3500_GRAY_SCAN 1
#define	HP3500_LINEART_SCAN 2
#define HP3500_TOTAL_SCANS 3

static char const *scan_mode_list[HP3500_TOTAL_SCANS + 1] = { 0 };

static SANE_Status attachScanner (const char *name);
static SANE_Status init_options (struct hp3500_data *scanner);
static int reader_process (void *);
static void calculateDerivedValues (struct hp3500_data *scanner);
static void do_reset (struct hp3500_data *scanner);
static void do_cancel (struct hp3500_data *scanner);

/*
 * used by sane_get_devices
 */
static const SANE_Device **devlist = 0;

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
  authorize = authorize;	/* get rid of compiler warning */

  DBG_INIT ();
  DBG (10, "sane_init\n");

  sanei_usb_init ();
  sanei_thread_init ();

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

  sanei_usb_find_devices (0x03f0, 0x2205, attachScanner);
  sanei_usb_find_devices (0x03f0, 0x2005, attachScanner);

  return SANE_STATUS_GOOD;
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
  int i;
  struct hp3500_data *dev;

  DBG (10, "sane_get_devices %d\n", local_only);

  if (devlist)
    free (devlist);
  devlist = calloc (num_devices + 1, sizeof (SANE_Device *));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  for (dev = first_dev, i = 0; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;

  return SANE_STATUS_GOOD;
}


/**
 * Called to establish connection with the scanner. This function will
 * also establish meaningful defauls and initialize the options.
 *
 * From the SANE spec:
 * This function is used to establish a connection to a particular
 * device. The name of the device to be opened is passed in argument
 * name. If the call completes successfully, a handle for the device
 * is returned in *h. As a special case, specifying a zero-length
 * string as the device requests opening the first available device
 * (if there is such a device).
 */
SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * handle)
{
  struct hp3500_data *dev = NULL;
  struct hp3500_data *scanner = NULL;

  if (name[0] == 0)
    {
      DBG (10, "sane_open: no device requested, using default\n");
      if (first_dev)
	{
	  scanner = (struct hp3500_data *) first_dev;
	  DBG (10, "sane_open: device %s found\n", first_dev->sane.name);
	}
    }
  else
    {
      DBG (10, "sane_open: device %s requested\n", name);

      for (dev = first_dev; dev; dev = dev->next)
	{
	  if (strcmp (dev->sane.name, name) == 0)
	    {
	      DBG (10, "sane_open: device %s found\n", name);
	      scanner = (struct hp3500_data *) dev;
	    }
	}
    }

  if (!scanner)
    {
      DBG (10, "sane_open: no device found\n");
      return SANE_STATUS_INVAL;
    }

  *handle = scanner;

  init_options (scanner);

  scanner->resolution = 200;
  scanner->request_mm.left = 0;
  scanner->request_mm.top = 0;
  scanner->request_mm.right = SCANNER_UNIT_TO_FIXED_MM (10200);
  scanner->request_mm.bottom = SCANNER_UNIT_TO_FIXED_MM (14100);
  scanner->mode = 0;
  scanner->brightness = 128;
  scanner->contrast = 64;
  calculateDerivedValues (scanner);

  return SANE_STATUS_GOOD;

}


/**
 * An advanced method we don't support but have to define.
 */
SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool non_blocking)
{
  DBG (10, "sane_set_io_mode\n");
  DBG (99, "%d %p\n", non_blocking, h);
  return SANE_STATUS_UNSUPPORTED;
}


/**
 * An advanced method we don't support but have to define.
 */
SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int * fdp)
{
  struct hp3500_data *scanner = (struct hp3500_data *) h;
  DBG (10, "sane_get_select_fd\n");
  *fdp = scanner->pipe_r;
  DBG (99, "%p %d\n", h, *fdp);
  return SANE_STATUS_GOOD;
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
  struct hp3500_data *scanner = handle;

  DBG (MSG_GET,
       "sane_get_option_descriptor: \"%s\"\n", scanner->opt[option].name);

  if ((unsigned) option >= NUM_OPTIONS)
    return NULL;
  return &scanner->opt[option];
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
 */
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  struct hp3500_data *scanner = (struct hp3500_data *) handle;
  SANE_Status status;
  SANE_Word cap;
  SANE_Int dummy;
  int i;

  /* Make sure that all those statements involving *info cannot break (better
   * than having to do "if (info) ..." everywhere!)
   */
  if (info == 0)
    info = &dummy;

  *info = 0;

  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  cap = scanner->opt[option].cap;

  /*
   * SANE_ACTION_GET_VALUE: We have to find out the current setting and
   * return it in a human-readable form (often, text).
   */
  if (action == SANE_ACTION_GET_VALUE)
    {
      DBG (MSG_GET, "sane_control_option: get value \"%s\"\n",
	   scanner->opt[option].name);
      DBG (11, "\tcap = %d\n", cap);

      if (!SANE_OPTION_IS_ACTIVE (cap))
	{
	  DBG (10, "\tinactive\n");
	  return SANE_STATUS_INVAL;
	}

      switch (option)
	{
	case OPT_NUM_OPTS:
	  *(SANE_Word *) val = NUM_OPTIONS;
	  return SANE_STATUS_GOOD;

	case OPT_RESOLUTION:
	  *(SANE_Word *) val = scanner->resolution;
	  return SANE_STATUS_GOOD;

	case OPT_TL_X:
	  *(SANE_Word *) val = scanner->request_mm.left;
	  return SANE_STATUS_GOOD;

	case OPT_TL_Y:
	  *(SANE_Word *) val = scanner->request_mm.top;
	  return SANE_STATUS_GOOD;

	case OPT_BR_X:
	  *(SANE_Word *) val = scanner->request_mm.right;
	  return SANE_STATUS_GOOD;

	case OPT_BR_Y:
	  *(SANE_Word *) val = scanner->request_mm.bottom;
	  return SANE_STATUS_GOOD;

	case OPT_MODE:
	  strcpy ((SANE_Char *) val, scan_mode_list[scanner->mode]);
	  return SANE_STATUS_GOOD;

	case OPT_CONTRAST:
	  *(SANE_Word *) val = scanner->contrast;
	  return SANE_STATUS_GOOD;

	case OPT_BRIGHTNESS:
	  *(SANE_Word *) val = scanner->brightness;
	  return SANE_STATUS_GOOD;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      DBG (10, "sane_control_option: set value \"%s\"\n",
	   scanner->opt[option].name);

      if (!SANE_OPTION_IS_ACTIVE (cap))
	{
	  DBG (10, "\tinactive\n");
	  return SANE_STATUS_INVAL;
	}

      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (10, "\tnot settable\n");
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (scanner->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (10, "\tbad value\n");
	  return status;
	}

      /*
       * Note - for those options which can assume one of a list of
       * valid values, we can safely assume that they will have
       * exactly one of those values because that's what
       * sanei_constrain_value does. Hence no "else: invalid" branches
       * below.
       */
      switch (option)
	{
	case OPT_RESOLUTION:
	  if (scanner->resolution == *(SANE_Word *) val)
	    {
	      return SANE_STATUS_GOOD;
	    }
	  scanner->resolution = (*(SANE_Word *) val);
	  calculateDerivedValues (scanner);
	  *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_TL_X:
	  if (scanner->request_mm.left == *(SANE_Word *) val)
	    return SANE_STATUS_GOOD;
	  scanner->request_mm.left = *(SANE_Word *) val;
	  calculateDerivedValues (scanner);
	  if (scanner->actual_mm.left != scanner->request_mm.left)
	    *info |= SANE_INFO_INEXACT;
	  *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_TL_Y:
	  if (scanner->request_mm.top == *(SANE_Word *) val)
	    return SANE_STATUS_GOOD;
	  scanner->request_mm.top = *(SANE_Word *) val;
	  calculateDerivedValues (scanner);
	  if (scanner->actual_mm.top != scanner->request_mm.top)
	    *info |= SANE_INFO_INEXACT;
	  *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_BR_X:
	  if (scanner->request_mm.right == *(SANE_Word *) val)
	    {
	      return SANE_STATUS_GOOD;
	    }
	  scanner->request_mm.right = *(SANE_Word *) val;
	  calculateDerivedValues (scanner);
	  if (scanner->actual_mm.right != scanner->request_mm.right)
	    *info |= SANE_INFO_INEXACT;
	  *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_BR_Y:
	  if (scanner->request_mm.bottom == *(SANE_Word *) val)
	    {
	      return SANE_STATUS_GOOD;
	    }
	  scanner->request_mm.bottom = *(SANE_Word *) val;
	  calculateDerivedValues (scanner);
	  if (scanner->actual_mm.bottom != scanner->request_mm.bottom)
	    *info |= SANE_INFO_INEXACT;
	  *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_MODE:
	  for (i = 0; scan_mode_list[i]; ++i)
	    {
	      if (!strcmp ((SANE_Char const *) val, scan_mode_list[i]))
		{
		  DBG (10, "Setting scan mode to %s (request: %s)\n",
		       scan_mode_list[i], (SANE_Char const *) val);
		  scanner->mode = i;
		  return SANE_STATUS_GOOD;
		}
	    }
	  /* Impossible */
	  return SANE_STATUS_INVAL;

	case OPT_BRIGHTNESS:
	  scanner->brightness = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	case OPT_CONTRAST:
	  scanner->contrast = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;
	}			/* switch */
    }				/* else */
  return SANE_STATUS_INVAL;
}

/**
 * Called by SANE when a page acquisition operation is to be started.
 *
 */
SANE_Status
sane_start (SANE_Handle handle)
{
  struct hp3500_data *scanner = handle;
  int defaultFds[2];
  int ret;

  DBG (10, "sane_start\n");

  if (scanner->sfd < 0)
    {
      /* first call */
      DBG (10, "sane_start opening USB device\n");
      if (sanei_usb_open (scanner->sane.name, &(scanner->sfd)) !=
	  SANE_STATUS_GOOD)
	{
	  DBG (MSG_ERR,
	       "sane_start: open of %s failed:\n", scanner->sane.name);
	  return SANE_STATUS_INVAL;
	}
    }

  calculateDerivedValues (scanner);

  DBG (10, "\tbytes per line = %d\n", scanner->bytes_per_scan_line);
  DBG (10, "\tpixels_per_line = %d\n", scanner->scan_width_pixels);
  DBG (10, "\tlines = %d\n", scanner->scan_height_pixels);


  /* create a pipe, fds[0]=read-fd, fds[1]=write-fd */
  if (pipe (defaultFds) < 0)
    {
      DBG (MSG_ERR, "ERROR: could not create pipe\n");
      do_cancel (scanner);
      return SANE_STATUS_IO_ERROR;
    }

  scanner->pipe_r = defaultFds[0];
  scanner->pipe_w = defaultFds[1];

  ret = SANE_STATUS_GOOD;

  scanner->reader_pid = sanei_thread_begin (reader_process, scanner);
  time (&scanner->last_scan);

  if (scanner->reader_pid == -1)
    {
      DBG (MSG_ERR, "cannot fork reader process.\n");
      DBG (MSG_ERR, "%s", strerror (errno));
      ret = SANE_STATUS_IO_ERROR;
    }

  if (sanei_thread_is_forked ())
    {
      close (scanner->pipe_w);
    }

  if (ret == SANE_STATUS_GOOD)
    {
      DBG (10, "sane_start: ok\n");
    }

  return ret;
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
 * be. The parameters passed to this function are the handle h of the
 * device for which the parameters should be obtained and a pointer p
 * to a parameter structure.
 */
SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  struct hp3500_data *scanner = (struct hp3500_data *) handle;


  DBG (10, "sane_get_parameters\n");

  calculateDerivedValues (scanner);

  params->format =
    (scanner->mode == HP3500_COLOR_SCAN) ? SANE_FRAME_RGB : SANE_FRAME_GRAY;
  params->depth = (scanner->mode == HP3500_LINEART_SCAN) ? 1 : 8;

  params->pixels_per_line = scanner->scan_width_pixels;
  params->lines = scanner->scan_height_pixels;

  params->bytes_per_line = scanner->bytes_per_scan_line;

  params->last_frame = 1;
  DBG (10, "\tdepth %d\n", params->depth);
  DBG (10, "\tlines %d\n", params->lines);
  DBG (10, "\tpixels_per_line %d\n", params->pixels_per_line);
  DBG (10, "\tbytes_per_line %d\n", params->bytes_per_line);
  return SANE_STATUS_GOOD;
}


/**
 * Called by SANE to read data.
 * 
 * In this implementation, sane_read does nothing much besides reading
 * data from a pipe and handing it back. On the other end of the pipe
 * there's the reader process which gets data from the scanner and
 * stuffs it into the pipe.
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
  struct hp3500_data *scanner = (struct hp3500_data *) handle;
  ssize_t nread;
  int source = scanner->pipe_r;

  *len = 0;

  nread = read (source, buf, max_len);
  DBG (30, "sane_read: read %ld bytes of %ld\n",
       (long) nread, (long) max_len);

  if (nread < 0)
    {
      if (errno == EAGAIN)
	{
	  return SANE_STATUS_GOOD;
	}
      else
	{
	  do_cancel (scanner);
	  return SANE_STATUS_IO_ERROR;
	}
    }

  *len = nread;

  if (nread == 0)
    {
      close (source);
      DBG (10, "sane_read: pipe closed\n");
      return SANE_STATUS_EOF;
    }

  return SANE_STATUS_GOOD;
}				/* sane_read */


/**
 * Cancels a scan. 
 *
 * It has been said on the mailing list that sane_cancel is a bit of a
 * misnomer because it is routinely called to signal the end of a
 * batch - quoting David Mosberger-Tang:
 * 
 * > In other words, the idea is to have sane_start() be called, and
 * > collect as many images as the frontend wants (which could in turn
 * > consist of multiple frames each as indicated by frame-type) and
 * > when the frontend is done, it should call sane_cancel(). 
 * > Sometimes it's better to think of sane_cancel() as "sane_stop()"
 * > but that name would have had some misleading connotations as
 * > well, that's why we stuck with "cancel".
 * 
 * The current consensus regarding duplex and ADF scans seems to be
 * the following call sequence: sane_start; sane_read (repeat until
 * EOF); sane_start; sane_read...  and then call sane_cancel if the
 * batch is at an end. I.e. do not call sane_cancel during the run but
 * as soon as you get a SANE_STATUS_NO_DOCS.
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
sane_cancel (SANE_Handle h)
{
  DBG (10, "sane_cancel\n");
  do_cancel ((struct hp3500_data *) h);
}


/**
 * Ends use of the scanner.
 * 
 * From the SANE spec:
 * This function terminates the association between the device handle
 * passed in argument h and the device it represents. If the device is
 * presently active, a call to sane_cancel() is performed first. After
 * this function returns, handle h must not be used anymore.
 */
void
sane_close (SANE_Handle handle)
{
  DBG (10, "sane_close\n");
  do_reset (handle);
  do_cancel (handle);
}


/**
 * Terminates the backend.
 * 
 * From the SANE spec:
 * This function must be called to terminate use of a backend. The
 * function will first close all device handles that still might be
 * open (it is recommended to close device handles explicitly through
 * a call to sane_clo-se(), but backends are required to release all
 * resources upon a call to this function). After this function
 * returns, no function other than sane_init() may be called
 * (regardless of the status value returned by sane_exit(). Neglecting
 * to call this function may result in some resources not being
 * released properly.
 */
void
sane_exit (void)
{
  struct hp3500_data *dev, *next;

  DBG (10, "sane_exit\n");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free (dev->devicename);
      free (dev);
    }

  if (devlist)
    free (devlist);
}

/*
 * The scanning code
 */

static SANE_Status
attachScanner (const char *devicename)
{
  struct hp3500_data *dev;

  DBG (15, "attach_scanner: %s\n", devicename);

  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devicename) == 0)
	{
	  DBG (5, "attach_scanner: scanner already attached (is ok)!\n");
	  return SANE_STATUS_GOOD;
	}
    }


  if (NULL == (dev = malloc (sizeof (*dev))))
    return SANE_STATUS_NO_MEM;
  memset (dev, 0, sizeof (*dev));

  dev->devicename = strdup (devicename);
  dev->sfd = -1;
  dev->last_scan = 0;
  dev->reader_pid = -1;
  dev->pipe_r = dev->pipe_w = -1;

  dev->sane.name = dev->devicename;
  dev->sane.vendor = "Hewlett-Packard";
  dev->sane.model = "ScanJet 3500";
  dev->sane.type = "scanner";

  ++num_devices;
  *new_dev = dev;

  DBG (15, "attach_scanner: done\n");

  return SANE_STATUS_GOOD;
}

static SANE_Status
init_options (struct hp3500_data *scanner)
{
  int i;
  SANE_Option_Descriptor *opt;

  memset (scanner->opt, 0, sizeof (scanner->opt));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      scanner->opt[i].name = "filler";
      scanner->opt[i].size = sizeof (SANE_Word);
      scanner->opt[i].cap = SANE_CAP_INACTIVE;
    }

  opt = scanner->opt + OPT_NUM_OPTS;
  opt->title = SANE_TITLE_NUM_OPTIONS;
  opt->desc = SANE_DESC_NUM_OPTIONS;
  opt->type = SANE_TYPE_INT;
  opt->cap = SANE_CAP_SOFT_DETECT;

  opt = scanner->opt + OPT_RESOLUTION;
  opt->name = SANE_NAME_SCAN_RESOLUTION;
  opt->title = SANE_TITLE_SCAN_RESOLUTION;
  opt->desc = SANE_DESC_SCAN_RESOLUTION;
  opt->type = SANE_TYPE_INT;
  opt->constraint_type = SANE_CONSTRAINT_WORD_LIST;
  opt->constraint.word_list = res_list;
  opt->unit = SANE_UNIT_DPI;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  opt = scanner->opt + OPT_GEOMETRY_GROUP;
  opt->title = SANE_I18N ("Geometry");
  opt->desc = SANE_I18N ("Geometry Group");
  opt->type = SANE_TYPE_GROUP;
  opt->constraint_type = SANE_CONSTRAINT_NONE;

  opt = scanner->opt + OPT_TL_X;
  opt->name = SANE_NAME_SCAN_TL_X;
  opt->title = SANE_TITLE_SCAN_TL_X;
  opt->desc = SANE_DESC_SCAN_TL_X;
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &range_x;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  opt = scanner->opt + OPT_TL_Y;
  opt->name = SANE_NAME_SCAN_TL_Y;
  opt->title = SANE_TITLE_SCAN_TL_Y;
  opt->desc = SANE_DESC_SCAN_TL_Y;
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &range_y;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  opt = scanner->opt + OPT_BR_X;
  opt->name = SANE_NAME_SCAN_BR_X;
  opt->title = SANE_TITLE_SCAN_BR_X;
  opt->desc = SANE_DESC_SCAN_BR_X;
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &range_x;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  opt = scanner->opt + OPT_BR_Y;
  opt->name = SANE_NAME_SCAN_BR_Y;
  opt->title = SANE_TITLE_SCAN_BR_Y;
  opt->desc = SANE_DESC_SCAN_BR_Y;
  opt->type = SANE_TYPE_FIXED;
  opt->unit = SANE_UNIT_MM;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &range_y;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  if (!scan_mode_list[0])
    {
      scan_mode_list[HP3500_COLOR_SCAN] = SANE_VALUE_SCAN_MODE_COLOR;
      scan_mode_list[HP3500_GRAY_SCAN] = SANE_VALUE_SCAN_MODE_GRAY;
      scan_mode_list[HP3500_LINEART_SCAN] = SANE_VALUE_SCAN_MODE_LINEART;
      scan_mode_list[HP3500_TOTAL_SCANS] = 0;
    }

  opt = scanner->opt + OPT_MODE_GROUP;
  opt->title = SANE_I18N ("Scan Mode Group");
  opt->desc = SANE_I18N ("Scan Mode Group");
  opt->type = SANE_TYPE_GROUP;
  opt->constraint_type = SANE_CONSTRAINT_NONE;

  opt = scanner->opt + OPT_MODE;
  opt->name = SANE_NAME_SCAN_MODE;
  opt->title = SANE_TITLE_SCAN_MODE;
  opt->desc = SANE_DESC_SCAN_MODE;
  opt->type = SANE_TYPE_STRING;
  opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  opt->constraint.string_list = scan_mode_list;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  opt = scanner->opt + OPT_BRIGHTNESS;
  opt->name = SANE_NAME_BRIGHTNESS;
  opt->title = SANE_TITLE_BRIGHTNESS;
  opt->desc = SANE_DESC_BRIGHTNESS;
  opt->type = SANE_TYPE_INT;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &range_brightness;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  opt = scanner->opt + OPT_CONTRAST;
  opt->name = SANE_NAME_CONTRAST;
  opt->title = SANE_TITLE_CONTRAST;
  opt->desc = SANE_DESC_CONTRAST;
  opt->type = SANE_TYPE_INT;
  opt->constraint_type = SANE_CONSTRAINT_RANGE;
  opt->constraint.range = &range_contrast;
  opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;

  return SANE_STATUS_GOOD;
}

static void
do_reset (struct hp3500_data *scanner)
{
  scanner = scanner;		/* kill warning */
}

static void
do_cancel (struct hp3500_data *scanner)
{
  if (scanner->reader_pid != -1)
    {

      if (sanei_thread_kill (scanner->reader_pid) == 0)
	{
	  int exit_status;

	  sanei_thread_waitpid (scanner->reader_pid, &exit_status);
	}
      scanner->reader_pid = -1;
    }
  if (scanner->pipe_r >= 0)
    {
      close (scanner->pipe_r);
      scanner->pipe_r = -1;
    }
}

static void
calculateDerivedValues (struct hp3500_data *scanner)
{

  DBG (12, "calculateDerivedValues\n");

  /* Convert the SANE_FIXED values for the scan area into 1/1200 inch 
   * scanner units */

  scanner->fullres_pixels.left =
    FIXED_MM_TO_SCANNER_UNIT (scanner->request_mm.left);
  scanner->fullres_pixels.top =
    FIXED_MM_TO_SCANNER_UNIT (scanner->request_mm.top);
  scanner->fullres_pixels.right =
    FIXED_MM_TO_SCANNER_UNIT (scanner->request_mm.right);
  scanner->fullres_pixels.bottom =
    FIXED_MM_TO_SCANNER_UNIT (scanner->request_mm.bottom);

  DBG (12, "\tleft margin: %u\n", scanner->fullres_pixels.left);
  DBG (12, "\ttop margin: %u\n", scanner->fullres_pixels.top);
  DBG (12, "\tright margin: %u\n", scanner->fullres_pixels.right);
  DBG (12, "\tbottom margin: %u\n", scanner->fullres_pixels.bottom);


  scanner->scan_width_pixels =
    scanner->resolution * (scanner->fullres_pixels.right -
			   scanner->fullres_pixels.left) / 1200;
  scanner->scan_height_pixels =
    scanner->resolution * (scanner->fullres_pixels.bottom -
			   scanner->fullres_pixels.top) / 1200;
  if (scanner->mode == HP3500_LINEART_SCAN)
    scanner->bytes_per_scan_line = (scanner->scan_width_pixels + 7) / 8;
  else if (scanner->mode == HP3500_GRAY_SCAN)
    scanner->bytes_per_scan_line = scanner->scan_width_pixels;
  else
    scanner->bytes_per_scan_line = scanner->scan_width_pixels * 3;

  if (scanner->scan_width_pixels < 1)
    scanner->scan_width_pixels = 1;
  if (scanner->scan_height_pixels < 1)
    scanner->scan_height_pixels = 1;

  scanner->actres_pixels.left =
    scanner->fullres_pixels.left * scanner->resolution / 1200;
  scanner->actres_pixels.top =
    scanner->fullres_pixels.top * scanner->resolution / 1200;
  scanner->actres_pixels.right =
    scanner->actres_pixels.left + scanner->scan_width_pixels;
  scanner->actres_pixels.bottom =
    scanner->actres_pixels.top + scanner->scan_height_pixels;

  scanner->actual_mm.left =
    SCANNER_UNIT_TO_FIXED_MM (scanner->fullres_pixels.left);
  scanner->actual_mm.top =
    SCANNER_UNIT_TO_FIXED_MM (scanner->fullres_pixels.top);
  scanner->actual_mm.bottom =
    SCANNER_UNIT_TO_FIXED_MM (scanner->scan_width_pixels * 1200 /
			      scanner->resolution);
  scanner->actual_mm.right =
    SCANNER_UNIT_TO_FIXED_MM (scanner->scan_height_pixels * 1200 /
			      scanner->resolution);

  DBG (12, "calculateDerivedValues: ok\n");
}

/* From here on in we have the original code written for the scanner demo */

#define	MAX_COMMANDS_BYTES	131072
#define	MAX_READ_COMMANDS	1	/* Issuing more than one register
					 * read command in a single request
					 * seems to put the device in an
					 * unpredictable state.
					 */
#define	MAX_READ_BYTES		0xffc0

#define	REG_DESTINATION_POSITION 0x60
#define	REG_MOVE_CONTROL_TEST	0xb3

static int command_reads_outstanding = 0;
static int command_bytes_outstanding = 0;
static unsigned char command_buffer[MAX_COMMANDS_BYTES];
static int receive_bytes_outstanding = 0;
static char *command_readmem_outstanding[MAX_READ_COMMANDS];
static int command_readbytes_outstanding[MAX_READ_COMMANDS];
static unsigned char sram_access_method = 0;
static unsigned sram_size = 0;
static int udh;

static int
rt_execute_commands (void)
{
  SANE_Status result;
  size_t bytes;

  if (!command_bytes_outstanding)
    return 0;

  bytes = command_bytes_outstanding;

  result = sanei_usb_write_bulk (udh, /* 0x02, */ command_buffer, &bytes);

  if (result == SANE_STATUS_GOOD && receive_bytes_outstanding)
    {
      unsigned char readbuf[MAX_READ_BYTES];
      int total_read = 0;

      do
	{
	  bytes = receive_bytes_outstanding - total_read;
	  result = sanei_usb_read_bulk (udh,
					/* 0x81, */
					readbuf + total_read, &bytes);
	  if (result == SANE_STATUS_GOOD)
	    total_read += bytes;
	  else
	    break;
	}
      while (total_read < receive_bytes_outstanding);
      if (result == SANE_STATUS_GOOD)
	{
	  unsigned char *readptr;
	  int i;

	  for (i = 0, readptr = readbuf;
	       i < command_reads_outstanding;
	       readptr += command_readbytes_outstanding[i++])
	    {
	      memcpy (command_readmem_outstanding[i],
		      readptr, command_readbytes_outstanding[i]);
	    }
	}
    }
  receive_bytes_outstanding = command_reads_outstanding =
    command_bytes_outstanding = 0;
  return (result == SANE_STATUS_GOOD) ? 0 : -1;
}

static int
rt_queue_command (int command,
		  int reg,
		  int count,
		  int bytes, void const *data_, int readbytes, void *readdata)
{
  int len = 4 + bytes;
  unsigned char *buffer;
  unsigned char const *data = data_;

  /* We add "bytes" here to account for the possiblity that all of the
   * data bytes are 0xaa and hence require a following 0x00 byte.
   */
  if (command_bytes_outstanding + len + bytes > MAX_COMMANDS_BYTES ||
      (readbytes &&
       ((command_reads_outstanding >= MAX_READ_COMMANDS) ||
	(receive_bytes_outstanding >= MAX_READ_BYTES))))
    {
      if (rt_execute_commands () < 0)
	return -1;
    }

  buffer = command_buffer + command_bytes_outstanding;

  *buffer++ = command;
  *buffer++ = reg;
  *buffer++ = count >> 8;
  *buffer++ = count;
  while (bytes--)
    {
      *buffer++ = *data;
      if (*data++ == 0xaa)
	{
	  *buffer++ = 0;
	  ++len;
	}
    }
  command_bytes_outstanding += len;
  if (readbytes)
    {
      command_readbytes_outstanding[command_reads_outstanding] = readbytes;
      command_readmem_outstanding[command_reads_outstanding] = readdata;
      receive_bytes_outstanding += readbytes;
      ++command_reads_outstanding;
    }

  return 0;
}

static int
rt_send_command_immediate (int command,
			   int reg,
			   int count,
			   int bytes,
			   void *data, int readbytes, void *readdata)
{
  rt_queue_command (command, reg, count, bytes, data, readbytes, readdata);
  return rt_execute_commands ();
}

static int
rt_queue_read_register (int reg, int bytes, void *data)
{
  return rt_queue_command (RTCMD_GETREG, reg, bytes, 0, 0, bytes, data);
}

static int
rt_read_register_immediate (int reg, int bytes, void *data)
{
  if (rt_queue_read_register (reg, bytes, data) < 0)
    return -1;
  return rt_execute_commands ();
}

static int
rt_queue_set_register (int reg, int bytes, void *data)
{
  return rt_queue_command (RTCMD_SETREG, reg, bytes, bytes, data, 0, 0);
}

static int
rt_set_register_immediate (int reg, int bytes, void *data)
{
  if (reg < 0xb3 && reg + bytes > 0xb3)
    {
      int bytes_in_first_block = 0xb3 - reg;

      if (rt_set_register_immediate (reg, bytes_in_first_block, data) < 0 ||
	  rt_set_register_immediate (0xb4, bytes - bytes_in_first_block - 1,
				     (char *) data + bytes_in_first_block +
				     1) < 0)
	return -1;
      return 0;
    }
  if (rt_queue_set_register (reg, bytes, data) < 0)
    return -1;
  return rt_execute_commands ();
}

static int
rt_set_one_register (int reg, int val)
{
  char r = val;

  return rt_set_register_immediate (reg, 1, &r);
}

static int
rt_write_sram (int bytes, void *data_)
{
  unsigned char *data = (unsigned char *) data_;

  /* The number of bytes passed in could be much larger than we can transmit
   * (0xffc0) bytes. With 0xaa escapes it could be even larger. Accordingly
   * we need to count the 0xaa escapes and write in chunks if the number of
   * bytes would otherwise exceed a limit (I have used 0xf000 as the limit).
   */
  while (bytes > 0)
    {
      int now = 0;
      int bufsize = 0;

      while (now < bytes && bufsize < 0xf000)
	{
	  int i;

	  /* Try to avoid writing part pages */
	  for (i = 0; i < 32 && now < bytes; ++i)
	    {
	      ++bufsize;
	      if (data[now++] == 0xaa)
		++bufsize;
	    }
	}

      if (rt_send_command_immediate (RTCMD_WRITESRAM, 0, now, now, data, 0,
				     0) < 0)
	return -1;
      bytes -= now;
      data += now;
    }
  return 0;
}

static int
rt_read_sram (int bytes, void *data_)
{
  unsigned char *data = (unsigned char *) data_;

  while (bytes > 0)
    {
      int now = (bytes > 0xf000) ? 0xf000 : bytes;
      if (rt_send_command_immediate (RTCMD_READSRAM, 0, bytes, 0, 0, bytes,
				     data) < 0)
	return -1;
      bytes -= now;
      data += now;
    }
  return 0;
}

static int
rt_set_sram_page (int page)
{
  unsigned char regs[2];

  regs[0] = page;
  regs[1] = page >> 8;

  return rt_set_register_immediate (0x91, 2, regs);
}

static int
rt_detect_sram (unsigned *totalbytes, unsigned char *r93setting)
{
  char data[0x818];
  char testbuf[0x818];
  unsigned i;
  int test_values[] = { 6, 2, 1, -1 };

  for (i = 0; i < sizeof (data); ++i)
    data[i] = i % 0x61;


  for (i = 0; test_values[i] != -1; ++i)
    {
      if (rt_set_one_register (0x93, test_values[i]) ||
	  rt_set_sram_page (0x81) ||
	  rt_write_sram (0x818, data) ||
	  rt_set_sram_page (0x81) || rt_read_sram (0x818, testbuf))
	return -1;
      if (!memcmp (testbuf, data, 0x818))
	{
	  sram_access_method = test_values[i];
	  if (r93setting)
	    *r93setting = sram_access_method;
	  break;
	}
    }
  if (!sram_access_method)
    return -1;

  for (i = 0; i < 16; ++i)
    {
      int j;
      char write_data[32];
      char read_data[32];
      int pagesetting;

      for (j = 0; j < 16; j++)
	{
	  write_data[j * 2] = j * 2;
	  write_data[j * 2 + 1] = i;
	}

      pagesetting = i * 4096;


      if (rt_set_sram_page (pagesetting) < 0 ||
	  rt_write_sram (32, write_data) < 0)
	return -1;
      if (i)
	{
	  if (rt_set_sram_page (0) < 0 || rt_read_sram (32, read_data) < 0)
	    return -1;
	  if (!memcmp (read_data, write_data, 32))
	    {
	      sram_size = i * 0x20000;
	      if (totalbytes)
		*totalbytes = sram_size;
	      return 0;
	    }
	}
    }
  return -1;
}

static int
rt_get_available_bytes (void)
{
  unsigned char data[3];

  if (rt_queue_command (RTCMD_BYTESAVAIL, 0, 3, 0, 0, 3, data) < 0 ||
      rt_execute_commands () < 0)
    return -1;
  return ((unsigned) data[0]) |
    ((unsigned) data[1] << 8) | ((unsigned) data[2] << 16);
}

static int
rt_get_data (int bytes, void *data)
{
  int total = 0;

  while (bytes)
    {
      int bytesnow = bytes;

      if (bytesnow > 0xffc0)
	bytesnow = 0xffc0;
      if (rt_queue_command
	  (RTCMD_READBYTES, 0, bytesnow, 0, 0, bytesnow, data) < 0
	  || rt_execute_commands () < 0)
	return -1;
      total += bytesnow;
      bytes -= bytesnow;
      data = (char *) data + bytesnow;
    }
  return 0;
}

static int
rt_is_moving (void)
{
  char r;

  if (rt_read_register_immediate (REG_MOVE_CONTROL_TEST, 1, &r) < 0)
    return -1;
  if (r == 0x08)
    return 1;
  return 0;
}

static int
rt_is_rewound (void)
{
  char r;

  if (rt_read_register_immediate (0x1d, 1, &r) < 0)
    return -1;
  if (r & 0x02)
    return 1;
  return 0;
}

static int
rt_set_direction_forwards (unsigned char *regs)
{
  regs[0xc6] |= 0x08;
  return 0;
}

static int
rt_set_direction_rewind (unsigned char *regs)
{
  regs[0xc6] &= 0xf7;
  return 0;
}

static int
rt_set_stop_when_rewound (unsigned char *regs, int stop)
{
  if (stop)
    regs[0xb2] |= 0x10;
  else
    regs[0xb2] &= 0xef;
  return 0;
}

static int
rt_start_moving (void)
{
  if (rt_set_one_register (REG_MOVE_CONTROL_TEST, 2) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 2) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 0) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 0) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 8) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 8) < 0)
    return -1;
  return 0;
}

static int
rt_stop_moving (void)
{
  if (rt_set_one_register (REG_MOVE_CONTROL_TEST, 2) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 2) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 0) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, 0) < 0)
    return -1;
  return 0;
}

static int
rt_set_powersave_mode (int enable)
{
  unsigned char r;

  if (rt_read_register_immediate (REG_MOVE_CONTROL_TEST, 1, &r) < 0)
    return -1;
  if (r & 0x04)
    {
      if (enable == 1)
	return 0;
      r &= ~0x04;
    }
  else
    {
      if (enable == 0)
	return 0;
      r |= 0x04;
    }
  if (rt_set_one_register (REG_MOVE_CONTROL_TEST, r) < 0 ||
      rt_set_one_register (REG_MOVE_CONTROL_TEST, r) < 0)
    return -1;
  return 0;
}

static int
rt_turn_off_lamp (void)
{
  return rt_set_one_register (0x3a, 0);
}

static int
rt_turn_on_lamp (void)
{
  char r3ab[2];
  char r10;
  char r58;

  if (rt_read_register_immediate (0x3a, 1, r3ab) < 0 ||
      rt_read_register_immediate (0x10, 1, &r10) < 0 ||
      rt_read_register_immediate (0x58, 1, &r58) < 0)
    return -1;
  r3ab[0] |= 0x80;
  r3ab[1] = 0x40;
  r10 |= 0x01;
  r58 &= 0x0f;
  if (rt_set_register_immediate (0x3a, 2, r3ab) < 0 ||
      rt_set_one_register (0x10, r10) < 0 ||
      rt_set_one_register (0x58, r58) < 0)
    return -1;
  return 0;
}

static int
rt_set_value_lsbfirst (unsigned char *regs,
		       int firstreg, int totalregs, unsigned value)
{
  while (totalregs--)
    {
      regs[firstreg++] = value & 0xff;
      value >>= 8;
    }
  return 0;
}

#if 0
static int
rt_set_value_msbfirst (unsigned char *regs,
		       int firstreg, int totalregs, unsigned value)
{
  while (totalregs--)
    {
      regs[firstreg + totalregs] = value & 0xff;
      value >>= 8;
    }
  return 0;
}
#endif

static int
rt_set_ccd_shift_clock_multiplier (unsigned char *regs, unsigned value)
{
  return rt_set_value_lsbfirst (regs, 0xf0, 3, value);
}

static int
rt_set_ccd_clock_reset_interval (unsigned char *regs, unsigned value)
{
  return rt_set_value_lsbfirst (regs, 0xf9, 3, value);
}

static int
rt_set_ccd_clamp_clock_multiplier (unsigned char *regs, unsigned value)
{
  return rt_set_value_lsbfirst (regs, 0xfc, 3, value);
}

static int
rt_set_movement_pattern (unsigned char *regs, unsigned value)
{
  return rt_set_value_lsbfirst (regs, 0xc0, 3, value);
}

static int
rt_set_motor_movement_clock_multiplier (unsigned char *regs, unsigned value)
{
  regs[0x40] = (regs[0x40] & ~0xc0) | (value << 6);
  return 0;
}

static int
rt_set_motor_type (unsigned char *regs, unsigned value)
{
  regs[0xc9] = (regs[0xc9] & 0xf8) | (value & 0x7);
  return 0;
}

static int
rt_set_noscan_distance (unsigned char *regs, unsigned value)
{
  DBG (10, "Setting distance without scanning to %d\n", value);
  return rt_set_value_lsbfirst (regs, 0x60, 2, value);
}

static int
rt_set_total_distance (unsigned char *regs, unsigned value)
{
  DBG (10, "Setting total distance to %d\n", value);
  return rt_set_value_lsbfirst (regs, 0x62, 2, value);
}

static int
rt_set_scanline_start (unsigned char *regs, unsigned value)
{
  return rt_set_value_lsbfirst (regs, 0x66, 2, value);
}

static int
rt_set_scanline_end (unsigned char *regs, unsigned value)
{
  return rt_set_value_lsbfirst (regs, 0x6c, 2, value);
}

static int
rt_set_basic_calibration (unsigned char *regs,
			  int redoffset1,
			  int redoffset2,
			  int redgain,
			  int greenoffset1,
			  int greenoffset2,
			  int greengain,
			  int blueoffset1, int blueoffset2, int bluegain)
{
  regs[0x05] = redoffset1;
  regs[0x02] = redoffset2;
  regs[0x08] = redgain;
  regs[0x06] = greenoffset1;
  regs[0x03] = greenoffset2;
  regs[0x09] = greengain;
  regs[0x07] = blueoffset1;
  regs[0x04] = blueoffset2;
  regs[0x0a] = bluegain;
  return 0;
}

static int
rt_set_calibration_addresses (unsigned char *regs,
			      unsigned redaddr,
			      unsigned greenaddr,
			      unsigned blueaddr, unsigned endaddr)
{
  regs[0x84] = redaddr;
  regs[0x8e] = (regs[0x8e] & 0x0f) | ((redaddr >> 4) & 0xf0);
  rt_set_value_lsbfirst (regs, 0x85, 2, greenaddr);
  rt_set_value_lsbfirst (regs, 0x87, 2, blueaddr);
  rt_set_value_lsbfirst (regs, 0x89, 2, (endaddr + 31) / 32);
  return 0;
}

static int
rt_set_lamp_duty_cycle (unsigned char *regs,
			int enable, int frequency, int offduty)
{
  if (enable)
    regs[0x3b] |= 0x80;
  else
    regs[0x3b] &= 0x7f;

  regs[0x3b] =
    (regs[0x3b] & 0x80) | ((frequency & 0x7) << 4) | (offduty & 0x0f);
  regs[0x3d] = (regs[0x3d] & 0x7f) | ((frequency & 0x8) << 4);
  return 0;
}

static int
rt_set_data_feed_on (unsigned char *regs)
{
  regs[0xb2] &= ~0x04;
  return 0;
}

static int
rt_enable_ccd (unsigned char *regs, int enable)
{
  if (enable)
    regs[0x00] &= ~0x10;
  else
    regs[0x00] |= 0x10;
  return 0;
}

static int
rt_set_cdss (unsigned char *regs, int val1, int val2)
{
  regs[0x28] = (regs[0x28] & 0xe0) | (val1 & 0x1f);
  regs[0x2a] = (regs[0x2a] & 0xe0) | (val2 & 0x1f);
  return 0;
}

static int
rt_set_cdsc (unsigned char *regs, int val1, int val2)
{
  regs[0x29] = (regs[0x29] & 0xe0) | (val1 & 0x1f);
  regs[0x2b] = (regs[0x2b] & 0xe0) | (val2 & 0x1f);
  return 0;
}

static int
rt_update_after_setting_cdss2 (unsigned char *regs)
{
  int fullcolour = (!(regs[0x2f] & 0xc0) && (regs[0x2f] & 0x04));
  int value = regs[0x2a] & 0x1f;

  regs[0x2a] = (regs[0x2a] & 0xe0) | (value & 0x1f);

  if (fullcolour)
    value *= 3;
  if ((regs[0x40] & 0xc0) == 0x40)
    value += 17;
  else
    value += 16;

  regs[0x2c] = (regs[0x2c] & 0xe0) | (value % 24);
  regs[0x2d] = (regs[0x2d] & 0xe0) | ((value + 2) % 24);
  return 0;
}

static int
rt_set_cph0s (unsigned char *regs, int on)
{
  if (on)
    regs[0x2d] |= 0x20;		/* 1200dpi horizontal coordinate space */
  else
    regs[0x2d] &= ~0x20;	/* 600dpi horizontal coordinate space */
  return 0;
}

static int
rt_set_cvtr_lm (unsigned char *regs, int val1, int val2, int val3)
{
  regs[0x28] = (regs[0x28] & ~0xe0) | (val1 << 5);
  regs[0x29] = (regs[0x29] & ~0xe0) | (val2 << 5);
  regs[0x2a] = (regs[0x2a] & ~0xe0) | (val3 << 5);
  return 0;
}

static int
rt_set_cvtr_mpt (unsigned char *regs, int val1, int val2, int val3)
{
  regs[0x3c] = (val1 & 0x0f) | (val2 << 4);
  regs[0x3d] = (regs[0x3d] & 0xf0) | (val3 & 0x0f);
  return 0;
}

static int
rt_set_cvtr_wparams (unsigned char *regs,
		     unsigned fpw, unsigned bpw, unsigned w)
{
  regs[0x31] = (w & 0x0f) | ((bpw << 4) & 0x30) | (fpw << 6);
  return 0;
}

static int
rt_enable_movement (unsigned char *regs, int enable)
{
  if (enable)
    regs[0xc3] |= 0x80;
  else
    regs[0xc3] &= ~0x80;
  return 0;
}

static int
rt_set_scan_frequency (unsigned char *regs, int frequency)
{
  regs[0x64] = (regs[0x64] & 0xf0) | (frequency & 0x0f);
  return 0;
}

static int
rt_set_merge_channels (unsigned char *regs, int on)
{
  /* RGBRGB instead of RRRRR...GGGGG...BBBB */
  regs[0x2f] &= ~0x14;
  regs[0x2f] |= on ? 0x04 : 0x10;
  return 0;
}

static int
rt_set_channel (unsigned char *regs, int channel)
{
  regs[0x2f] = (regs[0x2f] & ~0xc0) | (channel << 6);
  return 0;
}

static int
rt_set_single_channel_scanning (unsigned char *regs, int on)
{
  if (on)
    regs[0x2f] |= 0x20;
  else
    regs[0x2f] &= ~0x20;
  return 0;
}

static int
rt_set_colour_mode (unsigned char *regs, int on)
{
  if (on)
    regs[0x2f] |= 0x02;
  else
    regs[0x2f] &= ~0x02;
  return 0;
}

static int
rt_set_horizontal_resolution (unsigned char *regs, int resolution)
{
  int base_resolution = 300;

  if (regs[0x2d] & 0x20)
    base_resolution *= 2;
  if (regs[0xd3] & 0x08)
    base_resolution *= 2;
  regs[0x7a] = base_resolution / resolution;
  return 0;
}

static int
rt_set_last_sram_page (unsigned char *regs, int pagenum)
{
  rt_set_value_lsbfirst (regs, 0x8b, 2, pagenum);
  return 0;
}

static int
rt_set_step_size (unsigned char *regs, int stepsize)
{
  rt_set_value_lsbfirst (regs, 0xe2, 2, stepsize);
  rt_set_value_lsbfirst (regs, 0xe0, 2, 0);
  return 0;
}

static int
rt_set_all_registers (void const *regs_)
{
  char regs[255];

  memcpy (regs, regs_, 255);
  regs[0x32] &= ~0x40;

  if (rt_set_one_register (0x32, regs[0x32]) < 0 ||
      rt_set_register_immediate (0, 255, regs) < 0 ||
      rt_set_one_register (0x32, regs[0x32] | 0x40) < 0)
    return -1;
  return 0;
}

static int
rt_adjust_misc_registers (unsigned char *regs)
{
  /* Mostly unknown purposes - probably no need to adjust */
  regs[0xc6] = (regs[0xc6] & 0x0f) | 0x20;	/* Purpose unknown - appears to do nothing */
  regs[0x2e] = 0x86;		/* ???? - Always has this value */
  regs[0x30] = 2;		/* CCPL = 1 */
  regs[0xc9] |= 0x38;		/* Doesn't have any obvious effect, but the Windows driver does this */
  return 0;
}


#define NVR_MAX_ADDRESS_SIZE	11
#define NVR_MAX_OPCODE_SIZE	3
#define NVR_DATA_SIZE		8
#define	NVR_MAX_COMMAND_SIZE	((NVR_MAX_ADDRESS_SIZE + \
				  NVR_MAX_OPCODE_SIZE + \
				  NVR_DATA_SIZE) * 2 + 1)

static int
rt_nvram_enable_controller (int enable)
{
  unsigned char r;

  if (rt_read_register_immediate (0x1d, 1, &r) < 0)
    return -1;
  if (enable)
    r |= 1;
  else
    r &= ~1;
  return rt_set_one_register (0x1d, r);

}

static int
rt_nvram_init_command (void)
{
  unsigned char regs[13];

  if (rt_read_register_immediate (0x10, 13, regs) < 0)
    return -1;
  regs[2] |= 0xf0;
  regs[4] = (regs[4] & 0x1f) | 0x60;
  return rt_set_register_immediate (0x10, 13, regs);
}

static int
rt_nvram_init_stdvars (int block, int *addrbits, unsigned char *basereg)
{
  int bitsneeded;
  int capacity;

  switch (block)
    {
    case 0:
      bitsneeded = 7;
      break;

    case 1:
      bitsneeded = 9;
      break;

    case 2:
      bitsneeded = 11;
      break;

    default:
      bitsneeded = 0;
      capacity = 1;
      while (capacity < block)
	capacity <<= 1, ++bitsneeded;
      break;
    }

  *addrbits = bitsneeded;

  if (rt_read_register_immediate (0x10, 1, basereg) < 0)
    return -1;

  *basereg &= ~0x60;
  return 0;
}

static void
rt_nvram_set_half_bit (unsigned char *buffer,
		       int value, unsigned char stdbits, int whichhalf)
{
  *buffer = stdbits | (value ? 0x40 : 0) | (whichhalf ? 0x20 : 0);
}

static void
rt_nvram_set_command_bit (unsigned char *buffer,
			  int value, unsigned char stdbits)
{
  rt_nvram_set_half_bit (buffer, value, stdbits, 0);
  rt_nvram_set_half_bit (buffer + 1, value, stdbits, 1);
}

static void
rt_nvram_set_addressing_bits (unsigned char *buffer,
			      int location,
			      int addressingbits, unsigned char stdbits)
{
  int currentbit = 1 << (addressingbits - 1);

  while (addressingbits--)
    {
      rt_nvram_set_command_bit (buffer,
				(location & currentbit) ? 1 : 0, stdbits);
      buffer += 2;
      currentbit >>= 1;
    }
}

#if 0
static int
rt_nvram_enable_write (int addressingbits, int enable, unsigned char stdbits)
{
  unsigned char cmdbuffer[NVR_MAX_COMMAND_SIZE];
  int cmdsize = 6 + addressingbits * 2;

  rt_nvram_set_command_bit (cmdbuffer, 1, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 2, 0, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 4, 0, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 6, enable, stdbits);
  if (addressingbits > 1)
    rt_nvram_set_addressing_bits (cmdbuffer + 8, 0, addressingbits - 1,
				  stdbits);

  if (rt_nvram_enable_controller (1) < 0 ||
      rt_send_command_immediate (RTCMD_NVRAMCONTROL, 0, cmdsize, cmdsize,
				 cmdbuffer, 0, 0) < 0
      || rt_nvram_enable_controller (0) < 0)
    {
      return -1;
    }
  return 0;
}

static int
rt_nvram_write (int block, int location, char const *data, int bytes)
{
  int addressingbits;
  unsigned char stdbits;
  unsigned char cmdbuffer[NVR_MAX_COMMAND_SIZE];
  unsigned char *address_bits;
  unsigned char *data_bits;
  int cmdsize;

  /* This routine doesn't appear to work, but I can't see anything wrong with it */
  if (rt_nvram_init_stdvars (block, &addressingbits, &stdbits) < 0)
    return -1;

  cmdsize = (addressingbits + 8) * 2 + 6;
  address_bits = cmdbuffer + 6;
  data_bits = address_bits + (addressingbits * 2);

  rt_nvram_set_command_bit (cmdbuffer, 1, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 2, 0, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 4, 1, stdbits);

  if (rt_nvram_init_command () < 0 ||
      rt_nvram_enable_write (addressingbits, 1, stdbits) < 0)
    return -1;

  while (bytes--)
    {
      int i;

      rt_nvram_set_addressing_bits (address_bits, location, addressingbits,
				    stdbits);
      rt_nvram_set_addressing_bits (data_bits, *data++, 8, stdbits);

      if (rt_nvram_enable_controller (1) < 0 ||
	  rt_send_command_immediate (RTCMD_NVRAMCONTROL, 0, cmdsize, cmdsize,
				     cmdbuffer, 0, 0) < 0
	  || rt_nvram_enable_controller (0) < 0)
	return -1;

      if (rt_nvram_enable_controller (1) < 0)
	return -1;
      for (i = 0; i < cmdsize; ++i)
	{
	  unsigned char r;
	  unsigned char cmd;

	  rt_nvram_set_half_bit (&cmd, 0, stdbits, i & 1);
	  if (rt_send_command_immediate
	      (RTCMD_NVRAMCONTROL, 0, 1, 1, &cmd, 0, 0) < 0
	      || rt_read_register_immediate (0x10, 1, &r) < 0)
	    {
	      return -1;
	    }
	  else if (r & 0x80)
	    {
	      break;
	    }
	}
      if (rt_nvram_enable_controller (0) < 0)
	return -1;

      ++location;
    }

  if (rt_nvram_enable_write (addressingbits, 0, stdbits) < 0)
    return -1;
  return 0;
}
#endif

static int
rt_nvram_read (int block, int location, unsigned char *data, int bytes)
{
  int addressingbits;
  unsigned char stdbits;
  unsigned char cmdbuffer[NVR_MAX_COMMAND_SIZE];
  unsigned char *address_bits;
  unsigned char readbit_command[2];
  int cmdsize;

  if (rt_nvram_init_stdvars (block, &addressingbits, &stdbits) < 0)
    return -1;

  cmdsize = addressingbits * 2 + 7;
  address_bits = cmdbuffer + 6;

  rt_nvram_set_command_bit (cmdbuffer, 1, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 2, 1, stdbits);
  rt_nvram_set_command_bit (cmdbuffer + 4, 0, stdbits);
  rt_nvram_set_half_bit (cmdbuffer + cmdsize - 1, 0, stdbits, 0);

  rt_nvram_set_half_bit (readbit_command, 0, stdbits, 1);
  rt_nvram_set_half_bit (readbit_command + 1, 0, stdbits, 0);

  if (rt_nvram_init_command () < 0)
    return -1;

  while (bytes--)
    {
      char c = 0;
      unsigned char r;
      int i;

      rt_nvram_set_addressing_bits (address_bits, location, addressingbits,
				    stdbits);

      if (rt_nvram_enable_controller (1) < 0 ||
	  rt_send_command_immediate (RTCMD_NVRAMCONTROL, 0x1d, cmdsize,
				     cmdsize, cmdbuffer, 0, 0) < 0)
	return -1;

      for (i = 0; i < 8; ++i)
	{
	  c <<= 1;

	  if (rt_send_command_immediate
	      (RTCMD_NVRAMCONTROL, 0x1d, 2, 2, readbit_command, 0, 0) < 0
	      || rt_read_register_immediate (0x10, 1, &r) < 0)
	    return -1;
	  if (r & 0x80)
	    c |= 1;
	}
      if (rt_nvram_enable_controller (0) < 0)
	return -1;

      *data++ = c;
      ++location;
    }
  return 0;
}

static unsigned char initial_regs[] = {
  /* 0x00 */ 0xf5, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x08 */ 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00,
  /* 0x10 */ 0xe1, 0xfc, 0xff, 0xff, 0x00, 0x00, 0x00, 0xfc,
  /* 0x18 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
  /* 0x20 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x28 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x06, 0x19,
  /* 0x30 */ 0xd0, 0x7a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x38 */ 0x00, 0x00, 0xa0, 0x37, 0xff, 0x0f, 0x00, 0x00,
  /* 0x40 */ 0x80, 0x00, 0x00, 0x00, 0x8c, 0x76, 0x00, 0x00,
  /* 0x48 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x50 */ 0x20, 0xbc, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x58 */ 0x1d, 0x1f, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00,
  /* 0x60 */ 0x5e, 0xea, 0x5f, 0xea, 0x00, 0x80, 0x64, 0x00,
  /* 0x68 */ 0x00, 0x00, 0x00, 0x00, 0x84, 0x04, 0x00, 0x00,
  /* 0x70 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x78 */ 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x80 */ 0x0f, 0x02, 0x4b, 0x02, 0x00, 0xec, 0x19, 0xd8,
  /* 0x88 */ 0x2d, 0x87, 0x02, 0xff, 0x3f, 0x78, 0x60, 0x00,
  /* 0x90 */ 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x98 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0xa0 */ 0x00, 0x00, 0x00, 0x0c, 0x27, 0x64, 0x00, 0x00,
  /* 0xa8 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0xb0 */ 0x12, 0x08, 0x06, 0x04, 0x00, 0x00, 0x00, 0x00,
  /* 0xb8 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0xc0 */ 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00,
  /* 0xc8 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0xd0 */ 0xff, 0xbf, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff,
  /* 0xd8 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0xe0 */ 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0xe8 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0xf0 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0xf8 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#define RT_NORMAL_TG 0
#define RT_DOUBLE_TG 1
#define RT_TRIPLE_TG 2
#define RT_DDOUBLE_TG 3
#define RT_300_TG 4
#define RT_150_TG 5
#define RT_TEST_TG 6
static struct tg_info__
{
  int tg_cph0p;
  int tg_crsp;
  int tg_cclpp;
  int tg_cph0s;
  int tg_cdss1;
  int tg_cdsc1;
  int tg_cdss2;
  int tg_cdsc2;
} tg_info[] =
{
  /* CPH              CCD Shifting Clock
   *    0P            ??? Perhaps CCD rising edge position
   *    0S            ???
   * CRS              Reset CCD Clock
   *    P             ??? Perhaps CCD falling edge position
   * CCLP             CCD Clamp Clock
   *     P            ???
   * CDS              ???
   *    S1            ???
   *    S2            ???
   *    C1            ???
   *    C2            ???
   */
  /*CPH0P     CRSP      CCLPP     CPH0S CDSS1 CDSC1 CDSS2 CDSC2 */
  {
  0x01FFE0, 0x3c0000, 0x003000, 1, 0xb, 0xd, 0x00, 0x01},	/* NORMAL */
  {
  0x7ff800, 0xf00000, 0x01c000, 0, 0xb, 0xc, 0x14, 0x15},	/* DOUBLE */
  {
  0x033fcc, 0x300000, 0x060000, 1, 0x8, 0xa, 0x00, 0x01},	/* TRIPLE */
  {
  0x028028, 0x300000, 0x060000, 1, 0x8, 0xa, 0x00, 0x01},	/* DDOUBLE */
  {
  0x7ff800, 0x030000, 0x060000, 0, 0xa, 0xc, 0x17, 0x01},	/* 300 */
  {
  0x7fc700, 0x030000, 0x060000, 0, 0x7, 0x9, 0x17, 0x01},	/* 150 */
  {
  0x7ff800, 0x300000, 0x060000, 0, 0xa, 0xc, 0x17, 0x01},	/* TEST */
};

struct resolution_parameters
{
  unsigned resolution;
  int reg_39_value;
  int reg_c3_value;
  int reg_c6_value;
  int scan_frequency;
  int cph0s;
  int red_green_offset;
  int green_blue_offset;
  int intra_channel_offset;
  int motor_movement_clock_multiplier;
  int d3_bit_3_value;
  int tg;
  int step_size;
};

/* The TG value sets seem to affect the exposure time:
 * At 200dpi:
 * NORMAL gets higher values than DOUBLE
 * DDOUBLE gives a crazy spike in the data
 * TRIPLE gives a black result
 * TEST gives a black result
 * 300 gives a black result
 * 150 gives a black result
 */

static struct resolution_parameters resparms[] = {
  /* Acceptable values for stepsz are:
   * 0x157b 0xabd, 0x55e, 0x2af, 0x157, 0xab, 0x55
   */
  /* My values - all work */
  /*res   r39 rC3 rC6 freq cph0s rgo gbo intra mmcm d3 tg            stepsz */
  {1200, 3, 6, 4, 2, 1, 22, 22, 4, 2, 1, RT_NORMAL_TG, 0x157b},
  {600, 15, 6, 4, 1, 0, 9, 10, 0, 2, 1, RT_NORMAL_TG, 0x055e},
  {400, 3, 1, 4, 1, 1, 6, 6, 1, 2, 1, RT_NORMAL_TG, 0x157b},
  {300, 15, 3, 4, 1, 0, 5, 4, 0, 2, 1, RT_NORMAL_TG, 0x02af},
  {200, 7, 1, 4, 1, 0, 3, 3, 0, 2, 1, RT_NORMAL_TG, 0x055e},
  {150, 15, 3, 1, 1, 0, 2, 2, 0, 2, 1, RT_NORMAL_TG, 0x02af},
  {100, 3, 1, 3, 1, 0, 1, 1, 0, 2, 1, RT_NORMAL_TG, 0x0abd},
  {75, 15, 3, 3, 1, 0, 1, 1, 0, 2, 1, RT_NORMAL_TG, 0x02af},
  {50, 15, 1, 1, 1, 0, 0, 0, 0, 2, 1, RT_NORMAL_TG, 0x055e},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

struct dcalibdata
{
  unsigned char *buffers[3];
  int pixelsperrow;
  int pixelnow;
  int channelnow;
  int firstrowdone;
};

static void dump_registers (unsigned char const *);
static int
rts8801_rewind (void)
{
  unsigned char regs[255];
  int n;
  int tg_setting = RT_DOUBLE_TG;

  rt_read_register_immediate (0, 255, regs);

  rt_set_noscan_distance (regs, 59998);
  rt_set_total_distance (regs, 59999);

  rt_set_stop_when_rewound (regs, 0);

  rt_set_one_register (0xc6, 0);
  rt_set_one_register (0xc6, 0);


  rt_set_direction_rewind (regs);

  rt_set_step_size (regs, 0x55);
  regs[0x39] = 3;
  regs[0xc3] = (regs[0xc3] & 0xf8) | 0x86;
  regs[0xc6] = (regs[0xc6] & 0xf8) | 4;

  rt_set_horizontal_resolution (regs, 25);
  rt_set_ccd_shift_clock_multiplier (regs, tg_info[tg_setting].tg_cph0p);
  rt_set_ccd_clock_reset_interval (regs, tg_info[tg_setting].tg_crsp);
  rt_set_ccd_clamp_clock_multiplier (regs, tg_info[tg_setting].tg_cclpp);
  rt_set_cdss (regs, tg_info[tg_setting].tg_cdss1,
	       tg_info[tg_setting].tg_cdss2);
  rt_set_cdsc (regs, tg_info[tg_setting].tg_cdsc1,
	       tg_info[tg_setting].tg_cdsc2);
  rt_update_after_setting_cdss2 (regs);
  rt_set_cvtr_wparams (regs, 3, 0, 6);
  rt_set_cvtr_mpt (regs, 15, 15, 15);
  rt_set_cvtr_lm (regs, 7, 7, 7);
  rt_set_motor_type (regs, 2);

  if (DBG_LEVEL >= 5)
    dump_registers (regs);

  rt_set_all_registers (regs);
  rt_set_one_register (0x2c, regs[0x2c]);

  rt_start_moving ();

  while (!rt_is_rewound () &&
	 ((n = rt_get_available_bytes ()) > 0 || rt_is_moving () > 0))
    {
      if (n)
	{
	  char buffer[0xffc0];

	  if (n > (int) sizeof (buffer))
	    n = sizeof (buffer);
	  rt_get_data (n, buffer);
	}
      else
	{
	  usleep (10000);
	}
    }

  rt_stop_moving ();
  return 0;
}

static int cancelled_scan = 0;

static unsigned
get_lsbfirst_int (unsigned char const *p, int n)
{
  unsigned value = *p++;
  int shift = 8;

  while (--n)
    {
      unsigned now = *p++;
      value |= now << shift;
      shift += 8;
    }
  return value;
}

static int
convert_c6 (int i)
{
  switch (i)
    {
    case 3:
      return 1;

    case 1:
      return 2;

    case 4:
      return 4;
    }
  return -1;
}

static void
dump_registers (unsigned char const *regs)
{
  int i = 0;
  long pixels;

  DBG (5, "Scan commencing with registers:\n");
  while (i < 255)
    {
      int j = 0;
      char buffer[80];

      buffer[0] = 0;

      sprintf (buffer + strlen (buffer), "%02x:", i);
      while (j < 8)
	{
	  sprintf (buffer + strlen (buffer), " %02x", regs[i++]);
	  j++;
	}
      sprintf (buffer + strlen (buffer), " -");
      while (j++ < 16 && i < 255)
	sprintf (buffer + strlen (buffer), " %02x", regs[i++]);
      DBG (5, "    %s\n", buffer);
    }

  DBG (5, "  Position:\n");
  DBG (5, "    Distance without scanning:       %u\n",
       get_lsbfirst_int (regs + 0x60, 2));
  DBG (5, "    Total distance:                  %u\n",
       get_lsbfirst_int (regs + 0x62, 2));
  DBG (5, "    Scanning distance:               %u\n",
       get_lsbfirst_int (regs + 0x62, 2) - get_lsbfirst_int (regs + 0x60, 2));
  DBG (5, "    Direction:                       %s\n",
       (regs[0xc6] & 0x08) ? "forward" : "rewind");
  DBG (5, "    Motor:                           %s\n",
       (regs[0xc3] & 0x80) ? "enabled" : "disabled");
  if (regs[0x7a])
    DBG (5, "    X range:                         %u-%u\n",
	 get_lsbfirst_int (regs + 0x66, 2) / regs[0x7a],
	 get_lsbfirst_int (regs + 0x6c, 2) / regs[0x7a]);
  DBG (5, "  TG Info:\n");
  DBG (5, "    CPH0P:                           %06x\n",
       get_lsbfirst_int (regs + 0xf0, 3));
  DBG (5, "    CRSP:                            %06x\n",
       get_lsbfirst_int (regs + 0xf9, 3));
  DBG (5, "    CCLPP:                           %06x\n",
       get_lsbfirst_int (regs + 0xfc, 3));
  DBG (5, "    CPH0S:                           %d\n",
       (regs[0x2d] & 0x20) ? 1 : 0);
  DBG (5, "    CDSS1:                           %02x\n", regs[0x28] & 0x1f);
  DBG (5, "    CDSC1:                           %02x\n", regs[0x29] & 0x1f);
  DBG (5, "    CDSS2:                           %02x\n", regs[0x2a] & 0x1f);
  DBG (5, "    CDSC2:                           %02x\n", regs[0x2b] & 0x1f);

  DBG (5, "  Resolution specific:\n");
  if (!regs[0x7a])
    DBG (5, "    Horizontal resolution:           Denominator is zero!\n");
  else
    DBG (5, "    Horizontal resolution:           %u\n", 300
	 * ((regs[0x2d] & 0x20) ? 2 : 1)
	 * ((regs[0xd3] & 0x08) ? 2 : 1) / regs[0x7a]);
  DBG (5, "    Derived vertical resolution:     %u\n",
       400 * (regs[0xc3] & 0x1f) * convert_c6 (regs[0xc6] & 0x7) /
       (regs[0x39] + 1));
  DBG (5, "    Register D3:3                    %u\n",
       (regs[0xd3] & 0x08) ? 1 : 0);
  DBG (5, "    Register 39:                     %u\n", regs[0x39]);
  DBG (5, "    Register C3:0-5:                 %u\n", regs[0xc3] & 0x1f);
  DBG (5, "    Register C6:0-2:                 %u\n", regs[0xc6] & 0x7);
  DBG (5, "    Motor movement clock multiplier: %u\n", regs[0x40] >> 6);
  DBG (5, "    Step Size:                       %04x\n",
       get_lsbfirst_int (regs + 0xe2, 2));
  DBG (5, "    Frequency:                       %u\n", regs[0x64] & 0xf);
  DBG (5, "  Colour registers\n");
  DBG (5, "    Register 2F:                     %02x\n", regs[0x2f]);
  DBG (5, "    Register 2C:                     %02x\n", regs[0x2c]);
  if (regs[0x7a])
    {
      DBG (5, "  Scan data estimates:\n");
      pixels =
	(long) (get_lsbfirst_int (regs + 0x62, 2) -
		get_lsbfirst_int (regs + 0x60,
				  2)) * (long) (get_lsbfirst_int (regs + 0x6c,
								  2) -
						get_lsbfirst_int (regs + 0x66,
								  2)) /
	regs[0x7a];
      DBG (5, "    Pixels:                          %ld\n", pixels);
      DBG (5, "    Bytes at 24BPP:                  %ld\n", pixels * 3);
      DBG (5, "    Bytes at 1BPP:                   %ld\n", pixels / 8);
    }
  DBG (5, "\n");
}

static int
constrain (int val, int min, int max)
{
  if (val < min)
    {
      DBG (10, "Clipped %d to %d\n", val, min);
      val = min;
    }
  else if (val > max)
    {
      DBG (10, "Clipped %d to %d\n", val, max);
      val = max;
    }
  return val;
}


static int
rts8801_doscan (unsigned width,
		unsigned height,
		unsigned colour,
		unsigned red_green_offset,
		unsigned green_blue_offset,
		unsigned intra_channel_offset,
		rts8801_callback cbfunc,
		void *params,
		int oddfirst,
		unsigned char const *calib_info,
		int merged_channels,
		detailed_calibration_data const *detailed_calib_data)
{
  unsigned rowbytes = 0;
  unsigned output_rowbytes = 0;
  unsigned channels = 0;
  unsigned total_rows = 0;
  unsigned bytesperchannel;
  unsigned char *row_buffer;
  unsigned char *output_buffer;
  unsigned buffered_rows;
  int rows_to_begin;
  int rowbuffer_bytes;
  int n;
  unsigned rownow = 0;
  unsigned bytenow = 0;
  unsigned char *channel_data[3][2];
  unsigned i;
  unsigned j;
  int result = 0;
  unsigned rows_supplied = 0;

  calib_info = calib_info;	/* Kill warning */
  if (cancelled_scan)
    return -1;
  rt_start_moving ();

  channels = 3;
  rowbytes = width * 3;
  bytesperchannel = width;
  switch (colour)
    {
    case HP3500_GRAY_SCAN:
      output_rowbytes = width;
      break;

    case HP3500_COLOR_SCAN:
      output_rowbytes = rowbytes;
      break;

    case HP3500_LINEART_SCAN:
      output_rowbytes = (width + 7) / 8;
      break;
    }

  buffered_rows =
    red_green_offset + green_blue_offset + intra_channel_offset + 1;
  rows_to_begin = buffered_rows;
  rowbuffer_bytes = buffered_rows * rowbytes;
  row_buffer = (unsigned char *) malloc (rowbuffer_bytes);
  output_buffer = (unsigned char *) malloc (rowbytes);

  for (i = j = 0; i < channels; ++i)
    {
      if (i == 1)
	j += red_green_offset;
      else if (i == 2)
	j += green_blue_offset;
      if (merged_channels)
	channel_data[i][1 - oddfirst] = row_buffer + rowbytes * j + i;
      else
	channel_data[i][1 - oddfirst] = row_buffer + rowbytes * j + width * i;
      channel_data[i][oddfirst] =
	channel_data[i][1 - oddfirst] + rowbytes * intra_channel_offset;
    }

  while (((n = rt_get_available_bytes ()) > 0 || rt_is_moving () > 0)
	 && !cancelled_scan)
    {
      if (n == 1 && (rt_is_moving () || rt_get_available_bytes () != 1))
	n = 0;
      if (n > 0)
	{
	 unsigned char buffer[0xffc0];

	  if (n > 0xffc0)
	    n = 0xffc0;
	  else if ((n > 1) && (n & 1))
	    --n;
	  if (rt_get_data (n, buffer) >= 0)
	    {
	      unsigned char *bufnow = buffer;

	      while (n)
		{
		  int numcopy = rowbytes - bytenow;

		  if (numcopy > n)
		    numcopy = n;

		  memcpy (row_buffer + rownow * rowbytes + bytenow,
		  	  bufnow, numcopy);
		  bytenow += numcopy;
		  bufnow += numcopy;
		  n -= numcopy;

		  if (bytenow == rowbytes)
		    {
		      if (!rows_to_begin || !--rows_to_begin)
			{
			  unsigned char *outnow = output_buffer;

			  for (i = 0;
			       i < (merged_channels ? rowbytes : width);
			       i += merged_channels ? channels : 1)
			    {
			      for (j = 0; j < channels; ++j)
				{
				  unsigned pix =
				    (unsigned char) channel_data[j][i & 1][i];

				  if (detailed_calib_data)
				    {
				      unsigned char const *calib_start =
					detailed_calib_data->channeldata[j] +
					2 *
					detailed_calib_data->
					resolution_divisor * i /
					(merged_channels ? channels : 1);
				      pix =
					constrain ((int) pix -
						   (int) calib_start[0], 0,
						   255);
				      pix =
					constrain (pix * calib_start[1] /
						   0x40, 0, 255);
				    }
				  *outnow++ = pix;
				}
			    }

			  if (colour == HP3500_GRAY_SCAN || colour == HP3500_LINEART_SCAN)
			    {
			      unsigned char const *in_now = output_buffer;
			      int	bit = 7;

			      outnow = output_buffer;
			      for (i = 0; i < width; ++i)
				{

				  if (colour == HP3500_GRAY_SCAN)
				    {
				      *outnow++ = ((unsigned) in_now[0] * 2989 +
						   (unsigned) in_now[1] * 5870 +
						   (unsigned) in_now[2] * 1140) / 10000;
				    }
				  else
				    {
				      if (bit == 7)
					*outnow = ((in_now[1] < 0x80) ? 0x80 : 0);
				      else if (in_now[1] < 0x80)
					*outnow |= (1 << bit);
				      if (bit == 0)
					{
					  ++outnow;
					  bit = 7;
					}
				      else
					{
					  --bit;
					}
				    }
				  in_now += 3;
				}
			    }
			  if (rows_supplied++ < height &&
			      !((*cbfunc) (params, output_rowbytes, output_buffer)))
			    break;

			  for (i = 0; i < channels; ++i)
			    {
			      for (j = 0; j < 2; ++j)
				{
				  channel_data[i][j] += rowbytes;
				  if (channel_data[i][j] - row_buffer >=
				      rowbuffer_bytes)
				    channel_data[i][j] -= rowbuffer_bytes;
				}
			    }
			}
		      ++total_rows;
		      if (++rownow == buffered_rows)
			rownow = 0;
		      bytenow = 0;
		    }
		}
	    }
	  DBG (30, "total_rows = %d\r", total_rows);
	}
      else
	{
	  usleep (10000);
	}
    }
  DBG (10, "\n");
  if (n < 0)
    result = -1;

  free (output_buffer);
  free (row_buffer);

  rt_stop_moving ();
  return result;
}

static unsigned local_sram_size;
static unsigned char r93setting;

#define RTS8801_F_SUPPRESS_MOVEMENT	1

static int
find_resolution_index (unsigned resolution)
{
  int res = 0;

  for (res = 0; resparms[res].resolution != resolution; ++res)
    {
      if (!resparms[res].resolution)
	return -1;
    }
  return res;
}

static int
rts8801_fullscan (unsigned x,
		  unsigned y,
		  unsigned w,
		  unsigned h,
		  unsigned xresolution,
		  unsigned yresolution,
		  unsigned colour,
		  rts8801_callback cbfunc,
		  void *param,
		  unsigned char *calib_info,
		  int flags,
		  int red_calib_offset,
		  int green_calib_offset,
		  int blue_calib_offset,
		  int end_calib_offset,
		  detailed_calibration_data const *detailed_calib_data)
{
  int ires, jres;
  int tg_setting;
  unsigned char regs[256];
  unsigned char offdutytime;
  int result;
  int scan_frequency;

  ires = find_resolution_index (xresolution);
  jres = find_resolution_index (yresolution);

  if (ires < 0 || jres < 0)
    return -1;

  /* Set scan parameters */

  rt_read_register_immediate (0, 255, regs);
  regs[255] = 0;

  rt_enable_ccd (regs, 1);
  rt_enable_movement (regs, 1);
  rt_set_scan_frequency (regs, 1);

  rt_adjust_misc_registers (regs);

  rt_set_cvtr_wparams (regs, 3, 0, 6);
  rt_set_cvtr_mpt (regs, 15, 15, 15);
  rt_set_cvtr_lm (regs, 7, 7, 7);
  rt_set_motor_type (regs, 2);

  if (rt_nvram_read (0, 0x7b, &offdutytime, 1) < 0 || offdutytime >= 15)
    {
      offdutytime = 6;
    }
  rt_set_lamp_duty_cycle (regs, 1,	/* On */
			  10,	/* Frequency */
			  offdutytime);	/* Off duty time */

  rt_set_movement_pattern (regs, 0x800000);


  tg_setting = resparms[jres].tg;
  rt_set_ccd_shift_clock_multiplier (regs, tg_info[tg_setting].tg_cph0p);
  rt_set_ccd_clock_reset_interval (regs, tg_info[tg_setting].tg_crsp);
  rt_set_ccd_clamp_clock_multiplier (regs, tg_info[tg_setting].tg_cclpp);


  rt_set_one_register (0xc6, 0);
  rt_set_one_register (0xc6, 0);

  rt_set_step_size (regs, resparms[jres].step_size);

  rt_set_direction_forwards (regs);

  rt_set_stop_when_rewound (regs, 0);
  rt_set_data_feed_on (regs);

  rt_set_calibration_addresses (regs, 0, 0, 0, 0);

  rt_set_basic_calibration (regs,
			    calib_info[0], calib_info[1], calib_info[2],
			    calib_info[3], calib_info[4], calib_info[5],
			    calib_info[6], calib_info[7], calib_info[8]);
  regs[0x0b] = 0x70;		/* If set to 0x71, the alternative, all values are low */

  if (red_calib_offset >= 0
      && green_calib_offset >= 0
      && blue_calib_offset >= 0 &&
      yresolution < 400)
    {
      rt_set_calibration_addresses (regs, red_calib_offset,
				    green_calib_offset, blue_calib_offset,
				    end_calib_offset);
      regs[0x40] |= 0x2f;
      detailed_calib_data = 0;
    }
  else if (end_calib_offset >= 0)
    {
      rt_set_calibration_addresses (regs, 0x600, 0x600, 0x600,
				    end_calib_offset);
      regs[0x40] &= 0xc0;
    }

  rt_set_channel (regs, RT_CHANNEL_ALL);
  rt_set_single_channel_scanning (regs, 0);
  rt_set_merge_channels (regs, 1);
  rt_set_colour_mode (regs, 1);

  rt_set_motor_movement_clock_multiplier (regs,
					  resparms[jres].
					  motor_movement_clock_multiplier);

  rt_set_cdss (regs, tg_info[tg_setting].tg_cdss1,
	       tg_info[tg_setting].tg_cdss2);
  rt_set_cdsc (regs, tg_info[tg_setting].tg_cdsc1,
	       tg_info[tg_setting].tg_cdsc2);
  rt_update_after_setting_cdss2 (regs);

  rt_set_last_sram_page (regs, (local_sram_size - 1) >> 5);

  regs[0x39] = resparms[jres].reg_39_value;
  regs[0xc3] = (regs[0xc3] & 0xf8) | resparms[jres].reg_c3_value;
  regs[0xc6] = (regs[0xc6] & 0xf8) | resparms[jres].reg_c6_value;
  scan_frequency = resparms[jres].scan_frequency;
  rt_set_scan_frequency (regs, scan_frequency);
  rt_set_cph0s (regs, resparms[ires].cph0s);
  if (resparms[ires].d3_bit_3_value)
    regs[0xd3] |= 0x08;
  else
    regs[0xd3] &= 0xf7;

  if (flags & RTS8801_F_SUPPRESS_MOVEMENT)
    regs[0xc3] &= 0x7f;
  rt_set_horizontal_resolution (regs, xresolution);

  rt_set_noscan_distance (regs, y * scan_frequency - 1);
  rt_set_total_distance (regs, scan_frequency *
			 (y +
			  h +
			  resparms[jres].red_green_offset +
			  resparms[jres].green_blue_offset +
			  resparms[jres].intra_channel_offset) - 1);

  rt_set_scanline_start (regs,
			 x * (1200 / xresolution) /
			 (resparms[ires].cph0s ? 1 : 2) /
			 (resparms[ires].d3_bit_3_value ? 1 : 2));
  rt_set_scanline_end (regs,
		       (x +
			w) * (1200 / xresolution) /
		       (resparms[ires].cph0s ? 1 : 2) /
		       (resparms[ires].d3_bit_3_value ? 1 : 2));

  rt_set_all_registers (regs);

  rt_set_one_register (0x2c, regs[0x2c]);

  if (DBG_LEVEL >= 5)
    dump_registers (regs);

  result = rts8801_doscan (w,
			   h,
			   colour,
			   resparms[jres].red_green_offset,
			   resparms[jres].green_blue_offset,
			   resparms[jres].intra_channel_offset,
			   cbfunc, param, (x & 1), calib_info,
			   (regs[0x2f] & 0x04) != 0, detailed_calib_data);

  return result;
}

static int
accumfunc (struct dcalibdata *dcd, int bytes, char *data)
{
  unsigned char *c = (unsigned char *) data;

  while (bytes > 0)
    {
      if (dcd->firstrowdone)
	dcd->buffers[dcd->channelnow][dcd->pixelnow - dcd->pixelsperrow] = *c;
      if (++dcd->channelnow >= 3)
	{
	  dcd->channelnow = 0;
	  if (++dcd->pixelnow == dcd->pixelsperrow)
	    ++dcd->firstrowdone;
	}
      c++;
      bytes--;
    }
  return 1;
}

static int
calcmedian (unsigned char const *data,
	    int pixel, int pixels_per_row, int elements)
{
  int tallies[256];
  int i;
  int elemstogo = elements / 2;

  memset (tallies, 0, sizeof (tallies));
  data += pixel;
  for (i = 0; i < elements; ++i)
    {
      ++tallies[*data];
      data += pixels_per_row;
    }
  i = 0;
  while (elemstogo - tallies[i] > 0)
    elemstogo -= tallies[i++];
  return i;
}

struct calibdata
{
  unsigned char *buffer;
  int space;
};

static int
storefunc (struct calibdata *cd, int bytes, char *data)
{
  if (cd->space > 0)
    {
      if (bytes > cd->space)
	bytes = cd->space;
      memcpy (cd->buffer, data, bytes);
      cd->buffer += bytes;
      cd->space -= bytes;
    }
  return 1;
}

static unsigned
sum_channel (unsigned char *p, int n, int bytwo)
{
  unsigned v = 0;

  while (n-- > 0)
    {
      v += *p;
      p += 3;
      if (bytwo)
	p += 3;
    }
  return v;
}

static int do_warmup = 1;

static int
rts8801_scan (unsigned x,
	      unsigned y,
	      unsigned w,
	      unsigned h,
	      unsigned resolution,
	      unsigned colour,
	      unsigned brightness,
	      unsigned contrast,
	      rts8801_callback cbfunc,
	      void *param)
{
  unsigned char calib_info[9];
  unsigned char calibbuf[2400];
  struct dcalibdata dcd;
  struct calibdata cd;
  unsigned char *detail_buffer = 0;
  int iCalibOffset;
  int iCalibX;
  int iCalibY;
  int iCalibWidth;
  int iCalibTarget;
  int iCalibPixels;
  int iMoveFlags = 0;
  unsigned int aiLow[3] = { 0, 0, 0 };
  unsigned int aiHigh[3] = { 256, 256, 256 };
  unsigned aiBestOffset[3];
  int i;
  unsigned j;
  int anychanged;
  int calibration_size;
  unsigned char *pDetailedCalib;
  int red_calibration_offset;
  int green_calibration_offset;
  int blue_calibration_offset;
  int end_calibration_offset;
  int base_resolution;
  int resolution_divisor;
  int resolution_index;
  int detailed_calibration_rows = 50;
  unsigned char *tdetail_buffer;
  detailed_calibration_data detailed_calib_data;

  /* Initialise and power up */

  rt_set_all_registers (initial_regs);
  rt_set_powersave_mode (0);

  /* Initial rewind in case scanner is stuck away from home position */

  rts8801_rewind ();

  /* Detect SRAM */

  rt_detect_sram (&local_sram_size, &r93setting);

  /* Warm up the lamp */

  DBG (10, "Warming up the lamp\n");

  rt_turn_on_lamp ();
  if (do_warmup)
    sleep (25);

  /* Basic calibration */

  DBG (10, "Calibrating (stage 1)\n");

  calib_info[2] = calib_info[5] = calib_info[8] = 1;

  calib_info[0] = calib_info[1] = calib_info[3] = calib_info[4] =
    calib_info[6] = calib_info[7] = 0xb4;

  iCalibOffset = 0;		/* Note that horizontal resolution is always 600dpi for calibration. 330 is 110 dots in (for R,G,B channels) */
  iCalibX = 1;
  iCalibPixels = 50;
  iCalibY = (resolution == 25) ? 1 : 2;	/* Was 1200 / resolution, which would take us past the calibration area for 50dpi */
  iCalibWidth = 100;
  iCalibTarget = 550;

  for (i = 0; i < 3; ++i)
    aiBestOffset[i] = 0xb4;

  do
    {
      DBG (30, "Initial calibration pass commences\n");
      anychanged = 0;

      for (i = 0; i < 3; ++i)
	{
	  aiBestOffset[i] = (aiHigh[i] + aiLow[i] + 1) / 2;
	}

      for (i = 0; i < 3; ++i)
	calib_info[i * 3] = calib_info[i * 3 + 1] = aiBestOffset[i];

      cd.buffer = calibbuf;
      cd.space = sizeof (calibbuf);
      DBG (30, "Commencing scan for initial calibration pass\n");
      rts8801_fullscan (iCalibX, iCalibY, iCalibWidth, 2, 600, resolution,
			HP3500_COLOR_SCAN, (rts8801_callback) storefunc, &cd,
			calib_info, iMoveFlags, -1, -1, -1, -1, 0);
      DBG (30, "Completed scan for initial calibration pass\n");
      iMoveFlags = RTS8801_F_SUPPRESS_MOVEMENT;

      for (i = 0; i < 3; ++i)
	{
	  int sum;

	  if (aiBestOffset[i] >= 255)
	    continue;
	  sum = sum_channel (calibbuf + iCalibOffset + i, iCalibPixels, 0);
	  DBG (20, "channel[%d] sum = %d (target %d)\n", i, sum,
	       iCalibTarget);

	  if (sum >= iCalibTarget)
	    aiHigh[i] = aiBestOffset[i];
	  else
	    aiLow[i] = aiBestOffset[i];
	}
      DBG (30, "Initial calibration pass completed\n");
    }
  while (aiLow[0] < aiHigh[0] - 1 && aiLow[1] < aiHigh[1] - 1
	 && aiLow[1] < aiHigh[1] + 1);

  DBG (20, "Offsets calculated\n");
  cd.buffer = calibbuf;
  cd.space = sizeof (calibbuf);
  DBG (20, "Scanning for part 2 of initial calibration\n");
  rts8801_fullscan (iCalibX + 2100, iCalibY, iCalibWidth, 2, 600, resolution,
		    HP3500_COLOR_SCAN, (rts8801_callback) storefunc, &cd,
		    calib_info, RTS8801_F_SUPPRESS_MOVEMENT, -1, -1, -1, -1,
		    0);
  DBG (20, "Scan for part 2 of initial calibration completed\n");

  DBG (20, "Initial calibration completed\n");

  tdetail_buffer =
    (unsigned char *) malloc (w * 3 * detailed_calibration_rows);
  aiLow[0] = aiLow[1] = aiLow[2] = 1;
  aiHigh[0] = aiHigh[1] = aiHigh[2] = 64;

  do
    {
      struct dcalibdata dcdt;

      for (i = 0; i < 3; ++i)
	calib_info[i * 3 + 2] = (aiLow[i] + aiHigh[i]) / 2;

      dcdt.buffers[0] = tdetail_buffer;
      dcdt.buffers[1] = (tdetail_buffer + w * detailed_calibration_rows);
      dcdt.buffers[2] = (dcdt.buffers[1] + w * detailed_calibration_rows);
      dcdt.pixelsperrow = w;
      dcdt.pixelnow = dcdt.channelnow = dcdt.firstrowdone = 0;
      rts8801_fullscan (x, 4, w, detailed_calibration_rows + 1, resolution,
			resolution, HP3500_COLOR_SCAN,
			(rts8801_callback) accumfunc, &dcdt, calib_info,
			RTS8801_F_SUPPRESS_MOVEMENT, -1, -1, -1, -1, 0);
      for (i = 0; i < 3; ++i)
	{
	  int largest = 1;

	  for (j = 0; j < w; ++j)
	    {
	      int val =
		calcmedian (dcdt.buffers[i], j, w, detailed_calibration_rows);

	      if (val > largest)
		largest = val;
	    }

	  if (largest < 0xe0)
	    aiLow[i] = calib_info[i * 3 + 2];
	  else
	    aiHigh[i] = calib_info[i * 3 + 2];
	}
    }
  while (aiLow[0] < aiHigh[0] - 1 && aiLow[1] < aiHigh[1] - 1
	 && aiLow[1] < aiHigh[1] + 1);

  for (i = 0; i < 3; ++i)
    calib_info[i * 3 + 2] = aiLow[i];

  for (i = 0; i < 3; ++i)
    {
      DBG (10, "Channel [%d] gain=%02x  offset=%02x\n",
	   i, calib_info[i * 3] + 2, calib_info[i * 3]);
    }

  DBG (20, "Gain factors calculated\n");

  /* Stage 2 calibration */

  DBG (10, "Calibrating (stage 2)\n");

  detail_buffer =
    (unsigned char *) malloc (w * 3 * detailed_calibration_rows);

  dcd.buffers[0] = detail_buffer;
  dcd.buffers[1] = (detail_buffer + w * detailed_calibration_rows);
  dcd.buffers[2] = (dcd.buffers[1] + w * detailed_calibration_rows);
  dcd.pixelsperrow = w;
  dcd.pixelnow = dcd.channelnow = dcd.firstrowdone = 0;

  DBG (10, "Performing detailed calibration scan\n");
  rts8801_fullscan (x, iCalibY, w, detailed_calibration_rows + 1,
		    resolution, resolution, HP3500_COLOR_SCAN,
		    (rts8801_callback) accumfunc, &dcd, calib_info,
		    RTS8801_F_SUPPRESS_MOVEMENT, -1, -1, -1, -1, 0);

  DBG (10, "Detailed calibration scan completed\n");

  /* And now for the detailed calibration */
  resolution_index = find_resolution_index (resolution);
  base_resolution = 300;
  if (resparms[resolution_index].cph0s)
    base_resolution *= 2;
  if (resparms[resolution_index].d3_bit_3_value)
    base_resolution *= 2;
  resolution_divisor = base_resolution / resolution;

  calibration_size = w * resolution_divisor * 6 + 1536;
  red_calibration_offset = 1536;
  blue_calibration_offset =
    red_calibration_offset + w * resolution_divisor * 2;
  green_calibration_offset =
    blue_calibration_offset + w * resolution_divisor * 2;
  end_calibration_offset =
    green_calibration_offset + w * resolution_divisor * 2;
  pDetailedCalib = (unsigned char *) malloc (calibration_size);

  memset (pDetailedCalib, 0, calibration_size);
  for (i = 0; i < 3; ++i)
    {
      int idx =
	(i == 0) ? red_calibration_offset : (i ==
					     1) ? green_calibration_offset :
	blue_calibration_offset;
      double g = calib_info[i * 3 + 2];

      for (j = 0; j < 256; j++)
	{
	  int val = j;

	  if (val < 0)
	    val = 0;
	  if (val > 255)
	    val = 255;
	  pDetailedCalib[i * 512 + j * 2] = val;
	  pDetailedCalib[i * 512 + j * 2 + 1] = val;
	}

      for (j = 0; j < w; ++j)
	{
	  int multnow;
	  int offnow;

	  /* This seems to be the approach for reg 0x40 & 0x3f == 0x27, which allows detailed
	   * calibration to return either higher or lower values.
	   */
	  int k;

	  {
	    double denom1 =
	      calcmedian (dcd.buffers[i], j, w, detailed_calibration_rows);
	    double f = 0xff / (denom1 - 2 * g);
	    double contrast_adjust = (double) (contrast + 1) / 64;

	    multnow = f * 64 * contrast_adjust;
	    offnow = 4 * g + 128 - brightness;
	  }
	  if (multnow < 0)
	    multnow = 0;
	  if (multnow > 255)
	    multnow = 255;
	  if (offnow < 0)
	    offnow = 0;
	  if (offnow > 255)
	    offnow = 255;

	  for (k = 0; k < resolution_divisor; ++k)
	    {
	      /*multnow = j * resolution_divisor + k; */
	      pDetailedCalib[idx++] = offnow;	/* Subtract this value from the result */
	      pDetailedCalib[idx++] = multnow;	/* Then multiply by this value divided by 0x40 */
	    }
	}
    }

  DBG (10, "\n");

  rt_set_sram_page (0);
  rt_set_one_register (0x93, r93setting);
  rt_write_sram (calibration_size, pDetailedCalib);

  /* And finally, perform the scan */

  DBG (10, "Scanning\n");

  rts8801_rewind ();

  detailed_calib_data.channeldata[0] =
    pDetailedCalib + red_calibration_offset;
  detailed_calib_data.channeldata[1] =
    pDetailedCalib + green_calibration_offset;
  detailed_calib_data.channeldata[2] =
    pDetailedCalib + blue_calibration_offset;
  detailed_calib_data.resolution_divisor = resolution_divisor;

  rts8801_fullscan (x, y, w, h, resolution, resolution, colour, cbfunc, param,
		    calib_info, 0,
		    red_calibration_offset, green_calibration_offset,
		    blue_calibration_offset, end_calibration_offset,
		    &detailed_calib_data);

  rt_turn_off_lamp ();
  rts8801_rewind ();
  rt_set_powersave_mode (1);

  if (pDetailedCalib)
    free (pDetailedCalib);
  if (detail_buffer)
    free (detail_buffer);
  return 0;
}

static int
writefunc (struct hp3500_write_info *winfo, int bytes, char *data)
{
  static int warned = 0;

  if (bytes > winfo->bytesleft)
    {
      if (!warned)
	{
	  warned = 1;
	  DBG (1, "Overflow protection triggered\n");
	  rt_stop_moving ();
	}
      bytes = winfo->bytesleft;
      if (!bytes)
	return 0;
    }
  winfo->bytesleft -= bytes;
  return write (winfo->scanner->pipe_w, data, bytes) == bytes;
}

static void
sigtermHandler (int signal)
{
  signal = signal;		/* get rid of compiler warning */
  cancelled_scan = 1;
}

static int
reader_process (void *pv)
{
  struct hp3500_data *scanner = pv;
  time_t t;
  sigset_t ignore_set;
  sigset_t sigterm_set;
  struct SIGACTION act;
  struct hp3500_write_info winfo;
  int status;

  if (sanei_thread_is_forked ())
    {
      close (scanner->pipe_r);

      sigfillset (&ignore_set);
      sigdelset (&ignore_set, SIGTERM);
#if     defined (__APPLE__) && defined (__MACH__)
      sigdelset (&ignore_set, SIGUSR2);
#endif
      sigprocmask (SIG_SETMASK, &ignore_set, 0);

      sigemptyset (&sigterm_set);
      sigaddset (&sigterm_set, SIGTERM);

      memset (&act, 0, sizeof (act));
#ifdef     _POSIX_SOURCE
      act.sa_handler = sigtermHandler;
#endif
      sigaction (SIGTERM, &act, 0);
    }


  /* Warm up the lamp again if our last scan ended more than 5 minutes ago. */
  time (&t);
  do_warmup = (t - scanner->last_scan) > 300;

  if (getenv ("HP3500_NOWARMUP") && atoi (getenv ("HP3500_NOWARMUP")) > 0)
    do_warmup = 0;

  udh = scanner->sfd;

  cancelled_scan = 0;

  winfo.scanner = scanner;
  winfo.bytesleft =
    scanner->bytes_per_scan_line * scanner->scan_height_pixels;

  if (getenv ("HP3500_SLEEP"))
    {
      int seconds = atoi (getenv ("HP3500_SLEEP"));

      DBG (1, "Backend process %d sleeping for %d seconds\n", getpid (),
	   seconds);
      sleep (seconds);
    }
  DBG (10, "Scanning at %ddpi, mode=%s\n", scanner->resolution,
       scan_mode_list[scanner->mode]);
  if (rts8801_scan
      (scanner->actres_pixels.left + 250 * scanner->resolution / 1200,
       scanner->actres_pixels.top + 599 * scanner->resolution / 1200,
       scanner->actres_pixels.right - scanner->actres_pixels.left,
       scanner->actres_pixels.bottom - scanner->actres_pixels.top,
       scanner->resolution, scanner->mode, scanner->brightness,
       scanner->contrast, (rts8801_callback) writefunc, &winfo) >= 0)
    status = SANE_STATUS_GOOD;
  status = SANE_STATUS_IO_ERROR;
  close (scanner->pipe_w);
  return status;
}
