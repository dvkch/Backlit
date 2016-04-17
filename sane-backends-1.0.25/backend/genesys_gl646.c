/* sane - Scanner Access Now Easy.

   Copyright (C) 2003 Oliver Rauch
   Copyright (C) 2003, 2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004-2013 Stéphane Voltz <stef.dev@free.fr>
   Copyright (C) 2005-2009 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
   Copyright (C) 2007 Luke <iceyfor@gmail.com>
   Copyright (C) 2011 Alexey Osipov <simba@lerlan.ru> for HP2400 description
                      and tuning

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
#define BACKEND_NAME genesys_gl646

#include "genesys_gl646.h"

/**
 * returns the value hold by a 3 word register
 * @param regs register set from which reading the value
 * @param regnum number of the register to read
 * @return 24 bit value of the register
 */
static uint32_t
gl646_get_triple_reg (Genesys_Register_Set * regs, int regnum)
{
  Genesys_Register_Set *r = NULL;
  uint32_t ret = 0;

  r = sanei_genesys_get_address (regs, regnum);
  ret = r->value;
  r = sanei_genesys_get_address (regs, regnum + 1);
  ret = (ret << 8) + r->value;
  r = sanei_genesys_get_address (regs, regnum + 2);
  ret = (ret << 8) + r->value;

  return ret;
}

/**
 * returns the value hold by a 2 word register
 * @param regs register set from which reading the value
 * @param regnum number of the register to read
 * @return 16 bit value of the register
 */
static uint32_t
gl646_get_double_reg (Genesys_Register_Set * regs, int regnum)
{
  Genesys_Register_Set *r = NULL;
  uint32_t ret = 0;

  r = sanei_genesys_get_address (regs, regnum);
  ret = r->value;
  r = sanei_genesys_get_address (regs, regnum + 1);
  ret = (ret << 8) + r->value;

  return ret;
}

/* Write to many registers */
static SANE_Status
gl646_bulk_write_register (Genesys_Device * dev,
			   Genesys_Register_Set * reg, size_t elems)
{
  SANE_Status status;
  uint8_t outdata[8];
  uint8_t buffer[GENESYS_MAX_REGS * 2];
  size_t size;
  unsigned int i;

  /* handle differently sized register sets, reg[0x00] may be the last one */
  i = 0;
  while ((i < elems) && (reg[i].address != 0))
    i++;
  elems = i;
  size = i * 2;

  DBG (DBG_io, "gl646_bulk_write_register (elems= %lu, size = %lu)\n",
       (u_long) elems, (u_long) size);


  outdata[0] = BULK_OUT;
  outdata[1] = BULK_REGISTER;
  outdata[2] = 0x00;
  outdata[3] = 0x00;
  outdata[4] = (size & 0xff);
  outdata[5] = ((size >> 8) & 0xff);
  outdata[6] = ((size >> 16) & 0xff);
  outdata[7] = ((size >> 24) & 0xff);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_BUFFER,
			   VALUE_BUFFER, INDEX, sizeof (outdata), outdata);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_bulk_write_register: failed while writing command: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* copy registers and values in data buffer */
  for (i = 0; i < size; i += 2)
    {
      buffer[i] = reg[i / 2].address;
      buffer[i + 1] = reg[i / 2].value;
    }

  status = sanei_usb_write_bulk (dev->dn, buffer, &size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_bulk_write_register: failed while writing bulk data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (DBG_LEVEL >= DBG_io2)
    {
      for (i = 0; i < size; i += 2)
	{
	  DBG (DBG_io2, "reg[0x%02x] = 0x%02x\n", buffer[i], buffer[i + 1]);
	}
      /* when full size, decode register content */
      if (elems > 60)
	{
	  DBG (DBG_io2, "DPISET   =%d\n",
	       gl646_get_double_reg (reg, REG_DPISET));
	  DBG (DBG_io2, "DUMMY    =%d\n",
	       sanei_genesys_get_address (reg, REG_DUMMY)->value);
	  DBG (DBG_io2, "STRPIXEL =%d\n",
	       gl646_get_double_reg (reg, REG_STRPIXEL));
	  DBG (DBG_io2, "ENDPIXEL =%d\n",
	       gl646_get_double_reg (reg, REG_ENDPIXEL));
	  DBG (DBG_io2, "LINCNT   =%d\n",
	       gl646_get_triple_reg (reg, REG_LINCNT));
	  DBG (DBG_io2, "MAXWD    =%d\n",
	       gl646_get_triple_reg (reg, REG_MAXWD));
	  DBG (DBG_io2, "LPERIOD  =%d\n",
	       gl646_get_double_reg (reg, REG_LPERIOD));
	  DBG (DBG_io2, "FEEDL    =%d\n",
	       gl646_get_triple_reg (reg, REG_FEEDL));
	}
    }

  DBG (DBG_io, "gl646_bulk_write_register: wrote %lu bytes, %lu registers\n",
       (u_long) size, (u_long) elems);
  return status;
}

/* Write bulk data (e.g. shading, gamma) */
static SANE_Status
gl646_bulk_write_data (Genesys_Device * dev, uint8_t addr,
		       uint8_t * data, size_t len)
{
  SANE_Status status;
  size_t size;
  uint8_t outdata[8];

  DBG (DBG_io, "gl646_bulk_write_data writing %lu bytes\n", (u_long) len);

  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_bulk_write_data failed while setting register: %s\n",
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
      outdata[2] = 0x00;
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
	       "gl646_bulk_write_data failed while writing command: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = sanei_usb_write_bulk (dev->dn, data, &size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_bulk_write_data failed while writing bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "gl646_bulk_write_data wrote %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (len - size));

      len -= size;
      data += size;
    }

  DBG (DBG_io, "gl646_bulk_write_data: end\n");

  return status;
}

/**
 * reads value from gpio endpoint
 */
static SANE_Status
gl646_gpio_read (SANE_Int dn, uint8_t * value)
{
  return sanei_usb_control_msg (dn, REQUEST_TYPE_IN,
				REQUEST_REGISTER, GPIO_READ, INDEX, 1, value);
}

/**
 * writes the given value to gpio endpoint
 */
static SANE_Status
gl646_gpio_write (SANE_Int dn, uint8_t value)
{
  DBG (DBG_proc, "gl646_gpio_write(0x%02x)\n", value);
  return sanei_usb_control_msg (dn, REQUEST_TYPE_OUT,
				REQUEST_REGISTER, GPIO_WRITE,
				INDEX, 1, &value);
}

/**
 * writes the given value to gpio output enable endpoint
 */
static SANE_Status
gl646_gpio_output_enable (SANE_Int dn, uint8_t value)
{
  DBG (DBG_proc, "gl646_gpio_output_enable(0x%02x)\n", value);
  return sanei_usb_control_msg (dn, REQUEST_TYPE_OUT,
				REQUEST_REGISTER, GPIO_OUTPUT_ENABLE,
				INDEX, 1, &value);
}

/* Read bulk data (e.g. scanned data) */
static SANE_Status
gl646_bulk_read_data (Genesys_Device * dev, uint8_t addr,
		      uint8_t * data, size_t len)
{
  SANE_Status status;
  size_t size;
  uint8_t outdata[8];

  DBG (DBG_io, "gl646_bulk_read_data: requesting %lu bytes\n", (u_long) len);

  /* write requested size */
  status =
    sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER,
			   VALUE_SET_REGISTER, INDEX, 1, &addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_bulk_read_data failed while setting register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  outdata[0] = BULK_IN;
  outdata[1] = BULK_RAM;
  outdata[2] = 0x00;
  outdata[3] = 0x00;
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
	   "gl646_bulk_read_data failed while writing command: %s\n",
	   sane_strstatus (status));
      return status;
    }

  while (len)
    {
      if (len > GL646_BULKIN_MAXSIZE)
	size = GL646_BULKIN_MAXSIZE;
      else
	size = len;

      DBG (DBG_io2,
	   "gl646_bulk_read_data: trying to read %lu bytes of data\n",
	   (u_long) size);
      status = sanei_usb_read_bulk (dev->dn, data, &size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_bulk_read_data failed while reading bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      DBG (DBG_io2,
	   "gl646_bulk_read_data read %lu bytes, %lu remaining\n",
	   (u_long) size, (u_long) (len - size));

      len -= size;
      data += size;
    }

  if (dev->model->is_sheetfed == SANE_TRUE)
    {
      gl646_detect_document_end (dev);
    }

  DBG (DBG_io, "gl646_bulk_read_data: end\n");

  return status;
}

#if 0
static SANE_Status
read_triple_reg (Genesys_Device * dev, int index, unsigned int *words)
{
  SANE_Status status;
  uint8_t value;

  DBG (DBG_proc, "read_triple_reg\n");

  RIE (sanei_genesys_read_register (dev, index + 2, &value));
  *words = value;
  RIE (sanei_genesys_read_register (dev, index + 1, &value));
  *words += (value * 256);
  RIE (sanei_genesys_read_register (dev, index, &value));
  if (dev->model->asic_type == GENESYS_GL646)
    *words += ((value & 0x03) * 256 * 256);
  else
    *words += ((value & 0x0f) * 256 * 256);

  DBG (DBG_proc, "read_triple_reg: value=%d\n", *words);
  return status;
}
#endif


static SANE_Bool
gl646_get_fast_feed_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x02);
  if (r && (r->value & REG02_FASTFED))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl646_get_filter_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x04);
  if (r && (r->value & REG04_FILTER))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl646_get_lineart_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x04);
  if (r && (r->value & REG04_LINEART))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl646_get_bitset_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x04);
  if (r && (r->value & REG04_BITSET))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl646_get_gain4_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x06);
  if (r && (r->value & REG06_GAIN4))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl646_test_buffer_empty_bit (SANE_Byte val)
{
  if (val & REG41_BUFEMPTY)
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl646_test_motor_flag_bit (SANE_Byte val)
{
  if (val & REG41_MOTMFLG)
    return SANE_TRUE;
  return SANE_FALSE;
}

static void
gl646_set_triple_reg (Genesys_Register_Set * regs, int regnum, uint32_t value)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, regnum);
  r->value = LOBYTE (HIWORD (value));
  r = sanei_genesys_get_address (regs, regnum + 1);
  r->value = HIBYTE (LOWORD (value));
  r = sanei_genesys_get_address (regs, regnum + 2);
  r->value = LOBYTE (LOWORD (value));
}

static void
gl646_set_double_reg (Genesys_Register_Set * regs, int regnum, uint16_t value)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, regnum);
  r->value = HIBYTE (LOWORD (value));
  r = sanei_genesys_get_address (regs, regnum + 1);
  r->value = LOBYTE (LOWORD (value));
}

/**
 * decodes and prints content of status (0x41) register
 * @param val value read from reg41
 */
static void
print_status (uint8_t val)
{
  char msg[80];

  sprintf (msg, "%s%s%s%s%s%s%s%s",
	   val & REG41_PWRBIT ? "PWRBIT " : "",
	   val & REG41_BUFEMPTY ? "BUFEMPTY " : "",
	   val & REG41_FEEDFSH ? "FEEDFSH " : "",
	   val & REG41_SCANFSH ? "SCANFSH " : "",
	   val & REG41_HOMESNR ? "HOMESNR " : "",
	   val & REG41_LAMPSTS ? "LAMPSTS " : "",
	   val & REG41_FEBUSY ? "FEBUSY " : "",
	   val & REG41_MOTMFLG ? "MOTMFLG" : "");
  DBG (DBG_info, "status=%s\n", msg);
}

/**
 * start scanner's motor
 * @param dev scanner's device
 */
static SANE_Status
gl646_start_motor (Genesys_Device * dev)
{
  return sanei_genesys_write_register (dev, 0x0f, 0x01);
}


/**
 * stop scanner's motor
 * @param dev scanner's device
 */
static SANE_Status
gl646_stop_motor (Genesys_Device * dev)
{
  return sanei_genesys_write_register (dev, 0x0f, 0x00);
}


/**
 * find the lowest resolution for the sensor in the given mode.
 * @param sensor id of the sensor
 * @param color true is color mode
 * @return the closest resolution for the sensor and mode
 */
static int
get_lowest_resolution (int sensor, SANE_Bool color)
{
  int i, nb;
  int dpi;

  i = 0;
  dpi = 9600;
  nb = sizeof (sensor_master) / sizeof (Sensor_Master);
  while (i < nb)
    {
      /* computes distance and keep mode if it is closer than previous */
      if (sensor == sensor_master[i].sensor
	  && sensor_master[i].color == color)
	{
	  if (sensor_master[i].dpi < dpi)
	    {
	      dpi = sensor_master[i].dpi;
	    }
	}
      i++;
    }
  DBG (DBG_info, "get_lowest_resolution: %d\n", dpi);
  return dpi;
}

/**
 * find the closest match in mode tables for the given resolution and scan mode.
 * @param sensor id of the sensor
 * @param required required resolution
 * @param color true is color mode
 * @return the closest resolution for the sensor and mode
 */
static int
get_closest_resolution (int sensor, int required, SANE_Bool color)
{
  int i, nb;
  int dist, dpi;

  i = 0;
  dpi = 0;
  dist = 9600;
  nb = sizeof (sensor_master) / sizeof (Sensor_Master);
  while (i < nb)
    {
      /* exit on perfect match */
      if (sensor == sensor_master[i].sensor
	  && sensor_master[i].dpi == required
	  && sensor_master[i].color == color)
	{
	  DBG (DBG_info, "get_closest_resolution: match found for %d\n",
	       required);
	  return required;
	}
      /* computes distance and keep mode if it is closer than previous */
      if (sensor == sensor_master[i].sensor
	  && sensor_master[i].color == color)
	{
	  if (abs (sensor_master[i].dpi - required) < dist)
	    {
	      dpi = sensor_master[i].dpi;
	      dist = abs (sensor_master[i].dpi - required);
	    }
	}
      i++;
    }
  DBG (DBG_info, "get_closest_resolution: closest match for %d is %d\n",
       required, dpi);
  return dpi;
}

/**
 * Computes if sensor will be set up for half ccd pixels for the given
 * scan mode.
 * @param sensor id of the sensor
 * @param required required resolution
 * @param color true is color mode
 * @return SANE_TRUE if half ccd is used
 */
static SANE_Bool
is_half_ccd (int sensor, int required, SANE_Bool color)
{
  int i, nb;

  i = 0;
  nb = sizeof (sensor_master) / sizeof (Sensor_Master);
  while (i < nb)
    {
      /* exit on perfect match */
      if (sensor == sensor_master[i].sensor
	  && sensor_master[i].dpi == required
	  && sensor_master[i].color == color)
	{
	  DBG (DBG_io, "is_half_ccd: match found for %d (half_ccd=%d)\n",
	       required, sensor_master[i].half_ccd);
	  return sensor_master[i].half_ccd;
	}
      i++;
    }
  DBG (DBG_info, "is_half_ccd: failed to find match for %d dpi\n", required);
  return SANE_FALSE;
}

/**
 * Returns the cksel values used by the required scan mode.
 * @param sensor id of the sensor
 * @param required required resolution
 * @param color true is color mode
 * @return cksel value for mode
 */
static int
get_cksel (int sensor, int required, SANE_Bool color)
{
  int i, nb;

  i = 0;
  nb = sizeof (sensor_master) / sizeof (Sensor_Master);
  while (i < nb)
    {
      /* exit on perfect match */
      if (sensor == sensor_master[i].sensor
	  && sensor_master[i].dpi == required
	  && sensor_master[i].color == color)
	{
	  DBG (DBG_io, "get_cksel: match found for %d (cksel=%d)\n",
	       required, sensor_master[i].cksel);
	  return sensor_master[i].cksel;
	}
      i++;
    }
  DBG (DBG_error, "get_cksel: failed to find match for %d dpi\n", required);
  /* fail safe fallback */
  return 1;
}

/**
 * Setup register and motor tables for a scan at the
 * given resolution and color mode. TODO try to not use any filed from
 * the device.
 * @param dev          pointer to a struct describing the device
 * @param regs         register set to fill
 * @param scan_settings scan's settings
 * @param slope_table1 first motor table to fill
 * @param slope_table2 second motor table to fill
 * @param resolution   dpi of the scan
 * @param move         distance to move (at scan's dpi) before scan
 * @param linecnt      number of lines to scan at scan's dpi
 * @param startx       start of scan area on CCD at CCD's optical resolution
 * @param endx         end of scan area on CCD at CCD's optical resolution
 * @param color        SANE_TRUE is color scan
 * @param depth        1, 8 or 16 bits data sample
 * @return SANE_STATUS_GOOD if registers could be set, SANE_STATUS_INVAL if
 * conditions can't be met.
 * @note No harcoded SENSOR or MOTOR 'names' should be present and
 * registers are set from settings tables and flags related
 * to the hardware capabilities.
 * */
