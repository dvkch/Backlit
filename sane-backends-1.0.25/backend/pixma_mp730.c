/* SANE - Scanner Access Now Easy.

   Copyright (C) 2011-2015 Rolf Bensch <rolf at bensch hyphen online dot de>
   Copyright (C) 2007-2008 Nicolas Martin, <nicols-guest at alioth dot debian dot org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>		/* localtime(C90) */

#include "pixma_rename.h"
#include "pixma_common.h"
#include "pixma_io.h"


#ifdef __GNUC__
# define UNUSED(v) (void) v
#else
# define UNUSED(v)
#endif

#define IMAGE_BLOCK_SIZE (0xc000)
#define CMDBUF_SIZE 512

#define MP730_PID 0x262f
#define MP700_PID 0x2630

#define MP360_PID 0x263c
#define MP370_PID 0x263d
#define MP390_PID 0x263e
#define MP375R_PID 0x263f   /* untested */

#define MP740_PID 0x264c	/* Untested */
#define MP710_PID 0x264d

#define MF5730_PID 0x265d	/* Untested */
#define MF5750_PID 0x265e	/* Untested */
#define MF5770_PID 0x265f
#define MF3110_PID 0x2660

#define IR1020_PID 0x26e6

enum mp730_state_t
{
  state_idle,
  state_warmup,
  state_scanning,
  state_transfering,
  state_finished
};

enum mp730_cmd_t
{
  cmd_start_session = 0xdb20,
  cmd_select_source = 0xdd20,
  cmd_gamma         = 0xee20,
  cmd_scan_param    = 0xde20,
  cmd_status        = 0xf320,
  cmd_abort_session = 0xef20,
  cmd_time          = 0xeb80,
  cmd_read_image    = 0xd420,
  cmd_error_info    = 0xff20,

  cmd_activate      = 0xcf60,
  cmd_calibrate     = 0xe920
};

typedef struct mp730_t
{
  enum mp730_state_t state;
  pixma_cmdbuf_t cb;
  unsigned raw_width;
  uint8_t current_status[12];

  uint8_t *buf, *imgbuf, *lbuf;
  unsigned imgbuf_len;

  unsigned last_block:1;
} mp730_t;


static void mp730_finish_scan (pixma_t * s);

static int
has_paper (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  return (mp->current_status[1] == 0);
}

static void
drain_bulk_in (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  while (pixma_read (s->io, mp->imgbuf, IMAGE_BLOCK_SIZE) >= 0);
}

static int
abort_session (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_abort_session);
}

static int
query_status (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  uint8_t *data;
  int error;

  data = pixma_newcmd (&mp->cb, cmd_status, 0, 12);
  error = pixma_exec (s, &mp->cb);
  if (error >= 0)
    {
      memcpy (mp->current_status, data, 12);
      PDBG (pixma_dbg (3, "Current status: paper=%u cal=%u lamp=%u\n",
		       data[1], data[8], data[7]));
    }
  return error;
}

static int
activate (pixma_t * s, uint8_t x)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  uint8_t *data = pixma_newcmd (&mp->cb, cmd_activate, 10, 0);
  data[0] = 1;
  data[3] = x;
  return pixma_exec (s, &mp->cb);
}

static int
start_session (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_start_session);
}

static int
select_source (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  uint8_t *data = pixma_newcmd (&mp->cb, cmd_select_source, 10, 0);
  switch (s->param->source)
    {
    case PIXMA_SOURCE_ADF:
      data[0] = 2;
      break;

    case PIXMA_SOURCE_ADFDUP:
      data[0] = 2;
      data[5] = 3;
      break;

    default:
      data[0] = 1;
      break;
    }
  return pixma_exec (s, &mp->cb);
}

