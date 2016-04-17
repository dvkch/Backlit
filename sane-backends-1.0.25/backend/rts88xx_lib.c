/* sane - Scanner Access Now Easy.

   Copyright (C) 2007-2012 stef.dev@free.fr

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

/* this file contains functions common to rts88xx ASICs */

#undef BACKEND_NAME
#define BACKEND_NAME rts88xx_lib

#include "../include/sane/config.h"
#include "../include/sane/sane.h"
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_usb.h"
#include "rts88xx_lib.h"

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>

#include "../include/_stdint.h"

#define RTS88XX_LIB_BUILD 30

/* init rts88xx library */
void
sanei_rts88xx_lib_init (void)
{
  DBG_INIT ();
  DBG (DBG_info, "RTS88XX library, version %d.%d-%d\n", SANE_CURRENT_MAJOR, V_MINOR,
       RTS88XX_LIB_BUILD);
}

/*
 * registers helpers to avoid direct access
 */
SANE_Bool
sanei_rts88xx_is_color (SANE_Byte * regs)
{
  if ((regs[0x2f] & 0x11) == 0x11)
    return SANE_TRUE;
  return SANE_FALSE;
}

void
sanei_rts88xx_set_gray_scan (SANE_Byte * regs)
{
  regs[0x2f] = (regs[0x2f] & 0x0f) | 0x20;
}

void
sanei_rts88xx_set_color_scan (SANE_Byte * regs)
{
  regs[0x2f] = (regs[0x2f] & 0x0f) | 0x10;
}

void
sanei_rts88xx_set_offset (SANE_Byte * regs, SANE_Byte red, SANE_Byte green,
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

void
sanei_rts88xx_set_gain (SANE_Byte * regs, SANE_Byte red, SANE_Byte green,
                        SANE_Byte blue)
{
  regs[0x08] = red;
  regs[0x09] = green;
  regs[0x0a] = blue;
}

void
sanei_rts88xx_set_scan_frequency (SANE_Byte * regs, int frequency)
{
  regs[0x64] = (regs[0x64] & 0xf0) | (frequency & 0x0f);
}

/*
 * read one register at given index 
 */
SANE_Status
sanei_rts88xx_read_reg (SANE_Int devnum, SANE_Int index, SANE_Byte * reg)
{
  SANE_Status status = SANE_STATUS_GOOD;
  unsigned char cmd[] = { 0x80, 0x00, 0x00, 0x01 };
  size_t size;

  cmd[1] = index;

  size = 4;
  status = sanei_usb_write_bulk (devnum, cmd, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_rts88xx_read_reg: bulk write failed\n");
      return status;
    }
  size = 1;
  status = sanei_usb_read_bulk (devnum, reg, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_rts88xx_read_reg: bulk read failed\n");
      return status;
    }
  DBG (DBG_io2, "sanei_rts88xx_read_reg: reg[0x%02x]=0x%02x\n", index, *reg);
  return status;
}

/*
 * write one register at given index 
 */
SANE_Status
sanei_rts88xx_write_reg (SANE_Int devnum, SANE_Int index, SANE_Byte * reg)
{
  SANE_Status status = SANE_STATUS_GOOD;
  unsigned char cmd[] = { 0x88, 0x00, 0x00, 0x01, 0xff };
  size_t size;

  cmd[1] = index;
  cmd[4] = *reg;

  size = 5;
  status = sanei_usb_write_bulk (devnum, cmd, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_rts88xx_write_reg: bulk write failed\n");
      return status;
    }
  DBG (DBG_io2, "sanei_rts88xx_write_reg: reg[0x%02x]=0x%02x\n", index, *reg);
  return status;
}

/*
 * write length consecutive registers, starting at index
 * register 0xb3 is never wrote in bulk register write, so we split
 * write if it belongs to the register set sent
 */
SANE_Status
sanei_rts88xx_write_regs (SANE_Int devnum, SANE_Int start,
                          SANE_Byte * source, SANE_Int length)
{
  size_t size = 0;
  size_t i;
  SANE_Byte buffer[260];
  char message[256 * 5];

  if (DBG_LEVEL > DBG_io)
    {
      for (i = 0; i < (size_t) length; i++)
        {
          sprintf (message + 5 * i, "0x%02x ", source[i]);
        }
      DBG (DBG_io, "sanei_rts88xx_write_regs : write_regs(0x%02x,%d)=%s\n",
           start, length, message);
    }

  /* when writing several registers at a time, we avoid writing the 0xb3 register 
   * which is used to control the status of the scanner */
  if ((start + length > 0xb3) && (length > 1))
    {
      size = 0xb3 - start;
      buffer[0] = 0x88;
      buffer[1] = start;
      buffer[2] = 0x00;
      buffer[3] = size;
      for (i = 0; i < size; i++)
        buffer[i + 4] = source[i];
      /* the USB block is size + 4 bytes of header long */
      size += 4;
      if (sanei_usb_write_bulk (devnum, buffer, &size) != SANE_STATUS_GOOD)
        {
          DBG (DBG_error,
               "sanei_rts88xx_write_regs : write registers part 1 failed ...\n");
          return SANE_STATUS_IO_ERROR;
        }

      /* skip 0xb3 register */
      size -= 3;
      start = 0xb4;
      source = source + size;
    }
  size = length - size;
  buffer[0] = 0x88;
  buffer[1] = start;
  buffer[2] = 0x00;
  buffer[3] = size;
  for (i = 0; i < size; i++)
    buffer[i + 4] = source[i];
  /* the USB block is size + 4 bytes of header long */
  size += 4;
  if (sanei_usb_write_bulk (devnum, buffer, &size) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "sanei_rts88xx_write_regs : write registers part 2 failed ...\n");
      return SANE_STATUS_IO_ERROR;
    }

  return SANE_STATUS_GOOD;

}

