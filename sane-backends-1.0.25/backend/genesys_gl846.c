/* sane - Scanner Access Now Easy.

   Copyright (C) 2012-2013 Stéphane Voltz <stef.dev@free.fr>


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

/** @file
 *
 * This file handles GL846 and GL845 ASICs since they are really close to each other.
 */
#undef BACKEND_NAME
#define BACKEND_NAME genesys_gl846

#include "genesys_gl846.h"

/****************************************************************************
 Low level function
 ****************************************************************************/

/* ------------------------------------------------------------------------ */
/*                  Read and write RAM, registers and AFE                   */
/* ------------------------------------------------------------------------ */


/** @brief read scanned data
 * Read in 0xeff0 maximum sized blocks. This read is done in 2
 * parts if not multple of 512. First read is rounded to a multiple of 512 bytes, last read fetches the
 * remainder. Read addr is always 0x10000000 with the memory layout setup.
 * @param dev device to read data from
 * @param addr address within ASIC memory space, unused but kept for API
 * @param data pointer where to store the read data
 * @param len size to read
 */
static SANE_Status
gl846_bulk_read_data (Genesys_Device * dev, uint8_t addr,
		      uint8_t * data, size_t len)
{
  SANE_Status status;
  size_t size, target, read, done;
  uint8_t outdata[8];
  uint8_t *buffer;

  DBG (DBG_io, "gl846_bulk_read_data: requesting %lu bytes at addr=0x%02x\n", (u_long) len, addr);

  if (len == 0)
    return SANE_STATUS_GOOD;

  target = len;
  buffer = data;

  /* loop until computed data size is read */
  while (target)
    {
      if (target > 0xeff0)
	{
	  size = 0xeff0;
	}
      else
	{
	  size = target;
	}

      /* hard coded 0x10000000 addr */
      outdata[0] = 0;
      outdata[1] = 0;
      outdata[2] = 0;
      outdata[3] = 0x10;

      /* data size to transfer */
      outdata[4] = (size & 0xff);
      outdata[5] = ((size >> 8) & 0xff);
      outdata[6] = ((size >> 16) & 0xff);
      outdata[7] = ((size >> 24) & 0xff);

      status =
	sanei_usb_control_msg (dev->dn, REQUEST_TYPE_OUT, REQUEST_BUFFER,
			       VALUE_BUFFER, 0x00, sizeof (outdata),
			       outdata);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "%s failed while writing command: %s\n",
	       __FUNCTION__, sane_strstatus (status));
	  return status;
	}

      /* blocks must be multiple of 512 but not last block */
      read = size;
      if (read >= 512)
	{
	  read /= 512;
	  read *= 512;
	}

      DBG (DBG_io2,
	   "gl846_bulk_read_data: trying to read %lu bytes of data\n",
	   (u_long) read);
      status = sanei_usb_read_bulk (dev->dn, buffer, &read);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl846_bulk_read_data failed while reading bulk data: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      done=read;
      DBG (DBG_io2, "gl846_bulk_read_data: %lu bytes of data read\n", (u_long) done);

      /* read less than 512 bytes remainder */
      if (read < size)
	{
	  read = size - read;
	  DBG (DBG_io2,
	       "gl846_bulk_read_data: trying to read %lu bytes of data\n",
	       (u_long) read);
	  status = sanei_usb_read_bulk (dev->dn, buffer+done, &read);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl846_bulk_read_data failed while reading bulk data: %s\n",
		   sane_strstatus (status));
	      return status;
	    }
          done=read;
          DBG (DBG_io2, "gl846_bulk_read_data: %lu bytes of data read\n", (u_long) done);
	}

      DBG (DBG_io2, "%s: read %lu bytes, %lu remaining\n", __FUNCTION__,
	   (u_long) size, (u_long) (target - size));

      target -= size;
      buffer += size;
    }

  if (DBG_LEVEL >= DBG_data && dev->binary!=NULL)
    {
      fwrite(data, len, 1, dev->binary);
    }

  DBGCOMPLETED;

  return SANE_STATUS_GOOD;
}

/****************************************************************************
 Mid level functions
 ****************************************************************************/

static SANE_Bool
gl846_get_fast_feed_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, REG02);
  if (r && (r->value & REG02_FASTFED))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl846_get_filter_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, REG04);
  if (r && (r->value & REG04_FILTER))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl846_get_lineart_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, REG04);
  if (r && (r->value & REG04_LINEART))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl846_get_bitset_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, REG04);
  if (r && (r->value & REG04_BITSET))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl846_get_gain4_bit (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;

  r = sanei_genesys_get_address (regs, 0x06);
  if (r && (r->value & REG06_GAIN4))
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl846_test_buffer_empty_bit (SANE_Byte val)
{
  if (val & REG41_BUFEMPTY)
    return SANE_TRUE;
  return SANE_FALSE;
}

static SANE_Bool
gl846_test_motor_flag_bit (SANE_Byte val)
{
  if (val & REG41_MOTORENB)
    return SANE_TRUE;
  return SANE_FALSE;
}

/**
 * compute the step multiplier used
 */
static int
gl846_get_step_multiplier (Genesys_Register_Set * regs)
{
  Genesys_Register_Set *r = NULL;
  int value = 1;

  r = sanei_genesys_get_address (regs, 0x9d);
  if (r != NULL)
    {
      value = (r->value & 0x0f)>>1;
      value = 1 << value;
    }
  DBG (DBG_io, "%s: step multiplier is %d\n", __FUNCTION__, value);
  return value;
}

/** @brief sensor profile
 * search for the database of motor profiles and get the best one. Each
 * profile is at a specific dpihw. Use LiDE 110 table by default.
 * @param sensor_type sensor id
 * @param dpi hardware dpi for the scan
 * @return a pointer to a Sensor_Profile struct
 */
static Sensor_Profile *get_sensor_profile(int sensor_type, int dpi)
{
  unsigned int i;
  int idx;

  i=0;
  idx=-1;
  while(i<sizeof(sensors)/sizeof(Sensor_Profile))
    {
      /* exact match */
      if(sensors[i].sensor_type==sensor_type && sensors[i].dpi==dpi)
        {
          return &(sensors[i]);
        }

      /* closest match */
      if(sensors[i].sensor_type==sensor_type)
        {
          if(idx<0)
            {
              idx=i;
            }
          else
            {
              if(sensors[i].dpi>=dpi
              && sensors[i].dpi<sensors[idx].dpi)
                {
                  idx=i;
                }
            }
        }
      i++;
    }

  /* default fallback */
  if(idx<0)
    {
      DBG (DBG_warn,"%s: using default sensor profile\n",__FUNCTION__);
      idx=0;
    }

  return &(sensors[idx]);
}

/**@brief compute exposure to use
 * compute the sensor exposure based on target resolution
 */
static int gl846_compute_exposure(Genesys_Device *dev, int xres)
{
  Sensor_Profile *sensor;

  sensor=get_sensor_profile(dev->model->ccd_type, xres);
  return sensor->exposure;
}


/** @brief sensor specific settings
*/
static void
gl846_setup_sensor (Genesys_Device * dev, Genesys_Register_Set * regs, int dpi)
{
  Genesys_Register_Set *r;
  Sensor_Profile *sensor;
  int dpihw, i;
  uint16_t exp;

  DBGSTART;
  dpihw=sanei_genesys_compute_dpihw(dev,dpi);

  for (i = 0x06; i < 0x0e; i++)
    {
      r = sanei_genesys_get_address (regs, 0x10 + i);
      if (r)
	r->value = dev->sensor.regs_0x10_0x1d[i];
    }

  for (i = 0; i < 9; i++)
    {
      r = sanei_genesys_get_address (regs, 0x52 + i);
      if (r)
	r->value = dev->sensor.regs_0x52_0x5e[i];
    }

  /* set EXPDUMMY and CKxMAP */
  dpihw=sanei_genesys_compute_dpihw(dev,dpi);
  sensor=get_sensor_profile(dev->model->ccd_type, dpihw);

  sanei_genesys_set_reg_from_set(regs,REG_EXPDMY,(uint8_t)((sensor->expdummy) & 0xff));

  /* if no calibration has been done, set default values for exposures */
  exp=dev->sensor.regs_0x10_0x1d[0]*256+dev->sensor.regs_0x10_0x1d[1];
  if(exp==0)
    {
      exp=sensor->expr;
    }
  sanei_genesys_set_double(regs,REG_EXPR,exp);

  exp=dev->sensor.regs_0x10_0x1d[2]*256+dev->sensor.regs_0x10_0x1d[3];
  if(exp==0)
    {
      exp=sensor->expg;
    }
  sanei_genesys_set_double(regs,REG_EXPG,exp);

  exp=dev->sensor.regs_0x10_0x1d[4]*256+dev->sensor.regs_0x10_0x1d[5];
  if(exp==0)
    {
      exp=sensor->expb;
    }
  sanei_genesys_set_double(regs,REG_EXPB,exp);

  sanei_genesys_set_triple(regs,REG_CK1MAP,sensor->ck1map);
  sanei_genesys_set_triple(regs,REG_CK3MAP,sensor->ck3map);
  sanei_genesys_set_triple(regs,REG_CK4MAP,sensor->ck4map);

  /* order of the sub-segments */
  dev->order=sensor->order;

  r = sanei_genesys_get_address (regs, 0x17);
  r->value = sensor->r17;

  DBGCOMPLETED;
}


/* returns the max register bulk size */
static int
gl846_bulk_full_size (void)
{
  return GENESYS_GL846_MAX_REGS;
}

/** @brief set all registers to default values .
 * This function is called only once at the beginning and
 * fills register startup values for registers reused across scans.
 * Those that are rarely modified or not modified are written
 * individually.
 * @param dev device structure holding register set to initialize
 */
static void
gl846_init_registers (Genesys_Device * dev)
{
  DBGSTART;

  memset (dev->reg, 0,
	  GENESYS_GL846_MAX_REGS * sizeof (Genesys_Register_Set));

  SETREG (0x01,0x60);
  SETREG (0x02,0x38);
  SETREG (0x03,0x03);
  SETREG (0x04,0x22);
  SETREG (0x05,0x60);
  SETREG (0x06,0x10);
  SETREG (0x08,0x60);
  SETREG (0x09,0x00);
  SETREG (0x0a,0x00);
  SETREG (0x0b,0x8b);
  SETREG (0x0c,0x00);
  SETREG (0x0d,0x00);
  SETREG (0x10,0x00);
  SETREG (0x11,0x00);
  SETREG (0x12,0x00);
  SETREG (0x13,0x00);
  SETREG (0x14,0x00);
  SETREG (0x15,0x00);
  SETREG (0x16,0xbb);
  SETREG (0x17,0x13);
  SETREG (0x18,0x10);
  SETREG (0x19,0x2a);
  SETREG (0x1a,0x34);
  SETREG (0x1b,0x00);
  SETREG (0x1c,0x20);
  SETREG (0x1d,0x06);
  SETREG (0x1e,0xf0);
  SETREG (0x1f,0x01);
  SETREG (0x20,0x03);
  SETREG (0x21,0x10);
  SETREG (0x22,0x60);
  SETREG (0x23,0x60);
  SETREG (0x24,0x60);
  SETREG (0x25,0x00);
  SETREG (0x26,0x00);
  SETREG (0x27,0x00);
  SETREG (0x2c,0x00);
  SETREG (0x2d,0x00);
  SETREG (0x2e,0x80);
  SETREG (0x2f,0x80);
  SETREG (0x30,0x00);
  SETREG (0x31,0x00);
  SETREG (0x32,0x00);
  SETREG (0x33,0x00);
  SETREG (0x34,0x1f);
  SETREG (0x35,0x00);
  SETREG (0x36,0x40);
  SETREG (0x37,0x00);
  SETREG (0x38,0x2a);
  SETREG (0x39,0xf8);
  SETREG (0x3d,0x00);
  SETREG (0x3e,0x00);
  SETREG (0x3f,0x01);
  SETREG (0x52,0x02);
  SETREG (0x53,0x04);
  SETREG (0x54,0x06);
  SETREG (0x55,0x08);
  SETREG (0x56,0x0a);
  SETREG (0x57,0x00);
  SETREG (0x58,0x59);
  SETREG (0x59,0x31);
  SETREG (0x5a,0x40);
  SETREG (0x5e,0x1f);
  SETREG (0x5f,0x01);
  SETREG (0x60,0x00);
  SETREG (0x61,0x00);
  SETREG (0x62,0x00);
  SETREG (0x63,0x00);
  SETREG (0x64,0x00);
  SETREG (0x65,0x00);
  SETREG (0x67,0x7f);
  SETREG (0x68,0x7f);
  SETREG (0x69,0x01);
  SETREG (0x6a,0x01);
  SETREG (0x70,0x01);
  SETREG (0x71,0x00);
  SETREG (0x72,0x02);
  SETREG (0x73,0x01);
  SETREG (0x74,0x00);
  SETREG (0x75,0x00);
  SETREG (0x76,0x00);
  SETREG (0x77,0x00);
  SETREG (0x78,0x00);
  SETREG (0x79,0x3f);
  SETREG (0x7a,0x00);
  SETREG (0x7b,0x09);
  SETREG (0x7c,0x99);
  SETREG (0x7d,0x20);
  SETREG (0x7f,0x05);
  SETREG (0x80,0x4f);
  SETREG (0x87,0x02);
  SETREG (0x94,0xff);
  SETREG (0x9d,0x04);
  SETREG (0x9e,0x00);
  SETREG (0xa1,0xe0);
  SETREG (0xa2,0x1f);
  SETREG (0xab,0xc0);
  SETREG (0xbb,0x00);
  SETREG (0xbc,0x0f);
  SETREG (0xdb,0xff);
  SETREG (0xfe,0x08);
  SETREG (0xff,0x02);
  SETREG (0x98,0x20);
  SETREG (0x99,0x00);
  SETREG (0x9a,0x90);
  SETREG (0x9b,0x00);
  SETREG (0xf8,0x05);

  /* fine tune upon device description */
  dev->reg[reg_0x05].value &= ~REG05_DPIHW;
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
    case 4800:
      dev->reg[reg_0x05].value |= REG05_DPIHW_4800;
      break;
    }

  /* initalize calibration reg */
  memcpy (dev->calib_reg, dev->reg,
	  GENESYS_GL846_MAX_REGS * sizeof (Genesys_Register_Set));

  DBGCOMPLETED;
}

