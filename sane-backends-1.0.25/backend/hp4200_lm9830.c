/* sane - Scanner Access Now Easy.

   Copyright (C) 2000 Adrian Perez Jorge

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

   This file implements a backend for the HP4200C flatbed scanner
*/


#include "hp4200_lm9830.h"

static SANE_Status
lm9830_read_register (int fd, unsigned char reg, unsigned char *data)
{
  SANE_Status retval;

  if (!data)
    return -SANE_STATUS_INVAL;
  retval = sanei_pv8630_write_byte (fd, PV8630_REPPADDRESS, reg);
  if (retval != SANE_STATUS_GOOD)
    return retval;
  return sanei_pv8630_read_byte (fd, PV8630_RDATA, data);
}

static SANE_Status
lm9830_write_register (int fd, unsigned char reg, unsigned char value)
{
  SANE_Status retval;

  retval = sanei_pv8630_write_byte (fd, PV8630_REPPADDRESS, reg);
  if (retval != SANE_STATUS_GOOD)
    return retval;

  return sanei_pv8630_write_byte (fd, PV8630_RDATA, value);
}

#ifdef DEBUG
static int
lm9830_dump_registers (int fd)
{
  int i;
  unsigned char value = 0;

  for (i = 0; i < 0x80; i++)
    {
      lm9830_read_register (fd, i, &value);
      printf ("%.2x:0x%.2x", i, value);
      if ((i + 1) % 8)
	printf (", ");
      else
	printf ("\n");
    }
  puts ("");
  return 0;
}
#endif

#if 0
static int
pv8630_reset_buttons (int fd)
{
  lm9830_write_register (fd, 0x59, 0x10);
  lm9830_write_register (fd, 0x59, 0x90);
  return 0;
}
#endif

#if 0
static int
lm9830_lamp_off (int fd)
{
  lm9830_write_register (fd, 0x07, 0x00);
  lm9830_write_register (fd, 0x2c, 0x00);
  lm9830_write_register (fd, 0x2d, 0x01);
  lm9830_write_register (fd, 0x2e, 0x3f);
  lm9830_write_register (fd, 0x2f, 0xff);
  return 0;
}

static int
lm9830_lamp_on (int fd)
{
  lm9830_write_register (fd, 0x07, 0x00);
  lm9830_write_register (fd, 0x2c, 0x3f);
  lm9830_write_register (fd, 0x2d, 0xff);
  lm9830_write_register (fd, 0x2e, 0x00);
  lm9830_write_register (fd, 0x2f, 0x01);
  return 0;
}
#endif

#if 0
/*
 * This function prints what button was pressed (the time before this
 * code was executed).
 */

static int
hp4200c_what_button (int fd)
{
  unsigned char button;

  pv8630_read_buttons (fd, &button);

  if (button & 0x08)
    puts ("Scan");
  if (button & 0x10)
    puts ("Copy");
  if (button & 0x20)
    puts ("E-mail");
  if ((button & 0x38) == 0)
    puts ("None");

  pv8630_reset_buttons (fd);
  return 0;
}
#endif

static int
lm9830_ini_scanner (int fd, unsigned char *regs)
{
#ifdef unused
  unsigned char inittable[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x1A, 0x00, 0x0A, 0x60, 0x2F, 0x13, 0x06,
    0x17, 0x01, 0x03, 0x03, 0x05, 0x00, 0x00, 0x0B,
    0x00, 0x00, 0x00, 0x00, 0x0D, 0x21, 0x00, 0x40,
    0x15, 0x18, 0x00, 0x40, 0x02, 0x98, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x3F, 0xFF, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x30,
    0x25, 0x25, 0x24, 0x28, 0x24, 0x28, 0x00, 0x00,
    0x00, 0x00, 0x06, 0x1D, 0x00, 0x13, 0x05, 0x48,
    0x01, 0xC2, 0x00, 0x00, 0x00, 0x00, 0x5E, 0x02,
    0x00, 0x15, 0x00, 0x45, 0x00, 0x10, 0x08, 0x17,
    0x2B, 0x90, 0x00, 0x00, 0x01, 0x00, 0x80, 0x00
  };
#endif

  unsigned char daisy[] = { 0x99, 0x66, 0xcc, 0x33 };
  unsigned char *regdata;
  unsigned int i;

  sanei_pv8630_write_byte (fd, PV8630_RMODE, 0x02);
  for (i = 0; i < sizeof (daisy); i++)
    {
      sanei_pv8630_write_byte (fd, PV8630_RDATA, daisy[i]);
    }
  sanei_pv8630_write_byte (fd, PV8630_RMODE, 0x16);
  lm9830_write_register (fd, 0x42, 0x06);

  if (!regs)
    return 0;
  /*    regdata = inittable; */
  else
    regdata = regs;

  for (i = 8; i < 0x60; i++)
    {
      lm9830_write_register (fd, i, regdata[i]);
    }
  for (i = 0x60; i < 0x70; i++)
    {
      lm9830_write_register (fd, i, 0);
    }
  lm9830_write_register (fd, 0x70, 0x70);
  for (i = 0x71; i < 0x80; i++)
    {
      lm9830_write_register (fd, i, 0);
    }
  return 0;
}

static int
lm9830_reset (int fd)
{
  lm9830_write_register (fd, 0x07, 0x08);
  usleep (100);
  lm9830_write_register (fd, 0x07, 0x00);
  usleep (100);

  return 0;
}
