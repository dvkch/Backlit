/* lexmark-low.c: scanner-interface file for low Lexmark scanners.

   (C) 2005 Fred Odendaal
   (C) 2006-2013 Stéphane Voltz	<stef.dev@free.fr>
   (C) 2010 "Torsten Houwaart" <ToHo@gmx.de> X74 support
   
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

   **************************************************************************/

#undef BACKEND_NAME
#define BACKEND_NAME lexmark_low

#include "lexmark.h"

#include "lexmark_sensors.c"
#include "lexmark_models.c"

/* numbre of ranges for offset */
#define OFFSET_RANGES 5

typedef enum
{
  black = 0,
  white
}
region_type;

#define HomeTolerance 32


#define LOBYTE(x)  ((uint8_t)((x) & 0xFF))
#define HIBYTE(x)  ((uint8_t)((x) >> 8))

/* Static low function proto-types */
static SANE_Status low_usb_bulk_write (SANE_Int devnum,
				       SANE_Byte * cmd, size_t * size);
static SANE_Status low_usb_bulk_read (SANE_Int devnum,
				      SANE_Byte * buf, size_t * size);
static SANE_Status low_write_all_regs (SANE_Int devnum, SANE_Byte * regs);
static SANE_Bool low_is_home_line (Lexmark_Device * dev,
				   unsigned char *buffer);
static SANE_Status low_get_start_loc (SANE_Int resolution,
				      SANE_Int * vert_start,
				      SANE_Int * hor_start, SANE_Int offset,
				      Lexmark_Device * dev);
static void low_rewind (Lexmark_Device * dev, SANE_Byte * regs);
static SANE_Status low_start_mvmt (SANE_Int devnum);
static SANE_Status low_stop_mvmt (SANE_Int devnum);
static SANE_Status low_clr_c6 (SANE_Int devnum);
static SANE_Status low_simple_scan (Lexmark_Device * dev,
				    SANE_Byte * regs,
				    int xoffset,
				    int pixels,
				    int yoffset,
				    int lines, SANE_Byte ** data);
static void low_set_scan_area (SANE_Int res,
			       SANE_Int tlx,
			       SANE_Int tly,
			       SANE_Int brx,
			       SANE_Int bry,
			       SANE_Int offset,
			       SANE_Bool half_step,
			       SANE_Byte * regs, Lexmark_Device * dev);

/* Static Read Buffer Proto-types */
static SANE_Status read_buffer_init (Lexmark_Device * dev, int bytesperline);
static SANE_Status read_buffer_free (Read_Buffer * rb);
static size_t read_buffer_bytes_available (Read_Buffer * rb);
static SANE_Status read_buffer_add_byte (Read_Buffer * rb,
					 SANE_Byte * byte_pointer);
static SANE_Status read_buffer_add_byte_gray (Read_Buffer * rb,
					      SANE_Byte * byte_pointer);
static SANE_Status read_buffer_add_bit_lineart (Read_Buffer * rb,
						SANE_Byte * byte_pointer,
						SANE_Byte threshold);
static size_t read_buffer_get_bytes (Read_Buffer * rb, SANE_Byte * buffer,
				     size_t rqst_size);
static SANE_Bool read_buffer_is_empty (Read_Buffer * rb);


/*
 *         RTS88XX	START
 *
 *         these rts88xx functions will be spin off in a separate lib
 *         so that they can be reused.
 */

/*
 * registers helpers to avoid direct access
 */
static SANE_Bool
rts88xx_is_color (SANE_Byte * regs)
{
  if ((regs[0x2f] & 0x11) == 0x11)
    return SANE_TRUE;
  return SANE_FALSE;
}

static void
rts88xx_set_gray_scan (SANE_Byte * regs)
{
  regs[0x2f] = (regs[0x2f] & 0x0f) | 0x20;
}

#if 0
static void
rts88xx_set_color_scan (SANE_Byte * regs)
{
  regs[0x2f] = (regs[0x2f] & 0x0f) | 0x10;
}
#endif

static void
rts88xx_set_offset (SANE_Byte * regs, SANE_Byte red, SANE_Byte green,
		    SANE_Byte blue)
{
  /* offset for odd pixels */
  regs[0x02] = red;
  regs[0x03] = green;
  regs[0x04] = blue;

  /* offset for even pixels */
  regs[0x05] = red;
  regs[0x06] = green;
  regs[0x07] = blue;
}

static void
rts88xx_set_gain (SANE_Byte * regs, SANE_Byte red, SANE_Byte green,
		  SANE_Byte blue)
{
  regs[0x08] = red;
  regs[0x09] = green;
  regs[0x0a] = blue;
}

/* set # of head moves per CIS read */
static int
rts88xx_set_scan_frequency (SANE_Byte * regs, int frequency)
{
  regs[0x64] = (regs[0x64] & 0xf0) | (frequency & 0x0f);
  return 0;
}

/*
 * read one register at given index 
 */
static SANE_Status
rts88xx_read_reg (SANE_Int devnum, SANE_Int index, SANE_Byte * reg)
{
  SANE_Status status = SANE_STATUS_GOOD;
  unsigned char cmd[] = { 0x80, 0x00, 0x00, 0x01 };
  size_t size;

  cmd[1] = index;

  size = 4;
#ifdef FAKE_USB
  status = SANE_STATUS_GOOD;
#else
  status = sanei_usb_write_bulk (devnum, cmd, &size);
#endif
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5, "rts88xx_read_reg: bulk write failed\n");
      return status;
    }
  size = 1;
#ifdef FAKE_USB
  status = SANE_STATUS_GOOD;
#else
  status = sanei_usb_read_bulk (devnum, reg, &size);
#endif
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5, "rts88xx_read_reg: bulk read failed\n");
      return status;
    }
  DBG (15, "rts88xx_read_reg: reg[0x%02x]=0x%02x\n", index, *reg);
  return status;
}

/*
 * write one register at given index 
 */
static SANE_Status
rts88xx_write_reg (SANE_Int devnum, SANE_Int index, SANE_Byte * reg)
{
  SANE_Status status = SANE_STATUS_GOOD;
  unsigned char cmd[] = { 0x88, 0x00, 0x00, 0x01 };
  size_t size;

  cmd[1] = index;

  size = 4;
#ifdef FAKE_USB
  status = SANE_STATUS_GOOD;
#else
  status = sanei_usb_write_bulk (devnum, cmd, &size);
#endif
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5, "rts88xx_write_reg: bulk write failed\n");
      return status;
    }
  size = 1;
#ifdef FAKE_USB
  status = SANE_STATUS_GOOD;
#else
  status = sanei_usb_write_bulk (devnum, reg, &size);
#endif
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5, "rts88xx_write_reg: bulk write failed\n");
      return status;
    }
  DBG (15, "rts88xx_write_reg: reg[0x%02x]=0x%02x\n", index, *reg);
  return status;
}

/*
 * write length consecutive registers, starting at index
 * register 0xb3 is never wrote in bulk register write, so we split
 * write if it belongs to the register set sent
 */
static SANE_Status
rts88xx_write_regs (SANE_Int devnum, SANE_Int start, SANE_Byte * source,
		    SANE_Int length)
{
  size_t size = 0;

  /* when writing several registers at a time, we avoid writing 0xb3
     register */
  if ((start + length > 0xb3) && (length > 1))
    {
      size = 0xb3 - start;
      if (low_usb_bulk_write (devnum, source, &size) != SANE_STATUS_GOOD)
	{
	  DBG (5, "rts88xx_write_regs : write registers part 1 failed ...\n");
	  return SANE_STATUS_IO_ERROR;
	}

      /* skip 0xB3 register */
      size++;
      start = 0xb4;
      source = source + size;
    }
  size = length - size;
  if (low_usb_bulk_write (devnum, source, &size) != SANE_STATUS_GOOD)
    {
      DBG (5, "rts88xx_write_regs : write registers part 2 failed ...\n");
      return SANE_STATUS_IO_ERROR;
    }

  return SANE_STATUS_GOOD;

}

/*
 * reads 'needed' bytes of scanned data into 'data'. Actual number of bytes get
 * is retruned in 'size'
 */
static SANE_Status
rts88xx_read_data (SANE_Int devnum, size_t needed, SANE_Byte * data,
		   size_t * size)
{
  SANE_Byte read_cmd[] = { 0x91, 0x00, 0x00, 0x00 };
  size_t cmd_size;
  SANE_Status status = SANE_STATUS_GOOD;

  /* this block would deserve to be a function */
  if (needed > MAX_XFER_SIZE)
    *size = MAX_XFER_SIZE;
  else
    *size = needed;
  read_cmd[3] = (*size) & 0xff;
  read_cmd[2] = (*size >> 8) & 0xff;
  read_cmd[1] = (*size >> 16) & 0xff;

  /* send header for 'get scanned data' */
  cmd_size = 4;
  status = low_usb_bulk_write (devnum, read_cmd, &cmd_size);
  if (status != SANE_STATUS_GOOD)
    {
      *size = 0;
      DBG (5, "rts88xx_read_data : header sending failed ...\n");
      return status;
    }
  /* get actual scanned data */
  status = low_usb_bulk_read (devnum, data, size);
  if (status != SANE_STATUS_GOOD)
    {
      *size = 0;
      DBG (5, "rts88xx_read_data : data reading failed ...\n");
    }
  return status;
}

/* starts scan by sending color depth, stopping head, the starting it */
static SANE_Status
rts88xx_commit (SANE_Int devnum, SANE_Byte depth)
{
  SANE_Status status;
  SANE_Byte reg;

  DBG (2, "rts88xx_commit: start\n");

  /* send color depth depth ??
   * X1100 -> 0x0f
   * X1100/B2 -> 0x0d
   * X1200 -> 0x01 */
  reg = depth;
  rts88xx_write_reg (devnum, 0x2c, &reg);

  /* stop before starting */
  low_stop_mvmt (devnum);

  /* effective start */
  status = low_start_mvmt (devnum);

  DBG (2, "rts88xx_commit: end\n");

  return status;
}

/*
 *         RTS88XX     END
 */



/*
 * sets the scanner idle
 */
static SANE_Status
lexmark_low_set_idle (SANE_Int devnum)
{
  SANE_Byte regs[14] =
    { 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x60
  };
  if (rts88xx_write_regs (devnum, 16, regs, 14) != SANE_STATUS_GOOD)
    {
      DBG (5, "lexmark_low_set_idle : register write failed ...\n");
      return SANE_STATUS_IO_ERROR;
    }
  return SANE_STATUS_GOOD;
}


/* wake up scanner */
#if 0
static SANE_Status
lexmark_low_wake_up (Lexmark_Device * dev)
{
  SANE_Byte regs[5] = { 0x12, 0x14, 0x16, 0x18, 0x1a };
  SANE_Byte values[5] = { 0x0f, 0x00, 0x07, 0x00, 0x00 };
  int i;

  /* send the wake-up sequence, one reg at at time */
  for (i = 0; i < 10; i++)
    {
      if (rts88xx_write_reg (dev->devnum, regs[i], values + i) !=
	  SANE_STATUS_GOOD)
	{
	  DBG (5,
	       "lexmark_low_wake_up : register write pass %d failed ...\n",
	       i);
	  return SANE_STATUS_IO_ERROR;
	}
    }
  return SANE_STATUS_GOOD;
}
#endif


/**
 * 
 */
#ifdef DEEP_DEBUG
static void
write_pnm_file (char *title, int pixels, int lines, int color,
		unsigned char *data)
{
  FILE *fdbg;
  int x, y;

  fdbg = fopen (title, "wb");
  if (fdbg == NULL)
    return;

  if (color)
    {
      fprintf (fdbg, "P6\n%d %d\n255\n", pixels, lines);
      for (y = 0; y < lines; y++)
	{
	  for (x = 0; x < pixels; x += 2)
	    {
	      fputc (data[y * pixels * 3 + x + 1], fdbg);
	      fputc (data[y * pixels * 3 + x + 1 + pixels], fdbg);
	      fputc (data[y * pixels * 3 + x + 1 + pixels * 2], fdbg);
	      fputc (data[y * pixels * 3 + x], fdbg);
	      fputc (data[y * pixels * 3 + x + pixels], fdbg);
	      fputc (data[y * pixels * 3 + x + pixels * 2], fdbg);
	    }
	}
    }
  else
    {
      fprintf (fdbg, "P5\n%d %d\n255\n", pixels, lines);
      fwrite (data, pixels, lines, fdbg);
    }
  fclose (fdbg);
}
#endif

/*
 * mid level hardware functions
 */
/*
 * model init
 */
SANE_Status
sanei_lexmark_low_init (Lexmark_Device * dev)
{
  int i;
  SANE_Status status;

  DBG_INIT ();

  status = SANE_STATUS_UNSUPPORTED;
  DBG (2, "low_init: start\n");

  /* clear all registers first */
  for (i = 0; i < 255; i++)
    {
      dev->shadow_regs[i] = 0;
    }

  /* set up per model constant values */
  dev->shadow_regs[0xf3] = 0xf8;
  dev->shadow_regs[0xf4] = 0x7f;

  switch (dev->model.sensor_type)
    {
    case X74_SENSOR:
      dev->shadow_regs[0x00] = 0x04;
      dev->shadow_regs[0x01] = 0x43;
      dev->shadow_regs[0x0b] = 0x70;
      dev->shadow_regs[0x12] = 0x0f;
      dev->shadow_regs[0x16] = 0x07;
      dev->shadow_regs[0x1d] = 0x20;
      dev->shadow_regs[0x28] = 0xe0;
      dev->shadow_regs[0x29] = 0xe3;
      dev->shadow_regs[0x2a] = 0xeb;
      dev->shadow_regs[0x2b] = 0x0d;
      dev->shadow_regs[0x2e] = 0x40;
      dev->shadow_regs[0x2e] = 0x86;
      dev->shadow_regs[0x2f] = 0x01;
      dev->shadow_regs[0x30] = 0x48;
      dev->shadow_regs[0x31] = 0x06;
      dev->shadow_regs[0x33] = 0x01;
      dev->shadow_regs[0x34] = 0x50;
      dev->shadow_regs[0x35] = 0x01;
      dev->shadow_regs[0x36] = 0x50;
      dev->shadow_regs[0x37] = 0x01;
      dev->shadow_regs[0x38] = 0x50;
      dev->shadow_regs[0x3a] = 0x20;
      dev->shadow_regs[0x3c] = 0x88;
      dev->shadow_regs[0x3d] = 0x08;
      dev->shadow_regs[0x65] = 0x80;
      dev->shadow_regs[0x66] = 0x64;
      dev->shadow_regs[0x6c] = 0xc8;
      dev->shadow_regs[0x72] = 0x1a;
      dev->shadow_regs[0x74] = 0x23;
      dev->shadow_regs[0x75] = 0x03;
      dev->shadow_regs[0x79] = 0x40;
      dev->shadow_regs[0x7A] = 0x01;
      dev->shadow_regs[0x8d] = 0x01;
      dev->shadow_regs[0x8e] = 0x60;
      dev->shadow_regs[0x8f] = 0x80;
      dev->shadow_regs[0x93] = 0x02;
      dev->shadow_regs[0x94] = 0x0e;
      dev->shadow_regs[0xa3] = 0xcc;
      dev->shadow_regs[0xa4] = 0x27;
      dev->shadow_regs[0xa5] = 0x24;
      dev->shadow_regs[0xc2] = 0x80;
      dev->shadow_regs[0xc3] = 0x01;
      dev->shadow_regs[0xc4] = 0x20;
      dev->shadow_regs[0xc5] = 0x0a;
      dev->shadow_regs[0xc8] = 0x04;
      dev->shadow_regs[0xc9] = 0x39;
      dev->shadow_regs[0xca] = 0x0a;
      dev->shadow_regs[0xe2] = 0x70;
      dev->shadow_regs[0xe3] = 0x17;
      dev->shadow_regs[0xf3] = 0xe0;
      dev->shadow_regs[0xf4] = 0xff;
      dev->shadow_regs[0xf5] = 0x01;
      status = SANE_STATUS_GOOD;
      break;
    case X1100_B2_SENSOR:
      dev->shadow_regs[0x01] = 0x43;
      dev->shadow_regs[0x0b] = 0x70;
      dev->shadow_regs[0x11] = 0x01;
      dev->shadow_regs[0x12] = 0x0f;
      dev->shadow_regs[0x13] = 0x01;
      dev->shadow_regs[0x15] = 0x01;
      dev->shadow_regs[0x16] = 0x0f;
      dev->shadow_regs[0x1d] = 0x20;
      dev->shadow_regs[0x28] = 0xeb;
      dev->shadow_regs[0x29] = 0xee;
      dev->shadow_regs[0x2a] = 0xf7;
      dev->shadow_regs[0x2b] = 0x01;
      dev->shadow_regs[0x2e] = 0x86;
      dev->shadow_regs[0x30] = 0x48;
      dev->shadow_regs[0x33] = 0x01;
      dev->shadow_regs[0x3a] = 0x20;
      dev->shadow_regs[0x3b] = 0x37;
      dev->shadow_regs[0x3c] = 0x88;
      dev->shadow_regs[0x3d] = 0x08;
      dev->shadow_regs[0x40] = 0x80;
      dev->shadow_regs[0x72] = 0x05;
      dev->shadow_regs[0x74] = 0x0e;
      dev->shadow_regs[0x8b] = 0xff;
      dev->shadow_regs[0x8c] = 0x02;
      dev->shadow_regs[0x8d] = 0x01;
      dev->shadow_regs[0x8e] = 0x60;
      dev->shadow_regs[0x8f] = 0x80;
      dev->shadow_regs[0x94] = 0x0e;
      dev->shadow_regs[0xa3] = 0xcc;
      dev->shadow_regs[0xa4] = 0x27;
      dev->shadow_regs[0xa5] = 0x24;
      dev->shadow_regs[0xb0] = 0xb2;
      dev->shadow_regs[0xb2] = 0x04;
      dev->shadow_regs[0xc2] = 0x80;
      dev->shadow_regs[0xc4] = 0x20;
      dev->shadow_regs[0xc8] = 0x04;
      dev->shadow_regs[0xc9] = 0x3b;
      dev->shadow_regs[0xed] = 0xc2;
      dev->shadow_regs[0xee] = 0x02;
      status = SANE_STATUS_GOOD;
      break;
    case X1100_2C_SENSOR:
      dev->shadow_regs[0x00] = 0x00;
      dev->shadow_regs[0x01] = 0x43;
      dev->shadow_regs[0x0b] = 0x70;
      dev->shadow_regs[0x0c] = 0x28;
      dev->shadow_regs[0x0d] = 0xa4;
      dev->shadow_regs[0x11] = 0x01;
      dev->shadow_regs[0x12] = 0x0f;
      dev->shadow_regs[0x13] = 0x01;
      dev->shadow_regs[0x15] = 0x01;
      dev->shadow_regs[0x16] = 0x0f;
      dev->shadow_regs[0x17] = 0x00;
      dev->shadow_regs[0x1d] = 0x20;
      dev->shadow_regs[0x28] = 0xf5;
      dev->shadow_regs[0x29] = 0xf7;
      dev->shadow_regs[0x2a] = 0xf5;
      dev->shadow_regs[0x2b] = 0x17;
      dev->shadow_regs[0x2d] = 0x41;
      dev->shadow_regs[0x2e] = 0x86;
      dev->shadow_regs[0x2f] = 0x11;
      dev->shadow_regs[0x30] = 0x48;
      dev->shadow_regs[0x31] = 0x01;
      dev->shadow_regs[0x33] = 0x01;
      dev->shadow_regs[0x34] = 0x50;
      dev->shadow_regs[0x35] = 0x01;
      dev->shadow_regs[0x36] = 0x50;
      dev->shadow_regs[0x37] = 0x01;
      dev->shadow_regs[0x38] = 0x50;
      dev->shadow_regs[0x3a] = 0x20;
      dev->shadow_regs[0x3b] = 0x37;
      dev->shadow_regs[0x3c] = 0x88;
      dev->shadow_regs[0x3d] = 0x08;
      dev->shadow_regs[0x40] = 0x80;
      dev->shadow_regs[0x47] = 0x01;
      dev->shadow_regs[0x48] = 0x1a;
      dev->shadow_regs[0x49] = 0x5b;
      dev->shadow_regs[0x4a] = 0x1b;
      dev->shadow_regs[0x4b] = 0x5b;
      dev->shadow_regs[0x4c] = 0x05;
      dev->shadow_regs[0x4d] = 0x3f;
      dev->shadow_regs[0x60] = 0x2f;
      dev->shadow_regs[0x61] = 0x36;
      dev->shadow_regs[0x62] = 0x30;
      dev->shadow_regs[0x63] = 0x36;
      dev->shadow_regs[0x65] = 0x80;
      dev->shadow_regs[0x66] = 0x64;
      dev->shadow_regs[0x6c] = 0xc8;
      dev->shadow_regs[0x6d] = 0x00;
      dev->shadow_regs[0x72] = 0x35;
      dev->shadow_regs[0x74] = 0x4e;
      dev->shadow_regs[0x75] = 0x03;
      dev->shadow_regs[0x79] = 0x40;
      dev->shadow_regs[0x7a] = 0x01;
      dev->shadow_regs[0x85] = 0x02;
      dev->shadow_regs[0x86] = 0x33;
      dev->shadow_regs[0x87] = 0x0f;
      dev->shadow_regs[0x88] = 0x24;
      dev->shadow_regs[0x8b] = 0xff;
      dev->shadow_regs[0x8c] = 0x02;
      dev->shadow_regs[0x8d] = 0x01;
      dev->shadow_regs[0x8e] = 0x60;
      dev->shadow_regs[0x8f] = 0x80;
      dev->shadow_regs[0x91] = 0x19;
      dev->shadow_regs[0x92] = 0x20;
      dev->shadow_regs[0x93] = 0x02;
      dev->shadow_regs[0x94] = 0x0e;
      dev->shadow_regs[0xa3] = 0x0d;
      dev->shadow_regs[0xa4] = 0x5e;
      dev->shadow_regs[0xa5] = 0x23;
      dev->shadow_regs[0xb0] = 0x2c;
      dev->shadow_regs[0xb1] = 0x07;
      dev->shadow_regs[0xb2] = 0x04;
      dev->shadow_regs[0xc2] = 0x80;
      dev->shadow_regs[0xc3] = 0x01;
      dev->shadow_regs[0xc4] = 0x20;
      dev->shadow_regs[0xc5] = 0x0a;
      dev->shadow_regs[0xc8] = 0x04;
      dev->shadow_regs[0xc9] = 0x3b;
      dev->shadow_regs[0xca] = 0x0a;
      dev->shadow_regs[0xe2] = 0xf8;
      dev->shadow_regs[0xe3] = 0x2a;
      status = SANE_STATUS_GOOD;
      break;
    case X1200_USB2_SENSOR:
      dev->shadow_regs[0x01] = 0x43;
      dev->shadow_regs[0x11] = 0x01;
      dev->shadow_regs[0x12] = 0x0f;
      dev->shadow_regs[0x13] = 0x01;
      dev->shadow_regs[0x15] = 0x01;
      dev->shadow_regs[0x16] = 0x0f;
      dev->shadow_regs[0x17] = 0x00;
      dev->shadow_regs[0x1d] = 0x20;
      dev->shadow_regs[0x28] = 0xf5;
      dev->shadow_regs[0x29] = 0xf7;
      dev->shadow_regs[0x2a] = 0xf5;
      dev->shadow_regs[0x2b] = 0x17;
      dev->shadow_regs[0x2d] = 0x41;
      dev->shadow_regs[0x2e] = 0x86;
      dev->shadow_regs[0x30] = 0x48;
      dev->shadow_regs[0x31] = 0x01;
      dev->shadow_regs[0x33] = 0x01;
      dev->shadow_regs[0x34] = 0x50;
      dev->shadow_regs[0x35] = 0x01;
      dev->shadow_regs[0x36] = 0x50;
      dev->shadow_regs[0x37] = 0x01;
      dev->shadow_regs[0x38] = 0x50;
      dev->shadow_regs[0x3c] = 0x88;
      dev->shadow_regs[0x3d] = 0x08;
      dev->shadow_regs[0x66] = 0x64;
      dev->shadow_regs[0x67] = 0x00;
      dev->shadow_regs[0x6c] = 0xc8;
      dev->shadow_regs[0x6d] = 0x00;
      dev->shadow_regs[0x72] = 0x35;
      dev->shadow_regs[0x74] = 0x4e;
      dev->shadow_regs[0x75] = 0x03;
      dev->shadow_regs[0x7a] = 0x01;
      dev->shadow_regs[0x93] = 0x0a;
      dev->shadow_regs[0x94] = 0x0e;

      dev->shadow_regs[0xc3] = 0x01;
      dev->shadow_regs[0xc4] = 0x20;
      dev->shadow_regs[0xc5] = 0x0a;
      dev->shadow_regs[0xc8] = 0x04;
      dev->shadow_regs[0xc9] = 0x3b;
      dev->shadow_regs[0xca] = 0x0a;
      dev->shadow_regs[0xe2] = 0xf8;
      dev->shadow_regs[0xe3] = 0x2a;
      status = SANE_STATUS_GOOD;
      break;
    case A920_SENSOR:
      dev->shadow_regs[0x01] = 0x43;
      dev->shadow_regs[0x0b] = 0x70;
      dev->shadow_regs[0x0c] = 0x28;
      dev->shadow_regs[0x0d] = 0xa4;
      dev->shadow_regs[0x11] = 0x01;
      dev->shadow_regs[0x12] = 0x0f;
      dev->shadow_regs[0x13] = 0x01;
      dev->shadow_regs[0x15] = 0x01;
      dev->shadow_regs[0x16] = 0x07;
      dev->shadow_regs[0x1d] = 0x20;
      dev->shadow_regs[0x28] = 0xf5;
      dev->shadow_regs[0x29] = 0xf7;
      dev->shadow_regs[0x2a] = 0xf5;
      dev->shadow_regs[0x2b] = 0x17;
      dev->shadow_regs[0x2e] = 0x86;
      dev->shadow_regs[0x30] = 0x48;
      dev->shadow_regs[0x31] = 0x01;
      dev->shadow_regs[0x33] = 0x01;
      dev->shadow_regs[0x3a] = 0x20;
      dev->shadow_regs[0x3b] = 0x37;
      dev->shadow_regs[0x3c] = 0x88;
      dev->shadow_regs[0x3d] = 0x08;
      dev->shadow_regs[0x47] = 0x21;
      dev->shadow_regs[0x48] = 0x1a;
      dev->shadow_regs[0x49] = 0x5b;
      dev->shadow_regs[0x4a] = 0x1b;
      dev->shadow_regs[0x4b] = 0x5b;
      dev->shadow_regs[0x4c] = 0x05;
      dev->shadow_regs[0x4d] = 0x3f;
      dev->shadow_regs[0x65] = 0x80;
      dev->shadow_regs[0x86] = 0x14;
      dev->shadow_regs[0x87] = 0x06;
      dev->shadow_regs[0x89] = 0xf5;
      dev->shadow_regs[0x8d] = 0x01;
      dev->shadow_regs[0x8e] = 0x60;
      dev->shadow_regs[0x8f] = 0x80;
      dev->shadow_regs[0x94] = 0x0e;
      dev->shadow_regs[0xa3] = 0x0d;
      dev->shadow_regs[0xa4] = 0x5e;
      dev->shadow_regs[0xa5] = 0x23;
      dev->shadow_regs[0xb0] = 0x2c;
      dev->shadow_regs[0xb1] = 0x0f;
      dev->shadow_regs[0xc2] = 0x80;
      dev->shadow_regs[0xc4] = 0x20;
      dev->shadow_regs[0xc8] = 0x04;
      status = SANE_STATUS_GOOD;
      break;
    case X1200_SENSOR:
      dev->shadow_regs[0x01] = 0x43;
      dev->shadow_regs[0x0b] = 0x70;
      dev->shadow_regs[0x0c] = 0x28;
      dev->shadow_regs[0x0d] = 0xa4;
      dev->shadow_regs[0x11] = 0x01;
      dev->shadow_regs[0x12] = 0x0f;
      dev->shadow_regs[0x13] = 0x01;
      dev->shadow_regs[0x15] = 0x01;
      dev->shadow_regs[0x16] = 0x0f;
      dev->shadow_regs[0x1d] = 0x20;
      dev->shadow_regs[0x28] = 0xe9;
      dev->shadow_regs[0x29] = 0xeb;
      dev->shadow_regs[0x2a] = 0xe9;
      dev->shadow_regs[0x2b] = 0x0b;
      dev->shadow_regs[0x2d] = 0x01;
      dev->shadow_regs[0x2e] = 0x86;
      dev->shadow_regs[0x2f] = 0x11;
      dev->shadow_regs[0x30] = 0x48;
      dev->shadow_regs[0x33] = 0x01;
      dev->shadow_regs[0x34] = 0x50;
      dev->shadow_regs[0x35] = 0x01;
      dev->shadow_regs[0x36] = 0x50;
      dev->shadow_regs[0x37] = 0x01;
      dev->shadow_regs[0x38] = 0x50;
      dev->shadow_regs[0x3a] = 0x20;
      dev->shadow_regs[0x3b] = 0x37;
      dev->shadow_regs[0x3c] = 0x88;
      dev->shadow_regs[0x3d] = 0x08;
      dev->shadow_regs[0x40] = 0x80;
      dev->shadow_regs[0x47] = 0x01;
      dev->shadow_regs[0x48] = 0x1a;
      dev->shadow_regs[0x49] = 0x5b;
      dev->shadow_regs[0x4a] = 0x1b;
      dev->shadow_regs[0x4b] = 0x5b;
      dev->shadow_regs[0x4c] = 0x05;
      dev->shadow_regs[0x4d] = 0x3f;
      dev->shadow_regs[0x60] = 0x12;
      dev->shadow_regs[0x62] = 0x81;
      dev->shadow_regs[0x63] = 0x03;
      dev->shadow_regs[0x65] = 0x80;
      dev->shadow_regs[0x66] = 0x64;
      dev->shadow_regs[0x6c] = 0xc8;
      dev->shadow_regs[0x72] = 0x1e;
      dev->shadow_regs[0x74] = 0x3c;
      dev->shadow_regs[0x75] = 0x03;
      dev->shadow_regs[0x79] = 0x40;
      dev->shadow_regs[0x7a] = 0x01;
      dev->shadow_regs[0x85] = 0x20;
      dev->shadow_regs[0x86] = 0x1e;
      dev->shadow_regs[0x87] = 0x39;
      dev->shadow_regs[0x8b] = 0xff;
      dev->shadow_regs[0x8c] = 0x02;
      dev->shadow_regs[0x8d] = 0x01;
      dev->shadow_regs[0x8e] = 0x60;
      dev->shadow_regs[0x8f] = 0x80;
      dev->shadow_regs[0x92] = 0x92;
      dev->shadow_regs[0x93] = 0x02;
      dev->shadow_regs[0x94] = 0x0e;
      dev->shadow_regs[0xa3] = 0x0d;
      dev->shadow_regs[0xa4] = 0x5e;
      dev->shadow_regs[0xa5] = 0x23;
      dev->shadow_regs[0xb0] = 0x2c;
      dev->shadow_regs[0xb1] = 0x07;
      dev->shadow_regs[0xb2] = 0x04;
      dev->shadow_regs[0xc2] = 0x80;
      dev->shadow_regs[0xc3] = 0x01;
      dev->shadow_regs[0xc4] = 0x20;
      dev->shadow_regs[0xc5] = 0x0a;
      dev->shadow_regs[0xc8] = 0x04;
      dev->shadow_regs[0xc9] = 0x3b;
      dev->shadow_regs[0xca] = 0x0a;
      dev->shadow_regs[0xe2] = 0xf8;
      dev->shadow_regs[0xe3] = 0x2a;
      dev->shadow_regs[0xf3] = 0xff;
      dev->shadow_regs[0xf4] = 0x0f;
      break;
    }
  DBG (5, "sanei_lexmark_low_init: init done for model %s/%s\n",
       dev->model.model, dev->model.name);
  DBG (2, "low_init: done\n");
  return status;
}

