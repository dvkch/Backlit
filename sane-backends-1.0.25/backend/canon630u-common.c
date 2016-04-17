/*
   (c) 2001,2002 Nathan Rutman  nathan@gordian.com  10/17/01

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
   Communication, calibration, and scanning with the Canon CanoScan FB630U
   flatbed scanner under linux.
   
   Reworked into SANE-compatible format.
   
   The usb-parallel port interface chip is GL640usb, on the far side of
   which is an LM9830 parallel-port scanner-on-a-chip.

   This code has not been tested on anything other than Linux/i386.
*/

#include <errno.h>
#include <fcntl.h>		/* open */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>		/* usleep */
#include <time.h>
#include <math.h>               /* exp() */
#ifdef HAVE_OS2_H
#include <sys/types.h> 		/* mode_t */
#endif
#include <sys/stat.h>
#include "lm9830.h"

#define USB_TYPE_VENDOR                 (0x02 << 5)
#define USB_RECIP_DEVICE                0x00
#define USB_DIR_OUT                     0x00
#define USB_DIR_IN                      0x80

/* Assign status and verify a good return code */
#define CHK(A) {if( (status = A) != SANE_STATUS_GOOD ) { \
                 DBG( 1, "Failure on line of %s: %d\n", __FILE__, \
                      __LINE__ ); return A; }}


typedef SANE_Byte byte;


/*****************************************************
            GL640 communication primitives
   Provides I/O routines to Genesys Logic GL640USB USB-IEEE1284 parallel
   port bridge.  Used in HP3300c, Canon FB630u.
******************************************************/

/* Register codes for the bridge.  These are NOT the registers for the
   scanner chip on the other side of the bridge. */
typedef enum
{
  GL640_BULK_SETUP = 0x82,
  GL640_EPP_ADDR = 0x83,
  GL640_EPP_DATA_READ = 0x84,
  GL640_EPP_DATA_WRITE = 0x85,
  GL640_SPP_STATUS = 0x86,
  GL640_SPP_CONTROL = 0x87,
  GL640_SPP_DATA = 0x88,
  GL640_GPIO_OE = 0x89,
  GL640_GPIO_READ = 0x8a,
  GL640_GPIO_WRITE = 0x8b
}
GL640_Request;

/* Write to the usb-parallel port bridge. */
static SANE_Status
gl640WriteControl (int fd, GL640_Request req, byte * data, unsigned int size)
{
  SANE_Status status;
  status = sanei_usb_control_msg (fd,
				  /* rqttype */ USB_TYPE_VENDOR |
				  USB_RECIP_DEVICE | USB_DIR_OUT /*0x40? */ ,
				  /* rqt */ (size > 1) ? 0x04 : 0x0C,
				  /* val */ (SANE_Int) req,
				  /* ind */ 0,
				  /* len */ size,
				  /* dat */ data);
  if (status != SANE_STATUS_GOOD)
    DBG (1, "gl640WriteControl error\n");
  return status;
}


/* Read from the usb-parallel port bridge. */
static SANE_Status
gl640ReadControl (int fd, GL640_Request req, byte * data, unsigned int size)
{
  SANE_Status status;
  status = sanei_usb_control_msg (fd,
				  /* rqttype */ USB_TYPE_VENDOR |
				  USB_RECIP_DEVICE | USB_DIR_IN /*0xc0? */ ,
				  /* rqt */ (size > 1) ? 0x04 : 0x0C,
				  /* val */ (SANE_Int) req,
				  /* ind */ 0,
				  /* len */ size,
				  /* dat */ data);
  if (status != SANE_STATUS_GOOD)
    DBG (1, "gl640ReadControl error\n");
  return status;
}


/* Wrappers to read or write a single byte to the bridge */
static inline SANE_Status
gl640WriteReq (int fd, GL640_Request req, byte data)
{
  return gl640WriteControl (fd, req, &data, 1);
}

static inline SANE_Status
gl640ReadReq (int fd, GL640_Request req, byte * data)
{
  return gl640ReadControl (fd, req, data, 1);
}


/* Write USB bulk data 
   setup is an apparently scanner-specific sequence:
   {(0=read, 1=write), 0x00, 0x00, 0x00, sizelo, sizehi, 0x00, 0x00}
   hp3400: setup[1] = 0x01
   fb630u: setup[2] = 0x80
*/
static SANE_Status
gl640WriteBulk (int fd, byte * setup, byte * data, size_t size)
{
  SANE_Status status;
  setup[0] = 1;
  setup[4] = (size) & 0xFF;
  setup[5] = (size >> 8) & 0xFF;

  CHK (gl640WriteControl (fd, GL640_BULK_SETUP, setup, 8));

  status = sanei_usb_write_bulk (fd, data, &size);
  if (status != SANE_STATUS_GOOD)
    DBG (1, "gl640WriteBulk error\n");

  return status;
}


/* Read USB bulk data 
   setup is an apparently scanner-specific sequence:
   {(0=read, 1=write), 0x00, 0x00, 0x00, sizelo, sizehi, 0x00, 0x00}
   fb630u: setup[2] = 0x80
*/
static SANE_Status
gl640ReadBulk (int fd, byte * setup, byte * data, size_t size)
{
  SANE_Status status;
  setup[0] = 0;
  setup[4] = (size) & 0xFF;
  setup[5] = (size >> 8) & 0xFF;

  CHK (gl640WriteControl (fd, GL640_BULK_SETUP, setup, 8));

  status = sanei_usb_read_bulk (fd, data, &size);
  if (status != SANE_STATUS_GOOD)
    DBG (1, "gl640ReadBulk error\n");

  return status;
}


/*****************************************************
            LM9830 communication primitives
	    parallel-port scanner-on-a-chip.
******************************************************/

/* write 1 byte to a LM9830 register address */
static SANE_Status
write_byte (int fd, byte addr, byte val)
{
  SANE_Status status;
  DBG (14, "write_byte(fd, 0x%02x, 0x%02x);\n", addr, val);
  CHK (gl640WriteReq (fd, GL640_EPP_ADDR, addr));
  CHK (gl640WriteReq (fd, GL640_EPP_DATA_WRITE, val));
  return status;
}


/* read 1 byte from a LM9830 register address */
static SANE_Status
read_byte (int fd, byte addr, byte * val)
{
  SANE_Status status;
  CHK (gl640WriteReq (fd, GL640_EPP_ADDR, addr));
  CHK (gl640ReadReq (fd, GL640_EPP_DATA_READ, val));
  DBG (14, "read_byte(fd, 0x%02x, &result); /* got %02x */\n", addr, *val);
  return status;
}


static byte bulk_setup_data[] = { 0, 0, 0x80, 0, 0, 0, 0, 0 };

/* Bulk write */
static SANE_Status
write_bulk (int fd, unsigned int addr, void *src, size_t count)
{
  SANE_Status status;

  DBG (13, "write_bulk(fd, 0x%02x, buf, 0x%04lx);\n", addr, (u_long) count);

  if (!src)
    {
      DBG (1, "write_bulk: bad src\n");
      return SANE_STATUS_INVAL;
    }

  /* destination address */
  CHK (gl640WriteReq (fd, GL640_EPP_ADDR, addr));
  /* write */
  CHK (gl640WriteBulk (fd, bulk_setup_data, src, count));
  return status;
}


