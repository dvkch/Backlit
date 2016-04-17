/* SANE - Scanner Access Now Easy.

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

/****************************************************************************
 * Credits should go to Martin Schewe (http://pixma.schewe.com) who analysed
 * the protocol of MP750.
 ****************************************************************************/

#include "../include/sane/config.h"

#include <stdlib.h>
#include <string.h>

#include "pixma_rename.h"
#include "pixma_common.h"
#include "pixma_io.h"

/* TODO: remove lines marked with SIM. They are inserted so that I can test
   the subdriver with the simulator. WY */

#ifdef __GNUC__
# define UNUSED(v) (void) v
#else
# define UNUSED(v)
#endif

#define IMAGE_BLOCK_SIZE 0xc000
#define CMDBUF_SIZE 512
#define HAS_PAPER(s) (s[1] == 0)
#define IS_WARMING_UP(s) (s[7] != 3)
#define IS_CALIBRATED(s) (s[8] == 0xf)

#define MP750_PID 0x1706
#define MP760_PID 0x1708
#define MP780_PID 0x1707


enum mp750_state_t
{
  state_idle,
  state_warmup,
  state_scanning,
  state_transfering,
  state_finished
};

enum mp750_cmd_t
{
  cmd_start_session = 0xdb20,
  cmd_select_source = 0xdd20,
  cmd_scan_param = 0xde20,
  cmd_status = 0xf320,
  cmd_abort_session = 0xef20,
  cmd_time = 0xeb80,
  cmd_read_image = 0xd420,

  cmd_activate = 0xcf60,
  cmd_calibrate = 0xe920,
  cmd_error_info = 0xff20
};

typedef struct mp750_t
{
  enum mp750_state_t state;
  pixma_cmdbuf_t cb;
  unsigned raw_width, raw_height;
  uint8_t current_status[12];

  uint8_t *buf, *rawimg, *img;
  /* make new buffer for rgb_to_gray to act on */
  uint8_t *imgcol;
  unsigned line_size; /* need in 2 functions */

  unsigned rawimg_left, imgbuf_len, last_block_size, imgbuf_ofs;
  int shifted_bytes;
  int stripe_shift; /* for 2400dpi */
  unsigned last_block;

  unsigned monochrome:1;
  unsigned needs_abort:1;
} mp750_t;



static void mp750_finish_scan (pixma_t * s);
static void check_status (pixma_t * s);

static int
has_paper (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  return HAS_PAPER (mp->current_status);
}

static int
is_warming_up (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  return IS_WARMING_UP (mp->current_status);
}

static int
is_calibrated (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  return IS_CALIBRATED (mp->current_status);
}

static void
drain_bulk_in (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  while (pixma_read (s->io, mp->buf, IMAGE_BLOCK_SIZE) >= 0);
}

static int
abort_session (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_abort_session);
}

static int
query_status (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
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
  mp750_t *mp = (mp750_t *) s->subdriver;
  uint8_t *data = pixma_newcmd (&mp->cb, cmd_activate, 10, 0);
  data[0] = 1;
  data[3] = x;
  return pixma_exec (s, &mp->cb);
}

static int
activate_cs (pixma_t * s, uint8_t x)
{
   /*SIM*/ check_status (s);
  return activate (s, x);
}

static int
start_session (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_start_session);
}

static int
select_source (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  uint8_t *data = pixma_newcmd (&mp->cb, cmd_select_source, 10, 0);
  data[0] = (s->param->source == PIXMA_SOURCE_ADF) ? 2 : 1;
  data[1] = 1;
  return pixma_exec (s, &mp->cb);
}

static int
has_ccd_sensor (pixma_t * s)
{
  return ((s->cfg->cap & PIXMA_CAP_CCD) != 0);
}

static int
is_ccd_grayscale (pixma_t * s)
{
  return (has_ccd_sensor (s) && (s->param->channels == 1));
}

/* CCD sensors don't have a Grayscale mode, but use color mode instead */
static unsigned
get_cis_ccd_line_size (pixma_t * s)
{
  return (s->param->wx ? s->param->line_size / s->param->w * s->param->wx 
	  : s->param->line_size) * ((is_ccd_grayscale (s)) ? 3 : 1); 
}

