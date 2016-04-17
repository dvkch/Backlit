/* sane - Scanner Access Now Easy.

   ScanMaker 3840 Backend
   Copyright (C) 2005-7 Earle F. Philhower, III
   earle@ziplabel.com - http://www.ziplabel.com

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

#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include "sm3840_lib.h"

#ifndef BACKENDNAME
/* For standalone compilation */
static void
DBG (int ignored, const char *fmt, ...)
{
  va_list a;
  va_start (a, fmt);
  vfprintf (stderr, fmt, a);
  va_end (a);
}
#else
/* For SANE compilation, convert libusb to sanei_usb */
static int
my_usb_bulk_write (p_usb_dev_handle dev, int ep,
		   unsigned char *bytes, int size, int timeout)
{
  SANE_Status status;
  size_t my_size;

  timeout = timeout;
  ep = ep;
  my_size = size;
  status =
    sanei_usb_write_bulk ((SANE_Int) dev, (SANE_Byte *) bytes, &my_size);
  if (status != SANE_STATUS_GOOD)
    my_size = -1;
  return my_size;
}

static int
my_usb_bulk_read (p_usb_dev_handle dev, int ep,
		  unsigned char *bytes, int size, int timeout)
{
  SANE_Status status;
  size_t my_size;

  timeout = timeout;
  ep = ep;
  my_size = size;
  status =
    sanei_usb_read_bulk ((SANE_Int) dev, (SANE_Byte *) bytes, &my_size);
  if (status != SANE_STATUS_GOOD)
    my_size = -1;
  return my_size;
}

static int
my_usb_control_msg (p_usb_dev_handle dev, int requesttype,
		    int request, int value, int index,
		    unsigned char *bytes, int size, int timeout)
{
  SANE_Status status;

  timeout = timeout;
  status = sanei_usb_control_msg ((SANE_Int) dev, (SANE_Int) requesttype,
				  (SANE_Int) request, (SANE_Int) value,
				  (SANE_Int) index, (SANE_Int) size,
				  (SANE_Byte *) bytes);
  return status;
}
#endif


/* Sanitize and convert scan parameters from inches to pixels */
static void
prepare_params (SM3840_Params * params)
{
  if (params->gray)
    params->gray = 1;
  if (params->lineart) {
    params->gray = 1;
    params->lineart = 1;
  }
  if (params->halftone) {
    params->gray = 1;
    params->halftone = 1;
  }
  if (params->dpi != 1200 && params->dpi != 600 && params->dpi != 300
      && params->dpi != 150)
    params->dpi = 150;
  if (params->bpp != 8 && params->bpp != 16)
    params->bpp = 8;

  /* Sanity check input sizes */
  if (params->top < 0)
    params->top = 0;
  if (params->left < 0)
    params->left = 0;
  if (params->width < 0)
    params->width = 0;
  if (params->height < 0)
    params->height = 0;
  if ((params->top + params->height) > 11.7)
    params->height = 11.7 - params->top;
  if ((params->left + params->width) > 8.5)
    params->width = 8.5 - params->left;

  params->topline = params->top * params->dpi;
  params->scanlines = params->height * params->dpi;
  params->leftpix = params->left * params->dpi;
  params->leftpix &= ~1;	/* Always start on even pixel boundary for remap */
  /* Scanpix always a multiple of 128 */
  params->scanpix = params->width * params->dpi;
  params->scanpix = (params->scanpix + 127) & ~127;

  /* Sanity check outputs... */
  if (params->topline < 0)
    params->topline = 0;
  if (params->scanlines < 1)
    params->scanlines = 1;
  if (params->leftpix < 0)
    params->leftpix = 0;
  if (params->scanpix < 128)
    params->scanpix = 128;

  /* Some handy calculations */
  params->linelen =
    params->scanpix * (params->bpp / 8) * (params->gray ? 1 : 3);
}

