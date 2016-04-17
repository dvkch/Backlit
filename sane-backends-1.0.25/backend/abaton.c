/* sane - Scanner Access Now Easy.

   Copyright (C) 1998 David Huggins-Daines, heavily based on the Apple
   scanner driver (since Abaton scanners are very similar to old Apple
   scanners), which is (C) 1998 Milon Firikis, which is, in turn, based
   on the Mustek driver, (C) 1996-7 David Mosberger-Tang.

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

   This file implements a SANE backend for Abaton flatbed scanners.  */

#include "../include/sane/config.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../include/_stdint.h"

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"

#define BACKEND_NAME	abaton
#include "../include/sane/sanei_backend.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#include "../include/sane/sanei_config.h"
#define ABATON_CONFIG_FILE "abaton.conf"

#include "abaton.h"



static const SANE_Device **devlist = 0;
static int num_devices;
static Abaton_Device *first_dev;
static Abaton_Scanner *first_handle;

static SANE_String_Const mode_list[5];

static const SANE_String_Const halftone_pattern_list[] =
{
  "spiral", "bayer",
  0
};

static const SANE_Range dpi_range =
{
  /* min, max, quant */
  72,
  300,
  1
};

static const SANE_Range enhance_range =
{
  1,
  255,
  1
};

static const SANE_Range x_range =
{
  0,
  8.5 * MM_PER_INCH,
  1
};

static const SANE_Range y_range =
{
  0,
  14.0 * MM_PER_INCH,
  1
};

#define ERROR_MESSAGE	1
#define USER_MESSAGE	5
#define FLOW_CONTROL	50
#define VARIABLE_CONTROL 70
#define DEBUG_SPECIAL	100
#define IO_MESSAGE	110
#define INNER_LOOP	120


/* SCSI commands that the Abaton scanners understand: */
#define TEST_UNIT_READY	0x00
#define REQUEST_SENSE	0x03
#define INQUIRY		0x12
#define START_STOP	0x1b
#define SET_WINDOW	0x24
#define READ_10		0x28
#define WRITE_10	0x2b	/* not used, AFAIK */
#define GET_DATA_STATUS	0x34


#define INQ_LEN	0x60
static const uint8_t inquiry[] =
{
  INQUIRY, 0x00, 0x00, 0x00, INQ_LEN, 0x00
};