/* Bulk read */
static SANE_Status
read_bulk (int fd, unsigned int addr, void *dst, size_t count)
{
  SANE_Status status;

  DBG (13, "read_bulk(fd, 0x%02x, buf, 0x%04lx);\n", addr, (u_long) count);

  if (!dst)
    {
      DBG (1, "read_bulk: bad dest\n");
      return SANE_STATUS_INVAL;
    }

  /* destination address */
  CHK (gl640WriteReq (fd, GL640_EPP_ADDR, addr));
  /* read */
  CHK (gl640ReadBulk (fd, bulk_setup_data, dst, count));
  return status;
}



/*****************************************************
            useful macro routines
******************************************************/

/* write a 16-bit int to two sequential registers */
static SANE_Status
write_word (int fd, unsigned int addr, unsigned int data)
{
  SANE_Status status;
  /* MSB */
  CHK (write_byte (fd, addr, (data >> 8) & 0xff));
  /* LSB */
  CHK (write_byte (fd, addr + 1, data & 0xff));
  return status;
}


/* write multiple bytes, one at a time (non-bulk) */
static SANE_Status
write_many (int fd, unsigned int addr, void *src, size_t count)
{
  SANE_Status status;
  size_t i;

  DBG (14, "multi write %lu\n", (u_long) count);
  for (i = 0; i < count; i++)
    {
      DBG (15, " %04lx:%02x", (u_long) (addr + i), ((byte *) src)[i]);
      status = write_byte (fd, addr + i, ((byte *) src)[i]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (15, "\n");
	  return status;
	}
    }
  DBG (15, "\n");
  return SANE_STATUS_GOOD;
}


/* read multiple bytes, one at a time (non-bulk) */
static SANE_Status
read_many (int fd, unsigned int addr, void *dst, size_t count)
{
  SANE_Status status;
  size_t i;
  byte val;

  DBG (14, "multi read %lu\n", (u_long) count);
  for (i = 0; i < count; i++)
    {
      status = read_byte (fd, addr + i, &val);
      ((byte *) dst)[i] = val;
      DBG (15, " %04lx:%02x", (u_long) (addr + i), ((byte *) dst)[i]);
      /* on err, return number of success */
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (15, "\n");
	  return status;
	}
    }
  DBG (15, "\n");
  return SANE_STATUS_GOOD;
}


/* Poll addr until result & mask = val */
static int
read_poll_flag (int fd,
		unsigned int addr, unsigned int mask, unsigned int val)
{
  SANE_Status status;
  byte result = 0;
  time_t start_time = time (NULL);

  DBG (12, "read_poll_flag...\n");
  do
    {
      status = read_byte (fd, addr, &result);
      if (status != SANE_STATUS_GOOD)
	return -1;
      /* Give it a minute */
      if ((time (NULL) - start_time) > 60)
	{
	  DBG (1, "read_poll_flag: timed out (%d)\n", result);
	  return -1;
	}
      usleep (100000);
    }
  while ((result & mask) != val);
  return result;
}


/* Keep reading addr until results >= min */
static int
read_poll_min (int fd, unsigned int addr, unsigned int min)
{
  SANE_Status status;
  byte result;
  time_t start_time = time (NULL);

  DBG (12, "waiting...\n");
  do
    {
      status = read_byte (fd, addr, &result);
      if (status != SANE_STATUS_GOOD)
	return -1;
      /* Give it a minute */
      if ((time (NULL) - start_time) > 60)
	{
	  DBG (1, "read_poll_min: timed out (%d < %d)\n", result, min);
	  return -1;
	}
      /* no sleep here, or calibration gets unhappy. */
    }
  while (result < min);
  return result;
}


/* Bulk read "ks" kilobytes + "remainder" bytes of data, to a buffer if the
   buffer is valid. */
static int
read_bulk_size (int fd, int ks, int remainder, byte * dest, int destsize)
{
  byte *buf;
  int bytes = (ks - 1) * 1024 + remainder;
  int dropdata = ((dest == 0) || (destsize < bytes));

  if (bytes < 0)
    {
      DBG (1, "read_bulk_size: invalid size %02x (%d)\n", ks, bytes);
      return -1;
    }
  if (destsize && (destsize < bytes))
    {
      DBG (3, "read_bulk_size: more data than buffer (%d/%d)\n",
	   destsize, bytes);
      bytes = destsize;
    }

  if (bytes == 0)
    return 0;

  if (dropdata)
    {
      buf = malloc (bytes);
      DBG (3, " ignoring data ");
    }
  else
    buf = dest;

  read_bulk (fd, 0x00, buf, bytes);

  if (dropdata)
    free (buf);
  return bytes;
}



/*****************************************************

            fb630u calibration and scan

******************************************************/

/* data structures and constants */

typedef struct CANON_Handle
{
  int fd;			/* scanner fd */
  int x1, x2, y1, y2;		/* in pixels, 600 dpi */
  int width, height;		/* at scan resolution */
  int resolution;		/* dpi */
  char *fname;			/* output file name */
  FILE *fp;			/* output file pointer (for reading) */
  char *buf, *ptr;		/* data buffer */
  unsigned char gain;		/* static analog gain, 0 - 31 */
  double gamma;		        /* gamma correction */
  int flags;
#define FLG_GRAY	0x01	/* grayscale */
#define FLG_FORCE_CAL	0x02	/* force calibration */
#define FLG_BUF		0x04	/* save scan to buffer instead of file */
#define FLG_NO_INTERLEAVE 0x08	/* don't interleave r,g,b pixels; leave them 
				   in row format */
#define FLG_PPM_HEADER	0x10	/* include PPM header in scan file */
}
CANON_Handle;


/* offset/gain calibration file name */
#define CAL_FILE_OGN "/tmp/canon.cal"

/* at 600 dpi */
#define CANON_MAX_WIDTH    5100	/* 8.5in */
/* this may not be right */
#define CANON_MAX_HEIGHT   7000	/* 11.66in */

/* scanline end-of-line data byte, returned after each r,g,b segment,
   specific to the FB630u */
#define SCANLINE_END	0x0c


static const byte seq002[] =
  { /*r08 */ 0x04, /*300 dpi */ 0x1a, 0x00, 0x0d, 0x4c, 0x2f, 0x00, 0x01,
/*r10 */ 0x07, 0x04, 0x05, 0x06, 0x07, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x25, 0x00,
0x4b, /*r20 */ 0x15, 0xe0, /*data px start */ 0x00, 0x4b, /*data px end */ 0x14, 0x37, 0x15, 0x00 };

static const byte seq003[] =
  { 0x02, 0x00, 0x00, /*lights out */ 0x03, 0xff, 0x00, 0x01, 0x03, 0xff,
0x00, 0x01, 0x03, 0xff, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x06,
0x1d, 0x00, 0x13, 0x04, 0x1a, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0x57, 0x02, 0x00, 0x3c, 0x35, 0x94,
0x00, 0x10, 0x08, 0x3f, 0x2b, 0x91, 0x00, 0x00, 0x01, 0x00, 0x80, 0x00 };