#ifndef BACKENDNAME
usb_dev_handle *
find_device (unsigned int idVendor, unsigned int idProduct)
{
  struct usb_bus *bus;
  struct usb_device *dev;

  usb_init ();
  usb_find_busses ();
  usb_find_devices ();

  for (bus = usb_get_busses (); bus; bus = bus->next)
    for (dev = bus->devices; dev; dev = dev->next)
      if (dev->descriptor.idVendor == idVendor &&
	  dev->descriptor.idProduct == idProduct)
	return usb_open (dev);

  return NULL;
}
#endif

static void
idle_ab (p_usb_dev_handle udev)
{
  int len, i;
  unsigned char buff[8] = { 0x64, 0x65, 0x16, 0x17, 0x64, 0x65, 0x44, 0x45 };
  for (i = 0; i < 8; i++)
    {
      len = usb_control_msg (udev, 0x40, 0x0c, 0x0090, 0x0000, buff + i,
			     0x0001, wr_timeout);
    }
}

/* CW: 40 04 00b0 0000 <len> :  <reg1> <value1> <reg2> <value2> ... */
static void
write_regs (p_usb_dev_handle udev, int regs, unsigned char reg1,
	    unsigned char val1,
	    ... /*unsigned char reg, unsigned char val, ... */ )
{
  unsigned char buff[512];
  va_list marker;
  int len, i;

  va_start (marker, val1);
  buff[0] = reg1;
  buff[1] = val1;
  for (i = 1; i < regs; i++)
    {
      buff[i * 2] = va_arg (marker, int);
      buff[i * 2 + 1] = va_arg (marker, int);
    }
  va_end (marker);

  len = usb_control_msg (udev, 0x40, 0x04, 0x00b0, 0x0000, buff,
			 regs * 2, wr_timeout);
}

static int
write_vctl (p_usb_dev_handle udev, int request, int value,
	    int index, unsigned char byte)
{
  return usb_control_msg (udev, 0x40, request, value, index,
			  &byte, 1, wr_timeout);
}

static int
read_vctl (p_usb_dev_handle udev, int request, int value,
	   int index, unsigned char *byte)
{
  return usb_control_msg (udev, 0xc0, request, value, index,
			  byte, 1, wr_timeout);
}

#ifndef BACKENDNAME
/* Copy from a USB data pipe to a file with optional header */
static void
record_head (p_usb_dev_handle udev, char *fname, int bytes, char *header)
{
  FILE *fp;
  unsigned char buff[65536];
  int len;
  int toread;

  fp = fopen (fname, "wb");
  if (header)
    fprintf (fp, "%s", header);
  do
    {
      toread = (bytes > 65536) ? 65536 : bytes;
      len = usb_bulk_read (udev, 1, buff, toread, rd_timeout);
      if (len > 0)
	{
	  fwrite (buff, len, 1, fp);
	  bytes -= len;
	}
      else
	{
	  DBG (2, "TIMEOUT\n");
	}
    }
  while (bytes);
  fclose (fp);
}

/* Copy from a USB data pipe to a file without header */
static void
record (p_usb_dev_handle udev, char *fname, int bytes)
{
  record_head (udev, fname, bytes, "");
}
#endif

static int
max (int a, int b)
{
  return (a > b) ? a : b;
}


