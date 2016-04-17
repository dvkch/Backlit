/* sane - Scanner Access Now Easy.
   Copyright (C) 1999 Juergen G. Schimmer
   Updates and bugfixes (C) 2002 - 2004 Henning Meier-Geinitz

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

   This file implements a SANE backend for v4l-Devices.
*/

#define BUILD 5

#include "../include/sane/config.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mman.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#include <sys/ioctl.h>
#include <asm/types.h>		/* XXX glibc */

#define BACKEND_NAME v4l
#include "../include/sane/sanei_backend.h"

#ifndef PATH_MAX
# define PATH_MAX       1024
#endif

#include "../include/sane/sanei_config.h"
#define V4L_CONFIG_FILE "v4l.conf"

#include <libv4l1.h>
#include "v4l.h"

static const SANE_Device **devlist = NULL;
static int num_devices;
static V4L_Device *first_dev;
static V4L_Scanner *first_handle;
static char *buffer;

static const SANE_String_Const mode_list[] = {
  SANE_VALUE_SCAN_MODE_GRAY, SANE_VALUE_SCAN_MODE_COLOR,
  0
};

static const SANE_Range u8_range = {
  /* min, max, quantization */
  0, 255, 0
};

static SANE_Range x_range = { 0, 338, 2 };

static SANE_Range odd_x_range = { 1, 339, 2 };

static SANE_Range y_range = { 0, 249, 1 };

static SANE_Range odd_y_range = { 1, 250, 1 };


static SANE_Parameters parms = {
  SANE_FRAME_RGB,
  1,				/* 1 = Last Frame , 0 = More Frames to come */
  0,				/* Number of bytes returned per scan line: */
  0,				/* Number of pixels per scan line.  */
  0,				/* Number of lines for the current scan.  */
  8,				/* Number of bits per sample. */
};

static SANE_Status
attach (const char *devname, V4L_Device ** devp)
{
  V4L_Device *dev;
  static int v4lfd;
  static struct video_capability capability;

  errno = 0;

  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0)
      {
	if (devp)
	  *devp = dev;
	DBG (5, "attach: device %s is already known\n", devname);
	return SANE_STATUS_GOOD;
      }

  DBG (3, "attach: trying to open %s\n", devname);
  v4lfd = v4l1_open (devname, O_RDWR);
  if (v4lfd != -1)
    {
      if (v4l1_ioctl (v4lfd, VIDIOCGCAP, &capability) == -1)
	{
	  DBG (1,
	       "attach: ioctl (%d, VIDIOCGCAP,..) failed on `%s': %s\n",
	       v4lfd, devname, strerror (errno));
	  v4l1_close (v4lfd);
	  return SANE_STATUS_INVAL;
	}
      if (!(VID_TYPE_CAPTURE & capability.type))
	{
	  DBG (1, "attach: device %s can't capture to memory -- exiting\n",
	       devname);
	  v4l1_close (v4lfd);
	  return SANE_STATUS_UNSUPPORTED;
	}
      DBG (2, "attach: found videodev `%s' on `%s'\n", capability.name,
	   devname);
      v4l1_close (v4lfd);
    }
  else
    {
      DBG (1, "attach: failed to open device `%s': %s\n", devname,
	   strerror (errno));
      return SANE_STATUS_INVAL;
    }

  dev = malloc (sizeof (*dev));
  if (!dev)
    return SANE_STATUS_NO_MEM;

  memset (dev, 0, sizeof (*dev));

  dev->sane.name = strdup (devname);
  if (!dev->sane.name)
    return SANE_STATUS_NO_MEM;
  dev->sane.vendor = "Noname";
  dev->sane.model = strdup (capability.name);
  if (!dev->sane.model)
    return SANE_STATUS_NO_MEM;
  dev->sane.type = "virtual device";

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;
  return SANE_STATUS_GOOD;
}