void
sanei_lexmark_low_destroy (Lexmark_Device * dev)
{
  /* free the read buffer */
  if (dev->read_buffer != NULL)
    read_buffer_free (dev->read_buffer);
}


SANE_Status
low_usb_bulk_write (SANE_Int devnum, SANE_Byte * cmd, size_t * size)
{
  SANE_Status status;
  size_t cmd_size;

  cmd_size = *size;
#ifdef FAKE_USB
  status = SANE_STATUS_GOOD;
#else
  status = sanei_usb_write_bulk (devnum, cmd, size);
#endif
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "low_usb_bulk_write: returned %s (size = %lu, expected %lu)\n",
	   sane_strstatus (status), (u_long) * size, (u_long) cmd_size);
      /* F.O. should reset the pipe here... */
    }
  return status;
}

SANE_Status
low_usb_bulk_read (SANE_Int devnum, SANE_Byte * buf, size_t * size)
{
  SANE_Status status;
  size_t exp_size;

  exp_size = *size;
#ifdef FAKE_USB
  status = SANE_STATUS_GOOD;
#else
  status = sanei_usb_read_bulk (devnum, buf, size);
#endif
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "low_usb_bulk_read: returned %s (size = %lu, expected %lu)\n",
	   sane_strstatus (status), (u_long) * size, (u_long) exp_size);
      /* F.O. should reset the pipe here... */
    }
  DBG (7, "low_usb_bulk_read: returned size = %lu (required %lu)\n",
       (u_long) * size, (u_long) exp_size);
  return status;
}


SANE_Status
low_start_mvmt (SANE_Int devnum)
{
  SANE_Status status;
  SANE_Byte reg;

  reg = 0x68;
  rts88xx_write_reg (devnum, 0xb3, &reg);
  status = rts88xx_write_reg (devnum, 0xb3, &reg);
  return status;
}

SANE_Status
low_stop_mvmt (SANE_Int devnum)
{
  SANE_Status status;
  SANE_Byte reg;

  /* Stop scanner - clear reg 0xb3: */
  reg = 0x02;
  rts88xx_write_reg (devnum, 0xb3, &reg);
  rts88xx_write_reg (devnum, 0xb3, &reg);
  reg = 0x00;
  rts88xx_write_reg (devnum, 0xb3, &reg);
  status = rts88xx_write_reg (devnum, 0xb3, &reg);
  return status;
}

SANE_Status
low_clr_c6 (SANE_Int devnum)
{
  SANE_Status status;
  SANE_Byte reg;

  /* Clear register 0xC6 */
  /* cmd_size = 0x05;
     return low_usb_bulk_write (devnum, clearC6_command_block, &cmd_size); */

  reg = 0x00;
  status = rts88xx_write_reg (devnum, 0xc6, &reg);
  return status;
}

/* stops current scan */
static SANE_Status
low_cancel (SANE_Int devnum)
{
  SANE_Status status;

  DBG (2, "low_cancel: start\n");
  status = low_stop_mvmt (devnum);
  if (status != SANE_STATUS_GOOD)
    return status;
  status = low_clr_c6 (devnum);
  if (status != SANE_STATUS_GOOD)
    return status;
  DBG (2, "low_cancel: end.\n");
  return status;
}

static SANE_Status
low_start_scan (SANE_Int devnum, SANE_Byte * regs)
{
  SANE_Status status;

  DBG (2, "low_start_scan: start\n");

  /* writes registers to scanner */
  regs[0x32] = 0x00;
  status = low_write_all_regs (devnum, regs);
  if (status != SANE_STATUS_GOOD)
    return status;
  regs[0x32] = 0x40;
  status = low_write_all_regs (devnum, regs);
  if (status != SANE_STATUS_GOOD)
    return status;

  /* Stop scanner - clear reg 0xb3: */
  /* status = low_stop_mvmt (devnum);
     if (status != SANE_STATUS_GOOD)
     return status; */

  /* then start */
  status = rts88xx_commit (devnum, regs[0x2c]);
  DBG (2, "low_start_scan: end.\n");
  return status;
}

/* wait for scan data being available */
static SANE_Status
low_poll_data (SANE_Int devnum)
{
  SANE_Status status;
  int loops = 0;
  size_t size;
  static SANE_Byte command4_block[] = { 0x90, 0x00, 0x00, 0x03 };
  SANE_Byte result[3];
  SANE_Word count;

  /* Poll the available byte count until not 0 */
  while (loops < 1000)
    {
      /* 10 ms sleep */
      usleep (10000);

      /* as stated in sanei_lexmark_low_search_home_bwd, we read
       * available data count twice */
      size = 4;
      status = low_usb_bulk_write (devnum, command4_block, &size);
      if (status != SANE_STATUS_GOOD)
	return status;
      size = 0x3;
      status = low_usb_bulk_read (devnum, result, &size);
      if (status != SANE_STATUS_GOOD)
	return status;
      size = 4;
      /* read availbale data size again */
      status = low_usb_bulk_write (devnum, command4_block, &size);
      if (status != SANE_STATUS_GOOD)
	return status;
      size = 0x3;
      status = low_usb_bulk_read (devnum, result, &size);
      if (status != SANE_STATUS_GOOD)
	return status;
      count = result[0] + (result[1] << 8) + (result[2] << 16);
      if (count != 0)
	{
	  DBG (15, "low_poll_data: %d bytes available\n", count);
	  return SANE_STATUS_GOOD;
	}
      loops++;
    }
  return SANE_STATUS_IO_ERROR;
}

/**
 * do a simple scan with the given registers. data buffer is allocated within
 * the function
 */
static SANE_Status
low_simple_scan (Lexmark_Device * dev, SANE_Byte * regs, int xoffset,
		 int pixels, int yoffset, int lines, SANE_Byte ** data)
{
  SANE_Status status = SANE_STATUS_GOOD;
  static SANE_Byte reg;
  size_t size, read, needed;
  int i, bpl, yend;

  DBG (2, "low_simple_scan: start\n");
  DBG (15, "low_simple_scan: x=%d, pixels=%d (ex=%d), y=%d, lines=%d\n",
       xoffset, pixels, xoffset + pixels * regs[0x7a], yoffset, lines);

  /* set up registers */
  regs[0x60] = LOBYTE (yoffset);
  regs[0x61] = HIBYTE (yoffset);
  yend = yoffset + lines;
  if ((dev->model.motor_type == A920_MOTOR
       || dev->model.motor_type == X74_MOTOR) && rts88xx_is_color (regs)
      && dev->val[OPT_RESOLUTION].w == 600)
    yend *= 2;
  regs[0x62] = LOBYTE (yend);
  regs[0x63] = HIBYTE (yend);

  regs[0x66] = LOBYTE (xoffset);
  regs[0x67] = HIBYTE (xoffset);

  regs[0x6c] = LOBYTE (xoffset + pixels * regs[0x7a]);
  regs[0x6d] = HIBYTE (xoffset + pixels * regs[0x7a]);

  /* allocate memory */
  if (rts88xx_is_color (regs))
    bpl = 3 * pixels;
  else
    bpl = pixels;
  *data = (SANE_Byte *) malloc (bpl * lines);
  if (*data == NULL)
    {
      DBG (2,
	   "low_simple_scan: failed to allocate %d bytes !\n", bpl * lines);
      return SANE_STATUS_NO_MEM;
    }

  /* start scan */
  status = low_cancel (dev->devnum);
  if (status != SANE_STATUS_GOOD)
    return status;


  status = low_start_scan (dev->devnum, regs);
  if (status != SANE_STATUS_GOOD)
    return status;

  /* wait for data */
  status = low_poll_data (dev->devnum);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "low_simple_scan: time-out while waiting for data.\n");
      return status;
    }

  /* data reading loop */
  needed = bpl * lines;
  DBG (1, "low_simple_scan: bpl=%d, lines=%d, needed=%lu.\n", bpl, lines,
       (u_long) needed);
  read = 0;
  do
    {
      /* this block would deserve to be a function */
      status =
	rts88xx_read_data (dev->devnum, needed - read, (*data) + read, &size);
      if (status != SANE_STATUS_GOOD)
	return status;
      read += size;
    }
  while (read < needed);

  /* if needed, wait for motor to stop */
  if (regs[0xc3] & 0x80)
    {
      i = 0;
      do
	{
	  if (rts88xx_read_reg (dev->devnum, 0xb3, &reg) != SANE_STATUS_GOOD)
	    {
	      DBG (5, "low_simple_scan: register read failed ...\n");
	      return SANE_STATUS_IO_ERROR;
	    }
	  usleep (100000);
	  i++;
	}
      while ((reg & 0x08) && (i < 100));
      if (reg & 0x08)
	{
	  DBG (5,
	       "low_simple_scan : timeout waiting for motor to stop ...\n");
	  return SANE_STATUS_IO_ERROR;
	}
    }

  /* stop scan */
  status = low_cancel (dev->devnum);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "low_simple_scan: cancel failed.\n");
      return status;
    }

  DBG (2, "low_simple_scan: end.\n");
  return status;
}

/*
 * open USB device ,read initial registers values and probe sensor
 */
SANE_Status
sanei_lexmark_low_open_device (Lexmark_Device * dev)
{
  /* This function calls the Sane Interface to open this usb device.
     It also needlessly does what the Windows driver does and reads
     the entire register set - this may be removed. */

  SANE_Status result;
  static SANE_Byte command_block[] = { 0x80, 0, 0x00, 0xFF };
  size_t size;
  SANE_Byte variant = 0;
  SANE_Byte shadow_regs[255];
  int sx, ex;
  int sy, ey;
  int i;
  char msg[2048];


#ifdef FAKE_USB
  result = SANE_STATUS_GOOD;
  shadow_regs[0x00] = 0x91;
  shadow_regs[0xb0] = 0x2c;
  shadow_regs[0x10] = 0x97;
  shadow_regs[0x10] = 0x87;
  shadow_regs[0xf3] = 0xf8;
  shadow_regs[0xf4] = 0x7f;
#else
  result = sanei_usb_open (dev->sane.name, &(dev->devnum));
#endif
  DBG (2, "sanei_lexmark_low_open_device: devnum=%d\n", dev->devnum);

  size = 4;
  low_usb_bulk_write (dev->devnum, command_block, &size);
  size = 0xFF;
  memset (shadow_regs, 0, sizeof (shadow_regs));
  low_usb_bulk_read (dev->devnum, shadow_regs, &size);

  if (DBG_LEVEL > 2)
    {
      DBG (2, "sanei_lexmark_low_open_device: initial registers values\n");
      for (i = 0; i < 255; i++)
	{
	  sprintf (msg + i * 5, "0x%02x ", shadow_regs[i]);
	}
      DBG (3, "%s\n", msg);
    }

  /* it seems that at first read after reset, registers hold information
   * about the scanner. Register 0x00 is overwritten with 0, so only first read
   * after USB plug-in gives this value */
  if (shadow_regs[0] == 0x91)
    {
      sx = shadow_regs[0x67] * 256 + shadow_regs[0x66];
      ex = shadow_regs[0x6d] * 256 + shadow_regs[0x6c];
      DBG (7, "startx=%d, endx=%d, pixels=%d, coef=%d, r2f=0x%02x\n", sx, ex,
	   ex - sx, dev->shadow_regs[0x7a], shadow_regs[0x2f]);
      sy = shadow_regs[0x61] * 256 + shadow_regs[0x60];
      ey = shadow_regs[0x63] * 256 + shadow_regs[0x62];
      DBG (7, "starty=%d, endy=%d, lines=%d\n", sy, ey, ey - sy);
    }

  /* we use register 0xb0 to identify details about models   */
  /* this register isn't overwritten during normal operation */
  if (shadow_regs[0xb0] == 0x2c && dev->model.sensor_type == X1100_B2_SENSOR)
    {
      variant = shadow_regs[0xb0];
    }
  /* now the same with register 0x10 */
  /* which most likely signals USB2.0/USB1.1 */
  if ((dev->model.sensor_type == X1200_SENSOR) && (shadow_regs[0x10] == 0x97))
    {
      variant = shadow_regs[0x10];
    }

  /* if find a case where default model given is inappropriate, reassign it
   * since we have now the informations to get the real one.
   * We could avoid this if attach() did open and read registers, not init */
  if (variant != 0)
    {
      DBG (3,
	   "sanei_lexmark_low_open_device: reassign model/sensor for variant 0x%02x\n",
	   variant);
      sanei_lexmark_low_assign_model (dev, dev->sane.name,
				      dev->model.vendor_id,
				      dev->model.product_id, variant);
      /* since model has changed, run init again */
      sanei_lexmark_low_init (dev);
    }
  DBG (2, "sanei_lexmark_low_open_device: end\n");
  return result;
}

void
sanei_lexmark_low_close_device (Lexmark_Device * dev)
{
  /* put scanner in idle state */
  lexmark_low_set_idle (dev->devnum);

  /* This function calls the Sane USB library to close this usb device */
#ifndef FAKE_USB
  sanei_usb_close (dev->devnum);
#endif
  return;
}


/* This function writes the contents of the given registers to the 
     scanner. */
SANE_Status
low_write_all_regs (SANE_Int devnum, SANE_Byte * regs)
{
  int i;
  SANE_Status status;
  size_t size;
  static SANE_Byte command_block1[0xb7];
  static SANE_Byte command_block2[0x4f];
  command_block1[0] = 0x88;
  command_block1[1] = 0x00;
  command_block1[2] = 0x00;
  command_block1[3] = 0xb3;
  for (i = 0; i < 0xb3; i++)
    {
      command_block1[i + 4] = regs[i];
    }
  command_block2[0] = 0x88;
  command_block2[1] = 0xb4;
  command_block2[2] = 0x00;
  command_block2[3] = 0x4b;
  for (i = 0; i < 0x4b; i++)
    {
      command_block2[i + 4] = regs[i + 0xb4];
    }
  size = 0xb7;

#ifdef DEEP_DEBUG
  fprintf (stderr, "write_all(0x00,255)=");
  for (i = 0; i < 255; i++)
    {
      fprintf (stderr, "0x%02x ", regs[i]);
    }
  fprintf (stderr, "\n");
#endif

  status = low_usb_bulk_write (devnum, command_block1, &size);
  if (status != SANE_STATUS_GOOD)
    return status;
  size = 0x4f;
  status = low_usb_bulk_write (devnum, command_block2, &size);
  if (status != SANE_STATUS_GOOD)
    return status;
  return SANE_STATUS_GOOD;
}


SANE_Bool
low_is_home_line (Lexmark_Device * dev, unsigned char *buffer)
{
  /*
     This function assumes the buffer has a size of 2500 bytes.It is 
     destructive to the buffer.

     Here is what it does:

     Go through the buffer finding low and high values, which are computed by 
     comparing to the average: 
     average = (lowest value + highest value)/2
     High bytes are changed to 0xFF (white), lower or equal bytes are changed 
     to 0x00 (black),so that the buffer only contains white (0xFF) or black 
     (0x00) values.

     Next, we go through the buffer. We use a tolerance of 5 bytes on each end
     of the buffer and check a region from bytes 5 to 2495. We start assuming
     we are in a white region and look for the start of a black region. We save
     this index as the transition from white to black. We also save where we 
     change from black back to white. We continue checking for transitions 
     until the end of the check region. If we don't have exactly two 
     transitions when we reach the end we return SANE_FALSE.

     The final check compares the transition indices to the nominal values
     plus or minus the tolerence. For the first transition (white to black
     index) the value must lie in the range 1235-30 (1205) to 1235+30 (1265).
     For the second transition (black to white) the value must lie in the range
     1258-30 (1228) to 1258+30 (1288). If the indices are out of range we 
     return SANE_FALSE. Otherwise, we return SANE_TRUE.
   */


  unsigned char max_byte = 0;
  unsigned char min_byte = 0xFF;
  unsigned char average;
  int i;
  int home_point1;
  int home_point2;
  region_type region;
  int transition_counter;
  int index1 = 0;
  int index2 = 0;
  int low_range, high_range;

#ifdef DEEP_DEBUG
  static int numero = 0;
  char titre[80];
  FILE *trace = NULL;
  sprintf (titre, "lgn%03d.pnm", numero);
  trace = fopen (titre, "wb");
  if (trace)
    {
      fprintf (trace, "P5\n2500 1\n255\n");
      fwrite (buffer, 2500, 1, trace);
      fclose (trace);
    }
  numero++;
#endif

  DBG (15, "low_is_home_line: start\n");
  /* Find the max and the min */
  for (i = 0; i < 2500; i++)
    {
      if (*(buffer + i) > max_byte)
	max_byte = *(buffer + i);
      if (*(buffer + i) < min_byte)
	min_byte = *(buffer + i);
    }

  /* The average */
  average = ((max_byte + min_byte) / 2);

  /* Set bytes as white (0xFF) or black (0x00) */
  for (i = 0; i < 2500; i++)
    {
      if (*(buffer + i) > average)
	*(buffer + i) = 0xFF;
      else
	*(buffer + i) = 0x00;
    }

  region = white;
  transition_counter = 0;

  /* Go through the check region - bytes 5 to 2495 */
  /* XXX STEF XXX shrink the area to where the dot should be
   * +-100 around the 1250 expected location */
  for (i = 1150; i <= 1350; i++)
    {
      /* Check for transition to black */
      if ((region == white) && (*(buffer + i) == 0))
	{
	  if (transition_counter < 2)
	    {
	      region = black;
	      index1 = i;
	      transition_counter++;
	    }
	  else
	    {
	      DBG (15, "low_is_home_line: no transition to black \n");
	      return SANE_FALSE;
	    }
	}
      /* Check for transition to white */
      else if ((region == black) && (*(buffer + i) == 0xFF))
	{
	  if (transition_counter < 2)
	    {
	      region = white;
	      index2 = i;
	      transition_counter++;
	    }
	  else
	    {
	      DBG (15, "low_is_home_line: no transition to white \n");
	      return SANE_FALSE;
	    }
	}
    }

  /* Check that the number of transitions is 2 */
  if (transition_counter != 2)
    {
      DBG (15, "low_is_home_line: transitions!=2 (%d)\n", transition_counter);
      return SANE_FALSE;
    }




  /* Check that the 1st index is in range */
  home_point1 = dev->model.HomeEdgePoint1;
  low_range = home_point1 - HomeTolerance;
  high_range = home_point1 + HomeTolerance;

  if ((index1 < low_range) || (index1 > high_range))
    {
      DBG (15, "low_is_home_line: index1=%d out of range\n", index1);
      return SANE_FALSE;
    }


  /* Check that the 2nd index is in range */
  home_point2 = dev->model.HomeEdgePoint2;
  low_range = home_point2 - HomeTolerance;
  high_range = home_point2 + HomeTolerance;

  if ((index2 < low_range) || (index2 > high_range))
    {
      DBG (15, "low_is_home_line: index2=%d out of range.\n", index2);
      return SANE_FALSE;
    }

  /* We made it this far, so its a good home line. Return True */
  DBG (15, "low_is_home_line: success\n");
  return SANE_TRUE;
}