#define DPI1200SHUFFLE 6
static void
record_line (int reset,
	     p_usb_dev_handle udev,
	     unsigned char *storeline,
	     int dpi, int scanpix, int gray, int bpp16,
	     int *save_i,
	     unsigned char **save_scan_line,
	     unsigned char **save_dpi1200_remap,
	     unsigned char **save_color_remap)
{
  int len;
  unsigned char *scan_line, *dpi1200_remap;
  unsigned char *color_remap;
  int i;
  int red_delay, blue_delay, green_delay;
  int j;
  int linelen;
  int pixsize;
  unsigned char *ptrcur, *ptrprev;
  unsigned char *savestoreline;
  int lineptr, outline, bufflines;

  i = *save_i;
  scan_line = *save_scan_line;
  dpi1200_remap = *save_dpi1200_remap;
  color_remap = *save_color_remap;

  pixsize = (gray ? 1 : 3) * (bpp16 ? 2 : 1);
  linelen = scanpix * pixsize;

  if (gray)
    {
      red_delay = 0;
      blue_delay = 0;
      green_delay = 0;
    }
  else if (dpi == 150)
    {
      red_delay = 0;
      blue_delay = 6;
      green_delay = 3;
    }
  else if (dpi == 300)
    {
      red_delay = 0;
      blue_delay = 12;
      green_delay = 6;
    }
  else if (dpi == 600)
    {
      red_delay = 0;
      blue_delay = 24;
      green_delay = 12;
    }
  else				/*(dpi==1200) */
    {
      red_delay = 0;
      blue_delay = 48;
      green_delay = 24;
    }

  bufflines = max (max (red_delay, blue_delay), green_delay) + 1;

  if (reset)
    {
      if (dpi1200_remap)
	free (dpi1200_remap);
      if (scan_line)
	free (scan_line);
      if (color_remap)
	free (color_remap);

      *save_color_remap = color_remap =
	(unsigned char *) malloc (bufflines * linelen);

      *save_scan_line = scan_line = (unsigned char *) calloc (linelen, 1);
      if (dpi == 1200)
	*save_dpi1200_remap = dpi1200_remap =
	  (unsigned char *) calloc (linelen, DPI1200SHUFFLE);
      else
	*save_dpi1200_remap = dpi1200_remap = NULL;

      *save_i = i = 0;		/* i is our linenumber */
    }

  while (1)
    {				/* We'll exit inside the loop... */
      len = usb_bulk_read (udev, 1, scan_line, linelen, rd_timeout);
      if (dpi == 1200)
	{
	  ptrcur = dpi1200_remap + (linelen * (i % DPI1200SHUFFLE));
	  ptrprev =
	    dpi1200_remap +
	    (linelen * ((i - (DPI1200SHUFFLE - 2)) % DPI1200SHUFFLE));
	}
      else
	{
	  ptrcur = scan_line;
	  ptrprev = NULL;
	}
      if (gray)
	{
	  memcpy (ptrcur, scan_line, linelen);
	}
      else
	{
	  int pixsize = bpp16 ? 2 : 1;
	  int pix = linelen / (3 * pixsize);
	  unsigned char *outbuff = ptrcur;

	  outline = i;
	  lineptr = i % bufflines;

	  memcpy (color_remap + linelen * lineptr, scan_line, linelen);

	  outline++;
	  if (outline > bufflines)
	    {
	      int redline = (outline + red_delay) % bufflines;
	      int greenline = (outline + green_delay) % bufflines;
	      int blueline = (outline + blue_delay) % bufflines;
	      unsigned char *rp, *gp, *bp;

	      rp = color_remap + linelen * redline;
	      gp = color_remap + linelen * greenline + 1 * pixsize;
	      bp = color_remap + linelen * blueline + 2 * pixsize;

	      for (j = 0; j < pix; j++)
		{
		  if (outbuff)
		    {
		      *(outbuff++) = *rp;
		      if (pixsize == 2)
			*(outbuff++) = *(rp + 1);
		      *(outbuff++) = *gp;
		      if (pixsize == 2)
			*(outbuff++) = *(gp + 1);
		      *(outbuff++) = *bp;
		      if (pixsize == 2)
			*(outbuff++) = *(bp + 1);
		    }
		  rp += 3 * pixsize;
		  gp += 3 * pixsize;
		  bp += 3 * pixsize;
		}
	    }
	  lineptr = (lineptr + 1) % bufflines;
	}

      if (dpi != 1200)
	{
	  if (i > blue_delay)
	    {
	      savestoreline = storeline;
	      for (j = 0; j < scanpix; j++)
		{
		  memcpy (storeline, ptrcur + linelen - (j + 1) * pixsize,
			  pixsize);
		  storeline += pixsize;
		}
	      if (dpi == 150)
		{
		  /* 150 DPI skips every 4th returned line */
		  if (i % 4)
		    {
		      i++;
		      *save_i = i;
		      if (bpp16)
			fix_endian_short ((unsigned short *) storeline,
					  scanpix);
		      return;
		    }
		  storeline = savestoreline;
		}
	      else
		{
		  i++;
		  *save_i = i;
		  if (bpp16)
		    fix_endian_short ((unsigned short *) storeline, scanpix);
		  return;
		}
	    }
	}
      else			/* dpi==1200 */
	{
	  if (i > (DPI1200SHUFFLE + blue_delay))
	    {
	      for (j = 0; j < scanpix; j++)
		{
		  if (1 == (j & 1))
		    memcpy (storeline, ptrcur + linelen - (j + 1) * pixsize,
			    pixsize);
		  else
		    memcpy (storeline, ptrprev + linelen - (j + 1) * pixsize,
			    pixsize);
		  storeline += pixsize;
		}		/* end for */
	      i++;
	      *save_i = i;
	      if (bpp16)
		fix_endian_short ((unsigned short *) storeline, scanpix);
	      return;
	    }			/* end if >dpi1200shuffle */
	}			/* end if dpi1200 */
      i++;
    }				/* end for */
}


