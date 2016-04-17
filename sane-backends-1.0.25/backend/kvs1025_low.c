/*
   Copyright (C) 2008, Panasonic Russia Ltd.
*/
/* sane - Scanner Access Now Easy.
   Panasonic KV-S1020C / KV-S1025C USB scanners.
*/

#define DEBUG_DECLARE_ONLY

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
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/lassert.h"
#include "../include/sane/sanei_magic.h"

#include "kvs1025.h"
#include "kvs1025_low.h"
#include "kvs1025_usb.h"

#include "../include/sane/sanei_debug.h"

/* Global storage */

PKV_DEV g_devices = NULL;	/* Chain of devices */
const SANE_Device **g_devlist = NULL;

/* Static functions */

/* Free one device */
static void
kv_free (KV_DEV ** pdev)
{
  KV_DEV *dev;

  dev = *pdev;

  if (dev == NULL)
    return;

  DBG (DBG_proc, "kv_free : enter\n");

  kv_close (dev);

  DBG (DBG_proc, "kv_free : free image buffer 0 \n");
  if (dev->img_buffers[0])
    free (dev->img_buffers[0]);
  DBG (DBG_proc, "kv_free : free image buffer 1 \n");
  if (dev->img_buffers[1])
    free (dev->img_buffers[1]);
  DBG (DBG_proc, "kv_free : free scsi device name\n");
  if (dev->scsi_device_name)
    free (dev->scsi_device_name);

  DBG (DBG_proc, "kv_free : free SCSI buffer\n");
  if (dev->buffer0)
    free (dev->buffer0);

  DBG (DBG_proc, "kv_free : free dev \n");
  free (dev);

  *pdev = NULL;

  DBG (DBG_proc, "kv_free : exit\n");
}

/* Free all devices */
static void
kv_free_devices (void)
{
  PKV_DEV dev;
  while (g_devices)
    {
      dev = g_devices;
      g_devices = dev->next;
      kv_free (&dev);
    }
  if (g_devlist)
    {
      free (g_devlist);
      g_devlist = NULL;
    }
}

/* Get all supported scanners, and store into g_scanners_supported */
SANE_Status
kv_enum_devices (void)
{
  SANE_Status status;
  kv_free_devices ();
  status = kv_usb_enum_devices ();
  if (status)
    {
      kv_free_devices ();
    }

  return status;
}

/* Return devices list to the front end */
void
kv_get_devices_list (const SANE_Device *** devices_list)
{
  *devices_list = g_devlist;
}

/* Close all open handles and clean up global storage */
void
kv_exit (void)
{
  kv_free_devices ();		/* Free all devices */
  kv_usb_cleanup ();		/* Clean USB bus */
}

/* Open device by name */
SANE_Status
kv_open_by_name (SANE_String_Const devicename, SANE_Handle * handle)
{

  PKV_DEV pd = g_devices;
  DBG (DBG_proc, "sane_open: enter (dev_name=%s)\n", devicename);
  while (pd)
    {
      if (strcmp (pd->sane.name, devicename) == 0)
	{
	  if (kv_open (pd) == 0)
	    {
	      *handle = (SANE_Handle) pd;
	      DBG (DBG_proc, "sane_open: leave\n");
	      return SANE_STATUS_GOOD;
	    }
	}
      pd = pd->next;
    }
  DBG (DBG_proc, "sane_open: leave -- no device found\n");
  return SANE_STATUS_UNSUPPORTED;
}

/* Open a device */
SANE_Status
kv_open (PKV_DEV dev)
{
  SANE_Status status = SANE_STATUS_UNSUPPORTED;
  int i;
#define RETRAY_NUM 3


  if (dev->bus_mode == KV_USB_BUS)
    {
      status = kv_usb_open (dev);
    }
  if (status)
    return status;
  for (i = 0; i < RETRAY_NUM; i++)
    {
      SANE_Bool dev_ready;
      status = CMD_test_unit_ready (dev, &dev_ready);
      if (!status && dev_ready)
	break;
    }

  if (status == 0)
    {
      /* Read device support info */
      status = CMD_read_support_info (dev);

      if (status == 0)
	{
	  /* Init options */
	  kv_init_options (dev);
	  status = CMD_set_timeout (dev, dev->val[OPT_FEED_TIMEOUT].w);
	}
    }
  dev->scanning = 0;
  return status;
}

/* Check if device is already open */

SANE_Bool
kv_already_open (PKV_DEV dev)
{
  SANE_Bool status = 0;

  if (dev->bus_mode == KV_USB_BUS)
    {
      status = kv_usb_already_open (dev);
    }

  return status;
}