void
sanei_lexmark_low_move_fwd (SANE_Int distance, Lexmark_Device * dev,
			    SANE_Byte * regs)
{
  /*
     This function moves the scan head forward with the highest vertical 
     resolution of 1200dpi. The distance moved is given by the distance
     parameter. 

     As an example, given a distance parameter of 600, the scan head will
     move 600/1200", or 1/2" forward.
   */

  static SANE_Byte pollstopmoving_command_block[] =
    { 0x80, 0xb3, 0x00, 0x01 };


  size_t cmd_size;
  SANE_Int devnum;
  SANE_Bool scan_head_moving;
  SANE_Byte read_result;

  DBG (2, "sanei_lexmark_low_move_fwd: \n");
  devnum = dev->devnum;


  /* registers set-up */
  regs[0x2c] = 0x00;
  regs[0x2d] = 0x41;
  regs[0x65] = 0x80;
  switch (dev->model.sensor_type)
    {
    case X74_SENSOR:
      rts88xx_set_scan_frequency (regs, 0);
      regs[0x93] = 0x06;
      break;
    case X1100_B2_SENSOR:
      regs[0x8b] = 0x00;
      regs[0x8c] = 0x00;
      regs[0x93] = 0x06;
      break;
    case X1100_2C_SENSOR:
      rts88xx_set_scan_frequency (regs, 0);
      regs[0x93] = 0x06;
      break;
    case A920_SENSOR:
      rts88xx_set_scan_frequency (regs, 0);
      regs[0x8b] = 0xff;
      regs[0x8c] = 0x02;
      regs[0x93] = 0x0e;
      break;
    case X1200_SENSOR:
      dev->shadow_regs[0x2d] = 0x01;
      rts88xx_set_scan_frequency (regs, 0);
      break;
    case X1200_USB2_SENSOR:
      dev->shadow_regs[0x2d] = 0x4f;
      rts88xx_set_scan_frequency (regs, 0);
      break;
    }

  /* set grayscale scan + nodata/nochannel?  */
  regs[0x2f] = 0xa1;

  /* set ? */
  regs[0x34] = 0x50;
  regs[0x35] = 0x01;
  regs[0x36] = 0x50;
  regs[0x37] = 0x01;
  regs[0x38] = 0x50;
  /* set motor resolution divisor */
  regs[0x39] = 0x00;
  /* set vertical start/end positions */
  regs[0x60] = LOBYTE (distance - 1);
  regs[0x61] = HIBYTE (distance - 1);
  regs[0x62] = LOBYTE (distance);
  regs[0x63] = HIBYTE (distance);
  /* set horizontal start position */
  regs[0x66] = 0x64;
  regs[0x67] = 0x00;
  /* set horizontal end position */
  regs[0x6c] = 0xc8;
  regs[0x6d] = 0x00;
  /* set horizontal resolution */
  regs[0x79] = 0x40;
  regs[0x7a] = 0x01;
  /* don't buffer data for this scan */
  regs[0xb2] = 0x04;
  /* Motor enable & Coordinate space denominator */
  regs[0xc3] = 0x81;
  /* Movement direction & step size */
  regs[0xc6] = 0x09;
  /* ? */
  regs[0x80] = 0x00;
  regs[0x81] = 0x00;
  regs[0x82] = 0x00;
  regs[0xc5] = 0x0a;


  switch (dev->model.motor_type)
    {
    case X1100_MOTOR:
    case A920_MOTOR:
      /* step size range2 */
      regs[0xc9] = 0x3b;
      /* ? */
      regs[0xca] = 0x0a;
      /* motor curve stuff */
      regs[0xe0] = 0x00;
      regs[0xe1] = 0x00;
      regs[0xe4] = 0x00;
      regs[0xe5] = 0x00;
      regs[0xe7] = 0x00;
      regs[0xe8] = 0x00;
      regs[0xe2] = 0x09;
      regs[0xe3] = 0x1a;
      regs[0xe6] = 0xdc;
      regs[0xe9] = 0x1b;
      regs[0xec] = 0x07;
      regs[0xef] = 0x03;
      break;
    case X74_MOTOR:
      regs[0xc5] = 0x41;
      /* step size range2 */
      regs[0xc9] = 0x39;
      /* ? */
      regs[0xca] = 0x40;
      /* motor curve stuff */
      regs[0xe0] = 0x00;
      regs[0xe1] = 0x00;
      regs[0xe2] = 0x09;
      regs[0xe3] = 0x1a;
      regs[0xe4] = 0x00;
      regs[0xe5] = 0x00;
      regs[0xe6] = 0x64;
      regs[0xe7] = 0x00;
      regs[0xe8] = 0x00;
      regs[0xe9] = 0x32;
      regs[0xec] = 0x0c;
      regs[0xef] = 0x08;
      break;
    }


  /* prepare for register write */
  low_clr_c6 (devnum);
  low_stop_mvmt (devnum);

/* Move Forward without scanning: */
  regs[0x32] = 0x00;
  low_write_all_regs (devnum, regs);
  regs[0x32] = 0x40;
  low_write_all_regs (devnum, regs);

  /* Stop scanner - clear reg 0xb3: */
  /* low_stop_mvmt (devnum); */

  rts88xx_commit (devnum, regs[0x2c]);

  /* Poll for scanner stopped - return value(3:0) = 0: */
  scan_head_moving = SANE_TRUE;
  while (scan_head_moving)
    {
#ifdef FAKE_USB
      scan_head_moving = SANE_FALSE;
#else
      cmd_size = 0x04;
      low_usb_bulk_write (devnum, pollstopmoving_command_block, &cmd_size);
      cmd_size = 0x1;
      low_usb_bulk_read (devnum, &read_result, &cmd_size);
      if ((read_result & 0xF) == 0x0)
	{
	  scan_head_moving = SANE_FALSE;
	}
#endif
    }

  /* this is needed to find the start line properly */
  if (dev->model.sensor_type == X74_SENSOR)
    low_stop_mvmt (devnum);

  DBG (2, "sanei_lexmark_low_move_fwd: end.\n");
}

SANE_Bool
sanei_lexmark_low_search_home_fwd (Lexmark_Device * dev)
{
  /* This function actually searches backwards one line looking for home */

  SANE_Int devnum;
  int i;
  SANE_Byte poll_result[3];
  SANE_Byte *buffer;
  SANE_Byte temp_byte;

  static SANE_Byte command4_block[] = { 0x90, 0x00, 0x00, 0x03 };

  static SANE_Byte command5_block[] = { 0x91, 0x00, 0x09, 0xc4 };

  size_t cmd_size;
  SANE_Bool got_line;
  SANE_Bool ret_val;

  devnum = dev->devnum;

  DBG (2, "sanei_lexmark_low_search_home_fwd:\n");

  /* set up registers according to the sensor type */
  switch (dev->model.sensor_type)
    {
    case X74_SENSOR:
      dev->shadow_regs[0x2c] = 0x03;
      dev->shadow_regs[0x2d] = 0x45;
      dev->shadow_regs[0x2f] = 0x21;
      dev->shadow_regs[0x30] = 0x48;
      dev->shadow_regs[0x31] = 0x06;
      dev->shadow_regs[0x34] = 0x05;
      dev->shadow_regs[0x35] = 0x05;
      dev->shadow_regs[0x36] = 0x09;
      dev->shadow_regs[0x37] = 0x09;
      dev->shadow_regs[0x38] = 0x0d;
      dev->shadow_regs[0x40] = 0x80;
      dev->shadow_regs[0x75] = 0x00;
      dev->shadow_regs[0x8b] = 0xff;
      dev->shadow_regs[0x93] = 0x06;
      break;
    case X1100_B2_SENSOR:
      dev->shadow_regs[0x2c] = 0x0f;
      dev->shadow_regs[0x2d] = 0x51;
      dev->shadow_regs[0x2f] = 0x21;
      dev->shadow_regs[0x34] = 0x04;
      dev->shadow_regs[0x35] = 0x04;
      dev->shadow_regs[0x36] = 0x08;
      dev->shadow_regs[0x37] = 0x08;
      dev->shadow_regs[0x38] = 0x0b;
      dev->shadow_regs[0x93] = 0x06;
      break;
    case X1100_2C_SENSOR:
      dev->shadow_regs[0x2c] = 0x0d;
      dev->shadow_regs[0x2d] = 0x4f;
      dev->shadow_regs[0x34] = 0x05;
      dev->shadow_regs[0x35] = 0x05;
      dev->shadow_regs[0x36] = 0x09;
      dev->shadow_regs[0x37] = 0x09;
      dev->shadow_regs[0x38] = 0x0d;
      dev->shadow_regs[0x40] = 0x80;
      dev->shadow_regs[0x72] = 0x35;
      dev->shadow_regs[0x74] = 0x4e;

      dev->shadow_regs[0x85] = 0x20;	/* 05 */
      dev->shadow_regs[0x86] = 0x00;	/* 05 */
      dev->shadow_regs[0x87] = 0x00;	/* 05 */
      dev->shadow_regs[0x88] = 0x00;	/* 45 */
      dev->shadow_regs[0x89] = 0x00;
      dev->shadow_regs[0x8b] = 0xff;

      dev->shadow_regs[0x93] = 0x06;	/* 0e */

      dev->shadow_regs[0x75] = 0x00;	/*    */
      dev->shadow_regs[0x91] = 0x00;	/* 60 */
      dev->shadow_regs[0x92] = 0x00;	/* 8d */
      dev->shadow_regs[0xb1] = 0x00;	/*    */
      dev->shadow_regs[0xc5] = 0x00;	/*    */
      dev->shadow_regs[0xca] = 0x00;	/*    */
      dev->shadow_regs[0xc3] = 0x01;	/*    */
      break;
    case A920_SENSOR:
      dev->shadow_regs[0x2c] = 0x0d;
      dev->shadow_regs[0x2d] = 0x4f;
      dev->shadow_regs[0x34] = 0x05;
      dev->shadow_regs[0x35] = 0x05;
      dev->shadow_regs[0x36] = 0x09;
      dev->shadow_regs[0x37] = 0x09;
      dev->shadow_regs[0x38] = 0x0d;
      dev->shadow_regs[0x40] = 0x80;
      dev->shadow_regs[0x72] = 0x35;
      dev->shadow_regs[0x74] = 0x4e;
      dev->shadow_regs[0x85] = 0x05;
      dev->shadow_regs[0x88] = 0x45;
      dev->shadow_regs[0x89] = 0x00;
      dev->shadow_regs[0x8b] = 0xff;
      dev->shadow_regs[0x91] = 0x60;
      dev->shadow_regs[0x92] = 0x8d;
      dev->shadow_regs[0x93] = 0x0e;
      break;
    case X1200_SENSOR:
      dev->shadow_regs[0x2c] = 0x01;
      dev->shadow_regs[0x2d] = 0x03;
      dev->shadow_regs[0x34] = 0x04;
      dev->shadow_regs[0x35] = 0x04;
      dev->shadow_regs[0x36] = 0x08;
      dev->shadow_regs[0x37] = 0x08;
      dev->shadow_regs[0x38] = 0x0b;
      dev->shadow_regs[0x66] = 0x88;
      dev->shadow_regs[0x6c] = 0x10;
      dev->shadow_regs[0x6d] = 0x14;
      dev->shadow_regs[0x75] = 0x00;
      dev->shadow_regs[0x93] = 0x06;
      dev->shadow_regs[0xc5] = 0x00;
      dev->shadow_regs[0xca] = 0x00;
      break;
    case X1200_USB2_SENSOR:
      dev->shadow_regs[0x0b] = 0x70;
      dev->shadow_regs[0x0c] = 0x28;
      dev->shadow_regs[0x0d] = 0xa4;
      dev->shadow_regs[0x2c] = 0x0d;
      dev->shadow_regs[0x2d] = 0x4f;
      dev->shadow_regs[0x2f] = 0x21;
      dev->shadow_regs[0x32] = 0x40;
      dev->shadow_regs[0x34] = 0x05;
      dev->shadow_regs[0x35] = 0x05;
      dev->shadow_regs[0x36] = 0x09;
      dev->shadow_regs[0x37] = 0x09;
      dev->shadow_regs[0x38] = 0x0d;
      dev->shadow_regs[0x3a] = 0x20;
      dev->shadow_regs[0x3b] = 0x37;
      dev->shadow_regs[0x40] = 0x80;
      dev->shadow_regs[0x47] = 0x01;
      dev->shadow_regs[0x48] = 0x1a;
      dev->shadow_regs[0x49] = 0x5b;
      dev->shadow_regs[0x4a] = 0x1b;
      dev->shadow_regs[0x4b] = 0x5b;
      dev->shadow_regs[0x4c] = 0x05;
      dev->shadow_regs[0x4d] = 0x3f;
      dev->shadow_regs[0x75] = 0x00;
      dev->shadow_regs[0x85] = 0x03;
      dev->shadow_regs[0x86] = 0x33;
      dev->shadow_regs[0x87] = 0x8f;
      dev->shadow_regs[0x88] = 0x34;
      dev->shadow_regs[0x8b] = 0xff;
      dev->shadow_regs[0x8e] = 0x60;
      dev->shadow_regs[0x8f] = 0x80;
      dev->shadow_regs[0x91] = 0x59;
      dev->shadow_regs[0x92] = 0x10;
      dev->shadow_regs[0x93] = 0x06;
      dev->shadow_regs[0xa3] = 0x0d;
      dev->shadow_regs[0xa4] = 0x5e;
      dev->shadow_regs[0xa5] = 0x23;
      dev->shadow_regs[0xb1] = 0x07;
      dev->shadow_regs[0xc2] = 0x80;
      dev->shadow_regs[0xc5] = 0x00;
      dev->shadow_regs[0xca] = 0x00;
      break;
    }
  dev->shadow_regs[0x65] = 0x80;
  dev->shadow_regs[0x8c] = 0x02;
  dev->shadow_regs[0x8d] = 0x01;
  dev->shadow_regs[0xb2] = 0x00;
  dev->shadow_regs[0xed] = 0x00;
  dev->shadow_regs[0xee] = 0x00;

  rts88xx_set_gain (dev->shadow_regs, dev->sensor->default_gain,
		    dev->sensor->default_gain, dev->sensor->default_gain);
  rts88xx_set_offset (dev->shadow_regs, 0x80, 0x80, 0x80);

  /* set grayscale scan */
  rts88xx_set_gray_scan (dev->shadow_regs);

  /* set motor resolution divisor */
  dev->shadow_regs[0x39] = 0x07;

  /* set vertical start/end positions */
  dev->shadow_regs[0x60] = 0x01;
  dev->shadow_regs[0x61] = 0x00;
  dev->shadow_regs[0x62] = 0x02;
  dev->shadow_regs[0x63] = 0x00;

  /* set # of head moves per CIS read */
  rts88xx_set_scan_frequency (dev->shadow_regs, 1);

  /* set horizontal start position */
  dev->shadow_regs[0x66] = 0x6a;	/* 0x88 for X1200 */
  dev->shadow_regs[0x67] = 0x00;
  /* set horizontal end position */
  dev->shadow_regs[0x6c] = 0xf2;	/* 0x1410 for X1200 */
  dev->shadow_regs[0x6d] = 0x13;
  /* set horizontal resolution */
  dev->shadow_regs[0x79] = 0x40;
  dev->shadow_regs[0x7a] = 0x02;
  /* Motor disable & Coordinate space denominator */
  dev->shadow_regs[0xc3] = 0x01;
  /* Movement direction & step size */
  dev->shadow_regs[0xc6] = 0x01;

  switch (dev->model.motor_type)
    {
    case A920_MOTOR:
    case X1100_MOTOR:
      /* step size range2 */
      dev->shadow_regs[0xc9] = 0x3b;
      /* step size range0 */
      dev->shadow_regs[0xe2] = 0x01;
      /* ? */
      dev->shadow_regs[0xe3] = 0x03;
      break;
    case X74_MOTOR:
      dev->shadow_regs[0xc4] = 0x20;
      dev->shadow_regs[0xc5] = 0x00;
      dev->shadow_regs[0xc8] = 0x04;
      /* step size range2 */
      dev->shadow_regs[0xc9] = 0x39;
      dev->shadow_regs[0xca] = 0x00;
      /* motor curve stuff */
      dev->shadow_regs[0xe0] = 0x29;
      dev->shadow_regs[0xe1] = 0x17;
      dev->shadow_regs[0xe2] = 0x8f;
      dev->shadow_regs[0xe3] = 0x06;
      dev->shadow_regs[0xe4] = 0x61;
      dev->shadow_regs[0xe5] = 0x16;
      dev->shadow_regs[0xe6] = 0x64;
      dev->shadow_regs[0xe7] = 0xb5;
      dev->shadow_regs[0xe8] = 0x08;
      dev->shadow_regs[0xe9] = 0x32;
      dev->shadow_regs[0xec] = 0x0c;
      dev->shadow_regs[0xef] = 0x08;
      break;
    }

  /* Stop the scanner */
  low_stop_mvmt (devnum);

  /* write regs out twice */
  dev->shadow_regs[0x32] = 0x00;
  low_write_all_regs (devnum, dev->shadow_regs);
  dev->shadow_regs[0x32] = 0x40;
  low_write_all_regs (devnum, dev->shadow_regs);

  /* Start Scan */
  rts88xx_commit (devnum, dev->shadow_regs[0x2c]);

  /* Poll the available byte count until not 0 */
  got_line = SANE_FALSE;
  while (!got_line)
    {
      cmd_size = 4;
      low_usb_bulk_write (devnum, command4_block, &cmd_size);
      cmd_size = 0x3;
      low_usb_bulk_read (devnum, poll_result, &cmd_size);
      if (!
	  (poll_result[0] == 0 && poll_result[1] == 0 && poll_result[2] == 0))
	{
	  /* if result != 00 00 00 we got data */
	  got_line = SANE_TRUE;
	}
    }

  /* create buffer for scan data */
  buffer = calloc (2500, sizeof (char));
  if (buffer == NULL)
    {
      return SANE_FALSE;
    }

  /* Tell the scanner to send the data */
  /* Write: 91 00 09 c4 */
  cmd_size = 4;
  low_usb_bulk_write (devnum, command5_block, &cmd_size);
  /* Read it */
  cmd_size = 0x09c4;
  low_usb_bulk_read (devnum, buffer, &cmd_size);

  /* Reverse order of bytes in words of buffer */
  for (i = 0; i < 2500; i = i + 2)
    {
      temp_byte = *(buffer + i);
      *(buffer + i) = *(buffer + i + 1);
      *(buffer + i + 1) = temp_byte;
    }

  /* check for home position */
  ret_val = low_is_home_line (dev, buffer);

  if (ret_val)
    DBG (2, "sanei_lexmark_low_search_home_fwd: !!!HOME POSITION!!!\n");

  /* free the buffer */
  free (buffer);
  DBG (2, "sanei_lexmark_low_search_home_fwd: end.\n");

  return ret_val;
}

SANE_Bool
sanei_lexmark_low_search_home_bwd (Lexmark_Device * dev)
{
/* This function must only be called if the scan head is past the home dot.
   It could damage the scanner if not.

   This function tells the scanner to do a grayscale scan backwards with a 
   300dpi resolution. It reads 2500 bytes of data between horizontal
   co-ordinates 0x6a and 0x13f2.

   The scan is set to read between vertical co-ordinates from 0x0a to 0x0f46,
   or 3900 lines. This equates to 13" at 300dpi, so we must stop the scan
   before it bangs against the end. A line limit is set so that a maximum of
   0x0F3C (13"*300dpi) lines can be read.

   To read the scan data we create a buffer space large enough to hold 10 
   lines of data. For each read we poll twice, ignoring the first poll. This 
   is required for timing. We repeat the double poll until there is data
   available. The number of lines (or number of buffers in our buffer space)
   is calculated from the size of the data available from the scanner. The
   number of buffers is calculated as the space required to hold 1.5 times
   the size of the data available from the scanner.

   After data is read from the scanner each line is checked if it is on the
   home dot. Lines are continued to be read until we are no longer on the home
   dot. */


  SANE_Int devnum;
  SANE_Status status;
  int i, j;
  SANE_Byte poll_result[3];
  SANE_Byte *buffer;
  SANE_Byte *buffer_start;
  SANE_Byte temp_byte;

  SANE_Int buffer_count = 0;
  SANE_Int size_requested;
  SANE_Int size_returned;
  SANE_Int no_of_buffers;
  SANE_Int buffer_limit = 0xF3C;

  SANE_Int high_byte, mid_byte, low_byte;
  SANE_Int home_line_count;
  SANE_Bool in_home_region;

  static SANE_Byte command4_block[] = { 0x90, 0x00, 0x00, 0x03 };

  static SANE_Byte command5_block[] = { 0x91, 0x00, 0xff, 0xc0 };
#ifdef DEEP_DEBUG
  FILE *img = NULL;
#endif

  size_t cmd_size;
  SANE_Bool got_line;

  devnum = dev->devnum;

  DBG (2, "sanei_lexmark_low_search_home_bwd:\n");

  /* set up registers */
  switch (dev->model.sensor_type)
    {
    case X74_SENSOR:
      dev->shadow_regs[0x2c] = 0x03;
      dev->shadow_regs[0x2d] = 0x45;
      dev->shadow_regs[0x34] = 0x09;
      dev->shadow_regs[0x35] = 0x09;
      dev->shadow_regs[0x36] = 0x11;
      dev->shadow_regs[0x37] = 0x11;
      dev->shadow_regs[0x38] = 0x19;
      dev->shadow_regs[0x85] = 0x00;
      dev->shadow_regs[0x93] = 0x06;
      dev->shadow_regs[0x40] = 0x80;
      /* important for detection of b/w transitions */
      dev->shadow_regs[0x75] = 0x00;
      dev->shadow_regs[0x8b] = 0xff;
      break;
    case X1100_B2_SENSOR:
      dev->shadow_regs[0x2c] = 0x0f;
      dev->shadow_regs[0x2d] = 0x51;
      dev->shadow_regs[0x34] = 0x07;
      dev->shadow_regs[0x35] = 0x07;
      dev->shadow_regs[0x36] = 0x0f;
      dev->shadow_regs[0x37] = 0x0f;
      dev->shadow_regs[0x38] = 0x15;
      dev->shadow_regs[0x85] = 0x20;
      dev->shadow_regs[0x93] = 0x06;
      break;
    case X1100_2C_SENSOR:
      dev->shadow_regs[0x2c] = 0x0d;
      dev->shadow_regs[0x2d] = 0x4f;
      dev->shadow_regs[0x34] = 0x09;
      dev->shadow_regs[0x35] = 0x09;
      dev->shadow_regs[0x36] = 0x11;
      dev->shadow_regs[0x37] = 0x11;
      dev->shadow_regs[0x38] = 0x19;
      dev->shadow_regs[0x85] = 0x20;
      dev->shadow_regs[0x93] = 0x06;
      break;
    case A920_SENSOR:
      dev->shadow_regs[0x2c] = 0x0d;
      dev->shadow_regs[0x2d] = 0x4f;
      dev->shadow_regs[0x34] = 0x09;
      dev->shadow_regs[0x35] = 0x09;
      dev->shadow_regs[0x36] = 0x11;
      dev->shadow_regs[0x37] = 0x11;
      dev->shadow_regs[0x38] = 0x19;
      dev->shadow_regs[0x85] = 0x05;
      dev->shadow_regs[0x93] = 0x0e;
      break;
    case X1200_SENSOR:
      dev->shadow_regs[0x2c] = 0x01;
      dev->shadow_regs[0x2d] = 0x03;
      dev->shadow_regs[0x34] = 0x07;
      dev->shadow_regs[0x35] = 0x07;
      dev->shadow_regs[0x36] = 0x0f;
      dev->shadow_regs[0x37] = 0x0f;
      dev->shadow_regs[0x38] = 0x15;
      break;

    case X1200_USB2_SENSOR:
      dev->shadow_regs[0x2c] = 0x0d;
      dev->shadow_regs[0x2d] = 0x4f;
      dev->shadow_regs[0x34] = 0x09;
      dev->shadow_regs[0x35] = 0x09;
      dev->shadow_regs[0x36] = 0x11;
      dev->shadow_regs[0x37] = 0x11;
      dev->shadow_regs[0x38] = 0x19;
      dev->shadow_regs[0x85] = 0x03;
      dev->shadow_regs[0x93] = 0x06;
      break;
    }
  rts88xx_set_gain (dev->shadow_regs, dev->sensor->default_gain,
		    dev->sensor->default_gain, dev->sensor->default_gain);
  dev->shadow_regs[0x65] = 0x80;
  dev->shadow_regs[0x8b] = 0xff;
  dev->shadow_regs[0x8c] = 0x02;
  dev->shadow_regs[0xb2] = 0x00;

  /* set calibration */
  rts88xx_set_offset (dev->shadow_regs, 0x80, 0x80, 0x80);

  /* set grayscale  scan  */
  dev->shadow_regs[0x2f] = 0x21;
  /* set motor resolution divisor */
  dev->shadow_regs[0x39] = 0x03;
  /* set vertical start/end positions */
  dev->shadow_regs[0x60] = 0x0a;
  dev->shadow_regs[0x61] = 0x00;
  dev->shadow_regs[0x62] = 0x46;
  dev->shadow_regs[0x63] = 0x0f;
  /* set # of head moves per CIS read */
  rts88xx_set_scan_frequency (dev->shadow_regs, 2);
  /* set horizontal start position */
  dev->shadow_regs[0x66] = 0x6a;	/* 0x88 for X1200 */
  dev->shadow_regs[0x67] = 0x00;
  /* set horizontal end position */
  dev->shadow_regs[0x6c] = 0xf2;	/* 0x1410 for X1200, 13f2 for X1200/rev. 97 */
  dev->shadow_regs[0x6d] = 0x13;
  /* set horizontal resolution */
  dev->shadow_regs[0x79] = 0x40;
  dev->shadow_regs[0x7a] = 0x02;

  /* Movement direction & step size */
  dev->shadow_regs[0xc6] = 0x01;
  /* Motor enable & Coordinate space denominator */
  dev->shadow_regs[0xc3] = 0x81;

  switch (dev->model.motor_type)
    {
    case X74_MOTOR:
      dev->shadow_regs[0xc4] = 0x20;
      dev->shadow_regs[0xc5] = 0x00;
      dev->shadow_regs[0xc8] = 0x04;
      /* step size range2 */
      dev->shadow_regs[0xc9] = 0x39;
      dev->shadow_regs[0xca] = 0x00;
      /* motor curve stuff */
      dev->shadow_regs[0xe0] = 0x29;
      dev->shadow_regs[0xe1] = 0x17;
      dev->shadow_regs[0xe2] = 0x8f;
      dev->shadow_regs[0xe3] = 0x06;
      dev->shadow_regs[0xe4] = 0x61;
      dev->shadow_regs[0xe5] = 0x16;
      dev->shadow_regs[0xe6] = 0x64;
      dev->shadow_regs[0xe7] = 0xb5;
      dev->shadow_regs[0xe8] = 0x08;
      dev->shadow_regs[0xe9] = 0x32;
      dev->shadow_regs[0xec] = 0x0c;
      dev->shadow_regs[0xef] = 0x08;
      break;
    case A920_MOTOR:
    case X1100_MOTOR:
      /* ? */
      dev->shadow_regs[0xc5] = 0x19;
      /* step size range2 */
      dev->shadow_regs[0xc9] = 0x3a;
      /* ? */
      dev->shadow_regs[0xca] = 0x08;
      /* motor curve stuff */
      dev->shadow_regs[0xe0] = 0xe3;
      dev->shadow_regs[0xe1] = 0x18;
      dev->shadow_regs[0xe2] = 0x03;
      dev->shadow_regs[0xe3] = 0x06;
      dev->shadow_regs[0xe4] = 0x2b;
      dev->shadow_regs[0xe5] = 0x17;
      dev->shadow_regs[0xe6] = 0xdc;
      dev->shadow_regs[0xe7] = 0xb3;
      dev->shadow_regs[0xe8] = 0x07;
      dev->shadow_regs[0xe9] = 0x1b;
      dev->shadow_regs[0xec] = 0x07;
      dev->shadow_regs[0xef] = 0x03;
      break;
    }

  /* Stop the scanner */
  low_stop_mvmt (devnum);

  /* write regs out twice */
  dev->shadow_regs[0x32] = 0x00;
  low_write_all_regs (devnum, dev->shadow_regs);
  dev->shadow_regs[0x32] = 0x40;
  low_write_all_regs (devnum, dev->shadow_regs);

  /* Start Scan */
  status = rts88xx_commit (devnum, dev->shadow_regs[0x2c]);

  /* create buffer to hold up to 10 lines of  scan data */
  buffer = calloc (10 * 2500, sizeof (char));
  if (buffer == NULL)
    {
      return SANE_FALSE;
    }

  home_line_count = 0;
  in_home_region = SANE_FALSE;

#ifdef DEEP_DEBUG
  img = fopen ("find_bwd.pnm", "wb");
  fprintf (img, "P5\n2500 100\n255\n");
#endif
  while (buffer_count < buffer_limit)
    {
      size_returned = 0;
      got_line = SANE_FALSE;
      while (!got_line)
	{
	  /* always poll twice (needed for timing) - disregard 1st poll */
	  cmd_size = 4;
	  status = low_usb_bulk_write (devnum, command4_block, &cmd_size);
	  if (status != SANE_STATUS_GOOD)
	    return SANE_FALSE;
	  cmd_size = 0x3;
	  status = low_usb_bulk_read (devnum, poll_result, &cmd_size);
	  if (status != SANE_STATUS_GOOD)
	    return SANE_FALSE;
	  cmd_size = 4;
	  status = low_usb_bulk_write (devnum, command4_block, &cmd_size);
	  if (status != SANE_STATUS_GOOD)
	    return SANE_FALSE;
	  cmd_size = 0x3;
	  status = low_usb_bulk_read (devnum, poll_result, &cmd_size);
	  if (status != SANE_STATUS_GOOD)
	    return SANE_FALSE;
	  if (!
	      (poll_result[0] == 0 && poll_result[1] == 0
	       && poll_result[2] == 0))
	    {
	      /* if result != 00 00 00 we got data */
	      got_line = SANE_TRUE;
	      high_byte = poll_result[2] << 16;
	      mid_byte = poll_result[1] << 8;
	      low_byte = poll_result[0];
	      size_returned = high_byte + mid_byte + low_byte;
	    }
	}

      /*size_requested = size_returned;*/
      size_requested = 2500;
      no_of_buffers = size_returned * 3;
      no_of_buffers = no_of_buffers / 2500;
      no_of_buffers = no_of_buffers >> 1;
      /* force 1 buffer at a time to improve accuray, which slow downs search */
      no_of_buffers = 1;

      if (no_of_buffers < 1)
	no_of_buffers = 1;
      else if (no_of_buffers > 10)
	no_of_buffers = 10;
      buffer_count = buffer_count + no_of_buffers;

      size_requested = no_of_buffers * 2500;

      /* Tell the scanner to send the data */
      /* Write: 91 <size_requested> */
      command5_block[1] = (SANE_Byte) (size_requested >> 16);
      command5_block[2] = (SANE_Byte) (size_requested >> 8);
      command5_block[3] = (SANE_Byte) (size_requested & 0xFF);

      cmd_size = 4;
      status = low_usb_bulk_write (devnum, command5_block, &cmd_size);
      if (status != SANE_STATUS_GOOD)
	return SANE_FALSE;
      /* Read it */
      cmd_size = size_requested;
      status = low_usb_bulk_read (devnum, buffer, &cmd_size);
      if (status != SANE_STATUS_GOOD)
	return SANE_FALSE;
      for (i = 0; i < no_of_buffers; i++)
	{
	  buffer_start = buffer + (i * 2500);
	  /* Reverse order of bytes in words of buffer */
	  for (j = 0; j < 2500; j = j + 2)
	    {
	      temp_byte = *(buffer_start + j);
	      *(buffer_start + j) = *(buffer_start + j + 1);
	      *(buffer_start + j + 1) = temp_byte;
	    }
#ifdef DEEP_DEBUG
	  fwrite (buffer + (i * 2500), 2500, 1, img);
#endif
	  if (low_is_home_line (dev, buffer_start))
	    {
	      home_line_count++;
	      if (home_line_count > 7)
		in_home_region = SANE_TRUE;
	    }
	  if (in_home_region)
	    {
	      /* slow down scanning : on purpose backtracking */
	      if (home_line_count)
		sleep (1);
	      free (buffer);
#ifdef DEEP_DEBUG
	      fflush (img);
	      i = ftell (img) / 2500;
	      rewind (img);
	      DBG (2, "sanei_lexmark_low_search_home_bwd: offset=%d\n", i);
	      fprintf (img, "P5\n2500 %03d\n", i);
	      fclose (img);
#endif
	      low_stop_mvmt (devnum);
	      DBG (2,
		   "sanei_lexmark_low_search_home_bwd: in home region, end.\n");
	      return SANE_TRUE;
	    }
	}
    }				/*   end while (buffer_count > buffer_limit); */
  free (buffer);
#ifdef DEEP_DEBUG
  fflush (img);
  i = ftell (img) / 2500;
  rewind (img);
  fprintf (img, "P5\n2500 %03d\n", i);
  fclose (img);
#endif
  low_stop_mvmt (devnum);

  DBG (2, "sanei_lexmark_low_search_home_bwd: end.\n");

  return SANE_FALSE;
}

