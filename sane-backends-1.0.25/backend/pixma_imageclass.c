/* SANE - Scanner Access Now Easy.

   Copyright (C) 2011-2015 Rolf Bensch <rolf at bensch hyphen online dot de>
   Copyright (C) 2007-2009 Nicolas Martin, <nicols-guest at alioth dot debian dot org>
   Copyright (C) 2008 Dennis Lou, dlou 99 at yahoo dot com

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
 * imageCLASS backend based on pixma_mp730.c
 */

#include "../include/sane/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pixma_rename.h"
#include "pixma_common.h"
#include "pixma_io.h"


#ifdef __GNUC__
# define UNUSED(v) (void) v
#else
# define UNUSED(v)
#endif

#define IMAGE_BLOCK_SIZE (0x80000)
#define MAX_CHUNK_SIZE   (0x1000)
#define MIN_CHUNK_SIZE   (0x0200)
#define CMDBUF_SIZE 512

#define MF4100_PID 0x26a3
#define MF4600_PID 0x26b0
#define MF4010_PID 0x26b4
#define MF4200_PID 0x26b5
#define MF4360_PID 0x26ec
#define D480_PID   0x26ed
#define MF4320_PID 0x26ee
#define D420_PID   0x26ef
#define MF3200_PID 0x2684
#define MF6500_PID 0x2686
/* generation 2 scanners (>=0x2707) */
#define MF8300_PID 0x2708
#define MF4500_PID 0x2736
#define MF4410_PID 0x2737
#define MF3010_PID 0x2759
#define MF4570_PID 0x275a
#define MF4800_PID 0x2773
#define MF4700_PID 0x2774
#define MF8200_PID 0x2779
/* the following are all untested */
#define MF5630_PID 0x264e
#define MF5650_PID 0x264f
#define MF8100_PID 0x2659
#define MF5880_PID 0x26f9
#define MF6680_PID 0x26fa
#define MF8030_PID 0x2707
#define IR1133_PID 0x2742
#define MF5900_PID 0x2743
#define D530_PID   0x2775
#define MF8500_PID 0x277a
#define MF6100_PID 0x278e
#define MF820_PID  0x27a6
#define MF220_PID  0x27a8
#define MF210_PID  0x27a9


enum iclass_state_t
{
  state_idle,
  state_warmup,			/* MF4200 always warm/calibrated; others? */
  state_scanning,
  state_finished
};

enum iclass_cmd_t
{
  cmd_start_session = 0xdb20,
  cmd_select_source = 0xdd20,
  cmd_scan_param = 0xde20,
  cmd_status = 0xf320,
  cmd_abort_session = 0xef20,
  cmd_read_image  = 0xd420,
  cmd_read_image2 = 0xd460,     /* New multifunctionals, such as MF4410 */
  cmd_error_info = 0xff20,

  cmd_activate = 0xcf60
};

typedef struct iclass_t
{
  enum iclass_state_t state;
  pixma_cmdbuf_t cb;
  unsigned raw_width;
  uint8_t current_status[12];

  uint8_t *buf, *blkptr, *lineptr;
  unsigned buf_len, blk_len;

  unsigned last_block;

  uint8_t generation;           /* New multifunctionals are (generation == 2) */
} iclass_t;


static void iclass_finish_scan (pixma_t * s);

/* checksumming is sometimes different than pixmas */
static int
iclass_exec (pixma_t * s, pixma_cmdbuf_t * cb, char invcksum)
{
  if (cb->cmdlen > cb->cmd_header_len)
    pixma_fill_checksum (cb->buf + cb->cmd_header_len,
			 cb->buf + cb->cmdlen - 2);
  cb->buf[cb->cmdlen - 1] = invcksum ? -cb->buf[cb->cmdlen - 2] : 0;
  cb->reslen =
    pixma_cmd_transaction (s, cb->buf, cb->cmdlen, cb->buf,
			   cb->expected_reslen);
  return pixma_check_result (cb);
}