static SANE_Status
gl646_setup_registers (Genesys_Device * dev,
		       Genesys_Register_Set * regs,
		       Genesys_Settings scan_settings,
		       uint16_t * slope_table1,
		       uint16_t * slope_table2,
		       SANE_Int resolution,
		       uint32_t move,
		       uint32_t linecnt,
		       uint16_t startx,
		       uint16_t endx, SANE_Bool color,
                       SANE_Int depth)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int i, nb;
  Sensor_Master *sensor = NULL;
  Motor_Master *motor = NULL;
  Sensor_Settings *settings = NULL;
  Genesys_Register_Set *r;
  unsigned int used1, used2, vfinal;
  unsigned int bpp;   /**> bytes per pixel */
  uint32_t z1, z2;
  uint16_t ex, sx;
  int channels = 1, stagger, words_per_line, max_shift;
  size_t requested_buffer_size;
  size_t read_buffer_size;
  SANE_Bool half_ccd = SANE_FALSE;
  SANE_Int xresolution;
  int feedl;

  DBG (DBG_proc, "gl646_setup_registers: start\n");
  DBG (DBG_info, "gl646_setup_registers: startx=%d, endx=%d, linecnt=%d\n",
       startx, endx, linecnt);

  /* x resolution is capped by sensor's capability */
  if (resolution > dev->sensor.optical_res)
    {
      xresolution = dev->sensor.optical_res;
    }
  else
    {
      xresolution = resolution;
    }

  /* for the given resolution, search for master
   * sensor mode setting */
  i = 0;
  nb = sizeof (sensor_master) / sizeof (Sensor_Master);
  while (i < nb)
    {
      if (dev->model->ccd_type == sensor_master[i].sensor
	  && sensor_master[i].dpi == xresolution
	  && sensor_master[i].color == color)
	{
	  sensor = &sensor_master[i];
	}
      i++;
    }
  if (sensor == NULL)
    {
      DBG (DBG_error,
	   "gl646_setup_registers: unable to find settings for sensor %d at %d dpi color=%d\n",
	   dev->model->ccd_type, xresolution, color);
      return SANE_STATUS_INVAL;
    }

  /* for the given resolution, search for master
   * motor mode setting */
  i = 0;
  nb = sizeof (motor_master) / sizeof (Motor_Master);
  while (i < nb)
    {
      if (dev->model->motor_type == motor_master[i].motor
	  && motor_master[i].dpi == resolution
	  && motor_master[i].color == color)
	{
	  motor = &motor_master[i];
	}
      i++;
    }
  if (motor == NULL)
    {
      DBG (DBG_error,
	   "gl646_setup_registers: unable to find settings for motor %d at %d dpi, color=%d\n",
	   dev->model->motor_type, resolution, color);
      return SANE_STATUS_INVAL;
    }

  /* now we can search for the specific sensor settings */
  i = 0;
  nb = sizeof (sensor_settings) / sizeof (Sensor_Settings);
  while (i < nb)
    {
      if (sensor->sensor == sensor_settings[i].sensor
	  && sensor->cksel == sensor_settings[i].cksel)
	{
	  settings = &sensor_settings[i];
	}
      i++;
    }
  if (settings == NULL)
    {
      DBG (DBG_error,
	   "gl646_setup_registers: unable to find settings for sensor %d with '%d' ccd timing\n",
	   sensor->sensor, sensor->cksel);
      return SANE_STATUS_INVAL;
    }

  /* half_ccd if manual clock programming or dpi is half dpiset */
  half_ccd = sensor->half_ccd;

  /* now apply values from settings to registers */
  if (sensor->regs_0x10_0x15 != NULL)
    {
      for (i = 0; i < 6; i++)
	{
	  r = sanei_genesys_get_address (regs, 0x10 + i);
	  r->value = sensor->regs_0x10_0x15[i];
	}
    }
  else
    {
      for (i = 0; i < 6; i++)
	{
	  r = sanei_genesys_get_address (regs, 0x10 + i);
	  r->value = 0;
	}
    }

  for (i = 0; i < 4; i++)
    {
      r = sanei_genesys_get_address (regs, 0x08 + i);
      if (half_ccd == SANE_TRUE)
	r->value = settings->manual_0x08_0x0b[i];
      else
	r->value = settings->regs_0x08_0x0b[i];
    }

  for (i = 0; i < 8; i++)
    {
      r = sanei_genesys_get_address (regs, 0x16 + i);
      r->value = settings->regs_0x16_0x1d[i];
    }

  for (i = 0; i < 13; i++)
    {
      r = sanei_genesys_get_address (regs, 0x52 + i);
      r->value = settings->regs_0x52_0x5e[i];
    }
  if (half_ccd == SANE_TRUE)
    {
      for (i = 0; i < 7; i++)
	{
	  r = sanei_genesys_get_address (regs, 0x52 + i);
	  r->value = settings->manual_0x52_0x58[i];
	}
    }

  /* now generate slope tables : we are not using generate_slope_table3 yet */
  sanei_genesys_generate_slope_table (slope_table1, motor->steps1,
				      motor->steps1 + 1, motor->vend1,
				      motor->vstart1, motor->vend1,
				      motor->steps1, motor->g1, &used1,
				      &vfinal);
  sanei_genesys_generate_slope_table (slope_table2, motor->steps2,
				      motor->steps2 + 1, motor->vend2,
				      motor->vstart2, motor->vend2,
				      motor->steps2, motor->g2, &used2,
				      &vfinal);

  if (color == SANE_TRUE)
    channels = 3;
  else
    channels = 1;

  /* R01 */
  /* now setup other registers for final scan (ie with shading enabled) */
  /* watch dog + shading + scan enable */
  regs[reg_0x01].value |= REG01_DOGENB | REG01_DVDSET | REG01_SCAN;
  if (dev->model->is_cis == SANE_TRUE)
    regs[reg_0x01].value |= REG01_CISSET;
  else
    regs[reg_0x01].value &= ~REG01_CISSET;

  /* if device has no calibration, don't enable shading correction */
  if (dev->model->flags & GENESYS_FLAG_NO_CALIBRATION)
    {
      regs[reg_0x01].value &= ~REG01_DVDSET;
    }

  regs[reg_0x01].value &= ~REG01_FASTMOD;
  if (motor->fastmod)
    regs[reg_0x01].value |= REG01_FASTMOD;

  /* R02 */
  /* allow moving when buffer full by default */
  if (dev->model->is_sheetfed == SANE_FALSE)
    dev->reg[reg_0x02].value &= ~REG02_ACDCDIS;
  else
    dev->reg[reg_0x02].value |= REG02_ACDCDIS;

  /* setup motor power and direction */
  regs[reg_0x02].value |= REG02_MTRPWR;
  regs[reg_0x02].value &= ~REG02_MTRREV;

  /* fastfed enabled (2 motor slope tables) */
  if (motor->fastfed)
    regs[reg_0x02].value |= REG02_FASTFED;
  else
    regs[reg_0x02].value &= ~REG02_FASTFED;

  /* step type */
  regs[reg_0x02].value &= ~REG02_STEPSEL;
  switch (motor->steptype)
    {
    case FULL_STEP:
      break;
    case HALF_STEP:
      regs[reg_0x02].value |= 1;
      break;
    case QUATER_STEP:
      regs[reg_0x02].value |= 2;
      break;
    default:
      regs[reg_0x02].value |= 3;
      break;
    }

  /* if sheetfed, no AGOHOME */
  if (dev->model->is_sheetfed == SANE_TRUE)
    {
      regs[reg_0x02].value &= ~REG02_AGOHOME;
    }
  else
    {
      regs[reg_0x02].value |= REG02_AGOHOME;
    }

  /* R03 */
  regs[reg_0x03].value &= ~REG03_AVEENB;
  /* regs[reg_0x03].value |= REG03_AVEENB; */
  regs[reg_0x03].value &= ~REG03_LAMPDOG;

  /* select XPA */
  regs[reg_0x03].value &= ~REG03_XPASEL;
  if (scan_settings.scan_method == SCAN_METHOD_TRANSPARENCY)
    {
      regs[reg_0x03].value |= REG03_XPASEL;
    }

  /* R04 */
  /* monochrome / color scan */
  switch (depth)
    {
    case 1:
      regs[reg_0x04].value &= ~REG04_BITSET;
      regs[reg_0x04].value |= REG04_LINEART;
      break;
    case 8:
      regs[reg_0x04].value &= ~(REG04_LINEART | REG04_BITSET);
      break;
    case 16:
      regs[reg_0x04].value &= ~REG04_LINEART;
      regs[reg_0x04].value |= REG04_BITSET;
      break;
    }

  /* R05 */
  regs[reg_0x05].value &= ~REG05_DPIHW;
  switch (dev->sensor.optical_res)
    {
    case 600:
      regs[reg_0x05].value |= REG05_DPIHW_600;
      break;
    case 1200:
      regs[reg_0x05].value |= REG05_DPIHW_1200;
      break;
    case 2400:
      regs[reg_0x05].value |= REG05_DPIHW_2400;
      break;
    default:
      regs[reg_0x05].value |= REG05_DPIHW;
    }

  /* gamma enable for scans */
  if (dev->model->flags & GENESYS_FLAG_14BIT_GAMMA)
    regs[reg_0x05].value |= REG05_GMM14BIT;
  
  regs[reg_0x05].value &= ~REG05_GMMENB;

  /* true CIS gray if needed */
  if (dev->model->is_cis == SANE_TRUE && color == SANE_FALSE
      && dev->settings.true_gray)
    {
      regs[reg_0x05].value |= REG05_LEDADD;
    }
  else
    {
      regs[reg_0x05].value &= ~REG05_LEDADD;
    }

  /* cktoggle, ckdelay and cksel at once, cktdelay=2 => half_ccd for md5345 */
  regs[reg_0x18].value = sensor->r18;

  /* manual CCD/2 clock programming => half_ccd for hp2300 */
  regs[reg_0x1d].value = sensor->r1d;

  /* HP2400 1200dpi mode tuning */

  if (dev->model->ccd_type == CCD_HP2400)
    {
      /* reset count of dummy lines to zero */
      regs[reg_0x1e].value &= ~REG1E_LINESEL;
      if (scan_settings.xres >= 1200)
        {
          /* there must be one dummy line */
          regs[reg_0x1e].value |= 1 & REG1E_LINESEL;

          /* GPO12 need to be set to zero */
          regs[reg_0x66].value &= ~0x20;
        }
        else
        {
          /* set GPO12 back to one */
          regs[reg_0x66].value |= 0x20;
        }
    }

  /* motor steps used */
  regs[reg_0x21].value = motor->steps1;
  regs[reg_0x22].value = motor->fwdbwd;
  regs[reg_0x23].value = motor->fwdbwd;
  regs[reg_0x24].value = motor->steps1;

  /* scanned area height must be enlarged by max color shift needed */
  max_shift=sanei_genesys_compute_max_shift(dev,channels,scan_settings.yres,0);

  /* we adjust linecnt according to real motor dpi */
  linecnt = (linecnt * motor->ydpi) / scan_settings.yres + max_shift;

  /* at QUATER_STEP lines are 'staggered' and need correction */
  stagger = 0;
  if ((!half_ccd) && (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE))
    {
      /* for HP3670, stagger happens only at >=1200 dpi */
      if ((dev->model->motor_type != MOTOR_HP3670
	   && dev->model->motor_type != MOTOR_HP2400)
	  || scan_settings.yres >= dev->sensor.optical_res)
	{
	  stagger = (4 * scan_settings.yres) / dev->motor.base_ydpi;
	}
    }
  linecnt += stagger;

  DBG (DBG_info, "gl646_setup_registers :  max_shift=%d, stagger=%d lines\n",
       max_shift, stagger);

  /* CIS scanners read one line per color channel
   * since gray mode use 'add' we also read 3 channels even not in
   * color mode */
  if (dev->model->is_cis == SANE_TRUE)
    {
      gl646_set_triple_reg (regs, REG_LINCNT, linecnt * 3);
      linecnt *= channels;
    }
  else
    {
      gl646_set_triple_reg (regs, REG_LINCNT, linecnt);
    }

  /* scanner's x coordinates are expressed in physical DPI but they must be divided by cksel */
  sx = startx / sensor->cksel;
  ex = endx / sensor->cksel;
  if (half_ccd == SANE_TRUE)
    {
      sx /= 2;
      ex /= 2;
    }
  gl646_set_double_reg (regs, REG_STRPIXEL, sx);
  gl646_set_double_reg (regs, REG_ENDPIXEL, ex);
  DBG (DBG_info, "gl646_setup_registers: startx=%d, endx=%d, half_ccd=%d\n",
       sx, ex, half_ccd);

  /* words_per_line must be computed according to the scan's resolution */
  /* in fact, words_per_line _gives_ the actual scan resolution */
  words_per_line = (((endx - startx) * sensor->xdpi) / dev->sensor.optical_res);
  bpp=depth/8;
  if (depth == 1)
    {
      words_per_line = (words_per_line+7)/8 ;
      bpp=1;
    }
  else
    {
      words_per_line *= bpp;
    }
  dev->bpl = words_per_line;
  words_per_line *= channels;
  dev->wpl = words_per_line;

  DBG (DBG_info, "gl646_setup_registers: wpl=%d\n", words_per_line);
  gl646_set_triple_reg (regs, REG_MAXWD, words_per_line);

  gl646_set_double_reg (regs, REG_DPISET, sensor->dpiset);
  gl646_set_double_reg (regs, REG_LPERIOD, sensor->exposure);

  /* move distance must be adjusted to take into account the extra lines
   * read to reorder data */
  feedl = move;
  if (stagger + max_shift > 0 && feedl != 0)
    {
      if (feedl >
	  ((max_shift + stagger) * dev->motor.optical_ydpi) / motor->ydpi)
	feedl =
	  feedl -
	  ((max_shift + stagger) * dev->motor.optical_ydpi) / motor->ydpi;
    }

  /* we assume all scans are done with 2 tables */
  /*
     feedl = feed_steps - fast_slope_steps*2 -
     (slow_slope_steps >> scan_step_type); */
  /* but head has moved due to shading calibration => dev->scanhead_position_in_steps */
  if (feedl > 0)
    {
      /* take into account the distance moved during calibration */
      /* feedl -= dev->scanhead_position_in_steps; */
      DBG (DBG_info, "gl646_setup_registers: initial move=%d\n", feedl);
      DBG (DBG_info, "gl646_setup_registers: scanhead_position_in_steps=%d\n",
	   dev->scanhead_position_in_steps);

      /* TODO clean up this when I'll fully understand.
       * for now, special casing each motor */
      switch (dev->model->motor_type)
	{
	case MOTOR_5345:
	  switch (motor->ydpi)
	    {
	    case 200:
	      feedl -= 70;
	      break;
	    case 300:
	      feedl -= 70;
	      break;
	    case 400:
	      feedl += 130;
	      break;
	    case 600:
	      feedl += 160;
	      break;
	    case 1200:
	      feedl += 160;
	      break;
	    case 2400:
	      feedl += 180;
	      break;
	    default:
	      break;
	    }
	  break;
	case MOTOR_HP2300:
	  switch (motor->ydpi)
	    {
	    case 75:
	      feedl -= 180;
	      break;
	    case 150:
	      feedl += 0;
	      break;
	    case 300:
	      feedl += 30;
	      break;
	    case 600:
	      feedl += 35;
	      break;
	    case 1200:
	      feedl += 45;
	      break;
	    default:
	      break;
	    }
	  break;
	case MOTOR_HP2400:
	  switch (motor->ydpi)
	    {
	    case 150:
	      feedl += 150;
	      break;
	    case 300:
	      feedl += 220;
	      break;
	    case 600:
	      feedl += 260;
	      break;
	    case 1200:
	      feedl += 280; /* 300 */
	      break;
	    case 50:
	      feedl += 0;
	      break;
	    case 100:
	      feedl += 100;
	      break;
	    default:
	      break;
	    }
	  break;

	  /* theorical value */
	default:
	  if (motor->fastfed)
	    {
	      feedl =
		feedl - 2 * motor->steps2 -
		(motor->steps1 >> motor->steptype);
	    }
	  else
	    {
	      feedl = feedl - (motor->steps1 >> motor->steptype);
	    }
	  break;
	}
      /* security */
      if (feedl < 0)
	feedl = 0;
    }

  DBG (DBG_info, "gl646_setup_registers: final move=%d\n", feedl);
  gl646_set_triple_reg (regs, REG_FEEDL, feedl);

  regs[reg_0x65].value = motor->mtrpwm;

  sanei_genesys_calculate_zmode2 (regs[reg_0x02].value & REG02_FASTFED,
				  sensor->exposure,
				  slope_table1,
				  motor->steps1,
				  move, motor->fwdbwd, &z1, &z2);

  /* no z1/z2 for sheetfed scanners */
  if (dev->model->is_sheetfed == SANE_TRUE)
    {
      z1 = 0;
      z2 = 0;
    }
  gl646_set_double_reg (regs, REG_Z1MOD, z1);
  gl646_set_double_reg (regs, REG_Z2MOD, z2);
  regs[reg_0x6b].value = motor->steps2;
  regs[reg_0x6c].value =
    (regs[reg_0x6c].value & REG6C_TGTIME) | ((z1 >> 13) & 0x38) | ((z2 >> 16)
								   & 0x07);

  RIE (write_control (dev, xresolution));

  /* setup analog frontend */
  RIE (gl646_set_fe (dev, AFE_SET, xresolution));

  /* now we're done with registers setup values used by data transfer */
  /* we setup values needed for the data transfer */

  /* we must use a round number of words_per_line */
  requested_buffer_size = 8 * words_per_line;
  read_buffer_size =
    2 * requested_buffer_size +
    ((max_shift + stagger) * scan_settings.pixels * channels * depth) / 8;

  RIE (sanei_genesys_buffer_free (&(dev->read_buffer)));
  RIE (sanei_genesys_buffer_alloc (&(dev->read_buffer), read_buffer_size));

  RIE (sanei_genesys_buffer_free (&(dev->lines_buffer)));
  RIE (sanei_genesys_buffer_alloc (&(dev->lines_buffer), read_buffer_size));

  RIE (sanei_genesys_buffer_free (&(dev->shrink_buffer)));
  RIE (sanei_genesys_buffer_alloc (&(dev->shrink_buffer),
				   requested_buffer_size));

  RIE (sanei_genesys_buffer_free (&(dev->out_buffer)));
  RIE (sanei_genesys_buffer_alloc (&(dev->out_buffer),
				   8 * scan_settings.pixels * channels * bpp));

  /* scan bytes to read */
  dev->read_bytes_left = words_per_line * linecnt;

  DBG (DBG_info,
       "gl646_setup_registers: physical bytes to read = %lu\n",
       (u_long) dev->read_bytes_left);
  dev->read_active = SANE_TRUE;

  dev->current_setup.pixels =
    ((endx - startx) * sensor->xdpi) / dev->sensor.optical_res;
  dev->current_setup.lines = linecnt;
  dev->current_setup.depth = depth;
  dev->current_setup.channels = channels;
  dev->current_setup.exposure_time = sensor->exposure;
  dev->current_setup.xres = sensor->xdpi;
  dev->current_setup.yres = motor->ydpi;
  dev->current_setup.half_ccd = half_ccd;
  dev->current_setup.stagger = stagger;
  dev->current_setup.max_shift = max_shift + stagger;

  /* total_bytes_to_read is the number of byte to send to frontend
   * total_bytes_read is the number of bytes sent to frontend
   * read_bytes_left is the number of bytes to read from the scanner
   */
  dev->total_bytes_read = 0;
  if (depth == 1)
    dev->total_bytes_to_read =
      ((scan_settings.pixels * scan_settings.lines) / 8 +
       (((scan_settings.pixels * scan_settings.lines) % 8) ? 1 : 0)) *
      channels;
  else
    dev->total_bytes_to_read =
      scan_settings.pixels * scan_settings.lines * channels * bpp;

  DBG (DBG_proc, "gl646_setup_registers: end\n");
  return SANE_STATUS_GOOD;
}


/** copy sensor specific settings */
/* *dev  : device infos
   *regs : regiters to be set
   extended : do extended set up
   half_ccd: set up for half ccd resolution
   all registers 08-0B, 10-1D, 52-5E are set up. They shouldn't
   appear anywhere else but in register init
*/
static void
gl646_setup_sensor (Genesys_Device * dev, Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r;
  int i;

  DBG (DBG_proc, "gl646_setup_sensor: start\n");
  for (i = 0; i < 4; i++)
    {
      r = sanei_genesys_get_address (regs, 0x08 + i);
      r->value = dev->sensor.regs_0x08_0x0b[i];
    }

  for (i = 0; i < 14; i++)
    {
      r = sanei_genesys_get_address (regs, 0x10 + i);
      r->value = dev->sensor.regs_0x10_0x1d[i];
    }

  for (i = 0; i < 13; i++)
    {
      r = sanei_genesys_get_address (regs, 0x52 + i);
      r->value = dev->sensor.regs_0x52_0x5e[i];
    }
  DBG (DBG_proc, "gl646_setup_sensor: end\n");

}

/** Test if the ASIC works
 */
