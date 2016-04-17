/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Frank Zago (sane at zago dot net)
   
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
*/

/*
   $Id$
   VM3552 (and maybe VM4552 and VM6552)
*/

/*--------------------------------------------------------------------------*/

#define BUILD 1			/* 2002/08/06 */
#define BACKEND_NAME teco3
#define TECO_CONFIG_FILE "teco3.conf"

/*--------------------------------------------------------------------------*/

#include "../include/sane/config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/lassert.h"

#include "teco3.h"

/*--------------------------------------------------------------------------*/

/* Lists of possible scan modes. */
static SANE_String_Const scan_mode_list[] = {
  BLACK_WHITE_STR,
  GRAY_STR,
  COLOR_STR,
  NULL
};

/*--------------------------------------------------------------------------*/

/* Minimum and maximum width and length supported. */
static SANE_Range x_range = { SANE_FIX (0), SANE_FIX (8.5 * MM_PER_INCH), 0 };
static SANE_Range y_range = { SANE_FIX (0), SANE_FIX (14 * MM_PER_INCH), 0 };

/*--------------------------------------------------------------------------*/

/* Gamma range */
static const SANE_Range gamma_range = {
  0,				/* minimum */
  255,				/* maximum */
  0				/* quantization */
};

/*--------------------------------------------------------------------------*/

/* List of dithering options. */
static SANE_String_Const dither_list[] = {
  "Line art",
  "2x2",
  "3x3",
  "4x4 bayer",
  "4x4 smooth",
  "8x8 bayer",
  "8x8 smooth",
  "8x8 horizontal",
  "8x8 vertical",
  NULL
};
static const int dither_val[] = {
  0x00,
  0x01,
  0x02,
  0x03,
  0x04,
  0x05,
  0x06,
  0x07,
  0x08
};

/*--------------------------------------------------------------------------*/

static const SANE_Range threshold_range = {
  0,				/* minimum */
  255,				/* maximum */
  0				/* quantization */
};

/*--------------------------------------------------------------------------*/

/* Define the supported scanners and their characteristics. */
static const struct scanners_supported scanners[] = {
  {
   6, "TECO VM3552",
   TECO_VM3552,
   "Relisys", "Scorpio",
   {1, 1200, 1},		/* resolution range */
   300, 1200			/* max x and Y resolution */
   }
};

/*--------------------------------------------------------------------------*/

/* List of scanner attached. */
static Teco_Scanner *first_dev = NULL;
static int num_devices = 0;
static const SANE_Device **devlist = NULL;


/* Local functions. */

/* Display a buffer in the log. */
static void
hexdump (int level, const char *comment, unsigned char *p, int l)
{
  int i;
  char line[128];
  char *ptr;
  char asc_buf[17];
  char *asc_ptr;

  DBG (level, "%s\n", comment);

  ptr = line;
  *ptr = '\0';
  asc_ptr = asc_buf;
  *asc_ptr = '\0';

  for (i = 0; i < l; i++, p++)
    {
      if ((i % 16) == 0)
	{
	  if (ptr != line)
	    {
	      DBG (level, "%s    %s\n", line, asc_buf);
	      ptr = line;
	      *ptr = '\0';
	      asc_ptr = asc_buf;
	      *asc_ptr = '\0';
	    }
	  sprintf (ptr, "%3.3d:", i);
	  ptr += 4;
	}
      ptr += sprintf (ptr, " %2.2x", *p);
      if (*p >= 32 && *p <= 127)
	{
	  asc_ptr += sprintf (asc_ptr, "%c", *p);
	}
      else
	{
	  asc_ptr += sprintf (asc_ptr, ".");
	}
    }
  *ptr = '\0';
  DBG (level, "%s    %s\n", line, asc_buf);
}

/* Returns the length of the longest string, including the terminating
 * character. */
static size_t
max_string_size (SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	{
	  max_size = size;
	}
    }

  return max_size;
}

/* Lookup a string list from one array and return its index. */
static int
get_string_list_index (SANE_String_Const list[], SANE_String_Const name)
{
  int index;

  index = 0;
  while (list[index] != NULL)
    {
      if (strcmp (list[index], name) == 0)
	{
	  return (index);
	}
      index++;
    }

  DBG (DBG_error, "name %s not found in list\n", name);

  assert (0 == 1);		/* bug in backend, core dump */

  return (-1);
}

/* Initialize a scanner entry. Return an allocated scanner with some
 * preset values. */
static Teco_Scanner *
teco_init (void)
{
  Teco_Scanner *dev;

  DBG (DBG_proc, "teco_init: enter\n");

  /* Allocate a new scanner entry. */
  dev = malloc (sizeof (Teco_Scanner));
  if (dev == NULL)
    {
      return NULL;
    }

  memset (dev, 0, sizeof (Teco_Scanner));

  /* Allocate the buffer used to transfer the SCSI data. */
  dev->buffer_size = 64 * 1024;
  dev->buffer = malloc (dev->buffer_size);
  if (dev->buffer == NULL)
    {
      free (dev);
      return NULL;
    }

  dev->sfd = -1;

  DBG (DBG_proc, "teco_init: exit\n");

  return (dev);
}

/* Closes an open scanner. */
static void
teco_close (Teco_Scanner * dev)
{
  DBG (DBG_proc, "teco_close: enter\n");

  if (dev->sfd != -1)
    {
      sanei_scsi_close (dev->sfd);
      dev->sfd = -1;
    }

  DBG (DBG_proc, "teco_close: exit\n");
}

/* Frees the memory used by a scanner. */
static void
teco_free (Teco_Scanner * dev)
{
  int i;

  DBG (DBG_proc, "teco_free: enter\n");

  if (dev == NULL)
    return;

  teco_close (dev);
  if (dev->devicename)
    {
      free (dev->devicename);
    }
  if (dev->buffer)
    {
      free (dev->buffer);
    }
  if (dev->image)
    {
      free (dev->image);
    }
  for (i = 1; i < OPT_NUM_OPTIONS; i++)
    {
      if (dev->opt[i].type == SANE_TYPE_STRING && dev->val[i].s)
	{
	  free (dev->val[i].s);
	}
    }

  free (dev);

  DBG (DBG_proc, "teco_free: exit\n");
}

