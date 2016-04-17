/* sane - Scanner Access Now Easy.
   (C) 2003 Henning Meier-Geinitz <henning@meier-geinitz.de>.

   Based on the mustek (SCSI) backend.

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

   This file implements a SANE backend for scanners based on the Mustek
   MA-1509 chipset. Currently the Mustek BearPaw 1200F is known to work.
*/


/**************************************************************************/
/* ma1509 backend version                                                 */
#define BUILD 3
/**************************************************************************/

#include "../include/sane/config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_usb.h"

#define BACKEND_NAME	ma1509
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"

#include "ma1509.h"

#ifndef SANE_I18N
#define SANE_I18N(text) text
#endif

/* Debug level from sanei_init_debug */
static SANE_Int debug_level;

static SANE_Int num_devices;
static Ma1509_Device *first_dev;
static Ma1509_Scanner *first_handle;
static const SANE_Device **devlist = 0;

static int warmup_time = MA1509_WARMUP_TIME;

/* Array of newly attached devices */
static Ma1509_Device **new_dev;

/* Length of new_dev array */
static SANE_Int new_dev_len;

/* Number of entries alloced for new_dev */
static SANE_Int new_dev_alloced;

static SANE_String_Const mode_list[] = {
  SANE_VALUE_SCAN_MODE_LINEART,
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_COLOR,
  0
};

static SANE_String_Const ta_source_list[] = {
  SANE_I18N ("Flatbed"), SANE_I18N ("Transparency Adapter"),
  0
};

static SANE_Word resolution_list[] = {
  9,
  50, 100, 150, 200, 300, 400, 450, 500, 600
};

static const SANE_Range u8_range = {
  0,				/* minimum */
  255,				/* maximum */
  0				/* quantization */
};


/* "SCSI" command buffers used by the backend */
static const SANE_Byte scsi_inquiry[] = {
  0x12, 0x01, 0x00, 0x00, 0x00, 0x00, INQ_LEN, 0x00
};
static const SANE_Byte scsi_test_unit_ready[] = {
  0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00
};
static const SANE_Byte scsi_set_window[] = {
  0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00
};


static void
print_data_buffer (const SANE_Byte * buffer, size_t len)
{
  SANE_Byte buffer_byte_list[50];
  SANE_Byte buffer_byte[5];
  const SANE_Byte *pp;

  buffer_byte_list[0] = '\0';
  for (pp = buffer; pp < (buffer + len); pp++)
    {
      sprintf ((SANE_String) buffer_byte, " %02x", *pp);
      strcat ((SANE_String) buffer_byte_list, (SANE_String) buffer_byte);
      if (((pp - buffer) % 0x10 == 0x0f) || (pp >= (buffer + len - 1)))
	{
	  DBG (5, "buffer: %s\n", buffer_byte_list);
	  buffer_byte_list[0] = '\0';
	}
    }
}

static SANE_Status
ma1509_cmd (Ma1509_Scanner * s, const SANE_Byte * cmd, SANE_Byte * data,
	    size_t * data_size)
{
  SANE_Status status;
  size_t size;
#define MA1509_WRITE_LIMIT (1024 * 64)
#define MA1509_READ_LIMIT (1024 * 256)

  DBG (5, "ma1509_cmd: fd=%d, cmd=%p, data=%p, data_size=%ld\n",
       s->fd, cmd, data, (long int) (data_size ? *data_size : 0));
  DBG (5, "ma1509_cmd: cmd = %02x %02x %02x %02x %02x %02x %02x %02x \n",
       cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7]);


  size = MA1509_COMMAND_LENGTH;
  status = sanei_usb_write_bulk (s->fd, cmd, &size);
  if (status != SANE_STATUS_GOOD || size != MA1509_COMMAND_LENGTH)
    {
      DBG (5,
	   "ma1509_cmd: sanei_usb_write_bulk returned %s (size = %ld, expected %d)\n",
	   sane_strstatus (status), (long int) size, MA1509_COMMAND_LENGTH);
      return status;
    }

  if (cmd[1] == 1)
    {
      /* receive data */
      if (data && data_size && *data_size)
	{
	  size_t bytes_left = *data_size;
	  DBG (5, "ma1509_cmd: trying to receive %ld bytes of data\n",
	       (long int) *data_size);

	  while (status == SANE_STATUS_GOOD && bytes_left > 0)
	    {
	      size = bytes_left;
	      if (size > MA1509_READ_LIMIT)
		size = MA1509_READ_LIMIT;

	      status =
		sanei_usb_read_bulk (s->fd, data + *data_size - bytes_left,
				     &size);

	      if (status != SANE_STATUS_GOOD)
		{
		  DBG (1, "ma1509_cmd: sanei_usb_read_bulk returned %s\n",
		       sane_strstatus (status));
		  return status;
		}
	      bytes_left -= size;
	      DBG (5, "ma1509_cmd: read %ld bytes, %ld bytes to go\n",
		   (long int) size, (long int) bytes_left);
	    }
	  if (debug_level >= 5)
	    print_data_buffer (data, *data_size);
	}
    }
  else
    {
      /* send data */
      if (data && data_size && *data_size)
	{
	  size_t bytes_left = *data_size;

	  DBG (5, "ma1509_cmd: sending %ld bytes of data\n",
	       (long int) *data_size);
	  if (debug_level >= 5)
	    print_data_buffer (data, *data_size);

	  while (status == SANE_STATUS_GOOD && bytes_left > 0)
	    {
	      size = bytes_left;
	      if (size > MA1509_WRITE_LIMIT)
		size = MA1509_WRITE_LIMIT;
	      status =
		sanei_usb_write_bulk (s->fd, data + *data_size - bytes_left,
				      &size);
	      if (status != SANE_STATUS_GOOD)
		{
		  DBG (1, "ma1509_cmd: sanei_usb_write_bulk returned %s\n",
		       sane_strstatus (status));
		  return status;
		}
	      bytes_left -= size;
	      DBG (5, "ma1509_cmd: wrote %ld bytes, %ld bytes to go\n",
		   (long int) size, (long int) bytes_left);
	    }

	}
    }

  DBG (5, "ma1509_cmd: finished: data_size=%ld, status=%s\n",
       (long int) (data_size ? *data_size : 0), sane_strstatus (status));
  return status;
}

