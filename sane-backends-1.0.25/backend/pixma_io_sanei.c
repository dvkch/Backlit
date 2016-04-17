/* SANE - Scanner Access Now Easy.
 * For limitations, see function sanei_usb_get_vendor_product().

   Copyright (C) 2011-2015 Rolf Bensch <rolf at bensch hyphen online dot de>
   Copyright (C) 2006-2007 Wittawat Yamwong <wittawat@web.de>

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
#include "../include/sane/config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>		/* INT_MAX */

#include "pixma_rename.h"
#include "pixma_common.h"
#include "pixma_io.h"
#include "pixma_bjnp.h"

#include "../include/sane/sanei_usb.h"


#ifdef __GNUC__
# define UNUSED(v) (void) v
#else
# define UNUSED(v)
#endif

/* MAC OS X does not support timeouts in darwin/libusb interrupt reads
 * This is a very basic turnaround for MAC OS X
 * Button scan will not work with this wrapper */
#ifdef __APPLE__
# define sanei_usb_read_int sanei_usb_read_bulk
#endif


struct pixma_io_t
{
  pixma_io_t *next;
  int interface;
  SANE_Int dev;
};

typedef struct scanner_info_t
{
  struct scanner_info_t *next;
  char *devname;
  int interface;
  const pixma_config_t *cfg;
  char serial[PIXMA_MAX_ID_LEN + 1];	/* "xxxxyyyy_zzzzzzz..."
					   x = vid, y = pid, z = serial */
} scanner_info_t;

#define INT_USB 0
#define INT_BJNP 1

static scanner_info_t *first_scanner = NULL;
static pixma_io_t *first_io = NULL;
static unsigned nscanners;


static scanner_info_t *
get_scanner_info (unsigned devnr)
{
  scanner_info_t *si;
  for (si = first_scanner; si && devnr != 0; --devnr, si = si->next)
    {
    }
  return si;
}

static const struct pixma_config_t *lookup_scanner(const char *makemodel, 
                                                   const struct pixma_config_t *const pixma_devices[])
{ 
  int i;
  const struct pixma_config_t *cfg;
  char *match;

  for (i = 0; pixma_devices[i]; i++)
    {
      /* loop through the device classes (mp150, mp730 etc) */
      for (cfg = pixma_devices[i]; cfg->name; cfg++)
        {
          /* loop through devices in class */
          if ((match = strcasestr (makemodel, cfg->model)) != NULL)
            { 
              /* possible match found, make sure it is not a partial match */
              /* MP600 and MP600R are different models! */
              /* some models contain ranges, so check for a '-' too */

              if ((match[strlen(cfg->model)] == ' ') || 
                  (match[strlen(cfg->model)] == '\0') ||
                  (match[strlen(cfg->model)] == '-'))
                {
                  pixma_dbg (3, "Scanner model found: Name %s(%s) matches %s\n", cfg->model, cfg->name, makemodel);
                  return cfg;
                }
            }
          pixma_dbg (20, "Scanner model %s(%s) not found, giving up! %s\n", cfg->model, cfg->name, makemodel);
       }
    }
  return NULL;
}

static SANE_Status
attach (SANE_String_Const devname)
{
  scanner_info_t *si;

  si = (scanner_info_t *) calloc (1, sizeof (*si));
  if (!si)
    return SANE_STATUS_NO_MEM;
  si->devname = strdup (devname);
  if (!si->devname)
    return SANE_STATUS_NO_MEM;
  si -> interface = INT_USB;
  si->next = first_scanner;
  first_scanner = si;
  nscanners++;
  return SANE_STATUS_GOOD;
}


static SANE_Status
attach_bjnp (SANE_String_Const devname,  SANE_String_Const makemodel, 
             SANE_String_Const serial, 
             const struct pixma_config_t *const pixma_devices[])
{
  scanner_info_t *si;
  const pixma_config_t *cfg;
  SANE_Status error;

  si = (scanner_info_t *) calloc (1, sizeof (*si));
  if (!si)
    return SANE_STATUS_NO_MEM;
  si->devname = strdup (devname);
  if (!si->devname)
    return SANE_STATUS_NO_MEM;
  if ((cfg = lookup_scanner(makemodel, pixma_devices)) == (struct pixma_config_t *)NULL)
    error = SANE_STATUS_INVAL;
  else
    {
      si->cfg = cfg;
      sprintf(si->serial, "%s_%s", cfg->model, serial);
      si -> interface = INT_BJNP;
      si->next = first_scanner;
      first_scanner = si;
      nscanners++;
      error = SANE_STATUS_GOOD;
    }
  return error;
}

