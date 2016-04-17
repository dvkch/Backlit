/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Gordon Matzigkeit
   Copyright (C) 1997 David Mosberger-Tang
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

#include "../include/sane/config.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
extern int errno;

#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"

#include <unistd.h>
#include <fcntl.h>

#include "../include/sane/sanei_backend.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#include "../include/sane/sanei_config.h"
#define PINT_CONFIG_FILE "pint.conf"

#include "pint.h"

#define DECIPOINTS_PER_MM	(720.0 / MM_PER_INCH)
#define TWELVEHUNDS_PER_MM	(1200.0 / MM_PER_INCH)


static int num_devices;
static PINT_Device *first_dev;
static PINT_Scanner *first_handle;

/* A zero-terminated list of valid scanner modes. */
static SANE_String_Const mode_list[8];

static const SANE_Range s7_range =
  {
    -127,				/* minimum */
     127,				/* maximum */
       1				/* quantization */
  };

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
attach (const char *devname, PINT_Device **devp)
{
  int fd;
  long lastguess, inc;
  PINT_Device *dev;
  struct scan_io scanio;

  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0)
      {
	if (devp)
	  *devp = dev;
	return SANE_STATUS_GOOD;
      }

  DBG(3, "attach: opening %s\n", devname);
  fd = open (devname, O_RDONLY, 0);
  if (fd < 0)
    {
      DBG(1, "attach: open failed (%s)\n", strerror (errno));
      return SANE_STATUS_INVAL;
    }

  DBG(3, "attach: sending SCIOCGET\n");
  if (ioctl (fd, SCIOCGET, &scanio) < 0)
    {
      DBG(1, "attach: get status failed (%s)\n", strerror (errno));
      close (fd);
      return SANE_STATUS_INVAL;
    }

  dev = malloc (sizeof (*dev));
  if (!dev)
    return SANE_STATUS_NO_MEM;

  memset(dev, 0, sizeof (*dev));

  /* Copy the original scanner state to the device structure. */
  memcpy (&dev->scanio, &scanio, sizeof (dev->scanio));

  /* FIXME: PINT currently has no good way to determine maxima and minima.
     So, do binary searches to find out what limits the driver has. */

  /* Assume that minimum range of x and y is 0. */
  dev->x_range.min = SANE_FIX (0);
  dev->y_range.min = SANE_FIX (0);
  dev->x_range.quant = 0;
  dev->y_range.quant = 0;

  /* x range */
  inc = 8.5 * 1200;

  /* Converge on the maximum scan width. */
  while ((inc /= 2) != 0)
    {
      /* Move towards the extremum until we overflow. */
      do
	{
	  lastguess = scanio.scan_width;
	  scanio.scan_width += inc;
	}
      while (ioctl (fd, SCIOCSET, &scanio) >= 0);

      /* Pick the last valid guess, divide by two, and try again. */
      scanio.scan_width = lastguess;
    }
  dev->x_range.max = SANE_FIX (scanio.scan_width / TWELVEHUNDS_PER_MM);

  /* y range */
  inc = 11 * 1200;
  while ((inc /= 2) != 0)
    {
      do
	{
	  lastguess = scanio.scan_height;
	  scanio.scan_height += inc;
	}
      while (ioctl (fd, SCIOCSET, &scanio) >= 0);
      scanio.scan_height = lastguess;
    }
  dev->y_range.max = SANE_FIX (scanio.scan_height / TWELVEHUNDS_PER_MM);

  /* Converge on the minimum scan resolution. */
  dev->dpi_range.quant = 1;

  if (scanio.scan_x_resolution > scanio.scan_y_resolution)
    scanio.scan_x_resolution = scanio.scan_y_resolution;
  else
    scanio.scan_y_resolution = scanio.scan_x_resolution;

  inc = -scanio.scan_x_resolution;
  while ((inc /= 2) != 0)
    {
      do
	{
	  lastguess = scanio.scan_x_resolution;
	  scanio.scan_x_resolution = scanio.scan_y_resolution += inc;
	}
      while (ioctl (fd, SCIOCSET, &scanio) >= 0);
      scanio.scan_x_resolution = scanio.scan_y_resolution = lastguess;
    }
  dev->dpi_range.min = scanio.scan_x_resolution;

  /* Converge on the maximum scan resolution. */
  inc = 600;
  while ((inc /= 2) != 0)
    {
      do
	{
	  lastguess = scanio.scan_x_resolution;
	  scanio.scan_x_resolution = scanio.scan_y_resolution += inc;
	}
      while (ioctl (fd, SCIOCSET, &scanio) >= 0);
      scanio.scan_x_resolution = scanio.scan_y_resolution = lastguess;
    }
  dev->dpi_range.max = scanio.scan_x_resolution;

  /* Determine the valid scan modes for mode_list. */
  lastguess = 0;