/* read several registers starting at the given index */
SANE_Status
sanei_rts88xx_read_regs (SANE_Int devnum, SANE_Int start,
                         SANE_Byte * dest, SANE_Int length)
{
  SANE_Status status;
  static SANE_Byte command_block[] = { 0x80, 0, 0x00, 0xFF };
  size_t size, i;
  char message[256 * 5];

  if (start + length > 255)
    {
      DBG (DBG_error,
           "sanei_rts88xx_read_regs: start and length must be within [0..255]\n");
      return SANE_STATUS_INVAL;
    }

  /* write header */
  size = 4;
  command_block[1] = start;
  command_block[3] = length;
  status = sanei_usb_write_bulk (devnum, command_block, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_rts88xx_read_regs: failed to write header\n");
      return status;
    }

  /* read data */
  size = length;
  status = sanei_usb_read_bulk (devnum, dest, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_rts88xx_read_regs: failed to read data\n");
      return status;
    }
  if (size != (size_t) length)
    {
      DBG (DBG_warn, "sanei_rts88xx_read_regs: read got only %lu bytes\n",
           (u_long) size);
    }
  if (DBG_LEVEL >= DBG_io)
    {
      for (i = 0; i < size; i++)
        sprintf (message + 5 * i, "0x%02x ", dest[i]);
      DBG (DBG_io, "sanei_rts88xx_read_regs: read_regs(0x%02x,%d)=%s\n",
           start, length, message);
    }
  return status;
}

/*
 * get status by reading registers 0x10 and 0x11 
 */
SANE_Status
sanei_rts88xx_get_status (SANE_Int devnum, SANE_Byte * regs)
{
  SANE_Status status;
  status = sanei_rts88xx_read_regs (devnum, 0x10, regs + 0x10, 2);
  DBG (DBG_io, "sanei_rts88xx_get_status: get_status()=0x%02x 0x%02x\n",
       regs[0x10], regs[0x11]);
  return status;
}

/*
 * set status by writing registers 0x10 and 0x11 
 */