static void
clear_scanner_list (void)
{
  scanner_info_t *si = first_scanner;
  while (si)
    {
      scanner_info_t *temp = si;
      free (si->devname);
      si = si->next;
      free (temp);
    }
  nscanners = 0;
  first_scanner = NULL;
}

static SANE_Status
get_descriptor (SANE_Int dn, SANE_Int type, SANE_Int descidx,
		SANE_Int index, SANE_Int length, SANE_Byte * data)
{
  return sanei_usb_control_msg (dn, 0x80, USB_REQ_GET_DESCRIPTOR,
				((type & 0xff) << 8) | (descidx & 0xff),
				index, length, data);
}

static SANE_Status
get_string_descriptor (SANE_Int dn, SANE_Int index, SANE_Int lang,
		       SANE_Int length, SANE_Byte * data)
{
  return get_descriptor (dn, USB_DT_STRING, index, lang, length, data);
}

static void
u16tohex (uint16_t x, char *str)
{
  static const char hdigit[16] =
    { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D',
    'E', 'F'
    };
  str[0] = hdigit[(x >> 12) & 0xf];
  str[1] = hdigit[(x >> 8) & 0xf];
  str[2] = hdigit[(x >> 4) & 0xf];
  str[3] = hdigit[x & 0xf];
  str[4] = '\0';
}

static void
read_serial_number (scanner_info_t * si)
{
  uint8_t unicode[2 * (PIXMA_MAX_ID_LEN - 9) + 2];
  uint8_t ddesc[18];
  int iSerialNumber;
  SANE_Int usb;
  char *serial = si->serial;

  u16tohex (si->cfg->vid, serial);
  u16tohex (si->cfg->pid, serial + 4);

  if (SANE_STATUS_GOOD != sanei_usb_open (si->devname, &usb))
    return;
  if (get_descriptor (usb, USB_DT_DEVICE, 0, 0, 18, ddesc)
      != SANE_STATUS_GOOD)
    goto done;
  iSerialNumber = ddesc[16];
  if (iSerialNumber != 0)
    {
      int i, len;
      SANE_Status status;

      /*int iSerialNumber = ddesc[16];*/
      /* Read the first language code. Assumed that there is at least one. */
      if (get_string_descriptor (usb, 0, 0, 4, unicode) != SANE_STATUS_GOOD)
        goto done;
      /* Read the serial number string. */
      status = get_string_descriptor (usb, iSerialNumber,
                                      unicode[3] * 256 + unicode[2],
                                      sizeof (unicode), unicode);
      if (status != SANE_STATUS_GOOD)
        goto done;
      /* Assumed charset: Latin1 */
      len = unicode[0];
      if (len > (int) sizeof (unicode))
        {
          len = sizeof (unicode);
          PDBG (pixma_dbg (1, "WARNING:Truncated serial number\n"));
        }
            serial[8] = '_';
            for (i = 2; i < len; i += 2)
        {
          serial[9 + i / 2 - 1] = unicode[i];
        }
      serial[9 + i / 2 - 1] = '\0';
    }
  else
    {
      PDBG (pixma_dbg (1, "WARNING:No serial number\n"));
    }
done:
  sanei_usb_close (usb);
}