static int
send_scan_param (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  uint8_t *data;

  data = pixma_newcmd (&mp->cb, cmd_scan_param, 0x2e, 0);
  pixma_set_be16 (s->param->xdpi | 0x8000, data + 0x04);
  pixma_set_be16 (s->param->ydpi | 0x8000, data + 0x06);
  pixma_set_be32 (s->param->x, data + 0x08);
  pixma_set_be32 (s->param->y, data + 0x0c);
  pixma_set_be32 (mp->raw_width, data + 0x10);
  pixma_set_be32 (mp->raw_height, data + 0x14);
  data[0x18] = 8;		/* 8 = color, 4 = grayscale(?) */
  /* GH: No, there is no grayscale for CCD devices, Windows shows same  */
  data[0x19] = s->param->depth * ((is_ccd_grayscale (s)) ? 3 : s->param->channels);	/* bits per pixel */
  data[0x20] = 0xff;
  data[0x23] = 0x81;
  data[0x26] = 0x02;
  data[0x27] = 0x01;
  data[0x29] = mp->monochrome ? 0 : 1;

  return pixma_exec (s, &mp->cb);
}

static int
calibrate (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_calibrate);
}

static int
calibrate_cs (pixma_t * s)
{
   /*SIM*/ check_status (s);
  return calibrate (s);
}

static int
request_image_block_ex (pixma_t * s, unsigned *size, uint8_t * info,
			unsigned flag)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  int error;

  memset (mp->cb.buf, 0, 10);
  pixma_set_be16 (cmd_read_image, mp->cb.buf);
  mp->cb.buf[7] = *size >> 8;
  mp->cb.buf[8] = 4 | flag;
  mp->cb.reslen = pixma_cmd_transaction (s, mp->cb.buf, 10, mp->cb.buf, 6);
  mp->cb.expected_reslen = 0;
  error = pixma_check_result (&mp->cb);
  if (error >= 0)
    {
      if (mp->cb.reslen == 6)
        {
          *info = mp->cb.buf[2];
          *size = pixma_get_be16 (mp->cb.buf + 4);
        }
      else
        {
          error = PIXMA_EPROTO;
        }
    }
  return error;
}

static int
request_image_block (pixma_t * s, unsigned *size, uint8_t * info)
{
  return request_image_block_ex (s, size, info, 0);
}

static int
request_image_block2 (pixma_t * s, uint8_t * info)
{
  unsigned temp = 0;
  return request_image_block_ex (s, &temp, info, 0x20);
}

static int
read_image_block (pixma_t * s, uint8_t * data)
{
  int count, temp;

  count = pixma_read (s->io, data, IMAGE_BLOCK_SIZE);
  if (count < 0)
    return count;
  if (count == IMAGE_BLOCK_SIZE)
    {
      int error = pixma_read (s->io, &temp, 0);
      if (error < 0)
        {
          PDBG (pixma_dbg
          (1, "WARNING: reading zero-length packet failed %d\n", error));
        }
    }
  return count;
}

static int
read_error_info (pixma_t * s, void *buf, unsigned size)
{
  unsigned len = 16;
  mp750_t *mp = (mp750_t *) s->subdriver;
  uint8_t *data;
  int error;

  data = pixma_newcmd (&mp->cb, cmd_error_info, 0, len);
  error = pixma_exec (s, &mp->cb);
  if (error >= 0 && buf)
    {
      if (len < size)
        size = len;
      /* NOTE: I've absolutely no idea what the returned data mean. */
      memcpy (buf, data, size);
      error = len;
    }
  return error;
}

static int
send_time (pixma_t * s)
{
  /* TODO: implement send_time() */
  UNUSED (s);
  PDBG (pixma_dbg (3, "send_time() is not yet implemented.\n"));
  return 0;
}

static int
handle_interrupt (pixma_t * s, int timeout)
{
  int error;
  uint8_t intr[16];

  error = pixma_wait_interrupt (s->io, intr, sizeof (intr), timeout);
  if (error == PIXMA_ETIMEDOUT)
    return 0;
  if (error < 0)
    return error;
  if (error != 16)
    {
      PDBG (pixma_dbg (1, "WARNING: unexpected interrupt packet length %d\n",
		       error));
      return PIXMA_EPROTO;
    }

  if (intr[10] & 0x40)
    send_time (s);
  if (intr[12] & 0x40)
    query_status (s);
  if (intr[15] & 1)
    s->events = PIXMA_EV_BUTTON2;	/* b/w scan */
  if (intr[15] & 2)
    s->events = PIXMA_EV_BUTTON1;	/* color scan */
  return 1;
}

static void
check_status (pixma_t * s)
{
  while (handle_interrupt (s, 0) > 0);
}

