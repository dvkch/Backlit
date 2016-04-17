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
#include <stdarg.h>
#include <math.h>		/* pow(C90) */

#include <sys/time.h>		/* gettimeofday(4.3BSD) */
#include <unistd.h>		/* usleep */

#include "pixma_rename.h"
#include "pixma_common.h"
#include "pixma_io.h"


#ifdef __GNUC__
# define UNUSED(v) (void) v
#else
# define UNUSED(v)
#endif

extern const pixma_config_t pixma_mp150_devices[];
extern const pixma_config_t pixma_mp750_devices[];
extern const pixma_config_t pixma_mp730_devices[];
extern const pixma_config_t pixma_mp810_devices[];
extern const pixma_config_t pixma_iclass_devices[];

static const pixma_config_t *const pixma_devices[] = {
  pixma_mp150_devices,
  pixma_mp750_devices,
  pixma_mp730_devices,
  pixma_mp810_devices,
  pixma_iclass_devices,
  NULL
};

static pixma_t *first_pixma = NULL;
static time_t tstart_sec = 0;
static uint32_t tstart_usec = 0;
static int debug_level = 1;


#ifndef NDEBUG

static void
u8tohex (uint8_t x, char *str)
{
  static const char hdigit[16] =
    { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd',
    'e', 'f'
    };
  str[0] = hdigit[(x >> 4) & 0xf];
  str[1] = hdigit[x & 0xf];
  str[2] = '\0';
}

static void
u32tohex (uint32_t x, char *str)
{
  u8tohex (x >> 24, str);
  u8tohex (x >> 16, str + 2);
  u8tohex (x >> 8, str + 4);
  u8tohex (x, str + 6);
}

void
pixma_hexdump (int level, const void *d_, unsigned len)
{
  const uint8_t *d = (const uint8_t *) (d_);
  unsigned ofs, c, plen;
  char line[100];		/* actually only 1+8+1+8*3+1+8*3+1 = 61 bytes needed */

  if (level > debug_level)
    return;
  if (level == debug_level)
    /* if debuglevel == exact match and buffer contains more than 3 lines, print 2 lines + .... */
    plen = (len > 64) ? 32: len;
  else
    plen = len;
  ofs = 0;
  while (ofs < plen)
    {
      char *p;
      line[0] = ' ';
      u32tohex (ofs, line + 1);
      line[9] = ':';
      p = line + 10;
      for (c = 0; c != 16 && (ofs + c) < plen; c++)
        {
          u8tohex (d[ofs + c], p);
          p[2] = ' ';
          p += 3;
          if (c == 7)
            {
              p[0] = ' ';
              p++;
            }
        }
      p[0] = '\0';
      pixma_dbg (level, "%s\n", line);
      ofs += c;
    }
  if (len > plen)
    pixma_dbg(level, "......\n");
}

static void
time2str (char *buf, unsigned size)
{
  time_t sec;
  uint32_t usec;

  pixma_get_time (&sec, &usec);
  sec -= tstart_sec;
  if (usec >= tstart_usec)
    {
      usec -= tstart_usec;
    }
  else
    {
      usec = 1000000 + usec - tstart_usec;
      sec--;
    }
  snprintf (buf, size, "%lu.%03u", (unsigned long) sec,
	    (unsigned) (usec / 1000));
}

void
pixma_dump (int level, const char *type, const void *data, int len,
	    int size, int max)
{
  int actual_len, print_len;
  char buf[20];

  if (level > debug_level)
    return;
  if (debug_level >= 20)
    max = -1;			/* dump every bytes */

  time2str (buf, sizeof (buf));
  pixma_dbg (level, "%s T=%s len=%d\n", type, buf, len);

  actual_len = (size >= 0) ? size : len;
  print_len = (max >= 0 && max < actual_len) ? max : actual_len;
  if (print_len >= 0)
    {
      pixma_hexdump (level, data, print_len);
      if (print_len < actual_len)
	pixma_dbg (level, " ...\n");
    }
  if (len < 0)
    pixma_dbg (level, "  ERROR: %s\n", pixma_strerror (len));
  pixma_dbg (level, "\n");
}


#endif /* NDEBUG */