static SANE_Status
gl646_asic_test (Genesys_Device * dev)
{
  SANE_Status status;
  uint8_t val;
  uint8_t *data;
  uint8_t *verify_data;
  size_t size, verify_size;
  unsigned int i;

  DBG (DBG_proc, "gl646_asic_test: start\n");

  /* set and read exposure time, compare if it's the same */
  status = sanei_genesys_write_register (dev, 0x38, 0xde);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to write register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_write_register (dev, 0x39, 0xad);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to write register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = sanei_genesys_read_register (dev, 0x4e, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to read register: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (val != 0xde)		/* value of register 0x38 */
    {
      DBG (DBG_error, "gl646_asic_test: register contains invalid value\n");
      return SANE_STATUS_IO_ERROR;
    }

  status = sanei_genesys_read_register (dev, 0x4f, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to read register: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (val != 0xad)		/* value of register 0x39 */
    {
      DBG (DBG_error, "gl646_asic_test: register contains invalid value\n");
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
      DBG (DBG_error, "gl646_asic_test: could not allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  verify_data = (uint8_t *) malloc (verify_size);
  if (!verify_data)
    {
      free (data);
      DBG (DBG_error, "gl646_asic_test: could not allocate memory\n");
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
      DBG (DBG_error, "gl646_asic_test: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

  status = gl646_bulk_write_data (dev, 0x3c, data, size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to bulk write data: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

  status = sanei_genesys_set_buffer_address (dev, 0x0000);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

  status =
    gl646_bulk_read_data (dev, 0x45, (uint8_t *) verify_data, verify_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_asic_test: failed to bulk read data: %s\n",
	   sane_strstatus (status));
      free (data);
      free (verify_data);
      return status;
    }

  /* i + 2 is needed as the changed address goes into effect only after one
     data word is sent. */
  for (i = 0; i < size; i++)
    {
      if (verify_data[i + 2] != data[i])
	{
	  DBG (DBG_error, "gl646_asic_test: data verification error\n");
	  free (data);
	  free (verify_data);
	  return SANE_STATUS_IO_ERROR;
	}
    }

  free (data);
  free (verify_data);

  DBG (DBG_info, "gl646_asic_test: end\n");

  return SANE_STATUS_GOOD;
}

/* returns the max register bulk size */
static int
gl646_bulk_full_size (void)
{
  return GENESYS_GL646_MAX_REGS;
}

/**
 * Set all registers to default values after init
 * @param dev scannerr's device to set
 */
static void
gl646_init_regs (Genesys_Device * dev)
{
  int nr, addr;

  DBG (DBG_proc, "gl646_init_regs\n");

  nr = 0;
  memset (dev->reg, 0, GENESYS_MAX_REGS * sizeof (Genesys_Register_Set));

  for (addr = 1; addr <= 0x0b; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x10; addr <= 0x29; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x2c; addr <= 0x39; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x3d; addr <= 0x3f; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x52; addr <= 0x5e; addr++)
    dev->reg[nr++].address = addr;
  for (addr = 0x60; addr <= 0x6d; addr++)
    dev->reg[nr++].address = addr;

  dev->reg[reg_0x01].value = 0x20 /*0x22 */ ;	/* enable shading, CCD, color, 1M */
  dev->reg[reg_0x02].value = 0x30 /*0x38 */ ;	/* auto home, one-table-move, full step */
  if (dev->model->motor_type == MOTOR_5345)
    dev->reg[reg_0x02].value |= 0x01;	/* half-step */
  switch (dev->model->motor_type)
    {
    case MOTOR_5345:
      dev->reg[reg_0x02].value |= 0x01;	/* half-step */
      break;
    case MOTOR_XP200:
      /* for this sheetfed scanner, no AGOHOME, nor backtracking */
      dev->reg[reg_0x02].value = 0x50;
      break;
    default:
      break;
    }
  dev->reg[reg_0x03].value = 0x1f /*0x17 */ ;	/* lamp on */
  dev->reg[reg_0x04].value = 0x13 /*0x03 */ ;	/* 8 bits data, 16 bits A/D, color, Wolfson fe *//* todo: according to spec, 0x0 is reserved? */
  switch (dev->model->dac_type)
    {
    case DAC_AD_XP200:
      dev->reg[reg_0x04].value = 0x12;
      break;
    default:
      /* Wolfson frontend */
      dev->reg[reg_0x04].value = 0x13;
      break;
    }

  dev->reg[reg_0x05].value = 0x00;	/* 12 bits gamma, disable gamma, 24 clocks/pixel */
  switch (dev->sensor.optical_res)
    {
    case 600:
      dev->reg[reg_0x05].value |= REG05_DPIHW_600;
      break;
    case 1200:
      dev->reg[reg_0x05].value |= REG05_DPIHW_1200;
      break;
    case 2400:
      dev->reg[reg_0x05].value |= REG05_DPIHW_2400;
      break;
    default:
      dev->reg[reg_0x05].value |= REG05_DPIHW;
      break;
    }
  if (dev->model->flags & GENESYS_FLAG_14BIT_GAMMA)
    dev->reg[reg_0x05].value |= REG05_GMM14BIT;
  if (dev->model->dac_type == DAC_AD_XP200)
    dev->reg[reg_0x05].value |= 0x01;	/* 12 clocks/pixel */

  if (dev->model->ccd_type == CCD_HP2300)
    dev->reg[reg_0x06].value = 0x00;	/* PWRBIT off, shading gain=4, normal AFE image capture */
  else
    dev->reg[reg_0x06].value = 0x18;	/* PWRBIT on, shading gain=8, normal AFE image capture */


  gl646_setup_sensor (dev, dev->reg);

  dev->reg[reg_0x1e].value = 0xf0;	/* watch-dog time */

  switch (dev->model->ccd_type)
    {
    case CCD_HP2300:
      dev->reg[reg_0x1e].value = 0xf0;
      dev->reg[reg_0x1f].value = 0x10;
      dev->reg[reg_0x20].value = 0x20;
      break;
    case CCD_HP2400:
      dev->reg[reg_0x1e].value = 0x80;
      dev->reg[reg_0x1f].value = 0x10;
      dev->reg[reg_0x20].value = 0x20;
      break;
    case CCD_HP3670:
      dev->reg[reg_0x19].value = 0x2a;
      dev->reg[reg_0x1e].value = 0x80;
      dev->reg[reg_0x1f].value = 0x10;
      dev->reg[reg_0x20].value = 0x20;
      break;
    case CIS_XP200:
      dev->reg[reg_0x1e].value = 0x10;
      dev->reg[reg_0x1f].value = 0x01;
      dev->reg[reg_0x20].value = 0x50;
      break;
    default:
      dev->reg[reg_0x1f].value = 0x01;
      dev->reg[reg_0x20].value = 0x50;
      break;
    }

  dev->reg[reg_0x21].value = 0x08 /*0x20 */ ;	/* table one steps number for forward slope curve of the acc/dec */
  dev->reg[reg_0x22].value = 0x10 /*0x08 */ ;	/* steps number of the forward steps for start/stop */
  dev->reg[reg_0x23].value = 0x10 /*0x08 */ ;	/* steps number of the backward steps for start/stop */
  dev->reg[reg_0x24].value = 0x08 /*0x20 */ ;	/* table one steps number backward slope curve of the acc/dec */
  dev->reg[reg_0x25].value = 0x00;	/* scan line numbers (7000) */
  dev->reg[reg_0x26].value = 0x00 /*0x1b */ ;
  dev->reg[reg_0x27].value = 0xd4 /*0x58 */ ;
  dev->reg[reg_0x28].value = 0x01;	/* PWM duty for lamp control */
  dev->reg[reg_0x29].value = 0xff;

  dev->reg[reg_0x2c].value = 0x02;	/* set resolution (600 DPI) */
  dev->reg[reg_0x2d].value = 0x58;
  dev->reg[reg_0x2e].value = 0x78;	/* set black&white threshold high level */
  dev->reg[reg_0x2f].value = 0x7f;	/* set black&white threshold low level */

  dev->reg[reg_0x30].value = 0x00;	/* begin pixel position (16) */
  dev->reg[reg_0x31].value = dev->sensor.dummy_pixel /*0x10 */ ;	/* TGW + 2*TG_SHLD + x  */
  dev->reg[reg_0x32].value = 0x2a /*0x15 */ ;	/* end pixel position (5390) */
  dev->reg[reg_0x33].value = 0xf8 /*0x0e */ ;	/* TGW + 2*TG_SHLD + y   */
  dev->reg[reg_0x34].value = dev->sensor.dummy_pixel;
  dev->reg[reg_0x35].value = 0x01 /*0x00 */ ;	/* set maximum word size per line, for buffer full control (10800) */
  dev->reg[reg_0x36].value = 0x00 /*0x2a */ ;
  dev->reg[reg_0x37].value = 0x00 /*0x30 */ ;
  dev->reg[reg_0x38].value = HIBYTE (dev->settings.exposure_time) /*0x2a */ ;	/* line period (exposure time = 11000 pixels) */
  dev->reg[reg_0x39].value = LOBYTE (dev->settings.exposure_time) /*0xf8 */ ;
  dev->reg[reg_0x3d].value = 0x00;	/* set feed steps number of motor move */
  dev->reg[reg_0x3e].value = 0x00;
  dev->reg[reg_0x3f].value = 0x01 /*0x00 */ ;

  dev->reg[reg_0x60].value = 0x00;	/* Z1MOD, 60h:61h:(6D b5:b3), remainder for start/stop */
  dev->reg[reg_0x61].value = 0x00;	/* (21h+22h)/LPeriod */
  dev->reg[reg_0x62].value = 0x00;	/* Z2MODE, 62h:63h:(6D b2:b0), remainder for start scan */
  dev->reg[reg_0x63].value = 0x00;	/* (3Dh+3Eh+3Fh)/LPeriod for one-table mode,(21h+1Fh)/LPeriod */
  dev->reg[reg_0x64].value = 0x00;	/* motor PWM frequency */
  dev->reg[reg_0x65].value = 0x00;	/* PWM duty cycle for table one motor phase (63 = max) */
  if (dev->model->motor_type == MOTOR_5345)
    dev->reg[reg_0x65].value = 0x02;	/* PWM duty cycle for table one motor phase (63 = max) */
  dev->reg[reg_0x66].value = dev->gpo.value[0];
  dev->reg[reg_0x67].value = dev->gpo.value[1];
  dev->reg[reg_0x68].value = dev->gpo.enable[0];
  dev->reg[reg_0x69].value = dev->gpo.enable[1];

  switch (dev->model->motor_type)
    {
    case MOTOR_HP2300:
    case MOTOR_HP2400:
      dev->reg[reg_0x6a].value = 0x7f;	/* table two steps number for acc/dec */
      dev->reg[reg_0x6b].value = 0x78;	/* table two steps number for acc/dec */
      dev->reg[reg_0x6d].value = 0x7f;
      break;
    case MOTOR_5345:
      dev->reg[reg_0x6a].value = 0x42;	/* table two fast moving step type, PWM duty for table two */
      dev->reg[reg_0x6b].value = 0xff;	/* table two steps number for acc/dec */
      dev->reg[reg_0x6d].value = 0x41;	/* select deceleration steps whenever go home (0), accel/decel stop time (31 * LPeriod) */
      break;
    case MOTOR_XP200:
      dev->reg[reg_0x6a].value = 0x7f;	/* table two fast moving step type, PWM duty for table two */
      dev->reg[reg_0x6b].value = 0x08;	/* table two steps number for acc/dec */
      dev->reg[reg_0x6d].value = 0x01;	/* select deceleration steps whenever go home (0), accel/decel stop time (31 * LPeriod) */
      break;
    case MOTOR_HP3670:
      dev->reg[reg_0x6a].value = 0x41;	/* table two steps number for acc/dec */
      dev->reg[reg_0x6b].value = 0xc8;	/* table two steps number for acc/dec */
      dev->reg[reg_0x6d].value = 0x7f;
      break;
    default:
      dev->reg[reg_0x6a].value = 0x40;	/* table two fast moving step type, PWM duty for table two */
      dev->reg[reg_0x6b].value = 0xff;	/* table two steps number for acc/dec */
      dev->reg[reg_0x6d].value = 0x01;	/* select deceleration steps whenever go home (0), accel/decel stop time (31 * LPeriod) */
      break;
    }
  dev->reg[reg_0x6c].value = 0x00;	/* peroid times for LPeriod, expR,expG,expB, Z1MODE, Z2MODE (one period time) */
}


/* Send slope table for motor movement
   slope_table in machine byte order
*/
static SANE_Status
gl646_send_slope_table (Genesys_Device * dev, int table_nr,
			uint16_t * slope_table, int steps)
{
  int dpihw;
  int start_address;
  SANE_Status status;
  uint8_t *table;
#ifdef WORDS_BIGENDIAN
  int i;
#endif

  DBG (DBG_proc,
       "gl646_send_slope_table (table_nr = %d, steps = %d)=%d .. %d\n",
       table_nr, steps, slope_table[0], slope_table[steps - 1]);

  dpihw = dev->reg[reg_0x05].value >> 6;

  if (dpihw == 0)		/* 600 dpi */
    start_address = 0x08000;
  else if (dpihw == 1)		/* 1200 dpi */
    start_address = 0x10000;
  else if (dpihw == 2)		/* 2400 dpi */
    start_address = 0x1f800;
  else				/* reserved */
    return SANE_STATUS_INVAL;

#ifdef WORDS_BIGENDIAN
  table = (uint8_t *) malloc (steps * 2);
  for (i = 0; i < steps; i++)
    {
      table[i * 2] = slope_table[i] & 0xff;
      table[i * 2 + 1] = slope_table[i] >> 8;
    }
#else
  table = (uint8_t *) slope_table;
#endif

  status =
    sanei_genesys_set_buffer_address (dev, start_address + table_nr * 0x100);
  if (status != SANE_STATUS_GOOD)
    {
#ifdef WORDS_BIGENDIAN
      free (table);
#endif
      DBG (DBG_error,
	   "gl646_send_slope_table: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl646_bulk_write_data (dev, 0x3c, (uint8_t *) table, steps * 2);
  if (status != SANE_STATUS_GOOD)
    {
#ifdef WORDS_BIGENDIAN
      free (table);
#endif
      DBG (DBG_error,
	   "gl646_send_slope_table: failed to send slope table: %s\n",
	   sane_strstatus (status));
      return status;
    }

#ifdef WORDS_BIGENDIAN
  free (table);
#endif
  DBG (DBG_proc, "gl646_send_slope_table: end\n");
  return status;
}

/* Set values of Analog Device type frontend */
static SANE_Status
gl646_set_ad_fe (Genesys_Device * dev, uint8_t set)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int i;
  uint16_t val;

  DBG (DBG_proc, "gl646_set_ad_fe(): start\n");
  if (set == AFE_INIT)
    {
      DBG (DBG_proc, "gl646_set_ad_fe(): setting DAC %u\n",
	   dev->model->dac_type);

      /* sets to default values */
      sanei_genesys_init_fe (dev);

      /* write them to analog frontend */
      val = dev->frontend.reg[0];
      status = sanei_genesys_fe_write_data (dev, 0x00, val);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_set_ad_fe: failed to write reg0: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      val = dev->frontend.reg[1];
      status = sanei_genesys_fe_write_data (dev, 0x01, val);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_set_ad_fe: failed to write reg1: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }
  if (set == AFE_SET)
    {
      for (i = 0; i < 3; i++)
	{
	  val = dev->frontend.gain[i];
	  status = sanei_genesys_fe_write_data (dev, 0x02 + i, val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl646_set_ad_fe: failed to write gain %d: %s\n", i,
		   sane_strstatus (status));
	      return status;
	    }
	}
      for (i = 0; i < 3; i++)
	{
	  val = dev->frontend.offset[i];
	  status = sanei_genesys_fe_write_data (dev, 0x05 + i, val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl646_set_ad_fe: failed to write offset %d: %s\n", i,
		   sane_strstatus (status));
	      return status;
	    }
	}
    }
  /*
     if (set == AFE_POWER_SAVE)
     {
     status =
     sanei_genesys_fe_write_data (dev, 0x00, dev->frontend.reg[0] | 0x04);
     if (status != SANE_STATUS_GOOD)
     {
     DBG (DBG_error, "gl646_set_ad_fe: failed to write reg0: %s\n",
     sane_strstatus (status));
     return status;
     }
     } */
  DBG (DBG_proc, "gl646_set_ad_fe(): end\n");

  return status;
}

/** set up analog frontend
 * set up analog frontend
 * @param dev device to set up
 * @param set action from AFE_SET, AFE_INIT and AFE_POWERSAVE
 * @param dpi resolution of the scan since it affects settings
 * @return SANE_STATUS_GOOD if evrithing OK
 */
static SANE_Status
gl646_wm_hp3670 (Genesys_Device * dev, uint8_t set, int dpi)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int i;

  DBGSTART;
  switch (set)
    {
    case AFE_INIT:
      status = sanei_genesys_fe_write_data (dev, 0x04, 0x80);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_wm_hp3670: reset failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      usleep (200000UL);
      RIE (sanei_genesys_write_register (dev, 0x50, 0x00));
      sanei_genesys_init_fe (dev);
      status = sanei_genesys_fe_write_data (dev, 0x01, dev->frontend.reg[1]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_wm_hp3670: writing reg1 failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      status = sanei_genesys_fe_write_data (dev, 0x02, dev->frontend.reg[2]);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_wm_hp3670: writing reg2 failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      status = gl646_gpio_output_enable (dev->dn, 0x07);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_wm_hp3670: failed to enable GPIO: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      break;
    case AFE_POWER_SAVE:
      status = sanei_genesys_fe_write_data (dev, 0x01, 0x06);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_wm_hp3670: writing reg1 failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      status = sanei_genesys_fe_write_data (dev, 0x06, 0x0f);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_wm_hp3670: writing reg6 failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      return status;
      break;
    default:			/* AFE_SET */
      /* mode setup */
      i = dev->frontend.reg[3];
      if (dpi > dev->sensor.optical_res / 2)
	{
	  /* fe_reg_0x03 must be 0x12 for 1200 dpi in DAC_WOLFSON_HP3670.
	   * DAC_WOLFSON_HP2400 in 1200 dpi mode works well with
	   * fe_reg_0x03 set to 0x32 or 0x12 but not to 0x02 */
	  i = 0x12;
	}
      status = sanei_genesys_fe_write_data (dev, 0x03, i);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_wm_hp3670: writing reg3 failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      /* offset and sign (or msb/lsb ?) */
      for (i = 0; i < 3; i++)
	{
	  status =
	    sanei_genesys_fe_write_data (dev, 0x20 + i,
					 dev->frontend.offset[i]);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl646_wm_hp3670: writing offset%d failed: %s\n", i,
		   sane_strstatus (status));
	      return status;
	    }
	  status = sanei_genesys_fe_write_data (dev, 0x24 + i, dev->frontend.sign[i]);	/* MSB/LSB ? */
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error, "gl646_wm_hp3670: writing sign%d failed: %s\n",
		   i, sane_strstatus (status));
	      return status;
	    }
	}

      /* gain */
      for (i = 0; i < 3; i++)
	{
	  status =
	    sanei_genesys_fe_write_data (dev, 0x28 + i,
					 dev->frontend.gain[i]);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error, "gl646_wm_hp3670: writing gain%d failed: %s\n",
		   i, sane_strstatus (status));
	      return status;
	    }
	}
    }

  DBGCOMPLETED;
  return status;
}

/** Set values of analog frontend
 * @param dev device to set
 * @param set action to execute
 * @param dpi dpi to setup the AFE
 * @return error or SANE_STATUS_GOOD */
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
gl646_set_fe (Genesys_Device * dev, uint8_t set, int dpi)
{
  SANE_Status status;
  int i;
  uint8_t val;

  DBG (DBG_proc, "gl646_set_fe (%s,%d)\n",
       set == AFE_INIT ? "init" : set == AFE_SET ? "set" : set ==
       AFE_POWER_SAVE ? "powersave" : "huh?", dpi);

  /* Analog Device type frontend */
  if ((dev->reg[reg_0x04].value & REG04_FESET) == 0x02)
    return gl646_set_ad_fe (dev, set);

  /* Wolfson type frontend */
  if ((dev->reg[reg_0x04].value & REG04_FESET) != 0x03)
    {
      DBG (DBG_proc, "gl646_set_fe(): unspported frontend type %d\n",
	   dev->reg[reg_0x04].value & REG04_FESET);
      return SANE_STATUS_UNSUPPORTED;
    }

  /* per frontend function to keep code clean */
  switch (dev->model->dac_type)
    {
    case DAC_WOLFSON_HP3670:
    case DAC_WOLFSON_HP2400:
      return gl646_wm_hp3670 (dev, set, dpi);
      break;
    default:
      DBG (DBG_proc, "gl646_set_fe(): using old method\n");
      break;
    }

  /* initialize analog frontend */
  if (set == AFE_INIT)
    {
      DBG (DBG_proc, "gl646_set_fe(): setting DAC %u\n",
	   dev->model->dac_type);
      sanei_genesys_init_fe (dev);

      /* reset only done on init */
      status = sanei_genesys_fe_write_data (dev, 0x04, 0x80);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_set_fe: init fe failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      /* enable GPIO for some models */
      if (dev->model->ccd_type == CCD_HP2300)
	{
	  val = 0x07;
	  status = gl646_gpio_output_enable (dev->dn, val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl646_set_fe: failed to enable GPIO: %s\n",
		   sane_strstatus (status));
	      return status;
	    }
	}
      return status;
    }

  /* set fontend to power saving mode */
  if (set == AFE_POWER_SAVE)
    {
      status = sanei_genesys_fe_write_data (dev, 0x01, 0x02);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_set_fe: writing data failed: %s\n",
	       sane_strstatus (status));
	}
      return status;
    }

  /* here starts AFE_SET */
  /* TODO :  base this test on cfg reg3 or a CCD family flag to be created */
  /* if (dev->model->ccd_type != CCD_HP2300
     && dev->model->ccd_type != CCD_HP3670
     && dev->model->ccd_type != CCD_HP2400) */
  {
    status = sanei_genesys_fe_write_data (dev, 0x00, dev->frontend.reg[0]);
    if (status != SANE_STATUS_GOOD)
      {
	DBG (DBG_error, "gl646_set_fe: writing reg0 failed: %s\n",
	     sane_strstatus (status));
	return status;
      }
    status = sanei_genesys_fe_write_data (dev, 0x02, dev->frontend.reg[2]);
    if (status != SANE_STATUS_GOOD)
      {
	DBG (DBG_error, "gl646_set_fe: writing reg2 failed: %s\n",
	     sane_strstatus (status));
	return status;
      }
  }

  /* start with reg3 */
  status = sanei_genesys_fe_write_data (dev, 0x03, dev->frontend.reg[3]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_set_fe: writing reg3 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }

  switch (dev->model->ccd_type)
    {
    default:
      for (i = 0; i < 3; i++)
	{
	  status =
	    sanei_genesys_fe_write_data (dev, 0x24 + i,
					 dev->frontend.sign[i]);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error, "gl646_set_fe: writing sign[%d] failed: %s\n",
		   i, sane_strstatus (status));
	      return status;
	    }

	  status =
	    sanei_genesys_fe_write_data (dev, 0x28 + i,
					 dev->frontend.gain[i]);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error, "gl646_set_fe: writing gain[%d] failed: %s\n",
		   i, sane_strstatus (status));
	      return status;
	    }

	  status =
	    sanei_genesys_fe_write_data (dev, 0x20 + i,
					 dev->frontend.offset[i]);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl646_set_fe: writing offset[%d] failed: %s\n", i,
		   sane_strstatus (status));
	      return status;
	    }
	}
      break;
      /* just can't have it to work ....
         case CCD_HP2300:
         case CCD_HP2400:
         case CCD_HP3670:

         status =
         sanei_genesys_fe_write_data (dev, 0x23, dev->frontend.offset[1]);
         if (status != SANE_STATUS_GOOD)
         {
         DBG (DBG_error,
         "gl646_set_fe: writing offset[1] failed: %s\n",
         sane_strstatus (status));
         return status;
         }
         status = sanei_genesys_fe_write_data (dev, 0x28, dev->frontend.gain[1]);
         if (status != SANE_STATUS_GOOD)
         {
         DBG (DBG_error, "gl646_set_fe: writing gain[1] failed: %s\n",
         sane_strstatus (status));
         return status;
         }
         break; */
    }

  /* end with reg1 */
  status = sanei_genesys_fe_write_data (dev, 0x01, dev->frontend.reg[1]);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_set_fe: writing reg1 failed: %s\n",
	   sane_strstatus (status));
      return status;
    }


  DBG (DBG_proc, "gl646_set_fe: end\n");

  return SANE_STATUS_GOOD;
}