/* Scanner init, called at calibration and scan time.  Returns 1 if this
   was the first time the scanner was plugged in, 0 afterward, and
   -1 on error. */
static int
init (int fd)
{
  byte result, rv;

  if (gl640WriteReq (fd, GL640_GPIO_OE, 0x71) != SANE_STATUS_GOOD) { 
      DBG(1, "Initial write request failed.\n");
      return -1;
  }
  /* Gets 0x04 or 0x05 first run, gets 0x64 subsequent runs. */
  if (gl640ReadReq (fd, GL640_GPIO_READ, &rv) != SANE_STATUS_GOOD) {
      DBG(1, "Initial read request failed.\n");
      return -1;
  }
  gl640WriteReq (fd, GL640_GPIO_OE, 0x70);

  DBG (2, "init query: %x\n", rv);
  if (rv != 0x64)
    {
      gl640WriteReq (fd, GL640_GPIO_WRITE, 0x00);
      gl640WriteReq (fd, GL640_GPIO_WRITE, 0x40);
    }

  gl640WriteReq (fd, GL640_SPP_DATA, 0x99);
  gl640WriteReq (fd, GL640_SPP_DATA, 0x66);
  gl640WriteReq (fd, GL640_SPP_DATA, 0xcc);
  gl640WriteReq (fd, GL640_SPP_DATA, 0x33);
  /* parallel port setting */
  write_byte (fd, PARALLEL_PORT, 0x06);
  /* sensor control settings */
  write_byte (fd, 0x0b, 0x0d);
  write_byte (fd, 0x0c, 0x4c);
  write_byte (fd, 0x0d, 0x2f);
  read_byte (fd, 0x0b, &result);	/* wants 0d */
  read_byte (fd, 0x0c, &result);	/* wants 4c */
  read_byte (fd, 0x0d, &result);	/* wants 2f */
  /* parallel port noise filter */
  write_byte (fd, 0x70, 0x73);

  DBG (2, "init post-reset: %x\n", rv);
  /* Returns 1 if this was the first time the scanner was plugged in. */
  return (rv != 0x64);
}


/* Turn off the lamps */
static void
lights_out (int fd)
{
  write_word (fd, LAMP_R_ON, 0x3fff);
  write_word (fd, LAMP_R_OFF, 0x0001);
  write_word (fd, LAMP_G_ON, 0x3fff);
  write_word (fd, LAMP_G_OFF, 0x0001);
  write_word (fd, LAMP_B_ON, 0x3fff);
  write_word (fd, LAMP_B_OFF, 0x0001);
}


/* Do the scan and save the resulting image as r,g,b interleaved PPM
   file.  */
static SANE_Status
do_scan (CANON_Handle * s)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int numbytes, datasize, level = 0, line = 0, pixel = 0;
  byte *buf, *ptr, *redptr;
  FILE *fp;

#define BUFSIZE 0xf000
  buf = malloc (BUFSIZE);
  if (!buf)
    return SANE_STATUS_NO_MEM;

  if (s->flags & FLG_BUF)
    {
      /* read the whole thing into buf */
      if (!s->buf)
	return SANE_STATUS_NO_MEM;
      s->ptr = s->buf;
      fp = NULL;
    }
  else
    {
      fp = fopen (s->fname, "w");
      if (!fp)
	{
	  free (buf);
	  DBG (1, "err:%s when opening %s\n", strerror (errno), s->fname);
	  return SANE_STATUS_IO_ERROR;
	}
    }
  if (fp && (s->flags & FLG_PPM_HEADER))
    /* PPM format header */
    fprintf (fp, "P6\n%d %d\n255\n", s->width, s->height);

  /* lights off */
  write_byte (s->fd, COMMAND, 0x08);
  /* lights on */
  write_byte (s->fd, COMMAND, 0x00);
  /* begin scan */
  write_byte (s->fd, COMMAND, 0x03);

  ptr = redptr = buf;
  while (line < s->height)
    {
      datasize = read_poll_min (s->fd, IMAGE_DATA_AVAIL, 2);
      if (datasize < 0)
	{
	  DBG (1, "no data\n");
	  break;
	}
      DBG (12, "scan line %d %dk\n", line, datasize - 1);
      /* Read may cause scan head to move */
      numbytes = read_bulk_size (s->fd, datasize, 0, ptr, BUFSIZE - level);
      if (numbytes < 0)
	{
	  status = SANE_STATUS_INVAL;
	  break;
	}
      /* Data coming back is "width" bytes Red data followed by 0x0c,
         width bytes Green, 0x0c, width bytes Blue, 0x0c, repeat for
         "height" lines. */
      if (s->flags & FLG_NO_INTERLEAVE)
	{
	  /* number of full lines */
	  line += (numbytes + level) / (s->width * 3);
	  /* remainder (partial line) */
	  level = (numbytes + level) % (s->width * 3);
	  /* but if last line, don't store extra */
	  if (line >= s->height)
	    numbytes -= (line - s->height) * s->width * 3 + level;
	  if (fp)
	    fwrite (buf, 1, numbytes, fp);
	  else
	    {
	      memcpy (s->ptr, buf, numbytes);
	      s->ptr += numbytes;
	    }
	}
      else
	{
	  /* Contorsions to convert data from line-by-line RGB to
	     byte-by-byte RGB, without reading in the whole buffer first.
	     We use the sliding window redptr with the temp buffer buf. */
	  ptr += numbytes;	/* point to the end of data */
	  /* while we have RGB triple data */
	  while (redptr + s->width + s->width <= ptr)
	    {
	      if (*redptr == SCANLINE_END)
		DBG (13, "-%d- ", pixel);
	      if (fp)
		{
		  /* for PPM binary (P6), 3-byte RGB pixel */
		  fwrite (redptr, 1, 1, fp);	/* Red */
		  fwrite (redptr + s->width, 1, 1, fp);	/* Green */
		  fwrite (redptr + s->width + s->width, 1, 1, fp);	/* Blue */
		  /* for PPM ascii (P3)
		     fprintf(fp, "%3d %3d %3d\n",  *redptr, 
		     *(redptr + s->width),
		     *(redptr + s->width + s->width));
		   */
		}
	      else
		{
		  /* R */ *s->ptr = *redptr;
		  s->ptr++;
		  /* G */ *s->ptr = *(redptr + s->width);
		  s->ptr++;
		  /* B */ *s->ptr = *(redptr + s->width + s->width);
		  s->ptr++;
		}
	      redptr++;
	      pixel++;
	      if (pixel && !(pixel % s->width))
		{
		  /* end of a line, move redptr to the next Red section */
		  line++;
		  redptr += s->width + s->width;
#if 0
		  /* progress */
		  printf ("%2d%%\r", line * 100 / s->height);
		  fflush (stdout);
#endif
		  /* don't record any extra */
		  if (line >= s->height)
		    break;
		}
	    }
	  /* keep the extra around for next time */
	  level = ptr - redptr;
	  if (level < 0)
	    level = 0;
	  memmove (buf, redptr, level);
	  ptr = buf + level;
	  redptr = buf;
	}
    }

  if (fp)
    {
      fclose (fp);
      DBG (6, "created scan file %s\n", s->fname);
    }
  free (buf);
  DBG (6, "%d lines, %d pixels, %d extra bytes\n", line, pixel, level);

  /* motor off */
  write_byte (s->fd, COMMAND, 0x00);

  return status;
}