SANE_Status
sanei_rts88xx_set_status (SANE_Int devnum, SANE_Byte * regs,
                          SANE_Byte reg10, SANE_Byte reg11)
{
  SANE_Status status;

  regs[0x10] = reg10;
  regs[0x11] = reg11;
  status = sanei_rts88xx_write_regs (devnum, 0x10, regs + 0x10, 2);
  DBG (DBG_io, "sanei_rts88xx_set_status: 0x%02x 0x%02x\n", regs[0x10],
       regs[0x11]);
  return status;
}

/*
 * get lamp status by reading registers 0x84 to 0x8f, only 0x8F is currently usefull
 * 0x84 and following could "on" timers
 */
SANE_Status
sanei_rts88xx_get_lamp_status (SANE_Int devnum, SANE_Byte * regs)
{
  SANE_Status status;
  status = sanei_rts88xx_read_regs (devnum, 0x84, regs + 0x84, 11);
  return status;
}

/* resets lamp */
SANE_Status
sanei_rts88xx_reset_lamp (SANE_Int devnum, SANE_Byte * regs)
{
  SANE_Status status;
  SANE_Byte reg;

  /* read the 0xda register, then clear lower nibble and write it back */
  status = sanei_rts88xx_read_reg (devnum, 0xda, &reg);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "sanei_rts88xx_reset_lamp: failed to read 0xda register\n");
      return status;
    }
  reg = 0xa0;
  status = sanei_rts88xx_write_reg (devnum, 0xda, &reg);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "sanei_rts88xx_reset_lamp: failed to write 0xda register\n");
      return status;
    }

  /* on cleared, get status */
  status = sanei_rts88xx_get_status (devnum, regs);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_rts88xx_reset_lamp: failed to get status\n");
      return status;
    }
  DBG (DBG_io, "sanei_rts88xx_reset_lamp: status=0x%02x 0x%02x\n", regs[0x10],
       regs[0x11]);

  /* set low nibble to 7 and write it */
  reg = reg | 0x07;
  status = sanei_rts88xx_write_reg (devnum, 0xda, &reg);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "sanei_rts88xx_reset_lamp: failed to write 0xda register\n");
      return status;
    }
  status = sanei_rts88xx_read_reg (devnum, 0xda, &reg);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "sanei_rts88xx_reset_lamp: failed to read 0xda register\n");
      return status;
    }
  if (reg != 0xa7)
    {
      DBG (DBG_warn,
           "sanei_rts88xx_reset_lamp: expected reg[0xda]=0xa7, got 0x%02x\n",
           reg);
    }
  
  /* store read value in shadow register */
  regs[0xda] = reg;

  return status;
}

/*
 * get lcd status by reading registers 0x20, 0x21 and 0x22
 */
SANE_Status
sanei_rts88xx_get_lcd (SANE_Int devnum, SANE_Byte * regs)
{
  SANE_Status status;
  status = sanei_rts88xx_read_regs (devnum, 0x20, regs + 0x20, 3);
  DBG (DBG_io, "sanei_rts88xx_get_lcd: 0x%02x 0x%02x 0x%02x\n", regs[0x20],
       regs[0x21], regs[0x22]);
  return status;
}

/*
 * write to special control register CONTROL_REG=0xb3 
 */
SANE_Status
sanei_rts88xx_write_control (SANE_Int devnum, SANE_Byte value)
{
  SANE_Status status;
  status = sanei_rts88xx_write_reg (devnum, CONTROL_REG, &value);
  return status;
}

/*
 * send the cancel control sequence
 */
SANE_Status
sanei_rts88xx_cancel (SANE_Int devnum)
{
  SANE_Status status;

  status = sanei_rts88xx_write_control (devnum, 0x02);
  if (status != SANE_STATUS_GOOD)
    return status;
  status = sanei_rts88xx_write_control (devnum, 0x02);
  if (status != SANE_STATUS_GOOD)
    return status;
  status = sanei_rts88xx_write_control (devnum, 0x00);
  if (status != SANE_STATUS_GOOD)
    return status;
  status = sanei_rts88xx_write_control (devnum, 0x00);
  return status;
}

/*
 * write the given number of bytes pointed by value into memory
 * length is payload length
 * extra is number of bytes to add to the usb write length
 */