SANE_Status
low_get_start_loc (SANE_Int resolution, SANE_Int * vert_start,
		   SANE_Int * hor_start, SANE_Int offset,
		   Lexmark_Device * dev)
{
  SANE_Int start_600;

  switch (dev->model.sensor_type)
    {
    case X1100_2C_SENSOR:
    case X1200_USB2_SENSOR:
    case A920_SENSOR:
    case X1200_SENSOR:
      start_600 = 195 - offset;
      *hor_start = 0x68;
      break;
    case X1100_B2_SENSOR:
      start_600 = 195 - offset;
      switch (resolution)
	{
	case 75:
	  *hor_start = 0x68;
	  break;
	case 150:
	  *hor_start = 0x68;
	  break;
	case 300:
	  *hor_start = 0x6a;
	  break;
	case 600:
	  *hor_start = 0x6b;
	  break;
	case 1200:
	  *hor_start = 0x6b;
	  break;
	default:
	  /* If we're here we have an invalid resolution */
	  return SANE_STATUS_INVAL;
	}
      break;
    case X74_SENSOR:
      start_600 = 268 - offset;
      switch (resolution)
	{
	case 75:
	  *hor_start = 0x48;
	  break;
	case 150:
	  *hor_start = 0x48;
	  break;
	case 300:
	  *hor_start = 0x4a;
	  break;
	case 600:
	  *hor_start = 0x4b;
	  break;
	default:
	  /* If we're here we have an invalid resolution */
	  return SANE_STATUS_INVAL;
	}
      break;
    }
  /* Calculate vertical start distance at 600dpi */
  switch (resolution)
    {
    case 75:
      *vert_start = start_600 / 8;
      break;
    case 150:
      *vert_start = start_600 / 4;
      break;
    case 300:
      *vert_start = start_600 / 2;
      break;
    case 600:
      *vert_start = start_600;
      break;
    case 1200:
      *vert_start = start_600 * 2;
      break;
    default:
      /* If we're here we have an invalid resolution */
      return SANE_STATUS_INVAL;
    }

  return SANE_STATUS_GOOD;
}

void
low_set_scan_area (SANE_Int res,
		   SANE_Int tlx,
		   SANE_Int tly,
		   SANE_Int brx,
		   SANE_Int bry,
		   SANE_Int offset,
		   SANE_Bool half_step,
		   SANE_Byte * regs, Lexmark_Device * dev)
{

  SANE_Int vert_start = 0;
  SANE_Int hor_start = 0;
  SANE_Int vert_end;
  SANE_Int hor_end;

  low_get_start_loc (res, &vert_start, &hor_start, offset, dev);

  /* convert pixel height to vertical location coordinates */
  vert_end = vert_start + (bry * res) / 600;
  vert_start += (tly * res) / 600;

  /* scan area size : for A920, 600 color scans are done at 1200 y dpi */
  /* this follow what was found in usb logs                            */
  if (half_step)
    {
      vert_end = vert_end * 2;
      vert_start = vert_start * 2;
    }

  /* set vertical start position registers */
  regs[0x60] = LOBYTE (vert_start);
  regs[0x61] = HIBYTE (vert_start);
  /* set vertical end position registers */
  regs[0x62] = LOBYTE (vert_end);
  regs[0x63] = HIBYTE (vert_end);

  /* convert pixel width to horizontal location coordinates */

  hor_end = hor_start + brx;
  hor_start += tlx;

  regs[0x66] = LOBYTE (hor_start);
  regs[0x67] = HIBYTE (hor_start);
  /* set horizontal end position registers */
  regs[0x6c] = LOBYTE (hor_end);
  regs[0x6d] = HIBYTE (hor_end);

  /* Debug */
  DBG (2, "low_set_scan_area: vert_start: %d (tly=%d)\n", vert_start, tly);
  DBG (2, "low_set_scan_area: vert_end: %d\n", vert_end);
  DBG (2, "low_set_scan_area: hor_start: %d\n", hor_start);
  DBG (2, "low_set_scan_area: hor_end: %d\n", hor_end);

}

SANE_Int
sanei_lexmark_low_find_start_line (Lexmark_Device * dev)
{
  /*
     This function scans forward 59 lines, reading 88 bytes per line from the
     middle of the horizontal line: pixel 0xa84 to pixel 0x9d4. It scans with
     the following parameters:
     dir=fwd 
     mode=grayscale
     h.res=300 dpi
     v.res=600 dpi   
     hor. pixels = (0xa84 - 0x9d4)/2 = 0x58 = 88
     vert. pixels = 0x3e - 0x03 = 0x3b = 59
     data = 88x59=5192=0x1448

     It assumes we are in the start dot, or just before it. We are reading
     enough lines at 600dpi to read past the dot. We return the number of
     entirely white lines read consecutively, so we know how far past the
     dot we are.

     To find the number of consecutive white lines we do the following:

     Byte swap the order of the bytes in the buffer.

     Go through the buffer finding low and high values, which are computed by 
     comparing to the weighted average: 
     weighted_average = (lowest value + (highest value - lowest value)/4)
     Low bytes are changed to 0xFF (white), higher of equal bytes are changed 
     to 0x00 (black),so that the buffer only contains white (0xFF) or black 
     (0x00) values.

     Next, we go through the buffer a line (88 bytes) at a time for 59 lines
     to read the entire buffer. For each byte in a line we check if the
     byte is black. If it is we increment the black byte counter.

     After each line we check the black byte counter. If it is greater than 0 
     we increment the black line count and set the white line count to 0. If
     there were no black bytes in the line we set the black line count to 0
     and increment the white line count.

     When all lines have been processed we return the white line count.
   */


  int blackLineCount = 0;
  int whiteLineCount = 0;
  int blackByteCounter = 0;
  unsigned char max_byte = 0;
  unsigned char min_byte = 0xFF;
  unsigned char weighted_average;
  int i, j;
#ifdef DEEP_DEBUG
  FILE *fdbg = NULL;
#endif

  SANE_Byte poll_result[3];
  SANE_Byte *buffer;
  SANE_Byte temp_byte;

  static SANE_Byte command4_block[] = { 0x90, 0x00, 0x00, 0x03 };

  static SANE_Byte command5_block[] = { 0x91, 0x00, 0x14, 0x48 };

  size_t cmd_size;
  SANE_Bool got_line;

  DBG (2, "sanei_lexmark_low_find_start_line:\n");


  /* set up registers */
  switch (dev->model.sensor_type)
    {
    case X74_SENSOR:
      dev->shadow_regs[0x2c] = 0x04;
      dev->shadow_regs[0x2d] = 0x46;
      dev->shadow_regs[0x34] = 0x05;
      dev->shadow_regs[0x35] = 0x05;
      dev->shadow_regs[0x36] = 0x0b;
      dev->shadow_regs[0x37] = 0x0b;
      dev->shadow_regs[0x38] = 0x11;
      dev->shadow_regs[0x40] = 0x40;
      rts88xx_set_gain (dev->shadow_regs, 6, 6, 6);
      break;
    case X1100_B2_SENSOR:
      dev->shadow_regs[0x2c] = 0x0f;
      dev->shadow_regs[0x2d] = 0x51;
      dev->shadow_regs[0x34] = 0x0d;
      dev->shadow_regs[0x35] = 0x0d;
      dev->shadow_regs[0x36] = 0x1d;
      dev->shadow_regs[0x37] = 0x1d;
      dev->shadow_regs[0x38] = 0x29;
      dev->shadow_regs[0x65] = 0x80;
      dev->shadow_regs[0x85] = 0x00;
      dev->shadow_regs[0x93] = 0x06;
      rts88xx_set_gain (dev->shadow_regs, 6, 6, 6);
      break;
    case X1100_2C_SENSOR:
      rts88xx_set_gain (dev->shadow_regs, 10, 10, 10);
      dev->shadow_regs[0x28] = 0xf5;
      dev->shadow_regs[0x29] = 0xf7;
      dev->shadow_regs[0x2a] = 0xf5;
      dev->shadow_regs[0x2b] = 0x17;

      dev->shadow_regs[0x2c] = 0x0d;
      dev->shadow_regs[0x2d] = 0x4f;
      dev->shadow_regs[0x31] = 0x01;
      dev->shadow_regs[0x34] = 0x11;
      dev->shadow_regs[0x35] = 0x11;
      dev->shadow_regs[0x36] = 0x21;
      dev->shadow_regs[0x37] = 0x21;
      dev->shadow_regs[0x38] = 0x31;
      dev->shadow_regs[0x72] = 0x35;
      dev->shadow_regs[0x74] = 0x4e;
      dev->shadow_regs[0x85] = 0x02;
      dev->shadow_regs[0x86] = 0x33;
      dev->shadow_regs[0x87] = 0x0f;
      dev->shadow_regs[0x88] = 0x24;
      dev->shadow_regs[0x91] = 0x19;
      dev->shadow_regs[0x92] = 0x20;
      dev->shadow_regs[0xea] = 0x00;
      dev->shadow_regs[0xeb] = 0x00;
      break;
    case A920_SENSOR:
      dev->shadow_regs[0x2c] = 0x0d;
      dev->shadow_regs[0x2d] = 0x4f;
      dev->shadow_regs[0x34] = 0x11;
      dev->shadow_regs[0x35] = 0x11;
      dev->shadow_regs[0x36] = 0x21;
      dev->shadow_regs[0x37] = 0x21;
      dev->shadow_regs[0x38] = 0x31;
      dev->shadow_regs[0x85] = 0x05;
      dev->shadow_regs[0x88] = 0x44;
      dev->shadow_regs[0x92] = 0x85;
      dev->shadow_regs[0x93] = 0x0e;
      rts88xx_set_gain (dev->shadow_regs, 6, 6, 6);
      break;
    case X1200_SENSOR:
      dev->shadow_regs[0x2c] = 0x01;
      dev->shadow_regs[0x2d] = 0x03;
      dev->shadow_regs[0x34] = 0x0d;
      dev->shadow_regs[0x35] = 0x0d;
      dev->shadow_regs[0x36] = 0x1d;
      dev->shadow_regs[0x37] = 0x1d;
      dev->shadow_regs[0x38] = 0x29;
      dev->shadow_regs[0xea] = 0x00;
      dev->shadow_regs[0xeb] = 0x00;
      dev->shadow_regs[0x40] = 0x80;
      dev->shadow_regs[0x50] = 0x00;
      dev->shadow_regs[0x81] = 0x00;
      dev->shadow_regs[0x82] = 0x00;
      dev->shadow_regs[0x85] = 0x00;
      dev->shadow_regs[0x86] = 0x00;
      dev->shadow_regs[0x87] = 0xff;
      dev->shadow_regs[0x88] = 0x02;
      dev->shadow_regs[0x92] = 0x00;
      rts88xx_set_gain (dev->shadow_regs, 10, 10, 10);
      break;
    case X1200_USB2_SENSOR:
      dev->shadow_regs[0x2c] = 0x01;
      dev->shadow_regs[0x2d] = 0x03;
      dev->shadow_regs[0x34] = 0x0d;
      dev->shadow_regs[0x35] = 0x0d;
      dev->shadow_regs[0x36] = 0x1d;
      dev->shadow_regs[0x37] = 0x1d;
      dev->shadow_regs[0x38] = 0x29;
      dev->shadow_regs[0xea] = 0x00;
      dev->shadow_regs[0xeb] = 0x00;
      rts88xx_set_gain (dev->shadow_regs, 10, 10, 10);
      break;
    }

  /* set offset to a safe value */
  rts88xx_set_offset (dev->shadow_regs, 0x80, 0x80, 0x80);
  /* set grayscale  scan  */
  dev->shadow_regs[0x2f] = 0x21;
  /* set motor resolution divisor */
  dev->shadow_regs[0x39] = 0x01;
  /* set vertical start/end positions */
  dev->shadow_regs[0x60] = 0x03;
  dev->shadow_regs[0x61] = 0x00;
  dev->shadow_regs[0x62] = 0x3e;
  dev->shadow_regs[0x63] = 0x00;
  /* set # of head moves per CIS read */
  rts88xx_set_scan_frequency (dev->shadow_regs, 1);
  /* set horizontal start position */
  dev->shadow_regs[0x66] = 0xd4;	/* 0xf2 for X1200 */
  dev->shadow_regs[0x67] = 0x09;
  /* set horizontal end position */
  dev->shadow_regs[0x6c] = 0x84;	/* 0xa2 for X1200 */
  dev->shadow_regs[0x6d] = 0x0a;
  /* set horizontal resolution */
  dev->shadow_regs[0x79] = 0x40;
  dev->shadow_regs[0x7a] = 0x02;
  /* set for ? */
  /* Motor enable & Coordinate space denominator */
  dev->shadow_regs[0xc3] = 0x81;





  switch (dev->model.motor_type)
    {
    case X1100_MOTOR:
    case A920_MOTOR:
      /* set for ? */
      dev->shadow_regs[0xc5] = 0x22;
      /* Movement direction & step size */
      dev->shadow_regs[0xc6] = 0x09;
      /* step size range2 */
      dev->shadow_regs[0xc9] = 0x3b;
      /* set for ? */
      dev->shadow_regs[0xca] = 0x1f;
      dev->shadow_regs[0xe0] = 0xf7;
      dev->shadow_regs[0xe1] = 0x16;
      /* step size range0 */
      dev->shadow_regs[0xe2] = 0x87;
      /* ? */
      dev->shadow_regs[0xe3] = 0x13;
      dev->shadow_regs[0xe4] = 0x1b;
      dev->shadow_regs[0xe5] = 0x16;
      dev->shadow_regs[0xe6] = 0xdc;
      dev->shadow_regs[0xe7] = 0x00;
      dev->shadow_regs[0xe8] = 0x00;
      dev->shadow_regs[0xe9] = 0x1b;
      dev->shadow_regs[0xec] = 0x07;
      dev->shadow_regs[0xef] = 0x03;
      break;
    case X74_MOTOR:
      dev->shadow_regs[0xc4] = 0x20;
      dev->shadow_regs[0xc5] = 0x22;
      /* Movement direction & step size */
      dev->shadow_regs[0xc6] = 0x0b;

      dev->shadow_regs[0xc8] = 0x04;
      dev->shadow_regs[0xc9] = 0x39;
      dev->shadow_regs[0xca] = 0x1f;

      /* bounds of movement range0 */
      dev->shadow_regs[0xe0] = 0x2f;
      dev->shadow_regs[0xe1] = 0x11;
      /* step size range0 */
      dev->shadow_regs[0xe2] = 0x9f;
      /* ? */
      dev->shadow_regs[0xe3] = 0x0f;
      /* bounds of movement range1 */
      dev->shadow_regs[0xe4] = 0xcb;

      dev->shadow_regs[0xe5] = 0x10;
      /* step size range1 */
      dev->shadow_regs[0xe6] = 0x64;
      /* bounds of movement range2 */
      dev->shadow_regs[0xe7] = 0x00;
      dev->shadow_regs[0xe8] = 0x00;
      /* step size range2 */
      dev->shadow_regs[0xe9] = 0x32;
      /* bounds of movement range3 */
      dev->shadow_regs[0xea] = 0x00;
      dev->shadow_regs[0xeb] = 0x00;
      /* step size range3 */
      dev->shadow_regs[0xec] = 0x0c;
      /* bounds of movement range4 -only for 75dpi grayscale */
      dev->shadow_regs[0xed] = 0x00;
      dev->shadow_regs[0xee] = 0x00;
      /* step size range4 */
      dev->shadow_regs[0xef] = 0x08;
      break;
    }


  /* Stop the scanner */
  low_stop_mvmt (dev->devnum);

  /* write regs out twice */
  dev->shadow_regs[0x32] = 0x00;
  low_write_all_regs (dev->devnum, dev->shadow_regs);
  dev->shadow_regs[0x32] = 0x40;
  low_write_all_regs (dev->devnum, dev->shadow_regs);

  /* Start Scan */
  rts88xx_commit (dev->devnum, dev->shadow_regs[0x2c]);

  /* Poll the available byte count until not 0 */
  got_line = SANE_FALSE;
  while (!got_line)
    {
      cmd_size = 4;
      low_usb_bulk_write (dev->devnum, command4_block, &cmd_size);
      cmd_size = 0x3;
      low_usb_bulk_read (dev->devnum, poll_result, &cmd_size);
      if (!
	  (poll_result[0] == 0 && poll_result[1] == 0 && poll_result[2] == 0))
	{
	  /* if result != 00 00 00 we got data */
	  got_line = SANE_TRUE;
	}
#ifdef FAKE_USB
      got_line = SANE_TRUE;
#endif
    }


  /* create buffer for scan data */
  buffer = calloc (5192, sizeof (char));
  if (buffer == NULL)
    {
      return -1;
    }

  /* Tell the scanner to send the data */
  /* Write: 91 00 14 48 */
  cmd_size = 4;
  low_usb_bulk_write (dev->devnum, command5_block, &cmd_size);
  /* Read it */
  cmd_size = 0x1448;
  low_usb_bulk_read (dev->devnum, buffer, &cmd_size);

  /* Stop the scanner */
  low_stop_mvmt (dev->devnum);


  /* Reverse order of bytes in words of buffer */
  for (i = 0; i < 5192; i = i + 2)
    {
      temp_byte = *(buffer + i);
      *(buffer + i) = *(buffer + i + 1);
      *(buffer + i + 1) = temp_byte;
    }

#ifdef DEEP_DEBUG
  fdbg = fopen ("find_start.pnm", "wb");
  if (fdbg != NULL)
    {
      fprintf (fdbg, "P5\n%d %d\n255\n", 88, 59);
      fwrite (buffer, 5192, 1, fdbg);
      fclose (fdbg);
    }
#endif

  /* Find the max and the min */
  for (i = 0; i < 5192; i++)
    {
      if (*(buffer + i) > max_byte)
	max_byte = *(buffer + i);
      if (*(buffer + i) < min_byte)
	min_byte = *(buffer + i);
    }

  weighted_average = min_byte + ((max_byte - min_byte) / 4);

  /* Set bytes as black (0x00) or white (0xFF) */
  for (i = 0; i < 5192; i++)
    {
      if (*(buffer + i) > weighted_average)
	*(buffer + i) = 0xFF;
      else
	*(buffer + i) = 0x00;
    }

#ifdef DEEP_DEBUG
  fdbg = fopen ("find_start_after.pnm", "wb");
  if (fdbg != NULL)
    {
      fprintf (fdbg, "P5\n%d %d\n255\n", 88, 59);
      fwrite (buffer, 5192, 1, fdbg);
      fclose (fdbg);
    }
#endif

  /* Go through 59 lines */
  for (j = 0; j < 59; j++)
    {
      blackByteCounter = 0;
      /* Go through 88 bytes per line */
      for (i = 0; i < 88; i++)
	{
	  /* Is byte black? */
	  if (*(buffer + (j * 88) + i) == 0)
	    {
	      blackByteCounter++;
	    }
	}			/* end for line */
      if (blackByteCounter > 0)
	{
	  /* This was a black line */
	  blackLineCount++;
	  whiteLineCount = 0;
	}
      else
	{
	  /* This is a white line */
	  whiteLineCount++;
	  blackLineCount = 0;
	}
    }				/* end for buffer */

  free (buffer);
  /* Stop the scanner. 
     This is needed to get the right distance to the scanning area */
  if (dev->model.sensor_type == X74_SENSOR)
    low_stop_mvmt (dev->devnum);

  DBG (2, "sanei_lexmark_low_find_start_line: end.\n");
  return whiteLineCount;
}