/* Inquiry a device and returns TRUE if is supported. */
static int
teco_identify_scanner (Teco_Scanner * dev)
{
  CDB cdb;
  SANE_Status status;
  size_t size;
  int i;

  DBG (DBG_proc, "teco_identify_scanner: enter\n");

  size = 5;
  MKSCSI_INQUIRY (cdb, size);
  status = sanei_scsi_cmd2 (dev->sfd, cdb.data, cdb.len,
			    NULL, 0, dev->buffer, &size);

  if (status)
    {
      DBG (DBG_error,
	   "teco_identify_scanner: inquiry failed with status %s\n",
	   sane_strstatus (status));
      return (SANE_FALSE);
    }

  size = dev->buffer[4] + 5;	/* total length of the inquiry data */

  if (size < 53)
    {
      DBG (DBG_error,
	   "teco_identify_scanner: not enough data to identify device\n");
      return (SANE_FALSE);
    }

  MKSCSI_INQUIRY (cdb, size);
  status = sanei_scsi_cmd2 (dev->sfd, cdb.data, cdb.len,
			    NULL, 0, dev->buffer, &size);

  if (status)
    {
      DBG (DBG_error,
	   "teco_identify_scanner: inquiry failed with status %s\n",
	   sane_strstatus (status));
      return (SANE_FALSE);
    }

  hexdump (DBG_info2, "inquiry", dev->buffer, size);

  dev->scsi_type = dev->buffer[0] & 0x1f;
  memcpy (dev->scsi_vendor, dev->buffer + 0x08, 0x08);
  dev->scsi_vendor[0x08] = 0;
  memcpy (dev->scsi_product, dev->buffer + 0x10, 0x010);
  dev->scsi_product[0x10] = 0;
  memcpy (dev->scsi_version, dev->buffer + 0x20, 0x04);
  dev->scsi_version[0x04] = 0;
  memcpy (dev->scsi_teco_name, dev->buffer + 0x2A, 0x0B);
  dev->scsi_teco_name[0x0B] = 0;

  DBG (DBG_info, "device is \"%s\" \"%s\" \"%s\" \"%s\"\n",
       dev->scsi_vendor, dev->scsi_product, dev->scsi_version,
       dev->scsi_teco_name);

  /* Lookup through the supported scanners table to find if this
   * backend supports that one. */
  for (i = 0; i < NELEMS (scanners); i++)
    {

      if (dev->scsi_type == scanners[i].scsi_type &&
	  strcmp (dev->scsi_teco_name, scanners[i].scsi_teco_name) == 0)
	{

	  DBG (DBG_error, "teco_identify_scanner: scanner supported\n");

	  dev->def = &(scanners[i]);

	  return (SANE_TRUE);
	}
    }

  DBG (DBG_proc, "teco_identify_scanner: exit, device not supported\n");

  return (SANE_FALSE);
}

/* SCSI sense handler. Callback for SANE.
 * These scanners never set asc or ascq. */
static SANE_Status
teco_sense_handler (int __sane_unused__ scsi_fd, unsigned char *result, void __sane_unused__ *arg)
{
  int sensekey;
  int len;

  DBG (DBG_proc, "teco_sense_handler: enter\n");

  sensekey = get_RS_sense_key (result);
  len = 7 + get_RS_additional_length (result);

  hexdump (DBG_info2, "sense", result, len);

  if (get_RS_error_code (result) != 0x70)
    {
      DBG (DBG_error,
	   "teco_sense_handler: invalid sense key error code (%d)\n",
	   get_RS_error_code (result));

      return SANE_STATUS_IO_ERROR;
    }

  if (len < 14)
    {
      DBG (DBG_error, "teco_sense_handler: sense too short, no ASC/ASCQ\n");

      return SANE_STATUS_IO_ERROR;
    }

  DBG (DBG_sense, "teco_sense_handler: sense=%d\n", sensekey);

  if (sensekey == 0x00)
    {
      return SANE_STATUS_GOOD;
    }

  return SANE_STATUS_IO_ERROR;
}

/* Set a window. */
static SANE_Status
teco_set_window (Teco_Scanner * dev)
{
  size_t window_size;
  CDB cdb;
  unsigned char window[255];
  SANE_Status status;
  int i;

  DBG (DBG_proc, "teco_set_window: enter\n");

  /* size of the whole windows block */
  switch (dev->def->tecoref)
    {
    case TECO_VM3552:
      window_size = 69;
      break;
	default:
		assert(0);
    }


  MKSCSI_SET_WINDOW (cdb, window_size);

  memset (window, 0, window_size);

  /* size of the windows descriptor block */
  window[7] = window_size - 8;

  /* X and Y resolution */
  Ito16 (dev->x_resolution, &window[10]);
  Ito16 (dev->y_resolution, &window[12]);

  /* Upper Left (X,Y) */
  Ito32 (dev->x_tl, &window[14]);
  Ito32 (dev->y_tl, &window[18]);

  /* Width and length */
  Ito32 (dev->width, &window[22]);
  Ito32 (dev->length, &window[26]);

  /* Image Composition */
  switch (dev->scan_mode)
    {
    case TECO_BW:
      window[31] = dev->val[OPT_THRESHOLD].w;
      window[33] = 0x00;
      i = get_string_list_index (dither_list, dev->val[OPT_DITHER].s);
      window[36] = dither_val[i];
      break;
    case TECO_GRAYSCALE:
	  window[31] = 0x80;
      window[33] = 0x02;
      break;
    case TECO_COLOR:
	  window[31] = 0x80;
      window[33] = 0x05;
      break;
    }

  /* Depth */
  window[34] = dev->depth;

  /* Unknown - invariants */
  window[37] = 0x80;

  switch (dev->def->tecoref)
    {
    case TECO_VM3552:
      window[48] = 0x01;
      window[50] = 0x02;
      window[53] = 0xff;
      window[57] = 0xff;
      window[61] = 0xff;
      window[65] = 0xff;
      break;
    }

  hexdump (DBG_info2, "windows", window, window_size);

  status = sanei_scsi_cmd2 (dev->sfd, cdb.data, cdb.len,
			    window, window_size, NULL, NULL);

  DBG (DBG_proc, "teco_set_window: exit, status=%d\n", status);

  return status;
}

/* Park the CCD */
static SANE_Status
teco_reset_window (Teco_Scanner * dev)
{
  SANE_Status status;
  CDB cdb;

  DBG (DBG_proc, "teco_reset_window: enter\n");

  MKSCSI_OBJECT_POSITION (cdb, 0);

  hexdump (DBG_info2, "CDB:", cdb.data, cdb.len);
  status = sanei_scsi_cmd2 (dev->sfd, cdb.data, cdb.len, NULL, 0, NULL, NULL);

  DBG (DBG_proc, "teco_reset_window: leave, status=%d\n", status);

  return status;
}

/* Return the number of byte that can be read. */
static SANE_Status
get_filled_data_length (Teco_Scanner * dev, size_t * to_read)
{
  size_t size;
  CDB cdb;
  SANE_Status status;

  DBG (DBG_proc, "get_filled_data_length: enter\n");

  *to_read = 0;

  size = 0x12;
  MKSCSI_GET_DATA_BUFFER_STATUS (cdb, 1, size);
  status = sanei_scsi_cmd2 (dev->sfd, cdb.data, cdb.len,
			    NULL, 0, dev->buffer, &size);

  if (size < 0x10)
    {
      DBG (DBG_error,
	   "get_filled_data_length: not enough data returned (%ld)\n",
	   (long) size);
    }

  hexdump (DBG_info2, "get_filled_data_length return", dev->buffer, size);

  *to_read = B24TOI (&dev->buffer[9]);

  DBG (DBG_info, "%d %d  -  %d %d\n",
       dev->params.lines, B16TOI (&dev->buffer[12]),
       dev->params.bytes_per_line, B16TOI (&dev->buffer[14]));

  if (dev->real_bytes_left == 0)
    {

      DBG (DBG_error,
	   "get_filled_data_length: internal scanner buffer size is %d bytes\n",
	   B24TOI (&dev->buffer[6]));

      /* Beginning of a scan. */
      dev->params.lines = B16TOI (&dev->buffer[12]);
      dev->bytes_per_raster = B16TOI (&dev->buffer[14]);

      switch (dev->scan_mode)
	{
	case TECO_BW:
	  dev->params.bytes_per_line = B16TOI (&dev->buffer[14]);
	  dev->params.pixels_per_line = dev->params.bytes_per_line * 8;
	  break;

	case TECO_GRAYSCALE:
	  dev->params.pixels_per_line = B16TOI (&dev->buffer[14]);
	  dev->params.bytes_per_line = dev->params.pixels_per_line;
	  break;

	case TECO_COLOR:
	  dev->params.pixels_per_line = B16TOI (&dev->buffer[14]);
	  dev->params.bytes_per_line = dev->params.pixels_per_line * 3;
	  if (dev->buffer[17] == 0x07)
	    {
	      /* There is no RAM extension present. The colors will
	       * be shifted and the backend will need to fix that. 
	       */
	      dev->does_color_shift = 1;
	    }
	  else
	    {
	      dev->does_color_shift = 0;
	    }
	  break;
	}
    }

  DBG (DBG_info, "get_filled_data_length: to read = %ld\n", (long) *to_read);

  DBG (DBG_proc, "get_filled_data_length: exit, status=%d\n", status);

  return (status);
}

