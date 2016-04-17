/*
  Copyright (C) 2000 by Adrian Perez Jorge

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.  
 */

/* Developers:

       Adrian Perez Jorge (APJ) - 
            Creator of the original HP4200C backend code.
	    adrianpj@easynews.com
				  
       Andrew John Lewis  (AJL) - 
	    lewi0235@tc.umn.edu

       Arnar Mar Hrafnkelsson (AMH) -
            addi@umich.edu

       Frank Zago
            some cleanups and integration into SANE

       Henning Meier-Geinitz <henning@meier-geinitz.de>
            more cleanups, bug fixes

TODO:

   - support more scanning resolutions.
   - support different color depths.
   - support gray and lineart.
   - improve scanning speed. Compute scanning parameters based on the
     image size and the scanner-to-host bandwidth.
   - improve image quality.
   - fix problem concerning mangled images
  
*/

#define BUILD 2
#define BACKEND_NAME hp4200

#include "../include/sane/config.h"

#include <sys/ioctl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <assert.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_pv8630.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_backend.h"
#include "hp4200.h"

#include "hp4200_lm9830.c"

#define HP4200_CONFIG_FILE "hp4200.conf"

/*--------------------------------------------------------------------------*/

#if 0
/* Some of these resolution need work in color shifting. */
static const SANE_Int dpi_list[] =
  { 8, 50, 75, 100, 150, 200, 300, 400, 600 };
#else
static const SANE_Int dpi_list[] = { 4, 75, 150, 300, 600 };
#endif
static SANE_Range x_range = { SANE_FIX (0), SANE_FIX (8.5 * MM_PER_INCH), 0 };
static SANE_Range y_range =
  { SANE_FIX (0), SANE_FIX (11.75 * MM_PER_INCH), 0 };
static const SANE_Range u8_range = { 0, 255, 0 };

struct coarse_t
{
  int min_red;
  int min_green;
  int min_blue;
  int max_red;
  int max_green;
  int max_blue;
  int red_gain;
  int red_offset;
  int green_gain;
  int green_offset;
  int blue_gain;
  int blue_offset;
};

static const double hdpi_mapping[8] = { 1, 1.5, 2, 3, 4, 6, 8, 12 };

static HP4200_Device *first_device = NULL;	/* device list head */
static int n_devices = 0;	/* the device count */
static const SANE_Device **devlist = NULL;

static unsigned char
getreg (HP4200_Scanner * s, unsigned char reg)
{
  unsigned char reg_value;

  if ((reg > 0x08) && (reg < 0x5b))
    return (unsigned char) LOBYTE (s->regs[reg]);
  else
    {
      lm9830_read_register (s->fd, reg, &reg_value);
      return reg_value;
    }
}

static void
setreg (HP4200_Scanner * s, unsigned char reg, unsigned char reg_value)
{
  s->regs[reg] = reg_value;	/* dirty bit should be clear with this */
  if ((reg < 0x08) || (reg > 0x5b))
    {
      lm9830_write_register (s->fd, reg, reg_value);
    }
}

static void
setbits (HP4200_Scanner * s, unsigned char reg, unsigned char bitmap)
{
  s->regs[reg] = (s->regs[reg] & 0xff) | bitmap;
  if ((reg < 0x08) || (reg > 0x5b))
    {
      lm9830_write_register (s->fd, reg, LOBYTE (s->regs[reg]));
    }
}

static void
clearbits (HP4200_Scanner * s, unsigned char reg, unsigned char mask)
{
  s->regs[reg] = (s->regs[reg] & ~mask) & 0xff;
  if ((reg < 0x08) || (reg > 0x5b))
    {
      lm9830_write_register (s->fd, reg, LOBYTE (s->regs[reg]));
    }
}

static int
cache_write (HP4200_Scanner * s)
{
  int i;
#ifdef DEBUG_REG_CACHE
  int counter = 0;
#endif

  DBG (DBG_proc, "Writing registers\n");

  for (i = 0; i < 0x80; i++)
    if (!(s->regs[i] & 0x100))
      {				/* modified register */
#ifdef DEBUG_REG_CACHE
	fprintf (stderr, "%.2x", i);
	if (counter == 8)
	  fprintf (stderr, "\n");
	else
	  fprintf (stderr, ", ");
	counter = (counter + 1) % 9;
#endif
	lm9830_write_register (s->fd, i, s->regs[i]);
	s->regs[i] |= 0x100;	/* register is updated */
      }
  return 0;
}

/*
 * HP4200-dependent register initialization.
 */

static int
hp4200_init_registers (HP4200_Scanner * s)
{
  /* set up hardware parameters */

  s->hw_parms.crystal_frequency = 48000000;
  s->hw_parms.SRAM_size = 128;	/* Kb */
  s->hw_parms.scan_area_width = 5100;	/* pixels */
  s->hw_parms.scan_area_length = 11;	/* inches */
  s->hw_parms.min_pixel_data_buffer_limit = 1024;	/* bytes */
  s->hw_parms.sensor_line_separation = 4;	/* lines */
  s->hw_parms.sensor_max_integration_time = 12;	/* milliseconds */
  s->hw_parms.home_sensor = 2;
  s->hw_parms.sensor_resolution = 1;	/* 600 dpi */
  s->hw_parms.motor_full_steps_per_inch = 300;
  s->hw_parms.motor_max_speed = 1.4;	/* inches/second */
  s->hw_parms.num_tr_pulses = 1;
  s->hw_parms.guard_band_duration = 1;
  s->hw_parms.pulse_duration = 3;
  s->hw_parms.fsteps_25_speed = 3;
  s->hw_parms.fsteps_50_speed = 3;
  s->hw_parms.target_value.red = 1000;
  s->hw_parms.target_value.green = 1000;
  s->hw_parms.target_value.blue = 1000;

  {
    int i;

    /*
     * we are using a cache-like data structure so registers whose
     * values were written to the lm9830 and aren't volatile, have
     * bit 0x100 activated.  This bit must be cleared if you want the
     * value to be written to the chip once cache_write() is called.
     */

    /* clears the registers cache */
    memset (s->regs, 0, sizeof (s->regs));

    /*
     * registers 0x00 - 0x07 are non-cacheable/volatile, so don't
     * read the values using the cache.  Instead use direct functions
     * to read/write registers.
     */

    for (i = 0; i < 0x08; i++)
      s->regs[i] = 0x100;
  }

  setreg (s, 0x70, 0x70);	/* noise filter */

  setreg (s, 0x0b,
	  INPUT_SIGNAL_POLARITY_NEGATIVE |
	  CDS_ON |
	  SENSOR_STANDARD |
	  SENSOR_RESOLUTION_600 | LINE_SKIPPING_COLOR_PHASE_DELAY (0));

  setreg (s, 0x0c,
	  PHI1_POLARITY_POSITIVE |
	  PHI2_POLARITY_POSITIVE |
	  RS_POLARITY_POSITIVE |
	  CP1_POLARITY_POSITIVE |
	  CP2_POLARITY_POSITIVE |
	  TR1_POLARITY_NEGATIVE | TR2_POLARITY_NEGATIVE);

  setreg (s, 0x0d,
	  PHI1_ACTIVE |
	  PHI2_ACTIVE |
	  RS_ACTIVE |
	  CP1_ACTIVE |
	  CP2_OFF |
	  TR1_ACTIVE |
	  TR2_OFF | NUMBER_OF_TR_PULSES (s->hw_parms.num_tr_pulses));

  setreg (s, 0x0e,
	  TR_PULSE_DURATION (s->hw_parms.pulse_duration) |
	  TR_PHI1_GUARDBAND_DURATION (s->hw_parms.guard_band_duration));

  /* for pixel rate timing */
  setreg (s, 0x0f, 6);
  setreg (s, 0x10, 23);
  setreg (s, 0x11, 1);
  setreg (s, 0x12, 3);
  setreg (s, 0x13, 3);		/* 0 */
  setreg (s, 0x14, 5);		/* 0 */
  setreg (s, 0x15, 0);
  setreg (s, 0x16, 0);
  setreg (s, 0x17, 11);
  setreg (s, 0x18, 2);		/* 1 */

  setreg (s, 0x19, CIS_TR1_TIMING_OFF | FAKE_OPTICAL_BLACK_PIXELS_OFF);

  setreg (s, 0x1a, 0);
  setreg (s, 0x1b, 0);

  setreg (s, 0x1c, 0x0d);
  setreg (s, 0x1d, 0x21);

  setreg (s, 0x27, TR_RED_DROP (0) | TR_GREEN_DROP (0) | TR_BLUE_DROP (0));

  setreg (s, 0x28, 0x00);

  setreg (s, 0x29, ILLUMINATION_MODE (1));
  setreg (s, 0x2a, HIBYTE (0));	/* 0 */
  setreg (s, 0x2b, LOBYTE (0));	/* 0 */

  setreg (s, 0x2c, HIBYTE (16383));
  setreg (s, 0x2d, LOBYTE (16383));

  setreg (s, 0x2e, HIBYTE (2));	/* 2 */
  setreg (s, 0x2f, LOBYTE (2));	/* 1 */

  setreg (s, 0x30, HIBYTE (0));
  setreg (s, 0x31, LOBYTE (0));

  setreg (s, 0x32, HIBYTE (0));
  setreg (s, 0x33, LOBYTE (0));

  setreg (s, 0x34, HIBYTE (32));
  setreg (s, 0x35, LOBYTE (32));

  setreg (s, 0x36, HIBYTE (48));
  setreg (s, 0x37, LOBYTE (48));

  setreg (s, 0x42, EPP_MODE | PPORT_DRIVE_CURRENT (3));

  setreg (s, 0x43,
	  RAM_SIZE_128 |
	  SRAM_DRIVER_CURRENT (3) | SRAM_BANDWIDTH_8 | SCANNING_FULL_DUPLEX);

  setreg (s, 0x45,
	  MICRO_STEPPING |
	  CURRENT_SENSING_PHASES (2) |
	  PHASE_A_POLARITY_POSITIVE |
	  PHASE_B_POLARITY_POSITIVE | STEPPER_MOTOR_OUTPUT);

  setreg (s, 0x4a, HIBYTE (100));
  setreg (s, 0x4b, LOBYTE (100));

  setreg (s, 0x4c, HIBYTE (0));
  setreg (s, 0x4d, LOBYTE (0));

  /* resume scan threshold */
  setreg (s, 0x4f, 64);
  /* steps to reverse */
  setreg (s, 0x50, 40);
  setreg (s, 0x51,
	  ACCELERATION_PROFILE_STOPPED (3) |
	  ACCELERATION_PROFILE_25P (s->hw_parms.fsteps_25_speed) |
	  ACCELERATION_PROFILE_50P (s->hw_parms.fsteps_50_speed));
  setreg (s, 0x54, NON_REVERSING_EXTRA_LINES (0) | FIRST_LINE_TO_PROCESS (0));
  setreg (s, 0x55, KICKSTART_STEPS (0) | HOLD_CURRENT_TIMEOUT (2));

  /* stepper PWM frequency */
  setreg (s, 0x56, 8);
  /* stepper pwm duty cycle */
  setreg (s, 0x57, 23);

  setreg (s, 0x58,
	  PAPER_SENSOR_1_POLARITY_HIGH |
	  PAPER_SENSOR_1_TRIGGER_EDGE |
	  PAPER_SENSOR_1_NO_STOP_SCAN |
	  PAPER_SENSOR_2_POLARITY_HIGH |
	  PAPER_SENSOR_2_TRIGGER_EDGE | PAPER_SENSOR_2_STOP_SCAN);
  setreg (s, 0x59,
	  MISCIO_1_TYPE_OUTPUT |
	  MISCIO_1_POLARITY_HIGH |
	  MISCIO_1_TRIGGER_EDGE |
	  MISCIO_1_OUTPUT_STATE_HIGH |
	  MISCIO_2_TYPE_OUTPUT |
	  MISCIO_2_POLARITY_HIGH |
	  MISCIO_2_TRIGGER_EDGE | MISCIO_2_OUTPUT_STATE_HIGH);

  return 0;
}