static int
step1 (pixma_t * s)
{
  int error, tmo;

  error = activate (s, 0);
  if (error < 0)
    return error;
  error = query_status (s);
  if (error < 0)
    return error;
  if (s->param->source == PIXMA_SOURCE_ADF && !has_paper (s))
    return PIXMA_ENO_PAPER;
  error = activate_cs (s, 0);
   /*SIM*/ if (error < 0)
    return error;
  error = activate_cs (s, 0x20);
  if (error < 0)
    return error;

  tmo = 60;
  error = calibrate_cs (s);
  while (error == PIXMA_EBUSY && --tmo >= 0)
    {
      if (s->cancel)
	return PIXMA_ECANCELED;
      PDBG (pixma_dbg
	    (2, "Scanner is busy. Timed out in %d sec.\n", tmo + 1));
      pixma_sleep (1000000);
      error = calibrate_cs (s);
    }
  return error;
}

static void
shift_rgb (const uint8_t * src, unsigned pixels,
	   int sr, int sg, int sb, int stripe_shift,
	   int line_size, uint8_t * dst)
{
  unsigned st;

  for (; pixels != 0; pixels--)
    {
      st = (pixels % 2 == 0) ? -2 * stripe_shift * line_size : 0;
      *(dst++ + sr + st) = *src++;
      *(dst++ + sg + st) = *src++;
      *(dst++ + sb + st) = *src++;
    }
}

static uint8_t *
rgb_to_gray (uint8_t * gptr, const uint8_t * cptr, unsigned pixels, unsigned c)
{
  unsigned i, j, g;

  /* gptr: destination gray scale buffer */
  /* cptr: source color scale buffer */
  /* c: 3 for 3-channel single-byte data, 6 for double-byte data */

  for (i=0; i < pixels; i++)
    {
      for (j = 0, g = 0; j < 3; j++)
        {
          g += *cptr++;
	  if (c == 6) g += (*cptr++ << 8);
        }
      g /= 3;
      *gptr++ = g;
      if (c == 6) *gptr++ = (g >> 8);
    }
  return gptr;
}

static int
calc_component_shifting (pixma_t * s)
{
  switch (s->cfg->pid)
    {
    case MP760_PID:
      switch (s->param->ydpi)
	{
	case 300:
	  return 3;
	case 600:
	  return 6;
	default:
	  return s->param->ydpi / 75;
	}
      /* never reached */
      break;

    case MP750_PID:
    case MP780_PID:
    default:
      return 2 * s->param->ydpi / 75;
    }
}

static void
workaround_first_command (pixma_t * s)
{
  /* FIXME: Send a dummy command because the device doesn't response to the
     first command that is sent directly after the USB interface has been
     set up. Why? USB isn't set up properly? */
  uint8_t cmd[10];
  int error;

  if (s->cfg->pid == MP750_PID)
    return;			/* MP750 doesn't have this problem(?) */

  PDBG (pixma_dbg
	(1,
	 "Work-around for the problem: device doesn't response to the first command.\n"));
  memset (cmd, 0, sizeof (cmd));
  pixma_set_be16 (cmd_calibrate, cmd);
  error = pixma_write (s->io, cmd, 10);
  if (error != 10)
    {
      if (error < 0)
	{
	  PDBG (pixma_dbg
		(1, "  Sending a dummy command failed: %s\n",
		 pixma_strerror (error)));
	}
      else
	{
	  PDBG (pixma_dbg
		(1, "  Sending a dummy command failed: count = %d\n", error));
	}
      return;
    }
  error = pixma_read (s->io, cmd, sizeof (cmd));
  if (error >= 0)
    {
      PDBG (pixma_dbg
	    (1, "  Got %d bytes response from a dummy command.\n", error));
    }
  else
    {
      PDBG (pixma_dbg
	    (1, "  Reading response of a dummy command failed: %s\n",
	     pixma_strerror (error)));
    }
}

static int
mp750_open (pixma_t * s)
{
  mp750_t *mp;
  uint8_t *buf;

  mp = (mp750_t *) calloc (1, sizeof (*mp));
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

  /* ofs:   0   1    2  3  4  5  6  7  8  9
     cmd: cmd1 cmd2 00 00 00 00 00 00 00 00
     data length-^^^^^    => cmd_len_field_ofs
     |--------- cmd_header_len --------|

     res: res1 res2
     |---------| res_header_len
   */
  mp->cb.buf = buf;
  mp->cb.size = CMDBUF_SIZE;
  mp->cb.res_header_len = 2;
  mp->cb.cmd_header_len = 10;
  mp->cb.cmd_len_field_ofs = 7;

  handle_interrupt (s, 200);
  workaround_first_command (s);
  return 0;
}

