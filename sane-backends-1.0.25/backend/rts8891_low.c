/* sane - Scanner Access Now Easy.

   Copyright (C) 2007-2013 stef.dev@free.fr
   
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

/* this file contains all the low level functions needed for the higher level
 * functions of the sane standard. They are put there to keep files smaller
 * and separate functions with different goals.
 */

#include "../include/sane/config.h"
#include "../include/sane/sane.h"
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_usb.h"

#include <stdio.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include "rts8891_low.h"

#define RTS8891_BUILD           30
#define RTS8891_MAX_REGISTERS	244

/* init rts8891 library */
static void
rts8891_low_init (void)
{
  DBG_INIT ();
  DBG (DBG_info, "RTS8891 low-level  functions, version %d.%d-%d\n",
       SANE_CURRENT_MAJOR, V_MINOR, RTS8891_BUILD);
}


/****************************************************************/
/*                 ASIC specific functions                      */
/****************************************************************/

/* write all registers, taking care of the special 0xaa value which
 * must be escaped with a zero
 */
static SANE_Status
rts8891_write_all (SANE_Int devnum, SANE_Byte * regs, SANE_Int count)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte local_regs[RTS8891_MAX_REGISTERS];
  size_t size = 0;
  SANE_Byte buffer[260];
  unsigned int i, j;
  char message[256 * 5];

  if (DBG_LEVEL > DBG_io)
    {
      for (i = 0; i < (unsigned int) count; i++)
	{
	  if (i != 0xb3)
	    sprintf (message + 5 * i, "0x%02x ", regs[i]);
	  else
	    sprintf (message + 5 * i, "---- ");
	}
      DBG (DBG_io, "rts8891_write_all : write_all(0x00,%d)=%s\n", count,
	   message);
    }

  /* copy register set and escaping 0xaa values                     */
  /* b0, b1 abd b3 values may be scribled, but that isn't important */
  /* since they are read-only registers                             */
  j = 0;
  for (i = 0; i < 0xb3; i++)
    {
      local_regs[j] = regs[i];
      if (local_regs[j] == 0xaa && i < 0xb3)
	{
	  j++;
	  local_regs[j] = 0x00;
	}
      j++;
    }
  buffer[0] = 0x88;
  buffer[1] = 0;
  buffer[2] = 0x00;
  buffer[3] = 0xb3;
  for (i = 0; i < j; i++)
    buffer[i + 4] = local_regs[i];
  /* the USB block is size + 4 bytes of header long */
  size = j + 4;
  if (sanei_usb_write_bulk (devnum, buffer, &size) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "rts88xx_write_all : write registers part 1 failed ...\n");
      return SANE_STATUS_IO_ERROR;
    }

  size = count - 0xb4;		/*  we need to substract one reg since b3 won't be written */
  buffer[0] = 0x88;
  buffer[1] = 0xb4;
  buffer[2] = 0x00;
  buffer[3] = size;
  for (i = 0; i < size; i++)
    buffer[i + 4] = regs[0xb4 + i];
  /* the USB block is size + 4 bytes of header long */
  size += 4;
  if (sanei_usb_write_bulk (devnum, buffer, &size) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "rts88xx_write_all : write registers part 2 failed ...\n");
      return SANE_STATUS_IO_ERROR;
    }
  return status;
}


/* this functions "commits" pending scan command */
static SANE_Status
rts8891_commit (SANE_Int devnum, SANE_Byte value)
{
  SANE_Status status;
  SANE_Byte reg;

  reg = value;
  sanei_rts88xx_write_reg (devnum, 0xd3, &reg);
  sanei_rts88xx_cancel (devnum);
  sanei_rts88xx_write_control (devnum, 0x08);
  status = sanei_rts88xx_write_control (devnum, 0x08);
  return status;
}