static const uint8_t test_unit_ready[] =
{
  TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* convenience macros */
#define ENABLE(OPTION)  s->opt[OPTION].cap &= ~SANE_CAP_INACTIVE
#define DISABLE(OPTION) s->opt[OPTION].cap |=  SANE_CAP_INACTIVE
#define IS_ACTIVE(OPTION) (((s->opt[OPTION].cap) & SANE_CAP_INACTIVE) == 0)

/* store an 8-bit-wide value at the location specified by ptr */
#define STORE8(ptr, val) (*((uint8_t *) ptr) = val)

/* store a 16-bit-wide value in network (big-endian) byte order */
#define STORE16(ptr, val)			\
  {						\
  *((uint8_t *) ptr)     = (val >> 8) & 0xff;	\
  *((uint8_t *) ptr+1)   = val & 0xff;		\
  }

/* store a 24-bit-wide value in network (big-endian) byte order */
#define STORE24(ptr, val)			\
  {						\
  *((uint8_t *) ptr)     = (val >> 16) & 0xff;	\
  *((uint8_t *) ptr+1)   = (val >> 8) & 0xff;	\
  *((uint8_t *) ptr+2)   = val & 0xff;		\
  }

/* store a 32-bit-wide value in network (big-endian) byte order */
#define STORE32(ptr, val)			\
  {						\
  *((uint8_t *) ptr)     = (val >> 24) & 0xff;	\
  *((uint8_t *) ptr+1)   = (val >> 16) & 0xff;	\
  *((uint8_t *) ptr+2)   = (val >> 8) & 0xff;	\
  *((uint8_t *) ptr+3)   = val & 0xff;		\
  }

/* retrieve a 24-bit-wide big-endian value at ptr */
#define GET24(ptr) \
  (*((uint8_t *) ptr) << 16)  + \
  (*((uint8_t *) ptr+1) << 8) + \
  (*((uint8_t *) ptr+2))

static SANE_Status
wait_ready (int fd)
{
#define MAX_WAITING_TIME	60	/* one minute, at most */
  struct timeval now, start;
  SANE_Status status;

  gettimeofday (&start, 0);

  while (1)
    {
      DBG (USER_MESSAGE, "wait_ready: sending TEST_UNIT_READY\n");

      status = sanei_scsi_cmd (fd, test_unit_ready, sizeof (test_unit_ready),
			       0, 0);
      switch (status)
	{
	default:
	  /* Ignore errors while waiting for scanner to become ready.
	     Some SCSI drivers return EIO while the scanner is
	     returning to the home position.  */
	  DBG (ERROR_MESSAGE, "wait_ready: test unit ready failed (%s)\n",
	       sane_strstatus (status));
	  /* fall through */
	case SANE_STATUS_DEVICE_BUSY:
	  gettimeofday (&now, 0);
	  if (now.tv_sec - start.tv_sec >= MAX_WAITING_TIME)
	    {
	      DBG (ERROR_MESSAGE, "wait_ready: timed out after %ld seconds\n",
		   (long) (now.tv_sec - start.tv_sec));
	      return SANE_STATUS_INVAL;
	    }
	  usleep (100000);	/* retry after 100ms */
	  break;

	case SANE_STATUS_GOOD:
	  return status;
	}
    }
  return SANE_STATUS_INVAL;
}

static SANE_Status
sense_handler (int scsi_fd, u_char * result, void *arg)
{
  scsi_fd = scsi_fd;			/* silence gcc */
  arg = arg;					/* silence gcc */

  switch (result[2] & 0x0F)
    {
    case 0:
      DBG (USER_MESSAGE, "Sense: No sense Error\n");
      return SANE_STATUS_GOOD;
    case 2:
      DBG (ERROR_MESSAGE, "Sense: Scanner not ready\n");
      return SANE_STATUS_DEVICE_BUSY;
    case 4:
      DBG (ERROR_MESSAGE, "Sense: Hardware Error. Read more...\n");
      return SANE_STATUS_IO_ERROR;
    case 5:
      DBG (ERROR_MESSAGE, "Sense: Illegal request\n");
      return SANE_STATUS_UNSUPPORTED;
    case 6:
      DBG (ERROR_MESSAGE, "Sense: Unit Attention (Wait until scanner "
	   "boots)\n");
      return SANE_STATUS_DEVICE_BUSY;
    case 9:
      DBG (ERROR_MESSAGE, "Sense: Vendor Unique. Read more...\n");
      return SANE_STATUS_IO_ERROR;
    default:
      DBG (ERROR_MESSAGE, "Sense: Unknown Sense Key. Read more...\n");
      return SANE_STATUS_IO_ERROR;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
request_sense (Abaton_Scanner * s)
{
  uint8_t cmd[6];
  uint8_t result[22];
  size_t size = sizeof (result);
  SANE_Status status;

  memset (cmd, 0, sizeof (cmd));
  memset (result, 0, sizeof (result));

  cmd[0] = REQUEST_SENSE;
  STORE8 (cmd + 4, sizeof (result));
  sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), result, &size);

  if (result[7] != 14)
    {
      DBG (ERROR_MESSAGE, "Additional Length %u\n", (unsigned int) result[7]);
      status = SANE_STATUS_IO_ERROR;
    }


  status = sense_handler (s->fd, result, NULL);
  if (status == SANE_STATUS_IO_ERROR)
    {

      /* Since I haven't figured out the vendor unique error codes on
	 this thing, I'll just handle the normal ones for now */

      if (result[18] & 0x80)
	DBG (ERROR_MESSAGE, "Sense: Dim Light (output of lamp below 70%%).\n");

      if (result[18] & 0x40)
	DBG (ERROR_MESSAGE, "Sense: No Light at all.\n");

      if (result[18] & 0x20)
	DBG (ERROR_MESSAGE, "Sense: No Home.\n");

      if (result[18] & 0x10)
	DBG (ERROR_MESSAGE, "Sense: No Limit. Tried to scan out of range.\n");
    }

  DBG (USER_MESSAGE, "Sense: Optical gain %u.\n", (unsigned int) result[20]);
  return status;
}

static SANE_Status
set_window (Abaton_Scanner * s)
{
  uint8_t cmd[10 + 40];
  uint8_t *window = cmd + 10 + 8;
  int invert;
  
  memset (cmd, 0, sizeof (cmd));
  cmd[0] = SET_WINDOW;
  cmd[8] = 40;

  /* Just like the Apple scanners, we put the resolution here */
  STORE16 (window + 2, s->val[OPT_X_RESOLUTION].w);
  STORE16 (window + 4, s->val[OPT_Y_RESOLUTION].w);

  /* Unlike Apple scanners, these are pixel values */
  STORE16 (window + 6, s->ULx);
  STORE16 (window + 8, s->ULy);
  STORE16 (window + 10, s->Width);
  STORE16 (window + 12, s->Height);

  STORE8 (window + 14, s->val[OPT_BRIGHTNESS].w);
  STORE8 (window + 15, s->val[OPT_THRESHOLD].w);
  STORE8 (window + 16, s->val[OPT_CONTRAST].w);

  invert = s->val[OPT_NEGATIVE].w;

  if (!strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART))
    {
      STORE8 (window + 17, 0);
    }
  else if (!strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_HALFTONE))
    {
      STORE8 (window + 17, 1);
    }
  else if (!strcmp (s->val[OPT_MODE].s, "Gray256")
	   || !strcmp (s->val[OPT_MODE].s, "Gray16"))
    {
      STORE8 (window + 17, 2);
      invert = !s->val[OPT_NEGATIVE].w;
    }
  else
    {
      DBG (ERROR_MESSAGE, "Can't match mode %s\n", s->val[OPT_MODE].s);
      return SANE_STATUS_INVAL;
    }

  STORE8 (window + 18, s->bpp);

  if (!strcmp (s->val[OPT_HALFTONE_PATTERN].s, "spiral"))
    {
      STORE8 (window + 20, 0);
    }
  else if (!strcmp (s->val[OPT_HALFTONE_PATTERN].s, "bayer"))
    {
      STORE8 (window + 20, 1);
    }
  else
    {
      DBG (ERROR_MESSAGE, "Can't match haftone pattern %s\n",
	   s->val[OPT_HALFTONE_PATTERN].s);
      return SANE_STATUS_INVAL;
    }
  
  /* We have to invert these ones for some reason, so why not
     let the scanner do it for us... */
  STORE8 (window + 21, invert ? 0x80 : 0);
  
  STORE16 (window + 22, (s->val[OPT_MIRROR].w != 0));

  return sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), 0, 0);
}

