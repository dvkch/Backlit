/* sane - Scanner Access Now Easy.

   Copyright (C) 2003 Oliver Rauch
   Copyright (C) 2003, 2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004-2013 Stéphane Voltz <stef.dev@free.fr>
   Copyright (C) 2005 Philipp Schmid <philipp8288@web.de>
   Copyright (C) 2005-2009 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
   Copyright (C) 2006 Laurent Charpentier <laurent_pubs@yahoo.com>
   Copyright (C) 2010 Chris Berry <s0457957@sms.ed.ac.uk> and Michael Rickmann <mrickma@gwdg.de>
                 for Plustek Opticbook 3600 support


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

#undef BACKEND_NAME
#define BACKEND_NAME genesys_gl841

#include "genesys_gl841.h"

/****************************************************************************
 Low level function
 ****************************************************************************/

/* ------------------------------------------------------------------------ */
/*                  Read and write RAM, registers and AFE                   */
/* ------------------------------------------------------------------------ */

/* Write to many registers */
/* Note: There is no known bulk register write,
   this function is sending single registers instead */
static SANE_Status
gl841_bulk_write_register (Genesys_Device * dev,
			   Genesys_Register_Set * reg, size_t elems)
{
  SANE_Status status = SANE_STATUS_GOOD;
  unsigned int i, c;
  uint8_t buffer[GENESYS_MAX_REGS * 2];

  /* handle differently sized register sets, reg[0x00] is the last one */
  i = 0;
  while ((i < elems) && (reg[i].address != 0))
    i++;

  elems = i;

  DBG (DBG_io, "gl841_bulk_write_register (elems = %lu)\n",
       (u_long) elems);

  for (i = 0; i < elems; i++) {

      buffer[i * 2 + 0] = reg[i].address;
      buffer[i * 2 + 1] = reg[i].value;

      DBG (DBG_io2, "reg[0x%02x] = 0x%02x\n", buffer[i * 2 + 0],
	   buffer[i * 2 + 1]);
  }

  for (i = 0; i < elems;) {
      c = elems - i;
      if (c > 32)  /*32 is max. checked that.*/
	  c = 32;
      status =
	  sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_BUFFER,
				 VALUE_SET_REGISTER, INDEX, c * 2, buffer + i * 2);
      if (status != SANE_STATUS_GOOD)
      {
	  DBG (DBG_error,
	       "gl841_bulk_write_register: failed while writing command: %s\n",
	       sane_strstatus (status));
	  return status;
      }

      i += c;
  }

  DBG (DBG_io, "gl841_bulk_write_register: wrote %lu registers\n",
       (u_long) elems);
  return status;
}

/* Write bulk data (e.g. shading, gamma) */
static SANE_Status
gl841_bulk_write_data (Genesys_Device * dev, uint8_t addr,
			       uint8_t * data, size_t len)
{
  SANE_Status status;
  size_t size;
  uint8_t outdata[8];

  DBG (DBG_io, "gl841_bulk_write_data writing %lu bytes\n",
       (u_long) len);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_bulk_write_data failed while setting register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  while (len)
    {
      if (len > BULKOUT_MAXSIZE)
	size = BULKOUT_MAXSIZE;
      else
	size = len;

      outdata[0] = BULK_OUT;
      outdata[1] = BULK_RAM;
      outdata[2] = VALUE_BUFFER & 0xff;
      outdata[3] = (VALUE_BUFFER >> 8) & 0xff;
      outdata[4] = (size & 0xff);
      outdata[5] = ((size >> 8) & 0xff);
      outdata[6] = ((size >> 16) & 0xff);
      outdata[7] = ((size >> 24) & 0xff);

      status =
	sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_BUFFER,
			       VALUE_BUFFER, INDEX, sizeof (outdata),
			       outdata);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl841_bulk_write_data failed while writing command: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = sanei_usb_write_bulk (dev->dn, data, &size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl841_bulk_write_data failed while writing bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "gl841_bulk_write_data wrote %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (len - size));

      len -= size;
      data += size;
    }

  DBG (DBG_io, "gl841_bulk_write_data: completed\n");

  return status;
}

/* for debugging transfer rate*/
/*
#include <sys/time.h>
static struct timeval start_time;
static void
starttime(){
    gettimeofday(&start_time,NULL);
}
static void
printtime(char *p) {
    struct timeval t;
    long long int dif;
    gettimeofday(&t,NULL);
    dif = t.tv_sec - start_time.tv_sec;
    dif = dif*1000000 + t.tv_usec - start_time.tv_usec;
    fprintf(stderr,"%s %lluµs\n",p,dif);
}
*/

/* Read bulk data (e.g. scanned data) */
static SANE_Status
gl841_bulk_read_data (Genesys_Device * dev, uint8_t addr,
			      uint8_t * data, size_t len)
{
  SANE_Status status;
  size_t size, target;
  uint8_t outdata[8], *buffer;

  DBG (DBG_io, "gl841_bulk_read_data: requesting %lu bytes\n",
       (u_long) len);

  if (len == 0)
      return SANE_STATUS_GOOD;

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_bulk_read_data failed while setting register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  outdata[0] = BULK_IN;
  outdata[1] = BULK_RAM;
  outdata[2] = VALUE_BUFFER & 0xff;
  outdata[3] = (VALUE_BUFFER >> 8) & 0xff;
  outdata[4] = (len & 0xff);
  outdata[5] = ((len >> 8) & 0xff);
  outdata[6] = ((len >> 16) & 0xff);
  outdata[7] = ((len >> 24) & 0xff);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_BUFFER,
			   VALUE_BUFFER, INDEX, sizeof (outdata), outdata);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_bulk_read_data failed while writing command: %s\n",
	   sane_strstatus (status));
      return status;
    }

  target = len;
  buffer = data;
  while (target)
    {
      if (target > BULKIN_MAXSIZE)
	size = BULKIN_MAXSIZE;
      else
	size = target;

      DBG (DBG_io2,
	   "gl841_bulk_read_data: trying to read %lu bytes of data\n",
	   (u_long) size);

      status = sanei_usb_read_bulk (dev->dn, data, &size);

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl841_bulk_read_data failed while reading bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "gl841_bulk_read_data read %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (target - size));

      target -= size;
      data += size;
    }


  if (DBG_LEVEL >= DBG_data && dev->binary!=NULL)
    {
      fwrite(buffer, len, 1, dev->binary);
    }
  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/* Set address for writing data */
static SANE_Status
gl841_set_buffer_address_gamma (Genesys_Device * dev, uint32_t addr)
{
  SANE_Status status;

  DBG (DBG_io, "gl841_set_buffer_address_gamma: setting address to 0x%05x\n",
       addr & 0xfffffff0);

  addr = addr >> 4;

  status = sanei_genesys_write_register (dev, 0x5c, (addr & 0xff));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_set_buffer_address_gamma: failed while writing low byte: %s\n",
	   sane_strstatus (status));
      return status;
    }

  addr = addr >> 8;
  status = sanei_genesys_write_register (dev, 0x5b, (addr & 0xff));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_set_buffer_address_gamma: failed while writing high byte: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_io, "gl841_set_buffer_address_gamma: completed\n");

  return status;
}

/* Write bulk data (e.g. gamma) */
GENESYS_STATIC SANE_Status
gl841_bulk_write_data_gamma (Genesys_Device * dev, uint8_t addr,
			 uint8_t * data, size_t len)
{
  SANE_Status status;
  size_t size;
  uint8_t outdata[8];

  DBG (DBG_io, "gl841_bulk_write_data_gamma writing %lu bytes\n",
       (u_long) len);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "genesys_bulk_write_data_gamma failed while setting register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  while (len)
    {
      if (len > BULKOUT_MAXSIZE)
	size = BULKOUT_MAXSIZE;
      else
	size = len;

      outdata[0] = BULK_OUT;
      outdata[1] = BULK_RAM;
      outdata[2] = 0x00;/* 0x82 works, too */
      outdata[3] = 0x00;
      outdata[4] = (size & 0xff);
      outdata[5] = ((size >> 8) & 0xff);
      outdata[6] = ((size >> 16) & 0xff);
      outdata[7] = ((size >> 24) & 0xff);

      status =
	sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_BUFFER,
			       VALUE_BUFFER, INDEX, sizeof (outdata),
			       outdata);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_bulk_write_data_gamma failed while writing command: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = sanei_usb_write_bulk (dev->dn, data, &size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "genesys_bulk_write_data_gamma failed while writing bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "genesys_bulk_write_data:gamma wrote %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (len - size));

      len -= size;
      data += size;
    }

  DBG (DBG_io, "genesys_bulk_write_data_gamma: completed\n");

  return status;
}


/****************************************************************************
 Mid level functions
 ****************************************************************************/