static SANE_Status
test_unit_ready (Ma1509_Scanner * s)
{
  SANE_Status status;
  SANE_Byte buffer[0x04];
  size_t size = sizeof (buffer);

  status = ma1509_cmd (s, scsi_test_unit_ready, buffer, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "test_unit_ready: ma1509_cmd failed: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (buffer[1] == 0x14)
    s->hw->has_adf = SANE_TRUE;
  else
    s->hw->has_adf = SANE_FALSE;

  return status;
}

static SANE_Status
attach (SANE_String_Const devname, Ma1509_Device ** devp)
{
  SANE_Int fw_revision;
  SANE_Byte result[INQ_LEN];
  SANE_Byte inquiry_byte_list[50], inquiry_text_list[17];
  SANE_Byte inquiry_byte[5], inquiry_text[5];
  SANE_Byte *model_name = result + 44;
  Ma1509_Scanner s;
  Ma1509_Device *dev, new_dev;
  SANE_Status status;
  size_t size;
  SANE_Byte *pp;
  SANE_Word vendor, product;

  if (devp)
    *devp = 0;

  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0)
      {
	if (devp)
	  *devp = dev;
	return SANE_STATUS_GOOD;
      }

  memset (&new_dev, 0, sizeof (new_dev));
  memset (&s, 0, sizeof (s));
  s.hw = &new_dev;

  DBG (3, "attach: trying device %s\n", devname);

  status = sanei_usb_open (devname, &s.fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: sanei_usb_open failed: %s\n", sane_strstatus (status));
      return status;
    }

  status = sanei_usb_get_vendor_product (s.fd, &vendor, &product);
  if (status != SANE_STATUS_GOOD && status != SANE_STATUS_UNSUPPORTED)
    {
      DBG (1, "attach: sanei_usb_get_vendor_product failed: %s\n",
	   sane_strstatus (status));
      sanei_usb_close (s.fd);
      return status;
    }
  if (status == SANE_STATUS_UNSUPPORTED)
    {
      DBG (3, "attach: can't detect vendor/product, trying anyway\n");
    }
  else if (vendor != 0x055f || product != 0x0010)
    {
      DBG (1, "attach: unknown vendor/product (0x%x/0x%x)\n", vendor,
	   product);
      sanei_usb_close (s.fd);
      return SANE_STATUS_UNSUPPORTED;
    }

  DBG (4, "attach: sending TEST_UNIT_READY\n");
  status = test_unit_ready (&s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: test_unit_ready device %s failed (%s)\n", devname,
	   sane_strstatus (status));
      sanei_usb_close (s.fd);
      return status;
    }

  DBG (4, "attach: sending INQUIRY\n");
  size = sizeof (result);
  memset (result, 0, sizeof (result));
  status = ma1509_cmd (&s, scsi_inquiry, result, &size);
  if (status != SANE_STATUS_GOOD || size != INQ_LEN)
    {
      DBG (1, "attach: inquiry for device %s failed (%s)\n", devname,
	   sane_strstatus (status));
      sanei_usb_close (s.fd);
      return status;
    }

  sanei_usb_close (s.fd);

  if ((result[0] & 0x1f) != 0x06)
    {
      DBG (1, "attach: device %s doesn't look like a scanner at all (%d)\n",
	   devname, result[0] & 0x1f);
      return SANE_STATUS_INVAL;
    }

  if (debug_level >= 5)
    {
      /* print out inquiry */
      DBG (5, "attach: inquiry output:\n");
      inquiry_byte_list[0] = '\0';
      inquiry_text_list[0] = '\0';
      for (pp = result; pp < (result + INQ_LEN); pp++)
	{
	  sprintf ((SANE_String) inquiry_text, "%c",
		   (*pp < 127) && (*pp > 31) ? *pp : '.');
	  strcat ((SANE_String) inquiry_text_list,
		  (SANE_String) inquiry_text);
	  sprintf ((SANE_String) inquiry_byte, " %02x", *pp);
	  strcat ((SANE_String) inquiry_byte_list,
		  (SANE_String) inquiry_byte);
	  if ((pp - result) % 0x10 == 0x0f)
	    {
	      DBG (5, "%s  %s\n", inquiry_byte_list, inquiry_text_list);
	      inquiry_byte_list[0] = '\0';
	      inquiry_text_list[0] = '\0';
	    }
	}
    }

  /* get firmware revision as BCD number:             */
  fw_revision = (result[32] - '0') << 8 | (result[34] - '0') << 4
    | (result[35] - '0');
  DBG (4, "attach: firmware revision %d.%02x\n", fw_revision >> 8,
       fw_revision & 0xff);

  dev = malloc (sizeof (*dev));
  if (!dev)
    return SANE_STATUS_NO_MEM;

  memcpy (dev, &new_dev, sizeof (*dev));

  dev->name = strdup (devname);
  if (!dev->name)
    return SANE_STATUS_NO_MEM;
  dev->sane.name = (SANE_String_Const) dev->name;
  dev->sane.vendor = "Mustek";
  dev->sane.type = "flatbed scanner";

  dev->x_range.min = 0;
  dev->y_range.min = 0;
  dev->x_range.quant = SANE_FIX (0.1);
  dev->y_range.quant = SANE_FIX (0.1);
  dev->x_trans_range.min = 0;
  dev->y_trans_range.min = 0;
  /* default to something really small to be on the safe side: */
  dev->x_trans_range.max = SANE_FIX (8.0 * MM_PER_INCH);
  dev->y_trans_range.max = SANE_FIX (5.0 * MM_PER_INCH);
  dev->x_trans_range.quant = SANE_FIX (0.1);
  dev->y_trans_range.quant = SANE_FIX (0.1);

  DBG (3, "attach: scanner id: %.11s\n", model_name);

  /* BearPaw 1200F (SCSI-over-USB) */
  if (strncmp ((SANE_String) model_name, " B06", 4) == 0)
    {
      dev->x_range.max = SANE_FIX (211.3);
      dev->y_range.min = SANE_FIX (0);
      dev->y_range.max = SANE_FIX (296.7);

      dev->x_trans_range.min = SANE_FIX (0);
      dev->y_trans_range.min = SANE_FIX (0);
      dev->x_trans_range.max = SANE_FIX (150.0);
      dev->y_trans_range.max = SANE_FIX (175.0);

      dev->sane.model = "BearPaw 1200F";
    }
  else
    {
      DBG (0, "attach: this scanner (ID: %s) is not supported yet\n",
	   model_name);
      DBG (0, "attach: please set the debug level to 5 and send a debug "
	   "report\n");
      DBG (0, "attach: to henning@meier-geinitz.de (export "
	   "SANE_DEBUG_MA1509=5\n");
      DBG (0, "attach: scanimage -L 2>debug.txt). Thank you.\n");
      free (dev);
      return SANE_STATUS_INVAL;
    }

  DBG (2, "attach: found Mustek %s %s %s%s\n",
       dev->sane.model, dev->sane.type, dev->has_ta ? "(TA)" : "",
       dev->has_adf ? "(ADF)" : "");

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;
  return SANE_STATUS_GOOD;
}


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