#ifdef DEBUG
static int
dump_register_cache (HP4200_Scanner * s)
{
  int i;

  for (i = 0; i < 0x80; i++)
    {
      fprintf (stderr, "%.2x:0x%.2x", i, s->regs[i]);
      if ((i + 1) % 8)
	fprintf (stderr, ", ");
      else
	fprintf (stderr, "\n");
    }
  fputs ("", stderr);
  return 0;
}
#endif

/*
 * returns the scanner head to home position
 */

static int
hp4200_goto_home (HP4200_Scanner * s)
{
  unsigned char cmd_reg;
  unsigned char status_reg;
  unsigned char old_paper_sensor_reg;

  cmd_reg = getreg (s, 0x07);
  if (cmd_reg != 2)
    {
      unsigned char paper_sensor_reg;
      unsigned char sensor_bit[2] = { 0x02, 0x10 };
      /* sensor head is not returning */

      /* let's see if it's already at home */
      /* first put paper (head) sensor level sensitive */
      paper_sensor_reg = getreg (s, 0x58);
      old_paper_sensor_reg = paper_sensor_reg;
      paper_sensor_reg &= ~sensor_bit[s->hw_parms.home_sensor - 1];
      setreg (s, 0x58, paper_sensor_reg);
      cache_write (s);

      /* if the scan head is not at home then move motor backwards */
      status_reg = getreg (s, 0x02);
      setreg (s, 0x58, old_paper_sensor_reg);
      cache_write (s);
      if (!(status_reg & s->hw_parms.home_sensor))
	{
	  setreg (s, 0x07, 0x08);
	  usleep (10 * 1000);
	  setreg (s, 0x07, 0x00);
	  usleep (10 * 1000);
	  setreg (s, 0x07, 0x02);
	}
    }
  return 0;
}

#define HP4200_CHECK_INTERVAL 1000	/* usecs between status checks */
static int
hp4200_wait_homed (HP4200_Scanner * s)
{
  unsigned char cmd_reg;

  cmd_reg = getreg (s, 0x07);
  while (cmd_reg != 0)
    {
      usleep (HP4200_CHECK_INTERVAL);
      cmd_reg = getreg (s, 0x07);
    }
  return 0;
}

static int
compute_fastfeed_step_size (unsigned long crystal_freq, int mclk,
			    float max_speed, int steps_per_inch,
			    int color_mode)
{
  int aux;
  int r;

  if (color_mode == 0)
    r = 24;
  else
    r = 8;

  aux = floor (crystal_freq / ((double) mclk * max_speed * 4.0 *
			       steps_per_inch * r));


  if (aux < 2)
    aux = 2;
  return aux;
}

static SANE_Status
read_available_data (HP4200_Scanner * s, SANE_Byte * buffer,
		     size_t * byte_count)
{
  SANE_Status status;
  unsigned char scankb1;
  unsigned char scankb2;
  size_t to_read;
  size_t really_read;
  size_t chunk;

  assert (buffer != NULL);

  *byte_count = 0;
  do
    {
      scankb1 = getreg (s, 0x01);
      scankb2 = getreg (s, 0x01);
      if (s->aborted_by_user)
	return SANE_STATUS_CANCELLED;
    }
  while ((scankb1 != scankb2) || (scankb1 < 12));

  to_read = scankb1 * 1024;

  while (to_read)
    {
      if (s->aborted_by_user)
	return SANE_STATUS_CANCELLED;
      chunk = (to_read > 0xffff) ? 0xffff : to_read;

      sanei_pv8630_write_byte (s->fd, PV8630_REPPADDRESS, 0x00);
      sanei_pv8630_prep_bulkread (s->fd, chunk);
      really_read = chunk;
      if ((status = sanei_usb_read_bulk (s->fd, buffer, &really_read)) !=
	  SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "sanei_usb_read_bulk failed (%s)\n",
	       sane_strstatus (status));
	  return status;
	}
      if (really_read > to_read)
	{
	  DBG (DBG_error, "USB stack read more bytes than requested!\n");
	  return SANE_STATUS_IO_ERROR;
	}
      *byte_count += really_read;
      buffer += really_read;
      to_read -= really_read;
#ifdef DEBUG
      fprintf (stderr, "read %d bytes\n", really_read);
#endif
    }
  return SANE_STATUS_GOOD;
}

#ifdef unused
static int
compute_datalink_bandwidth (HP4200_Scanner * s)
{
  int line_size;
  int pause_limit;
  unsigned int color_mode;

  /*
   * Line size for 8 bpp, the entire scan area width (plus the
   * status byte) at optical resolution.
   */

  if (s->user_parms.color)
    {
      line_size = 3 * s->hw_parms.scan_area_width + 1;
      color_mode = 0;
      setreg (s, 0x26, color_mode);	/* 3 channel pixel rate color */
    }
  else
    {
      line_size = s->hw_parms.scan_area_width + 1;
      color_mode = 4;
      setreg (s, 0x26, 0x08 | color_mode);	/* 1 channel mode A (green) */
    }
  setreg (s, 0x09, (3 << 3));	/* h-divider = 1, 8 bpp */

  {
    int first_white_pixel;
    unsigned int line_end;

    first_white_pixel = s->hw_parms.sensor_pixel_end - 10;
    line_end = first_white_pixel + s->hw_parms.scan_area_width;
    if (line_end > (s->hw_parms.sensor_num_pixels - 20))
      line_end = s->hw_parms.sensor_num_pixels - 20;

    setreg (s, 0x1c, HIBYTE (s->hw_parms.sensor_pixel_start));
    setreg (s, 0x1d, LOBYTE (s->hw_parms.sensor_pixel_end));
    setreg (s, 0x1e, HIBYTE (first_white_pixel));
    setreg (s, 0x1f, LOBYTE (first_white_pixel));
    setreg (s, 0x20, HIBYTE (s->hw_parms.sensor_num_pixels));
    setreg (s, 0x21, LOBYTE (s->hw_parms.sensor_num_pixels));
    setreg (s, 0x22, getreg (s, 0x1e));
    setreg (s, 0x23, getreg (s, 0x1f));
    setreg (s, 0x24, HIBYTE (line_end));
    setreg (s, 0x25, LOBYTE (line_end));
  }

  /*
   * During transfer rate calculation don't forward scanner sensor.
   * Stay in the calibration region.
   */

  setreg (s, 0x4f, 0);
  clearbits (s, 0x45, 0x10);

  /*
   * Pause the scan when memory is full.
   */

  pause_limit = s->hw_parms.SRAM_size - (line_size / 1024) - 1;
  setreg (s, 0x4e, pause_limit & 0xff);

  s->mclk = compute_min_mclk (s->hw_parms.SRAM_bandwidth,
			      s->hw_parms.crystal_frequency);


  /*
   * Set step size to fast speed.
   */

  {
    int step_size;

    step_size =
      compute_fastfeed_step_size (s->hw_parms.crystal_frequency,
				  s->mclk,
				  s->hw_parms.scan_bar_max_speed,
				  s->hw_parms.motor_full_steps_per_inch,
				  color_mode);

    setreg (s, 0x46, HIBYTE (step_size));
    setreg (s, 0x47, LOBYTE (step_size));
    setreg (s, 0x48, HIBYTE (step_size));
    setreg (s, 0x49, LOBYTE (step_size));
  }

  cache_write (s);

  /*  dump_register_cache (s); */

  /*
   * scan during 1 sec. aprox.
   */

  setreg (s, 0x07, 0x08);
  setreg (s, 0x07, 0x03);

  {
    struct timeval tv_before;
    struct timeval tv_after;
    int elapsed_time_ms = 0;
    long bytes_read_total;
    SANE_Byte *buffer;

    buffer = malloc (2 * 98304);	/* check this */
    if (!buffer)
      {
	DBG (DBG_error, "compute_datalink_bandwidth: malloc failed\n");
	return 0;
      }
    bytes_read_total = 0;
    gettimeofday (&tv_before, NULL);
    do
      {
	size_t bytes_read;
	SANE_Status status;

	status = read_available_data (s, buffer, &bytes_read);
	if (status != SANE_STATUS_GOOD)
	  {
	    DBG (DBG_error, "read_available_data failed (%s)\n",
		 sane_strstatus (status));
	    return 0;
	  }
	bytes_read_total += bytes_read;
	gettimeofday (&tv_after, NULL);
	elapsed_time_ms = (tv_after.tv_sec - tv_before.tv_sec) * 1000;
	elapsed_time_ms += (tv_after.tv_usec - tv_before.tv_usec) / 1000;
      }
    while (elapsed_time_ms < 1000);

    setreg (s, 0x07, 0x00);
    free (buffer);

    s->msrd_parms.datalink_bandwidth = bytes_read_total /
      (elapsed_time_ms / 1000);

#ifdef DEBUG
    fprintf (stderr, "PC Transfer rate = %d bytes/sec. (%ld/%d)\n",
	     s->msrd_parms.datalink_bandwidth, bytes_read_total,
	     elapsed_time_ms);
#endif
  }
  return 0;
}
#endif

static void
compute_first_gain_offset (int target, int max, int min, int *gain,
			   int *offset, int *max_gain, int *min_offset)
{
  *gain = (int) 15.0 *(target / (max - min) - 0.933);
  *offset = (int) (-1.0 * min / (512.0 * 0.0195));
  if (*gain >= 32)
    {
      *gain = (int) 15.0 *(target / 3.0 / (max - min) - 0.933);
      *offset = (int) -3.0 * min / (512.0 * 0.0195);
    }
  if (*gain < 0)
    *gain = 0;
  else if (*gain > 63)
    *gain = 63;

  if (*offset < -31)
    *offset = -31;
  else if (*offset > 31)
    *offset = 31;

  *max_gain = 63;
  *min_offset = -31;
}

#define DATA_PORT_READ (1 << 5)
#define DATA_PORT_WRITE 0

