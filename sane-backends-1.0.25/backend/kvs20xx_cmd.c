/*
   Copyright (C) 2008, Panasonic Russia Ltd.
   Copyright (C) 2010, m. allan noah
*/
/*
   Panasonic KV-S20xx USB-SCSI scanners.
*/

#include "../include/sane/config.h"

#include <string.h>
/*#include <unistd.h>*/

#define DEBUG_DECLARE_ONLY
#define BACKEND_NAME kvs20xx

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_usb.h"

#include "kvs20xx.h"
#include "kvs20xx_cmd.h"

static SANE_Status
usb_send_command (struct scanner *s, struct cmd *c, struct response *r,
		  void *buf)
{
  SANE_Status st;
  struct bulk_header *h = (struct bulk_header *) buf;
  u8 resp[sizeof (*h) + STATUS_SIZE];
  size_t sz = sizeof (*h) + MAX_CMD_SIZE;
  memset (h, 0, sz);
  h->length = cpu2be32 (sz);
  h->type = cpu2be16 (COMMAND_BLOCK);
  h->code = cpu2be16 (COMMAND_CODE);
  memcpy (h + 1, c->cmd, c->cmd_size);

  st = sanei_usb_write_bulk (s->file, (const SANE_Byte *) h, &sz);
  if (st)
    return st;
  if (sz != sizeof (*h) + MAX_CMD_SIZE)
    return SANE_STATUS_IO_ERROR;
  if (c->dir == CMD_IN)
    {
      sz = sizeof (*h) + c->data_size;
      st = sanei_usb_read_bulk (s->file, (SANE_Byte *) h, &sz);
      c->data = h + 1;
      c->data_size = sz - sizeof (*h);

      if (st || sz < sizeof (*h))
	{
	  st = sanei_usb_release_interface (s->file, 0);
	  if (st)
	    return st;
	  st = sanei_usb_claim_interface (s->file, 0);
	  if (st)
	    return st;
	  r->status = CHECK_CONDITION;
	  return SANE_STATUS_GOOD;
	}

    }
  else if (c->dir == CMD_OUT)
    {
      sz = sizeof (*h) + c->data_size;
      memset (h, 0, sizeof (*h));
      h->length = cpu2be32 (sizeof (*h) + c->data_size);
      h->type = cpu2be16 (DATA_BLOCK);
      h->code = cpu2be16 (DATA_CODE);
      memcpy (h + 1, c->data, c->data_size);
      st = sanei_usb_write_bulk (s->file, (const SANE_Byte *) h, &sz);
      if (st)
	return st;
    }
  sz = sizeof (resp);
  st = sanei_usb_read_bulk (s->file, resp, &sz);
  if (st || sz != sizeof (resp))
    return SANE_STATUS_IO_ERROR;
  r->status = be2cpu32 (*((u32 *) (resp + sizeof (*h))));
  return st;
}

SANE_Status
kvs20xx_sense_handler (int __sane_unused__ fd,
		       u_char * sense_buffer, void __sane_unused__ * arg)
{
  unsigned i;
  SANE_Status st = SANE_STATUS_GOOD;
  for (i = 0; i < sizeof (s_errors) / sizeof (s_errors[0]); i++)
    if ((sense_buffer[2] & 0xf) == s_errors[i].sense
	&& sense_buffer[12] == s_errors[i].asc
	&& sense_buffer[13] == s_errors[i].ascq)
      {
	st = s_errors[i].st;
	break;
      }
  if (st == SANE_STATUS_GOOD && sense_buffer[2] & END_OF_MEDIUM)
    st = SANE_STATUS_EOF;
  if (i == sizeof (s_errors) / sizeof (s_errors[0]))
    st = SANE_STATUS_IO_ERROR;
  DBG (DBG_ERR,
       "send_command: CHECK_CONDITION: sence:0x%x ASC:0x%x ASCQ:0x%x\n",
       sense_buffer[2], sense_buffer[12], sense_buffer[13]);

  return st;
}

static SANE_Status
send_command (struct scanner * s, struct cmd * c)
{
  SANE_Status st = SANE_STATUS_GOOD;
  if (s->bus == USB)
    {
      struct response r;
      memset (&r, 0, sizeof (r));
      st = usb_send_command (s, c, &r, s->buffer);
      if (st)
	return st;
      if (r.status)
	{
	  u8 b[sizeof (struct bulk_header) + RESPONSE_SIZE];
	  struct cmd c2 = {
            {0},
	    6,
            0,
	    RESPONSE_SIZE,
	    CMD_IN
	  };
	  c2.cmd[0] = REQUEST_SENSE;
	  c2.cmd[4] = RESPONSE_SIZE;
	  st = usb_send_command (s, &c2, &r, b);
	  if (st)
	    return st;
	  st = kvs20xx_sense_handler (0, b + sizeof (struct bulk_header), NULL);
	}
    }
  else
    {
      if (c->dir == CMD_OUT)
	{
	  memcpy (s->buffer, c->cmd, c->cmd_size);
	  memcpy (s->buffer + c->cmd_size, c->data, c->data_size);
	  st = sanei_scsi_cmd (s->file, s->buffer, c->cmd_size + c->data_size,
			       NULL, NULL);
	}
      else if (c->dir == CMD_IN)
	{
	  c->data = s->buffer;
	  st = sanei_scsi_cmd (s->file, c->cmd, c->cmd_size,
			       c->data, (size_t *) & c->data_size);
	}
      else
	{
	  st = sanei_scsi_cmd (s->file, c->cmd, c->cmd_size, NULL, NULL);
	}
    }
  return st;
}