static int
send_scan_param (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  uint8_t *data;

  data = pixma_newcmd (&mp->cb, cmd_scan_param, 0x2e, 0);
  pixma_set_be16 (s->param->xdpi | 0x1000, data + 0x04);
  pixma_set_be16 (s->param->ydpi | 0x1000, data + 0x06);
  pixma_set_be32 (s->param->x, data + 0x08);
  pixma_set_be32 (s->param->y, data + 0x0c);
  pixma_set_be32 (mp->raw_width, data + 0x10);
  pixma_set_be32 (s->param->h, data + 0x14);

  if (s->param->channels == 1)
    {
      if (s->param->depth == 1)
        data[0x18] = 0x01;
      else
        data[0x18] = 0x04;
    }
  else
    data[0x18] = 0x08;

  data[0x19] = s->param->channels * s->param->depth;  /* bits per pixel, for lineart should be 0x01 */
  data[0x1e] = (s->param->depth == 1) ? 0x80 : 0x00;  /* modify for lineart: 0x80 NEW */
  data[0x1f] = (s->param->depth == 1) ? 0x80 : 0x7f;  /* modify for lineart: 0x80 */
  data[0x20] = (s->param->depth == 1) ? 0x01 : 0xff;  /* modify for lineart: 0x01 */
  data[0x23] = 0x81;

  return pixma_exec (s, &mp->cb);
}

static int
calibrate (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_calibrate);
}

static int
read_image_block (pixma_t * s, uint8_t * header, uint8_t * data)
{
  static const uint8_t cmd[10] =	/* 0xd420 cmd */
  { 0xd4, 0x20, 0, 0, 0, 0, 0, IMAGE_BLOCK_SIZE / 256, 4, 0 };
  mp730_t *mp = (mp730_t *) s->subdriver;
  const int hlen = 2 + 4;
  int error, datalen;

  mp->state = state_transfering;
  mp->cb.reslen =
    pixma_cmd_transaction (s, cmd, sizeof (cmd), mp->cb.buf, 512);
  datalen = mp->cb.reslen;
  if (datalen < 0)
    return datalen;

  memcpy (header, mp->cb.buf, hlen);
  if (datalen >= hlen)
    {
      datalen -= hlen;
      memcpy (data, mp->cb.buf + hlen, datalen);
      data += datalen;
      if (mp->cb.reslen == 512)
	{
	  error = pixma_read (s->io, data, IMAGE_BLOCK_SIZE - 512 + hlen);
	  if (error < 0)
	    return error;
	  datalen += error;
	}
    }

  mp->state = state_scanning;
  mp->cb.expected_reslen = 0;
  error = pixma_check_result (&mp->cb);
  if (error < 0)
    return error;
  if (mp->cb.reslen < hlen)
    return PIXMA_EPROTO;
  return datalen;
}

static int
send_time (pixma_t * s)
{
  /* Why does a scanner need a time? */
  time_t now;
  struct tm *t;
  uint8_t *data;
  mp730_t *mp = (mp730_t *) s->subdriver;

  data = pixma_newcmd (&mp->cb, cmd_time, 20, 0);
  pixma_get_time (&now, NULL);
  t = localtime (&now);
  snprintf ((char *) data, 16,
	    "%02d/%02d/%02d %02d:%02d",
	    t->tm_year % 100, t->tm_mon + 1, t->tm_mday,
	    t->tm_hour, t->tm_min);
  PDBG (pixma_dbg (3, "Sending time: '%s'\n", (char *) data));
  return pixma_exec (s, &mp->cb);
}

