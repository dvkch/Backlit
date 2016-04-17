/*
   Copyright (C) 2008, Panasonic Russia Ltd.
   Copyright (C) 2010, m. allan noah
*/
/*
   Panasonic KV-S20xx USB-SCSI scanners.
*/

#define DEBUG_NOT_STATIC
#define BUILD 2

#include "../include/sane/config.h"

#include <string.h>
#include <unistd.h>

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_config.h"
#include "../include/lassert.h"

#include "kvs20xx.h"
#include "kvs20xx_cmd.h"

struct known_device
{
  const SANE_Int id;
  const SANE_Device scanner;
};

static const struct known_device known_devices[] = {
  {
    KV_S2025C,
    { "", "MATSHITA", "KV-S2025C", "sheetfed scanner" },
  },
  {
    KV_S2045C,
    { "", "MATSHITA", "KV-S2045C", "sheetfed scanner" },
  },
  {
    KV_S2026C,
    { "", "MATSHITA", "KV-S2026C", "sheetfed scanner" },
  },
  {
    KV_S2046C,
    { "", "MATSHITA", "KV-S2046C", "sheetfed scanner" },
  },
  {
    KV_S2028C,
    { "", "MATSHITA", "KV-S2028C", "sheetfed scanner" },
  },
  {
    KV_S2048C,
    { "", "MATSHITA", "KV-S2048C", "sheetfed scanner" },
  },
};

SANE_Status
sane_init (SANE_Int __sane_unused__ * version_code,
	   SANE_Auth_Callback __sane_unused__ authorize)
{
  DBG_INIT ();
  DBG (DBG_INFO, "This is panasonic kvs20xx driver\n");

  *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, BUILD);

  /* Initialize USB */
  sanei_usb_init ();

  return SANE_STATUS_GOOD;
}

/*
 * List of available devices, allocated by sane_get_devices, released
 * by sane_exit()
 */
static SANE_Device **devlist = NULL;
static unsigned curr_scan_dev = 0;

void
sane_exit (void)
{
  if (devlist)
    {
      int i;
      for (i = 0; devlist[i]; i++)
	{
	  free ((void *) devlist[i]->name);
	  free ((void *) devlist[i]);
	}
      free ((void *) devlist);
      devlist = NULL;
    }
}

static SANE_Status
attach (SANE_String_Const devname)
{
  int i = 0;
  if (devlist)
    {
      for (; devlist[i]; i++);
      devlist = realloc (devlist, sizeof (SANE_Device *) * (i + 1));
      if (!devlist)
	return SANE_STATUS_NO_MEM;
    }
  else
    {
      devlist = malloc (sizeof (SANE_Device *) * 2);
      if (!devlist)
	return SANE_STATUS_NO_MEM;
    }
  devlist[i] = malloc (sizeof (SANE_Device));
  if (!devlist[i])
    return SANE_STATUS_NO_MEM;
  memcpy (devlist[i], &known_devices[curr_scan_dev].scanner,
	  sizeof (SANE_Device));
  devlist[i]->name = strdup (devname);
  /* terminate device list with NULL entry: */
  devlist[i + 1] = 0;
  DBG (DBG_INFO, "%s device attached\n", devname);
  return SANE_STATUS_GOOD;
}

/* Get device list */
SANE_Status
sane_get_devices (const SANE_Device *** device_list,
		  SANE_Bool __sane_unused__ local_only)
{
  if (devlist)
    {
      int i;
      for (i = 0; devlist[i]; i++)
	{
	  free ((void *) devlist[i]->name);
	  free ((void *) devlist[i]);
	}
      free ((void *) devlist);
      devlist = NULL;
    }

  for (curr_scan_dev = 0;
       curr_scan_dev < sizeof (known_devices) / sizeof (known_devices[0]);
       curr_scan_dev++)
    {
      sanei_usb_find_devices (PANASONIC_ID,
			      known_devices[curr_scan_dev].id, attach);
    }
  for (curr_scan_dev = 0;
       curr_scan_dev < sizeof (known_devices) / sizeof (known_devices[0]);
       curr_scan_dev++)
    {
      sanei_scsi_find_devices (known_devices[curr_scan_dev].scanner.vendor,
			       known_devices[curr_scan_dev].scanner.model,
			       NULL, -1, -1, -1, -1, attach);
    }
  *device_list = (const SANE_Device **) devlist;
  return SANE_STATUS_GOOD;
}