/* NOTE: non-reentrant */
const char *
pixma_strerror (int error)
{
  static char buf[50];

  /* TODO: more human friendly messages */
  switch (error)
    {
    case PIXMA_EIO:
      return "EIO";
    case PIXMA_ENODEV:
      return "ENODEV";
    case PIXMA_EACCES:
      return "EACCES";
    case PIXMA_ENOMEM:
      return "ENOMEM";
    case PIXMA_EINVAL:
      return "EINVAL";
    case PIXMA_EBUSY:
      return "EBUSY";
    case PIXMA_ECANCELED:
      return "ECANCELED";
    case PIXMA_ENOTSUP:
      return "ENOTSUP";
    case PIXMA_ETIMEDOUT:
      return "ETIMEDOUT";
    case PIXMA_EPROTO:
      return "EPROTO";
    case PIXMA_EPAPER_JAMMED:
      return "EPAPER_JAMMED";
    case PIXMA_ECOVER_OPEN:
      return "ECOVER_OPEN";
    case PIXMA_ENO_PAPER:
      return "ENO_PAPER";
    case PIXMA_EOF:
      return "EEOF";
    }
  snprintf (buf, sizeof (buf), "EUNKNOWN:%d", error);
  return buf;
}

void
pixma_set_debug_level (int level)
{
  debug_level = level;
}

void
pixma_set_be16 (uint16_t x, uint8_t * buf)
{
  buf[0] = x >> 8;
  buf[1] = x;
}

void
pixma_set_be32 (uint32_t x, uint8_t * buf)
{
  buf[0] = x >> 24;
  buf[1] = x >> 16;
  buf[2] = x >> 8;
  buf[3] = x;
}

uint16_t
pixma_get_be16 (const uint8_t * buf)
{
  return ((uint16_t) buf[0] << 8) | buf[1];
}

uint32_t
pixma_get_be32 (const uint8_t * buf)
{
  return ((uint32_t) buf[0] << 24) + ((uint32_t) buf[1] << 16) +
    ((uint32_t) buf[2] << 8) + buf[3];
}

uint8_t
pixma_sum_bytes (const void *data, unsigned len)
{
  const uint8_t *d = (const uint8_t *) data;
  unsigned i, sum = 0;
  for (i = 0; i != len; i++)
    sum += d[i];
  return sum;
}

void
pixma_sleep (unsigned long usec)
{
  usleep (usec);
}

void
pixma_get_time (time_t * sec, uint32_t * usec)
{
  struct timeval tv;
  gettimeofday (&tv, NULL);
  if (sec)
    *sec = tv.tv_sec;
  if (usec)
    *usec = tv.tv_usec;
}

/* convert 24/48 bit RGB to 8/16 bit ir
 *
 * Formular: g = R
 *           drop G + B
 *
 * sptr: source color scale buffer
 * gptr: destination gray scale buffer
 * c == 3: 24 bit RGB -> 8 bit ir
 * c == 6: 48 bit RGB -> 16 bit ir
 */
uint8_t *
pixma_r_to_ir (uint8_t * gptr, uint8_t * sptr, unsigned w, unsigned c)
{
  unsigned i;

  /* PDBG (pixma_dbg (4, "*pixma_rgb_to_ir*****\n")); */

  for (i = 0; i < w; i++)
    {
      *gptr++ = *sptr++;
      if (c == 6) *gptr++ = *sptr++;            /* 48 bit RGB: high byte */
      sptr += (c == 6) ? 4 : 2;                 /* drop G + B */
    }
  return gptr;
}

/* convert 24/48 bit RGB to 8/16 bit grayscale
 *
 * Formular: g = (R + G + B) / 3
 *
 * sptr: source color scale buffer
 * gptr: destination gray scale buffer
 * c == 3: 24 bit RGB -> 8 bit gray
 * c == 6: 48 bit RGB -> 16 bit gray
 */
uint8_t *
pixma_rgb_to_gray (uint8_t * gptr, uint8_t * sptr, unsigned w, unsigned c)
{
  unsigned i, j, g;

  /* PDBG (pixma_dbg (4, "*pixma_rgb_to_gray*****\n")); */

  for (i = 0; i < w; i++)
    {
      for (j = 0, g = 0; j < 3; j++)
        {
          g += *sptr++;
          if (c == 6) g += (*sptr++ << 8);      /* 48 bit RGB: high byte */
        }

      g /= 3;                                   /* 8 or 16 bit gray */
      *gptr++ = g;
      if (c == 6) *gptr++ = (g >> 8);           /* 16 bit gray: high byte */
    }
  return gptr;
}

