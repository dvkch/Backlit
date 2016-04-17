/*
   Copyright (C) 2009, Panasonic Russia Ltd.
   Copyright (C) 2010,2011, m. allan noah
*/
/*
   Panasonic KV-S40xx USB-SCSI scanner driver.
*/
#include "../include/sane/config.h"
#include <string.h>

#define DEBUG_DECLARE_ONLY
#define BACKEND_NAME kvs40xx

#include "../include/sane/sanei_backend.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_config.h"

#include "kvs40xx.h"

#include "../include/sane/sanei_debug.h"

#define COMMAND_BLOCK	1
#define DATA_BLOCK	2
#define RESPONSE_BLOCK	3

#define COMMAND_CODE	0x9000
#define DATA_CODE	0xb000
#define RESPONSE_CODE	0xa000
#define STATUS_SIZE 4

struct bulk_header
{
  u32 length;
  u16 type;
  u16 code;
  u32 transaction_id;
};

#define TEST_UNIT_READY        0x00
#define INQUIRY                0x12
#define SET_WINDOW             0x24
#define SCAN                   0x1B
#define SEND_10                0x2A
#define READ_10                0x28
#define REQUEST_SENSE          0x03
#define GET_BUFFER_STATUS      0x34
#define SET_TIMEOUT	    	0xE1
#define GET_ADJUST_DATA	    	0xE0
#define HOPPER_DOWN	    	0xE1
#define STOP_ADF		0xE1



#define SUPPORT_INFO		0x93


#define GOOD 0
#define CHECK_CONDITION 2

typedef enum
{
  CMD_NONE = 0,
  CMD_IN = 0x81,		/* scanner to pc */
  CMD_OUT = 0x02		/* pc to scanner */
} CMD_DIRECTION;		/* equals to endpoint address */

#define RESPONSE_SIZE	0x12
#define MAX_CMD_SIZE	12
struct cmd
{
  unsigned char cmd[MAX_CMD_SIZE];
  int cmd_size;
  void *data;
  int data_size;
  int dir;
};
struct response
{
  int status;
  unsigned char data[RESPONSE_SIZE];
};

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
      unsigned l;
      sz = sizeof (*h) + c->data_size;
      c->data_size = 0;
      st = sanei_usb_read_bulk (s->file, (SANE_Byte *) h, &sz);
      for (l = sz; !st && l != be2cpu32 (h->length); l += sz)
	{
	  DBG (DBG_WARN, "usb wrong read (%d instead %d)\n",
	       c->data_size, be2cpu32 (h->length));
	  sz = be2cpu32 (h->length) - l;
	  st = sanei_usb_read_bulk (s->file, ((SANE_Byte *) h) + l, &sz);

	}

      c->data = h + 1;

      if (st)
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

      c->data_size = sz - sizeof (*h);


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

#define END_OF_MEDIUM			(1<<6)
#define INCORRECT_LENGTH_INDICATOR	(1<<5)
static const struct
{
  unsigned sense, asc, ascq;
  SANE_Status st;
} s_errors[] =
{
  {
  2, 0, 0, SANE_STATUS_DEVICE_BUSY},
  {
  2, 4, 1, SANE_STATUS_DEVICE_BUSY},
  {
  2, 4, 0x80, SANE_STATUS_COVER_OPEN},
  {
  2, 4, 0x81, SANE_STATUS_COVER_OPEN},
  {
  2, 4, 0x82, SANE_STATUS_COVER_OPEN},
  {
  2, 4, 0x83, SANE_STATUS_COVER_OPEN},
  {
  2, 4, 0x84, SANE_STATUS_COVER_OPEN},
  {
  2, 0x80, 1, SANE_STATUS_CANCELLED},
  {
  2, 0x80, 2, SANE_STATUS_CANCELLED},
  {
  3, 0x3a, 0, SANE_STATUS_NO_DOCS},
  {
  3, 0x80, 1, SANE_STATUS_JAMMED},
  {
  3, 0x80, 2, SANE_STATUS_JAMMED},
  {
  3, 0x80, 3, SANE_STATUS_JAMMED},
  {
  3, 0x80, 4, SANE_STATUS_JAMMED},
  {
  3, 0x80, 5, SANE_STATUS_JAMMED},
  {
  3, 0x80, 6, SANE_STATUS_JAMMED},
  {
  3, 0x80, 7, SANE_STATUS_JAMMED},
  {
  3, 0x80, 8, SANE_STATUS_JAMMED},
  {
  3, 0x80, 9, SANE_STATUS_JAMMED},
  {
  3, 0x80, 0xa, SANE_STATUS_JAMMED},
  {
  3, 0x80, 0xb, SANE_STATUS_JAMMED},
  {
  3, 0x80, 0xc, SANE_STATUS_JAMMED},
  {
  3, 0x80, 0xd, SANE_STATUS_JAMMED},
  {
  3, 0x80, 0xe, SANE_STATUS_JAMMED},
  {
  3, 0x80, 0xf, SANE_STATUS_JAMMED},
  {
  3, 0x80, 0x10, SANE_STATUS_JAMMED},
  {
  3, 0x80, 0x11, SANE_STATUS_JAMMED},
  {
  5, 0x1a, 0x0, SANE_STATUS_INVAL},
  {
  5, 0x20, 0x0, SANE_STATUS_INVAL},
  {
  5, 0x24, 0x0, SANE_STATUS_INVAL},
  {
  5, 0x25, 0x0, SANE_STATUS_INVAL},
  {
  5, 0x26, 0x0, SANE_STATUS_INVAL},
  {
  5, 0x2c, 0x01, SANE_STATUS_INVAL},
  {
  5, 0x2c, 0x02, SANE_STATUS_INVAL},
  {
  5, 0x2c, 0x80, SANE_STATUS_INVAL},
  {
  5, 0x2c, 0x81, SANE_STATUS_INVAL},
  {
  5, 0x2c, 0x82, SANE_STATUS_INVAL},
  {
5, 0x2c, 0x83, SANE_STATUS_INVAL},};