static void
mp750_close (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;

  mp750_finish_scan (s);
  free (mp->cb.buf);
  free (mp);
  s->subdriver = NULL;
}

static int
mp750_check_param (pixma_t * s, pixma_scan_param_t * sp)
{
  unsigned raw_width;

  UNUSED (s);

  sp->depth = 8;		/* FIXME: Does MP750 supports other depth? */

  /* GH: my implementation */
  /*   if ((sp->channels == 3) || (is_ccd_grayscale (s)))
    raw_width = ALIGN_SUP (sp->w, 4);
  else
  raw_width = ALIGN_SUP (sp->w, 12);*/

  /* the above code gives segmentation fault?!? why... it seems to work in the mp750_scan function */
  raw_width = ALIGN_SUP (sp->w, 4);

  /*sp->line_size = raw_width * sp->channels;*/
  sp->line_size = raw_width * sp->channels * (sp->depth / 8);  /* no cropping? */
  return 0;
}

static int
mp750_scan (pixma_t * s)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  int error;
  uint8_t *buf;
  unsigned size, dpi, spare;

  dpi = s->param->ydpi;
  /* add a stripe shift for 2400dpi */
  mp->stripe_shift = (dpi == 2400) ? 4 : 0;

  if (mp->state != state_idle)
    return PIXMA_EBUSY;

  /* clear interrupt packets buffer */
  while (handle_interrupt (s, 0) > 0)
    {
    }

  /*  if (s->param->channels == 1)
    mp->raw_width = ALIGN_SUP (s->param->w, 12);
  else
    mp->raw_width = ALIGN_SUP (s->param->w, 4);*/

  /* change to use CCD grayscale mode --- why does this give segmentation error at runtime in mp750_check_param? */
  if ((s->param->channels == 3) || (is_ccd_grayscale (s)))
    mp->raw_width = ALIGN_SUP (s->param->w, 4);
  else
    mp->raw_width = ALIGN_SUP (s->param->w, 12);
  /* not sure about MP750, but there is no need for aligning at 12 for the MP760/770, MP780/790 since always use CCD color mode */

  /* modify for stripe shift */
  spare = 2 * calc_component_shifting (s) + 2 * mp->stripe_shift; /* FIXME: or maybe (2*... + 1)? */
  mp->raw_height = s->param->h + spare;
  PDBG (pixma_dbg (3, "raw_width=%u raw_height=%u dpi=%u\n",
		   mp->raw_width, mp->raw_height, dpi));

  /* PDBG (pixma_dbg (4, "line_size=%"PRIu64"\n",s->param->line_size)); */

  mp->line_size = get_cis_ccd_line_size (s); /* scanner hardware line_size multiplied by 3 for CCD grayscale */

  size = 8 + 2 * IMAGE_BLOCK_SIZE + spare * mp->line_size;
  buf = (uint8_t *) malloc (size);
  if (!buf)
    return PIXMA_ENOMEM;
  mp->buf = buf;
  mp->rawimg = buf;
  mp->imgbuf_ofs = spare * mp->line_size;
  mp->imgcol = mp->rawimg + IMAGE_BLOCK_SIZE + 8; /* added to make rgb->gray */
  mp->img = mp->rawimg + IMAGE_BLOCK_SIZE + 8;
  mp->imgbuf_len = IMAGE_BLOCK_SIZE + mp->imgbuf_ofs;
  mp->rawimg_left = 0;
  mp->last_block_size = 0;
  mp->shifted_bytes = -(int) mp->imgbuf_ofs;

  error = step1 (s);
  if (error >= 0)
    error = start_session (s);
  if (error >= 0)
    mp->state = state_warmup;
  if (error >= 0)
    error = select_source (s);
  if (error >= 0)
    error = send_scan_param (s);
  if (error < 0)
    {
      mp750_finish_scan (s);
      return error;
    }
  return 0;
}