static int
write_gamma (HP4200_Scanner * s)
{
  SANE_Status status;
  int color;
  int i;
  unsigned char gamma[1024];
  unsigned char read_gamma[1024];
  int retval;
  size_t to_read;
  size_t to_write;

  for (color = 0; color < 3; color++)
    {
      for (i = 0; i < 1024; i++)
	gamma[i] = s->user_parms.gamma[color][i];

      setreg (s, 0x03, color << 1);
      setreg (s, 0x04, DATA_PORT_WRITE);
      setreg (s, 0x05, 0x00);
      sanei_pv8630_write_byte (s->fd, PV8630_REPPADDRESS, 0x06);
      sanei_pv8630_prep_bulkwrite (s->fd, sizeof (gamma));
      to_write = sizeof (gamma);
      sanei_usb_write_bulk (s->fd, gamma, &to_write);

      /* check if gamma vector was correctly written */

      setreg (s, 0x03, color << 1);
      setreg (s, 0x04, DATA_PORT_READ);
      setreg (s, 0x05, 0x00);
      sanei_pv8630_write_byte (s->fd, PV8630_REPPADDRESS, 0x06);
      sanei_pv8630_prep_bulkread (s->fd, sizeof (read_gamma));
      to_read = sizeof (read_gamma);
      status = sanei_usb_read_bulk (s->fd, read_gamma, &to_read);
      retval = memcmp (read_gamma, gamma, sizeof (read_gamma));
      if (retval != 0)
	{
	  DBG (DBG_error, "error: color %d has bad gamma table\n", color);
	}
#ifdef DEBUG
      else
	fprintf (stderr, "color %d gamma table is good\n", color);
#endif
    }

  return 0;
}

static int
write_default_offset_gain (HP4200_Scanner * s, SANE_Byte * gain_offset,
			   int size, int color)
{
  SANE_Byte *check_data;
  int retval;
  size_t to_read;
  size_t to_write;

  setreg (s, 0x03, (color << 1) | 1);
  setreg (s, 0x04, DATA_PORT_WRITE);
  setreg (s, 0x05, 0x00);
  sanei_pv8630_write_byte (s->fd, PV8630_REPPADDRESS, 0x06);
  sanei_pv8630_prep_bulkwrite (s->fd, size);
  to_write = size;
  sanei_usb_write_bulk (s->fd, gain_offset, &to_write);

  check_data = malloc (size);
  setreg (s, 0x03, (color << 1) | 1);
  setreg (s, 0x04, DATA_PORT_READ);
  setreg (s, 0x05, 0x00);
  sanei_pv8630_write_byte (s->fd, PV8630_REPPADDRESS, 0x06);
  sanei_pv8630_prep_bulkread (s->fd, size);
  to_read = size;
  sanei_usb_read_bulk (s->fd, check_data, &to_read);
  retval = memcmp (gain_offset, check_data, size);
  free (check_data);
  if (retval != 0)
    {
      DBG (DBG_error, "error: color %d has bad gain/offset table\n", color);
    }
#ifdef DEBUG
  else
    fprintf (stderr, "color %d gain/offset table is good\n", color);
#endif

  return 0;
}

static int
compute_gain_offset (int target, int max, int min, int *gain,
		     int *offset, int *max_gain, int *min_offset)
{
  int gain_stable;
  int is_unstable;

  gain_stable = 1;		/* unless the opposite is said */
  is_unstable = 0;

  if (max > target)
    {
      if (*gain > 0)
	{
	  (*gain)--;
	  *max_gain = *gain;
	  gain_stable = 0;
	  is_unstable |= 1;
	}
      else
	{
	  DBG (DBG_error, "error: integration time too long.\n");
	  return -1;
	}
    }
  else
    {
      if (*gain < *max_gain)
	{
	  (*gain)++;
	  gain_stable = 0;
	  is_unstable |= 1;
	}
    }

  if (min == 0)
    {
      if (*offset < 31)
	{
	  (*offset)++;
	  if (gain_stable)
	    *min_offset = *offset;
	  is_unstable |= 1;
	}
      else
	{
	  DBG (DBG_error, "error: max static has pixel value == 0\n");
	  return -1;
	}
    }
  else
    {
      if (*offset > *min_offset)
	{
	  (*offset)--;
	  is_unstable |= 1;
	}
    }
  return is_unstable;
}

static int
compute_bytes_per_line (int width_in_pixels, unsigned char hdpi_code,
			unsigned char pixel_packing,
			unsigned char data_mode,
			unsigned char AFE_operation, int m)
{
  const int dpi_qot_mul[] = { 1, 2, 1, 1, 1, 1, 1, 1 };
  const int dpi_qot_div[] = { 1, 3, 2, 3, 4, 6, 8, 12 };
  int pixels_per_line;
  int bytes_per_line;
  int pixels_per_byte;
  int status_bytes;
  const int pixels_per_byte_mapping[] = { 8, 4, 2, 1 };

  assert (hdpi_code <= 7);
  pixels_per_line = (width_in_pixels * dpi_qot_mul[hdpi_code]) /
    dpi_qot_div[hdpi_code];
  if ((width_in_pixels * dpi_qot_mul[hdpi_code]) % dpi_qot_div[hdpi_code])
    pixels_per_line++;


  status_bytes = (m == 0) ? 1 : m;

  if (data_mode == 1)
    pixels_per_byte = 1;	/* should be 0.5 but later
				   bytes_per_line will be multiplied
				   by 2, and also the number of status
				   bytes, that in this case should be
				   2.
				   umm.. maybe this should be done in
				   the cleaner way.
				 */
  else
    {
      assert (pixel_packing <= 3);
      pixels_per_byte = pixels_per_byte_mapping[pixel_packing];
    }

  switch (AFE_operation)
    {
    case PIXEL_RATE_3_CHANNELS:
      bytes_per_line = ((pixels_per_line * 3) / pixels_per_byte) +
	status_bytes;
      break;
    case MODEA_1_CHANNEL:
      bytes_per_line = (pixels_per_line / pixels_per_byte) + status_bytes;
      break;
    default:
      /* Not implemented! (yet?) and not used.
       * This case should not happen. */
      assert (0);
    }

  if (data_mode == 1)		/* see big note above */
    bytes_per_line *= 2;

  return bytes_per_line;
}

static int
compute_pause_limit (hardware_parameters_t * hw_parms, int bytes_per_line)
{
  int coef_size;
  const int coef_mapping[] = { 16, 32 };
  int pause_limit;

  coef_size = coef_mapping[hw_parms->sensor_resolution & 0x01];
  pause_limit = hw_parms->SRAM_size - coef_size - (bytes_per_line / 1024) - 1;

  if (pause_limit > 2)
    pause_limit -= 2;

  return pause_limit;
}

static int
compute_dpd (HP4200_Scanner * s, int step_size, int line_end)
{
  int tr, dpd;

  tr = 1 /* color mode */  *
    (line_end + ((s->hw_parms.num_tr_pulses + 1) *
		 (2 * s->hw_parms.guard_band_duration +
		  s->hw_parms.pulse_duration + 1) +
		 3 - s->hw_parms.num_tr_pulses));

  if (tr == 0)
    return 0;

  dpd = (((s->hw_parms.fsteps_25_speed * 4) +
	  (s->hw_parms.fsteps_50_speed * 2) +
	  s->hw_parms.steps_to_reverse) * 4 * step_size) % tr;
  dpd = tr - dpd;

  return dpd;
}