SANE_Status
sanei_lexmark_low_set_scan_regs (Lexmark_Device * dev, SANE_Int resolution,
				 SANE_Int offset, SANE_Bool calibrated)
{
  SANE_Bool isColourScan;

  DBG (2, "sanei_lexmark_low_set_scan_regs:\n");

  DBG (7, "sanei_lexmark_low_set_scan_regs: resolution=%d DPI\n", resolution);

  /* colour mode */
  if (strcmp (dev->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR) == 0)
    isColourScan = SANE_TRUE;
  else
    isColourScan = SANE_FALSE;

  /* set up registers */
  switch (dev->model.sensor_type)
    {
    case X74_SENSOR:
      dev->shadow_regs[0x2c] = 0x03;
      dev->shadow_regs[0x2d] = 0x45;
      break;
    case X1100_B2_SENSOR:
      dev->shadow_regs[0x2c] = 0x0f;
      dev->shadow_regs[0x2d] = 0x51;
      break;
    case X1100_2C_SENSOR:
      dev->shadow_regs[0x2c] = 0x0d;
      dev->shadow_regs[0x2d] = 0x4f;
      break;
    case A920_SENSOR:
      dev->shadow_regs[0x2c] = 0x0d;
      dev->shadow_regs[0x2d] = 0x4f;
      break;
    case X1200_SENSOR:
      dev->shadow_regs[0x2c] = 0x01;
      dev->shadow_regs[0x2d] = 0x03;
      break;
    case X1200_USB2_SENSOR:
      dev->shadow_regs[0x2c] = 0x01;
      dev->shadow_regs[0x2d] = 0x03;
      break;
    }

  low_set_scan_area (resolution,
		     dev->val[OPT_TL_X].w,
		     dev->val[OPT_TL_Y].w,
		     dev->val[OPT_BR_X].w,
		     dev->val[OPT_BR_Y].w,
		     offset,
		     (dev->model.motor_type == A920_MOTOR
		      || dev->model.motor_type == X74_MOTOR) && isColourScan
		     && (resolution == 600), dev->shadow_regs, dev);

  /* may be we could use a sensor descriptor that would held the max horiz dpi */
  if (dev->val[OPT_RESOLUTION].w < 600)
    dev->shadow_regs[0x7a] = 600 / dev->val[OPT_RESOLUTION].w;
  else
    dev->shadow_regs[0x7a] = 1;

  /* 75dpi x 75dpi */
  if (resolution == 75)
    {
      DBG (5, "sanei_lexmark_low_set_scan_regs(): 75 DPI resolution\n");

      if (isColourScan)
	{
	  switch (dev->model.sensor_type)
	    {
	    case X74_SENSOR:

	      dev->shadow_regs[0x34] = 0x04;
	      dev->shadow_regs[0x36] = 0x03;
	      dev->shadow_regs[0x38] = 0x03;

	      dev->shadow_regs[0x79] = 0x08;

	      dev->shadow_regs[0x80] = 0x0d;
	      dev->shadow_regs[0x81] = 0x0e;
	      dev->shadow_regs[0x82] = 0x02;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;;

	      break;

	    case X1100_B2_SENSOR:
	      dev->shadow_regs[0x34] = 0x05;
	      dev->shadow_regs[0x36] = 0x05;
	      dev->shadow_regs[0x38] = 0x05;

	      dev->shadow_regs[0x80] = 0x0c;
	      dev->shadow_regs[0x81] = 0x0c;
	      dev->shadow_regs[0x82] = 0x09;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x8c;
	      dev->shadow_regs[0x92] = 0x40;
	      dev->shadow_regs[0x93] = 0x06;
	      break;

	    case X1100_2C_SENSOR:
	      dev->shadow_regs[0x34] = 0x03;
	      dev->shadow_regs[0x36] = 0x04;
	      dev->shadow_regs[0x38] = 0x03;

	      dev->shadow_regs[0x80] = 0x00;
	      dev->shadow_regs[0x81] = 0x02;
	      dev->shadow_regs[0x82] = 0x03;

	      dev->shadow_regs[0x85] = 0x20;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;

	    case A920_SENSOR:
	      dev->shadow_regs[0x34] = 0x02;
	      dev->shadow_regs[0x36] = 0x04;
	      dev->shadow_regs[0x38] = 0x03;

	      dev->shadow_regs[0x80] = 0x07;
	      dev->shadow_regs[0x81] = 0x0f;
	      dev->shadow_regs[0x82] = 0x03;

	      dev->shadow_regs[0x85] = 0x05;
	      dev->shadow_regs[0x86] = 0x14;
	      dev->shadow_regs[0x87] = 0x06;
	      dev->shadow_regs[0x88] = 0x44;

	      dev->shadow_regs[0x91] = 0x60;
	      dev->shadow_regs[0x92] = 0x85;
	      dev->shadow_regs[0x93] = 0x0e;
	      break;

	    case X1200_SENSOR:
	      dev->shadow_regs[0x34] = 0x02;
	      dev->shadow_regs[0x36] = 0x03;
	      dev->shadow_regs[0x38] = 0x01;

	      dev->shadow_regs[0x79] = 0x20;

	      dev->shadow_regs[0x80] = 0x08;
	      dev->shadow_regs[0x81] = 0x02;
	      dev->shadow_regs[0x82] = 0x0d;

	      dev->shadow_regs[0x85] = 0x20;
	      dev->shadow_regs[0x86] = 0x1e;
	      dev->shadow_regs[0x87] = 0x39;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      /* dev->shadow_regs[0x92] = 0x92; */
	      dev->shadow_regs[0x93] = 0x06;
	      break;

	    case X1200_USB2_SENSOR:
	      dev->shadow_regs[0x34] = 0x04;
	      dev->shadow_regs[0x36] = 0x05;
	      dev->shadow_regs[0x38] = 0x04;

	      dev->shadow_regs[0x80] = 0x01;
	      dev->shadow_regs[0x81] = 0x0a;
	      dev->shadow_regs[0x82] = 0x0b;
	      break;
	    }

	  switch (dev->model.motor_type)
	    {
	    case X74_MOTOR:
	      dev->shadow_regs[0xc2] = 0x80;
	      /*  ? */
	      dev->shadow_regs[0xc4] = 0x20;
	      dev->shadow_regs[0xc5] = 0x0c;
	      dev->shadow_regs[0xc6] = 0x0b;

	      dev->shadow_regs[0xc8] = 0x04;
	      dev->shadow_regs[0xc9] = 0x39;
	      dev->shadow_regs[0xca] = 0x01;

	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0x1b;
	      dev->shadow_regs[0xe1] = 0x0a;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0x4f;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x01;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0xb3;

	      dev->shadow_regs[0xe5] = 0x09;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0x0d;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0xe5;
	      dev->shadow_regs[0xe8] = 0x02;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x05;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0xa0;
	      dev->shadow_regs[0xeb] = 0x01;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x01;
	      /* bounds of movement range4 */
	      dev->shadow_regs[0xed] = 0x00;
	      dev->shadow_regs[0xee] = 0x00;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x01;
	      break;
	    case A920_MOTOR:
	    case X1100_MOTOR:
	      /*  ? */
	      dev->shadow_regs[0xc5] = 0x0a;
	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0x2b;
	      dev->shadow_regs[0xe1] = 0x0a;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0x7f;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x01;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0xbb;
	      dev->shadow_regs[0xe5] = 0x09;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0x0e;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0x2b;
	      dev->shadow_regs[0xe8] = 0x03;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x05;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0xa0;
	      dev->shadow_regs[0xeb] = 0x01;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x01;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x01;
	      break;
	    }

	  /* set colour scan */
	  dev->shadow_regs[0x2f] = 0x11;

	  dev->shadow_regs[0x35] = 0x01;
	  dev->shadow_regs[0x37] = 0x01;
	  /* Motor enable & Coordinate space denominator */
	  dev->shadow_regs[0xc3] = 0x83;
	}
      else			/* 75 dpi gray */
	{
	  switch (dev->model.sensor_type)
	    {
	    case X74_SENSOR:

	      dev->shadow_regs[0x34] = 0x01;
	      dev->shadow_regs[0x35] = 0x01;
	      dev->shadow_regs[0x36] = 0x02;
	      dev->shadow_regs[0x37] = 0x02;
	      dev->shadow_regs[0x38] = 0x03;
	      dev->shadow_regs[0x39] = 0x0f;

	      dev->shadow_regs[0x40] = 0x80;

	      dev->shadow_regs[0x79] = 0x08;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x8c] = 0x02;
	      dev->shadow_regs[0x8d] = 0x01;
	      dev->shadow_regs[0x8e] = 0x60;
	      dev->shadow_regs[0x8f] = 0x80;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;

	      break;

	    case X1100_B2_SENSOR:
	      dev->shadow_regs[0x34] = 0x02;
	      dev->shadow_regs[0x35] = 0x02;
	      dev->shadow_regs[0x36] = 0x04;
	      dev->shadow_regs[0x37] = 0x04;
	      dev->shadow_regs[0x38] = 0x06;

	      dev->shadow_regs[0x85] = 0x20;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case X1100_2C_SENSOR:
	      dev->shadow_regs[0x34] = 0x01;
	      dev->shadow_regs[0x35] = 0x01;
	      dev->shadow_regs[0x36] = 0x02;
	      dev->shadow_regs[0x37] = 0x02;
	      dev->shadow_regs[0x38] = 0x03;	/* these are half of B2 sensor */

	      dev->shadow_regs[0x85] = 0x20;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case A920_SENSOR:
	      dev->shadow_regs[0x34] = 0x01;
	      dev->shadow_regs[0x35] = 0x01;
	      dev->shadow_regs[0x36] = 0x02;
	      dev->shadow_regs[0x37] = 0x02;
	      dev->shadow_regs[0x38] = 0x03;

	      dev->shadow_regs[0x85] = 0x0d;
	      dev->shadow_regs[0x86] = 0x14;
	      dev->shadow_regs[0x87] = 0x06;
	      dev->shadow_regs[0x88] = 0x45;

	      dev->shadow_regs[0x91] = 0x60;
	      dev->shadow_regs[0x92] = 0x8d;
	      dev->shadow_regs[0x93] = 0x0e;
	      break;
	    case X1200_SENSOR:
	      dev->shadow_regs[0x34] = 0x01;
	      dev->shadow_regs[0x35] = 0x01;
	      dev->shadow_regs[0x36] = 0x02;
	      dev->shadow_regs[0x37] = 0x02;
	      dev->shadow_regs[0x38] = 0x02;

	      dev->shadow_regs[0x79] = 0x20;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0xff;
	      dev->shadow_regs[0x88] = 0x02;

	      dev->shadow_regs[0x92] = 0x00;
	      break;
	    case X1200_USB2_SENSOR:
	      dev->shadow_regs[0x34] = 0x01;
	      dev->shadow_regs[0x35] = 0x01;
	      dev->shadow_regs[0x36] = 0x02;
	      dev->shadow_regs[0x37] = 0x02;
	      dev->shadow_regs[0x38] = 0x02;

	      dev->shadow_regs[0x79] = 0x20;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0xff;
	      dev->shadow_regs[0x88] = 0x02;

	      dev->shadow_regs[0x92] = 0x00;
	      break;
	    }
	  switch (dev->model.motor_type)
	    {
	    case X74_MOTOR:
	      /*  ? */
	      dev->shadow_regs[0xc4] = 0x20;
	      dev->shadow_regs[0xc5] = 0x0a;
	      dev->shadow_regs[0xc6] = 0x0b;

	      dev->shadow_regs[0xc8] = 0x04;
	      dev->shadow_regs[0xc9] = 0x39;
	      dev->shadow_regs[0xca] = 0x01;

	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0x07;
	      dev->shadow_regs[0xe1] = 0x18;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0xe7;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x03;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0xe7;
	      dev->shadow_regs[0xe5] = 0x14;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0x64;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0xcb;
	      dev->shadow_regs[0xe8] = 0x08;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x32;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0xe3;
	      dev->shadow_regs[0xeb] = 0x04;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x0c;
	      /* bounds of movement range4 */
	      dev->shadow_regs[0xed] = 0x00;
	      dev->shadow_regs[0xee] = 0x00;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x08;
	      break;
	    case A920_MOTOR:
	    case X1100_MOTOR:
	      /*  ? */
	      dev->shadow_regs[0xc5] = 0x10;
	      /* Movement direction & step size */
	      dev->shadow_regs[0xc6] = 0x09;

	      dev->shadow_regs[0xc9] = 0x3b;
	      dev->shadow_regs[0xca] = 0x01;
	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0x4d;
	      dev->shadow_regs[0xe1] = 0x1c;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0x71;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x02;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0x6d;
	      dev->shadow_regs[0xe5] = 0x15;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0xdc;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0xad;
	      dev->shadow_regs[0xe8] = 0x07;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x1b;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0xe1;
	      dev->shadow_regs[0xeb] = 0x03;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x07;
	      /* bounds of movement range4 -only for 75dpi grayscale */
	      dev->shadow_regs[0xed] = 0xc2;
	      dev->shadow_regs[0xee] = 0x02;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x03;
	      break;
	    }

	  /* set grayscale  scan  */
	  dev->shadow_regs[0x2f] = 0x21;

	  /* set ? only for colour? */
	  dev->shadow_regs[0x80] = 0x00;
	  dev->shadow_regs[0x81] = 0x00;
	  dev->shadow_regs[0x82] = 0x00;
	  /* Motor enable & Coordinate space denominator */
	  dev->shadow_regs[0xc3] = 0x81;

	}

      /* set motor resolution divisor */
      dev->shadow_regs[0x39] = 0x0f;

      /* set # of head moves per CIS read */
      rts88xx_set_scan_frequency (dev->shadow_regs, 1);

      /* set horizontal resolution */
      if (dev->model.sensor_type != X1200_SENSOR)
	dev->shadow_regs[0x79] = 0x08;

    }

  /* 150dpi x 150dpi */
  if (resolution == 150)
    {
      DBG (5, "sanei_lexmark_low_set_scan_regs(): 150 DPI resolution\n");

      if (isColourScan)
	{

	  switch (dev->model.sensor_type)
	    {
	    case X74_SENSOR:
	      dev->shadow_regs[0x34] = 0x08;
	      dev->shadow_regs[0x36] = 0x06;
	      dev->shadow_regs[0x38] = 0x05;
	      dev->shadow_regs[0x39] = 0x07;

	      /* resolution divisor */
	      dev->shadow_regs[0x79] = 0x08;

	      dev->shadow_regs[0x80] = 0x0a;
	      dev->shadow_regs[0x81] = 0x0c;
	      dev->shadow_regs[0x82] = 0x04;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case X1100_B2_SENSOR:
	      dev->shadow_regs[0x34] = 0x0b;
	      dev->shadow_regs[0x36] = 0x0b;
	      dev->shadow_regs[0x38] = 0x0a;

	      dev->shadow_regs[0x80] = 0x05;
	      dev->shadow_regs[0x81] = 0x05;
	      dev->shadow_regs[0x82] = 0x0a;

	      dev->shadow_regs[0x85] = 0x83;
	      dev->shadow_regs[0x86] = 0x7e;
	      dev->shadow_regs[0x87] = 0xad;
	      dev->shadow_regs[0x88] = 0x35;

	      dev->shadow_regs[0x91] = 0xfe;
	      dev->shadow_regs[0x92] = 0xdf;
	      dev->shadow_regs[0x93] = 0x0e;
	      break;
	    case X1100_2C_SENSOR:
	      dev->shadow_regs[0x34] = 0x05;
	      dev->shadow_regs[0x36] = 0x07;
	      dev->shadow_regs[0x38] = 0x05;

	      dev->shadow_regs[0x80] = 0x00;
	      dev->shadow_regs[0x81] = 0x02;
	      dev->shadow_regs[0x82] = 0x06;

	      dev->shadow_regs[0x85] = 0x20;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case A920_SENSOR:
	      dev->shadow_regs[0x34] = 0x03;
	      dev->shadow_regs[0x36] = 0x08;
	      dev->shadow_regs[0x38] = 0x05;

	      dev->shadow_regs[0x80] = 0x0e;
	      dev->shadow_regs[0x81] = 0x07;
	      dev->shadow_regs[0x82] = 0x02;

	      dev->shadow_regs[0x85] = 0x05;
	      dev->shadow_regs[0x86] = 0x14;
	      dev->shadow_regs[0x87] = 0x06;
	      dev->shadow_regs[0x88] = 0x04;

	      dev->shadow_regs[0x91] = 0xe0;
	      dev->shadow_regs[0x92] = 0x85;
	      dev->shadow_regs[0x93] = 0x0e;
	      break;
	    case X1200_SENSOR:
	      dev->shadow_regs[0x34] = 0x04;
	      dev->shadow_regs[0x36] = 0x05;
	      dev->shadow_regs[0x38] = 0x02;
	      /* data compression
	         dev->shadow_regs[0x40] = 0x90;
	         dev->shadow_regs[0x50] = 0x20; */
	      /* no data compression */
	      dev->shadow_regs[0x40] = 0x80;
	      dev->shadow_regs[0x50] = 0x00;

	      dev->shadow_regs[0x79] = 0x20;

	      dev->shadow_regs[0x80] = 0x00;
	      dev->shadow_regs[0x81] = 0x07;
	      dev->shadow_regs[0x82] = 0x0b;

	      dev->shadow_regs[0x85] = 0x20;
	      dev->shadow_regs[0x86] = 0x1e;
	      dev->shadow_regs[0x87] = 0x39;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x92] = 0x92;

	      break;
	    case X1200_USB2_SENSOR:
	      dev->shadow_regs[0x34] = 0x04;
	      dev->shadow_regs[0x36] = 0x05;
	      dev->shadow_regs[0x38] = 0x02;

	      dev->shadow_regs[0x40] = 0x80;
	      dev->shadow_regs[0x50] = 0x00;

	      dev->shadow_regs[0x79] = 0x20;

	      dev->shadow_regs[0x80] = 0x00;
	      dev->shadow_regs[0x81] = 0x07;
	      dev->shadow_regs[0x82] = 0x0b;

	      dev->shadow_regs[0x85] = 0x20;
	      dev->shadow_regs[0x86] = 0x1e;
	      dev->shadow_regs[0x87] = 0x39;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x92] = 0x92;
	      break;
	    }			/* switch */
	  switch (dev->model.motor_type)
	    {
	    case X74_MOTOR:
	      dev->shadow_regs[0xc2] = 0x80;
	      /*  ? */
	      dev->shadow_regs[0xc4] = 0x20;

	      dev->shadow_regs[0xc5] = 0x0e;
	      /* Movement direction & step size */
	      dev->shadow_regs[0xc6] = 0x0b;
	      dev->shadow_regs[0xc7] = 0x00;
	      dev->shadow_regs[0xc8] = 0x04;
	      dev->shadow_regs[0xc9] = 0x39;
	      dev->shadow_regs[0xca] = 0x03;

	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0x41;
	      dev->shadow_regs[0xe1] = 0x09;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0x89;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x02;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0x0d;

	      dev->shadow_regs[0xe5] = 0x09;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0x0d;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0xe8;
	      dev->shadow_regs[0xe8] = 0x02;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x05;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0x00;
	      dev->shadow_regs[0xeb] = 0x00;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x01;
	      /* bounds of movement range4 */
	      dev->shadow_regs[0xed] = 0x00;
	      dev->shadow_regs[0xee] = 0x00;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x01;
	      break;
	    case X1100_MOTOR:
	    case A920_MOTOR:
	      /*  ? */
	      dev->shadow_regs[0xc5] = 0x0e;
	      /*  ? */
	      dev->shadow_regs[0xc9] = 0x3a;
	      dev->shadow_regs[0xca] = 0x03;
	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0x61;
	      dev->shadow_regs[0xe1] = 0x0a;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0xed;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x02;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0x29;
	      dev->shadow_regs[0xe5] = 0x0a;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0x0e;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0x29;
	      dev->shadow_regs[0xe8] = 0x03;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x05;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0x00;
	      dev->shadow_regs[0xeb] = 0x00;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x01;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x01;
	      break;
	    }
	  /* set colour scan */
	  dev->shadow_regs[0x2f] = 0x11;

	  dev->shadow_regs[0x35] = 0x01;
	  dev->shadow_regs[0x37] = 0x01;
	  /* Motor enable & Coordinate space denominator */
	  dev->shadow_regs[0xc3] = 0x83;

	}			/* if (isColourScan) */
      else
	{
	  switch (dev->model.sensor_type)
	    {
	    case X74_SENSOR:

	      dev->shadow_regs[0x34] = 0x02;
	      dev->shadow_regs[0x35] = 0x02;
	      dev->shadow_regs[0x36] = 0x04;
	      dev->shadow_regs[0x37] = 0x04;
	      dev->shadow_regs[0x38] = 0x06;
	      dev->shadow_regs[0x39] = 0x07;
	      /* Motor enable & Coordinate space denominator */
	      dev->shadow_regs[0x40] = 0x40;

	      /* resolution divisor */
	      dev->shadow_regs[0x79] = 0x08;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case X1100_B2_SENSOR:
	      dev->shadow_regs[0x34] = 0x04;
	      dev->shadow_regs[0x35] = 0x04;
	      dev->shadow_regs[0x36] = 0x07;
	      dev->shadow_regs[0x37] = 0x07;
	      dev->shadow_regs[0x38] = 0x0a;


	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case X1100_2C_SENSOR:
	      dev->shadow_regs[0x34] = 0x02;
	      dev->shadow_regs[0x35] = 0x02;
	      dev->shadow_regs[0x36] = 0x04;
	      dev->shadow_regs[0x37] = 0x04;
	      dev->shadow_regs[0x38] = 0x05;

	      dev->shadow_regs[0x85] = 0x20;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case A920_SENSOR:
	      dev->shadow_regs[0x34] = 0x02;
	      dev->shadow_regs[0x35] = 0x02;
	      dev->shadow_regs[0x36] = 0x04;
	      dev->shadow_regs[0x37] = 0x04;
	      dev->shadow_regs[0x38] = 0x05;

	      dev->shadow_regs[0x85] = 0x0d;
	      dev->shadow_regs[0x86] = 0x14;
	      dev->shadow_regs[0x87] = 0x06;
	      dev->shadow_regs[0x88] = 0x45;

	      dev->shadow_regs[0x91] = 0x60;
	      dev->shadow_regs[0x92] = 0x8d;
	      dev->shadow_regs[0x93] = 0x0e;
	      break;
	    case X1200_SENSOR:
	      dev->shadow_regs[0x34] = 0x01;
	      dev->shadow_regs[0x35] = 0x01;
	      dev->shadow_regs[0x36] = 0x02;
	      dev->shadow_regs[0x37] = 0x02;
	      dev->shadow_regs[0x38] = 0x03;

	      /* dev->shadow_regs[0x40] = 0x90;
	         dev->shadow_regs[0x50] = 0x20; */
	      /* no data compression */
	      dev->shadow_regs[0x40] = 0x80;
	      dev->shadow_regs[0x50] = 0x00;

	      dev->shadow_regs[0x79] = 0x20;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0xff;
	      dev->shadow_regs[0x88] = 0x02;

	      dev->shadow_regs[0x92] = 0x92;
	      break;
	    case X1200_USB2_SENSOR:
	      dev->shadow_regs[0x34] = 0x01;
	      dev->shadow_regs[0x35] = 0x01;
	      dev->shadow_regs[0x36] = 0x02;
	      dev->shadow_regs[0x37] = 0x02;
	      dev->shadow_regs[0x38] = 0x03;

	      dev->shadow_regs[0x40] = 0x80;
	      dev->shadow_regs[0x50] = 0x00;

	      dev->shadow_regs[0x79] = 0x20;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0xff;
	      dev->shadow_regs[0x88] = 0x02;

	      dev->shadow_regs[0x92] = 0x92;
	      break;
	    }			/* switch */
	  switch (dev->model.motor_type)
	    {
	    case X74_MOTOR:
	      /*  ? */
	      dev->shadow_regs[0xc4] = 0x20;
	      dev->shadow_regs[0xc5] = 0x14;
	      /* Movement direction & step size */
	      dev->shadow_regs[0xc6] = 0x0b;

	      dev->shadow_regs[0xc8] = 0x04;
	      dev->shadow_regs[0xc9] = 0x39;
	      dev->shadow_regs[0xca] = 0x01;

	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0x09;
	      dev->shadow_regs[0xe1] = 0x18;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0xe9;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x03;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0x79;

	      dev->shadow_regs[0xe5] = 0x16;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0x64;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0xcd;
	      dev->shadow_regs[0xe8] = 0x08;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x32;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0xe5;
	      dev->shadow_regs[0xeb] = 0x04;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x0c;
	      /* bounds of movement range4 */
	      dev->shadow_regs[0xed] = 0x00;
	      dev->shadow_regs[0xee] = 0x00;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x08;
	      break;
	    case X1100_MOTOR:
	    case A920_MOTOR:
	      /*  ? */
	      dev->shadow_regs[0xc5] = 0x16;
	      /* Movement direction & step size */
	      dev->shadow_regs[0xc6] = 0x09;
	      /*  ? */
	      dev->shadow_regs[0xc9] = 0x3b;
	      dev->shadow_regs[0xca] = 0x01;
	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0xdd;
	      dev->shadow_regs[0xe1] = 0x18;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0x01;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x03;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0x6d;
	      dev->shadow_regs[0xe5] = 0x15;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0xdc;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0xad;
	      dev->shadow_regs[0xe8] = 0x07;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x1b;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0xe1;
	      dev->shadow_regs[0xeb] = 0x03;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x07;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x03;
	      break;
	    }

	  /* set grayscale  scan */
	  dev->shadow_regs[0x2f] = 0x21;
	  /* set motor resolution divisor */
	  dev->shadow_regs[0x39] = 0x07;
	  /* set ? only for colour? */
	  dev->shadow_regs[0x80] = 0x00;
	  dev->shadow_regs[0x81] = 0x00;
	  dev->shadow_regs[0x82] = 0x00;

	  /* Motor enable & Coordinate space denominator */
	  dev->shadow_regs[0xc3] = 0x81;
	}			/* else (greyscale) */




      /* set # of head moves per CIS read */
      rts88xx_set_scan_frequency (dev->shadow_regs, 1);

      /* hum, horizontal resolution different for X1200 ? */
      /* if (dev->model.sensor_type != X1200_SENSOR)
         dev->shadow_regs[0x79] = 0x20; */

    }

  /*300dpi x 300dpi */
  if (resolution == 300)
    {
      DBG (5, "sanei_lexmark_low_set_scan_regs(): 300 DPI resolution\n");

      if (isColourScan)
	{

	  switch (dev->model.sensor_type)
	    {
	    case X74_SENSOR:
	      dev->shadow_regs[0x34] = 0x08;
	      dev->shadow_regs[0x36] = 0x06;
	      dev->shadow_regs[0x38] = 0x05;
	      dev->shadow_regs[0x39] = 0x07;

	      dev->shadow_regs[0x80] = 0x08;
	      dev->shadow_regs[0x81] = 0x0a;
	      dev->shadow_regs[0x82] = 0x03;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case X1100_B2_SENSOR:
	      dev->shadow_regs[0x34] = 0x15;
	      dev->shadow_regs[0x36] = 0x15;
	      dev->shadow_regs[0x38] = 0x14;
	      /* set motor resolution divisor */
	      dev->shadow_regs[0x39] = 0x03;

	      dev->shadow_regs[0x80] = 0x0a;
	      dev->shadow_regs[0x81] = 0x0a;
	      dev->shadow_regs[0x82] = 0x06;

	      dev->shadow_regs[0x85] = 0x83;
	      dev->shadow_regs[0x86] = 0x7e;
	      dev->shadow_regs[0x87] = 0xad;
	      dev->shadow_regs[0x88] = 0x35;

	      dev->shadow_regs[0x91] = 0xfe;
	      dev->shadow_regs[0x92] = 0xdf;
	      dev->shadow_regs[0x93] = 0x0e;
	      break;
	    case X1100_2C_SENSOR:
	      dev->shadow_regs[0x34] = 0x08;
	      dev->shadow_regs[0x36] = 0x0d;
	      dev->shadow_regs[0x38] = 0x09;
	      /* set motor resolution divisor */
	      dev->shadow_regs[0x39] = 0x03;

	      dev->shadow_regs[0x80] = 0x0e;
	      dev->shadow_regs[0x81] = 0x04;
	      dev->shadow_regs[0x82] = 0x0a;

	      dev->shadow_regs[0x85] = 0x20;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case A920_SENSOR:
	      dev->shadow_regs[0x34] = 0x06;
	      dev->shadow_regs[0x36] = 0x10;
	      dev->shadow_regs[0x38] = 0x09;
	      /* set motor resolution divisor */
	      dev->shadow_regs[0x39] = 0x03;

	      dev->shadow_regs[0x80] = 0x0c;
	      dev->shadow_regs[0x81] = 0x02;
	      dev->shadow_regs[0x82] = 0x04;

	      dev->shadow_regs[0x85] = 0x05;
	      dev->shadow_regs[0x86] = 0x14;
	      dev->shadow_regs[0x87] = 0x06;
	      dev->shadow_regs[0x88] = 0x04;

	      dev->shadow_regs[0x91] = 0xe0;
	      dev->shadow_regs[0x92] = 0x85;
	      dev->shadow_regs[0x93] = 0x0e;
	      break;
	    case X1200_SENSOR:
	      dev->shadow_regs[0x34] = 0x07;
	      dev->shadow_regs[0x36] = 0x09;
	      dev->shadow_regs[0x38] = 0x04;
	      /* set motor resolution divisor */
	      dev->shadow_regs[0x39] = 0x03;

	      /* data compression 
	         dev->shadow_regs[0x40] = 0x90;
	         dev->shadow_regs[0x50] = 0x20; */
	      /* no data compression */
	      dev->shadow_regs[0x40] = 0x80;
	      dev->shadow_regs[0x50] = 0x00;

	      dev->shadow_regs[0x80] = 0x00;
	      dev->shadow_regs[0x81] = 0x0e;
	      dev->shadow_regs[0x82] = 0x06;
	      break;
	    case X1200_USB2_SENSOR:
	      dev->shadow_regs[0x34] = 0x07;
	      dev->shadow_regs[0x36] = 0x09;
	      dev->shadow_regs[0x38] = 0x04;
	      /* set motor resolution divisor */
	      dev->shadow_regs[0x39] = 0x03;

	      dev->shadow_regs[0x40] = 0x80;
	      dev->shadow_regs[0x50] = 0x00;

	      dev->shadow_regs[0x80] = 0x00;
	      dev->shadow_regs[0x81] = 0x0e;
	      dev->shadow_regs[0x82] = 0x06;
	      break;
	    }
	  switch (dev->model.motor_type)
	    {
	    case X74_MOTOR:
	      /*  ? */
	      dev->shadow_regs[0xc4] = 0x20;
	      dev->shadow_regs[0xc5] = 0x12;
	      /* Movement direction & step size */
	      dev->shadow_regs[0xc6] = 0x09;

	      dev->shadow_regs[0xc8] = 0x04;
	      dev->shadow_regs[0xc9] = 0x39;
	      dev->shadow_regs[0xca] = 0x0f;

	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0x5d;
	      dev->shadow_regs[0xe1] = 0x05;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0xed;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x02;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0x29;
	      dev->shadow_regs[0xe5] = 0x05;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0x0d;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0x00;
	      dev->shadow_regs[0xe8] = 0x00;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x05;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0x00;
	      dev->shadow_regs[0xeb] = 0x00;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x01;
	      /* bounds of movement range4 -only for 75dpi grayscale */
	      dev->shadow_regs[0xed] = 0x00;
	      dev->shadow_regs[0xee] = 0x00;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x01;
	      break;
	    case A920_MOTOR:
	    case X1100_MOTOR:
	      /*  ? */
	      dev->shadow_regs[0xc5] = 0x17;
	      /* Movement direction & step size */
	      dev->shadow_regs[0xc6] = 0x09;
	      /*  ? */
	      dev->shadow_regs[0xc9] = 0x3a;
	      dev->shadow_regs[0xca] = 0x0a;
	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0x75;
	      dev->shadow_regs[0xe1] = 0x0a;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0xdd;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x05;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0x59;
	      dev->shadow_regs[0xe5] = 0x0a;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0x0e;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0x00;
	      dev->shadow_regs[0xe8] = 0x00;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x05;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0x00;
	      dev->shadow_regs[0xeb] = 0x00;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x01;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x01;
	      break;
	    }

	  dev->shadow_regs[0x35] = 0x01;
	  dev->shadow_regs[0x37] = 0x01;

	  /* set colour scan */
	  dev->shadow_regs[0x2f] = 0x11;

	  /* Motor enable & Coordinate space denominator */
	  dev->shadow_regs[0xc3] = 0x83;

	}
      else			/* greyscale */
	{

	  switch (dev->model.sensor_type)
	    {
	    case X74_SENSOR:
	      dev->shadow_regs[0x34] = 0x04;
	      dev->shadow_regs[0x35] = 0x04;
	      dev->shadow_regs[0x36] = 0x08;
	      dev->shadow_regs[0x37] = 0x08;
	      dev->shadow_regs[0x38] = 0x0c;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case X1100_B2_SENSOR:
	      dev->shadow_regs[0x34] = 0x08;
	      dev->shadow_regs[0x35] = 0x08;
	      dev->shadow_regs[0x36] = 0x0f;
	      dev->shadow_regs[0x37] = 0x0f;
	      dev->shadow_regs[0x38] = 0x16;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case X1100_2C_SENSOR:
	      dev->shadow_regs[0x34] = 0x04;
	      dev->shadow_regs[0x35] = 0x04;
	      dev->shadow_regs[0x36] = 0x07;
	      dev->shadow_regs[0x37] = 0x07;
	      dev->shadow_regs[0x38] = 0x0a;

	      dev->shadow_regs[0x85] = 0x20;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case A920_SENSOR:
	      dev->shadow_regs[0x34] = 0x03;
	      dev->shadow_regs[0x35] = 0x03;
	      dev->shadow_regs[0x36] = 0x06;
	      dev->shadow_regs[0x37] = 0x06;
	      dev->shadow_regs[0x38] = 0x09;

	      dev->shadow_regs[0x85] = 0x05;
	      dev->shadow_regs[0x86] = 0x14;
	      dev->shadow_regs[0x87] = 0x06;
	      dev->shadow_regs[0x88] = 0x04;

	      dev->shadow_regs[0x91] = 0xe0;
	      dev->shadow_regs[0x92] = 0x85;
	      dev->shadow_regs[0x93] = 0x0e;
	      break;
	    case X1200_SENSOR:
	      dev->shadow_regs[0x34] = 0x02;
	      dev->shadow_regs[0x35] = 0x02;
	      dev->shadow_regs[0x36] = 0x04;
	      dev->shadow_regs[0x37] = 0x04;
	      dev->shadow_regs[0x38] = 0x06;
	      break;
	    case X1200_USB2_SENSOR:
	      dev->shadow_regs[0x34] = 0x02;
	      dev->shadow_regs[0x35] = 0x02;
	      dev->shadow_regs[0x36] = 0x04;
	      dev->shadow_regs[0x37] = 0x04;
	      dev->shadow_regs[0x38] = 0x06;
	      break;
	    }
	  switch (dev->model.motor_type)
	    {
	    case X74_MOTOR:
	      /*  ? */
	      dev->shadow_regs[0xc4] = 0x20;
	      dev->shadow_regs[0xc5] = 0x1c;
	      /* Movement direction & step size */
	      dev->shadow_regs[0xc6] = 0x0b;

	      dev->shadow_regs[0xc8] = 0x04;
	      dev->shadow_regs[0xc9] = 0x39;
	      dev->shadow_regs[0xca] = 0x05;

	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0x29;
	      dev->shadow_regs[0xe1] = 0x17;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0x8f;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x06;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0x61;

	      dev->shadow_regs[0xe5] = 0x16;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0x64;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0xb5;
	      dev->shadow_regs[0xe8] = 0x08;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x32;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0x00;
	      dev->shadow_regs[0xeb] = 0x00;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x0c;
	      /* bounds of movement range4 -only for 75dpi grayscale */
	      dev->shadow_regs[0xed] = 0x00;
	      dev->shadow_regs[0xee] = 0x00;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x08;
	      break;
	    case A920_MOTOR:
	    case X1100_MOTOR:
	      /*  ? */
	      dev->shadow_regs[0xc5] = 0x19;
	      /* Movement direction & step size */
	      dev->shadow_regs[0xc6] = 0x09;
	      /*  ? */
	      dev->shadow_regs[0xc9] = 0x3a;
	      dev->shadow_regs[0xca] = 0x08;
	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0xe3;
	      dev->shadow_regs[0xe1] = 0x18;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0x03;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x06;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0x2b;
	      dev->shadow_regs[0xe5] = 0x17;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0xdc;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0xb3;
	      dev->shadow_regs[0xe8] = 0x07;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x1b;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0x00;
	      dev->shadow_regs[0xeb] = 0x00;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x07;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x03;
	      break;
	    }			/* switch motortype */
	  /* set grayscale  scan */
	  dev->shadow_regs[0x2f] = 0x21;
	  /* set motor resolution divisor */
	  dev->shadow_regs[0x39] = 0x03;

	  /* set ? only for colour? */
	  dev->shadow_regs[0x80] = 0x00;
	  dev->shadow_regs[0x81] = 0x00;
	  dev->shadow_regs[0x82] = 0x00;
	  /* Motor enable & Coordinate space denominator */
	  dev->shadow_regs[0xc3] = 0x81;


	}			/* else (gray) */

      /* set # of head moves per CIS read */
      rts88xx_set_scan_frequency (dev->shadow_regs, 1);
      /* set horizontal resolution */
      dev->shadow_regs[0x79] = 0x20;
    }

  /* 600dpi x 600dpi */
  if (resolution == 600)
    {
      DBG (5, "sanei_lexmark_low_set_scan_regs(): 600 DPI resolution\n");



      if (isColourScan)
	{
	  /* 600 dpi color doesn't work for X74 yet */
	  if (dev->model.sensor_type == X74_SENSOR)
	    return SANE_STATUS_INVAL;

	  switch (dev->model.sensor_type)
	    {
	    case X74_SENSOR:
	      dev->shadow_regs[0x34] = 0x10;
	      dev->shadow_regs[0x35] = 0x01;
	      dev->shadow_regs[0x36] = 0x0c;
	      dev->shadow_regs[0x37] = 0x01;
	      dev->shadow_regs[0x38] = 0x09;

	      dev->shadow_regs[0x80] = 0x02;
	      dev->shadow_regs[0x81] = 0x08;
	      dev->shadow_regs[0x82] = 0x08;


	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;

	      /*dev->shadow_regs[0x34] = 0x08;
	         dev->shadow_regs[0x35] = 0x01;
	         dev->shadow_regs[0x36] = 0x06;
	         dev->shadow_regs[0x37] = 0x01;
	         dev->shadow_regs[0x38] = 0x05;


	         dev->shadow_regs[0x80] = 0x09;
	         dev->shadow_regs[0x81] = 0x0c;
	         dev->shadow_regs[0x82] = 0x04;


	         dev->shadow_regs[0x85] = 0x00;
	         dev->shadow_regs[0x86] = 0x00;
	         dev->shadow_regs[0x87] = 0x00;
	         dev->shadow_regs[0x88] = 0x00;

	         dev->shadow_regs[0x91] = 0x00;
	         dev->shadow_regs[0x92] = 0x00;
	         dev->shadow_regs[0x93] = 0x06;
	         break; */



	    case X1100_B2_SENSOR:
	      dev->shadow_regs[0x34] = 0x15;
	      dev->shadow_regs[0x36] = 0x15;
	      dev->shadow_regs[0x38] = 0x14;

	      dev->shadow_regs[0x80] = 0x02;
	      dev->shadow_regs[0x81] = 0x02;
	      dev->shadow_regs[0x82] = 0x08;

	      dev->shadow_regs[0x85] = 0x83;
	      dev->shadow_regs[0x86] = 0x7e;
	      dev->shadow_regs[0x87] = 0xad;
	      dev->shadow_regs[0x88] = 0x35;

	      dev->shadow_regs[0x91] = 0xfe;
	      dev->shadow_regs[0x92] = 0xdf;
	      dev->shadow_regs[0x93] = 0x0e;
	      break;
	    case X1100_2C_SENSOR:
	      dev->shadow_regs[0x34] = 0x08;
	      dev->shadow_regs[0x36] = 0x0d;
	      dev->shadow_regs[0x38] = 0x09;

	      dev->shadow_regs[0x80] = 0x0e;
	      dev->shadow_regs[0x81] = 0x02;
	      dev->shadow_regs[0x82] = 0x0a;

	      dev->shadow_regs[0x85] = 0x20;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case A920_SENSOR:
	      dev->shadow_regs[0x34] = 0x06;
	      dev->shadow_regs[0x36] = 0x0f;
	      dev->shadow_regs[0x38] = 0x09;

	      dev->shadow_regs[0x79] = 0x40;

	      dev->shadow_regs[0x80] = 0x0e;
	      dev->shadow_regs[0x81] = 0x0e;
	      dev->shadow_regs[0x82] = 0x00;

	      dev->shadow_regs[0x85] = 0x05;
	      dev->shadow_regs[0x86] = 0x14;
	      dev->shadow_regs[0x87] = 0x06;
	      dev->shadow_regs[0x88] = 0x04;

	      dev->shadow_regs[0x91] = 0x60;
	      dev->shadow_regs[0x92] = 0x85;
	      dev->shadow_regs[0x93] = 0x0e;
	      break;
	    case X1200_SENSOR:
	      dev->shadow_regs[0x34] = 0x07;
	      dev->shadow_regs[0x36] = 0x0a;
	      dev->shadow_regs[0x38] = 0x04;

	      /* data compression 
	         dev->shadow_regs[0x40] = 0x90;
	         dev->shadow_regs[0x50] = 0x20; */

	      /* no data compression */
	      dev->shadow_regs[0x40] = 0x80;
	      dev->shadow_regs[0x50] = 0x00;

	      dev->shadow_regs[0x80] = 0x02;
	      dev->shadow_regs[0x81] = 0x00;
	      dev->shadow_regs[0x82] = 0x06;
	      break;
	    case X1200_USB2_SENSOR:
	      dev->shadow_regs[0x34] = 0x0d;
	      dev->shadow_regs[0x36] = 0x13;
	      dev->shadow_regs[0x38] = 0x10;

	      dev->shadow_regs[0x80] = 0x04;
	      dev->shadow_regs[0x81] = 0x0e;
	      dev->shadow_regs[0x82] = 0x08;

	      dev->shadow_regs[0x85] = 0x02;
	      dev->shadow_regs[0x86] = 0x3b;
	      dev->shadow_regs[0x87] = 0x0f;
	      dev->shadow_regs[0x88] = 0x24;

	      dev->shadow_regs[0x91] = 0x19;
	      dev->shadow_regs[0x92] = 0x30;
	      dev->shadow_regs[0x93] = 0x0e;
	      dev->shadow_regs[0xc5] = 0x17;
	      dev->shadow_regs[0xc6] = 0x09;
	      dev->shadow_regs[0xca] = 0x0a;
	      break;
	    }
	  switch (dev->model.motor_type)
	    {
	    case X74_MOTOR:
	      /* Motor enable & Coordinate space denominator */
	      dev->shadow_regs[0xc3] = 0x81;
	      /*  ? */
	      dev->shadow_regs[0xc4] = 0x20;
	      dev->shadow_regs[0xc5] = 0x21;
	      /* Movement direction & step size */
	      dev->shadow_regs[0xc6] = 0x09;
	      dev->shadow_regs[0xc8] = 0x04;
	      dev->shadow_regs[0xc9] = 0x39;
	      dev->shadow_regs[0xca] = 0x20;
	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0x00;
	      dev->shadow_regs[0xe1] = 0x00;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0xbf;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x05;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0x00;
	      dev->shadow_regs[0xe5] = 0x00;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0x0d;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0x00;
	      dev->shadow_regs[0xe8] = 0x00;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x05;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0x00;
	      dev->shadow_regs[0xeb] = 0x00;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x01;
	      /* bounds of movement range4 -only for 75dpi grayscale */
	      dev->shadow_regs[0xed] = 0x00;
	      dev->shadow_regs[0xee] = 0x00;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x01;
	      break;
	    case A920_MOTOR:
	    case X1100_MOTOR:
	      /* Motor enable & Coordinate space denominator */
	      dev->shadow_regs[0xc3] = 0x86;
	      /*  ? */
	      dev->shadow_regs[0xc5] = 0x27;
	      /* Movement direction & step size */
	      dev->shadow_regs[0xc6] = 0x0c;
	      /*  ? */
	      dev->shadow_regs[0xc9] = 0x3a;
	      dev->shadow_regs[0xca] = 0x1a;
	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0x57;
	      dev->shadow_regs[0xe1] = 0x0a;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0xbf;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x05;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0x3b;
	      dev->shadow_regs[0xe5] = 0x0a;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0x0e;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0x00;
	      dev->shadow_regs[0xe8] = 0x00;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x05;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0x00;
	      dev->shadow_regs[0xeb] = 0x00;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x01;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x01;
	      break;
	    }
	  /* set colour scan */
	  dev->shadow_regs[0x2f] = 0x11;

	  dev->shadow_regs[0x35] = 0x01;
	  dev->shadow_regs[0x37] = 0x01;

	  /* set motor resolution divisor */
	  dev->shadow_regs[0x39] = 0x03;
	  /* set # of head moves per CIS read */
	  rts88xx_set_scan_frequency (dev->shadow_regs, 2);


	}
      else
	{
	  switch (dev->model.sensor_type)
	    {
	    case X74_SENSOR:
	      dev->shadow_regs[0x2c] = 0x04;
	      dev->shadow_regs[0x2d] = 0x46;
	      dev->shadow_regs[0x34] = 0x05;
	      dev->shadow_regs[0x35] = 0x05;
	      dev->shadow_regs[0x36] = 0x0b;
	      dev->shadow_regs[0x37] = 0x0b;
	      dev->shadow_regs[0x38] = 0x11;
	      dev->shadow_regs[0x40] = 0x40;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case X1100_B2_SENSOR:
	      dev->shadow_regs[0x34] = 0x11;
	      dev->shadow_regs[0x35] = 0x11;
	      dev->shadow_regs[0x36] = 0x21;
	      dev->shadow_regs[0x37] = 0x21;
	      dev->shadow_regs[0x38] = 0x31;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case X1100_2C_SENSOR:
	      dev->shadow_regs[0x34] = 0x07;
	      dev->shadow_regs[0x35] = 0x07;
	      dev->shadow_regs[0x36] = 0x0d;
	      dev->shadow_regs[0x37] = 0x0d;
	      dev->shadow_regs[0x38] = 0x13;

	      dev->shadow_regs[0x85] = 0x20;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0x00;
	      dev->shadow_regs[0x88] = 0x00;

	      dev->shadow_regs[0x91] = 0x00;
	      dev->shadow_regs[0x92] = 0x00;
	      dev->shadow_regs[0x93] = 0x06;
	      break;
	    case A920_SENSOR:
	      dev->shadow_regs[0x34] = 0x05;
	      dev->shadow_regs[0x35] = 0x05;
	      dev->shadow_regs[0x36] = 0x0b;
	      dev->shadow_regs[0x37] = 0x0b;
	      dev->shadow_regs[0x38] = 0x11;

	      dev->shadow_regs[0x85] = 0x05;
	      dev->shadow_regs[0x86] = 0x14;
	      dev->shadow_regs[0x87] = 0x06;
	      dev->shadow_regs[0x88] = 0x04;

	      dev->shadow_regs[0x91] = 0xe0;
	      dev->shadow_regs[0x92] = 0x85;
	      dev->shadow_regs[0x93] = 0x0e;
	      break;
	    case X1200_SENSOR:
	      dev->shadow_regs[0x34] = 0x03;
	      dev->shadow_regs[0x35] = 0x03;
	      dev->shadow_regs[0x36] = 0x07;
	      dev->shadow_regs[0x37] = 0x07;
	      dev->shadow_regs[0x38] = 0x0b;

	      /* data compression 
	         dev->shadow_regs[0x40] = 0x90; 
	         dev->shadow_regs[0x50] = 0x20; */
	      /* no data compression */
	      dev->shadow_regs[0x40] = 0x80;
	      dev->shadow_regs[0x50] = 0x00;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0xff;
	      dev->shadow_regs[0x88] = 0x02;

	      dev->shadow_regs[0x92] = 0x00;

	      break;
	    case X1200_USB2_SENSOR:
	      dev->shadow_regs[0x34] = 0x03;
	      dev->shadow_regs[0x35] = 0x03;
	      dev->shadow_regs[0x36] = 0x07;
	      dev->shadow_regs[0x37] = 0x07;
	      dev->shadow_regs[0x38] = 0x0b;

	      dev->shadow_regs[0x40] = 0x80;
	      dev->shadow_regs[0x50] = 0x00;

	      dev->shadow_regs[0x85] = 0x00;
	      dev->shadow_regs[0x86] = 0x00;
	      dev->shadow_regs[0x87] = 0xff;
	      dev->shadow_regs[0x88] = 0x02;

	      dev->shadow_regs[0x92] = 0x00;
	      break;
	    }
	  switch (dev->model.motor_type)
	    {
	    case X74_MOTOR:
	      /* set # of head moves per CIS read */
	      rts88xx_set_scan_frequency (dev->shadow_regs, 1);
	      /*  ? */
	      dev->shadow_regs[0xc4] = 0x20;
	      dev->shadow_regs[0xc5] = 0x22;
	      /* Movement direction & step size */
	      dev->shadow_regs[0xc6] = 0x0b;

	      dev->shadow_regs[0xc8] = 0x04;
	      dev->shadow_regs[0xc9] = 0x39;
	      dev->shadow_regs[0xca] = 0x1f;

	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0x2f;
	      dev->shadow_regs[0xe1] = 0x11;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0x9f;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x0f;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0xcb;

	      dev->shadow_regs[0xe5] = 0x10;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0x64;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0x00;
	      dev->shadow_regs[0xe8] = 0x00;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x32;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0x00;
	      dev->shadow_regs[0xeb] = 0x00;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x0c;
	      /* bounds of movement range4 -only for 75dpi grayscale */
	      dev->shadow_regs[0xed] = 0x00;
	      dev->shadow_regs[0xee] = 0x00;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x08;
	      break;
	    case X1100_MOTOR:
	    case A920_MOTOR:
	      /* set ? only for colour? */
	      dev->shadow_regs[0x80] = 0x00;
	      dev->shadow_regs[0x81] = 0x00;
	      dev->shadow_regs[0x82] = 0x00;
	      /*  ? */
	      dev->shadow_regs[0xc5] = 0x22;
	      /* Movement direction & step size */
	      dev->shadow_regs[0xc6] = 0x09;
	      /*  ? */
	      dev->shadow_regs[0xc9] = 0x3b;
	      dev->shadow_regs[0xca] = 0x1f;
	      /* bounds of movement range0 */
	      dev->shadow_regs[0xe0] = 0xf7;
	      dev->shadow_regs[0xe1] = 0x16;
	      /* step size range0 */
	      dev->shadow_regs[0xe2] = 0x87;
	      /* ? */
	      dev->shadow_regs[0xe3] = 0x13;
	      /* bounds of movement range1 */
	      dev->shadow_regs[0xe4] = 0x1b;
	      dev->shadow_regs[0xe5] = 0x16;
	      /* step size range1 */
	      dev->shadow_regs[0xe6] = 0xdc;
	      /* bounds of movement range2 */
	      dev->shadow_regs[0xe7] = 0x00;
	      dev->shadow_regs[0xe8] = 0x00;
	      /* step size range2 */
	      dev->shadow_regs[0xe9] = 0x1b;
	      /* bounds of movement range3 */
	      dev->shadow_regs[0xea] = 0x00;
	      dev->shadow_regs[0xeb] = 0x00;
	      /* step size range3 */
	      dev->shadow_regs[0xec] = 0x07;
	      /* step size range4 */
	      dev->shadow_regs[0xef] = 0x03;
	      break;
	    }

	  /* set grayscale  scan */
	  dev->shadow_regs[0x2f] = 0x21;

	  /* set motor resolution divisor */
	  dev->shadow_regs[0x39] = 0x01;

	  /* set # of head moves per CIS read */
	  rts88xx_set_scan_frequency (dev->shadow_regs, 1);

	  /* Motor enable & Coordinate space denominator */
	  dev->shadow_regs[0xc3] = 0x81;
	}			/* else (grayscale) */

      /* set horizontal resolution */
      dev->shadow_regs[0x79] = 0x40;

    }
  /*600dpi x 1200dpi */
  if (resolution == 1200)
    {
      DBG (5, "sanei_lexmark_low_set_scan_regs(): 1200 DPI resolution\n");

      /* 1200 dpi doesn't work for X74 yet */
      if (dev->model.sensor_type == X74_SENSOR)
	return SANE_STATUS_INVAL;

      if (isColourScan)
	{
	  /* set colour scan */
	  dev->shadow_regs[0x2f] = 0x11;
	  /* set motor resolution divisor */
	  dev->shadow_regs[0x39] = 0x01;
	  /* set # of head moves per CIS read */
	  rts88xx_set_scan_frequency (dev->shadow_regs, 2);

	  if (dev->model.sensor_type == X1100_B2_SENSOR)
	    {
	      /* set ? */
	      dev->shadow_regs[0x34] = 0x29;
	      dev->shadow_regs[0x36] = 0x29;
	      dev->shadow_regs[0x38] = 0x28;
	      /* set ? */
	      dev->shadow_regs[0x80] = 0x04;
	      dev->shadow_regs[0x81] = 0x04;
	      dev->shadow_regs[0x82] = 0x08;
	      dev->shadow_regs[0x85] = 0x83;
	      dev->shadow_regs[0x86] = 0x7e;
	      dev->shadow_regs[0x87] = 0xad;
	      dev->shadow_regs[0x88] = 0x35;
	      dev->shadow_regs[0x91] = 0xfe;
	      dev->shadow_regs[0x92] = 0xdf;
	    }
	  else
	    {			/* A920 case */
	      dev->shadow_regs[0x34] = 0x0c;
	      dev->shadow_regs[0x36] = 0x1e;
	      dev->shadow_regs[0x38] = 0x10;

	      dev->shadow_regs[0x80] = 0x0c;
	      dev->shadow_regs[0x81] = 0x08;
	      dev->shadow_regs[0x82] = 0x0c;

	      dev->shadow_regs[0x85] = 0x05;
	      dev->shadow_regs[0x86] = 0x14;
	      dev->shadow_regs[0x87] = 0x06;
	      dev->shadow_regs[0x88] = 0x04;

	      dev->shadow_regs[0x91] = 0x60;
	      dev->shadow_regs[0x92] = 0x85;
	    }

	  dev->shadow_regs[0x35] = 0x01;
	  dev->shadow_regs[0x37] = 0x01;
	  /* set motor resolution divisor */
	  dev->shadow_regs[0x39] = 0x01;
	  dev->shadow_regs[0x93] = 0x0e;

	  /* Motor enable & Coordinate space denominator */
	  dev->shadow_regs[0xc3] = 0x86;
	  /*  ? */
	  dev->shadow_regs[0xc5] = 0x41;
	  /* Movement direction & step size */
	  dev->shadow_regs[0xc6] = 0x0c;
	  /*  ? */
	  dev->shadow_regs[0xc9] = 0x3a;
	  dev->shadow_regs[0xca] = 0x40;
	  /* bounds of movement range0 */
	  dev->shadow_regs[0xe0] = 0x00;
	  dev->shadow_regs[0xe1] = 0x00;
	  /* step size range0 */
	  dev->shadow_regs[0xe2] = 0x85;
	  /* ? */
	  dev->shadow_regs[0xe3] = 0x0b;
	  /* bounds of movement range1 */
	  dev->shadow_regs[0xe4] = 0x00;
	  dev->shadow_regs[0xe5] = 0x00;
	  /* step size range1 */
	  dev->shadow_regs[0xe6] = 0x0e;
	  /* bounds of movement range2 */
	  dev->shadow_regs[0xe7] = 0x00;
	  dev->shadow_regs[0xe8] = 0x00;
	  /* step size range2 */
	  dev->shadow_regs[0xe9] = 0x05;
	  /* bounds of movement range3 */
	  dev->shadow_regs[0xea] = 0x00;
	  dev->shadow_regs[0xeb] = 0x00;
	  /* step size range3 */
	  dev->shadow_regs[0xec] = 0x01;
	  /* step size range4 */
	  dev->shadow_regs[0xef] = 0x01;
	}
      else
	{
	  /* set grayscale  scan */
	  dev->shadow_regs[0x2f] = 0x21;
	  /* set ? */
	  dev->shadow_regs[0x34] = 0x22;
	  dev->shadow_regs[0x35] = 0x22;
	  dev->shadow_regs[0x36] = 0x42;
	  dev->shadow_regs[0x37] = 0x42;
	  dev->shadow_regs[0x38] = 0x62;
	  /* set motor resolution divisor */
	  dev->shadow_regs[0x39] = 0x01;
	  /* set # of head moves per CIS read */
	  rts88xx_set_scan_frequency (dev->shadow_regs, 0);

	  /* set ? only for colour? */
	  dev->shadow_regs[0x80] = 0x00;
	  dev->shadow_regs[0x81] = 0x00;
	  dev->shadow_regs[0x82] = 0x00;
	  dev->shadow_regs[0x85] = 0x00;
	  dev->shadow_regs[0x86] = 0x00;
	  dev->shadow_regs[0x87] = 0x00;
	  dev->shadow_regs[0x88] = 0x00;
	  dev->shadow_regs[0x91] = 0x00;
	  dev->shadow_regs[0x92] = 0x00;
	  dev->shadow_regs[0x93] = 0x06;
	  /* Motor enable & Coordinate space denominator */
	  dev->shadow_regs[0xc3] = 0x81;
	  /*  ? */
	  dev->shadow_regs[0xc5] = 0x41;
	  /* Movement direction & step size */
	  dev->shadow_regs[0xc6] = 0x09;
	  /*  ? */
	  dev->shadow_regs[0xc9] = 0x3a;
	  dev->shadow_regs[0xca] = 0x40;
	  /* bounds of movement range0 */
	  dev->shadow_regs[0xe0] = 0x00;
	  dev->shadow_regs[0xe1] = 0x00;
	  /* step size range0 */
	  dev->shadow_regs[0xe2] = 0xc7;
	  /* ? */
	  dev->shadow_regs[0xe3] = 0x29;
	  /* bounds of movement range1 */
	  dev->shadow_regs[0xe4] = 0x00;
	  dev->shadow_regs[0xe5] = 0x00;
	  /* step size range1 */
	  dev->shadow_regs[0xe6] = 0xdc;
	  /* bounds of movement range2 */
	  dev->shadow_regs[0xe7] = 0x00;
	  dev->shadow_regs[0xe8] = 0x00;
	  /* step size range2 */
	  dev->shadow_regs[0xe9] = 0x1b;
	  /* bounds of movement range3 */
	  dev->shadow_regs[0xea] = 0x00;
	  dev->shadow_regs[0xeb] = 0x00;
	  /* step size range3 */
	  dev->shadow_regs[0xec] = 0x07;
	  /* step size range4 */
	  dev->shadow_regs[0xef] = 0x03;
	}

      /* set horizontal resolution */
      dev->shadow_regs[0x79] = 0x40;
    }

  /* is calibration has been done, we override fixed settings with detected ones */
  if (calibrated)
    {
      /* override fixed values with ones from calibration */
      if (rts88xx_is_color (dev->shadow_regs))
	{
	  rts88xx_set_offset (dev->shadow_regs,
			      dev->offset.red,
			      dev->offset.green, dev->offset.blue);
	  rts88xx_set_gain (dev->shadow_regs,
			    dev->gain.red, dev->gain.green, dev->gain.blue);
	}
      else
	{
	  rts88xx_set_offset (dev->shadow_regs,
			      dev->offset.gray,
			      dev->offset.gray, dev->offset.gray);
	  rts88xx_set_gain (dev->shadow_regs,
			    dev->gain.gray, dev->gain.gray, dev->gain.gray);
	}
    }
  DBG (2, "sanei_lexmark_low_set_scan_regs: end.\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_lexmark_low_start_scan (Lexmark_Device * dev)
{
  SANE_Int devnum;

  static SANE_Byte command4_block[] = { 0x90, 0x00, 0x00, 0x03 };

  static SANE_Byte command5_block[] = { 0x80, 0xb3, 0x00, 0x01 };

  SANE_Byte poll_result[3];
  SANE_Byte read_result;
  SANE_Bool scan_head_moving;
  size_t size;

  devnum = dev->devnum;

  dev->transfer_buffer = NULL;	/* No data xferred yet */
  DBG (2, "sanei_lexmark_low_start_scan:\n");


  /* 80 b3 00 01  - poll for scanner not moving */
  scan_head_moving = SANE_TRUE;
  while (scan_head_moving)
    {
      size = 4;
      low_usb_bulk_write (devnum, command5_block, &size);
      size = 0x1;
      low_usb_bulk_read (devnum, &read_result, &size);
      if ((read_result & 0xF) == 0x0)
	{
	  scan_head_moving = SANE_FALSE;
	}
      /* F.O. Should be a timeout here so we don't hang if something breaks */
#ifdef FAKE_USB
      scan_head_moving = SANE_FALSE;
#endif
    }

  /* Clear C6 */
  low_clr_c6 (devnum);
  /* Stop the scanner */
  low_stop_mvmt (devnum);

  /*Set regs x2 */
  dev->shadow_regs[0x32] = 0x00;
  low_write_all_regs (devnum, dev->shadow_regs);
  dev->shadow_regs[0x32] = 0x40;
  low_write_all_regs (devnum, dev->shadow_regs);

  /* Start Scan */
  rts88xx_commit (devnum, dev->shadow_regs[0x2c]);

  /* We start with 0 bytes remaining to be read */
  dev->bytes_remaining = 0;
  /* and 0 bytes in the transfer buffer */
  dev->bytes_in_buffer = 0;
  dev->bytes_read = 0;

  /* Poll the available byte count until not 0 */
  while (1)
    {
      size = 4;
      low_usb_bulk_write (devnum, command4_block, &size);
      size = 0x3;
      low_usb_bulk_read (devnum, poll_result, &size);
      if (!
	  (poll_result[0] == 0 && poll_result[1] == 0 && poll_result[2] == 0))
	{
	  /* if result != 00 00 00 we got data */

	  /* data_size should be used to set bytes_remaining */
	  /* data_size is set from sane_get_parameters () */
	  dev->bytes_remaining = dev->data_size;
	  /* Initialize the read buffer */
	  read_buffer_init (dev, dev->params.bytes_per_line);
	  return SANE_STATUS_GOOD;

	}
      size = 4;
      /* I'm not sure why the Windows driver does this - probably a timeout? */
      low_usb_bulk_write (devnum, command5_block, &size);
      size = 0x1;
      low_usb_bulk_read (devnum, &read_result, &size);
      if (read_result != 0x68)
	{
	  dev->bytes_remaining = 0;
	  return SANE_STATUS_IO_ERROR;
	}
    }

  DBG (2, "sanei_lexmark_low_start_scan: end.\n");
  return SANE_STATUS_GOOD;
}

long
sanei_lexmark_low_read_scan_data (SANE_Byte * data, SANE_Int size,
				  Lexmark_Device * dev)
{
  SANE_Bool isColourScan, isGrayScan;
  static SANE_Byte command1_block[] = { 0x91, 0x00, 0xff, 0xc0 };
  size_t cmd_size, xfer_request;
  long bytes_read;
  SANE_Bool even_byte;
  SANE_Status status;
  int i, k, val;

  DBG (2, "sanei_lexmark_low_read_scan_data:\n");

  /* colour mode */
  isGrayScan = SANE_FALSE;
  if (strcmp (dev->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR) == 0)
    isColourScan = SANE_TRUE;
  else
    {
      isColourScan = SANE_FALSE;
      /* grayscale  mode */
      if (strcmp (dev->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_GRAY) == 0)
	isGrayScan = SANE_TRUE;
    }

  /* Check if we have a transfer buffer. Create one and fill it if we don't */
  if (dev->transfer_buffer == NULL)
    {
      if (dev->bytes_remaining > 0)
	{
	  if (dev->bytes_remaining > MAX_XFER_SIZE)
	    xfer_request = MAX_XFER_SIZE;
	  else
	    xfer_request = dev->bytes_remaining;

	  command1_block[2] = (SANE_Byte) (xfer_request >> 8);
	  command1_block[3] = (SANE_Byte) (xfer_request & 0xFF);

	  /* wait for data */
	  status = low_poll_data (dev->devnum);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (1,
		   "sanei_lexmark_low_read_scan_data: time-out while waiting for data.\n");
	      return status;
	    }

	  /* Create buffer to hold the amount we will request */
	  dev->transfer_buffer = (SANE_Byte *) malloc (MAX_XFER_SIZE);
	  if (dev->transfer_buffer == NULL)
	    return SANE_STATUS_NO_MEM;

	  /* Fill it */
	  /* Write: 91 00 (xfer_size) */
	  cmd_size = 4;
	  low_usb_bulk_write (dev->devnum, command1_block, &cmd_size);

	  /* Read: xfer_size  bytes */
	  cmd_size = xfer_request;
	  low_usb_bulk_read (dev->devnum, dev->transfer_buffer, &cmd_size);

	  /* apply shading coefficients */
	  k = dev->bytes_read % dev->read_buffer->linesize;
	  for (i = 0; i < (int) cmd_size; i++)
	    {
	      val = dev->transfer_buffer[i];
	      val = (int) ((float) val * dev->shading_coeff[k] + 0.5);
	      if (val > 255)
		val = 255;
	      dev->transfer_buffer[i] = val;
	      k++;
	      if ((size_t) k == dev->read_buffer->linesize)
		k = 0;
	    }

	  /* advance by the amount actually read from device */
	  dev->bytes_read += cmd_size;
	  dev->bytes_remaining -= cmd_size;
	  dev->bytes_in_buffer = cmd_size;
	  dev->read_pointer = dev->transfer_buffer;
	  DBG (2, "sanei_lexmark_low_read_scan_data:\n");
	  DBG (2, "   Filled a buffer from the scanner\n");
	  DBG (2, "   bytes_remaining: %lu\n", (u_long) dev->bytes_remaining);
	  DBG (2, "   bytes_in_buffer: %lu\n", (u_long) dev->bytes_in_buffer);
	  DBG (2, "   read_pointer: %p\n", dev->read_pointer);
	}
    }

  DBG (5, "READ BUFFER INFO: \n");
  DBG (5, "   write ptr:     %p\n", dev->read_buffer->writeptr);
  DBG (5, "   read ptr:      %p\n", dev->read_buffer->readptr);
  DBG (5, "   max write ptr: %p\n", dev->read_buffer->max_writeptr);
  DBG (5, "   buffer size:   %lu\n", (u_long) dev->read_buffer->size);
  DBG (5, "   line size:     %lu\n", (u_long) dev->read_buffer->linesize);
  DBG (5, "   empty:         %d\n", dev->read_buffer->empty);
  DBG (5, "   line no:       %d\n", dev->read_buffer->image_line_no);


  /* If there is space in the read buffer, copy the transfer buffer over */
  if (read_buffer_bytes_available (dev->read_buffer) >= dev->bytes_in_buffer)
    {
      even_byte = SANE_TRUE;
      while (dev->bytes_in_buffer)
	{

	  /* Colour Scan */
	  if (isColourScan)
	    {
	      if (even_byte)
		read_buffer_add_byte (dev->read_buffer,
				      dev->read_pointer + 1);
	      else
		read_buffer_add_byte (dev->read_buffer,
				      dev->read_pointer - 1);
	      even_byte = !even_byte;
	    }
	  /* Gray Scan */
	  else if (isGrayScan)
	    {
	      if (even_byte)
		read_buffer_add_byte_gray (dev->read_buffer,
					   dev->read_pointer + 1);
	      else
		read_buffer_add_byte_gray (dev->read_buffer,
					   dev->read_pointer - 1);
	      even_byte = !even_byte;
	    }
	  /* Lineart Scan */
	  else
	    {
	      if (even_byte)
		read_buffer_add_bit_lineart (dev->read_buffer,
					     dev->read_pointer + 1,
					     dev->threshold);
	      else
		read_buffer_add_bit_lineart (dev->read_buffer,
					     dev->read_pointer - 1,
					     dev->threshold);
	      even_byte = !even_byte;
	    }
	  dev->read_pointer = dev->read_pointer + sizeof (SANE_Byte);
	  dev->bytes_in_buffer--;
	}
      /* free the transfer buffer */
      free (dev->transfer_buffer);
      dev->transfer_buffer = NULL;
    }

  DBG (5, "READ BUFFER INFO: \n");
  DBG (5, "   write ptr:     %p\n", dev->read_buffer->writeptr);
  DBG (5, "   read ptr:      %p\n", dev->read_buffer->readptr);
  DBG (5, "   max write ptr: %p\n", dev->read_buffer->max_writeptr);
  DBG (5, "   buffer size:   %lu\n", (u_long) dev->read_buffer->size);
  DBG (5, "   line size:     %lu\n", (u_long) dev->read_buffer->linesize);
  DBG (5, "   empty:         %d\n", dev->read_buffer->empty);
  DBG (5, "   line no:       %d\n", dev->read_buffer->image_line_no);

  /* Read blocks out of read buffer */
  bytes_read = read_buffer_get_bytes (dev->read_buffer, data, size);

  DBG (2, "sanei_lexmark_low_read_scan_data:\n");
  DBG (2, "    Copying lines from buffer to data\n");
  DBG (2, "    bytes_remaining: %lu\n", (u_long) dev->bytes_remaining);
  DBG (2, "    bytes_in_buffer: %lu\n", (u_long) dev->bytes_in_buffer);
  DBG (2, "    read_pointer: %p\n", dev->read_buffer->readptr);
  DBG (2, "    bytes_read %lu\n", (u_long) bytes_read);

  /* if no more bytes to xfer and read buffer empty we're at the end */
  if ((dev->bytes_remaining == 0) && read_buffer_is_empty (dev->read_buffer))
    {
      if (!dev->eof)
	{
	  DBG (2,
	       "sanei_lexmark_low_read_scan_data: EOF- parking the scanner\n");
	  dev->eof = SANE_TRUE;
	  low_rewind (dev, dev->shadow_regs);
	}
      else
	{
	  DBG (2, "ERROR: Why are we trying to set eof more than once?\n");
	}
    }

  DBG (2, "sanei_lexmark_low_read_scan_data: end.\n");
  return bytes_read;
}