static int
handle_interrupt (pixma_t * s, int timeout)
{
  uint8_t buf[16];
  int len;

  len = pixma_wait_interrupt (s->io, buf, sizeof (buf), timeout);
  if (len == PIXMA_ETIMEDOUT)
    return 0;
  if (len < 0)
    return len;
  switch (s->cfg->pid)
    {
    case MP360_PID:
    case MP370_PID:
    case MP375R_PID:
    case MP390_PID:
    case MF5730_PID:
    case MF5750_PID:
    case MF5770_PID:
    case MF3110_PID:
    case IR1020_PID:
      if (len != 16)
	{
	  PDBG (pixma_dbg
		(1, "WARNING:unexpected interrupt packet length %d\n", len));
	  return PIXMA_EPROTO;
	}
      if (buf[12] & 0x40)
	query_status (s);
      if (buf[10] & 0x40)
	send_time (s);
      /* FIXME: following is unverified! */
      if (buf[15] & 1)
	s->events = PIXMA_EV_BUTTON2;	/* b/w scan */
      if (buf[15] & 2)
	s->events = PIXMA_EV_BUTTON1;	/* color scan */
      break;

    case MP700_PID:
    case MP730_PID:
    case MP710_PID:
    case MP740_PID:
      if (len != 8)
	{
	  PDBG (pixma_dbg
		(1, "WARNING:unexpected interrupt packet length %d\n", len));
	  return PIXMA_EPROTO;
	}
      if (buf[7] & 0x10)
	s->events = PIXMA_EV_BUTTON1;
      if (buf[5] & 8)
	send_time (s);
      break;

    default:
      PDBG (pixma_dbg (1, "WARNING:unknown interrupt, please report!\n"));
      PDBG (pixma_hexdump (1, buf, len));
    }
  return 1;
}

static int
has_ccd_sensor (pixma_t * s)
{
  return (s->cfg->pid == MP360_PID ||
          s->cfg->pid == MP370_PID ||
          s->cfg->pid == MP375R_PID ||
          s->cfg->pid == MP390_PID ||
          s->cfg->pid == MF5730_PID ||
          s->cfg->pid == MF5750_PID ||
          s->cfg->pid == MF5770_PID);
}

static int
read_error_info (pixma_t * s, void *buf, unsigned size)
{
  unsigned len = 16;
  mp730_t *mp = (mp730_t *) s->subdriver;
  uint8_t *data;
  int error;

  data = pixma_newcmd (&mp->cb, cmd_error_info, 0, len);
  error = pixma_exec (s, &mp->cb);
  if (error < 0)
    return error;
  if (buf && len < size)
    {
      size = len;
      /* NOTE: I've absolutely no idea what the returned data mean. */
      memcpy (buf, data, size);
      error = len;
    }
  return error;
}

static int
step1 (pixma_t * s)
{
  int error;

  error = query_status (s);
  if (error < 0)
    return error;
  if ((s->param->source == PIXMA_SOURCE_ADF
       || s->param->source == PIXMA_SOURCE_ADFDUP)
      && !has_paper (s))
    return PIXMA_ENO_PAPER;
  if (has_ccd_sensor (s))
    {
      switch (s->cfg->pid)
        {
          case MF5730_PID:
          case MF5750_PID:
          case MF5770_PID:
          /* MF57x0: Wait 10 sec before starting for 1st page only */
            if (s->param->adf_pageid == 0)
	      {
                int tmo = 10;  /* like Windows driver, 10 sec CCD calibration ? */
                while (--tmo >= 0)
                  {
                    error = handle_interrupt (s, 1000);		\
                    if (s->cancel)				\
                      return PIXMA_ECANCELED;			\
                    if (error != PIXMA_ECANCELED && error < 0)	\
                      return error;
                    PDBG (pixma_dbg (2, "CCD Calibration ends in %d sec.\n", tmo));
                  }
              }
            break;

          default:
            break;
        }
        
      activate (s, 0);
      error = calibrate (s);
      
      switch (s->cfg->pid)
        {
          case MF5730_PID:
          case MF5750_PID:
          case MF5770_PID:
          /* MF57x0: calibration returns PIXMA_STATUS_FAILED */
            if (error == PIXMA_ECANCELED)
              error = read_error_info (s, NULL, 0);
            break;

          default:
            break;
        }
    }
  if (error >= 0)
    error = activate (s, 0);
  if (error >= 0)
    error = activate (s, 4);
  return error;
}

static void
pack_rgb (const uint8_t * src, unsigned nlines, unsigned w, uint8_t * dst)
{
  unsigned w2, stride;

  w2 = 2 * w;
  stride = 3 * w;
  for (; nlines != 0; nlines--)
    {
      unsigned x;
      for (x = 0; x != w; x++)
	{
	  *dst++ = src[x + 0];
	  *dst++ = src[x + w];
	  *dst++ = src[x + w2];
	}
      src += stride;
    }
}

