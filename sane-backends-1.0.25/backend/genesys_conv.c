/* sane - Scanner Access Now Easy.

   Copyright (C) 2005, 2006 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
   Copyright (C) 2010-2013 Stéphane Voltz <stef.dev@free.fr>

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
 * Conversion filters for genesys backend
 */


/*8 bit*/
#define SINGLE_BYTE
#define BYTES_PER_COMPONENT 1
#define COMPONENT_TYPE uint8_t

#define FUNC_NAME(f) f ## _8

#include "genesys_conv_hlp.c"

#undef FUNC_NAME

#undef COMPONENT_TYPE
#undef BYTES_PER_COMPONENT
#undef SINGLE_BYTE

/*16 bit*/
#define DOUBLE_BYTE
#define BYTES_PER_COMPONENT 2
#define COMPONENT_TYPE uint16_t

#define FUNC_NAME(f) f ## _16

#include "genesys_conv_hlp.c"

#undef FUNC_NAME

#undef COMPONENT_TYPE
#undef BYTES_PER_COMPONENT
#undef DOUBLE_BYTE

static SANE_Status
genesys_reverse_bits(
    uint8_t *src_data,
    uint8_t *dst_data,
    size_t bytes)
{
    size_t i;
    for(i = 0; i < bytes; i++) {
	*dst_data++ = ~ *src_data++;
    }
    return SANE_STATUS_GOOD;
}

/**
 * uses the threshold/threshold_curve to control software binarization
 * This code was taken from the epjistsu backend by m. allan noah
 * @param dev device set up for the scan
 * @param src pointer to raw data
 * @param dst pointer where to store result
 * @param width width of the processed line
 * */
static SANE_Status
binarize_line(Genesys_Device * dev, uint8_t *src, uint8_t *dst, int width)
{
  int j, windowX, sum = 0;
  int thresh;
  int offset, addCol, dropCol;
  unsigned char mask;

  int x;
  uint8_t min, max;

  /* normalize line */
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

  /* ~1mm works best, but the window needs to have odd # of pixels */
  windowX = (6 * dev->settings.xres) / 150;
  if (!(windowX % 2))
    windowX++;

  /* second, prefill the sliding sum */
  for (j = 0; j < windowX; j++)
    sum += src[j];

  /* third, walk the input buffer, update the sliding sum, */
  /* determine threshold, output bits */
  for (j = 0; j < width; j++)
    {
      /* output image location */
      offset = j % 8;
      mask = 0x80 >> offset;
      thresh = dev->settings.threshold;

      /* move sum/update threshold only if there is a curve */
      if (dev->settings.threshold_curve)
	{
	  addCol = j + windowX / 2;
	  dropCol = addCol - windowX;

	  if (dropCol >= 0 && addCol < width)
	    {
	      sum -= src[dropCol];
	      sum += src[addCol];
	    }
	  thresh = dev->lineart_lut[sum / windowX];
	}

      /* use average to lookup threshold */
      if (src[j] > thresh)
	*dst &= ~mask;		/* white */
      else
	*dst |= mask;		/* black */

      if (offset == 7)
	dst++;
    }

  return SANE_STATUS_GOOD;
}

/**
 * software lineart using data from a 8 bit gray scan. We assume true gray
 * or monochrome scan as input.
 */
static SANE_Status
genesys_gray_lineart(
    Genesys_Device *dev,
    uint8_t *src_data,
    uint8_t *dst_data,
    size_t pixels,
    size_t lines,
    uint8_t threshold)
{
  size_t y;

  DBG (DBG_io2, "genesys_gray_lineart: converting %lu lines of %lu pixels\n",
       (unsigned long)lines, (unsigned long)pixels);
  DBG (DBG_io2, "genesys_gray_lineart: threshold=%d\n",threshold);

  for (y = 0; y < lines; y++)
    {
      binarize_line (dev, src_data + y * pixels, dst_data, pixels);
      dst_data += pixels / 8;
    }
  return SANE_STATUS_GOOD;
}

/** @brief shrink or grow scanned data to fit the final scan size
 * This function shrinks the scanned data it the required resolution is lower than the hardware one,
 * or grows it in case it is the opposite like when motor resolution is higher than
 * sensor's one.
 */