/* Close a device */
void
kv_close (PKV_DEV dev)
{
  if (dev->bus_mode == KV_USB_BUS)
    {
      kv_usb_close (dev);
    }
  dev->scanning = 0;
}

/* Send command to a device */
SANE_Status
kv_send_command (PKV_DEV dev,
		 PKV_CMD_HEADER header, PKV_CMD_RESPONSE response)
{
  SANE_Status status = SANE_STATUS_UNSUPPORTED;
  if (dev->bus_mode == KV_USB_BUS)
    {
      if (!kv_usb_already_open(dev))
	{
	  DBG (DBG_error, "kv_send_command error: device not open.\n");
	  return SANE_STATUS_IO_ERROR;
	}

      status = kv_usb_send_command (dev, header, response);
    }

  return status;
}

/* Commands */

SANE_Status
CMD_test_unit_ready (PKV_DEV dev, SANE_Bool * ready)
{
  SANE_Status status;
  KV_CMD_HEADER hdr;
  KV_CMD_RESPONSE rs;

  DBG (DBG_proc, "CMD_test_unit_ready\n");

  memset (&hdr, 0, sizeof (hdr));

  hdr.direction = KV_CMD_NONE;
  hdr.cdb[0] = SCSI_TEST_UNIT_READY;
  hdr.cdb_size = 6;

  status = kv_send_command (dev, &hdr, &rs);

  if (status == 0)
    {
      *ready = (rs.status == KV_SUCCESS ? 1 : 0);
    }

  return status;
}

SANE_Status
CMD_set_timeout (PKV_DEV dev, SANE_Word timeout)
{
  SANE_Status status;
  KV_CMD_HEADER hdr;
  KV_CMD_RESPONSE rs;

  DBG (DBG_proc, "CMD_set_timeout\n");

  memset (&hdr, 0, sizeof (hdr));

  hdr.direction = KV_CMD_OUT;
  hdr.cdb[0] = SCSI_SET_TIMEOUT;
  hdr.cdb[2] = 0x8D;
  hdr.cdb[8] = 0x2;
  hdr.cdb_size = 10;
  hdr.data = dev->buffer;
  dev->buffer[0] = 0;
  dev->buffer[1] = (SANE_Byte) timeout;
  hdr.data_size = 2;

  status = kv_send_command (dev, &hdr, &rs);

  return status;
}

SANE_Status
CMD_read_support_info (PKV_DEV dev)
{
  SANE_Status status;
  KV_CMD_HEADER hdr;
  KV_CMD_RESPONSE rs;

  DBG (DBG_proc, "CMD_read_support_info\n");

  memset (&hdr, 0, sizeof (hdr));

  hdr.direction = KV_CMD_IN;
  hdr.cdb_size = 10;
  hdr.cdb[0] = SCSI_READ_10;
  hdr.cdb[2] = 0x93;
  Ito24 (32, &hdr.cdb[6]);
  hdr.data = dev->buffer;
  hdr.data_size = 32;

  status = kv_send_command (dev, &hdr, &rs);

  DBG (DBG_error, "test.\n");

  if (status == 0)
    {
      if (rs.status == 0)
	{
	  int min_x_res, min_y_res, max_x_res, max_y_res;
	  int step_x_res, step_y_res;

	  dev->support_info.memory_size
	    = (dev->buffer[2] << 8 | dev->buffer[3]);
	  min_x_res = (dev->buffer[4] << 8) | dev->buffer[5];
	  min_y_res = (dev->buffer[6] << 8) | dev->buffer[7];
	  max_x_res = (dev->buffer[8] << 8) | dev->buffer[9];
	  max_y_res = (dev->buffer[10] << 8) | dev->buffer[11];
	  step_x_res = (dev->buffer[12] << 8) | dev->buffer[13];
	  step_y_res = (dev->buffer[14] << 8) | dev->buffer[15];

	  dev->support_info.min_resolution =
	    min_x_res > min_y_res ? min_x_res : min_y_res;
	  dev->support_info.max_resolution =
	    max_x_res < max_y_res ? max_x_res : max_y_res;
	  dev->support_info.step_resolution =
	    step_x_res > step_y_res ? step_x_res : step_y_res;
	  dev->support_info.support_duplex =
	    ((dev->buffer[0] & 0x08) == 0) ? 1 : 0;
	  dev->support_info.support_lamp =
	    ((dev->buffer[23] & 0x80) != 0) ? 1 : 0;

	  dev->support_info.max_x_range = KV_MAX_X_RANGE;
	  dev->support_info.max_y_range = KV_MAX_Y_RANGE;

	  dev->x_range.min = dev->y_range.min = 0;
	  dev->x_range.max = SANE_FIX (dev->support_info.max_x_range);
	  dev->y_range.max = SANE_FIX (dev->support_info.max_y_range);
	  dev->x_range.quant = dev->y_range.quant = 0;

	  DBG (DBG_error,
	       "support_info.memory_size = %d (MB)\n",
	       dev->support_info.memory_size);
	  DBG (DBG_error,
	       "support_info.min_resolution = %d (DPI)\n",
	       dev->support_info.min_resolution);
	  DBG (DBG_error,
	       "support_info.max_resolution = %d (DPI)\n",
	       dev->support_info.max_resolution);
	  DBG (DBG_error,
	       "support_info.step_resolution = %d (DPI)\n",
	       dev->support_info.step_resolution);
	  DBG (DBG_error,
	       "support_info.support_duplex = %s\n",
	       dev->support_info.support_duplex ? "TRUE" : "FALSE");
	  DBG (DBG_error, "support_info.support_lamp = %s\n",
	       dev->support_info.support_lamp ? "TRUE" : "FALSE");
	}
      else
	{
	  DBG (DBG_error, "Error in CMD_get_support_info, "
	       "sense_key=%d, ASC=%d, ASCQ=%d\n",
	       get_RS_sense_key (rs.sense),
	       get_RS_ASC (rs.sense), get_RS_ASCQ (rs.sense));

	}
    }

  return status;
}

