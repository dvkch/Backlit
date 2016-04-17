/* lexmark.c: SANE backend for Lexmark scanners.

   (C) 2003-2004 Lexmark International, Inc. (Original Source code)
   (C) 2005 Fred Odendaal
   (C) 2006-2013 Stéphane Voltz <stef.dev@free.fr>
   (C) 2010 "Torsten Houwaart" <ToHo@gmx.de> X74 support
   
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

   **************************************************************************/

#include "lexmark.h"

#define LEXMARK_CONFIG_FILE "lexmark.conf"
#define BUILD 32
#define MAX_OPTION_STRING_SIZE 255

static Lexmark_Device *first_lexmark_device = 0;
static SANE_Int num_lexmark_device = 0;
static const SANE_Device **sane_device_list = NULL;

/* Program globals F.O - Should this be per device?*/
static SANE_Bool initialized = SANE_FALSE;

static SANE_String_Const mode_list[] = {
  SANE_VALUE_SCAN_MODE_COLOR,
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_LINEART,
  NULL
};

/* possible resolutions are: 75x75, 150x150, 300x300, 600x600, 600x1200 */

static SANE_Int x1100_dpi_list[] = {
  5, 75, 150, 300, 600, 1200
};

static SANE_Int a920_dpi_list[] = {
  4, 75, 150, 300, 600
};

static SANE_Int x1200_dpi_list[] = {
  4, 75, 150, 300, 600
};

static SANE_Int x74_dpi_list[] = {
  75, 150, 300, 600
};

static SANE_Range threshold_range = {
  SANE_FIX (0.0),		/* minimum */
  SANE_FIX (100.0),		/* maximum */
  SANE_FIX (1.0)		/* quantization */
};

static const SANE_Range gain_range = {
  0,				/* minimum */
  31,				/* maximum */
  0				/* quantization */
};

/* for now known models (2 ...) have the same scan window geometry.
   coordinates are expressed in pixels, with a quantization factor of
   8 to have 'even' coordinates at 75 dpi */
static SANE_Range x_range = {
  0,				/* minimum */
  5104,				/* maximum */
  16				/* quantization : 16 is required so we
				   never have an odd width */
};

static SANE_Range y_range = {
  0,				/* minimum */
  6848,				/* maximum */
  /* 7032, for X74 */
  8				/* quantization */
};

/* static functions */
static SANE_Status init_options (Lexmark_Device * lexmark_device);
static SANE_Status attachLexmark (SANE_String_Const devname);