/* Open device, return the device handle */
SANE_Status
sane_open (SANE_String_Const devname, SANE_Handle * handle)
{
  unsigned i, j, id = 0;
  struct scanner *s;
  SANE_Int h, bus;
  SANE_Status st;
  for (i = 0; devlist[i]; i++)
    {
      if (!strcmp (devlist[i]->name, devname))
	break;
    }
  if (!devlist[i])
    return SANE_STATUS_INVAL;
  for (j = 0; j < sizeof (known_devices) / sizeof (known_devices[0]); j++)
    {
      if (!strcmp (devlist[i]->model, known_devices[j].scanner.model))
	{
	  id = known_devices[j].id;
	  break;
	}
    }

  st = sanei_usb_open (devname, &h);
  if (st == SANE_STATUS_ACCESS_DENIED)
    return st;
  if (st)
    {
      st = sanei_scsi_open (devname, &h, kvs20xx_sense_handler, NULL);
      if (st)
	{
	  return st;
	}
      bus = SCSI;
    }
  else
    {
      bus = USB;
      st = sanei_usb_claim_interface (h, 0);
      if (st)
	{
	  sanei_usb_close (h);
	  return st;
	}
    }

  s = malloc (sizeof (struct scanner));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (struct scanner));
  s->buffer = malloc (MAX_READ_DATA_SIZE + BULK_HEADER_SIZE);
  if (!s->buffer)
    return SANE_STATUS_NO_MEM;
  s->file = h;
  s->bus = bus;
  s->id = id;
  kvs20xx_init_options (s);
  *handle = s;
  for (i = 0; i < 3; i++)
    {
      st = kvs20xx_test_unit_ready (s);
      if (st)
	{
	  if (s->bus == SCSI)
	    {
	      sanei_scsi_close (s->file);
	      st = sanei_scsi_open (devname, &h, kvs20xx_sense_handler, NULL);
	      if (st)
		return st;
	    }
	  else
	    {
	      sanei_usb_release_interface (s->file, 0);
	      sanei_usb_close (s->file);
	      st = sanei_usb_open (devname, &h);
	      if (st)
		return st;
	      st = sanei_usb_claim_interface (h, 0);
	      if (st)
		{
		  sanei_usb_close (h);
		  return st;
		}
	    }
	  s->file = h;
	}
      else
	break;
    }
  if (i == 3)
    return SANE_STATUS_DEVICE_BUSY;

  st = kvs20xx_set_timeout (s, s->val[FEED_TIMEOUT].w);
  if (st)
    {
      sane_close (s);
      return st;
    }

  return SANE_STATUS_GOOD;
}

/* Close device */
void
sane_close (SANE_Handle handle)
{
  struct scanner *s = (struct scanner *) handle;
  int i;
  if (s->bus == USB)
    {
      sanei_usb_release_interface (s->file, 0);
      sanei_usb_close (s->file);
    }
  else
    sanei_scsi_close (s->file);

  for (i = 1; i < NUM_OPTIONS; i++)
    {
      if (s->opt[i].type == SANE_TYPE_STRING && s->val[i].s)
	free (s->val[i].s);
    }
  if (s->data)
    free (s->data);
  free (s->buffer);
  free (s);

}

/* Get option descriptor */
const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  struct scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS || option < 0)
    return NULL;
  return s->opt + option;
}

static SANE_Status
wait_document (struct scanner *s)
{
  SANE_Status st;
  int i;
  if (!strcmp ("off", s->val[MANUALFEED].s))
    return kvs20xx_document_exist (s);

  for (i = 0; i < s->val[FEED_TIMEOUT].w; i++)
    {
      st = kvs20xx_document_exist (s);
      if (st != SANE_STATUS_NO_DOCS)
	return st;
      sleep (1);
    }
  return SANE_STATUS_NO_DOCS;
}