/** Set values of analog frontend
 * this this the public interface, the gl646 as to use one more
 * parameter to work effectively, hence the redirection
 * @param dev device to set
 * @param set action to execute
 * @return error or SANE_STATUS_GOOD */
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
gl646_public_set_fe (Genesys_Device * dev, uint8_t set)
{
  return gl646_set_fe (dev, set, dev->settings.yres);
}

static void
gl646_set_motor_power (Genesys_Register_Set * regs, SANE_Bool set)
{
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
gl646_set_lamp_power (Genesys_Device * dev,
		      Genesys_Register_Set * regs, SANE_Bool set)
{
  if (dev)
    {
      if (set)
	{
	  sanei_genesys_set_reg_from_set (regs, 0x03,
					  sanei_genesys_read_reg_from_set
					  (regs, 0x03) | REG03_LAMPPWR);
	}
      else
	{
	  sanei_genesys_set_reg_from_set (regs, 0x03,
					  sanei_genesys_read_reg_from_set
					  (regs, 0x03) & ~REG03_LAMPPWR);
	}
    }
}

/**
 * enters or leaves power saving mode
 * limited to AFE for now.
 * @param dev scanner's device
 * @param enable SANE_TRUE to enable power saving, SANE_FALSE to leave it
 * @return allways SANE_STATUS_GOOD
 */
GENESYS_STATIC
SANE_Status
gl646_save_power (Genesys_Device * dev, SANE_Bool enable)
{

  DBGSTART;
  DBG (DBG_info, "gl646_save_power: enable = %d\n", enable);

  if (enable)
    {
      /* gl646_set_fe (dev, AFE_POWER_SAVE); */
    }
  else
    {
      gl646_set_fe (dev, AFE_INIT, 0);
    }

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl646_set_powersaving (Genesys_Device * dev, int delay /* in minutes */ )
{
  SANE_Status status = SANE_STATUS_GOOD;
  Genesys_Register_Set local_reg[6];
  int rate, exposure_time, tgtime, time;

  DBG (DBG_proc, "gl646_set_powersaving (delay = %d)\n", delay);

  local_reg[0].address = 0x01;
  local_reg[0].value = sanei_genesys_read_reg_from_set (dev->reg, 0x01);	/* disable fastmode */

  local_reg[1].address = 0x03;
  local_reg[1].value = sanei_genesys_read_reg_from_set (dev->reg, 0x03);	/* Lamp power control */

  local_reg[2].address = 0x05;
  local_reg[2].value = sanei_genesys_read_reg_from_set (dev->reg, 0x05) & ~REG05_BASESEL;	/* 24 clocks/pixel */

  local_reg[3].address = 0x38;	/* line period low */
  local_reg[3].value = 0x00;

  local_reg[4].address = 0x39;	/* line period high */
  local_reg[4].value = 0x00;

  local_reg[5].address = 0x6c;	/* period times for LPeriod, expR,expG,expB, Z1MODE, Z2MODE */
  local_reg[5].value = 0x00;

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

  local_reg[5].value |= tgtime << 6;
  exposure_time /= rate;

  if (exposure_time > 65535)
    exposure_time = 65535;

  local_reg[3].value = exposure_time / 256;	/* highbyte */
  local_reg[4].value = exposure_time & 255;	/* lowbyte */

  status = gl646_bulk_write_register (dev, local_reg,
				      sizeof (local_reg) /
				      sizeof (local_reg[0]));
  if (status != SANE_STATUS_GOOD)
    DBG (DBG_error,
	 "gl646_set_powersaving: Failed to bulk write registers: %s\n",
	 sane_strstatus (status));

  DBG (DBG_proc, "gl646_set_powersaving: end\n");
  return status;
}


/**
 * loads document into scanner
 * currently only used by XP200
 * bit2 (0x04) of gpio is paper event (document in/out) on XP200
 * HOMESNR is set if no document in front of sensor, the sequence of events is
 * paper event -> document is in the sheet feeder
 * HOMESNR becomes 0 -> document reach sensor
 * HOMESNR becomes 1 ->document left sensor
 * paper event -> document is out
 */
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
gl646_load_document (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  Genesys_Register_Set regs[11];
  unsigned int used, vfinal, count;
  uint16_t slope_table[255];
  uint8_t val;

  DBG (DBG_proc, "gl646_load_document: start\n");

  /* no need to load document is flatbed scanner */
  if (dev->model->is_sheetfed == SANE_FALSE)
    {
      DBG (DBG_proc, "gl646_load_document: nothing to load\n");
      DBG (DBG_proc, "gl646_load_document: end\n");
      return SANE_STATUS_GOOD;
    }

  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_load_document: failed to read status: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* HOMSNR is set if a document is inserted */
  if ((val & REG41_HOMESNR))
    {
      /* if no document, waits for a paper event to start loading */
      /* with a 60 seconde minutes timeout                        */
      count = 0;
      do
	{
	  status = gl646_gpio_read (dev->dn, &val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl646_load_document: failed to read paper sensor %s\n",
		   sane_strstatus (status));
	      return status;
	    }
	  DBG (DBG_info, "gl646_load_document: GPIO=0x%02x\n", val);
	  if ((val & 0x04) != 0x04)
	    {
	      DBG (DBG_warn, "gl646_load_document: no paper detected\n");
	    }
	  usleep (200000UL);	/* sleep 200 ms */
	  count++;
	}
      while (((val & 0x04) != 0x04) && (count < 300));	/* 1 min time out */
      if (count == 300)
	{
	  DBG (DBG_error,
	       "gl646_load_document: timeout waiting for document\n");
	  return SANE_STATUS_NO_DOCS;
	}
    }

  /* set up to fast move before scan then move until document is detected */
  regs[0].address = 0x01;
  regs[0].value = 0x90;

  /* AGOME, 2 slopes motor moving */
  regs[1].address = 0x02;
  regs[1].value = 0x79;

  /* motor feeding steps to 0 */
  regs[2].address = 0x3d;
  regs[2].value = 0;
  regs[3].address = 0x3e;
  regs[3].value = 0;
  regs[4].address = 0x3f;
  regs[4].value = 0;

  /* 50 fast moving steps */
  regs[5].address = 0x6b;
  regs[5].value = 50;

  /* set GPO */
  regs[6].address = 0x66;
  regs[6].value = 0x30;

  /* stesp NO */
  regs[7].address = 0x21;
  regs[7].value = 4;
  regs[8].address = 0x22;
  regs[8].value = 1;
  regs[9].address = 0x23;
  regs[9].value = 1;
  regs[10].address = 0x24;
  regs[10].value = 4;

  /* generate slope table 2 */
  sanei_genesys_generate_slope_table (slope_table,
				      50,
				      51,
				      2400,
				      6000, 2400, 50, 0.25, &used, &vfinal);
/* document loading:
 * send regs
 * start motor
 * wait e1 status to become e0
 */
  status = gl646_send_slope_table (dev, 1, slope_table, 50);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_load_document: failed to send slope table 1: %s\n",
	   sane_strstatus (status));
      return status;
    }
  status =
    gl646_bulk_write_register (dev, regs, sizeof (regs) / sizeof (regs[0]));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_load_document: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl646_start_motor (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_load_document: failed to start motor: %s\n",
	   sane_strstatus (status));
      return SANE_STATUS_IO_ERROR;
    }

  count = 0;
  do
    {
      status = sanei_genesys_get_status (dev, &val);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_load_document: failed to read status: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      usleep (200000UL);	/* sleep 200 ms */
      count++;
    }
  while ((val & REG41_MOTMFLG) && (count < 300));
  if (count == 300)
    {
      DBG (DBG_error, "gl646_load_document: can't load document\n");
      return SANE_STATUS_JAMMED;
    }

  /* when loading OK, document is here */
  dev->document = SANE_TRUE;

  /* set up to idle */
  regs[1].value = 0x71;
  regs[4].value = 1;
  regs[5].value = 8;
  status =
    gl646_bulk_write_register (dev, regs, sizeof (regs) / sizeof (regs[0]));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_load_document: failed to bulk write idle registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl646_load_document: end\n");

  return status;
}

/**
 * detects end of document and adjust current scan
 * to take it into account
 * used by sheetfed scanners
 */
static SANE_Status
gl646_detect_document_end (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t val, gpio;
  unsigned int bytes_left, lines;

  DBG (DBG_proc, "gl646_detect_document_end: start\n");

  /* test for document presence */
  RIE (sanei_genesys_get_status (dev, &val));
  if (DBG_LEVEL > DBG_info)
    {
      print_status (val);
    }
  status = gl646_gpio_read (dev->dn, &gpio);
  DBG (DBG_info, "gl646_detect_document_end: GPIO=0x%02x\n", gpio);

  /* detect document event. There one event when the document go in,
   * then another when it leaves */
  if ((dev->document == SANE_TRUE) && (gpio & 0x04)
      && (dev->total_bytes_read > 0))
    {
      DBG (DBG_info, "gl646_detect_document_end: no more document\n");
      dev->document = SANE_FALSE;

      /* adjust number of bytes to read:
       * total_bytes_to_read is the number of byte to send to frontend
       * total_bytes_read is the number of bytes sent to frontend
       * read_bytes_left is the number of bytes to read from the scanner
       */
      DBG (DBG_io, "gl646_detect_document_end: total_bytes_to_read=%lu\n",
	   (u_long) dev->total_bytes_to_read);
      DBG (DBG_io, "gl646_detect_document_end: total_bytes_read   =%lu\n",
	   (u_long) dev->total_bytes_read);
      DBG (DBG_io, "gl646_detect_document_end: read_bytes_left    =%lu\n",
	   (u_long) dev->read_bytes_left);

      /* amount of data available from scanner is what to scan */
      status = sanei_genesys_read_valid_words (dev, &bytes_left);

      /* we add the number of lines needed to read the last part of the document in */
      lines =
	(SANE_UNFIX (dev->model->y_offset) * dev->current_setup.yres) /
	MM_PER_INCH;
      DBG (DBG_io, "gl646_detect_document_end: adding %d line to flush\n",
	   lines);
      bytes_left += lines * dev->wpl;
      if (dev->current_setup.depth > 8)
	{
	  bytes_left = 2 * bytes_left;
	}
      if (dev->current_setup.channels > 1)
	{
	  bytes_left = 3 * bytes_left;
	}
      if (bytes_left < dev->read_bytes_left)
	{
	  dev->total_bytes_to_read = dev->total_bytes_read + bytes_left;
	  dev->read_bytes_left = bytes_left;
	}
      DBG (DBG_io, "gl646_detect_document_end: total_bytes_to_read=%lu\n",
	   (u_long) dev->total_bytes_to_read);
      DBG (DBG_io, "gl646_detect_document_end: total_bytes_read   =%lu\n",
	   (u_long) dev->total_bytes_read);
      DBG (DBG_io, "gl646_detect_document_end: read_bytes_left    =%lu\n",
	   (u_long) dev->read_bytes_left);
    }
  DBG (DBG_proc, "gl646_detect_document_end: end\n");

  return status;
}

/**
 * eject document from the feeder
 * currently only used by XP200
 * TODO we currently rely on AGOHOME not being set for sheetfed scanners,
 * maybe check this flag in eject to let the document being eject automaticaly
 */
static SANE_Status
gl646_eject_document (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  Genesys_Register_Set regs[11];
  unsigned int used, vfinal, count;
  uint16_t slope_table[255];
  uint8_t gpio, state;

  DBG (DBG_proc, "gl646_eject_document: start\n");

  /* at the end there will be noe more document */
  dev->document = SANE_FALSE;

  /* first check for document event */
  status = gl646_gpio_read (dev->dn, &gpio);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_eject_document: failed to read paper sensor %s\n",
	   sane_strstatus (status));
      return status;
    }
  DBG (DBG_info, "gl646_eject_document: GPIO=0x%02x\n", gpio);

  /* test status : paper event + HOMESNR -> no more doc ? */
  status = sanei_genesys_get_status (dev, &state);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_eject_document: failed to read status: %s\n",
	   sane_strstatus (status));
      return status;
    }
  DBG (DBG_info, "gl646_eject_document: state=0x%02x\n", state);
  if (DBG_LEVEL > DBG_info)
    {
      print_status (state);
    }

  /* HOMSNR=0 if no document inserted */
  if ((state & REG41_HOMESNR) != 0)
    {
      dev->document = SANE_FALSE;
      DBG (DBG_info, "gl646_eject_document: no more document to eject\n");
      DBG (DBG_proc, "gl646_eject_document: end\n");
      return status;
    }

  /* there is a document inserted, eject it */
  status = sanei_genesys_write_register (dev, 0x01, 0xb0);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_eject_document: failed to write register: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* wait for motor to stop */
  do
    {
      usleep (200000UL);
      status = sanei_genesys_get_status (dev, &state);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_eject_document: failed to read status: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }
  while (state & REG41_MOTMFLG);

  /* set up to fast move before scan then move until document is detected */
  regs[0].address = 0x01;
  regs[0].value = 0xb0;

  /* AGOME, 2 slopes motor moving , eject 'backward' */
  regs[1].address = 0x02;
  regs[1].value = 0x5d;

  /* motor feeding steps to 119880 */
  regs[2].address = 0x3d;
  regs[2].value = 1;
  regs[3].address = 0x3e;
  regs[3].value = 0xd4;
  regs[4].address = 0x3f;
  regs[4].value = 0x48;

  /* 60 fast moving steps */
  regs[5].address = 0x6b;
  regs[5].value = 60;

  /* set GPO */
  regs[6].address = 0x66;
  regs[6].value = 0x30;

  /* stesp NO */
  regs[7].address = 0x21;
  regs[7].value = 4;
  regs[8].address = 0x22;
  regs[8].value = 1;
  regs[9].address = 0x23;
  regs[9].value = 1;
  regs[10].address = 0x24;
  regs[10].value = 4;

  /* generate slope table 2 */
  sanei_genesys_generate_slope_table (slope_table,
				      60,
				      61,
				      1600,
				      10000, 1600, 60, 0.25, &used, &vfinal);
/* document eject:
 * send regs
 * start motor
 * wait c1 status to become c8 : HOMESNR and ~MOTFLAG
 */
  status = gl646_send_slope_table (dev, 1, slope_table, 60);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_eject_document: failed to send slope table 1: %s\n",
	   sane_strstatus (status));
      return status;
    }
  status =
    gl646_bulk_write_register (dev, regs, sizeof (regs) / sizeof (regs[0]));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_eject_document: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl646_start_motor (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_eject_document: failed to start motor: %s\n",
	   sane_strstatus (status));
      return SANE_STATUS_IO_ERROR;
    }

  /* loop until paper sensor tells paper is out, and till motor is running */
  /* use a 30 timeout */
  count = 0;
  do
    {
      status = sanei_genesys_get_status (dev, &state);
      print_status (state);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_eject_document: failed to read status: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      usleep (200000UL);	/* sleep 200 ms */
      count++;
    }
  while (((state & REG41_HOMESNR) == 0) && (count < 150));

  /* read GPIO on exit */
  status = gl646_gpio_read (dev->dn, &gpio);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_eject_document: failed to read paper sensor %s\n",
	   sane_strstatus (status));
      return status;
    }
  DBG (DBG_info, "gl646_eject_document: GPIO=0x%02x\n", gpio);

  DBG (DBG_proc, "gl646_eject_document: end\n");
  return status;
}

/* Send the low-level scan command */
static SANE_Status
gl646_begin_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
		  SANE_Bool start_motor)
{
  SANE_Status status;
  Genesys_Register_Set local_reg[3];

  DBG (DBG_proc, "gl646_begin_scan\n");

  local_reg[0].address = 0x03;
  local_reg[0].value = sanei_genesys_read_reg_from_set (reg, 0x03);

  local_reg[1].address = 0x01;
  local_reg[1].value = sanei_genesys_read_reg_from_set (reg, 0x01) | REG01_SCAN;	/* set scan bit */

  local_reg[2].address = 0x0f;
  if (start_motor)
    local_reg[2].value = 0x01;
  else
    local_reg[2].value = 0x00;	/* do not start motor yet */

  status = gl646_bulk_write_register (dev, local_reg,
				      sizeof (local_reg) /
				      sizeof (local_reg[0]));

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_begin_scan: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "gl646_begin_scan: end\n");

  return status;
}


/* Send the stop scan command */
static SANE_Status
end_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
	  SANE_Bool check_stop, SANE_Bool eject)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int i = 0;
  uint8_t val, scanfsh = 0;

  DBG (DBG_proc, "end_scan (check_stop = %d, eject = %d)\n", check_stop,
       eject);

  /* we need to compute scanfsh before cancelling scan */
  if (dev->model->is_sheetfed == SANE_TRUE)
    {
      status = sanei_genesys_get_status (dev, &val);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "end_scan: failed to read register: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      if (val & REG41_SCANFSH)
	scanfsh = 1;
      if (DBG_LEVEL > DBG_io2)
	{
	  print_status (val);
	}
    }

  /* ends scan */
  val = sanei_genesys_read_reg_from_set (reg, 0x01);
  val &= ~REG01_SCAN;
  sanei_genesys_set_reg_from_set (reg, 0x01, val);
  status = sanei_genesys_write_register (dev, 0x01, val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "end_scan: failed to write register 01: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* for sheetfed scanners, we may have to eject document */
  if (dev->model->is_sheetfed == SANE_TRUE)
    {
      if (eject == SANE_TRUE && dev->document == SANE_TRUE)
	{
	  status = gl646_eject_document (dev);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error, "end_scan: failed to eject document\n");
	      return status;
	    }
	}
      if (check_stop)
	{
	  for (i = 0; i < 30; i++)	/* do not wait longer than wait 3 seconds */
	    {
	      status = sanei_genesys_get_status (dev, &val);
	      if (status != SANE_STATUS_GOOD)
		{
		  DBG (DBG_error,
		       "end_scan: failed to read register: %s\n",
		       sane_strstatus (status));
		  return status;
		}
	      if (val & REG41_SCANFSH)
		scanfsh = 1;
	      if (DBG_LEVEL > DBG_io2)
		{
		  print_status (val);
		}

	      if (!(val & REG41_MOTMFLG) && (val & REG41_FEEDFSH) && scanfsh)
		{
		  DBG (DBG_proc, "end_scan: scanfeed finished\n");
		  break;	/* leave for loop */
		}

	      usleep (10000UL);	/* sleep 100 ms */
	    }
	}
    }
  else				/* flat bed scanners */
    {
      if (check_stop)
	{
	  for (i = 0; i < 300; i++)	/* do not wait longer than wait 30 seconds */
	    {
	      status = sanei_genesys_get_status (dev, &val);
	      if (status != SANE_STATUS_GOOD)
		{
		  DBG (DBG_error,
		       "end_scan: failed to read register: %s\n",
		       sane_strstatus (status));
		  return status;
		}
	      if (val & REG41_SCANFSH)
		scanfsh = 1;
	      if (DBG_LEVEL > DBG_io)
		{
		  print_status (val);
		}

	      if (!(val & REG41_MOTMFLG) && (val & REG41_FEEDFSH) && scanfsh)
		{
		  DBG (DBG_proc, "end_scan: scanfeed finished\n");
		  break;	/* leave while loop */
		}

	      if ((!(val & REG41_MOTMFLG)) && (val & REG41_HOMESNR))
		{
		  DBG (DBG_proc, "end_scan: head at home\n");
		  break;	/* leave while loop */
		}

	      usleep (10000UL);	/* sleep 100 ms */
	    }
	}
    }

  DBG (DBG_proc, "end_scan: end (i=%u)\n", i);

  return status;
}

/* Send the stop scan command */
static SANE_Status
gl646_end_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
		SANE_Bool check_stop)
{
  return end_scan (dev, reg, check_stop, SANE_FALSE);
}

/**
 * parks head
 * @param dev scanner's device
 * @param wait_until_home true if the function waits until head parked
 */
