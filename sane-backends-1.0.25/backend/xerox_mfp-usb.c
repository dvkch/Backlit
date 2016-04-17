/*
 * SANE backend for Xerox Phaser 3200MFP
 * Copyright 2008 ABC <abc@telekom.ru>
 *
 * This program is licensed under GPL + SANE exception.
 * More info at http://www.sane-project.org/license.html
 */

#undef	BACKEND_NAME
#define	BACKEND_NAME xerox_mfp
#define DEBUG_DECLARE_ONLY
#define DEBUG_NOT_STATIC
#include "sane/config.h"
#include "sane/saneopts.h"
#include "sane/sanei_config.h"
#include "sane/sanei_backend.h"
#include "sane/sanei_debug.h"
#include "sane/sanei_usb.h"
#include "xerox_mfp.h"


extern int sanei_debug_xerox_mfp;

int
usb_dev_request (struct device *dev,
	     SANE_Byte *cmd, size_t cmdlen,
	     SANE_Byte *resp, size_t *resplen)
{
  SANE_Status status;
  size_t len = cmdlen;

  if (cmd && cmdlen) {
    status = sanei_usb_write_bulk (dev->dn, cmd, &cmdlen);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "%s: sanei_usb_write_bulk: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      return SANE_STATUS_IO_ERROR;
    }

    if (cmdlen != len) {
      DBG (1, "%s: sanei_usb_write_bulk: wanted %lu bytes, wrote %lu bytes\n",
	   __FUNCTION__, (size_t)len, (size_t)cmdlen);
      return SANE_STATUS_IO_ERROR;
    }
  }

  if (resp && resplen) {
    status = sanei_usb_read_bulk (dev->dn, resp, resplen);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "%s: sanei_usb_read_bulk: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      return SANE_STATUS_IO_ERROR;
    }
  }

  return SANE_STATUS_GOOD;
}


SANE_Status
usb_dev_open (struct device *dev)
{
  SANE_Status status;

  DBG (3, "%s: open %p\n", __FUNCTION__, (void *)dev);
  status = sanei_usb_open (dev->sane.name, &dev->dn);
  if (status != SANE_STATUS_GOOD) {
      DBG (1, "%s: sanei_usb_open(%s): %s\n", __FUNCTION__,
	   dev->sane.name, sane_strstatus (status));
      dev->dn = -1;
      return status;
    }
  sanei_usb_clear_halt (dev->dn);
  return SANE_STATUS_GOOD;
}

void
usb_dev_close (struct device *dev)
{
  if (!dev)
    return;
  DBG (3, "%s: closing dev %p\n", __FUNCTION__, (void *)dev);

  /* finish all operations */
  if (dev->scanning) {
    dev->cancel = 1;
    /* flush READ_IMAGE data */
    if (dev->reading)
      sane_read(dev, NULL, 1, NULL);
    /* send cancel if not sent before */
    if (dev->state != SANE_STATUS_CANCELLED)
      ret_cancel(dev, 0);
  }

  sanei_usb_clear_halt (dev->dn);	/* unstall for next users */
  sanei_usb_close (dev->dn);
  dev->dn = -1;
}


/* SANE API ignores return code of this callback */
SANE_Status
usb_configure_device (const char *devname, SANE_Status (*attach) (const char *dev))
{
  sanei_usb_set_timeout (1000);
  sanei_usb_attach_matching_devices (devname, attach);
  sanei_usb_set_timeout (30000);
  return SANE_STATUS_GOOD;
}


/* xerox_mfp-usb.c */