static int
wait_for_return (int fd)
{
  return read_poll_flag (fd, STATUS, STATUS_HOME, STATUS_HOME);
}


static SANE_Status compute_ogn (char *calfilename);


/* This is the calibration rountine Win2k goes through when the scanner is
   first plugged in.
   Original usb trace from Win2k with USBSnoopy ("usb sniffer for w2k"
   http://benoit.papillault.free.fr/speedtouch/sniff-2000.en.php3)
 */
static int
plugin_cal (CANON_Handle * s)
{
  SANE_Status status;
  unsigned int temp;
  byte result;
  byte *buf;
  int fd = s->fd;

  DBG (6, "Calibrating\n");

  /* reserved? */
  read_byte (fd, 0x69, &result);	/* wants 02 */

  /* parallel port setting */
  write_byte (fd, PARALLEL_PORT, 0x06);

  write_many (fd, 0x08, (byte *) seq002, sizeof (seq002));
  /* addr 0x28 isn't written */
  write_many (fd, 0x29, (byte *) seq003, sizeof (seq003));
  /* Verification */
  buf = malloc (0x400);
  read_many (fd, 0x08, buf, sizeof (seq002));
  if (memcmp (seq002, buf, sizeof (seq002)))
    DBG (1, "seq002 verification error\n");
  /* addr 0x28 isn't read */
  read_many (fd, 0x29, buf, sizeof (seq003));
  if (memcmp (seq003, buf, sizeof (seq003)))
    DBG (1, "seq003 verification error\n");

  /* parallel port noise filter */
  write_byte (fd, 0x70, 0x73);

  lights_out (fd);

  /* Home motor */
  read_byte (fd, STATUS, &result);	/* wants 2f or 2d */
  if (!(result & STATUS_HOME) /*0x2d */ )
    write_byte (fd, COMMAND, 0x02);

  wait_for_return (fd);

  /* Motor forward */
  write_byte (fd, COMMAND, 0x01);
  usleep (600000);
  read_byte (fd, STATUS, &result);	/* wants 0c or 2c */
  read_byte (fd, STATUS, &result);	/* wants 0c */
  /* Return home */
  write_byte (fd, COMMAND, 0x02);

  /* Gamma tables */
  /* Linear gamma */
  for (temp = 0; temp < 0x0400; temp++)
    buf[temp] = temp / 4;
  /* Gamma Red */
  write_byte (fd, DATAPORT_TARGET, DP_R | DP_GAMMA);
  write_word (fd, DATAPORT_ADDR, DP_WRITE);
  write_bulk (fd, DATAPORT, buf, 0x0400);
  /* Gamma Green */
  write_byte (fd, DATAPORT_TARGET, DP_G | DP_GAMMA);
  write_word (fd, DATAPORT_ADDR, DP_WRITE);
  write_bulk (fd, DATAPORT, buf, 0x0400);
  /* Gamma Blue */
  write_byte (fd, DATAPORT_TARGET, DP_B | DP_GAMMA);
  write_word (fd, DATAPORT_ADDR, DP_WRITE);
  write_bulk (fd, DATAPORT, buf, 0x0400);

  /* Read back gamma tables.  I suppose I should check results... */
  /* Gamma Red */
  write_byte (fd, DATAPORT_TARGET, DP_R | DP_GAMMA);
  write_word (fd, DATAPORT_ADDR, DP_READ);
  read_bulk (fd, DATAPORT, buf, 0x0400);
  /* Gamma Green */
  write_byte (fd, DATAPORT_TARGET, DP_G | DP_GAMMA);
  write_word (fd, DATAPORT_ADDR, DP_READ);
  read_bulk (fd, DATAPORT, buf, 0x0400);
  /* Gamma Blue */
  write_byte (fd, DATAPORT_TARGET, DP_B | DP_GAMMA);
  write_word (fd, DATAPORT_ADDR, DP_READ);
  read_bulk (fd, DATAPORT, buf, 0x0400);
  free (buf);

  /* Make sure STATUS_HOME */
  read_byte (fd, STATUS, &result);	/* wants 0e */
  /* stepper forward */
  write_byte (fd, COMMAND, 0x01);
  read_byte (fd, STATUS, &result);	/* wants 0c */
  /* not sure if this rigid read/write pattern is required */
  read_byte (fd, CLOCK_DIV, &result);	/* wants 04 */
  write_byte (fd, CLOCK_DIV, 0x04);
  read_byte (fd, STEP_SIZE, &result);	/* wants 04 */
  write_byte (fd, STEP_SIZE, 0x3f);
  read_byte (fd, 0x47, &result);	/* wants 1a */
  write_byte (fd, 0x47, 0xff);
  read_byte (fd, FAST_STEP, &result);	/* wants 01 */
  write_byte (fd, FAST_STEP, 0x01);
  read_byte (fd, 0x49, &result);	/* wants 04 */
  write_byte (fd, 0x49, 0x04);
  read_byte (fd, SKIP_STEPS, &result);	/* wants 00 */
  write_byte (fd, SKIP_STEPS, 0x00);
  read_byte (fd, 0x4b, &result);	/* wants 00 */
  write_byte (fd, 0x4b, 0xc8);
  read_byte (fd, BUFFER_LIMIT, &result);	/* wants 57 */
  write_byte (fd, BUFFER_LIMIT, 0x04);
  read_byte (fd, BUFFER_RESUME, &result);	/* wants 02 */
  write_byte (fd, BUFFER_RESUME, 0x02);
  read_byte (fd, REVERSE_STEPS, &result);	/* wants 00 */
  write_byte (fd, REVERSE_STEPS, 0x00);
  write_byte (fd, STEP_PWM, 0x1f);

  /* Reset motor */
  write_byte (fd, COMMAND, 0x08);
  write_byte (fd, COMMAND, 0x00);
  /* Scan */
  write_byte (fd, COMMAND, 0x03);
  /* Wants 02 or 03, gets a bunch of 0's first */
  read_poll_min (fd, IMAGE_DATA_AVAIL, 2);
  write_byte (fd, COMMAND, 0x00);

  write_byte (fd, STEP_PWM, 0x3f);
  write_byte (fd, CLOCK_DIV, 0x04);
  /* 300 dpi */
  write_word (fd, STEP_SIZE, 0x041a);
  write_word (fd, FAST_STEP, 0x0104);
  /* Don't skip the black/white calibration area at the bottom of the
     scanner. */
  write_word (fd, SKIP_STEPS, 0x0000);
  write_byte (fd, BUFFER_LIMIT, 0x57);
  write_byte (fd, BUFFER_RESUME, 0x02);
  write_byte (fd, REVERSE_STEPS, 0x00);
  write_byte (fd, BUFFER_LIMIT, 0x09);
  write_byte (fd, STEP_PWM, 0x1f);
  read_byte (fd, MICROSTEP, &result);	/* wants 13, active */
  write_byte (fd, MICROSTEP, 0x03 /* tristate */ );

  /* Calibration data taken under 3 different lighting conditions */
  /* dark */
  write_word (fd, LAMP_R_ON, 0x0017);
  write_word (fd, LAMP_R_OFF, 0x0100);
  write_word (fd, LAMP_G_ON, 0x0017);
  write_word (fd, LAMP_G_OFF, 0x0100);
  write_word (fd, LAMP_B_ON, 0x0017);
  write_word (fd, LAMP_B_OFF, 0x0100);
  /* coming in, we've got 300dpi,
     data px start : 0x004b
     data px end  : 0x1437 for a total of 5100(13ec) 600-dpi pixels, 
     (8.5 inches) or 2550 300-dpi pixels (7653 bytes). 
     Interestingly, the scan head never moves, no matter how many rows
     are read. */
  s->width = 2551;
  s->height = 1;
  s->flags = FLG_BUF | FLG_NO_INTERLEAVE;
  s->buf = malloc (s->width * s->height * 3);
  /* FIXME do something with this data */
  CHK (do_scan (s));

  /* Lighting */
  /* medium */
  write_word (fd, LAMP_R_ON, 0x0017);
  write_word (fd, LAMP_R_OFF, 0x0200);
  write_word (fd, LAMP_G_ON, 0x0017);
  write_word (fd, LAMP_G_OFF, 0x01d7 /* also 01db */ );
  write_word (fd, LAMP_B_ON, 0x0017);
  write_word (fd, LAMP_B_OFF, 0x01af /* also 01b2 */ );
  /* FIXME do something with this data */
  CHK (do_scan (s));

  /* Lighting */
  /* bright */
  write_word (fd, LAMP_R_ON, 0x0017);
  write_word (fd, LAMP_R_OFF, 0x0e8e /* also 1040 */ );
  write_word (fd, LAMP_G_ON, 0x0017);
  write_word (fd, LAMP_G_OFF, 0x0753 /* also 0718 */ );
  write_word (fd, LAMP_B_ON, 0x0017);
  write_word (fd, LAMP_B_OFF, 0x03f8 /* also 040d */ );
  /* FIXME do something with this data */
  CHK (do_scan (s));
  free (s->buf);
  s->buf = NULL;

  /* The trace gets a little iffy from here on out since the log files
     are missing different urb's.  This is kind of a puzzled-out
     compilation. */

  write_byte (fd, MICROSTEP, 0x13 /* pins active */ );
  write_byte (fd, STEP_PWM, 0x3f);
  read_byte (fd, STATUS, &result);	/* wants 0c */

  /* Stepper home */
  write_byte (fd, COMMAND, 0x02);
  /* Step size */
  write_word (fd, STEP_SIZE, 0x041a /* 300 dpi */ );
  /* Skip steps */
  write_word (fd, SKIP_STEPS, 0x0000);
  /* Pause buffer levels */
  write_byte (fd, BUFFER_LIMIT, 0x57);
  /* Resume buffer levels */
  write_byte (fd, BUFFER_RESUME, 0x02);

  wait_for_return (fd);
  /* stepper forward small */
  write_byte (fd, COMMAND, 0x01);
  read_byte (fd, STATUS, &result);	/* wants 0c */
  usleep (200000);
  write_byte (fd, STEP_PWM, 0x1f);

  /* Read in cal strip at bottom of scanner (to adjust gain/offset
     tables.  Note that this isn't the brightest lighting condition.)
     At 300 dpi: black rows 0-25; white rows 30-75; beginning
     of glass 90.
     This produces 574k of data, so save it to a temp file. */
  if (!s->fname)
    {
      DBG (1, "No temp filename!\n");
      s->fname = strdup ("/tmp/cal.XXXXXX");
      mktemp (s->fname);
    }
  s->width = 2551;
  s->height = 75;
  s->flags = FLG_PPM_HEADER | FLG_NO_INTERLEAVE;
  CHK (do_scan (s));
  compute_ogn (s->fname);
  unlink (s->fname);

  write_byte (fd, STEP_PWM, 0x3f);
  /* stepper home */
  write_byte (fd, COMMAND, 0x02);

  /* discard the remaining data */
  read_byte (fd, IMAGE_DATA_AVAIL, &result);	/* wants 42,4c */
  if (result > 1)
    {
      read_bulk_size (fd, result, 0, 0, 0);
      DBG (11, "read %dk extra\n", result);
    }
  read_byte (fd, 0x69, &result);	/* wants 02 */
  write_byte (fd, 0x69, 0x0a);

  lights_out (fd);
  init (fd);

#if 0
  /* Repeatedly send this every 1 second.  Button scan?  FIXME */
  gl640ReadReq (fd, GL640_GPIO_READ, &result);	/* wants 00 */
#endif

  return 0;
}