/* this functions reads button status */
static SANE_Status
rts8891_read_buttons (SANE_Int devnum, SANE_Int * mask)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte reg;

  /* check CONTROL_REG */
  sanei_rts88xx_read_reg (devnum, CONTROL_REG, &reg);

  /* read 'base' button status */
  sanei_rts88xx_read_reg (devnum, 0x25, &reg);
  DBG (DBG_io, "rts8891_read_buttons: r25=0x%02x\n", reg);
  *mask |= reg;

  /* read 'extended' button status */
  sanei_rts88xx_read_reg (devnum, 0x1a, &reg);
  DBG (DBG_io, "rts8891_read_buttons: r1a=0x%02x\n", reg);
  *mask |= reg << 8;

  /* clear register r25 */
  reg = 0x00;
  sanei_rts88xx_write_reg (devnum, 0x25, &reg);

  /* clear register r1a */
  sanei_rts88xx_read_reg (devnum, 0x1a, &reg);
  reg = 0x00;
  status = sanei_rts88xx_write_reg (devnum, 0x1a, &reg);

  DBG (DBG_info, "rts8891_read_buttons: mask=0x%04x\n", *mask);
  return status;
}

/*
 * Does a simple scan based on the given register set, returning data in a
 * preallocated buffer of the claimed size.
 * sanei_rts88xx_data_count cannot be made reliable, when the announced data
 * amount is read, it may no be ready, leading to errors. To work around
 * it, we read data count one more time before reading.
 */
static SANE_Status
rts8891_simple_scan (SANE_Int devnum, SANE_Byte * regs, int regcount,
		     SANE_Int format, SANE_Word total, unsigned char *image)
{
  SANE_Word count, read, len, dummy;
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte control;

  rts8891_write_all (devnum, regs, regcount);
  rts8891_commit (devnum, format);

  read = 0;
  count = 0;
  while (count == 0)
    {
      status = sanei_rts88xx_data_count (devnum, &count);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "simple_scan: failed to wait for data\n");
	  return status;
	}
      if (count == 0)
	{
	  status = sanei_rts88xx_read_reg (devnum, CONTROL_REG, &control);
	  if (((control & 0x08) == 0) || (status != SANE_STATUS_GOOD))
	    {
	      DBG (DBG_error, "simple_scan: failed to wait for data\n");
	      return SANE_STATUS_IO_ERROR;
	    }
	}
    }

  /* data reading */
  read = 0;
  while ((read < total) && (count != 0 || (control & 0x08) == 0x08))
    {
      /* sync ? */
      status = sanei_rts88xx_data_count (devnum, &dummy);

      /* read */
      if (count > 0)
	{
	  len = count;
	  /* read even size unless last chunk */
	  if ((len & 1) && (read + len < total))
	    {
	      len++;
	    }
	  if (len > RTS88XX_MAX_XFER_SIZE)
	    {
	      len = RTS88XX_MAX_XFER_SIZE;
	    }
	  if (len > 0)
	    {
	      status = sanei_rts88xx_read_data (devnum, &len, image + read);
	      if (status != SANE_STATUS_GOOD)
		{
		  DBG (DBG_error,
		       "simple_scan: failed to read from scanner\n");
		  return status;
		}
	      read += len;
	    }
	}

      /* don't try to read data count if we have enough data */
      if (read < total)
	{
	  status = sanei_rts88xx_data_count (devnum, &count);
	}
      else
	{
	  count = 0;
	}
      if (count == 0)
	{
	  sanei_rts88xx_read_reg (devnum, CONTROL_REG, &control);
	}
    }

  /* sanity check */
  if (read < total)
    {
      DBG (DBG_io2, "simple_scan: ERROR, %d bytes missing ... \n",
	   total - read);
    }

  /* wait for the motor to stop */
  do
    {
      sanei_rts88xx_read_reg (devnum, CONTROL_REG, &control);
    }
  while ((control & 0x08) == 0x08);

  return status;
}

 /**
  * set the data format. Is part of the commit sequence. Then returned
  * value is the value used in d3 register for a scan.
  */