static SANE_Status
genesys_shrink_lines_1 (
    uint8_t *src_data,
    uint8_t *dst_data,
    unsigned int lines,
    unsigned int src_pixels,
    unsigned int dst_pixels,
    unsigned int channels)
{
  unsigned int dst_x, src_x, y, c, cnt;
  unsigned int avg[3], val;
  uint8_t *src = (uint8_t *) src_data;
  uint8_t *dst = (uint8_t *) dst_data;

  /* choose between case where me must reduce or grow the scanned data */
  if (src_pixels > dst_pixels)
    {
      /* shrink data */
      /* TODO action must be taken at bit level, no bytes */
      src_pixels /= 8;
      dst_pixels /= 8;
      /*take first _byte_ */
      for (y = 0; y < lines; y++)
	{
	  cnt = src_pixels / 2;
	  src_x = 0;
	  for (dst_x = 0; dst_x < dst_pixels; dst_x++)
	    {
	      while (cnt < src_pixels && src_x < src_pixels)
		{
		  cnt += dst_pixels;

		  for (c = 0; c < channels; c++)
		    avg[c] = *src++;
		  src_x++;
		}
	      cnt -= src_pixels;

	      for (c = 0; c < channels; c++)
		*dst++ = avg[c];
	    }
	}
    }
  else
    {
      /* common case where y res is double x res */
      for (y = 0; y < lines; y++)
	{
	  if (2 * src_pixels == dst_pixels)
	    {
	      /* double and interleave on line */
	      for (c = 0; c < src_pixels/8; c++)
		{
		  /* first 4 bits */
		  val = 0;
		  val |= (*src & 0x80) >> 0;	/* X___ ____ --> X___ ____ */
		  val |= (*src & 0x80) >> 1;	/* X___ ____ --> _X__ ____ */
		  val |= (*src & 0x40) >> 1;	/* _X__ ____ --> __X_ ____ */
		  val |= (*src & 0x40) >> 2;	/* _X__ ____ --> ___X ____ */
		  val |= (*src & 0x20) >> 2;	/* __X_ ____ --> ____ X___ */
		  val |= (*src & 0x20) >> 3;	/* __X_ ____ --> ____ _X__ */
		  val |= (*src & 0x10) >> 3;	/* ___X ____ --> ____ __X_ */
		  val |= (*src & 0x10) >> 4;	/* ___X ____ --> ____ ___X */
		  *dst = val;
		  dst++;

		  /* last for bits */
		  val = 0;
		  val |= (*src & 0x08) << 4;	/* ____ X___ --> X___ ____ */
		  val |= (*src & 0x08) << 3;	/* ____ X___ --> _X__ ____ */
		  val |= (*src & 0x04) << 3;	/* ____ _X__ --> __X_ ____ */
		  val |= (*src & 0x04) << 2;	/* ____ _X__ --> ___X ____ */
		  val |= (*src & 0x02) << 2;	/* ____ __X_ --> ____ X___ */
		  val |= (*src & 0x02) << 1;	/* ____ __X_ --> ____ _X__ */
		  val |= (*src & 0x01) << 1;	/* ____ ___X --> ____ __X_ */
		  val |= (*src & 0x01) << 0;	/* ____ ___X --> ____ ___X */
		  *dst = val;
		  dst++;
		  src++;
		}
	    }
	  else
	    {
	      /* TODO: since depth is 1, we must interpolate bit within bytes */
	      DBG (DBG_warn, "%s: inaccurate bit expansion!\n", __FUNCTION__);
	      cnt = dst_pixels / 2;
	      dst_x = 0;
	      for (src_x = 0; src_x < src_pixels; src_x++)
		{
		  for (c = 0; c < channels; c++)
		    avg[c] = *src++;
		  while (cnt < dst_pixels && dst_x < dst_pixels)
		    {
		      cnt += src_pixels;
		      for (c = 0; c < channels; c++)
			*dst++ = avg[c];
		      dst_x++;
		    }
		  cnt -= dst_pixels;
		}
	    }
	}
    }

  return SANE_STATUS_GOOD;
}


/** Look in image for likely left/right/bottom paper edges, then crop image.
 * Since failing to crop isn't fatal, we always return SANE_STATUS_GOOD .
 */