static void
update_parameters (V4L_Scanner * s)
{
  /* ??? should be per-device */
  x_range.min = 0;
  x_range.max = s->capability.maxwidth - s->capability.minwidth;
  x_range.quant = 1;

  y_range.min = 0;
  y_range.max = s->capability.maxheight - s->capability.minheight;
  y_range.quant = 1;

  odd_x_range.min = s->capability.minwidth;
  odd_x_range.max = s->capability.maxwidth;
  if (odd_x_range.max > 767)
    {
      odd_x_range.max = 767;
      x_range.max = 767 - s->capability.minwidth;
    };
  odd_x_range.quant = 1;

  odd_y_range.min = s->capability.minheight;
  odd_y_range.max = s->capability.maxheight;
  if (odd_y_range.max > 511)
    {
      odd_y_range.max = 511;
      y_range.max = 511 - s->capability.minheight;
    };
  odd_y_range.quant = 1;

  parms.lines = s->window.height;
  parms.pixels_per_line = s->window.width;

  switch (s->pict.palette)
    {
    case VIDEO_PALETTE_GREY:	/* Linear greyscale */
      {
	parms.format = SANE_FRAME_GRAY;
	parms.depth = 8;
	parms.bytes_per_line = s->window.width;
	break;
      }
    case VIDEO_PALETTE_RGB24:	/* 24bit RGB */
      {
	parms.format = SANE_FRAME_RGB;
	parms.depth = 8;
	parms.bytes_per_line = s->window.width * 3;
	break;
      }
    default:
      {
	parms.format = SANE_FRAME_GRAY;
	parms.bytes_per_line = s->window.width;
	break;
      }
    }
}

static SANE_Status
init_options (V4L_Scanner * s)
{
  int i;

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = (SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT);
    }

  /* Number of options */
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

  /* mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].unit = SANE_UNIT_NONE;
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE].s = strdup (mode_list[0]);
  if (!s->val[OPT_MODE].s)
    return SANE_STATUS_NO_MEM;
  s->opt[OPT_MODE].size = 1; /* '\0' */
  for (i = 0; mode_list[i] != 0; ++i)
    {
      int len = strlen(mode_list[i]) + 1;
      if (s->opt[OPT_MODE].size < len)
        s->opt[OPT_MODE].size = len;
    }

  /* channel */
  s->opt[OPT_CHANNEL].name = "channel";
  s->opt[OPT_CHANNEL].title = "Channel";
  s->opt[OPT_CHANNEL].desc =
    "Selects the channel of the v4l device (e.g. television " "or video-in.";
  s->opt[OPT_CHANNEL].type = SANE_TYPE_STRING;
  s->opt[OPT_CHANNEL].unit = SANE_UNIT_NONE;
  s->opt[OPT_CHANNEL].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_CHANNEL].constraint.string_list = s->channel;
  s->val[OPT_CHANNEL].s = strdup (s->channel[0]);
  if (!s->val[OPT_CHANNEL].s)
    return SANE_STATUS_NO_MEM;
  if (s->channel[0] == 0 || s->channel[1] == 0)
    s->opt[OPT_CHANNEL].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_CHANNEL].size = 1; /* '\0' */
  for (i = 0; s->channel[i] != 0; ++i)
    {
      int len = strlen(s->channel[i]) + 1;
      if (s->opt[OPT_CHANNEL].size < len)
        s->opt[OPT_CHANNEL].size = len;
    }

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