/**
 * This code was taken from the genesys backend
 * uses threshold and threshold_curve to control software binarization
 * @param sp    device set up for the scan
 * @param dst   pointer where to store result
 * @param src   pointer to raw data
 * @param width width of the processed line
 * @param c     1 for 1-channel single-byte data,
 *              3 for 3-channel single-byte data,
 *              6 for double-byte data
 * */
uint8_t *
pixma_binarize_line(pixma_scan_param_t * sp, uint8_t * dst, uint8_t * src, unsigned width, unsigned c)
{
  unsigned j, x, windowX, sum = 0;
  unsigned threshold;
  unsigned offset, addCol;
  int dropCol, offsetX;
  unsigned char mask;
  uint8_t min, max;

  /* PDBG (pixma_dbg (4, "*pixma_binarize_line***** src = %u, dst = %u, width = %u, c = %u, threshold = %u, threshold_curve = %u *****\n",
                      src, dst, width, c, sp->threshold, sp->threshold_curve)); */

  /* 16 bit grayscale not supported */
  if (c == 6)
    {
      PDBG (pixma_dbg (1, "*pixma_binarize_line***** Error: 16 bit grayscale not supported\n"));
      return dst;
    }

  /* first, color convert to grayscale */
    if (c != 1)
      pixma_rgb_to_gray(dst, src, width, c);

  /* second, normalize line */
    min = 255;
    max = 0;
    for (x = 0; x < width; x++)
      {
        if (src[x] > max)
          {
            max = src[x];
          }
        if (src[x] < min)
          {
            min = src[x];
          }
      }

    /* safeguard against dark or white areas */
    if(min>80)
        min=0;
    if(max<80)
        max=255;
    for (x = 0; x < width; x++)
      {
        src[x] = ((src[x] - min) * 255) / (max - min);
      }

  /* third, create sliding window, prefill the sliding sum */
    /* ~1mm works best, but the window needs to have odd # of pixels */
    windowX = (6 * sp->xdpi) / 150;
    if (!(windowX % 2))
      windowX++;

    /* to avoid conflicts with *dst start with offset */
    offsetX = 1 + (windowX / 2) / 8;
    for (j = offsetX; j <= windowX; j++)
      sum += src[j];
    /* PDBG (pixma_dbg (4, " *pixma_binarize_line***** windowX = %u, startX = %u, sum = %u\n",
                     windowX, startX, sum)); */

  /* fourth, walk the input buffer, output bits */
    for (j = 0; j < width; j++)
      {
        /* output image location */
        offset = j % 8;
        mask = 0x80 >> offset;
        threshold = sp->threshold;

        /* move sum/update threshold only if there is a curve */
        if (sp->threshold_curve)
          {
            addCol = j + windowX / 2;
            dropCol = addCol - windowX;

            if (dropCol >= offsetX && addCol < width)
              {
                sum += src[addCol];
                sum -= (sum < src[dropCol] ? sum : src[dropCol]);       /* no negative sum */
              }
            threshold = sp->lineart_lut[sum / windowX];
            /* PDBG (pixma_dbg (4, " *pixma_binarize_line***** addCol = %u, dropCol = %d, sum = %u, windowX = %u, lut-element = %d, threshold = %u\n",
                             addCol, dropCol, sum, windowX, sum/windowX, threshold)); */
          }

        /* lookup threshold */
        if (src[j] > threshold)
            *dst &= ~mask;      /* white */
        else
            *dst |= mask;       /* black */

        if (offset == 7)
            dst++;
      }

  /* PDBG (pixma_dbg (4, " *pixma_binarize_line***** ready: src = %u, dst = %u *****\n", src, dst)); */

  return dst;
}