/**@brief send slope table for motor movement
 * Send slope_table in machine byte order
 * @param dev device to send slope table
 * @param table_nr index of the slope table in ASIC memory
 * Must be in the [0-4] range.
 * @param slope_table pointer to 16 bit values array of the slope table
 * @param steps number of elements in the slope table
 */
static SANE_Status
gl846_send_slope_table (Genesys_Device * dev, int table_nr,
			uint16_t * slope_table, int steps)
{
  SANE_Status status;
  uint8_t *table;
  int i;
  char msg[10000];

  DBG (DBG_proc, "%s (table_nr = %d, steps = %d)\n", __FUNCTION__,
       table_nr, steps);

  /* sanity check */
  if(table_nr<0 || table_nr>4)
    {
      DBG (DBG_error, "%s: invalid table number %d!\n", __FUNCTION__, table_nr);
      return SANE_STATUS_INVAL;
    }

  table = (uint8_t *) malloc (steps * 2);
  for (i = 0; i < steps; i++)
    {
      table[i * 2] = slope_table[i] & 0xff;
      table[i * 2 + 1] = slope_table[i] >> 8;
    }

  if (DBG_LEVEL >= DBG_io)
    {
      sprintf (msg, "write slope %d (%d)=", table_nr, steps);
      for (i = 0; i < steps; i++)
	{
	  sprintf (msg+strlen(msg), "%d", slope_table[i]);
	}
      DBG (DBG_io, "%s: %s\n", __FUNCTION__, msg);
    }

  /* slope table addresses are fixed */
  status = sanei_genesys_write_ahb (dev->dn, dev->usb_mode, 0x10000000 + 0x4000 * table_nr, steps * 2, table);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "%s: write to AHB failed writing slope table %d (%s)\n",
	   __FUNCTION__, table_nr, sane_strstatus (status));
    }

  free (table);
  DBGCOMPLETED;
  return status;
}

/**
 * Set register values of Analog Device type frontend
 * */
static SANE_Status
gl846_set_adi_fe (Genesys_Device * dev, uint8_t set)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int i;
  uint16_t val;
  uint8_t val8;

  DBGSTART;

  /* wait for FE to be ready */
  status = sanei_genesys_get_status (dev, &val8);
  while (val8 & REG41_FEBUSY)
    {
      usleep (10000);
      status = sanei_genesys_get_status (dev, &val8);
    };

  if (set == AFE_INIT)
    {
      DBG (DBG_proc, "%s(): setting DAC %u\n", __FUNCTION__, dev->model->dac_type);

      /* sets to default values */
      sanei_genesys_init_fe (dev);
    }

  /* write them to analog frontend */
  val = dev->frontend.reg[0];
  status = sanei_genesys_fe_write_data (dev, 0x00, val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to write reg0: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      return status;
    }
  val = dev->frontend.reg[1];
  status = sanei_genesys_fe_write_data (dev, 0x01, val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to write reg1: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      return status;
    }

  for (i = 0; i < 3; i++)
    {
      val = dev->frontend.gain[i];
      status = sanei_genesys_fe_write_data (dev, 0x02 + i, val);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "%s: failed to write gain %d: %s\n", __FUNCTION__, i,
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
	       "%s: failed to write offset %d: %s\n", __FUNCTION__, i,
	       sane_strstatus (status));
	  return status;
	}
    }

  DBGCOMPLETED;
  return status;
}

static SANE_Status
gl846_homsnr_gpio(Genesys_Device *dev)
{
uint8_t val;
SANE_Status status=SANE_STATUS_GOOD;

  RIE (sanei_genesys_read_register (dev, REG6C, &val));
  val |= 0x41;
  RIE (sanei_genesys_write_register (dev, REG6C, val));

  return status;
}

/* Set values of analog frontend */
static SANE_Status
gl846_set_fe (Genesys_Device * dev, uint8_t set)
{
  SANE_Status status;

  DBG(DBG_proc, "gl846_set_fe (%s)\n",
      set == AFE_INIT ? "init" : set == AFE_SET ? "set" :
      set == AFE_POWER_SAVE ? "powersave" : "huh?");

  /* route to specific analog frontend setup */
  switch (dev->reg[reg_0x04].value & REG04_FESET)
    {
      case 0x02: /* ADI FE */
        status = gl846_set_adi_fe(dev, set);
        break;
      default:
        DBG(DBG_proc, "gl846_set_fe(): unsupported frontend type %d\n",
            dev->reg[reg_0x04].value & REG04_FESET);
        status = SANE_STATUS_UNSUPPORTED;
    }

  DBGCOMPLETED;
  return status;
}


/** @brief set up motor related register for scan
 */
static SANE_Status
gl846_init_motor_regs_scan (Genesys_Device * dev,
                            Genesys_Register_Set * reg,
                            unsigned int scan_exposure_time,
			    float scan_yres,
			    int scan_step_type,
			    unsigned int scan_lines,
			    unsigned int scan_dummy,
			    unsigned int feed_steps,
			    int scan_power_mode,
                            unsigned int flags)
{
  SANE_Status status;
  int use_fast_fed;
  unsigned int fast_dpi;
  uint16_t scan_table[SLOPE_TABLE_SIZE];
  uint16_t fast_table[SLOPE_TABLE_SIZE];
  int scan_steps, fast_steps, factor;
  unsigned int feedl, dist;
  Genesys_Register_Set *r;
  uint32_t z1, z2;
  unsigned int min_restep = 0x20;
  uint8_t val;
  int fast_step_type;
  unsigned int ccdlmt,tgtime;

  DBGSTART;
  DBG (DBG_proc, "gl846_init_motor_regs_scan : scan_exposure_time=%d, "
       "scan_yres=%g, scan_step_type=%d, scan_lines=%d, scan_dummy=%d, "
       "feed_steps=%d, scan_power_mode=%d, flags=%x\n",
       scan_exposure_time,
       scan_yres,
       scan_step_type,
       scan_lines, scan_dummy, feed_steps, scan_power_mode, flags);

  /* get step multiplier */
  factor = gl846_get_step_multiplier (reg);

  use_fast_fed=0;
  /* no fast fed since feed works well */
  if(dev->settings.yres==4444 && feed_steps>100
     && ((flags & MOTOR_FLAG_FEED)==0))
    {
      use_fast_fed=1;
    }
  DBG (DBG_io, "%s: use_fast_fed=%d\n", __FUNCTION__, use_fast_fed);

  sanei_genesys_set_triple(reg, REG_LINCNT, scan_lines);
  DBG (DBG_io, "%s: lincnt=%d\n", __FUNCTION__, scan_lines);

  /* compute register 02 value */
  r = sanei_genesys_get_address (reg, REG02);
  r->value = 0x00;
  r->value |= REG02_MTRPWR;

  if (use_fast_fed)
    r->value |= REG02_FASTFED;
  else
    r->value &= ~REG02_FASTFED;

  if (flags & MOTOR_FLAG_AUTO_GO_HOME)
    r->value |= REG02_AGOHOME | REG02_NOTHOME;

  if ((flags & MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE)
      ||(scan_yres>=dev->sensor.optical_res))
    {
      r->value |= REG02_ACDCDIS;
    }

  /* scan and backtracking slope table */
  sanei_genesys_slope_table(scan_table,
                            &scan_steps,
                            scan_yres,
                            scan_exposure_time,
                            dev->motor.base_ydpi,
                            scan_step_type,
                            factor,
                            dev->model->motor_type,
                            gl846_motors);
  RIE(gl846_send_slope_table (dev, SCAN_TABLE, scan_table, scan_steps*factor));
  RIE(gl846_send_slope_table (dev, BACKTRACK_TABLE, scan_table, scan_steps*factor));

  /* fast table */
  fast_dpi=sanei_genesys_get_lowest_ydpi(dev);
  fast_step_type=scan_step_type;
  if(scan_step_type>=2)
    {
      fast_step_type=2;
    }

  sanei_genesys_slope_table(fast_table,
                            &fast_steps,
                            fast_dpi,
                            scan_exposure_time,
                            dev->motor.base_ydpi,
                            fast_step_type,
                            factor,
                            dev->model->motor_type,
                            gl846_motors);

  /* manual override of high start value */
  fast_table[0]=fast_table[1];

  RIE(gl846_send_slope_table (dev, STOP_TABLE, fast_table, fast_steps*factor));
  RIE(gl846_send_slope_table (dev, FAST_TABLE, fast_table, fast_steps*factor));
  RIE(gl846_send_slope_table (dev, HOME_TABLE, fast_table, fast_steps*factor));

  /* correct move distance by acceleration and deceleration amounts */
  feedl=feed_steps;
  if (use_fast_fed)
    {
        feedl<<=fast_step_type;
        dist=(scan_steps+2*fast_steps)*factor;
        /* TODO read and decode REGAB */
        r = sanei_genesys_get_address (reg, 0x5e);
        dist += (r->value & 31);
        /* FEDCNT */
        r = sanei_genesys_get_address (reg, REG_FEDCNT);
        dist += r->value;
    }
  else
    {
      feedl<<=scan_step_type;
      dist=scan_steps*factor;
      if (flags & MOTOR_FLAG_FEED)
        dist *=2;
    }
  DBG (DBG_io2, "%s: scan steps=%d\n", __FUNCTION__, scan_steps);
  DBG (DBG_io2, "%s: acceleration distance=%d\n", __FUNCTION__, dist);

  /* check for overflow */
  if(dist<feedl)
    feedl -= dist;
  else
    feedl = 0;

  sanei_genesys_set_triple(reg,REG_FEEDL,feedl);
  DBG (DBG_io ,"%s: feedl=%d\n",__FUNCTION__,feedl);

  r = sanei_genesys_get_address (reg, REG0C);
  ccdlmt=(r->value & REG0C_CCDLMT)+1;

  r = sanei_genesys_get_address (reg, REG1C);
  tgtime=1<<(r->value & REG1C_TGTIME);

  /* hi res motor speed GPIO */
  /*
  RIE (sanei_genesys_read_register (dev, REG6C, &effective));
  */

  /* if quarter step, bipolar Vref2 */
  /* XXX STEF XXX GPIO
  if (scan_step_type > 1)
    {
      if (scan_step_type < 3)
        {
          val = effective & ~REG6C_GPIO13;
        }
      else
        {
          val = effective | REG6C_GPIO13;
        }
    }
  else
    {
      val = effective;
    }
  RIE (sanei_genesys_write_register (dev, REG6C, val));
    */

  /* effective scan */
  /*
  RIE (sanei_genesys_read_register (dev, REG6C, &effective));
  val = effective | REG6C_GPIO10;
  RIE (sanei_genesys_write_register (dev, REG6C, val));
  */

  if(dev->model->gpo_type==GPO_IMG101)
    {
      if(scan_yres==sanei_genesys_compute_dpihw(dev,scan_yres))
        {
          val=1;
        }
      else
        {
          val=0;
        }
      RIE (sanei_genesys_write_register (dev, REG7E, val));
    }

  min_restep=scan_steps/2-1;
  if (min_restep < 1)
    min_restep = 1;
  r = sanei_genesys_get_address (reg, REG_FWDSTEP);
  r->value = min_restep;
  r = sanei_genesys_get_address (reg, REG_BWDSTEP);
  r->value = min_restep;

  sanei_genesys_calculate_zmode2(use_fast_fed,
			         scan_exposure_time*ccdlmt*tgtime,
				 scan_table,
				 scan_steps*factor,
				 feedl,
                                 min_restep*factor,
                                 &z1,
                                 &z2);

  DBG (DBG_info, "gl846_init_motor_regs_scan: z1 = %d\n", z1);
  sanei_genesys_set_triple(reg, REG60, z1 | (scan_step_type << (16+REG60S_STEPSEL)));

  DBG (DBG_info, "gl846_init_motor_regs_scan: z2 = %d\n", z2);
  sanei_genesys_set_triple(reg, REG63, z2 | (scan_step_type << (16+REG63S_FSTPSEL)));

  r = sanei_genesys_get_address (reg, 0x1e);
  r->value &= 0xf0;		/* 0 dummy lines */
  r->value |= scan_dummy;	/* dummy lines */

  r = sanei_genesys_get_address (reg, REG67);
  r->value = 0x7f;

  r = sanei_genesys_get_address (reg, REG68);
  r->value = 0x7f;

  r = sanei_genesys_get_address (reg, REG_STEPNO);
  r->value = scan_steps;

  r = sanei_genesys_get_address (reg, REG_FASTNO);
  r->value = scan_steps;

  r = sanei_genesys_get_address (reg, REG_FSHDEC);
  r->value = scan_steps;

  r = sanei_genesys_get_address (reg, REG_FMOVNO);
  r->value = fast_steps;

  r = sanei_genesys_get_address (reg, REG_FMOVDEC);
  r->value = fast_steps;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}