GENESYS_STATIC
SANE_Status
gl646_slow_back_home (Genesys_Device * dev, SANE_Bool wait_until_home)
{
  SANE_Status status;
  Genesys_Settings settings;
  uint8_t val;
  int i;
  int loop = 0;

  DBG (DBG_proc, "gl646_slow_back_home: start , wait_until_home = %d\n",
       wait_until_home);

  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_slow_back_home: failed to read home sensor: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (DBG_LEVEL > DBG_io)
    {
      print_status (val);
    }

  dev->scanhead_position_in_steps = 0;

  if (val & REG41_HOMESNR)	/* is sensor at home? */
    {
      DBG (DBG_info, "gl646_slow_back_home: end since already at home\n");
      return SANE_STATUS_GOOD;
    }

  /* stop motor if needed */
  if (val & REG41_MOTMFLG)
    {
      status = gl646_stop_motor (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_slow_back_home: failed to stop motor: %s\n",
	       sane_strstatus (status));
	  return SANE_STATUS_IO_ERROR;
	}
      usleep (200000UL);
    }

  /* when scanhead is moving then wait until scanhead stops or timeout */
  DBG (DBG_info, "gl646_slow_back_home: ensuring that motor is off\n");
  val = REG41_MOTMFLG;
  for (i = 400; i > 0 && (val & REG41_MOTMFLG); i--)	/* do not wait longer than 40 seconds, count down to get i = 0 when busy */
    {
      status = sanei_genesys_get_status (dev, &val);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_slow_back_home: Failed to read home sensor & motor status: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      if (((val & (REG41_MOTMFLG | REG41_HOMESNR)) == REG41_HOMESNR))	/* at home and motor is off */
	{
	  DBG (DBG_info,
	       "gl646_slow_back_home: already at home and not moving\n");
	  return SANE_STATUS_GOOD;
	}
      usleep (100 * 1000);	/* sleep 100 ms (todo: fixed to really sleep 100 ms) */
    }

  if (!i)			/* the loop counted down to 0, scanner still is busy */
    {
      DBG (DBG_error,
	   "gl646_slow_back_home: motor is still on: device busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  /* setup for a backward scan of 65535 steps, with no actual data reading */
  settings.scan_method = SCAN_METHOD_FLATBED;
  settings.scan_mode = SCAN_MODE_COLOR;
  settings.xres = get_lowest_resolution (dev->model->ccd_type, SANE_FALSE);
  settings.yres = settings.xres;
  settings.tl_x = 0;
  settings.tl_y = 0;
  settings.pixels = 600;
  settings.lines = 1;
  settings.depth = 8;
  settings.color_filter = 0;

  settings.disable_interpolation = 0;
  settings.threshold = 0;
  settings.exposure_time = 0;
  settings.dynamic_lineart = SANE_FALSE;

  status = setup_for_scan (dev, dev->reg, settings, SANE_TRUE, SANE_TRUE, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "%s: failed to setup for scan: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      DBGCOMPLETED;
      return status;
    }

  /* backward , no actual data scanned TODO more setup flags to avoid this register manipulations ? */
  dev->reg[reg_0x02].value |= REG02_MTRREV;
  dev->reg[reg_0x01].value &= ~REG01_SCAN;
  gl646_set_triple_reg (dev->reg, REG_FEEDL, 65535);

  /* sets frontend */
  status = gl646_set_fe (dev, AFE_SET, settings.xres);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "%s: failed to set frontend: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      DBGCOMPLETED;
      return status;
    }

  /* write scan registers */
  status = gl646_bulk_write_register (dev, dev->reg,
				      sizeof (dev->reg) /
				      sizeof (dev->reg[0]));
  if (status != SANE_STATUS_GOOD)
    DBG (DBG_error,
	 "gl646_slow_back_home: failed to bulk write registers: %s\n",
	 sane_strstatus (status));

  /* registers are restored to an iddl state, give up if no head to park */
  if (dev->model->is_sheetfed == SANE_TRUE)
    {
      DBG (DBG_proc, "gl646_slow_back_home: end \n");
      return SANE_STATUS_GOOD;
    }

  /* starts scan */
  status = gl646_begin_scan (dev, dev->reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_slow_back_home: failed to begin scan: \n");
      return status;
    }

  /* loop until head parked */
  if (wait_until_home)
    {
      while (loop < 300)		/* do not wait longer then 30 seconds */
	{
	  status = sanei_genesys_get_status (dev, &val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl646_slow_back_home: Failed to read home sensor: %s\n",
		   sane_strstatus (status));
	      return status;
	    }

	  if (val & 0x08)	/* home sensor */
	    {
	      DBG (DBG_info, "gl646_slow_back_home: reached home position\n");
	      DBG (DBG_proc, "gl646_slow_back_home: end\n");
	      usleep (500000);	/* sleep 500 ms before returning */
	      return SANE_STATUS_GOOD;
	    }
	  usleep (100000);	/* sleep 100 ms */
          ++loop;
	}

      /* when we come here then the scanner needed too much time for this, so we better stop the motor */
      gl646_stop_motor (dev);
      end_scan (dev, dev->reg, SANE_TRUE, SANE_FALSE);
      DBG (DBG_error,
	   "gl646_slow_back_home: timeout while waiting for scanhead to go home\n");
      return SANE_STATUS_IO_ERROR;
    }


  DBG (DBG_info, "gl646_slow_back_home: scanhead is still moving\n");
  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/**
 * Automatically set top-left edge of the scan area by scanning an
 * area at 300 dpi from very top of scanner
 * @param dev  device stucture describing the scanner
 * @return SANE_STATUS_GOOD in cas of success, else failure code
 */
static SANE_Status
gl646_search_start_position (Genesys_Device * dev)
{
  SANE_Status status;
  unsigned char *data = NULL;
  Genesys_Settings settings;
  unsigned int resolution, x, y;

  DBG (DBG_proc, "gl646_search_start_position: start\n");

  /* we scan at 300 dpi */
  resolution = get_closest_resolution (dev->model->ccd_type, 300, SANE_FALSE);

  /* fill settings for a gray level scan */
  settings.scan_method = SCAN_METHOD_FLATBED;
  settings.scan_mode = SCAN_MODE_GRAY;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_x = 0;
  settings.tl_y = 0;
  settings.pixels = 600;
  settings.lines = dev->model->search_lines;
  settings.depth = 8;
  settings.color_filter = 0;

  settings.disable_interpolation = 0;
  settings.threshold = 0;
  settings.exposure_time = 0;
  settings.dynamic_lineart = SANE_FALSE;

  /* scan the desired area */
  status =
    simple_scan (dev, settings, SANE_TRUE, SANE_TRUE, SANE_FALSE, &data);

  /* process data if scan is OK */
  if (status == SANE_STATUS_GOOD)
    {
      /* handle stagger case : reorder gray data and thus loose some lines */
      if (dev->current_setup.stagger > 0)
	{
	  DBG (DBG_proc, "gl646_search_start_position: 'un-staggering'\n");
	  for (y = 0; y < settings.lines - dev->current_setup.stagger; y++)
	    {
	      /* one point out of 2 is 'unaligned' */
	      for (x = 0; x < settings.pixels; x += 2)
		{
		  data[y * settings.pixels + x] =
		    data[(y + dev->current_setup.stagger) * settings.pixels +
			 x];
		}
	    }
	  /* correct line number */
	  settings.lines -= dev->current_setup.stagger;
	}
      if (DBG_LEVEL >= DBG_data)
	{
	  sanei_genesys_write_pnm_file ("search_position.pnm",
					data,
					settings.depth,
					1, settings.pixels, settings.lines);
	}
    }
  else
    {
      DBG (DBG_error, "gl646_search_start_position: simple_scan failed\n");
      free (data);
      DBGCOMPLETED;
      return status;
    }

  /* now search reference points on the data */
  status =
    sanei_genesys_search_reference_point (dev, data,
					  dev->sensor.CCD_start_xoffset,
					  resolution, settings.pixels,
					  settings.lines);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_search_start_position: failed to set search reference point: %s\n",
	   sane_strstatus (status));
    }

  free (data);
  DBGCOMPLETED;
  return status;
}

/**
 * internally overriden during effective calibration
 * sets up register for coarse gain calibration
 */
static SANE_Status
gl646_init_regs_for_coarse_calibration (Genesys_Device * dev)
{
  DBG (DBG_proc, "gl646_init_regs_for_coarse_calibration\n");
  DBG (DBG_proc, "gl646_init_register_for_coarse_calibration: end\n");

  /* to make compilers happy ... */
  if (!dev)
    {
      return SANE_STATUS_INVAL;
    }

  return SANE_STATUS_GOOD;
}


/**
 * init registers for shading calibration
 * we assume that scanner's head is on an area suiting shading calibration.
 * We scan a full scan width area by the shading line number for the device
 * at either at full sensor's resolution or half depending upon half_ccd
 * @param dev scanner's device
 * @return SANE_STATUS_GOOD if success, else error code
 */
static SANE_Status
gl646_init_regs_for_shading (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  Genesys_Settings settings;
  /* 1: no half_ccd, 2: use half number of pixels */
  int half_ccd = 1;
  int cksel = 1;

  DBG (DBG_proc, "gl646_init_register_for_shading: start\n");

  /* when shading all (full width) line, we must adapt to half_ccd case */
  if (dev->model->flags & GENESYS_FLAG_HALF_CCD_MODE)
    {
      /* walk the master mode list to find if half_ccd */
      if (is_half_ccd (dev->model->ccd_type, dev->settings.xres, SANE_TRUE) ==
	  SANE_TRUE)
	{
	  half_ccd = 2;
	}
    }

  /* fill settings for scan : always a color scan */
  settings.scan_method = dev->settings.scan_method;
  settings.scan_mode = dev->settings.scan_mode;
  if (dev->model->is_cis == SANE_FALSE)
    {
      settings.scan_mode = SCAN_MODE_COLOR;
    }
  settings.xres = dev->sensor.optical_res / half_ccd;
  cksel = get_cksel (dev->model->ccd_type, dev->settings.xres, SANE_TRUE);
  settings.xres = settings.xres / cksel;
  settings.yres = settings.xres;
  settings.tl_x = 0;
  settings.tl_y = 0;
  settings.pixels =
    (dev->sensor.sensor_pixels * settings.xres) / dev->sensor.optical_res;
  dev->calib_lines = dev->model->shading_lines;
  settings.lines = dev->calib_lines * (3 - half_ccd);
  settings.depth = 16;
  settings.color_filter = dev->settings.color_filter;

  settings.disable_interpolation = dev->settings.disable_interpolation;
  settings.threshold = dev->settings.threshold;
  settings.exposure_time = dev->settings.exposure_time;
  settings.dynamic_lineart = SANE_FALSE;

  /* keep account of the movement for final scan move */
  dev->scanhead_position_in_steps += settings.lines;

  /* we don't want top offset, but we need right margin to be the same
   * than the one for the final scan */
  status = setup_for_scan (dev, dev->reg, settings, SANE_TRUE, SANE_FALSE, SANE_FALSE);

  /* used when sending shading calibration data */
  dev->calib_pixels = settings.pixels;
  dev->calib_channels = dev->current_setup.channels;
  if (dev->model->is_cis == SANE_FALSE)
    {
      dev->calib_channels = 3;
    }

  /* no shading */
  dev->reg[reg_0x01].value &= ~REG01_DVDSET;
  dev->reg[reg_0x02].value |= REG02_ACDCDIS;	/* ease backtracking */
  dev->reg[reg_0x02].value &= ~(REG02_FASTFED | REG02_AGOHOME);
  dev->reg[reg_0x05].value &= ~REG05_GMMENB;
  gl646_set_motor_power (dev->reg, SANE_FALSE);

  /* TODO another flag to setup regs ? */
  /* enforce needed LINCNT, getting rid of extra lines for color reordering */
  if (dev->model->is_cis == SANE_FALSE)
    {
      gl646_set_triple_reg (dev->reg, REG_LINCNT, dev->calib_lines);
    }
  else
    {
      gl646_set_triple_reg (dev->reg, REG_LINCNT, dev->calib_lines * 3);
    }

  /* copy reg to calib_reg */
  memcpy (dev->calib_reg, dev->reg,
	  GENESYS_GL646_MAX_REGS * sizeof (Genesys_Register_Set));

  /* this is an hack to make calibration cache working .... */
  /* if we don't do this, cache will be identified at the shading calibration
   * dpi which is different from calibration one */
  dev->current_setup.xres = dev->settings.xres;
  DBG (DBG_info,
       "gl646_init_register_for_shading:\n\tdev->settings.xres=%d\n\tdev->settings.yres=%d\n",
       dev->settings.xres, dev->settings.yres);

  DBG (DBG_proc, "gl646_init_register_for_shading: end\n");
  return status;
}


/**
 * set up registers for the actual scan. The scan's parameters are given
 * through the device settings. It allocates the scan buffers.
 */
static SANE_Status
gl646_init_regs_for_scan (Genesys_Device * dev)
{
  SANE_Status status;

  DBGSTART;

  /* park head after calibration if needed */
  if (dev->scanhead_position_in_steps > 0
      && dev->settings.scan_method == SCAN_METHOD_FLATBED)
    {
      RIE(gl646_slow_back_home (dev, SANE_TRUE));
      dev->scanhead_position_in_steps = 0;
    }

  RIE(setup_for_scan (dev, dev->reg, dev->settings, SANE_FALSE, SANE_TRUE, SANE_TRUE));

  /* gamma is only enabled at final scan time */
  if (dev->settings.depth < 16)
    dev->reg[reg_0x05].value |= REG05_GMMENB;

  DBGCOMPLETED;
  return status;
}

/**
 * set up registers for the actual scan. The scan's parameters are given
 * through the device settings. It allocates the scan buffers.
 * @param dev scanner's device
 * @param regs     registers to set up
 * @param settings settings of scan
 * @param split SANE_TRUE if move to scan area is split from scan, SANE_FALSE is
 *              scan first moves to area
 * @param xcorrection take x geometry correction into account (fixed and detected offsets)
 * @param ycorrection take y geometry correction into account
 */
GENESYS_STATIC SANE_Status
setup_for_scan (Genesys_Device * dev,
                Genesys_Register_Set *regs,
                Genesys_Settings settings,
                SANE_Bool split,
                SANE_Bool xcorrection,
                SANE_Bool ycorrection)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Bool color;
  SANE_Int depth;
  int channels;
  uint16_t startx = 0, endx, pixels;
  int move = 0;

  DBGSTART;
  DBG (DBG_info,
       "%s settings:\nResolution: %ux%uDPI\n"
       "Lines     : %u\nPixels    : %u\nStartpos  : %.3f/%.3f\nScan mode : %d\nScan method: %s\n\n",
       __FUNCTION__,
       settings.xres, settings.yres, settings.lines, settings.pixels,
       settings.tl_x, settings.tl_y, settings.scan_mode,
       settings.scan_method == SCAN_METHOD_FLATBED ? "flatbed" : "XPA");

  if (settings.scan_mode == SCAN_MODE_COLOR)	/* single pass color */
    {
      channels = 3;
      color = SANE_TRUE;
    }
  else
    {
      channels = 1;
      color = SANE_FALSE;
    }

  depth=settings.depth;
  if (settings.scan_mode == SCAN_MODE_LINEART)
    {
      if (settings.dynamic_lineart == SANE_TRUE)
        {
          depth = 8;
        }
      else
        {
          /* XXX STEF XXX : why does the common layer never send depth=1 ? */
          depth = 1;
        }
    }

  /* compute distance to move */
  move = 0;
  /* XXX STEF XXX MD5345 -> optical_ydpi, other base_ydpi => half/full step ? */
  if (split == SANE_FALSE)
    {
      if (dev->model->is_sheetfed == SANE_FALSE)
	{
	  if (ycorrection == SANE_TRUE)
	    {
	      move =
		(SANE_UNFIX (dev->model->y_offset) *
		 dev->motor.optical_ydpi) / MM_PER_INCH;
	    }

	  /* add tl_y to base movement */
	  move += (settings.tl_y * dev->motor.optical_ydpi) / MM_PER_INCH;

	}
      else
	{
	  move += (settings.tl_y * dev->motor.optical_ydpi) / MM_PER_INCH;
	}

      DBG (DBG_info, "%s: move=%d steps\n", __FUNCTION__, move);

      /* security check */
      if (move < 0)
	{
	  DBG (DBG_error, "%s: overriding negative move value %d\n", __FUNCTION__, move);
	  move = 0;
	}
    }
  DBG (DBG_info, "%s: move=%d steps\n", __FUNCTION__, move);

  /* pixels are allways given at full CCD optical resolution */
  /* use detected left margin and fixed value */
  if (xcorrection == SANE_TRUE)
    {
      if (dev->sensor.CCD_start_xoffset > 0)
	startx = dev->sensor.CCD_start_xoffset;
      else
	startx = dev->sensor.dummy_pixel;
      if (settings.scan_method == SCAN_METHOD_FLATBED)
	{
	  startx +=
	    ((SANE_UNFIX (dev->model->x_offset) * dev->sensor.optical_res) /
	     MM_PER_INCH);
	}
      else
	{
	  startx +=
	    ((SANE_UNFIX (dev->model->x_offset_ta) *
	      dev->sensor.optical_res) / MM_PER_INCH);
	}
    }
  else
    {
      /* startx cannot be below dummy pixel value */
      startx = dev->sensor.dummy_pixel;
    }

  /* add x coordinates : expressed in sensor max dpi */
  startx += (settings.tl_x * dev->sensor.optical_res) / MM_PER_INCH;

  /* stagger works with odd start cordinates */
  if (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE)
    startx |= 1;

  pixels = (settings.pixels * dev->sensor.optical_res) / settings.xres;
  /* special requirement for 400 dpi on 1200 dpi sensors */
  if (settings.xres == 400)
    {
      pixels = (pixels / 6) * 6;
    }
  endx = startx + pixels;

  /* TODO check for pixel width overflow */

  /* set up correct values for scan (gamma and shading enabled) */
  status = gl646_setup_registers (dev,
				  regs,
				  settings,
				  dev->slope_table0,
				  dev->slope_table1,
				  settings.xres,
				  move,
				  settings.lines,
				  startx, endx, color,
                                  depth);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed setup registers: %s\n", __FUNCTION__, sane_strstatus (status));
      return status;
    }

  /* now post-process values for register and options fine tuning */

  /* select color filter based on settings */
  regs[reg_0x04].value &= ~REG04_FILTER;
  if (channels == 1)
    {
      switch (settings.color_filter)
	{
	  /* red */
	case 0:
	  regs[reg_0x04].value |= 0x04;
	  break;
	  /* green */
	case 1:
	  regs[reg_0x04].value |= 0x08;
	  break;
	  /* blue */
	case 2:
	  regs[reg_0x04].value |= 0x0c;
	  break;
	default:
	  break;
	}
    }

  /* send computed slope tables */
  status =
    gl646_send_slope_table (dev, 0, dev->slope_table0,
			    sanei_genesys_read_reg_from_set (regs, 0x21));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to send slope table 0: %s\n", __FUNCTION__, sane_strstatus (status));
      return status;
    }

  status =
    gl646_send_slope_table (dev, 1, dev->slope_table1,
			    sanei_genesys_read_reg_from_set (regs, 0x6b));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to send slope table 1: %s\n", __FUNCTION__, sane_strstatus (status));
      return status;
    }

  DBGCOMPLETED;
  return status;
}

/**
 * this function sen gamm table to ASIC
 */