/**
   This code was taken from the genesys backend
   Function to build a lookup table (LUT), often
   used by scanners to implement brightness/contrast/gamma
   or by backends to speed binarization/thresholding

   offset and slope inputs are -127 to +127

   slope rotates line around central input/output val,
   0 makes horizontal line

       pos           zero          neg
       .       x     .             .  x
       .      x      .             .   x
   out .     x       .xxxxxxxxxxx  .    x
       .    x        .             .     x
       ....x.......  ............  .......x....
            in            in            in

   offset moves line vertically, and clamps to output range
   0 keeps the line crossing the center of the table

       high           low
       .   xxxxxxxx   .
       . x            .
   out x              .          x
       .              .        x
       ............   xxxxxxxx....
            in             in

   out_min/max provide bounds on output values,
   useful when building thresholding lut.
   0 and 255 are good defaults otherwise.
  * */
static SANE_Status
load_lut (unsigned char * lut,
  int in_bits, int out_bits,
  int out_min, int out_max,
  int slope, int offset)
{
  int i, j;
  double shift, rise;
  int max_in_val = (1 << in_bits) - 1;
  int max_out_val = (1 << out_bits) - 1;
  unsigned char * lut_p = lut;

  /* PDBG (pixma_dbg (4, "*load_lut***** start %d %d *****\n", slope, offset)); */

  /* slope is converted to rise per unit run:
   * first [-127,127] to [-1,1]
   * then multiply by PI/2 to convert to radians
   * then take the tangent (T.O.A)
   * then multiply by the normal linear slope
   * because the table may not be square, i.e. 1024x256*/
  rise = tan((double)slope/127 * M_PI/2) * max_out_val / max_in_val;

  /* line must stay vertically centered, so figure
   * out vertical offset at central input value */
  shift = (double)max_out_val/2 - (rise*max_in_val/2);

  /* convert the user offset setting to scale of output
   * first [-127,127] to [-1,1]
   * then to [-max_out_val/2,max_out_val/2]*/
  shift += (double)offset / 127 * max_out_val / 2;

  for(i=0;i<=max_in_val;i++){
    j = rise*i + shift;

    if(j<out_min){
      j=out_min;
    }
    else if(j>out_max){
      j=out_max;
    }

    *lut_p=j;
    lut_p++;
  }

  /* PDBG (pixma_dbg (4, "*load_lut***** finish *****\n")); */
  /* PDBG (pixma_hexdump (4, lut, max_in_val+1)); */

  return SANE_STATUS_GOOD;
}

int
pixma_map_status_errno (unsigned status)
{
  switch (status)
    {
    case PIXMA_STATUS_OK:
      return 0;
    case PIXMA_STATUS_FAILED:
      return PIXMA_ECANCELED;
    case PIXMA_STATUS_BUSY:
      return PIXMA_EBUSY;
    default:
      return PIXMA_EPROTO;
    }
}

int
pixma_check_result (pixma_cmdbuf_t * cb)
{
  const uint8_t *r = cb->buf;
  unsigned header_len = cb->res_header_len;
  unsigned expected_reslen = cb->expected_reslen;
  int error;
  unsigned len;

  if (cb->reslen < 0)
    return cb->reslen;

  len = (unsigned) cb->reslen;
  if (len >= header_len)
    {
      error = pixma_map_status_errno (pixma_get_be16 (r));
      if (expected_reslen != 0)
        {
          if (len == expected_reslen)
            {
              if (pixma_sum_bytes (r + header_len, len - header_len) != 0)
                 error = PIXMA_EPROTO;
            }
          else
            {
              /* This case will happen when a command cannot be completely
                 executed, e.g. because you press the cancel button. The
                 device will return only a header with PIXMA_STATUS_FAILED. */
              if (len != header_len)
                 error = PIXMA_EPROTO;
            }
        }
    }
  else
    error = PIXMA_EPROTO;

#ifndef NDEBUG
  if (error == PIXMA_EPROTO)
    {
      pixma_dbg (1, "WARNING: result len=%d expected %d\n",
		 len, cb->expected_reslen);
      pixma_hexdump (1, r, MIN (len, 64));
    }
#endif
  return error;
}