SANE_Status
sanei_rts88xx_write_mem (SANE_Int devnum, SANE_Int length, SANE_Int extra,
                         SANE_Byte * value)
{
  SANE_Status status;
  SANE_Byte *buffer;
  size_t i, size;
  char message[(0xFFC0 + 10) * 3] = "";

  buffer = (SANE_Byte *) malloc (length + 10);
  if (buffer == NULL)
    return SANE_STATUS_NO_MEM;
  memset (buffer, 0, length + 10);

  buffer[0] = 0x89;
  buffer[1] = 0x00;
  buffer[2] = HIBYTE (length);
  buffer[3] = LOBYTE (length);
  for (i = 0; i < (size_t) length; i++)
    {
      buffer[i + 4] = value[i];

      if (DBG_LEVEL > DBG_io2)
        {
          sprintf (message + 3 * i, "%02x ", buffer[i + 4]);
        }
    }
  DBG (DBG_io, "sanei_rts88xx_write_mem: %02x %02x %02x %02x -> %s\n",
       buffer[0], buffer[1], buffer[2], buffer[3], message);

  size = length + 4 + extra;
  status = sanei_usb_write_bulk (devnum, buffer, &size);
  free (buffer);
  if ((status == SANE_STATUS_GOOD) && (size != (size_t) length + 4 + extra))
    {
      DBG (DBG_error,
           "sanei_rts88xx_write_mem: only wrote %lu bytes out of %d\n",
           (u_long) size, length + 4);
      status = SANE_STATUS_IO_ERROR;
    }
  return status;
}

/*
 * set memory with the given data
 */
SANE_Status
sanei_rts88xx_set_mem (SANE_Int devnum, SANE_Byte ctrl1,
                       SANE_Byte ctrl2, SANE_Int length, SANE_Byte * value)
{
  SANE_Status status;
  SANE_Byte regs[2];
  regs[0] = ctrl1;
  regs[1] = ctrl2;

  status = sanei_rts88xx_write_regs (devnum, 0x91, regs, 2);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "sanei_rts88xx_set_mem: failed to write 0x91/0x92 registers\n");
      return status;
    }
  status = sanei_rts88xx_write_mem (devnum, length, 0, value);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_rts88xx_set_mem: failed to write memory\n");
    }
  return status;
}

/*
 * read length bytes of memory into area pointed by value
 */
SANE_Status
sanei_rts88xx_read_mem (SANE_Int devnum, SANE_Int length, SANE_Byte * value)
{
  SANE_Status status;
  size_t size, read, want;
  SANE_Byte header[4];

  /* build and write length header */
  header[0] = 0x81;
  header[1] = 0x00;
  header[2] = HIBYTE (length);
  header[3] = LOBYTE (length);
  size = 4;
  status = sanei_usb_write_bulk (devnum, header, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "sanei_rts88xx_read_mem: failed to write length header\n");
      return status;
    }
  DBG (DBG_io, "sanei_rts88xx_read_mem: %02x %02x %02x %02x -> ...\n",
       header[0], header[1], header[2], header[3]);
  read = 0;
  while (length > 0)
    {
      if (length > 2048)
        want = 2048;
      else
        want = length;
      size = want;
      status = sanei_usb_read_bulk (devnum, value + read, &size);
      if (size != want)
        {
          DBG (DBG_error,
               "sanei_rts88xx_read_mem: only read %lu bytes out of %lu\n",
               (u_long) size, (u_long) want);
          status = SANE_STATUS_IO_ERROR;
        }
      length -= size;
      read += size;
    }
  return status;
}

/*
 * set memory with the given data
 */
SANE_Status
sanei_rts88xx_get_mem (SANE_Int devnum, SANE_Byte ctrl1,
                       SANE_Byte ctrl2, SANE_Int length, SANE_Byte * value)
{
  SANE_Status status;
  SANE_Byte regs[2];
  regs[0] = ctrl1;
  regs[1] = ctrl2;

  status = sanei_rts88xx_write_regs (devnum, 0x91, regs, 2);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "sanei_rts88xx_get_mem: failed to write 0x91/0x92 registers\n");
      return status;
    }
  status = sanei_rts88xx_read_mem (devnum, length, value);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_rts88xx_get_mem: failed to read memory\n");
    }
  return status;
}