static int
mp730_open (pixma_t * s)
{
  mp730_t *mp;
  uint8_t *buf;

  mp = (mp730_t *) calloc (1, sizeof (*mp));
  if (!mp)
    return PIXMA_ENOMEM;

  buf = (uint8_t *) malloc (CMDBUF_SIZE);
  if (!buf)
    {
      free (mp);
      return PIXMA_ENOMEM;
    }

  s->subdriver = mp;
  mp->state = state_idle;

  mp->cb.buf = buf;
  mp->cb.size = CMDBUF_SIZE;
  mp->cb.res_header_len = 2;
  mp->cb.cmd_header_len = 10;
  mp->cb.cmd_len_field_ofs = 7;

  PDBG (pixma_dbg (3, "Trying to clear the interrupt buffer...\n"));
  if (handle_interrupt (s, 200) == 0)
    {
      PDBG (pixma_dbg (3, "  no packets in buffer\n"));
    }
  return 0;
}

static void
mp730_close (pixma_t * s)
{
  mp730_t *mp = (mp730_t *) s->subdriver;

  mp730_finish_scan (s);
  free (mp->cb.buf);
  free (mp->buf);
  free (mp);
  s->subdriver = NULL;
}

static unsigned
calc_raw_width (pixma_t * s, const pixma_scan_param_t * sp)
{
  unsigned raw_width;
  /* FIXME: Does MP730 need the alignment? */
  /*  TODO test: MP710/740 */
  if (sp->channels == 1)
    {
      if (sp->depth == 8)   /* grayscale  */
        {
          if (s->cfg->pid == MP700_PID ||
              s->cfg->pid == MP730_PID ||
              s->cfg->pid == MP360_PID ||
              s->cfg->pid == MP370_PID ||
              s->cfg->pid == MP375R_PID ||
              s->cfg->pid == MP390_PID ||
	      s->cfg->pid == IR1020_PID)
            raw_width = ALIGN_SUP (sp->w, 4);
          else
            raw_width = ALIGN_SUP (sp->w, 12);
        }
      else   /* depth = 1 : LINEART */
        raw_width = ALIGN_SUP (sp->w, 16);
    }
  else
    raw_width = ALIGN_SUP (sp->w, 4);
  return raw_width;
}

static int
mp730_check_param (pixma_t * s, pixma_scan_param_t * sp)
{
  uint8_t k = 1;

  /* check if channels is 3, or if depth is 1 then channels also 1 else set depth to 8 */
  if ((sp->channels==3) || !(sp->channels==1 && sp->depth==1))
    {
      sp->depth=8;
    }
  /* for MP360/370, MP700/730 in grayscale & lineart modes, max scan res is 600 dpi */
  if (s->cfg->pid == MP700_PID ||
      s->cfg->pid == MP730_PID ||
      s->cfg->pid == MP360_PID ||
      s->cfg->pid == MP370_PID ||
      s->cfg->pid == MP375R_PID ||
      s->cfg->pid == MP390_PID)
    {
      if (sp->channels == 1)
          k = sp->xdpi / MIN (sp->xdpi, 600);
    }

  sp->x /= k;
  sp->y /= k;
  sp->h /= k;
  sp->xdpi /= k;
  sp->ydpi = sp->xdpi;

  sp->w = calc_raw_width (s, sp);
  sp->w /= k;
  sp->line_size = (calc_raw_width (s, sp) * sp->channels * sp->depth) / 8;

  return 0;
}

static int
mp730_scan (pixma_t * s)
{
  int error, n;
  mp730_t *mp = (mp730_t *) s->subdriver;
  uint8_t *buf;

  if (mp->state != state_idle)
    return PIXMA_EBUSY;

  /* clear interrupt packets buffer */
  while (handle_interrupt (s, 0) > 0)
    {
    }

  mp->raw_width = calc_raw_width (s, s->param);
  PDBG (pixma_dbg (3, "raw_width = %u\n", mp->raw_width));

  n = IMAGE_BLOCK_SIZE / s->param->line_size + 1;
  buf = (uint8_t *) malloc ((n + 1) * s->param->line_size + IMAGE_BLOCK_SIZE);
  if (!buf)
    return PIXMA_ENOMEM;
  mp->buf = buf;
  mp->lbuf = buf;
  mp->imgbuf = buf + n * s->param->line_size;
  mp->imgbuf_len = 0;

  error = step1 (s);
  if (error >= 0)
    error = start_session (s);
  if (error >= 0)
    mp->state = state_scanning;
  if (error >= 0)
    error = select_source (s);
  if (error >= 0)
    error = send_scan_param (s);
  if (error < 0)
    {
      mp730_finish_scan (s);
      return error;
    }
  mp->last_block = 0;
  return 0;
}