static int
has_paper (pixma_t * s)
{
  iclass_t *mf = (iclass_t *) s->subdriver;
  return ((mf->current_status[1] & 0x0f) == 0);         /* allow 0x10 as ADF paper OK */
}

static int
abort_session (pixma_t * s)
{
  iclass_t *mf = (iclass_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mf->cb, cmd_abort_session);
}

static int
query_status (pixma_t * s)
{
  iclass_t *mf = (iclass_t *) s->subdriver;
  uint8_t *data;
  int error;

  data = pixma_newcmd (&mf->cb, cmd_status, 0, 12);
  error = pixma_exec (s, &mf->cb);
  if (error >= 0)
    {
      memcpy (mf->current_status, data, 12);
      DBG (3, "Current status: paper=%u cal=%u lamp=%u\n",
	   data[1], data[8], data[7]);
      PDBG (pixma_dbg (3, "Current status: paper=%u cal=%u lamp=%u\n",
		       data[1], data[8], data[7]));
    }
  return error;
}

static int
activate (pixma_t * s, uint8_t x)
{
  iclass_t *mf = (iclass_t *) s->subdriver;
  uint8_t *data = pixma_newcmd (&mf->cb, cmd_activate, 10, 0);
  data[0] = 1;
  data[3] = x;
  switch (s->cfg->pid)
    {
    case MF4200_PID:
    case MF4600_PID:
    case MF6500_PID:
    case D480_PID:
    case D420_PID:
    case MF4360_PID:
    case MF4100_PID:
    case MF8300_PID:
      return iclass_exec (s, &mf->cb, 1);
      break;
    default:
      return pixma_exec (s, &mf->cb);
    }
}

static int
start_session (pixma_t * s)
{
  iclass_t *mf = (iclass_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mf->cb, cmd_start_session);
}

static int
select_source (pixma_t * s)
{
  iclass_t *mf = (iclass_t *) s->subdriver;
  uint8_t *data = pixma_newcmd (&mf->cb, cmd_select_source, 10, 0);
  data[0] = (s->param->source == PIXMA_SOURCE_ADF ||
             s->param->source == PIXMA_SOURCE_ADFDUP) ? 2 : 1;
  data[5] = (s->param->source == PIXMA_SOURCE_ADFDUP) ? 3 : 0;
  switch (s->cfg->pid)
    {
    case MF4200_PID:
    case MF4600_PID:
    case MF6500_PID:
    case D480_PID:
    case D420_PID:
    case MF4360_PID:
    case MF4100_PID:
    case MF8300_PID:
      return iclass_exec (s, &mf->cb, 0);
      break;
    default:
      return pixma_exec (s, &mf->cb);
    }
}

static int
send_scan_param (pixma_t * s)
{
  iclass_t *mf = (iclass_t *) s->subdriver;
  uint8_t *data;

  data = pixma_newcmd (&mf->cb, cmd_scan_param, 0x2e, 0);
  pixma_set_be16 (s->param->xdpi | 0x1000, data + 0x04);
  pixma_set_be16 (s->param->ydpi | 0x1000, data + 0x06);
  pixma_set_be32 (s->param->x, data + 0x08);
  pixma_set_be32 (s->param->y, data + 0x0c);
  pixma_set_be32 (mf->raw_width, data + 0x10);
  pixma_set_be32 (s->param->h, data + 0x14);
  data[0x18] = (s->param->channels == 1) ? 0x04 : 0x08;
  data[0x19] = s->param->channels * s->param->depth;	/* bits per pixel */
  data[0x1f] = 0x7f;
  data[0x20] = 0xff;
  data[0x23] = 0x81;
  switch (s->cfg->pid)
    {
    case MF4200_PID:
    case MF4600_PID:
    case MF6500_PID:
    case D480_PID:
    case D420_PID:
    case MF4360_PID:
    case MF4100_PID:
    case MF8300_PID:
      return iclass_exec (s, &mf->cb, 0);
      break;
    default:
      return pixma_exec (s, &mf->cb);
    }
}