SANE_Status
kvs20xx_test_unit_ready (struct scanner * s)
{
  struct cmd c = {
    {0},
    6,
    0,
    0,
    CMD_NONE
  };
  c.cmd[0] = TEST_UNIT_READY;
  if (send_command (s, &c))
    return SANE_STATUS_DEVICE_BUSY;

  return SANE_STATUS_GOOD;
}

SANE_Status
kvs20xx_set_timeout (struct scanner * s, int timeout)
{
  u16 t = cpu2be16 ((u16) timeout);
  struct cmd c = {
    {0},
    10,
    0,
    0,
    CMD_OUT
  };
  c.cmd[0] = SET_TIMEOUT;
  c.cmd[2] = 0x8d;
  *((u16 *) (c.cmd + 7)) = cpu2be16 (sizeof (t));

  c.data = &t;
  c.data_size = sizeof (t);

  if (s->bus == USB)
    sanei_usb_set_timeout (timeout * 1000);

  return send_command (s, &c);
}

SANE_Status
kvs20xx_set_window (struct scanner * s, int wnd_id)
{
  struct window wnd;
  struct cmd c = {
    {0},
    10,
    0,
    0,
    CMD_OUT
  };
  c.cmd[0] = SET_WINDOW;
  *((u16 *) (c.cmd + 7)) = cpu2be16 (sizeof (wnd));

  c.data = &wnd;
  c.data_size = sizeof (wnd);

  kvs20xx_init_window (s, &wnd, wnd_id);

  return send_command (s, &c);
}

SANE_Status
kvs20xx_reset_window (struct scanner * s)
{
  struct cmd c = {
    {0},
    10,
    0,
    0,
    CMD_NONE
  };
  c.cmd[0] = SET_WINDOW;

  return send_command (s, &c);
}

SANE_Status
kvs20xx_scan (struct scanner * s)
{
  struct cmd c = {
    {0},
    6,
    0,
    0,
    CMD_NONE
  };
  c.cmd[0] = SCAN;
  return send_command (s, &c);
}

SANE_Status
kvs20xx_document_exist (struct scanner * s)
{
  SANE_Status status;
  struct cmd c = {
    {0},
    10,
    0,
    6,
    CMD_IN,
  };
  u8 *d;
  c.cmd[0] = READ_10;
  c.cmd[2] = 0x81;
  set24 (c.cmd + 6, c.data_size);
  status = send_command (s, &c);
  if (status)
    return status;
  d = c.data;
  if (d[0] & 0x20)
    return SANE_STATUS_GOOD;

  return SANE_STATUS_NO_DOCS;
}

SANE_Status
kvs20xx_read_picture_element (struct scanner * s, unsigned side,
			      SANE_Parameters * p)
{
  SANE_Status status;
  struct cmd c = {
    {0},
    10,
    0,
    16,
    CMD_IN
  };
  u32 *data;
  c.cmd[0] = READ_10;
  c.cmd[2] = 0x80;
  c.cmd[5] = side;
  set24 (c.cmd + 6, c.data_size);

  status = send_command (s, &c);
  if (status)
    return status;
  data = (u32 *) c.data;
  p->pixels_per_line = be2cpu32 (data[0]);
  p->lines = be2cpu32 (data[1]);
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_buffer_status (struct scanner * s, unsigned *data_avalible)
{
  SANE_Status status;
  struct cmd c = {
    {0},
    10,
    0,
    12,
    CMD_IN
  };
  u32 *data;
  c.cmd[0] = GET_BUFFER_STATUS;
  c.cmd[7] = 12;

  status = send_command (s, &c);
  if (status)
    return status;
  data = (u32 *) c.data;
  *data_avalible = be2cpu32 (data[3]);
  return SANE_STATUS_GOOD;
}

SANE_Status
kvs20xx_read_image_data (struct scanner * s, unsigned page, unsigned side,
			 void *buf, unsigned max_size, unsigned *size)
{
  SANE_Status status;
  struct cmd c = {
    {0},
    10,
    0,
    0,
    CMD_IN
  };
  c.cmd[0] = READ_10;
  c.cmd[4] = page;
  c.cmd[5] = side;

  c.data_size = max_size < MAX_READ_DATA_SIZE ? max_size : MAX_READ_DATA_SIZE;

  set24 (c.cmd + 6, c.data_size);
  status = send_command (s, &c);

  if (status && status != SANE_STATUS_EOF)
    return status;

  *size = c.data_size;
  DBG (DBG_INFO, "kvs20xx_read_image_data: read %d, status %d\n", *size, status);
  memcpy (buf, c.data, *size);
  return status;
}

SANE_Status
get_adjust_data (struct scanner * s, unsigned *dummy_length)
{
  SANE_Status status;
  struct cmd c = {
    {0},
    10,
    0,
    40,
    CMD_IN
  };
  u16 *data;

  c.cmd[0] = GET_ADJUST_DATA;
  c.cmd[2] = 0x9b;
  c.cmd[8] = 40;
  status = send_command (s, &c);
  if (status)
    return status;
  data = (u16 *) c.data;
  *dummy_length = be2cpu16 (data[0]);
  return SANE_STATUS_GOOD;
}