/* Start scanning */
SANE_Status
sane_start (SANE_Handle handle)
{
  struct scanner *s = (struct scanner *) handle;
  SANE_Status st;
  int duplex = s->val[DUPLEX].w;

  if (!s->scanning)
    {
      unsigned dummy_length;
      st = kvs20xx_test_unit_ready (s);
      if (st)
	return st;

      st = wait_document (s);
      if (st)
	return st;

      st = kvs20xx_reset_window (s);
      if (st)
	return st;
      st = kvs20xx_set_window (s, SIDE_FRONT);
      if (st)
	return st;
      if (duplex)
	{
	  st = kvs20xx_set_window (s, SIDE_BACK);
	  if (st)
	    return st;
	}
      st = kvs20xx_scan (s);
      if (st)
	return st;

      st = kvs20xx_read_picture_element (s, SIDE_FRONT, &s->params);
      if (st)
	return st;
      if (duplex)
	{
	  st = get_adjust_data (s, &dummy_length);
	  if (st)
	    return st;
	}
      else
	{
	  dummy_length = 0;
	}
      s->scanning = 1;
      s->page = 0;
      s->read = 0;
      s->side = SIDE_FRONT;
      sane_get_parameters (s, NULL);
      s->saved_dummy_size = s->dummy_size = dummy_length
	? (dummy_length * s->val[RESOLUTION].w / 1200 - 1)
	* s->params.bytes_per_line : 0;
      s->side_size = s->params.lines * s->params.bytes_per_line;

      s->data = realloc (s->data, duplex ? s->side_size * 2 : s->side_size);
      if (!s->data)
	{
	  s->scanning = 0;
	  return SANE_STATUS_NO_MEM;
	}
    }

  if (duplex)
    {
      unsigned side = SIDE_FRONT;
      unsigned read, mx;
      if (s->side == SIDE_FRONT && s->read == s->side_size - s->dummy_size)
	{
	  s->side = SIDE_BACK;
	  s->read = s->dummy_size;
	  s->dummy_size = 0;
	  return SANE_STATUS_GOOD;
	}
      s->read = 0;
      s->dummy_size = s->saved_dummy_size;
      s->side = SIDE_FRONT;
      st = kvs20xx_document_exist (s);
      if (st)
	return st;
      for (mx = s->side_size * 2; !st; mx -= read, side ^= SIDE_BACK)
	st = kvs20xx_read_image_data (s, s->page, side,
				      &s->data[s->side_size * 2 - mx], mx,
				      &read);
    }
  else
    {
      unsigned read, mx;
      s->read = 0;
      st = kvs20xx_document_exist (s);
      if (st)
	return st;
      DBG (DBG_INFO, "start: %d\n", s->page);

      for (mx = s->side_size; !st; mx -= read)
	st = kvs20xx_read_image_data (s, s->page, SIDE_FRONT,
				      &s->data[s->side_size - mx], mx, &read);
    }
  if (st && st != SANE_STATUS_EOF)
    {
      s->scanning = 0;
      return st;
    }
  s->page++;
  return SANE_STATUS_GOOD;
}

inline static void
memcpy24 (u8 * dest, u8 * src, unsigned size, unsigned ls)
{
  unsigned i;
  for (i = 0; i < size; i++)
    {
      dest[i * 3] = src[i];
      dest[i * 3 + 1] = src[i + ls];
      dest[i * 3 + 2] = src[i + 2 * ls];
    }
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf,
	   SANE_Int max_len, SANE_Int * len)
{
  struct scanner *s = (struct scanner *) handle;
  int duplex = s->val[DUPLEX].w;
  int color = !strcmp (s->val[MODE].s, SANE_VALUE_SCAN_MODE_COLOR);
  int rest = s->side_size - s->read - s->dummy_size;
  *len = 0;

  if (!s->scanning || !rest)
    {
      if (strcmp (s->val[FEEDER_MODE].s, SANE_I18N ("continuous")))
	{
	  if (!duplex || s->side == SIDE_BACK)
	    s->scanning = 0;
	}
      return SANE_STATUS_EOF;
    }

  *len = max_len < rest ? max_len : rest;
  if (duplex && (s->id == KV_S2025C
		 || s->id == KV_S2026C || s->id == KV_S2028C))
    {
      if (color)
	{
	  unsigned ls = s->params.bytes_per_line;
	  unsigned i, a = s->side == SIDE_FRONT ? 0 : ls / 3;
	  u8 *data;
	  *len = (*len / ls) * ls;
	  for (i = 0, data = s->data + s->read * 2 + a;
	       i < *len / ls; buf += ls, data += 2 * ls, i++)
	    memcpy24 (buf, data, ls / 3, ls * 2 / 3);
	}
      else
	{
	  unsigned ls = s->params.bytes_per_line;
	  unsigned i = s->side == SIDE_FRONT ? 0 : ls;
	  unsigned head = ls - (s->read % ls);
	  unsigned tail = (*len - head) % ls;
	  unsigned lines = (*len - head) / ls;
	  u8 *data = s->data + (s->read / ls) * ls * 2 + i + s->read % ls;
	  assert (data <= s->data + s->side_size * 2);
	  memcpy (buf, data, head);
	  for (i = 0, buf += head, data += head + (head ? ls : 0);
	       i < lines; buf += ls, data += ls * 2, i++)
	    {
	      assert (data <= s->data + s->side_size * 2);
	      memcpy (buf, data, ls);
	    }
	  assert ((data <= s->data + s->side_size * 2) || !tail);
	  memcpy (buf, data, tail);
	}
      s->read += *len;
    }
  else
    {
      if (color)
	{
	  unsigned i, ls = s->params.bytes_per_line;
	  u8 *data = s->data + s->read;
	  *len = (*len / ls) * ls;
	  for (i = 0; i < *len / ls; buf += ls, data += ls, i++)
	    memcpy24 (buf, data, ls / 3, ls / 3);
	}
      else
	{
	  memcpy (buf, s->data + s->read, *len);
	}
      s->read += *len;
    }
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  struct scanner *s = (struct scanner *) handle;
  s->scanning = 0;
}

SANE_Status
sane_set_io_mode (SANE_Handle __sane_unused__ h, SANE_Bool __sane_unused__ m)
{
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle __sane_unused__ h,
		    SANE_Int __sane_unused__ * fd)
{
  return SANE_STATUS_UNSUPPORTED;
}