/** @brief set up registers related to sensor
 * Set up the following registers
   0x01
   0x03
   0x10-0x015     R/G/B exposures
   0x19           EXPDMY
   0x2e           BWHI
   0x2f           BWLO
   0x04
   0x87
   0x05
   0x2c,0x2d      DPISET
   0x30,0x31      STRPIXEL
   0x32,0x33      ENDPIXEL
   0x35,0x36,0x37 MAXWD [25:2] (>>2)
   0x38,0x39      LPERIOD
   0x34           DUMMY
 */
static SANE_Status
gl846_init_optical_regs_scan (Genesys_Device * dev,
			      Genesys_Register_Set * reg,
			      unsigned int exposure_time,
			      int used_res,
			      unsigned int start,
			      unsigned int pixels,
			      int channels,
			      int depth,
			      SANE_Bool half_ccd, int color_filter, int flags)
{
  unsigned int words_per_line;
  unsigned int startx, endx, used_pixels;
  unsigned int dpiset, dpihw,segnb,cksel,factor;
  unsigned int bytes;
  Genesys_Register_Set *r;
  SANE_Status status;
  Sensor_Profile *sensor;

  DBG (DBG_proc, "gl846_init_optical_regs_scan :  exposure_time=%d, "
       "used_res=%d, start=%d, pixels=%d, channels=%d, depth=%d, "
       "half_ccd=%d, flags=%x\n", exposure_time,
       used_res, start, pixels, channels, depth, half_ccd, flags);

  /* resolution is divided according to CKSEL */
  r = sanei_genesys_get_address (reg, REG18);
  cksel= (r->value & REG18_CKSEL)+1;
  DBG (DBG_io2, "%s: cksel=%d\n", __FUNCTION__, cksel);

  /* to manage high resolution device while keeping good
   * low resolution scanning speed, we make hardware dpi vary */
  dpihw=sanei_genesys_compute_dpihw(dev, used_res * cksel);
  factor=dev->sensor.optical_res/dpihw;
  DBG (DBG_io2, "%s: dpihw=%d (factor=%d)\n", __FUNCTION__, dpihw, factor);

  /* sensor parameters */
  sensor=get_sensor_profile(dev->model->ccd_type, dpihw);
  gl846_setup_sensor (dev, reg, dpihw);
  dpiset = used_res * cksel;

  /* start and end coordinate in optical dpi coordinates */
  startx = start/cksel+dev->sensor.CCD_start_xoffset;
  used_pixels=pixels/cksel;

  /* end of sensor window */
  endx = startx + used_pixels;

  /* sensors are built from 600 dpi segments for LiDE 100/200
   * and 1200 dpi for the 700F */
  if (dev->model->flags & GENESYS_FLAG_SIS_SENSOR)
    {
      segnb=dpihw/600;
    }
  else
    {
      segnb=1;
    }

  /* compute pixel coordinate in the given dpihw space,
   * taking segments into account */
  startx/=factor*segnb;
  endx/=factor*segnb;
  dev->len=endx-startx;
  dev->dist=0;
  dev->skip=0;

  /* in cas of multi-segments sensor, we have to add the witdh
   * of the sensor crossed by the scan area */
  if (dev->model->flags & GENESYS_FLAG_SIS_SENSOR && segnb>1)
    {
      dev->dist = sensor->segcnt;
    }

  /* use a segcnt rounded to next even number */
  endx += ((dev->dist+1)&0xfffe)*(segnb-1);
  used_pixels=endx-startx;

  status = gl846_set_fe (dev, AFE_SET);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl846_init_optical_regs_scan: failed to set frontend: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* enable shading */
  r = sanei_genesys_get_address (reg, REG01);
  r->value &= ~REG01_SCAN;
  r->value |= REG01_SHDAREA;
  if ((flags & OPTICAL_FLAG_DISABLE_SHADING) ||
      (dev->model->flags & GENESYS_FLAG_NO_CALIBRATION))
    {
      r->value &= ~REG01_DVDSET;
    }
  else
    {
      r->value |= REG01_DVDSET;
    }

  r = sanei_genesys_get_address (reg, REG03);
  r->value &= ~REG03_AVEENB;

  if (flags & OPTICAL_FLAG_DISABLE_LAMP)
    r->value &= ~REG03_LAMPPWR;
  else
    r->value |= REG03_LAMPPWR;

  /* BW threshold */
  r = sanei_genesys_get_address (reg, 0x2e);
  r->value = dev->settings.threshold;
  r = sanei_genesys_get_address (reg, 0x2f);
  r->value = dev->settings.threshold;

  /* monochrome / color scan */
  r = sanei_genesys_get_address (reg, REG04);
  switch (depth)
    {
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

  r->value &= ~(REG04_FILTER | REG04_AFEMOD);
  if (channels == 1)
    {
      switch (color_filter)
	{
	case 0:
	  r->value |= 0x24;	/* red filter */
	  break;
	case 2:
	  r->value |= 0x2c;	/* blue filter */
	  break;
	default:
	  r->value |= 0x28;	/* green filter */
	  break;
	}
    }
  else
    r->value |= 0x20;		/* mono */

  /* register 05 */
  r = sanei_genesys_get_address (reg, REG05);

  /* set up dpihw */
  r->value &= ~REG05_DPIHW;
  switch(dpihw)
    {
      case 600:
        r->value |= REG05_DPIHW_600;
        break;
      case 1200:
        r->value |= REG05_DPIHW_1200;
        break;
      case 2400:
        r->value |= REG05_DPIHW_2400;
        break;
      case 4800:
        r->value |= REG05_DPIHW_4800;
        break;
    }

  /* enable gamma tables */
  if (flags & OPTICAL_FLAG_DISABLE_GAMMA)
    r->value &= ~REG05_GMMENB;
  else
    r->value |= REG05_GMMENB;

  /* CIS scanners can do true gray by setting LEDADD */
  /* we set up LEDADD only when asked */
  if (dev->model->is_cis == SANE_TRUE)
    {
      r = sanei_genesys_get_address (reg, 0x87);
      r->value &= ~REG87_LEDADD;
      if (channels == 1 && (flags & OPTICAL_FLAG_ENABLE_LEDADD))
	{
	  r->value |= REG87_LEDADD;
	}
      /* RGB weighting
      r = sanei_genesys_get_address (reg, 0x01);
      r->value &= ~REG01_TRUEGRAY;
      if (channels == 1 && (flags & OPTICAL_FLAG_ENABLE_LEDADD))
	{
	  r->value |= REG01_TRUEGRAY;
	}*/
    }

  /* words(16bit) before gamma, conversion to 8 bit or lineart*/
  words_per_line = (used_pixels * dpiset) / dpihw;
  bytes=depth/8;
  if (depth == 1)
    {
      words_per_line = (words_per_line+7)/8 ;
      dev->len = (dev->len >> 3) + ((dev->len & 7) ? 1 : 0);
      dev->dist = (dev->dist >> 3) + ((dev->dist & 7) ? 1 : 0);
    }
  else
    {
      words_per_line *= bytes;
      dev->dist *= bytes;
      dev->len *= bytes;
    }

  dev->bpl = words_per_line;
  dev->cur=0;
  dev->segnb=segnb;
  dev->line_interp = 0;

  sanei_genesys_set_double(reg,REG_DPISET,dpiset);
  DBG (DBG_io2, "%s: dpiset used=%d\n", __FUNCTION__, dpiset);

  sanei_genesys_set_double(reg,REG_STRPIXEL,startx);
  sanei_genesys_set_double(reg,REG_ENDPIXEL,endx);
  DBG (DBG_io2, "%s: startx=%d\n", __FUNCTION__, startx);
  DBG (DBG_io2, "%s: endx  =%d\n", __FUNCTION__, endx);

  DBG (DBG_io2, "%s: used_pixels=%d\n", __FUNCTION__, used_pixels);
  DBG (DBG_io2, "%s: pixels     =%d\n", __FUNCTION__, pixels);
  DBG (DBG_io2, "%s: depth      =%d\n", __FUNCTION__, depth);
  DBG (DBG_io2, "%s: dev->bpl   =%lu\n", __FUNCTION__, (unsigned long)dev->bpl);
  DBG (DBG_io2, "%s: dev->len   =%lu\n", __FUNCTION__, (unsigned long)dev->len);
  DBG (DBG_io2, "%s: dev->dist  =%lu\n", __FUNCTION__, (unsigned long)dev->dist);
  DBG (DBG_io2, "%s: dev->segnb =%lu\n", __FUNCTION__, (unsigned long)dev->segnb);

  words_per_line *= channels;
  dev->wpl = words_per_line;

  if(dev->oe_buffer.buffer!=NULL)
    {
      sanei_genesys_buffer_free (&(dev->oe_buffer));
    }
  RIE (sanei_genesys_buffer_alloc (&(dev->oe_buffer), dev->wpl));

  /* MAXWD is expressed in 4 words unit */
  sanei_genesys_set_triple(reg, REG_MAXWD, (words_per_line >> 2));
  DBG (DBG_io2, "%s: words_per_line used=%d\n", __FUNCTION__, words_per_line);

  sanei_genesys_set_double(reg, REG_LPERIOD, exposure_time);
  DBG (DBG_io2, "%s: exposure_time used=%d\n", __FUNCTION__, exposure_time);

  r = sanei_genesys_get_address (reg, 0x34);
  r->value = dev->sensor.dummy_pixel;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/* set up registers for an actual scan
 *
 * this function sets up the scanner to scan in normal or single line mode
 */
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
gl846_init_scan_regs (Genesys_Device * dev,
                      Genesys_Register_Set * reg,
                      float xres,	/*dpi */
		      float yres,	/*dpi */
		      float startx,	/*optical_res, from dummy_pixel+1 */
		      float starty,	/*base_ydpi, from home! */
		      float pixels,
		      float lines,
		      unsigned int depth,
		      unsigned int channels,
		      int color_filter,
                      unsigned int flags)
{
  int used_res;
  int start, used_pixels;
  int bytes_per_line;
  int move;
  unsigned int lincnt;
  unsigned int oflags; /**> optical flags */
  unsigned int mflags; /**> motor flags */
  int exposure_time;
  int stagger;

  int slope_dpi = 0;
  int dummy = 0;
  int scan_step_type = 1;
  int scan_power_mode = 0;
  int max_shift;
  size_t requested_buffer_size, read_buffer_size;

  SANE_Bool half_ccd;		/* false: full CCD res is used, true, half max CCD res is used */
  int optical_res;
  SANE_Status status;

  DBG (DBG_info,
       "gl846_init_scan_regs settings:\n"
       "Resolution    : %gDPI/%gDPI\n"
       "Lines         : %g\n"
       "PPL           : %g\n"
       "Startpos      : %g/%g\n"
       "Depth/Channels: %u/%u\n"
       "Flags         : %x\n\n",
       xres, yres, lines, pixels, startx, starty, depth, channels, flags);

  /* we may have 2 domains for ccd: xres below or above half ccd max dpi */
  if (dev->sensor.optical_res < 2 * xres ||
      !(dev->model->flags & GENESYS_FLAG_HALF_CCD_MODE))
    {
      half_ccd = SANE_FALSE;
    }
  else
    {
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
  DBG (DBG_info, "gl846_init_scan_regs : stagger=%d lines\n", stagger);

  /* used_res */
  if (flags & SCAN_FLAG_USE_OPTICAL_RES)
    {
      used_res = optical_res;
    }
  else
    {
      /* resolution is choosen from a list */
      used_res = xres;
    }

  /* compute scan parameters values */
  /* pixels are allways given at full optical resolution */
  /* use detected left margin and fixed value */
  /* start */
  /* add x coordinates */
  start = startx;

  if (stagger > 0)
    start |= 1;

  /* compute correct pixels number */
  /* pixels */
  used_pixels = (pixels * optical_res) / xres;

  /* round up pixels number if needed */
  if (used_pixels * xres < pixels * optical_res)
    used_pixels++;

  dummy = 3-channels;

/* slope_dpi */
/* cis color scan is effectively a gray scan with 3 gray lines per color
   line and a FILTER of 0 */
  if (dev->model->is_cis)
    slope_dpi = yres * channels;
  else
    slope_dpi = yres;

  slope_dpi = slope_dpi * (1 + dummy);

  exposure_time = gl846_compute_exposure (dev, used_res);
  scan_step_type = sanei_genesys_compute_step_type(gl846_motors, dev->model->motor_type, exposure_time);

  DBG (DBG_info, "gl846_init_scan_regs : exposure_time=%d pixels\n", exposure_time);
  DBG (DBG_info, "gl846_init_scan_regs : scan_step_type=%d\n", scan_step_type);

/*** optical parameters ***/
  /* in case of dynamic lineart, we use an internal 8 bit gray scan
   * to generate 1 lineart data */
  if ((flags & SCAN_FLAG_DYNAMIC_LINEART) && (dev->settings.scan_mode == SCAN_MODE_LINEART))
    {
      depth = 8;
    }

  /* we enable true gray for cis scanners only, and just when doing
   * scan since color calibration is OK for this mode
   */
  oflags = 0;
  if(flags & SCAN_FLAG_DISABLE_SHADING)
    oflags |= OPTICAL_FLAG_DISABLE_SHADING;
  if(flags & SCAN_FLAG_DISABLE_GAMMA)
    oflags |= OPTICAL_FLAG_DISABLE_GAMMA;
  if(flags & SCAN_FLAG_DISABLE_LAMP)
    oflags |= OPTICAL_FLAG_DISABLE_LAMP;

  if (dev->model->is_cis && dev->settings.true_gray)
    {
      oflags |= OPTICAL_FLAG_ENABLE_LEDADD;
    }

  status = gl846_init_optical_regs_scan (dev,
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
    return status;

/*** motor parameters ***/

  /* max_shift */
  max_shift=sanei_genesys_compute_max_shift(dev,channels,yres,flags);

  /* lincnt */
  lincnt = lines + max_shift + stagger;

  /* add tl_y to base movement */
  move = starty;
  DBG (DBG_info, "gl846_init_scan_regs: move=%d steps\n", move);

  mflags=0;
  if(flags & SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE)
    mflags |= MOTOR_FLAG_DISABLE_BUFFER_FULL_MOVE;
  if(flags & SCAN_FLAG_FEEDING)
    mflags |= MOTOR_FLAG_FEED;

    status = gl846_init_motor_regs_scan (dev,
					 reg,
					 exposure_time,
					 slope_dpi,
					 scan_step_type,
					 dev->model->is_cis ? lincnt *
					 channels : lincnt, dummy, move,
					 scan_power_mode,
					 mflags);

  if (status != SANE_STATUS_GOOD)
    return status;


  /*** prepares data reordering ***/

/* words_per_line */
  bytes_per_line = (used_pixels * used_res) / optical_res;
  bytes_per_line = (bytes_per_line * channels * depth) / 8;

  requested_buffer_size = 8 * bytes_per_line;
  /* we must use a round number of bytes_per_line */
  /* XXX STEF XXX
  if (requested_buffer_size > BULKIN_MAXSIZE)
    requested_buffer_size =
      (BULKIN_MAXSIZE / bytes_per_line) * bytes_per_line;
  */

  read_buffer_size =
    2 * requested_buffer_size +
    ((max_shift + stagger) * used_pixels * channels * depth) / 8;

  RIE (sanei_genesys_buffer_free (&(dev->read_buffer)));
  RIE (sanei_genesys_buffer_alloc (&(dev->read_buffer), read_buffer_size));

  RIE (sanei_genesys_buffer_free (&(dev->lines_buffer)));
  RIE (sanei_genesys_buffer_alloc (&(dev->lines_buffer), read_buffer_size));

  RIE (sanei_genesys_buffer_free (&(dev->shrink_buffer)));
  RIE (sanei_genesys_buffer_alloc (&(dev->shrink_buffer),
				   requested_buffer_size));

  RIE (sanei_genesys_buffer_free (&(dev->out_buffer)));
  RIE (sanei_genesys_buffer_alloc (&(dev->out_buffer),
				   (8 * dev->settings.pixels * channels *
				    depth) / 8));


  dev->read_bytes_left = bytes_per_line * lincnt;

  DBG (DBG_info,
       "gl846_init_scan_regs: physical bytes to read = %lu\n",
       (u_long) dev->read_bytes_left);
  dev->read_active = SANE_TRUE;


  dev->current_setup.pixels = (used_pixels * used_res) / optical_res;
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
       (((dev->settings.pixels * dev->settings.lines) % 8) ? 1 : 0)) *
      channels;
  else
    dev->total_bytes_to_read =
      dev->settings.pixels * dev->settings.lines * channels * (depth / 8);

  DBG (DBG_info, "gl846_init_scan_regs: total bytes to send = %lu\n",
       (u_long) dev->total_bytes_to_read);
/* END TODO */

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl846_calculate_current_setup (Genesys_Device * dev)
{
  int channels;
  int depth;
  int start;

  float xres;			/*dpi */
  float yres;			/*dpi */
  float startx;			/*optical_res, from dummy_pixel+1 */
  float pixels;
  float lines;

  int used_res;
  int used_pixels;
  unsigned int lincnt;
  int exposure_time;
  int stagger;

  int slope_dpi;
  int dummy = 0;
  int max_shift;

  SANE_Bool half_ccd;		/* false: full CCD res is used, true, half max CCD res is used */
  int optical_res;

  DBG (DBG_info,
       "gl846_calculate_current_setup settings:\n"
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


  xres = dev->settings.xres;	/*dpi */
  yres = dev->settings.yres;	/*dpi */
  startx = start;		/*optical_res, from dummy_pixel+1 */
  pixels = dev->settings.pixels;
  lines = dev->settings.lines;

  DBG (DBG_info,
       "gl846_calculate_current_setup settings:\n"
       "Resolution    : %gDPI/%gDPI\n"
       "Lines         : %g\n"
       "PPL           : %g\n"
       "Startpos      : %g\n"
       "Depth/Channels: %u/%u\n\n",
       xres, yres, lines, pixels, startx, depth, channels);

/* half_ccd */
  /* we have 2 domains for ccd: xres below or above half ccd max dpi */
  if ((dev->sensor.optical_res < 2 * xres) ||
      !(dev->model->flags & GENESYS_FLAG_HALF_CCD_MODE))
    {
      half_ccd = SANE_FALSE;
    }
  else
    {
      half_ccd = SANE_TRUE;
    }

  /* optical_res */
  optical_res = dev->sensor.optical_res;

  /* stagger */
  if (dev->model->flags & GENESYS_FLAG_STAGGERED_LINE)
    stagger = (4 * yres) / dev->motor.base_ydpi;
  else
    stagger = 0;
  DBG (DBG_info, "gl846_calculate_current_setup: stagger=%d lines\n",
       stagger);

  /* resolution is choosen from a fixed list */
  used_res = xres;

  /* compute scan parameters values */
  /* pixels are allways given at half or full CCD optical resolution */
  /* use detected left margin  and fixed value */

  /* compute correct pixels number */
  used_pixels = (pixels * optical_res) / used_res;
  dummy = 3-channels;

  /* slope_dpi */
  /* cis color scan is effectively a gray scan with 3 gray lines per color
   line and a FILTER of 0 */
  if (dev->model->is_cis)
    slope_dpi = yres * channels;
  else
    slope_dpi = yres;

  slope_dpi = slope_dpi * (1 + dummy);

  exposure_time = gl846_compute_exposure (dev, used_res);
  DBG (DBG_info, "%s : exposure_time=%d pixels\n", __FUNCTION__, exposure_time);

  /* max_shift */
  max_shift=sanei_genesys_compute_max_shift(dev,channels,yres,0);

  /* lincnt */
  lincnt = lines + max_shift + stagger;

  dev->current_setup.pixels = (used_pixels * used_res) / optical_res;
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
gl846_set_motor_power (Genesys_Register_Set * regs, SANE_Bool set)
{

  DBG (DBG_proc, "gl846_set_motor_power\n");

  if (set)
    {
      sanei_genesys_set_reg_from_set (regs, REG02,
				      sanei_genesys_read_reg_from_set (regs,
								       REG02)
				      | REG02_MTRPWR);
    }
  else
    {
      sanei_genesys_set_reg_from_set (regs, REG02,
				      sanei_genesys_read_reg_from_set (regs,
								       REG02)
				      & ~REG02_MTRPWR);
    }
}

static void
gl846_set_lamp_power (Genesys_Device __sane_unused__ * dev,
		      Genesys_Register_Set * regs, SANE_Bool set)
{
  if (set)
    {
      sanei_genesys_set_reg_from_set (regs, REG03,
				      sanei_genesys_read_reg_from_set (regs, REG03)
				      | REG03_LAMPPWR);
    }
  else
    {
      sanei_genesys_set_reg_from_set (regs, REG03,
				      sanei_genesys_read_reg_from_set (regs, REG03)
				      & ~REG03_LAMPPWR);
    }
}

/*for fast power saving methods only, like disabling certain amplifiers*/
static SANE_Status
gl846_save_power (Genesys_Device * dev, SANE_Bool enable)
{
  DBG (DBG_proc, "gl846_save_power: enable = %d\n", enable);
  if (dev == NULL)
    return SANE_STATUS_INVAL;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl846_set_powersaving (Genesys_Device * dev, int delay /* in minutes */ )
{
  DBG (DBG_proc, "gl846_set_powersaving (delay = %d)\n", delay);
  if (dev == NULL)
    return SANE_STATUS_INVAL;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl846_start_action (Genesys_Device * dev)
{
  return sanei_genesys_write_register (dev, 0x0f, 0x01);
}

#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl846_stop_action (Genesys_Device * dev)
{
  SANE_Status status;
  uint8_t val40, val;
  unsigned int loop;

  DBGSTART;

  /* post scan gpio : without that HOMSNR is unreliable */
  gl846_homsnr_gpio(dev);
  status = sanei_genesys_get_status (dev, &val);
  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }

  status = sanei_genesys_read_register (dev, REG40, &val40);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "%s: failed to read home sensor: %s\n", __FUNCTION__,
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

  /* ends scan */
  val = sanei_genesys_read_reg_from_set (dev->reg, REG01);
  val &= ~REG01_SCAN;
  sanei_genesys_set_reg_from_set (dev->reg, REG01, val);
  status = sanei_genesys_write_register (dev, REG01, val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "%s: failed to write register 01: %s\n", __FUNCTION__,
	   sane_strstatus (status));
      return status;
    }
  usleep (100 * 1000);

  loop = 10;
  while (loop > 0)
    {
      status = sanei_genesys_get_status (dev, &val);
      if (DBG_LEVEL >= DBG_io)
	{
	  sanei_genesys_print_status (val);
	}
      status = sanei_genesys_read_register (dev, REG40, &val40);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "%s: failed to read home sensor: %s\n", __FUNCTION__,
	       sane_strstatus (status));
          DBGCOMPLETED;
	  return status;
	}

      /* if scanner is in command mode, we are done */
      if (!(val40 & REG40_DATAENB) && !(val40 & REG40_MOTMFLG)
	  && !(val & REG41_MOTORENB))
	{
          DBGCOMPLETED;
	  return SANE_STATUS_GOOD;
	}

      usleep (100 * 1000);
      loop--;
    }

  DBGCOMPLETED;
  return SANE_STATUS_IO_ERROR;
}

/* Send the low-level scan command */
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
gl846_begin_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
		  SANE_Bool start_motor)
{
  SANE_Status status;
  uint8_t val;
  Genesys_Register_Set *r;

  DBGSTART;

  /* XXX STEF XXX SCAN GPIO */
  /*
  RIE (sanei_genesys_read_register (dev, REG6C, &val));
  RIE (sanei_genesys_write_register (dev, REG6C, val));
  */

  val = REG0D_CLRLNCNT;
  RIE (sanei_genesys_write_register (dev, REG0D, val));
  val = REG0D_CLRMCNT;
  RIE (sanei_genesys_write_register (dev, REG0D, val));

  RIE (sanei_genesys_read_register (dev, REG01, &val));
  val |= REG01_SCAN;
  RIE (sanei_genesys_write_register (dev, REG01, val));
  r = sanei_genesys_get_address (reg, REG01);
  r->value = val;

  if (start_motor)
    {
      RIE (sanei_genesys_write_register (dev, REG0F, 1));
    }
  else
    {
      RIE (sanei_genesys_write_register (dev, REG0F, 0));
    }

  DBGCOMPLETED;

  return status;
}


/* Send the stop scan command */
#ifndef UNIT_TESTING
static
#endif
  SANE_Status
gl846_end_scan (Genesys_Device * dev, Genesys_Register_Set * reg,
		SANE_Bool check_stop)
{
  SANE_Status status;

  DBG (DBG_proc, "gl846_end_scan (check_stop = %d)\n", check_stop);
  if (reg == NULL)
    return SANE_STATUS_INVAL;

  if (dev->model->is_sheetfed == SANE_TRUE)
    {
      status = SANE_STATUS_GOOD;
    }
  else				/* flat bed scanners */
    {
      status = gl846_stop_action (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl846_end_scan: failed to stop: %s\n",
	       sane_strstatus (status));
	  return status;
	}
    }

  DBGCOMPLETED;
  return status;
}

/* Moves the slider to the home (top) postion slowly */
#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl846_slow_back_home (Genesys_Device * dev, SANE_Bool wait_until_home)
{
  Genesys_Register_Set local_reg[GENESYS_GL846_MAX_REGS];
  SANE_Status status;
  Genesys_Register_Set *r;
  float resolution;
  uint8_t val;
  int loop = 0;
  int scan_mode;

  DBG (DBG_proc, "gl846_slow_back_home (wait_until_home = %d)\n",
       wait_until_home);

  if(dev->usb_mode<0)
    {
      DBGCOMPLETED;
      return SANE_STATUS_GOOD;
    }

  /* post scan gpio : without that HOMSNR is unreliable */
  gl846_homsnr_gpio(dev);

  /* first read gives HOME_SENSOR true */
  status = sanei_genesys_get_status (dev, &val);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl846_slow_back_home: failed to read home sensor: %s\n",
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
	   "gl846_slow_back_home: failed to read home sensor: %s\n",
	   sane_strstatus (status));
      return status;
    }
  if (DBG_LEVEL >= DBG_io)
    {
      sanei_genesys_print_status (val);
    }

  /* is sensor at home? */
  if (val & HOMESNR)
    {
      DBG (DBG_info, "%s: already at home, completed\n", __FUNCTION__);
      dev->scanhead_position_in_steps = 0;
      DBGCOMPLETED;
      return SANE_STATUS_GOOD;
    }

  memcpy (local_reg, dev->reg, GENESYS_GL846_MAX_REGS * sizeof (Genesys_Register_Set));

  resolution=sanei_genesys_get_lowest_ydpi(dev);

  /* TODO add scan_mode to the API */
  scan_mode= dev->settings.scan_mode;
  dev->settings.scan_mode=SCAN_MODE_LINEART;
  status = gl846_init_scan_regs (dev,
			local_reg,
			resolution,
			resolution,
			100,
			30000,
			100,
			100,
			8,
			1,
			0,
			SCAN_FLAG_DISABLE_SHADING |
			SCAN_FLAG_DISABLE_GAMMA |
			SCAN_FLAG_IGNORE_LINE_DISTANCE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "gl846_slow_back_home: failed to set up registers: %s\n",
           sane_strstatus (status));
      DBGCOMPLETED;
      return status;
    }
  dev->settings.scan_mode=scan_mode;

  /* clear scan and feed count */
  RIE (sanei_genesys_write_register (dev, REG0D, REG0D_CLRLNCNT | REG0D_CLRMCNT));

  /* set up for reverse */
  r = sanei_genesys_get_address (local_reg, REG02);
  r->value |= REG02_MTRREV;

  RIE (dev->model->cmd_set->bulk_write_register (dev, local_reg, GENESYS_GL846_MAX_REGS));

  status = gl846_start_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl846_slow_back_home: failed to start motor: %s\n",
	   sane_strstatus (status));
      gl846_stop_action (dev);
      /* send original registers */
      dev->model->cmd_set->bulk_write_register (dev, dev->reg, GENESYS_GL846_MAX_REGS);
      return status;
    }

  /* post scan gpio : without that HOMSNR is unreliable */
  gl846_homsnr_gpio(dev);

  if (wait_until_home)
    {
      while (loop < 300)	/* do not wait longer then 30 seconds */
	{
	  status = sanei_genesys_get_status (dev, &val);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error,
		   "gl846_slow_back_home: failed to read home sensor: %s\n",
		   sane_strstatus (status));
	      return status;
	    }

	  if (val & HOMESNR)	/* home sensor */
	    {
	      DBG (DBG_info, "gl846_slow_back_home: reached home position\n");
              gl846_stop_action (dev);
              dev->scanhead_position_in_steps = 0;
	      DBGCOMPLETED;
	      return SANE_STATUS_GOOD;
	    }
	  usleep (100000);	/* sleep 100 ms */
	  ++loop;
	}

      /* when we come here then the scanner needed too much time for this, so we better stop the motor */
      gl846_stop_action (dev);
      DBG (DBG_error,
	   "gl846_slow_back_home: timeout while waiting for scanhead to go home\n");
      return SANE_STATUS_IO_ERROR;
    }

  DBG (DBG_info, "gl846_slow_back_home: scanhead is still moving\n");
  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/* Automatically set top-left edge of the scan area by scanning a 200x200 pixels
   area at 600 dpi from very top of scanner */