#ifndef BACKENDNAME
/* Record an image to a file with remapping/reordering/etc. */
void
record_image (p_usb_dev_handle udev, char *fname, int dpi,
	      int scanpix, int scanlines, int gray, char *head, int bpp16)
{
  FILE *fp;
  int i;
  int save_i;
  unsigned char *save_scan_line;
  unsigned char *save_dpi1200_remap;
  unsigned char *save_color_remap;
  unsigned char *storeline;

  save_i = 0;
  save_scan_line = save_dpi1200_remap = save_color_remap = NULL;
  storeline =
    (unsigned char *) malloc (scanpix * (gray ? 1 : 3) * (bpp16 ? 2 : 1));

  fp = fopen (fname, "wb");
  if (head)
    fprintf (fp, "%s", head);

  for (i = 0; i < scanlines; i++)
    {
      record_line ((i == 0) ? 1 : 0, udev, storeline, dpi, scanpix, gray,
		   bpp16, &save_i, &save_scan_line, &save_dpi1200_remap,
		   &save_color_remap);
      fwrite (storeline, scanpix * (gray ? 1 : 3) * (bpp16 ? 2 : 1), 1, fp);
    }
  fclose (fp);
  if (save_scan_line)
    free (save_scan_line);
  if (save_dpi1200_remap)
    free (save_dpi1200_remap);
  if (save_color_remap)
    free (save_color_remap);
  free (storeline);
}
#endif

static void
record_mem (p_usb_dev_handle udev, unsigned char **dest, int bytes)
{
  unsigned char *mem;
  unsigned char buff[65536];
  int len;

  mem = (unsigned char *) malloc (bytes);
  *dest = mem;
  do
    {
      len =
	usb_bulk_read (udev, 1, buff, bytes > 65536 ? 65536 : bytes,
		       rd_timeout);
      if (len > 0)
	{
	  memcpy (mem, buff, len);
	  bytes -= len;
	  mem += len;
	}
    }
  while (bytes);
}