SANE_Status
CMD_scan (PKV_DEV dev)
{
  SANE_Status status;
  KV_CMD_HEADER hdr;
  KV_CMD_RESPONSE rs;

  DBG (DBG_proc, "CMD_scan\n");

  memset (&hdr, 0, sizeof (hdr));

  hdr.direction = KV_CMD_NONE;
  hdr.cdb[0] = SCSI_SCAN;
  hdr.cdb_size = 6;

  status = kv_send_command (dev, &hdr, &rs);

  if (status == 0 && rs.status != 0)
    {
      DBG (DBG_error,
	   "Error in CMD_scan, sense_key=%d, ASC=%d, ASCQ=%d\n",
	   get_RS_sense_key (rs.sense), get_RS_ASC (rs.sense),
	   get_RS_ASCQ (rs.sense));
    }

  return status;
}

SANE_Status
CMD_set_window (PKV_DEV dev, int side, PKV_CMD_RESPONSE rs)
{
  unsigned char *window;
  unsigned char *windowdata;
  int size = 74;
  KV_SCAN_MODE scan_mode;
  KV_CMD_HEADER hdr;

  DBG (DBG_proc, "CMD_set_window\n");

  window = (unsigned char *) dev->buffer;
  windowdata = window + 8;

  memset (&hdr, 0, sizeof (hdr));
  memset (window, 0, size);

  Ito16 (66, &window[6]);	/* Window descriptor block length */

  /* Set window data */

  scan_mode = kv_get_mode (dev);

  kv_set_window_data (dev, scan_mode, side, windowdata);

  hdr.direction = KV_CMD_OUT;
  hdr.cdb_size = 10;
  hdr.cdb[0] = SCSI_SET_WINDOW;
  Ito24 (size, &hdr.cdb[6]);
  hdr.data = window;
  hdr.data_size = size;

  hexdump (DBG_error, "window", window, size);

  return kv_send_command (dev, &hdr, rs);
}

SANE_Status
CMD_reset_window (PKV_DEV dev)
{
  KV_CMD_HEADER hdr;
  KV_CMD_RESPONSE rs;
  SANE_Status status;

  DBG (DBG_proc, "CMD_reset_window\n");

  memset (&hdr, 0, sizeof (hdr));

  hdr.direction = KV_CMD_NONE;
  hdr.cdb_size = 10;
  hdr.cdb[0] = SCSI_SET_WINDOW;

  status = kv_send_command (dev, &hdr, &rs);
  if (rs.status != 0)
    status = SANE_STATUS_INVAL;

  return status;
}