SANE_Status
init_options (Lexmark_Device * dev)
{

  SANE_Option_Descriptor *od;

  DBG (2, "init_options: dev = %p\n", (void *) dev);

  /* number of options */
  od = &(dev->opt[OPT_NUM_OPTS]);
  od->name = SANE_NAME_NUM_OPTIONS;
  od->title = SANE_TITLE_NUM_OPTIONS;
  od->desc = SANE_DESC_NUM_OPTIONS;
  od->type = SANE_TYPE_INT;
  od->unit = SANE_UNIT_NONE;
  od->size = sizeof (SANE_Word);
  od->cap = SANE_CAP_SOFT_DETECT;
  od->constraint_type = SANE_CONSTRAINT_NONE;
  od->constraint.range = 0;
  dev->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* mode - sets the scan mode: Color, Gray, or Line Art */
  od = &(dev->opt[OPT_MODE]);
  od->name = SANE_NAME_SCAN_MODE;
  od->title = SANE_TITLE_SCAN_MODE;
  od->desc = SANE_DESC_SCAN_MODE;;
  od->type = SANE_TYPE_STRING;
  od->unit = SANE_UNIT_NONE;
  od->size = MAX_OPTION_STRING_SIZE;
  od->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  od->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  od->constraint.string_list = mode_list;
  dev->val[OPT_MODE].s = malloc (od->size);
  if (!dev->val[OPT_MODE].s)
    return SANE_STATUS_NO_MEM;
  strcpy (dev->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR);

  /* resolution */
  od = &(dev->opt[OPT_RESOLUTION]);
  od->name = SANE_NAME_SCAN_RESOLUTION;
  od->title = SANE_TITLE_SCAN_RESOLUTION;
  od->desc = SANE_DESC_SCAN_RESOLUTION;
  od->type = SANE_TYPE_INT;
  od->unit = SANE_UNIT_DPI;
  od->size = sizeof (SANE_Word);
  od->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  od->constraint_type = SANE_CONSTRAINT_WORD_LIST;
  switch (dev->model.sensor_type)
    {
    case X1100_2C_SENSOR:
    case A920_SENSOR:
      od->constraint.word_list = a920_dpi_list;
      break;
    case X1100_B2_SENSOR:
      od->constraint.word_list = x1100_dpi_list;
      break;
    case X1200_SENSOR:
    case X1200_USB2_SENSOR:
      od->constraint.word_list = x1200_dpi_list;
      break;
    case X74_SENSOR:
      od->constraint.word_list = x74_dpi_list;
      break;
    }
  dev->val[OPT_RESOLUTION].w = 75;

  /* preview mode */
  od = &(dev->opt[OPT_PREVIEW]);
  od->name = SANE_NAME_PREVIEW;
  od->title = SANE_TITLE_PREVIEW;
  od->desc = SANE_DESC_PREVIEW;
  od->size = sizeof (SANE_Word);
  od->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  od->type = SANE_TYPE_BOOL;
  od->constraint_type = SANE_CONSTRAINT_NONE;
  dev->val[OPT_PREVIEW].w = SANE_FALSE;

  /* "Geometry" group: */
  od = &(dev->opt[OPT_GEOMETRY_GROUP]);
  od->name = "";
  od->title = SANE_I18N ("Geometry");
  od->desc = "";
  od->type = SANE_TYPE_GROUP;
  od->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  od->size = 0;
  od->constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  od = &(dev->opt[OPT_TL_X]);
  od->name = SANE_NAME_SCAN_TL_X;
  od->title = SANE_TITLE_SCAN_TL_X;
  od->desc = SANE_DESC_SCAN_TL_X;
  od->type = SANE_TYPE_INT;
  od->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  od->size = sizeof (SANE_Word);
  od->unit = SANE_UNIT_PIXEL;
  od->constraint_type = SANE_CONSTRAINT_RANGE;
  od->constraint.range = &x_range;
  dev->val[OPT_TL_X].w = 0;

  /* top-left y */
  od = &(dev->opt[OPT_TL_Y]);
  od->name = SANE_NAME_SCAN_TL_Y;
  od->title = SANE_TITLE_SCAN_TL_Y;
  od->desc = SANE_DESC_SCAN_TL_Y;
  od->type = SANE_TYPE_INT;
  od->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  od->size = sizeof (SANE_Word);
  od->unit = SANE_UNIT_PIXEL;
  od->constraint_type = SANE_CONSTRAINT_RANGE;
  od->constraint.range = &y_range;
  dev->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  od = &(dev->opt[OPT_BR_X]);
  od->name = SANE_NAME_SCAN_BR_X;
  od->title = SANE_TITLE_SCAN_BR_X;
  od->desc = SANE_DESC_SCAN_BR_X;
  od->type = SANE_TYPE_INT;
  od->size = sizeof (SANE_Word);
  od->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  od->unit = SANE_UNIT_PIXEL;
  od->constraint_type = SANE_CONSTRAINT_RANGE;
  od->constraint.range = &x_range;
  dev->val[OPT_BR_X].w = x_range.max;

  /* bottom-right y */
  od = &(dev->opt[OPT_BR_Y]);
  od->name = SANE_NAME_SCAN_BR_Y;
  od->title = SANE_TITLE_SCAN_BR_Y;
  od->desc = SANE_DESC_SCAN_BR_Y;
  od->type = SANE_TYPE_INT;
  od->size = sizeof (SANE_Word);
  od->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  od->unit = SANE_UNIT_PIXEL;
  od->constraint_type = SANE_CONSTRAINT_RANGE;
  od->constraint.range = &y_range;
  dev->val[OPT_BR_Y].w = y_range.max;

  /* threshold */
  od = &(dev->opt[OPT_THRESHOLD]);
  od->name = SANE_NAME_THRESHOLD;
  od->title = SANE_TITLE_THRESHOLD;
  od->desc = SANE_DESC_THRESHOLD;
  od->type = SANE_TYPE_FIXED;
  od->unit = SANE_UNIT_PERCENT;
  od->size = sizeof (SANE_Fixed);
  od->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE;
  od->constraint_type = SANE_CONSTRAINT_RANGE;
  od->constraint.range = &threshold_range;
  dev->val[OPT_THRESHOLD].w = SANE_FIX (50.0);

  /*  gain group */
  dev->opt[OPT_MANUAL_GAIN].name = "manual-channel-gain";
  dev->opt[OPT_MANUAL_GAIN].title = SANE_I18N ("Gain");
  dev->opt[OPT_MANUAL_GAIN].desc = SANE_I18N ("Color channels gain settings");
  dev->opt[OPT_MANUAL_GAIN].type = SANE_TYPE_BOOL;
  dev->opt[OPT_MANUAL_GAIN].cap =
    SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT | SANE_CAP_ADVANCED;
  dev->opt[OPT_MANUAL_GAIN].size = sizeof (SANE_Bool);
  dev->val[OPT_MANUAL_GAIN].w = SANE_FALSE;

  /* gray gain */
  dev->opt[OPT_GRAY_GAIN].name = "gray-gain";
  dev->opt[OPT_GRAY_GAIN].title = SANE_I18N ("Gray gain");
  dev->opt[OPT_GRAY_GAIN].desc = SANE_I18N ("Sets gray channel gain");
  dev->opt[OPT_GRAY_GAIN].type = SANE_TYPE_INT;
  dev->opt[OPT_GRAY_GAIN].cap =
    SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT | SANE_CAP_INACTIVE |
    SANE_CAP_ADVANCED;
  dev->opt[OPT_GRAY_GAIN].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GRAY_GAIN].size = sizeof (SANE_Int);
  dev->opt[OPT_GRAY_GAIN].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GRAY_GAIN].constraint.range = &gain_range;
  dev->val[OPT_GRAY_GAIN].w = 10;

  /* red gain */
  dev->opt[OPT_RED_GAIN].name = "red-gain";
  dev->opt[OPT_RED_GAIN].title = SANE_I18N ("Red gain");
  dev->opt[OPT_RED_GAIN].desc = SANE_I18N ("Sets red channel gain");
  dev->opt[OPT_RED_GAIN].type = SANE_TYPE_INT;
  dev->opt[OPT_RED_GAIN].cap =
    SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT | SANE_CAP_INACTIVE |
    SANE_CAP_ADVANCED;
  dev->opt[OPT_RED_GAIN].unit = SANE_UNIT_NONE;
  dev->opt[OPT_RED_GAIN].size = sizeof (SANE_Int);
  dev->opt[OPT_RED_GAIN].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_RED_GAIN].constraint.range = &gain_range;
  dev->val[OPT_RED_GAIN].w = 10;

  /* green gain */
  dev->opt[OPT_GREEN_GAIN].name = "green-gain";
  dev->opt[OPT_GREEN_GAIN].title = SANE_I18N ("Green gain");
  dev->opt[OPT_GREEN_GAIN].desc = SANE_I18N ("Sets green channel gain");
  dev->opt[OPT_GREEN_GAIN].type = SANE_TYPE_INT;
  dev->opt[OPT_GREEN_GAIN].cap =
    SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT | SANE_CAP_INACTIVE |
    SANE_CAP_ADVANCED;
  dev->opt[OPT_GREEN_GAIN].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GREEN_GAIN].size = sizeof (SANE_Int);
  dev->opt[OPT_GREEN_GAIN].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GREEN_GAIN].constraint.range = &gain_range;
  dev->val[OPT_GREEN_GAIN].w = 10;

  /* blue gain */
  dev->opt[OPT_BLUE_GAIN].name = "blue-gain";
  dev->opt[OPT_BLUE_GAIN].title = SANE_I18N ("Blue gain");
  dev->opt[OPT_BLUE_GAIN].desc = SANE_I18N ("Sets blue channel gain");
  dev->opt[OPT_BLUE_GAIN].type = SANE_TYPE_INT;
  dev->opt[OPT_BLUE_GAIN].cap =
    SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT | SANE_CAP_INACTIVE |
    SANE_CAP_ADVANCED;
  dev->opt[OPT_BLUE_GAIN].unit = SANE_UNIT_NONE;
  dev->opt[OPT_BLUE_GAIN].size = sizeof (SANE_Int);
  dev->opt[OPT_BLUE_GAIN].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_BLUE_GAIN].constraint.range = &gain_range;
  dev->val[OPT_BLUE_GAIN].w = 10;

  return SANE_STATUS_GOOD;
}