void
low_rewind (Lexmark_Device * dev, SANE_Byte * regs)
{
  SANE_Int new_location;
  SANE_Int location;
  SANE_Int scale;

  DBG (2, "low_rewind: \n");

  /* We rewind at 1200dpi resolution. We rely on content of shadow registers
     to compute the number of lines at 1200 dpi to go back */

  /* first move to start of scanning area */
  scale = 600 / dev->val[OPT_RESOLUTION].w;
  new_location = ((dev->val[OPT_BR_Y].w / scale) * scale) * 2;

  /* then add distance to go to the "origin dot" */
  if (rts88xx_is_color (regs))
    new_location += 400;
  else
    new_location += 420;

  if (dev->model.sensor_type == X74_SENSOR)
    new_location += 150;


  location = new_location - 1;
  DBG (2, "low_rewind: %d=>new_location=%d\n", dev->val[OPT_BR_Y].w,
       new_location);

  /* stops any pending scan */
  low_clr_c6 (dev->devnum);
  low_cancel (dev->devnum);

  /* set regs for rewind */
  regs[0x2f] = 0xa1;
  regs[0x32] = 0x00;
  regs[0x39] = 0x00;

  /* all other regs are always the same. these ones change with parameters */
  /* the following 4 regs are the location 61,60 and the location+1 63,62 */

  regs[0x60] = LOBYTE (location);
  regs[0x61] = HIBYTE (location);
  regs[0x62] = LOBYTE (new_location);
  regs[0x63] = HIBYTE (new_location);

  switch (dev->model.motor_type)
    {
    case X74_MOTOR:
      regs[0xc3] = 0x81;
      regs[0xc6] = 0x03;
      regs[0xc9] = 0x39;
      regs[0xe0] = 0x81;
      regs[0xe1] = 0x16;
      regs[0xe2] = 0xe1;
      regs[0xe3] = 0x04;
      regs[0xe4] = 0xe7;
      regs[0xe5] = 0x14;
      regs[0xe6] = 0x64;
      regs[0xe7] = 0xd5;
      regs[0xe8] = 0x08;
      regs[0xe9] = 0x32;
      regs[0xea] = 0xed;
      regs[0xeb] = 0x04;
      regs[0xec] = 0x0c;
      regs[0xef] = 0x08;
      break;
    case X1100_MOTOR:
    case A920_MOTOR:
      /* set regs for rewind */
      regs[0x79] = 0x40;
      regs[0xb2] = 0x04;
      regs[0xc3] = 0x81;
      regs[0xc6] = 0x01;
      regs[0xc9] = 0x3b;
      regs[0xe0] = 0x2b;
      regs[0xe1] = 0x17;
      regs[0xe2] = 0xe7;
      regs[0xe3] = 0x03;
      regs[0xe6] = 0xdc;
      regs[0xe7] = 0xb3;
      regs[0xe8] = 0x07;
      regs[0xe9] = 0x1b;
      regs[0xea] = 0x00;
      regs[0xeb] = 0x00;
      regs[0xec] = 0x07;
      regs[0xef] = 0x03;
      break;
    }


  /* starts scan */
  low_start_scan (dev->devnum, regs);
  DBG (2, "low_rewind: end.\n");
}