static SANE_Status
init_options (Ma1509_Scanner * s)
{
  SANE_Int i;

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  s->opt[OPT_NUM_OPTS].name = "";
  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Mode" group: */
  s->opt[OPT_MODE_GROUP].title = SANE_I18N ("Scan Mode");
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].size = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE].s = strdup (mode_list[1]);
  if (!s->val[OPT_MODE].s)
    return SANE_STATUS_NO_MEM;

  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_RESOLUTION].constraint.word_list = resolution_list;
  s->val[OPT_RESOLUTION].w = 50;

  /* source */
  s->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  s->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  s->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  s->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
  s->opt[OPT_SOURCE].size = max_string_size (ta_source_list);
  s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SOURCE].constraint.string_list = ta_source_list;
  s->val[OPT_SOURCE].s = strdup (ta_source_list[0]);
  if (!s->val[OPT_SOURCE].s)
    return SANE_STATUS_NO_MEM;
  s->opt[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;

  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  s->val[OPT_PREVIEW].w = 0;

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].title = SANE_I18N ("Geometry");
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].size = 0;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &s->hw->x_range;
  s->val[OPT_TL_X].w = s->hw->x_range.min;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_TL_Y].w = s->hw->y_range.min;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;
  s->val[OPT_BR_X].w = s->hw->x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_BR_Y].w = s->hw->y_range.max;

  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = SANE_I18N ("Enhancement");
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].size = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &u8_range;
  s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_THRESHOLD].w = 128;

  /* custom-gamma table */
  s->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  s->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;

  /* red gamma vector */
  s->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_R].size = MA1509_GAMMA_SIZE * sizeof (SANE_Word);
  s->val[OPT_GAMMA_VECTOR_R].wa = &s->red_gamma_table[0];
  s->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
  s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
  for (i = 0; i < MA1509_GAMMA_SIZE; i++)
    s->red_gamma_table[i] = i * MA1509_GAMMA_SIZE / 256;

  /* green gamma vector */
  s->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_G].size = MA1509_GAMMA_SIZE * sizeof (SANE_Word);
  s->val[OPT_GAMMA_VECTOR_G].wa = &s->green_gamma_table[0];
  s->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
  s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
  for (i = 0; i < MA1509_GAMMA_SIZE; i++)
    s->green_gamma_table[i] = i * MA1509_GAMMA_SIZE / 256;

  /* blue gamma vector */
  s->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_B].size = MA1509_GAMMA_SIZE * sizeof (SANE_Word);
  s->val[OPT_GAMMA_VECTOR_B].wa = &s->blue_gamma_table[0];
  s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
  s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
  for (i = 0; i < MA1509_GAMMA_SIZE; i++)
    s->blue_gamma_table[i] = i * MA1509_GAMMA_SIZE / 256;

  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one_device (SANE_String_Const devname)
{
  Ma1509_Device *dev;

  attach (devname, &dev);
  if (dev)
    {
      /* Keep track of newly attached devices so we can set options as
         necessary.  */
      if (new_dev_len >= new_dev_alloced)
	{
	  new_dev_alloced += 4;
	  if (new_dev)
	    new_dev =
	      realloc (new_dev, new_dev_alloced * sizeof (new_dev[0]));
	  else
	    new_dev = malloc (new_dev_alloced * sizeof (new_dev[0]));
	  if (!new_dev)
	    {
	      DBG (1, "attach_one_device: out of memory\n");
	      return SANE_STATUS_NO_MEM;
	    }
	}
      new_dev[new_dev_len++] = dev;
    }
  return SANE_STATUS_GOOD;
}