static SANE_Status
start_scan (Abaton_Scanner * s)
{
  SANE_Status status;
  uint8_t start[7];


  memset (start, 0, sizeof (start));
  start[0] = START_STOP;
  start[4] = 1;

  status = sanei_scsi_cmd (s->fd, start, sizeof (start), 0, 0);
  return status;
}

static SANE_Status
attach (const char *devname, Abaton_Device ** devp, int may_wait)
{
  char result[INQ_LEN];
  const char *model_name = result + 44;
  int fd, abaton_scanner;
  Abaton_Device *dev;
  SANE_Status status;
  size_t size;

  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0)
      {
	if (devp)
	  *devp = dev;
	return SANE_STATUS_GOOD;
      }

  DBG (USER_MESSAGE, "attach: opening %s\n", devname);
  status = sanei_scsi_open (devname, &fd, sense_handler, 0);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (ERROR_MESSAGE, "attach: open failed (%s)\n",
	   sane_strstatus (status));
      return SANE_STATUS_INVAL;
    }

  if (may_wait)
    wait_ready (fd);

  DBG (USER_MESSAGE, "attach: sending INQUIRY\n");
  size = sizeof (result);
  status = sanei_scsi_cmd (fd, inquiry, sizeof (inquiry), result, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (ERROR_MESSAGE, "attach: inquiry failed (%s)\n",
	   sane_strstatus (status));
      sanei_scsi_close (fd);
      return status;
    }

  status = wait_ready (fd);
  sanei_scsi_close (fd);
  if (status != SANE_STATUS_GOOD)
    return status;

  /* check that we've got an Abaton */
  abaton_scanner = (strncmp (result + 8, "ABATON  ", 8) == 0);
  model_name = result + 16;

  /* make sure it's a scanner ;-) */
  abaton_scanner = abaton_scanner && (result[0] == 0x06);

  if (!abaton_scanner)
    {
      DBG (ERROR_MESSAGE, "attach: device doesn't look like an Abaton scanner "
	   "(result[0]=%#02x)\n", result[0]);
      return SANE_STATUS_INVAL;
    }

  dev = malloc (sizeof (*dev));
  if (!dev)
    return SANE_STATUS_NO_MEM;

  memset (dev, 0, sizeof (*dev));

  dev->sane.name = strdup (devname);
  dev->sane.vendor = "Abaton";
  dev->sane.model = strndup (model_name, 16);
  dev->sane.type = "flatbed scanner";

  if (!strncmp (model_name, "SCAN 300/GS", 11))
    {
      dev->ScannerModel = ABATON_300GS;
    }
  else if (!strncmp (model_name, "SCAN 300/S", 10))
    {
      dev->ScannerModel = ABATON_300S;
    }

  DBG (USER_MESSAGE, "attach: found Abaton scanner model %s (%s)\n",
       dev->sane.model, dev->sane.type);

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;

  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one (const char *devname)
{
  return attach (devname, 0, SANE_FALSE);
}