#define CHECK_MODE(flag,modename) \
  scanio.scan_image_mode = flag; \
  if (ioctl (fd, SCIOCSET, &scanio) >= 0) \
    mode_list[lastguess ++] = modename

  CHECK_MODE(SIM_BINARY_MONOCHROME, SANE_VALUE_SCAN_MODE_LINEART);
  CHECK_MODE(SIM_DITHERED_MONOCHROME, SANE_VALUE_SCAN_MODE_HALFTONE);
  CHECK_MODE(SIM_GRAYSCALE, SANE_VALUE_SCAN_MODE_GRAY);
  CHECK_MODE(SIM_COLOR, SANE_VALUE_SCAN_MODE_COLOR);
  CHECK_MODE(SIM_RED, "Red");
  CHECK_MODE(SIM_GREEN, "Green");
  CHECK_MODE(SIM_BLUE, "Blue");
#undef CHECK_MODE

  /* Zero-terminate the list of modes. */
  mode_list[lastguess] = 0;

  /* Restore the scanner state. */
  if (ioctl (fd, SCIOCSET, &dev->scanio))
    DBG (2, "cannot reset original scanner state: %s\n", strerror (errno));
  close (fd);

  dev->sane.name   = strdup (devname);

  /* Determine vendor. */
  switch (scanio.scan_scanner_type)
    {
    case EPSON_ES300C:
      dev->sane.vendor = "Epson";
      break;

    case FUJITSU_M3096G:
      dev->sane.vendor = "Fujitsu";
      break;

    case HP_SCANJET_IIC:
      dev->sane.vendor = "HP";
      break;

    case IBM_2456:
      dev->sane.vendor = "IBM";
      break;

    case MUSTEK_06000CX:
    case MUSTEK_12000CX:
      dev->sane.vendor = "Mustek";
      break;

    case RICOH_FS1:
    case RICOH_IS410:
    case RICOH_IS50:
      dev->sane.vendor = "Ricoh";
      break;

    case SHARP_JX600:
      dev->sane.vendor = "Sharp";
      break;

    case UMAX_UC630:
    case UMAX_UG630:
      dev->sane.vendor = "UMAX";
      break;

    default:
      dev->sane.vendor = "PINT";
    }

  /* Determine model. */
  switch (scanio.scan_scanner_type)
    {
    case EPSON_ES300C:
      dev->sane.vendor = "Epson";
      break;

    case FUJITSU_M3096G:
      dev->sane.model = "M3096G";
      break;

    case HP_SCANJET_IIC:
      dev->sane.model = "ScanJet IIc";
      break;

    case IBM_2456:
      dev->sane.vendor = "IBM";
      break;

    case MUSTEK_06000CX:
    case MUSTEK_12000CX:
      dev->sane.vendor = "Mustek";
      break;

    case RICOH_FS1:
      dev->sane.model = "FS1";
      break;

    case RICOH_IS410:
      dev->sane.model = "IS-410";
      break;

    case RICOH_IS50:
      dev->sane.vendor = "Ricoh";
      break;

    case SHARP_JX600:
      dev->sane.vendor = "Sharp";
      break;

    case UMAX_UC630:
    case UMAX_UG630:
      dev->sane.vendor = "UMAX";
      break;

    default:
      dev->sane.model = "unknown";
    }

  /* Determine the scanner type. */
  switch (scanio.scan_scanner_type)
    {
    case HP_SCANJET_IIC:
      dev->sane.type = "flatbed scanner";

      /* FIXME: which of these are flatbed or handhelds? */
    case EPSON_ES300C:
    case FUJITSU_M3096G:
    case IBM_2456:
    case MUSTEK_06000CX:
    case MUSTEK_12000CX:
    case RICOH_FS1:
    case RICOH_IS410:
    case RICOH_IS50:
    case SHARP_JX600:
    case UMAX_UC630:
    case UMAX_UG630:
    default:
      dev->sane.type = "generic scanner";
    }

  DBG(1, "attach: found %s %s, x=%g-%gmm, y=%g-%gmm, "
      "resolution=%d-%ddpi\n", dev->sane.vendor, dev->sane.model,
      SANE_UNFIX (dev->x_range.min), SANE_UNFIX (dev->x_range.max),
      SANE_UNFIX (dev->y_range.min), SANE_UNFIX (dev->y_range.max),
      dev->dpi_range.min, dev->dpi_range.max);

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;
  return SANE_STATUS_GOOD;
}