static int
mp750_fill_buffer (pixma_t * s, pixma_imagebuf_t * ib)
{
  mp750_t *mp = (mp750_t *) s->subdriver;
  int error;
  uint8_t info;
  unsigned block_size, bytes_received, n;
  int shift[3], base_shift;
  int c;

  c = ((is_ccd_grayscale (s)) ? 3 : s->param->channels) * s->param->depth / 8; /* single-byte or double-byte data */

  if (mp->state == state_warmup)
    {
      int tmo = 60;

      query_status (s);
      check_status (s);
 /*SIM*/ while (!is_calibrated (s) && --tmo >= 0)
        {
          if (s->cancel)
            return PIXMA_ECANCELED;
          if (handle_interrupt (s, 1000) > 0)
            {
              block_size = 0;
              error = request_image_block (s, &block_size, &info);
               /*SIM*/ if (error < 0)
          return error;
            }
        }
      if (tmo < 0)
        {
          PDBG (pixma_dbg (1, "WARNING: Timed out waiting for calibration\n"));
          return PIXMA_ETIMEDOUT;
        }
      pixma_sleep (100000);
      query_status (s);
      if (is_warming_up (s) || !is_calibrated (s))
        {
          PDBG (pixma_dbg (1, "WARNING: Wrong status: wup=%d cal=%d\n",
               is_warming_up (s), is_calibrated (s)));
          return PIXMA_EPROTO;
        }
      block_size = 0;
      request_image_block (s, &block_size, &info);
       /*SIM*/ mp->state = state_scanning;
      mp->last_block = 0;
    }

  /* TODO: Move to other place, values are constant. */
  base_shift = calc_component_shifting (s) * mp->line_size;
  if (s->param->source == PIXMA_SOURCE_ADF)
    {
      shift[0] = 0;
      shift[1] = -base_shift;
      shift[2] = -2 * base_shift;
    }
  else
    {
      shift[0] = -2 * base_shift;
      shift[1] = -base_shift;
      shift[2] = 0;
    }

  do
    {
      if (mp->last_block_size > 0)
        {
          block_size = mp->imgbuf_len - mp->last_block_size;
          memcpy (mp->img, mp->img + mp->last_block_size, block_size);
        }

      do
        {
          if (s->cancel)
            return PIXMA_ECANCELED;
          if (mp->last_block)
            {
              /* end of image */
              info = mp->last_block;
              if (info != 0x38)
                {
                  query_status (s);
                   /*SIM*/ while ((info & 0x28) != 0x28)
                    {
                      pixma_sleep (10000);
                      error = request_image_block2 (s, &info);
                      if (s->cancel)
                        return PIXMA_ECANCELED;	/* FIXME: Is it safe to cancel here? */
                      if (error < 0)
                        return error;
                    }
                }
              mp->needs_abort = (info != 0x38);
              mp->last_block = info;
              mp->state = state_finished;
              return 0;
            }

          check_status (s);
           /*SIM*/ while (handle_interrupt (s, 1) > 0);
           /*SIM*/ block_size = IMAGE_BLOCK_SIZE;
          error = request_image_block (s, &block_size, &info);
          if (error < 0)
            {
              if (error == PIXMA_ECANCELED)
                read_error_info (s, NULL, 0);
              return error;
            }
          mp->last_block = info;
          if ((info & ~0x38) != 0)
            {
              PDBG (pixma_dbg (1, "WARNING: Unknown info byte %x\n", info));
            }
          if (block_size == 0)
            {
              /* no image data at this moment. */
              pixma_sleep (10000);
            }
        }
      while (block_size == 0);

      error = read_image_block (s, mp->rawimg + mp->rawimg_left);
      if (error < 0)
	{
	  mp->state = state_transfering;
	  return error;
	}
      bytes_received = error;
      PASSERT (bytes_received == block_size);

      /* TODO: simplify! */
      mp->rawimg_left += bytes_received;
      n = mp->rawimg_left / 3;
      /* n = number of pixels in the buffer? */

      /* Color to Grayscale converion for CCD sensor */
      if (is_ccd_grayscale (s)) {
	shift_rgb (mp->rawimg, n, shift[0], shift[1], shift[2], mp->stripe_shift, mp->line_size, 
		   mp->imgcol + mp->imgbuf_ofs); 
	/* dst: img, src: imgcol */
	rgb_to_gray (mp->img, mp->imgcol, n, c); /* cropping occurs later? */  
	PDBG (pixma_dbg (4, "*fill_buffer: did grayscale conversion \n"));
      }
      /* Color image processing */
      else {
	shift_rgb (mp->rawimg, n, shift[0], shift[1], shift[2], mp->stripe_shift, mp->line_size, 
		   mp->img + mp->imgbuf_ofs);
	PDBG (pixma_dbg (4, "*fill_buffer: no grayscale conversion---keep color \n")); 
      }

      /* entering remaining unprocessed bytes after last complete pixel into mp->rawimg buffer -- no influence on mp->img */
      n *= 3;
      mp->shifted_bytes += n;
      mp->rawimg_left -= n;	/* rawimg_left = 0, 1 or 2 bytes left in the buffer. */
      mp->last_block_size = n;
      memcpy (mp->rawimg, mp->rawimg + n, mp->rawimg_left);

    }
  while (mp->shifted_bytes <= 0);

  if ((unsigned) mp->shifted_bytes < mp->last_block_size) 
    {
      if (is_ccd_grayscale (s))
	ib->rptr = mp->img + mp->last_block_size/3 - mp->shifted_bytes/3; /* testing---works OK */
      else
	ib->rptr = mp->img + mp->last_block_size - mp->shifted_bytes;
    }
  else
    ib->rptr = mp->img;
  if (is_ccd_grayscale (s))
    ib->rend = mp->img + mp->last_block_size/3; /* testing---works OK */
  else
    ib->rend = mp->img + mp->last_block_size; 
  return ib->rend - ib->rptr;
}