static SANE_Status
calc_parameters (Abaton_Scanner * s)
{
  SANE_String val = s->val[OPT_MODE].s;
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int dpix = s->val[OPT_X_RESOLUTION].w;
  SANE_Int dpiy = s->val[OPT_Y_RESOLUTION].w;
  double ulx, uly, width, height;
  
  DBG (FLOW_CONTROL, "Entering calc_parameters\n");

  if (!strcmp (val, SANE_VALUE_SCAN_MODE_LINEART) || !strcmp (val, SANE_VALUE_SCAN_MODE_HALFTONE))
    {
      s->params.depth = 1;
      s->bpp = 1;
    }
  else if (!strcmp (val, "Gray16"))
    {
      s->params.depth = 8;
      s->bpp = 4;
    }
  else if (!strcmp (val, "Gray256"))
    {
      s->params.depth = 8;
      s->bpp = 8;
    }
  else
    {
      DBG (ERROR_MESSAGE, "calc_parameters: Invalid mode %s\n", (char *) val);
      status = SANE_STATUS_INVAL;
    }

  /* in inches */
  ulx    = (double) s->val[OPT_TL_X].w / MM_PER_INCH;
  uly    = (double) s->val[OPT_TL_Y].w / MM_PER_INCH;
  width  = (double) s->val[OPT_BR_X].w / MM_PER_INCH - ulx;
  height = (double) s->val[OPT_BR_Y].w / MM_PER_INCH - uly;

  DBG (VARIABLE_CONTROL, "(inches) ulx: %f, uly: %f, width: %f, height: %f\n",
       ulx, uly, width, height);

  /* turn 'em into pixel quantities */
  s->ULx    = ulx    * dpix;
  s->ULy    = uly    * dpiy;
  s->Width  = width  * dpix;
  s->Height = height * dpiy;
  
  DBG (VARIABLE_CONTROL, "(pixels) ulx: %d, uly: %d, width: %d, height: %d\n",
       s->ULx, s->ULy, s->Width, s->Height);

  /* floor width to a byte multiple */
  if ((s->Width * s->bpp) % 8)
    {
      s->Width /= 8;
      s->Width *= 8;
      DBG (VARIABLE_CONTROL, "Adapting to width %d\n", s->Width);
    }

  s->params.pixels_per_line = s->Width;
  s->params.lines = s->Height;
  s->params.bytes_per_line = s->params.pixels_per_line * s->params.depth / 8;


  DBG (VARIABLE_CONTROL, "format=%d\n", s->params.format);
  DBG (VARIABLE_CONTROL, "last_frame=%d\n", s->params.last_frame);
  DBG (VARIABLE_CONTROL, "lines=%d\n", s->params.lines);
  DBG (VARIABLE_CONTROL, "depth=%d (%d)\n", s->params.depth, s->bpp);
  DBG (VARIABLE_CONTROL, "pixels_per_line=%d\n", s->params.pixels_per_line);
  DBG (VARIABLE_CONTROL, "bytes_per_line=%d\n", s->params.bytes_per_line);
  DBG (VARIABLE_CONTROL, "Pixels %dx%dx%d\n",
       s->params.pixels_per_line, s->params.lines, 1 << s->params.depth);

  DBG (FLOW_CONTROL, "Leaving calc_parameters\n");
  return status;
}

static SANE_Status
mode_update (SANE_Handle handle, char *val)
{
  Abaton_Scanner *s = handle;

  if (!strcmp (val, SANE_VALUE_SCAN_MODE_LINEART))
    {
      DISABLE (OPT_BRIGHTNESS);
      DISABLE (OPT_CONTRAST);
      ENABLE (OPT_THRESHOLD);
      DISABLE (OPT_HALFTONE_PATTERN);
    }
  else if (!strcmp (val, SANE_VALUE_SCAN_MODE_HALFTONE))
    {
      ENABLE (OPT_BRIGHTNESS);
      ENABLE (OPT_CONTRAST);
      DISABLE (OPT_THRESHOLD);
      ENABLE (OPT_HALFTONE_PATTERN);
    }
  else if (!strcmp (val, "Gray16") || !strcmp (val, "Gray256"))
    {
      ENABLE (OPT_BRIGHTNESS);
      ENABLE (OPT_CONTRAST);
      DISABLE (OPT_THRESHOLD);
      DISABLE (OPT_HALFTONE_PATTERN);
    }				/* End of Gray */
  else
    {
      DBG (ERROR_MESSAGE, "Invalid mode %s\n", (char *) val);
      return SANE_STATUS_INVAL;
    }

  calc_parameters (s);

  return SANE_STATUS_GOOD;
}