static SANE_Status
genesys_crop(Genesys_Scanner *s)
{
  SANE_Status status;
  Genesys_Device *dev = s->dev;
  int top = 0;
  int bottom = 0;
  int left = 0;
  int right = 0;

  DBG (DBG_proc, "%s: start\n", __FUNCTION__);

  /* first find edges if any */
  status = sanei_magic_findEdges (&s->params,
				  dev->img_buffer,
				  dev->settings.xres,
				  dev->settings.yres,
				  &top,
                                  &bottom,
                                  &left,
                                  &right);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_info, "%s: bad or no edges, bailing\n", __FUNCTION__);
      goto cleanup;
    }
  DBG (DBG_io, "%s: t:%d b:%d l:%d r:%d\n", __FUNCTION__, top, bottom, left,
       right);

  /* now crop the image */
  status =
    sanei_magic_crop (&(s->params), dev->img_buffer, top, bottom, left, right);
  if (status)
    {
      DBG (DBG_warn, "%s: failed to crop\n", __FUNCTION__);
      goto cleanup;
    }

  /* update counters to new image size */
  dev->total_bytes_to_read = s->params.bytes_per_line * s->params.lines;

cleanup:
  DBG (DBG_proc, "%s: completed\n", __FUNCTION__);
  return SANE_STATUS_GOOD;
}

/** Look in image for likely upper and left paper edges, then rotate
 * image so that upper left corner of paper is upper left of image.
 * @return since failure doens't prevent scanning, we always return
 * SANE_STATUS_GOOD
 */
static SANE_Status
genesys_deskew(Genesys_Scanner *s)
{
  SANE_Status status;
  Genesys_Device *dev = s->dev;

  int x = 0, y = 0, bg;
  double slope = 0;

  DBG (DBG_proc, "%s: start\n", __FUNCTION__);

  bg=0;
  if(s->params.format==SANE_FRAME_GRAY && s->params.depth == 1)
    {
      bg=0xff;
    }
  status = sanei_magic_findSkew (&s->params,
				 dev->img_buffer,
				 dev->sensor.optical_res,
				 dev->sensor.optical_res,
                                 &x,
                                 &y,
                                 &slope);
  if (status!=SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: bad findSkew, bailing\n", __FUNCTION__);
      return SANE_STATUS_GOOD;
    }
  DBG(DBG_info, "%s: slope=%f => %f\n",__FUNCTION__,slope, (slope/M_PI_2)*90);
  /* rotate image slope is in [-PI/2,PI/2]
   * positive values rotate trigonometric direction wise */
  status = sanei_magic_rotate (&s->params,
			       dev->img_buffer,
                               x,
                               y,
                               slope,
                               bg);
  if (status!=SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: rotate error: %s", __FUNCTION__, sane_strstatus(status));
    }

  DBG (DBG_proc, "%s: completed\n", __FUNCTION__);
  return SANE_STATUS_GOOD;
}

/** remove lone dots
 * @return since failure doens't prevent scanning, we always return
 * SANE_STATUS_GOOD
 */
static SANE_Status
genesys_despeck(Genesys_Scanner *s)
{
  if(sanei_magic_despeck(&s->params,
                         s->dev->img_buffer,
                         s->val[OPT_DESPECK].w)!=SANE_STATUS_GOOD)
  {
    DBG (DBG_error, "%s: bad despeck, bailing\n",__FUNCTION__);
  }

  return SANE_STATUS_GOOD;
}

/** Look if image needs rotation and apply it
 * */
static SANE_Status
genesys_derotate (Genesys_Scanner * s)
{
  SANE_Status status;
  int angle = 0;
  int resolution = s->val[OPT_RESOLUTION].w;

  DBGSTART;
  status = sanei_magic_findTurn (&s->params,
				 s->dev->img_buffer,
				 resolution,
                                 resolution,
                                 &angle);

  if (status)
    {
      DBG (DBG_warn, "%s: failed : %d\n", __FUNCTION__, status);
      DBGCOMPLETED;
      return SANE_STATUS_GOOD;
    }

  /* apply rotation angle found */
  status = sanei_magic_turn (&s->params, s->dev->img_buffer, angle);
  if (status)
    {
      DBG (DBG_warn, "%s: failed : %d\n", __FUNCTION__, status);
      DBGCOMPLETED;
      return SANE_STATUS_GOOD;
    }

  /* update counters to new image size */
  s->dev->total_bytes_to_read = s->params.bytes_per_line * s->params.lines;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