/* Start a scan. */
static SANE_Status
teco_scan (Teco_Scanner * dev)
{
  CDB cdb;
  SANE_Status status;

  DBG (DBG_proc, "teco_scan: enter\n");

  MKSCSI_SCAN (cdb);

  status = sanei_scsi_cmd2 (dev->sfd, cdb.data, cdb.len, NULL, 0, NULL, NULL);

  DBG (DBG_proc, "teco_scan: exit, status=%d\n", status);

  return status;
}

/* Do some vendor specific stuff. */
static SANE_Status
teco_vendor_spec (Teco_Scanner * dev)
{
  CDB cdb;
  SANE_Status status;
  size_t size;

  DBG (DBG_proc, "teco_vendor_spec: enter\n");

  size = 0x7800;

  cdb.data[0] = 0x09;
  cdb.data[1] = 0;
  cdb.data[2] = 0;
  cdb.data[3] = (size >> 8) & 0xff;
  cdb.data[4] = (size >> 0) & 0xff;
  cdb.data[5] = 0;
  cdb.len = 6;

  status = sanei_scsi_cmd2 (dev->sfd, cdb.data, cdb.len,
			    NULL, 0, dev->buffer, &size);

  /*hexdump (DBG_info2, "calibration:", dev->buffer, size); */

  cdb.data[0] = 0x0E;
  cdb.data[1] = 0;
  cdb.data[2] = 0;
  cdb.data[3] = 0;
  cdb.data[4] = 0;
  cdb.data[5] = 0;
  cdb.len = 6;

  status = sanei_scsi_cmd2 (dev->sfd, cdb.data, cdb.len, NULL, 0, NULL, NULL);

  return status;
}

/* Send the gamma */
static SANE_Status
teco_send_gamma (Teco_Scanner * dev)
{
  CDB cdb;
  SANE_Status status;
  struct
  {
    unsigned char gamma_R[GAMMA_LENGTH];
    unsigned char gamma_G[GAMMA_LENGTH];	/* also gray */
    unsigned char gamma_B[GAMMA_LENGTH];
    unsigned char gamma_unused[GAMMA_LENGTH];
  }
  param;
  size_t i;
  size_t size;

  DBG (DBG_proc, "teco_send_gamma: enter\n");

  size = sizeof (param);
  assert (size == 4 * GAMMA_LENGTH);
  MKSCSI_SEND_10 (cdb, 0x03, 0x02, size);

  if (dev->val[OPT_CUSTOM_GAMMA].w)
    {
      /* Use the custom gamma. */
      if (dev->scan_mode == TECO_GRAYSCALE)
	{
	  /* Gray */
	  for (i = 0; i < GAMMA_LENGTH; i++)
	    {
	      param.gamma_R[i] = 0;
	      param.gamma_G[i] = dev->gamma_GRAY[i];
	      param.gamma_B[i] = 0;
	      param.gamma_unused[i] = 0;
	    }
	}
      else
	{
	  /* Color */
	  for (i = 0; i < GAMMA_LENGTH; i++)
	    {
	      param.gamma_R[i] = dev->gamma_R[i];
	      param.gamma_G[i] = dev->gamma_G[i];
	      param.gamma_B[i] = dev->gamma_B[i];
	      param.gamma_unused[i] = 0;
	    }
	}
    }
  else
    {
      for (i = 0; i < GAMMA_LENGTH; i++)
	{
	  param.gamma_R[i] = i / 4;
	  param.gamma_G[i] = i / 4;
	  param.gamma_B[i] = i / 4;
	  param.gamma_unused[i] = 0;
	}
    }

  hexdump (DBG_info2, "teco_send_gamma:", cdb.data, cdb.len);

  status = sanei_scsi_cmd2 (dev->sfd, cdb.data, cdb.len,
			    &param, size, NULL, NULL);

  DBG (DBG_proc, "teco_send_gamma: exit, status=%d\n", status);

  return (status);
}

/* Attach a scanner to this backend. */
static SANE_Status
attach_scanner (const char *devicename, Teco_Scanner ** devp)
{
  Teco_Scanner *dev;
  SANE_Status status;
  int sfd;

  DBG (DBG_sane_proc, "attach_scanner: %s\n", devicename);

  if (devp)
    *devp = NULL;

  /* Check if we know this device name. */
  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devicename) == 0)
	{
	  if (devp)
	    {
	      *devp = dev;
	    }
	  DBG (DBG_info, "device is already known\n");
	  return SANE_STATUS_GOOD;
	}
    }

  /* Allocate a new scanner entry. */
  dev = teco_init ();
  if (dev == NULL)
    {
      DBG (DBG_error, "ERROR: not enough memory\n");
      return SANE_STATUS_NO_MEM;
    }

  DBG (DBG_info, "attach_scanner: opening %s\n", devicename);

  status = sanei_scsi_open (devicename, &sfd, teco_sense_handler, dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "ERROR: attach_scanner: open failed (%s)\n",
	   sane_strstatus (status));
      teco_free (dev);
      return SANE_STATUS_INVAL;
    }

  /* Fill some scanner specific values. */
  dev->devicename = strdup (devicename);
  dev->sfd = sfd;

  /* Now, check that it is a scanner we support. */
  if (teco_identify_scanner (dev) == SANE_FALSE)
    {
      DBG (DBG_error,
	   "ERROR: attach_scanner: scanner-identification failed\n");
      teco_free (dev);
      return SANE_STATUS_INVAL;
    }

  teco_close (dev);

  /* Set the default options for that scanner. */
  dev->sane.name = dev->devicename;
  dev->sane.vendor = dev->def->real_vendor;
  dev->sane.model = dev->def->real_product;
  dev->sane.type = "flatbed scanner";

  /* Link the scanner with the others. */
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    {
      *devp = dev;
    }

  num_devices++;

  DBG (DBG_proc, "attach_scanner: exit\n");

  return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one (const char *dev)
{
  attach_scanner (dev, NULL);
  return SANE_STATUS_GOOD;
}