SANE_Status
CMD_get_buff_status (PKV_DEV dev, int *front_size, int *back_size)
{
  KV_CMD_HEADER hdr;
  KV_CMD_RESPONSE rs;
  SANE_Status status;
  unsigned char *data = (unsigned char *) dev->buffer;
  int size = 12;
  memset (&hdr, 0, sizeof (hdr));
  memset (data, 0, size);

  hdr.direction = KV_CMD_IN;
  hdr.cdb_size = 10;
  hdr.cdb[0] = SCSI_GET_BUFFER_STATUS;
  hdr.cdb[8] = size;
  hdr.data = data;
  hdr.data_size = size;

  status = kv_send_command (dev, &hdr, &rs);
  if (status == 0)
    {
      if (rs.status == KV_CHK_CONDITION)
	return SANE_STATUS_NO_DOCS;
      else
	{
	  unsigned char *p = data + 4;
	  if (p[0] == SIDE_FRONT)
	    {
	      *front_size = (p[5] << 16) | (p[6] << 8) | p[7];
	    }
	  else
	    {
	      *back_size = (p[5] << 16) | (p[6] << 8) | p[7];
	    }
	  return SANE_STATUS_GOOD;
	}
    }
  return status;
}

SANE_Status
CMD_wait_buff_status (PKV_DEV dev, int *front_size, int *back_size)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int cnt = 0;
  *front_size = 0;
  *back_size = 0;

  DBG (DBG_proc, "CMD_wait_buff_status: enter feed %s\n",
       dev->val[OPT_MANUALFEED].s);

  do
    {
      DBG (DBG_proc, "CMD_wait_buff_status: tray #%d of %d\n", cnt,
	   dev->val[OPT_FEED_TIMEOUT].w);
      status = CMD_get_buff_status (dev, front_size, back_size);
      sleep (1);
    }
  while (status == SANE_STATUS_GOOD && (*front_size == 0)
	 && (*back_size == 0) && cnt++ < dev->val[OPT_FEED_TIMEOUT].w);

  if (cnt > dev->val[OPT_FEED_TIMEOUT].w)
    status = SANE_STATUS_NO_DOCS;

  if (status == 0)
    DBG (DBG_proc, "CMD_wait_buff_status: exit "
	 "front_size %d, back_size %d\n", *front_size, *back_size);
  else
    DBG (DBG_proc, "CMD_wait_buff_status: exit with no docs\n");
  return status;
}


SANE_Status
CMD_read_pic_elements (PKV_DEV dev, int page, int side,
		       int *width, int *height)
{
  SANE_Status status;
  KV_CMD_HEADER hdr;
  KV_CMD_RESPONSE rs;

  DBG (DBG_proc, "CMD_read_pic_elements\n");

  memset (&hdr, 0, sizeof (hdr));

  hdr.direction = KV_CMD_IN;
  hdr.cdb_size = 10;
  hdr.cdb[0] = SCSI_READ_10;
  hdr.cdb[2] = 0x80;
  hdr.cdb[4] = page;
  hdr.cdb[5] = side;
  Ito24 (16, &hdr.cdb[6]);
  hdr.data = dev->buffer;
  hdr.data_size = 16;

  status = kv_send_command (dev, &hdr, &rs);
  if (status == 0)
    {
      if (rs.status == 0)
	{
	  int s = side == SIDE_FRONT ? 0 : 1;
	  int depth = kv_get_depth (kv_get_mode (dev));
	  *width = B32TOI (dev->buffer);
	  *height = B32TOI (&dev->buffer[4]);

	  assert ((*width) % 8 == 0);

	  DBG (DBG_proc, "CMD_read_pic_elements: "
	       "Page %d, Side %s, W=%d, H=%d\n",
	       page, side == SIDE_FRONT ? "F" : "B", *width, *height);

	  dev->params[s].format = kv_get_mode (dev) == SM_COLOR ?
	    SANE_FRAME_RGB : SANE_FRAME_GRAY;
	  dev->params[s].last_frame = SANE_TRUE;
	  dev->params[s].depth = depth > 8 ? 8 : depth;
	  dev->params[s].lines = *height ? *height
	    : dev->val[OPT_LANDSCAPE].w ? (*width * 3) / 4 : (*width * 4) / 3;
	  dev->params[s].pixels_per_line = *width;
	  dev->params[s].bytes_per_line =
	    (dev->params[s].pixels_per_line / 8) * depth;
	}
      else
	{
	  DBG (DBG_proc, "CMD_read_pic_elements: failed\n");
	  status = SANE_STATUS_INVAL;
	}
    }

  return status;
}