/* The number of regions in the calibration strip (black & white). */
#define NREGIONS 2

/* Compute the offset/gain table from the calibration strip.  This is
   somewhat more complicated than necessary because I don't hard-code the
   strip widths; I try to figure out the regions based on the scan data.
   Theoretically, the region-finder should work for any number of distinct
   regions (but there are only 2 on this scanner.) 
   This produces the CAL_FILE_OGN file, the final offset/gain table. */
static SANE_Status
compute_ogn (char *calfilename)
{
  byte *linebuf, *oldline, *newline;
  mode_t oldmask;
  FILE *fp;
  int width, height, nlines = 0, region = -1, i, transition = 1, badcnt;
  int pct;
  int reglines[NREGIONS];
  float *avg;
  float max_range[3], tmp1, tmp2;

  fp = fopen (calfilename, "r");
  if (!fp)
    {
      DBG (1, "open %s\n", calfilename);
      return SANE_STATUS_EOF;
    }
  fscanf (fp, "P6 %d %d %*d ", &width, &height);
  DBG (12, "cal file %s %dx%d\n", calfilename, width, height);
  width = width * 3;		/* 1 byte each of r, g, b */
  /* make a buffer holding 2 lines of data */
  linebuf = calloc (width * 2, sizeof (linebuf[0]));
  /* first line is data read buffer */
  newline = linebuf;
  /* second line is a temporary holding spot in case the next line read
     is the black/white transition, in which case we'll disregard this
     one. */
  oldline = linebuf + width;
  /* column averages per region */
  avg = calloc (width * NREGIONS, sizeof (avg[0]));

  while (nlines < height)
    {
      if (fread (newline, 1, width, fp) != (size_t) width)
	break;
      nlines++;
      /* Check if new line is majorly different than old.
         Criteria is 10 pixels differing by more than 10%. */
      badcnt = 0;
      for (i = 0; i < width; i++)
	{
	  pct = newline[i] - oldline[i];
	  /* Fix by M.Reinelt <reinelt@eunet.at>
	   * do NOT use 10% (think of a dark area with
	   * oldline=4 and newline=5, which is a change of 20% !!
	   * Use an absolute difference of 10 as criteria
	   */
	  if (pct < -10 || pct > 10)
	    {
	      badcnt++;
	      DBG (16, "pix%d[%d/%d] ", i, newline[i], oldline[i]);
	    }
	}
      DBG (13, "line %d changed %d\n", nlines, badcnt);
      if ((badcnt > 10) || (nlines == height))
	{
	  /* End of region.  Lines are different or end of data. */
	  transition++;
	  if (transition == 1)
	    DBG (12, "Region %d lines %d-%d\n",
		 region, nlines - reglines[region], nlines - 1);
	}
      else
	{
	  /* Lines are similar, so still in region.  */
	  if (transition)
	    {
	      /* There was just a transition, so this is the start of a
	         new region. */
	      region++;
	      if (region >= NREGIONS)
		/* Too many regions detected.  Err below. */
		break;
	      transition = 0;
	      reglines[region] = 0;
	    }
	  /* Add oldline to the current region's average */
	  for (i = 0; i < width; i++)
	    avg[i + region * width] += oldline[i];
	  reglines[region]++;
	}
      /* And newline becomes old */
      memcpy (oldline, newline, width);
    }
  fclose (fp);
  free (linebuf);
  region++;			/* now call it number of regions instead of index */
  DBG (11, "read %d lines as %d regions\n", nlines, region);

  /* Check to see if we screwed up */
  if (region != NREGIONS)
    {
      DBG (1, "Warning: gain/offset compute failed.\n"
	   "Found %d regions instead of %d.\n", region, NREGIONS);
      for (i = 0; i < region; i++)
	DBG (1, "   Region %d: %d lines\n",
	     i, (i >= NREGIONS) ? -1 : reglines[i]);
      free (avg);
      return SANE_STATUS_UNSUPPORTED;
    }

  /* Now we've got regions and sums.  Find averages and range. */
  max_range[0] = max_range[1] = max_range[2] = 0.0;
  for (i = 0; i < width; i++)
    {
      /* Convert sums to averages */
      /* black region */
      tmp1 = avg[i] /= reglines[0];
      /* white region */
      tmp2 = avg[i + width] /= reglines[1];
      /* Track largest range for each color.
         If image is interleaved, use 'i%3', if not, 'i/(width/3)' */
      if ((tmp2 - tmp1) > max_range[i / (width / 3)])
	{
	  max_range[i / (width / 3)] = tmp2 - tmp1;
	  DBG (14, "max %d@%d %f-%f=%f\n",
	       i / (width / 3), i, tmp2, tmp1, tmp2 - tmp1);
	}
    }
  DBG (13, "max range r %f\n", max_range[0]);
  DBG (13, "max range g %f\n", max_range[1]);
  DBG (13, "max range b %f\n", max_range[2]);

  /* Set umask to world r/w so other users can overwrite common cal... */
  oldmask = umask (0);
  fp = fopen (CAL_FILE_OGN, "w");
  /* ... and set it back. */
  umask (oldmask);
  if (!fp)
    {
      DBG (1, "open " CAL_FILE_OGN);
      free (avg);
      return SANE_STATUS_IO_ERROR;
    }

  /* Finally, compute offset and gain */
  for (i = 0; i < width; i++)
    {
      int gain, offset;
      byte ogn[2];

      /* skip line termination flags */
      if (!((i + 1) % (width / 3)))
	{
	  DBG (13, "skip scanline EOL %d/%d\n", i, width);
	  continue;
	}

      /* Gain multiplier: 
         255 : 1.5 times brighter
         511 : 2 times brighter
         1023: 3 times brighter */
#if 1
      /* Original gain/offset */
      gain = 512 * ((max_range[i / (width / 3)] /
		     (avg[i + width] - avg[i])) - 1);
      offset = avg[i];
#else
      /* This doesn't work for some people.  For instance, a negative
         offset would be bad.  */

      /* Enhanced offset and gain calculation by M.Reinelt <reinelt@eunet.at>
       * These expressions were found by an iterative calibration process, 
       * by changing gain and offset values for every pixel until the desired
       * values for black and white were reached, and finding an approximation 
       * formula.
       * Note that offset is linear, but gain isn't!
       */
      offset = (double)3.53 * avg[i] - 125;
      gain = (double)3861.0 * exp(-0.0168 * (avg[i + width] - avg[i]));
#endif

      DBG (14, "%d wht=%f blk=%f diff=%f gain=%d offset=%d\n", i,
	   avg[i + width], avg[i], avg[i + width] - avg[i], gain, offset);
      /* 10-bit gain, 6-bit offset (subtractor) in two bytes */
      ogn[0] = (byte) (((offset << 2) + (gain >> 8)) & 0xFF);
      ogn[1] = (byte) (gain & 0xFF);
      fwrite (ogn, sizeof (byte), 2, fp);
      /* Annoyingly, we seem to use ogn data at 600dpi, while we scanned
         at 300, so double our file.  Much easier than doubling at the
         read. */
      fwrite (ogn, sizeof (byte), 2, fp);
    }

  fclose (fp);
  free (avg);
  return SANE_STATUS_GOOD;
}

