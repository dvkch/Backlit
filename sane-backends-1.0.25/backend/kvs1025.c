/*
   Copyright (C) 2008, Panasonic Russia Ltd.
   Copyright (C) 2010-2011, m. allan noah
*/
/* sane - Scanner Access Now Easy.
   Panasonic KV-S1020C / KV-S1025C USB scanners.
*/

#define DEBUG_NOT_STATIC

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

#include "kvs1025.h"
#include "kvs1025_low.h"

#include "../include/sane/sanei_debug.h"

/* SANE backend operations, see Sane standard 1.04 documents (sane_dev.pdf)
   for details */

/* Init the KV-S1025 SANE backend. This function must be called before any other
   SANE function can be called. */
SANE_Status
sane_init (SANE_Int * version_code,
	   SANE_Auth_Callback __sane_unused__ authorize)
{
  SANE_Status status;

  DBG_INIT ();

  DBG (DBG_sane_init, "sane_init\n");

  DBG (DBG_error,
       "This is panasonic KV-S1020C / KV-S1025C version %d.%d build %d\n",
       V_MAJOR, V_MINOR, V_BUILD);

  if (version_code)
    {
      *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, V_BUILD);
    }

  /* Initialize USB */
  sanei_usb_init ();

  status = kv_enum_devices ();
  if (status)
    return status;

  DBG (DBG_proc, "sane_init: leave\n");
  return SANE_STATUS_GOOD;
}

/* Terminate the KV-S1025 SANE backend */
void
sane_exit (void)
{
  DBG (DBG_proc, "sane_exit: enter\n");

  kv_exit ();

  DBG (DBG_proc, "sane_exit: exit\n");
}

/* Get device list */
SANE_Status
sane_get_devices (const SANE_Device *** device_list,
		  SANE_Bool __sane_unused__ local_only)
{
  DBG (DBG_proc, "sane_get_devices: enter\n");
  kv_get_devices_list (device_list);
  DBG (DBG_proc, "sane_get_devices: leave\n");
  return SANE_STATUS_GOOD;
}

/* Open device, return the device handle */
SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  return kv_open_by_name (devicename, handle);
}

/* Close device */
void
sane_close (SANE_Handle handle)
{
  DBG (DBG_proc, "sane_close: enter\n");
  kv_close ((PKV_DEV) handle);
  DBG (DBG_proc, "sane_close: leave\n");
}

/* Get option descriptor */
const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  return kv_get_option_descriptor ((PKV_DEV) handle, option);
}

/* Control option */
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  return kv_control_option ((PKV_DEV) handle, option, action, val, info);
}

/* Get scan parameters */
SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  PKV_DEV dev = (PKV_DEV) handle;

  int side = dev->current_side == SIDE_FRONT ? 0 : 1;

  DBG (DBG_proc, "sane_get_parameters: enter\n");

  if (!(dev->scanning))
    {
      /* Setup the parameters for the scan. (guessed value) */
      int resolution = dev->val[OPT_RESOLUTION].w;
      int width, length, depth = kv_get_depth (kv_get_mode (dev));;

      DBG (DBG_proc, "sane_get_parameters: initial settings\n");
      kv_calc_paper_size (dev, &width, &length);

      DBG (DBG_error, "Resolution = %d\n", resolution);
      DBG (DBG_error, "Paper width = %d, height = %d\n", width, length);

      /* Prepare the parameters for the caller. */
      dev->params[0].format = kv_get_mode (dev) == SM_COLOR ?
	SANE_FRAME_RGB : SANE_FRAME_GRAY;

      dev->params[0].last_frame = SANE_TRUE;
      dev->params[0].pixels_per_line = ((width * resolution) / 1200) & (~0xf);

      dev->params[0].depth = depth > 8 ? 8 : depth;

      dev->params[0].bytes_per_line =
	(dev->params[0].pixels_per_line / 8) * depth;
      dev->params[0].lines = (length * resolution) / 1200;

      memcpy (&dev->params[1], &dev->params[0], sizeof (SANE_Parameters));
    }

  /* Return the current values. */
  if (params)
    *params = (dev->params[side]);

  DBG (DBG_proc, "sane_get_parameters: exit\n");
  return SANE_STATUS_GOOD;
}