SANE_Status
kvs40xx_sense_handler (int __sane_unused__ fd,
		       u_char * sense_buffer, void __sane_unused__ * arg)
{
  unsigned i;
  SANE_Status st = SANE_STATUS_GOOD;
  if (sense_buffer[2] & 0xf)
    {				/*error */
      for (i = 0; i < sizeof (s_errors) / sizeof (s_errors[0]); i++)
	{
	  if ((sense_buffer[2] & 0xf) == s_errors[i].sense
	      && sense_buffer[12] == s_errors[i].asc
	      && sense_buffer[13] == s_errors[i].ascq)
	    {
	      st = s_errors[i].st;
	      break;
	    }
	}
      if (i == sizeof (s_errors) / sizeof (s_errors[0]))
	st = SANE_STATUS_IO_ERROR;
    }
  else
    {
      if (sense_buffer[2] & END_OF_MEDIUM)
	st = SANE_STATUS_EOF;
      else if (sense_buffer[2] & INCORRECT_LENGTH_INDICATOR)
	st = INCORRECT_LENGTH;
    }

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
            {0}, 6,
	    NULL, RESPONSE_SIZE,
	    CMD_IN
	  };
	  c2.cmd[0] = REQUEST_SENSE;
	  c2.cmd[4] = RESPONSE_SIZE;

	  st = usb_send_command (s, &c2, &r, b);
	  if (st)
	    return st;
	  st = kvs40xx_sense_handler (0, b + sizeof (struct bulk_header), NULL);
	}
    }
  else
    {
      if (c->dir == CMD_OUT)
	{
	  memcpy (s->buffer, c->cmd, c->cmd_size);
	  memcpy (s->buffer + c->cmd_size, c->data, c->data_size);
	  st = sanei_scsi_cmd (s->file, s->buffer,
			       c->cmd_size + c->data_size, NULL, NULL);
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
kvs40xx_test_unit_ready (struct scanner * s)
{
  struct cmd c = {
    {0}, 6,
    NULL, 0,
    CMD_NONE
  };
  c.cmd[0] = TEST_UNIT_READY;
  if (send_command (s, &c))
    return SANE_STATUS_DEVICE_BUSY;

  return SANE_STATUS_GOOD;
}

SANE_Status
kvs40xx_set_timeout (struct scanner * s, int timeout)
{
  u16 t = cpu2be16 ((u16) timeout);
  struct cmd c = {
    {0}, 10,
    NULL, 0,
    CMD_OUT
  };
  c.data = &t;
  c.data_size = sizeof (t);
  c.cmd[0] = SET_TIMEOUT;
  c.cmd[2] = 0x8d;
  *((u16 *) (c.cmd + 7)) = cpu2be16 (sizeof (t));
  if (s->bus == USB)
    sanei_usb_set_timeout (timeout * 1000);

  return send_command (s, &c);
}

SANE_Status
kvs40xx_set_window (struct scanner * s, int wnd_id)
{
  struct window wnd;
  struct cmd c = {
    {0}, 10,
    NULL, 0,
    CMD_OUT
  };
  c.data = &wnd;
  c.data_size = sizeof (wnd);
  c.cmd[0] = SET_WINDOW;
  *((u16 *) (c.cmd + 7)) = cpu2be16 (sizeof (wnd));
  kvs40xx_init_window (s, &wnd, wnd_id);

  return send_command (s, &c);
}

SANE_Status
kvs40xx_reset_window (struct scanner * s)
{
  struct cmd c = {
    {0}, 10,
    NULL, 0,
    CMD_NONE
  };
  c.cmd[0] = SET_WINDOW;

  return send_command (s, &c);
}

SANE_Status
kvs40xx_scan (struct scanner * s)
{
  struct cmd c = {
    {0}, 6,
    NULL, 0,
    CMD_NONE
  };
  c.cmd[0] = SCAN;
  return send_command (s, &c);
}

SANE_Status
hopper_down (struct scanner * s)
{
  struct cmd c = {
    {0}, 10,
    NULL, 0,
    CMD_NONE
  };
  c.cmd[0] = HOPPER_DOWN;
  c.cmd[2] = 5;

  if (s->id == KV_S7075C)
    return SANE_STATUS_GOOD;
  return send_command (s, &c);
}

SANE_Status
stop_adf (struct scanner * s)
{
  struct cmd c = {
    {0}, 10,
    NULL, 0,
    CMD_NONE
  };
  c.cmd[0] = STOP_ADF;
  c.cmd[2] = 0x8b;
  return send_command (s, &c);
}

SANE_Status
kvs40xx_document_exist (struct scanner * s)
{
  SANE_Status status;
  struct cmd c = {
    {0}, 10,
    NULL, 6,
    CMD_IN
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
kvs40xx_read_picture_element (struct scanner * s, unsigned side,
			      SANE_Parameters * p)
{
  SANE_Status status;
  struct cmd c = {
     {0}, 10,
     NULL, 16,
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

SANE_Status
get_buffer_status (struct scanner * s, unsigned *data_avalible)
{
  SANE_Status status;
  struct cmd c = {
    {0}, 10,
    NULL, 12,
    CMD_IN
  };
  c.cmd[0] = GET_BUFFER_STATUS;
  c.cmd[7] = 12;

  status = send_command (s, &c);
  if (status)
    return status;
  *data_avalible = get24 ((unsigned char *)c.data + 9);
  return SANE_STATUS_GOOD;
}

SANE_Status
kvs40xx_read_image_data (struct scanner * s, unsigned page, unsigned side,
			 void *buf, unsigned max_size, unsigned *size)
{
  SANE_Status status;
  struct cmd c = {
    {0}, 10,
    NULL, 0,
    CMD_IN
  };
  c.data_size = max_size < MAX_READ_DATA_SIZE ? max_size : MAX_READ_DATA_SIZE;
  c.cmd[0] = READ_10;
  c.cmd[4] = page;
  c.cmd[5] = side;

  set24 (c.cmd + 6, c.data_size);
  *size = 0;
  status = send_command (s, &c);

  if (status && status != SANE_STATUS_EOF && status != INCORRECT_LENGTH)
    return status;

  *size = c.data_size;
  memcpy (buf, c.data, *size);
  return status;
}

static SANE_Status
get_adjust_data (struct scanner * s, unsigned *dummy_length)
{
  SANE_Status status;
  struct cmd c = {
    {0}, 10,
    NULL, 40,
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

SANE_Status
read_support_info (struct scanner * s, struct support_info * inf)
{
  SANE_Status st;
  struct cmd c = {
    {0}, 10,
    NULL, sizeof (*inf),
    CMD_IN
  };

  c.cmd[0] = READ_10;
  c.cmd[2] = SUPPORT_INFO;
  set24 (c.cmd + 6, c.data_size);

  st = send_command (s, &c);
  if (st)
    return st;
  memcpy (inf, c.data, sizeof (*inf));
  return SANE_STATUS_GOOD;
}

SANE_Status
inquiry (struct scanner * s, char *id)
{
  int i;
  SANE_Status st;
  struct cmd c = {
    {0}, 5,
    NULL, 0x60,
    CMD_IN
  };

  c.cmd[0] = INQUIRY;
  c.cmd[4] = c.data_size;

  st = send_command (s, &c);
  if (st)
    return st;
  memcpy (id, (unsigned char *)c.data + 16, 16);
  for (i = 0; i < 15 && id[i] != ' '; i++);
  id[i] = 0;
  return SANE_STATUS_GOOD;
}