static SANE_Status
init_options (PINT_Scanner *s)
{
  int i;
  int x0, x1, y0, y1;

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

  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].constraint.string_list = mode_list;

  /* Translate the current PINT mode into a string. */
  switch (s->hw->scanio.scan_image_mode)
    {
    case SIM_BINARY_MONOCHROME:
      s->val[OPT_MODE].s = strdup (mode_list[0]);
      break;

    case SIM_DITHERED_MONOCHROME:
      s->val[OPT_MODE].s = strdup (mode_list[1]);
      break;

    case SIM_COLOR:
      s->val[OPT_MODE].s = strdup (mode_list[3]);
      break;

    case SIM_RED:
      s->val[OPT_MODE].s = strdup (mode_list[4]);
      break;

    case SIM_GREEN:
      s->val[OPT_MODE].s = strdup (mode_list[5]);
      break;

    case SIM_BLUE:
      s->val[OPT_MODE].s = strdup (mode_list[6]);
      break;

    case SIM_GRAYSCALE:
    default:
      s->val[OPT_MODE].s = strdup (mode_list[2]);
    }

  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_RESOLUTION].constraint.range = &s->hw->dpi_range;
  s->val[OPT_RESOLUTION].w =
    (s->hw->scanio.scan_x_resolution > s->hw->scanio.scan_y_resolution) ?
    s->hw->scanio.scan_x_resolution : s->hw->scanio.scan_y_resolution;

  /* "Geometry" group: */

  s->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Calculate the x and y millimetre coordinates from the scanio. */
  x0 = SANE_FIX (s->hw->scanio.scan_x_origin / TWELVEHUNDS_PER_MM);
  y0 = SANE_FIX (s->hw->scanio.scan_y_origin / TWELVEHUNDS_PER_MM);
  x1 = SANE_FIX ((s->hw->scanio.scan_x_origin + s->hw->scanio.scan_width)
		 / TWELVEHUNDS_PER_MM);
  y1 = SANE_FIX ((s->hw->scanio.scan_y_origin + s->hw->scanio.scan_height)
		 / TWELVEHUNDS_PER_MM);

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &s->hw->x_range;
  s->val[OPT_TL_X].w = x0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_TL_Y].w = y0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;
  s->val[OPT_BR_X].w = x1;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_BR_Y].w = y1;

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
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &s7_range;
  s->val[OPT_BRIGHTNESS].w = s->hw->scanio.scan_brightness - 128;

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &s7_range;
  s->val[OPT_CONTRAST].w = s->hw->scanio.scan_contrast - 128;
  return SANE_STATUS_GOOD;
}

static SANE_Status
do_cancel (PINT_Scanner *s)
{
  /* FIXME: PINT doesn't have any good way to cancel ScanJets right now. */
#define gobble_up_buf_len 1024
  char buf[gobble_up_buf_len];

  /* Send the restart code. */
  buf[0] = ioctl (s->fd, SCIOCRESTART, 0);

  if (!s->scanning)
    return SANE_STATUS_CANCELLED;

  s->scanning = SANE_FALSE;

  /* Read to the end of the file. */
  while (read (s->fd, buf, gobble_up_buf_len) > 0)
    ;
#undef gobble_up_buf_len

  /* Finally, close the file descriptor. */
  if (s->fd >= 0)
    {
      close (s->fd);
      s->fd = -1;
    }
  return SANE_STATUS_CANCELLED;
}