static SANE_Status
set_window (Ma1509_Scanner * s)
{
  SANE_Byte buffer[0x30], *cp;
  double pixels_per_mm;
  size_t size = sizeof (buffer);
  SANE_Status status;
  SANE_Int tlx, tly, width, height;
  SANE_Int offset = 0;
  struct timeval now;
  long remaining_time;

  /* check if lamp is warmed up */
  gettimeofday (&now, 0);
  remaining_time = warmup_time - (now.tv_sec - s->lamp_time);
  if (remaining_time > 0)
    {
      DBG (0, "Warm-up in progress: please wait %2ld seconds\n",
	   remaining_time);
      sleep (remaining_time);
    }

  memset (buffer, 0, size);
  cp = buffer;

  STORE16B (cp, 0);		/* window identifier            */
  STORE16B (cp, s->val[OPT_RESOLUTION].w);
  STORE16B (cp, 0);		/* not used acc. to specs       */

  pixels_per_mm = s->val[OPT_RESOLUTION].w / MM_PER_INCH;

  tlx = SANE_UNFIX (s->val[OPT_TL_X].w) * pixels_per_mm + 0.5;
  tly = SANE_UNFIX (s->val[OPT_TL_Y].w) * pixels_per_mm + 0.5;

  width = (SANE_UNFIX (s->val[OPT_BR_X].w) - SANE_UNFIX (s->val[OPT_TL_X].w))
    * pixels_per_mm + 0.5;
  height = (SANE_UNFIX (s->val[OPT_BR_Y].w) - SANE_UNFIX (s->val[OPT_TL_Y].w))
    * pixels_per_mm + 0.5 + offset;

  if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART) == 0)
    {
      width /= 64;
      width *= 64;
      if (!width)
	width = 64;
    }
  else
    {
      width /= 8;
      width *= 8;
      if (!width)
	width = 8;
    }


  DBG (4, "set_window: tlx=%d (%d mm); tly=%d (%d mm); width=%d (%d mm); "
       "height=%d (%d mm)\n", tlx, (int) (tlx / pixels_per_mm), tly,
       (int) (tly / pixels_per_mm), width, (int) (width / pixels_per_mm),
       height, (int) (height / pixels_per_mm));


  STORE16B (cp, 0);
  STORE16B (cp, tlx);
  STORE16B (cp, 0);
  STORE16B (cp, tly);
  *cp++ = 0x14;
  *cp++ = 0xc0;
  STORE16B (cp, width);
  *cp++ = 0x28;
  *cp++ = 0x20;
  STORE16B (cp, height);

  s->hw->ppl = width;
  s->hw->bpl = s->hw->ppl;

  s->hw->lines = height;

  *cp++ = 0x00;			/* brightness, not impl.        */
  /* threshold */
  if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART) == 0)
    *cp++ = (SANE_Byte) s->val[OPT_THRESHOLD].w;
  else
    *cp++ = 0x80;
  *cp++ = 0x00;			/* contrast, not impl.          */
  *cp++ = 0x00;			/* ???               .          */

  /* Note that 'image composition' has no meaning for the SE series     */
  /* Mode selection is accomplished solely by bits/pixel (1, 8, 24)     */
  if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR) == 0)
    {
      *cp++ = 24;		/* 24 bits/pixel in color mode  */
      s->hw->bpl *= 3;
    }
  else if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_GRAY) == 0)
    *cp++ = 8;			/* 8 bits/pixel in gray mode    */
  else
    {
      *cp++ = 1;		/* 1 bit/pixel in lineart mode  */
      s->hw->bpl /= 8;
    }

  cp += 13;			/* skip reserved bytes          */
  *cp++ = 0x00;			/* lamp mode  */
  if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART) != 0)
    *cp++ = 0x02;		/* ???  */

  status = ma1509_cmd (s, scsi_set_window, buffer, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "set_window: ma1509_cmd failed: %s\n", sane_strstatus (status));
      return status;
    }
  return status;
}

static SANE_Status
calibration (Ma1509_Scanner * s)
{
  SANE_Byte cmd[0x08], *buffer, *calibration_buffer;
  SANE_Status status;
  SANE_Int ppl = 5312;
  SANE_Int lines = 40;
  size_t total_size = lines * ppl;
  SANE_Int color, column, line;

  buffer = malloc (total_size * 3);
  if (!buffer)
    {
      DBG (1,
	   "calibration: couldn't malloc %lu bytes for calibration buffer\n",
	   (u_long) (total_size * 3));
      return SANE_STATUS_NO_MEM;
    }
  memset (buffer, 0x00, total_size);

  memset (cmd, 0, 8);
  cmd[0] = 0x28;		/* read data */
  cmd[1] = 0x01;		/* read */
  cmd[2] = 0x01;		/* calibration */
  cmd[4] = (total_size >> 16) & 0xff;
  cmd[5] = (total_size >> 8) & 0xff;
  cmd[6] = total_size & 0xff;
  total_size *= 3;
  status = ma1509_cmd (s, cmd, buffer, &total_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "calibration: ma1509_cmd read data failed: %s\n",
	   sane_strstatus (status));
      free (buffer);
      return status;
    }

  calibration_buffer = malloc (ppl);
  if (!calibration_buffer)
    {
      DBG (1,
	   "calibration: couldn't malloc %d bytes for calibration buffer\n",
	   ppl);
      return SANE_STATUS_NO_MEM;
    }
  memset (calibration_buffer, 0x00, ppl);

  memset (cmd, 0, 8);
  cmd[0] = 0x2a;		/* send data */
  cmd[1] = 0x00;		/* write */
  cmd[2] = 0x01;		/* calibration */
  cmd[5] = (ppl >> 8) & 0xff;
  cmd[6] = ppl & 0xff;

  for (color = 1; color < 4; color++)
    {
      cmd[4] = color;

      for (column = 0; column < ppl; column++)
	{
	  SANE_Int average = 0;

	  for (line = 0; line < lines; line++)
	    average += buffer[line * ppl * 3 + column * 3 + (color - 1)];
	  average /= lines;
	  if (average < 1)
	    average = 1;
	  if (average > 255)
	    average = 255;

	  average = (256 * 256) / average - 256;
	  if (average < 0)
	    average = 0;
	  if (average > 255)
	    average = 255;
	  calibration_buffer[column] = average;
	}

      total_size = ppl;
      status = ma1509_cmd (s, cmd, calibration_buffer, &total_size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "calibration: ma1509_cmd send data failed: %s\n",
	       sane_strstatus (status));
	  free (buffer);
	  free (calibration_buffer);
	  return status;
	}
    }
  free (buffer);
  free (calibration_buffer);
  DBG (4, "calibration: done\n");
  return status;
}


static SANE_Status
send_gamma (Ma1509_Scanner * s)
{
  SANE_Byte cmd[0x08], *buffer;
  SANE_Status status;
  size_t total_size = MA1509_GAMMA_SIZE;
  SANE_Int color;

  buffer = malloc (total_size);
  if (!buffer)
    {
      DBG (1, "send_gamma: couldn't malloc %lu bytes for gamma  buffer\n",
	   (u_long) total_size);
      return SANE_STATUS_NO_MEM;
    }

  memset (cmd, 0, 8);
  cmd[0] = 0x2a;		/* send data */
  cmd[1] = 0x00;		/* write */
  cmd[2] = 0x03;		/* gamma */
  cmd[5] = (total_size >> 8) & 0xff;
  cmd[6] = total_size & 0xff;
  for (color = 1; color < 4; color++)
    {
      unsigned int i;

      if (s->val[OPT_CUSTOM_GAMMA].w)
	{
	  SANE_Int *int_buffer;

	  if (color == 1)
	    int_buffer = s->red_gamma_table;
	  else if (color == 2)
	    int_buffer = s->green_gamma_table;
	  else
	    int_buffer = s->blue_gamma_table;
	  for (i = 0; i < total_size; i++)
	    buffer[i] = int_buffer[i];
	}
      else
	{
	  /* linear tables */
	  for (i = 0; i < total_size; i++)
	    buffer[i] = i * 256 / total_size;
	}

      cmd[4] = color;
      status = ma1509_cmd (s, cmd, buffer, &total_size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "send_gamma: ma1509_cmd send data failed: %s\n",
	       sane_strstatus (status));
	  free (buffer);
	  return status;
	}
    }
  if (!s->val[OPT_CUSTOM_GAMMA].w)
    free (buffer);
  DBG (4, "send_gamma: done\n");
  return status;
}