SANE_Status
CMD_read_image (PKV_DEV dev, int page, int side,
		unsigned char *buffer, int *psize, KV_CMD_RESPONSE * rs)
{
  SANE_Status status;
  KV_CMD_HEADER hdr;
  int size = *psize;

  DBG (DBG_proc, "CMD_read_image\n");

  memset (&hdr, 0, sizeof (hdr));

  hdr.direction = KV_CMD_IN;
  hdr.cdb_size = 10;
  hdr.cdb[0] = SCSI_READ_10;
  hdr.cdb[4] = page;
  hdr.cdb[5] = side;
  Ito24 (size, &hdr.cdb[6]);
  hdr.data = buffer;
  hdr.data_size = size;

  *psize = 0;

  status = kv_send_command (dev, &hdr, rs);

  if (status)
    return status;

  *psize = size;

  if (rs->status == KV_CHK_CONDITION && get_RS_ILI (rs->sense))
    {
      int delta = B32TOI (&rs->sense[3]);
      DBG (DBG_error, "size=%d, delta=0x%x (%d)\n", size, delta, delta);
      *psize = size - delta;
    }

  DBG (DBG_error, "CMD_read_image: bytes requested=%d, read=%d\n",
       size, *psize);
  DBG (DBG_error, "CMD_read_image: ILI=%d, EOM=%d\n",
       get_RS_ILI (rs->sense), get_RS_EOM (rs->sense));

  return status;
}

SANE_Status
CMD_get_document_existanse (PKV_DEV dev)
{
  SANE_Status status;
  KV_CMD_HEADER hdr;
  KV_CMD_RESPONSE rs;

  DBG (DBG_proc, "CMD_get_document_existanse\n");

  memset (&hdr, 0, sizeof (hdr));

  hdr.direction = KV_CMD_IN;
  hdr.cdb_size = 10;
  hdr.cdb[0] = SCSI_READ_10;
  hdr.cdb[2] = 0x81;
  Ito24 (6, &hdr.cdb[6]);
  hdr.data = dev->buffer;
  hdr.data_size = 6;

  status = kv_send_command (dev, &hdr, &rs);
  if (status)
    return status;
  if (rs.status)
    return SANE_STATUS_NO_DOCS;
  if ((dev->buffer[0] & 0x20) != 0)
    {
      return SANE_STATUS_GOOD;
    }

  return SANE_STATUS_NO_DOCS;
}

SANE_Status
CMD_wait_document_existanse (PKV_DEV dev)
{
  SANE_Status status;
  KV_CMD_HEADER hdr;
  KV_CMD_RESPONSE rs;
  int cnt;

  DBG (DBG_proc, "CMD_wait_document_existanse\n");

  memset (&hdr, 0, sizeof (hdr));

  hdr.direction = KV_CMD_IN;
  hdr.cdb_size = 10;
  hdr.cdb[0] = SCSI_READ_10;
  hdr.cdb[2] = 0x81;
  Ito24 (6, &hdr.cdb[6]);
  hdr.data = dev->buffer;
  hdr.data_size = 6;

  for (cnt = 0; cnt < dev->val[OPT_FEED_TIMEOUT].w; cnt++)
    {
      DBG (DBG_proc, "CMD_wait_document_existanse: tray #%d of %d\n", cnt,
	   dev->val[OPT_FEED_TIMEOUT].w);
      status = kv_send_command (dev, &hdr, &rs);
      if (status)
	return status;
      if (rs.status)
	return SANE_STATUS_NO_DOCS;
      if ((dev->buffer[0] & 0x20) != 0)
	{
	  return SANE_STATUS_GOOD;
	}
      else if (strcmp (dev->val[OPT_MANUALFEED].s, "off") == 0)
	{
	  return SANE_STATUS_NO_DOCS;
	}
      sleep (1);
    }

  return SANE_STATUS_NO_DOCS;
}

SANE_Status
CMD_request_sense (PKV_DEV dev)
{
  KV_CMD_HEADER hdr;
  KV_CMD_RESPONSE rs;

  DBG (DBG_proc, "CMD_request_sense\n");
  memset (&hdr, 0, sizeof (hdr));
  hdr.direction = KV_CMD_IN;
  hdr.cdb[0] = SCSI_REQUEST_SENSE;
  hdr.cdb[4] = 0x12;
  hdr.cdb_size = 6;
  hdr.data_size = 0x12;
  hdr.data = dev->buffer;

  return kv_send_command (dev, &hdr, &rs);
}

/* Scan routines */

/* Allocate image buffer for one page (1 or 2 sides) */