/* Start scanning */
SANE_Status
sane_start (SANE_Handle handle)
{
  SANE_Status status;
  PKV_DEV dev = (PKV_DEV) handle;
  SANE_Bool dev_ready;
  KV_CMD_RESPONSE rs;

  DBG (DBG_proc, "sane_start: enter\n");
  if (!dev->scanning)
    {
      /* open device */
      if (!kv_already_open (dev))
	{
	  DBG (DBG_proc, "sane_start: need to open device\n");
	  status = kv_open (dev);
	  if (status)
	    {
	      return status;
	    }
	}
      /* Begin scan */
      DBG (DBG_proc, "sane_start: begin scan\n");

      /* Get necessary parameters */
      sane_get_parameters (dev, NULL);

      dev->current_page = 0;
      dev->current_side = SIDE_FRONT;

      /* The scanner must be ready. */
      status = CMD_test_unit_ready (dev, &dev_ready);
      if (status || !dev_ready)
	{
	  return SANE_STATUS_DEVICE_BUSY;
	}

      if (!strcmp (dev->val[OPT_MANUALFEED].s, "off"))
	{
	  status = CMD_get_document_existanse (dev);
	  if (status)
	    {
	      DBG (DBG_proc, "sane_start: exit with no more docs\n");
	      return status;
	    }
	}

      /* Set window */
      status = CMD_reset_window (dev);
      if (status)
	{
	  return status;
	}

      status = CMD_set_window (dev, SIDE_FRONT, &rs);
      if (status)
	{
	  DBG (DBG_proc, "sane_start: error setting window\n");
	  return status;
	}

      if (rs.status)
	{
	  DBG (DBG_proc, "sane_start: error setting window\n");
	  DBG (DBG_proc,
	       "sane_start: sense_key=0x%x, ASC=0x%x, ASCQ=0x%x\n",
	       get_RS_sense_key (rs.sense),
	       get_RS_ASC (rs.sense), get_RS_ASCQ (rs.sense));
	  return SANE_STATUS_DEVICE_BUSY;
	}

      if (IS_DUPLEX (dev))
	{
	  status = CMD_set_window (dev, SIDE_BACK, &rs);

	  if (status)
	    {
	      DBG (DBG_proc, "sane_start: error setting window\n");
	      return status;
	    }
	  if (rs.status)
	    {
	      DBG (DBG_proc, "sane_start: error setting window\n");
	      DBG (DBG_proc,
		   "sane_start: sense_key=0x%x, "
		   "ASC=0x%x, ASCQ=0x%x\n",
		   get_RS_sense_key (rs.sense),
		   get_RS_ASC (rs.sense), get_RS_ASCQ (rs.sense));
	      return SANE_STATUS_INVAL;
	    }
	}

      /* Scan */
      status = CMD_scan (dev);
      if (status)
	{
	  return status;
	}

      status = AllocateImageBuffer (dev);
      if (status)
	{
	  return status;
	}
      dev->scanning = 1;
    }
  else
    {
      /* renew page */
      if (IS_DUPLEX (dev))
	{
	  if (dev->current_side == SIDE_FRONT)
	    {
	      /* back image data already read, so just return */
	      dev->current_side = SIDE_BACK;
	      DBG (DBG_proc, "sane_start: duplex back\n");
	      status = SANE_STATUS_GOOD;
              goto cleanup;
	    }
	  else
	    {
	      dev->current_side = SIDE_FRONT;
	      dev->current_page++;
	    }
	}
      else
	{
	  dev->current_page++;
	}
    }
  DBG (DBG_proc, "sane_start: NOW SCANNING page\n");

  /* Read image data */
  status = ReadImageData (dev, dev->current_page);
  if (status)
    {
      dev->scanning = 0;
      return status;
    }

  /* Get picture element size */
  {
    int width, height;
    status = CMD_read_pic_elements (dev, dev->current_page,
				    SIDE_FRONT, &width, &height);
    if (status)
      return status;
  }

  if (IS_DUPLEX (dev))
    {
      int width, height;
      status = CMD_read_pic_elements (dev, dev->current_page,
				      SIDE_BACK, &width, &height);
      if (status)
	return status;
    }

  /* software based enhancement functions from sanei_magic */
  /* these will modify the image, and adjust the params */
  /* at this point, we are only looking at the front image */
  /* of simplex or duplex data, back side has already exited */
  /* so, we do both sides now, if required */
  if (dev->val[OPT_SWDESKEW].w){
    buffer_deskew(dev,SIDE_FRONT);
  }
  if (dev->val[OPT_SWCROP].w){
    buffer_crop(dev,SIDE_FRONT);
  }
  if (dev->val[OPT_SWDESPECK].w){
    buffer_despeck(dev,SIDE_FRONT);
  }
  if (dev->val[OPT_SWDEROTATE].w || dev->val[OPT_ROTATE].w){
    buffer_rotate(dev,SIDE_FRONT);
  }

  if (IS_DUPLEX (dev)){
    if (dev->val[OPT_SWDESKEW].w){
      buffer_deskew(dev,SIDE_BACK);
    }
    if (dev->val[OPT_SWCROP].w){
      buffer_crop(dev,SIDE_BACK);
    }
    if (dev->val[OPT_SWDESPECK].w){
      buffer_despeck(dev,SIDE_BACK);
    }
    if (dev->val[OPT_SWDEROTATE].w || dev->val[OPT_ROTATE].w){
      buffer_rotate(dev,SIDE_BACK);
    }
  }

  cleanup:

  /* check if we need to skip this page */
  if (dev->val[OPT_SWSKIP].w && buffer_isblank(dev,dev->current_side)){
    DBG (DBG_proc, "sane_start: blank page, recurse\n");
    return sane_start(handle);
  }

  DBG (DBG_proc, "sane_start: exit\n");
  return status;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf,
	   SANE_Int max_len, SANE_Int * len)
{
  PKV_DEV dev = (PKV_DEV) handle;
  int side = dev->current_side == SIDE_FRONT ? 0 : 1;

  int size = max_len;
  if (!dev->scanning)
    return SANE_STATUS_EOF;

  if (size > dev->img_size[side])
    size = dev->img_size[side];

  if (size == 0)
    {
      *len = size;
      return SANE_STATUS_EOF;
    }

  if (dev->val[OPT_INVERSE].w &&
      (kv_get_mode (dev) == SM_BINARY || kv_get_mode (dev) == SM_DITHER))
    {
      int i;
      unsigned char *p = dev->img_pt[side];
      for (i = 0; i < size; i++)
	{
	  buf[i] = ~p[i];
	}
    }
  else
    {
      memcpy (buf, dev->img_pt[side], size);
    }

  /*hexdump(DBG_error, "img data", buf, 128); */

  dev->img_pt[side] += size;
  dev->img_size[side] -= size;

  DBG (DBG_proc, "sane_read: %d bytes to read, "
       "%d bytes read, EOF=%s  %d\n",
       max_len, size, dev->img_size[side] == 0 ? "True" : "False", side);

  if (len)
    {
      *len = size;
    }
  if (dev->img_size[side] == 0)
    {
      if (!strcmp (dev->val[OPT_FEEDER_MODE].s, "single"))
	if ((IS_DUPLEX (dev) && side) || !IS_DUPLEX (dev))
	  dev->scanning = 0;
    }
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  PKV_DEV dev = (PKV_DEV) handle;
  DBG (DBG_proc, "sane_cancel: scan canceled.\n");
  dev->scanning = 0;

  kv_close (dev);
}

SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool m)
{
  h=h;
  m=m;
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int * fd)
{
  h=h;
  fd=fd;
  return SANE_STATUS_UNSUPPORTED;
}