/***************************** SANE API ****************************/

SANE_Status
attachLexmark (SANE_String_Const devname)
{
  Lexmark_Device *lexmark_device;
  SANE_Int dn, vendor, product, variant;
  SANE_Status status;

  DBG (2, "attachLexmark: devname=%s\n", devname);

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      /* already attached devices */
      if (strcmp (lexmark_device->sane.name, devname) == 0)
      {
        lexmark_device->missing = SANE_FALSE;
	return SANE_STATUS_GOOD;
      }
    }

  lexmark_device = (Lexmark_Device *) malloc (sizeof (Lexmark_Device));
  if (lexmark_device == NULL)
    return SANE_STATUS_NO_MEM;

#ifdef FAKE_USB
  status = SANE_STATUS_GOOD;
#else
  status = sanei_usb_open (devname, &dn);
#endif
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attachLexmark: couldn't open device `%s': %s\n", devname,
	   sane_strstatus (status));
      return status;
    }
  else
    DBG (2, "attachLexmark: device `%s' successfully opened\n", devname);

#ifdef FAKE_USB
  status = SANE_STATUS_GOOD;
  /* put the id of the model you want to fake here */
  vendor = 0x043d;
  product = 0x007c;		/* X11xx */
  variant = 0xb2;
#else
  variant = 0;
  status = sanei_usb_get_vendor_product (dn, &vendor, &product);
#endif
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1,
	   "attachLexmark: couldn't get vendor and product ids of device `%s': %s\n",
	   devname, sane_strstatus (status));
#ifndef FAKE_USB
      sanei_usb_close (dn);
#endif
      return status;
    }
#ifndef FAKE_USB
  sanei_usb_close (dn);