static SANE_Bool
gl841_get_fast_feed_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x02);
  if (r && (r->value & REG02_FASTFED))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl841_get_filter_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x04);
  if (r && (r->value & REG04_FILTER))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl841_get_lineart_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x04);
  if (r && (r->value & REG04_LINEART))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl841_get_bitset_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x04);
  if (r && (r->value & REG04_BITSET))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl841_get_gain4_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x06);
  if (r && (r->value & REG06_GAIN4))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl841_test_buffer_empty_bit (SANE_Byte val)
{
  if (val & REG41_BUFEMPTY)
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl841_test_motor_flag_bit (SANE_Byte val)
{
  if (val & REG41_MOTORENB)
    return SANE_TRUE;
  return SANE_FALSE;
}

/** copy sensor specific settings */
/* *dev  : device infos
   *regs : registers to be set
   extended : do extended set up
   half_ccd: set up for half ccd resolution
   all registers 08-0B, 10-1D, 52-59 are set up. They shouldn't
   appear anywhere else but in register_ini

Responsible for signals to CCD/CIS:
  CCD_CK1X (CK1INV(0x16),CKDIS(0x16),CKTOGGLE(0x18),CKDELAY(0x18),MANUAL1(0x1A),CK1MTGL(0x1C),CK1LOW(0x1D),CK1MAP(0x74,0x75,0x76),CK1NEG(0x7D))
  CCD_CK2X (CK2INV(0x16),CKDIS(0x16),CKTOGGLE(0x18),CKDELAY(0x18),MANUAL1(0x1A),CK1LOW(0x1D),CK1NEG(0x7D))
  CCD_CK3X (MANUAL3(0x1A),CK3INV(0x1A),CK3MTGL(0x1C),CK3LOW(0x1D),CK3MAP(0x77,0x78,0x79),CK3NEG(0x7D))
  CCD_CK4X (MANUAL3(0x1A),CK4INV(0x1A),CK4MTGL(0x1C),CK4LOW(0x1D),CK4MAP(0x7A,0x7B,0x7C),CK4NEG(0x7D))
  CCD_CPX  (CTRLHI(0x16),CTRLINV(0x16),CTRLDIS(0x16),CPH(0x72),CPL(0x73),CPNEG(0x7D))
  CCD_RSX  (CTRLHI(0x16),CTRLINV(0x16),CTRLDIS(0x16),RSH(0x70),RSL(0x71),RSNEG(0x7D))
  CCD_TGX  (TGINV(0x16),TGMODE(0x17),TGW(0x17),EXPR(0x10,0x11),TGSHLD(0x1D))
  CCD_TGG  (TGINV(0x16),TGMODE(0x17),TGW(0x17),EXPG(0x12,0x13),TGSHLD(0x1D))
  CCD_TGB  (TGINV(0x16),TGMODE(0x17),TGW(0x17),EXPB(0x14,0x15),TGSHLD(0x1D))
  LAMP_SW  (EXPR(0x10,0x11),XPA_SEL(0x03),LAMP_PWR(0x03),LAMPTIM(0x03),MTLLAMP(0x04),LAMPPWM(0x29))
  XPA_SW   (EXPG(0x12,0x13),XPA_SEL(0x03),LAMP_PWR(0x03),LAMPTIM(0x03),MTLLAMP(0x04),LAMPPWM(0x29))
  LAMP_B   (EXPB(0x14,0x15),LAMP_PWR(0x03))

other registers:
  CISSET(0x01),CNSET(0x18),DCKSEL(0x18),SCANMOD(0x18),EXPDMY(0x19),LINECLP(0x1A),CKAREA(0x1C),TGTIME(0x1C),LINESEL(0x1E),DUMMY(0x34)

Responsible for signals to AFE:
  VSMP  (VSMP(0x58),VSMPW(0x58))
  BSMP  (BSMP(0x59),BSMPW(0x59))

other register settings depending on this:
  RHI(0x52),RLOW(0x53),GHI(0x54),GLOW(0x55),BHI(0x56),BLOW(0x57),

*/
static void
sanei_gl841_setup_sensor (Genesys_Device * dev,
			  Genesys_Register_Set * regs,
			  SANE_Bool extended, SANE_Bool half_ccd)
{
  Genesys_Register_Set *r;
  int i;

  DBG (DBG_proc, "gl841_setup_sensor\n");

  /* that one is tricky at least ....*/
  r = sanei_genesys_get_address (regs, 0x70);
  for (i = 0; i < 4; i++, r++)
    r->value = dev->sensor.regs_0x08_0x0b[i];

  r = sanei_genesys_get_address (regs, 0x16);
  for (i = 0x06; i < 0x0a; i++, r++)
    r->value = dev->sensor.regs_0x10_0x1d[i];

  r = sanei_genesys_get_address (regs, 0x1a);
  for (i = 0x0a; i < 0x0e; i++, r++)
    r->value = dev->sensor.regs_0x10_0x1d[i];

  r = sanei_genesys_get_address (regs, 0x52);
  for (i = 0; i < 9; i++, r++)
    r->value = dev->sensor.regs_0x52_0x5e[i];

  /* don't go any further if no extended setup */
  if (!extended)
    return;

  /* todo : add more CCD types if needed */
  /* we might want to expand the Sensor struct to have these
     2 kind of settings */
  if (dev->model->ccd_type == CCD_5345)
    {
      if (half_ccd)
	{
	  /* settings for CCD used at half is max resolution */
	  r = sanei_genesys_get_address (regs, 0x70);
	  r->value = 0x00;
	  r = sanei_genesys_get_address (regs, 0x71);
	  r->value = 0x05;
	  r = sanei_genesys_get_address (regs, 0x72);
	  r->value = 0x06;
	  r = sanei_genesys_get_address (regs, 0x73);
	  r->value = 0x08;
	  r = sanei_genesys_get_address (regs, 0x18);
	  r->value = 0x28;
	  r = sanei_genesys_get_address (regs, 0x58);
	  r->value = 0x80 | (r->value & 0x03);	/* VSMP=16 */
	}
      else
	{
	  /* swap latch times */
	  r = sanei_genesys_get_address (regs, 0x18);
	  r->value = 0x30;
	  r = sanei_genesys_get_address (regs, 0x52);
	  for (i = 0; i < 6; i++, r++)
	    r->value = dev->sensor.regs_0x52_0x5e[(i + 3) % 6];
	  r = sanei_genesys_get_address (regs, 0x58);
	  r->value = 0x20 | (r->value & 0x03);	/* VSMP=4 */
	}
      return;
    }

  if (dev->model->ccd_type == CCD_HP2300)
    {
      /* settings for CCD used at half is max resolution */
      if (half_ccd)
	{
	  r = sanei_genesys_get_address (regs, 0x70);
	  r->value = 0x16;
	  r = sanei_genesys_get_address (regs, 0x71);
	  r->value = 0x00;
	  r = sanei_genesys_get_address (regs, 0x72);
	  r->value = 0x01;
	  r = sanei_genesys_get_address (regs, 0x73);
	  r->value = 0x03;
	  /* manual clock programming */
	  r = sanei_genesys_get_address (regs, 0x1d);
	  r->value |= 0x80;
	}
      else
	{
	  r = sanei_genesys_get_address (regs, 0x70);
	  r->value = 1;
	  r = sanei_genesys_get_address (regs, 0x71);
	  r->value = 3;
	  r = sanei_genesys_get_address (regs, 0x72);
	  r->value = 4;
	  r = sanei_genesys_get_address (regs, 0x73);
	  r->value = 6;
	}
      r = sanei_genesys_get_address (regs, 0x58);
      r->value = 0x80 | (r->value & 0x03);	/* VSMP=16 */
      return;
    }
}

/** Test if the ASIC works
 */
/*TODO: make this functional*/
static SANE_Status
sanei_gl841_asic_test (Genesys_Device * dev)
{
  SANE_Status status;
  uint8_t val;
  uint8_t *data;
  uint8_t *verify_data;
  size_t size, verify_size;
  unsigned int i;

  DBG (DBG_proc, "sanei_gl841_asic_test\n");

  return SANE_STATUS_INVAL;

  /* set and read exposure time, compare if it's the same */
  status = sanei_genesys_write_register (dev, 0x38, 0xde);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_gl841_asic_test: failed to write register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_write_register (dev, 0x39, 0xad);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_gl841_asic_test: failed to write register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_read_register (dev, 0x38, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_gl841_asic_test: failed to read register: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (val != 0xde)		/* value of register 0x38 */
    {
      DBG (DBG_error,
	   "sanei_gl841_asic_test: register contains invalid value\n");
      return SANE_STATUS_IO_ERROR;
    }

  status = sanei_genesys_read_register (dev, 0x39, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_gl841_asic_test: failed to read register: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (val != 0xad)		/* value of register 0x39 */
    {
      DBG (DBG_error,
	   "sanei_gl841_asic_test: register contains invalid value\n");
      return SANE_STATUS_IO_ERROR;
    }

  /* ram test: */
  size = 0x40000;
  verify_size = size + 0x80;
  /* todo: looks like the read size must be a multiple of 128?
     otherwise the read doesn't succeed the second time after the scanner has
     been plugged in. Very strange. */

  data = (uint8_t *) malloc (size);
  if (!data)
    {
      DBG (DBG_error, "sanei_gl841_asic_test: could not allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  verify_data = (uint8_t *) malloc (verify_size);
  if (!verify_data)
    {
      free (data);
      DBG (DBG_error, "sanei_gl841_asic_test: could not allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  for (i = 0; i < (size - 1); i += 2)
    {
      data[i] = i / 512;
      data[i + 1] = (i / 2) % 256;
    }

  status = sanei_genesys_set_buffer_address (dev, 0x0000);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_gl841_asic_test: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

/*  status = gl841_bulk_write_data (dev, 0x3c, data, size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_gl841_asic_test: failed to bulk write data: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
      }*/

  status = sanei_genesys_set_buffer_address (dev, 0x0000);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "sanei_gl841_asic_test: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

  status =
    gl841_bulk_read_data (dev, 0x45, (uint8_t *) verify_data,
				  verify_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "sanei_gl841_asic_test: failed to bulk read data: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

  /* todo: why i + 2 ? */
  for (i = 0; i < size; i++)
    {
      if (verify_data[i] != data[i])
	{
	  DBG (DBG_error, "sanei_gl841_asic_test: data verification error\n");
	  DBG (DBG_info, "0x%.8x: got %.2x %.2x %.2x %.2x, expected %.2x %.2x %.2x %.2x\n",
	       i,
	       verify_data[i],
	       verify_data[i+1],
	       verify_data[i+2],
	       verify_data[i+3],
	       data[i],
	       data[i+1],
	       data[i+2],
	       data[i+3]);
	  free (data);
	  free (verify_data);
	  return SANE_STATUS_IO_ERROR;
	}
    }

  free (data);
  free (verify_data);

  DBG (DBG_info, "sanei_gl841_asic_test: completed\n");

  return SANE_STATUS_GOOD;
}

/* returns the max register bulk size */
static int
gl841_bulk_full_size (void)
{
  return GENESYS_GL841_MAX_REGS;
}

/*
 * Set all registers LiDE 80 to default values
 * (function called only once at the beginning)
 * we are doing a special case to ease development
 */
static void
gl841_init_lide80 (Genesys_Device * dev)
{
  uint8_t val;
  int index=0;

  INITREG (0x01, 0x82); /* 0x02 = SHDAREA  and no CISSET ! */
  INITREG (0x02, 0x10);
  INITREG (0x03, 0x50);
  INITREG (0x04, 0x02);
  INITREG (0x05, 0x4c); /* 1200 DPI */
  INITREG (0x06, 0x38); /* 0x38 scanmod=1, pwrbit, GAIN4 */
  INITREG (0x07, 0x00);
  INITREG (0x08, 0x00);
  INITREG (0x09, 0x11);
  INITREG (0x0a, 0x00);

  INITREG (0x10, 0x40);
  INITREG (0x11, 0x00);
  INITREG (0x12, 0x40);
  INITREG (0x13, 0x00);
  INITREG (0x14, 0x40);
  INITREG (0x15, 0x00);
  INITREG (0x16, 0x00);
  INITREG (0x17, 0x01);
  INITREG (0x18, 0x00);
  INITREG (0x19, 0x06);
  INITREG (0x1a, 0x00);
  INITREG (0x1b, 0x00);
  INITREG (0x1c, 0x00);
  INITREG (0x1d, 0x04);
  INITREG (0x1e, 0x10);
  INITREG (0x1f, 0x04);
  INITREG (0x20, 0x02);
  INITREG (0x21, 0x10);
  INITREG (0x22, 0x20);
  INITREG (0x23, 0x20);
  INITREG (0x24, 0x10);
  INITREG (0x25, 0x00);
  INITREG (0x26, 0x00);
  INITREG (0x27, 0x00);

  INITREG (0x29, 0xff);

  INITREG (0x2c, dev->sensor.optical_res>>8);
  INITREG (0x2d, dev->sensor.optical_res & 0xff);
  INITREG (0x2e, 0x80);
  INITREG (0x2f, 0x80);
  INITREG (0x30, 0x00);
  INITREG (0x31, 0x10);
  INITREG (0x32, 0x15);
  INITREG (0x33, 0x0e);
  INITREG (0x34, 0x40);
  INITREG (0x35, 0x00);
  INITREG (0x36, 0x2a);
  INITREG (0x37, 0x30);
  INITREG (0x38, 0x2a);
  INITREG (0x39, 0xf8);

  INITREG (0x3d, 0x00);
  INITREG (0x3e, 0x00);
  INITREG (0x3f, 0x00);

  INITREG (0x52, 0x03);
  INITREG (0x53, 0x07);
  INITREG (0x54, 0x00);
  INITREG (0x55, 0x00);
  INITREG (0x56, 0x00);
  INITREG (0x57, 0x00);
  INITREG (0x58, 0x29);
  INITREG (0x59, 0x69);
  INITREG (0x5a, 0x55);

  INITREG (0x5d, 0x20);
  INITREG (0x5e, 0x41);
  INITREG (0x5f, 0x40);
  INITREG (0x60, 0x00);
  INITREG (0x61, 0x00);
  INITREG (0x62, 0x00);
  INITREG (0x63, 0x00);
  INITREG (0x64, 0x00);
  INITREG (0x65, 0x00);
  INITREG (0x66, 0x00);
  INITREG (0x67, 0x40);
  INITREG (0x68, 0x40);
  INITREG (0x69, 0x20);
  INITREG (0x6a, 0x20);
  INITREG (0x6c, dev->gpo.value[0]);
  INITREG (0x6d, dev->gpo.value[1]);
  INITREG (0x6e, dev->gpo.enable[0]);
  INITREG (0x6f, dev->gpo.enable[1]);
  INITREG (0x70, 0x00);
  INITREG (0x71, 0x05);
  INITREG (0x72, 0x07);
  INITREG (0x73, 0x09);
  INITREG (0x74, 0x00);
  INITREG (0x75, 0x01);
  INITREG (0x76, 0xff);
  INITREG (0x77, 0x00);
  INITREG (0x78, 0x0f);
  INITREG (0x79, 0xf0);
  INITREG (0x7a, 0xf0);
  INITREG (0x7b, 0x00);
  INITREG (0x7c, 0x1e);
  INITREG (0x7d, 0x11);
  INITREG (0x7e, 0x00);
  INITREG (0x7f, 0x50);
  INITREG (0x80, 0x00);
  INITREG (0x81, 0x00);
  INITREG (0x82, 0x0f);
  INITREG (0x83, 0x00);
  INITREG (0x84, 0x0e);
  INITREG (0x85, 0x00);
  INITREG (0x86, 0x0d);
  INITREG (0x87, 0x02);
  INITREG (0x88, 0x00);
  INITREG (0x89, 0x00);

  /* specific scanner settings, clock and gpio first */
  sanei_genesys_read_register (dev, REG6B, &val);
  sanei_genesys_write_register (dev, REG6B, 0x0c);
  sanei_genesys_write_register (dev, 0x06, 0x10);
  sanei_genesys_write_register (dev, REG6E, 0x6d);
  sanei_genesys_write_register (dev, REG6F, 0x80);
  sanei_genesys_write_register (dev, REG6B, 0x0e);
  sanei_genesys_read_register (dev, REG6C, &val);
  sanei_genesys_write_register (dev, REG6C, 0x00);
  sanei_genesys_read_register (dev, REG6D, &val);
  sanei_genesys_write_register (dev, REG6D, 0x8f);
  sanei_genesys_read_register (dev, REG6B, &val);
  sanei_genesys_write_register (dev, REG6B, 0x0e);
  sanei_genesys_read_register (dev, REG6B, &val);
  sanei_genesys_write_register (dev, REG6B, 0x0e);
  sanei_genesys_read_register (dev, REG6B, &val);
  sanei_genesys_write_register (dev, REG6B, 0x0a);
  sanei_genesys_read_register (dev, REG6B, &val);
  sanei_genesys_write_register (dev, REG6B, 0x02);
  sanei_genesys_read_register (dev, REG6B, &val);
  sanei_genesys_write_register (dev, REG6B, 0x06);

  sanei_genesys_write_0x8c (dev, 0x10, 0x94);
  sanei_genesys_write_register (dev, 0x09, 0x10);

  /* set up GPIO : no address, so no bulk write, doesn't written directly either ? */
  /*
  dev->reg[reg_0x6c].value = dev->gpo.value[0];
  dev->reg[reg_0x6d].value = dev->gpo.value[1];
  dev->reg[reg_0x6e].value = dev->gpo.enable[0];
  dev->reg[reg_0x6f].value = dev->gpo.enable[1]; */

  dev->reg[reg_0x6b].value |= REG6B_GPO18;
  dev->reg[reg_0x6b].value &= ~REG6B_GPO17;

  sanei_gl841_setup_sensor (dev, dev->reg, 0, 0);
}

/*
 * Set all registers to default values
 * (function called only once at the beginning)
 */
static void
gl841_init_registers (Genesys_Device * dev)
{
  int nr, addr;

  DBG (DBG_proc, "gl841_init_registers\n");

  nr = 0;
  memset (dev->reg, 0, GENESYS_MAX_REGS * sizeof (Genesys_Register_Set));
  if (strcmp (dev->model->name, "canon-lide-80") == 0)
    {
      gl841_init_lide80(dev);
      return ;
    }

  for (addr = 1; addr <= 0x0a; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x10; addr <= 0x27; addr++)
    dev->reg[nr++].address = addr;
  dev->reg[nr++].address = 0x29;
  for (addr = 0x2c; addr <= 0x39; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x3d; addr <= 0x3f; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x52; addr <= 0x5a; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x5d; addr <= 0x87; addr++)
    dev->reg[nr++].address = addr;


  dev->reg[reg_0x01].value = 0x20;	/* (enable shading), CCD, color, 1M */
  if (dev->model->is_cis == SANE_TRUE)
    {
      dev->reg[reg_0x01].value |= REG01_CISSET;
    }
  else
    {
      dev->reg[reg_0x01].value &= ~REG01_CISSET;
    }

  dev->reg[reg_0x02].value = 0x30 /*0x38 */ ;	/* auto home, one-table-move, full step */
  dev->reg[reg_0x02].value |= REG02_AGOHOME;
  dev->reg[reg_0x02].value |= REG02_MTRPWR;
  dev->reg[reg_0x02].value |= REG02_FASTFED;

  dev->reg[reg_0x03].value = 0x1f /*0x17 */ ;	/* lamp on */
  dev->reg[reg_0x03].value |= REG03_AVEENB;

  if (dev->model->ccd_type == CCD_PLUSTEK_3600)  /* AD front end */
    {
      dev->reg[reg_0x04].value  = (2 << REG04S_AFEMOD) | 0x02;
    }
  else /* Wolfson front end */
    {
      dev->reg[reg_0x04].value |= 1 << REG04S_AFEMOD;
    }

  dev->reg[reg_0x05].value = 0x00;	/* disable gamma, 24 clocks/pixel */
  if (dev->sensor.sensor_pixels < 0x1500)
    dev->reg[reg_0x05].value |= REG05_DPIHW_600;
  else if (dev->sensor.sensor_pixels < 0x2a80)
    dev->reg[reg_0x05].value |= REG05_DPIHW_1200;
  else if (dev->sensor.sensor_pixels < 0x5400)
    dev->reg[reg_0x05].value |= REG05_DPIHW_2400;
  else
    {
      dev->reg[reg_0x05].value |= REG05_DPIHW_2400;
      DBG (DBG_warn,
	   "gl841_init_registers: Cannot handle sensor pixel count %d\n",
	   dev->sensor.sensor_pixels);
    }


  dev->reg[reg_0x06].value |= REG06_PWRBIT;
  dev->reg[reg_0x06].value |= REG06_GAIN4;

  /* XP300 CCD needs different clock and clock/pixels values */
  if (dev->model->ccd_type != CCD_XP300 && dev->model->ccd_type != CCD_DP685
                                        && dev->model->ccd_type != CCD_PLUSTEK_3600)
    {
      dev->reg[reg_0x06].value |= 0 << REG06S_SCANMOD;
      dev->reg[reg_0x09].value |= 1 << REG09S_CLKSET;
    }
  else
    {
      dev->reg[reg_0x06].value |= 0x05 << REG06S_SCANMOD; /* 15 clocks/pixel */
      dev->reg[reg_0x09].value = 0; /* 24 MHz CLKSET */
    }

  dev->reg[reg_0x1e].value = 0xf0;	/* watch-dog time */

  dev->reg[reg_0x17].value |= 1 << REG17S_TGW;

  dev->reg[reg_0x19].value = 0x50;

  dev->reg[reg_0x1d].value |= 1 << REG1DS_TGSHLD;

  dev->reg[reg_0x1e].value |= 1 << REG1ES_WDTIME;

/*SCANFED*/
  dev->reg[reg_0x1f].value = 0x01;

/*BUFSEL*/
  dev->reg[reg_0x20].value = 0x20;

/*LAMPPWM*/
  dev->reg[reg_0x29].value = 0xff;

/*BWHI*/
  dev->reg[reg_0x2e].value = 0x80;

/*BWLOW*/
  dev->reg[reg_0x2f].value = 0x80;

/*LPERIOD*/
  dev->reg[reg_0x38].value = 0x4f;
  dev->reg[reg_0x39].value = 0xc1;

/*VSMPW*/
  dev->reg[reg_0x58].value |= 3 << REG58S_VSMPW;

/*BSMPW*/
  dev->reg[reg_0x59].value |= 3 << REG59S_BSMPW;

/*RLCSEL*/
  dev->reg[reg_0x5a].value |= REG5A_RLCSEL;

/*STOPTIM*/
  dev->reg[reg_0x5e].value |= 0x2 << REG5ES_STOPTIM;

  sanei_gl841_setup_sensor (dev, dev->reg, 0, 0);

  /* set up GPIO */
  dev->reg[reg_0x6c].value = dev->gpo.value[0];
  dev->reg[reg_0x6d].value = dev->gpo.value[1];
  dev->reg[reg_0x6e].value = dev->gpo.enable[0];
  dev->reg[reg_0x6f].value = dev->gpo.enable[1];

  /* TODO there is a switch calling to be written here */
  if (dev->model->gpo_type == GPO_CANONLIDE35)
    {
      dev->reg[reg_0x6b].value |= REG6B_GPO18;
      dev->reg[reg_0x6b].value &= ~REG6B_GPO17;
    }
  if (dev->model->gpo_type == GPO_CANONLIDE80)
    {
      dev->reg[reg_0x6b].value |= REG6B_GPO18;
      dev->reg[reg_0x6b].value &= ~REG6B_GPO17;
    }

  if (dev->model->gpo_type == GPO_XP300)
    {
      dev->reg[reg_0x6b].value |= REG6B_GPO17;
    }

  if (dev->model->gpo_type == GPO_DP685)
    {
      /* REG6B_GPO18 lights on green led */
      dev->reg[reg_0x6b].value |= REG6B_GPO17|REG6B_GPO18;
    }

  DBG (DBG_proc, "gl841_init_registers complete\n");
}

/* Send slope table for motor movement
   slope_table in machine byte order
 */
GENESYS_STATIC SANE_Status
gl841_send_slope_table (Genesys_Device * dev, int table_nr,
			      uint16_t * slope_table, int steps)
{
  int dpihw;
  int start_address;
  SANE_Status status;
  uint8_t *table;
  char msg[4000];
/*#ifdef WORDS_BIGENDIAN*/
  int i;
/*#endif*/

  DBG (DBG_proc, "gl841_send_slope_table (table_nr = %d, steps = %d)\n",
       table_nr, steps);

  dpihw = dev->reg[reg_0x05].value >> 6;

  if (dpihw == 0)		/* 600 dpi */
    start_address = 0x08000;
  else if (dpihw == 1)		/* 1200 dpi */
    start_address = 0x10000;
  else if (dpihw == 2)		/* 2400 dpi */
    start_address = 0x20000;
  else				/* reserved */
    return SANE_STATUS_INVAL;

/*#ifdef WORDS_BIGENDIAN*/
  table = (uint8_t*)malloc(steps * 2);
  for(i = 0; i < steps; i++) {
      table[i * 2] = slope_table[i] & 0xff;
      table[i * 2 + 1] = slope_table[i] >> 8;
  }
/*#else
  table = (uint8_t*)slope_table;
  #endif*/
  if (DBG_LEVEL >= DBG_io)
    {
      sprintf (msg, "write slope %d (%d)=", table_nr, steps);
      for (i = 0; i < steps; i++)
	{
	  sprintf (msg+strlen(msg), ",%d", slope_table[i]);
	}
      DBG (DBG_io, "%s: %s\n", __FUNCTION__, msg);
    }

  status =
    sanei_genesys_set_buffer_address (dev, start_address + table_nr * 0x200);
  if (status != SANE_STATUS_GOOD)
    {
/*#ifdef WORDS_BIGENDIAN*/
      free(table);
/*#endif*/
      DBG (DBG_error,
	   "gl841_send_slope_table: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status =
    gl841_bulk_write_data (dev, 0x3c, (uint8_t *) table,
				   steps * 2);
  if (status != SANE_STATUS_GOOD)
    {
/*#ifdef WORDS_BIGENDIAN*/
      free(table);
/*#endif*/
      DBG (DBG_error,
	   "gl841_send_slope_table: failed to send slope table: %s\n",
	   sane_strstatus (status));
      return status;
    }

/*#ifdef WORDS_BIGENDIAN*/
  free(table);
/*#endif*/
  DBG (DBG_proc, "gl841_send_slope_table: completed\n");
  return status;
}

static SANE_Status
gl841_set_lide80_fe (Genesys_Device * dev, uint8_t set)
{
  SANE_Status status = SANE_STATUS_GOOD;

  DBGSTART;

  if (set == AFE_INIT)
    {
      DBG (DBG_proc, "%s(): setting DAC %u\n", __FUNCTION__,
	   dev->model->dac_type);

      /* sets to default values */
      sanei_genesys_init_fe (dev);

      /* write them to analog frontend */
      status = sanei_genesys_fe_write_data (dev, 0x00, dev->frontend.reg[0]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "%s: writing reg 0x00 failed: %s\n", __FUNCTION__,
	       sane_strstatus (status));
	  return status;
	}
      status = sanei_genesys_fe_write_data (dev, 0x03, dev->frontend.reg[1]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "%s: writing reg 0x03 failed: %s\n", __FUNCTION__,
	       sane_strstatus (status));
	  return status;
	}
      status = sanei_genesys_fe_write_data (dev, 0x06, dev->frontend.reg[2]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "%s: writing reg 0x06 failed: %s\n", __FUNCTION__,
	       sane_strstatus (status));
	  return status;
	}
    }

  if (set == AFE_SET)
    {
      status = sanei_genesys_fe_write_data (dev, 0x00, dev->frontend.reg[0]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "%s: writing reg 0x00 failed: %s\n", __FUNCTION__,
	       sane_strstatus (status));
	  return status;
	}
      status = sanei_genesys_fe_write_data (dev, 0x06, dev->frontend.offset[0]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "%s: writing offset failed: %s\n", __FUNCTION__,
	       sane_strstatus (status));
	  return status;
	}
      status = sanei_genesys_fe_write_data (dev, 0x03, dev->frontend.gain[0]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "%s: writing gain failed: %s\n", __FUNCTION__,
	       sane_strstatus (status));
	  return status;
	}
    }

  return status;
  DBGCOMPLETED;
}

/* Set values of Analog Device type frontend */
static SANE_Status
gl841_set_ad_fe (Genesys_Device * dev, uint8_t set)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int i;

  /* special case for LiDE 80 analog frontend */
  if(dev->model->dac_type==DAC_CANONLIDE80)
    {
      return gl841_set_lide80_fe(dev, set);
    }

  DBG (DBG_proc, "gl841_set_ad_fe(): start\n");
  if (set == AFE_INIT)
    {
      DBG (DBG_proc, "gl841_set_ad_fe(): setting DAC %u\n",
	   dev->model->dac_type);

      /* sets to default values */
      sanei_genesys_init_fe (dev);

      /* write them to analog frontend */
      status = sanei_genesys_fe_write_data (dev, 0x00, dev->frontend.reg[0]);
      if (status != SANE_STATUS_GOOD)
            {
      	DBG (DBG_error, "gl841_set_ad_fe: writing reg 0x00 failed: %s\n",
      	     sane_strstatus (status));
      	return status;
            }

      status = sanei_genesys_fe_write_data (dev, 0x01, dev->frontend.reg[1]);
      if (status != SANE_STATUS_GOOD)
            {
    	DBG (DBG_error, "gl841_set_ad_fe: writing reg 0x01 failed: %s\n",
            sane_strstatus (status));
        return status;
            }

      for (i = 0; i < 6; i++)
        {
    	status =
    	  sanei_genesys_fe_write_data (dev, 0x02 + i, 0x00);
    	if (status != SANE_STATUS_GOOD)
    	  {
    	    DBG (DBG_error,
    		 "gl841_set_ad_fe: writing sign[%d] failed: %s\n", 0x02 + i,
    		 sane_strstatus (status));
    	    return status;
    	  }
        }
    }
  if (set == AFE_SET)
    {
      /* write them to analog frontend */
      status = sanei_genesys_fe_write_data (dev, 0x00, dev->frontend.reg[0]);
      if (status != SANE_STATUS_GOOD)
            {
      	DBG (DBG_error, "gl841_set_ad_fe: writing reg 0x00 failed: %s\n",
      	     sane_strstatus (status));
      	return status;
            }

      status = sanei_genesys_fe_write_data (dev, 0x01, dev->frontend.reg[1]);
      if (status != SANE_STATUS_GOOD)
            {
    	DBG (DBG_error, "gl841_set_ad_fe: writing reg 0x01 failed: %s\n",
            sane_strstatus (status));
        return status;
            }

      /* Write fe 0x02 (red gain)*/
      status = sanei_genesys_fe_write_data (dev, 0x02, dev->frontend.gain[0]);
      if (status != SANE_STATUS_GOOD)
            {
    	DBG (DBG_error, "gl841_set_ad_fe: writing fe 0x02 (gain r) fail: %s\n",
            sane_strstatus (status));
        return status;
            }

      /* Write fe 0x03 (green gain)*/
      status = sanei_genesys_fe_write_data (dev, 0x03, dev->frontend.gain[1]);
      if (status != SANE_STATUS_GOOD)
            {
        DBG (DBG_error, "gl841_set_ad_fe: writing fe 0x03 (gain g) fail: %s\n",
            sane_strstatus (status));
        return status;
            }

      /* Write fe 0x04 (blue gain)*/
      status = sanei_genesys_fe_write_data (dev, 0x04, dev->frontend.gain[2]);
      if (status != SANE_STATUS_GOOD)
            {
        DBG (DBG_error, "gl841_set_ad_fe: writing fe 0x04 (gain b) fail: %s\n",
            sane_strstatus (status));
        return status;
            }

      /* Write fe 0x05 (red offset)*/
      status =
    	  sanei_genesys_fe_write_data (dev, 0x05, dev->frontend.offset[0]);
      if (status != SANE_STATUS_GOOD)
            {
        DBG (DBG_error, "gl841_set_ad_fe: write fe 0x05 (offset r) fail: %s\n",
            sane_strstatus (status));
        return status;
            }

      /* Write fe 0x06 (green offset)*/
      status =
    	  sanei_genesys_fe_write_data (dev, 0x06, dev->frontend.offset[1]);
      if (status != SANE_STATUS_GOOD)
            {
        DBG (DBG_error, "gl841_set_ad_fe: write fe 0x06 (offset g) fail: %s\n",
            sane_strstatus (status));
        return status;
            }

      /* Write fe 0x07 (blue offset)*/
      status =
    	  sanei_genesys_fe_write_data (dev, 0x07, dev->frontend.offset[2]);
      if (status != SANE_STATUS_GOOD)
            {
        DBG (DBG_error, "gl841_set_ad_fe: write fe 0x07 (offset b) fail: %s\n",
            sane_strstatus (status));
        return status;
            }
          }
  DBG (DBG_proc, "gl841_set_ad_fe(): end\n");

  return status;
}

/* Set values of analog frontend */
static SANE_Status
gl841_set_fe (Genesys_Device * dev, uint8_t set)
{
  SANE_Status status;
  int i;

  DBG (DBG_proc, "gl841_set_fe (%s)\n",
       set == AFE_INIT ? "init" : set == AFE_SET ? "set" : set ==
       AFE_POWER_SAVE ? "powersave" : "huh?");

  /* Analog Device type frontend */
  if ((dev->reg[reg_0x04].value & REG04_FESET) == 0x02)
    {
      return gl841_set_ad_fe (dev, set);
    }

  if ((dev->reg[reg_0x04].value & REG04_FESET) != 0x00)
    {
      DBG (DBG_proc, "gl841_set_fe(): unsupported frontend type %d\n",
	   dev->reg[reg_0x04].value & REG04_FESET);
      return SANE_STATUS_UNSUPPORTED;
    }

  if (set == AFE_INIT)
    {
      DBG (DBG_proc, "gl841_set_fe(): setting DAC %u\n",
	   dev->model->dac_type);
      sanei_genesys_init_fe (dev);

      /* reset only done on init */
      status = sanei_genesys_fe_write_data (dev, 0x04, 0x80);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl841_set_fe: reset fe failed: %s\n",
	       sane_strstatus (status));
	  return status;
          /*
	  if (dev->model->ccd_type == CCD_HP2300
	      || dev->model->ccd_type == CCD_HP2400)
	    {
	      val = 0x07;
	      status =
		sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT,
				       REQUEST_REGISTER, GPIO_OUTPUT_ENABLE,
				       INDEX, 1, &val);
	      if (status != SANE_STATUS_GOOD)
		{
		  DBG (DBG_error,
		       "gl841_set_fe failed resetting frontend: %s\n",
		       sane_strstatus (status));
		  return status;
		}
	    }*/
	}
      DBG (DBG_proc, "gl841_set_fe(): frontend reset complete\n");
    }


  if (set == AFE_POWER_SAVE)
    {
      status = sanei_genesys_fe_write_data (dev, 0x01, 0x02);
      if (status != SANE_STATUS_GOOD)
	DBG (DBG_error, "gl841_set_fe: writing data failed: %s\n",
	     sane_strstatus (status));
      return status;
    }

  /* todo :  base this test on cfg reg3 or a CCD family flag to be created */
  /*if (dev->model->ccd_type!=CCD_HP2300 && dev->model->ccd_type!=CCD_HP2400) */
  {

    status = sanei_genesys_fe_write_data (dev, 0x00, dev->frontend.reg[0]);
    if (status != SANE_STATUS_GOOD)
      {
	DBG (DBG_error, "gl841_set_fe: writing reg0 failed: %s\n",
	     sane_strstatus (status));
	return status;
      }
    status = sanei_genesys_fe_write_data (dev, 0x02, dev->frontend.reg[2]);
    if (status != SANE_STATUS_GOOD)
      {
	DBG (DBG_error, "gl841_set_fe: writing reg2 failed: %s\n",
	     sane_strstatus (status));
	return status;
      }
  }

  status = sanei_genesys_fe_write_data (dev, 0x01, dev->frontend.reg[1]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl841_set_fe: writing reg1 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_fe_write_data (dev, 0x03, dev->frontend.reg[3]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl841_set_fe: writing reg3 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_fe_write_data (dev, 0x06, dev->frontend.reg2[0]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl841_set_fe: writing reg6 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_fe_write_data (dev, 0x08, dev->frontend.reg2[1]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl841_set_fe: writing reg8 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_fe_write_data (dev, 0x09, dev->frontend.reg2[2]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl841_set_fe: writing reg9 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  for (i = 0; i < 3; i++)
      {
	status =
	  sanei_genesys_fe_write_data (dev, 0x24 + i, dev->frontend.sign[i]);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (DBG_error,
		 "gl841_set_fe: writing sign[%d] failed: %s\n", i,
		 sane_strstatus (status));
	    return status;
	  }

	status =
	  sanei_genesys_fe_write_data (dev, 0x28 + i, dev->frontend.gain[i]);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (DBG_error,
		 "gl841_set_fe: writing gain[%d] failed: %s\n", i,
		 sane_strstatus (status));
	    return status;
	  }

	status =
	  sanei_genesys_fe_write_data (dev, 0x20 + i,
				       dev->frontend.offset[i]);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (DBG_error,
		 "gl841_set_fe: writing offset[%d] failed: %s\n", i,
		 sane_strstatus (status));
	    return status;
	  }
      }


  DBG (DBG_proc, "gl841_set_fe: completed\n");

  return SANE_STATUS_GOOD;
}

#define MOTOR_ACTION_FEED       1
#define MOTOR_ACTION_GO_HOME    2
#define MOTOR_ACTION_HOME_FREE  3

/** @brief turn off motor
 *
 */
static SANE_Status
gl841_init_motor_regs_off(Genesys_Register_Set * reg,
			  unsigned int scan_lines)
{
    unsigned int feedl;
    Genesys_Register_Set * r;

    DBG (DBG_proc, "gl841_init_motor_regs_off : scan_lines=%d\n",
	 scan_lines);

    feedl = 2;

    r = sanei_genesys_get_address (reg, 0x3d);
    r->value = (feedl >> 16) & 0xf;
    r = sanei_genesys_get_address (reg, 0x3e);
    r->value = (feedl >> 8) & 0xff;
    r = sanei_genesys_get_address (reg, 0x3f);
    r->value = feedl & 0xff;
    r = sanei_genesys_get_address (reg, 0x5e);
    r->value &= ~0xe0;

    r = sanei_genesys_get_address (reg, 0x25);
    r->value = (scan_lines >> 16) & 0xf;
    r = sanei_genesys_get_address (reg, 0x26);
    r->value = (scan_lines >> 8) & 0xff;
    r = sanei_genesys_get_address (reg, 0x27);
    r->value = scan_lines & 0xff;

    r = sanei_genesys_get_address (reg, 0x02);
    r->value &= ~0x01; /*LONGCURV OFF*/
    r->value &= ~0x80; /*NOT_HOME OFF*/

    r->value &= ~0x10;

    r->value &= ~0x06;

    r->value &= ~0x08;

    r->value &= ~0x20;

    r->value &= ~0x40;

    r = sanei_genesys_get_address (reg, 0x67);
    r->value = 0x3f;

    r = sanei_genesys_get_address (reg, 0x68);
    r->value = 0x3f;

    r = sanei_genesys_get_address (reg, REG_STEPNO);
    r->value = 0;

    r = sanei_genesys_get_address (reg, REG_FASTNO);
    r->value = 0;

    r = sanei_genesys_get_address (reg, 0x69);
    r->value = 0;

    r = sanei_genesys_get_address (reg, 0x6a);
    r->value = 0;

    r = sanei_genesys_get_address (reg, 0x5f);
    r->value = 0;


    DBGCOMPLETED;
    return SANE_STATUS_GOOD;
}

/** @brief write motor table frequency
 * Write motor frequency data table.
 * @param dev device to set up motor
 * @param ydpi motor target resolution
 * @return SANE_STATUS_GOOD on success
 */
GENESYS_STATIC SANE_Status gl841_write_freq(Genesys_Device *dev, unsigned int ydpi)
{
SANE_Status status;
/**< fast table */
uint8_t tdefault[] = {0x18,0x36,0x18,0x36,0x18,0x36,0x18,0x36,0x18,0x36,0x18,0x36,0x18,0x36,0x18,0x36,0x18,0x36,0x18,0x36,0x18,0x36,0x18,0x36,0x18,0x36,0x18,0x36,0x18,0x36,0x18,0x36,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xb6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0xf6,0x18,0x76,0x18,0x76,0x18,0x76,0x18,0x76,0x18,0x76,0x18,0x76,0x18,0x76,0x18,0x76,0x18,0x76,0x18,0x76,0x18,0x76,0x18,0x76,0x18,0x76,0x18,0x76,0x18,0x76,0x18,0x76};
uint8_t t1200[]    = {0xc7,0x31,0xc7,0x31,0xc7,0x31,0xc7,0x31,0xc7,0x31,0xc7,0x31,0xc7,0x31,0xc7,0x31,0xc0,0x11,0xc0,0x11,0xc0,0x11,0xc0,0x11,0xc0,0x11,0xc0,0x11,0xc0,0x11,0xc0,0x11,0xc7,0xb1,0xc7,0xb1,0xc7,0xb1,0xc7,0xb1,0xc7,0xb1,0xc7,0xb1,0xc7,0xb1,0xc7,0xb1,0x07,0xe0,0x07,0xe0,0x07,0xe0,0x07,0xe0,0x07,0xe0,0x07,0xe0,0x07,0xe0,0x07,0xe0,0xc7,0xf1,0xc7,0xf1,0xc7,0xf1,0xc7,0xf1,0xc7,0xf1,0xc7,0xf1,0xc7,0xf1,0xc7,0xf1,0xc0,0x51,0xc0,0x51,0xc0,0x51,0xc0,0x51,0xc0,0x51,0xc0,0x51,0xc0,0x51,0xc0,0x51,0xc7,0x71,0xc7,0x71,0xc7,0x71,0xc7,0x71,0xc7,0x71,0xc7,0x71,0xc7,0x71,0xc7,0x71,0x07,0x20,0x07,0x20,0x07,0x20,0x07,0x20,0x07,0x20,0x07,0x20,0x07,0x20,0x07,0x20};
uint8_t t300[]     = {0x08,0x32,0x08,0x32,0x08,0x32,0x08,0x32,0x08,0x32,0x08,0x32,0x08,0x32,0x08,0x32,0x00,0x13,0x00,0x13,0x00,0x13,0x00,0x13,0x00,0x13,0x00,0x13,0x00,0x13,0x00,0x13,0x08,0xb2,0x08,0xb2,0x08,0xb2,0x08,0xb2,0x08,0xb2,0x08,0xb2,0x08,0xb2,0x08,0xb2,0x0c,0xa0,0x0c,0xa0,0x0c,0xa0,0x0c,0xa0,0x0c,0xa0,0x0c,0xa0,0x0c,0xa0,0x0c,0xa0,0x08,0xf2,0x08,0xf2,0x08,0xf2,0x08,0xf2,0x08,0xf2,0x08,0xf2,0x08,0xf2,0x08,0xf2,0x00,0xd3,0x00,0xd3,0x00,0xd3,0x00,0xd3,0x00,0xd3,0x00,0xd3,0x00,0xd3,0x00,0xd3,0x08,0x72,0x08,0x72,0x08,0x72,0x08,0x72,0x08,0x72,0x08,0x72,0x08,0x72,0x08,0x72,0x0c,0x60,0x0c,0x60,0x0c,0x60,0x0c,0x60,0x0c,0x60,0x0c,0x60,0x0c,0x60,0x0c,0x60};
uint8_t t150[]     = {0x0c,0x33,0xcf,0x33,0xcf,0x33,0xcf,0x33,0xcf,0x33,0xcf,0x33,0xcf,0x33,0xcf,0x33,0x40,0x14,0x80,0x15,0x80,0x15,0x80,0x15,0x80,0x15,0x80,0x15,0x80,0x15,0x80,0x15,0x0c,0xb3,0xcf,0xb3,0xcf,0xb3,0xcf,0xb3,0xcf,0xb3,0xcf,0xb3,0xcf,0xb3,0xcf,0xb3,0x11,0xa0,0x16,0xa0,0x16,0xa0,0x16,0xa0,0x16,0xa0,0x16,0xa0,0x16,0xa0,0x16,0xa0,0x0c,0xf3,0xcf,0xf3,0xcf,0xf3,0xcf,0xf3,0xcf,0xf3,0xcf,0xf3,0xcf,0xf3,0xcf,0xf3,0x40,0xd4,0x80,0xd5,0x80,0xd5,0x80,0xd5,0x80,0xd5,0x80,0xd5,0x80,0xd5,0x80,0xd5,0x0c,0x73,0xcf,0x73,0xcf,0x73,0xcf,0x73,0xcf,0x73,0xcf,0x73,0xcf,0x73,0xcf,0x73,0x11,0x60,0x16,0x60,0x16,0x60,0x16,0x60,0x16,0x60,0x16,0x60,0x16,0x60,0x16,0x60};

uint8_t *table;

  DBGSTART;
  if(dev->model->motor_type == MOTOR_CANONLIDE80)
    {
      switch(ydpi)
        {
          case 3600:
          case 1200:
            table=t1200;
            break;
          case 900:
          case 300:
            table=t300;
            break;
          case 450:
          case 150:
            table=t150;
            break;
          default:
            table=tdefault;
        }
      RIE(sanei_genesys_write_register(dev, 0x66, 0x00));
      RIE(sanei_genesys_write_register(dev, 0x5b, 0x0c));
      RIE(sanei_genesys_write_register(dev, 0x5c, 0x00));
      RIE(gl841_bulk_write_data_gamma (dev, 0x28, table, 128));
      RIE(sanei_genesys_write_register(dev, 0x5b, 0x00));
      RIE(sanei_genesys_write_register(dev, 0x5c, 0x00));
    }
  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}


#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl841_init_motor_regs(Genesys_Device * dev,
		      Genesys_Register_Set * reg,
		      unsigned int feed_steps,/*1/base_ydpi*/
/*maybe float for half/quarter step resolution?*/
		      unsigned int action,
		      unsigned int flags)
{
    SANE_Status status;
    unsigned int fast_exposure;
    int scan_power_mode;
    int use_fast_fed = 0;
    uint16_t fast_slope_table[256];
    unsigned int fast_slope_steps = 0;
    unsigned int feedl;
    Genesys_Register_Set * r;
/*number of scan lines to add in a scan_lines line*/

    DBG (DBG_proc, "gl841_init_motor_regs : feed_steps=%d, action=%d, flags=%x\n",
	 feed_steps,
	 action,
	 flags);

    memset(fast_slope_table,0xff,512);

    gl841_send_slope_table (dev, 0, fast_slope_table, 256);
    gl841_send_slope_table (dev, 1, fast_slope_table, 256);
    gl841_send_slope_table (dev, 2, fast_slope_table, 256);
    gl841_send_slope_table (dev, 3, fast_slope_table, 256);
    gl841_send_slope_table (dev, 4, fast_slope_table, 256);

    gl841_write_freq(dev, dev->motor.base_ydpi / 4);

    fast_slope_steps = 256;
    if (action == MOTOR_ACTION_FEED || action == MOTOR_ACTION_GO_HOME)
      {
        /* FEED and GO_HOME can use fastest slopes available */
	fast_exposure = gl841_exposure_time(dev,
                                            dev->motor.base_ydpi / 4,
                                            0,
                                            0,
                                            0,
                                            &scan_power_mode);
	DBG (DBG_info, "%s : fast_exposure=%d pixels\n", __FUNCTION__, fast_exposure);
      }

    if (action == MOTOR_ACTION_HOME_FREE) {
/* HOME_FREE must be able to stop in one step, so do not try to get faster */
	fast_exposure = dev->motor.slopes[0][0].maximum_start_speed;
    }

     sanei_genesys_create_slope_table3 (
	dev,
	fast_slope_table,
        256,
	fast_slope_steps,
	0,
	fast_exposure,
	dev->motor.base_ydpi / 4,
	&fast_slope_steps,
	&fast_exposure, 0);

    feedl = feed_steps - fast_slope_steps*2;
    use_fast_fed = 1;

/* all needed slopes available. we did even decide which mode to use.
   what next?
   - transfer slopes
SCAN:
flags \ use_fast_fed    ! 0         1
------------------------\--------------------
                      0 ! 0,1,2     0,1,2,3
MOTOR_FLAG_AUTO_GO_HOME ! 0,1,2,4   0,1,2,3,4
OFF:       none
FEED:      3
GO_HOME:   3
HOME_FREE: 3
   - setup registers
     * slope specific registers (already done)
     * DECSEL for HOME_FREE/GO_HOME/SCAN
     * FEEDL
     * MTRREV
     * MTRPWR
     * FASTFED
     * STEPSEL
     * MTRPWM
     * FSTPSEL
     * FASTPWM
     * HOMENEG
     * BWDSTEP
     * FWDSTEP
     * Z1
     * Z2
 */

    r = sanei_genesys_get_address (reg, 0x3d);
    r->value = (feedl >> 16) & 0xf;
    r = sanei_genesys_get_address (reg, 0x3e);
    r->value = (feedl >> 8) & 0xff;
    r = sanei_genesys_get_address (reg, 0x3f);
    r->value = feedl & 0xff;
    r = sanei_genesys_get_address (reg, 0x5e);
    r->value &= ~0xe0;

    r = sanei_genesys_get_address (reg, 0x25);
    r->value = 0;
    r = sanei_genesys_get_address (reg, 0x26);
    r->value = 0;
    r = sanei_genesys_get_address (reg, 0x27);
    r->value = 0;

    r = sanei_genesys_get_address (reg, 0x02);
    r->value &= ~0x01; /*LONGCURV OFF*/
    r->value &= ~0x80; /*NOT_HOME OFF*/

    r->value |= 0x10;

    if (action == MOTOR_ACTION_GO_HOME)
	r->value |= 0x06;
    else
	r->value &= ~0x06;

    if (use_fast_fed)
	r->value |= 0x08;
    else
	r->value &= ~0x08;

    if (flags & MOTOR_FLAG_AUTO_GO_HOME)
	r->value |= 0x20;
    else
	r->value &= ~0x20;

    r->value &= ~0x40;

    status = gl841_send_slope_table (dev, 3, fast_slope_table, 256);

    if (status != SANE_STATUS_GOOD)
	return status;

    r = sanei_genesys_get_address (reg, 0x67);
    r->value = 0x3f;

    r = sanei_genesys_get_address (reg, 0x68);
    r->value = 0x3f;

    r = sanei_genesys_get_address (reg, REG_STEPNO);
    r->value = 0;

    r = sanei_genesys_get_address (reg, REG_FASTNO);
    r->value = 0;

    r = sanei_genesys_get_address (reg, 0x69);
    r->value = 0;

    r = sanei_genesys_get_address (reg, 0x6a);
    r->value = (fast_slope_steps >> 1) + (fast_slope_steps & 1);

    r = sanei_genesys_get_address (reg, 0x5f);
    r->value = (fast_slope_steps >> 1) + (fast_slope_steps & 1);


    DBGCOMPLETED;
    return SANE_STATUS_GOOD;
}

#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl841_init_motor_regs_scan(Genesys_Device * dev,
		      Genesys_Register_Set * reg,
		      unsigned int scan_exposure_time,/*pixel*/
		      float scan_yres,/*dpi, motor resolution*/
		      int scan_step_type,/*0: full, 1: half, 2: quarter*/
		      unsigned int scan_lines,/*lines, scan resolution*/
		      unsigned int scan_dummy,
/*number of scan lines to add in a scan_lines line*/
		      unsigned int feed_steps,/*1/base_ydpi*/
/*maybe float for half/quarter step resolution?*/
		      int scan_power_mode,
		      unsigned int flags)
{
    SANE_Status status;
    unsigned int fast_exposure;
    int use_fast_fed = 0;
    int dummy_power_mode;
    unsigned int fast_time;
    unsigned int slow_time;
    uint16_t slow_slope_table[256];
    uint16_t fast_slope_table[256];
    uint16_t back_slope_table[256];
    unsigned int slow_slope_time;
    unsigned int fast_slope_time;
    unsigned int slow_slope_steps = 0;
    unsigned int fast_slope_steps = 0;
    unsigned int back_slope_steps = 0;
    unsigned int feedl;
    Genesys_Register_Set * r;
    unsigned int min_restep = 0x20;
    uint32_t z1, z2;

    DBG (DBG_proc, "gl841_init_motor_regs_scan : scan_exposure_time=%d, "
	 "scan_yres=%g, scan_step_type=%d, scan_lines=%d, scan_dummy=%d, "
	 "feed_steps=%d, scan_power_mode=%d, flags=%x\n",
	 scan_exposure_time,
	 scan_yres,
	 scan_step_type,
	 scan_lines,
	 scan_dummy,
	 feed_steps,
	 scan_power_mode,
	 flags);

    fast_exposure = gl841_exposure_time(dev,
                                        dev->motor.base_ydpi / 4,
                                        0,
                                        0,
                                        0,
                                        &dummy_power_mode);

    DBG (DBG_info, "%s : fast_exposure=%d pixels\n", __FUNCTION__, fast_exposure);

    memset(slow_slope_table,0xff,512);

    gl841_send_slope_table (dev, 0, slow_slope_table, 256);
    gl841_send_slope_table (dev, 1, slow_slope_table, 256);
    gl841_send_slope_table (dev, 2, slow_slope_table, 256);
    gl841_send_slope_table (dev, 3, slow_slope_table, 256);
    gl841_send_slope_table (dev, 4, slow_slope_table, 256);

    /* motor frequency table */
    gl841_write_freq(dev, scan_yres);

/*
  we calculate both tables for SCAN. the fast slope step count depends on
  how many steps we need for slow acceleration and how much steps we are
  allowed to use.
 */
    slow_slope_time = sanei_genesys_create_slope_table3 (
	dev,
	slow_slope_table, 256,
	256,
	scan_step_type,
	scan_exposure_time,
	scan_yres,
	&slow_slope_steps,
	NULL,
	scan_power_mode);

     sanei_genesys_create_slope_table3 (
	dev,
	back_slope_table, 256,
	256,
	scan_step_type,
	0,
	scan_yres,
	&back_slope_steps,
	NULL,
	scan_power_mode);

    if (feed_steps < (slow_slope_steps >> scan_step_type)) {
	/*TODO: what should we do here?? go back to exposure calculation?*/
	feed_steps = slow_slope_steps >> scan_step_type;
    }

    if (feed_steps > fast_slope_steps*2 -
	    (slow_slope_steps >> scan_step_type))
	fast_slope_steps = 256;
    else
/* we need to shorten fast_slope_steps here. */
	fast_slope_steps = (feed_steps -
			    (slow_slope_steps >> scan_step_type))/2;

    DBG(DBG_info,"gl841_init_motor_regs_scan: Maximum allowed slope steps for fast slope: %d\n",fast_slope_steps);

    fast_slope_time = sanei_genesys_create_slope_table3 (
	dev,
	fast_slope_table, 256,
	fast_slope_steps,
	0,
	fast_exposure,
	dev->motor.base_ydpi / 4,
	&fast_slope_steps,
	&fast_exposure,
	scan_power_mode);

    /* fast fed special cases handling */
    if (dev->model->gpo_type == GPO_XP300
     || dev->model->gpo_type == GPO_DP685)
      {
	/* quirk: looks like at least this scanner is unable to use
	   2-feed mode */
	use_fast_fed = 0;
      }
    else if (feed_steps < fast_slope_steps*2 + (slow_slope_steps >> scan_step_type)) {
	use_fast_fed = 0;
	DBG(DBG_info,"gl841_init_motor_regs_scan: feed too short, slow move forced.\n");
    } else {
/* for deciding whether we should use fast mode we need to check how long we
   need for (fast)accelerating, moving, decelerating, (TODO: stopping?)
   (slow)accelerating again versus (slow)accelerating and moving. we need
   fast and slow tables here.
*/
/*NOTE: scan_exposure_time is per scan_yres*/
/*NOTE: fast_exposure is per base_ydpi/4*/
/*we use full steps as base unit here*/
	fast_time =
	    fast_exposure / 4 *
	    (feed_steps - fast_slope_steps*2 -
	     (slow_slope_steps >> scan_step_type))
	    + fast_slope_time*2 + slow_slope_time;
	slow_time =
	    (scan_exposure_time * scan_yres) / dev->motor.base_ydpi *
	    (feed_steps - (slow_slope_steps >> scan_step_type))
	    + slow_slope_time;

	DBG(DBG_info,"gl841_init_motor_regs_scan: Time for slow move: %d\n",
	    slow_time);
	DBG(DBG_info,"gl841_init_motor_regs_scan: Time for fast move: %d\n",
	    fast_time);

	use_fast_fed = fast_time < slow_time;
    }

    if (use_fast_fed)
	feedl = feed_steps - fast_slope_steps*2 -
	    (slow_slope_steps >> scan_step_type);
    else
	if ((feed_steps << scan_step_type) < slow_slope_steps)
	    feedl = 0;
	else
	    feedl = (feed_steps << scan_step_type) - slow_slope_steps;
    DBG(DBG_info,"gl841_init_motor_regs_scan: Decided to use %s mode\n",
	use_fast_fed?"fast feed":"slow feed");

/* all needed slopes available. we did even decide which mode to use.
   what next?
   - transfer slopes
SCAN:
flags \ use_fast_fed    ! 0         1
------------------------\--------------------
                      0 ! 0,1,2     0,1,2,3
MOTOR_FLAG_AUTO_GO_HOME ! 0,1,2,4   0,1,2,3,4
OFF:       none
FEED:      3
GO_HOME:   3
HOME_FREE: 3
   - setup registers
     * slope specific registers (already done)
     * DECSEL for HOME_FREE/GO_HOME/SCAN
     * FEEDL
     * MTRREV
     * MTRPWR
     * FASTFED
     * STEPSEL
     * MTRPWM
     * FSTPSEL
     * FASTPWM
     * HOMENEG
     * BWDSTEP
     * FWDSTEP
     * Z1
     * Z2
 */

    r = sanei_genesys_get_address (reg, 0x3d);
    r->value = (feedl >> 16) & 0xf;
    r = sanei_genesys_get_address (reg, 0x3e);
    r->value = (feedl >> 8) & 0xff;
    r = sanei_genesys_get_address (reg, 0x3f);
    r->value = feedl & 0xff;
    r = sanei_genesys_get_address (reg, 0x5e);
    r->value &= ~0xe0;

    r = sanei_genesys_get_address (reg, 0x25);
    r->value = (scan_lines >> 16) & 0xf;
    r = sanei_genesys_get_address (reg, 0x26);
    r->value = (scan_lines >> 8) & 0xff;
    r = sanei_genesys_get_address (reg, 0x27);
    r->value = scan_lines & 0xff;

    r = sanei_genesys_get_address (reg, 0x02);
    r->value &= ~0x01; /*LONGCURV OFF*/
    r->value &= ~0x80; /*NOT_HOME OFF*/
    r->value |= 0x10;

    r->value &= ~0x06;

    if (use_fast_fed)
	r->value |= 0x08;
    else
	r->value &= ~0x08;

    if (flags & MOTOR_FLAG_AUTO_GO_HOME)
	r->value |= 0x20;
    else
	r->value &= ~0x20;

    if (flags & MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE)
	r->value |= 0x40;
    else
	r->value &= ~0x40;

    status = gl841_send_slope_table (dev, 0, slow_slope_table, 256);

    if (status != SANE_STATUS_GOOD)
	return status;

    status = gl841_send_slope_table (dev, 1, back_slope_table, 256);

    if (status != SANE_STATUS_GOOD)
	return status;

    status = gl841_send_slope_table (dev, 2, slow_slope_table, 256);

    if (status != SANE_STATUS_GOOD)
	return status;

    if (use_fast_fed) {
	status = gl841_send_slope_table (dev, 3, fast_slope_table, 256);

	if (status != SANE_STATUS_GOOD)
	    return status;
    }

    if (flags & MOTOR_FLAG_AUTO_GO_HOME){
	status = gl841_send_slope_table (dev, 4, fast_slope_table, 256);

	if (status != SANE_STATUS_GOOD)
	    return status;
    }


/* now reg 0x21 and 0x24 are available, we can calculate reg 0x22 and 0x23,
   reg 0x60-0x62 and reg 0x63-0x65
   rule:
   2*STEPNO+FWDSTEP=2*FASTNO+BWDSTEP
*/
/* steps of table 0*/
    if (min_restep < slow_slope_steps*2+2)
	min_restep = slow_slope_steps*2+2;
/* steps of table 1*/
    if (min_restep < back_slope_steps*2+2)
	min_restep = back_slope_steps*2+2;
/* steps of table 0*/
    r = sanei_genesys_get_address (reg, REG_FWDSTEP);
    r->value = min_restep - slow_slope_steps*2;
/* steps of table 1*/
    r = sanei_genesys_get_address (reg, REG_BWDSTEP);
    r->value = min_restep - back_slope_steps*2;

/*
  for z1/z2:
  in dokumentation mentioned variables a-d:
  a = time needed for acceleration, table 1
  b = time needed for reg 0x1f... wouldn't that be reg0x1f*exposure_time?
  c = time needed for acceleration, table 1
  d = time needed for reg 0x22... wouldn't that be reg0x22*exposure_time?
  z1 = (c+d-1) % exposure_time
  z2 = (a+b-1) % exposure_time
*/
/* i don't see any effect of this. i can only guess that this will enhance
   sub-pixel accuracy
   z1 = (slope_0_time-1) % exposure_time;
   z2 = (slope_0_time-1) % exposure_time;
*/
    z1 = z2 = 0;

    DBG (DBG_info, "gl841_init_motor_regs_scan: z1 = %d\n", z1);
    DBG (DBG_info, "gl841_init_motor_regs_scan: z2 = %d\n", z2);
    r = sanei_genesys_get_address (reg, 0x60);
    r->value = ((z1 >> 16) & 0xff);
    r = sanei_genesys_get_address (reg, 0x61);
    r->value = ((z1 >> 8) & 0xff);
    r = sanei_genesys_get_address (reg, 0x62);
    r->value = (z1 & 0xff);
    r = sanei_genesys_get_address (reg, 0x63);
    r->value = ((z2 >> 16) & 0xff);
    r = sanei_genesys_get_address (reg, 0x64);
    r->value = ((z2 >> 8) & 0xff);
    r = sanei_genesys_get_address (reg, 0x65);
    r->value = (z2 & 0xff);

    r = sanei_genesys_get_address (reg, REG1E);
    r->value &= REG1E_WDTIME;
    r->value |= scan_dummy;

    r = sanei_genesys_get_address (reg, 0x67);
    r->value = 0x3f | (scan_step_type << 6);

    r = sanei_genesys_get_address (reg, 0x68);
    r->value = 0x3f;

    r = sanei_genesys_get_address (reg, REG_STEPNO);
    r->value = (slow_slope_steps >> 1) + (slow_slope_steps & 1);

    r = sanei_genesys_get_address (reg, REG_FASTNO);
    r->value = (back_slope_steps >> 1) + (back_slope_steps & 1);

    r = sanei_genesys_get_address (reg, 0x69);
    r->value = (slow_slope_steps >> 1) + (slow_slope_steps & 1);

    r = sanei_genesys_get_address (reg, 0x6a);
    r->value = (fast_slope_steps >> 1) + (fast_slope_steps & 1);

    r = sanei_genesys_get_address (reg, 0x5f);
    r->value = (fast_slope_steps >> 1) + (fast_slope_steps & 1);


    DBGCOMPLETED;
    return SANE_STATUS_GOOD;
}

static int
gl841_get_dpihw(Genesys_Device * dev)
{
  Genesys_Register_Set * r;
  r = sanei_genesys_get_address (dev->reg, 0x05);
  if ((r->value & REG05_DPIHW) == REG05_DPIHW_600)
    return 600;
  if ((r->value & REG05_DPIHW) == REG05_DPIHW_1200)
    return 1200;
  if ((r->value & REG05_DPIHW) == REG05_DPIHW_2400)
    return 2400;
  return 0;
}

static SANE_Status
gl841_init_optical_regs_off(Genesys_Register_Set * reg)
{
    Genesys_Register_Set * r;

    DBGSTART;

    r = sanei_genesys_get_address (reg, 0x01);
    r->value &= ~REG01_SCAN;

    DBGCOMPLETED;
    return SANE_STATUS_GOOD;
}

static SANE_Status
gl841_init_optical_regs_scan(Genesys_Device * dev,
			     Genesys_Register_Set * reg,
			     unsigned int exposure_time,
			     unsigned int used_res,
			     unsigned int start,
			     unsigned int pixels,
			     int channels,
			     int depth,
			     SANE_Bool half_ccd,
			     int color_filter,
			     int flags
    )
{
    unsigned int words_per_line;
    unsigned int end;
    unsigned int dpiset;
    unsigned int i;
    Genesys_Register_Set * r;
    SANE_Status status;
    uint16_t expavg, expr, expb, expg;

    DBG (DBG_proc, "gl841_init_optical_regs_scan :  exposure_time=%d, "
	 "used_res=%d, start=%d, pixels=%d, channels=%d, depth=%d, "
	 "half_ccd=%d, flags=%x\n",
	 exposure_time,
	 used_res,
	 start,
	 pixels,
	 channels,
	 depth,
	 half_ccd,
	 flags);

    end = start + pixels;

    status = gl841_set_fe (dev, AFE_SET);
    if (status != SANE_STATUS_GOOD)
    {
	DBG (DBG_error,
	     "gl841_init_optical_regs_scan: failed to set frontend: %s\n",
	     sane_strstatus (status));
	return status;
    }

    /* adjust used_res for chosen dpihw */
    used_res = used_res * gl841_get_dpihw(dev) / dev->sensor.optical_res;

/*
  with half_ccd the optical resolution of the ccd is halved. We don't apply this
  to dpihw, so we need to double dpiset.

  For the scanner only the ratio of dpiset and dpihw is of relevance to scale
  down properly.
*/
    if (half_ccd)
	dpiset = used_res * 2;
    else
	dpiset = used_res;

    /* gpio part.*/
    if (dev->model->gpo_type == GPO_CANONLIDE35)
      {
	r = sanei_genesys_get_address (reg, REG6C);
	if (half_ccd)
	  r->value &= ~0x80;
	else
	  r->value |= 0x80;
      }
    if (dev->model->gpo_type == GPO_CANONLIDE80)
      {
	r = sanei_genesys_get_address (reg, REG6C);
	if (half_ccd)
          {
	    r->value &= ~0x40;
	    r->value |= 0x20;
          }
	else
          {
	    r->value &= ~0x20;
	    r->value |= 0x40;
          }
      }

    /* enable shading */
    r = sanei_genesys_get_address (reg, 0x01);
    r->value |= REG01_SCAN;
    if ((flags & OPTICAL_FLAG_DISABLE_SHADING) ||
	(dev->model->flags & GENESYS_FLAG_NO_CALIBRATION))
	r->value &= ~REG01_DVDSET;
    else
	r->value |= REG01_DVDSET;

    /* average looks better than deletion, and we are already set up to
       use  one of the average enabled resolutions
    */
    r = sanei_genesys_get_address (reg, 0x03);
    r->value |= REG03_AVEENB;
    if (flags & OPTICAL_FLAG_DISABLE_LAMP)
	r->value &= ~REG03_LAMPPWR;
    else
	r->value |= REG03_LAMPPWR;

    /* exposure times */
    r = sanei_genesys_get_address (reg, 0x10);
    for (i = 0; i < 6; i++, r++) {
	if (flags & OPTICAL_FLAG_DISABLE_LAMP)
	    r->value = 0x01;/* 0x0101 is as off as possible */
	else
          { /* EXP[R,G,B] only matter for CIS scanners */
	    if (dev->sensor.regs_0x10_0x1d[i] == 0x00)
		r->value = 0x01; /*0x00 will not be accepted*/
	    else
		r->value = dev->sensor.regs_0x10_0x1d[i];
          }
    }

    r = sanei_genesys_get_address (reg, 0x19);
    if (flags & OPTICAL_FLAG_DISABLE_LAMP)
	r->value = 0xff;
    else
	r->value = 0x50;

    /* BW threshold */
    r = sanei_genesys_get_address (reg, 0x2e);
    r->value = dev->settings.threshold;
    r = sanei_genesys_get_address (reg, 0x2f);
    r->value = dev->settings.threshold;


    /* monochrome / color scan */
    r = sanei_genesys_get_address (reg, 0x04);
    switch (depth) {
	case 1:
	    r->value &= ~REG04_BITSET;
	    r->value |= REG04_LINEART;
	    break;
	case 8:
	    r->value &= ~(REG04_LINEART | REG04_BITSET);
	    break;
	case 16:
	    r->value &= ~REG04_LINEART;
	    r->value |= REG04_BITSET;
	    break;
    }

    /* AFEMOD should depend on FESET, and we should set these
     * bits separately */
    r->value &= ~(REG04_FILTER | REG04_AFEMOD);
    if (flags & OPTICAL_FLAG_ENABLE_LEDADD)
      {
	r->value |= 0x10;	/* no filter */
      }
    else if (channels == 1)
      {
	switch (color_filter)
	  {
	  case 0:
	    r->value |= 0x14;	/* red filter */
	    break;
	  case 1:
	    r->value |= 0x18;	/* green filter */
	    break;
	  case 2:
	    r->value |= 0x1c;	/* blue filter */
	    break;
	  default:
	    r->value |= 0x10;	/* no filter */
	    break;
	  }
      }
    else
      {
        if (dev->model->ccd_type == CCD_PLUSTEK_3600)
          {
            r->value |= 0x22;	/* slow color pixel by pixel */
          }
	else
          {
	    r->value |= 0x10;	/* color pixel by pixel */
          }
      }

    /* CIS scanners can do true gray by setting LEDADD */
    r = sanei_genesys_get_address (reg, 0x87);
    r->value &= ~REG87_LEDADD;
    if (flags & OPTICAL_FLAG_ENABLE_LEDADD)
      {
        r->value |= REG87_LEDADD;
	sanei_genesys_get_double (reg, REG_EXPR, &expr);
	sanei_genesys_get_double (reg, REG_EXPG, &expg);
	sanei_genesys_get_double (reg, REG_EXPB, &expb);

	/* use minimal exposure for best image quality */
	expavg = expg;
	if (expr < expg)
	  expavg = expr;
	if (expb < expavg)
	  expavg = expb;

	sanei_genesys_set_double (dev->reg, REG_EXPR, expavg);
	sanei_genesys_set_double (dev->reg, REG_EXPG, expavg);
	sanei_genesys_set_double (dev->reg, REG_EXPB, expavg);
      }

    /* enable gamma tables */
    r = sanei_genesys_get_address (reg, 0x05);
    if (flags & OPTICAL_FLAG_DISABLE_GAMMA)
	r->value &= ~REG05_GMMENB;
    else
	r->value |= REG05_GMMENB;

    /* sensor parameters */
    sanei_gl841_setup_sensor (dev, dev->reg, 1, half_ccd);

    r = sanei_genesys_get_address (reg, 0x29);
    r->value = 255; /*<<<"magic" number, only suitable for cis*/

    sanei_genesys_set_double(reg, REG_DPISET, dpiset);
    sanei_genesys_set_double(reg, REG_STRPIXEL, start);
    sanei_genesys_set_double(reg, REG_ENDPIXEL, end);
    DBG( DBG_io2, "%s: STRPIXEL=%d, ENDPIXEL=%d\n",__FUNCTION__,start,end);

    /* words(16bit) before gamma, conversion to 8 bit or lineart*/
    words_per_line = (pixels * dpiset) / gl841_get_dpihw(dev);

    words_per_line *= channels;

    if (depth == 1)
	words_per_line = (words_per_line >> 3) + ((words_per_line & 7)?1:0);
    else
	words_per_line *= depth / 8;

    dev->wpl = words_per_line;
    dev->bpl = words_per_line;

    r = sanei_genesys_get_address (reg, 0x35);
    r->value = LOBYTE (HIWORD (words_per_line));
    r = sanei_genesys_get_address (reg, 0x36);
    r->value = HIBYTE (LOWORD (words_per_line));
    r = sanei_genesys_get_address (reg, 0x37);
    r->value = LOBYTE (LOWORD (words_per_line));

    sanei_genesys_set_double(reg, REG_LPERIOD, exposure_time);

    r = sanei_genesys_get_address (reg, 0x34);
    r->value = dev->sensor.dummy_pixel;

    DBGCOMPLETED;
    return SANE_STATUS_GOOD;
}

static int
gl841_get_led_exposure(Genesys_Device * dev)
{
    int d,r,g,b,m;
    if (!dev->model->is_cis)
	return 0;
    d = dev->reg[reg_0x19].value;
    r = dev->sensor.regs_0x10_0x1d[1] | (dev->sensor.regs_0x10_0x1d[0] << 8);
    g = dev->sensor.regs_0x10_0x1d[3] | (dev->sensor.regs_0x10_0x1d[2] << 8);
    b = dev->sensor.regs_0x10_0x1d[5] | (dev->sensor.regs_0x10_0x1d[4] << 8);

    m = r;
    if (m < g)
	m = g;
    if (m < b)
	m = b;

    return m + d;
}

/** @brief compute exposure time
 * Compute exposure time for the device and the given scan resolution,
 * also compute scan_power_mode
 */
GENESYS_STATIC int
gl841_exposure_time(Genesys_Device *dev,
                    float slope_dpi,
                    int scan_step_type,
                    int start,
                    int used_pixels,
                    int *scan_power_mode)
{
int exposure_time = 0;
int exposure_time2 = 0;
int led_exposure;

  *scan_power_mode=0;
  led_exposure=gl841_get_led_exposure(dev);
  exposure_time = sanei_genesys_exposure_time2(
      dev,
      slope_dpi,
      scan_step_type,
      start+used_pixels,/*+tgtime? currently done in sanei_genesys_exposure_time2 with tgtime = 32 pixel*/
      led_exposure,
      *scan_power_mode);

  while(*scan_power_mode + 1 < dev->motor.power_mode_count) {
      exposure_time2 = sanei_genesys_exposure_time2(
	  dev,
	  slope_dpi,
	  scan_step_type,
	  start+used_pixels,/*+tgtime? currently done in sanei_genesys_exposure_time2 with tgtime = 32 pixel*/
	  led_exposure,
	  *scan_power_mode + 1);
      if (exposure_time < exposure_time2)
	  break;
      exposure_time = exposure_time2;
      (*scan_power_mode)++;
  }

  return exposure_time;
}

/**@brief compute scan_step_type
 * Try to do at least 4 steps per line. if that is impossible we will have to
 * live with that.
 * @param dev device
 * @param yres motor resolution
 */
GENESYS_STATIC int
gl841_scan_step_type(Genesys_Device *dev, int yres)
{
int scan_step_type=0;

  /* TODO : check if there is a bug around the use of max_step_type   */
  /* should be <=1, need to chek all devices entry in genesys_devices */
  if (yres*4 < dev->motor.base_ydpi || dev->motor.max_step_type <= 0)
    {
      scan_step_type = 0;
    }
  else if (yres*4 < dev->motor.base_ydpi*2 || dev->motor.max_step_type <= 1)
    {
      scan_step_type = 1;
    }
  else
    {
      scan_step_type = 2;
    }

  /* this motor behaves differently */
  if (dev->model->motor_type==MOTOR_CANONLIDE80)
    {
      /* driven by 'frequency' tables ? */
      scan_step_type = 0;
    }

  return scan_step_type;
}

/* set up registers for an actual scan
 *
 * this function sets up the scanner to scan in normal or single line mode
 */
GENESYS_STATIC
SANE_Status
gl841_init_scan_regs (Genesys_Device * dev,
		      Genesys_Register_Set * reg,
		      float xres,/*dpi*/
		      float yres,/*dpi*/
		      float startx,/*optical_res, from dummy_pixel+1*/
		      float starty,/*base_ydpi, from home!*/
		      float pixels,
		      float lines,
		      unsigned int depth,
		      unsigned int channels,
		      int color_filter,
		      unsigned int flags
		      )
{
  int used_res;
  int start, used_pixels;
  int bytes_per_line;
  int move;
  unsigned int lincnt;
  int exposure_time;
  int scan_power_mode;
  int i;
  int stagger;
  int avg;

  int slope_dpi = 0;
  int dummy = 0;
  int scan_step_type = 1;
  int max_shift;
  size_t requested_buffer_size, read_buffer_size;

  SANE_Bool half_ccd;		/* false: full CCD res is used, true, half max CCD res is used */
  int optical_res;
  SANE_Status status;
  unsigned int oflags;          /**> optical flags */

  DBG (DBG_info,
       "gl841_init_scan_regs settings:\n"
       "Resolution    : %gDPI/%gDPI\n"
       "Lines         : %g\n"
       "PPL           : %g\n"
       "Startpos      : %g/%g\n"
       "Depth/Channels: %u/%u\n"
       "Flags         : %x\n\n",
       xres, yres, lines, pixels,
       startx, starty,
       depth, channels,
       flags);

/*
results:

for scanner:
half_ccd
start
end
dpiset
exposure_time
dummy
z1
z2

for ordered_read:
  dev->words_per_line
  dev->read_factor
  dev->requested_buffer_size
  dev->read_buffer_size
  dev->read_pos
  dev->read_bytes_in_buffer
  dev->read_bytes_left
  dev->max_shift
  dev->stagger

independent of our calculated values:
  dev->total_bytes_read
  dev->bytes_to_read
 */

/* half_ccd */
  /* we have 2 domains for ccd: xres below or above half ccd max dpi */
  if (dev->sensor.optical_res  < 2 * xres ||
      !(dev->model->flags & GENESYS_FLAG_HALF_CCD_MODE)) {
      half_ccd = SANE_FALSE;
  } else {
      half_ccd = SANE_TRUE;
  }

/* optical_res */

  optical_res = dev->sensor.optical_res;
  if (half_ccd)
      optical_res /= 2;

/* stagger */

  if ((!half_ccd) && (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE))
    stagger = (4 * yres) / dev->motor.base_ydpi;
  else
    stagger = 0;
  DBG (DBG_info, "gl841_init_scan_regs : stagger=%d lines\n",
       stagger);

/* used_res */
  i = optical_res / xres;

/* gl841 supports 1/1 1/2 1/3 1/4 1/5 1/6 1/8 1/10 1/12 1/15 averaging */

  if (i < 2 || (flags & SCAN_FLAG_USE_OPTICAL_RES)) /* optical_res >= xres > optical_res/2 */
      used_res = optical_res;
  else if (i < 3)  /* optical_res/2 >= xres > optical_res/3 */
      used_res = optical_res/2;
  else if (i < 4)  /* optical_res/3 >= xres > optical_res/4 */
      used_res = optical_res/3;
  else if (i < 5)  /* optical_res/4 >= xres > optical_res/5 */
      used_res = optical_res/4;
  else if (i < 6)  /* optical_res/5 >= xres > optical_res/6 */
      used_res = optical_res/5;
  else if (i < 8)  /* optical_res/6 >= xres > optical_res/8 */
      used_res = optical_res/6;
  else if (i < 10)  /* optical_res/8 >= xres > optical_res/10 */
      used_res = optical_res/8;
  else if (i < 12)  /* optical_res/10 >= xres > optical_res/12 */
      used_res = optical_res/10;
  else if (i < 15)  /* optical_res/12 >= xres > optical_res/15 */
      used_res = optical_res/12;
  else
      used_res = optical_res/15;

  /* compute scan parameters values */
  /* pixels are allways given at half or full CCD optical resolution */
  /* use detected left margin  and fixed value */
  /* start */
  /* add x coordinates */
  start = ((dev->sensor.CCD_start_xoffset + startx) * used_res) / dev->sensor.optical_res;

  /* needs to be aligned for used_res */
  start = (start * optical_res) / used_res;

  start += dev->sensor.dummy_pixel + 1;

  if (stagger > 0)
    start |= 1;

  /* in case of SHDAREA, we need to align start
   * on pixel average factor, startx is different of
   * 0 only when calling for function to setup for
   * scan, where shading data needs to be align */
  if((dev->reg[reg_0x01].value & REG01_SHDAREA) != 0)
    {
      avg=optical_res/used_res;
      start=(start/avg)*avg;
    }

  /* compute correct pixels number */
  /* pixels */
  used_pixels = (pixels * optical_res) / xres;

  /* round up pixels number if needed */
  if (used_pixels * xres < pixels * optical_res)
      used_pixels++;

/* dummy */
  /* dummy lines: may not be usefull, for instance 250 dpi works with 0 or 1
     dummy line. Maybe the dummy line adds correctness since the motor runs
     slower (higher dpi)
  */
/* for cis this creates better aligned color lines:
dummy \ scanned lines
   0: R           G           B           R ...
   1: R        G        B        -        R ...
   2: R      G      B       -      -      R ...
   3: R     G     B     -     -     -     R ...
   4: R    G    B     -   -     -    -    R ...
   5: R    G   B    -   -   -    -   -    R ...
   6: R   G   B   -   -   -   -   -   -   R ...
   7: R   G  B   -  -   -   -  -   -  -   R ...
   8: R  G  B   -  -  -   -  -  -   -  -  R ...
   9: R  G  B  -  -  -  -  -  -  -  -  -  R ...
  10: R  G B  -  -  -  - -  -  -  -  - -  R ...
  11: R  G B  - -  - -  -  - -  - -  - -  R ...
  12: R G  B - -  - -  - -  - -  - - -  - R ...
  13: R G B  - - - -  - - -  - - - -  - - R ...
  14: R G B - - -  - - - - - -  - - - - - R ...
  15: R G B - - - - - - - - - - - - - - - R ...
 -- pierre
 */
  dummy = 0;

/* slope_dpi */
/* cis color scan is effectively a gray scan with 3 gray lines per color
   line and a FILTER of 0 */
  if (dev->model->is_cis)
      slope_dpi = yres*channels;
  else
      slope_dpi = yres;

  slope_dpi = slope_dpi * (1 + dummy);

  scan_step_type = gl841_scan_step_type(dev, yres);
  exposure_time = gl841_exposure_time(dev,
                    slope_dpi,
                    scan_step_type,
                    start,
                    used_pixels,
                    &scan_power_mode);
  DBG (DBG_info, "%s : exposure_time=%d pixels\n", __FUNCTION__, exposure_time);

  /*** optical parameters ***/
  /* in case of dynamic lineart, we use an internal 8 bit gray scan
   * to generate 1 lineart data */
  if(flags & SCAN_FLAG_DYNAMIC_LINEART)
    {
      depth=8;
    }

  oflags=0;
  if (flags & SCAN_FLAG_DISABLE_SHADING)
    {
      oflags |= OPTICAL_FLAG_DISABLE_SHADING;
    }
  if ((flags & SCAN_FLAG_DISABLE_GAMMA) || (depth==16))
    {
      oflags |= OPTICAL_FLAG_DISABLE_GAMMA;
    }
  if (flags & SCAN_FLAG_DISABLE_LAMP)
    {
      oflags |= OPTICAL_FLAG_DISABLE_LAMP;
    }
  if (flags & SCAN_FLAG_ENABLE_LEDADD)
    {
      oflags |= OPTICAL_FLAG_ENABLE_LEDADD;
    }

  status = gl841_init_optical_regs_scan(dev,
					reg,
					exposure_time,
					used_res,
					start,
					used_pixels,
					channels,
					depth,
					half_ccd,
					color_filter,
                                        oflags);
  if (status != SANE_STATUS_GOOD)
    {
      return status;
    }

/*** motor parameters ***/

  /* scanned area must be enlarged by max color shift needed */
  max_shift=sanei_genesys_compute_max_shift(dev,channels,yres,flags);

  /* lincnt */
  lincnt = lines + max_shift + stagger;

  /* add tl_y to base movement */
  move = starty;
  DBG (DBG_info, "gl841_init_scan_regs: move=%d steps\n", move);

  /* subtract current head position */
  move -= dev->scanhead_position_in_steps;
  DBG (DBG_info, "gl841_init_scan_regs: move=%d steps\n", move);

  if (move < 0)
      move = 0;

  /* round it */
/* the move is not affected by dummy -- pierre */
/*  move = ((move + dummy) / (dummy + 1)) * (dummy + 1);
    DBG (DBG_info, "gl841_init_scan_regs: move=%d steps\n", move);*/

  if (flags & SCAN_FLAG_SINGLE_LINE)
      status = gl841_init_motor_regs_off(reg, dev->model->is_cis?lincnt*channels:lincnt);
  else
      status = gl841_init_motor_regs_scan(dev,
					  reg,
					  exposure_time,
					  slope_dpi,
					  scan_step_type,
					  dev->model->is_cis?lincnt*channels:lincnt,
					  dummy,
					  move,
					  scan_power_mode,
					  (flags & SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE)?
					  MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE:0
	  );

  if (status != SANE_STATUS_GOOD)
      return status;


  /*** prepares data reordering ***/

/* words_per_line */
  bytes_per_line = (used_pixels * used_res) / optical_res;
  bytes_per_line = (bytes_per_line * channels * depth) / 8;

  requested_buffer_size = 8 * bytes_per_line;
  /* we must use a round number of bytes_per_line */
  if (requested_buffer_size > BULKIN_MAXSIZE)
    requested_buffer_size =
      (BULKIN_MAXSIZE / bytes_per_line) * bytes_per_line;

  read_buffer_size =
    2 * requested_buffer_size +
    ((max_shift + stagger) * used_pixels * channels * depth) / 8;

  RIE(sanei_genesys_buffer_free(&(dev->read_buffer)));
  RIE(sanei_genesys_buffer_alloc(&(dev->read_buffer), read_buffer_size));

  RIE(sanei_genesys_buffer_free(&(dev->lines_buffer)));
  RIE(sanei_genesys_buffer_alloc(&(dev->lines_buffer), read_buffer_size));

  RIE(sanei_genesys_buffer_free(&(dev->shrink_buffer)));
  RIE(sanei_genesys_buffer_alloc(&(dev->shrink_buffer),
				 requested_buffer_size));

  RIE(sanei_genesys_buffer_free(&(dev->out_buffer)));
  RIE(sanei_genesys_buffer_alloc(&(dev->out_buffer),
			(8 * dev->settings.pixels * channels * depth) / 8));


  dev->read_bytes_left = bytes_per_line * lincnt;

  DBG (DBG_info,
       "gl841_init_scan_regs: physical bytes to read = %lu\n",
       (u_long) dev->read_bytes_left);
  dev->read_active = SANE_TRUE;


  dev->current_setup.pixels = (used_pixels * used_res)/optical_res;
  dev->current_setup.lines = lincnt;
  dev->current_setup.depth = depth;
  dev->current_setup.channels = channels;
  dev->current_setup.exposure_time = exposure_time;
  dev->current_setup.xres = used_res;
  dev->current_setup.yres = yres;
  dev->current_setup.half_ccd = half_ccd;
  dev->current_setup.stagger = stagger;
  dev->current_setup.max_shift = max_shift + stagger;

/* TODO: should this be done elsewhere? */
  /* scan bytes to send to the frontend */
  /* theory :
     target_size =
     (dev->settings.pixels * dev->settings.lines * channels * depth) / 8;
     but it suffers from integer overflow so we do the following:

     1 bit color images store color data byte-wise, eg byte 0 contains
     8 bits of red data, byte 1 contains 8 bits of green, byte 2 contains
     8 bits of blue.
     This does not fix the overflow, though.
     644mp*16 = 10gp, leading to an overflow
   -- pierre
   */

  dev->total_bytes_read = 0;
  if (depth == 1)
      dev->total_bytes_to_read =
	  ((dev->settings.pixels * dev->settings.lines) / 8 +
	   (((dev->settings.pixels * dev->settings.lines)%8)?1:0)
	      ) * channels;
  else
      dev->total_bytes_to_read =
	  dev->settings.pixels * dev->settings.lines * channels * (depth / 8);

  DBG (DBG_info, "gl841_init_scan_regs: total bytes to send = %lu\n",
       (u_long) dev->total_bytes_to_read);
/* END TODO */

  DBG (DBG_proc, "gl841_init_scan_regs: completed\n");
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl841_calculate_current_setup (Genesys_Device * dev)
{
  int channels;
  int depth;
  int start;

  float xres;/*dpi*/
  float yres;/*dpi*/
  float startx;/*optical_res, from dummy_pixel+1*/
  float pixels;
  float lines;

  int used_res;
  int used_pixels;
  unsigned int lincnt;
  int exposure_time;
  int scan_power_mode;
  int i;
  int stagger;

  int slope_dpi = 0;
  int dummy = 0;
  int scan_step_type = 1;
  int max_shift;

  SANE_Bool half_ccd;		/* false: full CCD res is used, true, half max CCD res is used */
  int optical_res;

  DBG (DBG_info,
       "gl841_calculate_current_setup settings:\n"
       "Resolution: %uDPI\n"
       "Lines     : %u\n"
       "PPL       : %u\n"
       "Startpos  : %.3f/%.3f\n"
       "Scan mode : %d\n\n",
       dev->settings.yres, dev->settings.lines, dev->settings.pixels,
       dev->settings.tl_x, dev->settings.tl_y, dev->settings.scan_mode);

/* channels */
  if (dev->settings.scan_mode == 4)	/* single pass color */
    channels = 3;
  else
    channels = 1;

/* depth */
  depth = dev->settings.depth;
  if (dev->settings.scan_mode == 0)
      depth = 1;

/* start */
  start = SANE_UNFIX (dev->model->x_offset);

  start += dev->settings.tl_x;

  start = (start * dev->sensor.optical_res) / MM_PER_INCH;


  xres = dev->settings.xres;/*dpi*/
  yres = dev->settings.yres;/*dpi*/
  startx = start;/*optical_res, from dummy_pixel+1*/
  pixels = dev->settings.pixels;
  lines = dev->settings.lines;

  DBG (DBG_info,
       "gl841_calculate_current_setup settings:\n"
       "Resolution    : %gDPI/%gDPI\n"
       "Lines         : %g\n"
       "PPL           : %g\n"
       "Startpos      : %g\n"
       "Depth/Channels: %u/%u\n\n",
       xres, yres, lines, pixels,
       startx,
       depth, channels);

/* half_ccd */
  /* we have 2 domains for ccd: xres below or above half ccd max dpi */
  if ((dev->sensor.optical_res  < 2 * xres) ||
     !(dev->model->flags & GENESYS_FLAG_HALF_CCD_MODE)) {
      half_ccd = SANE_FALSE;
  } else {
      half_ccd = SANE_TRUE;
  }

/* optical_res */

  optical_res = dev->sensor.optical_res;
  if (half_ccd)
      optical_res /= 2;

/* stagger */

  if ((!half_ccd) && (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE))
    stagger = (4 * yres) / dev->motor.base_ydpi;
  else
    stagger = 0;
  DBG (DBG_info, "gl841_calculate_current_setup: stagger=%d lines\n",
       stagger);

/* used_res */
  i = optical_res / xres;

/* gl841 supports 1/1 1/2 1/3 1/4 1/5 1/6 1/8 1/10 1/12 1/15 averaging */

  if (i < 2) /* optical_res >= xres > optical_res/2 */
      used_res = optical_res;
  else if (i < 3)  /* optical_res/2 >= xres > optical_res/3 */
      used_res = optical_res/2;
  else if (i < 4)  /* optical_res/3 >= xres > optical_res/4 */
      used_res = optical_res/3;
  else if (i < 5)  /* optical_res/4 >= xres > optical_res/5 */
      used_res = optical_res/4;
  else if (i < 6)  /* optical_res/5 >= xres > optical_res/6 */
      used_res = optical_res/5;
  else if (i < 8)  /* optical_res/6 >= xres > optical_res/8 */
      used_res = optical_res/6;
  else if (i < 10)  /* optical_res/8 >= xres > optical_res/10 */
      used_res = optical_res/8;
  else if (i < 12)  /* optical_res/10 >= xres > optical_res/12 */
      used_res = optical_res/10;
  else if (i < 15)  /* optical_res/12 >= xres > optical_res/15 */
      used_res = optical_res/12;
  else
      used_res = optical_res/15;

  /* compute scan parameters values */
  /* pixels are allways given at half or full CCD optical resolution */
  /* use detected left margin  and fixed value */
/* start */
  /* add x coordinates */
  start =
      ((dev->sensor.CCD_start_xoffset + startx) * used_res) /
      dev->sensor.optical_res;

/* needs to be aligned for used_res */
  start = (start * optical_res) / used_res;

  start += dev->sensor.dummy_pixel + 1;

  if (stagger > 0)
    start |= 1;

  /* compute correct pixels number */
/* pixels */
  used_pixels =
    (pixels * optical_res) / xres;

  /* round up pixels number if needed */
  if (used_pixels * xres < pixels * optical_res)
      used_pixels++;

/* dummy */
  /* dummy lines: may not be usefull, for instance 250 dpi works with 0 or 1
     dummy line. Maybe the dummy line adds correctness since the motor runs
     slower (higher dpi)
  */
/* for cis this creates better aligned color lines:
dummy \ scanned lines
   0: R           G           B           R ...
   1: R        G        B        -        R ...
   2: R      G      B       -      -      R ...
   3: R     G     B     -     -     -     R ...
   4: R    G    B     -   -     -    -    R ...
   5: R    G   B    -   -   -    -   -    R ...
   6: R   G   B   -   -   -   -   -   -   R ...
   7: R   G  B   -  -   -   -  -   -  -   R ...
   8: R  G  B   -  -  -   -  -  -   -  -  R ...
   9: R  G  B  -  -  -  -  -  -  -  -  -  R ...
  10: R  G B  -  -  -  - -  -  -  -  - -  R ...
  11: R  G B  - -  - -  -  - -  - -  - -  R ...
  12: R G  B - -  - -  - -  - -  - - -  - R ...
  13: R G B  - - - -  - - -  - - - -  - - R ...
  14: R G B - - -  - - - - - -  - - - - - R ...
  15: R G B - - - - - - - - - - - - - - - R ...
 -- pierre
 */
  dummy = 0;

/* slope_dpi */
/* cis color scan is effectively a gray scan with 3 gray lines per color
   line and a FILTER of 0 */
  if (dev->model->is_cis)
      slope_dpi = yres*channels;
  else
      slope_dpi = yres;

  slope_dpi = slope_dpi * (1 + dummy);

  scan_step_type = gl841_scan_step_type(dev, yres);
  exposure_time = gl841_exposure_time(dev,
                    slope_dpi,
                    scan_step_type,
                    start,
                    used_pixels,
                    &scan_power_mode);
  DBG (DBG_info, "%s : exposure_time=%d pixels\n", __FUNCTION__, exposure_time);

  /* scanned area must be enlarged by max color shift needed */
  max_shift=sanei_genesys_compute_max_shift(dev,channels,yres,0);

  /* lincnt */
  lincnt = lines + max_shift + stagger;

  dev->current_setup.pixels = (used_pixels * used_res)/optical_res;
  dev->current_setup.lines = lincnt;
  dev->current_setup.depth = depth;
  dev->current_setup.channels = channels;
  dev->current_setup.exposure_time = exposure_time;
  dev->current_setup.xres = used_res;
  dev->current_setup.yres = yres;
  dev->current_setup.half_ccd = half_ccd;
  dev->current_setup.stagger = stagger;
  dev->current_setup.max_shift = max_shift + stagger;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static void
gl841_set_motor_power (Genesys_Register_Set * regs, SANE_Bool set)
{

  DBG (DBG_proc, "gl841_set_motor_power\n");

  if (set)
    {
      sanei_genesys_set_reg_from_set (regs, 0x02,
				      sanei_genesys_read_reg_from_set (regs,
								       0x02) |
				      REG02_MTRPWR);
    }
  else
    {
      sanei_genesys_set_reg_from_set (regs, 0x02,
				      sanei_genesys_read_reg_from_set (regs,
								       0x02) &
				      ~REG02_MTRPWR);
    }
}

static void
gl841_set_lamp_power (Genesys_Device * dev,
		      Genesys_Register_Set * regs, SANE_Bool set)
{
  Genesys_Register_Set * r;
  int i;

  if (set)
    {
      sanei_genesys_set_reg_from_set (regs, 0x03,
				      sanei_genesys_read_reg_from_set (regs,
								       0x03) |
				      REG03_LAMPPWR);

      r = sanei_genesys_get_address (regs, 0x10);
      for (i = 0; i < 6; i++, r++) {
	if (dev->sensor.regs_0x10_0x1d[i] == 0x00)
	  r->value = 0x01;/*0x00 will not be accepted*/
	else
	  r->value = dev->sensor.regs_0x10_0x1d[i];
      }
      r = sanei_genesys_get_address (regs, 0x19);
      r->value = 0x50;
    }
  else
    {
      sanei_genesys_set_reg_from_set (regs, 0x03,
				      sanei_genesys_read_reg_from_set (regs,
								       0x03) &
				      ~REG03_LAMPPWR);

      r = sanei_genesys_get_address (regs, 0x10);
      for (i = 0; i < 6; i++, r++) {
	r->value = 0x01;/* 0x0101 is as off as possible */
      }
      r = sanei_genesys_get_address (regs, 0x19);
      r->value = 0xff;
    }
}

/*for fast power saving methods only, like disabling certain amplifiers*/
static SANE_Status
gl841_save_power(Genesys_Device * dev, SANE_Bool enable) {
    uint8_t val;

    DBG(DBG_proc, "gl841_save_power: enable = %d\n", enable);

    if (enable)
    {
	if (dev->model->gpo_type == GPO_CANONLIDE35)
	{
/* expect GPIO17 to be enabled, and GPIO9 to be disabled,
   while GPIO8 is disabled*/
/* final state: GPIO8 disabled, GPIO9 enabled, GPIO17 disabled,
   GPIO18 disabled*/

	    sanei_genesys_read_register(dev, REG6D, &val);
	    sanei_genesys_write_register(dev, REG6D, val | 0x80);

	    usleep(1000);

	    /*enable GPIO9*/
	    sanei_genesys_read_register(dev, REG6C, &val);
	    sanei_genesys_write_register(dev, REG6C, val | 0x01);

	    /*disable GPO17*/
	    sanei_genesys_read_register(dev, REG6B, &val);
	    sanei_genesys_write_register(dev, REG6B, val & ~REG6B_GPO17);

	    /*disable GPO18*/
	    sanei_genesys_read_register(dev, REG6B, &val);
	    sanei_genesys_write_register(dev, REG6B, val & ~REG6B_GPO18);

	    usleep(1000);

	    sanei_genesys_read_register(dev, REG6D, &val);
	    sanei_genesys_write_register(dev, REG6D, val & ~0x80);

	}
	if (dev->model->gpo_type == GPO_DP685)
	  {
	    sanei_genesys_read_register(dev, REG6B, &val);
	    sanei_genesys_write_register(dev, REG6B, val & ~REG6B_GPO17);
	    dev->reg[reg_0x6b].value &= ~REG6B_GPO17;
	    dev->calib_reg[reg_0x6b].value &= ~REG6B_GPO17;
	  }

	gl841_set_fe (dev, AFE_POWER_SAVE);

    }
    else
    {
	if (dev->model->gpo_type == GPO_CANONLIDE35)
	{
/* expect GPIO17 to be enabled, and GPIO9 to be disabled,
   while GPIO8 is disabled*/
/* final state: GPIO8 enabled, GPIO9 disabled, GPIO17 enabled,
   GPIO18 enabled*/

	    sanei_genesys_read_register(dev, REG6D, &val);
	    sanei_genesys_write_register(dev, REG6D, val | 0x80);

	    usleep(10000);

	    /*disable GPIO9*/
	    sanei_genesys_read_register(dev, REG6C, &val);
	    sanei_genesys_write_register(dev, REG6C, val & ~0x01);

	    /*enable GPIO10*/
	    sanei_genesys_read_register(dev, REG6C, &val);
	    sanei_genesys_write_register(dev, REG6C, val | 0x02);

	    /*enable GPO17*/
	    sanei_genesys_read_register(dev, REG6B, &val);
	    sanei_genesys_write_register(dev, REG6B, val | REG6B_GPO17);
	    dev->reg[reg_0x6b].value |= REG6B_GPO17;
	    dev->calib_reg[reg_0x6b].value |= REG6B_GPO17;

	    /*enable GPO18*/
	    sanei_genesys_read_register(dev, REG6B, &val);
	    sanei_genesys_write_register(dev, REG6B, val | REG6B_GPO18);
	    dev->reg[reg_0x6b].value |= REG6B_GPO18;
	    dev->calib_reg[reg_0x6b].value |= REG6B_GPO18;

	}
	if (dev->model->gpo_type == GPO_DP665
            || dev->model->gpo_type == GPO_DP685)
	  {
	    sanei_genesys_read_register(dev, REG6B, &val);
	    sanei_genesys_write_register(dev, REG6B, val | REG6B_GPO17);
	    dev->reg[reg_0x6b].value |= REG6B_GPO17;
	    dev->calib_reg[reg_0x6b].value |= REG6B_GPO17;
	  }

    }

    return SANE_STATUS_GOOD;
}

static SANE_Status
gl841_set_powersaving (Genesys_Device * dev,
			     int delay /* in minutes */ )
{
  SANE_Status status;
  Genesys_Register_Set local_reg[7];
  int rate, exposure_time, tgtime, time;

  DBG (DBG_proc, "gl841_set_powersaving (delay = %d)\n", delay);

  local_reg[0].address = 0x01;
  local_reg[0].value = sanei_genesys_read_reg_from_set (dev->reg, 0x01);	/* disable fastmode */

  local_reg[1].address = 0x03;
  local_reg[1].value = sanei_genesys_read_reg_from_set (dev->reg, 0x03);	/* Lamp power control */

  local_reg[2].address = 0x05;
  local_reg[2].value = sanei_genesys_read_reg_from_set (dev->reg, 0x05) /*& ~REG05_BASESEL*/;	/* 24 clocks/pixel */

  local_reg[3].address = 0x18;	/* Set CCD type */
  local_reg[3].value = 0x00;

  local_reg[4].address = 0x38;	/* line period low */
  local_reg[4].value = 0x00;

  local_reg[5].address = 0x39;	/* line period high */
  local_reg[5].value = 0x00;

  local_reg[6].address = 0x1c;	/* period times for LPeriod, expR,expG,expB, Z1MODE, Z2MODE */
  local_reg[6].value = sanei_genesys_read_reg_from_set (dev->reg, 0x05) & ~REG1C_TGTIME;

  if (!delay)
    local_reg[1].value = local_reg[1].value & 0xf0;	/* disable lampdog and set lamptime = 0 */
  else if (delay < 20)
    local_reg[1].value = (local_reg[1].value & 0xf0) | 0x09;	/* enable lampdog and set lamptime = 1 */
  else
    local_reg[1].value = (local_reg[1].value & 0xf0) | 0x0f;	/* enable lampdog and set lamptime = 7 */

  time = delay * 1000 * 60;	/* -> msec */
  exposure_time =
    (uint32_t) (time * 32000.0 /
		 (24.0 * 64.0 * (local_reg[1].value & REG03_LAMPTIM) *
		  1024.0) + 0.5);
  /* 32000 = system clock, 24 = clocks per pixel */
  rate = (exposure_time + 65536) / 65536;
  if (rate > 4)
    {
      rate = 8;
      tgtime = 3;
    }
  else if (rate > 2)
    {
      rate = 4;
      tgtime = 2;
    }
  else if (rate > 1)
    {
      rate = 2;
      tgtime = 1;
    }
  else
    {
      rate = 1;
      tgtime = 0;
    }

  local_reg[6].value |= tgtime;
  exposure_time /= rate;

  if (exposure_time > 65535)
    exposure_time = 65535;

  local_reg[4].value = exposure_time >> 8;	/* highbyte */
  local_reg[5].value = exposure_time & 255;	/* lowbyte */

  status =
    gl841_bulk_write_register (dev, local_reg,
			       sizeof (local_reg)/sizeof (local_reg[0]));
  if (status != SANE_STATUS_GOOD)
    DBG (DBG_error,
	 "gl841_set_powersaving: failed to bulk write registers: %s\n",
	 sane_strstatus (status));

  DBG (DBG_proc, "gl841_set_powersaving: completed\n");
  return status;
}

#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl841_start_action (Genesys_Device * dev)
{
  return sanei_genesys_write_register (dev, 0x0f, 0x01);
}

#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl841_stop_action (Genesys_Device * dev)
{
  Genesys_Register_Set local_reg[GENESYS_GL841_MAX_REGS+1];
  SANE_Status status;
  uint8_t val40, val;
  unsigned int loop;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  status = sanei_genesys_get_status (dev, &val);
  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }

  status = sanei_genesys_read_register(dev, 0x40, &val40);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "%s: failed to read home sensor: %s\n",__FUNCTION__,
	   sane_strstatus (status));
      DBGCOMPLETED;
      return status;
    }

  /* only stop action if needed */
  if (!(val40 & REG40_DATAENB) && !(val40 & REG40_MOTMFLG))
    {
      DBG (DBG_info, "%s: already stopped\n", __FUNCTION__);
      DBGCOMPLETED;
      return SANE_STATUS_GOOD;
    }

  memcpy (local_reg, dev->reg, (GENESYS_GL841_MAX_REGS+1) * sizeof (Genesys_Register_Set));

  gl841_init_optical_regs_off(local_reg);

  gl841_init_motor_regs_off(local_reg,0);
  status = gl841_bulk_write_register (dev, local_reg, GENESYS_GL841_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to bulk write registers: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      return status;
    }

  /* looks like writing the right registers to zero is enough to get the chip
     out of scan mode into command mode, actually triggering(writing to
     register 0x0f) seems to be unnecessary */

  loop = 10;
  while (loop > 0)
    {
      status = sanei_genesys_read_register(dev, 0x40, &val40);
      if (DBG_LEVEL >= DBG_io)
	{
	  sanei_genesys_print_status (val);
        }
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "%s: failed to read home sensor: %s\n",__FUNCTION__,
	       sane_strstatus (status));
	  DBGCOMPLETED;
	  return status;
	}

      /* if scanner is in command mode, we are done */
      if (!(val40 & REG40_DATAENB) && !(val40 & REG40_MOTMFLG))
	{
	  DBGCOMPLETED;
	  return SANE_STATUS_GOOD;
	}

      usleep(100*1000);
      loop--;
    }

  DBGCOMPLETED;
  return SANE_STATUS_IO_ERROR;
}