/* top-left x *//* ??? first check if window is settable at all */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_INT;
  s->opt[OPT_TL_X].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_TL_X].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_INT;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_TL_Y].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_INT;
  s->opt[OPT_BR_X].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_BR_X].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &odd_x_range;
  s->val[OPT_BR_X].w = s->capability.maxwidth;
  if (s->val[OPT_BR_X].w > 767)
    s->val[OPT_BR_X].w = 767;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_INT;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_PIXEL;
  s->opt[OPT_BR_Y].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &odd_y_range;
  s->val[OPT_BR_Y].w = s->capability.maxheight;
  if (s->val[OPT_BR_Y].w > 511)
    s->val[OPT_BR_Y].w = 511;

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
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &u8_range;
  s->val[OPT_BRIGHTNESS].w = s->pict.brightness / 256;

  /* hue */
  s->opt[OPT_HUE].name = SANE_NAME_HUE;
  s->opt[OPT_HUE].title = SANE_TITLE_HUE;
  s->opt[OPT_HUE].desc = SANE_DESC_HUE;
  s->opt[OPT_HUE].type = SANE_TYPE_INT;
  s->opt[OPT_HUE].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_HUE].constraint.range = &u8_range;
  s->val[OPT_HUE].w = s->pict.hue / 256;

  /* colour */
  s->opt[OPT_COLOR].name = "color";
  s->opt[OPT_COLOR].title = "Picture color";
  s->opt[OPT_COLOR].desc = "Sets the picture's color.";
  s->opt[OPT_COLOR].type = SANE_TYPE_INT;
  s->opt[OPT_COLOR].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_COLOR].constraint.range = &u8_range;
  s->val[OPT_COLOR].w = s->pict.colour / 256;

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &u8_range;
  s->val[OPT_CONTRAST].w = s->pict.contrast / 256;

  /* whiteness */
  s->opt[OPT_WHITE_LEVEL].name = SANE_NAME_WHITE_LEVEL;
  s->opt[OPT_WHITE_LEVEL].title = SANE_TITLE_WHITE_LEVEL;
  s->opt[OPT_WHITE_LEVEL].desc = SANE_DESC_WHITE_LEVEL;
  s->opt[OPT_WHITE_LEVEL].type = SANE_TYPE_INT;
  s->opt[OPT_WHITE_LEVEL].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_WHITE_LEVEL].constraint.range = &u8_range;
  s->val[OPT_WHITE_LEVEL].w = s->pict.whiteness / 256;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX], *str;
  size_t len;
  FILE *fp;

  authorize = authorize;	/* stop gcc from complaining */
  DBG_INIT ();

  DBG (2, "SANE v4l backend version %d.%d build %d from %s\n", SANE_CURRENT_MAJOR,
       V_MINOR, BUILD, PACKAGE_STRING);

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  fp = sanei_config_open (V4L_CONFIG_FILE);
  if (!fp)
    {
      DBG (2,
	   "sane_init: file `%s' not accessible (%s), trying /dev/video0\n",
	   V4L_CONFIG_FILE, strerror (errno));

      return attach ("/dev/video0", 0);
    }

  while (sanei_config_read (dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')	/* ignore line comments */
	continue;
      len = strlen (dev_name);

      if (!len)
	continue;		/* ignore empty lines */

      /* Remove trailing space and trailing comments */
      for (str = dev_name; *str && !isspace (*str) && *str != '#'; ++str);
      attach (dev_name, 0);
    }
  fclose (fp);
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  V4L_Device *dev, *next;

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free ((void *) dev->sane.name);
      free ((void *) dev->sane.model);
      free (dev);
    }

  if (NULL != devlist)
    {
      free (devlist);
      devlist = NULL;
    }
  DBG (5, "sane_exit: all devices freed\n");
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  V4L_Device *dev;
  int i;

  DBG (5, "sane_get_devices\n");
  local_only = SANE_TRUE;	/* Avoid compile warning */

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
sane_open (SANE_String_Const devname, SANE_Handle * handle)
{
  V4L_Device *dev;
  V4L_Scanner *s;
  static int v4lfd;
  int i;
  struct video_channel channel;
  SANE_Status status;
  int max_channels = MAX_CHANNELS;

  if (!devname)
    {
      DBG (1, "sane_open: devname == 0\n");
      return SANE_STATUS_INVAL;
    }

  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0)
      {
	DBG (5, "sane_open: device %s found in devlist\n", devname);
	break;
      }
  if (!devname[0])
    dev = first_dev;
  if (!dev)
    {
      DBG (1, "sane_open: device %s doesn't seem to be a v4l "
	   "device\n", devname);
      return SANE_STATUS_INVAL;
    }

  v4lfd = v4l1_open (devname, O_RDWR);
  if (v4lfd == -1)
    {
      DBG (1, "sane_open: can't open %s (%s)\n", devname, strerror (errno));
      return SANE_STATUS_INVAL;
    }
  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->user_corner = 0;		/* ??? */
  s->devicename = devname;
  s->fd = v4lfd;

  if (v4l1_ioctl (s->fd, VIDIOCGCAP, &s->capability) == -1)
    {
      DBG (1, "sane_open: ioctl (%d, VIDIOCGCAP,..) failed on `%s': %s\n",
	   s->fd, devname, strerror (errno));
      v4l1_close (s->fd);
      return SANE_STATUS_INVAL;
    }

  DBG (5, "sane_open: %d channels, %d audio devices\n",
       s->capability.channels, s->capability.audios);
  DBG (5, "sane_open: minwidth=%d, minheight=%d, maxwidth=%d, "
       "maxheight=%d\n", s->capability.minwidth, s->capability.minheight,
       s->capability.maxwidth, s->capability.maxheight);
  if (VID_TYPE_CAPTURE & s->capability.type)
    DBG (5, "sane_open: V4L device can capture to memory\n");
  if (VID_TYPE_TUNER & s->capability.type)
    DBG (5, "sane_open: V4L device has a tuner of some form\n");
  if (VID_TYPE_TELETEXT & s->capability.type)
    DBG (5, "sane_open: V4L device supports teletext\n");
  if (VID_TYPE_OVERLAY & s->capability.type)
    DBG (5, "sane_open: V4L device can overlay its image onto the frame "
	 "buffer\n");
  if (VID_TYPE_CHROMAKEY & s->capability.type)
    DBG (5, "sane_open: V4L device uses chromakey on overlay\n");
  if (VID_TYPE_CLIPPING & s->capability.type)
    DBG (5, "sane_open: V4L device supports overlay clipping\n");
  if (VID_TYPE_FRAMERAM & s->capability.type)
    DBG (5, "sane_open: V4L device overwrites frame buffer memory\n");
  if (VID_TYPE_SCALES & s->capability.type)
    DBG (5, "sane_open: V4L device supports hardware scaling\n");
  if (VID_TYPE_MONOCHROME & s->capability.type)
    DBG (5, "sane_open: V4L device is grey scale only\n");
  if (VID_TYPE_SUBCAPTURE & s->capability.type)
    DBG (5, "sane_open: V4L device can capture parts of the image\n");

  if (s->capability.channels < max_channels)
    max_channels = s->capability.channels;
  for (i = 0; i < max_channels; i++)
    {
      channel.channel = i;
      if (-1 == v4l1_ioctl (v4lfd, VIDIOCGCHAN, &channel))
	{
	  DBG (1, "sane_open: can't ioctl VIDIOCGCHAN %s: %s\n", devname,
	       strerror (errno));
	  return SANE_STATUS_INVAL;
	}
      DBG (5, "sane_open: channel %d (%s), tuners=%d, flags=0x%x, "
	   "type=%d, norm=%d\n", channel.channel, channel.name,
	   channel.tuners, channel.flags, channel.type, channel.norm);
      if (VIDEO_VC_TUNER & channel.flags)
	DBG (5, "sane_open: channel has tuner(s)\n");
      if (VIDEO_VC_AUDIO & channel.flags)
	DBG (5, "sane_open: channel has audio\n");
      if (VIDEO_TYPE_TV == channel.type)
	DBG (5, "sane_open: input is TV input\n");
      if (VIDEO_TYPE_CAMERA == channel.type)
	DBG (5, "sane_open: input is camera input\n");
      s->channel[i] = strdup (channel.name);
      if (!s->channel[i])
	return SANE_STATUS_NO_MEM;
    }
  s->channel[i] = 0;
  if (-1 == v4l1_ioctl (v4lfd, VIDIOCGPICT, &s->pict))
    {
      DBG (1, "sane_open: can't ioctl VIDIOCGPICT %s: %s\n", devname,
	   strerror (errno));
      return SANE_STATUS_INVAL;
    }
  DBG (5, "sane_open: brightness=%d, hue=%d, colour=%d, contrast=%d\n",
       s->pict.brightness, s->pict.hue, s->pict.colour, s->pict.contrast);
  DBG (5, "sane_open: whiteness=%d, depth=%d, palette=%d\n",
       s->pict.whiteness, s->pict.depth, s->pict.palette);

  /* ??? */
  s->pict.palette = VIDEO_PALETTE_GREY;
  if (-1 == v4l1_ioctl (s->fd, VIDIOCSPICT, &s->pict))
    {
      DBG (1, "sane_open: ioctl VIDIOCSPICT failed (%s)\n", strerror (errno));
    }

  if (-1 == v4l1_ioctl (s->fd, VIDIOCGWIN, &s->window))
    {
      DBG (1, "sane_open: can't ioctl VIDIOCGWIN %s: %s\n", devname,
	   strerror (errno));
      return SANE_STATUS_INVAL;
    }
  DBG (5, "sane_open: x=%d, y=%d, width=%d, height=%d\n",
       s->window.x, s->window.y, s->window.width, s->window.height);

  /* already done in sane_start 
     if (-1 == v4l1_ioctl (v4lfd, VIDIOCGMBUF, &mbuf))
     DBG (1, "sane_open: can't ioctl VIDIOCGMBUF (no Fbuffer?)\n");
   */

  status = init_options (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  update_parameters (s);

  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;

  *handle = s;

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  V4L_Scanner *prev, *s;

  DBG (2, "sane_close: trying to close handle %p\n", (void *) handle);
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
      DBG (1, "sane_close: bad handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }
  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  if (s->scanning)
    sane_cancel (handle);
  v4l1_close (s->fd);
  free (s);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  V4L_Scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS || option < 0)
    return 0;
  DBG (4, "sane_get_option_descriptor: option %d (%s)\n", option,
       s->opt[option].name ? s->opt[option].name : s->opt[option].title);
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  V4L_Scanner *s = handle;
  SANE_Status status;
  SANE_Word cap;

  if (info)
    *info = 0;

  if (option >= NUM_OPTIONS || option < 0)
    return SANE_STATUS_INVAL;

  DBG (4, "sane_control_option: %s option %d (%s)\n",
       action == SANE_ACTION_GET_VALUE ? "get" :
       action == SANE_ACTION_SET_VALUE ? "set" :
       action == SANE_ACTION_SET_AUTO ? "auto set" :
       "(unknow action with)", option,
       s->opt[option].name ? s->opt[option].name : s->opt[option].title);

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      DBG (1, "sane_control option: option is inactive\n");
      return SANE_STATUS_INVAL;
    }

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_NUM_OPTS:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_BRIGHTNESS:
	case OPT_HUE:
	case OPT_COLOR:
	case OPT_CONTRAST:
	case OPT_WHITE_LEVEL:
	  *(SANE_Word *) val = s->val[option].w;
	  return SANE_STATUS_GOOD;
	case OPT_CHANNEL:	/* string list options */
	case OPT_MODE:
	  strcpy (val, s->val[option].s);
	  return SANE_STATUS_GOOD;
	default:
	  DBG (1, "sane_control_option: option %d unknown\n", option);
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (1, "sane_control_option: option is not settable\n");
	  return SANE_STATUS_INVAL;
	}
      status = sanei_constrain_value (s->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "sane_control_option: sanei_constarin_value failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      if (option >= OPT_TL_X && option <= OPT_BR_Y)
	{
	  s->user_corner |= 1 << (option - OPT_TL_X);
	  if (-1 == v4l1_ioctl (s->fd, VIDIOCGWIN, &s->window))
	    {
	      DBG (1, "sane_control_option: ioctl VIDIOCGWIN failed "
		   "(can not get window geometry)\n");
	      return SANE_STATUS_INVAL;
	    }
	  s->window.clipcount = 0;
	  s->window.clips = 0;
	  s->window.height = parms.lines;
	  s->window.width = parms.pixels_per_line;
	}


      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_TL_X:
	  break;
	case OPT_TL_Y:
	  break;
	case OPT_BR_X:
	  s->window.width = *(SANE_Word *) val;
	  parms.pixels_per_line = *(SANE_Word *) val;
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_BR_Y:
	  s->window.height = *(SANE_Word *) val;
	  parms.lines = *(SANE_Word *) val;
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_MODE:
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  s->val[option].s = strdup (val);
	  if (!s->val[option].s)
	    return SANE_STATUS_NO_MEM;
	  if (strcmp (s->val[option].s, SANE_VALUE_SCAN_MODE_GRAY) == 0)
	    s->pict.palette = VIDEO_PALETTE_GREY;
	  else
	    s->pict.palette = VIDEO_PALETTE_RGB24;
	  update_parameters (s);
	  break;
	case OPT_BRIGHTNESS:
	  s->pict.brightness = *(SANE_Word *) val *256;
	  s->val[option].w = *(SANE_Word *) val;
	  break;
	case OPT_HUE:
	  s->pict.hue = *(SANE_Word *) val *256;
	  s->val[option].w = *(SANE_Word *) val;
	  break;
	case OPT_COLOR:
	  s->pict.colour = *(SANE_Word *) val *256;
	  s->val[option].w = *(SANE_Word *) val;
	  break;
	case OPT_CONTRAST:
	  s->pict.contrast = *(SANE_Word *) val *256;
	  s->val[option].w = *(SANE_Word *) val;
	  break;
	case OPT_WHITE_LEVEL:
	  s->pict.whiteness = *(SANE_Word *) val *256;
	  s->val[option].w = *(SANE_Word *) val;
	  break;
	case OPT_CHANNEL:
	  {
	    int i;
	    struct video_channel channel;

	    s->val[option].s = strdup (val);
	    if (!s->val[option].s)
	      return SANE_STATUS_NO_MEM;
	    for (i = 0; i < MAX_CHANNELS; i++)
	      {
		if (strcmp (s->channel[i], val) == 0)
		  {
		    channel.channel = i;
		    if (-1 == v4l1_ioctl (s->fd, VIDIOCGCHAN, &channel))
		      {
			DBG (1, "sane_open: can't ioctl VIDIOCGCHAN %s: %s\n",
			     s->devicename, strerror (errno));
			return SANE_STATUS_INVAL;
		      }
		    if (-1 == v4l1_ioctl (s->fd, VIDIOCSCHAN, &channel))
		      {
			DBG (1, "sane_open: can't ioctl VIDIOCSCHAN %s: %s\n",
			     s->devicename, strerror (errno));
			return SANE_STATUS_INVAL;
		      }
		    break;
		  }
	      }
	    return SANE_STATUS_GOOD;
	    break;
	  }
	default:
	  DBG (1, "sane_control_option: option %d unknown\n", option);
	  return SANE_STATUS_INVAL;
	}
      if (option >= OPT_TL_X && option <= OPT_BR_Y)
	{
	  if (-1 == v4l1_ioctl (s->fd, VIDIOCSWIN, &s->window))
	    {
	      DBG (1, "sane_control_option: ioctl VIDIOCSWIN failed (%s)\n",
		   strerror (errno));
	      /* return SANE_STATUS_INVAL; */
	    }
	  if (-1 == v4l1_ioctl (s->fd, VIDIOCGWIN, &s->window))
	    {
	      DBG (1, "sane_control_option: ioctl VIDIOCGWIN failed (%s)\n",
		   strerror (errno));
	      return SANE_STATUS_INVAL;
	    }
	}
      if (option >= OPT_BRIGHTNESS && option <= OPT_WHITE_LEVEL)
	{
	  if (-1 == v4l1_ioctl (s->fd, VIDIOCSPICT, &s->pict))
	    {
	      DBG (1, "sane_control_option: ioctl VIDIOCSPICT failed (%s)\n",
		   strerror (errno));
	      /* return SANE_STATUS_INVAL; */
	    }
	}
      return SANE_STATUS_GOOD;
    }
  else if (action == SANE_ACTION_SET_AUTO)
    {
      if (!(cap & SANE_CAP_AUTOMATIC))
	{
	  DBG (1, "sane_control_option: option can't be set automatically\n");
	  return SANE_STATUS_INVAL;
	}
      switch (option)
	{
	case OPT_BRIGHTNESS:
	  /* not implemented yet */
	  return SANE_STATUS_GOOD;

	default:
	  break;
	}
    }
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  V4L_Scanner *s = handle;

  DBG (4, "sane_get_parameters\n");
  update_parameters (s);
  if (params == 0)
    {
      DBG (1, "sane_get_parameters: params == 0\n");
      return SANE_STATUS_INVAL;
    }
  if (-1 == v4l1_ioctl (s->fd, VIDIOCGWIN, &s->window))
    {
      DBG (1, "sane_control_option: ioctl VIDIOCGWIN failed "
	   "(can not get window geometry)\n");
      return SANE_STATUS_INVAL;
    }
  parms.pixels_per_line = s->window.width;
  parms.bytes_per_line = s->window.width;
  if (parms.format == SANE_FRAME_RGB)
    parms.bytes_per_line = s->window.width * 3;
  parms.lines = s->window.height;
  *params = parms;
  return SANE_STATUS_GOOD;

}