/*
 * write to the nvram controler
 */
SANE_Status
sanei_rts88xx_nvram_ctrl (SANE_Int devnum, SANE_Int length, SANE_Byte * value)
{
  SANE_Status status;
  SANE_Int i;
  char message[60 * 5];
#ifdef HAZARDOUS_EXPERIMENT
  SANE_Int size = 0;
  SANE_Byte buffer[60];
#endif

  if (DBG_LEVEL > DBG_io)
    {
      for (i = 0; i < length; i++)
        {
          sprintf (message + 5 * i, "0x%02x ", value[i]);
        }
      DBG (DBG_io, "sanei_rts88xx_nvram_ctrl : devnum=%d, nvram_ctrl(0x00,%d)=%s\n",
	   devnum, length, message);
    }

#ifdef HAZARDOUS_EXPERIMENT
  buffer[0] = 0x8a;
  buffer[1] = 0x00;
  buffer[2] = 0x00;
  buffer[3] = length;
  for (i = 0; i < size; i++)
    buffer[i + 4] = value[i];
  /* the USB block is size + 4 bytes of header long */
  size = length + 4;
  status = sanei_usb_write_bulk (devnum, buffer, &size);
#else
  status = SANE_STATUS_GOOD;
#endif
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_rts88xx_nvram_ctrl : write failed ...\n");
    }
  return status;
}

/*
 * setup nvram
 */
SANE_Status
sanei_rts88xx_setup_nvram (SANE_Int devnum, SANE_Int length,
                           SANE_Byte * value)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte local[2], reg;
  int i;

  status = sanei_rts88xx_nvram_ctrl (devnum, length, value);

#ifndef HAZARDOUS_EXPERIMENT
  return SANE_STATUS_GOOD;
#endif
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_rts88xx_setup_nvram : failed step #1 ...\n");
      return status;
    }
  local[0] = 0x18;
  local[1] = 0x08;
  for (i = 0; i < 8; i++)
    {
      status = sanei_rts88xx_nvram_ctrl (devnum, 2, local);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (DBG_error, "sanei_rts88xx_setup_nvram : failed loop #%d ...\n",
               i);
          return status;
        }
      status = sanei_rts88xx_read_reg (devnum, 0x10, &reg);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (DBG_error,
               "sanei_rts88xx_setup_nvram : register reading failed loop #%d ...\n",
               i);
          return status;
        }
      DBG (DBG_io, "sanei_rts88xx_setup_nvram: reg[0x10]=0x%02x\n", reg);
    }
  reg = 0;
  status = sanei_rts88xx_write_reg (devnum, CONTROLER_REG, &reg);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "sanei_rts88xx_setup_nvram : controler register write failed\n");
      return status;
    }
  reg = 1;
  status = sanei_rts88xx_write_reg (devnum, CONTROLER_REG, &reg);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "sanei_rts88xx_setup_nvram : controler register write failed\n");
      return status;
    }
  return status;
}

/*
 * Sets scan area, no checks are being done, so watch your steps
 */
void
sanei_rts88xx_set_scan_area (SANE_Byte * regs, SANE_Int ystart,
                             SANE_Int yend, SANE_Int xstart, SANE_Int xend)
{
  /* vertical lines to move before scan */
  regs[START_LINE] = LOBYTE (ystart);
  regs[START_LINE + 1] = HIBYTE (ystart);

  /* total number of line to move */
  regs[END_LINE] = LOBYTE (yend);
  regs[END_LINE + 1] = HIBYTE (yend);

  /* set horizontal start position */
  regs[START_PIXEL] = LOBYTE (xstart);
  regs[START_PIXEL + 1] = HIBYTE (xstart);

  /* set horizontal end position */
  regs[END_PIXEL] = LOBYTE (xend);
  regs[END_PIXEL + 1] = HIBYTE (xend);
}