static SANE_Status
gl841_get_paper_sensor(Genesys_Device * dev, SANE_Bool * paper_loaded)
{
  SANE_Status status;
  uint8_t val;

  status = sanei_genesys_read_register(dev, REG6D, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_get_paper_sensor: failed to read gpio: %s\n",
	   sane_strstatus (status));
      return status;
    }
  *paper_loaded = (val & 0x1) == 0;
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl841_eject_document (Genesys_Device * dev)
{
  Genesys_Register_Set local_reg[GENESYS_GL841_MAX_REGS+1];
  SANE_Status status;
  uint8_t val;
  SANE_Bool paper_loaded;
  unsigned int init_steps;
  float feed_mm;
  int loop;

  DBG (DBG_proc, "gl841_eject_document\n");

  if (!dev->model->is_sheetfed == SANE_TRUE)
    {
      DBG (DBG_proc, "gl841_eject_document: there is no \"eject sheet\"-concept for non sheet fed\n");
      DBG (DBG_proc, "gl841_eject_document: finished\n");
      return SANE_STATUS_GOOD;
    }


  memset (local_reg, 0, sizeof (local_reg));
  val = 0;

  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_eject_document: failed to read status register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl841_stop_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_eject_document: failed to stop motor: %s\n",
	   sane_strstatus (status));
      return SANE_STATUS_IO_ERROR;
    }

  memcpy (local_reg, dev->reg, (GENESYS_GL841_MAX_REGS+1) * sizeof (Genesys_Register_Set));

  gl841_init_optical_regs_off(local_reg);

  gl841_init_motor_regs(dev,local_reg,
			65536,MOTOR_ACTION_FEED,0);

  status =
    gl841_bulk_write_register (dev, local_reg, GENESYS_GL841_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_eject_document: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl841_start_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_eject_document: failed to start motor: %s\n",
	   sane_strstatus (status));
      gl841_stop_action (dev);
      /* send original registers */
      gl841_bulk_write_register (dev, dev->reg, GENESYS_GL841_MAX_REGS);
      return status;
    }

  RIE(gl841_get_paper_sensor(dev, &paper_loaded));
  if (paper_loaded)
    {
      DBG (DBG_info,
	   "gl841_eject_document: paper still loaded\n");
      /* force document TRUE, because it is definitely present */
      dev->document = SANE_TRUE;
      dev->scanhead_position_in_steps = 0;

      loop = 300;
      while (loop > 0)		/* do not wait longer then 30 seconds */
	{

	  RIE(gl841_get_paper_sensor(dev, &paper_loaded));

	  if (!paper_loaded)
	    {
	      DBG (DBG_info,
		   "gl841_eject_document: reached home position\n");
	      DBG (DBG_proc, "gl841_eject_document: finished\n");
	      break;
	    }
	  usleep (100000);	/* sleep 100 ms */
	  --loop;
	}

      if (loop == 0)
	{
	  /* when we come here then the scanner needed too much time for this, so we better stop the motor */
	  gl841_stop_action (dev);
	  DBG (DBG_error,
	       "gl841_eject_document: timeout while waiting for scanhead to go home\n");
	  return SANE_STATUS_IO_ERROR;
	}
    }

  feed_mm = SANE_UNFIX(dev->model->eject_feed);
  if (dev->document)
    {
      feed_mm += SANE_UNFIX(dev->model->post_scan);
    }

  status = sanei_genesys_read_feed_steps(dev, &init_steps);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_eject_document: failed to read feed steps: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* now feed for extra <number> steps */
  loop = 0;
  while (loop < 300)		/* do not wait longer then 30 seconds */
    {
      unsigned int steps;

      status = sanei_genesys_read_feed_steps(dev, &steps);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl841_eject_document: failed to read feed steps: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_info, "gl841_eject_document: init_steps: %d, steps: %d\n",
	   init_steps, steps);

      if (steps > init_steps + (feed_mm * dev->motor.base_ydpi) / MM_PER_INCH)
	{
	  break;
	}

      usleep (100000);	/* sleep 100 ms */
      ++loop;
    }

  status = gl841_stop_action(dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_eject_document: failed to stop motor: %s\n",
	   sane_strstatus (status));
      return status;
    }

  dev->document = SANE_FALSE;

  DBG (DBG_proc, "gl841_eject_document: finished\n");
  return SANE_STATUS_GOOD;
}