static void
reset_scanner (p_usb_dev_handle udev)
{
  unsigned char rd_byte;

  DBG (2, "+reset_scanner\n");
  write_regs (udev, 5, 0x83, 0x00, 0xa3, 0xff, 0xa4, 0xff, 0xa1, 0xff,
	      0xa2, 0xff);
  write_vctl (udev, 0x0c, 0x0001, 0x0000, 0x00);
  write_regs (udev, 2, 0xbe, 0x00, 0x84, 0x00);
  write_vctl (udev, 0x0c, 0x00c0, 0x8406, 0x00);
  write_vctl (udev, 0x0c, 0x00c0, 0x0406, 0x00);
  write_regs (udev, 16, 0xbe, 0x18, 0x80, 0x00, 0x84, 0x00, 0x89, 0x00,
	      0x88, 0x00, 0x86, 0x00, 0x90, 0x00, 0xc1, 0x00,
	      0xc2, 0x00, 0xc3, 0x00, 0xc4, 0x00, 0xc5, 0x00,
	      0xc6, 0x00, 0xc7, 0x00, 0xc8, 0x00, 0xc0, 0x00);
  write_regs (udev, 16, 0x84, 0x94, 0x80, 0xd1, 0x80, 0xc1, 0x82, 0x7f,
	      0xcf, 0x04, 0xc1, 0x02, 0xc2, 0x00, 0xc3, 0x06,
	      0xc4, 0xff, 0xc5, 0x40, 0xc6, 0x8c, 0xc7, 0xdc,
	      0xc8, 0x20, 0xc0, 0x72, 0x89, 0xff, 0x86, 0xff);
  write_regs (udev, 7, 0xa8, 0x80, 0x83, 0xa2, 0x85, 0xc8, 0x83, 0x82,
	      0x85, 0xaf, 0x83, 0x00, 0x93, 0x00);
  write_regs (udev, 3, 0xa8, 0x0a, 0x8c, 0x08, 0x94, 0x00);
  write_regs (udev, 4, 0x83, 0x00, 0xa3, 0x00, 0xa4, 0x00, 0x97, 0x0a);
  write_vctl (udev, 0x0c, 0x0004, 0x008b, 0x00);
  read_vctl (udev, 0x0c, 0x0007, 0x0000, &rd_byte);
  write_regs (udev, 1, 0x97, 0x0b);
  write_vctl (udev, 0x0c, 0x0004, 0x008b, 0x00);
  read_vctl (udev, 0x0c, 0x0007, 0x0000, &rd_byte);
  write_regs (udev, 1, 0x97, 0x0f);
  write_vctl (udev, 0x0c, 0x0004, 0x008b, 0x00);
  read_vctl (udev, 0x0c, 0x0007, 0x0000, &rd_byte);
  write_regs (udev, 1, 0x97, 0x05);
  write_vctl (udev, 0x0c, 0x0004, 0x008b, 0x00);
  read_vctl (udev, 0x0c, 0x0007, 0x0000, &rd_byte);
  DBG (2, "-reset_scanner\n");
}

static void
poll1 (p_usb_dev_handle udev)
{
  unsigned char rd_byte;
  DBG (2, "+poll1\n");
  do
    {
      write_regs (udev, 1, 0x97, 0x00);
      write_vctl (udev, 0x0c, 0x0004, 0x008b, 0x00);
      read_vctl (udev, 0x0c, 0x0007, 0x0000, &rd_byte);
    }
  while (0 == (rd_byte & 0x40));
  DBG (2, "-poll1\n");
}

static void
poll2 (p_usb_dev_handle udev)
{
  unsigned char rd_byte;
  DBG (2, "+poll2\n");
  do
    {
      write_vctl (udev, 0x0c, 0x0004, 0x008b, 0x00);
      read_vctl (udev, 0x0c, 0x0007, 0x0000, &rd_byte);
    }
  while (0 == (rd_byte & 0x02));
  DBG (2, "-poll2\n");
}

#ifndef BACKENDNAME
static void
check_buttons (p_usb_dev_handle udev, int *scan, int *print, int *mail)
{
  unsigned char rd_byte;

  write_regs (udev, 4, 0x83, 0x00, 0xa3, 0x00, 0xa4, 0x00, 0x97, 0x0a);
  write_vctl (udev, 0x0c, 0x0004, 0x008b, 0x00);
  read_vctl (udev, 0x0c, 0x0007, 0x0000, &rd_byte);
  if (scan)
    {
      if (0 == (rd_byte & 1))
	*scan = 1;
      else
	*scan = 0;
    }
  if (print)
    {
      if (0 == (rd_byte & 2))
	*print = 1;
      else
	*print = 0;
    }
  if (mail)
    {
      if (0 == (rd_byte & 4))
	*mail = 1;
      else
	*mail = 0;
    }
  write_regs (udev, 1, 0x97, 0x0b);
  write_vctl (udev, 0x0c, 0x0004, 0x008b, 0x00);
  read_vctl (udev, 0x0c, 0x0007, 0x0000, &rd_byte);
  idle_ab (udev);
  write_regs (udev, 1, 0x97, 0x0f);
  write_vctl (udev, 0x0c, 0x0004, 0x008b, 0x00);
  read_vctl (udev, 0x0c, 0x0007, 0x0000, &rd_byte);
  idle_ab (udev);
  write_regs (udev, 1, 0x97, 0x05);
  write_vctl (udev, 0x0c, 0x0004, 0x008b, 0x00);
  read_vctl (udev, 0x0c, 0x0007, 0x0000, &rd_byte);
  idle_ab (udev);
}
#endif