static SANE_Status
gl646_send_gamma_table (Genesys_Device * dev)
{
  int size;
  int address;
  SANE_Status status;
  uint8_t *gamma;
  int bits;

  DBGSTART;

  /* gamma table size */
  if (dev->reg[reg_0x05].value & REG05_GMMTYPE)
    {
      size = 16384;
      bits = 14;
    }
  else
    {
      size = 4096;
      bits = 12;
    }

  /* allocate temporary gamma tables: 16 bits words, 3 channels */
  gamma = (uint8_t *) malloc (size * 2 * 3);
  if (gamma==NULL)
    {
      return SANE_STATUS_NO_MEM;
    }

  RIE(sanei_genesys_generate_gamma_buffer(dev, bits, size-1, size, gamma));

  /* table address */
  switch (dev->reg[reg_0x05].value >> 6)
    {
    case 0:			/* 600 dpi */
      address = 0x09000;
      break;
    case 1:			/* 1200 dpi */
      address = 0x11000;
      break;
    case 2:			/* 2400 dpi */
      address = 0x20000;
      break;
    default:
      free (gamma);
      return SANE_STATUS_INVAL;
    }

  /* send address */
  status = sanei_genesys_set_buffer_address (dev, address);
  if (status != SANE_STATUS_GOOD)
    {
      free (gamma);
      DBG (DBG_error,
	   "gl646_send_gamma_table: failed to set buffer address: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* send data */
  status = gl646_bulk_write_data (dev, 0x3c, (uint8_t *) gamma, size * 2 * 3);
  if (status != SANE_STATUS_GOOD)
    {
      free (gamma);
      DBG (DBG_error,
	   "gl646_send_gamma_table: failed to send gamma table: %s\n",
	   sane_strstatus (status));
      return status;
    }
  free (gamma);

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/** @brief this function does the led calibration.
 * this function does the led calibration by scanning one line of the calibration
 * area below scanner's top on white strip. The scope of this function is
 * currently limited to the XP200
 */
static SANE_Status
gl646_led_calibration (Genesys_Device * dev)
{
  int total_size;
  uint8_t *line;
  unsigned int i, j;
  SANE_Status status = SANE_STATUS_GOOD;
  int val;
  unsigned int channels;
  int avg[3], avga, avge;
  int turn;
  char fn[20];
  uint16_t expr, expg, expb;
  Genesys_Settings settings;
  SANE_Int resolution;

  SANE_Bool acceptable = SANE_FALSE;

  DBG (DBG_proc, "gl646_led_calibration\n");
  if (!dev->model->is_cis)
    {
      DBG (DBG_proc,
	   "gl646_led_calibration: not a cis scanner, nothing to do...\n");
      return SANE_STATUS_GOOD;
    }

  /* get led calibration resolution */
  if (dev->settings.scan_mode == SCAN_MODE_COLOR)
    {
      resolution =
	get_closest_resolution (dev->model->ccd_type, dev->sensor.optical_res,
				SANE_TRUE);
      settings.scan_mode = SCAN_MODE_COLOR;
      channels = 3;
    }
  else
    {
      resolution =
	get_closest_resolution (dev->model->ccd_type, dev->sensor.optical_res,
				SANE_FALSE);
      settings.scan_mode = SCAN_MODE_GRAY;
      channels = 1;
    }

  /* offset calibration is always done in color mode */
  settings.scan_method = SCAN_METHOD_FLATBED;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_x = 0;
  settings.tl_y = 0;
  settings.pixels =
    (dev->sensor.sensor_pixels * resolution) / dev->sensor.optical_res;
  settings.lines = 1;
  settings.depth = 16;
  settings.color_filter = 0;

  settings.disable_interpolation = 0;
  settings.threshold = 0;
  settings.exposure_time = 0;
  settings.dynamic_lineart = SANE_FALSE;

  /* colors * bytes_per_color * scan lines */
  total_size = settings.pixels * channels * 2 * 1;

  line = malloc (total_size);
  if (!line)
    {
      DBG (DBG_error, "gl646_led_calibration: failed to allocate %d bytes\n",
	   total_size);
      return SANE_STATUS_NO_MEM;
    }

/*
   we try to get equal bright leds here:

   loop:
     average per color
     adjust exposure times

  Sensor_Master uint8_t regs_0x10_0x15[6];
 */

  expr = (dev->sensor.regs_0x10_0x1d[0] << 8) | dev->sensor.regs_0x10_0x1d[1];
  expg = (dev->sensor.regs_0x10_0x1d[2] << 8) | dev->sensor.regs_0x10_0x1d[3];
  expb = (dev->sensor.regs_0x10_0x1d[4] << 8) | dev->sensor.regs_0x10_0x1d[5];

  turn = 0;

  do
    {

      dev->sensor.regs_0x10_0x1d[0] = (expr >> 8) & 0xff;
      dev->sensor.regs_0x10_0x1d[1] = expr & 0xff;
      dev->sensor.regs_0x10_0x1d[2] = (expg >> 8) & 0xff;
      dev->sensor.regs_0x10_0x1d[3] = expg & 0xff;
      dev->sensor.regs_0x10_0x1d[4] = (expb >> 8) & 0xff;
      dev->sensor.regs_0x10_0x1d[5] = expb & 0xff;

      DBG (DBG_info, "gl646_led_calibration: starting first line reading\n");

      status =
	simple_scan (dev, settings, SANE_FALSE, SANE_TRUE, SANE_FALSE, &line);
      if (status != SANE_STATUS_GOOD)
	{
          free(line);
	  DBG (DBG_error,
	       "gl646_led_calibration: failed to setup scan: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      if (DBG_LEVEL >= DBG_data)
	{
	  snprintf (fn, 20, "led_%02d.pnm", turn);
	  sanei_genesys_write_pnm_file (fn,
					line,
					16, channels, settings.pixels, 1);
	}

      acceptable = SANE_TRUE;

      for (j = 0; j < channels; j++)
	{
	  avg[j] = 0;
	  for (i = 0; i < settings.pixels; i++)
	    {
	      if (dev->model->is_cis)
		val =
		  line[i * 2 + j * 2 * settings.pixels + 1] * 256 +
		  line[i * 2 + j * 2 * settings.pixels];
	      else
		val =
		  line[i * 2 * channels + 2 * j + 1] * 256 +
		  line[i * 2 * channels + 2 * j];
	      avg[j] += val;
	    }

	  avg[j] /= settings.pixels;
	}

      DBG (DBG_info, "gl646_led_calibration: average: "
	   "%d,%d,%d\n", avg[0], avg[1], avg[2]);

      acceptable = SANE_TRUE;

      if (!acceptable)
	{
	  avga = (avg[0] + avg[1] + avg[2]) / 3;
	  expr = (expr * avga) / avg[0];
	  expg = (expg * avga) / avg[1];
	  expb = (expb * avga) / avg[2];

	  /* keep exposure time in a working window */
	  avge = (expr + expg + expb) / 3;
	  if (avge > 0x2000)
	    {
	      expr = (expr * 0x2000) / avge;
	      expg = (expg * 0x2000) / avge;
	      expb = (expb * 0x2000) / avge;
	    }
	  if (avge < 0x400)
	    {
	      expr = (expr * 0x400) / avge;
	      expg = (expg * 0x400) / avge;
	      expb = (expb * 0x400) / avge;
	    }
	}

      turn++;

    }
  while (!acceptable && turn < 100);

  DBG (DBG_info,
       "gl646_led_calibration: acceptable exposure: 0x%04x,0x%04x,0x%04x\n",
       expr, expg, expb);

  /* cleanup before return */
  free (line);

  DBGCOMPLETED;
  return status;
}

/**
 * average dark pixels of a scan
 */
static int
dark_average (uint8_t * data, unsigned int pixels, unsigned int lines,
	      unsigned int channels, unsigned int black)
{
  unsigned int i, j, k, average, count;
  unsigned int avg[3];
  uint8_t val;

  /* computes average value on black margin */
  for (k = 0; k < channels; k++)
    {
      avg[k] = 0;
      count = 0;
      for (i = 0; i < lines; i++)
	{
	  for (j = 0; j < black; j++)
	    {
	      val = data[i * channels * pixels + j + k];
	      avg[k] += val;
	      count++;
	    }
	}
      if (count)
	avg[k] /= count;
      DBG (DBG_info, "dark_average: avg[%d] = %d\n", k, avg[k]);
    }
  average = 0;
  for (i = 0; i < channels; i++)
    average += avg[i];
  average /= channels;
  DBG (DBG_info, "dark_average: average = %d\n", average);
  return average;
}


/** @brief calibration for AD frontend devices
 * we do simple scan until all black_pixels are higher than 0,
 * raising offset at each turn.
 */
static SANE_Status
ad_fe_offset_calibration (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t *line;
  unsigned int channels;
  char title[32];
  int pass = 0;
  SANE_Int resolution;
  Genesys_Settings settings;
  unsigned int x, y, adr, min;
  unsigned int bottom, black_pixels;

  DBG (DBG_proc, "ad_fe_offset_calibration: start\n");
  resolution =
    get_closest_resolution (dev->model->ccd_type, dev->sensor.optical_res,
			    SANE_TRUE);
  channels = 3;
  black_pixels =
    (dev->sensor.black_pixels * resolution) / dev->sensor.optical_res;
  DBG (DBG_io2, "ad_fe_offset_calibration: black_pixels=%d\n", black_pixels);

  settings.scan_method = SCAN_METHOD_FLATBED;
  settings.scan_mode = SCAN_MODE_COLOR;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_x = 0;
  settings.tl_y = 0;
  settings.pixels =
    (dev->sensor.sensor_pixels * resolution) / dev->sensor.optical_res;
  settings.lines = CALIBRATION_LINES;
  settings.depth = 8;
  settings.color_filter = 0;

  settings.disable_interpolation = 0;
  settings.threshold = 0;
  settings.exposure_time = 0;
  settings.dynamic_lineart = SANE_FALSE;

  /* scan first line of data with no gain */
  dev->frontend.gain[0] = 0;
  dev->frontend.gain[1] = 0;
  dev->frontend.gain[2] = 0;

  /* scan with no move */
  bottom = 1;
  do
    {
      pass++;
      dev->frontend.offset[0] = bottom;
      dev->frontend.offset[1] = bottom;
      dev->frontend.offset[2] = bottom;
      status =
	simple_scan (dev, settings, SANE_FALSE, SANE_TRUE, SANE_FALSE, &line);
      if (status != SANE_STATUS_GOOD)
	{
          free(line);
	  DBG (DBG_error,
	       "ad_fe_offset_calibration: failed to scan first line\n");
	  return status;
	}
      if (DBG_LEVEL >= DBG_data)
	{
	  sprintf (title, "offset%03d.pnm", (int)bottom);
	  sanei_genesys_write_pnm_file (title, line, 8, channels,
					settings.pixels, settings.lines);
	}

      min = 0;
      for (y = 0; y < settings.lines; y++)
	{
	  for (x = 0; x < black_pixels; x++)
	    {
	      adr = (x + y * settings.pixels) * channels;
	      if (line[adr] > min)
		min = line[adr];
	      if (line[adr + 1] > min)
		min = line[adr + 1];
	      if (line[adr + 2] > min)
		min = line[adr + 2];
	    }
	}

      free (line);
      DBG (DBG_io2, "ad_fe_offset_calibration: pass=%d, min=%d\n", pass, min);
      bottom++;
    }
  while (pass < 128 && min == 0);
  if (pass == 128)
    {
      DBG (DBG_error,
	   "ad_fe_offset_calibration: failed to find correct offset\n");
      return SANE_STATUS_INVAL;
    }

  DBG (DBG_info, "ad_fe_offset_calibration: offset=(%d,%d,%d)\n",
       dev->frontend.offset[0], dev->frontend.offset[1],
       dev->frontend.offset[2]);
  DBG (DBG_proc, "ad_fe_offset_calibration: end\n");
  return status;
}

#define DARK_TARGET 8
/**
 * This function does the offset calibration by scanning one line of the calibration
 * area below scanner's top. There is a black margin and the remaining is white.
 * genesys_search_start() must have been called so that the offsets and margins
 * are already known.
 * @param dev scanner's device
 * @return SANE_STATUS_GOOD if success, else error code is failure
*/
static SANE_Status
gl646_offset_calibration (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t *first_line, *second_line;
  unsigned int channels;
  char title[32];
  int pass = 0, avg;
  SANE_Int resolution;
  Genesys_Settings settings;
  int topavg, bottomavg;
  int top, bottom, black_pixels;

  /* Analog Device fronted have a different calibration */
  if (dev->model->dac_type == DAC_AD_XP200)
    {
      return ad_fe_offset_calibration (dev);
    }

  DBG (DBG_proc, "gl646_offset_calibration: start\n");

  /* setup for a RGB scan, one full sensor's width line */
  /* resolution is the one from the final scan          */
  if (dev->settings.xres > dev->sensor.optical_res)
    {
      resolution =
	get_closest_resolution (dev->model->ccd_type, dev->sensor.optical_res,
				SANE_TRUE);
    }
  else
    {
      resolution =
	get_closest_resolution (dev->model->ccd_type, dev->settings.xres,
				SANE_TRUE);
    }
  channels = 3;
  black_pixels =
    (dev->sensor.black_pixels * resolution) / dev->sensor.optical_res;
  DBG (DBG_io2, "gl646_offset_calibration: black_pixels=%d\n", black_pixels);

  settings.scan_method = SCAN_METHOD_FLATBED;
  settings.scan_mode = SCAN_MODE_COLOR;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_x = 0;
  settings.tl_y = 0;
  settings.pixels =
    (dev->sensor.sensor_pixels * resolution) / dev->sensor.optical_res;
  settings.lines = CALIBRATION_LINES;
  settings.depth = 8;
  settings.color_filter = 0;

  settings.disable_interpolation = 0;
  settings.threshold = 0;
  settings.exposure_time = 0;
  settings.dynamic_lineart = SANE_FALSE;

  /* scan first line of data with no gain, but with offset from
   * last calibration */
  dev->frontend.gain[0] = 0;
  dev->frontend.gain[1] = 0;
  dev->frontend.gain[2] = 0;

  /* scan with no move */
  bottom = 90;
  dev->frontend.offset[0] = bottom;
  dev->frontend.offset[1] = bottom;
  dev->frontend.offset[2] = bottom;
  status =
    simple_scan (dev, settings, SANE_FALSE, SANE_TRUE, SANE_FALSE,
		 &first_line);
  if (status != SANE_STATUS_GOOD)
    {
      free(first_line);
      DBG (DBG_error,
	   "gl646_offset_calibration: failed to scan first line\n");
      return status;
    }
  if (DBG_LEVEL >= DBG_data)
    {
      sprintf (title, "offset%03d.pnm", bottom);
      sanei_genesys_write_pnm_file (title, first_line, 8, channels,
				    settings.pixels, settings.lines);
    }
  bottomavg =
    dark_average (first_line, settings.pixels, settings.lines, channels,
		  black_pixels);
  free (first_line);
  DBG (DBG_io2, "gl646_offset_calibration: bottom avg=%d\n", bottomavg);

  /* now top value */
  top = 231;
  dev->frontend.offset[0] = top;
  dev->frontend.offset[1] = top;
  dev->frontend.offset[2] = top;
  status =
    simple_scan (dev, settings, SANE_FALSE, SANE_TRUE, SANE_FALSE,
		 &second_line);
  if (status != SANE_STATUS_GOOD)
    {
      free(second_line);
      DBG (DBG_error,
	   "gl646_offset_calibration: failed to scan first line\n");
      return status;
    }

  if (DBG_LEVEL >= DBG_data)
    {
      sprintf (title, "offset%03d.pnm", top);
      sanei_genesys_write_pnm_file (title, second_line, 8, channels,
				    settings.pixels, settings.lines);
    }
  topavg =
    dark_average (second_line, settings.pixels, settings.lines, channels,
		  black_pixels);
  free (second_line);
  DBG (DBG_io2, "gl646_offset_calibration: top avg=%d\n", topavg);

  /* loop until acceptable level */
  while ((pass < 32) && (top - bottom > 1))
    {
      pass++;

      /* settings for new scan */
      dev->frontend.offset[0] = (top + bottom) / 2;
      dev->frontend.offset[1] = (top + bottom) / 2;
      dev->frontend.offset[2] = (top + bottom) / 2;

      /* scan with no move */
      status =
	simple_scan (dev, settings, SANE_FALSE, SANE_TRUE, SANE_FALSE,
		     &second_line);
      if (status != SANE_STATUS_GOOD)
	{
          free(second_line);
	  DBG (DBG_error,
	       "gl646_offset_calibration: failed to scan first line\n");
	  return status;
	}

      if (DBG_LEVEL >= DBG_data)
	{
	  sprintf (title, "offset%03d.pnm", dev->frontend.offset[1]);
	  sanei_genesys_write_pnm_file (title, second_line, 8, channels,
					settings.pixels, settings.lines);
	}

      avg =
	dark_average (second_line, settings.pixels, settings.lines, channels,
		      black_pixels);
      DBG (DBG_info, "gl646_offset_calibration: avg=%d offset=%d\n", avg,
	   dev->frontend.offset[1]);
      free (second_line);

      /* compute new boundaries */
      if (topavg == avg)
	{
	  topavg = avg;
	  top = dev->frontend.offset[1];
	}
      else
	{
	  bottomavg = avg;
	  bottom = dev->frontend.offset[1];
	}
    }

  /* in case of debug do a final scan to get result */
  if (DBG_LEVEL >= DBG_data)
    {
      status =
	simple_scan (dev, settings, SANE_FALSE, SANE_TRUE, SANE_FALSE,
		     &second_line);
      if (status != SANE_STATUS_GOOD)
	{
          free(second_line);
	  DBG (DBG_error,
	       "gl646_offset_calibration: failed to scan final line\n");
	  return status;
	}
      sanei_genesys_write_pnm_file ("offset-final.pnm", second_line, 8,
				    channels, settings.pixels,
				    settings.lines);
      free (second_line);
    }

  DBG (DBG_info, "gl646_offset_calibration: offset=(%d,%d,%d)\n",
       dev->frontend.offset[0], dev->frontend.offset[1],
       dev->frontend.offset[2]);
  DBG (DBG_proc, "gl646_offset_calibration: end\n");
  return status;
}

/** @brief gain calibration for Analog Device frontends
 * Alternative coarse gain calibration
 */
static SANE_Status
ad_fe_coarse_gain_calibration (Genesys_Device * dev, int dpi)
{
  uint8_t *line;
  unsigned int i, channels, val;
  unsigned int size, count, resolution, pass;
  SANE_Status status = SANE_STATUS_GOOD;
  float average;
  Genesys_Settings settings;
  char title[32];

  DBGSTART;

  /* setup for a RGB scan, one full sensor's width line */
  /* resolution is the one from the final scan          */
  resolution = get_closest_resolution (dev->model->ccd_type, dpi, SANE_TRUE);
  channels = 3;
  settings.scan_mode = SCAN_MODE_COLOR;

  settings.scan_method = SCAN_METHOD_FLATBED;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_x = 0;
  settings.tl_y = 0;
  settings.pixels =
    (dev->sensor.sensor_pixels * resolution) / dev->sensor.optical_res;
  settings.lines = CALIBRATION_LINES;
  settings.depth = 8;
  settings.color_filter = 0;

  settings.disable_interpolation = 0;
  settings.threshold = 0;
  settings.exposure_time = 0;
  settings.dynamic_lineart = SANE_FALSE;

  size = channels * settings.pixels * settings.lines;

  /* start gain value */
  dev->frontend.gain[0] = 1;
  dev->frontend.gain[1] = 1;
  dev->frontend.gain[2] = 1;

  average = 0;
  pass = 0;

  /* loop until each channel raises to acceptable level */
  while ((average < dev->sensor.gain_white_ref) && (pass < 30))
    {
      /* scan with no move */
      status =
	simple_scan (dev, settings, SANE_FALSE, SANE_TRUE, SANE_FALSE, &line);
      if (status != SANE_STATUS_GOOD)
	{
          free(line);
	  DBG (DBG_error,
	       "ad_fe_coarse_gain_calibration: failed to scan first line\n");
	  return status;
	}

      /* log scanning data */
      if (DBG_LEVEL >= DBG_data)
	{
	  sprintf (title, "alternative_coarse%02d.pnm", (int)pass);
	  sanei_genesys_write_pnm_file (title, line, 8,
					channels, settings.pixels,
					settings.lines);
	}
      pass++;

      /* computes white average */
      average = 0;
      count = 0;
      for (i = 0; i < size; i++)
	{
	  val = line[i];
	  average += val;
	  count++;
	}
      average = average / count;

      /* adjusts gain for the channel */
      if (average < dev->sensor.gain_white_ref)
	dev->frontend.gain[0]++;
      dev->frontend.gain[1] = dev->frontend.gain[0];
      dev->frontend.gain[2] = dev->frontend.gain[0];

      DBG (DBG_proc,
	   "ad_fe_coarse_gain_calibration: average = %.2f, gain = %d\n",
	   average, dev->frontend.gain[0]);
      free (line);
    }

  DBG (DBG_info, "ad_fe_coarse_gain_calibration: gains=(%d,%d,%d)\n",
       dev->frontend.gain[0], dev->frontend.gain[1], dev->frontend.gain[2]);
  DBGCOMPLETED;
  return status;
}

/**
 * Alternative coarse gain calibration
 * this on uses the settings from offset_calibration. First scan moves so
 * we can go to calibration area for XPA.
 * @param dev device for scan
 * @param dpi resolutnio to calibrate at
 */
static SANE_Status
gl646_coarse_gain_calibration (Genesys_Device * dev, int dpi)
{
  uint8_t *line;
  unsigned int i, j, k, channels, val, maximum, idx;
  unsigned int count, resolution, pass;
  SANE_Status status = SANE_STATUS_GOOD;
  float average[3];
  Genesys_Settings settings;
  char title[32];

  if (dev->model->ccd_type == CIS_XP200)
    {
      return ad_fe_coarse_gain_calibration (dev, dev->sensor.optical_res);
    }
  DBGSTART;

  /* setup for a RGB scan, one full sensor's width line */
  /* resolution is the one from the final scan          */
  channels = 3;

  /* we are searching a sensor resolution */
  if (dpi > dev->sensor.optical_res)
    {
      resolution = dev->sensor.optical_res;
    }
  else
    {
      resolution =
	get_closest_resolution (dev->model->ccd_type, dev->settings.xres,
				SANE_TRUE);
    }

  settings.scan_method = dev->settings.scan_method;
  settings.scan_mode = SCAN_MODE_COLOR;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_y = 0;
  if (settings.scan_method == SCAN_METHOD_FLATBED)
    {
      settings.tl_x = 0;
      settings.pixels = (dev->sensor.sensor_pixels * resolution) / dev->sensor.optical_res;
    }
  else
    {
      settings.tl_x = SANE_UNFIX (dev->model->x_offset_ta);
      settings.pixels = (SANE_UNFIX (dev->model->x_size_ta) * resolution) / MM_PER_INCH;
    }
  settings.lines = CALIBRATION_LINES;
  settings.depth = 8;
  settings.color_filter = 0;

  settings.disable_interpolation = 0;
  settings.threshold = 0;
  settings.exposure_time = 0;
  settings.dynamic_lineart = SANE_FALSE;

  /* start gain value */
  dev->frontend.gain[0] = 1;
  dev->frontend.gain[1] = 1;
  dev->frontend.gain[2] = 1;

  if (channels > 1)
    {
      average[0] = 0;
      average[1] = 0;
      average[2] = 0;
      idx = 0;
    }
  else
    {
      average[0] = 255;
      average[1] = 255;
      average[2] = 255;
      idx = dev->settings.color_filter;
      average[idx] = 0;
    }
  pass = 0;

  /* loop until each channel raises to acceptable level */
  while (((average[0] < dev->sensor.gain_white_ref)
	  || (average[1] < dev->sensor.gain_white_ref)
	  || (average[2] < dev->sensor.gain_white_ref)) && (pass < 30))
    {
      /* scan with no move */
      status =
	simple_scan (dev, settings, SANE_FALSE, SANE_TRUE, SANE_FALSE, &line);
      if (status != SANE_STATUS_GOOD)
	{
          free(line);
	  DBG (DBG_error, "%s: failed to scan first line\n", __FUNCTION__);
	  return status;
	}

      /* log scanning data */
      if (DBG_LEVEL >= DBG_data)
	{
	  sprintf (title, "coarse_gain%02d.pnm", (int)pass);
	  sanei_genesys_write_pnm_file (title, line, 8,
					channels, settings.pixels,
					settings.lines);
	}
      pass++;

      /* average high level for each channel and compute gain
         to reach the target code
         we only use the central half of the CCD data         */
      for (k = idx; k < idx + channels; k++)
	{
	  /* we find the maximum white value, so we can deduce a threshold
	     to average white values */
	  maximum = 0;
	  for (i = 0; i < settings.lines; i++)
	    {
	      for (j = 0; j < settings.pixels; j++)
		{
		  val = line[i * channels * settings.pixels + j + k];
		  if (val > maximum)
		    maximum = val;
		}
	    }

	  /* threshold */
	  maximum *= 0.9;

	  /* computes white average */
	  average[k] = 0;
	  count = 0;
	  for (i = 0; i < settings.lines; i++)
	    {
	      for (j = 0; j < settings.pixels; j++)
		{
		  /* averaging only white points allow us not to care about dark margins */
		  val = line[i * channels * settings.pixels + j + k];
		  if (val > maximum)
		    {
		      average[k] += val;
		      count++;
		    }
		}
	    }
	  average[k] = average[k] / count;

	  /* adjusts gain for the channel */
	  if (average[k] < dev->sensor.gain_white_ref)
	    dev->frontend.gain[k]++;

	  DBG (DBG_proc,
	       "%s: channel %d, average = %.2f, gain = %d\n", __FUNCTION__,
	       k, average[k], dev->frontend.gain[k]);
	}
      free (line);
    }

  if (channels < 3)
    {
      dev->frontend.gain[1] = dev->frontend.gain[0];
      dev->frontend.gain[2] = dev->frontend.gain[0];
    }

  DBG (DBG_info, "%s: gains=(%d,%d,%d)\n", __FUNCTION__,
       dev->frontend.gain[0], dev->frontend.gain[1], dev->frontend.gain[2]);
  DBGCOMPLETED;
  return status;
}

/**
 * sets up the scanner's register for warming up. We scan 2 lines without moving.
 *
 */
static SANE_Status
gl646_init_regs_for_warmup (Genesys_Device * dev,
			    Genesys_Register_Set * local_reg,
			    int *channels, int *total_size)
{
  SANE_Status status = SANE_STATUS_GOOD;
  Genesys_Settings settings;
  int resolution, lines;

  DBG (DBG_proc, "gl646_init_regs_for_warmup: start\n");

  sanei_genesys_init_fe (dev);

  resolution = get_closest_resolution (dev->model->ccd_type, 300, SANE_FALSE);

  /* set up for a half width 2 lines gray scan without moving */
  settings.scan_method = SCAN_METHOD_FLATBED;
  settings.scan_mode = SCAN_MODE_GRAY;
  settings.xres = resolution;
  settings.yres = resolution;
  settings.tl_x = 0;
  settings.tl_y = 0;
  settings.pixels =
    (dev->sensor.sensor_pixels * resolution) / dev->sensor.optical_res;
  settings.lines = 2;
  settings.depth = 8;
  settings.color_filter = 0;

  settings.disable_interpolation = 0;
  settings.threshold = 0;
  settings.exposure_time = 0;
  settings.dynamic_lineart = SANE_FALSE;

  /* setup for scan */
  status = setup_for_scan (dev, dev->reg, settings, SANE_TRUE, SANE_FALSE, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_init_regs_for_warmup: setup_for_scan failed (%s)\n",
	   sane_strstatus (status));
      return status;
    }

  /* we are not going to move, so clear these bits */
  dev->reg[reg_0x02].value &= ~(REG02_FASTFED | REG02_AGOHOME);

  /* don't enable any correction for this scan */
  dev->reg[reg_0x01].value &= ~REG01_DVDSET;

  /* copy to local_reg */
  memcpy (local_reg, dev->reg, (GENESYS_GL646_MAX_REGS + 1) * sizeof (Genesys_Register_Set));

  /* turn off motor during this scan */
  gl646_set_motor_power (local_reg, SANE_FALSE);

  /* returned value to higher level warmup function */
  *channels = 1;
  lines = gl646_get_triple_reg (local_reg, REG_LINCNT) + 1;
  *total_size = lines * settings.pixels;

  /* now registers are ok, write them to scanner */
  RIE (gl646_set_fe (dev, AFE_SET, settings.xres));
  RIE (gl646_bulk_write_register (dev, local_reg, GENESYS_GL646_MAX_REGS));

  DBGCOMPLETED;
  return status;
}


/*
 * this function moves head without scanning, forward, then backward
 * so that the head goes to park position.
 * as a by-product, also check for lock
 */
static SANE_Status
gl646_repark_head (Genesys_Device * dev)
{
  SANE_Status status;
  Genesys_Settings settings;
  unsigned int expected, steps;

  DBG (DBG_proc, "gl646_repark_head: start\n");

  settings.scan_method = SCAN_METHOD_FLATBED;
  settings.scan_mode = SCAN_MODE_COLOR;
  settings.xres =
    get_closest_resolution (dev->model->ccd_type, 75, SANE_FALSE);
  settings.yres = settings.xres;
  settings.tl_x = 0;
  settings.tl_y = 5;
  settings.pixels = 600;
  settings.lines = 4;
  settings.depth = 8;
  settings.color_filter = 0;

  settings.disable_interpolation = 0;
  settings.threshold = 0;
  settings.exposure_time = 0;
  settings.dynamic_lineart = SANE_FALSE;

  status = setup_for_scan (dev, dev->reg, settings, SANE_FALSE, SANE_FALSE, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_repark_head: failed to setup for scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* TODO seems wrong ... no effective scan */
  dev->reg[reg_0x01].value &= ~REG01_SCAN;

  status = gl646_bulk_write_register (dev, dev->reg, GENESYS_GL646_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_repark_head: failed to send registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* start scan */
  status = gl646_begin_scan (dev, dev->reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "gl646_repark_head: failed to begin scan: \n");
      return status;
    }

  expected = gl646_get_triple_reg (dev->reg, REG_FEEDL);
  do
    {
      usleep (100 * 1000);
      status = sanei_genesys_read_feed_steps (dev, &steps);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_repark_head: failed to read feed steps: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }
  while (steps < expected);

  /* toggle motor flag, put an huge step number and redo move backward */
  status = gl646_slow_back_home (dev, 1);
  DBG (DBG_proc, "gl646_repark_head: end\n");
  return status;
}

/* *
 * initialize ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 * @param dev device description of the scanner to initailize
 * @return SANE_STATUS_GOOD if success, error code if failure
 */
static SANE_Status
gl646_init (Genesys_Device * dev)
{
  SANE_Status status;
  struct timeval tv;
  uint8_t cold = 0, val = 0;
  uint32_t addr = 0xdead;
  int size, i;
  size_t len;

  DBG_INIT ();
  DBG (DBG_proc, "gl646_init: start\n");

  /* to detect real power up condition, we write to REG41
   * with pwrbit set, then read it back. When scanner is cold (just replugged)
   * PWRBIT will be set in the returned value
   */
  RIE (sanei_genesys_get_status (dev, &cold));
  DBG (DBG_info, "gl646_init: status=0x%02x\n", cold);
  cold = !(cold & REG41_PWRBIT);
  if (cold)
    {
      DBG (DBG_info, "gl646_init: device is cold\n");
    }
  else
    {
      DBG (DBG_info, "gl646_init: device is hot\n");
    }

  /* if scanning session hasn't been initialized, set it up */
  if (!dev->already_initialized)
    {
      dev->dark_average_data = NULL;
      dev->white_average_data = NULL;

      dev->settings.color_filter = 1;	/* green filter by default */
      gettimeofday (&tv, NULL);
      dev->init_date = tv.tv_sec;

      switch (dev->model->motor_type)
	{
	  /* set to 11111 to spot bugs, sanei_genesys_exposure_time should
	     have obsoleted this field  */
	case MOTOR_5345:
	  dev->settings.exposure_time = 11111;
	  break;

	case MOTOR_ST24:
	  dev->settings.exposure_time = 11000;
	  break;
	default:
	  dev->settings.exposure_time = 11000;
	  break;
	}

      /* Set default values for registers */
      gl646_init_regs (dev);

      /* build default gamma tables */
      if (dev->reg[reg_0x05].value & REG05_GMMTYPE)
	size = 16384;
      else
	size = 4096;

      for(i=0;i<3;i++)
        {
          if (dev->sensor.gamma_table[i] == NULL)
            {
              dev->sensor.gamma_table[i] = (uint16_t *) malloc (2 * size);
              if (dev->sensor.gamma_table[i] == NULL)
                {
                  DBG (DBG_error, "gl646_init: could not allocate memory for gamma table %d\n",i);
                  return SANE_STATUS_NO_MEM;
                }
              sanei_genesys_create_gamma_table (dev->sensor.gamma_table[i],
                                                size,
                                                size - 1,
                                                size - 1,
                                                dev->sensor.gamma[i]);
            }
        }

      /* Init shading data */
      RIE (sanei_genesys_init_shading_data (dev, dev->sensor.sensor_pixels));

      /* initial calibration reg values */
      memcpy (dev->calib_reg, dev->reg,
	      (GENESYS_GL646_MAX_REGS + 1) * sizeof (Genesys_Register_Set));
    }

  /* execute physical unit init only if cold */
  if (cold)
    {
      DBG (DBG_info, "gl646_init: device is cold\n");
      val = 0x04;
      RIE (sanei_usb_control_msg
	   (dev->dn, REQUEST_TYPE_OUT, REQUEST_REGISTER, VALUE_INIT,
	    INDEX, 1, &val));

      /* ASIC reset */
      RIE (sanei_genesys_write_register (dev, 0x0e, 0x00));
      usleep (100000UL);	/* sleep 100 ms */

      /* Write initial registers */
      RIE (gl646_bulk_write_register (dev, dev->reg, GENESYS_GL646_MAX_REGS));

      /* Test ASIC and RAM */
      if (!(dev->model->flags & GENESYS_FLAG_LAZY_INIT))
	{
	  RIE (gl646_asic_test (dev));
	}

      /* send gamma tables if needed */
      status = gl646_send_gamma_table (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl646_init: failed to send generic gamma tables: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      /* Set powersaving (default = 15 minutes) */
      RIE (gl646_set_powersaving (dev, 15));
    }				/* end if cold */

  /* Set analog frontend */
  RIE (gl646_set_fe (dev, AFE_INIT, 0));

  /* GPO enabling for XP200 */
  if (dev->model->ccd_type == CIS_XP200)
    {
      sanei_genesys_write_register (dev, 0x68, dev->gpo.enable[0]);
      sanei_genesys_write_register (dev, 0x69, dev->gpo.enable[1]);

      /* enable GPIO */
      val = 6;
      status = gl646_gpio_output_enable (dev->dn, val);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_init: GPO enable failed ... %s\n",
	       sane_strstatus (status));
	}
      val = 0;

      /* writes 0 to GPIO */
      status = gl646_gpio_write (dev->dn, val);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_init: GPO write failed ... %s\n",
	       sane_strstatus (status));
	}

      /* clear GPIO enable */
      status = gl646_gpio_output_enable (dev->dn, val);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_init: GPO disable failed ... %s\n",
	       sane_strstatus (status));
	}
      sanei_genesys_write_register (dev, 0x66, 0x10);
      sanei_genesys_write_register (dev, 0x66, 0x00);
      sanei_genesys_write_register (dev, 0x66, 0x10);
    }

  /* MD6471/G2410 and XP200 read/write data from an undocumented memory area which
   * is after the second slope table */
  if (dev->model->gpo_type != GPO_HP3670
      && dev->model->gpo_type != GPO_HP2400)
    {
      switch (dev->sensor.optical_res)
	{
	case 600:
	  addr = 0x08200;
	  break;
	case 1200:
	  addr = 0x10200;
	  break;
	case 2400:
	  addr = 0x1fa00;
	  break;
	}
      status = sanei_genesys_set_buffer_address (dev, addr);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_init: failed to set up control address\n");
	  return SANE_STATUS_INVAL;
	}
      sanei_usb_set_timeout (2 * 1000);
      len = 6;
      status = gl646_bulk_read_data (dev, 0x45, dev->control, len);
      /* for some reason, read fails here for MD6471, HP2300 and XP200
       * one time out of 2 scanimage launches
       */
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_warn, "gl646_init: failed to read control\n");
	  status = gl646_bulk_read_data (dev, 0x45, dev->control, len);
	}
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_warn, "gl646_init: failed to read control\n");
	  return SANE_STATUS_INVAL;
	}
      else
	{
	  DBG (DBG_info,
	       "gl646_init: control read=0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
	       dev->control[0], dev->control[1], dev->control[2],
	       dev->control[3], dev->control[4], dev->control[5]);
	}
      sanei_usb_set_timeout (30 * 1000);
    }
  else
    /* HP2400 and HP3670 case */
    {
      dev->control[0] = 0x00;
      dev->control[1] = 0x00;
      dev->control[2] = 0x01;
      dev->control[3] = 0x00;
      dev->control[4] = 0x00;
      dev->control[5] = 0x00;
    }

  /* ensure head is correctly parked, and check lock */
  if (dev->model->is_sheetfed == SANE_FALSE)
    {
      if (dev->model->flags & GENESYS_FLAG_REPARK)
	{
	  status = gl646_repark_head (dev);
	  if (status != SANE_STATUS_GOOD)
	    {
	      if (status == SANE_STATUS_INVAL)
		{
		  DBG (DBG_error0,
		       "Your scanner is locked. Please move the lock switch "
		       "to the unlocked position\n");
#ifdef SANE_STATUS_HW_LOCKED
		  return SANE_STATUS_HW_LOCKED;
#else
		  return SANE_STATUS_JAMMED;
#endif
		}
	      else
		DBG (DBG_error,
		     "gl646_init: gl646_repark_head failed: %s\n",
		     sane_strstatus (status));
	      return status;
	    }
	}
      else
	{
	  RIE (gl646_slow_back_home (dev, SANE_TRUE));
	}
    }

  /* here session and device are initialized */
  dev->already_initialized = SANE_TRUE;

  DBG (DBG_proc, "gl646_init: end\n");
  return SANE_STATUS_GOOD;
}