SANE_Status
sane_init (SANE_Int *version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX];
  size_t len;
  FILE *fp;

  DBG_INIT();

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (PINT_CONFIG_FILE);
  if (!fp)
    {
      /* default to /dev/scanner instead of insisting on config file */
      attach ("/dev/scanner", 0);
      return SANE_STATUS_GOOD;
    }

  while (sanei_config_read (dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')		/* ignore line comments */
	continue;
      len = strlen (dev_name);

      if (!len)
	continue;			/* ignore empty lines */

      attach (dev_name, 0);
    }
  fclose (fp);
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  PINT_Device *dev, *next;

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free ((void *) dev->sane.name);
      free (dev);
    }
}

SANE_Status
sane_get_devices (const SANE_Device ***device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  PINT_Device *dev;
  int i;

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
sane_open (SANE_String_Const devicename, SANE_Handle *handle)
{
  SANE_Status status;
  PINT_Device *dev;
  PINT_Scanner *s;

  if (devicename[0])
    {
      for (dev = first_dev; dev; dev = dev->next)
	if (strcmp (dev->sane.name, devicename) == 0)
	  break;

      if (!dev)
	{
	  status = attach (devicename, &dev);
	  if (status != SANE_STATUS_GOOD)
	    return status;
	}
    }
  else
    /* empty devicename -> use first device */
    dev = first_dev;

  if (!dev)
    return SANE_STATUS_INVAL;

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->hw = dev;
  s->fd = -1;

  init_options (s);

  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;

  *handle = s;
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  PINT_Scanner *prev, *s;

  /* remove handle from list of open handles: */
  prev = 0;
  for (s = first_handle; s; s = s->next)
    {
      if (s == handle)
	break;
      prev = s;
    }
  if (!s)
    {
      DBG(1, "close: invalid handle %p\n", handle);
      return;		/* oops, not a handle we know about */
    }

  if (s->scanning)
    do_cancel (handle);

  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  free (handle);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  PINT_Scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int *info)
{
  PINT_Scanner *s = handle;
  SANE_Status status;
  SANE_Word cap;

  if (info)
    *info = 0;

  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;

  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    return SANE_STATUS_INVAL;

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	  *(SANE_Word *) val = s->val[option].w;
	  return SANE_STATUS_GOOD;

	  /* string options: */
	case OPT_MODE:
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
	  /* (mostly) side-effect-free word options: */
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  /* fall through */
	case OPT_NUM_OPTS:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	  s->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	case OPT_MODE:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;
	}
    }
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters *params)
{
  PINT_Scanner *s = handle;
  struct scan_io scanio;

  if (!s->scanning)
    {
      u_long x0, y0, width, height;
      const char *mode;

      /* Grab the scanio for this device. */
      if (s->fd < 0)
	{
	  s->fd = open (s->hw->sane.name, O_RDONLY, 0);
	  if (s->fd < 0)
	    {
	      DBG(1, "open of %s failed: %s\n",
		  s->hw->sane.name, strerror (errno));
	      return SANE_STATUS_INVAL;
	    }
	}

      if (ioctl (s->fd, SCIOCGET, &scanio) < 0)
	{
	  DBG(1, "getting scanner state failed: %s", strerror (errno));
	  return SANE_STATUS_INVAL;
	}

      memset (&s->params, 0, sizeof (s->params));

      /* FIXME: there is some lossage here: the parameters change due to
	 roundoff errors between converting to fixed point millimetres
	 and back. */
      x0 = SANE_UNFIX (s->val[OPT_TL_X].w * TWELVEHUNDS_PER_MM);
      y0 = SANE_UNFIX (s->val[OPT_TL_Y].w * TWELVEHUNDS_PER_MM);
      width  = SANE_UNFIX ((s->val[OPT_BR_X].w - s->val[OPT_TL_X].w)
			   * TWELVEHUNDS_PER_MM);
      height = SANE_UNFIX ((s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w)
			   * TWELVEHUNDS_PER_MM);

      /* x and y dpi: */
      scanio.scan_x_resolution = s->val[OPT_RESOLUTION].w;
      scanio.scan_y_resolution = s->val[OPT_RESOLUTION].w;

      /* set scan extents, in 1/1200'ths of an inch */
      scanio.scan_x_origin = x0;
      scanio.scan_y_origin = y0;
      scanio.scan_width = width;
      scanio.scan_height = height;

      /* brightness and contrast */
      scanio.scan_brightness = s->val[OPT_BRIGHTNESS].w + 128;
      scanio.scan_contrast = s->val[OPT_CONTRAST].w + 128;

      /* set the scan image mode */
      mode = s->val[OPT_MODE].s;
      if (!strcmp (mode, SANE_VALUE_SCAN_MODE_LINEART))
	{
	  s->params.format = SANE_FRAME_GRAY;
	  scanio.scan_image_mode = SIM_BINARY_MONOCHROME;
	}
      else if (!strcmp (mode, SANE_VALUE_SCAN_MODE_HALFTONE))
	{
	  s->params.format = SANE_FRAME_GRAY;
	  scanio.scan_image_mode = SIM_DITHERED_MONOCHROME;
	}
      else if (!strcmp (mode, SANE_VALUE_SCAN_MODE_GRAY))
	{
	  s->params.format = SANE_FRAME_GRAY;
	  scanio.scan_image_mode = SIM_GRAYSCALE;
	}
      else if (!strcmp (mode, "Red"))
	{
	  s->params.format = SANE_FRAME_RED;
	  scanio.scan_image_mode = SIM_RED;
	}
      else if (!strcmp (mode, "Green"))
	{
	  s->params.format = SANE_FRAME_GREEN;
	  scanio.scan_image_mode = SIM_GREEN;
	}
      else if (!strcmp (mode, "Blue"))
	{
	  s->params.format = SANE_FRAME_BLUE;
	  scanio.scan_image_mode = SIM_BLUE;
	}
      else
	{
	  s->params.format = SANE_FRAME_RGB;
	  scanio.scan_image_mode = SIM_COLOR;
	}

      /* inquire resulting size of image after setting it up */
      if (ioctl (s->fd, SCIOCSET, &scanio) < 0)
	{
	  DBG(1, "setting scan parameters failed: %s", strerror (errno));
	  return SANE_STATUS_INVAL;
	}
      if (ioctl (s->fd, SCIOCGET, &scanio) < 0)
	{
	  DBG(1, "getting scan parameters failed: %s", strerror (errno));
	  return SANE_STATUS_INVAL;
	}

      /* Save all the PINT-computed values. */
      s->params.pixels_per_line = scanio.scan_pixels_per_line;
      s->params.bytes_per_line =
	(scanio.scan_bits_per_pixel * scanio.scan_pixels_per_line + 7) / 8;
      s->params.lines = scanio.scan_lines;
      s->params.depth = (scanio.scan_image_mode == SIM_COLOR) ?
	scanio.scan_bits_per_pixel / 3 : scanio.scan_bits_per_pixel;

      /* FIXME: this will need to be different for hand scanners. */
      s->params.last_frame = SANE_TRUE;
    }
  if (params)
    *params = s->params;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  PINT_Scanner *s = handle;
  SANE_Status status;

  /* First make sure we have a current parameter set.  This call actually
     uses the PINT driver to do the calculations, so we trust its results. */
  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;

  DBG(1, "%d pixels per line, %d bytes, %d lines high, dpi=%d\n",
      s->params.pixels_per_line, s->params.bytes_per_line, s->params.lines,
      s->val[OPT_RESOLUTION].w);

  /* The scan is triggered in sane_read. */
  s->scanning = SANE_TRUE;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte *buf, SANE_Int max_len, SANE_Int *len)
{
  PINT_Scanner *s = handle;
  ssize_t nread;

  *len = 0;

  if (!s->scanning)
    return do_cancel (s);

  /* Verrry simple.  Just suck up all the data PINT passes to us. */
  nread = read (s->fd, buf, max_len);
  if (nread <= 0)
    {
      do_cancel (s);
      return (nread == 0) ? SANE_STATUS_EOF : SANE_STATUS_IO_ERROR;
    }

  *len = nread;
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  PINT_Scanner *s = handle;
  do_cancel (s);
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int *fd)
{
  return SANE_STATUS_UNSUPPORTED;
}