static SANE_Status
gl841_load_document (Genesys_Device * dev)
{
  SANE_Status status;
  SANE_Bool paper_loaded;
  int loop = 300;
  DBG (DBG_proc, "gl841_load_document\n");
  while (loop > 0)		/* do not wait longer then 30 seconds */
    {

      RIE(gl841_get_paper_sensor(dev, &paper_loaded));

      if (paper_loaded)
	{
	  DBG (DBG_info,
	       "gl841_load_document: document inserted\n");

	  /* when loading OK, document is here */
	  dev->document = SANE_TRUE;

	  usleep (1000000); /* give user 1000ms to place document correctly */
	  break;
	}
      usleep (100000);	/* sleep 100 ms */
      --loop;
    }

  if (loop == 0)
    {
      /* when we come here then the user needed to much time for this */
      DBG (DBG_error,
	   "gl841_load_document: timeout while waiting for document\n");
      return SANE_STATUS_IO_ERROR;
    }

  DBG (DBG_proc, "gl841_load_document: finished\n");
  return SANE_STATUS_GOOD;
}

/**
 * detects end of document and adjust current scan
 * to take it into account
 * used by sheetfed scanners
 */
static SANE_Status
gl841_detect_document_end (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Bool paper_loaded;
  unsigned int scancnt = 0, lincnt, postcnt;
  uint8_t val;
  size_t total_bytes_to_read;

  DBG (DBG_proc, "%s: begin\n", __FUNCTION__);

  RIE (gl841_get_paper_sensor (dev, &paper_loaded));

  /* sheetfed scanner uses home sensor as paper present */
  if ((dev->document == SANE_TRUE) && !paper_loaded)
    {
      DBG (DBG_info, "%s: no more document\n", __FUNCTION__);
      dev->document = SANE_FALSE;

      /* we can't rely on total_bytes_to_read since the frontend
       * might have been slow to read data, so we re-evaluate the
       * amount of data to scan form the hardware settings
       */
      status=sanei_genesys_read_scancnt(dev,&scancnt);
      if(status!=SANE_STATUS_GOOD)
        {
          dev->total_bytes_to_read = dev->total_bytes_read;
          dev->read_bytes_left = 0;
          DBG (DBG_proc, "%s: finished\n", __FUNCTION__);
          return SANE_STATUS_GOOD;
        }
      if (dev->settings.scan_mode == SCAN_MODE_COLOR && dev->model->is_cis)
        {
          scancnt/=3;
        }
      DBG (DBG_io, "%s: scancnt=%u lines\n",__FUNCTION__, scancnt);

      RIE(sanei_genesys_read_register(dev, 0x25, &val));
      lincnt=65536*val;
      RIE(sanei_genesys_read_register(dev, 0x26, &val));
      lincnt+=256*val;
      RIE(sanei_genesys_read_register(dev, 0x27, &val));
      lincnt+=val;
      DBG (DBG_io, "%s: lincnt=%u lines\n",__FUNCTION__, lincnt);
      postcnt=(SANE_UNFIX(dev->model->post_scan)/MM_PER_INCH)*dev->settings.yres;
      DBG (DBG_io, "%s: postcnt=%u lines\n",__FUNCTION__, postcnt);

      /* the current scancnt is also the final one, so we use it to
       * compute total bytes to read. We also add the line count to eject document */
      total_bytes_to_read=(scancnt+postcnt)*dev->wpl;

      DBG (DBG_io, "%s: old total_bytes_to_read=%u\n",__FUNCTION__,(unsigned int)dev->total_bytes_to_read);
      DBG (DBG_io, "%s: new total_bytes_to_read=%u\n",__FUNCTION__,(unsigned int)total_bytes_to_read);

      /* assign new end value */
      if(dev->total_bytes_to_read>total_bytes_to_read)
        {
          DBG (DBG_io, "%s: scan shorten\n",__FUNCTION__);
          dev->total_bytes_to_read=total_bytes_to_read;
        }
    }

  DBG (DBG_proc, "%s: finished\n", __FUNCTION__);
  return SANE_STATUS_GOOD;
}