static int
lut (int val, double gain, int offset)
{
  /* int offset = 1800; */
  /* double exponent = 3.5; */
  return (offset + 8192.0 * (pow ((8192.0 - val) / 8192.0, gain)));
}


static void
calc_lightmap (unsigned short *buff,
	       unsigned short *storage, int index, int dpi,
	       double gain, int offset)
{
  int val, line;
  int px = 5632;
  int i;

  line = px * 3;

  for (i = 0; i < px; i++)
    {
      if ((i >= 2) && (i <= (px - 2)))
	{
	  val = 0;
	  val += 1 * buff[i * 3 + index - 3 * 2];
	  val += 3 * buff[i * 3 + index - 3 * 1];
	  val += 5 * buff[i * 3 + index + 3 * 0];
	  val += 3 * buff[i * 3 + index + 3 * 1];
	  val += 1 * buff[i * 3 + index + 3 * 2];
	  val += 2 * buff[i * 3 + index - 3 * 1 + line];
	  val += 3 * buff[i * 3 + index + 3 * 0 + line];
	  val += 2 * buff[i * 3 + index + 3 * 1 + line];
	  val += 1 * buff[i * 3 + index + 3 * 0 + 2 * line];
	  val /= 21;
	}
      else
	{
	  val = 1 * buff[i * 3 + index];
	}

      val = val >> 3;
      if (val > 8191)
	val = 8191;
      val = lut (val, gain, offset);

      if (val > 8191)
	val = 8191;
      if (val < 0)
	val = 0;
      storage[i * (dpi == 1200 ? 2 : 1)] = val;
      if (dpi == 1200)
	storage[i * 2 + 1] = val;
    }

  fix_endian_short (storage, (dpi == 1200) ? i * 2 : i);
}




/*#define VALMAP  0x1fff */
/*#define SCANMAP 0x2000*/
#define RAWPIXELS600 7320
#define RAWPIXELS1200 14640

static void
select_pixels (unsigned short *map, int dpi, int start, int end)
{
  int i;
  int skip, offset;
  unsigned short VALMAP = 0x1fff;
  unsigned short SCANMAP = 0x2000;

  fix_endian_short (&VALMAP, 1);
  fix_endian_short (&SCANMAP, 1);

  /* Clear the pixel-on flags for all bits */
  for (i = 0; i < ((dpi == 1200) ? RAWPIXELS1200 : RAWPIXELS600); i++)
    map[i] &= VALMAP;

  /* 300 and 150 have subsampling */
  if (dpi == 300)
    skip = -2;
  else if (dpi == 150)
    skip = -4;
  else
    skip = -1;

  /* 1200 has 2x pixels so start at 2x the offset */
  if (dpi == 1200)
    offset = 234 + (8.5 * 1200);
  else
    offset = 117 + (8.5 * 600);

  if (0 == (offset & 1))
    offset++;

  DBG (2, "offset=%d start=%d skip=%d\n", offset, start, skip);

  for (i = 0; i < end; i++)
    {
      int x;
      x = offset + (start * skip) + (i * skip);
      if (x < 0 || x > ((dpi == 1200) ? RAWPIXELS1200 : RAWPIXELS600))
	DBG (2, "ERR %d\n", x);
      else
	map[x] |= SCANMAP;
    }
}


static void
set_lightmap_white (unsigned short *map, int dpi, int color)
{
  int i;
  unsigned short VALMAP = 0x1fff;
  unsigned short SCANMAP = 0x2000;

  fix_endian_short (&VALMAP, 1);
  fix_endian_short (&SCANMAP, 1);

  if (dpi != 1200)
    {
      memset (map, 0, RAWPIXELS600 * 2);
      if (color != 0)
	return;			/* Only 1st has enables... */
      for (i = 0; i < 22; i++)
	map[7 + i] = SCANMAP;	/* Get some black... */
      for (i = 0; i < 1024; i++)
	map[2048 + i] = SCANMAP;	/* And some white... */
    }
  else
    {
      memset (map, 0, RAWPIXELS1200 * 2);
      if (color != 0)
	return;
      for (i = 16; i < 61; i++)
	map[i] = SCANMAP;
      for (i = 4076; i < 6145; i++)
	map[i] = SCANMAP;
      /* 3rd is all clear */
    }
}