int
pixma_cmd_transaction (pixma_t * s, const void *cmd, unsigned cmdlen,
		       void *data, unsigned expected_len)
{
  int error, tmo;

  error = pixma_write (s->io, cmd, cmdlen);
  if (error != (int) cmdlen)
    {
      if (error >= 0)
        {
          /* Write timeout is too low? */
          PDBG (pixma_dbg
          (1, "ERROR: incomplete write, %u out of %u written\n",
           (unsigned) error, cmdlen));
          error = PIXMA_ETIMEDOUT;
        }
      return error;
    }

  /* When you send the start_session command while the scanner optic is
     going back to the home position after the last scan session has been
     cancelled, you won't get the response before it arrives home. This takes
     about 5 seconds. If the last session was succeeded, the scanner will
     immediatly answer with PIXMA_STATUS_BUSY.

     Is 8 seconds timeout enough? This affects ALL commands that use
     pixma_cmd_transaction(). */
  tmo = 8;
  do
    {
      error = pixma_read (s->io, data, expected_len);
      if (error == PIXMA_ETIMEDOUT)
      {
        PDBG (pixma_dbg (2, "No response yet. Timed out in %d sec.\n", tmo));
        pixma_sleep (1000000);          /* 1s timeout */
      }
    }
  while (error == PIXMA_ETIMEDOUT && --tmo != 0);
  if (error < 0)
    {
      PDBG (pixma_dbg (1, "WARNING: Error in response phase. cmd:%02x%02x\n",
		       ((const uint8_t *) cmd)[0],
		       ((const uint8_t *) cmd)[1]));
      PDBG (pixma_dbg (1,"  If the scanner hangs, reset it and/or unplug the "
	                       "USB cable.\n"));
    }
  return error;			/* length of the result packet or error */
}

uint8_t *
pixma_newcmd (pixma_cmdbuf_t * cb, unsigned cmd,
	      unsigned dataout, unsigned datain)
{
  unsigned cmdlen = cb->cmd_header_len + dataout;
  unsigned reslen = cb->res_header_len + datain;

  if (cmdlen > cb->size || reslen > cb->size)
    return NULL;
  memset (cb->buf, 0, cmdlen);
  cb->cmdlen = cmdlen;
  cb->expected_reslen = reslen;
  pixma_set_be16 (cmd, cb->buf);
  pixma_set_be16 (dataout + datain, cb->buf + cb->cmd_len_field_ofs);
  if (dataout != 0)
    return cb->buf + cb->cmd_header_len;
  else
    return cb->buf + cb->res_header_len;
}

int
pixma_exec (pixma_t * s, pixma_cmdbuf_t * cb)
{
  if (cb->cmdlen > cb->cmd_header_len)
    pixma_fill_checksum (cb->buf + cb->cmd_header_len,
			 cb->buf + cb->cmdlen - 1);
  cb->reslen =
    pixma_cmd_transaction (s, cb->buf, cb->cmdlen, cb->buf,
			   cb->expected_reslen);
  return pixma_check_result (cb);
}

int
pixma_exec_short_cmd (pixma_t * s, pixma_cmdbuf_t * cb, unsigned cmd)
{
  pixma_newcmd (cb, cmd, 0, 0);
  return pixma_exec (s, cb);
}

int
pixma_check_dpi (unsigned dpi, unsigned max)
{
  /* valid dpi = 75 * 2^n */
  unsigned temp = dpi / 75;
  if (dpi > max || dpi < 75 || 75 * temp != dpi || (temp & (temp - 1)) != 0)
    return PIXMA_EINVAL;
  return 0;
}


int
pixma_init (void)
{
  PDBG (pixma_dbg (2, "pixma version %d.%d.%d\n", PIXMA_VERSION_MAJOR,
		   PIXMA_VERSION_MINOR, PIXMA_VERSION_BUILD));
  PASSERT (first_pixma == NULL);
  if (tstart_sec == 0)
    pixma_get_time (&tstart_sec, &tstart_usec);
  return pixma_io_init ();
}

void
pixma_cleanup (void)
{
  while (first_pixma)
    pixma_close (first_pixma);
  pixma_io_cleanup ();
}