static int
request_image_block (pixma_t * s, unsigned flag, uint8_t * info,
		     unsigned * size, uint8_t * data, unsigned * datalen)
{
  iclass_t *mf = (iclass_t *) s->subdriver;
  int error;
  unsigned expected_len;
  const int hlen = 2 + 6;

  memset (mf->cb.buf, 0, 11);
  pixma_set_be16 (((mf->generation >= 2) ? cmd_read_image2 : cmd_read_image), mf->cb.buf);
  mf->cb.buf[8] = flag;
  mf->cb.buf[10] = 0x06;
  expected_len = (mf->generation >= 2 ||
                  s->cfg->pid == MF4600_PID ||
                  s->cfg->pid == MF6500_PID ||
                  s->cfg->pid == MF8030_PID) ? 512 : hlen;
  mf->cb.reslen = pixma_cmd_transaction (s, mf->cb.buf, 11, mf->cb.buf, expected_len);
  if (mf->cb.reslen >= hlen)
    {
      *info = mf->cb.buf[2];
      *size = pixma_get_be16 (mf->cb.buf + 6);    /* 16bit size */
      error = 0;

      if (mf->generation >= 2 ||
          s->cfg->pid == MF4600_PID ||
          s->cfg->pid == MF6500_PID ||
          s->cfg->pid == MF8030_PID)
        {                                         /* 32bit size */
          *datalen = mf->cb.reslen - hlen;
          *size = (*datalen + hlen == 512) ? pixma_get_be32 (mf->cb.buf + 4) - *datalen : 0;
          memcpy (data, mf->cb.buf + hlen, *datalen);
        }
     PDBG (pixma_dbg (11, "*request_image_block***** size = %u *****\n", *size));
    }
  else
    {
       error = PIXMA_EPROTO;
    }
  return error;
}

static int
read_image_block (pixma_t * s, uint8_t * data, unsigned size)
{
  iclass_t *mf = (iclass_t *) s->subdriver;
  int error;
  unsigned maxchunksize, chunksize, count = 0;
  
  maxchunksize = MAX_CHUNK_SIZE * ((mf->generation >= 2 ||
                                    s->cfg->pid == MF4600_PID ||
                                    s->cfg->pid == MF6500_PID ||
                                    s->cfg->pid == MF8030_PID) ? 4 : 1);
  while (size)
    {
      if (size >= maxchunksize)
      	chunksize = maxchunksize;
      else if (size < MIN_CHUNK_SIZE)
      	chunksize = size;
      else
      	chunksize = size - (size % MIN_CHUNK_SIZE);
      error = pixma_read (s->io, data, chunksize);
      if (error < 0)
      	return count;
      count += error;
      data += error;
      size -= error;
    }
  return count;
}