static int
mp730_fill_buffer (pixma_t * s, pixma_imagebuf_t * ib)
{
  int error, n;
  mp730_t *mp = (mp730_t *) s->subdriver;
  unsigned block_size, bytes_received;
  uint8_t header[16];

  do
    {
      do
	{
	  if (s->cancel)
	    return PIXMA_ECANCELED;
	  if (mp->last_block)           /* end of image */
	      return 0;

	  error = read_image_block (s, header, mp->imgbuf + mp->imgbuf_len);
	  if (error < 0)
	    return error;

	  bytes_received = error;
	  block_size = pixma_get_be16 (header + 4);
	  mp->last_block = ((header[2] & 0x28) == 0x28);
	  if (mp->last_block)
	    {    /* end of image */
	      mp->state = state_finished;
	    }
	  if ((header[2] & ~0x38) != 0)
	    {
	      PDBG (pixma_dbg (1, "WARNING: Unexpected result header\n"));
	      PDBG (pixma_hexdump (1, header, 16));
	    }
	  PASSERT (bytes_received == block_size);

	  if (block_size == 0)
	    {
	      /* no image data at this moment. */
	      /*pixma_sleep(100000); *//* FIXME: too short, too long? */
	      handle_interrupt (s, 100);
            }
	}
      while (block_size == 0);

      /* TODO: simplify! */
      mp->imgbuf_len += bytes_received;
      n = mp->imgbuf_len / s->param->line_size;
      /* n = number of full lines (rows) we have in the buffer. */
      if (n != 0)
	{
	  if (s->param->channels != 1    &&
	      s->cfg->pid != MF5730_PID  &&
	      s->cfg->pid != MF5750_PID  &&
	      s->cfg->pid != MF5770_PID  &&
	      s->cfg->pid != MF3110_PID  &&
	      s->cfg->pid != IR1020_PID)
	    {
	      /* color, and not an MF57x0 nor MF3110 */
	      pack_rgb (mp->imgbuf, n, mp->raw_width, mp->lbuf);
	    }
	  else
             /* grayscale/lineart or MF57x0 or MF3110 */
             memcpy (mp->lbuf, mp->imgbuf, n * s->param->line_size);

	  block_size = n * s->param->line_size;
	  mp->imgbuf_len -= block_size;
	  memcpy (mp->imgbuf, mp->imgbuf + block_size, mp->imgbuf_len);
	}
    }
  while (n == 0);

  ib->rptr = mp->lbuf;
  ib->rend = mp->lbuf + block_size;
  return ib->rend - ib->rptr;
}

static void
mp730_finish_scan (pixma_t * s)
{
  int error, aborted = 0;
  mp730_t *mp = (mp730_t *) s->subdriver;

  switch (mp->state)
    {
    case state_transfering:
      drain_bulk_in (s);
      /* fall through */
    case state_scanning:
    case state_warmup:
      aborted = 1;
      error = abort_session (s);
      if (error < 0)
	PDBG (pixma_dbg
	      (1, "WARNING:abort_session() failed %s\n",
	       pixma_strerror (error)));
      /* fall through */
    case state_finished:
      query_status (s);
      query_status (s);
      activate (s, 0);

      if (! aborted && s->cfg->pid == IR1020_PID)
	{
	  error = abort_session (s);
	  if (error < 0)
	    {
	      PDBG (pixma_dbg
		    (1, "WARNING:abort_session() failed %s\n",
		     pixma_strerror (error)));
	      query_status (s);
	      query_status (s);
	      activate (s, 0);
	    }
	}
      mp->buf = mp->lbuf = mp->imgbuf = NULL;
      mp->state = state_idle;
      /* fall through */
    case state_idle:
      break;
    }
}