int
pixma_open (unsigned devnr, pixma_t ** handle)
{
  int error;
  pixma_t *s;
  const pixma_config_t *cfg;

  *handle = NULL;
  cfg = pixma_get_device_config (devnr);
  if (!cfg)
    return PIXMA_EINVAL;	/* invalid devnr */
  PDBG (pixma_dbg (2, "pixma_open(): %s\n", cfg->name));

  s = (pixma_t *) calloc (1, sizeof (s[0]));
  if (!s)
    return PIXMA_ENOMEM;
  s->next = first_pixma;
  first_pixma = s;

  s->cfg = cfg;
  error = pixma_connect (devnr, &s->io);
  if (error < 0)
    {
      PDBG (pixma_dbg
	    (2, "pixma_connect() failed %s\n", pixma_strerror (error)));
      goto rollback;
    }
  strncpy (s->id, pixma_get_device_id (devnr), sizeof (s->id) - 1);
  s->ops = s->cfg->ops;
  s->scanning = 0;
  error = s->ops->open (s);
  if (error < 0)
    goto rollback;
  error = pixma_deactivate (s->io);
  if (error < 0)
    goto rollback;
  *handle = s;
  return 0;

rollback:
  PDBG (pixma_dbg (2, "pixma_open() failed %s\n", pixma_strerror (error)));
  pixma_close (s);
  return error;
}

void
pixma_close (pixma_t * s)
{
  pixma_t **p;

  if (!s)
    return;
  for (p = &first_pixma; *p && *p != s; p = &((*p)->next))
    {
    }
  PASSERT (*p);
  if (!(*p))
    return;
  PDBG (pixma_dbg (2, "pixma_close(): %s\n", s->cfg->name));
  if (s->io)
    {
      if (s->scanning)
	{
	  PDBG (pixma_dbg (3, "pixma_close(): scanning in progress, call"
			   " finish_scan()\n"));
	  s->ops->finish_scan (s);
	}
      s->ops->close (s);
      pixma_disconnect (s->io);
    }
  *p = s->next;
  free (s);
}

int
pixma_scan (pixma_t * s, pixma_scan_param_t * sp)
{
  int error;

  error = pixma_check_scan_param (s, sp);
  if (error < 0)
    return error;

  if (sp->mode == PIXMA_SCAN_MODE_LINEART)
    {
      load_lut(sp->lineart_lut, 8, 8, 50, 205,
               sp->threshold_curve, sp->threshold-127);
    }

#ifndef NDEBUG
  pixma_dbg (3, "\n");
  pixma_dbg (3, "pixma_scan(): start\n");
  pixma_dbg (3, "  line_size=%"PRIu64" image_size=%"PRIu64" channels=%u depth=%u\n",
	     sp->line_size, sp->image_size, sp->channels, sp->depth);
  pixma_dbg (3, "  dpi=%ux%u offset=(%u,%u) dimension=%ux%u\n",
	     sp->xdpi, sp->ydpi, sp->x, sp->y, sp->w, sp->h);
  pixma_dbg (3, "  gamma_table=%p source=%d\n", sp->gamma_table, sp->source);
  pixma_dbg (3, "  threshold=%d threshold_curve=%d\n", sp->threshold, sp->threshold_curve);
  pixma_dbg (3, "  ADF page count: %d\n", sp->adf_pageid);
#endif

  s->param = sp;
  s->cancel = 0;
  s->cur_image_size = 0;
  s->imagebuf.wptr = NULL;
  s->imagebuf.wend = NULL;
  s->imagebuf.rptr = NULL;
  s->imagebuf.rend = NULL;
  s->underrun = 0;
  error = s->ops->scan (s);
  if (error >= 0)
    {
      s->scanning = 1;
    }
  else
    {
      PDBG (pixma_dbg
	    (3, "pixma_scan() failed %s\n", pixma_strerror (error)));
    }

  return error;
}

static uint8_t *
fill_pixels (pixma_t * s, uint8_t * ptr, uint8_t * end, uint8_t value)
{
  if (s->cur_image_size < s->param->image_size)
    {
      long n = s->param->image_size - s->cur_image_size;
      if (n > (end - ptr))
	n = end - ptr;
      memset (ptr, value, n);
      s->cur_image_size += n;
      ptr += n;
    }
  return ptr;
}