/* Send the low-level scan command */
/* todo : is this that useful ? */
#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl841_begin_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
			SANE_Bool start_motor)
{
  SANE_Status status;
  Genesys_Register_Set local_reg[4];
  uint8_t val;

  DBG (DBG_proc, "gl841_begin_scan\n");

  if (dev->model->gpo_type == GPO_CANONLIDE80)
    {
      RIE (sanei_genesys_read_register (dev, REG6B, &val));
      val = REG6B_GPO18;
      RIE (sanei_genesys_write_register (dev, REG6B, val));
    }

  local_reg[0].address = 0x03;
  if (dev->model->ccd_type != CCD_PLUSTEK_3600)
    {
      local_reg[0].value = sanei_genesys_read_reg_from_set (reg, 0x03) | REG03_LAMPPWR;
    }
  else
    {
      local_reg[0].value = sanei_genesys_read_reg_from_set (reg, 0x03); /* TODO PLUSTEK_3600: why ?? */
    }

  local_reg[1].address = 0x01;
  local_reg[1].value = sanei_genesys_read_reg_from_set (reg, 0x01) | REG01_SCAN;	/* set scan bit */

  local_reg[2].address = 0x0d;
  local_reg[2].value = 0x01;

  local_reg[3].address = 0x0f;
  if (start_motor)
    local_reg[3].value = 0x01;
  else
    local_reg[3].value = 0x00;	/* do not start motor yet */

  status =
    gl841_bulk_write_register (dev, local_reg,
			       sizeof (local_reg)/sizeof (local_reg[0]));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_begin_scan: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl841_begin_scan: completed\n");

  return status;
}