/* Reset the options for that scanner. */
static void
teco_init_options (Teco_Scanner * dev)
{
  int i;

  /* Pre-initialize the options. */
  memset (dev->opt, 0, sizeof (dev->opt));
  memset (dev->val, 0, sizeof (dev->val));

  for (i = 0; i < OPT_NUM_OPTIONS; ++i)
    {
      dev->opt[i].size = sizeof (SANE_Word);
      dev->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  /* Number of options. */
  dev->opt[OPT_NUM_OPTS].name = "";
  dev->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  dev->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  dev->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  dev->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  dev->val[OPT_NUM_OPTS].w = OPT_NUM_OPTIONS;

  /* Mode group */
  dev->opt[OPT_MODE_GROUP].title = SANE_TITLE_SCAN_MODE;
  dev->opt[OPT_MODE_GROUP].desc = "";	/* not valid for a group */
  dev->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  dev->opt[OPT_MODE_GROUP].cap = 0;
  dev->opt[OPT_MODE_GROUP].size = 0;
  dev->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Scanner supported modes */
  dev->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  dev->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  dev->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  dev->opt[OPT_MODE].type = SANE_TYPE_STRING;
  dev->opt[OPT_MODE].size = max_string_size (scan_mode_list);
  dev->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_MODE].constraint.string_list = scan_mode_list;
  dev->val[OPT_MODE].s = (SANE_Char *) strdup ("");	/* will be set later */

  /* X and Y resolution */
  dev->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  dev->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  dev->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_RESOLUTION].constraint.range = &dev->def->res_range;
  dev->val[OPT_RESOLUTION].w = 100;

  /* Geometry group */
  dev->opt[OPT_GEOMETRY_GROUP].title = SANE_I18N ("Geometry");
  dev->opt[OPT_GEOMETRY_GROUP].desc = "";	/* not valid for a group */
  dev->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  dev->opt[OPT_GEOMETRY_GROUP].cap = 0;
  dev->opt[OPT_GEOMETRY_GROUP].size = 0;
  dev->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Upper left X */
  dev->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  dev->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  dev->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  dev->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  dev->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  dev->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_TL_X].constraint.range = &x_range;
  dev->val[OPT_TL_X].w = x_range.min;

  /* Upper left Y */
  dev->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  dev->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  dev->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  dev->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  dev->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  dev->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_TL_Y].constraint.range = &y_range;
  dev->val[OPT_TL_Y].w = y_range.min;

  /* Bottom-right x */
  dev->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  dev->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  dev->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  dev->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  dev->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  dev->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_BR_X].constraint.range = &x_range;
  dev->val[OPT_BR_X].w = x_range.max;

  /* Bottom-right y */
  dev->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  dev->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  dev->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  dev->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  dev->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  dev->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_BR_Y].constraint.range = &y_range;
  dev->val[OPT_BR_Y].w = y_range.max;

  /* Enhancement group */
  dev->opt[OPT_ENHANCEMENT_GROUP].title = SANE_I18N ("Enhancement");
  dev->opt[OPT_ENHANCEMENT_GROUP].desc = "";	/* not valid for a group */
  dev->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  dev->opt[OPT_ENHANCEMENT_GROUP].cap = SANE_CAP_ADVANCED;
  dev->opt[OPT_ENHANCEMENT_GROUP].size = 0;
  dev->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Halftone pattern */
  dev->opt[OPT_DITHER].name = "dither";
  dev->opt[OPT_DITHER].title = SANE_I18N ("Dither");
  dev->opt[OPT_DITHER].desc = SANE_I18N ("Dither");
  dev->opt[OPT_DITHER].type = SANE_TYPE_STRING;
  dev->opt[OPT_DITHER].size = max_string_size (dither_list);
  dev->opt[OPT_DITHER].cap |= SANE_CAP_INACTIVE;
  dev->opt[OPT_DITHER].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_DITHER].constraint.string_list = dither_list;
  dev->val[OPT_DITHER].s = strdup (dither_list[0]);

  /* custom-gamma table */
  dev->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  dev->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  dev->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  dev->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  dev->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
  dev->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;

  /* red gamma vector */
  dev->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  dev->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  dev->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  dev->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  dev->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
  dev->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GAMMA_VECTOR_R].size = GAMMA_LENGTH * sizeof (SANE_Word);
  dev->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GAMMA_VECTOR_R].constraint.range = &gamma_range;
  dev->val[OPT_GAMMA_VECTOR_R].wa = dev->gamma_R;

  /* green and gamma vector */
  dev->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  dev->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  dev->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  dev->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  dev->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
  dev->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GAMMA_VECTOR_G].size = GAMMA_LENGTH * sizeof (SANE_Word);
  dev->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GAMMA_VECTOR_G].constraint.range = &gamma_range;
  dev->val[OPT_GAMMA_VECTOR_G].wa = dev->gamma_G;

  /* blue gamma vector */
  dev->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  dev->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  dev->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  dev->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  dev->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
  dev->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GAMMA_VECTOR_B].size = GAMMA_LENGTH * sizeof (SANE_Word);
  dev->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GAMMA_VECTOR_B].constraint.range = &gamma_range;
  dev->val[OPT_GAMMA_VECTOR_B].wa = dev->gamma_B;

  /* grayscale gamma vector */
  dev->opt[OPT_GAMMA_VECTOR_GRAY].name = SANE_NAME_GAMMA_VECTOR;
  dev->opt[OPT_GAMMA_VECTOR_GRAY].title = SANE_TITLE_GAMMA_VECTOR;
  dev->opt[OPT_GAMMA_VECTOR_GRAY].desc = SANE_DESC_GAMMA_VECTOR;
  dev->opt[OPT_GAMMA_VECTOR_GRAY].type = SANE_TYPE_INT;
  dev->opt[OPT_GAMMA_VECTOR_GRAY].cap |= SANE_CAP_INACTIVE;
  dev->opt[OPT_GAMMA_VECTOR_GRAY].unit = SANE_UNIT_NONE;
  dev->opt[OPT_GAMMA_VECTOR_GRAY].size = GAMMA_LENGTH * sizeof (SANE_Word);
  dev->opt[OPT_GAMMA_VECTOR_GRAY].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_GAMMA_VECTOR_GRAY].constraint.range = &gamma_range;
  dev->val[OPT_GAMMA_VECTOR_GRAY].wa = dev->gamma_GRAY;

  /* Threshold */
  dev->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  dev->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  dev->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  dev->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
  dev->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
  dev->opt[OPT_THRESHOLD].size = sizeof (SANE_Int);
  dev->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
  dev->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_THRESHOLD].constraint.range = &threshold_range;
  dev->val[OPT_THRESHOLD].w = 128;

  /* preview */
  dev->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  dev->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  dev->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  dev->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  dev->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  dev->val[OPT_PREVIEW].w = SANE_FALSE;

  /* Lastly, set the default scan mode. This might change some
   * values previously set here. */
  sane_control_option (dev, OPT_MODE, SANE_ACTION_SET_VALUE,
		       (SANE_String_Const *) scan_mode_list[0], NULL);
}

/* 
 * Wait until the scanner is ready.
 */