/**
 * read available data count from scanner
 * from tests it appears that advertised data
 * may not be really available, and that a pause must be made
 * before reading data so that it is really there.
 * Such as reading data twice.
 */
SANE_Status
sanei_rts88xx_data_count (SANE_Int devnum, SANE_Word * count)
{
  SANE_Status status;
  size_t size;
  static SANE_Byte header[4] = { 0x90, 0x00, 0x00, 3 };
  SANE_Byte result[3];

  /* set count in case of failure */
  *count = 0;

  size = 4;
  status = sanei_usb_write_bulk (devnum, header, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_rts88xx_data_count : failed to write header\n");
      return status;
    }
  size = 3;
  status = sanei_usb_read_bulk (devnum, result, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "sanei_rts88xx_data_count : failed to read data count\n");
      return status;
    }
  *count = result[0] + (result[1] << 8) + (result[2] << 16);
  DBG (DBG_io2, "sanei_rts88xx_data_count: %d bytes available (0x%06x)\n",
       *count, *count);
  return status;
}

/**
 * Waits for data being available while optionally polling motor. There is a timeout
 * to prevent scanner waiting forever non coming data.
 */
SANE_Status
sanei_rts88xx_wait_data (SANE_Int devnum, SANE_Bool busy, SANE_Word * count)
{
  SANE_Status status;
  SANE_Byte control;

  /* poll the available byte count until not 0 */
  while (SANE_TRUE)
    {
      status = sanei_rts88xx_data_count (devnum, count);
      if (*count != 0)
        {
          DBG (DBG_io, "sanei_rts88xx_wait_data: %d bytes available\n",
               *count);
          return status;
        }

      /* check that the scanner is busy scanning */
      if (busy)
        {
          sanei_rts88xx_read_reg (devnum, CONTROL_REG, &control);
          if ((control & 0x08) == 0 && (*count == 0))
            {
              DBG (DBG_error,
                   "sanei_rts88xx_wait_data: scanner stopped being busy before data are available\n");
              return SANE_STATUS_IO_ERROR;
            }
        }
    }

  /* we hit timeout */
  return SANE_STATUS_IO_ERROR;
}

/*
 * read scanned data from scanner up to the size given. The actual length read is returned.
 */
SANE_Status
sanei_rts88xx_read_data (SANE_Int devnum, SANE_Word * length,
                         unsigned char *dest)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte header[4];
  size_t size, len, remain, read;

  /* do not read too much data */
  if (*length > RTS88XX_MAX_XFER_SIZE)
    len = RTS88XX_MAX_XFER_SIZE;
  else
    len = *length;

  /* write command header first */
  header[0] = 0x91;
  header[1] = 0x00;
  header[2] = HIBYTE (len);
  header[3] = LOBYTE (len);
  size = 4;

  status = sanei_usb_write_bulk (devnum, header, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_rts88xx_read_data: failed to write header\n");
    }
  read = 0;

  /* first read blocks aligned on 64 bytes boundary */
  while (len - read > 64)
    {
      size = (len - read) & 0xFFC0;
      status = sanei_usb_read_bulk (devnum, dest + read, &size);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (DBG_error, "sanei_rts88xx_read_data: failed to read data\n");
          return status;
        }
      DBG (DBG_io2, "sanei_rts88xx_read_data: read %lu bytes\n",
           (u_long) size);
      read += size;
    }

  /* then read remainder */
  remain = len - read;
  if (remain > 0)
    {
      status = sanei_usb_read_bulk (devnum, dest + read, &remain);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (DBG_error, "sanei_rts88xx_read_data: failed to read data\n");
          return status;
        }
      DBG (DBG_io2, "sanei_rts88xx_read_data: read %lu bytes\n",
           (u_long) remain);
      read += remain;
    }

  /* update actual read length */
  DBG (DBG_io, "sanei_rts88xx_read_data: read %lu bytes, %d required\n",
       (u_long) read, *length);
  *length = read;
  return status;
}