static int
map_error (SANE_Status ss)
{
  switch (ss)
    {
    case SANE_STATUS_GOOD:
      return 0;
    case SANE_STATUS_UNSUPPORTED:
      return PIXMA_ENODEV;
    case SANE_STATUS_DEVICE_BUSY:
      return PIXMA_EBUSY;
    case SANE_STATUS_INVAL:
      return PIXMA_EINVAL;
    case SANE_STATUS_IO_ERROR:
      return PIXMA_EIO;
    case SANE_STATUS_NO_MEM:
      return PIXMA_ENOMEM;
    case SANE_STATUS_ACCESS_DENIED:
      return PIXMA_EACCES;
    case SANE_STATUS_CANCELLED:
      return PIXMA_ECANCELED;
    case SANE_STATUS_JAMMED:
       return PIXMA_EPAPER_JAMMED;
    case SANE_STATUS_COVER_OPEN:
       return PIXMA_ECOVER_OPEN;
    case SANE_STATUS_NO_DOCS:
       return PIXMA_ENO_PAPER;
    case SANE_STATUS_EOF:
       return PIXMA_EOF;
#ifdef SANE_STATUS_HW_LOCKED
    case SANE_STATUS_HW_LOCKED:       /* unused by pixma */
#endif
#ifdef SANE_STATUS_WARMING_UP
    case SANE_STATUS_WARMING_UP:      /* unused by pixma */
#endif
      break;
    }
  PDBG (pixma_dbg (1, "BUG:Unmapped SANE Status code %d\n", ss));
  return PIXMA_EIO;		/* should not happen */
}


int
pixma_io_init (void)
{
  sanei_usb_init ();
  sanei_bjnp_init();
  nscanners = 0;
  return 0;
}

void
pixma_io_cleanup (void)
{
  while (first_io)
    pixma_disconnect (first_io);
  clear_scanner_list ();
}

unsigned
pixma_collect_devices (const char **conf_devices,
                       const struct pixma_config_t *const pixma_devices[])
{
  unsigned i, j;
  struct scanner_info_t *si;
  const struct pixma_config_t *cfg;

  clear_scanner_list ();
  j = 0;
  for (i = 0; pixma_devices[i]; i++)
    {
      for (cfg = pixma_devices[i]; cfg->name; cfg++)
        {
          sanei_usb_find_devices (cfg->vid, cfg->pid, attach);
          si = first_scanner;
          while (j < nscanners)
            {
              PDBG (pixma_dbg (3, "pixma_collect_devices() found %s at %s\n",
                   cfg->name, si->devname));
              si->cfg = cfg;
              read_serial_number (si);
              si = si->next;
              j++;
            }
        }
    }
  sanei_bjnp_find_devices(conf_devices, attach_bjnp, pixma_devices);
  si = first_scanner;
  while (j < nscanners)
    {
      PDBG (pixma_dbg (3, "pixma_collect_devices() found %s at %s\n",
               si->cfg->name, si->devname));
      si = si->next;
      j++;

    }
  return nscanners;
}

const pixma_config_t *
pixma_get_device_config (unsigned devnr)
{
  const scanner_info_t *si = get_scanner_info (devnr);
  return (si) ? si->cfg : NULL;
}

const char *
pixma_get_device_id (unsigned devnr)
{
  const scanner_info_t *si = get_scanner_info (devnr);
  return (si) ? si->serial : NULL;
}

int
pixma_connect (unsigned devnr, pixma_io_t ** handle)
{
  pixma_io_t *io;
  SANE_Int dev;
  const scanner_info_t *si;
  int error;

  *handle = NULL;
  si = get_scanner_info (devnr);
  if (!si)
    return PIXMA_EINVAL;
  if (si-> interface == INT_BJNP)
    error = map_error (sanei_bjnp_open (si->devname, &dev));
  else
    error = map_error (sanei_usb_open (si->devname, &dev));
      
  if (error < 0)
    return error;
  io = (pixma_io_t *) calloc (1, sizeof (*io));
  if (!io)
    {
      if (si -> interface == INT_BJNP)
        sanei_bjnp_close (dev);
      else 
        sanei_usb_close (dev);
      return PIXMA_ENOMEM;
    }
  io->next = first_io;
  first_io = io;
  io->dev = dev;
  io->interface = si->interface;
  *handle = io;
  return 0;
}


void
pixma_disconnect (pixma_io_t * io)
{
  pixma_io_t **p;

  if (!io)
    return;
  for (p = &first_io; *p && *p != io; p = &((*p)->next))
    {
    }
  PASSERT (*p);
  if (!(*p))
    return;
  if (io-> interface == INT_BJNP)
    sanei_bjnp_close (io->dev);
  else
    sanei_usb_close (io->dev);
  *p = io->next;
  free (io);
}