GENESYS_STATIC
SANE_Status
gl646_move_to_ta (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;

  DBGSTART;
  if (simple_move (dev, SANE_UNFIX (dev->model->y_offset_calib_ta)) !=
      SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_move_to_ta: failed to move to calibration area\n");
      return status;
    }
  DBGCOMPLETED;
  return status;
}


/**
 * Does a simple scan: ie no line reordering and avanced data buffering and
 * shading correction. Memory for data is allocated in this function
 * and must be freed by caller.
 * @param dev device of the scanner
 * @param settings parameters of the scan
 * @param move SANE_TRUE if moving during scan
 * @param forward SANE_TRUE if moving forward during scan
 * @param shading SANE_TRUE to enable shading correction
 * @param data pointer for the data
 */
GENESYS_STATIC SANE_Status
simple_scan (Genesys_Device * dev, Genesys_Settings settings, SANE_Bool move,
	     SANE_Bool forward, SANE_Bool shading, unsigned char **data)
{
  SANE_Status status = SANE_STATUS_INVAL;
  unsigned int size, lines, x, y, bpp;
  SANE_Bool empty, split;
  unsigned char *buffer;
  int count;
  uint8_t val;

  DBG (DBG_proc, "simple_scan: starting\n");
  DBG (DBG_io, "simple_scan: move=%d, forward=%d, shading=%d\n", move,
       forward, shading);

  /* round up to multiple of 3 in case of CIS scanner */
  if (dev->model->is_cis == SANE_TRUE)
    {
      settings.lines = ((settings.lines + 2) / 3) * 3;
    }

  /* setup for move then scan */
  if (move == SANE_TRUE && settings.tl_y > 0)
    {
      split = SANE_FALSE;
    }
  else
    {
      split = SANE_TRUE;
    }
  status = setup_for_scan (dev, dev->reg, settings, split, SANE_FALSE, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "simple_scan: setup_for_scan failed (%s)\n",
	   sane_strstatus (status));
      return status;
    }

  /* allocate memory fo scan : LINCNT may have been adjusted for CCD reordering */
  if (dev->model->is_cis == SANE_TRUE)
    {
      lines = gl646_get_triple_reg (dev->reg, REG_LINCNT) / 3;
    }
  else
    {
      lines = gl646_get_triple_reg (dev->reg, REG_LINCNT) + 1;
    }
  size = lines * settings.pixels;
  if (settings.depth == 16)
    bpp = 2;
  else
    bpp = 1;
  size *= bpp;
  if (settings.scan_mode == SCAN_MODE_COLOR)	/* single pass color */
    size *= 3;
  *data = malloc (size);
  if (!*data)
    {
      DBG (DBG_error,
	   "simple_scan: failed to allocate %d bytes of memory\n", size);
      return SANE_STATUS_NO_MEM;
    }
  DBG (DBG_io, "simple_scan: allocated %d bytes of memory for %d lines\n",
       size, lines);

  /* put back real line number in settings */
  settings.lines = lines;

  /* initialize frontend */
  status = gl646_set_fe (dev, AFE_SET, settings.xres);
  if (status != SANE_STATUS_GOOD)
    {
      free (*data);
      DBG (DBG_error,
	   "simple_scan: failed to set frontend: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* no shading correction and not watch dog for simple scan */
  dev->reg[reg_0x01].value &= ~(REG01_DVDSET | REG01_DOGENB);
  if (shading == SANE_TRUE)
    {
      dev->reg[reg_0x01].value |= REG01_DVDSET;
    }

  /* enable gamma table for the scan */
  dev->reg[reg_0x05].value |= REG05_GMMENB;

  /* one table movement for simple scan */
  dev->reg[reg_0x02].value &= ~REG02_FASTFED;

  if (move == SANE_FALSE)
    {
      /* clear motor power flag if no move */
      dev->reg[reg_0x02].value &= ~REG02_MTRPWR;

      /* no automatic go home if no movement */
      dev->reg[reg_0x02].value &= ~REG02_AGOHOME;
    }
  if (forward == SANE_FALSE)
    {
      dev->reg[reg_0x02].value |= REG02_MTRREV;
    }
  else
    {
      dev->reg[reg_0x02].value &= ~REG02_MTRREV;
    }

  /* no automatic go home when using XPA */
  if (settings.scan_method == SCAN_METHOD_TRANSPARENCY)
    {
      dev->reg[reg_0x02].value &= ~REG02_AGOHOME;
    }

  /* write scan registers */
  status = gl646_bulk_write_register (dev, dev->reg,
				      sizeof (dev->reg) /
				      sizeof (dev->reg[0]));
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "simple_scan: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      free (*data);
      return status;
    }

  /* starts scan */
  status = gl646_begin_scan (dev, dev->reg, move);
  if (status != SANE_STATUS_GOOD)
    {
      free (*data);
      DBG (DBG_error, "simple_scan: failed to begin scan: \n");
      return status;
    }

  /* wait for buffers to be filled */
  count = 0;
  do
    {
      usleep (10000UL);
      RIE (sanei_genesys_get_status (dev, &val));
      if (DBG_LEVEL > DBG_info)
	{
	  print_status (val);
	}
      RIE (sanei_genesys_test_buffer_empty (dev, &empty));
      count++;
    }
  while (empty && count < 1000);
  if (count == 1000)
    {
      free (*data);
      DBG (DBG_error, "simple_scan: failed toread data\n");
      return SANE_STATUS_IO_ERROR;
    }

  /* now we're on target, we can read data */
  status = sanei_genesys_read_data_from_scanner (dev, *data, size);
  if (status != SANE_STATUS_GOOD)
    {
      free (*data);
      DBG (DBG_error,
	   "simple_scan: failed to read data: %s\n", sane_strstatus (status));
      return status;
    }

  /* in case of CIS scanner, we must reorder data */
  if (dev->model->is_cis == SANE_TRUE
      && settings.scan_mode == SCAN_MODE_COLOR)
    {
      /* alloc one line sized working buffer */
      buffer = (unsigned char *) malloc (settings.pixels * 3 * bpp);
      if (buffer == NULL)
	{
	  DBG (DBG_error,
	       "simple_scan: failed to allocate %d bytes of memory\n",
	       settings.pixels * 3);
	  return SANE_STATUS_NO_MEM;
	}

      /* reorder one line of data and put it back to buffer */
      if (bpp == 1)
	{
	  for (y = 0; y < lines; y++)
	    {
	      /* reorder line */
	      for (x = 0; x < settings.pixels; x++)
		{
		  buffer[x * 3] = (*data)[y * settings.pixels * 3 + x];
		  buffer[x * 3 + 1] =
		    (*data)[y * settings.pixels * 3 + settings.pixels + x];
		  buffer[x * 3 + 2] =
		    (*data)[y * settings.pixels * 3 + 2 * settings.pixels +
			    x];
		}
	      /* copy line back */
	      memcpy ((*data) + settings.pixels * 3 * y, buffer,
		      settings.pixels * 3);
	    }
	}
      else
	{
	  for (y = 0; y < lines; y++)
	    {
	      /* reorder line */
	      for (x = 0; x < settings.pixels; x++)
		{
		  buffer[x * 6] = (*data)[y * settings.pixels * 6 + x * 2];
		  buffer[x * 6 + 1] =
		    (*data)[y * settings.pixels * 6 + x * 2 + 1];
		  buffer[x * 6 + 2] =
		    (*data)[y * settings.pixels * 6 + 2 * settings.pixels +
			    x * 2];
		  buffer[x * 6 + 3] =
		    (*data)[y * settings.pixels * 6 + 2 * settings.pixels +
			    x * 2 + 1];
		  buffer[x * 6 + 4] =
		    (*data)[y * settings.pixels * 6 + 4 * settings.pixels +
			    x * 2];
		  buffer[x * 6 + 5] =
		    (*data)[y * settings.pixels * 6 + 4 * settings.pixels +
			    x * 2 + 1];
		}
	      /* copy line back */
	      memcpy ((*data) + settings.pixels * 6 * y, buffer,
		      settings.pixels * 6);
	    }
	}
      free (buffer);
    }

  /* end scan , waiting the motor to stop if needed (if moving), but without ejecting doc */
  status = end_scan (dev, dev->reg, SANE_TRUE, SANE_FALSE);
  if (status != SANE_STATUS_GOOD)
    {
      free (*data);
      DBG (DBG_error,
	   "simple_scan: failed to end scan: %s\n", sane_strstatus (status));
      return status;
    }

  DBG (DBG_proc, "simple_scan: end\n");
  return status;
}