int
pixma_read_image (pixma_t * s, void *buf, unsigned len)
{
  int result;
  pixma_imagebuf_t ib;

  if (!s->scanning)
    return 0;
  if (s->cancel)
    {
      result = PIXMA_ECANCELED;
      goto cancel;
    }

  ib = s->imagebuf;		/* get rptr and rend */
  ib.wptr = (uint8_t *) buf;
  ib.wend = ib.wptr + len;

  if (s->underrun)
    {
      if (s->cur_image_size < s->param->image_size)
        {
          ib.wptr = fill_pixels (s, ib.wptr, ib.wend, 0xff);
        }
      else
        {
          PDBG (pixma_dbg
          (3, "pixma_read_image(): completed (underrun detected)\n"));
          s->scanning = 0;
        }
      return ib.wptr - (uint8_t *) buf;
    }

  while (ib.wptr != ib.wend)
    {
      if (ib.rptr == ib.rend)
        {
          ib.rptr = ib.rend = NULL;
          result = s->ops->fill_buffer (s, &ib);
          if (result < 0)
            goto cancel;
          if (result == 0)
            {			/* end of image? */
              s->ops->finish_scan (s);
              if (s->cur_image_size != s->param->image_size)
                {
                  pixma_dbg (1, "WARNING:image size mismatches\n");
                  pixma_dbg (1,
                       "    %"PRIu64" expected (%d lines) but %"PRIu64" received (%"PRIu64" lines)\n",
                       s->param->image_size, s->param->h,
                       s->cur_image_size,
                       s->cur_image_size / s->param->line_size);
                  if ((s->cur_image_size % s->param->line_size) != 0)
                    {
                      pixma_dbg (1,
                     "BUG:received data not multiple of line_size\n");
                    }
                }
              if (s->cur_image_size < s->param->image_size)
                {
                  s->underrun = 1;
                  ib.wptr = fill_pixels (s, ib.wptr, ib.wend, 0xff);
                }
              else
                {
                  PDBG (pixma_dbg (3, "pixma_read_image():completed\n"));
                  s->scanning = 0;
                }
              break;
            }
          s->cur_image_size += result;

          PASSERT (s->cur_image_size <= s->param->image_size);
        }
      if (ib.rptr)
        {
          unsigned count = MIN (ib.rend - ib.rptr, ib.wend - ib.wptr);
          memcpy (ib.wptr, ib.rptr, count);
          ib.rptr += count;
          ib.wptr += count;
        }
    }
  s->imagebuf = ib;		/* store rptr and rend */
  return ib.wptr - (uint8_t *) buf;

cancel:
  s->ops->finish_scan (s);
  s->scanning = 0;
  if (result == PIXMA_ECANCELED)
    {
      PDBG (pixma_dbg (3, "pixma_read_image(): cancelled by %sware\n",
		       (s->cancel) ? "soft" : "hard"));
    }
  else
    {
      PDBG (pixma_dbg (3, "pixma_read_image() failed %s\n",
		       pixma_strerror (result)));
    }
  return result;
}

void
pixma_cancel (pixma_t * s)
{
  s->cancel = 1;
}

int
pixma_enable_background (pixma_t * s, int enabled)
{
  return pixma_set_interrupt_mode (s->io, enabled);
}

int
pixma_activate_connection(pixma_t * s)
{
  return pixma_activate (s->io);
}

int
pixma_deactivate_connection(pixma_t * s)
{
  return pixma_deactivate (s->io);
}

uint32_t
pixma_wait_event (pixma_t * s, int timeout /*ms */ )
{
  unsigned events;

  if (s->events == PIXMA_EV_NONE && s->ops->wait_event)
    s->ops->wait_event (s, timeout);
  events = s->events;
  s->events = PIXMA_EV_NONE;
  return events;
}

#define CLAMP2(x,w,min,max,dpi) do {		\
    unsigned m = (max) * (dpi) / 75;		\
    x = MIN(x, m - min);			\
    w = MIN(w, m - x);				\
    if (w < min)  w = min;			\
} while(0)