static void
mp730_wait_event (pixma_t * s, int timeout)
{
  /* FIXME: timeout is not correct. See usbGetCompleteUrbNoIntr() for
   * instance. */
  while (s->events == 0 && handle_interrupt (s, timeout) > 0)
    {
    }
}

static int
mp730_get_status (pixma_t * s, pixma_device_status_t * status)
{
  int error;

  error = query_status (s);
  if (error < 0)
    return error;
  status->hardware = PIXMA_HARDWARE_OK;
  status->adf = (has_paper (s)) ? PIXMA_ADF_OK : PIXMA_ADF_NO_PAPER;
  return 0;
}


static const pixma_scan_ops_t pixma_mp730_ops = {
  mp730_open,
  mp730_close,
  mp730_scan,
  mp730_fill_buffer,
  mp730_finish_scan,
  mp730_wait_event,
  mp730_check_param,
  mp730_get_status
};

/* TODO: implement adftpu_min_dpi & adftpu_max_dpi for grayscale & lineart */
#define DEVICE(name, model, pid, dpi, w, h, cap) {           \
              name,              /* name */           \
              model,             /* model */		      \
              0x04a9, pid,       /* vid pid */	      \
              1,                 /* iface */		      \
              &pixma_mp730_ops,  /* ops */            \
              dpi, dpi,          /* xdpi, ydpi */	    \
              0, 0,              /* adftpu_min_dpi & adftpu_max_dpi not used in this subdriver */ \
              0, 0,              /* tpuir_min_dpi & tpuir_max_dpi not used in this subdriver */  \
              w, h,              /* width, height */	\
        PIXMA_CAP_GRAY|PIXMA_CAP_EVENTS|cap           \
}
const pixma_config_t pixma_mp730_devices[] = {
/* TODO: check area limits */
  DEVICE ("PIXMA MP360", "MP360", MP360_PID, 1200, 636, 868, PIXMA_CAP_LINEART),
  DEVICE ("PIXMA MP370", "MP370", MP370_PID, 1200, 636, 868, PIXMA_CAP_LINEART),
  DEVICE ("PIXMA MP375R", "MP375R", MP375R_PID, 1200, 636, 868, PIXMA_CAP_LINEART),
  DEVICE ("PIXMA MP390", "MP390", MP390_PID, 1200, 636, 868, PIXMA_CAP_LINEART),
  DEVICE ("PIXMA MP700", "MP700", MP700_PID, 1200, 638, 877 /*1035 */ , PIXMA_CAP_LINEART),
  DEVICE ("PIXMA MP710", "MP710", MP710_PID, 1200, 637, 868, PIXMA_CAP_LINEART),
  DEVICE ("PIXMA MP730", "MP730", MP730_PID, 1200, 637, 868, PIXMA_CAP_ADF | PIXMA_CAP_LINEART),
  DEVICE ("PIXMA MP740", "MP740", MP740_PID, 1200, 637, 868, PIXMA_CAP_ADF | PIXMA_CAP_LINEART),
  
  DEVICE ("Canon imageCLASS MF5730", "MF5730", MF5730_PID, 1200, 636, 868, PIXMA_CAP_ADF),
  DEVICE ("Canon imageCLASS MF5750", "MF5750", MF5750_PID, 1200, 636, 868, PIXMA_CAP_ADF),
  DEVICE ("Canon imageCLASS MF5770", "MF5770", MF5770_PID, 1200, 636, 868, PIXMA_CAP_ADF),
  DEVICE ("Canon imageCLASS MF3110", "MF3110", MF3110_PID, 600, 636, 868, 0),

  DEVICE ("Canon iR 1020/1024/1025", "iR1020", IR1020_PID, 600, 636, 868, PIXMA_CAP_ADFDUP),

  DEVICE (NULL, NULL, 0, 0, 0, 0, 0)
};