/* Send the stop scan command */
#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl841_end_scan (Genesys_Device * dev, Genesys_Register_Set __sane_unused__ * reg,
		      SANE_Bool check_stop)
{
  SANE_Status status;

  DBG (DBG_proc, "gl841_end_scan (check_stop = %d)\n", check_stop);

  if (dev->model->is_sheetfed == SANE_TRUE)
    {
      status = SANE_STATUS_GOOD;
    }
  else				/* flat bed scanners */
    {
      status = gl841_stop_action (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl841_end_scan: failed to stop: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  DBG (DBG_proc, "gl841_end_scan: completed\n");

  return status;
}

/* Moves the slider to steps */
#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl841_feed (Genesys_Device * dev, int steps)
{
  Genesys_Register_Set local_reg[GENESYS_GL841_MAX_REGS+1];
  SANE_Status status;
  uint8_t val;
  int loop;

  DBG (DBG_proc, "gl841_feed (steps = %d)\n", steps);

  status = gl841_stop_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl841_feed: failed to stop action: %s\n",
	   sane_strstatus (status));
      return status;
    }

  memcpy (local_reg, dev->reg, (GENESYS_GL841_MAX_REGS+1) * sizeof (Genesys_Register_Set));

  gl841_init_optical_regs_off(local_reg);

  gl841_init_motor_regs(dev,local_reg, steps,MOTOR_ACTION_FEED,0);

  status = gl841_bulk_write_register (dev, local_reg, GENESYS_GL841_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_feed: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl841_start_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_feed: failed to start motor: %s\n",
	   sane_strstatus (status));
      gl841_stop_action (dev);
      /* send original registers */
      gl841_bulk_write_register (dev, dev->reg, GENESYS_GL841_MAX_REGS);
      return status;
    }

  loop = 0;
  while (loop < 300)		/* do not wait longer then 30 seconds */
  {
      status = sanei_genesys_get_status (dev, &val);
      if (status != SANE_STATUS_GOOD)
      {
	  DBG (DBG_error,
	       "gl841_feed: failed to read home sensor: %s\n",
	       sane_strstatus (status));
	  return status;
      }

      if (!(val & REG41_MOTORENB))	/* motor enabled */
      {
	  DBG (DBG_proc, "gl841_feed: finished\n");
	  dev->scanhead_position_in_steps += steps;
	  return SANE_STATUS_GOOD;
      }
      usleep (100000);	/* sleep 100 ms */
      ++loop;
  }

  /* when we come here then the scanner needed too much time for this, so we better stop the motor */
  gl841_stop_action (dev);

  DBG (DBG_error,
       "gl841_feed: timeout while waiting for scanhead to go home\n");
  return SANE_STATUS_IO_ERROR;
}

/* Moves the slider to the home (top) position slowly */
#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl841_slow_back_home (Genesys_Device * dev, SANE_Bool wait_until_home)
{
  Genesys_Register_Set local_reg[GENESYS_GL841_MAX_REGS+1];
  SANE_Status status;
  Genesys_Register_Set *r;
  uint8_t val;
  int loop = 0;

  DBG (DBG_proc, "gl841_slow_back_home (wait_until_home = %d)\n",
       wait_until_home);

  if (dev->model->is_sheetfed == SANE_TRUE)
    {
      DBG (DBG_proc, "gl841_slow_back_home: there is no \"home\"-concept for sheet fed\n");
      DBG (DBG_proc, "gl841_slow_back_home: finished\n");
      return SANE_STATUS_GOOD;
    }

  /* reset gpio pin */
  if (dev->model->gpo_type == GPO_CANONLIDE35)
    {
      RIE (sanei_genesys_read_register (dev, REG6C, &val));
      val = dev->gpo.value[0];
      RIE (sanei_genesys_write_register (dev, REG6C, val));
    }
  if (dev->model->gpo_type == GPO_CANONLIDE80)
    {
      RIE (sanei_genesys_read_register (dev, REG6B, &val));
      val = REG6B_GPO18 | REG6B_GPO17;
      RIE (sanei_genesys_write_register (dev, REG6B, val));
    }
  gl841_save_power(dev, SANE_FALSE);

  /* first read gives HOME_SENSOR true */
  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl847_slow_back_home: failed to read home sensor: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }
  usleep (100000);		/* sleep 100 ms */

  /* second is reliable */
  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_slow_back_home: failed to read home sensor: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }

  dev->scanhead_position_in_steps = 0;

  if (val & REG41_HOMESNR)	/* is sensor at home? */
    {
      DBG (DBG_info,
	   "gl841_slow_back_home: already at home, completed\n");
      dev->scanhead_position_in_steps = 0;
      return SANE_STATUS_GOOD;
    }

  /* end previous scan if any */
  r = sanei_genesys_get_address (dev->reg, REG01);
  r->value &= ~REG01_SCAN;
  status = sanei_genesys_write_register (dev, REG01, r->value);

  /* if motor is on, stop current action */
  if (val & REG41_MOTORENB)
    {
      status = gl841_stop_action (dev);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (DBG_error,
               "gl841_slow_back_home: failed to stop motor: %s\n",
               sane_strstatus (status));
          return SANE_STATUS_IO_ERROR;
        }
    }

  memcpy (local_reg, dev->reg, (GENESYS_GL841_MAX_REGS+1) * sizeof (Genesys_Register_Set));

  gl841_init_motor_regs(dev,local_reg, 65536,MOTOR_ACTION_GO_HOME,0);

  /* set up for reverse and no scan */
  r = sanei_genesys_get_address (local_reg, REG02);
  r->value |= REG02_MTRREV;
  r = sanei_genesys_get_address (local_reg, REG01);
  r->value &= ~REG01_SCAN;

  RIE (gl841_bulk_write_register (dev, local_reg, GENESYS_GL841_MAX_REGS));

  status = gl841_start_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_slow_back_home: failed to start motor: %s\n",
	   sane_strstatus (status));
      gl841_stop_action (dev);
      /* send original registers */
      gl841_bulk_write_register (dev, dev->reg, GENESYS_GL841_MAX_REGS);
      return status;
    }

  if (wait_until_home)
    {
      while (loop < 300)		/* do not wait longer then 30 seconds */
	{
	  status = sanei_genesys_get_status (dev, &val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl841_slow_back_home: failed to read home sensor: %s\n",
		   sane_strstatus (status));
	      return status;
	    }

	  if (val & REG41_HOMESNR)	/* home sensor */
	    {
	      DBG (DBG_info, "gl841_slow_back_home: reached home position\n");
	      DBG (DBG_proc, "gl841_slow_back_home: finished\n");
	      return SANE_STATUS_GOOD;
	    }
	  usleep (100000);	/* sleep 100 ms */
	  ++loop;
	}

      /* when we come here then the scanner needed too much time for this, so we better stop the motor */
      gl841_stop_action (dev);
      DBG (DBG_error,
	   "gl841_slow_back_home: timeout while waiting for scanhead to go home\n");
      return SANE_STATUS_IO_ERROR;
    }

  DBG (DBG_info, "gl841_slow_back_home: scanhead is still moving\n");
  DBG (DBG_proc, "gl841_slow_back_home: finished\n");
  return SANE_STATUS_GOOD;
}

/* Automatically set top-left edge of the scan area by scanning a 200x200 pixels
   area at 600 dpi from very top of scanner */
static SANE_Status
gl841_search_start_position (Genesys_Device * dev)
{
  int size;
  SANE_Status status;
  uint8_t *data;
  Genesys_Register_Set local_reg[GENESYS_GL841_MAX_REGS+1];
  int steps;

  int pixels = 600;
  int dpi = 300;

  DBGSTART;

  memcpy (local_reg, dev->reg, (GENESYS_GL841_MAX_REGS +1) * sizeof (Genesys_Register_Set));

  /* sets for a 200 lines * 600 pixels */
  /* normal scan with no shading */

  status = gl841_init_scan_regs (dev,
				 local_reg,
				 dpi,
				 dpi,
				 0,
				 0,/*we should give a small offset here~60 steps*/
				 600,
				 dev->model->search_lines,
				 8,
				 1,
				 1,/*green*/
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE);
  if(status!=SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to init scan registers: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      return status;
    }

  /* send to scanner */
  status =
    gl841_bulk_write_register (dev, local_reg, GENESYS_GL841_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to bulk write registers: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      return status;
    }

  size = pixels * dev->model->search_lines;

  data = malloc (size);
  if (!data)
    {
      DBG (DBG_error,
	   "gl841_search_start_position: failed to allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  status = gl841_begin_scan (dev, local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl841_search_start_position: failed to begin scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* waits for valid data */
  do
    sanei_genesys_test_buffer_empty (dev, &steps);
  while (steps);

  /* now we're on target, we can read data */
  status = sanei_genesys_read_data_from_scanner (dev, data, size);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl841_search_start_position: failed to read data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("search_position.pnm", data, 8, 1, pixels,
				  dev->model->search_lines);

  status = gl841_end_scan (dev, local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl841_search_start_position: failed to end scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* update regs to copy ASIC internal state */
  memcpy (dev->reg, local_reg, (GENESYS_GL841_MAX_REGS + 1) * sizeof (Genesys_Register_Set));

/*TODO: find out where sanei_genesys_search_reference_point
  stores information, and use that correctly*/
  status =
    sanei_genesys_search_reference_point (dev, data, 0, dpi, pixels,
					  dev->model->search_lines);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl841_search_start_position: failed to set search reference point: %s\n",
	   sane_strstatus (status));
      return status;
    }

  free (data);
  return SANE_STATUS_GOOD;
}

/*
 * sets up register for coarse gain calibration
 * todo: check it for scanners using it */
static SANE_Status
gl841_init_regs_for_coarse_calibration (Genesys_Device * dev)
{
  SANE_Status status;
  uint8_t channels;
  uint8_t cksel;

  DBGSTART;

  cksel = (dev->calib_reg[reg_0x18].value & REG18_CKSEL) + 1;	/* clock speed = 1..4 clocks */

  /* set line size */
  if (dev->settings.scan_mode == SCAN_MODE_COLOR)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  status = gl841_init_scan_regs (dev,
				 dev->calib_reg,
				 dev->settings.xres,
				 dev->settings.yres,
				 0,
				 0,
				 dev->sensor.optical_res / cksel, /* XXX STEF XXX !!! */
				 20,
				 16,
				 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE
      );
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_init_register_for_coarse_calibration: failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_info,
       "gl841_init_register_for_coarse_calibration: optical sensor res: %d dpi, actual res: %d\n",
       dev->sensor.optical_res / cksel, dev->settings.xres);

  status =
    gl841_bulk_write_register (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_init_register_for_coarse_calibration: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }


/*  if (DBG_LEVEL >= DBG_info)
    sanei_gl841_print_registers (dev->calib_reg);*/

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}


/* init registers for shading calibration */
static SANE_Status
gl841_init_regs_for_shading (Genesys_Device * dev)
{
  SANE_Status status;
  SANE_Int ydpi;
  float starty=0;

  DBGSTART;
  DBG (DBG_proc, "%s: lines = %d\n", __FUNCTION__, (int)(dev->calib_lines));

  /* initial calibration reg values */
  memcpy (dev->calib_reg, dev->reg, GENESYS_GL841_MAX_REGS * sizeof (Genesys_Register_Set));

  ydpi = dev->motor.base_ydpi;
  if (dev->model->motor_type == MOTOR_PLUSTEK_3600)  /* TODO PLUSTEK_3600: 1200dpi not yet working, produces dark bar */
    {
      ydpi = 600;
    }
  if (dev->model->motor_type == MOTOR_CANONLIDE80)
    {
      ydpi = gl841_get_dpihw(dev);
      /* get over extra dark area for this model */
      starty = 140;
    }

  dev->calib_channels = 3;
  dev->calib_lines = dev->model->shading_lines;
  status = gl841_init_scan_regs (dev,
				 dev->calib_reg,
				 dev->settings.xres,
				 ydpi,
				 0,
				 starty,
				 (dev->sensor.sensor_pixels * dev->settings.xres) / dev->sensor.optical_res,
				 dev->calib_lines,
				 16,
				 dev->calib_channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
                                 SCAN_FLAG_USE_OPTICAL_RES |
				 /*SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE |*/
				 SCAN_FLAG_IGNORE_LINE_DISTANCE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to setup scan: %s\n", __FUNCTION__, sane_strstatus (status));
      return status;
    }

  dev->calib_pixels = dev->current_setup.pixels;
  dev->scanhead_position_in_steps += dev->calib_lines + starty;

  status = gl841_bulk_write_register (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to bulk write registers: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      return status;
    }

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/* set up registers for the actual scan
 */
static SANE_Status
gl841_init_regs_for_scan (Genesys_Device * dev)
{
  int channels;
  int flags;
  int depth;
  float move;
  int move_dpi;
  float start;

  SANE_Status status;

  DBG (DBG_info,
       "gl841_init_regs_for_scan settings:\nResolution: %uDPI\n"
       "Lines     : %u\nPPL       : %u\nStartpos  : %.3f/%.3f\nScan mode : %d\n\n",
       dev->settings.yres, dev->settings.lines, dev->settings.pixels,
       dev->settings.tl_x, dev->settings.tl_y, dev->settings.scan_mode);

  gl841_slow_back_home(dev,SANE_TRUE);

/* channels */
  if (dev->settings.scan_mode == SCAN_MODE_COLOR)	/* single pass color */
    channels = 3;
  else
    channels = 1;

/* depth */
  depth = dev->settings.depth;
  if (dev->settings.scan_mode == SCAN_MODE_LINEART)
      depth = 1;


  /* steps to move to reach scanning area:
     - first we move to physical start of scanning
     either by a fixed steps amount from the black strip
     or by a fixed amount from parking position,
     minus the steps done during shading calibration
     - then we move by the needed offset whitin physical
     scanning area

     assumption: steps are expressed at maximum motor resolution

     we need:
     SANE_Fixed y_offset;
     SANE_Fixed y_size;
     SANE_Fixed y_offset_calib;
     mm_to_steps()=motor dpi / 2.54 / 10=motor dpi / MM_PER_INCH */

  /* if scanner uses GENESYS_FLAG_SEARCH_START y_offset is
     relative from origin, else, it is from parking position */

  move_dpi = dev->motor.base_ydpi;

  move = 0;
  if (dev->model->flags & GENESYS_FLAG_SEARCH_START)
    {
      move += SANE_UNFIX (dev->model->y_offset_calib);
    }

  DBG (DBG_info, "gl841_init_regs_for_scan: move=%f steps\n", move);

  move += SANE_UNFIX (dev->model->y_offset);
  DBG (DBG_info, "gl841_init_regs_for_scan: move=%f steps\n", move);

  move += dev->settings.tl_y;
  DBG (DBG_info, "gl841_init_regs_for_scan: move=%f steps\n", move);

  move = (move * move_dpi) / MM_PER_INCH;

/* start */
  start = SANE_UNFIX (dev->model->x_offset);

  start += dev->settings.tl_x;

  start = (start * dev->sensor.optical_res) / MM_PER_INCH;

  flags=0;

  /* we enable true gray for cis scanners only, and just when doing
   * scan since color calibration is OK for this mode
   */
  flags = 0;

  /* true gray (led add for cis scanners) */
  if(dev->model->is_cis && dev->settings.true_gray
    && dev->settings.scan_mode != SCAN_MODE_COLOR)
    {
      DBG (DBG_io, "%s: activating LEDADD\n", __FUNCTION__);
      flags |= SCAN_FLAG_ENABLE_LEDADD;
    }

  /* enable emulated lineart from gray data */
  if(dev->settings.scan_mode == SCAN_MODE_LINEART
     && dev->settings.dynamic_lineart)
    {
      flags |= SCAN_FLAG_DYNAMIC_LINEART;
    }

  status = gl841_init_scan_regs (dev,
				 dev->reg,
				 dev->settings.xres,
				 dev->settings.yres,
				 start,
				 move,
				 dev->settings.pixels,
				 dev->settings.lines,
				 depth,
				 channels,
				 dev->settings.color_filter,
				 flags);

  if (status != SANE_STATUS_GOOD)
      return status;


  DBG (DBG_proc, "gl841_init_register_for_scan: completed\n");
  return SANE_STATUS_GOOD;
}

/*
 * this function sends generic gamma table (ie linear ones)
 * or the Sensor specific one if provided
 */
static SANE_Status
gl841_send_gamma_table (Genesys_Device * dev)
{
  int size;
  SANE_Status status;
  uint8_t *gamma;

  DBGSTART;

  size = 256;

  /* allocate temporary gamma tables: 16 bits words, 3 channels */
  gamma = (uint8_t *) malloc (size * 2 * 3);
  if (gamma==NULL)
    {
      return SANE_STATUS_NO_MEM;
    }

  RIE(sanei_genesys_generate_gamma_buffer(dev, 16, 65535, size, gamma));

  /* send address */
  status = gl841_set_buffer_address_gamma (dev, 0x00000);
  if (status != SANE_STATUS_GOOD)
    {
      free (gamma);
      DBG (DBG_error,
	   "gl841_send_gamma_table: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* send data */
  status = gl841_bulk_write_data_gamma (dev, 0x28, (uint8_t *) gamma, size * 2 * 3);
  if (status != SANE_STATUS_GOOD)
    {
      free (gamma);
      DBG (DBG_error,
	   "gl841_send_gamma_table: failed to send gamma table: %s\n",
	   sane_strstatus (status));
      return status;
    }

  free (gamma);
  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}


/* this function does the led calibration by scanning one line of the calibration
   area below scanner's top on white strip.

-needs working coarse/gain
*/
GENESYS_STATIC SANE_Status
gl841_led_calibration (Genesys_Device * dev)
{
  int num_pixels;
  int total_size;
  uint8_t *line;
  int i, j;
  SANE_Status status = SANE_STATUS_GOOD;
  int val;
  int channels;
  int avg[3], avga, avge;
  int turn;
  char fn[20];
  uint16_t exp[3], target;
  Genesys_Register_Set *r;
  int move;

  SANE_Bool acceptable = SANE_FALSE;

  /* these 2 boundaries should be per sensor */
  uint16_t min_exposure=500;
  uint16_t max_exposure;

  DBGSTART;

  /* feed to white strip if needed */
  if (dev->model->y_offset_calib>0)
    {
      move = SANE_UNFIX (dev->model->y_offset_calib);
      move = (move * (dev->motor.base_ydpi)) / MM_PER_INCH;
      DBG (DBG_io, "%s: move=%d lines\n", __FUNCTION__, move);
      status = gl841_feed(dev, move);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "%s: failed to feed: %s\n", __FUNCTION__,
               sane_strstatus (status));
	  return status;
	}
    }

  /* offset calibration is always done in color mode */
  channels = 3;

  status = gl841_init_scan_regs (dev,
				 dev->calib_reg,
				 dev->settings.xres,
				 dev->settings.yres,
				 0,
				 0,
				 (dev->sensor.sensor_pixels*dev->settings.xres) / dev->sensor.optical_res,
				 1,
				 16,
				 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_USE_OPTICAL_RES
      );

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "%s: failed to setup scan: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      return status;
    }

  RIE (gl841_bulk_write_register(dev, dev->calib_reg, GENESYS_GL841_MAX_REGS));

  num_pixels = dev->current_setup.pixels;

  total_size = num_pixels * channels * 2 * 1;	/* colors * bytes_per_color * scan lines */

  line = malloc (total_size);
  if (!line)
    return SANE_STATUS_NO_MEM;

/*
   we try to get equal bright leds here:

   loop:
     average per color
     adjust exposure times
 */

  exp[0] = (dev->sensor.regs_0x10_0x1d[0] << 8) | dev->sensor.regs_0x10_0x1d[1];
  exp[1] = (dev->sensor.regs_0x10_0x1d[2] << 8) | dev->sensor.regs_0x10_0x1d[3];
  exp[2] = (dev->sensor.regs_0x10_0x1d[4] << 8) | dev->sensor.regs_0x10_0x1d[5];

  turn = 0;
  /* max exposure is set to ~2 time initial average
   * exposure, or 2 time last calibration exposure */
  max_exposure=((exp[0]+exp[1]+exp[2])/3)*2;
  target=dev->sensor.gain_white_ref*256;

  do {

      dev->sensor.regs_0x10_0x1d[0] = (exp[0] >> 8) & 0xff;
      dev->sensor.regs_0x10_0x1d[1] = exp[0] & 0xff;
      dev->sensor.regs_0x10_0x1d[2] = (exp[1] >> 8) & 0xff;
      dev->sensor.regs_0x10_0x1d[3] = exp[1] & 0xff;
      dev->sensor.regs_0x10_0x1d[4] = (exp[2] >> 8) & 0xff;
      dev->sensor.regs_0x10_0x1d[5] = exp[2] & 0xff;

      r = &(dev->calib_reg[reg_0x10]);
      for (i = 0; i < 6; i++, r++) {
	  r->value = dev->sensor.regs_0x10_0x1d[i];
	  RIE (sanei_genesys_write_register (dev, 0x10+i, dev->sensor.regs_0x10_0x1d[i]));
      }

      RIE (gl841_bulk_write_register (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS));

      DBG (DBG_info, "%s: starting line reading\n", __FUNCTION__);
      RIE (gl841_begin_scan (dev, dev->calib_reg, SANE_TRUE));
      RIE (sanei_genesys_read_data_from_scanner (dev, line, total_size));

      if (DBG_LEVEL >= DBG_data) {
	  snprintf(fn,20,"led_%d.pnm",turn);
	  sanei_genesys_write_pnm_file (fn,
					line,
					16,
					channels,
					num_pixels, 1);
      }

     /* compute average */
      for (j = 0; j < channels; j++)
      {
	  avg[j] = 0;
	  for (i = 0; i < num_pixels; i++)
	  {
	      if (dev->model->is_cis)
		  val =
		      line[i * 2 + j * 2 * num_pixels + 1] * 256 +
		      line[i * 2 + j * 2 * num_pixels];
	      else
		  val =
		      line[i * 2 * channels + 2 * j + 1] * 256 +
		      line[i * 2 * channels + 2 * j];
	      avg[j] += val;
	  }

	  avg[j] /= num_pixels;
      }

      DBG(DBG_info,"%s: average: %d,%d,%d\n", __FUNCTION__, avg[0], avg[1], avg[2]);

      acceptable = SANE_TRUE;

     /* exposure is acceptable if each color is in the %5 range
      * of other color channels */
      if (avg[0] < avg[1] * 0.95 || avg[1] < avg[0] * 0.95 ||
	  avg[0] < avg[2] * 0.95 || avg[2] < avg[0] * 0.95 ||
	  avg[1] < avg[2] * 0.95 || avg[2] < avg[1] * 0.95)
        {
	  acceptable = SANE_FALSE;
        }

      /* led exposure is not acceptable if white level is too low
       * ~80 hardcoded value for white level */
      if(avg[0]<20000 || avg[1]<20000 || avg[2]<20000)
        {
	  acceptable = SANE_FALSE;
        }
      
      /* for scanners using target value */
      if(target>0)
        {
          acceptable = SANE_TRUE;
          for(i=0;i<3;i++)
            {
              /* we accept +- 2% delta from target */
              if(abs(avg[i]-target)>target/50)
                {
                  exp[i]=(exp[i]*target)/avg[i];
                  acceptable = SANE_FALSE;
                }
            }
        }
      else
        {
          if (!acceptable)
            {
              avga = (avg[0]+avg[1]+avg[2])/3;
              exp[0] = (exp[0] * avga) / avg[0];
              exp[1] = (exp[1] * avga) / avg[1];
              exp[2] = (exp[2] * avga) / avg[2];
              /*
                keep the resulting exposures below this value.
                too long exposure drives the ccd into saturation.
                we may fix this by relying on the fact that
                we get a striped scan without shading, by means of
                statistical calculation
              */
              avge = (exp[0] + exp[1] + exp[2]) / 3;

              if (avge > max_exposure) {
                  exp[0] = (exp[0] * max_exposure) / avge;
                  exp[1] = (exp[1] * max_exposure) / avge;
                  exp[2] = (exp[2] * max_exposure) / avge;
              }
              if (avge < min_exposure) {
                  exp[0] = (exp[0] * min_exposure) / avge;
                  exp[1] = (exp[1] * min_exposure) / avge;
                  exp[2] = (exp[2] * min_exposure) / avge;
              }

            }
        }

      RIE (gl841_stop_action (dev));

      turn++;

  } while (!acceptable && turn < 100);

  DBG(DBG_info,"%s: acceptable exposure: %d,%d,%d\n", __FUNCTION__, exp[0],exp[1],exp[2]);

  /* cleanup before return */
  free (line);

  gl841_slow_back_home(dev, SANE_TRUE);

  DBGCOMPLETED;
  return status;
}