static SANE_Status
read_required_bytes (HP4200_Scanner * s, int required, SANE_Byte * buffer)
{
  int read_count = 0;
  unsigned char scankb1;
  unsigned char scankb2;
  size_t to_read;
  size_t really_read;
  size_t chunk;
  SANE_Status status;

  assert (buffer != NULL);

  while (required)
    {
      do
	{
	  scankb1 = getreg (s, 0x01);
	  scankb2 = getreg (s, 0x01);
	  if (s->aborted_by_user)
	    return SANE_STATUS_CANCELLED;
	}
      while ((scankb1 != scankb2) || (scankb1 < 12));

      to_read = min (required, (scankb1 * 1024));
      while (to_read)
	{
	  if (s->aborted_by_user)
	    return SANE_STATUS_CANCELLED;
	  chunk = (to_read > 0xffff) ? 0xffff : to_read;

	  sanei_pv8630_write_byte (s->fd, PV8630_REPPADDRESS, 0x00);
	  sanei_pv8630_prep_bulkread (s->fd, chunk);
	  really_read = chunk;
	  if ((status = sanei_usb_read_bulk (s->fd, buffer, &really_read)) !=
	      SANE_STATUS_GOOD)
	    {
	      DBG (DBG_error, "sanei_usb_read_bulk failed (%s)\n",
		   sane_strstatus (status));
	      return status;
	    }
	  if (really_read > chunk)
	    {
	      DBG (DBG_error, "USB stack read more bytes than requested!\n");
	      return SANE_STATUS_IO_ERROR;
	    }
	  buffer += really_read;
	  required -= really_read;
	  to_read -= really_read;
	  read_count += really_read;
	}
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
scanner_buffer_init (scanner_buffer_t * sb, int size_in_kb)
{

  sb->size = size_in_kb * 1024 + 3;
  sb->buffer = malloc (sb->size);
  if (!sb->buffer)
    return SANE_STATUS_NO_MEM;
  sb->num_bytes = 0;
  sb->data_ptr = sb->buffer;

  return SANE_STATUS_GOOD;
}

static SANE_Status
scanner_buffer_read (HP4200_Scanner * s)
{
  SANE_Status status;
  size_t num_bytes_read_now;

  assert (s->scanner_buffer.num_bytes <= 3);

  memcpy (s->scanner_buffer.buffer, s->scanner_buffer.data_ptr, 3);

  status = read_available_data (s, s->scanner_buffer.buffer +
				s->scanner_buffer.num_bytes,
				&num_bytes_read_now);
  s->scanner_buffer.data_ptr = s->scanner_buffer.buffer;
  s->scanner_buffer.num_bytes += num_bytes_read_now;
  return status;
}

#define OFFSET_CODE_SIGN(off) (((off) < 0) ? (-(off) & 0x1f) | 0x20 : (off))
#define OFFSET_DECODE_SIGN(off) (((off) & 0x20) ? -(off & 0x1f) : (off))

static SANE_Status
do_coarse_calibration (HP4200_Scanner * s, struct coarse_t *coarse)
{
  SANE_Status status;
  unsigned char *cal_line = NULL;
  unsigned char *cal_line_ptr;
  int cal_line_size;
  /* local scanning params */
  int active_pixels_start;
  int line_end;
  int data_pixels_start;
  int data_pixels_end;
  int dpd;
  int step_size;
  int ff_step_size;
  char steps_to_reverse;
  char hdpi_div;
  char line_rate_color;
  int vdpi;			/* vertical dots per inch */
  int hdpi_code;
  int calibrated;
  int first_time;

  int red_offset = 0;
  int green_offset = 0;
  int blue_offset = 0;

  int red_gain = 1;
  int green_gain = 1;
  int blue_gain = 1;

  int min_red_offset = -31;
  int min_green_offset = -31;
  int min_blue_offset = -31;

  int max_red_gain = 63;
  int max_green_gain = 63;
  int max_blue_gain = 63;

  int max_red;
  int min_red;
  int max_green;
  int min_green;
  int max_blue;
  int min_blue;
  static char me[] = "do_coarse_calibration";

  DBG (DBG_proc, "%s\n", me);

  setreg (s, 0x07, 0x00);
  usleep (10 * 1000);

  vdpi = 150;
  hdpi_code = 0;
  hdpi_div = hdpi_mapping[hdpi_code];
  active_pixels_start = 0x40;
  line_end = 0x2ee0;
  s->mclk_div = 2;
  data_pixels_start = 0x40;
  data_pixels_end = (int) (data_pixels_start + s->hw_parms.scan_area_width);
  data_pixels_end = min (data_pixels_end, line_end - 20);

  cal_line_size = s->hw_parms.scan_area_width * 3 * 2 + 2;

  setreg (s, 0x1e, HIBYTE (active_pixels_start));
  setreg (s, 0x1f, LOBYTE (active_pixels_start));
  setreg (s, 0x20, HIBYTE (line_end));
  setreg (s, 0x21, LOBYTE (line_end));
  setreg (s, 0x22, HIBYTE (data_pixels_start));
  setreg (s, 0x23, LOBYTE (data_pixels_start));
  setreg (s, 0x24, HIBYTE (data_pixels_end));
  setreg (s, 0x25, LOBYTE (data_pixels_end));

  setreg (s, 0x26,
	  PIXEL_RATE_3_CHANNELS |
	  GRAY_CHANNEL_RED | TR_RED (0) | TR_GREEN (0) | TR_BLUE (0));


  setreg (s, 0x08, (s->mclk_div - 1) * 2);
  setreg (s, 0x09, hdpi_code | PIXEL_PACKING (3) | DATAMODE (1));
  setreg (s, 0x0a, 0);		/* reserved and strange register */

  setreg (s, 0x38, red_offset);
  setreg (s, 0x39, green_offset);
  setreg (s, 0x3a, blue_offset);
  setreg (s, 0x3b, red_gain);
  setreg (s, 0x3c, green_gain);
  setreg (s, 0x3d, blue_gain);

  setreg (s, 0x5e, 0x80);

  setreg (s, 0x3e, 0x00);	/* 1.5:1, 6/10 bits, 2*fixed */
  setreg (s, 0x3f, 0x00);
  setreg (s, 0x40, 0x00);
  setreg (s, 0x41, 0x00);

  setreg (s, 0x4e, 0x5b - 0x3c);	/* max Kb to pause */
  setreg (s, 0x4f, 0x02);	/* min Kb to resume */

  line_rate_color = 1;
  step_size = (vdpi * line_end * line_rate_color) /
    (4 * s->hw_parms.motor_full_steps_per_inch);

  dpd = compute_dpd (s, step_size, line_end);	/* 0x0ada; */
#ifdef DEBUG
  fprintf (stderr, "dpd = %d\n", dpd);
#endif
  setreg (s, 0x52, HIBYTE (dpd));
  setreg (s, 0x53, LOBYTE (dpd));

  setreg (s, 0x46, HIBYTE (step_size));
  setreg (s, 0x47, LOBYTE (step_size));

  ff_step_size = compute_fastfeed_step_size (s->hw_parms.crystal_frequency, s->mclk_div, s->hw_parms.motor_max_speed, s->hw_parms.motor_full_steps_per_inch, 0);	/* 0x0190; */
  setreg (s, 0x48, HIBYTE (ff_step_size));
  setreg (s, 0x49, LOBYTE (ff_step_size));
  setreg (s, 0x4b, 0x15);
  steps_to_reverse = 0x3f;
  setreg (s, 0x50, steps_to_reverse);
  setreg (s, 0x51, 0x15);	/* accel profile */

  /* this is to stay the motor stopped */
  clearbits (s, 0x45, (1 << 4));

  cache_write (s);

  calibrated = 0;
  first_time = 1;
  cal_line = malloc (cal_line_size + 1024);

  do
    {
      unsigned char cmd_reg;

      /* resets the lm9830 before start scanning */
      setreg (s, 0x07, 0x08);
      do
	{
	  setreg (s, 0x07, 0x03);
	  cmd_reg = getreg (s, 0x07);
	}
      while (cmd_reg != 0x03);

      cal_line_ptr = cal_line;
      status = read_required_bytes (s, cal_line_size, cal_line_ptr);
      if (status != SANE_STATUS_GOOD)
	goto done;

      setreg (s, 0x07, 0x00);
      {
	unsigned int i;
	min_red = max_red = (cal_line[0] * 256 + cal_line[1]) >> 2;
	min_green = max_green = (cal_line[2] * 256 + cal_line[3]) >> 2;
	min_blue = max_blue = (cal_line[4] * 256 + cal_line[5]) >> 2;
	for (i = 6; i < (s->hw_parms.scan_area_width * 3 * 2); i += 6)
	  {
	    int value;

	    value = cal_line[i] * 256 + cal_line[i + 1];
	    value >>= 2;
	    if (value > max_red)
	      max_red = value;
	    value = cal_line[i + 2] * 256 + cal_line[i + 3];
	    value >>= 2;
	    if (value > max_green)
	      max_green = value;
	    value = cal_line[i + 4] * 256 + cal_line[i + 5];
	    value >>= 2;
	    if (value > max_blue)
	      max_blue = value;
	    value = cal_line[i] * 256 + cal_line[i + 1];
	    value >>= 2;
	    if (value < min_red)
	      min_red = value;
	    value = cal_line[i + 2] * 256 + cal_line[i + 3];
	    value >>= 2;
	    if (value < min_green)
	      min_green = value;
	    value = cal_line[i + 4] * 256 + cal_line[i + 5];
	    value >>= 2;
	    if (value < min_blue)
	      min_blue = value;
	  }
#ifdef DEBUG
	fprintf (stderr, "max_red:%d max_green:%d max_blue:%d\n",
		 max_red, max_green, max_blue);
	fprintf (stderr, "min_red:%d min_green:%d min_blue:%d\n",
		 min_red, min_green, min_blue);
#endif

	if (first_time)
	  {
	    first_time = 0;
	    compute_first_gain_offset (s->hw_parms.target_value.red,
				       max_red, min_red,
				       &red_gain, &red_offset,
				       &max_red_gain, &min_red_offset);
	    compute_first_gain_offset (s->hw_parms.target_value.green,
				       max_green, min_green,
				       &green_gain, &green_offset,
				       &max_green_gain, &min_green_offset);
	    compute_first_gain_offset (s->hw_parms.target_value.blue,
				       max_blue, min_blue, &blue_gain,
				       &blue_offset, &max_blue_gain,
				       &min_blue_offset);
	  }
	else
	  {
	    int retval;

	    /* this code should check return value -1 for error */

	    retval = compute_gain_offset (s->hw_parms.target_value.red,
					  max_red, min_red,
					  &red_gain, &red_offset,
					  &max_red_gain, &min_red_offset);
	    if (retval < 0)
	      break;
	    retval |= compute_gain_offset (s->hw_parms.target_value.green,
					   max_green, min_green,
					   &green_gain, &green_offset,
					   &max_green_gain,
					   &min_green_offset);
	    if (retval < 0)
	      break;
	    retval |= compute_gain_offset (s->hw_parms.target_value.blue,
					   max_blue, min_blue,
					   &blue_gain, &blue_offset,
					   &max_blue_gain, &min_blue_offset);
	    if (retval < 0)
	      break;
	    calibrated = !retval;
	  }

	setreg (s, 0x3b, red_gain);
	setreg (s, 0x3c, green_gain);
	setreg (s, 0x3d, blue_gain);

	setreg (s, 0x38, OFFSET_CODE_SIGN (red_offset));
	setreg (s, 0x39, OFFSET_CODE_SIGN (green_offset));
	setreg (s, 0x3a, OFFSET_CODE_SIGN (blue_offset));

#ifdef DEBUG
	fprintf (stderr, "%d, %d, %d   %d, %d, %d\n", red_gain,
		 green_gain, blue_gain, red_offset, green_offset,
		 blue_offset);
#endif
	cache_write (s);
      }
    }
  while (!calibrated);
  coarse->min_red = min_red;
  coarse->min_green = min_green;
  coarse->min_blue = min_blue;
  coarse->max_red = max_red;
  coarse->max_green = max_green;
  coarse->max_blue = max_blue;
  coarse->red_gain = red_gain;
  coarse->green_gain = green_gain;
  coarse->blue_gain = blue_gain;
  coarse->red_offset = red_offset;
  coarse->green_offset = green_offset;
  coarse->blue_offset = blue_offset;

  status = SANE_STATUS_GOOD;

done:
  if (cal_line)
    free (cal_line);

  return status;
}

static int
compute_corr_code (int average, int min_color, int range, int target)
{
  int value;
  int corr_code;

  value = average - min_color;
  if (value > 0)
    corr_code =
      (int) (range * ((double) target / (double) value - 1.0) + 0.5);
  else
    corr_code = 0;
  if (corr_code < 0)
    corr_code = 0;
  else if (corr_code > 2048)
    corr_code = 0;
  else if (corr_code > 1023)
    corr_code = 1023;
  return corr_code;
}

static int
compute_hdpi_code (int hres)
{
  int hdpi_code;

  /* Calculate the horizontal DPI code based on the requested
     horizontal resolution.  Defaults to 150dpi.  */
  switch (hres)
    {
    case 600:
      hdpi_code = 0;
      break;
    case 400:
      hdpi_code = 1;
      break;
    case 300:
      hdpi_code = 2;
      break;
    case 200:
      hdpi_code = 3;
      break;
    case 150:
      hdpi_code = 4;
      break;
    case 100:
      hdpi_code = 5;
      break;
    case 75:
      hdpi_code = 6;
      break;
    case 50:
      hdpi_code = 7;
      break;
    default:
      hdpi_code = 4;
    }
  return hdpi_code;
}


static SANE_Status
do_fine_calibration (HP4200_Scanner * s, struct coarse_t *coarse)
{
  SANE_Status status;
  unsigned char *cal_line;
  unsigned char *cal_line_ptr;
  int *average;
  SANE_Byte red_gain_offset[5460 * 2];
  SANE_Byte green_gain_offset[5460 * 2];
  SANE_Byte blue_gain_offset[5460 * 2];
  int *corr_red = NULL;
  int *corr_green = NULL;
  int *corr_blue = NULL;
  int registro[30][5460 * 3];
  int cal_line_size;
  /* local scanning params */
  int active_pixels_start;
  int line_end;
  int line_length;
  int data_pixels_start;
  int data_pixels_end;
  int dpd;
  int step_size;
  int ff_step_size;
  char steps_to_reverse;
  char hdpi_div;
  char line_rate_color;
  int vdpi;			/* vertical dots per inch */
  int hdpi_code;
  int calibrated;
  int first_time;
  int lines_to_process;

  static char me[] = "do_fine_calibration";

  DBG (DBG_proc, "%s\n", me);

  setreg (s, 0x07, 0x00);
  usleep (10 * 1000);

  vdpi = 150;
  hdpi_code = compute_hdpi_code (s->user_parms.horizontal_resolution);

  /* figure out which horizontal divider to use based on the
     calculated horizontal dpi code */
  hdpi_div = hdpi_mapping[hdpi_code];
  active_pixels_start = 0x40;
  line_end = 0x2ee0;
  line_length = s->user_parms.image_width * hdpi_div;
  s->mclk_div = 2;
  data_pixels_start = 0x72 + s->runtime_parms.first_pixel * hdpi_div;
  data_pixels_end =
    (int) (data_pixels_start + s->user_parms.image_width * hdpi_div);
  data_pixels_end = min (data_pixels_end, line_end - 20);

  cal_line_size = line_length * 3 * 2 + 2;

  setreg (s, 0x1e, HIBYTE (active_pixels_start));
  setreg (s, 0x1f, LOBYTE (active_pixels_start));
  setreg (s, 0x20, HIBYTE (line_end));
  setreg (s, 0x21, LOBYTE (line_end));
  setreg (s, 0x22, HIBYTE (data_pixels_start));
  setreg (s, 0x23, LOBYTE (data_pixels_start));
  setreg (s, 0x24, HIBYTE (data_pixels_end));
  setreg (s, 0x25, LOBYTE (data_pixels_end));

  setreg (s, 0x26,
	  PIXEL_RATE_3_CHANNELS |
	  GRAY_CHANNEL_RED | TR_RED (0) | TR_GREEN (0) | TR_BLUE (0));


  setreg (s, 0x08, (s->mclk_div - 1) * 2);
  setreg (s, 0x09, 0 | PIXEL_PACKING (3) | DATAMODE (1));
  setreg (s, 0x0a, 0);		/* reserved and strange register */

  setreg (s, 0x38, 1);
  setreg (s, 0x39, 1);
  setreg (s, 0x3a, 1);
  setreg (s, 0x3b, coarse->red_gain);
  setreg (s, 0x3c, coarse->green_gain);
  setreg (s, 0x3d, coarse->blue_gain);

  setreg (s, 0x5e, 0x80);

  setreg (s, 0x3e, 0x00);	/* 1.5:1, 6/10 bits, 2*fixed */
  setreg (s, 0x3f, 0x00);
  setreg (s, 0x40, 0x00);
  setreg (s, 0x41, 0x00);

  setreg (s, 0x4e, 0x5b - 0x3c);	/* max Kb to pause */
  setreg (s, 0x4f, 0x02);	/* min Kb to resume */

  line_rate_color = 1;
  step_size = (vdpi * line_end * line_rate_color) /
    (4 * s->hw_parms.motor_full_steps_per_inch);

  dpd = compute_dpd (s, step_size, line_end);	/* 0x0ada; */
#ifdef DEBUG
  fprintf (stderr, "dpd = %d\n", dpd);
#endif
  setreg (s, 0x52, HIBYTE (dpd));
  setreg (s, 0x53, LOBYTE (dpd));

  setreg (s, 0x46, HIBYTE (step_size));
  setreg (s, 0x47, LOBYTE (step_size));

  ff_step_size = compute_fastfeed_step_size (s->hw_parms.crystal_frequency, s->mclk_div, s->hw_parms.motor_max_speed, s->hw_parms.motor_full_steps_per_inch, 0);	/* 0x0190; */
  setreg (s, 0x48, HIBYTE (ff_step_size));
  setreg (s, 0x49, LOBYTE (ff_step_size));
  setreg (s, 0x4b, 0x15);
  steps_to_reverse = 0x3f;
  setreg (s, 0x50, steps_to_reverse);
  setreg (s, 0x51, 0x15);	/* accel profile */

  /* this is to activate the motor */
  setbits (s, 0x45, (1 << 4));

  lines_to_process = 8 * step_size * 4 / line_end;
  if (lines_to_process < 1)
    lines_to_process = 1;

#ifdef DEBUG
  fprintf (stderr, "lines to process = %d\n", lines_to_process);
#endif

  setreg (s, 0x58, 0);

  cache_write (s);

  calibrated = 0;
  first_time = 1;
  cal_line = malloc (cal_line_size + 1024);
  average = malloc (sizeof (int) * line_length * 3);
  memset (average, 0, sizeof (int) * line_length * 3);
  {
    int i;
    for (i = 0; i < 12; i++)
      {
	memset (registro[i], 0, 5460 * 3);
      }
  }

  /* resets the lm9830 before start scanning */
  setreg (s, 0x07, 0x08);
  setreg (s, 0x07, 0x03);

  usleep (100);

  do
    {

      cal_line_ptr = cal_line;

      status = read_required_bytes (s, cal_line_size, cal_line_ptr);
      if (status != SANE_STATUS_GOOD)
	goto done;
      {
	int i, j;

	if (calibrated == 0)
	  for (j = 0, i = 0; i < (line_length * 3); i++, j += 2)
	    {
	      average[i] = (cal_line[j] * 256 + cal_line[j + 1]) >> 2;
	      registro[calibrated][i] = average[i];
	    }
	else
	  for (j = 0, i = 0; i < (line_length * 3); i++, j += 2)
	    {
	      int value;
	      value = (cal_line[j] * 256 + cal_line[j + 1]) >> 2;
	      average[i] += value;
	      average[i] /= 2;
	      registro[calibrated][i] = value;
	    }
      }
      calibrated++;
    }
  while (calibrated < lines_to_process);
  lm9830_write_register (s->fd, 0x07, 0x00);
  usleep (10 * 1000);

#if 0
  {
    int i;
    int j = 0;
    do
      {
	for (i = 3; (i + 6) < (line_length * 3); i += 3)
	  {
	    average[i] =
	      (2 * average[i - 3] + average[i] + 2 * average[i + 3]) / 5;
	    average[i + 1] =
	      (2 * average[i - 2] + average[i + 1] + 2 * average[i + 4]) / 5;
	    average[i + 2] =
	      (2 * average[i - 1] + average[i + 2] + 2 * average[i + 5]) / 5;
	  }
	j++;
      }
    while (j < 3);
  }
#endif
  {
    int i;
    int max_red;
    int min_red;
    int max_green;
    int min_green;
    int max_blue;
    int min_blue;
    min_red = max_red = average[0];
    min_green = max_green = average[1];
    min_blue = max_blue = average[2];
    for (i = 3; i < (line_length * 3); i += 3)
      {
	int value;

	value = average[i];
	if (value > max_red)
	  max_red = value;
	value = average[i + 1];
	if (value > max_green)
	  max_green = value;
	value = average[i + 2];
	if (value > max_blue)
	  max_blue = value;
	value = average[i];
	if (value < min_red)
	  min_red = value;
	value = average[i + 1];
	if (value < min_green)
	  min_green = value;
	value = average[i + 2];
	if (value < min_blue)
	  min_blue = value;
      }
#ifdef DEBUG
    fprintf (stderr, "max_red:%d max_green:%d max_blue:%d\n",
	     max_red, max_green, max_blue);
    fprintf (stderr, "min_red:%d min_green:%d min_blue:%d\n",
	     min_red, min_green, min_blue);
#endif

    /* do fine calibration */
    {
      int min_white_red;
      int min_white_green;
      int min_white_blue;
      double ratio;
      int range;
      double aux;
      int min_white_err;
      int j;

      min_white_red = min_white_green = min_white_blue = 0x3ff;
      for (i = 0; i < (line_length * 3); i += 3)
	{
	  int value;

	  value = average[i] - coarse->min_red;
	  if ((value > 0) && (value < min_white_red))
	    min_white_red = value;
	  value = average[i + 1] - coarse->min_green;
	  if ((value > 0) && (value < min_white_green))
	    min_white_green = value;
	  value = average[i + 2] - coarse->min_blue;
	  if ((value > 0) && (value < min_white_blue))
	    min_white_blue = value;
	}

      ratio = 0;
      min_white_err = 0x3ff;

      aux = (double) s->hw_parms.target_value.red / min_white_red;
      if (aux > ratio)
	ratio = aux;
      if (min_white_err > min_white_red)
	min_white_err = min_white_red;
      aux = (double) s->hw_parms.target_value.green / min_white_green;
      if (aux > ratio)
	ratio = aux;
      if (min_white_err > min_white_green)
	min_white_err = min_white_green;
      aux = (double) s->hw_parms.target_value.blue / min_white_blue;
      if (aux > ratio)
	ratio = aux;
      if (min_white_err > min_white_blue)
	min_white_err = min_white_blue;

#ifdef DEBUG
      fprintf (stderr, "min_white_err = %d, ratio = %f\n",
	       min_white_err, ratio);
#endif
      if (ratio <= 1.5)
	range = 2048;
      else if (ratio <= 2.0)
	range = 1024;
      else
	range = 512;

      corr_red = malloc (sizeof (int) * line_length);
      corr_green = malloc (sizeof (int) * line_length);
      corr_blue = malloc (sizeof (int) * line_length);

      for (i = 0, j = 0; i < (line_length * 3); i += 3, j++)
	{
	  corr_red[j] = compute_corr_code (average[i],
					   coarse->min_red,
					   range,
					   s->hw_parms.target_value.red);
	  corr_green[j] =
	    compute_corr_code (average[i + 1], coarse->min_green,
			       range, s->hw_parms.target_value.green);
	  corr_blue[j] =
	    compute_corr_code (average[i + 2], coarse->min_blue,
			       range, s->hw_parms.target_value.blue);
	}
#ifdef DEBUG
      {
	FILE *kaka;
	int i;
	kaka = fopen ("corr.raw", "w");
	for (i = 0; i < line_length; i++)
	  {
	    fprintf (kaka, "%d %d %d %d %d %d ",
		     corr_red[i], corr_green[i], corr_blue[i],
		     average[3 * i], average[3 * i + 1], average[3 * i + 2]);
	    fprintf (kaka, "%d %d %d  %d %d %d  %d %d %d ",
		     registro[0][3 * i], registro[0][3 * i + 1],
		     registro[0][3 * i + 2], registro[1][3 * i],
		     registro[1][3 * i + 1], registro[1][3 * i + 2],
		     registro[2][3 * i], registro[2][3 * i + 1],
		     registro[2][3 * i + 2]);
	    fprintf (kaka, "%d %d %d  %d %d %d  %d %d %d\n",
		     registro[3][3 * i], registro[3][3 * i + 1],
		     registro[3][3 * i + 2], registro[4][3 * i],
		     registro[4][3 * i + 1], registro[4][3 * i + 2],
		     registro[5][3 * i], registro[5][3 * i + 1],
		     registro[5][3 * i + 2]);
	  }
	fclose (kaka);
      }
#endif
      {
	int max_black;
	int use_six_eight_bits;

	max_black = max (coarse->min_red, coarse->min_green);
	max_black = max (max_black, coarse->min_blue);
	use_six_eight_bits = (max_black < 64);

	if (use_six_eight_bits)
	  {
	    setreg (s, 0x3e, (1 << 4) | (1 << 3) | (1024 / range));
	  }
	else
	  {
	    setreg (s, 0x3e, (1 << 4) | (1 << 3) | (1 << 2) | (1024 / range));
	  }
	memset (red_gain_offset, 0, sizeof (red_gain_offset));
	memset (green_gain_offset, 0, sizeof (green_gain_offset));
	memset (blue_gain_offset, 0, sizeof (blue_gain_offset));
	for (i = 0, j = (data_pixels_start - active_pixels_start) * 2;
	     i < line_length; i++, j += 2)
	  {
	    if (use_six_eight_bits)
	      {
		red_gain_offset[j] = (coarse->min_red << 2) |
		  ((corr_red[i] >> 8) & 0x03);
		red_gain_offset[j + 1] = corr_red[i] & 0xff;
		green_gain_offset[j] = (coarse->min_green << 2) |
		  ((corr_green[i] >> 8) & 0x03);
		green_gain_offset[j + 1] = corr_green[i] & 0xff;
		blue_gain_offset[j] = (coarse->min_blue << 2) |
		  ((corr_blue[i] >> 8) & 0x03);
		blue_gain_offset[j + 1] = corr_blue[i] & 0xff;
	      }
	    else
	      {
		red_gain_offset[j] = coarse->min_red;
		red_gain_offset[j + 1] = corr_red[j] >> 2;
		green_gain_offset[j] = coarse->min_green;
		green_gain_offset[j + 1] = corr_green[j] >> 2;
		blue_gain_offset[j] = coarse->min_blue;
		blue_gain_offset[j + 1] = corr_blue[j] >> 2;
	      }
	  }
	write_default_offset_gain (s, red_gain_offset, 5460 * 2, 0);
	write_default_offset_gain (s, green_gain_offset, 5460 * 2, 1);
	write_default_offset_gain (s, blue_gain_offset, 5460 * 2, 2);
      }
    }
  }

  status = SANE_STATUS_GOOD;

done:
  if (corr_red)
    free (corr_red);
  if (corr_green)
    free (corr_green);
  if (corr_blue)
    free (corr_blue);
  if (cal_line)
    free (cal_line);
  if (average)
    free (average);

  return status;
}

static void
ciclic_buffer_init_offset_correction (ciclic_buffer_t * cb, int vres)
{
  cb->blue_idx = 0;
  switch (vres)
    {
    case 600:
      cb->green_idx = 4;
      cb->red_idx = 8;
      cb->first_good_line = 8;
      break;
    case 400:
      cb->green_idx = 3;
      cb->red_idx = 6;
      cb->first_good_line = 6;
      break;
    case 300:
      cb->green_idx = 2;
      cb->red_idx = 4;
      cb->first_good_line = 4;
      break;
    case 200:
      cb->blue_idx = 0;
      cb->green_idx = 1;
      cb->red_idx = 2;
      cb->first_good_line = 4;
      break;
    case 150:
      cb->green_idx = 1;
      cb->red_idx = 2;
      cb->first_good_line = 2;
      break;
    case 75:
      cb->green_idx = 1;
      cb->red_idx = 2;
      cb->first_good_line = 2;
      break;
    default:
      cb->green_idx = 0;
      cb->red_idx = 0;
      cb->first_good_line = 0;
      break;
    }

  cb->buffer_position = cb->buffer_ptrs[cb->first_good_line];
}

static SANE_Status
ciclic_buffer_init (ciclic_buffer_t * cb, SANE_Int bytes_per_line,
		    int vres, int status_bytes)
{
  cb->good_bytes = 0;
  cb->num_lines = 12;
  cb->size = bytes_per_line * cb->num_lines;
  cb->can_consume = cb->size + cb->num_lines * status_bytes;

  cb->buffer = malloc (cb->size);
  if (!cb->buffer)
    return SANE_STATUS_NO_MEM;

  {
    int i;
    unsigned char *buffer;
    unsigned char **ptrs;

    ptrs = cb->buffer_ptrs = (unsigned char **)
      malloc (sizeof (unsigned char *) * cb->num_lines);
    if (!cb->buffer_ptrs)
      return SANE_STATUS_NO_MEM;

    buffer = cb->buffer;
    for (i = 0; i < cb->num_lines; i++)
      {
	ptrs[i] = buffer;
	buffer += bytes_per_line;
      }
  }

  cb->current_line = 0;
  cb->pixel_position = 0;
  ciclic_buffer_init_offset_correction (cb, vres);

  return SANE_STATUS_GOOD;
}

static int
prepare_for_a_scan (HP4200_Scanner * s)
{
  /* local scanning params */
  int active_pixels_start;
  int line_end;
  int data_pixels_start;
  int data_pixels_end;
  int ff_step_size;
  int dpd;
  int step_size;
  char steps_to_reverse;
  char hdpi_div;
  char line_rate_color;
  int hdpi_code;
  unsigned char pixel_packing;
  unsigned char data_mode;
  unsigned char AFE_operation;
  int pause_limit;
  int n = 0, m = 0;

  setreg (s, 0x07, 0x00);
  usleep (10 * 1000);

  hdpi_code = compute_hdpi_code (s->user_parms.horizontal_resolution);
  /* figure out which horizontal divider to use based on the
     calculated horizontal dpi code */
  hdpi_div = hdpi_mapping[hdpi_code];

  /* image_width is set to the correct number of pixels by calling 
     fxn.  This might be the reason we can't do high res full width
     scans though...not sure.  */
  /*s->user_parms.image_width /= 4; */
  active_pixels_start = 0x40;
  line_end = 0x2ee0;		/* 2ee0 */
  s->mclk_div = 2;
  data_pixels_start = 0x72 + s->runtime_parms.first_pixel * hdpi_div;
  data_pixels_end =
    (int) (data_pixels_start + s->user_parms.image_width * hdpi_div);
  data_pixels_end = min (data_pixels_end, line_end - 20);
  setreg (s, 0x1e, HIBYTE (active_pixels_start));
  setreg (s, 0x1f, LOBYTE (active_pixels_start));
  setreg (s, 0x20, HIBYTE (line_end));
  setreg (s, 0x21, LOBYTE (line_end));
  setreg (s, 0x22, HIBYTE (data_pixels_start));
  setreg (s, 0x23, LOBYTE (data_pixels_start));
  setreg (s, 0x24, HIBYTE (data_pixels_end));
  setreg (s, 0x25, LOBYTE (data_pixels_end));

  AFE_operation = PIXEL_RATE_3_CHANNELS;
  setreg (s, 0x26,
	  AFE_operation |
	  GRAY_CHANNEL_RED | TR_RED (0) | TR_GREEN (0) | TR_BLUE (0));

  setreg (s, 0x08, (s->mclk_div - 1) * 2);
  pixel_packing = 3;
  data_mode = 0;
  setreg (s, 0x09, hdpi_code | PIXEL_PACKING (pixel_packing) |
	  DATAMODE (data_mode));
  setreg (s, 0x0a, 0);		/* reserved and strange register */

  setreg (s, 0x5c, 0x00);
  setreg (s, 0x5d, 0x00);
  setreg (s, 0x5e, 0x00);

  if (s->user_parms.vertical_resolution == 1200)
    {
      /* 1 out of 2 */
      n = 1;
      m = 2;
    }
  setreg (s, 0x44, (256 - n) & 0xff);
  setreg (s, 0x5a, m);
  s->runtime_parms.status_bytes = (m == 0) ? 1 : m;
  if (data_mode == 1)
    s->runtime_parms.status_bytes *= 2;

  s->runtime_parms.scanner_line_size =
    compute_bytes_per_line (data_pixels_end - data_pixels_start,
			    hdpi_code, pixel_packing, data_mode,
			    AFE_operation, m);
  pause_limit = compute_pause_limit (&(s->hw_parms),
				     s->runtime_parms.scanner_line_size);

#ifdef DEBUG
  fprintf (stderr, "scanner_line_size = %d\npause_limit = %d\n",
	   s->runtime_parms.scanner_line_size, pause_limit);
#endif

  setreg (s, 0x4e, pause_limit);	/* max Kb to pause */
  setreg (s, 0x4f, 0x02);	/* min Kb to resume */

  line_rate_color = 1;
  step_size =
    (s->user_parms.vertical_resolution * line_end * line_rate_color) /
    (4 * s->hw_parms.motor_full_steps_per_inch);

  if (s->val[OPT_BACKTRACK].b)
    {
      steps_to_reverse = 0x3f;
      setreg (s, 0x50, steps_to_reverse);
      setreg (s, 0x51, 0x15);	/* accel profile */
    }
  else
    {
      s->hw_parms.steps_to_reverse = 0;
      setreg (s, 0x50, s->hw_parms.steps_to_reverse);
      setreg (s, 0x51, 0);	/* accel profile */
      s->hw_parms.fsteps_25_speed = 0;
      s->hw_parms.fsteps_50_speed = 0;
    }

  dpd = compute_dpd (s, step_size, line_end);	/* 0x0ada; */
#ifdef DEBUG
  fprintf (stderr, "dpd = %d\n", dpd);
#endif
  setreg (s, 0x52, HIBYTE (dpd));
  setreg (s, 0x53, LOBYTE (dpd));

  setreg (s, 0x46, HIBYTE (step_size));
  setreg (s, 0x47, LOBYTE (step_size));

  ff_step_size = compute_fastfeed_step_size (s->hw_parms.crystal_frequency,
					     s->mclk_div,
					     s->hw_parms.motor_max_speed,
					     s->hw_parms.
					     motor_full_steps_per_inch, 0);
  setreg (s, 0x48, HIBYTE (ff_step_size));
  setreg (s, 0x49, LOBYTE (ff_step_size));
  setreg (s, 0x4b, 0x15);
  /* this is to stay the motor running */
  setbits (s, 0x45, (1 << 4));

  setreg (s, 0x4a, HIBYTE (47 + s->runtime_parms.steps_to_skip));
  setreg (s, 0x4b, LOBYTE (47 + s->runtime_parms.steps_to_skip));

  setreg (s, 0x58, 0);

  ciclic_buffer_init (&(s->ciclic_buffer),
		      s->runtime_parms.image_line_size,
		      s->user_parms.vertical_resolution,
		      s->runtime_parms.status_bytes);

  s->runtime_parms.num_bytes_left_to_scan =
    s->user_parms.lines_to_scan * s->runtime_parms.image_line_size;

#ifdef DEBUG
  fprintf (stderr, "bytes to scan = %ld\n",
	   s->runtime_parms.num_bytes_left_to_scan);
#endif

  cache_write (s);

#ifdef DEBUG
  lm9830_dump_registers (s->fd);
#endif

  lm9830_reset (s->fd);

  setreg (s, 0x07, 0x03);
  usleep (100);

  return SANE_STATUS_GOOD;
}

static SANE_Status
end_scan (HP4200_Scanner * s)
{
  s->scanning = SANE_FALSE;
  setreg (s, 0x07, 0x00);
  lm9830_reset (s->fd);
  setbits (s, 0x58, PAPER_SENSOR_2_STOP_SCAN);
  cache_write (s);
  setreg (s, 0x07, 0x02);

  /* Free some buffers */
  if (s->ciclic_buffer.buffer)
    {
      free (s->ciclic_buffer.buffer);
      s->ciclic_buffer.buffer = NULL;
    }
  if (s->ciclic_buffer.buffer_ptrs)
    {
      free (s->ciclic_buffer.buffer_ptrs);
      s->ciclic_buffer.buffer_ptrs = NULL;
    }
  if (s->scanner_buffer.buffer)
    {
      free (s->scanner_buffer.buffer);
      s->scanner_buffer.buffer = NULL;
    }

  return SANE_STATUS_GOOD;
}

static int
hp4200_init_scanner (HP4200_Scanner * s)
{
  int ff_step_size;
  int mclk_div;

  lm9830_ini_scanner (s->fd, NULL);
  hp4200_init_registers (s);
  scanner_buffer_init (&(s->scanner_buffer), s->hw_parms.SRAM_size);
  setreg (s, 0x07, 0x08);
  usleep (10 * 1000);
  setreg (s, 0x07, 0x00);
  usleep (10 * 1000);
  mclk_div = 2;

  setreg (s, 0x08, (mclk_div - 1) * 2);
  ff_step_size =
    compute_fastfeed_step_size (s->hw_parms.crystal_frequency,
				mclk_div,
				s->hw_parms.motor_max_speed,
				s->hw_parms.motor_full_steps_per_inch, 0);
  setreg (s, 0x48, HIBYTE (ff_step_size));
  setreg (s, 0x49, LOBYTE (ff_step_size));
  setbits (s, 0x45, (1 << 4));
  cache_write (s);
  return 0;
}

static void
ciclic_buffer_copy (ciclic_buffer_t * cb, SANE_Byte * buf,
		    SANE_Int num_bytes, int image_line_size, int status_bytes)
{
  int biggest_upper_block_size;
  int upper_block_size;
  int lower_block_size;
  int bytes_to_be_a_entire_line;

  /* copy the upper block */
  biggest_upper_block_size = cb->size - (cb->buffer_position - cb->buffer);
  upper_block_size = min (biggest_upper_block_size, num_bytes);
  memcpy (buf, cb->buffer_position, upper_block_size);
  cb->good_bytes -= upper_block_size;

  bytes_to_be_a_entire_line = (cb->buffer_position - cb->buffer) %
    image_line_size;
  cb->can_consume += upper_block_size +
    status_bytes * (((bytes_to_be_a_entire_line + upper_block_size) /
		     image_line_size) - 1);

  if (num_bytes < biggest_upper_block_size)
    {
      cb->buffer_position += num_bytes;
      return;
    }

  /* copy the lower block */
  lower_block_size = num_bytes - biggest_upper_block_size;
  if (lower_block_size > 0)
    {
      memcpy (buf + biggest_upper_block_size, cb->buffer, lower_block_size);
      cb->good_bytes -= lower_block_size;
      cb->can_consume += lower_block_size + status_bytes *
	(lower_block_size / image_line_size);
      cb->buffer_position = cb->buffer + lower_block_size;
    }
  else
    {
      cb->buffer_position = cb->buffer;
    }
  assert (cb->good_bytes >= 0);
  assert (lower_block_size >= 0);
}

static void
ciclic_buffer_consume (ciclic_buffer_t * cb,
		       scanner_buffer_t * scanner_buffer,
		       int image_width, int status_bytes)
{
  int to_consume;
  int to_consume_now;
  int i;
  int processed;

  to_consume = min (cb->can_consume, scanner_buffer->num_bytes);

  while (to_consume)
    {

      if (cb->pixel_position == image_width)
	{
	  if (scanner_buffer->num_bytes >= status_bytes)
	    {
	      /* forget status bytes */
	      scanner_buffer->data_ptr += status_bytes;
	      scanner_buffer->num_bytes -= status_bytes;
	      cb->can_consume -= status_bytes;
	      to_consume -= status_bytes;

	      cb->pixel_position = 0;	/* back to the start pixel */

	      cb->red_idx = (cb->red_idx + 1) % cb->num_lines;
	      cb->green_idx = (cb->green_idx + 1) % cb->num_lines;
	      cb->blue_idx = (cb->blue_idx + 1) % cb->num_lines;
	      cb->current_line++;
	    }
	  else
	    break;
	}

      to_consume_now = min ((image_width - cb->pixel_position) * 3,
			    to_consume);

      if (to_consume_now < 3)
	break;

      for (i = cb->pixel_position * 3; to_consume_now >= 3;
	   i += 3, to_consume_now -= 3)
	{
	  cb->buffer_ptrs[cb->red_idx][i] = scanner_buffer->data_ptr[0];
	  cb->buffer_ptrs[cb->green_idx][i + 1] = scanner_buffer->data_ptr[1];
	  cb->buffer_ptrs[cb->blue_idx][i + 2] = scanner_buffer->data_ptr[2];
	  scanner_buffer->data_ptr += 3;
	}
      processed = i - (cb->pixel_position * 3);
      cb->pixel_position = i / 3;
      to_consume -= processed;
      cb->can_consume -= processed;
      scanner_buffer->num_bytes -= processed;
      if (cb->current_line > cb->first_good_line)
	cb->good_bytes += processed;
    }
}

SANE_Status
sane_read (SANE_Handle h, SANE_Byte * buf, SANE_Int maxlen, SANE_Int * len)
{
  SANE_Status status;
  int to_copy_now;
  int bytes_to_copy_to_frontend;
  HP4200_Scanner *s = h;

  static char me[] = "sane_read";
  DBG (DBG_proc, "%s\n", me);

  if (!(s->scanning))
    {
      /* OOPS, not scanning */
      return SANE_STATUS_CANCELLED;
    }

  if (!buf || !len)
    return SANE_STATUS_INVAL;

  *len = 0;

  if (s->runtime_parms.num_bytes_left_to_scan == 0)
    {
      end_scan (s);
      return SANE_STATUS_EOF;
    }

  bytes_to_copy_to_frontend = min (s->runtime_parms.num_bytes_left_to_scan,
				   maxlen);

  /* first copy available data from the ciclic buffer */
  to_copy_now = min (s->ciclic_buffer.good_bytes, bytes_to_copy_to_frontend);

  if (to_copy_now > 0)
    {
      ciclic_buffer_copy (&(s->ciclic_buffer), buf, to_copy_now,
			  s->runtime_parms.image_line_size,
			  s->runtime_parms.status_bytes);
      buf += to_copy_now;
      bytes_to_copy_to_frontend -= to_copy_now;
      *len += to_copy_now;
    }

  /* if not enough bytes, get data from the scanner */
  while (bytes_to_copy_to_frontend)
    {
      if (s->scanner_buffer.num_bytes < 3)
	{			/* cicl buf consumes modulo 3
				   bytes at least now for rgb
				   color 8 bpp fixme: but this
				   is ugly and not generic
				 */
	  status = scanner_buffer_read (s);

	  if (status == SANE_STATUS_CANCELLED)
	    {
	      end_scan (s);
	      s->aborted_by_user = SANE_FALSE;
	      return status;
	    }
	  if (status != SANE_STATUS_GOOD)
	    return status;
	}

      while ((s->scanner_buffer.num_bytes > 3) && bytes_to_copy_to_frontend)
	{
	  ciclic_buffer_consume (&(s->ciclic_buffer), &(s->scanner_buffer),
				 s->user_parms.image_width,
				 s->runtime_parms.status_bytes);
	  to_copy_now = min (s->ciclic_buffer.good_bytes,
			     bytes_to_copy_to_frontend);

	  if (to_copy_now > 0)
	    {
	      ciclic_buffer_copy (&(s->ciclic_buffer), buf, to_copy_now,
				  s->runtime_parms.image_line_size,
				  s->runtime_parms.status_bytes);
	      buf += to_copy_now;
	      bytes_to_copy_to_frontend -= to_copy_now;
	      *len += to_copy_now;
	    }
	}
    }

  s->runtime_parms.num_bytes_left_to_scan -= *len;

  if (s->runtime_parms.num_bytes_left_to_scan < 0)
    *len += s->runtime_parms.num_bytes_left_to_scan;

  return SANE_STATUS_GOOD;
}

static HP4200_Device *
find_device (SANE_String_Const name)
{
  static char me[] = "find_device";
  HP4200_Device *dev;

  DBG (DBG_proc, "%s\n", me);

  for (dev = first_device; dev; dev = dev->next)
    {
      if (strcmp (dev->dev.name, name) == 0)
	{
	  return dev;
	}
    }
  return NULL;
}

static SANE_Status
add_device (SANE_String_Const name, HP4200_Device ** argpd)
{
  int fd;
  HP4200_Device *pd;
  static const char me[] = "add_device";
  SANE_Status status;

  DBG (DBG_proc, "%s(%s)\n", me, name);

  /* Avoid adding the same device more than once */
  if ((pd = find_device (name)))
    {
      if (argpd)
	*argpd = pd;
      return SANE_STATUS_GOOD;
    }

  /* open the device file, but read only or read/write to perform
     ioctl's ? */
  if ((status = sanei_usb_open (name, &fd)) != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "%s: open(%s) failed: %s\n", me, name,
	   sane_strstatus (status));
      return SANE_STATUS_INVAL;
    }

  /* put here some code to probe that the device attached to the
     device file is a supported scanner. Maybe some ioctl */
  sanei_usb_close (fd);

  pd = (HP4200_Device *) calloc (1, sizeof (HP4200_Device));
  if (!pd)
    {
      DBG (DBG_error, "%s: out of memory allocating device.\n", me);
      return SANE_STATUS_NO_MEM;
    }

  pd->dev.name = strdup (name);
  pd->dev.vendor = "Hewlett-Packard";
  pd->dev.model = "HP-4200";
  pd->dev.type = "flatbed scanner";

  if (!pd->dev.name || !pd->dev.vendor || !pd->dev.model || !pd->dev.type)
    {
      DBG (DBG_error,
	   "%s: out of memory allocating device descriptor strings.\n", me);
      free (pd);
      return SANE_STATUS_NO_MEM;
    }

  pd->handle = NULL;
  pd->next = first_device;
  first_device = pd;
  n_devices++;
  if (argpd)
    *argpd = pd;

  return SANE_STATUS_GOOD;
}

static SANE_Status
attach (SANE_String_Const name)
{
  static char me[] = "attach";
  DBG (DBG_proc, "%s\n", me);
  return add_device (name, NULL);
}

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  static const char me[] = "sane_hp4200_init";
  char dev_name[PATH_MAX];
  FILE *fp;

  authorize = authorize;	/* keep gcc quiet */

  DBG_INIT ();

  DBG (DBG_proc, "%s\n", me);
  DBG (DBG_error, "SANE hp4200 backend version %d.%d build %d from %s\n",
       SANE_CURRENT_MAJOR, V_MINOR, BUILD, PACKAGE_STRING);
  /* put some version_code checks here */

  if (NULL != version_code)
    {
      *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);
    }

  sanei_usb_init ();
  sanei_pv8630_init ();

  fp = sanei_config_open (HP4200_CONFIG_FILE);
  if (!fp)
    {
      DBG (DBG_error, "%s: configuration file not found!\n", me);

      return SANE_STATUS_INVAL;
    }
  else
    {
      while (sanei_config_read (dev_name, sizeof (dev_name), fp))
	{
	  if (dev_name[0] == '#')	/* ignore line comments */
	    continue;

	  if (strlen (dev_name) == 0)
	    continue;		/* ignore empty lines */

	  DBG (DBG_info, "%s: looking for devices matching %s\n",
	       me, dev_name);

	  sanei_usb_attach_matching_devices (dev_name, attach);
	}

      fclose (fp);
    }

  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  HP4200_Device *device, *next;

  DBG (DBG_proc, "sane_hp4200_exit\n");

  for (device = first_device; device; device = next)
    {
      next = device->next;
      if (device->handle)
	{
	  sane_close (device->handle);
	}
      if (device->dev.name)
	{
	  free (device->dev.name);
	}
      free (device);
    }
  first_device = NULL;

  if (devlist)
    {
      free (devlist);
      devlist = NULL;
    }

  n_devices = 0;

  DBG (DBG_proc, "sane_exit: exit\n");
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  int i;
  HP4200_Device *pdev;

  DBG (DBG_proc, "sane_get_devices (%p, %d)\n", (void *) device_list,
       local_only);

  /* Waste the last list returned from this function */
  if (devlist)
    free (devlist);

  devlist = (const SANE_Device **)
    malloc ((n_devices + 1) * sizeof (SANE_Device *));

  if (!devlist)
    {
      DBG (DBG_error, "sane_get_devices: out of memory\n");
      return SANE_STATUS_NO_MEM;
    }

  for (i = 0, pdev = first_device; pdev; i++, pdev = pdev->next)
    {
      devlist[i] = &(pdev->dev);
    }
  devlist[i] = NULL;

  *device_list = devlist;

  DBG (DBG_proc, "sane_get_devices: exit\n");

  return SANE_STATUS_GOOD;
}