static SANE_Status
start_scan (Ma1509_Scanner * s)
{
  SANE_Byte cmd[8];
  SANE_Status status;

  DBG (4, "start_scan\n");
  memset (cmd, 0, 8);

  cmd[0] = 0x1b;
  cmd[1] = 0x01;
  cmd[2] = 0x01;

  status = ma1509_cmd (s, cmd, NULL, NULL);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "start_scan: ma1509_cmd failed: %s\n", sane_strstatus (status));
      return status;
    }
  return status;
}

static SANE_Status
turn_lamp (Ma1509_Scanner * s, SANE_Bool is_on)
{
  SANE_Status status;
  SANE_Byte buffer[0x30];
  size_t size = sizeof (buffer);
  struct timeval lamp_time;

  DBG (4, "turn_lamp %s\n", is_on ? "on" : "off");
  memset (buffer, 0, size);
  if (is_on)
    buffer[0x28] = 0x01;
  else
    buffer[0x28] = 0x02;

  status = ma1509_cmd (s, scsi_set_window, buffer, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "turn_lamp: ma1509_cmd set_window failed: %s\n",
	   sane_strstatus (status));
      return status;
    }
  gettimeofday (&lamp_time, 0);
  s->lamp_time = lamp_time.tv_sec;
  return status;
}

static SANE_Status
stop_scan (Ma1509_Scanner * s)
{
  SANE_Byte cmd[8];
  SANE_Status status;

  DBG (4, "stop_scan\n");
  memset (cmd, 0, 8);

  cmd[0] = 0x1b;
  cmd[1] = 0x01;
  cmd[2] = 0x00;

  status = ma1509_cmd (s, cmd, NULL, NULL);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "stop_scan: ma1509_cmd failed: %s\n", sane_strstatus (status));
      return status;
    }

  DBG (4, "stop_scan: scan stopped\n");
  return status;
}


static SANE_Status
start_read_data (Ma1509_Scanner * s)
{
  SANE_Byte cmd[8];
  SANE_Status status;
  SANE_Int total_size = s->hw->ppl * s->hw->lines;

  if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART) == 0)
    total_size /= 8;

  memset (cmd, 0, 8);

  cmd[0] = 0x28;		/* read data */
  cmd[1] = 0x01;		/* read */
  cmd[2] = 0x00;		/* scan data */
  cmd[3] = (total_size >> 24) & 0xff;
  cmd[4] = (total_size >> 16) & 0xff;
  cmd[5] = (total_size >> 8) & 0xff;
  cmd[6] = total_size & 0xff;
  status = ma1509_cmd (s, cmd, NULL, NULL);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "stop_scan: ma1509_cmd failed: %s\n", sane_strstatus (status));
      return status;
    }
  return status;
}

static SANE_Status
read_data (Ma1509_Scanner * s, SANE_Byte * buffer, SANE_Int * size)
{
  size_t local_size = *size;
  SANE_Status status;

  status = sanei_usb_read_bulk (s->fd, buffer, &local_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "read_data: sanei_usb_read_bulk failed: %s\n",
	   sane_strstatus (status));
      return status;
    }
  *size = local_size;
  return status;
}




/**************************************************************************/
/*                            SANE API calls                              */
/**************************************************************************/

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  SANE_Char line[PATH_MAX], *word, *end;
  SANE_String_Const cp;
  SANE_Int linenumber;
  FILE *fp;

  DBG_INIT ();

#ifdef DBG_LEVEL
  debug_level = DBG_LEVEL;
#else
  debug_level = 0;