#endif

  DBG (2, "attachLexmark: testing device `%s': 0x%04x:0x%04x, variant=%d\n",
       devname, vendor, product, variant);
  if (sanei_lexmark_low_assign_model (lexmark_device,
				      devname,
				      vendor,
				      product, variant) != SANE_STATUS_GOOD)
    {
      DBG (2, "attachLexmark: unsupported device `%s': 0x%04x:0x%04x\n",
	   devname, vendor, product);
      return SANE_STATUS_UNSUPPORTED;
    }

  /* add new device to device list */

  /* there are two variant of the scanner with the same USB id,
   * so we need to read registers from scanner to detect which one
   * is really connected */
  status = sanei_lexmark_low_open_device (lexmark_device);
  sanei_usb_close (lexmark_device->devnum);

  /* set up scanner start status */
  sanei_lexmark_low_init (lexmark_device);

  /* Set the default resolution here */
  lexmark_device->x_dpi = 75;
  lexmark_device->y_dpi = 75;

  /* Make the pointer to the read buffer null here */
  lexmark_device->read_buffer = NULL;

  /* Set the default threshold for lineart mode here */
  lexmark_device->threshold = 0x80;

  lexmark_device->shading_coeff = NULL;

  /* mark device as present */
  lexmark_device->missing = SANE_FALSE;

  /* insert it a the start of the chained list */
  lexmark_device->next = first_lexmark_device;
  first_lexmark_device = lexmark_device;

  num_lexmark_device++;

  return status;
}

/** probe for supported lexmark devices
 * This function scan usb and try to attached to scanner
 * configured in lexmark.conf .
 */
static SANE_Status
probe_lexmark_devices (void)
{
  FILE *fp;
  SANE_Char line[PATH_MAX];
  const char *lp;
  SANE_Int vendor, product;
  size_t len;
  Lexmark_Device *dev;

  /* mark already detected devices as missing, during device probe
   * detected devices will clear this flag */
  dev = first_lexmark_device;
  while (dev != NULL)
    {
      dev->missing = SANE_TRUE;
      dev = dev->next;
    }

  /* open config file, parse option and try to open
   * any device configure in it */
  fp = sanei_config_open (LEXMARK_CONFIG_FILE);
  if (!fp)
    {
      return SANE_STATUS_ACCESS_DENIED;
    }

  while (sanei_config_read (line, PATH_MAX, fp))
    {
      /* ignore comments */
      if (line[0] == '#')
	continue;
      len = strlen (line);

      /* delete newline characters at end */
      if (line[len - 1] == '\n')
	line[--len] = '\0';

      lp = sanei_config_skip_whitespace (line);
      /* skip empty lines */
      if (*lp == 0)
	continue;

      if (sscanf (lp, "usb %i %i", &vendor, &product) == 2)
	;
      else if (strncmp ("libusb", lp, 6) == 0)
	;
      else if ((strncmp ("usb", lp, 3) == 0) && isspace (lp[3]))
	{
	  lp += 3;
	  lp = sanei_config_skip_whitespace (lp);
	}
      else
	continue;

#ifdef FAKE_USB
      attachLexmark ("FAKE_USB");
#else
      sanei_usb_attach_matching_devices (lp, attachLexmark);
#endif
    }

  fclose (fp);
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_init (SANE_Int * version_code,
	   SANE_Auth_Callback __sane_unused__ authorize)
{
  SANE_Status status;

  DBG_INIT ();

  DBG (1, "SANE Lexmark backend version %d.%d.%d-devel\n", SANE_CURRENT_MAJOR,
       V_MINOR, BUILD);

  DBG (2, "sane_init: version_code=%p\n", (void *) version_code);

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);

#ifndef FAKE_USB
  sanei_usb_init ();
#endif

  status = probe_lexmark_devices ();

  if (status == SANE_STATUS_GOOD)
    {
      initialized = SANE_TRUE;
    }
  else
    {
      initialized = SANE_FALSE;
    }

  return status;
}

void
sane_exit (void)
{
  Lexmark_Device *lexmark_device, *next_lexmark_device;

  DBG (2, "sane_exit\n");

  if (!initialized)
    return;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = next_lexmark_device)
    {
      next_lexmark_device = lexmark_device->next;
      sanei_lexmark_low_destroy (lexmark_device);
      free (lexmark_device);
    }

  if (sane_device_list)
    free (sane_device_list);

  sanei_usb_exit();
  initialized = SANE_FALSE;

  return;
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  Lexmark_Device *lexmark_device;
  SANE_Int index;

  DBG (2, "sane_get_devices: device_list=%p, local_only=%d\n",
       (void *) device_list, local_only);

  /* hot-plug case : detection of newly connected scanners */
  sanei_usb_scan_devices ();
  probe_lexmark_devices ();

  if (sane_device_list)
    free (sane_device_list);

  sane_device_list = malloc ((num_lexmark_device + 1) *
			     sizeof (sane_device_list[0]));

  if (!sane_device_list)
    return SANE_STATUS_NO_MEM;

  index = 0;
  lexmark_device = first_lexmark_device;
  while (lexmark_device != NULL)
    {
      if (lexmark_device->missing == SANE_FALSE)
	{
	  sane_device_list[index] = &(lexmark_device->sane);
	  index++;
	}
      lexmark_device = lexmark_device->next;
    }
  sane_device_list[index] = 0;

  *device_list = sane_device_list;

  return SANE_STATUS_GOOD;
}


/**
 * Open the backend, ie return the struct handle of a detected scanner
 * The struct returned is choosne if it matches the name given, which is
 * usefull when several scanners handled by the backend have been detected.
 * However, special case empty string "" and "lexmark" pick the first 
 * available handle.
 */
SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Lexmark_Device *lexmark_device;
  SANE_Status status;

  DBG (2, "sane_open: devicename=\"%s\", handle=%p\n", devicename,
       (void *) handle);

  if (!initialized)
    {
      DBG (2, "sane_open: not initialized\n");
      return SANE_STATUS_INVAL;
    }

  if (!handle)
    {
      DBG (2, "sane_open: no handle\n");
      return SANE_STATUS_INVAL;
    }

  /* walk the linked list of scanner device until ther is a match
   * with the device name */
  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      DBG (2, "sane_open: devname from list: %s\n",
	   lexmark_device->sane.name);
      if (strcmp (devicename, "") == 0
	  || strcmp (devicename, "lexmark") == 0
	  || strcmp (devicename, lexmark_device->sane.name) == 0)
	break;
    }

  *handle = lexmark_device;

  if (!lexmark_device)
    {
      DBG (2, "sane_open: Not a lexmark device\n");
      return SANE_STATUS_INVAL;
    }

  status = init_options (lexmark_device);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = sanei_lexmark_low_open_device (lexmark_device);
  DBG (2, "sane_open: end.\n");

  return status;
}

void
sane_close (SANE_Handle handle)
{
  Lexmark_Device *lexmark_device;

  DBG (2, "sane_close: handle=%p\n", (void *) handle);

  if (!initialized)
    return;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  if (!lexmark_device)
    return;

  sanei_lexmark_low_close_device (lexmark_device);

  return;
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Lexmark_Device *lexmark_device;

  DBG (2, "sane_get_option_descriptor: handle=%p, option = %d\n",
       (void *) handle, option);

  if (!initialized)
    return NULL;

  /* Check for valid option number */
  if ((option < 0) || (option >= NUM_OPTIONS))
    return NULL;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  if (!lexmark_device)
    return NULL;

  if (lexmark_device->opt[option].name)
    {
      DBG (2, "sane_get_option_descriptor: name=%s\n",
	   lexmark_device->opt[option].name);
    }

  return &(lexmark_device->opt[option]);
}

/* rebuilds parameters if needed, called each time SANE_INFO_RELOAD_OPTIONS
   is set */