static SANE_Int
rts8891_data_format (SANE_Int dpi, int sensor)
{
  SANE_Byte reg = 0x00;

  /* it seems that lower nibble is a divisor */
  if (sensor == SENSOR_TYPE_BARE || sensor == SENSOR_TYPE_XPA)
    {
      switch (dpi)
	{
	case 75:
	  reg = 0x02;
	  break;
	case 150:
	  if (sensor == SENSOR_TYPE_BARE)
	    reg = 0x0e;
	  else
	    reg = 0x0b;
	  break;
	case 300:
	  reg = 0x17;
	  break;
	case 600:
	  if (sensor == SENSOR_TYPE_BARE)
	    reg = 0x02;
	  else
	    reg = 0x0e;
	  break;
	case 1200:
	  if (sensor == SENSOR_TYPE_BARE)
	    reg = 0x17;
	  else
	    reg = 0x05;
	  break;
	}
    }
  if (sensor == SENSOR_TYPE_4400 || sensor == SENSOR_TYPE_4400_BARE)
    {
      switch (dpi)
	{
	case 75:
	  reg = 0x02;
	  break;
	case 150:
	  if (sensor == SENSOR_TYPE_4400)
	    reg = 0x0b;
	  else
	    reg = 0x17;
	  break;
	case 300:
	  reg = 0x17;
	  break;
	case 600:
	  if (sensor == SENSOR_TYPE_4400)
	    reg = 0x0e;
	  else
	    reg = 0x02;
	  break;
	case 1200:
	  if (sensor == SENSOR_TYPE_4400)
	    reg = 0x05;
	  else
	    reg = 0x17;
	  break;
	}
    }
  return reg;
}

/**
 * set up default values for a 75xdpi, 150 ydpi scan
 */
static void
rts8891_set_default_regs (SANE_Byte * scanner_regs)
{
  SANE_Byte default_75[RTS8891_MAX_REGISTERS] =
    { 0xe5, 0x41, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x0a, 0x0a, 0x0a, 0x70,
    0x00, 0x00, 0x00, 0x00, 0x28, 0x3f, 0xff, 0x20, 0xf8, 0x28, 0x07, 0x00,
    0xff, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00,
    0x00, 0x3a, 0xf2, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x8c,
    0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe1, 0x14, 0x18, 0x15, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xff, 0x3f, 0x80, 0x68, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcc,
    0x27, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x0f, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
    0x0e, 0x00, 0x00, 0xf0, 0xff, 0xf5, 0xf7, 0xea, 0x0b, 0x03, 0x05, 0x86,
    0x1b, 0x30, 0xf6, 0xa2, 0x27, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };
  unsigned int i;
  for (i = 0; i < RTS8891_MAX_REGISTERS; i++)
    scanner_regs[i] = default_75[i];
}