#endif

  DBG (2, "SANE ma1509 backend version %d.%d build %d from %s\n", SANE_CURRENT_MAJOR,
       V_MINOR, BUILD, PACKAGE_STRING);

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);

  DBG (4, "sane_init: authorize %s null\n", authorize ? "!=" : "==");

  sanei_usb_init ();

  num_devices = 0;
  first_dev = 0;
  first_handle = 0;
  devlist = 0;
  new_dev = 0;
  new_dev_len = 0;
  new_dev_alloced = 0;

  fp = sanei_config_open (MA1509_CONFIG_FILE);
  if (!fp)
    {
      /* default to /dev/usb/scanner0 instead of insisting on config file */
      DBG (3, "sane_init: couldn't find config file (%s), trying "
	   "/dev/usb/scanner0 directly\n", MA1509_CONFIG_FILE);
      attach ("/dev/usb/scanner0", 0);
      return SANE_STATUS_GOOD;
    }
  linenumber = 0;
  DBG (4, "sane_init: reading config file `%s'\n", MA1509_CONFIG_FILE);
  while (sanei_config_read (line, sizeof (line), fp))
    {
      word = 0;
      linenumber++;

      cp = sanei_config_get_string (line, &word);
      if (!word || cp == line)
	{
	  DBG (5, "sane_init: config file line %d: ignoring empty line\n",
	       linenumber);
	  if (word)
	    free (word);
	  continue;
	}
      if (word[0] == '#')
	{
	  DBG (5, "sane_init: config file line %d: ignoring comment line\n",
	       linenumber);
	  free (word);
	  continue;
	}
      if (strcmp (word, "option") == 0)
	{
	  free (word);
	  word = 0;
	  cp = sanei_config_get_string (cp, &word);

	  if (!word)
	    {
	      DBG (1, "sane_init: config file line %d: missing quotation mark?\n",
		   linenumber);
	      continue;
	    }

	  if (strcmp (word, "warmup-time") == 0)
	    {
	      long local_warmup_time;

	      free (word);
	      word = 0;
	      cp = sanei_config_get_string (cp, &word);

	      if (!word)
		{
		  DBG (1, "sane_init: config file line %d: missing quotation mark?\n",
		       linenumber);
		  continue;
		}

	      errno = 0;
	      local_warmup_time = strtol (word, &end, 0);

	      if (end == word)
		{
		  DBG (3, "sane-init: config file line %d: warmup-time must "
		       "have a parameter; using default (%d)\n",
		       linenumber, warmup_time);
		}
	      else if (errno)
		{
		  DBG (3, "sane-init: config file line %d: warmup-time `%s' "
		       "is invalid (%s); using default (%d)\n", linenumber,
		       word, strerror (errno), warmup_time);
		}
	      else
		{
		  warmup_time = local_warmup_time;
		  DBG (4,
		       "sane_init: config file line %d: warmup-time set "
		       "to %d seconds\n", linenumber, warmup_time);

		}
	      if (word)
		free (word);
	      word = 0;
	    }
	  else
	    {
	      DBG (3, "sane_init: config file line %d: ignoring unknown "
		   "option `%s'\n", linenumber, word);
	      if (word)
		free (word);
	      word = 0;
	    }
	}
      else
	{
	  new_dev_len = 0;
	  DBG (4, "sane_init: config file line %d: trying to attach `%s'\n",
	       linenumber, line);
	  sanei_usb_attach_matching_devices (line, attach_one_device);
	  if (word)
	    free (word);
	  word = 0;
	}
    }

  if (new_dev_alloced > 0)
    {
      new_dev_len = new_dev_alloced = 0;
      free (new_dev);
    }
  fclose (fp);
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Ma1509_Device *dev, *next;

  DBG (4, "sane_exit\n");
  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free (dev->name);
      free (dev);
    }
  if (devlist)
    free (devlist);
  devlist = 0;
  first_dev = 0;
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  Ma1509_Device *dev;
  SANE_Int i;

  DBG (4, "sane_get_devices: %d devices %s\n", num_devices,
       local_only ? "(local only)" : "");
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
  DBG (5, "sane_get_devices: end\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Ma1509_Device *dev;
  SANE_Status status;
  Ma1509_Scanner *s;

  if (!devicename)
    {
      DBG (1, "sane_open: devicename is null!\n");
      return SANE_STATUS_INVAL;
    }
  if (!handle)
    {
      DBG (1, "sane_open: handle is null!\n");
      return SANE_STATUS_INVAL;
    }
  DBG (4, "sane_open: devicename=%s\n", devicename);

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
    /* empty devicname -> use first device */
    dev = first_dev;

  if (!dev)
    {
      DBG (1, "sane_open: %s doesn't seem to exist\n", devicename);
      return SANE_STATUS_INVAL;
    }

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->fd = -1;
  s->hw = dev;
  init_options (s);

  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;

  status = sanei_usb_open (s->hw->sane.name, &s->fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_open: couldn't open %s: %s\n", s->hw->sane.name,
	   sane_strstatus (status));
      return status;
    }

  status = turn_lamp (s, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_open: couldn't turn on lamp: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = turn_lamp (s, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_open: couldn't turn on lamp: %s\n",
	   sane_strstatus (status));
      return status;
    }

  *handle = s;
  DBG (5, "sane_open: finished (handle=%p)\n", (void *) s);
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Ma1509_Scanner *prev, *s;
  SANE_Status status;

  DBG (4, "sane_close: handle=%p\n", handle);

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
      DBG (1, "sane_close: invalid handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }

  if (s->val[OPT_MODE].s)
    free (s->val[OPT_MODE].s);
  if (s->val[OPT_SOURCE].s)
    free (s->val[OPT_SOURCE].s);

  status = turn_lamp (s, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_close: couldn't turn off lamp: %s\n",
	   sane_strstatus (status));
      return;
    }
  sanei_usb_close (s->fd);

  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;
  free (handle);
  handle = 0;
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Ma1509_Scanner *s = handle;

  if (((unsigned) option >= NUM_OPTIONS) || (option < 0))
    {
      DBG (3, "sane_get_option_descriptor: option %d >= NUM_OPTIONS or < 0\n",
	   option);
      return 0;
    }
  if (!s)
    {
      DBG (1, "sane_get_option_descriptor: handle is null!\n");
      return 0;
    }
  if (s->opt[option].name && s->opt[option].name[0] != 0)
    DBG (4, "sane_get_option_descriptor for option %s (%sactive%s)\n",
	 s->opt[option].name,
	 s->opt[option].cap & SANE_CAP_INACTIVE ? "in" : "",
	 s->opt[option].cap & SANE_CAP_ADVANCED ? ", advanced" : "");
  else
    DBG (4, "sane_get_option_descriptor for option \"%s\" (%sactive%s)\n",
	 s->opt[option].title,
	 s->opt[option].cap & SANE_CAP_INACTIVE ? "in" : "",
	 s->opt[option].cap & SANE_CAP_ADVANCED ? ", advanced" : "");
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  Ma1509_Scanner *s = handle;
  SANE_Status status;
  SANE_Word cap;
  SANE_Word w;

  if (((unsigned) option >= NUM_OPTIONS) || (option < 0))
    {
      DBG (3, "sane_control_option: option %d < 0 or >= NUM_OPTIONS\n",
	   option);
      return SANE_STATUS_INVAL;
    }
  if (!s)
    {
      DBG (1, "sane_control_option: handle is null!\n");
      return SANE_STATUS_INVAL;
    }
  if (!val && s->opt[option].type != SANE_TYPE_BUTTON)
    {
      DBG (1, "sane_control_option: val is null!\n");
      return SANE_STATUS_INVAL;
    }

  if (s->opt[option].name && s->opt[option].name[0] != 0)
    DBG (4, "sane_control_option (%s option %s)\n",
	 action == SANE_ACTION_GET_VALUE ? "get" :
	 (action == SANE_ACTION_SET_VALUE ? "set" : "unknown action with"),
	 s->opt[option].name);
  else
    DBG (4, "sane_control_option (%s option \"%s\")\n",
	 action == SANE_ACTION_GET_VALUE ? "get" :
	 (action == SANE_ACTION_SET_VALUE ? "set" : "unknown action with"),
	 s->opt[option].title);

  if (info)
    *info = 0;

  if (s->scanning)
    {
      DBG (3, "sane_control_option: don't use while scanning (option %s)\n",
	   s->opt[option].name);
      return SANE_STATUS_DEVICE_BUSY;
    }

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      DBG (3, "sane_control_option: option %s is inactive\n",
	   s->opt[option].name);
      return SANE_STATUS_INVAL;
    }

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_PREVIEW:
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_THRESHOLD:
	case OPT_CUSTOM_GAMMA:
	case OPT_NUM_OPTS:
	  *(SANE_Word *) val = s->val[option].w;
	  return SANE_STATUS_GOOD;

	  /* word-array options: */
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (val, s->val[option].wa, s->opt[option].size);
	  return SANE_STATUS_GOOD;

	  /* string options: */
	case OPT_SOURCE:
	case OPT_MODE:
	  strcpy (val, s->val[option].s);
	  return SANE_STATUS_GOOD;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (3, "sane_control_option: option %s is not setable\n",
	       s->opt[option].name);
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (s->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (4, "sane_control_option: constrain_value error (option %s)\n",
	       s->opt[option].name);
	  return status;
	}

      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_BR_X:
	case OPT_TL_Y:
	case OPT_BR_Y:
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  /* fall through */
	case OPT_THRESHOLD:
	case OPT_PREVIEW:
	  s->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	  /* side-effect-free word-array options: */
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (s->val[option].wa, val, s->opt[option].size);
	  return SANE_STATUS_GOOD;

	case OPT_MODE:
	  {
	    SANE_Char *old_val = s->val[option].s;

	    if (old_val)
	      {
		if (strcmp (old_val, val) == 0)
		  return SANE_STATUS_GOOD;	/* no change */
		free (old_val);
	      }
	    if (info)
	      *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	    s->val[option].s = strdup (val);
	    if (!s->val[option].s)
	      return SANE_STATUS_NO_MEM;

	    s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;

	    if (strcmp (s->val[option].s, SANE_VALUE_SCAN_MODE_LINEART) == 0)
	      {
		s->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
	      }
	    else
	      {
		s->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;
		if (s->val[OPT_CUSTOM_GAMMA].w)
		  {
		    s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		  }
	      }
	    return SANE_STATUS_GOOD;
	  }

	case OPT_SOURCE:
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  if (!s->val[option].s)
	    return SANE_STATUS_NO_MEM;

	  if (strcmp (val, "Transparency Adapter") == 0)
	    {
	      s->opt[OPT_TL_X].constraint.range = &s->hw->x_trans_range;
	      s->opt[OPT_TL_Y].constraint.range = &s->hw->y_trans_range;
	      s->opt[OPT_BR_X].constraint.range = &s->hw->x_trans_range;
	      s->opt[OPT_BR_Y].constraint.range = &s->hw->y_trans_range;
	    }
	  else
	    {
	      s->opt[OPT_TL_X].constraint.range = &s->hw->x_range;
	      s->opt[OPT_TL_Y].constraint.range = &s->hw->y_range;
	      s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;
	      s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;
	    }
	  return SANE_STATUS_GOOD;

	  /* options with side-effects: */
	case OPT_CUSTOM_GAMMA:
	  w = *(SANE_Word *) val;

	  if (w == s->val[OPT_CUSTOM_GAMMA].w)
	    return SANE_STATUS_GOOD;	/* no change */

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  s->val[OPT_CUSTOM_GAMMA].w = w;

	  s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	  s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	  s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;

	  if (w && strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART) != 0)
	    {
	      s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
	    }
	  return SANE_STATUS_GOOD;
	}

    }
  DBG (4, "sane_control_option: unknown action for option %s\n",
       s->opt[option].name);
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Ma1509_Scanner *s = handle;
  SANE_String_Const mode;

  if (!s)
    {
      DBG (1, "sane_get_parameters: handle is null!\n");
      return SANE_STATUS_INVAL;
    }

  if (!s->scanning)
    {
      double width, height, dpi;

      memset (&s->params, 0, sizeof (s->params));

      width = SANE_UNFIX (s->val[OPT_BR_X].w - s->val[OPT_TL_X].w);
      height = SANE_UNFIX (s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w);
      dpi = s->val[OPT_RESOLUTION].w;

      /* make best-effort guess at what parameters will look like once
         scanning starts.  */
      if (dpi > 0.0 && width > 0.0 && height > 0.0)
	{
	  double dots_per_mm = dpi / MM_PER_INCH;

	  s->params.pixels_per_line = width * dots_per_mm;
	  s->params.lines = height * dots_per_mm;
	}
      mode = s->val[OPT_MODE].s;
      if (strcmp (mode, SANE_VALUE_SCAN_MODE_LINEART) == 0)
	{
	  s->params.format = SANE_FRAME_GRAY;
	  s->params.bytes_per_line = (s->params.pixels_per_line + 7) / 8;
	  s->params.depth = 1;
	}
      else if (strcmp (mode, SANE_VALUE_SCAN_MODE_GRAY) == 0)
	{
	  s->params.format = SANE_FRAME_GRAY;
	  s->params.bytes_per_line = s->params.pixels_per_line;
	  s->params.depth = 8;
	}
      else
	{
	  /* it's one of the color modes... */

	  s->params.bytes_per_line = 3 * s->params.pixels_per_line;
	  s->params.depth = 8;
	  s->params.format = SANE_FRAME_RGB;
	}
    }
  s->params.last_frame = SANE_TRUE;
  if (params)
    *params = s->params;
  DBG (4, "sane_get_parameters: frame = %d; last_frame = %s; depth = %d\n",
       s->params.format, s->params.last_frame ? "true" : "false",
       s->params.depth);
  DBG (4, "sane_get_parameters: lines = %d; ppl = %d; bpl = %d\n",
       s->params.lines, s->params.pixels_per_line, s->params.bytes_per_line);

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Ma1509_Scanner *s = handle;
  SANE_Status status;
  SANE_String_Const mode;
  struct timeval start;

  if (!s)
    {
      DBG (1, "sane_start: handle is null!\n");
      return SANE_STATUS_INVAL;
    }

  DBG (4, "sane_start\n");

  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;

  /* Check for inconsistencies */

  if (s->val[OPT_TL_X].w > s->val[OPT_BR_X].w)
    {
      DBG (0, "sane_start: %s (%.1f mm) is bigger than %s (%.1f mm) "
	   "-- aborting\n",
	   s->opt[OPT_TL_X].title, SANE_UNFIX (s->val[OPT_TL_X].w),
	   s->opt[OPT_BR_X].title, SANE_UNFIX (s->val[OPT_BR_X].w));
      return SANE_STATUS_INVAL;
    }
  if (s->val[OPT_TL_Y].w > s->val[OPT_BR_Y].w)
    {
      DBG (0, "sane_start: %s (%.1f mm) is bigger than %s (%.1f mm) "
	   "-- aborting\n",
	   s->opt[OPT_TL_Y].title, SANE_UNFIX (s->val[OPT_TL_Y].w),
	   s->opt[OPT_BR_Y].title, SANE_UNFIX (s->val[OPT_BR_Y].w));
      return SANE_STATUS_INVAL;
    }

  s->total_bytes = 0;
  s->read_bytes = 0;

  /* save start time */
  gettimeofday (&start, 0);
  s->start_time = start.tv_sec;
  /* translate options into s->mode for convenient access: */
  mode = s->val[OPT_MODE].s;

  status = set_window (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: set window command failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  status = test_unit_ready (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: test_unit_ready failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART) != 0)
    {
      status = calibration (s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "sane_start: calibration failed: %s\n",
	       sane_strstatus (status));
	  goto stop_scanner_and_return;
	}

      status = send_gamma (s);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "sane_start: send_gamma failed: %s\n",
	       sane_strstatus (status));
	  goto stop_scanner_and_return;
	}
    }

  s->scanning = SANE_TRUE;
  s->cancelled = SANE_FALSE;

  status = start_scan (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: start_scan command failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  status = start_read_data (s);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: start_read_data command failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }

  s->params.bytes_per_line = s->hw->bpl;
  s->params.pixels_per_line = s->params.bytes_per_line;
  if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR) == 0)
    s->params.pixels_per_line /= 3;
  else if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART) == 0)
    s->params.pixels_per_line *= 8;

  s->params.lines = s->hw->lines;

  s->buffer = (SANE_Byte *) malloc (MA1509_BUFFER_SIZE);
  if (!s->buffer)
    return SANE_STATUS_NO_MEM;
  s->buffer_bytes = 0;

  DBG (5, "sane_start: finished\n");
  return SANE_STATUS_GOOD;