static int
read_error_info (pixma_t * s, void *buf, unsigned size)
{
  unsigned len = 16;
  iclass_t *mf = (iclass_t *) s->subdriver;
  uint8_t *data;
  int error;

  data = pixma_newcmd (&mf->cb, cmd_error_info, 0, len);
  switch (s->cfg->pid)
    {
    case MF4200_PID:
    case MF4600_PID:
    case MF6500_PID:
    case D480_PID:
    case D420_PID:
    case MF4360_PID:
    case MF4100_PID:
    case MF8300_PID:
      error = iclass_exec (s, &mf->cb, 0);
      break;
    default:
      error = pixma_exec (s, &mf->cb);
    }
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
handle_interrupt (pixma_t * s, int timeout)
{
  uint8_t buf[16];
  int len;

  len = pixma_wait_interrupt (s->io, buf, sizeof (buf), timeout);
  if (len == PIXMA_ETIMEDOUT)
    return 0;
  if (len < 0)
    return len;
  if (len != 16)
    {
      PDBG (pixma_dbg
	    (1, "WARNING:unexpected interrupt packet length %d\n", len));
      return PIXMA_EPROTO;
    }
  if (buf[12] & 0x40)
    query_status (s);
  if (buf[15] & 1)
    s->events = PIXMA_EV_BUTTON1;
  return 1;
}

static int
step1 (pixma_t * s)
{
  int error;
  iclass_t *mf = (iclass_t *) s->subdriver;

  error = query_status (s);
  if (error < 0)
    return error;
  if (s->param->source == PIXMA_SOURCE_ADF && !has_paper (s))
    return PIXMA_ENO_PAPER;
  /* activate only seen for generation 1 scanners */
  if (mf->generation == 1)
    {
      if (error >= 0)
        error = activate (s, 0);
      if (error >= 0)
        error = activate (s, 4);
    }
  return error;
}

/* line in=rrr... ggg... bbb... line out=rgbrgbrgb...  */
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
iclass_open (pixma_t * s)
{
  iclass_t *mf;
  uint8_t *buf;

  mf = (iclass_t *) calloc (1, sizeof (*mf));
  if (!mf)
    return PIXMA_ENOMEM;

  buf = (uint8_t *) malloc (CMDBUF_SIZE);
  if (!buf)
    {
      free (mf);
      return PIXMA_ENOMEM;
    }

  s->subdriver = mf;
  mf->state = state_idle;

  mf->cb.buf = buf;
  mf->cb.size = CMDBUF_SIZE;
  mf->cb.res_header_len = 2;
  mf->cb.cmd_header_len = 10;
  mf->cb.cmd_len_field_ofs = 7;

  /* set generation = 2 for new multifunctionals */
  mf->generation = (s->cfg->pid >= MF8030_PID) ? 2 : 1;
  PDBG (pixma_dbg (3, "*iclass_open***** This is a generation %d scanner.  *****\n", mf->generation));

  PDBG (pixma_dbg (3, "Trying to clear the interrupt buffer...\n"));
  if (handle_interrupt (s, 200) == 0)
    {
      PDBG (pixma_dbg (3, "  no packets in buffer\n"));
    }
  return 0;
}

static void
iclass_close (pixma_t * s)
{
  iclass_t *mf = (iclass_t *) s->subdriver;

  iclass_finish_scan (s);
  free (mf->cb.buf);
  free (mf->buf);
  free (mf);
  s->subdriver = NULL;
}

static int
iclass_check_param (pixma_t * s, pixma_scan_param_t * sp)
{
  UNUSED (s);

  sp->depth = 8;
  sp->line_size = ALIGN_SUP (sp->w, 32) * sp->channels;

  /* Some exceptions here for particular devices */
  /* Those devices can scan up to Legal 14" with ADF, but A4 11.7" in flatbed */
  /* PIXMA_CAP_ADF also works for PIXMA_CAP_ADFDUP */
  if ((s->cfg->cap & PIXMA_CAP_ADF) && sp->source == PIXMA_SOURCE_FLATBED)
    sp->h = MIN (sp->h, 877 * sp->xdpi / 75);

  return 0;
}

static int
iclass_scan (pixma_t * s)
{
  int error, n;
  iclass_t *mf = (iclass_t *) s->subdriver;
  uint8_t *buf, ignore;
  unsigned buf_len, ignore2;

  if (mf->state != state_idle)
    return PIXMA_EBUSY;

  /* clear interrupt packets buffer */
  while (handle_interrupt (s, 0) > 0)
    {
    }

  mf->raw_width = ALIGN_SUP (s->param->w, 32);
  PDBG (pixma_dbg (3, "raw_width = %u\n", mf->raw_width));

  n = IMAGE_BLOCK_SIZE / s->param->line_size + 1;
  buf_len = (n + 1) * s->param->line_size + IMAGE_BLOCK_SIZE;
  if (buf_len > mf->buf_len)
    {
      buf = (uint8_t *) realloc (mf->buf, buf_len);
      if (!buf)
	return PIXMA_ENOMEM;
      mf->buf = buf;
      mf->buf_len = buf_len;
    }
  mf->lineptr = mf->buf;
  mf->blkptr = mf->buf + n * s->param->line_size;
  mf->blk_len = 0;

  error = step1 (s);
  if (error >= 0 && (s->param->adf_pageid == 0 || mf->generation == 1))
    { /* single sheet or first sheet from ADF */
      PDBG (pixma_dbg (3, "*iclass_scan***** start scanning *****\n"));
      error = start_session (s);
      if (error >= 0)
        mf->state = state_scanning;
      if (error >= 0)
        error = select_source (s);
    }
  else if (error >= 0)
    { /* next sheet from ADF */
      PDBG (pixma_dbg (3, "*iclass_scan***** scan next sheet from ADF  *****\n"));
      mf->state = state_scanning;
    }
  if (error >= 0)
    error = send_scan_param (s);
  if (error >= 0)
    error = request_image_block (s, 0, &ignore, &ignore2, &ignore, &ignore2);
  if (error < 0)
    {
      iclass_finish_scan (s);
      return error;
    }
  mf->last_block = 0;
  return 0;
}


static int
iclass_fill_buffer (pixma_t * s, pixma_imagebuf_t * ib)
{
  int error, n;
  iclass_t *mf = (iclass_t *) s->subdriver;
  unsigned block_size, lines_size, first_block_size;
  uint8_t info;

/*
 * 1. send a block request cmd (d4 20 00... 04 00 06)
 * 2. examine the response for block size and/or end-of-scan flag
 * 3. read the block one chunk at a time
 * 4. repeat until have enough to process >=1 lines
 */
  do
    {
      do
        {
          if (s->cancel)
            return PIXMA_ECANCELED;
          if (mf->last_block)
            {
              /* end of image */
              mf->state = state_finished;
              return 0;
            }

          first_block_size = 0;
          error = request_image_block (s, 4, &info, &block_size, 
                          mf->blkptr + mf->blk_len, &first_block_size);
          /* add current block to remainder of previous */
          mf->blk_len += first_block_size;
          if (error < 0)
            {
              /* NOTE: seen in traffic logs but don't know the meaning. */
              read_error_info (s, NULL, 0);
              if (error == PIXMA_ECANCELED)
                return error;
            }

          /* info: 0x28 = end; 0x38 = end + ADF empty */
          mf->last_block = info & 0x38;
          if ((info & ~0x38) != 0)
            {
              PDBG (pixma_dbg (1, "WARNING: Unexpected result header\n"));
              PDBG (pixma_hexdump (1, &info, 1));
            }

          if (block_size == 0)
            {
              /* no image data at this moment. */
              /*pixma_sleep(100000); *//* FIXME: too short, too long? */
              handle_interrupt (s, 100);
            }
        }
      while (block_size == 0 && first_block_size == 0);

      error = read_image_block (s, mf->blkptr + mf->blk_len, block_size);
      block_size = error;
      if (error < 0)
        return error;

      /* add current block to remainder of previous */
      mf->blk_len += block_size;
      /* n = number of full lines (rows) we have in the buffer. */
      n = mf->blk_len / s->param->line_size;
      if (n != 0)
        {
          if (s->param->channels != 1 &&
                  mf->generation == 1 &&
	          s->cfg->pid != MF4600_PID &&
	          s->cfg->pid != MF6500_PID &&
	          s->cfg->pid != MF8030_PID)
            {
              /* color and not MF46xx or MF65xx */
              pack_rgb (mf->blkptr, n, mf->raw_width, mf->lineptr);
            }
          else
            {
              /* grayscale */
              memcpy (mf->lineptr, mf->blkptr, n * s->param->line_size);
            }
          lines_size = n * s->param->line_size;
          /* cull remainder and shift left */
          mf->blk_len -= lines_size;
          memcpy (mf->blkptr, mf->blkptr + lines_size, mf->blk_len);
        }
    }
  while (n == 0);

  /* output full lines, keep partial lines for next block */
  ib->rptr = mf->lineptr;
  ib->rend = mf->lineptr + lines_size;
  return ib->rend - ib->rptr;
}

static void
iclass_finish_scan (pixma_t * s)
{
  int error;
  iclass_t *mf = (iclass_t *) s->subdriver;

  switch (mf->state)
    {
      /* fall through */
    case state_warmup:
    case state_scanning:
      error = abort_session (s);
      if (error < 0)
	PDBG (pixma_dbg
	      (1, "WARNING:abort_session() failed %s\n",
	       pixma_strerror (error)));
      /* fall through */
    case state_finished:
      query_status (s);
      query_status (s);
      if (mf->generation == 1)
        { /* activate only seen for generation 1 scanners */
          activate (s, 0);
          query_status (s);
        }
      /* generation = 1:
       * 0x28 = last block (no multi page scan)
       * generation >= 2:
       * 0x38 = last block and ADF empty (generation >= 2)
       * 0x28 = last block and Paper in ADF (multi page scan)
       * some generation 2 scanners don't use 0x38 for ADF empty => check status */
      if (mf->last_block==0x38                                  /* generation 2 scanner ADF empty */
          || (mf->generation == 1 && mf->last_block == 0x28)    /* generation 1 scanner last block */
          || (mf->generation >= 2 && !has_paper(s)))            /* check status: no paper in ADF */
	{
          PDBG (pixma_dbg (3, "*iclass_finish_scan***** abort session  *****\n"));
	  abort_session (s);
	}
      else
        PDBG (pixma_dbg (3, "*iclass_finish_scan***** wait for next page from ADF  *****\n"));

      mf->state = state_idle;
      /* fall through */
    case state_idle:
      break;
    }
}

static void
iclass_wait_event (pixma_t * s, int timeout)
{
  /* FIXME: timeout is not correct. See usbGetCompleteUrbNoIntr() for
   * instance. */
  while (s->events == 0 && handle_interrupt (s, timeout) > 0)
    {
    }
}

static int
iclass_get_status (pixma_t * s, pixma_device_status_t * status)
{
  int error;

  error = query_status (s);
  if (error < 0)
    return error;
  status->hardware = PIXMA_HARDWARE_OK;
  status->adf = (has_paper (s)) ? PIXMA_ADF_OK : PIXMA_ADF_NO_PAPER;
  return 0;
}


static const pixma_scan_ops_t pixma_iclass_ops = {
  iclass_open,
  iclass_close,
  iclass_scan,
  iclass_fill_buffer,
  iclass_finish_scan,
  iclass_wait_event,
  iclass_check_param,
  iclass_get_status
};

#define DEV(name, model, pid, dpi, adftpu_max_dpi, w, h, cap) {     \
            name,                     /* name */		\
            model,                    /* model */		\
            0x04a9, pid,              /* vid pid */	\
            1,                        /* iface */		\
            &pixma_iclass_ops,        /* ops */		\
            dpi, dpi,                 /* xdpi, ydpi */	\
            0,                        /* adftpu_min_dpi not used in this subdriver */ \
            adftpu_max_dpi,           /* adftpu_max_dpi */ \
            0, 0,                     /* tpuir_min_dpi & tpuir_max_dpi not used in this subdriver */   \
            w, h,                     /* width, height */	\
            PIXMA_CAP_GRAY|PIXMA_CAP_EVENTS|cap             \
}
const pixma_config_t pixma_iclass_devices[] = {
  DEV ("Canon imageCLASS MF4270", "MF4270", MF4200_PID, 600, 0, 640, 877, PIXMA_CAP_ADF),
  DEV ("Canon imageCLASS MF4150", "MF4100", MF4100_PID, 600, 0, 640, 877, PIXMA_CAP_ADF),
  DEV ("Canon imageCLASS MF4690", "MF4690", MF4600_PID, 600, 0, 640, 877, PIXMA_CAP_ADF),
  DEV ("Canon imageCLASS D420", "D420", D420_PID, 600, 0, 640, 877, PIXMA_CAP_ADFDUP),
  DEV ("Canon imageCLASS D480", "D480", D480_PID, 600, 0, 640, 877, PIXMA_CAP_ADFDUP),
  DEV ("Canon imageCLASS MF4360", "MF4360", MF4360_PID, 600, 0, 640, 877, PIXMA_CAP_ADFDUP),
  DEV ("Canon imageCLASS MF4320", "MF4320", MF4320_PID, 600, 0, 640, 877, PIXMA_CAP_ADF),
  DEV ("Canon imageCLASS MF4010", "MF4010", MF4010_PID, 600, 0, 640, 877, 0),
  DEV ("Canon imageCLASS MF3240", "MF3240", MF3200_PID, 600, 0, 640, 877, 0),
  DEV ("Canon imageClass MF6500", "MF6500", MF6500_PID, 600, 0, 640, 877, PIXMA_CAP_ADF),
  DEV ("Canon imageCLASS MF4410", "MF4410", MF4410_PID, 600, 0, 640, 877, PIXMA_CAP_ADF),
  DEV ("Canon i-SENSYS MF4500 Series", "MF4500", MF4500_PID, 600, 0, 640, 877, PIXMA_CAP_ADF),
  DEV ("Canon i-SENSYS MF3010", "MF3010", MF3010_PID, 600, 0, 640, 877, 0),
  DEV ("Canon i-SENSYS MF4700 Series", "MF4700", MF4700_PID, 600, 0, 640, 1050, PIXMA_CAP_ADF),
  DEV ("Canon i-SENSYS MF4800 Series", "MF4800", MF4800_PID, 600, 0, 640, 1050, PIXMA_CAP_ADF),
  DEV ("Canon imageCLASS MF4570dw", "MF4570dw", MF4570_PID, 600, 0, 640, 877, 0),
  DEV ("Canon i-SENSYS MF8200C Series", "MF8200C", MF8200_PID, 600, 300, 640, 1050, PIXMA_CAP_ADF),
  DEV ("Canon i-SENSYS MF8300 Series", "MF8300", MF8300_PID, 600, 0, 640, 1050, PIXMA_CAP_ADF),
  DEV ("Canon imageCLASS D530", "D530", D530_PID, 600, 0, 640, 877, 0),
  /* FIXME: the following capabilities all need updating/verifying */
  DEV ("Canon imageCLASS MF5630", "MF5630", MF5630_PID, 600, 0, 640, 877, PIXMA_CAP_ADF),
  DEV ("Canon laserBase MF5650", "MF5650", MF5650_PID, 600, 0, 640, 877, PIXMA_CAP_ADF),
  DEV ("Canon imageCLASS MF8170c", "MF8170c", MF8100_PID, 600, 0, 640, 877, PIXMA_CAP_ADF),
  DEV ("Canon imageClass MF8030", "MF8030", MF8030_PID, 600, 0, 640, 877, PIXMA_CAP_ADF),
  DEV ("Canon i-SENSYS MF5880dn", "MF5880", MF5880_PID, 600, 0, 640, 877, PIXMA_CAP_ADFDUP),
  DEV ("Canon i-SENSYS MF6680dn", "MF6680", MF6680_PID, 600, 0, 640, 877, PIXMA_CAP_ADFDUP),
  DEV ("Canon imageRUNNER 1133", "iR1133", IR1133_PID, 600, 0, 637, 877, PIXMA_CAP_ADFDUP),
  DEV ("Canon i-SENSYS MF5900 Series", "MF5900", MF5900_PID, 600, 0, 640, 1050, PIXMA_CAP_ADFDUP),
  DEV ("Canon i-SENSYS MF8500C Series", "MF8500C", MF8500_PID, 600, 0, 640, 1050, PIXMA_CAP_ADFDUP),
  DEV ("Canon i-SENSYS MF6100 Series", "MF6100", MF6100_PID, 600, 0, 640, 1050, PIXMA_CAP_ADFDUP),
  DEV ("Canon imageClass MF810/820", "MF810/820", MF820_PID, 600, 0, 640, 1050, PIXMA_CAP_ADFDUP),
  DEV ("Canon i-SENSYS MF220 Series", "MF220", MF220_PID, 600, 0, 640, 1050, PIXMA_CAP_ADFDUP),
  DEV ("Canon i-SENSYS MF210 Series", "MF210", MF210_PID, 600, 0, 640, 1050, PIXMA_CAP_ADF),
  DEV (NULL, NULL, 0, 0, 0, 0, 0, 0)
};