/* find the longest of a list of strings */
static size_t
max_string_size (const SANE_String_Const strings[])
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

static SANE_Status
init_options (Abaton_Scanner * s)
{
  int i;

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  
  /* "Mode" group: */
  s->opt[OPT_MODE_GROUP].title = "Scan Mode";
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  mode_list[0]=SANE_VALUE_SCAN_MODE_LINEART;
  
  switch (s->hw->ScannerModel)
    {
    case ABATON_300GS:
      mode_list[1]=SANE_VALUE_SCAN_MODE_HALFTONE;
      mode_list[2]="Gray16";
      mode_list[3]="Gray256";
      mode_list[4]=NULL;
      break;
    case ABATON_300S:
    default:
      mode_list[1]=NULL;
      break;
    }
  
  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE].s = strdup (mode_list[0]);

  /* resolution - horizontal */
  s->opt[OPT_X_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].title = SANE_TITLE_SCAN_X_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].desc = SANE_DESC_SCAN_X_RESOLUTION;
  s->opt[OPT_X_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_X_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_X_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_X_RESOLUTION].constraint.range = &dpi_range;
  s->val[OPT_X_RESOLUTION].w = 150;

  /* resolution - vertical */
  s->opt[OPT_Y_RESOLUTION].name = SANE_NAME_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].title = SANE_TITLE_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].desc = SANE_DESC_SCAN_Y_RESOLUTION;
  s->opt[OPT_Y_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_Y_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_Y_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_Y_RESOLUTION].constraint.range = &dpi_range;
  s->val[OPT_Y_RESOLUTION].w = 150;

  /* constrain resolutions */
  s->opt[OPT_RESOLUTION_BIND].name = SANE_NAME_RESOLUTION_BIND;
  s->opt[OPT_RESOLUTION_BIND].title = SANE_TITLE_RESOLUTION_BIND;
  s->opt[OPT_RESOLUTION_BIND].desc = SANE_DESC_RESOLUTION_BIND;
  s->opt[OPT_RESOLUTION_BIND].type = SANE_TYPE_BOOL;
  s->opt[OPT_RESOLUTION_BIND].unit = SANE_UNIT_NONE;
  s->opt[OPT_RESOLUTION_BIND].constraint_type = SANE_CONSTRAINT_NONE;
  /* until I fix it */
  s->val[OPT_RESOLUTION_BIND].w = SANE_FALSE;

  /* preview mode */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->val[OPT_PREVIEW].w = SANE_FALSE;
  
  /* halftone pattern  */
  s->opt[OPT_HALFTONE_PATTERN].name = SANE_NAME_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].title = SANE_TITLE_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].desc = SANE_DESC_HALFTONE_PATTERN;
  s->opt[OPT_HALFTONE_PATTERN].size = max_string_size (halftone_pattern_list);
  s->opt[OPT_HALFTONE_PATTERN].type = SANE_TYPE_STRING;
  s->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_HALFTONE_PATTERN].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_HALFTONE_PATTERN].constraint.string_list = halftone_pattern_list;
  s->val[OPT_HALFTONE_PATTERN].s = strdup (halftone_pattern_list[0]);


  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_INT;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_INT;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_INT;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &x_range;
  s->val[OPT_BR_X].w = x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_INT;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &y_range;
  s->val[OPT_BR_Y].w = y_range.max;


  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &enhance_range;
  s->val[OPT_BRIGHTNESS].w = 150;

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &enhance_range;
  s->val[OPT_CONTRAST].w = 150;

  /* threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &enhance_range;
  s->val[OPT_THRESHOLD].w = 150;

  /* negative */
  s->opt[OPT_NEGATIVE].name = SANE_NAME_NEGATIVE;
  s->opt[OPT_NEGATIVE].title = SANE_TITLE_NEGATIVE;
  s->opt[OPT_NEGATIVE].desc = SANE_DESC_NEGATIVE;
  s->opt[OPT_NEGATIVE].type = SANE_TYPE_BOOL;
  s->opt[OPT_NEGATIVE].unit = SANE_UNIT_NONE;
  s->opt[OPT_NEGATIVE].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_NEGATIVE].w = SANE_FALSE;
  
  /* mirror-image */
  s->opt[OPT_MIRROR].name = "mirror";
  s->opt[OPT_MIRROR].title = "Mirror Image";
  s->opt[OPT_MIRROR].desc = "Scan in mirror-image";
  s->opt[OPT_MIRROR].type = SANE_TYPE_BOOL;
  s->opt[OPT_MIRROR].unit = SANE_UNIT_NONE;
  s->opt[OPT_MIRROR].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_MIRROR].w = SANE_FALSE;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX];
  size_t len;
  FILE *fp;

  authorize = authorize;		/* silence gcc */

  DBG_INIT ();

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (ABATON_CONFIG_FILE);
  if (!fp)
    {
      /* default to /dev/scanner instead of insisting on config file */
      attach ("/dev/scanner", 0, SANE_FALSE);
      return SANE_STATUS_GOOD;
    }

  while (sanei_config_read (dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')	/* ignore line comments */
	continue;

      len = strlen (dev_name);

      if (!len)
	continue;		/* ignore empty lines */

      if (strncmp (dev_name, "option", 6) == 0
	  && isspace (dev_name[6]))
	{
	  const char *str = dev_name + 7;

	  while (isspace (*str))
	    ++str;

	  continue;
	}

      sanei_config_attach_matching_devices (dev_name, attach_one);
    }
  fclose (fp);
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Abaton_Device *dev, *next;

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free ((void *) dev->sane.name);
      free ((void *) dev->sane.model);
      free (dev);
    }

  if (devlist)
    free (devlist);
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  Abaton_Device *dev;
  int i;

  local_only = local_only;		/* silence gcc */

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
  Abaton_Device *dev;
  SANE_Status status;
  Abaton_Scanner *s;

  if (devicename[0])
    {
      for (dev = first_dev; dev; dev = dev->next)
	if (strcmp (dev->sane.name, devicename) == 0)
	  break;

      if (!dev)
	{
	  status = attach (devicename, &dev, SANE_TRUE);
	  if (status != SANE_STATUS_GOOD)
	    return status;
	}
    }
  else
    /* empty devicname -> use first device */
    dev = first_dev;

  if (!dev)
    return SANE_STATUS_INVAL;

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->fd = -1;
  s->hw = dev;

  init_options (s);

  /* set up some universal parameters */
  s->params.last_frame = SANE_TRUE;
  s->params.format = SANE_FRAME_GRAY;
  
  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;

  *handle = s;
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Abaton_Scanner *prev, *s;

  /* remove handle from list of open handles: */
  prev = 0;
  for (s = first_handle; s; s = s->next)
    {
      if (s == (Abaton_Scanner *) handle)
	break;
      prev = s;
    }
  if (!s)
    {
      DBG (ERROR_MESSAGE, "close: invalid handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }

  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  free (handle);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Abaton_Scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    return NULL;
  
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  Abaton_Scanner *s = handle;
  SANE_Status status;
  SANE_Word cap;


  if (option < 0 || option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  if (info != NULL)
    *info = 0;

  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    return SANE_STATUS_INVAL;

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_NUM_OPTS:
	case OPT_X_RESOLUTION:
	case OPT_Y_RESOLUTION:
	case OPT_RESOLUTION_BIND:
	case OPT_PREVIEW:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_THRESHOLD:
	case OPT_NEGATIVE:
	case OPT_MIRROR:
	  *(SANE_Word *) val = s->val[option].w;
	  return SANE_STATUS_GOOD;

	  /* string options */

	case OPT_MODE:
	case OPT_HALFTONE_PATTERN:
	  status = sanei_constrain_value (s->opt + option, s->val[option].s,
					  info);
	  strcpy (val, s->val[option].s);	  
	  return SANE_STATUS_GOOD;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return SANE_STATUS_INVAL;

      status = sanei_constrain_value (s->opt + option, val, info);

      if (status != SANE_STATUS_GOOD)
	return status;


      switch (option)
	{
	  /* resolution should be uniform for previews, or when the
	     user says so. */
	case OPT_PREVIEW:
	  s->val[option].w = *(SANE_Word *) val;
	  if (*(SANE_Word *) val) {
	    s->val[OPT_Y_RESOLUTION].w = s->val[OPT_X_RESOLUTION].w;
	    if (info)
	      *info |= SANE_INFO_RELOAD_OPTIONS;
	  }
	  /* always recalculate! */
	  calc_parameters (s);
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;
	    
	case OPT_RESOLUTION_BIND:
	  s->val[option].w = *(SANE_Word *) val;
	  if (*(SANE_Word *) val) {
	    s->val[OPT_Y_RESOLUTION].w = s->val[OPT_X_RESOLUTION].w;
	    calc_parameters (s);
	    if (info)
	      *info |= SANE_INFO_RELOAD_PARAMS |
		SANE_INFO_RELOAD_OPTIONS;
	  }
	  return SANE_STATUS_GOOD;
	  
	case OPT_X_RESOLUTION:
	  if (s->val[OPT_PREVIEW].w || s->val[OPT_RESOLUTION_BIND].w) {
	    s->val[OPT_Y_RESOLUTION].w = *(SANE_Word *)val;
	    if (info)
	      *info |= SANE_INFO_RELOAD_OPTIONS;
	  }
	  s->val[option].w = *(SANE_Word *) val;
	  calc_parameters (s);
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case OPT_Y_RESOLUTION:
	  if (s->val[OPT_PREVIEW].w || s->val[OPT_RESOLUTION_BIND].w) {
	    s->val[OPT_X_RESOLUTION].w = *(SANE_Word *)val;
	    if (info)
	      *info |= SANE_INFO_RELOAD_OPTIONS;
	  }
	  s->val[option].w = *(SANE_Word *) val;
	  calc_parameters (s);
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	  /* these ones don't have crazy side effects */
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_Y:
	  s->val[option].w = *(SANE_Word *) val;
	  calc_parameters (s);
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	  /* this one is somewhat imprecise */
	case OPT_BR_X:
	  s->val[option].w = *(SANE_Word *) val;
	  calc_parameters (s);
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS
	      | SANE_INFO_INEXACT;	  
	  return SANE_STATUS_GOOD;

	  /* no side-effects whatsoever */
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_THRESHOLD:
	case OPT_NEGATIVE:
	case OPT_MIRROR:

	  s->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	  /* string options */
	case OPT_HALFTONE_PATTERN:
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  return SANE_STATUS_GOOD;
	  
	case OPT_MODE:
	  status = mode_update (s, val);
	  if (status != SANE_STATUS_GOOD)
	    return status;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;
	}			/* End of switch */
    }				/* End of SET_VALUE */
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Abaton_Scanner *s = handle;

  DBG (FLOW_CONTROL, "Entering sane_get_parameters\n");
  calc_parameters (s);


  if (params)
    *params = s->params;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Abaton_Scanner *s = handle;
  SANE_Status status;

  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */

  calc_parameters (s);

  if (s->fd < 0)
    {
      /* this is the first (and maybe only) pass... */

      status = sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, 0);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (ERROR_MESSAGE, "open: open of %s failed: %s\n",
	       s->hw->sane.name, sane_strstatus (status));
	  return status;
	}
    }

  status = wait_ready (s->fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (ERROR_MESSAGE, "open: wait_ready() failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  status = request_sense (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (ERROR_MESSAGE, "sane_start: request_sense revealed error: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  status = set_window (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (ERROR_MESSAGE, "open: set scan area command failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  s->scanning = SANE_TRUE;
  s->AbortedByUser = SANE_FALSE;

  status = start_scan (s);
  if (status != SANE_STATUS_GOOD)
    goto stop_scanner_and_return;

  return SANE_STATUS_GOOD;

stop_scanner_and_return:
  s->scanning = SANE_FALSE;
  s->AbortedByUser = SANE_FALSE;
  return status;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  Abaton_Scanner *s = handle;
  SANE_Status status;

  uint8_t get_data_status[10];
  uint8_t read[10];

  uint8_t result[12];
  size_t size;
  SANE_Int data_av = 0;
  SANE_Int data_length = 0;
  SANE_Int offset = 0;
  SANE_Int rread = 0;
  SANE_Bool Pseudo8bit = SANE_FALSE;


  *len = 0;

  /* don't let bogus read requests reach the scanner */
  /* this is a sub-optimal way of doing this, I'm sure */
  if (!s->scanning)
    return SANE_STATUS_EOF;
  
  if (!strcmp (s->val[OPT_MODE].s, "Gray16"))
    Pseudo8bit = SANE_TRUE;

  memset (get_data_status, 0, sizeof (get_data_status));
  get_data_status[0] = GET_DATA_STATUS;
  /* This means "block" for Apple scanners, it seems to be the same
     for Abaton.  The scanner will do non-blocking I/O, but I don't
     want to go there right now. */
  get_data_status[1] = 1;
  STORE8 (get_data_status + 8, sizeof (result));

  memset (read, 0, sizeof (read));
  read[0] = READ_10;

  do
    {
      size = sizeof (result);
      /* this isn't necessary */
      /*  memset (result, 0, size); */
      status = sanei_scsi_cmd (s->fd, get_data_status,
			       sizeof (get_data_status), result, &size);

      if (status != SANE_STATUS_GOOD)
	return status;
      if (!size)
	{
	  DBG (ERROR_MESSAGE, "sane_read: cannot get_data_status.\n");
	  return SANE_STATUS_IO_ERROR;
	}

      /* this is not an accurate name, but oh well... */
      data_length = GET24 (result);
      data_av = GET24 (result + 9);

      /* don't check result[3] here, because that screws things up
	 somewhat */
      if (data_length) {
	DBG (IO_MESSAGE,
	     "sane_read: (status) Available in scanner buffer %u.\n",
	     data_av);

	if (Pseudo8bit)
	  {
	    if ((data_av * 2) + offset > max_len)
	      rread = (max_len - offset) / 2;
	    else
	      rread = data_av;
	  }
	else if (data_av + offset > max_len)
	  {
	    rread = max_len - offset;
	  }
	else
	  {
	    rread = data_av;
	  }
	
	DBG (IO_MESSAGE,
	     "sane_read: (action) Actual read request for %u bytes.\n",
	     rread);

	size = rread;
      
	STORE24 (read + 6, rread);

	status = sanei_scsi_cmd (s->fd, read, sizeof (read),
				 buf + offset, &size);

	if (Pseudo8bit)
	  {
	    SANE_Int byte;
	    SANE_Int pos = offset + (rread << 1) - 1;
	    SANE_Byte B;
	    for (byte = offset + rread - 1; byte >= offset; byte--)
	      {
		B = buf[byte];
		/* don't invert these! */
		buf[pos--] = B << 4;   /* low (right) nibble */
		buf[pos--] = B & 0xF0; /* high (left) nibble */
	      }
	    /* putting an end to bitop abuse here */
	    offset += size * 2;
	  }
	else
	  offset += size;

	DBG (IO_MESSAGE, "sane_read: Buffer %u of %u full %g%%\n",
	     offset, max_len, (double) (offset * 100. / max_len));
      }
    }
  while (offset < max_len && data_length != 0 && !s->AbortedByUser);

  if (s->AbortedByUser)
    {
      s->scanning = SANE_FALSE;

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (ERROR_MESSAGE, "sane_read: request_sense revealed error: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = sanei_scsi_cmd (s->fd, test_unit_ready,
			       sizeof (test_unit_ready), 0, 0);
      if (status != SANE_STATUS_GOOD || status != SANE_STATUS_INVAL)
	return status;
      return SANE_STATUS_CANCELLED;
    }

  if (!data_length)
    {
      s->scanning = SANE_FALSE;
      DBG (IO_MESSAGE, "sane_read: (status) No more data...");
      if (!offset)
	{
	  /* this shouldn't happen */
	  *len = 0;
	  DBG (IO_MESSAGE, "EOF\n");
	  return SANE_STATUS_EOF;
	}
      else
	{
	  *len = offset;
	  DBG (IO_MESSAGE, "GOOD\n");
	  return SANE_STATUS_GOOD;
	}
    }


  DBG (FLOW_CONTROL,
       "sane_read: Normal Exiting, Aborted=%u, data_length=%u\n",
       s->AbortedByUser, data_av);
  *len = offset;

  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Abaton_Scanner *s = handle;

  if (s->scanning)
    {
      if (s->AbortedByUser)
	{
	  DBG (FLOW_CONTROL,
	       "sane_cancel: Already Aborted. Please Wait...\n");
	}
      else
	{
	  s->scanning = SANE_FALSE;
	  s->AbortedByUser = SANE_TRUE;
	  DBG (FLOW_CONTROL, "sane_cancel: Signal Caught! Aborting...\n");
	}
    }
  else
    {
      if (s->AbortedByUser)
	{
	  DBG (FLOW_CONTROL, "sane_cancel: Scan has not been initiated yet."
	       "we probably received a signal while writing data.\n");
	  s->AbortedByUser = SANE_FALSE;
	}
      else
	{
	  DBG (FLOW_CONTROL, "sane_cancel: Scan has not been initiated "
	       "yet (or it's over).\n");
	}
    }

  return;
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  handle = handle;			/* silence gcc */
  non_blocking = non_blocking;	/* silence gcc */

  DBG (FLOW_CONTROL, "sane_set_io_mode: Don't call me please. "
       "Unimplemented function\n");
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  handle = handle;			/* silence gcc */
  fd = fd;						/* silence gcc */
  
  DBG (FLOW_CONTROL, "sane_get_select_fd: Don't call me please. "
       "Unimplemented function\n");
  return SANE_STATUS_UNSUPPORTED;
}