static void
set_lamp_timer (p_usb_dev_handle udev, int timeout_in_mins)
{
  write_regs (udev, 7, 0xa8, 0x80, 0x83, 0xa2, 0x85, 0xc8, 0x83, 0x82,
	      0x85, 0xaf, 0x83, 0x00, 0x93, 0x00);
  write_regs (udev, 3, 0xa8, timeout_in_mins * 2, 0x8c, 0x08, 0x94, 0x00);
  idle_ab (udev);
  write_regs (udev, 4, 0x83, 0x20, 0x8d, 0x26, 0x83, 0x00, 0x8d, 0xff);
}


static void
calculate_lut8 (double *poly, int skip, unsigned char *dest)
{
  int i;
  double sum, x;
  if (!poly || !dest)
    return;
  for (i = 0; i < 8192; i += skip)
    {
      sum = poly[0];
      x = (double) i;
      sum += poly[1] * x;
      x = x * i;
      sum += poly[2] * x;
      x = x * i;
      sum += poly[3] * x;
      x = x * i;
      sum += poly[4] * x;
      x = x * i;
      sum += poly[5] * x;
      x = x * i;
      sum += poly[6] * x;
      x = x * i;
      sum += poly[7] * x;
      x = x * i;
      sum += poly[8] * x;
      x = x * i;
      sum += poly[9] * x;
      x = x * i;
      if (sum < 0)
	sum = 0;
      if (sum > 255)
	sum = 255;
      *dest = (unsigned char) sum;
      dest++;
    }
}