static void
calc_parameters (Lexmark_Device * lexmark_device)
{
  if (strcmp (lexmark_device->val[OPT_MODE].s,
	      SANE_VALUE_SCAN_MODE_LINEART) == 0)
    {
      lexmark_device->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
    }
  else
    {
      lexmark_device->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
    }

  /* changing color mode implies changing gain setting */
  if (lexmark_device->val[OPT_MANUAL_GAIN].w == SANE_TRUE)
    {
      if (strcmp (lexmark_device->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR)
	  != 0)
	{
	  lexmark_device->opt[OPT_GRAY_GAIN].cap &= ~SANE_CAP_INACTIVE;
	  lexmark_device->opt[OPT_RED_GAIN].cap |= SANE_CAP_INACTIVE;
	  lexmark_device->opt[OPT_GREEN_GAIN].cap |= SANE_CAP_INACTIVE;
	  lexmark_device->opt[OPT_BLUE_GAIN].cap |= SANE_CAP_INACTIVE;
	}
      else
	{
	  lexmark_device->opt[OPT_GRAY_GAIN].cap |= SANE_CAP_INACTIVE;
	  lexmark_device->opt[OPT_RED_GAIN].cap &= ~SANE_CAP_INACTIVE;
	  lexmark_device->opt[OPT_GREEN_GAIN].cap &= ~SANE_CAP_INACTIVE;
	  lexmark_device->opt[OPT_BLUE_GAIN].cap &= ~SANE_CAP_INACTIVE;
	}
    }
  else
    {
      lexmark_device->opt[OPT_GRAY_GAIN].cap |= SANE_CAP_INACTIVE;
      lexmark_device->opt[OPT_RED_GAIN].cap |= SANE_CAP_INACTIVE;
      lexmark_device->opt[OPT_GREEN_GAIN].cap |= SANE_CAP_INACTIVE;
      lexmark_device->opt[OPT_BLUE_GAIN].cap |= SANE_CAP_INACTIVE;
    }
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option, SANE_Action action,
		     void *value, SANE_Int * info)
{
  Lexmark_Device *lexmark_device;
  SANE_Status status;
  SANE_Word w;

  DBG (2, "sane_control_option: handle=%p, opt=%d, act=%d, val=%p, info=%p\n",
       (void *) handle, option, action, (void *) value, (void *) info);

  if (!initialized)
    return SANE_STATUS_INVAL;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  if (!lexmark_device)
    return SANE_STATUS_INVAL;

  if (value == NULL)
    return SANE_STATUS_INVAL;

  if (info != NULL)
    *info = 0;

  if (option < 0 || option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  if (lexmark_device->opt[option].type == SANE_TYPE_GROUP)
    return SANE_STATUS_INVAL;

  switch (action)
    {
    case SANE_ACTION_SET_AUTO:

      if (!SANE_OPTION_IS_SETTABLE (lexmark_device->opt[option].cap))
	return SANE_STATUS_INVAL;
      if (!(lexmark_device->opt[option].cap & SANE_CAP_AUTOMATIC))
	return SANE_STATUS_INVAL;
      break;

    case SANE_ACTION_SET_VALUE:

      if (!SANE_OPTION_IS_SETTABLE (lexmark_device->opt[option].cap))
	return SANE_STATUS_INVAL;

      /* Make sure boolean values are only TRUE or FALSE */
      if (lexmark_device->opt[option].type == SANE_TYPE_BOOL)
	{
	  if (!
	      ((*(SANE_Bool *) value == SANE_FALSE)
	       || (*(SANE_Bool *) value == SANE_TRUE)))
	    return SANE_STATUS_INVAL;
	}

      /* Check range constraints */
      if (lexmark_device->opt[option].constraint_type ==
	  SANE_CONSTRAINT_RANGE)
	{
	  status =
	    sanei_constrain_value (&(lexmark_device->opt[option]), value,
				   info);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (2, "SANE_CONTROL_OPTION: Bad value for range\n");
	      return SANE_STATUS_INVAL;
	    }
	}

      switch (option)
	{
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	  lexmark_device->val[option].w = *(SANE_Int *) value;
	  sane_get_parameters (handle, 0);
	  break;
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  DBG (2, "Option value set to %d (%s)\n", *(SANE_Word *) value,
	       lexmark_device->opt[option].name);
	  lexmark_device->val[option].w = *(SANE_Word *) value;
	  if (lexmark_device->val[OPT_TL_X].w >
	      lexmark_device->val[OPT_BR_X].w)
	    {
	      w = lexmark_device->val[OPT_TL_X].w;
	      lexmark_device->val[OPT_TL_X].w =
		lexmark_device->val[OPT_BR_X].w;
	      lexmark_device->val[OPT_BR_X].w = w;
	      if (info)
		*info |= SANE_INFO_RELOAD_PARAMS;
	    }
	  if (lexmark_device->val[OPT_TL_Y].w >
	      lexmark_device->val[OPT_BR_Y].w)
	    {
	      w = lexmark_device->val[OPT_TL_Y].w;
	      lexmark_device->val[OPT_TL_Y].w =
		lexmark_device->val[OPT_BR_Y].w;
	      lexmark_device->val[OPT_BR_Y].w = w;
	      if (info)
		*info |= SANE_INFO_RELOAD_PARAMS;
	    }
	  break;
	case OPT_THRESHOLD:
	  lexmark_device->val[option].w = *(SANE_Fixed *) value;
	  lexmark_device->threshold =
	    (0xFF * lexmark_device->val[option].w) / 100;
	  break;
	case OPT_PREVIEW:
	  lexmark_device->val[option].w = *(SANE_Int *) value;
	  if (*(SANE_Word *) value)
	    {
	      lexmark_device->y_dpi = lexmark_device->val[OPT_RESOLUTION].w;
	      lexmark_device->val[OPT_RESOLUTION].w = 75;
	    }
	  else
	    {
	      lexmark_device->val[OPT_RESOLUTION].w = lexmark_device->y_dpi;
	    }
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  sane_get_parameters (handle, 0);
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  break;
	case OPT_GRAY_GAIN:
	case OPT_GREEN_GAIN:
	case OPT_RED_GAIN:
	case OPT_BLUE_GAIN:
	  lexmark_device->val[option].w = *(SANE_Word *) value;
	  return SANE_STATUS_GOOD;
	  break;
	case OPT_MODE:
	  strcpy (lexmark_device->val[option].s, value);
	  calc_parameters (lexmark_device);
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  return SANE_STATUS_GOOD;
	case OPT_MANUAL_GAIN:
	  w = *(SANE_Word *) value;

	  if (w == lexmark_device->val[OPT_MANUAL_GAIN].w)
	    return SANE_STATUS_GOOD;	/* no change */

	  lexmark_device->val[OPT_MANUAL_GAIN].w = w;
	  calc_parameters (lexmark_device);
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  return SANE_STATUS_GOOD;
	}

      if (info != NULL)
	*info |= SANE_INFO_RELOAD_PARAMS;

      break;

    case SANE_ACTION_GET_VALUE:

      switch (option)
	{
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_PREVIEW:
	case OPT_MANUAL_GAIN:
	case OPT_GRAY_GAIN:
	case OPT_GREEN_GAIN:
	case OPT_RED_GAIN:
	case OPT_BLUE_GAIN:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  *(SANE_Word *) value = lexmark_device->val[option].w;
	  DBG (2, "Option value = %d (%s)\n", *(SANE_Word *) value,
	       lexmark_device->opt[option].name);
	  break;
	case OPT_THRESHOLD:
	  *(SANE_Fixed *) value = lexmark_device->val[option].w;
	  DBG (2, "Option value = %f\n", SANE_UNFIX (*(SANE_Fixed *) value));
	  break;
	case OPT_MODE:
	  strcpy (value, lexmark_device->val[option].s);
	  break;
	default:
	  return SANE_STATUS_INVAL;
	}
      break;

    default:
      return SANE_STATUS_INVAL;

    }

  return SANE_STATUS_GOOD;
}


SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Lexmark_Device *lexmark_device;
  SANE_Parameters *device_params;
  SANE_Int xres, yres, width_px, height_px;
  SANE_Int channels, bitsperchannel;

  DBG (2, "sane_get_parameters: handle=%p, params=%p\n", (void *) handle,
       (void *) params);

  if (!initialized)
    return SANE_STATUS_INVAL;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  if (!lexmark_device)
    return SANE_STATUS_INVAL;

  yres = lexmark_device->val[OPT_RESOLUTION].w;
  if (yres == 1200)
    xres = 600;
  else
    xres = yres;

  /* 24 bit colour = 8 bits/channel for each of the RGB channels */
  channels = 3;
  bitsperchannel = 8;

  /* If not color there is only 1 channel */
  if (strcmp (lexmark_device->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR)
      != 0)
    {
      channels = 1;
      bitsperchannel = 8;
    }

  /* geometry in pixels */
  width_px =
    lexmark_device->val[OPT_BR_X].w - lexmark_device->val[OPT_TL_X].w;
  height_px =
    lexmark_device->val[OPT_BR_Y].w - lexmark_device->val[OPT_TL_Y].w;
  DBG (7, "sane_get_parameters: tl=(%d,%d) br=(%d,%d)\n",
       lexmark_device->val[OPT_TL_X].w, lexmark_device->val[OPT_TL_Y].w,
       lexmark_device->val[OPT_BR_X].w, lexmark_device->val[OPT_BR_Y].w);


  /* we must tell the front end the bitsperchannel for lineart is really */
  /* only 1, so it can calculate the correct image size */
  /* If not color there is only 1 channel */
  if (strcmp (lexmark_device->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART)
      == 0)
    {
      bitsperchannel = 1;
    }

  device_params = &(lexmark_device->params);
  device_params->format = SANE_FRAME_RGB;
  if (channels == 1)
    device_params->format = SANE_FRAME_GRAY;
  device_params->last_frame = SANE_TRUE;
  device_params->lines = (height_px * yres) / 600;
  device_params->depth = bitsperchannel;
  device_params->pixels_per_line = (width_px * xres) / 600;
  /* we always read an even number of sensor pixels */
  if (device_params->pixels_per_line & 1)
    device_params->pixels_per_line++;

  /* data_size is the size transferred from the scanner to the backend */
  /* therefore bitsperchannel is the same for gray and lineart */
  /* note: bytes_per_line has been divided by 8 in lineart mode */
  lexmark_device->data_size =
    channels * device_params->pixels_per_line * device_params->lines;

  if (bitsperchannel == 1)
    {
      device_params->bytes_per_line =
	(SANE_Int) ((7 + device_params->pixels_per_line) / 8);
    }
  else
    {
      device_params->bytes_per_line =
	(SANE_Int) (channels * device_params->pixels_per_line);
    }
  DBG (2, "sane_get_parameters: Data size determined as %ld\n",
       lexmark_device->data_size);

  DBG (2, "sane_get_parameters: \n");
  if (device_params->format == SANE_FRAME_GRAY)
    DBG (2, "  format: SANE_FRAME_GRAY\n");
  else if (device_params->format == SANE_FRAME_RGB)
    DBG (2, "  format: SANE_FRAME_RGB\n");
  else
    DBG (2, "  format: UNKNOWN\n");
  if (device_params->last_frame == SANE_TRUE)
    DBG (2, "  last_frame: TRUE\n");
  else
    DBG (2, "  last_frame: FALSE\n");
  DBG (2, "  lines %d\n", device_params->lines);
  DBG (2, "  depth %d\n", device_params->depth);
  DBG (2, "  pixels_per_line %d\n", device_params->pixels_per_line);
  DBG (2, "  bytes_per_line %d\n", device_params->bytes_per_line);

  if (params != 0)
    {
      params->format = device_params->format;
      params->last_frame = device_params->last_frame;
      params->lines = device_params->lines;
      params->depth = device_params->depth;
      params->pixels_per_line = device_params->pixels_per_line;
      params->bytes_per_line = device_params->bytes_per_line;
    }

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Lexmark_Device *lexmark_device;
  SANE_Int offset;
  SANE_Status status;
  int resolution;

  DBG (2, "sane_start: handle=%p\n", (void *) handle);

  if (!initialized)
    return SANE_STATUS_INVAL;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  sane_get_parameters (handle, 0);

  if ((lexmark_device->params.lines == 0) ||
      (lexmark_device->params.pixels_per_line == 0) ||
      (lexmark_device->params.bytes_per_line == 0))
    {
      DBG (2, "sane_start: \n");
      DBG (2, "  ERROR: Zero size encountered in:\n");
      DBG (2,
	   "         number of lines, bytes per line, or pixels per line\n");
      return SANE_STATUS_INVAL;
    }

  lexmark_device->device_cancelled = SANE_FALSE;
  lexmark_device->data_ctr = 0;
  lexmark_device->eof = SANE_FALSE;


  /* Need this cancel_ctr to determine how many times sane_cancel is called 
     since it is called more than once. */
  lexmark_device->cancel_ctr = 0;

  /* Find Home */
  if (sanei_lexmark_low_search_home_fwd (lexmark_device))
    {
      DBG (2, "sane_start: Scan head initially at home position\n");
    }
  else
    {
      /* We may have been rewound too far, so move forward the distance from
         the edge to the home position */
      sanei_lexmark_low_move_fwd (0x01a8, lexmark_device,
				  lexmark_device->shadow_regs);

      /* Scan backwards until we find home */
      sanei_lexmark_low_search_home_bwd (lexmark_device);
    }
  /* do calibration before offset detection , use sensor max dpi, not motor's one */
  resolution = lexmark_device->val[OPT_RESOLUTION].w;
  if (resolution > 600)
    {
      resolution = 600;
    }


  sanei_lexmark_low_set_scan_regs (lexmark_device, resolution, 0, SANE_FALSE);
  status = sanei_lexmark_low_calibration (lexmark_device);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: calibration failed : %s ! \n",
	   sane_strstatus (status));
      return status;
    }

  /* At this point we're somewhere in the dot. We need to read a number of 
     lines greater than the diameter of the dot and determine how many lines 
     past the dot we've gone. We then use this information to see how far the 
     scan head must move before starting the scan. */
  /* offset is in 600 dpi unit */
  offset = sanei_lexmark_low_find_start_line (lexmark_device);
  DBG (7, "start line offset=%d\n", offset);

  /* Set the shadow registers for scan with the options (resolution, mode, 
     size) set in the front end. Pass the offset so we can get the vert.
     start. */
  sanei_lexmark_low_set_scan_regs (lexmark_device,
				   lexmark_device->val[OPT_RESOLUTION].w,
				   offset, SANE_TRUE);

  if (sanei_lexmark_low_start_scan (lexmark_device) == SANE_STATUS_GOOD)
    {
      DBG (2, "sane_start: scan started\n");
      return SANE_STATUS_GOOD;
    }
  else
    {
      lexmark_device->device_cancelled = SANE_TRUE;
      return SANE_STATUS_INVAL;
    }
}


SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * data,
	   SANE_Int max_length, SANE_Int * length)
{
  Lexmark_Device *lexmark_device;
  long bytes_read;

  DBG (2, "sane_read: handle=%p, data=%p, max_length = %d, length=%p\n",
       (void *) handle, (void *) data, max_length, (void *) length);

  if (!initialized)
    {
      DBG (2, "sane_read: Not initialized\n");
      return SANE_STATUS_INVAL;
    }

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  if (lexmark_device->device_cancelled)
    {
      DBG (2, "sane_read: Device was cancelled\n");
      /* We don't know how far we've gone, so search for home. */
      sanei_lexmark_low_search_home_bwd (lexmark_device);
      return SANE_STATUS_EOF;
    }

  if (!length)
    {
      DBG (2, "sane_read: NULL length pointer\n");
      return SANE_STATUS_INVAL;
    }

  *length = 0;

  if (lexmark_device->eof)
    {
      DBG (2, "sane_read: Trying to read past EOF\n");
      return SANE_STATUS_EOF;
    }

  if (!data)
    return SANE_STATUS_INVAL;

  bytes_read = sanei_lexmark_low_read_scan_data (data, max_length,
						 lexmark_device);
  if (bytes_read < 0)
    return SANE_STATUS_IO_ERROR;
  else if (bytes_read == 0)
    return SANE_STATUS_EOF;
  else
    {
      *length = bytes_read;
      lexmark_device->data_ctr += bytes_read;
    }

  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Lexmark_Device *lexmark_device;
/*   ssize_t bytes_read; */
  DBG (2, "sane_cancel: handle = %p\n", (void *) handle);

  if (!initialized)
    return;


  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  /*If sane_cancel called more than once, return */
  if (++lexmark_device->cancel_ctr > 1)
    return;

  /* Set the device flag so the next call to sane_read() can stop the scan. */
  lexmark_device->device_cancelled = SANE_TRUE;

  return;
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Lexmark_Device *lexmark_device;

  DBG (2, "sane_set_io_mode: handle = %p, non_blocking = %d\n",
       (void *) handle, non_blocking);

  if (!initialized)
    return SANE_STATUS_INVAL;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  if (non_blocking)
    return SANE_STATUS_UNSUPPORTED;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  Lexmark_Device *lexmark_device;

  DBG (2, "sane_get_select_fd: handle = %p, fd %s 0\n", (void *) handle,
       fd ? "!=" : "=");

  if (!initialized)
    return SANE_STATUS_INVAL;

  for (lexmark_device = first_lexmark_device; lexmark_device;
       lexmark_device = lexmark_device->next)
    {
      if (lexmark_device == handle)
	break;
    }

  return SANE_STATUS_UNSUPPORTED;
}

/***************************** END OF SANE API ****************************/
/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