stop_scanner_and_return:
  sanei_usb_close (s->fd);
  s->scanning = SANE_FALSE;
  return status;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  Ma1509_Scanner *s = handle;
  SANE_Status status;
  SANE_Int total_size = s->hw->lines * s->hw->bpl;
  SANE_Int i;

  if (!s)
    {
      DBG (1, "sane_read: handle is null!\n");
      return SANE_STATUS_INVAL;
    }

  if (!buf)
    {
      DBG (1, "sane_read: buf is null!\n");
      return SANE_STATUS_INVAL;
    }

  if (!len)
    {
      DBG (1, "sane_read: len is null!\n");
      return SANE_STATUS_INVAL;
    }

  DBG (5, "sane_read\n");
  *len = 0;

  if (s->cancelled)
    {
      DBG (4, "sane_read: scan was cancelled\n");
      return SANE_STATUS_CANCELLED;
    }

  if (!s->scanning)
    {
      DBG (1, "sane_read: must call sane_start before sane_read\n");
      return SANE_STATUS_INVAL;
    }

  if (total_size - s->read_bytes <= 0)
    {
      DBG (4, "sane_read: EOF\n");
      stop_scan (s);
      s->scanning = SANE_FALSE;
      return SANE_STATUS_EOF;
    }

  if (s->buffer_bytes == 0)
    {
      SANE_Int size = MA1509_BUFFER_SIZE;
      if (size > (total_size - s->total_bytes))
	size = total_size - s->total_bytes;
      DBG (4, "sane_read: trying to read %d bytes\n", size);
      status = read_data (s, s->buffer, &size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "sane_read: read_data failed: %s\n",
	       sane_strstatus (status));
	  *len = 0;
	  return status;
	}
      s->total_bytes += size;
      s->buffer_start = s->buffer;
      s->buffer_bytes = size;
    }

  *len = max_len;
  if (*len > s->buffer_bytes)
    *len = s->buffer_bytes;

  memcpy (buf, s->buffer_start, *len);
  s->buffer_start += (*len);
  s->buffer_bytes -= (*len);
  s->read_bytes += (*len);

  /* invert for lineart mode */
  if (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART) == 0)
    {
      for (i = 0; i < *len; i++)
	buf[i] = ~buf[i];
    }

  DBG (4, "sane_read: read %d/%d bytes (%d bytes to go, %d total)\n", *len,
       max_len, total_size - s->read_bytes, total_size);

  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Ma1509_Scanner *s = handle;

  if (!s)
    {
      DBG (1, "sane_cancel: handle is null!\n");
      return;
    }

  DBG (4, "sane_cancel\n");
  if (s->scanning)
    {
      s->cancelled = SANE_TRUE;
      stop_scan (s);
      free (s->buffer);
    }
  s->scanning = SANE_FALSE;
  DBG (4, "sane_cancel finished\n");
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Ma1509_Scanner *s = handle;

  if (!s)
    {
      DBG (1, "sane_set_io_mode: handle is null!\n");
      return SANE_STATUS_INVAL;
    }

  DBG (4, "sane_set_io_mode: %s\n",
       non_blocking ? "non-blocking" : "blocking");

  if (!s->scanning)
    {
      DBG (1, "sane_set_io_mode: call sane_start before sane_set_io_mode");
      return SANE_STATUS_INVAL;
    }

  if (non_blocking)
    return SANE_STATUS_UNSUPPORTED;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  Ma1509_Scanner *s = handle;

  if (!s)
    {
      DBG (1, "sane_get_select_fd: handle is null!\n");
      return SANE_STATUS_INVAL;
    }
  if (!fd)
    {
      DBG (1, "sane_get_select_fd: fd is null!\n");
      return SANE_STATUS_INVAL;
    }

  DBG (4, "sane_get_select_fd\n");
  if (!s->scanning)
    return SANE_STATUS_INVAL;

  return SANE_STATUS_UNSUPPORTED;
}