static void
download_lut8 (p_usb_dev_handle udev, int dpi, int incolor)
{
  double color[10] = { 0.0, 1.84615261590892E-01, -2.19613868292657E-04,
    1.79549523214101E-07, -8.69378513113239E-11,
    2.56694911984996E-14, -4.67288886157239E-18,
    5.11622894146250E-22, -3.08729724411991E-26,
    7.88581670873938E-31
  };
  double gray[10] = { 0.0, 1.73089945056694E-01, -1.39794924306080E-04,
    9.70266873814926E-08, -4.57503008236968E-11,
    1.37092321631794E-14, -2.54395068387198E-18,
    2.82432084125966E-22, -1.71787408822688E-26,
    4.40306800664567E-31
  };
  unsigned char *lut;
  int len;

  DBG (2, "+download_lut8\n");
  switch (dpi)
    {
    case 150:
    case 300:
    case 600:
      lut = (unsigned char *) malloc (4096);
      if (!lut)
	return;

      if (!incolor)
	{
	  calculate_lut8 (gray, 2, lut);
	  write_regs (udev, 6, 0xb0, 0x00, 0xb1, 0x20, 0xb2, 0x07, 0xb3, 0xff,
		      0xb4, 0x2f, 0xb5, 0x07);
	  write_vctl (udev, 0x0c, 0x0002, 0x1000, 0x00);
	  len = usb_bulk_write (udev, 2, lut, 4096, wr_timeout);
	}
      else
	{
	  calculate_lut8 (color, 2, lut);
	  write_regs (udev, 6, 0xb0, 0x00, 0xb1, 0x10, 0xb2, 0x07, 0xb3, 0xff,
		      0xb4, 0x1f, 0xb5, 0x07);
	  write_vctl (udev, 0x0c, 0x0002, 0x1000, 0x00);
	  len = usb_bulk_write (udev, 2, lut, 4096, wr_timeout);
	  write_regs (udev, 6, 0xb0, 0x00, 0xb1, 0x20, 0xb2, 0x07, 0xb3, 0xff,
		      0xb4, 0x2f, 0xb5, 0x07);
	  write_vctl (udev, 0x0c, 0x0002, 0x1000, 0x00);
	  len = usb_bulk_write (udev, 2, lut, 4096, wr_timeout);
	  write_regs (udev, 6, 0xb0, 0x00, 0xb1, 0x30, 0xb2, 0x07, 0xb3, 0xff,
		      0xb4, 0x3f, 0xb5, 0x07);
	  write_vctl (udev, 0x0c, 0x0002, 0x1000, 0x00);
	  len = usb_bulk_write (udev, 2, lut, 4096, wr_timeout);
	}
      break;

    case 1200:
    default:
      lut = (unsigned char *) malloc (8192);
      if (!lut)
	return;

      if (!incolor)
	{
	  calculate_lut8 (gray, 1, lut);
	  write_regs (udev, 6, 0xb0, 0x00, 0xb1, 0x40, 0xb2, 0x06, 0xb3, 0xff,
		      0xb4, 0x5f, 0xb5, 0x06);
	  write_vctl (udev, 0x0c, 0x0002, 0x2000, 0x00);
	  len = usb_bulk_write (udev, 2, lut, 8192, wr_timeout);
	}
      else
	{
	  calculate_lut8 (color, 1, lut);
	  write_regs (udev, 6, 0xb0, 0x00, 0xb1, 0x20, 0xb2, 0x06, 0xb3, 0xff,
		      0xb4, 0x3f, 0xb5, 0x06);
	  write_vctl (udev, 0x0c, 0x0002, 0x2000, 0x00);
	  len = usb_bulk_write (udev, 2, lut, 8192, wr_timeout);
	  write_regs (udev, 6, 0xb0, 0x00, 0xb1, 0x40, 0xb2, 0x06, 0xb3, 0xff,
		      0xb4, 0x5f, 0xb5, 0x06);
	  write_vctl (udev, 0x0c, 0x0002, 0x2000, 0x00);
	  len = usb_bulk_write (udev, 2, lut, 8192, wr_timeout);
	  write_regs (udev, 6, 0xb0, 0x00, 0xb1, 0x60, 0xb2, 0x06, 0xb3, 0xff,
		      0xb4, 0x7f, 0xb5, 0x06);
	  write_vctl (udev, 0x0c, 0x0002, 0x2000, 0x00);
	  len = usb_bulk_write (udev, 2, lut, 8192, wr_timeout);
	}
      break;
    }

  free (lut);
  DBG (2, "-download_lut8\n");
}


static void
set_gain_black (p_usb_dev_handle udev,
		int r_gain, int g_gain, int b_gain,
		int r_black, int g_black, int b_black)
{
  write_regs (udev, 1, 0x80, 0x00);
  write_regs (udev, 1, 0x80, 0x01);
  write_regs (udev, 1, 0x04, 0x80);
  write_regs (udev, 1, 0x04, 0x00);
  write_regs (udev, 1, 0x00, 0x00);
  write_regs (udev, 1, 0x01, 0x03);
  write_regs (udev, 1, 0x02, 0x04);
  write_regs (udev, 1, 0x03, 0x11);
  write_regs (udev, 1, 0x05, 0x00);
  write_regs (udev, 1, 0x28, r_gain);
  write_regs (udev, 1, 0x29, g_gain);
  write_regs (udev, 1, 0x2a, b_gain);
  write_regs (udev, 1, 0x20, r_black);
  write_regs (udev, 1, 0x21, g_black);
  write_regs (udev, 1, 0x22, b_black);
  write_regs (udev, 1, 0x24, 0x00);
  write_regs (udev, 1, 0x25, 0x00);
  write_regs (udev, 1, 0x26, 0x00);
}

/* Handle short endianness issues */
static void
fix_endian_short (unsigned short *data, int count)
{
  unsigned short testvalue = 255;
  unsigned char *firstbyte = (unsigned char *) &testvalue;
  int i;

  if (*firstbyte == 255)
    return;			/* INTC endianness */

  DBG (2, "swapping endiannes...\n");
  for (i = 0; i < count; i++)
    data[i] = ((data[i] >> 8) & 0x00ff) | ((data[i] << 8) & 0xff00);
}