static SANE_Status
rts8891_move (struct Rts8891_Device *device, SANE_Byte * regs,
	      SANE_Int distance, SANE_Bool forward)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte regs10, regs11;

  DBG (DBG_proc, "rts8891_move: start\n");
  DBG (DBG_io, "rts8891_move: %d lines %s, sensor=%d\n", distance,
       forward == SANE_TRUE ? "forward" : "backward", device->sensor);

  /* prepare scan */
  rts8891_set_default_regs (regs);
  if (device->sensor != SENSOR_TYPE_4400
      && device->sensor != SENSOR_TYPE_4400_BARE)
    {
      regs10 = 0x20;
      regs11 = 0x28;
    }
  else
    {
      regs10 = 0x10;
      regs11 = 0x2a;
    }

  regs[0x32] = 0x80;
  regs[0x33] = 0x81;
  regs[0x35] = 0x10;
  regs[0x36] = 0x24;
  regs[0x39] = 0x02;
  regs[0x3a] = 0x0e;

  regs[0x64] = 0x01;
  regs[0x65] = 0x20;

  regs[0x79] = 0x20;
  regs[0x7a] = 0x01;

  regs[0x80] = 0x32;
  regs[0x82] = 0x33;
  regs[0x85] = 0x46;
  regs[0x86] = 0x0b;
  regs[0x87] = 0x8c;
  regs[0x88] = 0x10;
  regs[0x89] = 0xb2;
  regs[0x8d] = 0x3b;
  regs[0x8e] = 0x60;

  regs[0x90] = 0x1c;
  regs[0xb2] = 0x16;		/* 0x10 : stop when at home, 0x04: no data */
  regs[0xc0] = 0x00;
  regs[0xc1] = 0x00;
  regs[0xc3] = 0x00;
  regs[0xc4] = 0x00;
  regs[0xc5] = 0x00;
  regs[0xc6] = 0x00;
  regs[0xc7] = 0x00;
  regs[0xc8] = 0x00;
  regs[0xca] = 0x00;
  regs[0xcd] = 0x00;
  regs[0xce] = 0x00;
  regs[0xcf] = 0x00;
  regs[0xd0] = 0x00;
  regs[0xd1] = 0x00;
  regs[0xd2] = 0x00;
  regs[0xd3] = 0x00;
  regs[0xd4] = 0x00;
  regs[0xd6] = 0x6b;
  regs[0xd7] = 0x00;
  regs[0xd8] = 0x00;
  regs[0xd9] = 0xad;
  regs[0xda] = 0xa7;
  regs[0xe2] = 0x17;
  regs[0xe3] = 0x0d;
  regs[0xe4] = 0x06;
  regs[0xe5] = 0xf9;
  regs[0xe7] = 0x53;
  regs[0xe8] = 0x02;
  regs[0xe9] = 0x02;

  /* hp4400 sensors */
  if (device->sensor == SENSOR_TYPE_4400
      || device->sensor == SENSOR_TYPE_4400_BARE)
    {
      regs[0x13] = 0x39;	/* 0x20 */
      regs[0x14] = 0xf0;	/* 0xf8 */
      regs[0x15] = 0x29;	/* 0x28 */
      regs[0x16] = 0x0f;	/* 0x07 */
      regs[0x17] = 0x10;	/* 0x00 */
      regs[0x23] = 0x00;	/* 0xff */
      regs[0x35] = 0x29;	/* 0x10 */
      regs[0x36] = 0x21;	/* 0x24 */
      regs[0x39] = 0x00;	/* 0x02 */
      regs[0x80] = 0xb0;	/* 0x32 */
      regs[0x82] = 0xb1;	/* 0x33 */
      regs[0xe2] = 0x0b;	/* 0x17 */
      regs[0xe5] = 0xf3;	/* 0xf9 */
      regs[0xe6] = 0x01;	/* 0x00 */
    }

  /* disable CCD */
  regs[0] = 0xf5;

  sanei_rts88xx_set_status (device->devnum, regs, regs10, regs11);
  sanei_rts88xx_set_scan_area (regs, distance, distance + 1, 100, 200);
  sanei_rts88xx_set_gain (regs, 16, 16, 16);
  sanei_rts88xx_set_offset (regs, 127, 127, 127);

  /* forward/backward */
  /* 0x2c is forward, 0x24 backward */
  if (forward == SANE_TRUE)
    {				/* forward */
      regs[0x36] = regs[0x36] | 0x08;
    }
  else
    {				/* backward */
      regs[0x36] = regs[0x36] & 0xf7;
    }

  /* write regiters values */
  status = rts8891_write_all (device->devnum, regs, RTS8891_MAX_REGISTERS);

  /* commit it */
  rts8891_commit (device->devnum, 0x00);

  return status;
}

 /**
  * wait for the scanning head to reach home position
  */
static SANE_Status
rts8891_wait_for_home (struct Rts8891_Device *device, SANE_Byte * regs)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte motor, sensor, reg;

  DBG (DBG_proc, "rts8891_wait_for_home: start\n");

  /* wait for controller home bit to raise, no timeout */
  /* at each loop we check that motor is on, then that the sensor bit it cleared */
  do
    {
      sanei_rts88xx_read_reg (device->devnum, CONTROL_REG, &motor);
      sanei_rts88xx_read_reg (device->devnum, CONTROLER_REG, &sensor);
    }
  while ((motor & 0x08) && ((sensor & 0x02) == 0));

  /* flag that device has finished parking */
  device->parking=SANE_FALSE;

  /* check for error */
  if (((motor & 0x08) == 0x00) && ((sensor & 0x02) == 0))
    {
      DBG (DBG_error,
	   "rts8891_wait_for_home: error, motor stopped before head parked\n");
      status = SANE_STATUS_INVAL;
    }

  /* re-enable CCD */
  regs[0] = regs[0] & 0xef;

  sanei_rts88xx_cancel (device->devnum);

  /* reset ? so we don't need to read data */
  reg = 0;
  /* b7: movement on/off, b3-b0 : movement divisor */
  sanei_rts88xx_write_reg (device->devnum, 0x33, &reg);
  sanei_rts88xx_write_reg (device->devnum, 0x33, &reg);
  /* movement direction */
  sanei_rts88xx_write_reg (device->devnum, 0x36, &reg);
  sanei_rts88xx_cancel (device->devnum);

  DBG (DBG_proc, "rts8891_wait_for_home: end\n");
  return status;
}

 /**
  * move the head backward by a huge line number then poll home sensor until
  * head has get back home. We have our own copy of the registers to avoid
  * messing scanner status
  */