static SANE_Status
gl846_search_start_position (Genesys_Device * dev)
{
  int size;
  SANE_Status status;
  uint8_t *data;
  Genesys_Register_Set local_reg[GENESYS_GL846_MAX_REGS];
  int steps;

  int pixels = 600;
  int dpi = 300;

  DBG (DBG_proc, "gl846_search_start_position\n");

  memcpy (local_reg, dev->reg,
	  GENESYS_GL846_MAX_REGS * sizeof (Genesys_Register_Set));

  /* sets for a 200 lines * 600 pixels */
  /* normal scan with no shading */

  status = gl846_init_scan_regs (dev, local_reg, dpi, dpi, 0, 0,	/*we should give a small offset here~60 steps */
				 600, dev->model->search_lines, 8, 1, 1,	/*green */
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl846_search_start_position: failed to set up registers: %s\n",
	   sane_strstatus (status));
      DBGCOMPLETED;
      return status;
    }

  /* send to scanner */
  status = dev->model->cmd_set->bulk_write_register (dev, local_reg, GENESYS_GL846_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl846_search_start_position: failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      DBGCOMPLETED;
      return status;
    }

  size = pixels * dev->model->search_lines;

  data = malloc (size);
  if (!data)
    {
      DBG (DBG_error,
	   "gl846_search_start_position: failed to allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }

  status = gl846_begin_scan (dev, local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl846_search_start_position: failed to begin scan: %s\n",
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
	   "gl846_search_start_position: failed to read data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("search_position.pnm", data, 8, 1, pixels,
				  dev->model->search_lines);

  status = gl846_end_scan (dev, local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl846_search_start_position: failed to end scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* update regs to copy ASIC internal state */
  memcpy (dev->reg, local_reg,
	  GENESYS_GL846_MAX_REGS * sizeof (Genesys_Register_Set));

/*TODO: find out where sanei_genesys_search_reference_point
  stores information, and use that correctly*/
  status =
    sanei_genesys_search_reference_point (dev, data, 0, dpi, pixels,
					  dev->model->search_lines);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl846_search_start_position: failed to set search reference point: %s\n",
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
gl846_init_regs_for_coarse_calibration (Genesys_Device * dev)
{
  SANE_Status status;
  uint8_t channels;
  uint8_t cksel;

  DBG (DBG_proc, "gl846_init_regs_for_coarse_calibration\n");


  cksel = (dev->calib_reg[reg_0x18].value & REG18_CKSEL) + 1;	/* clock speed = 1..4 clocks */

  /* set line size */
  if (dev->settings.scan_mode == SCAN_MODE_COLOR)	/* single pass color */
    channels = 3;
  else
    channels = 1;

  status = gl846_init_scan_regs (dev,
				 dev->calib_reg,
				 dev->settings.xres,
				 dev->settings.yres,
				 0,
				 0,
				 dev->sensor.optical_res / cksel,
				 20,
				 16,
				 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl846_init_register_for_coarse_calibration: Failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBG (DBG_info,
       "gl846_init_register_for_coarse_calibration: optical sensor res: %d dpi, actual res: %d\n",
       dev->sensor.optical_res / cksel, dev->settings.xres);

  status = dev->model->cmd_set->bulk_write_register (dev, dev->calib_reg, GENESYS_GL846_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl846_init_register_for_coarse_calibration: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/** @brief moves the slider to steps at motor base dpi
 * @param dev device to work on
 * @param steps number of steps to move in base_dpi line count
 * */
#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl846_feed (Genesys_Device * dev, unsigned int steps)
{
  Genesys_Register_Set local_reg[GENESYS_GL846_MAX_REGS];
  SANE_Status status;
  Genesys_Register_Set *r;
  float resolution;
  uint8_t val;

  DBGSTART;
  DBG (DBG_io, "%s: steps=%d\n", __FUNCTION__, steps);

  /* prepare local registers */
  memcpy (local_reg, dev->reg, GENESYS_GL846_MAX_REGS * sizeof (Genesys_Register_Set));

  resolution=sanei_genesys_get_lowest_ydpi(dev);
  status = gl846_init_scan_regs (dev,
			local_reg,
			resolution,
			resolution,
			0,
			steps,
			100,
			3,
			8,
			3,
			dev->settings.color_filter,
			SCAN_FLAG_DISABLE_SHADING |
			SCAN_FLAG_DISABLE_GAMMA |
                        SCAN_FLAG_FEEDING |
			SCAN_FLAG_IGNORE_LINE_DISTANCE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "gl846_feed: failed to set up registers: %s\n",
           sane_strstatus (status));
      DBGCOMPLETED;
      return status;
    }

  /* set exposure to zero */
  sanei_genesys_set_triple(local_reg,REG_EXPR,0);
  sanei_genesys_set_triple(local_reg,REG_EXPG,0);
  sanei_genesys_set_triple(local_reg,REG_EXPB,0);

  /* clear scan and feed count */
  RIE (sanei_genesys_write_register (dev, REG0D, REG0D_CLRLNCNT));
  RIE (sanei_genesys_write_register (dev, REG0D, REG0D_CLRMCNT));

  /* set up for no scan */
  r = sanei_genesys_get_address (local_reg, REG01);
  r->value &= ~REG01_SCAN;

  /* send registers */
  RIE (dev->model->cmd_set->bulk_write_register (dev, local_reg, GENESYS_GL846_MAX_REGS));

  status = gl846_start_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to start motor: %s\n", __FUNCTION__, sane_strstatus (status));
      gl846_stop_action (dev);

      /* restore original registers */
      dev->model->cmd_set->bulk_write_register (dev, dev->reg, GENESYS_GL846_MAX_REGS);
      return status;
    }

  /* wait until feed count reaches the required value, but do not
   * exceed 30s */
  do
    {
          status = sanei_genesys_get_status (dev, &val);
    }
  while (status == SANE_STATUS_GOOD && !(val & FEEDFSH));

  /* then stop scanning */
  RIE(gl846_stop_action (dev));

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}


/* init registers for shading calibration */
static SANE_Status
gl846_init_regs_for_shading (Genesys_Device * dev)
{
  SANE_Status status;
  float move;

  DBGSTART;
  dev->calib_channels = 3;

  /* initial calibration reg values */
  memcpy (dev->calib_reg, dev->reg, GENESYS_GL846_MAX_REGS * sizeof (Genesys_Register_Set));

  dev->calib_resolution = sanei_genesys_compute_dpihw(dev,dev->settings.xres);
  dev->calib_lines = dev->model->shading_lines;
  if(dev->calib_resolution==4800)
    dev->calib_lines *= 2;
  dev->calib_pixels = (dev->sensor.sensor_pixels*dev->calib_resolution)/dev->sensor.optical_res;
  DBG (DBG_io, "%s: calib_lines  = %d\n", __FUNCTION__, (unsigned int)dev->calib_lines);
  DBG (DBG_io, "%s: calib_pixels = %d\n", __FUNCTION__, (unsigned int)dev->calib_pixels);

  /* this is aworkaround insufficent distance for slope
   * motor acceleration TODO special motor slope for shading  */
  move=1;
  if(dev->calib_resolution<1200)
    {
      move=40;
    }

  status = gl846_init_scan_regs (dev,
				 dev->calib_reg,
                                 dev->calib_resolution,
				 dev->calib_resolution,
				 0,
				 move,
				 dev->calib_pixels,
				 dev->calib_lines,
                                 16,
				 dev->calib_channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
  				 SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to setup scan: %s\n", __FUNCTION__, sane_strstatus (status));
      return status;
    }

  status = dev->model->cmd_set->bulk_write_register (dev, dev->calib_reg, GENESYS_GL846_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to bulk write registers: %s\n", __FUNCTION__, sane_strstatus (status));
      return status;
    }

  /* we use GENESYS_FLAG_SHADING_REPARK */
  dev->scanhead_position_in_steps = 0;

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/** @brief set up registers for the actual scan
 */
static SANE_Status
gl846_init_regs_for_scan (Genesys_Device * dev)
{
  int channels;
  int flags;
  int depth;
  float move;
  int move_dpi;
  float start;

  SANE_Status status;

  DBG (DBG_info,
       "gl846_init_regs_for_scan settings:\nResolution: %uDPI\n"
       "Lines     : %u\nPPL       : %u\nStartpos  : %.3f/%.3f\nScan mode : %d\n\n",
       dev->settings.yres, dev->settings.lines, dev->settings.pixels,
       dev->settings.tl_x, dev->settings.tl_y, dev->settings.scan_mode);

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

  move = SANE_UNFIX (dev->model->y_offset);
  move += dev->settings.tl_y;
  move = (move * move_dpi) / MM_PER_INCH;
  move -= dev->scanhead_position_in_steps;
  DBG (DBG_info, "%s: move=%f steps\n",__FUNCTION__, move);

  /* fast move to scan area */
  /* we don't move fast the whole distance since it would involve
   * computing acceleration/deceleration distance for scan
   * resolution. So leave a remainder for it so scan makes the final
   * move tuning */
  if(channels*dev->settings.yres>=600 && move>700)
    {
      status = gl846_feed (dev, move-500);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (DBG_error, "%s: failed to move to scan area\n",__FUNCTION__);
          return status;
        }
      move=500;
    }

  DBG (DBG_info, "gl846_init_regs_for_scan: move=%f steps\n", move);
  DBG (DBG_info, "%s: move=%f steps\n", __FUNCTION__, move);

  /* start */
  start = SANE_UNFIX (dev->model->x_offset);
  start += dev->settings.tl_x;
  start = (start * dev->sensor.optical_res) / MM_PER_INCH;

  flags = 0;

  /* emulated lineart from gray data is required for now */
  if(dev->settings.scan_mode == SCAN_MODE_LINEART
     && dev->settings.dynamic_lineart)
    {
      flags |= SCAN_FLAG_DYNAMIC_LINEART;
    }

  /* backtracking isn't handled well, so don't enable it */
  flags |= SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE;

  status = gl846_init_scan_regs (dev,
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

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}


/**
 * Send shading calibration data. The buffer is considered to always hold values
 * for all the channels.
 */
static SANE_Status
gl846_send_shading_data (Genesys_Device * dev, uint8_t * data, int size)
{
  SANE_Status status = SANE_STATUS_GOOD;
  uint32_t addr, length, i, x, factor, pixels;
  uint32_t dpiset, dpihw, strpixel, endpixel;
  uint16_t tempo;
  uint32_t lines, channels;
  uint8_t val,*buffer,*ptr,*src;

  DBGSTART;
  DBG( DBG_io2, "%s: writing %d bytes of shading data\n",__FUNCTION__,size);

  /* shading data is plit in 3 (up to 5 with IR) areas
     write(0x10014000,0x00000dd8)
     URB 23429  bulk_out len  3544  wrote 0x33 0x10 0x....
     write(0x1003e000,0x00000dd8)
     write(0x10068000,0x00000dd8)
   */
  length = (uint32_t) (size / 3);
  sanei_genesys_get_double(dev->reg,REG_STRPIXEL,&tempo);
  strpixel=tempo;
  sanei_genesys_get_double(dev->reg,REG_ENDPIXEL,&tempo);
  endpixel=tempo;

  /* compute deletion factor */
  sanei_genesys_get_double(dev->reg,REG_DPISET,&tempo);
  dpiset=tempo;
  DBG( DBG_io2, "%s: STRPIXEL=%d, ENDPIXEL=%d, PIXELS=%d, DPISET=%d\n",__FUNCTION__,strpixel,endpixel,endpixel-strpixel,dpiset);
  dpihw=sanei_genesys_compute_dpihw(dev,dpiset);
  factor=dpihw/dpiset;
  DBG( DBG_io2, "%s: factor=%d\n",__FUNCTION__,factor);

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

  pixels=endpixel-strpixel;

  /* since we're using SHDAREA, substract startx coordinate from shading */
  strpixel-=((dev->sensor.CCD_start_xoffset*600)/dev->sensor.optical_res);

  /* turn pixel value into bytes 2x16 bits words */
  strpixel*=2*2;
  pixels*=2*2;

  /* allocate temporary buffer */
  buffer=(uint8_t *)malloc(pixels);
  memset(buffer,0,pixels);
  DBG( DBG_io2, "%s: using chunks of %d (0x%04x) bytes\n",__FUNCTION__,pixels,pixels);

  /* base addr of data has been written in reg D0-D4 in 4K word, so AHB address
   * is 8192*reg value */

  /* write actual color channel data */
  for(i=0;i<3;i++)
    {
      /* build up actual shading data by copying the part from the full width one
       * to the one corresponding to SHDAREA */
      ptr=buffer;

      /* iterate on both sensor segment */
      for(x=0;x<pixels;x+=4*factor)
        {
          /* coefficient source */
          src=(data+strpixel+i*length)+x;

          /* coefficient copy */
          ptr[0]=src[0];
          ptr[1]=src[1];
          ptr[2]=src[2];
          ptr[3]=src[3];

          /* next shading coefficient */
          ptr+=4;
        }

      RIEF (sanei_genesys_read_register (dev, 0xd0+i, &val), buffer);
      addr = val * 8192 + 0x10000000;
      status = sanei_genesys_write_ahb (dev->dn, dev->usb_mode, addr, pixels, buffer);
      if (status != SANE_STATUS_GOOD)
        {
          DBG (DBG_error, "gl846_send_shading_data; write to AHB failed (%s)\n",
	      sane_strstatus (status));
          free(buffer);
          return status;
        }
    }

  free(buffer);
  DBGCOMPLETED;

  return status;
}

/** @brief calibrates led exposure
 * Calibrate exposure by scanning a white area until the used exposure gives
 * data white enough.
 * @param dev device to calibrate
 */
static SANE_Status
gl846_led_calibration (Genesys_Device * dev)
{
  int num_pixels;
  int total_size;
  int used_res;
  uint8_t *line;
  int i, j;
  SANE_Status status = SANE_STATUS_GOOD;
  int val;
  int channels, depth;
  int avg[3], top[3], bottom[3];
  int turn;
  char fn[20];
  uint16_t exp[3];
  Sensor_Profile *sensor;
  float move;
  SANE_Bool acceptable;

  DBGSTART;

  move = SANE_UNFIX (dev->model->y_offset_calib);
  move = (move * (dev->motor.base_ydpi/4)) / MM_PER_INCH;
  if(move>20)
    {
      RIE(gl846_feed (dev, move));
    }
  DBG (DBG_io, "%s: move=%f steps\n", __FUNCTION__, move);

  /* offset calibration is always done in color mode */
  channels = 3;
  depth=16;
  used_res=sanei_genesys_compute_dpihw(dev,dev->settings.xres);
  sensor=get_sensor_profile(dev->model->ccd_type, used_res);
  num_pixels = (dev->sensor.sensor_pixels*used_res)/dev->sensor.optical_res;

  /* initial calibration reg values */
  memcpy (dev->calib_reg, dev->reg, GENESYS_GL846_MAX_REGS * sizeof (Genesys_Register_Set));

  /* set up for the calibration scan */
  status = gl846_init_scan_regs (dev,
				 dev->calib_reg,
				 used_res,
				 used_res,
				 0,
				 0,
				 num_pixels,
                                 1,
                                 depth,
                                 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: failed to setup scan: %s\n", __FUNCTION__, sane_strstatus (status));
      return status;
    }

  total_size = num_pixels * channels * (depth/8) * 1;	/* colors * bytes_per_color * scan lines */
  line = malloc (total_size);
  if (!line)
    return SANE_STATUS_NO_MEM;

  /* initial loop values and boundaries */
  exp[0]=sensor->expr;
  exp[1]=sensor->expg;
  exp[2]=sensor->expb;

  bottom[0]=29000;
  bottom[1]=29000;
  bottom[2]=29000;

  top[0]=41000;
  top[1]=51000;
  top[2]=51000;

  turn = 0;

  /* no move during led calibration */
  gl846_set_motor_power (dev->calib_reg, SANE_FALSE);
  do
    {
      /* set up exposure */
      sanei_genesys_set_double(dev->calib_reg,REG_EXPR,exp[0]);
      sanei_genesys_set_double(dev->calib_reg,REG_EXPG,exp[1]);
      sanei_genesys_set_double(dev->calib_reg,REG_EXPB,exp[2]);

      /* write registers and scan data */
      RIEF (dev->model->cmd_set->bulk_write_register (dev, dev->calib_reg, GENESYS_GL846_MAX_REGS), line);

      DBG (DBG_info, "gl846_led_calibration: starting line reading\n");
      RIEF (gl846_begin_scan (dev, dev->calib_reg, SANE_TRUE), line);
      RIEF (sanei_genesys_read_data_from_scanner (dev, line, total_size), line);

      /* stop scanning */
      RIEF (gl846_stop_action (dev), line);

      if (DBG_LEVEL >= DBG_data)
	{
	  snprintf (fn, 20, "led_%02d.pnm", turn);
	  sanei_genesys_write_pnm_file (fn, line, depth, channels, num_pixels, 1);
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

      DBG (DBG_info, "gl846_led_calibration: average: %d,%d,%d\n", avg[0], avg[1], avg[2]);

      /* check if exposure gives average within the boundaries */
      acceptable = SANE_TRUE;
      for(i=0;i<3;i++)
        {
          if(avg[i]<bottom[i])
            {
              exp[i]=(exp[i]*bottom[i])/avg[i];
              acceptable = SANE_FALSE;
            }
          if(avg[i]>top[i])
            {
              exp[i]=(exp[i]*top[i])/avg[i];
              acceptable = SANE_FALSE;
            }
        }

      turn++;
    }
  while (!acceptable && turn < 100);

  DBG (DBG_info, "gl846_led_calibration: acceptable exposure: %d,%d,%d\n", exp[0], exp[1], exp[2]);

  /* set these values as final ones for scan */
  sanei_genesys_set_double(dev->reg,REG_EXPR,exp[0]);
  sanei_genesys_set_double(dev->reg,REG_EXPG,exp[1]);
  sanei_genesys_set_double(dev->reg,REG_EXPB,exp[2]);

  /* store in this struct since it is the one used by cache calibration */
  dev->sensor.regs_0x10_0x1d[0] = (exp[0] >> 8) & 0xff;
  dev->sensor.regs_0x10_0x1d[1] = exp[0] & 0xff;
  dev->sensor.regs_0x10_0x1d[2] = (exp[1] >> 8) & 0xff;
  dev->sensor.regs_0x10_0x1d[3] = exp[1] & 0xff;
  dev->sensor.regs_0x10_0x1d[4] = (exp[2] >> 8) & 0xff;
  dev->sensor.regs_0x10_0x1d[5] = exp[2] & 0xff;

  /* cleanup before return */
  free (line);

  /* go back home */
  if(move>20)
    {
      status=gl846_slow_back_home (dev, SANE_TRUE);
    }

  DBGCOMPLETED;
  return status;
}

/**
 * set up GPIO/GPOE for idle state
 */
static SANE_Status
gl846_init_gpio (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int idx=0;

  DBGSTART;

  /* search GPIO profile */
  while(gpios[idx].sensor_id!=0 && dev->model->gpo_type!=gpios[idx].sensor_id)
    {
      idx++;
    }
  if(gpios[idx].sensor_id==0)
    {
      DBG (DBG_error, "%s: failed to find GPIO profile for sensor_id=%d\n", __FUNCTION__, dev->model->ccd_type);
      return SANE_STATUS_INVAL;
    }

  RIE (sanei_genesys_write_register (dev, REGA7, gpios[idx].ra7));
  RIE (sanei_genesys_write_register (dev, REGA6, gpios[idx].ra6));

  RIE (sanei_genesys_write_register (dev, REG6B, gpios[idx].r6b));
  RIE (sanei_genesys_write_register (dev, REG6C, gpios[idx].r6c));
  RIE (sanei_genesys_write_register (dev, REG6D, gpios[idx].r6d));
  RIE (sanei_genesys_write_register (dev, REG6E, gpios[idx].r6e));
  RIE (sanei_genesys_write_register (dev, REG6F, gpios[idx].r6f));

  RIE (sanei_genesys_write_register (dev, REGA8, gpios[idx].ra8));
  RIE (sanei_genesys_write_register (dev, REGA9, gpios[idx].ra9));

  DBGCOMPLETED;
  return status;
}

/**
 * set memory layout by filling values in dedicated registers
 */
static SANE_Status
gl846_init_memory_layout (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  int idx = 0, i;
  uint8_t val;

  DBGSTART

  /* point to per model memory layout */
  idx = 0;
  while(layouts[idx].model!=NULL && strcmp(dev->model->name,layouts[idx].model)!=0)
    {
      if(strcmp(dev->model->name,layouts[idx].model)!=0)
        idx++;
    }
  if(layouts[idx].model==NULL)
    {
      DBG(DBG_error, "%s: failed to find memory layout for model %s!\n", __FUNCTION__, dev->model->name);
      return SANE_STATUS_INVAL;
    }

  /* CLKSET and DRAMSEL */
  val = layouts[idx].dramsel;
  RIE (sanei_genesys_write_register (dev, REG0B, val));
  dev->reg[reg_0x0b].value = val;

  /* prevent further writings by bulk write register */
  dev->reg[reg_0x0b].address = 0x00;

  /* setup base address for shading and scanned data. */
  for(i=0;i<10;i++)
    {
      sanei_genesys_write_register (dev, 0xe0+i, layouts[idx].rx[i]);
    }

  DBGCOMPLETED;
  return status;
}

/* *
 * initialize ASIC from power on condition
 */
#ifndef UNIT_TESTING
static
#endif
SANE_Status
gl846_boot (Genesys_Device * dev, SANE_Bool cold)
{
  SANE_Status status;
  uint8_t val;

  DBGSTART;

  /* reset ASIC if cold boot */
  if(cold)
    {
      RIE (sanei_genesys_write_register (dev, 0x0e, 0x01));
      RIE (sanei_genesys_write_register (dev, 0x0e, 0x00));
    }

  if(dev->usb_mode == 1)
    {
      val = 0x14;
    }
  else
    {
      val = 0x11;
    }
  RIE (sanei_genesys_write_0x8c (dev, 0x0f, val));

  /* test CHKVER */
  RIE (sanei_genesys_read_register (dev, REG40, &val));
  if (val & REG40_CHKVER)
    {
      RIE (sanei_genesys_read_register (dev, 0x00, &val));
      DBG (DBG_info, "%s: reported version for genesys chip is 0x%02x\n", __FUNCTION__, val);
    }

  /* Set default values for registers */
  gl846_init_registers (dev);

  /* Write initial registers */
  RIE (dev->model->cmd_set->bulk_write_register (dev, dev->reg, GENESYS_GL846_MAX_REGS));

  /* Enable DRAM by setting a rising edge on bit 3 of reg 0x0b */
  val = dev->reg[reg_0x0b].value & REG0B_DRAMSEL;
  val = (val | REG0B_ENBDRAM);
  RIE (sanei_genesys_write_register (dev, REG0B, val));
  dev->reg[reg_0x0b].value = val;

  /* CIS_LINE */
  if (dev->model->is_cis)
    {
      SETREG (0x08, REG08_CIS_LINE);
      RIE (sanei_genesys_write_register (dev, 0x08, dev->reg[reg_0x08].value));
    }

  /* set up clocks */
  RIE (sanei_genesys_write_0x8c (dev, 0x10, 0x0e));
  RIE (sanei_genesys_write_0x8c (dev, 0x13, 0x0e));

  /* setup gpio */
  RIE (gl846_init_gpio (dev));

  /* setup internal memory layout */
  RIE (gl846_init_memory_layout (dev));

  SETREG (0xf8, 0x05);
  RIE (sanei_genesys_write_register (dev, 0xf8, dev->reg[reg_0xf8].value));

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

/**
 * initialize backend and ASIC : registers, motor tables, and gamma tables
 * then ensure scanner's head is at home
 */
#ifndef UNIT_TESTING
static
#endif
SANE_Status gl846_init (Genesys_Device * dev)
{
  SANE_Status status;

  DBG_INIT ();
  DBGSTART;

  status=sanei_genesys_asic_init(dev, GENESYS_GL846_MAX_REGS);

  DBGCOMPLETED;
  return status;
}

static SANE_Status
gl846_update_hardware_sensors (Genesys_Scanner * s)
{
  /* do what is needed to get a new set of events, but try to not lose
     any of them.
   */
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t val;
  uint8_t scan, file, email, copy;
  switch(s->dev->model->gpo_type)
    {
      default:
        scan=0x01;
        file=0x02;
        email=0x04;
        copy=0x08;
    }
  RIE (sanei_genesys_read_register (s->dev, REG6D, &val));

  if (s->val[OPT_SCAN_SW].b == s->last_val[OPT_SCAN_SW].b)
    s->val[OPT_SCAN_SW].b = (val & scan) == 0;
  if (s->val[OPT_FILE_SW].b == s->last_val[OPT_FILE_SW].b)
    s->val[OPT_FILE_SW].b = (val & file) == 0;
  if (s->val[OPT_EMAIL_SW].b == s->last_val[OPT_EMAIL_SW].b)
    s->val[OPT_EMAIL_SW].b = (val & email) == 0;
  if (s->val[OPT_COPY_SW].b == s->last_val[OPT_COPY_SW].b)
    s->val[OPT_COPY_SW].b = (val & copy) == 0;

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
gl846_search_strip (Genesys_Device * dev, SANE_Bool forward, SANE_Bool black)
{
  unsigned int pixels, lines, channels;
  SANE_Status status;
  Genesys_Register_Set local_reg[GENESYS_GL846_MAX_REGS];
  size_t size;
  uint8_t *data;
  int steps, depth, dpi;
  unsigned int pass, count, found, x, y;
  char title[80];
  Genesys_Register_Set *r;

  DBG (DBG_proc, "gl846_search_strip %s %s\n", black ? "black" : "white",
       forward ? "forward" : "reverse");

  status = gl846_set_fe (dev, AFE_SET);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
           "gl846_search_strip: gl846_set_fe() failed: %s\n",
           sane_strstatus(status));
      return status;
    }

  status = gl846_stop_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl846_search_strip: failed to stop: %s\n",
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
  /* 10 MM */
  /* lines = (10 * dpi) / MM_PER_INCH; */
  /* shading calibation is done with dev->motor.base_ydpi */
  lines = (dev->model->shading_lines * dpi) / dev->motor.base_ydpi;
  depth = 8;
  pixels = (dev->sensor.sensor_pixels * dpi) / dev->sensor.optical_res;
  size = pixels * channels * lines * (depth / 8);
  data = malloc (size);
  if (!data)
    {
      DBG (DBG_error, "gl846_search_strip: failed to allocate memory\n");
      return SANE_STATUS_NO_MEM;
    }
  dev->scanhead_position_in_steps = 0;

  memcpy (local_reg, dev->reg,
	  GENESYS_GL846_MAX_REGS * sizeof (Genesys_Register_Set));

  status = gl846_init_scan_regs (dev,
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
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA);
  if (status != SANE_STATUS_GOOD)
    {
      free(data);
      DBG (DBG_error,
	   "gl846_search_strip: failed to setup for scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  /* set up for reverse or forward */
  r = sanei_genesys_get_address (local_reg, REG02);
  if (forward)
    r->value &= ~REG02_MTRREV;
  else
    r->value |= REG02_MTRREV;


  status = dev->model->cmd_set->bulk_write_register (dev, local_reg, GENESYS_GL846_MAX_REGS);
  if (status != SANE_STATUS_GOOD)
    {
      free(data);
      DBG (DBG_error,
	   "gl846_search_strip: Failed to bulk write registers: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl846_begin_scan (dev, local_reg, SANE_TRUE);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error,
	   "gl846_search_strip: failed to begin scan: %s\n",
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
	   "gl846_search_start_position: failed to read data: %s\n",
	   sane_strstatus (status));
      return status;
    }

  status = gl846_stop_action (dev);
  if (status != SANE_STATUS_GOOD)
    {
      free (data);
      DBG (DBG_error, "gl846_search_strip: gl846_stop_action failed\n");
      return status;
    }

  pass = 0;
  if (DBG_LEVEL >= DBG_data)
    {
      sprintf (title, "search_strip_%s_%s%02d.pnm",
	       black ? "black" : "white", forward ? "fwd" : "bwd", (int)pass);
      sanei_genesys_write_pnm_file (title, data, depth, channels, pixels,
				    lines);
    }

  /* loop until strip is found or maximum pass number done */
  found = 0;
  while (pass < 20 && !found)
    {
      status = dev->model->cmd_set->bulk_write_register (dev, local_reg, GENESYS_GL846_MAX_REGS);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error,
	       "gl846_search_strip: Failed to bulk write registers: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      /* now start scan */
      status = gl846_begin_scan (dev, local_reg, SANE_TRUE);
      if (status != SANE_STATUS_GOOD)
	{
	  free (data);
	  DBG (DBG_error,
	       "gl846_search_strip: failed to begin scan: %s\n",
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
	       "gl846_search_start_position: failed to read data: %s\n",
	       sane_strstatus (status));
	  return status;
	}

      status = gl846_stop_action (dev);
      if (status != SANE_STATUS_GOOD)
	{
	  free (data);
	  DBG (DBG_error, "gl846_search_strip: gl846_stop_action failed\n");
	  return status;
	}

      if (DBG_LEVEL >= DBG_data)
	{
	  sprintf (title, "search_strip_%s_%s%02d.pnm",
		   black ? "black" : "white", forward ? "fwd" : "bwd", (int)pass);
	  sanei_genesys_write_pnm_file (title, data, depth, channels,
					pixels, lines);
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
		  if (black && data[y * pixels + x] > 90)
		    {
		      count++;
		    }
		  /* when searching for white, detect black pixels */
		  if (!black && data[y * pixels + x] < 60)
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
		       "gl846_search_strip: strip found forward during pass %d at line %d\n",
		       pass, y);
		}
	      else
		{
		  DBG (DBG_data,
		       "gl846_search_strip: pixels=%d, count=%d (%d%%)\n",
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
		  if (black && data[y * pixels + x] > 90)
		    {
		      count++;
		    }
		  /* when searching for white, detect black pixels */
		  if (!black && data[y * pixels + x] < 60)
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
		   "gl846_search_strip: strip found backward during pass %d \n",
		   pass);
	    }
	  else
	    {
	      DBG (DBG_data,
		   "gl846_search_strip: pixels=%d, count=%d (%d%%)\n",
		   pixels, count, (100 * count) / pixels);
	    }
	}
      pass++;
    }
  free (data);
  if (found)
    {
      status = SANE_STATUS_GOOD;
      DBG (DBG_info, "gl846_search_strip: %s strip found\n",
	   black ? "black" : "white");
    }
  else
    {
      status = SANE_STATUS_UNSUPPORTED;
      DBG (DBG_info, "gl846_search_strip: %s strip not found\n",
	   black ? "black" : "white");
    }

  DBGCOMPLETED;
  return status;
}

/**
 * average dark pixels of a 8 bits scan
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

static SANE_Status
gl846_offset_calibration (Genesys_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  uint8_t *first_line, *second_line, reg04;
  unsigned int channels, bpp;
  char title[32];
  int pass = 0, avg, total_size;
  int topavg, bottomavg, resolution, lines;
  int top, bottom, black_pixels, pixels;

  DBGSTART;

  /* no gain nor offset for AKM AFE */
  RIE (sanei_genesys_read_register (dev, REG04, &reg04));
  if ((reg04 & REG04_FESET) == 0x02)
    {
      DBGCOMPLETED;
      return status;
    }

  /* offset calibration is always done in color mode */
  channels = 3;
  resolution=dev->sensor.optical_res;
  dev->calib_pixels = dev->sensor.sensor_pixels;
  lines=1;
  bpp=8;
  pixels= (dev->sensor.sensor_pixels*resolution) / dev->sensor.optical_res;
  black_pixels = (dev->sensor.black_pixels * resolution) / dev->sensor.optical_res;
  DBG (DBG_io2, "gl846_offset_calibration: black_pixels=%d\n", black_pixels);

  status = gl846_init_scan_regs (dev,
				 dev->calib_reg,
				 resolution,
				 resolution,
				 0,
				 0,
				 pixels,
				 lines,
				 bpp,
				 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl846_offset_calibration: failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }
  gl846_set_motor_power (dev->calib_reg, SANE_FALSE);

  /* allocate memory for scans */
  total_size = pixels * channels * lines * (bpp/8);	/* colors * bytes_per_color * scan lines */

  first_line = malloc (total_size);
  if (!first_line)
    return SANE_STATUS_NO_MEM;

  second_line = malloc (total_size);
  if (!second_line)
    {
      free (first_line);
      return SANE_STATUS_NO_MEM;
    }

  /* init gain */
  dev->frontend.gain[0] = 0;
  dev->frontend.gain[1] = 0;
  dev->frontend.gain[2] = 0;

  /* scan with no move */
  bottom = 10;
  dev->frontend.offset[0] = bottom;
  dev->frontend.offset[1] = bottom;
  dev->frontend.offset[2] = bottom;

  RIEF2 (gl846_set_fe(dev, AFE_SET), first_line, second_line);
  RIEF2 (dev->model->cmd_set->bulk_write_register (dev, dev->calib_reg, GENESYS_GL846_MAX_REGS), first_line, second_line);
  DBG (DBG_info, "gl846_offset_calibration: starting first line reading\n");
  RIEF2 (gl846_begin_scan (dev, dev->calib_reg, SANE_TRUE), first_line, second_line);
  RIEF2 (sanei_genesys_read_data_from_scanner (dev, first_line, total_size), first_line, second_line);
  if (DBG_LEVEL >= DBG_data)
   {
      snprintf(title,20,"offset%03d.pnm",bottom);
      sanei_genesys_write_pnm_file (title, first_line, bpp, channels, pixels, lines);
   }

  bottomavg = dark_average (first_line, pixels, lines, channels, black_pixels);
  DBG (DBG_io2, "gl846_offset_calibration: bottom avg=%d\n", bottomavg);

  /* now top value */
  top = 255;
  dev->frontend.offset[0] = top;
  dev->frontend.offset[1] = top;
  dev->frontend.offset[2] = top;
  RIEF2 (gl846_set_fe(dev, AFE_SET), first_line, second_line);
  RIEF2 (dev->model->cmd_set->bulk_write_register (dev, dev->calib_reg, GENESYS_GL846_MAX_REGS), first_line, second_line);
  DBG (DBG_info, "gl846_offset_calibration: starting second line reading\n");
  RIEF2 (gl846_begin_scan (dev, dev->calib_reg, SANE_TRUE), first_line, second_line);
  RIEF2 (sanei_genesys_read_data_from_scanner (dev, second_line, total_size), first_line, second_line);

  topavg = dark_average (second_line, pixels, lines, channels, black_pixels);
  DBG (DBG_io2, "gl846_offset_calibration: top avg=%d\n", topavg);

  /* loop until acceptable level */
  while ((pass < 32) && (top - bottom > 1))
    {
      pass++;

      /* settings for new scan */
      dev->frontend.offset[0] = (top + bottom) / 2;
      dev->frontend.offset[1] = (top + bottom) / 2;
      dev->frontend.offset[2] = (top + bottom) / 2;

      /* scan with no move */
      RIEF2 (gl846_set_fe(dev, AFE_SET), first_line, second_line);
      RIEF2 (dev->model->cmd_set->bulk_write_register (dev, dev->calib_reg, GENESYS_GL846_MAX_REGS), first_line, second_line);
      DBG (DBG_info, "gl846_offset_calibration: starting second line reading\n");
      RIEF2 (gl846_begin_scan (dev, dev->calib_reg, SANE_TRUE), first_line, second_line);
      RIEF2 (sanei_genesys_read_data_from_scanner (dev, second_line, total_size), first_line, second_line);

      if (DBG_LEVEL >= DBG_data)
	{
	  sprintf (title, "offset%03d.pnm", dev->frontend.offset[1]);
	  sanei_genesys_write_pnm_file (title, second_line, bpp, channels, pixels, lines);
	}

      avg = dark_average (second_line, pixels, lines, channels, black_pixels);
      DBG (DBG_info, "gl846_offset_calibration: avg=%d offset=%d\n", avg,
	   dev->frontend.offset[1]);

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
  DBG (DBG_info, "gl846_offset_calibration: offset=(%d,%d,%d)\n", dev->frontend.offset[0], dev->frontend.offset[1], dev->frontend.offset[2]);

  /* cleanup before return */
  free (first_line);
  free (second_line);

  DBGCOMPLETED;
  return SANE_STATUS_GOOD;
}

static SANE_Status
gl846_coarse_gain_calibration (Genesys_Device * dev, int dpi)
{
  int pixels;
  int total_size;
  uint8_t *line, reg04;
  int i, j, channels;
  SANE_Status status = SANE_STATUS_GOOD;
  int max[3];
  float gain[3],coeff;
  int val, code, lines;
  int resolution;
  int bpp;

  DBG (DBG_proc, "gl846_coarse_gain_calibration: dpi = %d\n", dpi);

  /* no gain nor offset for AKM AFE */
  RIE (sanei_genesys_read_register (dev, REG04, &reg04));
  if ((reg04 & REG04_FESET) == 0x02)
    {
      DBGCOMPLETED;
      return status;
    }

  /* coarse gain calibration is always done in color mode */
  channels = 3;

  /* follow CKSEL */
  if(dev->settings.xres<dev->sensor.optical_res)
    {
      coeff=0.9;
      /*resolution=dev->sensor.optical_res/2;*/
      resolution=dev->sensor.optical_res;
    }
  else
    {
      resolution=dev->sensor.optical_res;
      coeff=1.0;
    }
  lines=10;
  bpp=8;
  pixels = (dev->sensor.sensor_pixels * resolution) / dev->sensor.optical_res;

  status = gl846_init_scan_regs (dev,
				 dev->calib_reg,
				 resolution,
				 resolution,
				 0,
				 0,
				 pixels,
                                 lines,
                                 bpp,
                                 channels,
				 dev->settings.color_filter,
				 SCAN_FLAG_DISABLE_SHADING |
				 SCAN_FLAG_DISABLE_GAMMA |
				 SCAN_FLAG_SINGLE_LINE |
				 SCAN_FLAG_IGNORE_LINE_DISTANCE);
  gl846_set_motor_power (dev->calib_reg, SANE_FALSE);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error,
	   "gl846_coarse_calibration: failed to setup scan: %s\n",
	   sane_strstatus (status));
      return status;
    }

  RIE (dev->model->cmd_set->bulk_write_register
       (dev, dev->calib_reg, GENESYS_GL846_MAX_REGS));

  total_size = pixels * channels * (16/bpp) * lines;

  line = malloc (total_size);
  if (!line)
    return SANE_STATUS_NO_MEM;

  RIEF (gl846_set_fe(dev, AFE_SET), line);
  RIEF (gl846_begin_scan (dev, dev->calib_reg, SANE_TRUE), line);
  RIEF (sanei_genesys_read_data_from_scanner (dev, line, total_size), line);

  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file ("coarse.pnm", line, bpp, channels, pixels, lines);

  /* average value on each channel */
  for (j = 0; j < channels; j++)
    {
      max[j] = 0;
      for (i = pixels/4; i < (pixels*3/4); i++)
	{
	  if (dev->model->is_cis)
	    val = line[i + j * pixels];
	  else
	    val = line[i * channels + j];

	    max[j] += val;
	}
      max[j] = max[j] / (pixels/2);

      gain[j] = ((float) dev->sensor.gain_white_ref*coeff) / max[j];

      /* turn logical gain value into gain code, checking for overflow */
      code = 283 - 208 / gain[j];
      if (code > 255)
	code = 255;
      else if (code < 0)
	code = 0;
      dev->frontend.gain[j] = code;

      DBG (DBG_proc,
	   "gl846_coarse_gain_calibration: channel %d, max=%d, gain = %f, setting:%d\n",
	   j, max[j], gain[j], dev->frontend.gain[j]);
    }

  if (dev->model->is_cis)
    {
      if (dev->frontend.gain[0] > dev->frontend.gain[1])
	dev->frontend.gain[0] = dev->frontend.gain[1];
      if (dev->frontend.gain[0] > dev->frontend.gain[2])
	dev->frontend.gain[0] = dev->frontend.gain[2];
      dev->frontend.gain[2] = dev->frontend.gain[1] = dev->frontend.gain[0];
    }

  free (line);

  RIE (gl846_stop_action (dev));

  status=gl846_slow_back_home (dev, SANE_TRUE);

  DBGCOMPLETED;
  return status;
}


/** the gl846 command set */
static Genesys_Command_Set gl846_cmd_set = {
  "gl846-generic",		/* the name of this set */

  gl846_init,
  NULL,
  gl846_init_regs_for_coarse_calibration,
  gl846_init_regs_for_shading,
  gl846_init_regs_for_scan,

  gl846_get_filter_bit,
  gl846_get_lineart_bit,
  gl846_get_bitset_bit,
  gl846_get_gain4_bit,
  gl846_get_fast_feed_bit,
  gl846_test_buffer_empty_bit,
  gl846_test_motor_flag_bit,

  gl846_bulk_full_size,

  gl846_set_fe,
  gl846_set_powersaving,
  gl846_save_power,

  gl846_set_motor_power,
  gl846_set_lamp_power,

  gl846_begin_scan,
  gl846_end_scan,

  sanei_genesys_send_gamma_table,

  gl846_search_start_position,

  gl846_offset_calibration,
  gl846_coarse_gain_calibration,
  gl846_led_calibration,

  gl846_slow_back_home,

  sanei_genesys_bulk_write_register,
  NULL,
  gl846_bulk_read_data,

  gl846_update_hardware_sensors,

  NULL,
  NULL,
  NULL,
  gl846_search_strip,

  sanei_genesys_is_compatible_calibration,
  NULL,
  gl846_send_shading_data,
  gl846_calculate_current_setup,
  gl846_boot,
  NULL
};

SANE_Status
sanei_gl846_init_cmd_set (Genesys_Device * dev)
{
  dev->model->cmd_set = &gl846_cmd_set;
  return SANE_STATUS_GOOD;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