int
pixma_check_scan_param (pixma_t * s, pixma_scan_param_t * sp)
{
  unsigned cfg_xdpi;

  if (!(sp->channels == 3 ||
	(sp->channels == 1 && (s->cfg->cap & PIXMA_CAP_GRAY) != 0)))
    return PIXMA_EINVAL;

  /* flatbed: use s->cfg->xdpi
   * TPU/ADF: use s->cfg->adftpu_max_dpi, if configured with dpi value */
  cfg_xdpi = ((sp->source == PIXMA_SOURCE_FLATBED
               || s->cfg->adftpu_max_dpi == 0) ? s->cfg->xdpi
                                               : s->cfg->adftpu_max_dpi);

  if (pixma_check_dpi (sp->xdpi, cfg_xdpi) < 0 ||
      pixma_check_dpi (sp->ydpi, s->cfg->ydpi) < 0)
    return PIXMA_EINVAL;

  /* xdpi must be equal to ydpi except that
     xdpi = max_xdpi and ydpi = max_ydpi. */
  if (!(sp->xdpi == sp->ydpi ||
	(sp->xdpi == cfg_xdpi && sp->ydpi == s->cfg->ydpi)))
    return PIXMA_EINVAL;

  if (s->ops->check_param (s, sp) < 0)
    return PIXMA_EINVAL;

  /* FIXME: I assume the same minimum width and height for every model.
   * new scanners need minimum 16 px height
   * minimum image size: 16 px x 16 px */
  CLAMP2 (sp->x, sp->w, 16, s->cfg->width, sp->xdpi);
  CLAMP2 (sp->y, sp->h, 16, s->cfg->height, sp->ydpi);

  switch (sp->source)
    {
    case PIXMA_SOURCE_FLATBED:
      break;

    case PIXMA_SOURCE_TPU:
      if ((s->cfg->cap & PIXMA_CAP_TPU) != PIXMA_CAP_TPU)
        {
          sp->source = PIXMA_SOURCE_FLATBED;
          PDBG (pixma_dbg
          (1, "WARNING: TPU unsupported, fallback to flatbed.\n"));
        }
      break;

    case PIXMA_SOURCE_ADF:
      if ((s->cfg->cap & PIXMA_CAP_ADF) != PIXMA_CAP_ADF)
        {
          sp->source = PIXMA_SOURCE_FLATBED;
          PDBG (pixma_dbg
          (1, "WARNING: ADF unsupported, fallback to flatbed.\n"));
        }
      break;

    case PIXMA_SOURCE_ADFDUP:
      if ((s->cfg->cap & PIXMA_CAP_ADFDUP) != PIXMA_CAP_ADFDUP)
        {
          if (s->cfg->cap & PIXMA_CAP_ADF)
            {
              sp->source = PIXMA_SOURCE_ADF;
            }
          else
            {
              sp->source = PIXMA_SOURCE_FLATBED;
            }
          PDBG (pixma_dbg
          (1, "WARNING: ADF duplex unsupported, fallback to %d.\n",
           sp->source));
        }
      break;
    }

  if (sp->depth == 0)
    sp->depth = 8;
  if ((sp->depth % 8) != 0 && sp->depth != 1)
    return PIXMA_EINVAL;

  sp->line_size = 0;

  if (s->ops->check_param (s, sp) < 0)
    return PIXMA_EINVAL;

  if (sp->line_size == 0)
    sp->line_size = sp->depth / 8 * sp->channels * sp->w;
  sp->image_size = sp->line_size * sp->h;

  /* image_size for software lineart is counted in bits */
  if (sp->software_lineart == 1)
    sp->image_size /= 8;
  return 0;
}

const char *
pixma_get_string (pixma_t * s, pixma_string_index_t i)
{
  switch (i)
    {
    case PIXMA_STRING_MODEL:
      return s->cfg->name;
    case PIXMA_STRING_ID:
      return s->id;
    case PIXMA_STRING_LAST:
      return NULL;
    }
  return NULL;
}

const pixma_config_t *
pixma_get_config (pixma_t * s)
{
  return s->cfg;
}

void
pixma_fill_gamma_table (double gamma, uint8_t * table, unsigned n)
{
  int i;
  double r_gamma = 1.0 / gamma;
  double out_scale = 255.0;
  double in_scale = 1.0 / (n - 1);

  for (i = 0; (unsigned) i != n; i++)
    {
      table[i] = (int) (out_scale * pow (i * in_scale, r_gamma) + 0.5);
    }
}

int
pixma_find_scanners (const char **conf_devices)
{
  return pixma_collect_devices (conf_devices, pixma_devices);
}

const char *
pixma_get_device_model (unsigned devnr)
{
  const pixma_config_t *cfg = pixma_get_device_config (devnr);
  return (cfg) ? cfg->name : NULL;
}


int
pixma_get_device_status (pixma_t * s, pixma_device_status_t * status)
{
  if (!status)
    return PIXMA_EINVAL;
  memset (status, 0, sizeof (*status));
  return s->ops->get_status (s, status);
}