SANE_Status
read_buffer_init (Lexmark_Device * dev, int bytesperline)
{
  size_t no_lines_in_buffer;

  DBG (2, "read_buffer_init: Start\n");

  dev->read_buffer = (Read_Buffer *) malloc (sizeof (Read_Buffer));
  if (dev->read_buffer == NULL)
    return SANE_STATUS_NO_MEM;
  dev->read_buffer->linesize = bytesperline;
  dev->read_buffer->gray_offset = 0;
  dev->read_buffer->max_gray_offset = bytesperline - 1;
  dev->read_buffer->region = RED;
  dev->read_buffer->red_offset = 0;
  dev->read_buffer->green_offset = 1;
  dev->read_buffer->blue_offset = 2;
  dev->read_buffer->max_red_offset = bytesperline - 3;
  dev->read_buffer->max_green_offset = bytesperline - 2;
  dev->read_buffer->max_blue_offset = bytesperline - 1;
  no_lines_in_buffer = 3 * MAX_XFER_SIZE / bytesperline;
  dev->read_buffer->size = bytesperline * no_lines_in_buffer;
  dev->read_buffer->data = (SANE_Byte *) malloc (dev->read_buffer->size);
  if (dev->read_buffer->data == NULL)
    return SANE_STATUS_NO_MEM;
  dev->read_buffer->readptr = dev->read_buffer->data;
  dev->read_buffer->writeptr = dev->read_buffer->data;
  dev->read_buffer->max_writeptr = dev->read_buffer->data +
    (no_lines_in_buffer - 1) * bytesperline;
  dev->read_buffer->empty = SANE_TRUE;
  dev->read_buffer->image_line_no = 0;
  dev->read_buffer->bit_counter = 0;
  dev->read_buffer->max_lineart_offset = dev->params.pixels_per_line - 1;
  return SANE_STATUS_GOOD;
}

SANE_Status
read_buffer_free (Read_Buffer * read_buffer)
{
  DBG (2, "read_buffer_free:\n");
  if (read_buffer)
    {
      free (read_buffer->data);
      free (read_buffer);
      read_buffer = NULL;
    }
  return SANE_STATUS_GOOD;
}

size_t
read_buffer_bytes_available (Read_Buffer * rb)
{

  DBG (2, "read_buffer_bytes_available:\n");

  if (rb->empty)
    return rb->size;
  else if ((size_t) abs (rb->writeptr - rb->readptr) < rb->linesize)
    return 0;			/* ptrs are less than one line apart */
  else if (rb->writeptr < rb->readptr)
    return (rb->readptr - rb->writeptr - rb->linesize);
  else
    return (rb->size + rb->readptr - rb->writeptr - rb->linesize);
}