SANE_Status
sane_start (SANE_Handle handle)
{
  int len, loop;
  V4L_Scanner *s;
  char data;

  DBG (2, "sane_start\n");
  for (s = first_handle; s; s = s->next)
    {
      if (s == handle)
	break;
    }
  if (!s)
    {
      DBG (1, "sane_start: bad handle %p\n", handle);
      return SANE_STATUS_INVAL;	/* oops, not a handle we know about */
    }
  len = v4l1_ioctl (s->fd, VIDIOCGCAP, &s->capability);
  if (-1 == len)
    {
      DBG (1, "sane_start: can not get capabilities\n");
      return SANE_STATUS_INVAL;
    }
  s->buffercount = 0;
  if (-1 == v4l1_ioctl (s->fd, VIDIOCGMBUF, &s->mbuf))
    {
      s->is_mmap = SANE_FALSE;
      buffer =
	malloc (s->capability.maxwidth * s->capability.maxheight *
		s->pict.depth);
      if (0 == buffer)
	return SANE_STATUS_NO_MEM;
      DBG (3, "sane_start: V4L trying to read frame\n");
      len = v4l1_read (s->fd, buffer, parms.bytes_per_line * parms.lines);
      DBG (3, "sane_start: %d bytes read\n", len);
    }
  else
    {
      s->is_mmap = SANE_TRUE;
      DBG (3,
	   "sane_start: mmap frame, buffersize: %d bytes, buffers: %d, offset 0 %d\n",
	   s->mbuf.size, s->mbuf.frames, s->mbuf.offsets[0]);
      buffer =
	v4l1_mmap (0, s->mbuf.size, PROT_READ | PROT_WRITE, MAP_SHARED, s->fd, 0);
      if (buffer == (void *)-1)
	{
	  DBG (1, "sane_start: mmap failed: %s\n", strerror (errno));
	  buffer = NULL;
	  return SANE_STATUS_IO_ERROR;
	}
      DBG (3, "sane_start: mmapped frame, capture 1 pict into %p\n", buffer);
      s->mmap.frame = 0;
      s->mmap.width = s->window.width;
      /*   s->mmap.width = parms.pixels_per_line;  ??? huh? */
      s->mmap.height = s->window.height;
      /*      s->mmap.height = parms.lines;  ??? huh? */
      s->mmap.format = s->pict.palette;
      DBG (2, "sane_start: mmapped frame %d x %d with palette %d\n",
	   s->mmap.width, s->mmap.height, s->mmap.format);

      /* We need to loop here to empty the read buffers, so we don't
         get a stale image */
      for (loop = 0; loop <= s->mbuf.frames; loop++)
        {
          len = v4l1_ioctl (s->fd, VIDIOCMCAPTURE, &s->mmap);
          if (len == -1)
	    {
	      DBG (1, "sane_start: ioctl VIDIOCMCAPTURE failed: %s\n",
	           strerror (errno));
	      return SANE_STATUS_INVAL;
	    }
          DBG (3, "sane_start: waiting for frame %x, loop %d\n", s->mmap.frame, loop);
          len = v4l1_ioctl (s->fd, VIDIOCSYNC, &(s->mmap.frame));
          if (-1 == len)
	    {
	      DBG (1, "sane_start: call to ioctl(%d, VIDIOCSYNC, ..) failed\n",
	           s->fd);
	      return SANE_STATUS_INVAL;
	    }
        }
      DBG (3, "sane_start: frame %x done\n", s->mmap.frame);
    }

  /* v4l1 actually returns BGR when we ask for RGB, so convert it */
  if (s->pict.palette == VIDEO_PALETTE_RGB24)
    {
      DBG (3, "sane_start: converting from BGR to RGB\n");
      for (loop = 0; loop < (s->window.width * s->window.height * 3); loop += 3)
        {
          data = *(buffer + loop);
          *(buffer + loop) = *(buffer + loop + 2);
          *(buffer + loop + 2) = data;
        }
    }

  DBG (3, "sane_start: done\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * lenp)
{
  int i, min;
  V4L_Scanner *s = handle;

  DBG (4, "sane_read: max_len = %d\n", max_len);
  if (!lenp)
    {
      DBG (1, "sane_read: lenp == 0\n");
      return SANE_STATUS_INVAL;
    }
  if ((s->buffercount + 1) > (parms.lines * parms.bytes_per_line))
    {
      *lenp = 0;
      return SANE_STATUS_EOF;
    };
  min = parms.lines * parms.bytes_per_line;
  if (min > (max_len + s->buffercount))
    min = (max_len + s->buffercount);
  if (s->is_mmap == SANE_FALSE)
    {
      for (i = s->buffercount; i < (min + 0); i++)
	{
	  *(buf + i - s->buffercount) = *(buffer + i);
	};
      *lenp = (parms.lines * parms.bytes_per_line - s->buffercount);
      if (max_len < *lenp)
	*lenp = max_len;
      DBG (3, "sane_read: transferred %d bytes (from %d to %d)\n", *lenp,
	   s->buffercount, i);
      s->buffercount = i;
    }
  else
    {
      for (i = s->buffercount; i < (min + 0); i++)
	{
	  *(buf + i - s->buffercount) = *(buffer + i);
	};
      *lenp = (parms.lines * parms.bytes_per_line - s->buffercount);
      if ((i - s->buffercount) < *lenp)
	*lenp = (i - s->buffercount);
      DBG (3, "sane_read: transferred %d bytes (from %d to %d)\n", *lenp,
	   s->buffercount, i);
      s->buffercount = i;
    }
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  V4L_Scanner *s = handle;

  DBG (2, "sane_cancel\n");

  /* ??? buffer isn't checked in sane_read? */
  if (buffer)
    {
      if (s->is_mmap)
	v4l1_munmap(buffer, s->mbuf.size);
      else
	free (buffer);

      buffer = NULL;
    }
}


SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  /* Avoid compile warning */
  handle = 0;

  if (non_blocking == SANE_FALSE)
    return SANE_STATUS_GOOD;
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  /* Avoid compile warning */
  handle = 0;
  fd = 0;

  return SANE_STATUS_UNSUPPORTED;
}