/** @brief calibration for AD frontend devices
 * offset calibration assumes that the scanning head is on a black area
 * For LiDE80 analog frontend
 * 0x0003 : is gain and belongs to [0..63]
 * 0x0006 : is offset
 * We scan a line with no gain until average offset reaches the target
 */
static SANE_Status
ad_fe_offset_calibration (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int num_pixels;
  int total_size;
  uint8_t *line;
  int i;
  int average;
  int turn;
  char fn[20];
  int top;
  int bottom;
  int target;

  DBGSTART;

  /* don't impact 3600 behavior since we can't test it */
  if (dev->model->ccd_type == CCD_PLUSTEK_3600)
    {
      DBGCOMPLETED;
      return status;
    }

  status = gl841_init_scan_regs (dev,
				 dev->calib_reg,
				 dev->settings.xres,
				 dev->settings.yres,
				 0,
				 0,
				 (dev->sensor.sensor_pixels*dev->settings.xres) / dev->sensor.optical_res,
				 1,
				 8,
				 3,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_USE_OPTICAL_RES);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_offset_calibration: failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  num_pixels = dev->current_setup.pixels;
  total_size = num_pixels * 3 * 2 * 1;

  line = malloc (total_size);
  if (line==NULL)
    {
      DBGCOMPLETED;
      return SANE_STATUS_NO_MEM;
    }

  dev->frontend.gain[0] = 0x00;
  dev->frontend.gain[1] = 0x00;
  dev->frontend.gain[2] = 0x00;

  /* loop on scan until target offset is reached */
  turn=0;
  target=24;
  bottom=0;
  top=255;
  do {
      /* set up offset mid range */
      dev->frontend.offset[0] = (top+bottom)/2;
      dev->frontend.offset[1] = (top+bottom)/2;
      dev->frontend.offset[2] = (top+bottom)/2;

      /* scan line */
      DBG (DBG_info, "%s: starting line reading\n",__FUNCTION__);
      gl841_bulk_write_register (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS);
      gl841_set_fe(dev, AFE_SET);
      gl841_begin_scan (dev, dev->calib_reg, SANE_TRUE);
      sanei_genesys_read_data_from_scanner (dev, line, total_size);
      gl841_stop_action (dev);
      if (DBG_LEVEL >= DBG_data) {
	  snprintf(fn,20,"offset_%02d.pnm",turn);
	  sanei_genesys_write_pnm_file (fn, line, 8, 3, num_pixels, 1);
      }

      /* search for minimal value */
      average=0;
      for(i=0;i<total_size;i++)
        {
          average+=line[i];
        }
      average/=total_size;
      DBG (DBG_data, "%s: average=%d\n", __FUNCTION__, average);

      /* if min value is above target, the current value becomes the new top
       * else it is the new bottom */
      if(average>target)
        {
          top=(top+bottom)/2;
        }
      else
        {
          bottom=(top+bottom)/2;
        }
      turn++;
  } while ((top-bottom)>1 && turn < 100);

  dev->frontend.offset[0]=0;
  dev->frontend.offset[1]=0;
  dev->frontend.offset[2]=0;
  free(line);
  DBG (DBG_info, "%s: offset=(%d,%d,%d)\n", __FUNCTION__,
                                            dev->frontend.offset[0],
                                            dev->frontend.offset[1],
                                            dev->frontend.offset[2]);
  DBGCOMPLETED;
  return status;
}

/* this function does the offset calibration by scanning one line of the calibration
   area below scanner's top. There is a black margin and the remaining is white.
   sanei_genesys_search_start() must have been called so that the offsets and margins
   are allready known.

this function expects the slider to be where?
*/
GENESYS_STATIC SANE_Status
gl841_offset_calibration (Genesys_Device * dev)
{
  int num_pixels;
  int total_size;
  uint8_t *first_line, *second_line;
  int i, j;
  SANE_Status status = SANE_STATUS_GOOD;
  int val;
  int channels;
  int off[3],offh[3],offl[3],off1[3],off2[3];
  int min1[3],min2[3];
  int cmin[3],cmax[3];
  int turn;
  char fn[20];
  SANE_Bool acceptable = SANE_FALSE;
  int mintgt = 0x400;

  DBG (DBG_proc, "gl841_offset_calibration\n");

  /* Analog Device fronted have a different calibration */
  if ((dev->reg[reg_0x04].value & REG04_FESET) == 0x02)
    {
      return ad_fe_offset_calibration (dev);
    }

  /* offset calibration is always done in color mode */
  channels = 3;

  status = gl841_init_scan_regs (dev,
				 dev->calib_reg,
				 dev->settings.xres,
				 dev->settings.yres,
				 0,
				 0,
				 (dev->sensor.sensor_pixels*dev->settings.xres) / dev->sensor.optical_res,
				 1,
				 16,
				 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_USE_OPTICAL_RES |
				 SCAN_FLAG_DISABLE_LAMP
				 );

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_offset_calibration: failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  num_pixels = dev->current_setup.pixels;

  total_size = num_pixels * channels * 2 * 1;	/* colors * bytes_per_color * scan lines */

  first_line = malloc (total_size);
  if (!first_line)
    return SANE_STATUS_NO_MEM;

  second_line = malloc (total_size);
  if (!second_line)
    {
      free (first_line);
      return SANE_STATUS_NO_MEM;
    }

  /* scan first line of data with no offset nor gain */
/*WM8199: gain=0.73; offset=-260mV*/
/*okay. the sensor black level is now at -260mV. we only get 0 from AFE...*/
/* we should probably do real calibration here:
 * -detect acceptable offset with binary search
 * -calculate offset from this last version
 *
 * acceptable offset means
 *   - few completely black pixels(<10%?)
 *   - few completely white pixels(<10%?)
 *
 * final offset should map the minimum not completely black
 * pixel to 0(16 bits)
 *
 * this does account for dummy pixels at the end of ccd
 * this assumes slider is at black strip(which is not quite as black as "no
 * signal").
 *
 */
  dev->frontend.gain[0] = 0x00;
  dev->frontend.gain[1] = 0x00;
  dev->frontend.gain[2] = 0x00;
  offh[0] = 0xff;
  offh[1] = 0xff;
  offh[2] = 0xff;
  offl[0] = 0x00;
  offl[1] = 0x00;
  offl[2] = 0x00;
  turn = 0;

  do {

      RIEF2 (gl841_bulk_write_register
	   (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS), first_line, second_line);

      for (j=0; j < channels; j++) {
	  off[j] = (offh[j]+offl[j])/2;
	  dev->frontend.offset[j] = off[j];
      }

      status = gl841_set_fe(dev, AFE_SET);

      if (status != SANE_STATUS_GOOD)
      {
	  DBG (DBG_error,
	       "gl841_offset_calibration: failed to setup frontend: %s\n",
	   sane_strstatus (status));
	  return status;
      }

      DBG (DBG_info,
	   "gl841_offset_calibration: starting first line reading\n");
      RIEF2 (gl841_begin_scan (dev, dev->calib_reg, SANE_TRUE), first_line, second_line);

      RIEF2 (sanei_genesys_read_data_from_scanner (dev, first_line, total_size), first_line, second_line);

      if (DBG_LEVEL >= DBG_data) {
	  snprintf(fn,20,"offset1_%02d.pnm",turn);
	  sanei_genesys_write_pnm_file (fn,
					first_line,
					16,
					channels,
					num_pixels, 1);
      }

      acceptable = SANE_TRUE;

      for (j = 0; j < channels; j++)
      {
	  cmin[j] = 0;
	  cmax[j] = 0;

	  for (i = 0; i < num_pixels; i++)
	  {
	      if (dev->model->is_cis)
		  val =
		      first_line[i * 2 + j * 2 * num_pixels + 1] * 256 +
		      first_line[i * 2 + j * 2 * num_pixels];
	      else
		  val =
		      first_line[i * 2 * channels + 2 * j + 1] * 256 +
		      first_line[i * 2 * channels + 2 * j];
	      if (val < 10)
		  cmin[j]++;
	      if (val > 65525)
		  cmax[j]++;
	  }

          /* TODO the DP685 has a black strip in the middle of the sensor
           * should be handled in a more elegant way , could be a bug */
          if (dev->model->ccd_type == CCD_DP685)
              cmin[j] -= 20;

	  if (cmin[j] > num_pixels/100) {
	      acceptable = SANE_FALSE;
	      if (dev->model->is_cis)
		  offl[0] = off[0];
	      else
		  offl[j] = off[j];
	  }
	  if (cmax[j] > num_pixels/100) {
	      acceptable = SANE_FALSE;
	      if (dev->model->is_cis)
		  offh[0] = off[0];
	      else
		  offh[j] = off[j];
	  }
      }

      DBG(DBG_info,"gl841_offset_calibration: black/white pixels: "
	  "%d/%d,%d/%d,%d/%d\n",
	  cmin[0],cmax[0],cmin[1],cmax[1],cmin[2],cmax[2]);

      if (dev->model->is_cis) {
	  offh[2] = offh[1] = offh[0];
	  offl[2] = offl[1] = offl[0];
      }

      RIEF2 (gl841_stop_action (dev), first_line, second_line);

      turn++;
  } while (!acceptable && turn < 100);

  DBG(DBG_info,"gl841_offset_calibration: acceptable offsets: %d,%d,%d\n",
      off[0],off[1],off[2]);


  for (j = 0; j < channels; j++)
  {
      off1[j] = off[j];

      min1[j] = 65536;

      for (i = 0; i < num_pixels; i++)
      {
	  if (dev->model->is_cis)
	      val =
		  first_line[i * 2 + j * 2 * num_pixels + 1] * 256 +
		  first_line[i * 2 + j * 2 * num_pixels];
	  else
	      val =
		  first_line[i * 2 * channels + 2 * j + 1] * 256 +
		  first_line[i * 2 * channels + 2 * j];
	  if (min1[j] > val && val >= 10)
	      min1[j] = val;
      }
  }


  offl[0] = off[0];
  offl[1] = off[0];
  offl[2] = off[0];
  turn = 0;

  do {

      for (j=0; j < channels; j++) {
	  off[j] = (offh[j]+offl[j])/2;
	  dev->frontend.offset[j] = off[j];
      }

      status = gl841_set_fe(dev, AFE_SET);

      if (status != SANE_STATUS_GOOD)
      {
        free (first_line);
        free (second_line);
	  DBG (DBG_error,
	       "gl841_offset_calibration: failed to setup frontend: %s\n",
	   sane_strstatus (status));
	  return status;
      }

      DBG (DBG_info,
	   "gl841_offset_calibration: starting second line reading\n");
      RIEF2 (gl841_bulk_write_register
	   (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS), first_line, second_line);
      RIEF2 (gl841_begin_scan (dev, dev->calib_reg, SANE_TRUE), first_line, second_line);
      RIEF2 (sanei_genesys_read_data_from_scanner (dev, second_line, total_size), first_line, second_line);

      if (DBG_LEVEL >= DBG_data) {
	  snprintf(fn,20,"offset2_%02d.pnm",turn);
	  sanei_genesys_write_pnm_file (fn,
					second_line,
					16,
					channels,
					num_pixels, 1);
      }

      acceptable = SANE_TRUE;

      for (j = 0; j < channels; j++)
      {
	  cmin[j] = 0;
	  cmax[j] = 0;

	  for (i = 0; i < num_pixels; i++)
	  {
	      if (dev->model->is_cis)
		  val =
		      second_line[i * 2 + j * 2 * num_pixels + 1] * 256 +
		      second_line[i * 2 + j * 2 * num_pixels];
	      else
		  val =
		      second_line[i * 2 * channels + 2 * j + 1] * 256 +
		      second_line[i * 2 * channels + 2 * j];
	      if (val < 10)
		  cmin[j]++;
	      if (val > 65525)
		  cmax[j]++;
	  }

	  if (cmin[j] > num_pixels/100) {
	      acceptable = SANE_FALSE;
	      if (dev->model->is_cis)
		  offl[0] = off[0];
	      else
		  offl[j] = off[j];
	  }
	  if (cmax[j] > num_pixels/100) {
	      acceptable = SANE_FALSE;
	      if (dev->model->is_cis)
		  offh[0] = off[0];
	      else
		  offh[j] = off[j];
	  }
      }

      DBG(DBG_info,"gl841_offset_calibration: black/white pixels: "
	  "%d/%d,%d/%d,%d/%d\n",
	  cmin[0],cmax[0],cmin[1],cmax[1],cmin[2],cmax[2]);

      if (dev->model->is_cis) {
	  offh[2] = offh[1] = offh[0];
	  offl[2] = offl[1] = offl[0];
      }

      RIEF2 (gl841_stop_action (dev), first_line, second_line);

      turn++;

  } while (!acceptable && turn < 100);

  DBG(DBG_info,"gl841_offset_calibration: acceptable offsets: %d,%d,%d\n",
      off[0],off[1],off[2]);


  for (j = 0; j < channels; j++)
  {
      off2[j] = off[j];

      min2[j] = 65536;

      for (i = 0; i < num_pixels; i++)
      {
	  if (dev->model->is_cis)
	      val =
		  second_line[i * 2 + j * 2 * num_pixels + 1] * 256 +
		  second_line[i * 2 + j * 2 * num_pixels];
	  else
	      val =
		  second_line[i * 2 * channels + 2 * j + 1] * 256 +
		  second_line[i * 2 * channels + 2 * j];
	  if (min2[j] > val && val != 0)
	      min2[j] = val;
      }
  }

  DBG(DBG_info,"gl841_offset_calibration: first set: %d/%d,%d/%d,%d/%d\n",
      off1[0],min1[0],off1[1],min1[1],off1[2],min1[2]);

  DBG(DBG_info,"gl841_offset_calibration: second set: %d/%d,%d/%d,%d/%d\n",
      off2[0],min2[0],off2[1],min2[1],off2[2],min2[2]);

/*
  calculate offset for each channel
  based on minimal pixel value min1 at offset off1 and minimal pixel value min2
  at offset off2

  to get min at off, values are linearly interpolated:
  min=real+off*fact
  min1=real+off1*fact
  min2=real+off2*fact

  fact=(min1-min2)/(off1-off2)
  real=min1-off1*(min1-min2)/(off1-off2)

  off=(min-min1+off1*(min1-min2)/(off1-off2))/((min1-min2)/(off1-off2))

  off=(min*(off1-off2)+min1*off2-off1*min2)/(min1-min2)

 */
  for (j = 0; j < channels; j++)
  {
      if (min2[j]-min1[j] == 0) {
/*TODO: try to avoid this*/
	  DBG(DBG_warn,"gl841_offset_calibration: difference too small\n");
	  if (mintgt * (off1[j] - off2[j]) + min1[j] * off2[j] - min2[j] * off1[j] >= 0)
	      off[j] = 0x0000;
	  else
	      off[j] = 0xffff;
      } else
	  off[j] = (mintgt * (off1[j] - off2[j]) + min1[j] * off2[j] - min2[j] * off1[j])/(min1[j]-min2[j]);
      if (off[j] > 255)
	  off[j] = 255;
      if (off[j] < 0)
	  off[j] = 0;
      dev->frontend.offset[j] = off[j];
  }

  DBG(DBG_info,"gl841_offset_calibration: final offsets: %d,%d,%d\n",
      off[0],off[1],off[2]);

  if (dev->model->is_cis) {
      if (off[0] < off[1])
	  off[0] = off[1];
      if (off[0] < off[2])
	  off[0] = off[2];
      dev->frontend.offset[0] = off[0];
      dev->frontend.offset[1] = off[0];
      dev->frontend.offset[2] = off[0];
  }

  if (channels == 1)
    {
      dev->frontend.offset[1] = dev->frontend.offset[0];
      dev->frontend.offset[2] = dev->frontend.offset[0];
    }

  /* cleanup before return */
  free (first_line);
  free (second_line);
  DBG (DBG_proc, "gl841_offset_calibration: completed\n");
  return status;
}


/* alternative coarse gain calibration
   this on uses the settings from offset_calibration and
   uses only one scanline
 */
/*
  with offset and coarse calibration we only want to get our input range into
  a reasonable shape. the fine calibration of the upper and lower bounds will
  be done with shading.
 */
GENESYS_STATIC SANE_Status
gl841_coarse_gain_calibration (Genesys_Device * dev, int dpi)
{
  int num_pixels;
  int total_size;
  uint8_t *line;
  int i, j, channels;
  SANE_Status status = SANE_STATUS_GOOD;
  int max[3];
  float gain[3];
  int val;
  int lines=1;
  int move;

  DBG (DBG_proc, "%s: dpi=%d\n", __FUNCTION__, dpi);

  /* feed to white strip if needed */
  if (dev->model->y_offset_calib>0)
    {
      move = SANE_UNFIX (dev->model->y_offset_calib);
      move = (move * (dev->motor.base_ydpi)) / MM_PER_INCH;
      DBG (DBG_io, "%s: move=%d lines\n", __FUNCTION__, move);
      status = gl841_feed(dev, move);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "%s: failed to feed: %s\n", __FUNCTION__,
               sane_strstatus (status));
	  return status;
	}
    }

  /* coarse gain calibration is allways done in color mode */
  channels = 3;

  status = gl841_init_scan_regs (dev,
				 dev->calib_reg,
				 dev->settings.xres,
				 dev->settings.yres,
				 0,
				 0,
				 (dev->sensor.sensor_pixels*dev->settings.xres) / dev->sensor.optical_res,
				 lines,
				 16,
				 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_USE_OPTICAL_RES
      );

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to setup scan: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      return status;
    }

  RIE (gl841_bulk_write_register (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS));

  num_pixels = dev->current_setup.pixels;

  total_size = num_pixels * channels * 2 * lines;	/* colors * bytes_per_color * scan lines */

  line = malloc (total_size);
  if (!line)
    return SANE_STATUS_NO_MEM;

  RIEF (gl841_begin_scan (dev, dev->calib_reg, SANE_TRUE), line);
  RIEF (sanei_genesys_read_data_from_scanner (dev, line, total_size), line);

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("coarse.pnm", line, 16, channels, num_pixels, lines);

  /* average high level for each channel and compute gain
     to reach the target code
     we only use the central half of the CCD data         */
  for (j = 0; j < channels; j++)
    {
      max[j] = 0;
      for (i = 0; i < num_pixels; i++)
	{
	  if (dev->model->is_cis)
	      val =
		  line[i * 2 + j * 2 * num_pixels + 1] * 256 +
		  line[i * 2 + j * 2 * num_pixels];
	  else
	      val =
		  line[i * 2 * channels + 2 * j + 1] * 256 +
		  line[i * 2 * channels + 2 * j];

	  if (val > max[j])
	    max[j] = val;
	}

      gain[j] = 65535.0/max[j];

      if (dev->model->dac_type == DAC_CANONLIDE35 ||
	  dev->model->dac_type == DAC_WOLFSON_XP300 ||
	  dev->model->dac_type == DAC_WOLFSON_DSM600)
        {
	  gain[j] *= 0.69;/*seems we don't get the real maximum. empirically derived*/
	  if (283 - 208/gain[j] > 255)
	      dev->frontend.gain[j] = 255;
	  else if (283 - 208/gain[j] < 0)
	      dev->frontend.gain[j] = 0;
	  else
	      dev->frontend.gain[j] = 283 - 208/gain[j];
        }
      else if (dev->model->dac_type == DAC_CANONLIDE80)
        {
	      dev->frontend.gain[j] = gain[j]*12;
        }

      DBG (DBG_proc, "%s: channel %d, max=%d, gain = %f, setting:%d\n", __FUNCTION__,
	   j, max[j], gain[j],dev->frontend.gain[j]);
    }

  for (j = 0; j < channels; j++)
    {
      if(gain[j] > 10)
        {
	  DBG (DBG_error0, "**********************************************\n");
	  DBG (DBG_error0, "**********************************************\n");
	  DBG (DBG_error0, "****                                      ****\n");
	  DBG (DBG_error0, "****  Extremely low Brightness detected.  ****\n");
	  DBG (DBG_error0, "****  Check the scanning head is          ****\n");
	  DBG (DBG_error0, "****  unlocked and moving.                ****\n");
	  DBG (DBG_error0, "****                                      ****\n");
	  DBG (DBG_error0, "**********************************************\n");
	  DBG (DBG_error0, "**********************************************\n");
#ifdef SANE_STATUS_HW_LOCKED
	  return SANE_STATUS_HW_LOCKED;
#else
          return SANE_STATUS_JAMMED;
#endif
        }

    }

  if (dev->model->is_cis) {
      if (dev->frontend.gain[0] > dev->frontend.gain[1])
	  dev->frontend.gain[0] = dev->frontend.gain[1];
      if (dev->frontend.gain[0] > dev->frontend.gain[2])
	  dev->frontend.gain[0] = dev->frontend.gain[2];
      dev->frontend.gain[2] = dev->frontend.gain[1] = dev->frontend.gain[0];
  }

  if (channels == 1)
  {
      dev->frontend.gain[0] = dev->frontend.gain[1];
      dev->frontend.gain[2] = dev->frontend.gain[1];
  }

  free (line);
  DBG (DBG_info, "%s: gain=(%d,%d,%d)\n", __FUNCTION__,
                                            dev->frontend.gain[0],
                                            dev->frontend.gain[1],
                                            dev->frontend.gain[2]);

  RIE (gl841_stop_action (dev));

  gl841_slow_back_home(dev, SANE_TRUE);

  DBGCOMPLETED;
  return status;
}

