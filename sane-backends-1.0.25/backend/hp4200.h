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

#ifndef _HP4200_H
#define _HP4200_H

#include <sys/types.h>

#define MCLKDIV_SCALING 2
#define NUM_REGISTERS 0x80

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))


/*--------------------------------------------------------------------------*/

#define DBG_error0  0
#define DBG_error   1
#define DBG_sense   2
#define DBG_warning 3
#define DBG_inquiry 4
#define DBG_info    5
#define DBG_info2   6
#define DBG_proc    7
#define DBG_read    8
#define DBG_sane_init   10
#define DBG_sane_proc   11
#define DBG_sane_info   12
#define DBG_sane_option 13

/*--------------------------------------------------------------------------*/

enum HP4200_Option
{
  OPT_NUM_OPTS = 0,

  OPT_RES,

  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  OPT_BACKTRACK,

  OPT_GAMMA_VECTOR_R,
  OPT_GAMMA_VECTOR_G,
  OPT_GAMMA_VECTOR_B,

  OPT_PREVIEW,

  NUM_OPTIONS			/* must come last */
};

/* already declared in the sane includes...
typedef union
{
	SANE_Word w;
	SANE_Bool b;
	SANE_Fixed f;
	SANE_Word *wa;
}
Option_Value;
*/

enum ScannerModels
{
  HP4200
};

typedef struct HP4200_Device
{
  struct HP4200_Device *next;
  SANE_Device dev;
  SANE_Handle handle;
}
HP4200_Device;

struct _scanner_buffer_t
{
  unsigned char *buffer;	/* buffer memory space */
  int size;			/* size of the buffer */
  int num_bytes;		/* number of bytes left (to read) */
  unsigned char *data_ptr;	/* cursor in buffer */
};

typedef struct _scanner_buffer_t scanner_buffer_t;

struct _ciclic_buffer_t
{
  int good_bytes;		/* number of valid bytes of the image */
  int num_lines;		/* number of lines of the ciclic buffer */
  int size;			/* size in bytes of the buffer space */
  unsigned char *buffer;	/* pointer to the buffer space */
  unsigned char **buffer_ptrs;	/* pointers to the beginning of each
				   line in the buffer space */
  int can_consume;		/* num of bytes the ciclic buf can consume */
  int current_line;		/* current scanned line */
  int first_good_line;		/* number of lines to fill the ``pipeline'' */
  unsigned char *buffer_position;	/* pointer to the first byte that
					   can be copied */
  int pixel_position;		/* pixel position in current line */

  /* color indexes for the proper line in the ciclic buffer */
  int red_idx;
  int green_idx;
  int blue_idx;
};

typedef struct _ciclic_buffer_t ciclic_buffer_t;

struct _hardware_parameters_t
{
  unsigned int SRAM_size;
  unsigned char SRAM_bandwidth;
  unsigned long crystal_frequency;
  unsigned int min_pixel_data_buffer_limit;
  unsigned int motor_full_steps_per_inch;
  float motor_max_speed;
  unsigned int scan_bar_max_speed;
  unsigned int start_of_scanning_area;
  unsigned int calibration_strip_height;
  unsigned int scan_area_width;
  double scan_area_length;	/* in inches */
  unsigned int sensor_num_pixels;
  unsigned int sensor_pixel_start;
  unsigned int sensor_pixel_end;
  int sensor_cds_state;		/* 0 == off, 1 == on */
  int sensor_signal_polarity;	/* 0 == ??, 1 == ?? */
  int sensor_max_integration_time;
  int sensor_line_separation;
  int sensor_type;
  unsigned int sensor_resolution;
  int sensor_control_signals_polarity;
  int sensor_control_signals_state;
  int sensor_control_pixel_rate_timing;
  int sensor_control_line_rate_timing;
  unsigned int sensor_black_clamp_timing;	/* ??? */
  unsigned int sensor_CIS_timing;
  int sensor_toshiba_timing;
  int sensor_color_modes;	/* bitmask telling color modes supported */
  int illumination_mode;
  int motor_control_mode;
  int motor_paper_sense_mode;
  int motor_pause_reverse_mode;
  int misc_io_mode;
  int num_tr_pulses;
  int guard_band_duration;
  int pulse_duration;
  int fsteps_25_speed;
  int fsteps_50_speed;
  int steps_to_reverse;
  struct
  {
    int red;
    int green;
    int blue;
  }
  target_value;
  unsigned short home_sensor;
};

typedef struct _hardware_parameters_t hardware_parameters_t;

struct _user_parameters_t
{
  unsigned int image_width;
  unsigned int lines_to_scan;
  unsigned int horizontal_resolution;
  unsigned int vertical_resolution;
  int hres_reduction_method;	/* interpolation/??? */
  int vres_reduction_method;
  SANE_Bool color;		/* color/grayscale */
  int bpp;
  int scan_mode;		/* preview/full scan */
  SANE_Bool no_reverse;
  SANE_Word gamma[3][1024];	/* gamma table for rgb */
};

typedef struct _user_parameters_t user_parameters_t;

struct _measured_parameters_t
{
  unsigned int datalink_bandwidth;
  struct
  {
    int red;
    int green;
    int blue;
  }
  coarse_calibration_data;
  struct
  {
    int *pRedOffset;
    int *pGreenOffset;
    int *pBlueOffset;
    int *pRedGain;
    int *pGreenGain;
    int *pBlueGain;
  }
  fine_calibration_data;
  int max_integration_time;
  int color_mode;
};

typedef struct _measured_parameters_t measured_parameters_t;

struct _runtime_parameters_t
{
  long num_bytes_left_to_scan;
  int status_bytes;		/* number of status bytes per line */
  int image_line_size;		/* line size in bytes without status bytes */
  int scanner_line_size;	/* line size in bytes including the
				   status bytes */
  int first_pixel;		/* first pixel in the line to be scanned */
  int steps_to_skip;
};

typedef struct _runtime_parameters_t runtime_parameters_t;

struct _HP4200_Scanner
{
  struct _HP4200_Scanner *next;

  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];

  SANE_Bool scanning;
  SANE_Bool aborted_by_user;

  SANE_Parameters params;

  HP4200_Device *dev;
  hardware_parameters_t hw_parms;
  user_parameters_t user_parms;
  measured_parameters_t msrd_parms;
  unsigned int regs[NUM_REGISTERS];
  int mclk;
  float mclk_div;

  int fd;

  ciclic_buffer_t ciclic_buffer;
  scanner_buffer_t scanner_buffer;
  runtime_parameters_t runtime_parms;
};

typedef struct _HP4200_Scanner HP4200_Scanner;

#endif /* !_HP4200_H */