static SANE_Status
teco_wait_scanner (Teco_Scanner * dev)
{
  SANE_Status status;
  int timeout;
  CDB cdb;

  DBG (DBG_proc, "teco_wait_scanner: enter\n");

  MKSCSI_TEST_UNIT_READY (cdb);

  /* Set the timeout to 60 seconds. */
  timeout = 60;

  while (timeout > 0)
    {

      /* test unit ready */
      status = sanei_scsi_cmd2 (dev->sfd, cdb.data, cdb.len,
				NULL, 0, NULL, NULL);

      if (status == SANE_STATUS_GOOD)
	{
	  return SANE_STATUS_GOOD;
	}

      sleep (1);
    };

  DBG (DBG_proc, "teco_wait_scanner: scanner not ready\n");
  return (SANE_STATUS_IO_ERROR);
}

/* 
 * Get the sense
 */
static SANE_Status
teco_query_sense (Teco_Scanner * dev)
{
  SANE_Status status;
  unsigned char buf[255];
  CDB cdb;
  size_t size;

  DBG (DBG_proc, "teco_wait_scanner: enter\n");

  size = sizeof (buf);
  MKSCSI_REQUEST_SENSE (cdb, size);

  status = sanei_scsi_cmd2 (dev->sfd, cdb.data, cdb.len, NULL, 0, buf, &size);

  hexdump (DBG_info2, "sense", buf, size);

  DBG (DBG_error, "teco_query_sense: return (%s)\n", sane_strstatus (status));
  return (status);
}

/* 
 * Adjust the rasters. This function is used during a color scan,
 * because the scanner does not present a format sane can interpret
 * directly.
 *
 * The scanner sends the colors by rasters (B then G then R), whereas
 * sane is waiting for a group of 3 bytes per color. To make things
 * funnier, the rasters are shifted. The format of those raster is: 
 *   BGR...BGR
 *
 * For a proper scan, the first 2 R and 1 G, and the last 1 G and 2 B
 * must be ignored. (TODO)
 * 
 * So this function reorders all that mess. It gets the input from
 * dev->buffer and write the output in dev->image. size_in the the
 * length of the valid data in dev->buffer.  */

/* 0=red, 1=green, 2=blue */

#define COLOR_0 2
#define COLOR_1 1
#define COLOR_2 0
#define line_shift ( - dev->color_shift)

static void
teco_adjust_raster (Teco_Scanner * dev, size_t size_in)
{
  int nb_rasters;		/* number of rasters in dev->buffer */

  int raster;			/* current raster number in buffer */
  int line;			/* line number for that raster */
  int color = -1;			/* color for that raster */
  size_t offset;

  DBG (DBG_proc, "teco_adjust_raster: enter\n");

  assert (dev->scan_mode == TECO_COLOR);
  assert ((size_in % dev->bytes_per_raster) == 0);

  if (size_in == 0)
    {
      return;
    }

  /* 
   * The color coding is one line for each color (in the RGB order).
   * Recombine that stuff to create a RGB value for each pixel.
   */

  nb_rasters = size_in / dev->raster_size;

  for (raster = 0; raster < nb_rasters; raster++)
    {

      /* 
       * Find the color to which this raster belongs to.
       */
      line = 0;
      if (dev->raster_num < dev->color_shift)
	{
	  color = COLOR_0;
	  line = dev->raster_num;
	}
      else if (dev->raster_num < (3 * dev->color_shift))
	{

	  if ((dev->raster_num - line_shift) % 2)
	    {
	      color = COLOR_1;
	      line = (dev->raster_num + line_shift) / 2;
	    }
	  else
	    {
	      color = COLOR_0;
	      line = (dev->raster_num - line_shift) / 2;
	    }
	}
      else if (dev->raster_num >= dev->raster_real - dev->color_shift)
	{
	  color = COLOR_2;
	  line = dev->line;
	}
      else if (dev->raster_num >= dev->raster_real - 3 * dev->color_shift)
	{
	  if ((dev->raster_real - dev->raster_num - line_shift) % 2)
	    {
	      color = COLOR_2;
	      line = dev->line;
	    }
	  else
	    {
	      color = COLOR_1;
	      line = dev->line - line_shift;
	    }
	}
      else
	{
	  switch ((dev->raster_num - 3 * line_shift) % 3)
	    {
	    case 0:
	      color = COLOR_0;
	      line = (dev->raster_num - 3 * line_shift) / 3;
	      break;
	    case 1:
	      color = COLOR_1;
	      line = dev->raster_num / 3;
	      break;
	    case 2:
	      color = COLOR_2;
	      line = (dev->raster_num + 3 * line_shift) / 3;
	      break;
	    }
	}

      /* Adjust the line number relative to the image. */
      line -= dev->line;

      offset = dev->image_end + line * dev->params.bytes_per_line;

      assert (offset <= (dev->image_size - dev->params.bytes_per_line));

      /* Copy the raster to the temporary image. */
      {
	int i;
	unsigned char *src = dev->buffer + raster * dev->raster_size;
	unsigned char *dest = dev->image + offset + color;

	for (i = 0; i < dev->raster_size; i++)
	  {
	    *dest = *src;
	    src++;
	    dest += 3;
	  }

	assert (dest <= (dev->image + dev->image_size + 2));

      }

      DBG (DBG_info, "raster=%d, line=%d, color=%d\n", dev->raster_num,
	   dev->line + line, color);

      if (color == COLOR_2)
	{
	  /* This raster completes a new line */
	  dev->line++;
	  dev->image_end += dev->params.bytes_per_line;
	}

      dev->raster_num++;
    }

  DBG (DBG_proc, "teco_adjust_raster: exit\n");
}

/* Read the image from the scanner and fill the temporary buffer with it. */
static SANE_Status
teco_fill_image (Teco_Scanner * dev)
{
  SANE_Status status;
  size_t size;
  CDB cdb;
  unsigned char *image;

  DBG (DBG_proc, "teco_fill_image: enter\n");

  assert (dev->image_begin == dev->image_end);
  assert (dev->real_bytes_left > 0);

  /* Copy the complete lines, plus the incompletes
   * ones. We don't keep the real end of data used
   * in image, so we copy the biggest possible. 
   */
  if (dev->scan_mode == TECO_COLOR)
    {
      memmove (dev->image, dev->image + dev->image_begin, dev->raster_ahead);
    }

  dev->image_begin = 0;
  dev->image_end = 0;

  while (dev->real_bytes_left)
    {

      /* todo: teco2 too */
      /* Check that we can at least one line. */
      if (dev->raster_ahead + dev->image_end + dev->params.bytes_per_line >
	  dev->image_size)
	{
	  /* Probably reached the end of the buffer. 
	   * Check, just in case. */
	  assert (dev->image_end != 0);
	  return (SANE_STATUS_GOOD);
	}

      /* 
       * Try to read the maximum number of bytes.
       */
      size = 0;
      while (size == 0)
	{
	  status = get_filled_data_length (dev, &size);
	  if (status)
	    return (status);
	  if (size == 0)
	    usleep (100000);	/* sleep 1/10th of second */
	}

      if (size > dev->real_bytes_left)
	size = dev->real_bytes_left;
      if (size > dev->image_size - dev->raster_ahead - dev->image_end)
	size = dev->image_size - dev->raster_ahead - dev->image_end;
      if (size > dev->buffer_size)
	{
	  size = dev->buffer_size;
	}

      /* Always read a multiple of a raster. */
      size = size - (size % dev->bytes_per_raster);

      if (size == 0)
	{
	  /* Probably reached the end of the buffer. 
	   * Check, just in case. */
	  assert (dev->image_end != 0);
	  return (SANE_STATUS_GOOD);
	}

      DBG (DBG_info, "teco_fill_image: to read   = %ld bytes (bpl=%d)\n",
	   (long) size, dev->params.bytes_per_line);

      MKSCSI_READ_10 (cdb, 0, 0, size);

      hexdump (DBG_info2, "teco_fill_image: READ_10 CDB", cdb.data, 10);
      DBG (DBG_info, "  image_end=%lu\n", (u_long) dev->image_end);

      if (dev->scan_mode == TECO_COLOR && dev->does_color_shift)
	{
	  image = dev->buffer;
	}
      else
	{
	  image = dev->image + dev->image_end;
	}

      status = sanei_scsi_cmd2 (dev->sfd, cdb.data, cdb.len,
				NULL, 0, image, &size);

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "teco_fill_image: cannot read from the scanner\n");
	  return status;
	}

      /* The size this scanner returns is always a multiple of a
       * raster size. */
      assert ((size % dev->bytes_per_raster) == 0);

      DBG (DBG_info, "teco_fill_image: real bytes left = %ld\n",
	   (long) dev->real_bytes_left);

      if (dev->scan_mode == TECO_COLOR && dev->does_color_shift)
	{
	  teco_adjust_raster (dev, size);
	}
      else
	{
	  /* Already in dev->image. */
	  dev->image_end += size;
	}

      dev->real_bytes_left -= size;
    }

  return (SANE_STATUS_GOOD);	/* unreachable */
}