int pixma_activate (pixma_io_t * io)
{
  int error;
  if (io->interface == INT_BJNP)
    {
      error = map_error(sanei_bjnp_activate (io->dev));
    }
  else
    /* noop for USB interface */
    error = 0;
  return error;
}

int pixma_deactivate (pixma_io_t * io)
{
  int error;
  if (io->interface == INT_BJNP)
    {
      error = map_error(sanei_bjnp_deactivate (io->dev));
    }
  else
    /* noop for USB interface */
    error = 0;
  return error;

}

int
pixma_reset_device (pixma_io_t * io)
{
  UNUSED (io);
  return PIXMA_ENOTSUP;
}

int
pixma_write (pixma_io_t * io, const void *cmd, unsigned len)
{
  size_t count = len;
  int error;

  if (io->interface == INT_BJNP)
    {
    sanei_bjnp_set_timeout (io->dev, PIXMA_BULKOUT_TIMEOUT);
    error = map_error (sanei_bjnp_write_bulk (io->dev, cmd, &count));
    }
  else
    {
#ifdef HAVE_SANEI_USB_SET_TIMEOUT
    sanei_usb_set_timeout (PIXMA_BULKOUT_TIMEOUT);
#endif
    error = map_error (sanei_usb_write_bulk (io->dev, cmd, &count));
    }
  if (error == PIXMA_EIO)
    error = PIXMA_ETIMEDOUT;	/* FIXME: SANE doesn't have ETIMEDOUT!! */
  if (count != len)
    {
      PDBG (pixma_dbg (1, "WARNING:pixma_write(): count(%u) != len(%u)\n",
		       (unsigned) count, len));
      error = PIXMA_EIO;
    }
  if (error >= 0)
    error = count;
  PDBG (pixma_dump (10, "OUT ", cmd, error, len, 128));
  return error;
}

int
pixma_read (pixma_io_t * io, void *buf, unsigned size)
{
  size_t count = size;
  int error;

  if (io-> interface == INT_BJNP)
    {
    sanei_bjnp_set_timeout (io->dev, PIXMA_BULKIN_TIMEOUT);
    error = map_error (sanei_bjnp_read_bulk (io->dev, buf, &count));
    }
  else
    {
#ifdef HAVE_SANEI_USB_SET_TIMEOUT
      sanei_usb_set_timeout (PIXMA_BULKIN_TIMEOUT);
#endif
      error = map_error (sanei_usb_read_bulk (io->dev, buf, &count));
    }

  if (error == PIXMA_EIO)
    error = PIXMA_ETIMEDOUT;	/* FIXME: SANE doesn't have ETIMEDOUT!! */
  if (error >= 0)
    error = count;
  PDBG (pixma_dump (10, "IN  ", buf, error, -1, 128));
  return error;
}

int
pixma_wait_interrupt (pixma_io_t * io, void *buf, unsigned size, int timeout)
{
  size_t count = size;
  int error;

  /* FIXME: What is the meaning of "timeout" in sanei_usb? */
  if (timeout < 0)
    timeout = INT_MAX;
  else if (timeout < 100)
    timeout = 100;
  if (io-> interface == INT_BJNP)
    {
      sanei_bjnp_set_timeout (io->dev, timeout);
      error = map_error (sanei_bjnp_read_int (io->dev, buf, &count));
    }
  else
    {
#ifdef HAVE_SANEI_USB_SET_TIMEOUT
      sanei_usb_set_timeout (timeout);
#endif
      error = map_error (sanei_usb_read_int (io->dev, buf, &count));
    }
  if (error == PIXMA_EIO ||
      (io->interface == INT_BJNP && error == PIXMA_EOF))     /* EOF is a bjnp timeout error! */
    error = PIXMA_ETIMEDOUT;	/* FIXME: SANE doesn't have ETIMEDOUT!! */
  if (error == 0)
    error = count;
  if (error != PIXMA_ETIMEDOUT)
    PDBG (pixma_dump (10, "INTR", buf, error, -1, -1));
  return error;
}

int
pixma_set_interrupt_mode (pixma_io_t * s, int background)
{
  UNUSED (s);
  return (background) ? PIXMA_ENOTSUP : 0;
}