static void
init_options (HP4200_Scanner * s)
{
  s->opt[OPT_NUM_OPTS].name = "";
  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].unit = SANE_UNIT_NONE;
  s->opt[OPT_NUM_OPTS].size = sizeof (SANE_Word);
  s->opt[OPT_NUM_OPTS].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  s->opt[OPT_RES].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RES].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RES].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RES].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_RES].type = SANE_TYPE_INT;
  s->opt[OPT_RES].size = sizeof (SANE_Word);
  s->opt[OPT_RES].unit = SANE_UNIT_DPI;
  s->opt[OPT_RES].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_RES].constraint.word_list = dpi_list;
  s->val[OPT_RES].w = 150;

  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].size = sizeof (SANE_Fixed);
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &x_range;
  s->val[OPT_TL_X].w = x_range.min;

  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].size = sizeof (SANE_Fixed);
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &y_range;
  s->val[OPT_TL_Y].w = y_range.min;

  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].size = sizeof (SANE_Fixed);
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &x_range;
  s->val[OPT_BR_X].w = x_range.max;

  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].size = sizeof (SANE_Fixed);
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &y_range;
  s->val[OPT_BR_Y].w = y_range.max;

  s->opt[OPT_BACKTRACK].name = SANE_NAME_BACKTRACK;
  s->opt[OPT_BACKTRACK].title = SANE_TITLE_BACKTRACK;
  s->opt[OPT_BACKTRACK].desc = SANE_DESC_BACKTRACK;
  s->opt[OPT_BACKTRACK].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_BACKTRACK].type = SANE_TYPE_BOOL;
  s->opt[OPT_BACKTRACK].size = sizeof (SANE_Bool);
  s->opt[OPT_BACKTRACK].unit = SANE_UNIT_NONE;
  s->opt[OPT_BACKTRACK].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_BACKTRACK].b = SANE_TRUE;

  s->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_R].size = 1024 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_R].wa = s->user_parms.gamma[0];

  s->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_G].size = 1024 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_G].wa = s->user_parms.gamma[1];

  s->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].cap =
    SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_B].size = 1024 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_B].wa = s->user_parms.gamma[2];

  {
    int i;
    double gamma = 2.0;
    for (i = 0; i < 1024; i++)
      {
	s->user_parms.gamma[0][i] =
	  255 * pow (((double) i + 1) / 1024, 1.0 / gamma);
	s->user_parms.gamma[1][i] =	s->user_parms.gamma[0][i];
	s->user_parms.gamma[2][i] =	s->user_parms.gamma[0][i];
#ifdef DEBUG
	printf ("%d %d\n", i, s->user_parms.gamma[0][i]);
#endif
      }
  }

  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_PREVIEW].size = sizeof (SANE_Word);
  s->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  s->val[OPT_PREVIEW].w = SANE_FALSE;
}

SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * h)
{
  static const char me[] = "sane_hp4200_open";
  SANE_Status status;
  HP4200_Device *dev;
  HP4200_Scanner *s;

  DBG (DBG_proc, "%s (%s, %p)\n", me, name, (void *) h);

  if (name && name[0])
    {
      dev = find_device (name);
      if (!dev)
	{
	  status = add_device (name, &dev);
	  if (status != SANE_STATUS_GOOD)
	    return status;
	}
    }
  else
    {
      dev = first_device;
    }
  if (!dev)
    return SANE_STATUS_INVAL;

  if (!h)
    return SANE_STATUS_INVAL;

  s = *h = (HP4200_Scanner *) calloc (1, sizeof (HP4200_Scanner));
  if (!s)
    {
      DBG (DBG_error, "%s: out of memory creating scanner structure.\n", me);
      return SANE_STATUS_NO_MEM;
    }

  dev->handle = s;
  s->aborted_by_user = SANE_FALSE;
  s->ciclic_buffer.buffer = NULL;
  s->scanner_buffer.buffer = NULL;
  s->dev = dev;
  s->user_parms.image_width = 0;
  s->user_parms.lines_to_scan = 0;
  s->user_parms.vertical_resolution = 0;
  s->scanning = SANE_FALSE;
  s->fd = -1;

  init_options (s);

  if ((sanei_usb_open (dev->dev.name, &s->fd) != SANE_STATUS_GOOD))
    {
      DBG (DBG_error, "%s: Can't open %s.\n", me, dev->dev.name);
      return SANE_STATUS_IO_ERROR;	/* fixme: return busy when file is
					   being accessed already */
    }

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle h)
{
  HP4200_Scanner *s = (HP4200_Scanner *) h;
  DBG (DBG_proc, "sane_hp4200_close (%p)\n", (void *) h);

  if (s)
    {
      s->dev->handle = NULL;
      if (s->fd != -1)
	{
	  sanei_usb_close (s->fd);
	}
      free (s);
    }
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle h, SANE_Int n)
{
  static char me[] = "sane_get_option_descriptor";
  HP4200_Scanner *s = (HP4200_Scanner *) h;

  DBG (DBG_proc, "%s\n", me);

  if ((n < 0) || (n >= NUM_OPTIONS))
    return NULL;

  return s->opt + n;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  HP4200_Scanner *s = (HP4200_Scanner *) handle;
  SANE_Status status;
  SANE_Int myinfo = 0;
  SANE_Word cap;

  DBG (DBG_proc, "sane_control_option\n");

  if (info)
    *info = 0;

  if (s->scanning)
    {
      return SANE_STATUS_DEVICE_BUSY;
    }

  if (option < 0 || option >= NUM_OPTIONS)
    {
      return SANE_STATUS_INVAL;
    }

  cap = s->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      return SANE_STATUS_INVAL;
    }

  if (action == SANE_ACTION_GET_VALUE)
    {

      switch (option)
	{
	case OPT_NUM_OPTS:
	case OPT_RES:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_PREVIEW:
	  *(SANE_Word *) val = s->val[option].w;
	  break;

	case OPT_BACKTRACK:
	  *(SANE_Bool *) val = s->val[option].b;
	  break;

	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (val, s->val[option].wa, s->opt[option].size);
	  break;
	default:
	  return SANE_STATUS_UNSUPPORTED;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {

      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (DBG_error, "could not set option, not settable\n");
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (s->opt + option, val, &myinfo);
      if (status != SANE_STATUS_GOOD)
	return status;

      switch (option)
	{

	  /* Numeric side-effect free options */
	case OPT_PREVIEW:
	  s->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	  /* Numeric side-effect options */
	case OPT_RES:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  myinfo |= SANE_INFO_RELOAD_PARAMS;
	  s->val[option].w = *(SANE_Word *) val;
	  break;

	case OPT_BACKTRACK:
	  s->val[option].b = *(SANE_Bool *) val;
	  break;

	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (s->val[option].wa, val, s->opt[option].size);
	  break;
	default:
	  return SANE_STATUS_UNSUPPORTED;
	}
    }
  else
    {
      return SANE_STATUS_UNSUPPORTED;
    }

  if (info)
    *info = myinfo;

  return SANE_STATUS_GOOD;
}

static void
compute_parameters (HP4200_Scanner * s)
{
  int resolution;
  int opt_tl_x;
  int opt_br_x;
  int opt_tl_y;
  int opt_br_y;

  if (s->val[OPT_PREVIEW].w == SANE_TRUE)
    {
      resolution = 50;
      opt_tl_x = SANE_UNFIX (x_range.min);
      opt_tl_y = SANE_UNFIX (y_range.min);
      opt_br_x = SANE_UNFIX (x_range.max);
      opt_br_y = SANE_UNFIX (y_range.max);
    }
  else
    {
      resolution = s->val[OPT_RES].w;
      opt_tl_x = SANE_UNFIX (s->val[OPT_TL_X].w);
      opt_tl_y = SANE_UNFIX (s->val[OPT_TL_Y].w);
      opt_br_x = SANE_UNFIX (s->val[OPT_BR_X].w);
      opt_br_y = SANE_UNFIX (s->val[OPT_BR_Y].w);
    }

  s->user_parms.horizontal_resolution = resolution;
  s->user_parms.vertical_resolution = resolution;

  s->runtime_parms.steps_to_skip = floor (300.0 / MM_PER_INCH * opt_tl_y);
  s->user_parms.lines_to_scan =
    floor ((opt_br_y - opt_tl_y) / MM_PER_INCH * resolution);
  s->user_parms.image_width =
    floor ((opt_br_x - opt_tl_x) / MM_PER_INCH * resolution);
  s->runtime_parms.first_pixel = floor (opt_tl_x / MM_PER_INCH * resolution);

  /* fixme: add support for more depth's and bpp's. */
  s->runtime_parms.image_line_size = s->user_parms.image_width * 3;
}

SANE_Status
sane_get_parameters (SANE_Handle h, SANE_Parameters * p)
{
  static char me[] = "sane_get_parameters";
  HP4200_Scanner *s = (HP4200_Scanner *) h;

  DBG (DBG_proc, "%s\n", me);
  if (!p)
    return SANE_STATUS_INVAL;

  p->format = SANE_FRAME_RGB;
  p->last_frame = SANE_TRUE;
  p->depth = 8;
  if (!s->scanning)
    {
      compute_parameters (s);
    }

  p->lines = s->user_parms.lines_to_scan;
  p->pixels_per_line = s->user_parms.image_width;
  p->bytes_per_line = s->runtime_parms.image_line_size;

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle h)
{
  HP4200_Scanner *s = (HP4200_Scanner *) h;
  struct coarse_t coarse;

  static char me[] = "sane_start";
  DBG (DBG_proc, "%s\n", me);

  s->scanning = SANE_TRUE;
  s->aborted_by_user = SANE_FALSE;
  s->user_parms.color = SANE_TRUE;

  compute_parameters (s);

  hp4200_init_scanner (s);
  hp4200_goto_home (s);
  hp4200_wait_homed (s);
  /* restore default register values here... */
  write_gamma (s);
  hp4200_init_registers (s);
  lm9830_ini_scanner (s->fd, NULL);
  /* um... do not call cache_write() here, don't know why :( */
  do_coarse_calibration (s, &coarse);
  do_fine_calibration (s, &coarse);
  prepare_for_a_scan (s);

  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle h)
{
  static char me[] = "sane_cancel";
  HP4200_Scanner *s = (HP4200_Scanner *) h;
  DBG (DBG_proc, "%s\n", me);

  s->aborted_by_user = SANE_TRUE;

  end_scan (s);
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  HP4200_Scanner *dev = handle;
  SANE_Status status;

  non_blocking = non_blocking;	/* silence gcc */

  if (dev->scanning == SANE_FALSE)
    {
      return SANE_STATUS_INVAL;
    }

  if (non_blocking == SANE_FALSE)
    {
      status = SANE_STATUS_GOOD;
    }
  else
    {
      status = SANE_STATUS_UNSUPPORTED;
    }

  DBG (DBG_proc, "sane_set_io_mode: exit\n");

  return status;
}

SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int * fd)
{
  static char me[] = "sane_get_select_fd";

  h = h;			/* keep gcc quiet */
  fd = fd;			/* keep gcc quiet */

  DBG (DBG_proc, "%s\n", me);
  return SANE_STATUS_UNSUPPORTED;
}