/* Copy from the raw buffer to the buffer given by the backend. 
 *
 * len in input is the maximum length available in buf, and, in
 * output, is the length written into buf.
 */
static void
teco_copy_raw_to_frontend (Teco_Scanner * dev, SANE_Byte * buf, size_t * len)
{
  size_t size;

  size = dev->image_end - dev->image_begin;
  if (size > *len)
    {
      size = *len;
    }
  *len = size;

  switch (dev->scan_mode)
    {
    case TECO_BW:
      {
	/* For Black & White, the bits in every bytes are mirrored.
	 * for instance 11010001 is coded as 10001011 */

	unsigned char *src = dev->image + dev->image_begin;
	size_t i;
	unsigned char s;
	unsigned char d;

	for (i = 0; i < size; i++)
	  {
	    s = *src ^ 0xff;
	    d = 0;
	    if (s & 0x01)
	      d |= 0x80;
	    if (s & 0x02)
	      d |= 0x40;
	    if (s & 0x04)
	      d |= 0x20;
	    if (s & 0x08)
	      d |= 0x10;
	    if (s & 0x10)
	      d |= 0x08;
	    if (s & 0x20)
	      d |= 0x04;
	    if (s & 0x40)
	      d |= 0x02;
	    if (s & 0x80)
	      d |= 0x01;
	    *buf = d;
	    src++;
	    buf++;
	  }
      }
      break;

    case TECO_GRAYSCALE:
    case TECO_COLOR:
      memcpy (buf, dev->image + dev->image_begin, size);
      break;
    }

  dev->image_begin += size;
}

/* Stop a scan. */
static SANE_Status
do_cancel (Teco_Scanner * dev)
{
  DBG (DBG_sane_proc, "do_cancel enter\n");

  if (dev->scanning == SANE_TRUE)
    {
      teco_reset_window (dev);
      teco_close (dev);
    }

  dev->scanning = SANE_FALSE;

  DBG (DBG_sane_proc, "do_cancel exit\n");

  return SANE_STATUS_CANCELLED;
}

/*--------------------------------------------------------------------------*/