SANE_Status
read_buffer_add_byte (Read_Buffer * rb, SANE_Byte * byte_pointer)
{

  /* DBG(2, "read_buffer_add_byte:\n"); */
  /* F.O. Need to fix the endian byte ordering here */

  switch (rb->region)
    {
    case RED:
      *(rb->writeptr + rb->red_offset) = *byte_pointer;
      if (rb->red_offset == rb->max_red_offset)
	{
	  rb->red_offset = 0;
	  rb->region = GREEN;
	}
      else
	rb->red_offset = rb->red_offset + (3 * sizeof (SANE_Byte));
      return SANE_STATUS_GOOD;
    case GREEN:
      *(rb->writeptr + rb->green_offset) = *byte_pointer;
      if (rb->green_offset == rb->max_green_offset)
	{
	  rb->green_offset = 1;
	  rb->region = BLUE;
	}
      else
	rb->green_offset = rb->green_offset + (3 * sizeof (SANE_Byte));
      return SANE_STATUS_GOOD;
    case BLUE:
      *(rb->writeptr + rb->blue_offset) = *byte_pointer;
      if (rb->blue_offset == rb->max_blue_offset)
	{
	  rb->image_line_no++;
	  /* finished a line. read_buffer no longer empty */
	  rb->empty = SANE_FALSE;
	  rb->blue_offset = 2;
	  rb->region = RED;
	  if (rb->writeptr == rb->max_writeptr)
	    rb->writeptr = rb->data;	/* back to beginning of buffer */
	  else
	    rb->writeptr = rb->writeptr + rb->linesize;	/* next line */
	}
      else
	rb->blue_offset = rb->blue_offset + (3 * sizeof (SANE_Byte));
      return SANE_STATUS_GOOD;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
read_buffer_add_byte_gray (Read_Buffer * rb, SANE_Byte * byte_pointer)
{

  /*  DBG(2, "read_buffer_add_byte_gray:\n"); */

  *(rb->writeptr + rb->gray_offset) = *byte_pointer;

  if (rb->gray_offset == rb->max_gray_offset)
    {
      rb->image_line_no++;
      /* finished a line. read_buffer no longer empty */
      rb->empty = SANE_FALSE;
      rb->gray_offset = 0;

      if (rb->writeptr == rb->max_writeptr)
	rb->writeptr = rb->data;	/* back to beginning of buffer */
      else
	rb->writeptr = rb->writeptr + rb->linesize;	/* next line */
    }
  else
    rb->gray_offset = rb->gray_offset + (1 * sizeof (SANE_Byte));
  return SANE_STATUS_GOOD;
}

SANE_Status
read_buffer_add_bit_lineart (Read_Buffer * rb, SANE_Byte * byte_pointer,
			     SANE_Byte threshold)
{
  SANE_Byte tmpByte;
  SANE_Byte *currentBytePtr;
  SANE_Int bitIndex;

  /*  DBG(2, "read_buffer_add_bit_lineart:\n"); */

  /* threshold = 0x80;  */
  tmpByte = 0;
  /* Create a bit by comparing incoming byte to threshold */
  if (*byte_pointer <= threshold)
    {
      tmpByte = 128;
    }

  /* Calculate the bit index in the current byte */
  bitIndex = rb->bit_counter % 8;
  /* Move the bit to its correct position in the temporary byte */
  tmpByte = tmpByte >> bitIndex;
  /* Get the pointer to the current byte */
  currentBytePtr = rb->writeptr + rb->gray_offset;

  /* If this is the first write to this byte, clear the byte */
  if (bitIndex == 0)
    *currentBytePtr = 0;
  /* Set the value of the bit in the current byte */
  *currentBytePtr = *currentBytePtr | tmpByte;

  /* last bit in the line? */
  if (rb->bit_counter == rb->max_lineart_offset)
    {
      /* Check if we're at the last byte of the line - error if not */
      if (rb->gray_offset != rb->max_gray_offset)
	{
	  DBG (5, "read_buffer_add_bit_lineart:\n");
	  DBG (5, "  Last bit of line is not last byte.\n");
	  DBG (5, "  Bit Index: %d, Byte Index: %d. \n", rb->bit_counter,
	       rb->max_gray_offset);
	  return SANE_STATUS_INVAL;
	}
      rb->image_line_no++;
      /* line finished read_buffer no longer empty */
      rb->empty = SANE_FALSE;
      rb->gray_offset = 0;
      /* are we at the last line in the read buffer ? */
      if (rb->writeptr == rb->max_writeptr)
	rb->writeptr = rb->data;	/* back to beginning of buffer */
      else
	rb->writeptr = rb->writeptr + rb->linesize;	/* next line */
      /* clear the bit counter */
      rb->bit_counter = 0;
    }
  /* last bit in the byte? */
  else if (bitIndex == 7)
    {
      /* Not at the end of the line, but byte done. Increment byte offset */
      rb->gray_offset = rb->gray_offset + (1 * sizeof (SANE_Byte));
      /* increment bit counter */
      rb->bit_counter++;
    }
  else
    {
      /* else increment bit counter */
      rb->bit_counter++;
    }

  return SANE_STATUS_GOOD;
}


size_t
read_buffer_get_bytes (Read_Buffer * rb, SANE_Byte * buffer, size_t rqst_size)
{
  /* Read_Buffer *rb; */
  size_t available_bytes;

  /* rb = read_buffer; */
  if (rb->empty)
    return 0;
  else if (rb->writeptr > rb->readptr)
    {
      available_bytes = rb->writeptr - rb->readptr;
      if (available_bytes <= rqst_size)
	{
	  /* We can read from the read pointer up to the write pointer */
	  memcpy (buffer, rb->readptr, available_bytes);
	  rb->readptr = rb->writeptr;
	  rb->empty = SANE_TRUE;
	  return available_bytes;
	}
      else
	{
	  /* We can read from the full request size */
	  memcpy (buffer, rb->readptr, rqst_size);
	  rb->readptr = rb->readptr + rqst_size;
	  return rqst_size;
	}
    }
  else
    {
      /* The read pointer is ahead of the write pointer. Its wrapped around. */
      /* We can read to the end of the buffer and make a recursive call to */
      /* read any available lines at the beginning of the buffer */
      available_bytes = rb->data + rb->size - rb->readptr;
      if (available_bytes <= rqst_size)
	{
	  /* We can read from the read pointer up to the end of the buffer */
	  memcpy (buffer, rb->readptr, available_bytes);
	  rb->readptr = rb->data;
	  if (rb->writeptr == rb->readptr)
	    rb->empty = SANE_TRUE;
	  return available_bytes +
	    read_buffer_get_bytes (rb, buffer + available_bytes,
				   rqst_size - available_bytes);
	}
      else
	{
	  /* We can read from the full request size */
	  memcpy (buffer, rb->readptr, rqst_size);
	  rb->readptr = rb->readptr + rqst_size;
	  return rqst_size;
	}
    }
}

SANE_Bool
read_buffer_is_empty (Read_Buffer * read_buffer)
{
  return read_buffer->empty;
}

/* 
 * average a width*height rgb/monochrome area
 * return values in given pointers
 */
static int
average_area (SANE_Byte * regs, SANE_Byte * data, int width, int height,
	      int *ra, int *ga, int *ba)
{
  int x, y;
  int global = 0;
  int rc, gc, bc;

  *ra = 0;
  *ga = 0;
  *ba = 0;
  rc = 0;
  gc = 0;
  bc = 0;
  if (rts88xx_is_color (regs))
    {
      for (x = 0; x < width; x++)
	for (y = 0; y < height; y++)
	  {
	    rc += data[3 * width * y + x];
	    gc += data[3 * width * y + width + x];
	    bc += data[3 * width * y + 2 * width + x];
	  }
      global = (rc + gc + bc) / (3 * width * height);
      *ra = rc / (width * height);
      *ga = gc / (width * height);
      *ba = bc / (width * height);
    }
  else
    {
      for (x = 0; x < width; x++)
	for (y = 0; y < height; y++)
	  {
	    gc += data[width * y + x];
	  }
      global = gc / (width * height);
      *ga = gc / (width * height);
    }
  DBG (7, "average_area: global=%d, red=%d, green=%d, blue=%d\n", global, *ra,
       *ga, *ba);
  return global;
}

/**
 * we scan a dark area with gain minimum to detect offset
 */
SANE_Status
sanei_lexmark_low_offset_calibration (Lexmark_Device * dev)
{
  SANE_Byte regs[255];		/* we have our own copy of shadow registers */
  SANE_Status status = SANE_STATUS_GOOD;
  int i, lines = 8, yoffset = 2;
  int pixels;
  int failed = 0;
  /* offsets */
  int ro = 0, go = 0, bo = 0;
  /* averages */
  int ra, ga, ba, average;
  SANE_Byte *data = NULL;
  SANE_Byte top[OFFSET_RANGES] = { 0, 0x7f, 0x9f, 0xbf, 0xff };
#ifdef DEEP_DEBUG
  char title[20];
#endif

  DBG (2, "sanei_lexmark_low_offset_calibration: start\n");
  /* copy registers */
  for (i = 0; i < 255; i++)
    regs[i] = dev->shadow_regs[i];

  /* we clear movement bit */
  regs[0xc3] = regs[0xc3] & 0x7f;

  pixels =
    (dev->sensor->offset_endx - dev->sensor->offset_startx) / regs[0x7a];

  /* there are 4 ranges of offset:
     00-7F : almost no offset
     80-9F : high noise
     A0-BF : high noise
     C0-FF : high noise
     we start with the highest one and decrease until
     overall offset is ok
     First loop may have such an high offset that scanned data overflow
     and gives a low average. So we allways skip its results
   */

  /* minimal gains */
  DBG (3,
       "sanei_lexmark_low_offset_calibration: setting gains to (1,1,1).\n");
  rts88xx_set_gain (regs, 1, 1, 1);

  i = OFFSET_RANGES;
  average = 255;

  /* loop on ranges until one fits. Then adjust offset, first loop is
   * always done. TODO detect overflow by 'noise looking' data pattern */
  while (((i > 0) && (average > dev->sensor->offset_threshold))
	 || (i == OFFSET_RANGES))
    {
      /* next range  */
      i--;

      /* sets to top of range */
      ro = top[i];
      go = top[i];
      bo = top[i];
      rts88xx_set_offset (regs, ro, ro, ro);
      DBG (3,
	   "sanei_lexmark_low_offset_calibration: setting offsets to (%d,%d,%d).\n",
	   ro, ro, ro);

      status =
	low_simple_scan (dev, regs, dev->sensor->offset_startx, pixels,
			 yoffset, lines, &data);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1,
	       "sanei_lexmark_low_offset_calibration: low_simple_scan failed!\n");
	  if (data != NULL)
	    free (data);
	  return status;
	}
#ifdef DEEP_DEBUG
      sprintf (title, "offset%02x.pnm", ro);
      write_pnm_file (title, pixels, lines, rts88xx_is_color (regs), data);
#endif
      average = average_area (regs, data, pixels, lines, &ra, &ga, &ba);
      free (data);
    }
  if (i == 0)
    {
      DBG (2, "sanei_lexmark_low_offset_calibration: failed !\n");
      failed = 1;
    }

  /* increase gain and scan again */
  /* increase gain for fine offset tuning */
  rts88xx_set_gain (regs, 6, 6, 6);
  status =
    low_simple_scan (dev, regs, dev->sensor->offset_startx, pixels, yoffset,
		     lines, &data);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1,
	   "sanei_lexmark_low_offset_calibration: low_simple_scan failed!\n");
      if (data != NULL)
	free (data);
      return status;
    }
  average_area (regs, data, pixels, lines, &ra, &ga, &ba);
#ifdef DEEP_DEBUG
  write_pnm_file ("offset-final.pnm", pixels, lines, rts88xx_is_color (regs),
		  data);
#endif

  /* this "law" is a guess, may (should?) be changed ... */
  if (!failed)
    {
      if (ro > ra)
	dev->offset.red = ro - ra;
      if (go > ga)
	{
	  dev->offset.green = go - ga;
	  dev->offset.gray = go - ga;
	}
      if (bo > ba)
	dev->offset.blue = bo - ba;
    }
  else
    {
      dev->offset.red = dev->sensor->offset_fallback;
      dev->offset.green = dev->sensor->offset_fallback;
      dev->offset.blue = dev->sensor->offset_fallback;
    }
  DBG (7,
       "sanei_lexmark_low_offset_calibration: offset=(0x%02x,0x%02x,0x%02x).\n",
       dev->offset.red, dev->offset.green, dev->offset.blue);

  DBG (2, "sanei_lexmark_low_offset_calibration: end.\n");
  free (data);
  return status;
}

/*
 * we scan a white area until average is good enough
 * level is good enough when it maximize the range value of output:
 * ie max-min is maximum
 */
SANE_Status
sanei_lexmark_low_gain_calibration (Lexmark_Device * dev)
{
  SANE_Byte regs[255];		/* we have our own copy of shadow registers */
  SANE_Status status = SANE_STATUS_GOOD;
  int i, lines = 4, yoffset = 1;
  int sx, ex;
  int pixels;
  /* averages */
  int ra, ga, ba;
  SANE_Byte *data = NULL;
  int red, green, blue;
#ifdef DEEP_DEBUG
  char title[20];
#endif

  DBG (2, "sanei_lexmark_low_gain_calibration: start\n");
  /* copy registers */
  for (i = 0; i < 255; i++)
    regs[i] = dev->shadow_regs[i];

  /* we clear movement bit */
  regs[0xc3] = regs[0xc3] & 0x7f;
  sx = regs[0x67] * 256 + regs[0x66];
  ex = regs[0x6d] * 256 + regs[0x6c];
  pixels = (ex - sx) / regs[0x7a];


  /* set up inital gains */
  red = 6;
  green = 6;
  blue = 6;
  rts88xx_set_gain (regs, red, green, blue);

  /* init loop */
  i = 0;
  ra = 0;
  ba = 0;
  ga = 0;

  status = low_cancel (dev->devnum);
  if (status != SANE_STATUS_GOOD)
    return status;

  /* we do a simple scan all 3 averages give the choosen level */
  while (((rts88xx_is_color (regs)
	   && ((ra < dev->sensor->red_gain_target)
	       || (ga < dev->sensor->green_gain_target)
	       || (ba < dev->sensor->blue_gain_target)))
	  || (!rts88xx_is_color (regs)
	      && (ga < dev->sensor->gray_gain_target))) && (i < 25))
    {
      status = low_simple_scan (dev, regs, sx, pixels, yoffset, lines, &data);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1,
	       "sanei_lexmark_low_gain_calibration: low_simple_scan failed!\n");
	  if (data != NULL)
	    free (data);
	  return status;
	}
#ifdef DEEP_DEBUG
      sprintf (title, "gain%02d.pnm", i);
      write_pnm_file (title, pixels, lines, rts88xx_is_color (regs), data);
#endif
      average_area (regs, data, pixels, lines, &ra, &ga, &ba);
      free (data);
      if (ra < dev->sensor->red_gain_target)
	red++;
      if (ga < dev->sensor->green_gain_target
	  || (dev->sensor->gray_gain_target && !rts88xx_is_color (regs)))
	green++;
      if (ba < dev->sensor->blue_gain_target)
	blue++;
      rts88xx_set_gain (regs, red, green, blue);
      i++;
    }
  dev->gain.red = red;
  dev->gain.green = green;
  dev->gain.blue = blue;
  dev->gain.gray = green;
  DBG (7,
       "sanei_lexmark_low_gain_calibration: gain=(0x%02x,0x%02x,0x%02x).\n",
       dev->gain.red, dev->gain.green, dev->gain.blue);
  DBG (2, "sanei_lexmark_low_gain_calibration: end.\n");
  return status;
}

/**
 * there is no hardware shading correction. So we have to do it in software.
 * We do it by scanning a pure white area which is before scanning area. Then
 * we compute per pixel coefficient to move the scanned value to the target
 * value. These coefficients are used later to correct scanned data.
 * The scan is done with all the final scan settings but the heigth and vertical
 * start position.
 */
SANE_Status
sanei_lexmark_low_shading_calibration (Lexmark_Device * dev)
{
  SANE_Byte regs[255];		/* we have our own copy of shadow registers */
  int i, j, pixels, bpl;
  int sx, ex;
  SANE_Byte *data = NULL;
  SANE_Status status;
  /* enough 75 dpi lines to "go off" home position dot,
     and include shading area */
  int lines = 4 + 4;
  int lineoffset = 1;
  int linetotal = lines + lineoffset;
  int yoffset;
  int x, y;
  float rtarget, btarget, gtarget;

  DBG (2, "sanei_lexmark_low_shading_calibration: start\n");
  /* copy registers */
  for (i = 0; i < 255; i++)
    regs[i] = dev->shadow_regs[i];

  /* allocate memory for scan */
  sx = regs[0x67] * 256 + regs[0x66];
  ex = regs[0x6d] * 256 + regs[0x6c];


  DBG (7, "startx=%d, endx=%d, coef=%d, r2f=0x%02x\n",
       sx, ex, regs[0x7a], regs[0x2f]);

  pixels = (ex - sx) / regs[0x7a];
  if (rts88xx_is_color (regs))
    bpl = 3 * pixels;
  else
    bpl = pixels;

  /* adjust the target area to the scanning resolution */
  lines = (8 * lines) / regs[0x7a];
  lineoffset = (8 * lineoffset) / regs[0x7a];
  linetotal = (8 * linetotal) / regs[0x7a];

  data = (SANE_Byte *) malloc (bpl * lines);
  DBG (7, "pixels=%d, lines=%d, size=%d\n", pixels, lines, bpl * lines);
  if (data == NULL)
    {
      DBG (2,
	   "sanei_lexmark_low_shading_calibration: failed to allocate %d bytes !\n",
	   bpl * lines);
      return SANE_STATUS_NO_MEM;
    }
  if (dev->shading_coeff != NULL)
    free (dev->shading_coeff);
  dev->shading_coeff = (float *) malloc (bpl * sizeof (float));
  if (dev->shading_coeff == NULL)
    {
      DBG (2,
	   "sanei_lexmark_low_shading_calibration: failed to allocate %d floats !\n",
	   bpl);
      free (data);
      return SANE_STATUS_NO_MEM;
    }

  /* we set movement bit this time */
  regs[0xc3] = regs[0xc3] | 0x80;

  /* execute scan */
  status = low_simple_scan (dev, regs, sx, pixels, lineoffset, lines, &data);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1,
	   "sanei_lexmark_low_shading_calibration: low_simple_scan failed!\n");
      if (data != NULL)
	free (data);
      return status;
    }

  yoffset = -1;
  /* the very first lines of the scan may include the dark dot used
   * locate park position. We find the first line free of it in the scan.
   * We can't use is_home_line since it modifies data.
   */
  for (y = 0; (y < lines) && (yoffset == y - 1); y++)
    {
      if (rts88xx_is_color (regs))
	{
	  for (x = 0; x < 3 * pixels; x++)
	    {
	      if (data[x + y * 3 * pixels] < 30)
		yoffset = y;
	    }
	}
      else
	{
	  for (x = 0; x < pixels; x++)
	    {
	      if (data[x + y * pixels] < 30)
		yoffset = y;
	    }
	}
    }
  /* make sure we are really out of the dot */
  yoffset++;

  /* yoffset is index of last dot line, go to first white line */
  if (yoffset >= lines - 1)
    {
      DBG (7,
	   "sanei_lexmark_low_shading_calibration: failed to detect yoffset.\n");
      /* fail safe fallback, picture will be altered at dot position,
         but scanner is safe */
      yoffset = lines - 2;
    }
  else
    yoffset++;
  DBG (7, "sanei_lexmark_low_shading_calibration: yoffset=%d.\n", yoffset);

#ifdef DEEP_DEBUG
  write_pnm_file ("shading.pnm", pixels, lines, rts88xx_is_color (regs),
		  data);
#endif

  /* computes coefficients */
  /* there are 8 lines usable for shading calibration at 150 dpi, between
     bottom of "home position" dot and the start of the scanner's window 
     assembly, we only use 7 of them */
  if (yoffset + (8 * 4) / regs[0x7a] < lines)
    lines = yoffset + (8 * 4) / regs[0x7a];
  rtarget = dev->sensor->red_shading_target;
  gtarget = dev->sensor->green_shading_target;
  btarget = dev->sensor->blue_shading_target;
  for (i = 0; i < pixels; i++)
    {
      /* we computes the coefficient needed to move the scanned value to
         the target value */
      if (rts88xx_is_color (dev->shadow_regs))
	{
	  /* RED */
	  dev->shading_coeff[i] = 0;
	  for (j = yoffset; j < lines; j++)
	    dev->shading_coeff[i] += data[i + j * bpl];
	  dev->shading_coeff[i] =
	    (rtarget / (dev->shading_coeff[i] / (lines - yoffset)));

	  /* GREEN */
	  dev->shading_coeff[i + pixels] = 0;
	  for (j = yoffset; j < lines; j++)
	    dev->shading_coeff[i + pixels] += data[i + j * bpl + pixels];
	  dev->shading_coeff[i + pixels] =
	    ((gtarget / dev->shading_coeff[i + pixels]) * (lines - yoffset));

	  /* BLUE */
	  dev->shading_coeff[i + 2 * pixels] = 0;
	  for (j = yoffset; j < lines; j++)
	    dev->shading_coeff[i + 2 * pixels] +=
	      data[i + j * bpl + 2 * pixels];
	  dev->shading_coeff[i + 2 * pixels] =
	    ((btarget / dev->shading_coeff[i + 2 * pixels]) *
	     (lines - yoffset));
	}
      else
	{
	  dev->shading_coeff[i] = 0;
	  for (j = yoffset; j < lines; j++)
	    {
	      dev->shading_coeff[i] += data[i + j * bpl];
	    }
	  dev->shading_coeff[i] =
	    (rtarget / dev->shading_coeff[i]) * (lines - yoffset);
	}
    }
  free(data);

  /* do the scan backward to go back to start position */
  regs[0xc6] &= 0xF7;
  lines = (8 * 8) / regs[0x7a];
  /* it shoud use linetotal to account for the lineoffset */
  if (dev->model.sensor_type == X74_SENSOR)
    lines = linetotal;

  /* execute scan */
  status = low_simple_scan (dev, regs, sx, pixels, 1, lines, &data);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1,
	   "sanei_lexmark_low_shading_calibration: low_simple_scan failed!\n");
      if(data!=NULL)
	free(data);
      return status;
    }

#ifdef DEEP_DEBUG
  write_pnm_file ("shading_bwd.pnm", pixels, lines, rts88xx_is_color (regs),
		  data);
#endif
  free (data);

  DBG (2, "sanei_lexmark_low_shading_calibration: end.\n");
  return status;
}


SANE_Status
sanei_lexmark_low_calibration (Lexmark_Device * dev)
{
  SANE_Status status;

  DBG (2, "sanei_lexmark_low_calibration: start.\n");
  status = sanei_lexmark_low_offset_calibration (dev);
  if (status != SANE_STATUS_GOOD)
    return status;

  /* we put the offset just computed in scanning regs */
  if (rts88xx_is_color (dev->shadow_regs))
    {
      rts88xx_set_offset (dev->shadow_regs,
			  dev->offset.red,
			  dev->offset.green, dev->offset.blue);
    }
  else
    {
      rts88xx_set_offset (dev->shadow_regs,
			  dev->offset.gray,
			  dev->offset.gray, dev->offset.gray);
    }

  /* if manual gain settings, no gain calibration */
  if (dev->val[OPT_MANUAL_GAIN].w == SANE_TRUE)
    {
      if (rts88xx_is_color (dev->shadow_regs))
	{
	  dev->gain.red = dev->val[OPT_RED_GAIN].w;
	  dev->gain.green = dev->val[OPT_GREEN_GAIN].w;
	  dev->gain.blue = dev->val[OPT_BLUE_GAIN].w;
	}
      else
	dev->gain.gray = dev->val[OPT_GRAY_GAIN].w;
    }
  else
    {
      status = sanei_lexmark_low_gain_calibration (dev);
      if (status != SANE_STATUS_GOOD)
	return status;
    }

  /* put the calibrated or manual settings before shading calibration 
     which must be done with final setting values */
  if (rts88xx_is_color (dev->shadow_regs))
    {
      rts88xx_set_gain (dev->shadow_regs, dev->gain.red, dev->gain.green,
			dev->gain.blue);
    }
  else
    {
      rts88xx_set_gain (dev->shadow_regs, dev->gain.gray, dev->gain.gray,
			dev->gain.gray);
    }

  status = sanei_lexmark_low_shading_calibration (dev);

  if (status != SANE_STATUS_GOOD)
    return status;

  DBG (2, "sanei_lexmark_low_calibration: end.\n");
  return SANE_STATUS_GOOD;
}

/* assign sensor data */
static SANE_Status
sanei_lexmark_low_assign_sensor (Lexmark_Device * dev)
{
  int dn;

  /* init sensor data */
  dn = 0;
  while (sensor_list[dn].id != 0
	 && sensor_list[dn].id != dev->model.sensor_type)
    dn++;
  if (sensor_list[dn].id == 0)
    {
      DBG (1,
	   "sanei_lexmark_low_assign_sensor: unknown sensor %d\n",
	   dev->model.sensor_type);
      return SANE_STATUS_UNSUPPORTED;
    }
  dev->sensor = &(sensor_list[dn]);
  DBG (1, "sanei_lexmark_low_assign_sensor: assigned sensor number %d\n",
       dev->model.sensor_type);
  return SANE_STATUS_GOOD;
}

/* assign model description, based on USB id, and register content when 
 * available */
SANE_Status
sanei_lexmark_low_assign_model (Lexmark_Device * dev,
				SANE_String_Const devname, SANE_Int vendor,
				SANE_Int product, SANE_Byte mainboard)
{
  int dn;
  SANE_Bool found = SANE_FALSE;

  DBG_INIT ();

  DBG (2, "sanei_lexmark_low_assign_model: start\n");
  DBG (3,
       "sanei_lexmark_low_assign_model: assigning %04x:%04x, variant %d\n",
       vendor, product, mainboard);

  dn = 0;
  /* walk the list of known devices */
  while (!found && model_list[dn].vendor_id != 0)
    {
      /* no mainboard id given (at attach time) */
      if (mainboard == 0
	  && vendor == model_list[dn].vendor_id
	  && product == model_list[dn].product_id)
	found = SANE_TRUE;
      /* mainboard given (init time) */
      if (mainboard != 0
	  && mainboard == model_list[dn].mainboard_id
	  && vendor == model_list[dn].vendor_id
	  && product == model_list[dn].product_id)
	found = SANE_TRUE;

      if (!found)
	dn++;
    }

  /* we hit the end of list, so we don't know about the current model */
  if (!found)
    {
      DBG (1,
	   "sanei_lexmark_low_assign_model: unknown device 0x%04x:0x%04x\n",
	   vendor, product);
      return SANE_STATUS_UNSUPPORTED;
    }

  dev->sane.name = strdup (devname);
  dev->sane.vendor = model_list[dn].vendor;
  dev->sane.model = model_list[dn].model;
  dev->model = model_list[dn];
  dev->sane.type = "flatbed scanner";

  DBG (3, "sanei_lexmark_low_assign_model: assigned %s\n", dev->model.model);

  /* init sensor data */
  DBG (2, "sanei_lexmark_low_assign_model: end.\n");
  return sanei_lexmark_low_assign_sensor (dev);
}