SANE_Status
AllocateImageBuffer (PKV_DEV dev)
{
  int *size = dev->bytes_to_read;
  int sides = IS_DUPLEX (dev) ? 2 : 1;
  int i;
  size[0] = dev->params[0].bytes_per_line * dev->params[0].lines;
  size[1] = dev->params[1].bytes_per_line * dev->params[1].lines;

  DBG (DBG_proc, "AllocateImageBuffer: enter\n");

  for (i = 0; i < sides; i++)
    {
      SANE_Byte *p;
      DBG (DBG_proc, "AllocateImageBuffer: size(%c)=%d\n",
	   i ? 'B' : 'F', size[i]);

      if (dev->img_buffers[i] == NULL)
	{
	  p = (SANE_Byte *) malloc (size[i]);
	  if (p == NULL)
	    {
	      return SANE_STATUS_NO_MEM;
	    }
	  dev->img_buffers[i] = p;
	}
      else
	{
	  p = (SANE_Byte *) realloc (dev->img_buffers[i], size[i]);
	  if (p == NULL)
	    {
	      return SANE_STATUS_NO_MEM;
	    }
	  else
	    {
	      dev->img_buffers[i] = p;
	    }
	}
    }
  DBG (DBG_proc, "AllocateImageBuffer: exit\n");

  return SANE_STATUS_GOOD;
}

/* Read image data from scanner dev->img_buffers[0],
   for the simplex page */
SANE_Status
ReadImageDataSimplex (PKV_DEV dev, int page)
{
  int bytes_to_read = dev->bytes_to_read[0];
  SANE_Byte *buffer = (SANE_Byte *) dev->buffer;
  int buff_size = SCSI_BUFFER_SIZE;
  SANE_Byte *pt = dev->img_buffers[0];
  KV_CMD_RESPONSE rs;
  dev->img_size[0] = 0;
  dev->img_size[1] = 0;

  /* read loop */
  do
    {
      int size = buff_size;
      SANE_Status status;
      DBG (DBG_error, "Bytes left = %d\n", bytes_to_read);
      status = CMD_read_image (dev, page, SIDE_FRONT, buffer, &size, &rs);
      if (status)
	{
	  return status;
	}
      if (rs.status)
	{
	  if (get_RS_sense_key (rs.sense))
	    {
	      DBG (DBG_error, "Error reading image data, "
		   "sense_key=%d, ASC=%d, ASCQ=%d",
		   get_RS_sense_key (rs.sense),
		   get_RS_ASC (rs.sense), get_RS_ASCQ (rs.sense));

	      if (get_RS_sense_key (rs.sense) == 3)
		{
		  if (!get_RS_ASCQ (rs.sense))
		    return SANE_STATUS_NO_DOCS;
		  return SANE_STATUS_JAMMED;
		}
	      return SANE_STATUS_IO_ERROR;
	    }

	}
      /* copy data to image buffer */
      if (size > bytes_to_read)
	{
	  size = bytes_to_read;
	}
      if (size > 0)
	{
	  memcpy (pt, buffer, size);
	  bytes_to_read -= size;
	  pt += size;
	  dev->img_size[0] += size;
	}
    }
  while (!get_RS_EOM (rs.sense));

  assert (pt == dev->img_buffers[0] + dev->img_size[0]);
  DBG (DBG_error, "Image size = %d\n", dev->img_size[0]);
  return SANE_STATUS_GOOD;
}

/* Read image data from scanner dev->img_buffers[0],
   for the duplex page */
SANE_Status
ReadImageDataDuplex (PKV_DEV dev, int page)
{
  int bytes_to_read[2];
  SANE_Byte *buffer = (SANE_Byte *) dev->buffer;
  int buff_size[2];
  SANE_Byte *pt[2];
  KV_CMD_RESPONSE rs;
  int sides[2];
  SANE_Bool eoms[2];
  int current_side = 1;

  bytes_to_read[0] = dev->bytes_to_read[0];
  bytes_to_read[1] = dev->bytes_to_read[1];

  pt[0] = dev->img_buffers[0];
  pt[1] = dev->img_buffers[1];

  sides[0] = SIDE_FRONT;
  sides[1] = SIDE_BACK;
  eoms[0] = eoms[1] = 0;

  buff_size[0] = SCSI_BUFFER_SIZE;
  buff_size[1] = SCSI_BUFFER_SIZE;
  dev->img_size[0] = 0;
  dev->img_size[1] = 0;

  /* read loop */
  do
    {
      int size = buff_size[current_side];
      SANE_Status status;
      DBG (DBG_error, "Bytes left (F) = %d\n", bytes_to_read[0]);
      DBG (DBG_error, "Bytes left (B) = %d\n", bytes_to_read[1]);

      status = CMD_read_image (dev, page, sides[current_side],
			       buffer, &size, &rs);
      if (status)
	{
	  return status;
	}
      if (rs.status)
	{
	  if (get_RS_sense_key (rs.sense))
	    {
	      DBG (DBG_error, "Error reading image data, "
		   "sense_key=%d, ASC=%d, ASCQ=%d",
		   get_RS_sense_key (rs.sense),
		   get_RS_ASC (rs.sense), get_RS_ASCQ (rs.sense));

	      if (get_RS_sense_key (rs.sense) == 3)
		{
		  if (!get_RS_ASCQ (rs.sense))
		    return SANE_STATUS_NO_DOCS;
		  return SANE_STATUS_JAMMED;
		}
	      return SANE_STATUS_IO_ERROR;
	    }
	}

      /* copy data to image buffer */
      if (size > bytes_to_read[current_side])
	{
	  size = bytes_to_read[current_side];
	}
      if (size > 0)
	{
	  memcpy (pt[current_side], buffer, size);
	  bytes_to_read[current_side] -= size;
	  pt[current_side] += size;
	  dev->img_size[current_side] += size;
	}
      if (rs.status)
	{
	  if (get_RS_EOM (rs.sense))
	    {
	      eoms[current_side] = 1;
	    }
	  if (get_RS_ILI (rs.sense))
	    {
	      current_side++;
	      current_side &= 1;
	    }
	}
    }
  while (eoms[0] == 0 || eoms[1] == 0);

  DBG (DBG_error, "Image size (F) = %d\n", dev->img_size[0]);
  DBG (DBG_error, "Image size (B) = %d\n", dev->img_size[1]);

  assert (pt[0] == dev->img_buffers[0] + dev->img_size[0]);
  assert (pt[1] == dev->img_buffers[1] + dev->img_size[1]);

  return SANE_STATUS_GOOD;
}