/* Sane entry points */

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback __sane_unused__ authorize)
{
  FILE *fp;
  char dev_name[PATH_MAX];
  size_t len;

  DBG_INIT ();

  DBG (DBG_sane_init, "sane_init\n");

  DBG (DBG_error, "This is sane-teco3 version %d.%d-%d\n", SANE_CURRENT_MAJOR,
       V_MINOR, BUILD);
  DBG (DBG_error, "(C) 2002 by Frank Zago\n");

  if (version_code)
    {
      *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);
    }

  fp = sanei_config_open (TECO_CONFIG_FILE);
  if (!fp)
    {
      /* default to /dev/scanner instead of insisting on config file */
      attach_scanner ("/dev/scanner", 0);
      return SANE_STATUS_GOOD;
    }

  while (sanei_config_read (dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')	/* ignore line comments */
	continue;
      len = strlen (dev_name);

      if (!len)
	continue;		/* ignore empty lines */

      sanei_config_attach_matching_devices (dev_name, attach_one);
    }

  fclose (fp);

  DBG (DBG_proc, "sane_init: leave\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool __sane_unused__ local_only)
{
  Teco_Scanner *dev;
  int i;

  DBG (DBG_proc, "sane_get_devices: enter\n");

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

  DBG (DBG_proc, "sane_get_devices: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  Teco_Scanner *dev;
  SANE_Status status;
  int i;

  DBG (DBG_proc, "sane_open: enter\n");

  /* search for devicename */
  if (devicename[0])
    {
      DBG (DBG_info, "sane_open: devicename=%s\n", devicename);

      for (dev = first_dev; dev; dev = dev->next)
	{
	  if (strcmp (dev->sane.name, devicename) == 0)
	    {
	      break;
	    }
	}

      if (!dev)
	{
	  status = attach_scanner (devicename, &dev);
	  if (status != SANE_STATUS_GOOD)
	    {
	      return status;
	    }
	}
    }
  else
    {
      DBG (DBG_sane_info, "sane_open: no devicename, opening first device\n");
      dev = first_dev;		/* empty devicename -> use first device */
    }

  if (!dev)
    {
      DBG (DBG_error, "No scanner found\n");

      return SANE_STATUS_INVAL;
    }

  teco_init_options (dev);

  /* Initialize the gamma table. */
  for (i = 0; i < GAMMA_LENGTH; i++)
    {
      dev->gamma_R[i] = i / 4;
      dev->gamma_G[i] = i / 4;
      dev->gamma_B[i] = i / 4;
      dev->gamma_GRAY[i] = i / 4;
    }

  *handle = dev;

  DBG (DBG_proc, "sane_open: exit\n");

  return SANE_STATUS_GOOD;
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Teco_Scanner *dev = handle;

  DBG (DBG_proc, "sane_get_option_descriptor: enter, option %d\n", option);

  if ((unsigned) option >= OPT_NUM_OPTIONS)
    {
      return NULL;
    }

  DBG (DBG_proc, "sane_get_option_descriptor: exit\n");

  return dev->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  Teco_Scanner *dev = handle;
  SANE_Status status;
  SANE_Word cap;
  SANE_String_Const name;

  DBG (DBG_proc, "sane_control_option: enter, option %d, action %d\n",
       option, action);

  if (info)
    {
      *info = 0;
    }

  if (dev->scanning)
    {
      return SANE_STATUS_DEVICE_BUSY;
    }

  if (option < 0 || option >= OPT_NUM_OPTIONS)
    {
      return SANE_STATUS_INVAL;
    }

  cap = dev->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      return SANE_STATUS_INVAL;
    }

  name = dev->opt[option].name;
  if (!name)
    {
      name = "(no name)";
    }
  if (action == SANE_ACTION_GET_VALUE)
    {

      switch (option)
	{
	  /* word options */
	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_TL_Y:
	case OPT_BR_Y:
	case OPT_TL_X:
	case OPT_BR_X:
	case OPT_CUSTOM_GAMMA:
	case OPT_THRESHOLD:
	case OPT_PREVIEW:
	  *(SANE_Word *) val = dev->val[option].w;
	  return SANE_STATUS_GOOD;

	  /* string options */
	case OPT_MODE:
	case OPT_DITHER:
	  strcpy (val, dev->val[option].s);
	  return SANE_STATUS_GOOD;

	  /* Gamma */
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	case OPT_GAMMA_VECTOR_GRAY:
	  memcpy (val, dev->val[option].wa, dev->opt[option].size);
	  return SANE_STATUS_GOOD;

	default:
	  return SANE_STATUS_INVAL;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {

      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (DBG_error, "could not set option, not settable\n");
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (dev->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "could not set option, invalid value\n");
	  return status;
	}

      switch (option)
	{

	  /* Numeric side-effect options */
	case OPT_TL_Y:
	case OPT_BR_Y:
	case OPT_TL_X:
	case OPT_BR_X:
	case OPT_THRESHOLD:
	case OPT_RESOLUTION:
	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_PARAMS;
	    }
	  dev->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	  /* Numeric side-effect free options */
	case OPT_PREVIEW:
	  dev->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	  /* String side-effect free options */
	case OPT_DITHER:
	  free (dev->val[option].s);
	  dev->val[option].s = (SANE_String) strdup (val);
	  return SANE_STATUS_GOOD;

	  /* String side-effect options */
	case OPT_MODE:
	  if (strcmp (dev->val[option].s, val) == 0)
	    return SANE_STATUS_GOOD;

	  free (dev->val[OPT_MODE].s);
	  dev->val[OPT_MODE].s = (SANE_Char *) strdup (val);

	  dev->opt[OPT_DITHER].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_GAMMA_VECTOR_GRAY].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;

	  if (strcmp (dev->val[OPT_MODE].s, BLACK_WHITE_STR) == 0)
	    {
	      dev->depth = 8;
	      dev->scan_mode = TECO_BW;
	      dev->opt[OPT_DITHER].cap &= ~SANE_CAP_INACTIVE;
	      dev->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else if (strcmp (dev->val[OPT_MODE].s, GRAY_STR) == 0)
	    {
	      dev->scan_mode = TECO_GRAYSCALE;
	      dev->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;
	      if (dev->val[OPT_CUSTOM_GAMMA].w)
		{
		  dev->opt[OPT_GAMMA_VECTOR_GRAY].cap &= ~SANE_CAP_INACTIVE;
		}
	      dev->depth = 8;
	    }
	  else if (strcmp (dev->val[OPT_MODE].s, COLOR_STR) == 0)
	    {
	      dev->scan_mode = TECO_COLOR;
	      dev->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;
	      if (dev->val[OPT_CUSTOM_GAMMA].w)
		{
		  dev->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		}
	      dev->depth = 8;
	    }

	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	    }
	  return SANE_STATUS_GOOD;

	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	case OPT_GAMMA_VECTOR_GRAY:
	  memcpy (dev->val[option].wa, val, dev->opt[option].size);
	  return SANE_STATUS_GOOD;

	case OPT_CUSTOM_GAMMA:
	  dev->val[OPT_CUSTOM_GAMMA].w = *(SANE_Word *) val;
	  if (dev->val[OPT_CUSTOM_GAMMA].w)
	    {
	      /* use custom_gamma_table */
	      if (dev->scan_mode == TECO_GRAYSCALE)
		{
		  dev->opt[OPT_GAMMA_VECTOR_GRAY].cap &= ~SANE_CAP_INACTIVE;
		}
	      else
		{
		  /* color mode */
		  dev->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		  dev->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		}
	    }
	  else
	    {
	      dev->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	      dev->opt[OPT_GAMMA_VECTOR_GRAY].cap |= SANE_CAP_INACTIVE;
	    }
	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_OPTIONS;
	    }
	  return SANE_STATUS_GOOD;

	default:
	  return SANE_STATUS_INVAL;
	}
    }

  DBG (DBG_proc, "sane_control_option: exit, bad\n");

  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Teco_Scanner *dev = handle;

  DBG (DBG_proc, "sane_get_parameters: enter\n");

  if (!(dev->scanning))
    {

      /* Setup the parameters for the scan. These values will be re-used
       * in the SET WINDOWS command. */
      if (dev->val[OPT_PREVIEW].w == SANE_TRUE)
	{
	  dev->x_resolution = 50;
	  dev->y_resolution = 50;
	  dev->x_tl = 0;
	  dev->y_tl = 0;
	  dev->x_br = mmToIlu (SANE_UNFIX (x_range.max));
	  dev->y_br = mmToIlu (SANE_UNFIX (y_range.max));
	}
      else
	{
	  dev->x_resolution = dev->val[OPT_RESOLUTION].w;
	  dev->y_resolution = dev->val[OPT_RESOLUTION].w;
	  if (dev->x_resolution > dev->def->x_resolution_max)
	    {
	      dev->x_resolution = dev->def->x_resolution_max;
	    }

	  dev->x_tl = mmToIlu (SANE_UNFIX (dev->val[OPT_TL_X].w));
	  dev->y_tl = mmToIlu (SANE_UNFIX (dev->val[OPT_TL_Y].w));
	  dev->x_br = mmToIlu (SANE_UNFIX (dev->val[OPT_BR_X].w));
	  dev->y_br = mmToIlu (SANE_UNFIX (dev->val[OPT_BR_Y].w));
	}

      /* Check the corners are OK. */
      if (dev->x_tl > dev->x_br)
	{
	  int s;
	  s = dev->x_tl;
	  dev->x_tl = dev->x_br;
	  dev->x_br = s;
	}
      if (dev->y_tl > dev->y_br)
	{
	  int s;
	  s = dev->y_tl;
	  dev->y_tl = dev->y_br;
	  dev->y_br = s;
	}

      dev->width = dev->x_br - dev->x_tl;
      dev->length = dev->y_br - dev->y_tl;

      /* Prepare the parameters for the caller. */
      memset (&dev->params, 0, sizeof (SANE_Parameters));

      dev->params.last_frame = SANE_TRUE;

      switch (dev->scan_mode)
	{
	case TECO_BW:
	  dev->params.format = SANE_FRAME_GRAY;
	  dev->params.pixels_per_line =
	    ((dev->width * dev->x_resolution) / 300) & ~0x7;
	  dev->params.bytes_per_line = dev->params.pixels_per_line / 8;
	  dev->params.depth = 1;
	  dev->color_shift = 0;
	  break;
	case TECO_GRAYSCALE:
	  dev->params.format = SANE_FRAME_GRAY;
	  dev->params.pixels_per_line =
	    ((dev->width * dev->x_resolution) / 300);
	  dev->params.bytes_per_line = dev->params.pixels_per_line;
	  dev->params.depth = 8;
	  dev->color_shift = 0;
	  break;
	case TECO_COLOR:
	  dev->params.format = SANE_FRAME_RGB;
	  dev->params.pixels_per_line =
	    ((dev->width * dev->x_resolution) / 300);
	  dev->params.bytes_per_line = dev->params.pixels_per_line * 3;
	  dev->params.depth = 8;

	  /* If the scanner does not have enough memory, it will
	   * send the raw rasters instead of returning a full
	   * interleaved line. Unfortunatly this does not work well,
	   * because I don't know how to compute the color
	   * shifting. So here is the result of some trial and error
	   * process. This is ignored if the scanner has a RAM
	   * module.
	   */
	  dev->color_shift = dev->x_resolution / 75;

	  break;
	}

      dev->params.lines = (dev->length * dev->y_resolution) / 300;
    }

  /* Return the current values. */
  if (params)
    {
      *params = (dev->params);
    }

  DBG (DBG_proc, "sane_get_parameters: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Teco_Scanner *dev = handle;
  SANE_Status status;
  size_t size;

  DBG (DBG_proc, "sane_start: enter\n");

  if (!(dev->scanning))
    {

      /* Open again the scanner. */
      if (sanei_scsi_open
	  (dev->devicename, &(dev->sfd), teco_sense_handler, dev) != 0)
	{
	  DBG (DBG_error, "ERROR: sane_start: open failed\n");
	  return SANE_STATUS_INVAL;
	}

      /* Set the correct parameters. */
      sane_get_parameters (dev, NULL);

      /* The scanner must be ready. */
      status = teco_wait_scanner (dev);
      if (status)
	{
	  teco_close (dev);
	  return status;
	}

      teco_query_sense (dev);
      teco_reset_window (dev);

      status = teco_set_window (dev);
      if (status)
	{
	  teco_close (dev);
	  return status;
	}

      dev->real_bytes_left = 0;
      status = get_filled_data_length (dev, &size);
      if (status)
	{
	  teco_close (dev);
	  return status;
	}

      /* Compute the length necessary in image. The first part will store
       * the complete lines, and the rest is used to stored ahead
       * rasters.
       * Align image_size to a multiple of lines. (important)
       */
      dev->raster_ahead =
	(2 * dev->color_shift + 1) * dev->params.bytes_per_line;
      dev->image_size = dev->buffer_size + dev->raster_ahead;
      dev->image_size =
	dev->image_size - (dev->image_size % dev->params.bytes_per_line);
      dev->image = malloc (dev->image_size);
      if (dev->image == NULL)
	{
	  return SANE_STATUS_NO_MEM;
	}

      /* Rasters are meaningfull only in color mode. */
      dev->raster_size = dev->params.pixels_per_line;
      dev->raster_real = dev->params.lines * 3;
      dev->raster_num = 0;
      dev->line = 0;

      teco_vendor_spec (dev);
      if (status)
	{
	  teco_close (dev);
	  return status;
	}

      status = teco_send_gamma (dev);
      if (status)
	{
	  teco_close (dev);
	  return status;
	}

      status = teco_set_window (dev);
      if (status)
	{
	  teco_close (dev);
	  return status;
	}

      status = teco_scan (dev);
      if (status)
	{
	  teco_close (dev);
	  return status;
	}
    }

  dev->image_end = 0;
  dev->image_begin = 0;

  dev->bytes_left = dev->params.bytes_per_line * dev->params.lines;
  dev->real_bytes_left = dev->params.bytes_per_line * dev->params.lines;

  dev->scanning = SANE_TRUE;

  DBG (DBG_proc, "sane_start: exit\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  SANE_Status status;
  Teco_Scanner *dev = handle;
  size_t size;
  int buf_offset;		/* offset into buf */

  DBG (DBG_proc, "sane_read: enter\n");

  *len = 0;

  if (!(dev->scanning))
    {
      /* OOPS, not scanning */
      return do_cancel (dev);
    }

  if (dev->bytes_left <= 0)
    {
      return (SANE_STATUS_EOF);
    }

  buf_offset = 0;

  do
    {
      if (dev->image_begin == dev->image_end)
	{
	  /* Fill image */
	  status = teco_fill_image (dev);
	  if (status != SANE_STATUS_GOOD)
	    {
	      return (status);
	    }
	}

      /* Something must have been read */
      if (dev->image_begin == dev->image_end)
	{
	  DBG (DBG_info, "sane_read: nothing read\n");
	  return SANE_STATUS_IO_ERROR;
	}

      /* Copy the data to the frontend buffer. */
      size = max_len - buf_offset;
      if (size > dev->bytes_left)
	{
	  size = dev->bytes_left;
	}
      teco_copy_raw_to_frontend (dev, buf + buf_offset, &size);

      buf_offset += size;

      dev->bytes_left -= size;
      *len += size;

    }
  while ((buf_offset != max_len) && dev->bytes_left);

  DBG (DBG_info, "sane_read: leave, bytes_left=%ld\n",
       (long) dev->bytes_left);

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_set_io_mode (SANE_Handle __sane_unused__ handle, SANE_Bool __sane_unused__ non_blocking)
{
  SANE_Status status;
  Teco_Scanner *dev = handle;

  DBG (DBG_proc, "sane_set_io_mode: enter\n");

  if (dev->scanning == SANE_FALSE)
    {
      return SANE_STATUS_INVAL;
    }

  if (non_blocking == SANE_FALSE)
    {
      status = SANE_STATUS_GOOD;
    }
  else
    {
      status = SANE_STATUS_UNSUPPORTED;
    }

  DBG (DBG_proc, "sane_set_io_mode: exit\n");

  return status;
}

SANE_Status
sane_get_select_fd (SANE_Handle __sane_unused__ handle, SANE_Int __sane_unused__ * fd)
{
  DBG (DBG_proc, "sane_get_select_fd: enter\n");

  DBG (DBG_proc, "sane_get_select_fd: exit\n");

  return SANE_STATUS_UNSUPPORTED;
}

void
sane_cancel (SANE_Handle handle)
{
  Teco_Scanner *dev = handle;

  DBG (DBG_proc, "sane_cancel: enter\n");

  do_cancel (dev);

  DBG (DBG_proc, "sane_cancel: exit\n");
}

void
sane_close (SANE_Handle handle)
{
  Teco_Scanner *dev = handle;
  Teco_Scanner *dev_tmp;

  DBG (DBG_proc, "sane_close: enter\n");

  do_cancel (dev);
  teco_close (dev);

  /* Unlink dev. */
  if (first_dev == dev)
    {
      first_dev = dev->next;
    }
  else
    {
      dev_tmp = first_dev;
      while (dev_tmp->next && dev_tmp->next != dev)
	{
	  dev_tmp = dev_tmp->next;
	}
      if (dev_tmp->next != NULL)
	{
	  dev_tmp->next = dev_tmp->next->next;
	}
    }

  teco_free (dev);
  num_devices--;

  DBG (DBG_proc, "sane_close: exit\n");
}

void
sane_exit (void)
{
  DBG (DBG_proc, "sane_exit: enter\n");

  while (first_dev)
    {
      sane_close (first_dev);
    }

  if (devlist)
    {
      free (devlist);
      devlist = NULL;
    }

  DBG (DBG_proc, "sane_exit: exit\n");
}