/*
 * wait for lamp warmup by scanning the same line until difference
 * between 2 scans is below a threshold
 */
static SANE_Status
gl841_init_regs_for_warmup (Genesys_Device * dev,
				       Genesys_Register_Set * local_reg,
				       int *channels, int *total_size)
{
  int num_pixels = (int) (4 * 300);
  SANE_Status status = SANE_STATUS_GOOD;

  DBG (DBG_proc, "sanei_gl841_warmup_lamp\n");

  memcpy (local_reg, dev->reg, (GENESYS_GL841_MAX_REGS + 1) * sizeof (Genesys_Register_Set));

/* okay.. these should be defaults stored somewhere */
  dev->frontend.gain[0] = 0x00;
  dev->frontend.gain[1] = 0x00;
  dev->frontend.gain[2] = 0x00;
  dev->frontend.offset[0] = 0x80;
  dev->frontend.offset[1] = 0x80;
  dev->frontend.offset[2] = 0x80;

  status = gl841_init_scan_regs (dev,
				 local_reg,
				 dev->sensor.optical_res,
				 dev->settings.yres,
				 dev->sensor.dummy_pixel,
				 0,
				 num_pixels,
				 1,
				 16,
				 *channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_USE_OPTICAL_RES
      );

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_init_regs_for_warmup: failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  num_pixels = dev->current_setup.pixels;

  *total_size = num_pixels * 3 * 2 * 1;	/* colors * bytes_per_color * scan lines */

  RIE (gl841_bulk_write_register
       (dev, local_reg, GENESYS_GL841_MAX_REGS));

  return status;
}


/*
 * this function moves head without scanning, forward, then backward
 * so that the head goes to park position.
 * as a by-product, also check for lock
 */
#ifndef UNIT_TESTING
static
#endif
SANE_Status
sanei_gl841_repark_head (Genesys_Device * dev)
{
  SANE_Status status;

  DBG (DBG_proc, "sanei_gl841_repark_head\n");

  status = gl841_feed(dev,232);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_repark_head: failed to feed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* toggle motor flag, put an huge step number and redo move backward */
  status = gl841_slow_back_home (dev, SANE_TRUE);
  DBG (DBG_proc, "gl841_park_head: completed\n");
  return status;
}

static SANE_Status
gl841_is_compatible_calibration (Genesys_Device * dev,
				 Genesys_Calibration_Cache *cache,
				 int for_overwrite)
{
  SANE_Status status;
#ifdef HAVE_SYS_TIME_H
  struct timeval time;
#endif

  DBGSTART;

  /* calibration cache not working yet for this model */
  if (dev->model->ccd_type == CCD_PLUSTEK_3600)
    {
      return SANE_STATUS_UNSUPPORTED;
    }

  status = gl841_calculate_current_setup (dev);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_is_compatible_calibration: failed to calculate current setup: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl841_is_compatible_calibration: checking\n");

  if (dev->current_setup.half_ccd != cache->used_setup.half_ccd)
    return SANE_STATUS_UNSUPPORTED;

  /* a cache entry expires after 30 minutes for non sheetfed scanners */
  /* this is not taken into account when overwriting cache entries    */
#ifdef HAVE_SYS_TIME_H
  if(for_overwrite == SANE_FALSE)
    {
      gettimeofday (&time, NULL);
      if ((time.tv_sec - cache->last_calibration > 30 * 60)
          && (dev->model->is_sheetfed == SANE_FALSE))
        {
          DBG (DBG_proc, "%s: expired entry, non compatible cache\n",__FUNCTION__);
          return SANE_STATUS_UNSUPPORTED;
        }
    }
#endif

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/*
 * initialize ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 */
static SANE_Status
gl841_init (Genesys_Device * dev)
{
  SANE_Status status;
  uint8_t val;
  size_t size;
  uint8_t *line;
  int i;

  DBG_INIT ();
  DBGSTART;

  dev->scanhead_position_in_steps = 0;

  /* Check if the device has already been initialized and powered up */
  if (dev->already_initialized)
    {
      RIE (sanei_genesys_get_status (dev, &val));
      if (val & REG41_PWRBIT)
	{
	  DBG (DBG_info, "gl841_init: already initialized\n");
          DBGCOMPLETED;
	  return SANE_STATUS_GOOD;
	}
    }

  dev->dark_average_data = NULL;
  dev->white_average_data = NULL;

  dev->settings.color_filter = 0;

  /* ASIC reset */
  RIE (sanei_genesys_write_register (dev, 0x0e, 0x01));
  RIE (sanei_genesys_write_register (dev, 0x0e, 0x00));

  /* Set default values for registers */
  gl841_init_registers (dev);

  /* Write initial registers */
  RIE (gl841_bulk_write_register (dev, dev->reg, GENESYS_GL841_MAX_REGS));

  /* Test ASIC and RAM */
  if (!(dev->model->flags & GENESYS_FLAG_LAZY_INIT))
    {
      RIE (sanei_gl841_asic_test (dev));
    }

  /* Set analog frontend */
  RIE (gl841_set_fe (dev, AFE_INIT));

  /* Move home */
  RIE (gl841_slow_back_home (dev, SANE_TRUE));

  /* Init shading data */
  RIE (sanei_genesys_init_shading_data (dev, dev->sensor.sensor_pixels));

  /* ensure head is correctly parked, and check lock */
  if (dev->model->flags & GENESYS_FLAG_REPARK)
    {
      status = sanei_gl841_repark_head (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  if (status == SANE_STATUS_INVAL)
	    DBG (DBG_error0,
		 "Your scanner is locked. Please move the lock switch "
		 "to the unlocked position\n");
	  else
	    DBG (DBG_error,
		 "gl841_init: sanei_gl841_repark_head failed: %s\n",
		 sane_strstatus (status));
	  return status;
	}
    }

  /* initalize sensor gamma tables */
  size = 256;

  for(i=0;i<3;i++)
    {
      if (dev->sensor.gamma_table[i] == NULL)
        {
          dev->sensor.gamma_table[i] = (uint16_t *) malloc (2 * size);
          if (dev->sensor.gamma_table[i] == NULL)
            {
              DBG (DBG_error,
                   "gl841_init: could not allocate memory for gamma table %d\n",i);
              return SANE_STATUS_NO_MEM;
            }
          sanei_genesys_create_gamma_table (dev->sensor.gamma_table[i],
                                            size,
                                            65535,
                                            65535,
                                            dev->sensor.gamma[i]);
        }
    }

  /* send gamma tables */
  status = gl841_send_gamma_table (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_init: failed to send initial gamma tables: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* initial calibration reg values */
  memcpy (dev->calib_reg, dev->reg, (GENESYS_GL841_MAX_REGS + 1) * sizeof (Genesys_Register_Set));

  status = gl841_init_scan_regs (dev,
				 dev->calib_reg,
				 300,
				 300,
				 0,
				 0,
				 (16 * 300) / dev->sensor.optical_res,
				 1,
				 16,
				 3,
				 0,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE |
				 SCAN_FLAG_USE_OPTICAL_RES
      );

  RIE (gl841_bulk_write_register
       (dev, dev->calib_reg, GENESYS_GL841_MAX_REGS));

  size = dev->current_setup.pixels * 3 * 2 * 1;	/* colors * bytes_per_color * scan lines */

  line = malloc (size);
  if (!line)
    return SANE_STATUS_NO_MEM;

  DBG (DBG_info,
       "gl841_init: starting dummy data reading\n");
  RIEF (gl841_begin_scan (dev, dev->calib_reg, SANE_TRUE), line);

  sanei_usb_set_timeout(1000);/* 1 second*/

/*ignore errors. next read will succeed*/
  sanei_genesys_read_data_from_scanner (dev, line, size);
  free(line);

  sanei_usb_set_timeout(30 * 1000);/* 30 seconds*/

  RIE (gl841_end_scan (dev, dev->calib_reg, SANE_TRUE));


  memcpy (dev->calib_reg, dev->reg, (GENESYS_GL841_MAX_REGS + 1) * sizeof (Genesys_Register_Set));

  /* Set powersaving (default = 15 minutes) */
  RIE (gl841_set_powersaving (dev, 15));
  dev->already_initialized = SANE_TRUE;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl841_update_hardware_sensors (Genesys_Scanner * s)
{
  /* do what is needed to get a new set of events, but try to not lose
     any of them.
   */
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t val;

  if (s->dev->model->gpo_type == GPO_CANONLIDE35
   || s->dev->model->gpo_type == GPO_CANONLIDE80)
    {
      RIE(sanei_genesys_read_register(s->dev, REG6D, &val));

      if (s->val[OPT_SCAN_SW].b == s->last_val[OPT_SCAN_SW].b)
	s->val[OPT_SCAN_SW].b = (val & 0x01) == 0;
      if (s->val[OPT_FILE_SW].b == s->last_val[OPT_FILE_SW].b)
	s->val[OPT_FILE_SW].b = (val & 0x02) == 0;
      if (s->val[OPT_EMAIL_SW].b == s->last_val[OPT_EMAIL_SW].b)
	s->val[OPT_EMAIL_SW].b = (val & 0x04) == 0;
      if (s->val[OPT_COPY_SW].b == s->last_val[OPT_COPY_SW].b)
	s->val[OPT_COPY_SW].b = (val & 0x08) == 0;
    }

  if (s->dev->model->gpo_type == GPO_XP300 ||
      s->dev->model->gpo_type == GPO_DP665 ||
      s->dev->model->gpo_type == GPO_DP685)
    {
      RIE(sanei_genesys_read_register(s->dev, REG6D, &val));

      if (s->val[OPT_PAGE_LOADED_SW].b == s->last_val[OPT_PAGE_LOADED_SW].b)
	s->val[OPT_PAGE_LOADED_SW].b = (val & 0x01) == 0;
      if (s->val[OPT_SCAN_SW].b == s->last_val[OPT_SCAN_SW].b)
	s->val[OPT_SCAN_SW].b = (val & 0x02) == 0;
    }

  return status;
}

/** @brief search for a full width black or white strip.
 * This function searches for a black or white stripe across the scanning area.
 * When searching backward, the searched area must completely be of the desired
 * color since this area will be used for calibration which scans forward.
 * @param dev scanner device
 * @param forward SANE_TRUE if searching forward, SANE_FALSE if searching backward
 * @param black SANE_TRUE if searching for a black strip, SANE_FALSE for a white strip
 * @return SANE_STATUS_GOOD if a matching strip is found, SANE_STATUS_UNSUPPORTED if not
 */
static SANE_Status
gl841_search_strip (Genesys_Device * dev, SANE_Bool forward, SANE_Bool black)
{
  unsigned int pixels, lines, channels;
  SANE_Status status;
  Genesys_Register_Set local_reg[GENESYS_GL841_MAX_REGS + 1];
  size_t size;
  uint8_t *data;
  int steps, depth, dpi;
  unsigned int pass, count, found, x, y, length;
  char title[80];
  Genesys_Register_Set *r;
  uint8_t white_level=90;  /**< default white level to detect white dots */
  uint8_t black_level=60;  /**< default black level to detect black dots */

  DBG (DBG_proc, "gl841_search_strip %s %s\n", black ? "black" : "white",
       forward ? "forward" : "reverse");

  /* use maximum gain when doing forward white strip detection
   * since we don't have calibrated the sensor yet */
  if(!black && forward)
    {
      dev->frontend.gain[0] = 0xff;
      dev->frontend.gain[1] = 0xff;
      dev->frontend.gain[2] = 0xff;
    }

  gl841_set_fe (dev, AFE_SET);
  status = gl841_stop_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl841_search_strip: failed to stop: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* set up for a gray scan at lowest dpi */
  dpi = 9600;
  for (x = 0; x < MAX_RESOLUTIONS; x++)
    {
      if (dev->model->xdpi_values[x] > 0 && dev->model->xdpi_values[x] < dpi)
	dpi = dev->model->xdpi_values[x];
    }
  channels = 1;

  /* shading calibation is done with dev->motor.base_ydpi */
  /* lines = (dev->model->shading_lines * dpi) / dev->motor.base_ydpi; */
  lines = (10*dpi)/MM_PER_INCH;

  depth = 8;
  pixels = (dev->sensor.sensor_pixels * dpi) / dev->sensor.optical_res;
  size = pixels * channels * lines * (depth / 8);
  data = malloc (size);
  if (!data)
    {
      DBG (DBG_error, "gl841_search_strip: failed to allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  /* 20 cm max length for calibration sheet */
  length = ((200 * dpi) / MM_PER_INCH)/lines;

  dev->scanhead_position_in_steps = 0;

  memcpy (local_reg, dev->reg,
	  (GENESYS_GL841_MAX_REGS + 1) * sizeof (Genesys_Register_Set));

  status = gl841_init_scan_regs (dev,
				 local_reg,
				 dpi,
				 dpi,
				 0,
                                 0,
                                 pixels,
                                 lines,
                                 depth,
                                 channels,
                                 0,
                                 SCAN_FLAG_DISABLE_SHADING | SCAN_FLAG_DISABLE_GAMMA);
  if (status != SANE_STATUS_GOOD)
    {
      free(data);
      DBG (DBG_error, "%s: failed to setup for scan: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      return status;
    }

  /* set up for reverse or forward */
  r = sanei_genesys_get_address (local_reg, 0x02);
  if (forward)
    r->value &= ~4;
  else
    r->value |= 4;


  status = gl841_bulk_write_register (dev, local_reg, GENESYS_GL841_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      free(data);
      DBG (DBG_error,
	   "gl841_search_strip: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl841_begin_scan (dev, local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl841_search_strip: failed to begin scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* waits for valid data */
  do
    sanei_genesys_test_buffer_empty (dev, &steps);
  while (steps);

  /* now we're on target, we can read data */
  status = sanei_genesys_read_data_from_scanner (dev, data, size);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl841_search_start_position: failed to read data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl841_stop_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error, "gl841_search_strip: gl841_stop_action failed\n");
      return status;
    }

  pass = 0;
  if (DBG_LEVEL >= DBG_data)
    {
      sprintf (title, "search_strip_%s_%s%02u.pnm", black ? "black" : "white",
	       forward ? "fwd" : "bwd", pass);
      sanei_genesys_write_pnm_file (title, data, depth, channels, pixels,
				    lines);
    }

  /* loop until strip is found or maximum pass number done */
  found = 0;
  while (pass < length && !found)
    {
      status =
	gl841_bulk_write_register (dev, local_reg, GENESYS_GL841_MAX_REGS);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl841_search_strip: failed to bulk write registers: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      /* now start scan */
      status = gl841_begin_scan (dev, local_reg, SANE_TRUE);
      if (status != SANE_STATUS_GOOD)
	{
	  free (data);
	  DBG (DBG_error,
	       "gl841_search_strip: failed to begin scan: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      /* waits for valid data */
      do
	sanei_genesys_test_buffer_empty (dev, &steps);
      while (steps);

      /* now we're on target, we can read data */
      status = sanei_genesys_read_data_from_scanner (dev, data, size);
      if (status != SANE_STATUS_GOOD)
	{
	  free (data);
	  DBG (DBG_error,
	       "gl841_search_start_position: failed to read data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = gl841_stop_action (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  free (data);
	  DBG (DBG_error, "gl841_search_strip: gl841_stop_action failed\n");
	  return status;
	}

      if (DBG_LEVEL >= DBG_data)
	{
	  sprintf (title, "search_strip_%s_%s%02u.pnm",
		   black ? "black" : "white", forward ? "fwd" : "bwd", pass);
	  sanei_genesys_write_pnm_file (title, data, depth, channels, pixels,
					lines);
	}

      /* search data to find black strip */
      /* when searching forward, we only need one line of the searched color since we
       * will scan forward. But when doing backward search, we need all the area of the
       * same color */
      if (forward)
	{
	  for (y = 0; y < lines && !found; y++)
	    {
	      count = 0;
	      /* count of white/black pixels depending on the color searched */
	      for (x = 0; x < pixels; x++)
		{
		  /* when searching for black, detect white pixels */
		  if (black && data[y * pixels + x] > white_level)
		    {
		      count++;
		    }
		  /* when searching for white, detect black pixels */
		  if (!black && data[y * pixels + x] < black_level)
		    {
		      count++;
		    }
		}

	      /* at end of line, if count >= 3%, line is not fully of the desired color
	       * so we must go to next line of the buffer */
	      /* count*100/pixels < 3 */
	      if ((count * 100) / pixels < 3)
		{
		  found = 1;
		  DBG (DBG_data,
		       "gl841_search_strip: strip found forward during pass %d at line %d\n",
		       pass, y);
		}
	      else
		{
		  DBG (DBG_data,
		       "gl841_search_strip: pixels=%d, count=%d (%d%%)\n",
		       pixels, count, (100 * count) / pixels);
		}
	    }
	}
      else			/* since calibration scans are done forward, we need the whole area
				   to be of the required color when searching backward */
	{
	  count = 0;
	  for (y = 0; y < lines; y++)
	    {
	      /* count of white/black pixels depending on the color searched */
	      for (x = 0; x < pixels; x++)
		{
		  /* when searching for black, detect white pixels */
		  if (black && data[y * pixels + x] > white_level)
		    {
		      count++;
		    }
		  /* when searching for white, detect black pixels */
		  if (!black && data[y * pixels + x] < black_level)
		    {
		      count++;
		    }
		}
	    }

	  /* at end of area, if count >= 3%, area is not fully of the desired color
	   * so we must go to next buffer */
	  if ((count * 100) / (pixels * lines) < 3)
	    {
	      found = 1;
	      DBG (DBG_data,
		   "gl841_search_strip: strip found backward during pass %d \n",
		   pass);
	    }
	  else
	    {
	      DBG (DBG_data,
		   "gl841_search_strip: pixels=%d, count=%d (%d%%)\n", pixels,
		   count, (100 * count) / pixels);
	    }
	}
      pass++;
    }
  free (data);
  if (found)
    {
      status = SANE_STATUS_GOOD;
      DBG (DBG_info, "gl841_search_strip: %s strip found\n",
	   black ? "black" : "white");
    }
  else
    {
      status = SANE_STATUS_UNSUPPORTED;
      DBG (DBG_info, "gl841_search_strip: %s strip not found\n",
	   black ? "black" : "white");
    }

  DBG (DBG_proc, "gl841_search_strip: completed\n");
  return status;
}

/**
 * Send shading calibration data. The buffer is considered to always hold values
 * for all the channels.
 */
GENESYS_STATIC
SANE_Status
gl841_send_shading_data (Genesys_Device * dev, uint8_t * data, int size)
{
  SANE_Status status = SANE_STATUS_GOOD;
  uint32_t length, x, factor, pixels, i;
  uint32_t half;
  uint32_t lines, channels;
  uint16_t dpiset, dpihw, strpixel ,endpixel, beginpixel;
  uint8_t *buffer,*ptr,*src;

  DBGSTART;
  DBG( DBG_io2, "%s: writing %d bytes of shading data\n",__FUNCTION__,size);

  /* old method if no SHDAREA */
  if((dev->reg[reg_0x01].value & REG01_SHDAREA) == 0)
    {
      /* start address */
      status = sanei_genesys_set_buffer_address (dev, 0x0000);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (DBG_error, "%s: failed to set buffer address: %s\n", __FUNCTION__,
               sane_strstatus (status));
          return status;
        }

      /* shading data whole line */
      status = dev->model->cmd_set->bulk_write_data (dev, 0x3c, data, size);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (DBG_error, "%s: failed to send shading table: %s\n", __FUNCTION__,
               sane_strstatus (status));
          return status;
        }
      DBGCOMPLETED;
      return status;
    }

  /* data is whole line, we extract only the part for the scanned area */
  length = (uint32_t) (size / 3);
  sanei_genesys_get_double(dev->reg,REG_STRPIXEL,&strpixel);
  sanei_genesys_get_double(dev->reg,REG_ENDPIXEL,&endpixel);
  DBG( DBG_io2, "%s: STRPIXEL=%d, ENDPIXEL=%d, PIXELS=%d\n",__FUNCTION__,strpixel,endpixel,endpixel-strpixel);

  /* compute deletion/average factor */
  sanei_genesys_get_double(dev->reg,REG_DPISET,&dpiset);
  dpihw = gl841_get_dpihw(dev);
  half=dev->current_setup.half_ccd+1;
  factor=dpihw/dpiset;
  DBG( DBG_io2, "%s: dpihw=%d, dpiset=%d, half_ccd=%d, factor=%d\n",__FUNCTION__,dpihw,dpiset,half-1,factor);

  /* binary data logging */
  if(DBG_LEVEL>=DBG_data)
    {
      dev->binary=fopen("binary.pnm","wb");
      sanei_genesys_get_triple(dev->reg, REG_LINCNT, &lines);
      channels=dev->current_setup.channels;
      if(dev->binary!=NULL)
        {
          fprintf(dev->binary,"P5\n%d %d\n%d\n",(endpixel-strpixel)/factor*channels,lines/channels,255);
        }
    }

  /* turn pixel value into bytes 2x16 bits words */
  strpixel*=2*2; /* 2 words of 2 bytes */
  endpixel*=2*2;
  pixels=endpixel-strpixel;

  /* shading pixel begin is start pixel minus start pixel during shading
   * calibration. Currently only cases handled are full and half ccd resolution.
   */
  beginpixel = dev->sensor.CCD_start_xoffset / half;
  beginpixel += dev->sensor.dummy_pixel + 1;
  DBG(DBG_io2, "%s: ORIGIN PIXEL=%d\n", __FUNCTION__, beginpixel);
  beginpixel = (strpixel-beginpixel*2*2)/factor;
  DBG(DBG_io2, "%s: BEGIN PIXEL=%d\n",__FUNCTION__,beginpixel/4);

  DBG(DBG_io2, "%s: using chunks of %d bytes (%d shading data pixels)\n",__FUNCTION__,length, length/4);
  buffer=(uint8_t *)malloc(pixels);
  memset(buffer,0,pixels);

  /* write actual shading data contigously
   * channel by channel, starting at addr 0x0000
   * */
  for(i=0;i<3;i++)
    {
      /* copy data to work buffer and process it */
          /* coefficent destination */
      ptr=buffer;

      /* iterate on both sensor segment, data has been averaged,
       * so is in the right order and we only have to copy it */
      for(x=0;x<pixels;x+=4)
        {
          /* coefficient source */
          src=data+x+beginpixel+i*length;
          ptr[0]=src[0];
          ptr[1]=src[1];
          ptr[2]=src[2];
          ptr[3]=src[3];

          /* next shading coefficient */
          ptr+=4;
        }

      /* 0x5400 alignment for LIDE80 internal memory */
      RIEF(sanei_genesys_set_buffer_address (dev, 0x5400*i), buffer);
      RIEF(dev->model->cmd_set->bulk_write_data (dev, 0x3c, buffer, pixels), buffer);
    }

  free(buffer);
  DBGCOMPLETED;

  return status;
}


/** the gl841 command set */
static Genesys_Command_Set gl841_cmd_set = {
  "gl841-generic",		/* the name of this set */

  gl841_init,
  gl841_init_regs_for_warmup,
  gl841_init_regs_for_coarse_calibration,
  gl841_init_regs_for_shading,
  gl841_init_regs_for_scan,

  gl841_get_filter_bit,
  gl841_get_lineart_bit,
  gl841_get_bitset_bit,
  gl841_get_gain4_bit,
  gl841_get_fast_feed_bit,
  gl841_test_buffer_empty_bit,
  gl841_test_motor_flag_bit,

  gl841_bulk_full_size,

  gl841_set_fe,
  gl841_set_powersaving,
  gl841_save_power,

  gl841_set_motor_power,
  gl841_set_lamp_power,

  gl841_begin_scan,
  gl841_end_scan,

  gl841_send_gamma_table,

  gl841_search_start_position,

  gl841_offset_calibration,
  gl841_coarse_gain_calibration,
  gl841_led_calibration,

  gl841_slow_back_home,

  gl841_bulk_write_register,
  gl841_bulk_write_data,
  gl841_bulk_read_data,

  gl841_update_hardware_sensors,

  gl841_load_document,
  gl841_detect_document_end,
  gl841_eject_document,
  gl841_search_strip,

  gl841_is_compatible_calibration,
  NULL,
  gl841_send_shading_data,
  gl841_calculate_current_setup,
  NULL,
  NULL
};

SANE_Status
sanei_gl841_init_cmd_set (Genesys_Device * dev)
{
  dev->model->cmd_set = &gl841_cmd_set;
  return SANE_STATUS_GOOD;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