static int
check_ogn_file (void)
{
  FILE *fp;
  fp = fopen (CAL_FILE_OGN, "r");
  if (fp)
    {
      fclose (fp);
      return 1;
    }
  return 0;
}

/* Load or fake the offset/gain table */
static void
install_ogn (int fd)
{
  int temp;
  byte *buf;
  FILE *fp;

  /* 8.5in at 600dpi = 5104 pixels in scan head
     10-bit gain + 6-bit offset = 2 bytes per pixel, so 10208 bytes */
  buf = malloc (10208);

  fp = fopen (CAL_FILE_OGN, "r");
  if (fp)
    {
      fread (buf, 2, 5100, fp);
      /* screw the last 4 pixels */
    }
  else
    {
      /* Make up the gain/offset data. */
#define GAIN 256		/* 1.5x */
#define OFFSET 0
      for (temp = 0; temp < 10208; temp += 2)
	{
	  buf[temp] = (byte) ((OFFSET << 2) + (GAIN >> 8));
	  buf[temp + 1] = (byte) (GAIN & 0xFF);
	}
    }
  /* Gain/offset table (r,g,b) */
  write_byte (fd, DATAPORT_TARGET, DP_R | DP_OFFSET);
  write_word (fd, DATAPORT_ADDR, DP_WRITE);
  write_bulk (fd, DATAPORT, buf, 10208);
  if (fp)
    fread (buf, 2, 5100, fp);
  write_byte (fd, DATAPORT_TARGET, DP_G | DP_OFFSET);
  write_word (fd, DATAPORT_ADDR, DP_WRITE);
  write_bulk (fd, DATAPORT, buf, 10208);
  if (fp)
    {
      fread (buf, 2, 5100, fp);
      fclose (fp);
    }
  write_byte (fd, DATAPORT_TARGET, DP_B | DP_OFFSET);
  write_word (fd, DATAPORT_ADDR, DP_WRITE);
  write_bulk (fd, DATAPORT, buf, 10208);

  free (buf);
  return;
}


/* Scan sequence */
/* resolution is 75,150,300,600,1200
   scan coordinates in 600-dpi pixels */