/**
 * Does a simple move of the given distance by doing a scan at lowest resolution
 * shading correction. Memory for data is allocated in this function
 * and must be freed by caller.
 * @param dev device of the scanner
 * @param distance distance to move in MM
 */
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
simple_move (Genesys_Device * dev, SANE_Int distance)
{
  SANE_Status status;
  unsigned char *data = NULL;
  Genesys_Settings settings;

  DBG (DBG_proc, "simple_move: %d mm\n", distance);

  /* TODO give a no AGOHOME flag */
  settings.scan_method = SCAN_METHOD_TRANSPARENCY;
  settings.scan_mode = SCAN_MODE_COLOR;
  settings.xres = get_lowest_resolution (dev->model->ccd_type, SANE_TRUE);
  settings.yres = settings.xres;
  settings.tl_y = 0;
  settings.tl_x = 0;
  settings.pixels =
    (dev->sensor.sensor_pixels * settings.xres) / dev->sensor.optical_res;
  settings.lines = (distance * settings.xres) / MM_PER_INCH;
  settings.depth = 8;
  settings.color_filter = 0;

  settings.disable_interpolation = 0;
  settings.threshold = 0;
  settings.exposure_time = 0;
  settings.dynamic_lineart = SANE_FALSE;

  status = simple_scan (dev, settings, SANE_TRUE, SANE_TRUE, SANE_FALSE, &data);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "simple_move: simple_scan failed\n");
    }

  free (data);
  DBGCOMPLETED
  return status;
}

/**
 * update the status of the required sensor in the scanner session
 * the last_val fileds are used to make events 'sticky'
 */
static SANE_Status
gl646_update_hardware_sensors (Genesys_Scanner * session)
{
  Genesys_Device *dev = session->dev;
  uint8_t value;
  SANE_Status status;

  /* do what is needed to get a new set of events, but try to not loose
     any of them.
   */
  status = gl646_gpio_read (dev->dn, &value);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl646_update_hardware_sensors: failed to read GPIO %s\n",
	   sane_strstatus (status));
      return status;
    }
  DBG (DBG_io, "gl646_update_hardware_sensors: GPIO=0x%02x\n", value);

  /* scan button */
  if ((dev->model->buttons & GENESYS_HAS_SCAN_SW)
      && session->val[OPT_SCAN_SW].b == session->last_val[OPT_SCAN_SW].b)
    {
      switch (dev->model->gpo_type)
	{
	case GPO_XP200:
	  session->val[OPT_SCAN_SW].b = ((value & 0x02) != 0);
	  break;
	case GPO_5345:
	  session->val[OPT_SCAN_SW].b = (value == 0x16);
	  break;
	case GPO_HP2300:
	  session->val[OPT_SCAN_SW].b = (value == 0x6c);
	  break;
	case GPO_HP3670:
	case GPO_HP2400:
	  session->val[OPT_SCAN_SW].b = ((value & 0x20) == 0);
	  break;
	default:
	  return SANE_STATUS_UNSUPPORTED;
	}
    }

  /* email button */
  if ((dev->model->buttons & GENESYS_HAS_EMAIL_SW)
      && session->val[OPT_EMAIL_SW].b == session->last_val[OPT_EMAIL_SW].b)
    {
      switch (dev->model->gpo_type)
	{
	case GPO_5345:
	  session->val[OPT_EMAIL_SW].b = (value == 0x12);
	  break;
	case GPO_HP3670:
	case GPO_HP2400:
	  session->val[OPT_EMAIL_SW].b = ((value & 0x08) == 0);
	  break;
	default:
	  return SANE_STATUS_UNSUPPORTED;
	}
    }

  /* copy button */
  if ((dev->model->buttons & GENESYS_HAS_COPY_SW)
      && session->val[OPT_COPY_SW].b == session->last_val[OPT_COPY_SW].b)
    {
      switch (dev->model->gpo_type)
	{
	case GPO_5345:
	  session->val[OPT_COPY_SW].b = (value == 0x11);
	  break;
	case GPO_HP2300:
	  session->val[OPT_COPY_SW].b = (value == 0x5c);
	  break;
	case GPO_HP3670:
	case GPO_HP2400:
	  session->val[OPT_COPY_SW].b = ((value & 0x10) == 0);
	  break;
	default:
	  return SANE_STATUS_UNSUPPORTED;
	}
    }

  /* power button */
  if ((dev->model->buttons & GENESYS_HAS_POWER_SW)
      && session->val[OPT_POWER_SW].b == session->last_val[OPT_POWER_SW].b)
    {
      switch (dev->model->gpo_type)
	{
	case GPO_5345:
	  session->val[OPT_POWER_SW].b = (value == 0x14);
	  break;
	default:
	  return SANE_STATUS_UNSUPPORTED;
	}
    }

  /* ocr button */
  if ((dev->model->buttons & GENESYS_HAS_OCR_SW)
      && session->val[OPT_OCR_SW].b == session->last_val[OPT_OCR_SW].b)
    {
      switch (dev->model->gpo_type)
	{
	case GPO_5345:
	  session->val[OPT_OCR_SW].b = (value == 0x13);
	  break;
	default:
	  return SANE_STATUS_UNSUPPORTED;
	}
    }

  /* document detection */
  if ((dev->model->buttons & GENESYS_HAS_PAGE_LOADED_SW)
      && session->val[OPT_PAGE_LOADED_SW].b ==
      session->last_val[OPT_PAGE_LOADED_SW].b)
    {
      switch (dev->model->gpo_type)
	{
	case GPO_XP200:
	  session->val[OPT_PAGE_LOADED_SW].b = ((value & 0x04) != 0);
	  break;
	default:
	  return SANE_STATUS_UNSUPPORTED;
	}
    }

  /* XPA detection */
  if (dev->model->flags & GENESYS_FLAG_XPA)
    {
      switch (dev->model->gpo_type)
	{
	case GPO_HP3670:
	case GPO_HP2400:
	  /* test if XPA is plugged-in */
	  if ((value & 0x40) == 0)
	    {
	      DBG (DBG_io, "gl646_update_hardware_sensors: enabling XPA\n");
	      session->opt[OPT_SOURCE].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      DBG (DBG_io, "gl646_update_hardware_sensors: disabling XPA\n");
	      session->opt[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;
	    }
	  break;
	default:
	  return SANE_STATUS_UNSUPPORTED;
	}
    }

  return status;
}


static SANE_Status
write_control (Genesys_Device * dev, int resolution)
{
  SANE_Status status;
  uint8_t control[4];
  uint32_t addr = 0xdead;

  /* 2300 does not write to 'control' */
  if (dev->model->motor_type == MOTOR_HP2300)
    return SANE_STATUS_GOOD;

  /* MD6471/G2410/HP2300 and XP200 read/write data from an undocumented memory area which
   * is after the second slope table */
  switch (dev->sensor.optical_res)
    {
    case 600:
      addr = 0x08200;
      break;
    case 1200:
      addr = 0x10200;
      break;
    case 2400:
      addr = 0x1fa00;
      break;
    default:
      DBG (DBG_error, "write_control: failed to compute control address\n");
      return SANE_STATUS_INVAL;
    }

  /* XP200 sets dpi, what other scanner put is unknown yet */
  switch (dev->model->motor_type)
    {
    case MOTOR_XP200:
      /* we put scan's dpi, not motor one */
      control[0] = LOBYTE (resolution);
      control[1] = HIBYTE (resolution);
      control[2] = dev->control[4];
      control[3] = dev->control[5];
      break;
    case MOTOR_HP3670:
    case MOTOR_HP2400:
    case MOTOR_5345:
    default:
      control[0] = dev->control[2];
      control[1] = dev->control[3];
      control[2] = dev->control[4];
      control[3] = dev->control[5];
      break;
    }

  DBG (DBG_info,
       "write_control: control write=0x%02x 0x%02x 0x%02x 0x%02x\n",
       control[0], control[1], control[2], control[3]);
  status = sanei_genesys_set_buffer_address (dev, addr);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "write_control: failed to set up control address\n");
      return SANE_STATUS_INVAL;
    }
  status = gl646_bulk_write_data (dev, 0x3c, control, 4);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "write_control: failed to set up control\n");
      return SANE_STATUS_INVAL;
    }
  return status;
}

/**
 * check if a stored calibration is compatible with requested scan.
 * @return SANE_STATUS_GOOD if compatible, SANE_STATUS_UNSUPPORTED if not.
 * Whenever an error is met, it is returned.
 * @param dev scanner device
 * @param cache cache entry to test
 * @param for_overwrite reserved for future use ...
 */
static SANE_Status
gl646_is_compatible_calibration (Genesys_Device * dev,
				 Genesys_Calibration_Cache * cache,
				 int for_overwrite)
{
#ifdef HAVE_SYS_TIME_H
  struct timeval time;
#endif
  int compatible = 1;

  DBG (DBG_proc,
       "gl646_is_compatible_calibration: start (for_overwrite=%d)\n",
       for_overwrite);

  if (cache == NULL)
    return SANE_STATUS_UNSUPPORTED;

  /* build minimal current_setup for calibration cache use only, it will be better
   * computed when during setup for scan
   */
  if (dev->settings.scan_mode == SCAN_MODE_COLOR)
    {
      dev->current_setup.channels = 3;
    }
  else
    {
      dev->current_setup.channels = 1;
    }
  dev->current_setup.xres = dev->settings.xres;
  dev->current_setup.scan_method = dev->settings.scan_method;

  DBG (DBG_io,
       "gl646_is_compatible_calibration: requested=(%d,%f), tested=(%d,%f)\n",
       dev->current_setup.channels, dev->current_setup.xres,
       cache->used_setup.channels, cache->used_setup.xres);

  /* a calibration cache is compatible if color mode and x dpi match the user
   * requested scan. In the case of CIS scanners, dpi isn't a criteria */
  if (dev->model->is_cis == SANE_FALSE)
    {
      compatible =
	((dev->current_setup.channels == cache->used_setup.channels)
	 && (((int) dev->current_setup.xres) ==
	     ((int) cache->used_setup.xres)));
    }
  else
    {
      compatible =
	(dev->current_setup.channels == cache->used_setup.channels);
    }
  if (dev->current_setup.scan_method != cache->used_setup.scan_method)
    {
      DBG (DBG_io,
	   "gl646_is_compatible_calibration: current method=%d, used=%d\n",
	   dev->current_setup.scan_method, cache->used_setup.scan_method);
      compatible = 0;
    }
  if (!compatible)
    {
      DBG (DBG_proc,
	   "gl646_is_compatible_calibration: completed, non compatible cache\n");
      return SANE_STATUS_UNSUPPORTED;
    }

  /* a cache entry expires after 30 minutes for non sheetfed scanners */
  /* this is not taken into account when overwriting cache entries    */
#ifdef HAVE_SYS_TIME_H
  if(for_overwrite == SANE_FALSE)
    {
      gettimeofday (&time, NULL);
      if ((time.tv_sec - cache->last_calibration > 30 * 60)
          && (dev->model->is_sheetfed == SANE_FALSE))
        {
          DBG (DBG_proc,
               "gl646_is_compatible_calibration: expired entry, non compatible cache\n");
          return SANE_STATUS_UNSUPPORTED;
        }
    }
#endif

  DBG (DBG_proc,
       "gl646_is_compatible_calibration: completed, cache compatible\n");
  return SANE_STATUS_GOOD;
}

/**
 * search for a full width black or white strip.
 * @param dev scanner device
 * @param forward SANE_TRUE if searching forward, SANE_FALSE if searching backward
 * @param black SANE_TRUE if searching for a black strip, SANE_FALSE for a white strip
 * @return SANE_STATUS_GOOD if a matching strip is found, SANE_STATUS_UNSUPPORTED if not
 */
static SANE_Status
gl646_search_strip (Genesys_Device * dev, SANE_Bool forward, SANE_Bool black)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Bool half_ccd = SANE_FALSE;
  Genesys_Settings settings;
  int res = get_closest_resolution (dev->model->ccd_type, 75, SANE_FALSE);
  unsigned char *data = NULL;
  unsigned int pass, count, found, x, y;
  char title[80];

  DBG (DBG_proc, "gl646_search_strip: start\n");
  /* adapt to half_ccd case */
  if (dev->model->flags & GENESYS_FLAG_HALF_CCD_MODE)
    {
      /* walk the master mode list to find if half_ccd */
      if (is_half_ccd (dev->model->ccd_type, res, SANE_TRUE) == SANE_TRUE)
	{
	  half_ccd = SANE_TRUE;
	}
    }

  /* we set up for a lowest available resolution color grey scan, full width */
  settings.scan_method = SCAN_METHOD_FLATBED;
  settings.scan_mode = SCAN_MODE_GRAY;
  settings.xres = res;
  settings.yres = res;
  settings.tl_x = 0;
  settings.tl_y = 0;
  settings.pixels = (SANE_UNFIX (dev->model->x_size) * res) / MM_PER_INCH;
  if (half_ccd == SANE_TRUE)
    {
      settings.pixels /= 2;
    }

  /* 15 mm at at time */
  settings.lines = (15 * settings.yres) / MM_PER_INCH;	/* may become a parameter from genesys_devices.c */
  settings.depth = 8;
  settings.color_filter = 0;

  settings.disable_interpolation = 0;
  settings.threshold = 0;
  settings.exposure_time = 0;
  settings.dynamic_lineart = SANE_FALSE;

  /* signals if a strip of the given color has been found */
  found = 0;

  /* detection pass done */
  pass = 0;

  /* loop until strip is found or maximum pass number done */
  while (pass < 20 && !found)
    {
      /* scan a full width strip */
      status =
	simple_scan (dev, settings, SANE_TRUE, forward, SANE_FALSE, &data);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "gl646_search_strip: simple_scan failed\n");
	  free (data);
	  return status;
	}
      if (DBG_LEVEL >= DBG_data)
	{
	  sprintf (title, "search_strip_%s%02d.pnm", forward ? "fwd" : "bwd",
		   (int)pass);
	  sanei_genesys_write_pnm_file (title, data, settings.depth, 1,
					settings.pixels, settings.lines);
	}

      /* search data to find black strip */
      /* when searching forward, we only need one line of the searched color since we
       * will scan forward. But when doing backward search, we need all the area of the
       * same color */
      if (forward)
	{
	  for (y = 0; y < settings.lines && !found; y++)
	    {
	      count = 0;
	      /* count of white/black pixels depending on the color searched */
	      for (x = 0; x < settings.pixels; x++)
		{
		  /* when searching for black, detect white pixels */
		  if (black && data[y * settings.pixels + x] > 90)
		    {
		      count++;
		    }
		  /* when searching for white, detect black pixels */
		  if (!black && data[y * settings.pixels + x] < 60)
		    {
		      count++;
		    }
		}

	      /* at end of line, if count >= 3%, line is not fully of the desired color
	       * so we must go to next line of the buffer */
	      /* count*100/pixels < 3 */
	      if ((count * 100) / settings.pixels < 3)
		{
		  found = 1;
		  DBG (DBG_data,
		       "gl646_search_strip: strip found forward during pass %d at line %d\n",
		       pass, y);
		}
	      else
		{
		  DBG (DBG_data, "gl646_search_strip: pixels=%d, count=%d\n",
		       settings.pixels, count);
		}
	    }
	}
      else			/* since calibration scans are done forward, we need the whole area
				   to be of the required color when searching backward */
	{
	  count = 0;
	  for (y = 0; y < settings.lines; y++)
	    {
	      /* count of white/black pixels depending on the color searched */
	      for (x = 0; x < settings.pixels; x++)
		{
		  /* when searching for black, detect white pixels */
		  if (black && data[y * settings.pixels + x] > 60)
		    {
		      count++;
		    }
		  /* when searching for white, detect black pixels */
		  if (!black && data[y * settings.pixels + x] < 60)
		    {
		      count++;
		    }
		}
	    }

	  /* at end of area, if count >= 3%, area is not fully of the desired color
	   * so we must go to next buffer */
	  if ((count * 100) / (settings.pixels * settings.lines) < 3)
	    {
	      found = 1;
	      DBG (DBG_data,
		   "gl646_search_strip: strip found backward during pass %d \n",
		   pass);
	    }
	  else
	    {
	      DBG (DBG_data, "gl646_search_strip: pixels=%d, count=%d\n",
		   settings.pixels, count);
	    }
	}
      pass++;
    }
  free (data);
  if (found)
    {
      status = SANE_STATUS_GOOD;
      DBG (DBG_info, "gl646_search_strip: strip found\n");
    }
  else
    {
      status = SANE_STATUS_UNSUPPORTED;
      DBG (DBG_info, "gl646_search_strip: strip not found\n");
    }
  return status;
}

/** the gl646 command set */
static Genesys_Command_Set gl646_cmd_set = {
  "gl646-generic",		/* the name of this set */

  gl646_init,
  gl646_init_regs_for_warmup,
  gl646_init_regs_for_coarse_calibration,
  gl646_init_regs_for_shading,
  gl646_init_regs_for_scan,

  gl646_get_filter_bit,
  gl646_get_lineart_bit,
  gl646_get_bitset_bit,
  gl646_get_gain4_bit,
  gl646_get_fast_feed_bit,
  gl646_test_buffer_empty_bit,
  gl646_test_motor_flag_bit,

  gl646_bulk_full_size,

  gl646_public_set_fe,
  gl646_set_powersaving,
  gl646_save_power,
  gl646_set_motor_power,
  gl646_set_lamp_power,

  gl646_begin_scan,
  gl646_end_scan,

  gl646_send_gamma_table,

  gl646_search_start_position,

  gl646_offset_calibration,
  gl646_coarse_gain_calibration,
  gl646_led_calibration,

  gl646_slow_back_home,

  gl646_bulk_write_register,
  gl646_bulk_write_data,
  gl646_bulk_read_data,

  gl646_update_hardware_sensors,

  /* sheetfed related functions */
  gl646_load_document,
  gl646_detect_document_end,
  gl646_eject_document,
  gl646_search_strip,

  gl646_is_compatible_calibration,
  gl646_move_to_ta,
  NULL,
  NULL,
  NULL,
  NULL
};

SANE_Status
sanei_gl646_init_cmd_set (Genesys_Device * dev)
{
  dev->model->cmd_set = &gl646_cmd_set;
  return SANE_STATUS_GOOD;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