static SANE_Status
rts8891_park (struct Rts8891_Device *device, SANE_Byte *regs, SANE_Bool wait)
{
  SANE_Status status = SANE_STATUS_GOOD;

  DBG (DBG_proc, "rts8891_park: start\n");

  device->parking=SANE_TRUE;
  rts8891_move (device, regs, 8000, SANE_FALSE);

  if(wait==SANE_TRUE)
    {
      status = rts8891_wait_for_home (device,regs);
    }

  DBG (DBG_proc, "rts8891_park: end\n");
  return status;
}

/* reads data from scanner.
 * First we wait for some data to be available and then loop reading
 * from scanner until the required amount is reached.
 * We handle non blocking I/O by returning immediatly (with SANE_STATUS_BUSY)
 * if there is no data available from scanner. But once read is started,
 * all the required amount is read. Once wait for data succeeded, we still poll
 * for data in order no to read it too fast, but we don' take care of non blocking
 * mode since we cope with it on first data wait.
 */
static SANE_Status
read_data (struct Rts8891_Session *session, SANE_Byte * dest, SANE_Int length)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int count, read, len, dummy;
  struct Rts8891_Device *dev = session->dev;
  static FILE *raw = NULL;	/* for debugging purpose we need it static */
  SANE_Byte control = 0x08;
  unsigned char buffer[RTS88XX_MAX_XFER_SIZE];

  DBG (DBG_proc, "read_data: start\n");
  DBG (DBG_proc, "read_data: requiring %d bytes\n", length);

  /* wait for data being available and handle non blocking mode */
  /* only when data reading hasn't produce any data yet */
  if (dev->read == 0)
    {
      do
	{
	  status = sanei_rts88xx_data_count (dev->devnum, &count);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error, "read_data: failed to wait for data\n");
	      return status;
	    }
	  if (count == 0)
	    {
	      sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
	      if ((control & 0x08) == 0 && (count == 0))
		{
		  DBG (DBG_error,
		       "read_data: scanner stopped being busy before data are available\n");
		  return SANE_STATUS_IO_ERROR;
		}
	    }

	  /* in case there is no data, we return BUSY since this mean    */
	  /* that scanning head hasn't reach is position and data hasn't */
	  /* come yet */
	  if (session->non_blocking && count == 0)
	    {

	      dev->regs[LAMP_REG] = 0x8d;
	      sanei_rts88xx_write_reg (dev->devnum, LAMP_REG,
				       &(dev->regs[LAMP_REG]));
	      DBG (DBG_io, "read_data: no data vailable\n");
	      DBG (DBG_proc, "read_data: end\n");
	      return SANE_STATUS_DEVICE_BUSY;
	    }
	}
      while (count == 0);
    }
  else
    {				/* start of read for a new block */
      status = sanei_rts88xx_data_count (dev->devnum, &count);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "read_data: failed to wait for data\n");
	  return status;
	}
      if (count == 0)
	{
	  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
	  if ((control & 0x08) == 0 && (count == 0))
	    {
	      DBG (DBG_error,
		   "read_data: scanner stopped being busy before data are available\n");
	      return SANE_STATUS_IO_ERROR;
	    }
	}
    }

  /* fill scanned data buffer */
  read = 0;

  /* now loop reading data until we have the amount requested */
  /* we also take care of not reading too much data           */
  while (read < length && dev->read < dev->to_read
	 && ((control & 0x08) == 0x08))
    {
      /* used to sync */
      if (dev->read == 0)
	{
	  status = sanei_rts88xx_data_count (dev->devnum, &dummy);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error, "read_data: failed to read data count\n");
	      return status;
	    }
	}

      /* if there is data to read, read it */
      if (count > 0)
	{
	  len = count;

	  if (len > RTS88XX_MAX_XFER_SIZE)
	    {
	      len = RTS88XX_MAX_XFER_SIZE;
	    }

	  /* we only read even size blocks of data */
	  if (len & 1)
	    {
	      DBG (DBG_io, "read_data: round to next even number\n");
	      len++;
	    }

	  if (len > length - read)
	    {
	      len = length - read;
	    }

	  status = sanei_rts88xx_read_data (dev->devnum, &len, dest + read);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error, "read_data: failed to read from scanner\n");
	      return status;
	    }

	  /* raw data tracing */
	  if (DBG_LEVEL >= DBG_io2)
	    {
	      /* open a new file only when no data scanned */
	      if (dev->read == 0)
		{
		  raw = fopen ("raw_data.pnm", "wb");
		  if (raw != NULL)
		    {
		      /* PNM header */
		      fprintf (raw, "P%c\n%d %d\n255\n",
			       session->params.format ==
			       SANE_FRAME_RGB
			       || session->emulated_gray ==
			       SANE_TRUE ? '6' : '5', dev->pixels,
			       dev->lines);
		    }
		}
	      if (raw != NULL)
		{
		  fwrite (dest + read, 1, len, raw);
		}
	    }

	  /* move pointer and counter */
	  read += len;
	  dev->read += len;
	  DBG (DBG_io2, "read_data: %d/%d\n", dev->read, dev->to_read);
	}

      /* in fast scan mode, read data count
       * in slow scan, head moves by the amount of data read */
      status = sanei_rts88xx_data_count (dev->devnum, &count);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "read_data: failed to read data count\n");
	  return status;
	}

      /* if no data, check if still scanning */
      if (count == 0 && dev->read < dev->to_read)
	{
	  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
	  if ((control & 0x08) == 0x00)
	    {
	      DBG (DBG_error,
		   "read_data: scanner stopped being busy before data are available\n");
	      return SANE_STATUS_IO_ERROR;
	    }
	}
    }

  /* end of physical reads */
  if (dev->read >= dev->to_read)
    {
      /* check there is no more data in case of a bug */
      sanei_rts88xx_data_count (dev->devnum, &count);
      if (count > 0)
	{
	  DBG (DBG_warn,
	       "read_data: %d bytes are still available from scanner\n",
	       count);

	  /* flush left-over data */
	  while (count > 0)
	    {
	      len = count;
	      if (len > RTS88XX_MAX_XFER_SIZE)
		{
		  len = RTS88XX_MAX_XFER_SIZE;
		}

	      /* we only read even size blocks of data */
	      if (len & 1)
		{
		  len++;
		}
	      sanei_rts88xx_read_data (dev->devnum, &len, buffer);
	      sanei_rts88xx_data_count (dev->devnum, &count);
	    }
	}

      /* wait for motor to stop at the end of the scan */
      do
	{
	  sanei_rts88xx_read_reg (dev->devnum, CONTROL_REG, &control);
	}
      while ((control & 0x08) != 0);

      /* close log file if needed */
      if (DBG_LEVEL >= DBG_io2)
	{
	  if (raw != NULL)
	    {
	      fclose (raw);
	      raw = NULL;
	    }
	}
    }

  DBG (DBG_io, "read_data: read %d bytes from scanner\n", length);
  DBG (DBG_proc, "read_data: end\n");
  return status;
}

  /**
   * set scanner idle before leaving xxx_quiet()
write_reg(0x33,1)=0x00 
write_reg(0x33,1)=0x00 
write_reg(0x36,1)=0x00 
prepare();
------
write_reg(LAMP_REG,1)=0x80 
write_reg(LAMP_REG,1)=0xad 
read_reg(0x14,2)=0xf8 0x28 
write_reg(0x14,2)=0x78 0x28 
get_status()=0x20 0x3f 
read_reg(0xb1,1)=0x00 
read_control()=0x00 
reset_lamp()=(0x20,0x3f)
write_reg(LAMP_REG,1)=0x8d 
write_reg(LAMP_REG,1)=0xad 
   */

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