static SANE_Status
scan (CANON_Handle * opt)
{
  SANE_Status status;
  const int left_edge = 0x004b;	/* Just for my scanner, or is this
				   universal?  Calibrate? */
  int temp;
  int fd = opt->fd;
  byte result;
  byte *buf;

  /* Check status. (not in w2k driver) */
  read_byte (fd, STATUS, &result);	/* wants 2f or 2d */
  if (!(result & STATUS_HOME) /*0x2d */ )
    return SANE_STATUS_DEVICE_BUSY;
  /* or force it to return? 
     write_byte(fd, COMMAND, 0x02); 
     wait_for_return(fd);
   */

  /* reserved? */
  read_byte (fd, 0x69, &result);	/* wants 0a */

  read_byte (fd, STATUS, &result);	/* wants 0e */
  read_byte (fd, PAPER_SENSOR, &result);	/* wants 2b */
  write_byte (fd, PAPER_SENSOR, 0x2b);
  /* Color mode:
     1-Channel Line Rate Color 0x15.
     1-Channel Grayscale 0x14 (and we skip some of these tables) */
  write_byte (fd, COLOR_MODE, 0x15);

  /* install the offset/gain table */
  install_ogn (fd);

  read_byte (fd, STATUS, &result);	/* wants 0e */
  /* move forward to "glass 0" */
  write_byte (fd, COMMAND, 0x01);
  read_byte (fd, STATUS, &result);	/* wants 0c */

  /* create gamma table */
  buf = malloc (0x400);
  for (temp = 0; temp < 0x0400; temp++)
    /* gamma calculation by M.Reinelt <reinelt@eunet.at> */
    buf[temp] = (double) 255.0 * exp(log((temp + 0.5) / 1023.0) / opt->gamma)
	+ 0.5;

  /* Gamma R, write and verify */
  write_byte (fd, DATAPORT_TARGET, DP_R | DP_GAMMA);
  write_word (fd, DATAPORT_ADDR, DP_WRITE);
  write_bulk (fd, DATAPORT, buf, 0x0400);
  write_byte (fd, DATAPORT_TARGET, DP_R | DP_GAMMA);
  write_word (fd, DATAPORT_ADDR, DP_READ);
  read_bulk (fd, DATAPORT, buf, 0x0400);
  /* Gamma G */
  write_byte (fd, DATAPORT_TARGET, DP_G | DP_GAMMA);
  write_word (fd, DATAPORT_ADDR, DP_WRITE);
  write_bulk (fd, DATAPORT, buf, 0x0400);
  write_byte (fd, DATAPORT_TARGET, DP_G | DP_GAMMA);
  write_word (fd, DATAPORT_ADDR, DP_READ);
  read_bulk (fd, DATAPORT, buf, 0x0400);
  /* Gamma B */
  write_byte (fd, DATAPORT_TARGET, DP_B | DP_GAMMA);
  write_word (fd, DATAPORT_ADDR, DP_WRITE);
  write_bulk (fd, DATAPORT, buf, 0x0400);
  write_byte (fd, DATAPORT_TARGET, DP_B | DP_GAMMA);
  write_word (fd, DATAPORT_ADDR, DP_READ);
  read_bulk (fd, DATAPORT, buf, 0x0400);
  free (buf);

  write_byte (fd, CLOCK_DIV, 0x04);
  /* Resolution: dpi 75(ie) 100,150(1c) 200,300(1a) 600,1200(18) */
  switch (opt->resolution)
    {
    case 150:
      write_byte (fd, 0x09, 0x1c);
      break;
    case 300:
      write_byte (fd, 0x09, 0x1a);
      break;
    case 600:
    case 1200:
      /* actually 600 dpi horiz max */
      write_byte (fd, 0x09, 0x18);
      break;
    default:			/* 75 */
      write_byte (fd, 0x09, 0x1e);
      opt->resolution = 75;
    }

  write_word (fd, ACTIVE_PX_START, left_edge);
  /* Data pixel start.  Measured at 600dpi regardless of
     scan resolution.  0-position is 0x004b */
  write_word (fd, DATA_PX_START, left_edge + opt->x1);
  /* Data pixel end.  Measured at 600dpi regardless of scan
     resolution. */
  write_word (fd, DATA_PX_END, left_edge + opt->x2);
  /* greyscale has 14,03, different lights */
  write_byte (fd, COLOR_MODE, 0x15);
  write_byte (fd, 0x29, 0x02);
  /* Lights */
  write_word (fd, LAMP_R_ON, 0x0017);
  /* "Hi-low color" selection from windows driver.  low(1437) hi(1481) */
  write_word (fd, LAMP_R_OFF, 0x1437);
  write_word (fd, LAMP_G_ON, 0x0017);
  write_word (fd, LAMP_G_OFF, 0x094e);
  write_word (fd, LAMP_B_ON, 0x0017);
  write_word (fd, LAMP_B_OFF, 0x0543);

  /* Analog static offset R,G,B.  Greyscale has 0,0,0 */
  write_byte (fd, 0x38, 0x3f);
  write_byte (fd, 0x39, 0x3f);
  write_byte (fd, 0x3a, 0x3f);
  /* Analog static gain R,G,B (normally 0x01) */
  write_byte (fd, 0x3b, opt->gain);
  write_byte (fd, 0x3c, opt->gain);
  write_byte (fd, 0x3d, opt->gain);
  /* Digital gain/offset settings.  Greyscale has 0 */
  write_byte (fd, 0x3e, 0x1a);

  {
    /* Stepper motion setup. */
    int stepsize, faststep = 0x0104, reverse = 0x28, phase, pwm = 0x1f;
    switch (opt->resolution)
      {
      case 75:
	stepsize = 0x0106;
	faststep = 0x0106;
	reverse = 0;
	phase = 0x39a8;
	pwm = 0x3f;
	break;
      case 150:
	stepsize = 0x020d;
	phase = 0x3198;
	break;
      case 300:
	stepsize = 0x041a;
	phase = 0x2184;
	break;
      case 600:
	stepsize = 0x0835;
	phase = 0x0074;
	break;
      case 1200:
	/* 1200 dpi y only, x is 600 dpi */
	stepsize = 0x106b;
	phase = 0x41ac;
	break;
      default:
	DBG (1, "BAD RESOLUTION");
	return SANE_STATUS_UNSUPPORTED;
      }

    write_word (fd, STEP_SIZE, stepsize);
    write_word (fd, FAST_STEP, faststep);
    /* There sounds like a weird step disjoint at the end of skipsteps
       at 75dpi, so I think that's why skipsteps=0 at 75dpi in the
       Windows driver.  It still works at the normal 0x017a though. */
    /* cal strip is 0x17a steps, plus 2 300dpi microsteps per pixel */
    write_word (fd, SKIP_STEPS, 0x017a /* cal strip */  + opt->y1 * 2);
    /* FIXME could be 0x57, why not? */
    write_byte (fd, BUFFER_LIMIT, 0x20);
    write_byte (fd, BUFFER_RESUME, 0x02);
    write_byte (fd, REVERSE_STEPS, reverse);
    /* motor resume phasing */
    write_word (fd, 0x52, phase);
    write_byte (fd, STEP_PWM, pwm);
  }

  read_byte (fd, PAPER_SENSOR, &result);	/* wants 2b */
  write_byte (fd, PAPER_SENSOR, 0x0b);

  opt->width = (opt->x2 - opt->x1) * opt->resolution / 600 + 1;
  opt->height = (opt->y2 - opt->y1) * opt->resolution / 600;
  opt->flags = 0;
  DBG (1, "width=%d height=%d dpi=%d\n", opt->width, opt->height,
       opt->resolution);
  CHK (do_scan (opt));

  read_byte (fd, PAPER_SENSOR, &result);	/* wants 0b */
  write_byte (fd, PAPER_SENSOR, 0x2b);
  write_byte (fd, STEP_PWM, 0x3f);

  lights_out (fd);
  /* home */
  write_byte (fd, COMMAND, 0x02);

  return status;
}