/* Read image data for one page */
SANE_Status
ReadImageData (PKV_DEV dev, int page)
{
  SANE_Status status;
  DBG (DBG_proc, "Reading image data for page %d\n", page);

  if (IS_DUPLEX (dev))
    {
      DBG (DBG_proc, "ReadImageData: Duplex %d\n", page);
      status = ReadImageDataDuplex (dev, page);
    }
  else
    {
      DBG (DBG_proc, "ReadImageData: Simplex %d\n", page);
      status = ReadImageDataSimplex (dev, page);
    }
  dev->img_pt[0] = dev->img_buffers[0];
  dev->img_pt[1] = dev->img_buffers[1];

  DBG (DBG_proc, "Reading image data for page %d, finished\n", page);

  return status;
}

/* Look in image for likely upper and left paper edges, then rotate
 * image so that upper left corner of paper is upper left of image.
 * FIXME: should we do this before we binarize instead of after? */
SANE_Status
buffer_deskew(PKV_DEV s, int side)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int bg_color = 0xd6;
  int side_index = (side == SIDE_FRONT)?0:1;
  int resolution = s->val[OPT_RESOLUTION].w;

  DBG (10, "buffer_deskew: start\n");

  /*only find skew on first image from a page, or if first image had error */
  if(side == SIDE_FRONT || s->deskew_stat){

    s->deskew_stat = sanei_magic_findSkew(
      &s->params[side_index],s->img_buffers[side_index],
      resolution,resolution,
      &s->deskew_vals[0],&s->deskew_vals[1],&s->deskew_slope);
  
    if(s->deskew_stat){
      DBG (5, "buffer_despeck: bad findSkew, bailing\n");
      goto cleanup;
    }
  }
  /* backside images can use a 'flipped' version of frontside data */
  else{
    s->deskew_slope *= -1;
    s->deskew_vals[0] 
      = s->params[side_index].pixels_per_line - s->deskew_vals[0];
  }

  ret = sanei_magic_rotate(&s->params[side_index],s->img_buffers[side_index],
    s->deskew_vals[0],s->deskew_vals[1],s->deskew_slope,bg_color);

  if(ret){
    DBG(5,"buffer_deskew: rotate error: %d",ret);
    ret = SANE_STATUS_GOOD;
    goto cleanup;
  }

  cleanup:
  DBG (10, "buffer_deskew: finish\n");
  return ret;
}

/* Look in image for likely left/right/bottom paper edges, then crop image.
 * Does not attempt to rotate the image, that should be done first.
 * FIXME: should we do this before we binarize instead of after? */