static void
mp750_finish_scan (pixma_t * s)
{
  int error;
  mp750_t *mp = (mp750_t *) s->subdriver;

  switch (mp->state)
    {
    case state_transfering:
      drain_bulk_in (s);
      /* fall through */
    case state_scanning:
    case state_warmup:
      error = abort_session (s);
      if (error == PIXMA_ECANCELED)
	read_error_info (s, NULL, 0);
      /* fall through */
    case state_finished:
      if (s->param->source == PIXMA_SOURCE_FLATBED)
	{
	   /*SIM*/ query_status (s);
	  if (abort_session (s) == PIXMA_ECANCELED)
	    {
	      read_error_info (s, NULL, 0);
	      query_status (s);
	    }
	}
      query_status (s);
       /*SIM*/ activate (s, 0);
      if (mp->needs_abort)
	{
	  mp->needs_abort = 0;
	  abort_session (s);
	}
      free (mp->buf);
      mp->buf = mp->rawimg = NULL;
      mp->state = state_idle;
      /* fall through */
    case state_idle:
      break;
    }
}

static void
mp750_wait_event (pixma_t * s, int timeout)
{
  /* FIXME: timeout is not correct. See usbGetCompleteUrbNoIntr() for
   * instance. */
  while (s->events == 0 && handle_interrupt (s, timeout) > 0)
    {
    }
}

static int
mp750_get_status (pixma_t * s, pixma_device_status_t * status)
{
  int error;

  error = query_status (s);
  if (error < 0)
    return error;
  status->hardware = PIXMA_HARDWARE_OK;
  status->adf = (has_paper (s)) ? PIXMA_ADF_OK : PIXMA_ADF_NO_PAPER;
  status->cal =
    (is_calibrated (s)) ? PIXMA_CALIBRATION_OK : PIXMA_CALIBRATION_OFF;
  status->lamp = (is_warming_up (s)) ? PIXMA_LAMP_WARMING_UP : PIXMA_LAMP_OK;
  return 0;
}


static const pixma_scan_ops_t pixma_mp750_ops = {
  mp750_open,
  mp750_close,
  mp750_scan,
  mp750_fill_buffer,
  mp750_finish_scan,
  mp750_wait_event,
  mp750_check_param,
  mp750_get_status
};

#define DEVICE(name, model, pid, dpi, cap) {		\
	name,                  /* name */		\
	model,                 /* model */		\
	0x04a9, pid,           /* vid pid */		\
	0,                     /* iface */		\
	&pixma_mp750_ops,      /* ops */		\
	dpi, 2*(dpi),          /* xdpi, ydpi */		\
        0, 0,                  /* adftpu_min_dpi & adftpu_max_dpi not used in this subdriver */ \
        0, 0,                  /* tpuir_min_dpi & tpuir_max_dpi not used in this subdriver */   \
	637, 877,              /* width, height */	\
        PIXMA_CAP_GRAY|PIXMA_CAP_EVENTS|cap             \
}

const pixma_config_t pixma_mp750_devices[] = {
  DEVICE ("Canon PIXMA MP750", "MP750", MP750_PID, 2400, PIXMA_CAP_CCD | PIXMA_CAP_ADF),
  DEVICE ("Canon PIXMA MP760/770", "MP760/770", MP760_PID, 2400, PIXMA_CAP_CCD | PIXMA_CAP_TPU),
  DEVICE ("Canon PIXMA MP780/790", "MP780/790", MP780_PID, 2400, PIXMA_CAP_CCD | PIXMA_CAP_ADF),
  DEVICE (NULL, NULL, 0, 0, 0)
};