static SANE_Status
CANON_set_scan_parameters (CANON_Handle * scan,
			   const int forceCal,
			   const int gray,
			   const int left,
			   const int top,
			   const int right,
			   const int bottom, 
			   const int res, 
			   const int gain, 
			   const double gamma)
{
  DBG (2, "CANON_set_scan_parameters:\n");
  DBG (2, "cal   = %d\n", forceCal);
  DBG (2, "gray  = %d (ignored)\n", gray);
  DBG (2, "res   = %d\n", res);
  DBG (2, "gain  = %d\n", gain);
  DBG (2, "gamma = %f\n", gamma);
  DBG (2, "in 600dpi pixels:\n");
  DBG (2, "left  = %d, top    = %d\n", left, top);
  DBG (2, "right = %d, bottom = %d\n", right, bottom);

  /* Validate the input parameters */
  if ((left < 0) || (right > CANON_MAX_WIDTH))
    return SANE_STATUS_INVAL;

  if ((top < 0) || (bottom > CANON_MAX_HEIGHT))
    return SANE_STATUS_INVAL;

  if (((right - left) < 10) || ((bottom - top) < 10))
    return SANE_STATUS_INVAL;

  if ((res != 75) && (res != 150) && (res != 300)
      && (res != 600) && (res != 1200))
    return SANE_STATUS_INVAL;

  if ((gain < 0) || (gain > 64))
    return SANE_STATUS_INVAL;

  if (gamma <= 0.0)
    return SANE_STATUS_INVAL;

  /* Store params */
  scan->resolution = res;
  scan->x1 = left;
  scan->x2 = right - /* subtract 1 pixel */ 600 / scan->resolution;
  scan->y1 = top;
  scan->y2 = bottom;
  scan->gain = gain;
  scan->gamma = gamma;
  scan->flags = forceCal ? FLG_FORCE_CAL : 0;

  return SANE_STATUS_GOOD;
}


static SANE_Status
CANON_close_device (CANON_Handle * scan)
{
  DBG (3, "CANON_close_device:\n");
  sanei_usb_close (scan->fd);
  return SANE_STATUS_GOOD;
}


static SANE_Status
CANON_open_device (CANON_Handle * scan, const char *dev)
{
  SANE_Word vendor;
  SANE_Word product;
  SANE_Status res;

  DBG (3, "CANON_open_device: `%s'\n", dev);

  scan->fname = NULL;
  scan->fp = NULL;
  scan->flags = 0;

  res = sanei_usb_open (dev, &scan->fd);
  if (res != SANE_STATUS_GOOD)
    {
      DBG (1, "CANON_open_device: couldn't open device `%s': %s\n", dev,
	   sane_strstatus (res));
      return res;
    }

#ifndef NO_AUTODETECT
  /* We have opened the device. Check that it is a USB scanner. */
  if (sanei_usb_get_vendor_product (scan->fd, &vendor, &product) !=
      SANE_STATUS_GOOD)
    {
      DBG (1, "CANON_open_device: sanei_usb_get_vendor_product failed\n");
      /* This is not a USB scanner, or SANE or the OS doesn't support it. */
      sanei_usb_close (scan->fd);
      scan->fd = -1;
      return SANE_STATUS_UNSUPPORTED;
    }

  /* Make sure we have a CANON scanner */
  if ((vendor != 0x04a9) || (product != 0x2204))
    {
      DBG (1, "CANON_open_device: incorrect vendor/product (0x%x/0x%x)\n",
	   vendor, product);
      sanei_usb_close (scan->fd);
      scan->fd = -1;
      return SANE_STATUS_UNSUPPORTED;
    }
#endif

  return SANE_STATUS_GOOD;
}


static const char *
CANON_get_device_name (CANON_Handle * scanner)
{
  scanner = scanner;		/* Eliminate warning about unused parameters */
  return "Canoscan FB630U";
}


static SANE_Status
CANON_finish_scan (CANON_Handle * scanner)
{
  DBG (3, "CANON_finish_scan:\n");
  if (scanner->fp)
    fclose (scanner->fp);
  scanner->fp = NULL;

  /* remove temp file */
  if (scanner->fname)
    {
      DBG (4, "removing temp file %s\n", scanner->fname);
      unlink (scanner->fname);
      free (scanner->fname);
    }
  scanner->fname = NULL;

  return SANE_STATUS_GOOD;
}


static SANE_Status
CANON_start_scan (CANON_Handle * scanner)
{
  int rv;
  SANE_Status status;
  DBG (3, "CANON_start_scan called\n");

  /* choose a temp file name for scan data */
  scanner->fname = strdup ("/tmp/scan.XXXXXX");
  if (!mktemp (scanner->fname))
    return SANE_STATUS_IO_ERROR;

  /* calibrate if needed */
  rv = init (scanner->fd);
  if (rv < 0) {
      DBG(1, "Can't talk on USB.\n");
      return SANE_STATUS_IO_ERROR;
  }
  if ((rv == 1)
      || !check_ogn_file () 
      || (scanner->flags & FLG_FORCE_CAL)) {
      plugin_cal (scanner);
      wait_for_return (scanner->fd);
  }
  
  /* scan */
  if ((status = scan (scanner)) != SANE_STATUS_GOOD)
    {
      CANON_finish_scan (scanner);
      return status;
    }

  /* read the temp file back out */
  scanner->fp = fopen (scanner->fname, "r");
  DBG (4, "reading %s\n", scanner->fname);
  if (!scanner->fp)
    {
      DBG (1, "open %s", scanner->fname);
      return SANE_STATUS_IO_ERROR;
    }
  return SANE_STATUS_GOOD;
}


static SANE_Status
CANON_read (CANON_Handle * scanner, SANE_Byte * data,
	    SANE_Int max_length, SANE_Int * length)
{
  SANE_Status status;
  int red_len;

  DBG (5, "CANON_read called\n");
  if (!scanner->fp)
    return SANE_STATUS_INVAL;
  red_len = fread (data, 1, max_length, scanner->fp);
  /* return some data */
  if (red_len > 0)
    {
      *length = red_len;
      DBG (5, "CANON_read returned (%d/%d)\n", *length, max_length);
      return SANE_STATUS_GOOD;
    }

  /* EOF or file err */
  *length = 0;
  if (feof (scanner->fp))
    {
      DBG (4, "EOF\n");
      status = SANE_STATUS_EOF;
    }
  else
    {
      DBG (4, "IO ERR\n");
      status = SANE_STATUS_IO_ERROR;
    }

  CANON_finish_scan (scanner);
  DBG (5, "CANON_read returned (%d/%d)\n", *length, max_length);
  return status;
}