SANE_Status
buffer_crop(PKV_DEV s, int side)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int side_index = (side == SIDE_FRONT)?0:1;
  int resolution = s->val[OPT_RESOLUTION].w;

  DBG (10, "buffer_crop: start\n");

  /*only find edges on first image from a page, or if first image had error */
  if(side == SIDE_FRONT || s->crop_stat){

    s->crop_stat = sanei_magic_findEdges(
      &s->params[side_index],s->img_buffers[side_index],
      resolution,resolution,
      &s->crop_vals[0],&s->crop_vals[1],&s->crop_vals[2],&s->crop_vals[3]);

    if(s->crop_stat){
      DBG (5, "buffer_crop: bad edges, bailing\n");
      goto cleanup;
    }
  
    DBG (15, "buffer_crop: t:%d b:%d l:%d r:%d\n",
      s->crop_vals[0],s->crop_vals[1],s->crop_vals[2],s->crop_vals[3]);

    /* we dont listen to the 'top' value, since the top is not padded */
    /*s->crop_vals[0] = 0;*/
  }
  /* backside images can use a 'flipped' version of frontside data */
  else{
    int left  = s->crop_vals[2];
    int right = s->crop_vals[3];

    s->crop_vals[2] = s->params[side_index].pixels_per_line - right;
    s->crop_vals[3] = s->params[side_index].pixels_per_line - left;
  }

  /* now crop the image */
  ret = sanei_magic_crop(&s->params[side_index],s->img_buffers[side_index],
      s->crop_vals[0],s->crop_vals[1],s->crop_vals[2],s->crop_vals[3]);

  if(ret){
    DBG (5, "buffer_crop: bad crop, bailing\n");
    ret = SANE_STATUS_GOOD;
    goto cleanup;
  }

  /* update image size counter to new, smaller size */
  s->img_size[side_index] 
    = s->params[side_index].lines * s->params[side_index].bytes_per_line;
 
  cleanup:
  DBG (10, "buffer_crop: finish\n");
  return ret;
}

/* Look in image for disconnected 'spots' of the requested size.
 * Replace the spots with the average color of the surrounding pixels.
 * FIXME: should we do this before we binarize instead of after? */
SANE_Status
buffer_despeck(PKV_DEV s, int side)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int side_index = (side == SIDE_FRONT)?0:1;

  DBG (10, "buffer_despeck: start\n");

  ret = sanei_magic_despeck(
    &s->params[side_index],s->img_buffers[side_index],s->val[OPT_SWDESPECK].w
  );
  if(ret){
    DBG (5, "buffer_despeck: bad despeck, bailing\n");
    ret = SANE_STATUS_GOOD;
    goto cleanup;
  }

  cleanup:
  DBG (10, "buffer_despeck: finish\n");
  return ret;
}

/* Look if image has too few dark pixels.
 * FIXME: should we do this before we binarize instead of after? */
int
buffer_isblank(PKV_DEV s, int side)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int side_index = (side == SIDE_FRONT)?0:1;
  int status = 0;

  DBG (10, "buffer_isblank: start\n");

  ret = sanei_magic_isBlank(
    &s->params[side_index],s->img_buffers[side_index],
    SANE_UNFIX(s->val[OPT_SWSKIP].w)
  );

  if(ret == SANE_STATUS_NO_DOCS){
    DBG (5, "buffer_isblank: blank!\n");
    status = 1;
  }
  else if(ret){
    DBG (5, "buffer_isblank: error %d\n",ret);
  }

  DBG (10, "buffer_isblank: finished\n");
  return status;
}

/* Look if image needs rotation
 * FIXME: should we do this before we binarize instead of after? */
SANE_Status
buffer_rotate(PKV_DEV s, int side)
{
  SANE_Status ret = SANE_STATUS_GOOD;
  int angle = 0;
  int side_index = (side == SIDE_FRONT)?0:1;
  int resolution = s->val[OPT_RESOLUTION].w;

  DBG (10, "buffer_rotate: start\n");

  if(s->val[OPT_SWDEROTATE].w){
    ret = sanei_magic_findTurn(
      &s->params[side_index],s->img_buffers[side_index],
      resolution,resolution,&angle);
  
    if(ret){
      DBG (5, "buffer_rotate: error %d\n",ret);
      ret = SANE_STATUS_GOOD;
      goto cleanup;
    }
  }

  angle += s->val[OPT_ROTATE].w;

  /*90 or 270 degree rotations are reversed on back side*/
  if(side == SIDE_BACK && s->val[OPT_ROTATE].w % 180){
    angle += 180;
  }

  ret = sanei_magic_turn(
    &s->params[side_index],s->img_buffers[side_index],
    angle);
  
  if(ret){
    DBG (5, "buffer_rotate: error %d\n",ret);
    ret = SANE_STATUS_GOOD;
    goto cleanup;
  }

  /* update image size counter to new, smaller size */
  s->img_size[side_index] 
    = s->params[side_index].lines * s->params[side_index].bytes_per_line;
 
  cleanup:
  DBG (10, "buffer_rotate: finished\n");
  return ret;
}
